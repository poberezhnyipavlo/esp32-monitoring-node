#include <gtest/gtest.h>

#include "WifiKeeper.h"

namespace {
constexpr uint32_t k_timeout_ms = 15000;
constexpr uint32_t k_base_ms = 1000;
constexpr uint32_t k_max_ms = 30000;

WifiKeeper makeKeeper() { return WifiKeeper(k_timeout_ms, k_base_ms, k_max_ms); }

void connectAt(WifiKeeper& k, uint32_t t) {
    ASSERT_EQ(k.update(false, t), WifiAction::Connect);
    ASSERT_EQ(k.update(true, t + 100), WifiAction::Connected);
}
}  // namespace

TEST(WifiKeeper, FirstUpdateStartsConnecting) {
    WifiKeeper k = makeKeeper();
    EXPECT_EQ(k.update(false, 0), WifiAction::Connect);
    EXPECT_EQ(k.attempt(), 1U);
    EXPECT_FALSE(k.isConnected());
}

TEST(WifiKeeper, ConnectedIsReportedOnce) {
    WifiKeeper k = makeKeeper();
    ASSERT_EQ(k.update(false, 0), WifiAction::Connect);
    EXPECT_EQ(k.update(false, 500), WifiAction::None);
    EXPECT_EQ(k.update(true, 800), WifiAction::Connected);
    EXPECT_TRUE(k.isConnected());
    EXPECT_EQ(k.update(true, 900), WifiAction::None);
    EXPECT_EQ(k.attempt(), 0U);
}

TEST(WifiKeeper, TimeoutExactlyAtConnectTimeout) {
    WifiKeeper k = makeKeeper();
    ASSERT_EQ(k.update(false, 0), WifiAction::Connect);
    EXPECT_EQ(k.update(false, k_timeout_ms - 1), WifiAction::None);
    EXPECT_EQ(k.update(false, k_timeout_ms), WifiAction::Timeout);
    EXPECT_EQ(k.backoffMs(), k_base_ms);
}

TEST(WifiKeeper, RetriesWithExponentialBackoff) {
    WifiKeeper k = makeKeeper();
    uint32_t t = 0;
    ASSERT_EQ(k.update(false, t), WifiAction::Connect);
    uint32_t expected_wait = k_base_ms;
    for (uint32_t attempt = 2; attempt <= 4; ++attempt) {
        t += k_timeout_ms;
        ASSERT_EQ(k.update(false, t), WifiAction::Timeout);
        ASSERT_EQ(k.backoffMs(), expected_wait);
        EXPECT_EQ(k.update(false, t + expected_wait - 1), WifiAction::None);
        t += expected_wait;
        EXPECT_EQ(k.update(false, t), WifiAction::Connect);
        EXPECT_EQ(k.attempt(), attempt);
        expected_wait *= 2;
    }
}

TEST(WifiKeeper, BackoffIsCappedAtMax) {
    WifiKeeper k = makeKeeper();
    uint32_t t = 0;
    ASSERT_EQ(k.update(false, t), WifiAction::Connect);
    for (int i = 0; i < 20; ++i) {
        t += k_timeout_ms;
        ASSERT_EQ(k.update(false, t), WifiAction::Timeout);
        ASSERT_LE(k.backoffMs(), k_max_ms);
        t += k.backoffMs();
        ASSERT_EQ(k.update(false, t), WifiAction::Connect);
    }
    EXPECT_EQ(k.backoffMs(), k_max_ms);
}

TEST(WifiKeeper, LostConnectionWaitsBaseThenReconnects) {
    WifiKeeper k = makeKeeper();
    connectAt(k, 0);
    EXPECT_EQ(k.update(false, 5000), WifiAction::Lost);
    EXPECT_FALSE(k.isConnected());
    EXPECT_EQ(k.backoffMs(), k_base_ms);
    EXPECT_EQ(k.update(false, 5000 + k_base_ms - 1), WifiAction::None);
    EXPECT_EQ(k.update(false, 5000 + k_base_ms), WifiAction::Connect);
    EXPECT_EQ(k.attempt(), 1U);
}

TEST(WifiKeeper, SuccessResetsBackoff) {
    WifiKeeper k = makeKeeper();
    uint32_t t = 0;
    ASSERT_EQ(k.update(false, t), WifiAction::Connect);
    for (int i = 0; i < 3; ++i) {
        t += k_timeout_ms;
        ASSERT_EQ(k.update(false, t), WifiAction::Timeout);
        t += k.backoffMs();
        ASSERT_EQ(k.update(false, t), WifiAction::Connect);
    }
    ASSERT_GT(k.backoffMs(), k_base_ms);
    EXPECT_EQ(k.update(true, t + 10), WifiAction::Connected);
    EXPECT_EQ(k.backoffMs(), k_base_ms);
}

TEST(WifiKeeper, ConnectionDuringWaitingIsAccepted) {
    WifiKeeper k = makeKeeper();
    ASSERT_EQ(k.update(false, 0), WifiAction::Connect);
    ASSERT_EQ(k.update(false, k_timeout_ms), WifiAction::Timeout);
    EXPECT_EQ(k.update(true, k_timeout_ms + 10), WifiAction::Connected);
}

TEST(WifiKeeper, WorksAcrossMillisOverflow) {
    constexpr uint32_t start = 0xFFFFFFFF - 5000;
    WifiKeeper k = makeKeeper();
    ASSERT_EQ(k.update(false, start), WifiAction::Connect);
    EXPECT_EQ(k.update(false, start + k_timeout_ms - 1), WifiAction::None);
    EXPECT_EQ(k.update(false, start + k_timeout_ms), WifiAction::Timeout);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
