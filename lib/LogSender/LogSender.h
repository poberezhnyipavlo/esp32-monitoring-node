#pragma once

#include <metric_protocol/LogProtocol.h>

#include <array>
#include <cstddef>
#include <cstdint>

#include "ByteSink.h"
#include "Logger.h"
#include "SharedQueue.h"
#include "Units.h"

struct LogSendResult {
    uint32_t sent = 0;
    uint32_t skipped = 0;
    bool failed = false;
};

template <std::size_t N>
class LogSender {
public:
    LogSender(SharedQueue<LogEntry, N>& queue, ByteSink& sink) : queue_(queue), sink_(sink) {}

    [[nodiscard]] LogSendResult sendPending(std::string_view dev, uint32_t boot, uint32_t now_ms) {
        LogSendResult result;
        while (const auto entry = queue_.front()) {
            if (units::isAfter(entry->item.sampled_at_ms, now_ms)) {
                break;
            }
            const logproto::Record record{
                .dev = dev,
                .boot = boot,
                .seq = entry->item.seq,
                .age_ms = now_ms - entry->item.sampled_at_ms,
                .level = entry->item.level,
                .src = entry->item.src.data(),
                .text = entry->item.text.data(),
            };

            std::array<char, logproto::kMaxLineLen> buf{};
            const std::size_t len = logproto::serialize(record, buf.data(), buf.size());

            if (len == 0) {
                queue_.popFront(entry->id);
                ++result.skipped;
                continue;
            }
            if (!sink_.send(buf.data(), len)) {
                result.failed = true;
                break;
            }
            queue_.popFront(entry->id);
            ++result.sent;
        }

        return result;
    }

private:
    SharedQueue<LogEntry, N>& queue_;
    ByteSink& sink_;
};
