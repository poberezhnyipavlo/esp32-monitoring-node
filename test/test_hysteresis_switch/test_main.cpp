#include <gtest/gtest.h>

#include <initializer_list>

#include "HysteresisSwitch.h"

namespace {
constexpr uint16_t k_off_below = 2000;
constexpr uint16_t k_on_above = 2500;
constexpr uint16_t k_middle = 2250;

HysteresisSwitch makeSwitch() { return HysteresisSwitch(k_off_below, k_on_above); }

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
    EXPECT_EQ(sw.update(k_on_above), SwitchEvent::None);
    EXPECT_EQ(sw.update(k_on_above + 1), SwitchEvent::TurnedOn);
    EXPECT_TRUE(sw.isOn());
}

TEST(HysteresisSwitch, TurnsOffOnlyBelowLowerThreshold) {
    HysteresisSwitch sw = makeSwitch();
    ASSERT_EQ(sw.update(4095), SwitchEvent::TurnedOn);
    EXPECT_EQ(sw.update(k_off_below), SwitchEvent::None);
    EXPECT_EQ(sw.update(k_off_below - 1), SwitchEvent::TurnedOff);
    EXPECT_FALSE(sw.isOn());
}

TEST(HysteresisSwitch, EventIsReportedOnce) {
    HysteresisSwitch sw = makeSwitch();
    EXPECT_EQ(sw.update(3000), SwitchEvent::TurnedOn);
    EXPECT_EQ(sw.update(3000), SwitchEvent::None);
    EXPECT_EQ(sw.update(3500), SwitchEvent::None);
    EXPECT_EQ(sw.changeCount(), 1U);
}

TEST(HysteresisSwitch, SameValueDependsOnHistory) {
    HysteresisSwitch fromDark = makeSwitch();
    ASSERT_EQ(fromDark.update(4095), SwitchEvent::TurnedOn);
    EXPECT_EQ(fromDark.update(k_middle), SwitchEvent::None);
    EXPECT_TRUE(fromDark.isOn());

    HysteresisSwitch fromLight = makeSwitch();
    EXPECT_EQ(fromLight.update(0), SwitchEvent::None);
    EXPECT_EQ(fromLight.update(k_middle), SwitchEvent::None);
    EXPECT_FALSE(fromLight.isOn());
}

TEST(HysteresisSwitch, NoiseInsideBandDoesNotToggle) {
    HysteresisSwitch sw = makeSwitch();
    ASSERT_EQ(sw.update(3000), SwitchEvent::TurnedOn);
    const uint32_t changes = feed(sw, {2240, 2260, 2010, 2490, 2245, 2255, 2001, 2499});
    EXPECT_EQ(changes, 1U);
    EXPECT_TRUE(sw.isOn());
}

TEST(HysteresisSwitch, WithoutHysteresisNoiseToggles) {
    HysteresisSwitch sw(k_middle, k_middle);
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
