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

class Fixture {
public:
    void measuredAt(uint32_t at_ms) { watch_.onClimateMeasured(measured(at_ms)); }
    void failedAt(uint32_t at_ms) { watch_.onClimateReadFailed(failed(at_ms)); }
    [[nodiscard]] std::size_t size() const { return queue_.stats().size; }

    LogEntry popOldest() {
        const auto entry = queue_.front();
        EXPECT_TRUE(entry.has_value());
        queue_.popFront(entry->id);
        return entry->item;
    }

private:
    SharedQueue<LogEntry, kCapacity> queue_;
    Logger<kCapacity> logger_{queue_};
    SensorWatch<kCapacity> watch_{logger_, kStaleMs};
};

}  // namespace

TEST(SensorWatch, NormalReadingsLogNothing) {
    Fixture f;
    f.measuredAt(2000);
    f.measuredAt(4000);
    EXPECT_EQ(f.size(), 0U);
}

TEST(SensorWatch, ShortGapIsNotLost) {
    Fixture f;
    f.measuredAt(2000);
    f.failedAt(4000);
    f.failedAt(2000 + kStaleMs);  // exactly the threshold: not yet
    EXPECT_EQ(f.size(), 0U);
}

TEST(SensorWatch, LostOnceAfterThreshold) {
    Fixture f;
    f.measuredAt(2000);
    f.failedAt(2001 + kStaleMs);
    f.failedAt(14000);
    f.failedAt(16000);

    ASSERT_EQ(f.size(), 1U);
    const LogEntry entry = f.popOldest();
    EXPECT_EQ(entry.level, Level::kWarn);
    EXPECT_EQ(std::string_view(entry.src.data()), "dht");
    EXPECT_EQ(std::string_view(entry.text.data()), "sensor stopped answering");
    EXPECT_EQ(entry.sampled_at_ms, 2001 + kStaleMs);
}

TEST(SensorWatch, BackAfterLostTellsHowLongItWasSilent) {
    Fixture f;
    f.measuredAt(2000);
    f.failedAt(14000);
    f.measuredAt(25000);

    ASSERT_EQ(f.size(), 2U);
    f.popOldest();
    const LogEntry back = f.popOldest();
    EXPECT_EQ(back.level, Level::kInfo);
    EXPECT_EQ(std::string_view(back.text.data()), "sensor answers again after 23 s");
}

TEST(SensorWatch, MissingFromBootIsLostAfterThreshold) {
    Fixture f;
    f.failedAt(2000);
    f.failedAt(kStaleMs);
    EXPECT_EQ(f.size(), 0U);
    f.failedAt(kStaleMs + 2000);
    EXPECT_EQ(f.size(), 1U);
}

TEST(SensorWatch, CanBeLostAgainAfterComingBack) {
    Fixture f;
    f.measuredAt(2000);
    f.failedAt(14000);    // lost
    f.measuredAt(16000);  // back
    f.failedAt(28000);    // lost again
    EXPECT_EQ(f.size(), 3U);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
