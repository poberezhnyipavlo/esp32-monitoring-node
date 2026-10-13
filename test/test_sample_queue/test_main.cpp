#include <gtest/gtest.h>

#include <atomic>
#include <cstdint>
#include <thread>

#include "SampleQueue.h"

TEST(SampleQueue, PushFrontPop) {
    SampleQueue<4> q;
    q.push(Sample{"uptime_s", 12, 0, 1000});
    const auto e = q.front();
    ASSERT_TRUE(e.has_value());
    EXPECT_EQ(e->item.name, "uptime_s");
    EXPECT_EQ(e->item.seq, 0U);
    EXPECT_TRUE(q.popFront(e->id));
    EXPECT_EQ(q.stats().size, 0U);
}

TEST(SampleQueue, ProducerConsumerKeepsOrderAndCounts) {
    constexpr uint32_t k_total = 200000;
    SampleQueue<64> q;
    std::atomic<bool> done{false};

    std::thread producer([&] {
        for (uint32_t i = 0; i < k_total; ++i) {
            q.push(Sample{"x", 0, i, 0});
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
            ordered = ordered && static_cast<int64_t>(e->item.seq) > last;
            last = e->item.seq;
            ++received;
        }
    }
    producer.join();

    EXPECT_TRUE(ordered);
    EXPECT_EQ(received + q.stats().dropped, k_total);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
