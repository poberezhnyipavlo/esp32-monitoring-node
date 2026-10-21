#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "ClimateMeasured.h"
#include "ClimateReadFailed.h"
#include "QueueListener.h"
#include "SampleQueue.h"
#include "TemperatureAlert.h"

namespace {
constexpr std::size_t kCapacity = 16;

Sample popOldest(SampleQueue<kCapacity>& queue) {
    const auto entry = queue.front();
    EXPECT_TRUE(entry.has_value());
    queue.popFront(entry->id);
    return entry->item;
}

ClimateMeasured measured(TemperatureLevel level, uint32_t sampled_at_ms) {
    return ClimateMeasured{
        .temperature_c = 24.5F,
        .humidity_pct = 40.0F,
        .level = level,
        .sampled_at_ms = sampled_at_ms,
    };
}

void expectSample(const Sample& sample, std::string_view name, double value, uint32_t seq) {
    EXPECT_EQ(sample.name, name);
    EXPECT_DOUBLE_EQ(sample.value, value) << name;
    EXPECT_EQ(sample.seq, seq) << name;
    EXPECT_EQ(sample.sampled_at_ms, 2000U) << name;
}
}  // namespace

TEST(QueueListener, MeasurementBecomesFourMetricsInOrder) {
    SampleQueue<kCapacity> queue;
    QueueListener<kCapacity> listener(queue);

    listener.onClimateMeasured(measured(TemperatureLevel::kNormal, 2000));

    ASSERT_EQ(queue.stats().size, 4U);
    expectSample(popOldest(queue), "temperature_c", 24.5, 0);
    expectSample(popOldest(queue), "humidity_pct", 40.0, 1);
    expectSample(popOldest(queue), "temp_alert", 0, 2);
    expectSample(popOldest(queue), "dht_ok", 1, 3);
}

TEST(QueueListener, TempAlertFollowsTheLevel) {
    EXPECT_DOUBLE_EQ(toTempAlert(TemperatureLevel::kTooCold), -1);
    EXPECT_DOUBLE_EQ(toTempAlert(TemperatureLevel::kNormal), 0);
    EXPECT_DOUBLE_EQ(toTempAlert(TemperatureLevel::kTooHot), 1);
}

TEST(QueueListener, HotMeasurementSendsAlertOne) {
    SampleQueue<kCapacity> queue;
    QueueListener<kCapacity> listener(queue);

    listener.onClimateMeasured(measured(TemperatureLevel::kTooHot, 2000));

    popOldest(queue);
    popOldest(queue);
    expectSample(popOldest(queue), "temp_alert", 1, 2);
}

TEST(QueueListener, FailedReadSendsOnlyDhtOkZero) {
    SampleQueue<kCapacity> queue;
    QueueListener<kCapacity> listener(queue);

    listener.onClimateReadFailed(ClimateReadFailed{.sampled_at_ms = 2000});

    ASSERT_EQ(queue.stats().size, 1U);
    expectSample(popOldest(queue), "dht_ok", 0, 0);
}

TEST(QueueListener, SequenceContinuesAcrossEventKinds) {
    SampleQueue<kCapacity> queue;
    QueueListener<kCapacity> listener(queue);

    listener.onClimateMeasured(measured(TemperatureLevel::kNormal, 2000));
    listener.onClimateReadFailed(ClimateReadFailed{.sampled_at_ms = 4000});
    listener.onClimateMeasured(measured(TemperatureLevel::kNormal, 6000));

    ASSERT_EQ(queue.stats().size, 9U);
    for (uint32_t expected_seq = 0; expected_seq < 9; ++expected_seq) {
        EXPECT_EQ(popOldest(queue).seq, expected_seq);
    }
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
