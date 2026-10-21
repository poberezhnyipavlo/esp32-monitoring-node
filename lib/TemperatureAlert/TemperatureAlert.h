#pragma once

#include <cstdint>

enum class TemperatureLevel : uint8_t {
    kNormal,
    kTooHot,
    kTooCold,
};

struct TemperatureLimits {
    float min_c;
    float max_c;
    float hysteresis_c;
};

constexpr bool isValid(const TemperatureLimits& limits) {
    return limits.hysteresis_c > 0.0F &&
           limits.min_c + limits.hysteresis_c <= limits.max_c - limits.hysteresis_c;
}

class TemperatureAlert {
public:
    explicit TemperatureAlert(const TemperatureLimits& limits);

    [[nodiscard]] TemperatureLevel update(float temperature_c);
    [[nodiscard]] TemperatureLevel level() const { return level_; }

private:
    TemperatureLimits limits_;
    TemperatureLevel level_ = TemperatureLevel::kNormal;
};

[[nodiscard]] const char* toString(TemperatureLevel level);
