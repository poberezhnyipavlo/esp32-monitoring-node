#include <gtest/gtest.h>

#include "RingBuffer.h"

TEST(RingBuffer, StartsEmpty) {
    const RingBuffer<int, 3> b;
    EXPECT_TRUE(b.empty());
    EXPECT_EQ(b.size(), 0U);
    EXPECT_FALSE(b.front().has_value());
    EXPECT_EQ(b.capacity(), 3U);
}

TEST(RingBuffer, FifoOrderWithIds) {
    RingBuffer<int, 3> b;
    EXPECT_TRUE(b.push(10));
    EXPECT_TRUE(b.push(20));

    auto e = b.front();
    ASSERT_TRUE(e.has_value());
    EXPECT_EQ(e->id, 0U);
    EXPECT_EQ(e->item, 10);
    EXPECT_EQ(b.size(), 2U);

    EXPECT_TRUE(b.popFront(e->id));
    e = b.front();
    ASSERT_TRUE(e.has_value());
    EXPECT_EQ(e->id, 1U);
    EXPECT_EQ(e->item, 20);
}

TEST(RingBuffer, OverflowDropsOldest) {
    RingBuffer<int, 3> b;
    EXPECT_TRUE(b.push(1));
    EXPECT_TRUE(b.push(2));
    EXPECT_TRUE(b.push(3));
    EXPECT_FALSE(b.push(4));
    EXPECT_FALSE(b.push(5));
    EXPECT_EQ(b.size(), 3U);
    EXPECT_EQ(b.dropped(), 2U);

    const auto e = b.front();
    ASSERT_TRUE(e.has_value());
    EXPECT_EQ(e->item, 3);
    EXPECT_EQ(e->id, 2U);
}

TEST(RingBuffer, PopWithStaleIdDoesNothing) {
    RingBuffer<int, 2> b;
    b.push(1);
    const auto taken = b.front();
    b.push(2);
    b.push(3);
    EXPECT_FALSE(b.popFront(taken->id));
    EXPECT_EQ(b.size(), 2U);
    EXPECT_EQ(b.front()->item, 2);
}

TEST(RingBuffer, WrapsAroundManyTimes) {
    RingBuffer<int, 4> b;
    for (int i = 0; i < 1000; ++i) {
        b.push(i);
        if (i % 3 == 0) {
            b.popFront(b.front()->id);
        }
    }
    int prev = -1;
    while (const auto e = b.front()) {
        EXPECT_GT(e->item, prev);
        prev = e->item;
        b.popFront(e->id);
    }
    EXPECT_EQ(prev, 999);
    EXPECT_TRUE(b.empty());
}

TEST(RingBuffer, PopOnEmpty) {
    RingBuffer<int, 2> b;
    EXPECT_FALSE(b.popFront(0));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
