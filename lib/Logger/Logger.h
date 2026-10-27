#pragma once

#include <metric_protocol/LogProtocol.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string_view>

#include "SharedQueue.h"

struct LogEntry {
    logproto::Level level;
    std::array<char, logproto::kMaxSrcLen + 1> src;
    std::array<char, logproto::kMaxTextLen + 1> text;
    uint32_t seq;
    uint32_t sampled_at_ms;
};

template <std::size_t N>
class Logger {
public:
    explicit Logger(SharedQueue<LogEntry, N>& queue) : queue_(queue) {}

    void log(logproto::Level level, std::string_view src, std::string_view text, uint32_t now_ms) {
        LogEntry entry{
            .level = level,
            .src = {},
            .text = {},
            .seq = 0,  // assigned under the lock
            .sampled_at_ms = now_ms,
        };
        copyCut(src, entry.src);
        copyCut(text, entry.text);

        const std::scoped_lock lock(mutex_);
        entry.seq = next_seq_++;
        queue_.push(entry);
    }

private:
    template <std::size_t M>
    static void copyCut(std::string_view from, std::array<char, M>& to) {
        const std::size_t length = std::min(from.size(), to.size() - 1);
        std::copy_n(from.begin(), length, to.begin());
        to.at(length) = '\0';
    }

    SharedQueue<LogEntry, N>& queue_;
    std::mutex mutex_;
    uint32_t next_seq_ = 0;
};
