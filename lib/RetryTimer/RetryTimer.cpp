#include "RetryTimer.h"

#include <cstdint>

RetryTimer::RetryTimer(uint32_t base_ms, uint32_t max_ms) : base_ms_(base_ms), max_ms_(max_ms) {}

bool RetryTimer::due(uint32_t now_ms) const {
    if (failures_ == 0) {
        return true;
    }

    return now_ms - failed_at_ms_ >= delay_ms_;
}

void RetryTimer::failed(uint32_t now_ms) {
    if (failures_ == 0) {
        delay_ms_ = base_ms_;
    } else {
        delay_ms_ = (delay_ms_ > max_ms_ / 2) ? max_ms_ : delay_ms_ * 2;
    }
    failed_at_ms_ = now_ms;
    ++failures_;
}

void RetryTimer::succeeded() {
    failures_ = 0;
    delay_ms_ = 0;
}
