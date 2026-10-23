#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <optional>

#include "ClimateMeasured.h"
#include "ClimateReadFailed.h"
#include "Listener.h"
#include "LogProtocol.h"
#include "Logger.h"
#include "Units.h"

template<std::size_t N>
class SensorWatch : public Listener {
public:
    SensorWatch(Logger<N> & logger, uint32_t stale_ms) : logger_(logger), stale_ms_(stale_ms) {}

    void onClimateMeasured(const ClimateMeasured &event) override {
        if (lost_) {
            const uint32_t silent_s = (event.sampled_at_ms - last_ok_ms_.value_or(0)) / units::kMsPerSecond;
            std::array<char, logproto::kMaxTextLen> text{};

            const int written = std::snprintf(
                text.data(),
                text.size(),
                "sensor answers again after %lu s",
                static_cast<unsigned long>(silent_s)
            );

            logger_.log(
                logproto::Level::kInfo,
                "dht",
                written > 0 ? text.data() : "sensor answer again",
                event.sampled_at_ms
            );
            lost_ = false;
        }

        last_ok_ms_ = event.sampled_at_ms;
    }

    void onClimateReadFailed(const ClimateReadFailed &event) override {
        if (lost_) {
            return;
        }

        const uint32_t silent_ms = event.sampled_at_ms - last_ok_ms_.value_or(0);
        if (silent_ms > stale_ms_) {
            logger_.log(logproto::Level::kWarn, "dht", "sensor stopped answering", event.sampled_at_ms);
            lost_ = true;
        }
    }

private:
    Logger<N>& logger_;
    uint32_t stale_ms_;
    std::optional<uint32_t> last_ok_ms_;
    bool lost_ = false;
};
