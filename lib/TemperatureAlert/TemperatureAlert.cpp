#include "TemperatureAlert.h"

TemperatureAlert::TemperatureAlert(const TemperatureLimits& limits) : limits_(limits) {}

TemperatureLevel TemperatureAlert::update(float temperature_c) {
    const float normal_from = limits_.min_c + limits_.hysteresis_c;
    const float normal_to = limits_.max_c - limits_.hysteresis_c;

    if (temperature_c > limits_.max_c) {
        level_ = TemperatureLevel::kTooHot;
    }

    if (temperature_c < limits_.min_c) {
        level_ = TemperatureLevel::kTooCold;
    }

    if (temperature_c > normal_from && temperature_c < normal_to) {
        level_ = TemperatureLevel::kNormal;
    }

    // Otherwise t is inside a band (min..normal_from or normal_to..max): keep the previous level.
    return level_;
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
