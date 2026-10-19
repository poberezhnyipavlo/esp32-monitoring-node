#pragma once

#include <cstdint>

#include "SnapshotStore.h"

enum class StatusColor : uint8_t {
    kOff,
    kBlue,
    kRed,
    kYellow,
    kGreen,
};

struct Status {
    StatusColor color;
    bool blinking;
};

struct StatusConfig {
    uint32_t sensor_stale_ms;
    uint32_t blink_period_ms;
};

struct RgbChannels {
    bool red;
    bool green;
    bool blue;
};

class StatusIndicator {
public:
    explicit StatusIndicator(const StatusConfig& config) : config_(config) {};

    [[nodiscard]] Status status(
        const Snapshot& snapshot,
        bool wifi_up,
        bool tcp_up,
        uint32_t now_ms
    ) const;

    [[nodiscard]] StatusColor colorAt(const Status& status, uint32_t now_ms) const;
    [[nodiscard]] bool isSensorStale(const Snapshot& snapshot, uint32_t now_ms) const;

private:
    StatusConfig config_;
};

[[nodiscard]] const char* toString(StatusColor color);
RgbChannels toRgb(StatusColor color);
