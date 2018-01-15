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

#include "PointMovementGenerator.h"
#include "Creature.h"
#include "CreatureAI.h"
#include "CreatureGroupMgr.h"
#include "Pet.h"
#include "TemporarySummon.h"
#include "World.h"
#include "pet_behavior.h"
#include "MoveSpline.h"
#include "MoveSplineInit.h"

namespace movement
{
void PointMovementGenerator::start()
{
    if (owner_->hasUnitState(UNIT_STAT_CAN_NOT_REACT | UNIT_STAT_NOT_MOVE))
        return;

    if (!owner_->IsStopped())
        owner_->StopMoving();
    i_waitingPathId = 0;

    owner_->addUnitState(UNIT_STAT_ROAMING | UNIT_STAT_ROAMING_MOVE);

    if (m_generatePath)
    {
        std::vector<G3D::Vector3> path;
        uint32 id = movement::BuildRetailLikePath(
            path, owner_, G3D::Vector3(i_x, i_y, i_z));
        if (!id)
        {
            i_waitingPathId = 0;
            finished_path(std::move(path), 0);
        }
        else
        {
            i_waitingPathId = id;
        }
    }
    else
    {
        movement::MoveSplineInit init(*owner_);
        init.MoveTo(i_x, i_y, i_z);
        init.SetWalk(!i_run);
        init.Launch();
    }
}

void PointMovementGenerator::finished_path(
    std::vector<G3D::Vector3> path, uint32 id)
{
    if (i_waitingPathId != id)
        return;

    i_waitingPathId = 0;

    // Ignore queued path if something now prevents our moving
    if (!owner_->isAlive() || owner_->IsAffectedByThreatIgnoringCC() ||
        owner_->hasUnitState(UNIT_STAT_NOT_MOVE))
        return;

    // If no path was found, teleport to destination
    if (path.empty())
    {
        owner_->NearTeleportTo(i_x, i_y, i_z, owner_->GetO());
        return;
    }

    movement::MoveSplineInit init(*owner_);
    init.MovebyPath(path);
    init.SetWalk(!i_run);
    init.Launch();
}

void PointMovementGenerator::stop()
{
    owner_->clearUnitState(UNIT_STAT_ROAMING | UNIT_STAT_ROAMING_MOVE);
}

void PointMovementGenerator::finished()
{
    if (!owner_->movespline->Finalized() || i_waitingPathId != 0)
        return;

    if (owner_->GetTypeId() == TYPEID_PLAYER)
        return;

    auto cowner = static_cast<Creature*>(owner_);

    if (id_ == GWP_POINT_ID)
    {
        if (cowner->GetGroup() != nullptr)
            cowner->GetMap()->GetCreatureGroupMgr().ProcessGroupEvent(
                cowner->GetGroup()->GetId(),
                CREATURE_GROUP_EVENT_MOVEMENT_UPDATE, cowner);
        return;
    }

    if (cowner->AI())
        cowner->AI()->MovementInform(movement::gen::point, id_);

    if (cowner->IsTemporarySummon())
    {
        TemporarySummon* pSummon = (TemporarySummon*)cowner;
        if (pSummon->GetSummonerGuid().IsCreature())
            if (Creature* pSummoner =
                    owner_->GetMap()->GetCreature(pSummon->GetSummonerGuid()))
                if (pSummoner->AI())
                    pSummoner->AI()->SummonedMovementInform(
                        cowner, movement::gen::point, id_);
    }
}

bool PointMovementGenerator::update(uint32, uint32)
{
    if (owner_->hasUnitState(UNIT_STAT_CAN_NOT_MOVE))
    {
        owner_->clearUnitState(UNIT_STAT_ROAMING_MOVE);
        return false;
    }

    // Don't process further if waiting for a concurrent path generation request
    // to finish
    if (i_waitingPathId != 0)
        return false;

    if (!owner_->hasUnitState(UNIT_STAT_ROAMING_MOVE) &&
        owner_->movespline->Finalized())
        start();

    return owner_->movespline->Finalized();
}

std::string PointMovementGenerator::debug_str() const
{
    return "target loc: (" + std::to_string(i_x) + ", " + std::to_string(i_y) +
           ", " + std::to_string(i_z) + "); id: " + std::to_string(id_);
}

bool FallMovementGenerator::update(uint32, uint32)
{
    return owner_->movespline->Finalized();
}

void FallMovementGenerator::start()
{
    float tz = owner_->GetMap()->GetHeight(
        owner_->GetX(), owner_->GetY(), owner_->GetZ());
    if (tz <= INVALID_HEIGHT)
    {
        LOG_DEBUG(logging,
            "Unit::FallMovement: unable retrive a proper height at map %u "
            "(x: %f, y: %f, z: %f).",
            owner_->GetMap()->GetId(), owner_->GetX(), owner_->GetX(),
            owner_->GetZ());
        return;
    }

    // Abort too if the ground is very near
    if (fabs(owner_->GetZ() - tz) < 0.1f)
        return;

    owner_->m_movementInfo.AddMovementFlag(MOVEFLAG_GRAVITY);
    if (owner_->GetTypeId() == TYPEID_PLAYER)
        owner_->m_movementInfo.fallTime = 0;

    movement::MoveSplineInit init(*owner_);
    init.MoveTo(owner_->GetX(), owner_->GetY(), tz);
    init.SetFall();
    init.Launch();
}

void FallMovementGenerator::stop()
{
    owner_->m_movementInfo.RemoveMovementFlag(MOVEFLAG_GRAVITY);
}

void FallMovementGenerator::finished()
{
    if (owner_->GetTypeId() != TYPEID_UNIT)
        return;

    if (((Creature*)owner_)->AI() && owner_->movespline->Finalized())
        ((Creature*)owner_)->AI()->MovementInform(movement::gen::fall, 0);
}

void ChargeMovementGenerator::start()
{
    if (!owner_->IsStopped())
        owner_->StopMoving();

    if (owner_->GetTypeId() == TYPEID_UNIT)
    {
        if (!static_cast<Creature*>(owner_)->IsPet())
            owner_->AttackStop(true);
        owner_->SetTargetGuid(chargedTarget_);
    }

    owner_->addUnitState(UNIT_STAT_ROAMING | UNIT_STAT_ROAMING_MOVE);

    movement::MoveSplineInit init(*owner_);
    init.MovebyPath(path_);
    init.SetWalk(false);
    if (speed_)
        init.SetVelocity(speed_);

    init.Launch();
}

void ChargeMovementGenerator::stop()
{
    owner_->clearUnitState(UNIT_STAT_ROAMING | UNIT_STAT_ROAMING_MOVE);
}

void ChargeMovementGenerator::finished()
{
    if (owner_->GetTypeId() == TYPEID_UNIT)
    {
        Creature* c = (Creature*)owner_;
        if (c->AI() && owner_->movespline->Finalized())
        {
            c->AI()->MovementInform(movement::gen::charge,
                chargedTarget_
                    .GetCounter()); // Inform with low guid of charged mob
        }
    }
}

bool ChargeMovementGenerator::update(uint32, uint32)
{
    if (owner_->hasUnitState(UNIT_STAT_CAN_NOT_MOVE))
    {
        owner_->clearUnitState(UNIT_STAT_ROAMING_MOVE);
        return false;
    }

    if (owner_->GetTypeId() == TYPEID_UNIT)
        owner_->SetTargetGuid(chargedTarget_);

    owner_->addUnitState(UNIT_STAT_ROAMING_MOVE);
    return owner_->movespline->Finalized();
}
}
