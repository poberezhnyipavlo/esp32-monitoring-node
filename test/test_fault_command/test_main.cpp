#include <gtest/gtest.h>

#include <optional>

#include "FaultCommand.h"

TEST(FaultCommand, KnownCommands) {
    EXPECT_EQ(parseFault("panic"), Fault::kPanic);
    EXPECT_EQ(parseFault("hang loop"), Fault::kHangLoop);
    EXPECT_EQ(parseFault("hang net"), Fault::kHangNet);
    EXPECT_EQ(parseFault("hang ui"), Fault::kHangUi);
}

TEST(FaultCommand, LineEndingsAndSpacesAround) {
    EXPECT_EQ(parseFault("panic\r\n"), Fault::kPanic);
    EXPECT_EQ(parseFault("hang net\r"), Fault::kHangNet);
    EXPECT_EQ(parseFault("  hang ui \n"), Fault::kHangUi);
}

TEST(FaultCommand, UnknownIsNothing) {
    EXPECT_EQ(parseFault(""), std::nullopt);
    EXPECT_EQ(parseFault("\r\n"), std::nullopt);
    EXPECT_EQ(parseFault("hang"), std::nullopt);
    EXPECT_EQ(parseFault("hang  net"), std::nullopt);
    EXPECT_EQ(parseFault("PANIC"), std::nullopt);
    EXPECT_EQ(parseFault("panic now"), std::nullopt);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
