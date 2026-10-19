#include <gtest/gtest.h>

#include <atomic>
#include <cstdint>
#include <thread>
#include <utility>

#include "SampleQueue.h"

TEST(SampleQueue, PushFrontPop) {
    SampleQueue<4> q;
    q.push(Sample{.name = "uptime_s", .value = 12, .seq = 0, .sampled_at_ms = 1000});
    const auto e = q.front();
    ASSERT_TRUE(e.has_value());
    EXPECT_EQ(e->item.name, "uptime_s");
    EXPECT_EQ(e->item.seq, 0U);
    EXPECT_TRUE(q.popFront(e->id));
    EXPECT_EQ(q.stats().size, 0U);
}

TEST(SampleQueue, ProducerConsumerKeepsOrderAndCounts) {
    constexpr uint32_t kTotal = 200000;
    SampleQueue<64> q;
    std::atomic<bool> done{false};

    std::thread producer([&] {
        for (uint32_t i = 0; i < kTotal; ++i) {
            q.push(Sample{.name = "x", .value = 0, .seq = i, .sampled_at_ms = 0});
        }
        done = true;
    });

    uint32_t received = 0;
    int64_t last = -1;
    bool ordered = true;
    while (true) {
        const auto e = q.front();
        if (!e) {
            if (done) {
                break;
            }
            std::this_thread::yield();
            continue;
        }
        if (q.popFront(e->id)) {
            ordered = ordered && std::cmp_greater(e->item.seq, last);
            last = e->item.seq;
            ++received;
        }
    }
    producer.join();

    EXPECT_TRUE(ordered);
    EXPECT_EQ(received + q.stats().dropped, kTotal);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
