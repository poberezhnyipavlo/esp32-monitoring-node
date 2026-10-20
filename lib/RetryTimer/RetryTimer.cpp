#include "RetryTimer.h"

#include <cstdint>

RetryTimer::RetryTimer(uint32_t base_ms, uint32_t max_ms, uint32_t stable_ms)
    : base_ms_(base_ms), max_ms_(max_ms), stable_ms_(stable_ms) {}

bool RetryTimer::due(uint32_t now_ms) const {
    if (failures_ == 0) {
        return true;
    }

    return now_ms - failed_at_ms_ >= delay_ms_;
}

void RetryTimer::failed(uint32_t now_ms) {
    if (succeeded_at_ms_ && (now_ms - *succeeded_at_ms_ >= stable_ms_)) {
        failures_ = 0;
    }
    if (failures_ == 0) {
        delay_ms_ = base_ms_;
        succeeded_at_ms_.reset();
    } else {
        delay_ms_ = (delay_ms_ > max_ms_ / 2) ? max_ms_ : delay_ms_ * 2;
    }
    failed_at_ms_ = now_ms;
    ++failures_;
}

void RetryTimer::succeeded(uint32_t now_ms) { succeeded_at_ms_ = now_ms; }
