#pragma once

#include <cstdint>

#include "TemperatureAlert.h"

struct ClimateMeasured {
    float temperature_c;
    float humidity_pct;
    TemperatureLevel level;
    uint32_t sampled_at_ms;
};
