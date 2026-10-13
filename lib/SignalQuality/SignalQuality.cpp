#include "SignalQuality.h"

#include <cstdint>

const char* signalQuality(int32_t rssi_dbm) {
    if (rssi_dbm >= 0) {
        return "invalid";
    }
    if (rssi_dbm >= -50) {
        return "excellent";
    }
    if (rssi_dbm >= -60) {
        return "good";
    }
    if (rssi_dbm >= -70) {
        return "fair";
    }
    if (rssi_dbm >= -80) {
        return "weak";
    }
    return "unusable";
}
