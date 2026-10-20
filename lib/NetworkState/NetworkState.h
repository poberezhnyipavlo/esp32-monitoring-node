#pragma once

#include <cstdint>
#include <string_view>

enum class NetworkState : uint8_t {
    kNoWifi,      // not connected to the access point
    kNoReceiver,  // Wi-Fi is up, no TCP connection to the receiver
    kOnline,      // Wi-Fi and the connection to the receiver are up
};

[[nodiscard]] constexpr std::string_view toString(NetworkState state) {
    switch (state) {
        case NetworkState::kNoWifi:
            return "no_wifi";
        case NetworkState::kNoReceiver:
            return "no_receiver";
        case NetworkState::kOnline:
            return "online";
    }

    return "?";
}
