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

#ifndef GAME__MOVEMENT__RANDOM_MOVEMENT_GENERATOR_H
#define GAME__MOVEMENT__RANDOM_MOVEMENT_GENERATOR_H

#include "generators.h"
#include "Timer.h"

namespace movement
{
class MANGOS_DLL_SPEC RandomMovementGenerator : public Generator
{
public:
    RandomMovementGenerator(
        float distance = 0, G3D::Vector3 center = G3D::Vector3(0, 0, 0))
      : i_center(center), i_nextMoveTime(0), i_distance(distance),
        i_castedSpell(false)
    {
    }

    gen id() const override { return gen::random; }

    void _setRandomLocation();
    void pushed() override;
    void start() override;
    void stop() override;
    bool update(uint32 diff, uint32) override;
    std::string debug_str() const override;

private:
    G3D::Vector3 i_center; // center of random movement
    ShortTimeTracker i_nextMoveTime;
    float i_distance;
    bool i_castedSpell;
};

class MANGOS_DLL_SPEC RandomWaterAirMovementGenerator : public Generator
{
public:
    RandomWaterAirMovementGenerator(
        float distance = 0, G3D::Vector3 center = G3D::Vector3(0, 0, 0))
      : center_(center), dist_(distance), casted_spell_(false)
    {
    }

    gen id() const override { return gen::random_waterair; }

    void pushed() override;
    void start() override;
    void stop() override;
    bool update(uint32 diff, uint32) override;

private:
    G3D::Vector3 center_; // center of random movement
    ShortTimeTracker timer_;
    float dist_;
    bool casted_spell_;

    void set_point();
};
}

#endif
