#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "ClimateMeasured.h"
#include "ClimateReadFailed.h"
#include "LogProtocol.h"
#include "Logger.h"
#include "SensorWatch.h"
#include "SharedQueue.h"
#include "TemperatureAlert.h"

namespace {

using logproto::Level;

constexpr std::size_t kCapacity = 8;
constexpr uint32_t kStaleMs = 10000;

ClimateMeasured measured(uint32_t at_ms) {
    return ClimateMeasured{
        .temperature_c = 24.0F,
        .humidity_pct = 40.0F,
        .level = TemperatureLevel::kNormal,
        .sampled_at_ms = at_ms,
    };
}

ClimateReadFailed failed(uint32_t at_ms) { return ClimateReadFailed{.sampled_at_ms = at_ms}; }

struct Fixture {
    SharedQueue<LogEntry, kCapacity> queue;
    Logger<kCapacity> logger{queue};
    SensorWatch<kCapacity> watch{logger, kStaleMs};

    LogEntry popOldest() {
        const auto entry = queue.front();
        EXPECT_TRUE(entry.has_value());
        queue.popFront(entry->id);
        return entry->item;
    }
};

}  // namespace

TEST(SensorWatch, NormalReadingsLogNothing) {
    Fixture f;
    f.watch.onClimateMeasured(measured(2000));
    f.watch.onClimateMeasured(measured(4000));
    EXPECT_EQ(f.queue.stats().size, 0U);
}

TEST(SensorWatch, ShortGapIsNotLost) {
    Fixture f;
    f.watch.onClimateMeasured(measured(2000));
    f.watch.onClimateReadFailed(failed(4000));
    f.watch.onClimateReadFailed(failed(2000 + kStaleMs));  // exactly the threshold: not yet
    EXPECT_EQ(f.queue.stats().size, 0U);
}

TEST(SensorWatch, LostOnceAfterThreshold) {
    Fixture f;
    f.watch.onClimateMeasured(measured(2000));
    f.watch.onClimateReadFailed(failed(2001 + kStaleMs));
    f.watch.onClimateReadFailed(failed(14000));
    f.watch.onClimateReadFailed(failed(16000));

    ASSERT_EQ(f.queue.stats().size, 1U);
    const LogEntry entry = f.popOldest();
    EXPECT_EQ(entry.level, Level::kWarn);
    EXPECT_EQ(std::string_view(entry.src.data()), "dht");
    EXPECT_EQ(std::string_view(entry.text.data()), "sensor stopped answering");
    EXPECT_EQ(entry.sampled_at_ms, 2001 + kStaleMs);
}

TEST(SensorWatch, BackAfterLostTellsHowLongItWasSilent) {
    Fixture f;
    f.watch.onClimateMeasured(measured(2000));
    f.watch.onClimateReadFailed(failed(14000));
    f.watch.onClimateMeasured(measured(25000));

    ASSERT_EQ(f.queue.stats().size, 2U);
    f.popOldest();
    const LogEntry back = f.popOldest();
    EXPECT_EQ(back.level, Level::kInfo);
    EXPECT_EQ(std::string_view(back.text.data()), "sensor answers again after 23 s");
}

TEST(SensorWatch, MissingFromBootIsLostAfterThreshold) {
    Fixture f;
    f.watch.onClimateReadFailed(failed(2000));
    f.watch.onClimateReadFailed(failed(kStaleMs));
    EXPECT_EQ(f.queue.stats().size, 0U);
    f.watch.onClimateReadFailed(failed(kStaleMs + 2000));
    EXPECT_EQ(f.queue.stats().size, 1U);
}

TEST(SensorWatch, CanBeLostAgainAfterComingBack) {
    Fixture f;
    f.watch.onClimateMeasured(measured(2000));
    f.watch.onClimateReadFailed(failed(14000));   // lost
    f.watch.onClimateMeasured(measured(16000));   // back
    f.watch.onClimateReadFailed(failed(28000));   // lost again
    EXPECT_EQ(f.queue.stats().size, 3U);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
