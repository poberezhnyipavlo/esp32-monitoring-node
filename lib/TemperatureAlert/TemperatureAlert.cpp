#include "TemperatureAlert.h"

#include "ClimateMeasured.h"

TemperatureAlert::TemperatureAlert(const TemperatureLimits& limits) : limits_(limits) {}

void TemperatureAlert::onClimateMeasured(const ClimateMeasured& event) {
    const float t = event.temperature_c;
    const float normal_from = limits_.min_c + limits_.hysteresis_c;
    const float normal_to = limits_.max_c - limits_.hysteresis_c;

    if (t > limits_.max_c) {
        level_ = TemperatureLevel::kTooHot;
    } else if (t < limits_.min_c) {
        level_ = TemperatureLevel::kTooCold;
    } else if (t > normal_from && t < normal_to) {
        level_ = TemperatureLevel::kNormal;
    }
    // Otherwise t is inside a band (min..normal_from or normal_to..max): keep the previous level.
}

const char* toString(TemperatureLevel level) {
    switch (level) {
        case TemperatureLevel::kNormal:
            return "normal";
        case TemperatureLevel::kTooHot:
            return "too_hot";
        case TemperatureLevel::kTooCold:
            return "too_cold";
    }

    return "unknown";
}
