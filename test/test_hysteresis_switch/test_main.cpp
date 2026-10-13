#include <gtest/gtest.h>

#include <initializer_list>

#include "HysteresisSwitch.h"

namespace {
constexpr uint16_t kOffBelow = 2000;
constexpr uint16_t kOnAbove = 2500;
constexpr uint16_t kMiddle = 2250;

HysteresisSwitch makeSwitch() { return HysteresisSwitch(kOffBelow, kOnAbove); }

uint32_t feed(HysteresisSwitch& sw, std::initializer_list<uint16_t> values) {
    for (const uint16_t v : values) {
        static_cast<void>(sw.update(v));
    }
    return sw.changeCount();
}
}  // namespace

TEST(HysteresisSwitch, StartsOff) {
    const HysteresisSwitch sw = makeSwitch();
    EXPECT_FALSE(sw.isOn());
    EXPECT_EQ(sw.changeCount(), 0U);
}

TEST(HysteresisSwitch, TurnsOnOnlyAboveUpperThreshold) {
    HysteresisSwitch sw = makeSwitch();
    EXPECT_EQ(sw.update(kOnAbove), SwitchEvent::kNone);
    EXPECT_EQ(sw.update(kOnAbove + 1), SwitchEvent::kTurnedOn);
    EXPECT_TRUE(sw.isOn());
}

TEST(HysteresisSwitch, TurnsOffOnlyBelowLowerThreshold) {
    HysteresisSwitch sw = makeSwitch();
    ASSERT_EQ(sw.update(4095), SwitchEvent::kTurnedOn);
    EXPECT_EQ(sw.update(kOffBelow), SwitchEvent::kNone);
    EXPECT_EQ(sw.update(kOffBelow - 1), SwitchEvent::kTurnedOff);
    EXPECT_FALSE(sw.isOn());
}

TEST(HysteresisSwitch, EventIsReportedOnce) {
    HysteresisSwitch sw = makeSwitch();
    EXPECT_EQ(sw.update(3000), SwitchEvent::kTurnedOn);
    EXPECT_EQ(sw.update(3000), SwitchEvent::kNone);
    EXPECT_EQ(sw.update(3500), SwitchEvent::kNone);
    EXPECT_EQ(sw.changeCount(), 1U);
}

TEST(HysteresisSwitch, SameValueDependsOnHistory) {
    HysteresisSwitch from_dark = makeSwitch();
    ASSERT_EQ(from_dark.update(4095), SwitchEvent::kTurnedOn);
    EXPECT_EQ(from_dark.update(kMiddle), SwitchEvent::kNone);
    EXPECT_TRUE(from_dark.isOn());

    HysteresisSwitch from_light = makeSwitch();
    EXPECT_EQ(from_light.update(0), SwitchEvent::kNone);
    EXPECT_EQ(from_light.update(kMiddle), SwitchEvent::kNone);
    EXPECT_FALSE(from_light.isOn());
}

TEST(HysteresisSwitch, NoiseInsideBandDoesNotToggle) {
    HysteresisSwitch sw = makeSwitch();
    ASSERT_EQ(sw.update(3000), SwitchEvent::kTurnedOn);
    const uint32_t changes = feed(sw, {2240, 2260, 2010, 2490, 2245, 2255, 2001, 2499});
    EXPECT_EQ(changes, 1U);
    EXPECT_TRUE(sw.isOn());
}

TEST(HysteresisSwitch, WithoutHysteresisNoiseToggles) {
    HysteresisSwitch sw(kMiddle, kMiddle);
    const uint32_t changes = feed(sw, {2249, 2251, 2249, 2251, 2249});
    EXPECT_EQ(changes, 4U);
}

TEST(HysteresisSwitch, CountsBothDirections) {
    HysteresisSwitch sw = makeSwitch();
    const uint32_t changes = feed(sw, {3000, 1000, 3000, 1000});
    EXPECT_EQ(changes, 4U);
    EXPECT_FALSE(sw.isOn());
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
