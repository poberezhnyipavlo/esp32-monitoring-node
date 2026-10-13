#include "SignalQuality.h"

#include <cstdint>

namespace {
constexpr int32_t kExcellentDbm = -50;
constexpr int32_t kGoodDbm = -60;
constexpr int32_t kFairDbm = -70;
constexpr int32_t kWeakDbm = -80;
}  // namespace

const char* signalQuality(int32_t rssi_dbm) {
    if (rssi_dbm >= 0) {
        return "invalid";
    }
    if (rssi_dbm >= kExcellentDbm) {
        return "excellent";
    }
    if (rssi_dbm >= kGoodDbm) {
        return "good";
    }
    if (rssi_dbm >= kFairDbm) {
        return "fair";
    }
    if (rssi_dbm >= kWeakDbm) {
        return "weak";
    }
    return "unusable";
}
