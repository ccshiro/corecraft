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

#include "IdleMovementGenerator.h"
#include "Creature.h"
#include "CreatureAI.h"
#include "MoveSpline.h"
#include "MoveSplineInit.h"
#include "Player.h"

static auto& idle_logger = logging.get_logger("movegens.idle");

namespace movement
{
void IdleMovementGenerator::pushed()
{
    if (x_ == 0 && y_ == 0 && z_ == 0)
    {
        owner_->GetPosition(x_, y_, z_);
        o_ = owner_->GetO();
    }
}

void IdleMovementGenerator::start()
{
    if (owner_->GetTypeId() != TYPEID_UNIT)
        return;

    waiting_path_id_ = 0;

    float x, y, z;
    owner_->GetPosition(x, y, z);
    if (x != x_ || y_ != y || z != z)
    {
        LOG_DEBUG(idle_logger,
            "%s not at idle position, moving to pos (%.2f, %.2f, %.2f, %.2f)",
            owner_->GetObjectGuid().GetString().c_str(), x_, y_, z_, o_);

        std::vector<G3D::Vector3> path;
        uint32 id = movement::BuildRetailLikePath(
            path, owner_, G3D::Vector3(x_, y_, z_));
        if (!id)
        {
            waiting_path_id_ = 0;
            finished_path(std::move(path), 0);
        }
        else
            waiting_path_id_ = id;
    }
    else if (o_ != owner_->GetO())
    {
        LOG_DEBUG(idle_logger,
            "%s's orientation not matching idle orientation, turning to %f",
            owner_->GetObjectGuid().GetString().c_str(), o_);
        owner_->SetFacingTo(o_);
    }
    else
    {
        owner_->StopMoving();
    }
}

void IdleMovementGenerator::stop()
{
    if (owner_->GetTypeId() == TYPEID_UNIT &&
        !static_cast<Creature*>(owner_)->IsPlayerPet() && !owner_->IsStopped())
        owner_->StopMoving();
}

std::string IdleMovementGenerator::debug_str() const
{
    return "idle loc: (" + std::to_string(x_) + ", " + std::to_string(y_) +
           ", " + std::to_string(z_) + ", " + std::to_string(o_) + ")";
}

void IdleMovementGenerator::finished_path(
    std::vector<G3D::Vector3> path, uint32 id)
{
    if (waiting_path_id_ != id)
        return;

    waiting_path_id_ = 0;

    movement::MoveSplineInit init(*owner_);
    // We need to get back to idle position; cheat if we can't get a path
    if (path.empty())
        init.MoveTo(x_, y_, z_);
    else
        init.MovebyPath(path);

    init.SetWalk(false);
    init.SetFacing(o_);

    init.Launch();
}

void FaceMovementGenerator::start()
{
    movement::MoveSplineInit init(*owner_);
    init.SetFacing(o_);
    init.Launch();
}

bool FaceMovementGenerator::update(uint32, uint32)
{
    return owner_->movespline->Finalized();
}

void DistractMovementGenerator::start()
{
    owner_->addUnitState(UNIT_STAT_DISTRACTED);
}

void DistractMovementGenerator::removed(bool solo)
{
    if (!solo)
        return;
    owner_->clearUnitState(UNIT_STAT_DISTRACTED);
}

bool DistractMovementGenerator::update(uint32, uint32 top_diff)
{
    if (top_diff >= m_timer)
        return true;

    m_timer -= top_diff;
    return false;
}

void StunMovementGenerator::start()
{
    owner_->StopMoving();

    if (owner_->GetTypeId() == TYPEID_UNIT)
    {
        owner_->addUnitState(UNIT_STAT_CANNOT_ROTATE);
        static_cast<Creature*>(owner_)->SetKeepTargetEmptyDueToCC(true);
    }

    owner_->Root(true);
    owner_->addUnitState(UNIT_STAT_STUNNED);
    owner_->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_STUNNED);
}

void StunMovementGenerator::removed(bool solo)
{
    if (!solo)
        return;

    if (owner_->GetTypeId() == TYPEID_UNIT)
    {
        owner_->clearUnitState(UNIT_STAT_CANNOT_ROTATE);
        static_cast<Creature*>(owner_)->SetKeepTargetEmptyDueToCC(false);
    }

    if (!owner_->hasUnitState(UNIT_STAT_ROOT))
        owner_->Root(false);
    owner_->clearUnitState(UNIT_STAT_STUNNED);
    owner_->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_STUNNED);
}

void RootMovementGenerator::start()
{
    owner_->StopMoving();
    owner_->Root(true);
    owner_->addUnitState(UNIT_STAT_ROOT);
}

void RootMovementGenerator::removed(bool solo)
{
    if (!solo)
        return;

    if (!owner_->hasUnitState(UNIT_STAT_STUNNED))
        owner_->Root(false);
    owner_->clearUnitState(UNIT_STAT_ROOT);
}

ControlledMovementGenerator::ControlledMovementGenerator(
    Unit* caster, AuraHolder* aura)
  : caster_(caster->GetObjectGuid()), aura_(aura), caster_steering_(false)
{
}

void ControlledMovementGenerator::pushed()
{
    owner_->StopMoving();

    if (auto caster = owner_->GetMap()->GetUnit(caster_))
    {
        if (caster->GetTypeId() == TYPEID_PLAYER)
        {
            if (static_cast<Player*>(caster)->GetMovingUnit() == owner_)
            {
                static_cast<Player*>(caster)->SetClientControl(owner_, 1);
                static_cast<Player*>(caster)->SetClientControl(caster, 0);
                caster_steering_ = true;
            }
            else if (owner_->GetTypeId() == TYPEID_PLAYER)
            {
                static_cast<Player*>(owner_)->SetClientControl(owner_, 0);
                caster_steering_ = false;
            }
        }
    }
}

void ControlledMovementGenerator::removed(bool solo)
{
    if (!solo)
        return;
    if (auto caster = owner_->GetMap()->GetUnit(caster_))
    {
        if (caster->GetTypeId() == TYPEID_PLAYER)
        {
            if (caster_steering_)
            {
                static_cast<Player*>(caster)->SetClientControl(owner_, 0);
                static_cast<Player*>(caster)->SetClientControl(caster, 1);
            }
            else if (owner_->GetTypeId() == TYPEID_PLAYER)
                static_cast<Player*>(owner_)->SetClientControl(owner_, 0);
        }
    }
}

void StoppedMovementGenerator::start()
{
    owner_->StopMoving();
}

void StoppedMovementGenerator::finished()
{
    if (id_ < 0 && timer_ == 0)
        owner_->movement_gens.queue_movement_inform(id(), -id_);
    else if (id_ > 0)
        owner_->movement_gens.queue_movement_inform(id(), id_);
}

bool StoppedMovementGenerator::update(uint32 diff, uint32)
{
    if (unlikely(timer_))
    {
        if (timer_ <= diff)
        {
            timer_ = 0;
            return true;
        }
        timer_ -= diff;
    }

    return false;
}

std::string StoppedMovementGenerator::debug_str() const
{
    if (timer_)
        return "removed in: " + std::to_string(timer_);
    return "";
}
}
