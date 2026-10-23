#include <gtest/gtest.h>

#include <cstdint>

#include "Units.h"

TEST(Units, IsAfter) {
    EXPECT_TRUE(units::isAfter(1001, 1000));
    EXPECT_FALSE(units::isAfter(1000, 1000));
    EXPECT_FALSE(units::isAfter(999, 1000));
}

TEST(Units, IsAfterAcrossMillisOverflow) {
    EXPECT_TRUE(units::isAfter(5, UINT32_MAX - 5));  // 5 ms after the wrap is still "after"
    EXPECT_FALSE(units::isAfter(UINT32_MAX - 5, 5));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
