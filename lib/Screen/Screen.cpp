#include "Screen.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <string_view>

#include "SnapshotStore.h"
#include "TemperatureAlert.h"
#include "Units.h"

namespace {
namespace layout {
constexpr uint16_t kClimateY = 0;
constexpr uint16_t kStateY = 24;
constexpr uint16_t kNetworkY = 40;
constexpr uint16_t kBigText = 2;
constexpr uint16_t kSmallText = 1;
}  // namespace layout

std::string_view formatTemperatureLevel(TemperatureLevel level) {
    switch (level) {
        case TemperatureLevel::kTooHot:
            return "TOO HOT";
        case TemperatureLevel::kTooCold:
            return "TOO COLD";
        case TemperatureLevel::kNormal:
            return "OK";
    }

    return "?";
}

void setText(TextItem& item, std::string_view text) {
    const std::size_t length = std::min(text.size(), item.text.size() - 1);
    std::copy_n(text.begin(), length, item.text.begin());
    item.text.at(length) = '\0';
}

TextItem buildNetwork(bool wifi_up, bool tcp_up) {
    TextItem network{
        .x = 0,
        .y = layout::kNetworkY,
        .size = layout::kSmallText,
    };

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
    const int written = std::snprintf(
        network.text.data(),
        network.text.size(),
        "WiFi:%s TCP:%s",
        wifi_up ? "ok" : "no",
        tcp_up ? "ok" : "no"
    );

    if (written < 0) {
        setText(network, "?");
    }
    return network;
}

TextItem buildClimate(const Snapshot& snapshot) {
    TextItem climate{.x = 0, .y = layout::kClimateY, .size = layout::kBigText};

    if (snapshot.has_reading) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
        const int written = std::snprintf(
            climate.text.data(),
            climate.text.size(),
            "%.1fC %.0f%%",
            static_cast<double>(snapshot.temperature_c),
            static_cast<double>(snapshot.humidity_pct)
        );
        if (written < 0) {
            setText(climate, "?");
        }
    } else {
        setText(climate, "--.-C --%");
    }

    return climate;
}

TextItem buildState(const Snapshot& snapshot, bool sensor_stale, uint32_t now_ms) {
    TextItem state{
        .x = 0,
        .y = layout::kStateY,
        .size = layout::kSmallText,
    };

    if (sensor_stale) {
        const uint32_t silent_s = (now_ms - snapshot.last_ok_ms) / units::kMsPerSecond;
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
        const int written = std::snprintf(
            state.text.data(),
            state.text.size(),
            "SENSOR FAIL %lus",
            static_cast<unsigned long>(silent_s)
        );
        if (written < 0) {
            setText(state, "?");
        }
    } else if (!snapshot.has_reading) {
        setText(state, "STARTING");
    } else {
        setText(state, formatTemperatureLevel(snapshot.level));
    }

    return state;
}
}  // namespace

Screen buildScreen(
    const Snapshot& snapshot,
    bool sensor_stale,
    bool wifi_up,
    bool tcp_up,
    uint32_t now_ms
) {
    return Screen{
        .climate = buildClimate(snapshot),
        .state = buildState(snapshot, sensor_stale, now_ms),
        .network = buildNetwork(wifi_up, tcp_up),
    };
}
