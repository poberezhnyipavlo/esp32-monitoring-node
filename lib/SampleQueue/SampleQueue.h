#pragma once

#include <cstdint>
#include <mutex>
#include <string_view>

#include "SharedQueue.h"

struct Sample {
    std::string_view name;
    double value;
    uint32_t seq;
    uint32_t sampled_at_ms;
};

template <size_t N>
using  SampleQueue = SharedQueue<Sample, N>;
