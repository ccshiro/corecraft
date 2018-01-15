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

#include "HomeMovementGenerator.h"
#include "Creature.h"
#include "CreatureAI.h"
#include "CreatureGroupMgr.h"
#include "Map.h"
#include "MoveSpline.h"
#include "MoveSplineInit.h"
#include "ObjectMgr.h"
#include "WorldPacket.h"

static auto& logger = logging.get_logger("movegens.home");

namespace movement
{
void HomeMovementGenerator::pushed()
{
    if (x_ == 0 && y_ == 0 && z_ == 0)
    {
        owner_->GetPosition(x_, y_, z_);
        o_ = owner_->GetO();
    }
}

void HomeMovementGenerator::_setTargetLocation()
{
    if (owner_->hasUnitState(UNIT_STAT_NOT_MOVE) || waiting_path_id_ != 0)
        return;

    if (owner_->GetX() == x_ && owner_->GetY() == y_ && owner_->GetZ() == z_ &&
        owner_->GetO() == o_)
        return;

    LOG_DEBUG(logger, "%s moving to pos (%.2f, %.2f, %.2f, %.2f)",
        owner_->GetObjectGuid().GetString().c_str(), x_, y_, z_, o_);

    std::vector<G3D::Vector3> path;
    uint32 id =
        movement::BuildRetailLikePath(path, owner_, G3D::Vector3(x_, y_, z_));
    if (!id)
    {
        waiting_path_id_ = 0;
        finished_path(std::move(path), 0);
    }
    else
        waiting_path_id_ = id;
}

void HomeMovementGenerator::finished_path(
    std::vector<G3D::Vector3> path, uint32 id)
{
    if (waiting_path_id_ != id)
        return;

    waiting_path_id_ = 0;

    movement::MoveSplineInit init(*owner_);
    // If we can't draw a path home, we're allowed to cheat
    if (path.empty())
    {
        owner_->NearTeleportTo(x_, y_, z_, o_);
        return;
    }
    else
    {
        // Make sure final position is exactly equal to our x_, y_, z_
        path.back().x = x_;
        path.back().y = y_;
        path.back().z = z_;
        init.MovebyPath(path);
    }

    init.SetWalk(false);
    init.SetFacing(o_);

    init.Launch();
}

void HomeMovementGenerator::start()
{
    waiting_path_id_ = 0;
    _setTargetLocation();
}

bool HomeMovementGenerator::update(uint32, uint32)
{
    arrived_ = waiting_path_id_ == 0 && owner_->movespline->Finalized();
    return arrived_;
}

void HomeMovementGenerator::finished()
{
    if (owner_->GetTypeId() != TYPEID_UNIT)
        return;

    auto cowner = static_cast<Creature*>(owner_);

    // Remove active state of creature (only do so if we are the cause to
    // him being an active object, aka IsCreatureInCombatList() returning true)
    if (cowner->GetMap()->IsCreatureInCombatList(cowner))
    {
        owner_->SetActiveObjectState(false);
        owner_->GetMap()->CreatureLeaveCombat(cowner);
    }

    if (arrived_)
    {
        if (cowner->GetTemporaryFactionFlags() & TEMPFACTION_RESTORE_REACH_HOME)
            cowner->ClearTemporaryFaction();

        // Reset health and mana to curmana and curhealth
        auto info = cowner->GetCreatureInfo();
        auto data =
            sObjectMgr::Instance()->GetCreatureData(owner_->GetGUIDLow());
        if (info && data)
        {
            if (data->curhealth != 0)
                owner_->SetHealth(data->curhealth);
            if (!(data->curmana == 0 && info->RegenMana == true))
                owner_->SetPower(POWER_MANA, data->curmana);
        }

        cowner->LoadCreatureAddon(true);
        cowner->AI()->JustReachedHome();
    }
}
}
