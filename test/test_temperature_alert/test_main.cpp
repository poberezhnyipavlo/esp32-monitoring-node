#include <gtest/gtest.h>

#include "TemperatureAlert.h"

namespace {
constexpr TemperatureLimits kLimits{.min_c = 18.0F, .max_c = 27.0F, .hysteresis_c = 0.5F};
static_assert(isValid(kLimits));
static_assert(!isValid(TemperatureLimits{.min_c = 20.0F, .max_c = 21.0F, .hysteresis_c = 0.6F}));
static_assert(!isValid(TemperatureLimits{.min_c = 18.0F, .max_c = 27.0F, .hysteresis_c = 0.0F}));
}  // namespace

TEST(TemperatureAlert, StartsNormal) {
    const TemperatureAlert alert(kLimits);
    EXPECT_EQ(alert.level(), TemperatureLevel::kNormal);
}

TEST(TemperatureAlert, TooHotAboveMaxAndBackOnlyBelowBand) {
    TemperatureAlert alert(kLimits);
    EXPECT_EQ(alert.update(27.0F), TemperatureLevel::kNormal);
    EXPECT_EQ(alert.update(27.1F), TemperatureLevel::kTooHot);
    EXPECT_EQ(alert.update(26.6F), TemperatureLevel::kTooHot);
    EXPECT_EQ(alert.update(26.4F), TemperatureLevel::kNormal);
}

TEST(TemperatureAlert, TooColdBelowMinAndBackOnlyAboveBand) {
    TemperatureAlert alert(kLimits);
    EXPECT_EQ(alert.update(17.9F), TemperatureLevel::kTooCold);
    EXPECT_EQ(alert.update(18.4F), TemperatureLevel::kTooCold);
    EXPECT_EQ(alert.update(18.6F), TemperatureLevel::kNormal);
}

TEST(TemperatureAlert, NoiseAtTheEdgeDoesNotFlap) {
    TemperatureAlert alert(kLimits);
    static_cast<void>(alert.update(27.1F));
    for (const float t : {26.9F, 27.1F, 26.8F, 27.2F, 26.7F}) {
        EXPECT_EQ(alert.update(t), TemperatureLevel::kTooHot);
    }
}

TEST(TemperatureAlert, NegativeTemperatureIsCold) {
    TemperatureAlert alert(kLimits);
    EXPECT_EQ(alert.update(-5.0F), TemperatureLevel::kTooCold);
}

TEST(TemperatureAlert, LevelNames) {
    EXPECT_STREQ(toString(TemperatureLevel::kNormal), "normal");
    EXPECT_STREQ(toString(TemperatureLevel::kTooHot), "too_hot");
    EXPECT_STREQ(toString(TemperatureLevel::kTooCold), "too_cold");
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
