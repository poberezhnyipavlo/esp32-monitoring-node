#pragma once

#include <metric_protocol/MetricNames.h>

#include <cmath>
#include <cstddef>
#include <cstdint>

#include "ClimateMeasured.h"
#include "Listener.h"
#include "SampleQueue.h"

[[nodiscard]] constexpr double toTempAlert(TemperatureLevel level) {
    switch (level) {
        case TemperatureLevel::kTooCold:
            return metric::temp_alert::kTooCold;
        case TemperatureLevel::kTooHot:
            return metric::temp_alert::kTooHot;
        case TemperatureLevel::kNormal:
            return metric::temp_alert::kNormal;
    }

    return metric::temp_alert::kNormal;
}

[[nodiscard]] inline double toTenths(float value) {
    constexpr double kTenthsPerUnit = 10.0;
    return std::round(static_cast<double>(value) * kTenthsPerUnit) / kTenthsPerUnit;
}

template <std::size_t N>
class QueueListener : public Listener {
public:
    explicit QueueListener(SampleQueue<N>& queue) : queue_{queue} {}

    void onClimateMeasured(const ClimateMeasured& event) override {
        queue_.push(
            Sample{
                .name = metric::names::kTemperatureC,
                .value = toTenths(event.temperature_c),
                .seq = next_seq_++,
                .sampled_at_ms = event.sampled_at_ms,
            }
        );
        queue_.push(
            Sample{
                .name = metric::names::kHumidityPct,
                .value = toTenths(event.humidity_pct),
                .seq = next_seq_++,
                .sampled_at_ms = event.sampled_at_ms,
            }
        );
        queue_.push(
            Sample{
                .name = metric::names::kTempAlert,
                .value = toTempAlert(event.level),
                .seq = next_seq_++,
                .sampled_at_ms = event.sampled_at_ms,
            }
        );
        queue_.push(
            Sample{
                .name = metric::names::kDhtOk,
                .value = 1,
                .seq = next_seq_++,
                .sampled_at_ms = event.sampled_at_ms,
            }
        );
    }

    void onClimateReadFailed(const ClimateReadFailed& event) override {
        queue_.push(
            Sample{
                .name = metric::names::kDhtOk,
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
