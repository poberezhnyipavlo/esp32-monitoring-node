#include "StatusIndicator.h"

#include <cstdint>

#include "SnapshotStore.h"
#include "TemperatureAlert.h"

Status StatusIndicator::status(
    const Snapshot& snapshot,
    bool wifi_up,
    bool tcp_up,
    uint32_t now_ms
) const {
    if (isSensorStale(snapshot, now_ms)) {
        return Status{
            .color = StatusColor::kRed,
            .blinking = false,
        };
    }

    if (!snapshot.has_reading) {
        return Status{
            .color = StatusColor::kBlue,
            .blinking = false,
        };
    }

    if (snapshot.level != TemperatureLevel::kNormal) {
        return Status{
            .color = StatusColor::kRed,
            .blinking = true,
        };
    }

    if (!wifi_up || !tcp_up) {
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

bool StatusIndicator::isSensorStale(const Snapshot& snapshot, uint32_t now_ms) const {
    return now_ms - snapshot.last_ok_ms > config_.sensor_stale_ms;
}

RgbChannels toRgb(StatusColor color) {
    return RgbChannels{
        .red = color == StatusColor::kRed || color == StatusColor::kYellow,
        .green = color == StatusColor::kGreen || color == StatusColor::kYellow,
        .blue = color == StatusColor::kBlue,
    };
}
