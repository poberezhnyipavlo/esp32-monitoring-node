#pragma once

#include <metric_protocol/Signature.h>

#include <cstddef>

#include "SigningSink.h"

// HMAC-SHA256 with mbedTLS (part of the ESP32 Arduino core). The monitor computes the same
// with OpenSSL.
class MbedHmacSigner : public LineSigner {
public:
    // The key comes from secrets.h and is checked in Uplink::begin(), after Serial is up; until
    // then it is all zeros and the monitor rejects every line as "bad signature".
    void setKey(const sig::Key& key) { key_ = key; }

    // On failure (no SHA-256 in mbedTLS, cannot happen on the ESP32 core) returns an all-zero MAC:
    // the monitor rejects such a line as "bad signature", which is visible in its log.
    [[nodiscard]] sig::Mac sign(const char* data, std::size_t len) const override;

private:
    sig::Key key_{};
};
