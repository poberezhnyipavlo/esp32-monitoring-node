#include <gtest/gtest.h>
#include <metric_protocol/LogProtocol.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

#include "Logger.h"
#include "SharedQueue.h"

namespace {

using logproto::Level;

template <std::size_t N>
LogEntry popOldest(SharedQueue<LogEntry, N>& queue) {
    const auto entry = queue.front();
    EXPECT_TRUE(entry.has_value());
    queue.popFront(entry->id);
    return entry->item;
}

}  // namespace

TEST(Logger, QueuesEntryWithAllFields) {
    SharedQueue<LogEntry, 4> queue;
    Logger<4> logger(queue);

    logger.log(Level::kWarn, "dht", "sensor stopped answering", 2000);

    ASSERT_EQ(queue.stats().size, 1U);
    const LogEntry entry = popOldest(queue);
    EXPECT_EQ(entry.level, Level::kWarn);
    EXPECT_EQ(std::string_view(entry.src.data()), "dht");
    EXPECT_EQ(std::string_view(entry.text.data()), "sensor stopped answering");
    EXPECT_EQ(entry.seq, 0U);
    EXPECT_EQ(entry.sampled_at_ms, 2000U);
}

TEST(Logger, CopiesTextSoTheCallerMayDestroyIt) {
    SharedQueue<LogEntry, 4> queue;
    Logger<4> logger(queue);
    {
        std::string text = "temporary";
        logger.log(Level::kInfo, "app", text, 0);
        text.assign("overwritten");
    }
    EXPECT_EQ(std::string_view(popOldest(queue).text.data()), "temporary");
}

TEST(Logger, CutsTextAndSrcToProtocolLimits) {
    SharedQueue<LogEntry, 4> queue;
    Logger<4> logger(queue);
    const std::string long_text(200, 'x');
    const std::string long_src(30, 'a');

    logger.log(Level::kInfo, long_src, long_text, 0);

    const LogEntry entry = popOldest(queue);
    EXPECT_EQ(std::string_view(entry.text.data()).size(), logproto::kMaxTextLen);
    EXPECT_EQ(std::string_view(entry.src.data()).size(), logproto::kMaxSrcLen);
}

TEST(Logger, NumbersEntriesInOrder) {
    SharedQueue<LogEntry, 4> queue;
    Logger<4> logger(queue);
    logger.log(Level::kInfo, "app", "a", 0);
    logger.log(Level::kInfo, "app", "b", 0);
    logger.log(Level::kInfo, "app", "c", 0);

    EXPECT_EQ(popOldest(queue).seq, 0U);
    EXPECT_EQ(popOldest(queue).seq, 1U);
    EXPECT_EQ(popOldest(queue).seq, 2U);
}

TEST(Logger, TwoTasksNeverBreakTheOrder) {
    constexpr uint32_t kPerTask = 50000;
    SharedQueue<LogEntry, 64> queue;
    Logger<64> logger(queue);
    std::atomic<int> running{2};

    auto task = [&logger, &running](std::string_view src) {
        for (uint32_t i = 0; i < kPerTask; ++i) {
            logger.log(Level::kInfo, src, "x", i);
        }
        --running;
    };
    std::thread first(task, "first");
    std::thread second(task, "second");

    uint32_t received = 0;
    int64_t last = -1;
    bool ordered = true;
    while (true) {
        const auto entry = queue.front();
        if (!entry) {
            if (running == 0) {
                break;
            }
            std::this_thread::yield();
            continue;
        }
        if (queue.popFront(entry->id)) {
            ordered = ordered && std::cmp_greater(entry->item.seq, last);
            last = entry->item.seq;
            ++received;
        }
    }
    first.join();
    second.join();

    EXPECT_TRUE(ordered);
    EXPECT_EQ(received + queue.stats().dropped, 2 * kPerTask);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
