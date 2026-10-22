#include "LogProtocol.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <string_view>
#include <utility>

namespace {
bool isLower(char c) { return c >= 'a' && c <= 'z'; }

bool isDigit(char c) { return c >= '0' && c <= '9'; }
}  // namespace

std::string_view logproto::toString(Level level) {
    switch (level) {
        case Level::kError:
            return "error";
        case Level::kWarn:
            return "warn";
        case Level::kInfo:
            return "info";
    }

    return "error";
}

bool logproto::isValidSrc(std::string_view src) {
    if (src.empty() || src.length() > logproto::kMaxSrcLen) {
        return false;
    }

    return std::ranges::all_of(src, [](char c) noexcept {
        return isLower(c) || isDigit(c) || c == '_';
    });
}

std::size_t logproto::serialize(const Record& record, char* buf, std::size_t cap) {
    if (!isValidSrc(record.src)) {
        return 0;
    }

    const std::size_t text_len = std::min(record.text.size(), kMaxTextLen);
    std::array<char, kMaxTextLen> text{};

    std::ranges::transform(record.text.substr(0, text_len), text.begin(), [](char c) noexcept {
        return c == '\n' || c == '\r' ? ' ' : c;
    });

    const std::string_view level = toString(record.level);

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
    const int written = std::snprintf(
        buf,
        cap,
        "ver=%lu dev=%.*s boot=%08lx seq=%lu age_ms=%lu level=%.*s src=%.*s text=%.*s\n",
        static_cast<unsigned long>(kVersion),
        static_cast<int>(record.dev.size()),
        record.dev.data(),
        static_cast<unsigned long>(record.boot),
        static_cast<unsigned long>(record.seq),
        static_cast<unsigned long>(record.age_ms),
        static_cast<int>(level.size()),
        level.data(),
        static_cast<int>(record.src.size()),
        record.src.data(),
        static_cast<int>(text_len),
        text.data()
    );

    if (written <= 0 || std::cmp_greater_equal(written, cap)) {
        return 0;
    }

    return static_cast<size_t>(written);
}
