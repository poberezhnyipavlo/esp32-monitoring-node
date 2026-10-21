#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "ClimateMeasured.h"
#include "ClimateReadFailed.h"
#include "EventBus.h"
#include "Listener.h"
#include "TemperatureAlert.h"

namespace {
using ::testing::AllOf;
using ::testing::Field;
using ::testing::FloatEq;
using ::testing::InSequence;

class MockListener : public Listener {
public:
    MOCK_METHOD(void, onClimateMeasured, (const ClimateMeasured& event), (override));
    MOCK_METHOD(void, onClimateReadFailed, (const ClimateReadFailed& event), (override));
};

const ClimateMeasured kEvent{
    .temperature_c = 24.5F,
    .humidity_pct = 40.0F,
    .level = TemperatureLevel::kNormal,
    .sampled_at_ms = 2000,
};

}  // namespace

TEST(EventBus, DeliversEventToEveryListenerInOrder) {
    MockListener first;
    MockListener second;
    {
        const InSequence in_order;
        EXPECT_CALL(
            first,
            onClimateMeasured(AllOf(
                Field(&ClimateMeasured::temperature_c, FloatEq(24.5F)),
                Field(&ClimateMeasured::sampled_at_ms, 2000U)
            ))
        );
        EXPECT_CALL(
            second,
            onClimateMeasured(Field(&ClimateMeasured::humidity_pct, FloatEq(40.0F)))
        );
    }

    const EventBus<2> bus{{&first, &second}};
    bus.publish(kEvent);
}

TEST(EventBus, EachPublishReachesListenerOnce) {
    MockListener listener;
    EXPECT_CALL(listener, onClimateMeasured(::testing::_)).Times(3);

    const EventBus<1> bus{{&listener}};
    bus.publish(kEvent);
    bus.publish(kEvent);
    bus.publish(kEvent);
}

TEST(EventBus, FailureReachesEveryListenerAndNotAsMeasurement) {
    MockListener first;
    MockListener second;
    EXPECT_CALL(first, onClimateMeasured(::testing::_)).Times(0);
    EXPECT_CALL(second, onClimateMeasured(::testing::_)).Times(0);
    EXPECT_CALL(first, onClimateReadFailed(Field(&ClimateReadFailed::sampled_at_ms, 4000U)));
    EXPECT_CALL(second, onClimateReadFailed(Field(&ClimateReadFailed::sampled_at_ms, 4000U)));

    const EventBus<2> bus{{&first, &second}};
    bus.publish(ClimateReadFailed{.sampled_at_ms = 4000});
}

int main(int argc, char** argv) {
    ::testing::InitGoogleMock(&argc, argv);
    return RUN_ALL_TESTS();
}
