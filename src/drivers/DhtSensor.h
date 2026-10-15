#pragma once

#include <DHT.h>

#include <cstdint>
#include <optional>

struct ClimateReading {
    float temperature_c;
    float humidity_pct;
};

class DhtSensor {
public:
    DhtSensor(uint8_t pin, uint8_t type);

    void begin();
    [[nodiscard]] std::optional<ClimateReading> read();

private:
    DHT dht_;
};
