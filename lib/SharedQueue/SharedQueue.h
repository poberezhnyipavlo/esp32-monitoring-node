#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>

#include "RingBuffer.h"

template <typename  T, std::size_t N>
class SharedQueue {
public:
    using Entry = typename RingBuffer<T, N>::Entry;

    struct Stats {
        std::size_t size;
        uint32_t dropped;
    };

    bool push(const T& item) {
        const std::scoped_lock lock(mutex_);
        return buffer_.push(item);
    }

    [[nodiscard]] std::optional<Entry> front() const {
        const std::scoped_lock lock(mutex_);
        return buffer_.front();
    }

    bool popFront(uint32_t id) {
        const std::scoped_lock lock(mutex_);
        return buffer_.popFront(id);
    }

    [[nodiscard]] Stats stats() const {
        const std::scoped_lock lock(mutex_);
        return Stats{buffer_.size(), buffer_.dropped()};
    }

    [[nodiscard]] static  constexpr  std::size_t capacity() { return N; }
private:
    mutable std::mutex mutex_;
    RingBuffer<T, N> buffer_;
};
