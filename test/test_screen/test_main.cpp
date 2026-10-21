#include <gtest/gtest.h>

#include <optional>
#include <string_view>

#include "ClimateMeasured.h"
#include "NetworkState.h"
#include "Screen.h"
#include "TemperatureAlert.h"

namespace {

ClimateMeasured reading(float temperature_c, float humidity_pct, TemperatureLevel level) {
    return ClimateMeasured{
        .temperature_c = temperature_c,
        .humidity_pct = humidity_pct,
        .level = level,
        .sampled_at_ms = 5000,
    };
}

ClimateMeasured normal() { return reading(24.3F, 40.6F, TemperatureLevel::kNormal); }

std::string_view text(const TextItem& item) { return item.text.data(); }

}  // namespace

TEST(Screen, ClimateWithOneDecimalAndWholePercent) {
    const Screen screen = buildScreen(normal(), false, NetworkState::kOnline, 6000);
    EXPECT_EQ(text(screen.climate), "24.3C 41%");
}

TEST(Screen, NoReadingYetShowsDashes) {
    const Screen screen = buildScreen(std::nullopt, false, NetworkState::kNoWifi, 1000);
    EXPECT_EQ(text(screen.climate), "--.-C --%");
    EXPECT_EQ(text(screen.state), "STARTING");
}

TEST(Screen, NormalTemperatureIsOk) {
    EXPECT_EQ(text(buildScreen(normal(), false, NetworkState::kOnline, 6000).state), "OK");
}

TEST(Screen, TemperatureAlerts) {
    const ClimateMeasured hot = reading(28.0F, 50.0F, TemperatureLevel::kTooHot);
    const ClimateMeasured cold = reading(17.0F, 50.0F, TemperatureLevel::kTooCold);
    EXPECT_EQ(text(buildScreen(hot, false, NetworkState::kOnline, 6000).state), "TOO HOT");
    EXPECT_EQ(text(buildScreen(cold, false, NetworkState::kOnline, 6000).state), "TOO COLD");
}

TEST(Screen, StaleSensorShowsSecondsSinceLastReading) {
    const Screen screen = buildScreen(normal(), true, NetworkState::kOnline, 17000);
    EXPECT_EQ(text(screen.state), "SENSOR FAIL 12s");
}

TEST(Screen, StaleSensorKeepsLastKnownClimate) {
    const Screen screen = buildScreen(normal(), true, NetworkState::kOnline, 17000);
    EXPECT_EQ(text(screen.climate), "24.3C 41%");
}

TEST(Screen, SensorFailBeatsTemperatureAlert) {
    const ClimateMeasured old_hot = reading(28.0F, 50.0F, TemperatureLevel::kTooHot);
    EXPECT_EQ(
        text(buildScreen(old_hot, true, NetworkState::kOnline, 17000).state),
        "SENSOR FAIL 12s"
    );
}

TEST(Screen, NetworkFlags) {
    EXPECT_EQ(
        text(buildScreen(normal(), false, NetworkState::kOnline, 6000).network),
        "WiFi:ok TCP:ok"
    );
    EXPECT_EQ(
        text(buildScreen(normal(), false, NetworkState::kNoReceiver, 6000).network),
        "WiFi:ok TCP:no"
    );
    EXPECT_EQ(
        text(buildScreen(normal(), false, NetworkState::kNoWifi, 6000).network),
        "WiFi:no TCP:no"
    );
}

TEST(Screen, ClimateIsBigOtherLinesSmallAndApart) {
    const Screen screen = buildScreen(normal(), false, NetworkState::kOnline, 6000);
    EXPECT_EQ(screen.climate.size, 2);
    EXPECT_EQ(screen.state.size, 1);
    EXPECT_EQ(screen.network.size, 1);
    EXPECT_LT(screen.climate.y, screen.state.y);
    EXPECT_LT(screen.state.y, screen.network.y);
}

TEST(Screen, SameInputsGiveEqualScreens) {
    EXPECT_EQ(
        buildScreen(normal(), false, NetworkState::kOnline, 6000),
        buildScreen(normal(), false, NetworkState::kOnline, 6000)
    );
}

TEST(Screen, ChangeIsVisibleInComparison) {
    const Screen before = buildScreen(normal(), true, NetworkState::kOnline, 17000);
    EXPECT_EQ(before, buildScreen(normal(), true, NetworkState::kOnline, 17999));
    EXPECT_NE(before, buildScreen(normal(), true, NetworkState::kOnline, 18000));
    EXPECT_NE(before, buildScreen(normal(), true, NetworkState::kNoWifi, 17000));
}

TEST(Screen, SensorMissingFromBootBecomesStale) {
    EXPECT_EQ(
        text(buildScreen(std::nullopt, true, NetworkState::kOnline, 12000).state),
        "SENSOR FAIL 12s"
    );
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
