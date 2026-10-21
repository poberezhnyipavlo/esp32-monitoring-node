#pragma once

#include <cstdint>
#include <optional>

#include "ClimateMeasured.h"
#include "NetworkState.h"

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
        const std::optional<ClimateMeasured>& climate_measured,
        NetworkState network_state,
        uint32_t now_ms
    ) const;

    [[nodiscard]] StatusColor colorAt(const Status& status, uint32_t now_ms) const;
    [[nodiscard]] bool isSensorStale(
        const std::optional<ClimateMeasured>& climate_measured,
        uint32_t now_ms
    ) const;

private:
    StatusConfig config_;
};

[[nodiscard]] const char* toString(StatusColor color);
RgbChannels toRgb(StatusColor color);
