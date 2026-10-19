#include <gtest/gtest.h>

#include <atomic>
#include <cstdint>
#include <thread>

#include "SnapshotStore.h"
#include "TemperatureAlert.h"

TEST(SnapshotStore, StartsWithoutReading) {
    const SnapshotStore store;
    EXPECT_FALSE(store.read().has_reading);
}

TEST(SnapshotStore, ReadReturnsLastWritten) {
    SnapshotStore store;
    store.write(
        Snapshot{
            .has_reading = true,
            .temperature_c = 24.5F,
            .humidity_pct = 40.0F,
            .level = TemperatureLevel::kNormal,
            .last_ok_ms = 2000
        }
    );
    store.write(
        Snapshot{
            .has_reading = true,
            .temperature_c = 27.4F,
            .humidity_pct = 41.0F,
            .level = TemperatureLevel::kTooHot,
            .last_ok_ms = 4000
        }
    );

    const Snapshot snapshot = store.read();
    EXPECT_TRUE(snapshot.has_reading);
    EXPECT_FLOAT_EQ(snapshot.temperature_c, 27.4F);
    EXPECT_FLOAT_EQ(snapshot.humidity_pct, 41.0F);
    EXPECT_EQ(snapshot.level, TemperatureLevel::kTooHot);
    EXPECT_EQ(snapshot.last_ok_ms, 4000U);
}

TEST(SnapshotStore, ReaderNeverSeesHalfWrittenSnapshot) {
    constexpr uint32_t kWrites = 200000;
    SnapshotStore store;
    std::atomic<bool> done{false};

    std::thread writer([&store, &done] {
        for (uint32_t i = 1; i <= kWrites; ++i) {
            const auto value = static_cast<float>(i);
            store.write(
                Snapshot{
                    .has_reading = true,
                    .temperature_c = value,
                    .humidity_pct = value,
                    .level = TemperatureLevel::kNormal,
                    .last_ok_ms = i
                }
            );
        }
        done = true;
    });

    uint32_t torn = 0;
    while (!done) {
        const Snapshot snapshot = store.read();
        if (snapshot.temperature_c != snapshot.humidity_pct ||
            snapshot.temperature_c != static_cast<float>(snapshot.last_ok_ms)) {
            ++torn;
        }
    }
    writer.join();
    EXPECT_EQ(torn, 0U);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
