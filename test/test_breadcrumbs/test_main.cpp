#include <gtest/gtest.h>

#include <bit>
#include <cstdint>
#include <optional>
#include <string_view>

#include "Breadcrumbs.h"

namespace {

constexpr uint32_t kGarbage = 0xDEADBEEF;

Breadcrumbs garbage() {
    return Breadcrumbs{
        .magic = kGarbage,
        .boot_count = kGarbage,
        .steps = TaskSteps{.loop = Step::kReadDht, .net = Step::kSendLogs, .ui = Step::kDrawOled},
    };
}

void expectAllIdle(const TaskSteps& steps) {
    EXPECT_EQ(steps.loop, Step::kIdle);
    EXPECT_EQ(steps.net, Step::kIdle);
    EXPECT_EQ(steps.ui, Step::kIdle);
}

}  // namespace

TEST(Breadcrumbs, GarbageIsNotTrusted) {
    Breadcrumbs crumbs = garbage();
    EXPECT_FALSE(startBoot(crumbs, false).has_value());
    EXPECT_EQ(crumbs.magic, kBreadcrumbsMagic);
    EXPECT_EQ(crumbs.boot_count, 1U);
    expectAllIdle(crumbs.steps);
}

TEST(Breadcrumbs, ColdStartForgetsEvenValidCrumbs) {
    Breadcrumbs crumbs = garbage();
    (void)startBoot(crumbs, true);
    crumbs.steps.net = Step::kConnectLogs;

    EXPECT_FALSE(startBoot(crumbs, true).has_value());
    EXPECT_EQ(crumbs.boot_count, 1U);
    expectAllIdle(crumbs.steps);
}

TEST(Breadcrumbs, WarmStartReturnsWhereThePreviousBootStopped) {
    Breadcrumbs crumbs = garbage();
    (void)startBoot(crumbs, true);
    crumbs.steps.net = Step::kConnectLogs;
    crumbs.steps.ui = Step::kDrawOled;

    const std::optional<TaskSteps> previous = startBoot(crumbs, false);
    ASSERT_TRUE(previous.has_value());
    EXPECT_EQ(previous->loop, Step::kIdle);
    EXPECT_EQ(previous->net, Step::kConnectLogs);
    EXPECT_EQ(previous->ui, Step::kDrawOled);
    expectAllIdle(crumbs.steps);
}

TEST(Breadcrumbs, BootCountGrowsOnEveryWarmStart) {
    Breadcrumbs crumbs = garbage();
    (void)startBoot(crumbs, true);
    (void)startBoot(crumbs, false);
    (void)startBoot(crumbs, false);
    EXPECT_EQ(crumbs.boot_count, 3U);
}

TEST(Breadcrumbs, StepNames) {
    EXPECT_EQ(std::string_view(toString(Step::kIdle)), "idle");
    EXPECT_EQ(std::string_view(toString(Step::kConnectMetrics)), "connect_metrics");
    EXPECT_EQ(std::string_view(toString(Step::kConnectLogs)), "connect_logs");
    EXPECT_EQ(std::string_view(toString(Step::kSendMetrics)), "send_metrics");
    EXPECT_EQ(std::string_view(toString(Step::kSendLogs)), "send_logs");
    EXPECT_EQ(std::string_view(toString(Step::kReadDht)), "read_dht");
    EXPECT_EQ(std::string_view(toString(Step::kPublish)), "publish");
    EXPECT_EQ(std::string_view(toString(Step::kDrawOled)), "draw_oled");
}

TEST(Breadcrumbs, UnknownByteHasAName) {
    constexpr uint8_t kOutOfRange = 200;
    EXPECT_EQ(std::string_view(toString(std::bit_cast<Step>(kOutOfRange))), "unknown");
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
