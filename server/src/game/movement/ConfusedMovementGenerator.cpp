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

#include "ConfusedMovementGenerator.h"
#include "Creature.h"
#include "MapManager.h"
#include "PathFinder.h"
#include "Player.h"
#include "SpellAuras.h"
#include "movement/generators.h"
#include "MoveSpline.h"
#include "MoveSplineInit.h"
#include "vmap/VMapFactory.h"

static auto& logger = logging.get_logger("movegens.confused");

namespace movement
{
void ConfusedMovementGenerator::start()
{
    // Initial position is center of movement
    if (owner_->GetTransport())
        owner_->m_movementInfo.transport.pos.Get(pos_.x, pos_.y, pos_.z);
    else
        owner_->GetPosition(pos_.x, pos_.y, pos_.z);
    owner_->StopMoving();

    timer_.Reset(1);
    update(1, 0);

    owner_->addUnitState(UNIT_STAT_CONFUSED_MOVE);
}

void ConfusedMovementGenerator::stop()
{
    owner_->clearUnitState(UNIT_STAT_CONFUSED_MOVE);
    if (owner_->GetTypeId() == TYPEID_PLAYER)
        owner_->StopMoving();
}

void ConfusedMovementGenerator::pushed()
{
    owner_->addUnitState(UNIT_STAT_CONFUSED | UNIT_STAT_CONFUSED_MOVE);
    owner_->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_CONFUSED);
}

void ConfusedMovementGenerator::removed(bool solo)
{
    if (!solo)
        return;
    owner_->clearUnitState(UNIT_STAT_CONFUSED);
    owner_->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_CONFUSED);
}

bool ConfusedMovementGenerator::update(uint32 diff, uint32)
{
    if (owner_->hasUnitState(
            UNIT_STAT_ROOT | UNIT_STAT_STUNNED | UNIT_STAT_DISTRACTED))
        return false;

    // ignore in case other no reaction state
    if (owner_->hasUnitState(UNIT_STAT_CAN_NOT_REACT & ~UNIT_STAT_CONFUSED))
        return false;

    // Wait for the movement to end
    if (!owner_->movespline->Finalized())
        return false;

    timer_.Update(diff);
    if (timer_.Passed())
    {
        // Figure out if this is a big or small confuse path
        bool smallPath = false;
        auto& confuseAuras = owner_->GetAurasByType(SPELL_AURA_MOD_CONFUSE);
        for (const auto& confuseAura : confuseAuras)
        {
            SpellEntry const* entry = (confuseAura)->GetSpellProto();
            if (entry->EffectBasePoints[(confuseAura)->GetEffIndex()] == 0)
            {
                smallPath = true;
                break;
            }
        }

        do_movement(smallPath);

        // Big paths (polymorph) require short sleeps in between movements
        if (!smallPath)
            timer_.Reset(urand(800, 1000));
    }

    return false;
}
void ConfusedMovementGenerator::do_movement(bool smallPath)
{
    float range;
    float angle;

    if (smallPath)
        range = 1.2;
    else
        range = frand(2.5, 3.0);

    // Direction should swap between every movement but still be somewhat random
    if ((dir_ = /*=*/!dir_))
        angle = frand(0.1 * M_PI_F, 0.9 * M_PI_F);
    else
        angle = frand(-0.9 * M_PI_F, -0.1 * M_PI_F);

    auto pos = owner_->GetPointXYZ(
        pos_, angle, range, owner_->GetTypeId() == TYPEID_PLAYER, false, true);

    LOG_DEBUG(logger, "%s moving to new point at (%f, %f, %f); small path: %s",
        owner_->GetObjectGuid().GetString().c_str(), pos.x, pos.y, pos.z,
        smallPath ? "yes" : "no");

    movement::MoveSplineInit init(*owner_);
    init.MoveTo(pos);
    init.SetWalk(true);
    init.Launch();
}
}
