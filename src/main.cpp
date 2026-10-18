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
#include "SnapshotStore.h"
#include "StatusIndicator.h"
#include "TemperatureAlert.h"
#include "WifiKeeper.h"
#include "drivers/DhtSensor.h"
#include "drivers/RgbLed.h"
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
constexpr uint8_t kRgbRed = 18;
constexpr uint8_t kRgbGreen = 19;
constexpr uint8_t kRgbBlue = 23;
}  // namespace pins

namespace temperature {
constexpr TemperatureLimits kLimits{.min_c = 18.0F, .max_c = 27.0F, .hysteresis_c = 0.5F};
static_assert(isValid(kLimits), "temperature limits: bands overlap or no hysteresis");
}  // namespace temperature

namespace ui_task {
constexpr uint32_t kStackBytes = 4096;
constexpr UBaseType_t kPriority = 1;
constexpr BaseType_t kCore = 1;
constexpr uint32_t kTicksMs = 250;
constexpr uint32_t kSensorStaleMs = 10000;
}  // namespace ui_task

namespace status_led {
constexpr StatusConfig kConfig{
    .sensor_stale_ms = ui_task::kSensorStaleMs,
    .blink_period_ms = kMsPerSecond,
};
}

SampleQueue<sampling::kQueueCapacity> queue;
QueueListener<sampling::kQueueCapacity> queue_listener{queue};
TemperatureAlert temperature_alert{temperature::kLimits};
EventBus<2> bus{{&queue_listener, &temperature_alert}};

std::atomic<int32_t> last_rssi{0};
TaskHandle_t net_task_handle = nullptr;
DhtSensor dht{pins::kDht, DHT_SENSOR_TYPE};

SnapshotStore snapshot_store;
std::atomic<bool> wifi_up{false};
std::atomic<bool> tcp_up{false};
TaskHandle_t ui_task_handle = nullptr;

const StatusIndicator status_indicator{status_led::kConfig};

RgbLed led{pins::kRgbRed, pins::kRgbGreen, pins::kRgbBlue};

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
        wifi_up = wifi_.isConnected();
        if (!wifi_.isConnected()) {
            tcp_.close();
            tcp_up = false;
            last_rssi = 0;
            return;
        }
        last_rssi = wifi_.rssi();
        tcp_.update(now);
        tcp_up = tcp_.isOpen();
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

void uiTaskMain(void* /*arg*/) {
    Status last_logged{.color = StatusColor::kOff, .blinking = false};
    for (;;) {
        const uint32_t notified = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(ui_task::kTicksMs));
        const Snapshot snapshot = snapshot_store.read();
        const bool wifi = wifi_up.load();
        const bool tcp = tcp_up.load();
        const uint32_t now = millis();
        const Status status = status_indicator.status(snapshot, wifi, tcp, now);
        const RgbChannels rgb = toRgb(status_indicator.colorAt(status, now));
        led.set(rgb.red, rgb.green, rgb.blue);
        const bool changed =
            status.color != last_logged.color || status.blinking != last_logged.blinking;

        if (notified > 0 || changed) {
            Serial.printf(
                "[ui] led=%s%s T=%.1f H=%.1f alert=%s wifi=%d tcp=%d stack_free=%u\n",
                toString(status.color),
                status.blinking ? " blink" : "",
                snapshot.temperature_c,
                snapshot.humidity_pct,
                toString(snapshot.level),
                wifi ? 1 : 0,
                tcp ? 1 : 0,
                static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr))
            );
        }
        last_logged = status;
    }
}
}  // namespace

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println();
    Serial.println("esp32-monitoring-node");

    dht.begin();
    led.begin();

    const BaseType_t created = xTaskCreatePinnedToCore(
        netTaskMain,
        "net",
        net_task::kStackBytes,
        nullptr,
        net_task::kPriority,
        &net_task_handle,
        net_task::kCore
    );

    if (created != pdPASS) {
        Serial.println("[app] cannot create net task");
    }

    const BaseType_t ui_created = xTaskCreatePinnedToCore(
        uiTaskMain,
        "ui",
        ui_task::kStackBytes,
        nullptr,
        ui_task::kPriority,
        &ui_task_handle,
        ui_task::kCore
    );

    if (ui_created != pdPASS) {
        Serial.println("[app] cannot create ui task");
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

    bus.publish(
        ClimateMeasured{
            .temperature_c = reading->temperature_c,
            .humidity_pct = reading->humidity_pct,
            .sampled_at_ms = now,
        }
    );

    snapshot_store.write(
        Snapshot{
            .has_reading = true,
            .temperature_c = reading->temperature_c,
            .humidity_pct = reading->humidity_pct,
            .level = temperature_alert.level(),
            .last_ok_ms = now,
        }
    );
    xTaskNotifyGive(ui_task_handle);

    const auto stats = queue.stats();
    Serial.printf(
        "[app] sampled T=%.1f H=%.1f alert=%s interval=%lu ms queue=%u/%u dropped=%lu "
        "net_stack_free=%u\n",
        reading->temperature_c,
        reading->humidity_pct,
        toString(temperature_alert.level()),
        static_cast<unsigned long>(interval),
        static_cast<unsigned>(stats.size),
        static_cast<unsigned>(sampling::kQueueCapacity),
        static_cast<unsigned long>(stats.dropped),
        net_task_handle ? static_cast<unsigned>(uxTaskGetStackHighWaterMark(net_task_handle)) : 0U
    );
}
