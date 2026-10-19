#include "Oled.h"

#include <Wire.h>

#include <array>
#include <cstdint>
#include <optional>

namespace {
namespace oled {
constexpr uint16_t kWidth = 128;
constexpr uint16_t kHeight = 64;
constexpr std::array<uint8_t, 2> kAddresess = {0x3C, 0x3D};
}  // namespace oled
}  // namespace

Oled::Oled() : display_{oled::kWidth, oled::kHeight, &Wire, -1} {}

bool Oled::begin() {
    Wire.begin();

    std::optional<uint8_t> address;

    for (const uint8_t candidate : oled::kAddresess) {
        Wire.beginTransmission(candidate);
        if (Wire.endTransmission() == 0) {
            address = candidate;
            break;
        }
    }

    if (!address) {
        return false;
    }

    if (!display_.begin(SSD1306_SWITCHCAPVCC, *address)) {
        return false;
    }

    display_.clearDisplay();
    display_.setTextColor(SSD1306_WHITE);
    display_.display();

    return true;
}

void Oled::draw(const Screen& screen) {
    display_.clearDisplay();

    drawItem(screen.network);
    drawItem(screen.state);
    drawItem(screen.climate);

    display_.display();
}

void Oled::drawItem(const TextItem& item) {
    display_.setCursor(item.x, item.y);
    display_.setTextSize(item.size);
    display_.print(item.text.data());
}
