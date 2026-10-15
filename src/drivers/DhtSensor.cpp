#include "DhtSensor.h"

#include <cmath>

DhtSensor::DhtSensor(uint8_t pin, uint8_t type) : dht_(pin, type) {}

void DhtSensor::begin() { dht_.begin(); }

std::optional<ClimateReading> DhtSensor::read() {
    if (!dht_.read(true)) {
        return std::nullopt;
    }

    const float t = dht_.readTemperature();
    const float h = dht_.readHumidity();

    if (std::isnan(t) || std::isnan(h)) {
        return std::nullopt;
    }

    return ClimateReading{.temperature_c = t, .humidity_pct = h};
}
