#pragma once

#include <cstdint>
#include <mutex>

#include "TemperatureAlert.h"

struct Snapshot {
    bool has_reading = false;
    float temperature_c = 0.0F;
    float humidity_pct = 0.0F;
    TemperatureLevel level = TemperatureLevel::kNormal;
    uint32_t last_ok_ms = 0;
};

class SnapshotStore {
public:
    void write(const Snapshot& snapshot) {
        const std::scoped_lock lock(mutex_);
        snapshot_ = snapshot;
    }

    [[nodiscard]] Snapshot read() const {
        const std::scoped_lock lock(mutex_);
        return snapshot_;
    }

private:
    mutable std::mutex mutex_;
    Snapshot snapshot_;
};
