#include <Arduino.h>
#include <WiFi.h>
#include <esp_mac.h>
#include <esp_random.h>
#include <esp_system.h>
#include <esp_task_wdt.h>
#include <metric_protocol/MetricProtocol.h>

#include <array>
#include <atomic>

#include "EventBus.h"
#include "LogSender.h"
#include "Logger.h"
#include "NetworkState.h"
#include "QueueListener.h"
#include "QueueSender.h"
#include "RetryTimer.h"
#include "SampleQueue.h"
#include "Screen.h"
#include "SensorWatch.h"
#include "SnapshotStore.h"
#include "StatusIndicator.h"
#include "TemperatureAlert.h"
#include "Units.h"
#include "WifiKeeper.h"
#include "drivers/DhtSensor.h"
#include "drivers/Oled.h"
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
namespace sensor {
constexpr uint32_t kStaleMs = 10000;
}  // namespace sensor

namespace sampling {
constexpr uint32_t kPeriodMs = 2000;
constexpr size_t kQueueCapacity = 1000;
}  // namespace sampling

namespace logs {
constexpr uint16_t kPort = 9003;
constexpr std::size_t kQueueCapacity = 32;
}  // namespace logs

namespace tcp {
constexpr int32_t kConnectTimeoutMs = 3000;
constexpr uint32_t kRetryBaseMs = 1000;
constexpr uint32_t kStableMs = 10000;
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
constexpr uint32_t kStackBytes = 6144;
constexpr UBaseType_t kPriority = 1;
constexpr BaseType_t kCore = 1;
constexpr uint32_t kTicksMs = 250;
}  // namespace ui_task

namespace status_led {
constexpr StatusConfig kConfig{
    .sensor_stale_ms = sensor::kStaleMs,
    .blink_period_ms = units::kMsPerSecond,
};
}

namespace watchdog {
constexpr uint32_t kTimeoutMs = 15000;
// Longest legitimate blocking per pass: net connects two TcpLinks, loop sleeps between samples.
static_assert(kTimeoutMs > 2 * static_cast<uint32_t>(tcp::kConnectTimeoutMs) + units::kMsPerSecond);
static_assert(kTimeoutMs > sampling::kPeriodMs + units::kMsPerSecond);
}  // namespace watchdog

SampleQueue<sampling::kQueueCapacity> queue;
QueueListener<sampling::kQueueCapacity> queue_listener{queue};
TemperatureAlert temperature_alert{temperature::kLimits};

SharedQueue<LogEntry, logs::kQueueCapacity> log_queue;
Logger<logs::kQueueCapacity> logger{log_queue};

SensorWatch<logs::kQueueCapacity> sensor_watch{logger, sensor::kStaleMs};

EventBus<2> bus{{&queue_listener, &sensor_watch}};

std::atomic<int32_t> last_rssi{0};
TaskHandle_t net_task_handle = nullptr;
DhtSensor dht{pins::kDht, DHT_SENSOR_TYPE};

SnapshotStore snapshot_store;
std::atomic<NetworkState> network_state{NetworkState::kNoWifi};
TaskHandle_t ui_task_handle = nullptr;

const StatusIndicator status_indicator{status_led::kConfig};

RgbLed led{pins::kRgbRed, pins::kRgbGreen, pins::kRgbBlue};

Oled oled;
bool oled_ready = false;

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
            log_tcp_.close();
            network_state = NetworkState::kNoWifi;
            last_rssi = 0;
            return;
        }
        last_rssi = wifi_.rssi();
        tcp_.update(now);
        log_tcp_.update(now);
        if (log_tcp_.isOpen()) {
            const LogSendResult logs_result =
                log_sender_.sendPending(dev_.data(), origin_.boot, millis());
            if (logs_result.sent > 0) {
                Serial.printf(
                    "[net] logs sent %lu\n",
                    static_cast<unsigned long>(logs_result.sent)
                );
            }
        }
        network_state = tcp_.isOpen() ? NetworkState::kOnline : NetworkState::kNoReceiver;
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
        RetryTimer{tcp::kRetryBaseMs, tcp::kRetryMaxMs, tcp::kStableMs}
    };
    TcpLink log_tcp_{
        kMonitorHost,
        logs::kPort,
        tcp::kConnectTimeoutMs,
        RetryTimer{tcp::kRetryBaseMs, tcp::kRetryMaxMs, tcp::kStableMs}
    };
    QueueSender<sampling::kQueueCapacity> sender_{queue, tcp_};
    LogSender<logs::kQueueCapacity> log_sender_{log_queue, log_tcp_};
    std::array<char, metric::k_dev_len + 1> dev_{};
    metric::Origin origin_{};
};

void configureWatchdog() {
    const esp_task_wdt_config_t config{
        .timeout_ms = watchdog::kTimeoutMs,
        .idle_core_mask = 1U << 0,
        .trigger_panic = true,
    };

    if (esp_task_wdt_reconfigure(&config) != ESP_OK) {
        Serial.println("[app] cannot configure task watchdog");
    }
}

void watchCurrentTask(const char* name) {
    if (esp_task_wdt_add(nullptr) != ESP_OK) {
        Serial.printf("[app] cannot watch task %s\n", name);
    }
}

void netTaskMain(void* /*arg*/) {
    watchCurrentTask("net");
    static Uplink uplink;
    uplink.begin();
    for (;;) {
        esp_task_wdt_reset();
        uplink.loopOnce();
        vTaskDelay(pdMS_TO_TICKS(net_task::kIdleMs));
    }
}

void uiTaskMain(void* /*arg*/) {
    watchCurrentTask("ui");
    Status last_logged{.color = StatusColor::kOff, .blinking = false};
    Screen last_drawn{};
    for (;;) {
        esp_task_wdt_reset();
        const uint32_t notified = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(ui_task::kTicksMs));
        const std::optional<ClimateMeasured> snapshot = snapshot_store.read();
        const NetworkState network = network_state.load();
        const uint32_t now = millis();
        const Status status = status_indicator.status(snapshot, network, now);
        const RgbChannels rgb = toRgb(status_indicator.colorAt(status, now));
        led.set(rgb.red, rgb.green, rgb.blue);
        const bool changed =
            status.color != last_logged.color || status.blinking != last_logged.blinking;

        const Screen screen =
            buildScreen(snapshot, status_indicator.isSensorStale(snapshot, now), network, now);

        if (oled_ready && screen != last_drawn) {
            oled.draw(screen);
            last_drawn = screen;
        }

        if (notified > 0 || changed) {
            Serial.printf(
                "[ui] led=%s%s T=%.1f H=%.1f alert=%s net=%s stack_free=%u\n",
                toString(status.color),
                status.blinking ? " blink" : "",
                snapshot ? snapshot->temperature_c : 0.0F,
                snapshot ? snapshot->humidity_pct : 0.0,
                snapshot ? toString(snapshot->level) : "none",
                toString(network).data(),
                static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr))
            );
        }
        last_logged = status;
    }
}

struct ResetInfo {
    logproto::Level level;
    const char* name;
};

ResetInfo describeReset(esp_reset_reason_t reason) {
    switch (reason) {
        case ESP_RST_POWERON:
            return {.level = logproto::Level::kInfo, .name = "power_on"};
        case ESP_RST_EXT:
            return {.level = logproto::Level::kInfo, .name = "external_pin"};
        case ESP_RST_SW:
            return {.level = logproto::Level::kInfo, .name = "software"};
        case ESP_RST_DEEPSLEEP:
            return {.level = logproto::Level::kInfo, .name = "deep_sleep"};
        case ESP_RST_PANIC:
            return {.level = logproto::Level::kError, .name = "panic"};
        case ESP_RST_INT_WDT:
            return {.level = logproto::Level::kError, .name = "interrupt_watchdog"};
        case ESP_RST_TASK_WDT:
            return {.level = logproto::Level::kError, .name = "task_watchdog"};
        case ESP_RST_WDT:
            return {.level = logproto::Level::kError, .name = "watchdog"};
        case ESP_RST_BROWNOUT:
            return {.level = logproto::Level::kError, .name = "brownout"};
        default:
            return {.level = logproto::Level::kWarn, .name = "unknown"};
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
    oled_ready = oled.begin();

    Serial.println(oled_ready ? "[app] oled found" : "[app] oled not found");

    configureWatchdog();
    watchCurrentTask("loop");

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

    const ResetInfo reset = describeReset(esp_reset_reason());
    std::array<char, logproto::kMaxTextLen> text{};
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
    std::snprintf(text.data(), text.size(), "started, reason=%s", reset.name);
    logger.log(reset.level, "boot", text.data(), millis());
    Serial.printf("[app] reset reason: %s\n", reset.name);
}

void loop() {
    esp_task_wdt_reset();

    static TickType_t wake = xTaskGetTickCount();
    static uint32_t last_sample_ms = 0;

    vTaskDelayUntil(&wake, pdMS_TO_TICKS(sampling::kPeriodMs));

    const uint32_t now = millis();
    const uint32_t interval = last_sample_ms == 0 ? 0 : now - last_sample_ms;
    last_sample_ms = now;

    const std::optional<ClimateReading> reading = dht.read();
    if (!reading) {
        bus.publish(ClimateReadFailed{.sampled_at_ms = now});
        Serial.println("[app] dht read failed");
        return;
    }

    const TemperatureLevel level = temperature_alert.update(reading->temperature_c);

    const ClimateMeasured measured = ClimateMeasured{
        .temperature_c = reading->temperature_c,
        .humidity_pct = reading->humidity_pct,
        .level = level,
        .sampled_at_ms = now,
    };

    bus.publish(measured);
    snapshot_store.write(measured);

    xTaskNotifyGive(ui_task_handle);

    const auto stats = queue.stats();
    Serial.printf(
        "[app] sampled T=%.1f H=%.1f alert=%s interval=%lu ms queue=%u/%u dropped=%lu "
        "net_stack_free=%u\n",
        reading->temperature_c,
        reading->humidity_pct,
        toString(level),
        static_cast<unsigned long>(interval),
        static_cast<unsigned>(stats.size),
        static_cast<unsigned>(sampling::kQueueCapacity),
        static_cast<unsigned long>(stats.dropped),
        net_task_handle ? static_cast<unsigned>(uxTaskGetStackHighWaterMark(net_task_handle)) : 0U
    );
}
