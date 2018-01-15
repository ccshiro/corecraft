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

#ifndef GAME__MOVEMENT__CONFUSED_MOVEMENT_GENERATOR_H
#define GAME__MOVEMENT__CONFUSED_MOVEMENT_GENERATOR_H

#include "generators.h"
#include "Timer.h"

class AuraHolder;

namespace movement
{
class MANGOS_DLL_SPEC ConfusedMovementGenerator : public Generator
{
public:
    ConfusedMovementGenerator(AuraHolder* aura = nullptr)
      : aura_(aura), dir_(false)
    {
    }

    gen id() const override { return gen::confused; }

    // Not nullptr if owned by AuraHolder
    AuraHolder* holder() const { return aura_; }

    void start() override;
    void stop() override;
    void pushed() override;
    void removed(bool solo) override;
    bool update(uint32 diff, uint32) override;

    bool prevents_control() const override { return true; }

private:
    void do_movement(bool);

    AuraHolder* aura_;
    ShortTimeTracker timer_;
    G3D::Vector3 pos_;
    bool dir_;
};
}

#endif
