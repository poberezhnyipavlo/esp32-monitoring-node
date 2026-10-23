#pragma once

#include <cstdint>

namespace units {
inline constexpr uint32_t kMsPerSecond = 1000;

[[nodiscard]] constexpr bool isAfter(uint32_t a_ms, uint32_t b_ms) {
    return static_cast<int32_t>(a_ms - b_ms) > 0;
}
}  // namespace units
