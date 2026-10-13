#include <gtest/gtest.h>

#include <cstdint>

#include "RetryTimer.h"

TEST(RetryTimer, DueBeforeAnyFailure) {
    const RetryTimer t(1000, 30000);
    EXPECT_TRUE(t.due(0));
    EXPECT_TRUE(t.due(123456));
    EXPECT_EQ(t.delayMs(), 0U);
    EXPECT_EQ(t.failures(), 0U);
}

TEST(RetryTimer, WaitsBaseAfterFirstFailure) {
    RetryTimer t(1000, 30000);
    t.failed(5000);
    EXPECT_EQ(t.delayMs(), 1000U);
    EXPECT_FALSE(t.due(5000));
    EXPECT_FALSE(t.due(5999));
    EXPECT_TRUE(t.due(6000));
}

TEST(RetryTimer, DoublesUpToMax) {
    RetryTimer t(1000, 30000);
    uint32_t now = 0;
    const uint32_t expected[] = {1000, 2000, 4000, 8000, 16000, 30000, 30000};
    for (const uint32_t delay : expected) {
        t.failed(now);
        EXPECT_EQ(t.delayMs(), delay);
        now += t.delayMs();
    }
    EXPECT_EQ(t.failures(), 7U);
}

TEST(RetryTimer, SuccessResets) {
    RetryTimer t(1000, 30000);
    t.failed(0);
    t.failed(1000);
    t.failed(3000);
    EXPECT_EQ(t.delayMs(), 4000U);
    t.succeeded();
    EXPECT_TRUE(t.due(3001));
    EXPECT_EQ(t.failures(), 0U);
    t.failed(10000);
    EXPECT_EQ(t.delayMs(), 1000U);
}

TEST(RetryTimer, WorksAcrossMillisOverflow) {
    RetryTimer t(1000, 30000);
    const uint32_t before_wrap = UINT32_MAX - 300;
    t.failed(before_wrap);
    EXPECT_FALSE(t.due(before_wrap + 500));
    EXPECT_TRUE(t.due(before_wrap + 1000));
}

TEST(RetryTimer, MaxNotMultipleOfBase) {
    RetryTimer t(3000, 10000);
    t.failed(0);
    EXPECT_EQ(t.delayMs(), 3000U);
    t.failed(0);
    EXPECT_EQ(t.delayMs(), 6000U);
    t.failed(0);
    EXPECT_EQ(t.delayMs(), 10000U);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
