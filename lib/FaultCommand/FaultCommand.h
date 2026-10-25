#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

enum class Fault : uint8_t {
    kPanic,
    kHangLoop,
    kHangNet,
    kHangUi,
};

[[nodiscard]] std::optional<Fault> parseFault(std::string_view line);
