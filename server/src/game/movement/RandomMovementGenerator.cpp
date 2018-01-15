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

#include "RandomMovementGenerator.h"
#include "Creature.h"
#include "Map.h"
#include "MapManager.h"
#include "Util.h"
#include "MoveSpline.h"
#include "MoveSplineInit.h"

static auto& logger = logging.get_logger("movegens.random");
static auto& wa_logger = logging.get_logger("movegens.random_waterair");

namespace movement
{
void RandomMovementGenerator::_setRandomLocation()
{
    float wander_distance;
    if (i_distance)
        wander_distance = i_distance;
    else
        wander_distance =
            owner_->GetTypeId() == TYPEID_UNIT ?
                static_cast<Creature*>(owner_)->GetRespawnRadius() :
                5.0f;

    const float angle = rand_norm_f() * (M_PI_F * 2.0f);
    const float range = rand_norm_f() * wander_distance;

    owner_->addUnitState(UNIT_STAT_ROAMING_MOVE);

    auto pos = owner_->GetPointXYZ(i_center, angle, range, true);

    movement::MoveSplineInit init(*owner_);
    init.MoveTo(pos);
    init.SetWalk(true);
    init.Launch();

    // 1/3 chance to move instantly again
    if (!urand(0, 2))
        i_nextMoveTime.Reset(0);
    else
        i_nextMoveTime.Reset(urand(5000, 10000));

    LOG_DEBUG(logger,
        "%s moving to new point. Dist: %f, angle: %f, range: %f, next point: "
        "%u, center: (%.2f, %.2f, %.2f), pos: (%.2f, %.2f, %.2f)",
        owner_->GetObjectGuid().GetString().c_str(), wander_distance, angle,
        range, i_nextMoveTime.GetExpiry(), i_center.x, i_center.y, i_center.z,
        pos.x, pos.y, pos.z);
}

void RandomMovementGenerator::pushed()
{
    if (i_center.x == 0 && i_center.y == 0 && i_center.z == 0)
        owner_->GetPosition(i_center.x, i_center.y, i_center.z);
}

void RandomMovementGenerator::start()
{
    if (!owner_->isAlive())
        return;

    owner_->addUnitState(UNIT_STAT_ROAMING | UNIT_STAT_ROAMING_MOVE);
    _setRandomLocation();
}

void RandomMovementGenerator::stop()
{
    owner_->clearUnitState(UNIT_STAT_ROAMING | UNIT_STAT_ROAMING_MOVE);
    if (owner_->GetTypeId() == TYPEID_UNIT)
        static_cast<Creature*>(owner_)->SetWalk(false);
}

bool RandomMovementGenerator::update(uint32 diff, uint32)
{
    if (owner_->hasUnitState(UNIT_STAT_NOT_MOVE))
    {
        i_nextMoveTime.Reset(0); // Expire the timer
        owner_->clearUnitState(UNIT_STAT_ROAMING_MOVE);
        return false;
    }

    if (owner_->IsCastedSpellPreventingMovementOrAttack())
    {
        i_castedSpell = true;
        if (!owner_->IsStopped())
            owner_->StopMoving();
        return false;
    }
    else if (i_castedSpell)
    {
        i_castedSpell = false;
        if (owner_->IsStopped())
        {
            _setRandomLocation();
            return false;
        }
    }

    if (owner_->movespline->Finalized())
    {
        i_nextMoveTime.Update(diff);
        if (i_nextMoveTime.Passed())
            _setRandomLocation();
    }

    return false;
}

std::string RandomMovementGenerator::debug_str() const
{
    return "center loc: (" + std::to_string(i_center.x) + ", " +
           std::to_string(i_center.y) + ", " + std::to_string(i_center.z) + ")";
}

void RandomWaterAirMovementGenerator::set_point()
{
    float wander_distance;
    if (dist_)
        wander_distance = dist_;
    else
        wander_distance =
            owner_->GetTypeId() == TYPEID_UNIT ?
                static_cast<Creature*>(owner_)->GetRespawnRadius() :
                5.0f;

    const float angle = rand_norm_f() * (M_PI_F * 2.0f);
    const float range = rand_norm_f() * wander_distance;

    owner_->addUnitState(UNIT_STAT_ROAMING_MOVE);

    G3D::Vector3 pos = center_;
    pos.x = pos.x + range * cos(angle);
    pos.y = pos.y + range * sin(angle);

    movement::MoveSplineInit init(*owner_);
    init.MoveTo(pos);
    init.SetWalk(true);
    init.Launch();

    // 1/3 chance to move instantly again
    if (!urand(0, 2))
        timer_.Reset(0);
    else
        timer_.Reset(urand(5000, 10000));

    LOG_DEBUG(logger,
        "%s moving to new point. Dist: %f, angle: %f, range: %f, next point: "
        "%u, center: (%.2f, %.2f, %.2f), pos: (%.2f, %.2f, %.2f)",
        owner_->GetObjectGuid().GetString().c_str(), wander_distance, angle,
        range, timer_.GetExpiry(), center_.x, center_.y, center_.z, pos.x,
        pos.y, pos.z);
}

void RandomWaterAirMovementGenerator::pushed()
{
    if (center_.x == 0 && center_.y == 0 && center_.z == 0)
        owner_->GetPosition(center_.x, center_.y, center_.z);
}

void RandomWaterAirMovementGenerator::start()
{
    if (!owner_->isAlive())
        return;

    owner_->addUnitState(UNIT_STAT_ROAMING | UNIT_STAT_ROAMING_MOVE);
    set_point();
}

void RandomWaterAirMovementGenerator::stop()
{
    owner_->clearUnitState(UNIT_STAT_ROAMING | UNIT_STAT_ROAMING_MOVE);
    if (owner_->GetTypeId() == TYPEID_UNIT)
        static_cast<Creature*>(owner_)->SetWalk(false);
}

bool RandomWaterAirMovementGenerator::update(uint32 diff, uint32)
{
    if (owner_->hasUnitState(UNIT_STAT_NOT_MOVE))
    {
        timer_.Reset(0); // Start moving instantly when we can move again
        owner_->clearUnitState(UNIT_STAT_ROAMING_MOVE);
        return false;
    }

    if (owner_->IsCastedSpellPreventingMovementOrAttack())
    {
        casted_spell_ = true;
        if (!owner_->IsStopped())
            owner_->StopMoving();
        return false;
    }
    else if (casted_spell_)
    {
        casted_spell_ = false;
        if (owner_->IsStopped())
        {
            set_point();
            return false;
        }
    }

    if (owner_->movespline->Finalized())
    {
        timer_.Update(diff);
        if (timer_.Passed())
            set_point();
    }

    return false;
}
}
