#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <metric_protocol/MetricProtocol.h>

#include <cstddef>
#include <cstdint>

#include "ByteSink.h"
#include "QueueSender.h"
#include "SampleQueue.h"

namespace {
using ::testing::_;
using ::testing::HasSubstr;
using ::testing::InSequence;
using ::testing::Return;

class MockSink : public ByteSink {
public:
    MOCK_METHOD(bool, send, (const char* data, std::size_t len), (override));
};

constexpr std::size_t kCapacity = 8;
const metric::Origin kOrigin{.dev = "20:9b:a9:61:15:6c", .boot = 0x3f2a91c4};

void pushSamples(SampleQueue<kCapacity>& queue, uint32_t count, uint32_t sampled_at_ms) {
    for (uint32_t seq = 0; seq < count; ++seq) {
        queue.push(
            Sample{.name = "uptime_s", .value = 1.0, .seq = seq, .sampled_at_ms = sampled_at_ms}
        );
    }
}
}  // namespace

TEST(QueueSender, EmptyQueueSendsNothing) {
    SampleQueue<kCapacity> queue;
    MockSink sink;
    EXPECT_CALL(sink, send(_, _)).Times(0);

    QueueSender<kCapacity> sender(queue, sink);
    const SendResult result = sender.sendPending(kOrigin, 1000);

    EXPECT_EQ(result.sent, 0U);
    EXPECT_FALSE(result.failed);
}

TEST(QueueSender, SendsOldestFirstAndEmptiesQueue) {
    SampleQueue<kCapacity> queue;
    pushSamples(queue, 3, 0);
    MockSink sink;
    {
        const InSequence in_order;
        EXPECT_CALL(sink, send(HasSubstr("seq=0 "), _)).WillOnce(Return(true));
        EXPECT_CALL(sink, send(HasSubstr("seq=1 "), _)).WillOnce(Return(true));
        EXPECT_CALL(sink, send(HasSubstr("seq=2 "), _)).WillOnce(Return(true));
    }

    QueueSender<kCapacity> sender(queue, sink);
    const SendResult result = sender.sendPending(kOrigin, 1000);

    EXPECT_EQ(result.sent, 3U);
    EXPECT_FALSE(result.failed);
    EXPECT_EQ(queue.stats().size, 0U);
}

TEST(QueueSender, StopsOnFirstFailureAndKeepsSampleFirst) {
    SampleQueue<kCapacity> queue;
    pushSamples(queue, 4, 0);
    MockSink sink;
    EXPECT_CALL(sink, send(_, _))
        .WillOnce(Return(true))
        .WillOnce(Return(true))
        .WillOnce(Return(false));

    QueueSender<kCapacity> sender(queue, sink);
    const SendResult result = sender.sendPending(kOrigin, 1000);

    EXPECT_EQ(result.sent, 2U);
    EXPECT_TRUE(result.failed);
    EXPECT_EQ(queue.stats().size, 2U);
    const auto front = queue.front();
    ASSERT_TRUE(front.has_value());
    EXPECT_EQ(front->item.seq, 2U);
}

TEST(QueueSender, RetriesTheSameSampleNextTime) {
    SampleQueue<kCapacity> queue;
    pushSamples(queue, 2, 0);
    MockSink sink;
    {
        const InSequence in_order;
        EXPECT_CALL(sink, send(HasSubstr("seq=0 "), _)).WillOnce(Return(false));
        EXPECT_CALL(sink, send(HasSubstr("seq=0 "), _)).WillOnce(Return(true));
        EXPECT_CALL(sink, send(HasSubstr("seq=1 "), _)).WillOnce(Return(true));
    }

    QueueSender<kCapacity> sender(queue, sink);
    EXPECT_TRUE(sender.sendPending(kOrigin, 1000).failed);
    EXPECT_EQ(sender.sendPending(kOrigin, 3000).sent, 2U);
}

TEST(QueueSender, ReportsAgeOfOldestSent) {
    SampleQueue<kCapacity> queue;
    queue.push(Sample{.name = "uptime_s", .value = 1.0, .seq = 0, .sampled_at_ms = 1000});
    queue.push(Sample{.name = "uptime_s", .value = 1.0, .seq = 1, .sampled_at_ms = 4000});
    MockSink sink;
    {
        const InSequence in_order;
        EXPECT_CALL(sink, send(HasSubstr("age_ms=9000 "), _)).WillOnce(Return(true));
        EXPECT_CALL(sink, send(HasSubstr("age_ms=6000 "), _)).WillOnce(Return(true));
    }

    QueueSender<kCapacity> sender(queue, sink);
    EXPECT_EQ(sender.sendPending(kOrigin, 10000).max_age_ms, 9000U);
}

TEST(QueueSender, SkipsInvalidSampleInsteadOfBlocking) {
    SampleQueue<kCapacity> queue;
    queue.push(Sample{.name = "bad name", .value = 1.0, .seq = 0, .sampled_at_ms = 0});
    queue.push(Sample{.name = "uptime_s", .value = 1.0, .seq = 1, .sampled_at_ms = 0});
    MockSink sink;
    EXPECT_CALL(sink, send(HasSubstr("seq=1 "), _)).WillOnce(Return(true));

    QueueSender<kCapacity> sender(queue, sink);
    const SendResult result = sender.sendPending(kOrigin, 1000);

    EXPECT_EQ(result.skipped, 1U);
    EXPECT_EQ(result.sent, 1U);
}

int main(int argc, char* argv[]) {
    ::testing::InitGoogleMock(&argc, argv);
    return RUN_ALL_TESTS();
}
