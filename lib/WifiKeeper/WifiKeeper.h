#pragma once

#include <cstdint>

enum class WifiAction : uint8_t { kNone, kConnect, kConnected, kTimeout, kLost };

class WifiKeeper {
public:
    WifiKeeper(uint32_t connect_timeout_ms, uint32_t backoff_base_ms, uint32_t backoff_max_ms);

    [[nodiscard]] WifiAction update(bool link_up, uint32_t now_ms);

    [[nodiscard]] bool isConnected() const { return state_ == State::kConnected; }
    [[nodiscard]] uint32_t attempt() const { return attempt_; }
    [[nodiscard]] uint32_t backoffMs() const { return backoff_ms_; }

private:
    enum class State : uint8_t { kStart, kConnecting, kConnected, kWaiting };

    void startWaiting(uint32_t now_ms);
    void growBackoff();

    uint32_t connect_timeout_ms_;
    uint32_t backoff_base_ms_;
    uint32_t backoff_max_ms_;
    State state_ = State::kStart;
    uint32_t since_ms_ = 0;
    uint32_t attempt_ = 0;
    uint32_t backoff_ms_;
};
