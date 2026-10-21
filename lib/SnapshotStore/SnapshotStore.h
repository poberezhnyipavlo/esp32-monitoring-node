#pragma once

#include <cstdint>
#include <mutex>
#include <optional>

#include "ClimateMeasured.h"

class SnapshotStore {
public:
    void write(const ClimateMeasured& climate_measured) {
        const std::scoped_lock lock(mutex_);
        last_ = climate_measured;
    }

    std::optional<ClimateMeasured> read() const {
        const std::scoped_lock lock(mutex_);
        return last_;
    }

private:
    mutable std::mutex mutex_;
    std::optional<ClimateMeasured> last_;
};
