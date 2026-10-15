#include "HysteresisSwitch.h"

HysteresisSwitch::HysteresisSwitch(float off_below, float on_below)
    : off_below_(off_below), on_below_(on_below) {}

SwitchEvent HysteresisSwitch::update(float value) {
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
