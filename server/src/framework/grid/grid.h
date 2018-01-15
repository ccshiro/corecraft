/**
* Copyright (c) 2015 shiro <shiro@worldofcorecraft.com>
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
* THE SOFTWARE.
*/

/**
 * grid.h
 *
 * Provides the class "grid", which is, as the name implies, a grid.  This grid
 * is split up into cells. Each cell stores any type of objects. The intended
 * application of this class is the ability to store entities that have some
 * spatial locality to eachother for quick and successive access. For example, a
 * grid could be used to represent a game world, where game objects in proximity
 * to eachother interact with one another (live in the same or adjacent cells).
 *
 * The intended benefits of this grid are:
 * a) Type information is retained, which allows users to implement patterns
 *    commonly done using polymorphism at compile time (see below for an
 *    example).
 * b) Each type is stored in its own contiguous block of memory, improving
 *dcache
 *    benefits when iterating over the elements in the data structure.
 * c) All same-types are stored together, in an attempt to decrease the
 *    likelihood of icache trashing.
 */

#ifndef FRAMEWORK__GRID__GRID_H
#define FRAMEWORK__GRID__GRID_H

#include "framework/Platform/CompilerDefs.h"
#include <sparsehash/dense_hash_map>
#include <algorithm>
#include <cassert>
#include <climits>
#include <functional>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace framework
{
namespace grid
{
namespace _grid_impl
{
template <typename... Ts>
struct cell
{
    std::tuple<std::vector<Ts*>...> containers;

    template <typename T>
    std::vector<T*>& get()
    {
        return std::get<std::vector<T*>>(containers);
    }
};
}

// grid: a grid of NxN cells, storing a variable set of types
template <const int N, typename... Ts>
class grid
{
    static_assert(N * N < INT_MAX, "NxN must be packable");
    using cell_map = google::dense_hash_map<int, _grid_impl::cell<Ts...>>;

public:
    // cell_create_callback: This function is invoked when a cell is created.
    // prototype of callback: void(int x, int y)
    grid(std::function<void(int, int)> cell_create_callback = nullptr)
      : cell_create_callback_{cell_create_callback}
    {
        cells_.set_empty_key(-1);
    }

    // Insert an element into cell at (x,y)
    // Remarks: trying to insert the same element twice is undefined behavior
    template <typename T>
    void insert(int x, int y, T* t)
    {
        assert(x < N && y < N);
        auto& v = cell(x, y).template get<T>();
        assert(std::find(v.begin(), v.end(), t) == v.end());
        v.push_back(t);
    }

    // Remove an element from cell at (x,y)
    // Remarks: trying to remove non-existant element is undefined behavior
    template <typename T>
    void erase(int x, int y, T* t)
    {
        assert(x < N && y < N);
        auto& v = cell(x, y).template get<T>();
        auto itr = std::find(v.begin(), v.end(), t);
        assert(itr != v.end());
        v.erase(itr);
    }

    // Create a cell, does nothing if cell is already created
    void create_cell(int x, int y) { cell(x, y); }

    // Visit a single cell for a single type
    // Remarks: does nothing if cell isn't created yet
    template <typename T, typename F>
    void visit_single(int x, int y, F f)
    {
        assert(x < N && y < N);
        auto* c = safe_cell(x, y);
        if (!c)
            return;
        auto& v = c->template get<T>();
        for (T* t : v)
            f(t);
    }

    void clear() { cells_.clear(); }

    // Iterator to start of loaded cells
    typename cell_map::iterator begin() { return cells_.begin(); }
    typename cell_map::const_iterator begin() const { return cells_.cbegin(); }
    typename cell_map::const_iterator cbegin() const { return cells_.begin(); }

    // Iterator to one of the end of loaded cells
    typename cell_map::iterator end() { return cells_.end(); }
    typename cell_map::const_iterator end() const { return cells_.cend(); }
    typename cell_map::const_iterator cend() const { return cells_.cend(); }

private:
    cell_map cells_;
    std::function<void(int, int)> cell_create_callback_;

    // Return cell, create if it does not exist already
    _grid_impl::cell<Ts...>& cell(int x, int y)
    {
        int pack_id = y * N + x;
        auto itr = cells_.find(pack_id);
        if (unlikely(itr == cells_.end()))
        {
            auto& cell = cells_[pack_id];
            if (cell_create_callback_)
                cell_create_callback_(x, y);
            return cell;
        }
        return itr->second;
    }

    // Return cell, if cell is not created yet return nullptr instead
    _grid_impl::cell<Ts...>* safe_cell(int x, int y)
    {
        int pack_id = y * N + x;
        auto itr = cells_.find(pack_id);
        if (unlikely(itr == cells_.end()))
            return nullptr;
        return &itr->second;
    }
};

// Visitor that visits a single cell with a variadic amount of types
template <typename... Ts>
class single_visitor
{
public:
    template <typename Grid, typename F>
    void operator()(int x, int y, Grid& grid, F f)
    {
        unpack<Grid, F, Ts...>(x, y, grid, f);
    }

private:
    template <typename Grid, typename F>
    void unpack(int, int, Grid&, F)
    {
    }

    template <typename Grid, typename F, typename T, typename... T_s>
    void unpack(int x, int y, Grid& grid, F f)
    {
        grid.template visit_single<T>(x, y, f);
        unpack<Grid, F, T_s...>(x, y, grid, f);
    }
};
}
}

#endif

/* example: */
/*
// the below code relies on virtual dispatch to arrive at the derived's method.
vector<Object*> objects;
...
for (auto& object : objects)
    object->update();

// the following example solves the same problem using the grid, and does not
// need a base class or virtual functions, as the mapping to the correct call
// happens at compile-time:
grid<1, A, B, C> g;
...
single_visitor<A, B, C>()(0, 0, [](auto object) {
    object->update();
});
*/
