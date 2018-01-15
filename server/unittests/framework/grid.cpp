#include "gtest/gtest.h"
#include "grid/grid.h"

namespace
{
int base_calls;
int derive_a_calls;
int derive_b_calls;
std::vector<std::pair<int, int>> created_cells;

struct base
{
    void update() { ++base_calls; }
};

struct derive_a : base
{
    void update() { ++derive_a_calls; }
};

struct derive_b : base
{
    void update() { ++derive_b_calls; }
};

void create_cell(int x, int y)
{
    created_cells.push_back(std::make_pair(x, y));
}
}

namespace
{
TEST(framework, grid)
{
    framework::grid::grid<3, base, derive_a, derive_b> grid{create_cell};

    std::vector<base> base_vec(5);
    std::vector<derive_a> a_vec(5);
    std::vector<derive_b> b_vec(5);

    grid.insert(0, 0, &base_vec[0]);
    grid.insert(0, 0, &base_vec[1]);
    grid.insert(0, 0, &base_vec[2]);
    grid.visit_single<derive_a>(0, 0, [](derive_a* a)
        {
            a->update();
        });
    EXPECT_EQ(0, derive_a_calls);
    EXPECT_EQ(std::make_pair(0, 0), created_cells[0]);

    grid.visit_single<base>(0, 0, [](base* b)
        {
            b->update();
        });
    EXPECT_EQ(3, base_calls);
    base_calls = 0;

    grid.visit_single<base>(1, 1, [](base* b)
        {
            b->update();
        });
    EXPECT_EQ(0, base_calls);
    EXPECT_EQ(std::make_pair(1, 1), created_cells[1]);

    grid.insert(0, 1, &a_vec[0]);
    grid.insert(1, 0, &a_vec[1]);
    grid.insert(1, 2, &a_vec[2]);
    grid.insert(1, 2, &a_vec[3]);
    grid.insert(2, 1, &a_vec[4]);
    grid.insert(2, 1, &b_vec[0]);
    grid.insert(2, 2, &b_vec[1]);
    grid.insert(2, 2, &base_vec[3]);
    grid.insert(2, 2, &b_vec[2]);
    EXPECT_EQ(std::make_pair(0, 1), created_cells[2]);
    EXPECT_EQ(std::make_pair(1, 0), created_cells[3]);
    EXPECT_EQ(std::make_pair(1, 2), created_cells[4]);
    EXPECT_EQ(std::make_pair(2, 1), created_cells[5]);
    EXPECT_EQ(std::make_pair(2, 2), created_cells[6]);

    framework::grid::single_visitor<base, derive_a, derive_b> visitor;
    for (int i = 0; i < 3; ++i)
    {
        for (int j = 0; j < 3; ++j)
        {
            visitor(i, j, grid, [](auto&& o)
                {
                    o->update();
                });
        }
    }

    EXPECT_EQ(4, base_calls);
    EXPECT_EQ(5, derive_a_calls);
    EXPECT_EQ(3, derive_b_calls);
}
}
