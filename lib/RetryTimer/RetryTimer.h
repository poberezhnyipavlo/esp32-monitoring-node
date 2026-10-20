#pragma once

#include <cstdint>
#include <optional>

class RetryTimer {
public:
    RetryTimer(uint32_t base_ms, uint32_t max_ms, uint32_t stable_ms);

    [[nodiscard]] bool due(uint32_t now_ms) const;

    void failed(uint32_t now_ms);
    void succeeded(uint32_t now_ms);

    [[nodiscard]] uint32_t delayMs() const { return delay_ms_; }

    [[nodiscard]] uint32_t failures() const { return failures_; }

private:
    uint32_t base_ms_;
    uint32_t max_ms_;
    uint32_t delay_ms_{};
    uint32_t failed_at_ms_{};
    uint32_t failures_ = 0;
    uint32_t stable_ms_ = 0;
    std::optional<uint32_t> succeeded_at_ms_;
};
