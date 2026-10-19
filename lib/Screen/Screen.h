#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "SnapshotStore.h"

constexpr std::size_t kLineCapacity = 22;
struct TextItem {
    std::array<char, kLineCapacity> text{};
    int16_t x = 0;
    int16_t y = 0;
    uint8_t size = 1;

    friend bool operator==(const TextItem&, const TextItem&) = default;
};

struct Screen {
    TextItem climate;
    TextItem state;
    TextItem network;

    friend bool operator==(const Screen&, const Screen&) = default;
};

[[nodiscard]] Screen buildScreen(
    const Snapshot& snapshot,
    bool sensor_stale,
    bool wifi_up,
    bool tcp_up,
    uint32_t now_ms
);
