#include "FaultCommand.h"

#include <cstddef>
#include <optional>
#include <string_view>

namespace {
constexpr std::string_view kSpaces = " \t\r\n";
}
std::optional<Fault> parseFault(std::string_view line) {
    const std::size_t first_non_space = line.find_first_not_of(kSpaces);
    if (first_non_space == std::string_view::npos) {
        return std::nullopt;
    }
    line.remove_prefix(first_non_space);
    const size_t last_non_space = line.find_last_not_of(kSpaces);
    line.remove_suffix(line.size() - 1 - last_non_space);

    if (line == "panic") {
        return Fault::kPanic;
    }

    if (line == "hang loop") {
        return Fault::kHangLoop;
    }

    if (line == "hang net") {
        return Fault::kHangNet;
    }

    if (line == "hang ui") {
        return Fault::kHangUi;
    }

    return std::nullopt;
}
