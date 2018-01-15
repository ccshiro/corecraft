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

#include "FleeingMovementGenerator.h"
#include "Creature.h"
#include "CreatureAI.h"
#include "MapManager.h"
#include "ObjectAccessor.h"
#include "PathFinder.h"
#include "movement/MoveSpline.h"
#include "movement/MoveSplineInit.h"
#include "vmap/VMapFactory.h"

namespace movement
{
void FleeingMovementGenerator::_setTargetLocation()
{
    // ignore in case other no reaction state
    if (owner_->hasUnitState(UNIT_STAT_CAN_NOT_REACT & ~UNIT_STAT_FLEEING))
        return;

    ++i_ticks;
    float x, y, z;
    if (!_getPoint(x, y, z))
        return;

    owner_->addUnitState(UNIT_STAT_FLEEING_MOVE);

    movement::MoveSplineInit init(*owner_);
    init.MoveTo(G3D::Vector3(x, y, z));

    // Force velocity to use target's running speed (in case the target was
    // backing, MoveSplineInit::Launch would take the backwards speed)
    speed_ = owner_->GetSpeed(MOVE_RUN);
    init.SetVelocity(speed_);

    init.SetWalk(false);
    int32 traveltime = init.Launch();
    i_nextPointTime.Reset(traveltime + irand(800, 1500));
}

bool FleeingMovementGenerator::_getPoint(float& x, float& y, float& z)
{
    float angle = -1.0f;
    int dist;
    G3D::Vector3 pos;

    if (i_ticks <= 1)
        dist = urand(15, 20);
    else
        dist = urand(8, 16);

    // On the first tick we'll ensure we run away from the fear owner
    Unit* fright = owner_->GetMap()->GetUnit(i_frightGuid);
    if (fright && fright != owner_ && i_ticks <= 1)
    {
        angle = owner_->GetAngle(fright) + M_PI_F +
                frand(-M_PI_F / 8.0f, M_PI_F / 8.0f);
        angle -= owner_->GetO();
        pos = owner_->GetPoint(angle, dist,
            owner_->GetCharmerOrOwnerPlayerOrPlayerItself() != nullptr, false,
            true);
        // Go back to random if there's terrain blocking us from running away
        if (owner_->IsWithinDist3d(pos.x, pos.y, pos.z, 6.0f))
            angle = -1.0f;
    }

    if (angle == -1.0f)
    {
        angle = frand(0, 2 * M_PI_F);

        int tries = 0;
        while (
            tries < 4 &&
            (tries == 0 || owner_->IsWithinDist3d(pos.x, pos.y, pos.z, 6.0f)))
        {
            pos = owner_->GetPoint(angle - owner_->GetO(), dist,
                owner_->GetCharmerOrOwnerPlayerOrPlayerItself() != nullptr,
                false, true);
            angle += (M_PI_F / 4.0f) * tries++;
        }
    }

    x = pos.x;
    y = pos.y;
    z = pos.z;

    return true;
}

void FleeingMovementGenerator::pushed()
{
    owner_->addUnitState(UNIT_STAT_FLEEING | UNIT_STAT_FLEEING_MOVE);
    owner_->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_FLEEING);
    if (owner_->GetTypeId() == TYPEID_UNIT)
        owner_->SetTargetGuid(ObjectGuid());
}

void FleeingMovementGenerator::start()
{
    owner_->StopMoving();
    int32 prev_ticks = i_ticks;
    i_ticks = 0;

    // For fear auras that have a speed change component after the fear we
    // must wait for the speed change before calculating out first point
    if (prev_ticks == 0 && i_aura)
    {
        auto spell = i_aura->GetSpellProto();
        auto idx_fn = [spell](int i)
        {
            return spell->EffectApplyAuraName[i] ==
                       SPELL_AURA_MOD_INCREASE_SPEED ||
                   spell->EffectApplyAuraName[i] ==
                       SPELL_AURA_MOD_DECREASE_SPEED;
        };
        // check if fear comes before incr/decr of speed
        if ((spell->EffectApplyAuraName[0] == SPELL_AURA_MOD_FEAR &&
                (idx_fn(1) || idx_fn(2))) ||
            (spell->EffectApplyAuraName[1] == SPELL_AURA_MOD_FEAR && idx_fn(2)))
        {
            speed_ = -1; // Guarantee speed not equal
            return;
        }
    }

    if (prev_ticks == 0)
        _setTargetLocation();
    else
        i_nextPointTime.Reset(400);
}

void FleeingMovementGenerator::stop()
{
    // flee state still applied while movegen disabled
    owner_->clearUnitState(UNIT_STAT_FLEEING_MOVE);
}

void FleeingMovementGenerator::removed(bool solo)
{
    if (!solo)
        return;

    owner_->clearUnitState(UNIT_STAT_FLEEING | UNIT_STAT_FLEEING_MOVE);
    owner_->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_FLEEING);

    if (owner_->GetTypeId() == TYPEID_PLAYER)
        owner_->StopMoving();

    if (owner_->GetTypeId() == TYPEID_UNIT && owner_->getVictim() &&
        !owner_->hasUnitState(UNIT_STAT_STUNNED))
        owner_->SetTargetGuid(owner_->getVictim()->GetObjectGuid());
}

bool FleeingMovementGenerator::update(uint32 time_diff, uint32)
{
    if (!owner_->isAlive())
        return true;

    // ignore in case other no reaction state
    if (owner_->hasUnitState(UNIT_STAT_CAN_NOT_REACT & ~UNIT_STAT_FLEEING))
    {
        owner_->clearUnitState(UNIT_STAT_FLEEING_MOVE);
        return false;
    }

    i_nextPointTime.Update(time_diff);
    if (speed_ != owner_->GetSpeed(MOVE_RUN) ||
        (i_nextPointTime.Passed() && owner_->movespline->Finalized()))
        _setTargetLocation();

    return false;
}

void RunInFearMovementGenerator::start()
{
    FleeingMovementGenerator::start();
}

void RunInFearMovementGenerator::stop()
{
    FleeingMovementGenerator::stop();
}

void RunInFearMovementGenerator::removed(bool solo)
{
    FleeingMovementGenerator::removed(solo);
}

bool RunInFearMovementGenerator::update(uint32 diff, uint32 top_diff)
{
    if (time_remaining_ <= top_diff)
        return true;
    else
        time_remaining_ -= top_diff;

    return FleeingMovementGenerator::update(diff, top_diff);
}
}
