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
    state_ = State::kWaiting;
    since_ms_ = now_ms;
}

void WifiKeeper::growBackoff() {
    backoff_ms_ = (backoff_ms_ > backoff_max_ms_ / 2) ? backoff_max_ms_ : backoff_ms_ * 2;
}

WifiAction WifiKeeper::update(bool link_up, uint32_t now_ms) {
    switch (state_) {
        case State::kStart:
            state_ = State::kConnecting;
            since_ms_ = now_ms;
            attempt_ = 1;
            return WifiAction::kConnect;

        case State::kConnecting:
        case State::kWaiting:
            if (link_up) {
                state_ = State::kConnected;
                attempt_ = 0;
                backoff_ms_ = backoff_base_ms_;
                return WifiAction::kConnected;
            }
            if (state_ == State::kConnecting) {
                if (now_ms - since_ms_ < connect_timeout_ms_) {
                    return WifiAction::kNone;
                }
                startWaiting(now_ms);
                return WifiAction::kTimeout;
            }
            if (now_ms - since_ms_ < backoff_ms_) {
                return WifiAction::kNone;
            }
            growBackoff();
            state_ = State::kConnecting;
            since_ms_ = now_ms;
            ++attempt_;
            return WifiAction::kConnect;

        case State::kConnected:
            if (link_up) {
                return WifiAction::kNone;
            }
            attempt_ = 0;
            backoff_ms_ = backoff_base_ms_;
            startWaiting(now_ms);
            return WifiAction::kLost;
    }
    return WifiAction::kNone;
}

int  badly_formatted( ) {return 1;}
