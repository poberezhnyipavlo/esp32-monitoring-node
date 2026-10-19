#include <gtest/gtest.h>

#include "ClimateMeasured.h"
#include "TemperatureAlert.h"

namespace {
constexpr TemperatureLimits kLimits{.min_c = 18.0F, .max_c = 27.0F, .hysteresis_c = 0.5F};
static_assert(isValid(kLimits));
static_assert(!isValid(TemperatureLimits{.min_c = 20.0F, .max_c = 21.0F, .hysteresis_c = 0.6F}));
static_assert(!isValid(TemperatureLimits{.min_c = 18.0F, .max_c = 27.0F, .hysteresis_c = 0.0F}));

void feed(TemperatureAlert& alert, float temperature_c) {
    alert.onClimateMeasured(
        ClimateMeasured{.temperature_c = temperature_c, .humidity_pct = 50.0F, .sampled_at_ms = 0}
    );
}
}  // namespace

TEST(TemperatureAlert, StartsNormal) {
    const TemperatureAlert alert(kLimits);
    EXPECT_EQ(alert.level(), TemperatureLevel::kNormal);
}

TEST(TemperatureAlert, TooHotAboveMaxAndBackOnlyBelowBand) {
    TemperatureAlert alert(kLimits);
    feed(alert, 27.0F);
    EXPECT_EQ(alert.level(), TemperatureLevel::kNormal);
    feed(alert, 27.1F);
    EXPECT_EQ(alert.level(), TemperatureLevel::kTooHot);
    feed(alert, 26.6F);
    EXPECT_EQ(alert.level(), TemperatureLevel::kTooHot);
    feed(alert, 26.4F);
    EXPECT_EQ(alert.level(), TemperatureLevel::kNormal);
}

TEST(TemperatureAlert, TooColdBelowMinAndBackOnlyAboveBand) {
    TemperatureAlert alert(kLimits);
    feed(alert, 17.9F);
    EXPECT_EQ(alert.level(), TemperatureLevel::kTooCold);
    feed(alert, 18.4F);
    EXPECT_EQ(alert.level(), TemperatureLevel::kTooCold);
    feed(alert, 18.6F);
    EXPECT_EQ(alert.level(), TemperatureLevel::kNormal);
}

TEST(TemperatureAlert, NoiseAtTheEdgeDoesNotFlap) {
    TemperatureAlert alert(kLimits);
    feed(alert, 27.1F);
    for (const float t : {26.9F, 27.1F, 26.8F, 27.2F, 26.7F}) {
        feed(alert, t);
        EXPECT_EQ(alert.level(), TemperatureLevel::kTooHot);
    }
}

TEST(TemperatureAlert, NegativeTemperatureIsCold) {
    TemperatureAlert alert(kLimits);
    feed(alert, -5.0F);
    EXPECT_EQ(alert.level(), TemperatureLevel::kTooCold);
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
