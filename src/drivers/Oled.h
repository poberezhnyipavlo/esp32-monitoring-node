#pragma once

#include <Adafruit_SSD1306.h>

#include "Screen.h"

class Oled {
public:
    Oled();

    [[nodiscard]] bool begin();

    void draw(const Screen& screen);

private:
    Adafruit_SSD1306 display_;

    void drawItem(const TextItem& item);
};
