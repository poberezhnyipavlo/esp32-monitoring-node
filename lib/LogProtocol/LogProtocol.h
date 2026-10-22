#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace logproto {
inline constexpr uint32_t kVersion = 1;
inline constexpr std::size_t kMaxSrcLen = 15;
inline constexpr std::size_t kMaxTextLen = 96;
inline constexpr std::size_t kMaxLineLen = 220;

enum class Level : uint8_t {
    kInfo,
    kWarn,
    kError,
};

[[nodiscard]] std::string_view toString(Level level);

struct Record {
    std::string_view dev;
    uint32_t boot;
    uint32_t seq;
    uint32_t age_ms;
    Level level;
    std::string_view src;
    std::string_view text;
};

[[nodiscard]] bool isValidSrc(std::string_view src);
[[nodiscard]] std::size_t serialize(const Record& record, char* buf, std::size_t cap);
}  // namespace logproto
