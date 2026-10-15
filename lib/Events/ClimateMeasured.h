#pragma once

#include <cstdint>

struct ClimateMeasured {
    float temperature_c;
    float humidity_pct;
    uint32_t sampled_at_ms;
};
