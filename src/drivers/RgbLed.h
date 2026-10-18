#pragma once

#include <cstdint>

class RgbLed {
public:
    RgbLed(uint8_t red_pin, uint8_t green_pin, uint8_t blue_pin);

    void begin();
    void set(bool red, bool green, bool blue);

private:
    uint8_t red_pin_;
    uint8_t green_pin_;
    uint8_t blue_pin_;
};
