#include "HysteresisSwitch.h"

#include <cstdint>

HysteresisSwitch::HysteresisSwitch(uint16_t off_below, uint16_t on_below)
    : off_below_(off_below), on_below_(on_below) {}

SwitchEvent HysteresisSwitch::update(uint16_t value) {
    if (!on_ && value > on_below_) {
        on_ = true;
        ++changes_;

        return SwitchEvent::kTurnedOn;
    }

    if (on_ && value < off_below_) {
        on_ = false;
        ++changes_;

        return SwitchEvent::kTurnedOff;
    }

    return SwitchEvent::kNone;
}
