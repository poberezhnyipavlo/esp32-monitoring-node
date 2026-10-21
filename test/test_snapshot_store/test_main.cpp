#include <gtest/gtest.h>

#include <atomic>
#include <cstdint>
#include <thread>
#include <optional>

#include "ClimateMeasured.h"
#include "SnapshotStore.h"
#include "TemperatureAlert.h"

TEST(SnapshotStore, StartsWithoutReading) {
    const SnapshotStore store;
    EXPECT_FALSE(store.read().has_value());
}

TEST(SnapshotStore, ReadReturnsLastWritten) {
    SnapshotStore store;
    store.write(
        ClimateMeasured{
            .temperature_c = 24.5F,
            .humidity_pct = 40.0F,
            .level = TemperatureLevel::kNormal,
            .sampled_at_ms = 2000,
        }
    );
    store.write(
        ClimateMeasured{
            .temperature_c = 27.4F,
            .humidity_pct = 41.0F,
            .level = TemperatureLevel::kTooHot,
            .sampled_at_ms = 4000,
        }
    );

    const std::optional<ClimateMeasured> climate_measured = store.read();
    EXPECT_TRUE(climate_measured.has_value());
    EXPECT_FLOAT_EQ(climate_measured->temperature_c, 27.4F);
    EXPECT_FLOAT_EQ(climate_measured->humidity_pct, 41.0F);
    EXPECT_EQ(climate_measured->level, TemperatureLevel::kTooHot);
    EXPECT_EQ(climate_measured->sampled_at_ms, 4000U);
}

TEST(SnapshotStore, ReaderNeverSeesHalfWrittenSnapshot) {
    constexpr uint32_t kWrites = 200000;
    SnapshotStore store;
    std::atomic<bool> done{false};

    std::thread writer([&store, &done] {
        for (uint32_t i = 1; i <= kWrites; ++i) {
            const auto value = static_cast<float>(i);
            store.write(
                ClimateMeasured{
                    .temperature_c = value,
                    .humidity_pct = value,
                    .level = TemperatureLevel::kNormal,
                    .sampled_at_ms = i,
                }
            );
        }
        done = true;
    });

    uint32_t torn = 0;
    while (!done) {
        const std::optional<ClimateMeasured> climate_measured = store.read();
        if (!climate_measured) {
            continue;
        }
        if (climate_measured->temperature_c != climate_measured->humidity_pct ||
            climate_measured->temperature_c !=
                static_cast<float>(climate_measured->sampled_at_ms)) {
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
