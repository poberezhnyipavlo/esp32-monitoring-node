#pragma once

#include <cstdint>

enum class SwitchEvent : uint8_t { kNone, kTurnedOn, kTurnedOff };

class HysteresisSwitch {
public:
    HysteresisSwitch(float off_below, float on_below);

    [[nodiscard]] SwitchEvent update(float value);
    [[nodiscard]] bool isOn() const { return on_; };
    [[nodiscard]] uint32_t changeCount() const { return changes_; };

private:
    float off_below_;
    float on_below_;
    bool on_ = false;
    uint32_t changes_ = 0;
};
