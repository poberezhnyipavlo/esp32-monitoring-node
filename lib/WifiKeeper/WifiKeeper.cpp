#include "WifiKeeper.h"

#include <cstdint>

WifiKeeper::WifiKeeper(
    uint32_t connect_timeout_ms,
    uint32_t backoff_base_ms,
    uint32_t backoff_max_ms
)
    : connect_timeout_ms_(connect_timeout_ms),
      backoff_base_ms_(backoff_base_ms),
      backoff_max_ms_(backoff_max_ms),
      backoff_ms_(backoff_base_ms) {}

void WifiKeeper::startWaiting(uint32_t now_ms) {
    state_ = State::Waiting;
    since_ms_ = now_ms;
}

void WifiKeeper::growBackoff() {
    backoff_ms_ = (backoff_ms_ > backoff_max_ms_ / 2) ? backoff_max_ms_ : backoff_ms_ * 2;
}

WifiAction WifiKeeper::update(bool link_up, uint32_t now_ms) {
    switch (state_) {
        case State::Start:
            state_ = State::Connecting;
            since_ms_ = now_ms;
            attempt_ = 1;
            return WifiAction::Connect;

        case State::Connecting:
        case State::Waiting:
            if (link_up) {
                state_ = State::Connected;
                attempt_ = 0;
                backoff_ms_ = backoff_base_ms_;
                return WifiAction::Connected;
            }
            if (state_ == State::Connecting) {
                if (now_ms - since_ms_ < connect_timeout_ms_) {
                    return WifiAction::None;
                }
                startWaiting(now_ms);
                return WifiAction::Timeout;
            }
            if (now_ms - since_ms_ < backoff_ms_) {
                return WifiAction::None;
            }
            growBackoff();
            state_ = State::Connecting;
            since_ms_ = now_ms;
            ++attempt_;
            return WifiAction::Connect;

        case State::Connected:
            if (link_up) {
                return WifiAction::None;
            }
            attempt_ = 0;
            backoff_ms_ = backoff_base_ms_;
            startWaiting(now_ms);
            return WifiAction::Lost;
    }
    return WifiAction::None;
}
