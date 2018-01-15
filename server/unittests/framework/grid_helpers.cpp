#include "gtest/gtest.h"
#include "grid/grid.h"
#include "grid/grid_helpers.h"
#include <algorithm>
#include <bitset>

namespace
{
struct obj
{
};

#define CELL_BOUNDS_CHECK(N, d, x, y, r, x_l, x_u, y_l, y_u)         \
                                                                     \
    do                                                               \
                                                                     \
    {                                                                \
        auto cell_bounds =                                           \
            framework::grid::cell_bounds_from_circle(N, d, x, y, r); \
        EXPECT_EQ(x_l, cell_bounds.x_lower);                         \
        EXPECT_EQ(x_u, cell_bounds.x_upper);                         \
        EXPECT_EQ(y_l, cell_bounds.y_lower);                         \
        EXPECT_EQ(y_u, cell_bounds.y_upper);                         \
                                                                     \
    } while (0)

template <typename T>
void visit_bounds(T& v, int N, float s, float c_x, float c_y, float r)
{
    v.clear();
    framework::grid::visit_rectangle(N, s, c_x, c_y, r, [&v, N](int x, int y)
        {
            v.push_back(y * 2 * N + x);
        });
    std::sort(std::begin(v), std::end(v));
}

template <typename T>
void visit_circle(T& v, int N, float s, float x, float y, float r)
{
    v.clear();
    framework::grid::visit_circle(N, s, x, y, r, [&v, N](int x, int y)
        {
            v.push_back(y * 2 * N + x);
        });
    std::sort(std::begin(v), std::end(v));
}
}

namespace
{
TEST(framework, grid_helpers)
{
    // Numbers in this test are intentionally kept small and simple to work
    // with, so that if a test fails you can quickly sketch it on a piece of
    // paper and see what goes wrong. Some tests are added with a bit more
    // complex numbers to test floating point handled correctly.

    // Structure:
    // 1. Test cell id conversions
    // 2. Test Square Calculations
    // 3. Test Square Visits
    // 4. Test Circular Visits

    // Test cell id conversions

    EXPECT_EQ(520, framework::grid::cell_id(32, 8, 8));
    EXPECT_EQ(std::make_pair(8, 8), framework::grid::cell_coords(32, 520));

    // Test Square Calculations

    // CELL_BOUNDS_CHECK args:
    // N (NxN cells in grid), Side length (of a square cell), Circle Center X,
    // Circle Center Y, Circle Radius, Expected Lower X,
    // Expected Upper X, Expected Lower Y, Expected Upper Y
    // Remember that upper is touched cell + 1 (one of the end)

    CELL_BOUNDS_CHECK(2, 10, 0, 0, 10, 1, 4, 1, 4);

    CELL_BOUNDS_CHECK(2, 10, 5, 5, 5, 2, 4, 2, 4);

    CELL_BOUNDS_CHECK(2, 10, 0, 0, 11, 0, 4, 0, 4);

    CELL_BOUNDS_CHECK(2, 10, 5, 5, 6, 1, 4, 1, 4);

    // Floating point check
    CELL_BOUNDS_CHECK(16, 33.3333, 16.6666, 16.6666, 22.5, 15, 18, 15, 18);

    // Test Square Visits

    std::vector<int> visited, expected;

    visit_bounds(visited, 2, 10, 5, 5, 6);
    expected =
        std::vector<int>{1 * (4) + 1, 1 * (4) + 2, 1 * (4) + 3, 2 * (4) + 1,
            2 * (4) + 2, 2 * (4) + 3, 3 * (4) + 1, 3 * (4) + 2, 3 * (4) + 3};
    EXPECT_EQ(expected, visited);

    // Test Circular Visits

    // less than 9x9 => same as bounds
    visit_bounds(expected, 2, 10, 0, 5, 6);
    visit_circle(visited, 2, 10, 0, 5, 6);
    EXPECT_EQ(6, (int)visited.size());
    EXPECT_EQ(expected, visited);

    // 9x9 square, touches 5 cells (corners ignored by circle)
    visit_circle(visited, 2, 10, 5, 5, 6);
    EXPECT_EQ(5, (int)visited.size());
    expected = std::vector<int>{
        2 * (4) + 2,              // Middle
        2 * (4) + 1, 2 * (4) + 3, // Left, Right
        1 * (4) + 2, 3 * (4) + 2, // Top Bottom
    };
    std::sort(std::begin(expected), std::end(expected));
    EXPECT_EQ(expected, visited);

    // 7x7 square, circle touches all but 3 squares in each corner, making
    // touched cells 49-12=37
    visit_bounds(visited, 4, 10, 5, 5, 26);
    EXPECT_EQ(49, (int)visited.size());
    visit_circle(visited, 4, 10, 5, 5, 26);
    EXPECT_EQ(37, (int)visited.size());
    expected.clear();
    expected.reserve(37);
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x)
        {
            auto m = [x, y](int xx, int yy)
            {
                return (x == xx && y == yy);
            };
            // The cells that should be visited
            // middle cell
            if (m(4, 4) ||
                // cross left to right
                m(1, 4) || m(2, 4) || m(3, 4) || m(5, 4) || m(6, 4) ||
                m(7, 4) ||
                // cross up to down
                m(4, 1) || m(4, 2) || m(4, 3) || m(4, 5) || m(4, 6) ||
                m(4, 7) ||
                // top right quadrant
                m(5, 1) || m(5, 2) || m(6, 2) || m(5, 3) || m(6, 3) ||
                m(7, 3) ||
                // top left quadrant
                m(3, 1) || m(2, 2) || m(3, 2) || m(1, 3) || m(2, 3) ||
                m(3, 3) ||
                // bottom left quadrant
                m(1, 5) || m(2, 5) || m(3, 5) || m(2, 6) || m(3, 6) ||
                m(3, 7) ||
                // bottom right quadrant
                m(5, 5) || m(6, 5) || m(7, 5) || m(5, 6) || m(6, 6) || m(5, 7))
                expected.push_back(y * 8 + x);
        }
    std::sort(std::begin(expected), std::end(expected));
    EXPECT_EQ(expected, visited);
}
}

#undef CELL_BOUNDS_CHECK
