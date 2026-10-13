#include <Arduino.h>
#include <WiFi.h>
#include <esp_mac.h>
#include <esp_random.h>
#include <metric_protocol/MetricProtocol.h>

#include <array>
#include <atomic>
#include <optional>

#include "RetryTimer.h"
#include "SampleQueue.h"
#include "SignalQuality.h"
#include "WifiKeeper.h"

#if defined(WOKWI_WIFI)
constexpr char WIFI_SSID[] = "Wokwi-GUEST";
constexpr char WIFI_PASSWORD[] = "";
constexpr int32_t WIFI_CHANNEL = 6;
constexpr char MONITOR_HOST[] = "10.13.37.1";
#elif __has_include("secrets.h")
#include "secrets.h"  // WIFI_SSID, WIFI_PASSWORD, MONITOR_HOST
constexpr int32_t WIFI_CHANNEL = 0;
#else
#error "No include/secrets.h: copy include/secrets.example.h to include/secrets.h and fill it in"
#endif

namespace {
constexpr uint32_t SAMPLE_PERIOD_MS = 2000;
constexpr uint32_t MS_PER_SECOND = 1000;
constexpr size_t QUEUE_CAPACITY = 1000;

constexpr uint16_t MONITOR_PORT = 9002;
constexpr int32_t TCP_CONNECT_TIMEOUT_MS = 3000;
constexpr uint32_t TCP_RETRY_BASE_MS = 1000;
constexpr uint32_t TCP_RETRY_MAX_MS = 30000;
constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 15000;
constexpr uint32_t WIFI_BACKOFF_BASE_MS = 1000;
constexpr uint32_t WIFI_BACKOFF_MAX_MS = 30000;

constexpr uint32_t NET_STACK_BYTES = 8192;
constexpr UBaseType_t NET_PRIORITY = 1;
constexpr UBaseType_t NET_CORE = 0;
constexpr uint32_t NET_IDLE_MS = 20;

SampleQueue<QUEUE_CAPACITY> queue;
std::atomic<int32_t> lastRssi{0};
TaskHandle_t netTask = nullptr;

class Uplink {
public:
    void begin() {
        WiFi.persistent(false);
        WiFi.mode(WIFI_STA);
        WiFi.setAutoReconnect(false);

        std::array<uint8_t, 6> mac{};
        esp_efuse_mac_get_default(mac.data());
        dev_ = metric::formatDev(mac);
        origin_ =
            metric::Origin{dev_.data(), esp_random()};  // після WiFi.mode: RNG справді випадковий
        Serial.printf(
            "[net] dev=%s boot=%08lx, monitor %s:%u\n",
            dev_.data(),
            static_cast<unsigned long>(origin_.boot),
            MONITOR_HOST,
            static_cast<unsigned>(MONITOR_PORT)
        );
    }

    void loopOnce() {
        const uint32_t now = millis();
        updateWifi(now);
        if (!keeper_.isConnected()) {
            lastRssi = 0;
            return;
        }
        lastRssi = WiFi.RSSI();

        if (!monitorIp_) {
            resolveMonitor();
        }
        if (tcpUp_ && !client_.connected()) {
            dropConnection("connection lost");
        }
        if (!tcpUp_ && monitorIp_ && tcpRetry_.due(now)) {
            tryConnect();
        }
        if (tcpUp_) {
            sendPending();
        }
    }

private:
    void updateWifi(uint32_t now) {
        switch (keeper_.update(WiFi.status() == WL_CONNECTED, now)) {
            case WifiAction::Connect:
                Serial.printf(
                    "[net] Connecting to %s (attempt %lu)...\n",
                    WIFI_SSID,
                    static_cast<unsigned long>(keeper_.attempt())
                );
                if (beganOnce_) {
                    WiFi.disconnect();
                }
                beganOnce_ = true;
                WiFi.begin(WIFI_SSID, WIFI_PASSWORD, WIFI_CHANNEL);
                break;
            case WifiAction::Connected:
                Serial.printf(
                    "[net] Connected, IP %s, RSSI %d dBm (%s)\n",
                    WiFi.localIP().toString().c_str(),
                    WiFi.RSSI(),
                    signalQuality(WiFi.RSSI())
                );
                break;
            case WifiAction::Timeout:
                Serial.printf(
                    "[net] Connect timeout, retry in %lu ms\n",
                    static_cast<unsigned long>(keeper_.backoffMs())
                );
                break;
            case WifiAction::Lost:
                Serial.printf(
                    "[net] Connection lost, retry in %lu ms\n",
                    static_cast<unsigned long>(keeper_.backoffMs())
                );
                if (tcpUp_) {
                    client_.stop();
                    tcpUp_ = false;
                }
                break;
            case WifiAction::None:
                break;
        }
    }

    void resolveMonitor() {
        IPAddress ip;
        if (WiFi.hostByName(MONITOR_HOST, ip) != 1) {
            Serial.printf("[net] Cannot resolve %s, will retry\n", MONITOR_HOST);
            return;
        }
        monitorIp_ = ip;
    }

    void dropConnection(const char* reason) {
        client_.stop();
        tcpRetry_.failed(millis());
        tcpUp_ = false;
        Serial.printf(
            "[net] TCP: %s, retry in %lu ms\n",
            reason,
            static_cast<unsigned long>(tcpRetry_.delayMs())
        );
    }

    void tryConnect() {
        Serial.printf(
            "[net] TCP: connecting to %s:%u (attempt %lu)...\n",
            monitorIp_->toString().c_str(),
            static_cast<unsigned>(MONITOR_PORT),
            static_cast<unsigned long>(tcpRetry_.failures() + 1)
        );
        const uint32_t started = millis();
        const bool ok = client_.connect(*monitorIp_, MONITOR_PORT, TCP_CONNECT_TIMEOUT_MS) == 1;
        const uint32_t took = millis() - started;
        if (!ok) {
            tcpRetry_.failed(millis());
            Serial.printf(
                "[net] TCP: connect failed after %lu ms, retry in %lu ms\n",
                static_cast<unsigned long>(took),
                static_cast<unsigned long>(tcpRetry_.delayMs())
            );
            return;
        }
        client_.setNoDelay(true);
        tcpRetry_.succeeded();
        tcpUp_ = true;
        Serial.printf("[net] TCP: connected in %lu ms\n", static_cast<unsigned long>(took));
    }

    void sendPending() {
        uint32_t sent = 0;
        uint32_t maxAge = 0;
        while (const auto e = queue.front()) {
            const uint32_t age = millis() - e->item.sampled_at_ms;
            std::array<char, metric::k_max_message_len> buf{};
            const size_t len = metric::serialize(
                origin_,
                e->item.seq,
                age,
                metric::Metric{e->item.name, e->item.value},
                buf.data(),
                buf.size()
            );

            if (len == 0) {
                Serial.printf(
                    "[net] skip invalid sample seq=%lu\n",
                    static_cast<unsigned long>(e->item.seq)
                );
                queue.popFront(e->id);
                continue;
            }
            if (client_.write(reinterpret_cast<const uint8_t*>(buf.data()), len) != len) {
                dropConnection("connection lost");
                break;
            }
            queue.popFront(e->id);
            ++sent;
            maxAge = age > maxAge ? age : maxAge;
        }

        if (sent > 0) {
            Serial.printf(
                "[net] sent %lu, max age_ms=%lu, queue=%u\n",
                static_cast<unsigned long>(sent),
                static_cast<unsigned long>(maxAge),
                static_cast<unsigned>(queue.stats().size)
            );
        }
    }

    WifiKeeper keeper_{WIFI_CONNECT_TIMEOUT_MS, WIFI_BACKOFF_BASE_MS, WIFI_BACKOFF_MAX_MS};
    bool beganOnce_ = false;
    WiFiClient client_;
    std::optional<IPAddress> monitorIp_;
    RetryTimer tcpRetry_{TCP_RETRY_BASE_MS, TCP_RETRY_MAX_MS};
    bool tcpUp_ = false;
    std::array<char, metric::k_dev_len + 1> dev_{};
    metric::Origin origin_{};
};

void netTaskMain(void* /*arg*/) {
    static Uplink uplink;
    uplink.begin();
    for (;;) {
        uplink.loopOnce();
        vTaskDelay(pdMS_TO_TICKS(NET_IDLE_MS));
    }
}
}  // namespace

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println();
    Serial.println("13-freertos-tasks");

    const BaseType_t created = xTaskCreatePinnedToCore(
        netTaskMain,
        "net",
        NET_STACK_BYTES,
        nullptr,
        NET_PRIORITY,
        &netTask,
        NET_CORE
    );

    if (created != pdPASS) {
        Serial.println("[app] cannot create net task");
    }
    Serial.printf(
        "[app] Ready: sample every %lu ms, queue capacity %u\n",
        static_cast<unsigned long>(SAMPLE_PERIOD_MS),
        static_cast<unsigned>(QUEUE_CAPACITY)
    );
}

void loop() {
    static TickType_t wake = xTaskGetTickCount();
    static uint32_t seq = 0;
    static uint32_t lastSampleMs = 0;

    vTaskDelayUntil(&wake, pdMS_TO_TICKS(SAMPLE_PERIOD_MS));

    const uint32_t now = millis();
    const uint32_t interval = lastSampleMs == 0 ? 0 : now - lastSampleMs;
    lastSampleMs = now;

    const uint32_t first = seq;
    queue.push(Sample{"uptime_s", static_cast<double>(now / MS_PER_SECOND), seq++, now});
    queue.push(Sample{"free_heap_bytes", static_cast<double>(ESP.getFreeHeap()), seq++, now});
    const int32_t rssi = lastRssi;
    if (rssi != 0) {
        queue.push(Sample{"rssi_dbm", static_cast<double>(rssi), seq++, now});
    }

    const auto stats = queue.stats();
    Serial.printf(
        "[app] sampled seq=%lu..%lu interval=%lu ms queue=%u/%u dropped=%lu "
        "net_stack_free=%u\n",
        static_cast<unsigned long>(first),
        static_cast<unsigned long>(seq - 1),
        static_cast<unsigned long>(interval),
        static_cast<unsigned>(stats.size),
        static_cast<unsigned>(QUEUE_CAPACITY),
        static_cast<unsigned long>(stats.dropped),
        netTask ? static_cast<unsigned>(uxTaskGetStackHighWaterMark(netTask)) : 0U
    );
}
