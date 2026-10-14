#pragma once

#include <cstdint>

#include "WifiKeeper.h"

class WifiLink {
public:
    WifiLink(const char* ssid, const char* password, int32_t channel, WifiKeeper keeper);

    void begin();
    void update(uint32_t now_ms);
    [[nodiscard]] bool isConnected() const { return keeper_.isConnected(); }
    [[nodiscard]] int32_t rssi() const;

private:
    const char* ssid_;
    const char* password_;
    int32_t channel_;
    WifiKeeper keeper_;
    bool began_once_ = false;
};
