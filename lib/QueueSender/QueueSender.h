#pragma once

#include <metric_protocol/MetricProtocol.h>

#include <array>
#include <cstddef>
#include <cstdint>

#include "ByteSink.h"
#include "SampleQueue.h"

struct SendResult {
    uint32_t sent = 0;
    uint32_t skipped = 0;
    uint32_t max_age_ms = 0;
    bool failed = false;
};

template <std::size_t N>
class QueueSender {
public:
    QueueSender(SampleQueue<N>& queue, ByteSink& sink) : queue_(queue), sink_(sink) {};

    [[nodiscard]] SendResult sendPending(const metric::Origin& origin, uint32_t now_ms) {
        SendResult result;
        while (const auto entry = queue_.front()) {
            const Sample& sample = entry->item;
            const uint32_t age_ms = now_ms - sample.sampled_at_ms;
            std::array<char, metric::k_max_message_len> buf{};
            const std::size_t len = metric::serialize(
                origin,
                sample.seq,
                age_ms,
                metric::Metric{.name = sample.name, .value = sample.value},
                buf.data(),
                buf.size()
            );
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
            result.max_age_ms = age_ms > result.max_age_ms ? age_ms : result.max_age_ms;
        }
        return result;
    }

private:
    SampleQueue<N>& queue_;
    ByteSink& sink_;
};
