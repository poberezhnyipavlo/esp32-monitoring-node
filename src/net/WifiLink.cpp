#include "WifiLink.h"

#include <WiFi.h>

#include "SignalQuality.h"

WifiLink::WifiLink(const char* ssid, const char* password, int32_t channel, WifiKeeper keeper)
    : ssid_(ssid), password_(password), channel_(channel), keeper_(keeper) {}

void WifiLink::begin() {
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(false);
}

int32_t WifiLink::rssi() const { return WiFi.RSSI(); }

void WifiLink::update(uint32_t now_ms) {
    switch (keeper_.update(WiFi.status() == WL_CONNECTED, now_ms)) {
        case WifiAction::kConnect:
            Serial.printf("[net] Connecting to %s (attempt %lu)...\n", ssid_, keeper_.attempt());
            if (began_once_) {
                WiFi.disconnect();
            }
            began_once_ = true;
            WiFi.begin(ssid_, password_, channel_);
            break;
        case WifiAction::kConnected: {
            const int32_t rssi_dbm = rssi();
            Serial.printf(
                "[net] Connected, IP %s, RSSI %d dBm (%s)\n",
                WiFi.localIP().toString().c_str(),
                static_cast<int>(rssi_dbm),
                signalQuality(rssi_dbm)
            );
            break;
        }
        case WifiAction::kTimeout:
            Serial.printf("[net] Connect timeout, retry in %lu ms\n", keeper_.backoffMs());
            break;
        case WifiAction::kLost:
            Serial.printf("[net] Connection lost, retry in %lu ms\n", keeper_.backoffMs());
            break;
        case WifiAction::kNone:
            break;
    }
}
