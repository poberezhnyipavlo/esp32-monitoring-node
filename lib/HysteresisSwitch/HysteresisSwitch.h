#pragma once

#include <cstdint>

enum class SwitchEvent { None, TurnedOn, TurnedOff };

class HysteresisSwitch {
public:
    HysteresisSwitch(uint16_t off_below, uint16_t on_below);

    [[nodiscard]] SwitchEvent update(uint16_t value);
    [[nodiscard]] bool isOn() const { return on_; };
    [[nodiscard]] uint32_t changeCount() const { return changes_; };

private:
    uint16_t off_below_;
    uint16_t on_below_;
    bool on_ = false;
    uint32_t changes_ = 0;
};
