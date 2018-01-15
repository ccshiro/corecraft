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

#ifndef GAME__MOVEMENT__HOME_MOVEMENT_GENERATOR_H
#define GAME__MOVEMENT__HOME_MOVEMENT_GENERATOR_H

#include "generators.h"

namespace movement
{
class MANGOS_DLL_SPEC HomeMovementGenerator : public Generator
{
public:
    HomeMovementGenerator(float x = 0, float y = 0, float z = 0, float o = 0)
      : x_(x), y_(y), z_(z), o_(o), waiting_path_id_(0), arrived_(false)
    {
    }

    ~HomeMovementGenerator() {}

    gen id() const override { return gen::home; }

    void pushed() override;
    void start() override;
    bool update(uint32 diff, uint32) override;
    void finished() override;

    void finished_path(std::vector<G3D::Vector3> path, uint32 id) override;

    void get_combat_start_pos(float& x, float& y, float& z)
    {
        x = x_;
        y = y_;
        z = z_;
    }

private:
    void _setTargetLocation();
    float x_;
    float y_;
    float z_;
    float o_;
    uint32 waiting_path_id_;
    bool arrived_;
};
}

#endif
