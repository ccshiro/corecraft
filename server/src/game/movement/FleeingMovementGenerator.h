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

#ifndef GAME__MOVEMENT__FLEEING_MOVEMENT_GENERATOR_H
#define GAME__MOVEMENT__FLEEING_MOVEMENT_GENERATOR_H

#include "generators.h"
#include "ObjectGuid.h"
#include "Timer.h"

class AuraHolder;
class Creature;

namespace movement
{
class MANGOS_DLL_SPEC FleeingMovementGenerator : public Generator
{
public:
    FleeingMovementGenerator(ObjectGuid fright, AuraHolder* aura = nullptr)
      : i_aura(aura), i_frightGuid(std::move(fright)), speed_(0)
    {
    }

    gen id() const { return gen::fleeing; }

    void pushed() override;
    void start() override;
    void stop() override;
    void removed(bool solo) override;
    bool update(uint32 diff, uint32) override;

    bool prevents_control() const override { return true; }

    // Not nullptr if owned by AuraHolder
    AuraHolder* holder() const { return i_aura; }

private:
    void _setTargetLocation();
    bool _getPoint(float& x, float& y, float& z);

    AuraHolder* i_aura;
    int i_ticks;
    ObjectGuid i_frightGuid;
    ShortTimeTracker i_nextPointTime;
    float speed_;
};

class MANGOS_DLL_SPEC RunInFearMovementGenerator
    : public FleeingMovementGenerator
{
public:
    RunInFearMovementGenerator(ObjectGuid enemy, uint32 time)
      : FleeingMovementGenerator(enemy), time_remaining_(time)
    {
    }

    gen id() const { return gen::run_in_fear; }

    void start() override;
    void stop() override;
    void removed(bool solo) override;
    bool update(uint32 diff, uint32 top_diff) override;

private:
    uint32 time_remaining_;
};
}

#endif
