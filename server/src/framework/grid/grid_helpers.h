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
 * file: grid_helpers.h:
 *
 * The grid class is a generalization that can be used in any which way. This
 * file is here to provide helpers for common use cases. For example the grid
 * ignores its own spatial size, whereas these helpers will allow you to work
 * with concrete cases. For example, you could use the helper to visit multiple
 * cells that are touched by a circle in space.
 */

#ifndef FRAMEWORK__GRID__GRID_HELPERS_H
#define FRAMEWORK__GRID__GRID_HELPERS_H

#include <algorithm>
#include <cassert>
#include <climits>
#include <tuple>
#include <utility>
#include <vector>

namespace framework
{
namespace grid
{
// Defines a rectangle encapsulating cells, where x and y map to enclosed cells
// in the range [lower, upper)
struct cell_bounds
{
    int x_lower;
    int x_upper;
    int y_lower;
    int y_upper;
};

// Packs cell X and Y into a single unique identifier
// N: (N, N) is used as the origin of the grid (where 0, 0 is)
// (X, Y): Cell denoted by X, Y
inline int cell_id(int N, int X, int Y)
{
    return Y * N * 2 + X;
}

// Extract cell X and Y from packed cell id
// N: (N, N) is used as the origin of the grid (where 0, 0 is)
// cell_id: packed id of cell
// Returns: cell X and Y
inline std::pair<int, int> cell_coords(int N, int cell_id)
{
    auto mod = N * 2;
    return std::make_pair(cell_id % mod, cell_id / mod);
}

// Map world coordinate coordinate to cell coordinate.
// For example, if you have 2 coordinates X and Y, and they fall in
// cell A at (Ax, Ay), the the following condition holds true:
// Ax == coord_to_cell(N, S, X) && Ay == coord_to_cell(N, S, Y)
//
// Parameters:
// N: (N, N) is used as the origin of the grid (where 0, 0 is)
// side: the size a single side has in a cell (cells are squares)
// XorY: World coordinate
//
// Remarks:
// There also exists a coords_to_cell_pair() which combines X and Y and returns
// a pair making up the X and Y of the cell.
//
inline int coord_to_cell(int N, float side, float XorY)
{
    return XorY / side + N;
}

// Map world (X, Y) to cell (X, Y).
//
// Parameters:
// N: (N, N) is used as the origin of the grid (where 0, 0 is)
// side: the size a single side has in a cell (cells are squares)
// X and Y: World coordinates.
//
// Returns:
// pair<cell X, cell Y>
//
inline std::pair<int, int> coords_to_cell_pair(
    int N, float side, float X, float Y)
{
    return std::make_pair(X / side + N, Y / side + N);
}

// Map top left of cell (X, Y) to world coordinates.
//
// Parameters:
// N: (N, N) is used as the origin of the grid (where 0, 0 is)
// side: the size a single side has in a cell (cells are squares)
// X and Y: World coordinates.
//
// Returns:
// pair<world X, world Y>
//
inline std::pair<float, float> cell_pair_to_coords(
    int N, float side, int X, int Y)
{
    return std::make_pair((X - N) * side, (Y - N) * side);
}

// Returns a cell_bounds defining what cells are in the rectangular are derived
// from the circle we were given.
//
// Parameters:
// N: (N, N) is used as the origin of the grid (where 0, 0 is)
// side: the size a single side has in a cell (cells are squares)
// center_x, center_y: the center X and Y of the circle. This together with the
//                     radius will be used to calculate the boundaries.
// radius: the radius of the circle (must be bigger than 0)
//
// Returns:
// The boundaries of touched cells in a hypothetical grid defined by Parameters.
//
// Remarks:
// Returned range is [lower, upper). In other words, upper is one of the end.
//
inline cell_bounds cell_bounds_from_circle(
    int N, float side, float center_x, float center_y, float radius)
{
    assert(radius > 0);

    cell_bounds bounds;

    // Top Left
    bounds.x_lower = (center_x - radius) / side + N;
    bounds.y_lower = (center_y - radius) / side + N;
    // Bottom Right
    bounds.x_upper = (center_x + radius) / side + N + 1;
    bounds.y_upper = (center_y + radius) / side + N + 1;

    if (unlikely(bounds.x_lower < 0))
        bounds.x_lower = 0;
    if (unlikely(bounds.y_lower < 0))
        bounds.y_lower = 0;
    auto n2 = N * 2;
    if (unlikely(bounds.x_upper > n2))
        bounds.x_upper = n2;
    if (unlikely(bounds.y_upper > n2))
        bounds.y_upper = n2;

    return bounds;
}

// Invokes visitor object on all cells returns from cell_bounds_from_circle().
//
// Parameters:
// visitor: function object to visit on, must be callable like this:
//          visitor(grid_x, grid_y);
// For rest of parameters see cell_bounds_from_circle().
//
// Remarks:
// All cells calculated from circle are assumed to be valid, and this function
// peforms no validity checking (nor could it).
//
// Note that this function does not verify if the circle touches the cell before
// invoking the visitor. In the average case visit_circle() is more likely to
// have the effect you're looking for.
//
template <typename Visitor>
void visit_rectangle(int N, float side, float center_x, float center_y,
    float radius, Visitor visitor)
{
    assert(radius > 0);

    auto bounds = cell_bounds_from_circle(N, side, center_x, center_y, radius);

    for (int y = bounds.y_lower; y < bounds.y_upper; ++y)
        for (int x = bounds.x_lower; x < bounds.x_upper; ++x)
            visitor(x, y);
}

// Invokes visitor object on all cells returns from cell_bounds_from_circle()
// if they're touched by the specified circle.
//
// Remarks:
// Identical to visit_cell_bounds except it also makes sure the circle touches
// the visited cells.
//
// Notice that if the resulting boundary has an area of less than 9 cells, then
// the same functionality as visit_rectangle() will execute (as opposed to the
// more complex circular visit).
//
// This has the possiblity to leave out cells the circle actually touches. The
// route chosen was minimum branching with an acceptable outcome that the
// accuracy shifts a bit. For small radii (which we assume are more sensitive
// to errors) it will use visit_cell_bounds().
//
template <typename Visitor>
void visit_circle(int N, float side, float center_x, float center_y,
    float radius, Visitor visitor)
{
    assert(radius > 0);

    auto bounds = cell_bounds_from_circle(N, side, center_x, center_y, radius);

    // With an area of less than 9 cells we just visit the rectangle
    auto x_diff = bounds.x_upper - bounds.x_lower;
    auto y_diff = bounds.y_upper - bounds.y_lower;
    if (x_diff * y_diff < 9)
    {
        for (int y = bounds.y_lower; y < bounds.y_upper; ++y)
            for (int x = bounds.x_lower; x < bounds.x_upper; ++x)
                visitor(x, y);
        return;
    }

    // Cell that center_x and center_y is in
    auto cell_x = coord_to_cell(N, side, center_x);
    auto cell_y = coord_to_cell(N, side, center_y);

    auto r2 = radius * radius;

    // ## This is how visit_circle works:
    // NOTE: This code is CPU friendly (due to its sequential nature), but not
    //       so much for humans to read through. Here's a quick rundown of what
    //       happens to make it easier.
    // 1. Visit the cell the circle's center is in.
    // 2. The cross that ensues from the circle's center, split that up into 4
    //    parts (excluding the actual cell of the center, which we visited in
    //    #1) and visit each part. These are always touched by the circle, so
    //    no checking is needed.
    // 3. Split up the other cells into 4 quadrants. That makes it so we know
    //    which corner to check against. Then we visit the quadrants (making
    //    sure to exclude the cross and middle).

    // Cell the circle's center is in
    visitor(cell_x, cell_y);

    // Cross, from left to middle
    for (int x = bounds.x_lower; x < cell_x; ++x)
        visitor(x, cell_y);

    // Cross, middle to right
    for (int x = cell_x + 1; x < bounds.x_upper; ++x)
        visitor(x, cell_y);

    // Cross, from top to middle
    for (int y = bounds.y_lower; y < cell_y; ++y)
        visitor(cell_x, y);

    // Cross, from middle to bottom
    for (int y = cell_y + 1; y < bounds.y_upper; ++y)
        visitor(cell_x, y);

    // (Q1) Top Right Quadrant -> Check Bottom Left corner
    for (int y = bounds.y_lower; y < cell_y; ++y)
        for (int x = cell_x + 1; x < bounds.x_upper; ++x)
        {
            auto cx = (x - N) * side - center_x;
            auto cy = (y - N + 1) * side - center_y;
            if (cx * cx + cy * cy < r2)
                visitor(x, y);
        }

    // (Q2) Top Left Quadrant -> Check Bottom Right corner
    for (int y = bounds.y_lower; y < cell_y; ++y)
        for (int x = bounds.x_lower; x < cell_x; ++x)
        {
            auto cx = (x - N + 1) * side - center_x;
            auto cy = (y - N + 1) * side - center_y;
            if (cx * cx + cy * cy < r2)
                visitor(x, y);
        }

    // (Q3) Bottom Left Quadrant -> Check Top Right corner
    for (int y = cell_y + 1; y < bounds.y_upper; ++y)
        for (int x = bounds.x_lower; x < cell_x; ++x)
        {
            auto cx = (x - N + 1) * side - center_x;
            auto cy = (y - N) * side - center_y;
            if (cx * cx + cy * cy < r2)
                visitor(x, y);
        }

    // (Q4) Bottom Right Quadrant -> Check Top Left corner
    for (int y = cell_y + 1; y < bounds.y_upper; ++y)
        for (int x = cell_x + 1; x < bounds.x_upper; ++x)
        {
            auto cx = (x - N) * side - center_x;
            auto cy = (y - N) * side - center_y;
            if (cx * cx + cy * cy < r2)
                visitor(x, y);
        }
}
}
}

#endif
