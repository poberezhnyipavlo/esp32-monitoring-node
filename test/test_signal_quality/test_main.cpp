#include <gtest/gtest.h>

#include <cstdint>
#include <string_view>

#include "SignalQuality.h"

namespace {
std::string_view q(int32_t rssi) { return signalQuality(rssi); }
}  // namespace

TEST(SignalQuality, Boundaries) {
    EXPECT_EQ(q(-30), "excellent");
    EXPECT_EQ(q(-50), "excellent");
    EXPECT_EQ(q(-51), "good");
    EXPECT_EQ(q(-60), "good");
    EXPECT_EQ(q(-61), "fair");
    EXPECT_EQ(q(-70), "fair");
    EXPECT_EQ(q(-71), "weak");
    EXPECT_EQ(q(-80), "weak");
    EXPECT_EQ(q(-81), "unusable");
    EXPECT_EQ(q(-100), "unusable");
}

TEST(SignalQuality, ZeroOrPositiveIsInvalid) {
    EXPECT_EQ(q(0), "invalid");
    EXPECT_EQ(q(10), "invalid");
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
