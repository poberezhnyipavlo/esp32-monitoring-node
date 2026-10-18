#include "RgbLed.h"

#include <esp32-hal-gpio.h>

RgbLed::RgbLed(uint8_t red_pin, uint8_t green_pin, uint8_t blue_pin)
    : red_pin_(red_pin), green_pin_(green_pin), blue_pin_(blue_pin) {}

void RgbLed::begin() {
    pinMode(red_pin_, OUTPUT);
    pinMode(green_pin_, OUTPUT);
    pinMode(blue_pin_, OUTPUT);

    set(false, false, false);
}

void RgbLed::set(bool red, bool green, bool blue) {
    digitalWrite(red_pin_, red ? HIGH : LOW);
    digitalWrite(green_pin_, green ? HIGH : LOW);
    digitalWrite(blue_pin_, blue ? HIGH : LOW);
}
