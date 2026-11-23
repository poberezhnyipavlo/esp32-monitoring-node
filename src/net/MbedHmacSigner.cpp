#include "MbedHmacSigner.h"

#include <mbedtls/md.h>

sig::Mac MbedHmacSigner::sign(const char* data, std::size_t len) const {
    sig::Mac mac{};
    const mbedtls_md_info_t* sha256 = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (sha256 == nullptr) {
        return mac;
    }

    const int rc = mbedtls_md_hmac(
        sha256,
        key_.data(),
        key_.size(),
        reinterpret_cast<const unsigned char*>(data),
        len,
        mac.data()
    );
    if (rc != 0) {
        mac.fill(0);
    }
    return mac;
}
