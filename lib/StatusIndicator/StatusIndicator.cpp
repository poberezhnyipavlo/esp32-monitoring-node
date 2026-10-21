#include "StatusIndicator.h"

#include <cstdint>
#include <optional>

#include "NetworkState.h"
#include "TemperatureAlert.h"
#include "ClimateMeasured.h"

Status StatusIndicator::status(
    const std::optional<ClimateMeasured>& climate_measured,
    NetworkState network_state,
    uint32_t now_ms
) const {
    if (isSensorStale(climate_measured, now_ms)) {
        return Status{
            .color = StatusColor::kRed,
            .blinking = false,
        };
    }

    if (!climate_measured) {
        return Status{
            .color = StatusColor::kBlue,
            .blinking = false,
        };
    }

    if (climate_measured.value().level != TemperatureLevel::kNormal) {
        return Status{
            .color = StatusColor::kRed,
            .blinking = true,
        };
    }

    if (network_state != NetworkState::kOnline) {
        return Status{
            .color = StatusColor::kYellow,
            .blinking = false,
        };
    }

    return Status{
        .color = StatusColor::kGreen,
        .blinking = false,
    };
}

StatusColor StatusIndicator::colorAt(const Status& status, uint32_t now_ms) const {
    if (!status.blinking) {
        return status.color;
    }

    const auto half = config_.blink_period_ms / 2;
    const auto number = now_ms / half;

    return number % 2 == 0 ? status.color : StatusColor::kOff;
}

const char* toString(StatusColor color) {
    switch (color) {
        case StatusColor::kOff:
            return "off";
        case StatusColor::kRed:
            return "red";
        case StatusColor::kYellow:
            return "yellow";
        case StatusColor::kGreen:
            return "green";
        case StatusColor::kBlue:
            return "blue";
    }

    return "unknown";
}

bool StatusIndicator::isSensorStale(
    const std::optional<ClimateMeasured>& climate_measured,
    uint32_t now_ms
) const {
    if (!climate_measured) {
        return now_ms > config_.sensor_stale_ms;
    }

    return now_ms - climate_measured->sampled_at_ms > config_.sensor_stale_ms;
}

RgbChannels toRgb(StatusColor color) {
    return RgbChannels{
        .red = color == StatusColor::kRed || color == StatusColor::kYellow,
        .green = color == StatusColor::kGreen || color == StatusColor::kYellow,
        .blue = color == StatusColor::kBlue,
    };
}
