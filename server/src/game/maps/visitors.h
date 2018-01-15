/**
 * visitors.h
 *
 * Provide functionality for doing common case visitor operations.
 * A visitor is a functor that takes a source entity, a radius and then spits
 * out a result. It's often complemented with a check object to decide if an
 * entity should be included in the result. Checks can be found in
 * checks.h. Generic callbacks can be found in callbacks.h.
 *
 * Templated types for all visitors:
 *     T: what type is yielded
 *     Container: first cell object container to visit
 *     Args...: more cell object containers to visit
 */

#ifndef GAME__MAPS__CELL_VISITORS_H
#define GAME__MAPS__CELL_VISITORS_H

#include "framework/grid/grid.h"
#include "framework/grid/grid_helpers.h"
#include <vector>

namespace maps
{
namespace visitors
{
template <typename Src, typename T>
inline float dist_square_2d(Src* src, T* tar)
{
    float dx = src->GetX() - tar->GetX();
    float dy = src->GetY() - tar->GetY();
    float bb = src->GetObjectBoundingRadius() + tar->GetObjectBoundingRadius();

    float distsq = dx * dx + dy * dy - bb * bb;
    if (unlikely(distsq < 0))
        distsq = 0;

    return distsq;
}

template <typename Src, typename T>
inline float dist_square_3d(Src* src, T* tar)
{
    float dx = src->GetX() - tar->GetX();
    float dy = src->GetY() - tar->GetY();
    float dz = src->GetZ() - tar->GetZ();
    float bb = src->GetObjectBoundingRadius() + tar->GetObjectBoundingRadius();

    float distsq = dx * dx + dy * dy + dz * dz - bb * bb;
    if (unlikely(distsq < 0))
        distsq = 0;

    return distsq;
}

template <typename Src, typename T>
inline bool in_range_2d(Src* src, T* tar, float r)
{
    return dist_square_2d(src, tar) < r * r;
}

template <typename Src, typename T>
inline bool in_range_3d(Src* src, T* tar, float r)
{
    return dist_square_3d(src, tar) < r * r;
}

// Most simple visitor, does not yield any results.
//
// Accepts a callback instead of a check functor.
//
template <typename Container, typename... Args>
struct simple
{
    framework::grid::single_visitor<Container, Args...> visitor_obj;

    template <typename Source, typename Callback>
    void operator()(Source src, float radius, Callback callback)
    {
        if (unlikely(radius <= 0))
            return;

        auto map = src->GetMap();

        // Visit circle, and invoke callback for all T in range
        framework::grid::visit_circle(MAP_CELL_MID, MAP_CELL_SIZE, src->GetX(),
            src->GetY(), radius,
            [this, src, map, radius, callback](int x, int y) mutable
            {
                visitor_obj(x, y, map->get_map_grid().get_grid(),
                    [src, callback, radius](auto&& t) mutable
                    {
                        if (in_range_3d(src, t, radius))
                            callback(t);
                    });
            });
    }

    template <typename Source, typename Callback>
    void visit_2d(Source src, float radius, Callback callback)
    {
        if (unlikely(radius <= 0))
            return;

        auto map = src->GetMap();

        // Visit circle, and invoke callback for all T in range
        framework::grid::visit_circle(MAP_CELL_MID, MAP_CELL_SIZE, src->GetX(),
            src->GetY(), radius,
            [this, src, map, radius, callback](int x, int y) mutable
            {
                visitor_obj(x, y, map->get_map_grid().get_grid(),
                    [src, callback, radius](auto&& t) mutable
                    {
                        if (in_range_2d(src, t, radius))
                            callback(t);
                    });
            });
    }
};

// Visitor to yield a set.
//
// Returns a vector<T*>
//
template <typename T, typename Container = T, typename... Args>
struct yield_set
{
    framework::grid::single_visitor<Container, Args...> visitor_obj;

    template <typename Source, typename Check>
    std::vector<T*> operator()(Source src, float radius, Check check)
    {
        std::vector<T*> set;

        if (unlikely(radius <= 0))
            return set;

        auto map = src->GetMap();

        // Visit circle, and add all T that pass check to set
        framework::grid::visit_circle(MAP_CELL_MID, MAP_CELL_SIZE, src->GetX(),
            src->GetY(), radius,
            [this, src, &set, map, radius, &check](int x, int y) mutable
            {
                visitor_obj(x, y, map->get_map_grid().get_grid(),
                    [src, &set, &check, radius](auto&& t) mutable
                    {
                        if (in_range_3d(src, t, radius) && check(t))
                            set.push_back(t);

                    });
            });

        return set;
    }
};

// Visitor to yield a single.
//
// Returns a T*
//
template <typename T, typename Container = T, typename... Args>
struct yield_single
{
    framework::grid::single_visitor<Container, Args...> visitor_obj;

    template <typename Source, typename Check>
    T* operator()(Source src, float radius, Check check)
    {
        if (unlikely(radius <= 0))
            return nullptr;

        T* out = nullptr;

        auto map = src->GetMap();

        // Visit circle, accepting first T that pass check
        framework::grid::visit_circle(MAP_CELL_MID, MAP_CELL_SIZE, src->GetX(),
            src->GetY(), radius,
            [this, src, &out, map, radius, &check](int x, int y) mutable
            {
                visitor_obj(x, y, map->get_map_grid().get_grid(),
                    [src, &out, &check, radius](auto&& t) mutable
                    {
                        // TODO: Once we got our pointer we could just exit, but
                        //       the grid does not support this yet. This is a
                        //       fairly rare case, though.
                        if (out)
                            return;
                        if (in_range_3d(src, t, radius) && check(t))
                            out = t;

                    });
            });

        return out;
    }
};

// Visitor to yield the best matching, single target. The best match is the one
// that passes the check and is closer to the source object than any other
// object that passed the check.
//
// Returns a T*
//
template <typename T, typename Container = T, typename... Args>
struct yield_best_match
{
    framework::grid::single_visitor<Container, Args...> visitor_obj;

    template <typename Source, typename Check>
    T* operator()(Source src, float radius, Check check)
    {
        if (unlikely(radius <= 0))
            return nullptr;

        T* out = nullptr;
        float out_dist_sq = 0.0f;
        float radius_sq = radius * radius;

        auto map = src->GetMap();

        // Visit circle, accepting best-matching T. The check is responsible for
        // saving state and deciding if current T is better.
        framework::grid::visit_circle(MAP_CELL_MID, MAP_CELL_SIZE, src->GetX(),
            src->GetY(), radius,
            [this, src, &out, &out_dist_sq, map, radius_sq, &check](
                                          int x, int y) mutable
            {
                visitor_obj(x, y, map->get_map_grid().get_grid(),
                    [src, &out, &out_dist_sq, &check, map, radius_sq](
                                auto&& t) mutable
                    {
                        float dist_sq = dist_square_3d(src, t);
                        if (dist_sq > radius_sq)
                            return;
                        if (out && out_dist_sq < dist_sq)
                            return;
                        if (check(t))
                        {
                            out = t;
                            out_dist_sq = dist_sq;
                        }
                    });
            });

        return out;
    }
};

// Visitor specifically meant to invoke functors on the owner of Camera objects.
//
// Returns a T*
//
struct camera_owner
{
    framework::grid::single_visitor<Camera> visitor_obj;

    template <typename Source, typename Callback>
    void operator()(Source src, float radius, Callback callback)
    {
        if (unlikely(radius <= 0))
            return;

        auto map = src->GetMap();

        // Visit circle, and invoke callback for all Camera owners
        framework::grid::visit_circle(MAP_CELL_MID, MAP_CELL_SIZE, src->GetX(),
            src->GetY(), radius,
            [this, src, map, radius, callback](int x, int y) mutable
            {
                visitor_obj(x, y, map->get_map_grid().get_grid(),
                    [src, callback, radius](auto&& t) mutable
                    {
                        if (in_range_3d(src, t, radius))
                            callback(t->GetOwner());
                    });
            });
    }
};

// Typedefs for common usage cases
// X_all_t: visits all game entities (i.e. everything but Camera)

using simple_all_t =
    simple<Player, Creature, Pet, GameObject, DynamicObject, Corpse>;
}
}

#endif
