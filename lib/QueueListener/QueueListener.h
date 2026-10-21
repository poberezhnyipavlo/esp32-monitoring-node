#pragma once

#include <metric_protocol/MetricNames.h>

#include <cstddef>
#include <cstdint>

#include "ClimateMeasured.h"
#include "Listener.h"
#include "SampleQueue.h"

[[nodiscard]] constexpr double toTempAlert(TemperatureLevel level) {
    switch (level) {
        case TemperatureLevel::kTooCold:
            return metric::temp_alert::k_too_cold;
        case TemperatureLevel::kTooHot:
            return metric::temp_alert::k_too_hot;
        case TemperatureLevel::kNormal:
            return metric::temp_alert::k_normal;
    }

    return metric::temp_alert::k_normal;
}

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
        queue_.push(
            Sample{
                .name = metric::names::k_temp_alert,
                .value = toTempAlert(event.level),
                .seq = next_seq_++,
                .sampled_at_ms = event.sampled_at_ms,
            }
        );
        queue_.push(
            Sample{
                .name = metric::names::k_dht_ok,
                .value = 1,
                .seq = next_seq_++,
                .sampled_at_ms = event.sampled_at_ms,
            }
        );
    }

    void onClimateReadFailed(const ClimateReadFailed& event) override {
        queue_.push(
            Sample{
                .name = metric::names::k_dht_ok,
                .value = 0,
                .seq = next_seq_++,
                .sampled_at_ms = event.sampled_at_ms,
            }
        );
    }

private:
    SampleQueue<N>& queue_;
    uint32_t next_seq_ = 0;
};
