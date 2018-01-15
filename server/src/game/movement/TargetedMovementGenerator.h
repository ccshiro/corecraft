/*
 * Copyright (C) 2005-2012 MaNGOS <http://getmangos.com/>
 * Copyright (C) 2015 corecraft <https://www.worldofcorecraft.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef GAME__MOVEMENT__TARGETED_MOVEMENT_GENERATOR_H
#define GAME__MOVEMENT__TARGETED_MOVEMENT_GENERATOR_H

#include "generators.h"
#include "Pet.h"
#include "Unit.h"

#define CHARGE_DEFAULT_SPEED 42.0f

namespace movement
{
class MANGOS_DLL_SPEC ChaseMovementGenerator : public Generator
{
public:
    ChaseMovementGenerator(ObjectGuid enemy = ObjectGuid());

    gen id() const override { return gen::chase; }

    void start() override;
    void stop() override;
    bool update(uint32 diff, uint32) override;

    void finished_path(std::vector<G3D::Vector3> path, uint32 id) override;

    Unit* get_target() const;

    std::string debug_str() const override;

private:
    enum class State
    {
        relaxing,
        chasing,
        pausing,
        backing,
        spreading
    };
    const char* stringify(State s) const
    {
        switch (s)
        {
        case State::relaxing:
            return "relaxing";
        case State::chasing:
            return "chasing";
        case State::pausing:
            return "pausing";
        case State::backing:
            return "backing";
        case State::spreading:
            return "spreading";
        default:
            return "invalid";
        }
    }

    State state_;
    int spread_timer_;
    int back_timer_;
    float speed_;
    ObjectGuid target_;
    ObjectGuid current_target_;
    G3D::Vector3 target_last_pos_;
    uint32 waiting_path_id_;
    std::vector<G3D::Vector3> path_;
    bool casted_spell_;
    bool evading_;
    bool run_mode_;

    void set_state(State state);
    void set_target_loc(Unit* target);
    void handle_path_update();
    void do_back_movement(Unit* target);
    void do_spread_if_needed(Unit* target);
    void spread_move(Unit* target, const G3D::Vector3& pos);
    bool target_oor(Unit* target) const;
    bool target_in_bounds(Unit* target) const;
    bool target_deep_in_bounds(Unit* target) const;
    bool target_bounds_pct_dist(Unit* target, float pct) const;
};

class MANGOS_DLL_SPEC FollowMovementGenerator : public Generator
{
public:
    FollowMovementGenerator(Unit* target, float dist = PET_FOLLOW_DIST,
        float angle = PET_FOLLOW_ANGLE);

    gen id() const override { return gen::follow; }

    void start() override;
    void stop() override;
    bool update(uint32 diff, uint32) override;

    void finished_path(std::vector<G3D::Vector3> path, uint32 id) override;

private:
    enum class State
    {
        relaxing,
        following
    };

    State state_;
    float speed_;
    ObjectGuid target_;
    uint32 waiting_path_id_;
    std::vector<G3D::Vector3> path_;
    float offset_;
    float angle_;
    float last_x_;
    float last_y_;
    G3D::Vector3 target_last_pos_; // not same as last_x/y_
    G3D::Vector3 end_;
    bool casted_spell_;
    bool evading_;
    bool target_was_moving_;

    bool target_oor(Unit* target) const;
    void set_target_loc(Unit* target);
    void handle_path_update();
};
}

#endif
