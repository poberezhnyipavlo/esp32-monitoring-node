#include "Screen.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <string_view>

#include "ClimateMeasured.h"
#include "NetworkState.h"
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

TextItem buildNetwork(NetworkState network_state) {
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
        network_state != NetworkState::kNoWifi ? "ok" : "no",
        network_state == NetworkState::kOnline ? "ok" : "no"
    );

    if (written < 0) {
        setText(network, "?");
    }
    return network;
}

TextItem buildClimate(const std::optional<ClimateMeasured>& snapshot) {
    TextItem climate{.x = 0, .y = layout::kClimateY, .size = layout::kBigText};

    if (snapshot) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
        const int written = std::snprintf(
            climate.text.data(),
            climate.text.size(),
            "%.1fC %.0f%%",
            static_cast<double>(snapshot->temperature_c),
            static_cast<double>(snapshot->humidity_pct)
        );
        if (written < 0) {
            setText(climate, "?");
        }
    } else {
        setText(climate, "--.-C --%");
    }

    return climate;
}

TextItem buildState(
    const std::optional<ClimateMeasured>& climate_reading,
    bool sensor_stale,
    uint32_t now_ms
) {
    TextItem state{
        .x = 0,
        .y = layout::kStateY,
        .size = layout::kSmallText,
    };

    if (sensor_stale) {
        const uint32_t silent_since_ms = climate_reading ? climate_reading->sampled_at_ms : 0;
        const uint32_t silent_s = (now_ms - silent_since_ms) / units::kMsPerSecond;
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
    } else if (!climate_reading) {
        setText(state, "STARTING");
    } else {
        setText(state, formatTemperatureLevel(climate_reading->level));
    }

    return state;
}
}  // namespace

Screen buildScreen(
    const std::optional<ClimateMeasured>& climate_measured,
    bool sensor_stale,
    NetworkState network_state,
    uint32_t now_ms
) {
    return Screen{
        .climate = buildClimate(climate_measured),
        .state = buildState(climate_measured, sensor_stale, now_ms),
        .network = buildNetwork(network_state),
    };
}
