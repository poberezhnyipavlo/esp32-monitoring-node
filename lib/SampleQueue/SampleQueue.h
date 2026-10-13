#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string_view>

#include "RingBuffer.h"

struct Sample {
  std::string_view name;
  double value;
  uint32_t seq;
  uint32_t sampled_at_ms;
};

template <size_t N>
class SampleQueue {
public:
  using Entry = typename RingBuffer<Sample, N>::Entry;

  struct Stats {
    size_t size;
    uint32_t dropped;
  };

  bool push(const Sample& s) {
    const std::lock_guard<std::mutex> lock(mutex_);
    return buffer_.push(s);
  }

  [[nodiscard]] std::optional<Entry> front() const {
    const std::lock_guard<std::mutex> lock(mutex_);
    return buffer_.front();
  }

  bool popFront(uint32_t id) {
    const std::lock_guard<std::mutex> lock(mutex_);
    return buffer_.popFront(id);
  }

  [[nodiscard]] Stats stats() const {
    const std::lock_guard<std::mutex> lock(mutex_);
    return Stats{buffer_.size(), buffer_.dropped()};
  }

  [[nodiscard]] static constexpr size_t capacity() { return N; }

private:
  mutable std::mutex mutex_;
  RingBuffer<Sample, N> buffer_;
};
