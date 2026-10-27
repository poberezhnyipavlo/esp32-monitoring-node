#include <gtest/gtest.h>
#include <metric_protocol/LogProtocol.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "ByteSink.h"
#include "LogSender.h"
#include "Logger.h"
#include "SharedQueue.h"

namespace {

using logproto::Level;

constexpr std::size_t kCapacity = 8;
constexpr std::string_view kDev = "20:9b:a9:61:15:6c";
constexpr uint32_t kBoot = 0x3f2a91c4;

// Remembers every line it was given; refuses everything after `accept_count` lines.
class RecordingSink : public ByteSink {
public:
    explicit RecordingSink(std::size_t accept_count = 100) : accept_count_(accept_count) {}

    bool send(const char* data, std::size_t len) override {
        if (lines_.size() >= accept_count_) {
            return false;
        }
        lines_.emplace_back(data, len);
        return true;
    }

    [[nodiscard]] const std::vector<std::string>& lines() const { return lines_; }

private:
    std::size_t accept_count_;
    std::vector<std::string> lines_;
};

}  // namespace

TEST(LogSender, EmptyQueueSendsNothing) {
    SharedQueue<LogEntry, kCapacity> queue;
    RecordingSink sink;
    LogSender<kCapacity> sender(queue, sink);

    const LogSendResult result = sender.sendPending(kDev, kBoot, 1000);

    EXPECT_EQ(result.sent, 0U);
    EXPECT_FALSE(result.failed);
    EXPECT_TRUE(sink.lines().empty());
}

TEST(LogSender, SendsWholeLineWithAgeFromQueueTime) {
    SharedQueue<LogEntry, kCapacity> queue;
    Logger<kCapacity> logger(queue);
    logger.log(Level::kInfo, "boot", "started", 500);
    RecordingSink sink;
    LogSender<kCapacity> sender(queue, sink);

    const LogSendResult result = sender.sendPending(kDev, kBoot, 2000);

    EXPECT_EQ(result.sent, 1U);
    ASSERT_EQ(sink.lines().size(), 1U);
    EXPECT_EQ(
        sink.lines()[0],
        "ver=1 dev=20:9b:a9:61:15:6c boot=3f2a91c4 seq=0 age_ms=1500 level=info src=boot "
        "text=started\n"
    );
    EXPECT_EQ(queue.stats().size, 0U);
}

TEST(LogSender, SendsOldestFirst) {
    SharedQueue<LogEntry, kCapacity> queue;
    Logger<kCapacity> logger(queue);
    logger.log(Level::kInfo, "app", "first", 0);
    logger.log(Level::kInfo, "app", "second", 0);
    RecordingSink sink;
    LogSender<kCapacity> sender(queue, sink);

    static_cast<void>(sender.sendPending(kDev, kBoot, 0));

    ASSERT_EQ(sink.lines().size(), 2U);
    EXPECT_NE(sink.lines()[0].find("seq=0 "), std::string::npos);
    EXPECT_NE(sink.lines()[1].find("seq=1 "), std::string::npos);
}

TEST(LogSender, StopsOnFailureAndKeepsTheEntry) {
    SharedQueue<LogEntry, kCapacity> queue;
    Logger<kCapacity> logger(queue);
    logger.log(Level::kInfo, "app", "a", 0);
    logger.log(Level::kInfo, "app", "b", 0);
    logger.log(Level::kInfo, "app", "c", 0);
    RecordingSink sink(1);
    LogSender<kCapacity> sender(queue, sink);

    const LogSendResult result = sender.sendPending(kDev, kBoot, 0);

    EXPECT_EQ(result.sent, 1U);
    EXPECT_TRUE(result.failed);
    EXPECT_EQ(queue.stats().size, 2U);  // "b" waits for the next pass
    const auto next = queue.front();
    ASSERT_TRUE(next.has_value());
    EXPECT_EQ(next->item.seq, 1U);
}

TEST(LogSender, SkipsEntryThatCannotBeWritten) {
    SharedQueue<LogEntry, kCapacity> queue;
    Logger<kCapacity> logger(queue);
    logger.log(Level::kInfo, "Bad Src", "never sent", 0);  // invalid src: serialize() gives 0
    logger.log(Level::kInfo, "app", "sent", 0);
    RecordingSink sink;
    LogSender<kCapacity> sender(queue, sink);

    const LogSendResult result = sender.sendPending(kDev, kBoot, 0);

    EXPECT_EQ(result.skipped, 1U);
    EXPECT_EQ(result.sent, 1U);
    EXPECT_EQ(queue.stats().size, 0U);  // a bad entry must not block the queue forever
}

TEST(LogSender, EntryNewerThanPassTimeWaitsForNextPass) {
    SharedQueue<LogEntry, kCapacity> queue;
    Logger<kCapacity> logger(queue);
    logger.log(Level::kInfo, "app", "later", 1000);
    RecordingSink sink;
    LogSender<kCapacity> sender(queue, sink);

    EXPECT_EQ(sender.sendPending(kDev, kBoot, 900).sent, 0U);
    EXPECT_EQ(queue.stats().size, 1U);
    EXPECT_EQ(sender.sendPending(kDev, kBoot, 1100).sent, 1U);
    ASSERT_EQ(sink.lines().size(), 1U);
    EXPECT_NE(sink.lines()[0].find(" age_ms=100 "), std::string::npos);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
