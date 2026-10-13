#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

template <typename T, size_t N>
class RingBuffer {
    static_assert(N > 0, "RingBuffer must hold at least one item");

public:
    struct Entry {
        uint32_t id;
        T item;
    };

    bool push(const T& item) {
        bool lost = false;
        if (count_ == N) {
            head_ = (head_ + 1) % N;
            ++first_id_;
            --count_;
            ++dropped_;
            lost = true;
        }
        // Index is always taken modulo N, so it is in range.
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
        items_[(head_ + count_) % N] = item;
        ++count_;

        return !lost;
    }

    [[nodiscard]] std::optional<Entry> front() const {
        if (count_ == 0) {
            return std::nullopt;
        }

        // head_ < N is a class invariant (always advanced modulo N).
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
        return Entry{first_id_, items_[head_]};
    }

    bool popFront(uint32_t id) {
        if (count_ == 0 || id != first_id_) {
            return false;
        }
        // Index is always taken modulo N, so it is in range.
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
        head_ = (head_ + 1) % N;
        ++first_id_;
        --count_;
        return true;
    }

    [[nodiscard]] size_t size() const { return count_; }
    [[nodiscard]] bool empty() const { return count_ == 0; }
    [[nodiscard]] uint32_t dropped() const { return dropped_; }
    [[nodiscard]] static constexpr size_t capacity() { return N; }

private:
    std::array<T, N> items_{};
    size_t head_ = 0;
    size_t count_ = 0;
    uint32_t first_id_ = 0;
    uint32_t dropped_ = 0;
};
