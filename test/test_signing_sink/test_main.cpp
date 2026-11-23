#include <gtest/gtest.h>
#include <metric_protocol/LogProtocol.h>
#include <metric_protocol/Signature.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "ByteSink.h"
#include "SigningSink.h"

namespace {

// Remembers what reached the "network".
class RecordingSink : public ByteSink {
public:
    [[nodiscard]] bool send(const char* data, std::size_t len) override {
        sent_.emplace_back(data, len);
        return accept_;
    }

    [[nodiscard]] const std::vector<std::string>& sent() const { return sent_; }

    void refuse() { accept_ = false; }

private:
    std::vector<std::string> sent_;
    bool accept_ = true;
};

// MAC = first byte is the payload length, the rest 0xab: shows exactly what was signed.
class FakeSigner : public LineSigner {
public:
    [[nodiscard]] sig::Mac sign(const char* data, std::size_t len) const override {
        signed_payload_ = std::string(data, len);
        sig::Mac mac{};
        mac.fill(0xab);
        mac[0] = static_cast<uint8_t>(len);
        return mac;
    }

    [[nodiscard]] const std::string& signedPayload() const { return signed_payload_; }

private:
    // mutable: sign() is const (the real signer does not change), the fake only remembers.
    mutable std::string signed_payload_;
};

constexpr std::string_view kLine = "ver=2 dev=20:9b:a9:61:15:6c seq=1 name=t value=24.3\n";

}  // namespace

TEST(SigningSink, SignsPayloadWithoutNewlineAndAppendsSignature) {
    RecordingSink inner;
    const FakeSigner signer;
    SigningSink sink(inner, signer);

    ASSERT_TRUE(sink.send(kLine.data(), kLine.size()));

    const std::string_view payload = kLine.substr(0, kLine.size() - 1);
    EXPECT_EQ(signer.signedPayload(), payload);
    ASSERT_EQ(inner.sent().size(), 1U);
    const std::string& out = inner.sent().front();
    EXPECT_EQ(out.back(), '\n');

    const auto split = sig::splitSignature(out);
    ASSERT_TRUE(split.has_value());
    EXPECT_EQ(split->payload, payload);
    EXPECT_EQ(split->mac[0], payload.size());
    EXPECT_EQ(split->mac[1], 0xab);
}

TEST(SigningSink, PassesInnerFailureOn) {
    RecordingSink inner;
    inner.refuse();
    const FakeSigner signer;
    SigningSink sink(inner, signer);

    EXPECT_FALSE(sink.send(kLine.data(), kLine.size()));
}

TEST(SigningSink, RejectsWhatIsNotOneLine) {
    RecordingSink inner;
    const FakeSigner signer;
    SigningSink sink(inner, signer);

    const std::string_view no_newline = "ver=2 dev=20:9b:a9:61:15:6c";
    EXPECT_FALSE(sink.send(no_newline.data(), no_newline.size()));
    EXPECT_FALSE(sink.send("\n", 1));
    EXPECT_FALSE(sink.send(nullptr, 0));
    EXPECT_TRUE(inner.sent().empty());
}

// The longest log line logproto::serialize can write still fits with its signature.
TEST(SigningSink, LongestLogLineFits) {
    RecordingSink inner;
    const FakeSigner signer;
    SigningSink sink(inner, signer);

    const std::string text(logproto::kMaxTextLen, 'x');
    const logproto::Record record{
        .dev = "20:9b:a9:61:15:6c",
        .boot = 0xffffffff,
        .seq = 4294967295U,
        .age_ms = 4294967295U,
        .level = logproto::Level::kError,
        .src = "abcdefghijklmno",
        .text = text,
    };
    std::array<char, logproto::kMaxLineLen> line{};
    const std::size_t len = logproto::serialize(record, line.data(), line.size());
    ASSERT_GT(len, 0U);

    ASSERT_TRUE(sink.send(line.data(), len));
    EXPECT_EQ(inner.sent().front().size(), len + sig::kSuffixLen);
}

TEST(SigningSink, TooLongLineIsRejected) {
    RecordingSink inner;
    const FakeSigner signer;
    SigningSink sink(inner, signer);

    std::string line(logproto::kMaxLineLen + 1, 'x');
    line.back() = '\n';
    EXPECT_FALSE(sink.send(line.data(), line.size()));
}

int main(int argc, char* argv[]) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
