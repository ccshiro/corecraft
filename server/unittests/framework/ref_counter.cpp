#include "gtest/gtest.h"
#include "ref_counter.h"

namespace
{
TEST(framework, ref_counter)
{
    MaNGOS::ref_counter ref;
    EXPECT_TRUE(ref.empty());

    MaNGOS::ref_counter ref_copy{ref};
    EXPECT_FALSE(ref.empty());
    EXPECT_FALSE(ref_copy.empty());

    ref = ref_copy;
    EXPECT_FALSE(ref_copy.empty());

    MaNGOS::ref_counter ref_move{std::move(ref)};
    EXPECT_TRUE(ref.empty());
    EXPECT_FALSE(ref_move.empty());

    ref = ref_move;
    EXPECT_FALSE(ref_move.empty());

    ref_copy = std::move(ref);
    ref = MaNGOS::ref_counter{};

    EXPECT_TRUE(ref.empty());
    EXPECT_FALSE(ref_copy.empty());
    EXPECT_FALSE(ref_move.empty());
}
}
