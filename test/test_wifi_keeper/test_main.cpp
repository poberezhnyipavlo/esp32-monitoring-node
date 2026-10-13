#include <gtest/gtest.h>

#include <cstdint>

#include "WifiKeeper.h"

namespace {
constexpr uint32_t kTimeoutMs = 15000;
constexpr uint32_t kBaseMs = 1000;
constexpr uint32_t kMaxMs = 30000;

WifiKeeper makeKeeper() { return WifiKeeper(kTimeoutMs, kBaseMs, kMaxMs); }

void connectAt(WifiKeeper& k, uint32_t t) {
    ASSERT_EQ(k.update(false, t), WifiAction::kConnect);
    ASSERT_EQ(k.update(true, t + 100), WifiAction::kConnected);
}
}  // namespace

TEST(WifiKeeper, FirstUpdateStartsConnecting) {
    WifiKeeper k = makeKeeper();
    EXPECT_EQ(k.update(false, 0), WifiAction::kConnect);
    EXPECT_EQ(k.attempt(), 1U);
    EXPECT_FALSE(k.isConnected());
}

TEST(WifiKeeper, ConnectedIsReportedOnce) {
    WifiKeeper k = makeKeeper();
    ASSERT_EQ(k.update(false, 0), WifiAction::kConnect);
    EXPECT_EQ(k.update(false, 500), WifiAction::kNone);
    EXPECT_EQ(k.update(true, 800), WifiAction::kConnected);
    EXPECT_TRUE(k.isConnected());
    EXPECT_EQ(k.update(true, 900), WifiAction::kNone);
    EXPECT_EQ(k.attempt(), 0U);
}

TEST(WifiKeeper, TimeoutExactlyAtConnectTimeout) {
    WifiKeeper k = makeKeeper();
    ASSERT_EQ(k.update(false, 0), WifiAction::kConnect);
    EXPECT_EQ(k.update(false, kTimeoutMs - 1), WifiAction::kNone);
    EXPECT_EQ(k.update(false, kTimeoutMs), WifiAction::kTimeout);
    EXPECT_EQ(k.backoffMs(), kBaseMs);
}

TEST(WifiKeeper, RetriesWithExponentialBackoff) {
    WifiKeeper k = makeKeeper();
    uint32_t t = 0;
    ASSERT_EQ(k.update(false, t), WifiAction::kConnect);
    uint32_t expected_wait = kBaseMs;
    for (uint32_t attempt = 2; attempt <= 4; ++attempt) {
        t += kTimeoutMs;
        ASSERT_EQ(k.update(false, t), WifiAction::kTimeout);
        ASSERT_EQ(k.backoffMs(), expected_wait);
        EXPECT_EQ(k.update(false, t + expected_wait - 1), WifiAction::kNone);
        t += expected_wait;
        EXPECT_EQ(k.update(false, t), WifiAction::kConnect);
        EXPECT_EQ(k.attempt(), attempt);
        expected_wait *= 2;
    }
}

TEST(WifiKeeper, BackoffIsCappedAtMax) {
    WifiKeeper k = makeKeeper();
    uint32_t t = 0;
    ASSERT_EQ(k.update(false, t), WifiAction::kConnect);
    for (int i = 0; i < 20; ++i) {
        t += kTimeoutMs;
        ASSERT_EQ(k.update(false, t), WifiAction::kTimeout);
        ASSERT_LE(k.backoffMs(), kMaxMs);
        t += k.backoffMs();
        ASSERT_EQ(k.update(false, t), WifiAction::kConnect);
    }
    EXPECT_EQ(k.backoffMs(), kMaxMs);
}

TEST(WifiKeeper, LostConnectionWaitsBaseThenReconnects) {
    WifiKeeper k = makeKeeper();
    connectAt(k, 0);
    EXPECT_EQ(k.update(false, 5000), WifiAction::kLost);
    EXPECT_FALSE(k.isConnected());
    EXPECT_EQ(k.backoffMs(), kBaseMs);
    EXPECT_EQ(k.update(false, 5000 + kBaseMs - 1), WifiAction::kNone);
    EXPECT_EQ(k.update(false, 5000 + kBaseMs), WifiAction::kConnect);
    EXPECT_EQ(k.attempt(), 1U);
}

TEST(WifiKeeper, SuccessResetsBackoff) {
    WifiKeeper k = makeKeeper();
    uint32_t t = 0;
    ASSERT_EQ(k.update(false, t), WifiAction::kConnect);
    for (int i = 0; i < 3; ++i) {
        t += kTimeoutMs;
        ASSERT_EQ(k.update(false, t), WifiAction::kTimeout);
        t += k.backoffMs();
        ASSERT_EQ(k.update(false, t), WifiAction::kConnect);
    }
    ASSERT_GT(k.backoffMs(), kBaseMs);
    EXPECT_EQ(k.update(true, t + 10), WifiAction::kConnected);
    EXPECT_EQ(k.backoffMs(), kBaseMs);
}

TEST(WifiKeeper, ConnectionDuringWaitingIsAccepted) {
    WifiKeeper k = makeKeeper();
    ASSERT_EQ(k.update(false, 0), WifiAction::kConnect);
    ASSERT_EQ(k.update(false, kTimeoutMs), WifiAction::kTimeout);
    EXPECT_EQ(k.update(true, kTimeoutMs + 10), WifiAction::kConnected);
}

TEST(WifiKeeper, WorksAcrossMillisOverflow) {
    constexpr uint32_t kStart = 0xFFFFFFFF - 5000;
    WifiKeeper k = makeKeeper();
    ASSERT_EQ(k.update(false, kStart), WifiAction::kConnect);
    EXPECT_EQ(k.update(false, kStart + kTimeoutMs - 1), WifiAction::kNone);
    EXPECT_EQ(k.update(false, kStart + kTimeoutMs), WifiAction::kTimeout);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
