#pragma once

#include <metric_protocol/MetricNames.h>

#include <cstddef>
#include <cstdint>
#include <queue>
#include <string_view>

#include "ClimateMeasured.h"
#include "Listener.h"
#include "SampleQueue.h"

template <std::size_t N>
class QueueListener : public Listener {
public:
    explicit QueueListener(SampleQueue<N>& queue) : queue_{queue} {}

    void onClimateMeasured(const ClimateMeasured& event) override {
        queue_.push(
            Sample{
                .name = metric::names::k_temperature_c,
                .value = event.temperature_c,
                .seq = next_seq_++,
                .sampled_at_ms = event.sampled_at_ms,
            }
        );
        queue_.push(
            Sample{
                .name = metric::names::k_humidity_pct,
                .value = event.humidity_pct,
                .seq = next_seq_++,
                .sampled_at_ms = event.sampled_at_ms,
            }
        );
    }

private:
    SampleQueue<N>& queue_;
    uint32_t next_seq_ = 0;
};
