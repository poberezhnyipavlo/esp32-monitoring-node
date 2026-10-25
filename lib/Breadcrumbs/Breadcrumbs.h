#pragma once

#include <cstdint>
#include <optional>
#include <type_traits>

enum class Step : uint8_t {
    kIdle,
    kConnectMetrics,
    kConnectLogs,
    kSendMetrics,
    kSendLogs,
    kReadDht,
    kPublish,
    kDrawOled,
};

[[nodiscard]] constexpr const char* toString(Step step) {
    switch (step) {
        case Step::kIdle:
            return "idle";
        case Step::kConnectMetrics:
            return "connect_metrics";
        case Step::kConnectLogs:
            return "connect_logs";
        case Step::kSendMetrics:
            return "send_metrics";
        case Step::kSendLogs:
            return "send_logs";
        case Step::kReadDht:
            return "read_dht";
        case Step::kPublish:
            return "publish";
        case Step::kDrawOled:
            return "draw_oled";
    }
    return "unknown";
}

struct TaskSteps {
    Step loop;
    Step net;
    Step ui;
};

struct Breadcrumbs {
    uint32_t magic;
    uint32_t boot_count;
    TaskSteps steps;
};

static_assert(std::is_trivial_v<Breadcrumbs>);

inline constexpr uint32_t kBreadcrumbsMagic = 0xB1ACB0C5;

[[nodiscard]] std::optional<TaskSteps> startBoot(Breadcrumbs& crumbs, bool cold_start);
