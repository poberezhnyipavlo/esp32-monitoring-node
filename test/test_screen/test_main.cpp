#include <gtest/gtest.h>

#include <string_view>

#include "Screen.h"
#include "SnapshotStore.h"
#include "TemperatureAlert.h"

namespace {

Snapshot reading(float temperature_c, float humidity_pct, TemperatureLevel level) {
    return Snapshot{
        .has_reading = true,
        .temperature_c = temperature_c,
        .humidity_pct = humidity_pct,
        .level = level,
        .last_ok_ms = 5000,
    };
}

Snapshot normal() { return reading(24.3F, 40.6F, TemperatureLevel::kNormal); }

std::string_view text(const TextItem& item) { return item.text.data(); }

}  // namespace

TEST(Screen, ClimateWithOneDecimalAndWholePercent) {
    const Screen screen = buildScreen(normal(), false, true, true, 6000);
    EXPECT_EQ(text(screen.climate), "24.3C 41%");
}

TEST(Screen, NoReadingYetShowsDashes) {
    const Screen screen = buildScreen(Snapshot{}, false, false, false, 1000);
    EXPECT_EQ(text(screen.climate), "--.-C --%");
    EXPECT_EQ(text(screen.state), "STARTING");
}

TEST(Screen, NormalTemperatureIsOk) {
    EXPECT_EQ(text(buildScreen(normal(), false, true, true, 6000).state), "OK");
}

TEST(Screen, TemperatureAlerts) {
    const Snapshot hot = reading(28.0F, 50.0F, TemperatureLevel::kTooHot);
    const Snapshot cold = reading(17.0F, 50.0F, TemperatureLevel::kTooCold);
    EXPECT_EQ(text(buildScreen(hot, false, true, true, 6000).state), "TOO HOT");
    EXPECT_EQ(text(buildScreen(cold, false, true, true, 6000).state), "TOO COLD");
}

TEST(Screen, StaleSensorShowsSecondsSinceLastReading) {
    const Screen screen = buildScreen(normal(), true, true, true, 17000);
    EXPECT_EQ(text(screen.state), "SENSOR FAIL 12s");
}

TEST(Screen, StaleSensorKeepsLastKnownClimate) {
    const Screen screen = buildScreen(normal(), true, true, true, 17000);
    EXPECT_EQ(text(screen.climate), "24.3C 41%");
}

TEST(Screen, SensorFailBeatsTemperatureAlert) {
    const Snapshot old_hot = reading(28.0F, 50.0F, TemperatureLevel::kTooHot);
    EXPECT_EQ(text(buildScreen(old_hot, true, true, true, 17000).state), "SENSOR FAIL 12s");
}

TEST(Screen, NetworkFlags) {
    EXPECT_EQ(text(buildScreen(normal(), false, true, true, 6000).network), "WiFi:ok TCP:ok");
    EXPECT_EQ(text(buildScreen(normal(), false, true, false, 6000).network), "WiFi:ok TCP:no");
    EXPECT_EQ(text(buildScreen(normal(), false, false, false, 6000).network), "WiFi:no TCP:no");
}

TEST(Screen, ClimateIsBigOtherLinesSmallAndApart) {
    const Screen screen = buildScreen(normal(), false, true, true, 6000);
    EXPECT_EQ(screen.climate.size, 2);
    EXPECT_EQ(screen.state.size, 1);
    EXPECT_EQ(screen.network.size, 1);
    EXPECT_LT(screen.climate.y, screen.state.y);
    EXPECT_LT(screen.state.y, screen.network.y);
}

TEST(Screen, SameInputsGiveEqualScreens) {
    EXPECT_EQ(
        buildScreen(normal(), false, true, true, 6000),
        buildScreen(normal(), false, true, true, 6000)
    );
}

TEST(Screen, ChangeIsVisibleInComparison) {
    const Screen before = buildScreen(normal(), true, true, true, 17000);
    EXPECT_EQ(before, buildScreen(normal(), true, true, true, 17999));
    EXPECT_NE(before, buildScreen(normal(), true, true, true, 18000));
    EXPECT_NE(before, buildScreen(normal(), true, false, false, 17000));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
