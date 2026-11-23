#pragma once

#include <metric_protocol/LogProtocol.h>
#include <metric_protocol/MetricProtocol.h>
#include <metric_protocol/Signature.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <string_view>

#include "ByteSink.h"

// Computes the MAC of a payload. On the board: HMAC-SHA256 with mbedTLS (MbedHmacSigner);
// in native tests: a fake, so SigningSink is tested without crypto.
class LineSigner {
public:
    LineSigner() = default;
    LineSigner(const LineSigner&) = delete;
    LineSigner& operator=(const LineSigner&) = delete;
    LineSigner(LineSigner&&) = delete;
    LineSigner& operator=(LineSigner&&) = delete;
    virtual ~LineSigner() = default;

    [[nodiscard]] virtual sig::Mac sign(const char* data, std::size_t len) const = 0;
};

// Decorator: a ByteSink that signs every line on its way to another ByteSink (a TcpLink).
// QueueSender and LogSender do not change: they still send "...\n" to a ByteSink, and this one
// turns it into "... sig=<hex>\n".
class SigningSink : public ByteSink {
public:
    // The longest line a sender gives us (with '\n'), plus the signature.
    static_assert(metric::kMaxMessageLen <= logproto::kMaxLineLen);
    static constexpr std::size_t kBufLen = logproto::kMaxLineLen + sig::kSuffixLen;

    SigningSink(ByteSink& inner, const LineSigner& signer) : inner_(inner), signer_(signer) {}

    // data: exactly one line ending with '\n', as the senders write it. Anything else is a bug in
    // the caller; returning false keeps the entry in its queue, so it shows up as "send failed".
    [[nodiscard]] bool send(const char* data, std::size_t len) override {
        if (data == nullptr) {
            return false;
        }

        const std::string_view line(data, len);
        if (line.size() < 2 || line.back() != '\n') {
            return false;
        }

        const std::string_view payload = line.substr(0, line.size() - 1);
        if (payload.size() + sig::kSuffixLen + 1 > buf_.size()) {
            return false;
        }

        std::ranges::copy(payload, buf_.begin());

        const sig::Mac mac = signer_.sign(payload.data(), payload.size());
        const std::size_t signed_len =
            sig::appendSignature(buf_.data(), payload.size(), buf_.size(), mac);
        if (signed_len == 0) {
            return false;
        }

        return inner_.send(buf_.data(), signed_len);
    }

private:
    ByteSink& inner_;
    const LineSigner& signer_;
    // A member, not a local: send() runs on the net task, and 289 more bytes on its stack on top
    // of the sender's own buffer is what net_stack_free would have to absorb.
    std::array<char, kBufLen> buf_{};
};
