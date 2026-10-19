#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>

#include "ClimateMeasured.h"
#include "QueueListener.h"
#include "SampleQueue.h"

namespace {
constexpr std::size_t kCapacity = 8;

Sample popOldest(SampleQueue<kCapacity>& queue) {
    const auto entry = queue.front();
    EXPECT_TRUE(entry.has_value());
    queue.popFront(entry->id);
    return entry->item;
}
}  // namespace

TEST(QueueListener, OneEventBecomesTemperatureThenHumidity) {
    SampleQueue<kCapacity> queue;
    QueueListener<kCapacity> listener(queue);

    listener.onClimateMeasured(
        ClimateMeasured{.temperature_c = 24.5F, .humidity_pct = 40.0F, .sampled_at_ms = 2000}
    );

    ASSERT_EQ(queue.stats().size, 2U);
    const Sample temperature = popOldest(queue);
    EXPECT_EQ(temperature.name, "temperature_c");
    EXPECT_DOUBLE_EQ(temperature.value, 24.5);
    EXPECT_EQ(temperature.seq, 0U);
    EXPECT_EQ(temperature.sampled_at_ms, 2000U);

    const Sample humidity = popOldest(queue);
    EXPECT_EQ(humidity.name, "humidity_pct");
    EXPECT_DOUBLE_EQ(humidity.value, 40.0);
    EXPECT_EQ(humidity.seq, 1U);
    EXPECT_EQ(humidity.sampled_at_ms, 2000U);
}

TEST(QueueListener, SequenceContinuesAcrossEvents) {
    SampleQueue<kCapacity> queue;
    QueueListener<kCapacity> listener(queue);

    listener.onClimateMeasured(
        ClimateMeasured{.temperature_c = 24.5F, .humidity_pct = 40.0F, .sampled_at_ms = 2000}
    );
    listener.onClimateMeasured(
        ClimateMeasured{.temperature_c = 25.0F, .humidity_pct = 41.0F, .sampled_at_ms = 4000}
    );

    ASSERT_EQ(queue.stats().size, 4U);
    for (uint32_t expected_seq = 0; expected_seq < 4; ++expected_seq) {
        EXPECT_EQ(popOldest(queue).seq, expected_seq);
    }
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
