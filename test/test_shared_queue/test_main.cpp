#include <gtest/gtest.h>

#include <array>
#include <cstdint>

#include "SharedQueue.h"

namespace {
    struct Note {
        std::array<char, 8> text;
        uint32_t seq;
    };
}  // namespace

TEST(SharedQueue, WorksWithAnyItemType) {
    SharedQueue<Note, 2> q;
    q.push(Note{.text = {'a'}, .seq = 1});
    q.push(Note{.text = {'b'}, .seq = 2});
    q.push(Note{.text = {'c'}, .seq = 3});  // full: the oldest is dropped

    EXPECT_EQ(q.stats().size, 2U);
    EXPECT_EQ(q.stats().dropped, 1U);
    const auto oldest = q.front();
    ASSERT_TRUE(oldest.has_value());
    EXPECT_EQ(oldest->item.text[0], 'b');
    EXPECT_EQ(oldest->item.seq, 2U);
    EXPECT_EQ(decltype(q)::capacity(), 2U);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
