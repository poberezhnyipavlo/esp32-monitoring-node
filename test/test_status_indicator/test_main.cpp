#include <gtest/gtest.h>

#include <cstdint>

#include "NetworkState.h"
#include "SnapshotStore.h"
#include "StatusIndicator.h"
#include "TemperatureAlert.h"

namespace {
constexpr StatusConfig kConfig{.sensor_stale_ms = 10000, .blink_period_ms = 1000};

Snapshot reading(uint32_t last_ok_ms, TemperatureLevel level = TemperatureLevel::kNormal) {
    return Snapshot{
        .has_reading = true,
        .temperature_c = 24.0F,
        .humidity_pct = 40.0F,
        .level = level,
        .last_ok_ms = last_ok_ms
    };
}

void expectStatus(const Status& status, StatusColor color, bool blinking) {
    EXPECT_EQ(status.color, color) << toString(status.color);
    EXPECT_EQ(status.blinking, blinking);
}
}  // namespace

TEST(StatusIndicator, StartingIsBlue) {
    const StatusIndicator indicator(kConfig);
    expectStatus(
        indicator.status(Snapshot{}, NetworkState::kNoWifi, 1000),
        StatusColor::kBlue,
        false
    );
}

TEST(StatusIndicator, SensorMissingFromBootIsRedAfterStaleTime) {
    const StatusIndicator indicator(kConfig);
    expectStatus(
        indicator.status(Snapshot{}, NetworkState::kOnline, 10001),
        StatusColor::kRed,
        false
    );
}

TEST(StatusIndicator, StaleOnlyStrictlyAfterThreshold) {
    const StatusIndicator indicator(kConfig);
    expectStatus(
        indicator.status(reading(5000), NetworkState::kOnline, 15000),
        StatusColor::kGreen,
        false
    );
    expectStatus(
        indicator.status(reading(5000), NetworkState::kOnline, 15001),
        StatusColor::kRed,
        false
    );
}

TEST(StatusIndicator, SensorNotWorkingBeatsOldTemperatureAlert) {
    const StatusIndicator indicator(kConfig);
    const Snapshot old_hot = reading(5000, TemperatureLevel::kTooHot);
    expectStatus(indicator.status(old_hot, NetworkState::kOnline, 20000), StatusColor::kRed, false);
}

TEST(StatusIndicator, TemperatureOutOfRangeBlinksRed) {
    const StatusIndicator indicator(kConfig);
    expectStatus(
        indicator.status(reading(5000, TemperatureLevel::kTooHot), NetworkState::kOnline, 6000),
        StatusColor::kRed,
        true
    );
    expectStatus(
        indicator.status(reading(5000, TemperatureLevel::kTooCold), NetworkState::kOnline, 6000),
        StatusColor::kRed,
        true
    );
}

TEST(StatusIndicator, TemperatureAlertBeatsNetwork) {
    const StatusIndicator indicator(kConfig);
    expectStatus(
        indicator.status(reading(5000, TemperatureLevel::kTooHot), NetworkState::kNoWifi, 6000),
        StatusColor::kRed,
        true
    );
}

TEST(StatusIndicator, NoWifiOrNoReceiverIsYellow) {
    const StatusIndicator indicator(kConfig);
    expectStatus(
        indicator.status(reading(5000), NetworkState::kNoWifi, 6000),
        StatusColor::kYellow,
        false
    );
    expectStatus(
        indicator.status(reading(5000), NetworkState::kNoReceiver, 6000),
        StatusColor::kYellow,
        false
    );
}

TEST(StatusIndicator, AllGoodIsGreen) {
    const StatusIndicator indicator(kConfig);
    expectStatus(
        indicator.status(reading(5000), NetworkState::kOnline, 6000),
        StatusColor::kGreen,
        false
    );
}

TEST(StatusIndicator, SurvivesMillisWrapAround) {
    const StatusIndicator indicator(kConfig);
    const uint32_t just_before_wrap = 0xFFFFFF00U;
    const uint32_t just_after_wrap = 0x00000100U;
    expectStatus(
        indicator.status(reading(just_before_wrap), NetworkState::kOnline, just_after_wrap),
        StatusColor::kGreen,
        false
    );
}

TEST(StatusIndicator, BlinkingAlternatesEveryHalfPeriod) {
    const StatusIndicator indicator(kConfig);
    const Status blinking_red{.color = StatusColor::kRed, .blinking = true};
    EXPECT_EQ(indicator.colorAt(blinking_red, 0), StatusColor::kRed);
    EXPECT_EQ(indicator.colorAt(blinking_red, 499), StatusColor::kRed);
    EXPECT_EQ(indicator.colorAt(blinking_red, 500), StatusColor::kOff);
    EXPECT_EQ(indicator.colorAt(blinking_red, 999), StatusColor::kOff);
    EXPECT_EQ(indicator.colorAt(blinking_red, 1000), StatusColor::kRed);
    EXPECT_EQ(indicator.colorAt(blinking_red, 1500), StatusColor::kOff);
    EXPECT_EQ(indicator.colorAt(blinking_red, 60500), StatusColor::kOff);
    EXPECT_EQ(indicator.colorAt(blinking_red, 61000), StatusColor::kRed);
}

TEST(StatusIndicator, SteadyColourNeverGoesOff) {
    const StatusIndicator indicator(kConfig);
    const Status steady_green{.color = StatusColor::kGreen, .blinking = false};
    for (const uint32_t t : {0U, 500U, 750U, 1000U}) {
        EXPECT_EQ(indicator.colorAt(steady_green, t), StatusColor::kGreen);
    }
}

namespace {

void expectChannels(StatusColor color, bool red, bool green, bool blue) {
    const RgbChannels rgb = toRgb(color);
    EXPECT_EQ(rgb.red, red) << toString(color);
    EXPECT_EQ(rgb.green, green) << toString(color);
    EXPECT_EQ(rgb.blue, blue) << toString(color);
}

}  // namespace

TEST(ToRgb, OffLightsNothing) { expectChannels(StatusColor::kOff, false, false, false); }

TEST(ToRgb, PrimaryColoursLightOneChannel) {
    expectChannels(StatusColor::kRed, true, false, false);
    expectChannels(StatusColor::kGreen, false, true, false);
    expectChannels(StatusColor::kBlue, false, false, true);
}

TEST(ToRgb, YellowIsRedPlusGreen) { expectChannels(StatusColor::kYellow, true, true, false); }

TEST(StatusIndicator, SensorStaleOnlyStrictlyAfterThreshold) {
    const StatusIndicator indicator(kConfig);
    EXPECT_FALSE(indicator.isSensorStale(reading(5000), 15000));
    EXPECT_TRUE(indicator.isSensorStale(reading(5000), 15001));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
