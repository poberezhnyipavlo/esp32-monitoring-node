#include <Arduino.h>
#include <WiFi.h>
#include <esp_mac.h>
#include <esp_random.h>
#include <metric_protocol/MetricProtocol.h>

#include <array>
#include <atomic>

#include "EventBus.h"
#include "QueueListener.h"
#include "QueueSender.h"
#include "RetryTimer.h"
#include "SampleQueue.h"
#include "WifiKeeper.h"
#include "drivers/DhtSensor.h"
#include "net/TcpLink.h"
#include "net/WifiLink.h"

#ifdef WOKWI_WIFI
constexpr const char* kWifiSsid = "Wokwi-GUEST";
constexpr const char* kWifiPassword = "";
constexpr int32_t kWifiChannel = 6;
constexpr const char* kMonitorHost = "10.13.37.1";
#elif __has_include("secrets.h")
#include "secrets.h"
constexpr int32_t kWifiChannel = 0;
#else
#error "No include/secrets.h: copy include/secrets.example.h to include/secrets.h and fill it in"
#endif

#ifndef DHT_SENSOR_TYPE
#error "DHT_SENSOR_TYPE is not set: add -D DHT_SENSOR_TYPE=DHT11 (or DHT22) to build_flags"
#endif

namespace {
constexpr uint32_t kMsPerSecond = 1000;

namespace sampling {
constexpr uint32_t kPeriodMs = 2000;
constexpr size_t kQueueCapacity = 1000;
}  // namespace sampling

namespace tcp {
constexpr int32_t kConnectTimeoutMs = 3000;
constexpr uint32_t kRetryBaseMs = 1000;
constexpr uint32_t kRetryMaxMs = 30000;
constexpr uint16_t kPort = 9002;
}  // namespace tcp

namespace wifi {
constexpr uint32_t kConnectTimeoutMs = 15000;
constexpr uint32_t kRetryBaseMs = 1000;
constexpr uint32_t kRetryMaxMs = 30000;
}  // namespace wifi

namespace net_task {
constexpr uint32_t kStackBytes = 8192;
constexpr UBaseType_t kPriority = 1;
constexpr BaseType_t kCore = 0;
constexpr uint32_t kIdleMs = 20;
}  // namespace net_task

namespace pins {
constexpr uint8_t kDht = 4;
}

SampleQueue<sampling::kQueueCapacity> queue;
QueueListener<sampling::kQueueCapacity> queue_listener{queue};
EventBus<1> bus{{&queue_listener}};

std::atomic<int32_t> lastRssi{0};
TaskHandle_t netTask = nullptr;
DhtSensor dht{pins::kDht, DHT_SENSOR_TYPE};

class Uplink {
public:
    void begin() {
        wifi_.begin();
        std::array<uint8_t, 6> mac{};
        esp_efuse_mac_get_default(mac.data());
        dev_ = metric::formatDev(mac);
        origin_ = metric::Origin{.dev = dev_.data(), .boot = esp_random()};
        Serial.printf(
            "[net] dev=%s boot=%08lx, monitor %s:%u\n",
            dev_.data(),
            static_cast<unsigned long>(origin_.boot),
            kMonitorHost,
            static_cast<unsigned>(tcp::kPort)
        );
    }

    void loopOnce() {
        const uint32_t now = millis();
        wifi_.update(now);
        if (!wifi_.isConnected()) {
            tcp_.close();
            lastRssi = 0;
            return;
        }
        lastRssi = wifi_.rssi();
        tcp_.update(now);
        if (!tcp_.isOpen()) {
            return;
        }
        const SendResult result = sender_.sendPending(origin_, millis());
        if (result.skipped > 0) {
            Serial.printf(
                "[net] skipped %lu invalid samples\n",
                static_cast<unsigned long>(result.skipped)
            );
        }
        if (result.sent > 0) {
            Serial.printf(
                "[net] sent %lu, max age_ms=%lu, queue=%u\n",
                static_cast<unsigned long>(result.sent),
                static_cast<unsigned long>(result.max_age_ms),
                static_cast<unsigned>(queue.stats().size)
            );
        }
    }

private:
    WifiLink wifi_{
        kWifiSsid,
        kWifiPassword,
        kWifiChannel,
        WifiKeeper{wifi::kConnectTimeoutMs, wifi::kRetryBaseMs, wifi::kRetryMaxMs}
    };
    TcpLink tcp_{
        kMonitorHost,
        tcp::kPort,
        tcp::kConnectTimeoutMs,
        RetryTimer{tcp::kRetryBaseMs, tcp::kRetryMaxMs}
    };
    QueueSender<sampling::kQueueCapacity> sender_{queue, tcp_};
    std::array<char, metric::k_dev_len + 1> dev_{};
    metric::Origin origin_{};
};

void netTaskMain(void* /*arg*/) {
    static Uplink uplink;
    uplink.begin();
    for (;;) {
        uplink.loopOnce();
        vTaskDelay(pdMS_TO_TICKS(net_task::kIdleMs));
    }
}
}  // namespace

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println();
    Serial.println("esp32-monitoring-node");

    dht.begin();

    const BaseType_t created = xTaskCreatePinnedToCore(
        netTaskMain,
        "net",
        net_task::kStackBytes,
        nullptr,
        net_task::kPriority,
        &netTask,
        net_task::kCore
    );

    if (created != pdPASS) {
        Serial.println("[app] cannot create net task");
    }
    Serial.printf(
        "[app] Ready: sample every %lu ms, queue capacity %u\n",
        static_cast<unsigned long>(sampling::kPeriodMs),
        static_cast<unsigned>(sampling::kQueueCapacity)
    );
}

void loop() {
    static TickType_t wake = xTaskGetTickCount();
    static uint32_t last_sample_ms = 0;

    vTaskDelayUntil(&wake, pdMS_TO_TICKS(sampling::kPeriodMs));

    const uint32_t now = millis();
    const uint32_t interval = last_sample_ms == 0 ? 0 : now - last_sample_ms;
    last_sample_ms = now;

    const std::optional<ClimateReading> reading = dht.read();
    if (!reading) {
        Serial.println("[app] dht read failed");
        return;
    }

    bus.publish(ClimateMeasured{
        .temperature_c =  reading->temperature_c,
        .humidity_pct = reading->humidity_pct,
        .sampled_at_ms = now,
    });

    const auto stats = queue.stats();
    Serial.printf(
        "[app] sampled T=%.1f H=%.1f interval=%lu ms queue=%u/%u dropped=%lu "
        "net_stack_free=%u\n",
        reading->temperature_c,
        reading->humidity_pct,
        static_cast<unsigned long>(interval),
        static_cast<unsigned>(stats.size),
        static_cast<unsigned>(sampling::kQueueCapacity),
        static_cast<unsigned long>(stats.dropped),
        netTask ? static_cast<unsigned>(uxTaskGetStackHighWaterMark(netTask)) : 0U
    );
}
