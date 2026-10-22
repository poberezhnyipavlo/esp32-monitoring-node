#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <string>
#include <string_view>

#include "LogProtocol.h"

namespace {

using logproto::Level;
using logproto::Record;

constexpr std::string_view kDev = "20:9b:a9:61:15:6c";

Record record(Level level, std::string_view src, std::string_view text) {
    return Record{
        .dev = kDev,
        .boot = 0x3f2a91c4,
        .seq = 7,
        .age_ms = 1500,
        .level = level,
        .src = src,
        .text = text,
    };
}

std::string line(const Record& r) {
    std::array<char, logproto::kMaxLineLen> buf{};
    const std::size_t len = logproto::serialize(r, buf.data(), buf.size());
    return std::string(buf.data(), len);
}

}  // namespace

TEST(LogProtocol, ExactWireFormat) {
    EXPECT_EQ(
        line(record(Level::kWarn, "dht", "sensor stopped answering")),
        "ver=1 dev=20:9b:a9:61:15:6c boot=3f2a91c4 seq=7 age_ms=1500 level=warn src=dht "
        "text=sensor stopped answering\n"
    );
}

TEST(LogProtocol, LevelNames) {
    EXPECT_EQ(logproto::toString(Level::kInfo), "info");
    EXPECT_EQ(logproto::toString(Level::kWarn), "warn");
    EXPECT_EQ(logproto::toString(Level::kError), "error");
}

TEST(LogProtocol, BootIsAlwaysEightHexDigits) {
    Record r = record(Level::kInfo, "boot", "started");
    r.boot = 0x1a;
    EXPECT_NE(line(r).find(" boot=0000001a "), std::string::npos);
}

TEST(LogProtocol, LineBreaksInTextBecomeSpaces) {
    EXPECT_EQ(
        line(record(Level::kError, "app", "first\nsecond\r\nthird")),
        "ver=1 dev=20:9b:a9:61:15:6c boot=3f2a91c4 seq=7 age_ms=1500 level=error src=app "
        "text=first second  third\n"
    );
}

TEST(LogProtocol, LongTextIsCut) {
    const std::string long_text(200, 'x');
    const std::string out = line(record(Level::kInfo, "app", long_text));
    const std::string expected_text = "text=" + std::string(logproto::kMaxTextLen, 'x') + "\n";
    ASSERT_GE(out.size(), expected_text.size());
    EXPECT_EQ(out.substr(out.size() - expected_text.size()), expected_text);
}

TEST(LogProtocol, EmptyTextIsAllowed) {
    const std::string out = line(record(Level::kInfo, "app", ""));
    EXPECT_EQ(out.substr(out.size() - 6), "text=\n");
}

TEST(LogProtocol, SrcRules) {
    EXPECT_TRUE(logproto::isValidSrc("dht"));
    EXPECT_TRUE(logproto::isValidSrc("net_tcp2"));
    EXPECT_FALSE(logproto::isValidSrc(""));
    EXPECT_FALSE(logproto::isValidSrc("Dht"));
    EXPECT_FALSE(logproto::isValidSrc("d ht"));
    EXPECT_FALSE(logproto::isValidSrc(std::string(logproto::kMaxSrcLen + 1, 'a')));
    EXPECT_EQ(line(record(Level::kInfo, "bad src", "x")), "");
}

TEST(LogProtocol, LongestLineFits) {
    const std::string long_text(200, 'x');  // must outlive r: Record holds string_views
    Record r = record(Level::kError, "abcdefghijklmno", long_text);
    r.seq = 4294967295U;
    r.age_ms = 4294967295U;
    std::array<char, logproto::kMaxLineLen> buf{};
    EXPECT_GT(logproto::serialize(r, buf.data(), buf.size()), 0U);
}

TEST(LogProtocol, RejectsTooSmallBuffer) {
    std::array<char, 40> small{};
    EXPECT_EQ(
        logproto::serialize(record(Level::kInfo, "app", "x"), small.data(), small.size()),
        0U
    );
}

TEST(LogProtocol, BufferMustAlsoHoldTheTerminator) {
    const Record r = record(Level::kInfo, "app", "x");
    const std::size_t len = line(r).size();
    std::array<char, logproto::kMaxLineLen> buf{};
    EXPECT_EQ(
        logproto::serialize(r, buf.data(), len),
        0U
    );  // no room for '\0': line would lose '\n'
    EXPECT_EQ(logproto::serialize(r, buf.data(), len + 1), len);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
