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

/*
creature_movement Table

alter table creature_movement add `textid1` int(11) NOT NULL default '0';
alter table creature_movement add `textid2` int(11) NOT NULL default '0';
alter table creature_movement add `textid3` int(11) NOT NULL default '0';
alter table creature_movement add `textid4` int(11) NOT NULL default '0';
alter table creature_movement add `textid5` int(11) NOT NULL default '0';
alter table creature_movement add `emote` int(10) unsigned default '0';
alter table creature_movement add `spell` int(5) unsigned default '0';
alter table creature_movement add `wpguid` int(11) default '0';

*/

#include "WaypointMovementGenerator.h"
#include "Creature.h"
#include "CreatureAI.h"
#include "ObjectMgr.h"
#include "ScriptMgr.h"
#include "WorldPacket.h"
#include "MoveSpline.h"
#include "MoveSplineInit.h"
#include "movement/WaypointManager.h"
#include <cassert>
#include <ctime>

namespace movement
{
void WaypointMovementGenerator::LoadPath()
{
    assert(owner_);
    if (i_loaded)
    {
        if (i_path)
        {
            if (CanMove(0))
                StartMove();
        }
        return;
    }

    LOG_DEBUG(logging, "LoadPath: loading waypoint path for %s",
        owner_->GetGuidStr().c_str());

    i_path = sWaypointMgr::Instance()->GetPath(owner_->GetGUIDLow());

    // We may LoadPath() for several occasions:

    // 1: When owner_->MovementType=2
    //    1a) Path is selected by owner_->guid == creature_movement.id
    //    1b) Path for 1a) does not exist and then use path from
    //    owner_->GetEntry() == creature_movement_template.entry

    // 2: When creature_template.MovementType=2
    //    2a) Creature is summoned and has creature_template.MovementType=2
    //        Creators need to be sure that creature_movement_template is always
    //        valid for summons.
    //        Mob that can be summoned anywhere should not have
    //        creature_movement_template for example.

    // No movement found for guid
    if (!i_path)
    {
        i_path = sWaypointMgr::Instance()->GetPathTemplate(owner_->GetEntry());

        // No movement found for entry
        if (!i_path)
        {
            logging.error(
                "WaypointMovementGenerator::LoadPath: creature %s (Entry: %u "
                "GUID: %u) doesn't have waypoint path",
                owner_->GetName(), owner_->GetEntry(), owner_->GetGUIDLow());
            return;
        }
    }

    i_loaded = true;

    i_nextMoveTime.Reset(0);
    StartMove();
}

void WaypointMovementGenerator::start()
{
    owner_->addUnitState(UNIT_STAT_ROAMING | UNIT_STAT_ROAMING_MOVE);
    LoadPath();
}

void WaypointMovementGenerator::stop()
{
    owner_->clearUnitState(UNIT_STAT_ROAMING | UNIT_STAT_ROAMING_MOVE);
}

void WaypointMovementGenerator::OnArrived()
{
    if (!i_path || i_path->empty())
        return;

    if (m_isArrivalDone)
        return;

    owner_->clearUnitState(UNIT_STAT_ROAMING_MOVE);
    m_isArrivalDone = true;

    if (i_path->at(i_currentNode).script_id)
    {
        LOG_DEBUG(logging,
            "Creature movement start script %u at point %u for %s.",
            i_path->at(i_currentNode).script_id, i_currentNode,
            owner_->GetGuidStr().c_str());
        owner_->GetMap()->ScriptsStart(sCreatureMovementScripts,
            i_path->at(i_currentNode).script_id, owner_, owner_);
    }

    // We have reached the destination and can process behavior
    if (WaypointBehavior* behavior = i_path->at(i_currentNode).behavior)
    {
        if (behavior->emote != 0)
            owner_->HandleEmote(behavior->emote);

        if (behavior->spell != 0)
            owner_->CastSpell(owner_, behavior->spell, false);

        if (behavior->model1 != 0)
            owner_->SetDisplayId(behavior->model1);

        if (behavior->textid[0])
        {
            // Not only one text is set
            if (behavior->textid[1])
            {
                // Select one from max 5 texts (0 and 1 already checked)
                int i = 2;
                for (; i < MAX_WAYPOINT_TEXT; ++i)
                {
                    if (!behavior->textid[i])
                        break;
                }

                owner_->MonsterSay(
                    behavior->textid[urand(0, i - 1)], LANG_UNIVERSAL);
            }
            else
                owner_->MonsterSay(behavior->textid[0], LANG_UNIVERSAL);
        }
    }

    // Inform script
    owner_->movement_gens.queue_movement_inform(
        movement::gen::waypoint, i_currentNode);
    Stop(i_path->at(i_currentNode).delay);
}

void WaypointMovementGenerator::StartMove()
{
    if (!i_path || i_path->empty())
        return;

    if (Stopped())
        return;

    if (WaypointBehavior* behavior = i_path->at(i_currentNode).behavior)
    {
        if (behavior->model2 != 0)
            owner_->SetDisplayId(behavior->model2);
        owner_->SetUInt32Value(UNIT_NPC_EMOTESTATE, 0);
    }

    if (m_isArrivalDone)
        i_currentNode = (i_currentNode + 1) % i_path->size();

    m_isArrivalDone = false;

    owner_->addUnitState(UNIT_STAT_ROAMING_MOVE);

    const WaypointNode& node = i_path->at(i_currentNode);
    movement::MoveSplineInit init(*owner_);
    init.MoveTo(node.x, node.y, node.z);

    if (node.orientation != 100 && node.delay != 0)
        init.SetFacing(node.orientation);

    init.SetWalk(!i_path->at(i_currentNode).run);
    init.Launch();
}

bool WaypointMovementGenerator::update(uint32 diff, uint32)
{
    // Waypoint movement can be switched on/off
    // This is quite handy for escort quests and other stuff
    if (owner_->hasUnitState(UNIT_STAT_NOT_MOVE))
    {
        owner_->clearUnitState(UNIT_STAT_ROAMING_MOVE);
        return false;
    }

    // prevent a crash at empty waypoint path.
    if (!i_path || i_path->empty())
    {
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
    }

    if (Stopped())
    {
        if (CanMove(diff))
            StartMove();
    }
    else
    {
        if (owner_->IsStopped())
            Stop(10 * IN_MILLISECONDS);
        else if (owner_->movespline->Finalized())
        {
            OnArrived();
            StartMove();
        }
    }
    return false;
}

std::string WaypointMovementGenerator::debug_str() const
{
    return "current point: " + std::to_string(i_currentNode);
}

uint32 FlightPathMovementGenerator::GetPathAtMapEnd() const
{
    if (i_currentNode >= i_path->size())
        return i_path->size();

    uint32 curMapId = (*i_path)[i_currentNode].mapid;

    for (uint32 i = i_currentNode; i < i_path->size(); ++i)
    {
        if ((*i_path)[i].mapid != curMapId)
            return i;
    }

    return i_path->size();
}

#define PLAYER_FLIGHT_SPEED 32.0f

void FlightPathMovementGenerator::start()
{
    assert(owner_->GetTypeId() == TYPEID_PLAYER);
    auto player = static_cast<Player*>(owner_);

    if (i_mountDisplayId)
        player->Mount(i_mountDisplayId);

    player->getHostileRefManager().setOnlineOfflineState(false);
    player->addUnitState(UNIT_STAT_TAXI_FLIGHT);
    player->SetFlag(
        UNIT_FIELD_FLAGS, UNIT_FLAG_DISABLE_MOVE | UNIT_FLAG_TAXI_FLIGHT);

    movement::MoveSplineInit init(*player);
    uint32 end = GetPathAtMapEnd();
    for (uint32 i = i_currentNode; i != end; ++i)
    {
        G3D::Vector3 vertice((*i_path)[i].x, (*i_path)[i].y, (*i_path)[i].z);
        init.Path().push_back(vertice);
    }
    init.SetFirstPointId(i_currentNode);
    init.SetFly();
    init.SetVelocity(PLAYER_FLIGHT_SPEED);
    init.Launch();
}

void FlightPathMovementGenerator::stop()
{
    assert(owner_->GetTypeId() == TYPEID_PLAYER);
    auto player = static_cast<Player*>(owner_);

    // remove flag to prevent send object build movement packets for flight
    // state and crash (movement generator already not at top of stack)
    player->clearUnitState(UNIT_STAT_TAXI_FLIGHT);

    player->Unmount();
    player->RemoveFlag(
        UNIT_FIELD_FLAGS, UNIT_FLAG_DISABLE_MOVE | UNIT_FLAG_TAXI_FLIGHT);

    if (player->m_taxi.empty())
    {
        player->getHostileRefManager().setOnlineOfflineState(true);
        if (player->pvpInfo.inHostileArea)
            player->CastSpell(player, 2479, true);

        // update z position to ground and orientation for landing point
        // this prevent cheating with landing  point at lags
        // when client side flight end early in comparison server side
        player->StopMoving();
    }
}

bool FlightPathMovementGenerator::update(uint32, uint32)
{
    uint32 pointId = (uint32)owner_->movespline->currentPathIdx();
    if (pointId > i_currentNode)
    {
        bool departureEvent = true;
        do
        {
            DoEventIfAny((*i_path)[i_currentNode], departureEvent);
            if (pointId == i_currentNode)
                break;
            i_currentNode += (uint32)departureEvent;
            departureEvent = !departureEvent;
        } while (true);
    }

    return !(i_currentNode < (i_path->size() - 1));
}

void FlightPathMovementGenerator::SetCurrentNodeAfterTeleport()
{
    if (i_path->empty())
        return;

    uint32 map0 = (*i_path)[0].mapid;

    for (size_t i = 1; i < i_path->size(); ++i)
    {
        if ((*i_path)[i].mapid != map0)
        {
            i_currentNode = i;
            return;
        }
    }
}

void FlightPathMovementGenerator::DoEventIfAny(
    TaxiPathNodeEntry const& node, bool departure)
{
    if (uint32 eventid =
            departure ? node.departureEventID : node.arrivalEventID)
    {
        LOG_DEBUG(logging,
            "Taxi %s event %u of node %u of path %u for player %s",
            departure ? "departure" : "arrival", eventid, node.index, node.path,
            owner_->GetName());

        if (!sScriptMgr::Instance()->OnProcessEvent(
                eventid, owner_, owner_, departure))
            owner_->GetMap()->ScriptsStart(
                sEventScripts, eventid, owner_, owner_);
    }
}

DynamicWaypointMovementGenerator::DynamicWaypointMovementGenerator(
    const std::vector<DynamicWaypoint>& wpList, bool repeatPath)
  : i_nextMoveTime(0), i_path(wpList), i_currentNode(0),
    m_repeatPath(repeatPath), m_waitingForMove(false), m_isArrivalDone(false),
    i_castedSpell(false)
{
}

void DynamicWaypointMovementGenerator::start()
{
    StartMoveNow();
    owner_->addUnitState(UNIT_STAT_ROAMING | UNIT_STAT_ROAMING_MOVE);
}

void DynamicWaypointMovementGenerator::stop()
{
    owner_->clearUnitState(UNIT_STAT_ROAMING | UNIT_STAT_ROAMING_MOVE);
}

void DynamicWaypointMovementGenerator::OnArrived()
{
    if (i_path.empty())
        return;

    if (m_isArrivalDone)
        return;

    owner_->clearUnitState(UNIT_STAT_ROAMING_MOVE);
    m_isArrivalDone = true;

    if (i_currentNode + 1 < i_path.size())
    {
        uint32 delay = i_path[i_currentNode].delay;
        if (delay)
        {
            // At this point we're already at next point
            Stop(delay);
            m_waitingForMove = true;
        }

        // Inform script
        owner_->movement_gens.queue_movement_inform(
            movement::gen::waypoint, i_currentNode);
        ++i_currentNode;
        StartMove();
        return;
    }
    else
    {
        if (m_repeatPath)
        {
            // Inform script
            owner_->movement_gens.queue_movement_inform(
                movement::gen::waypoint, i_currentNode);
            i_currentNode = 0;
            StartMove();
            return;
        }
    }

    LOG_DEBUG(logging,
        "DynamicWaypointMovementGenerator::OnArrived finished walking "
        "through " SIZEFMTD " waypoints for Creature GUID: %s",
        i_path.size(), owner_->GetGuidStr().c_str());

    // Inform script
    owner_->movement_gens.queue_movement_inform(
        movement::gen::waypoint, i_currentNode);

    // Wp movement finished in update()
    i_currentNode = (uint32)i_path.size();
}

void DynamicWaypointMovementGenerator::StartMove()
{
    if (i_path.empty())
        return;

    if (Stopped())
        return;

    m_isArrivalDone = false;

    owner_->addUnitState(UNIT_STAT_ROAMING_MOVE);

    const DynamicWaypoint& node = i_path.at(i_currentNode);
    movement::MoveSplineInit init(*owner_);
    init.MoveTo(node.X, node.Y, node.Z);

    if (node.O != 100)
        init.SetFacing(node.O);

    init.SetWalk(!i_path.at(i_currentNode).run);
    init.Launch();
}

bool DynamicWaypointMovementGenerator::update(uint32 diff, uint32)
{
    // Remove ourselves if we're done
    if (m_isArrivalDone && i_currentNode >= i_path.size())
        return true;

    // prevent a crash at empty waypoint path.
    if (i_path.empty())
    {
        owner_->clearUnitState(UNIT_STAT_ROAMING_MOVE);
        return false;
    }

    // Waypoint movement can be switched on/off
    // This is quite handy for escort quests and other stuff
    if (owner_->hasUnitState(UNIT_STAT_NOT_MOVE))
    {
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
            m_waitingForMove = true;
            return false;
        }
    }

    if (m_waitingForMove)
    {
        if (CanMove(diff))
        {
            StartMove();
            m_waitingForMove = false;
        }

        return false; // Do not call OnArrived at this stage
    }

    if (owner_->movespline->Finalized())
        OnArrived();

    return false;
}

std::string DynamicWaypointMovementGenerator::debug_str() const
{
    return "current point: " + std::to_string(i_currentNode);
}

void SplineMovementGenerator::start()
{
    spline_ = sWaypointMgr::Instance()->GetSpline(id_);
    if (spline_)
    {
        movement::MoveSplineInit init(*owner_);

        init.MovebyPath(*spline_);
        init.SetWalk(false);
        init.SetCatmullRom();

        // TODO: Do we need to use EnterCycle?
        if (cyclic_)
            init.SetCyclic();

        init.Launch();
    }
}

void SplineMovementGenerator::stop()
{
    if (!owner_->movespline->Finalized())
        owner_->StopMoving();
}

bool SplineMovementGenerator::update(uint32, uint32)
{
    if (owner_->movespline->Finalized())
    {
        // Inform script
        owner_->movement_gens.queue_movement_inform(movement::gen::spline, id_);
        return true;
    }

    return false;
}

std::string SplineMovementGenerator::debug_str() const
{
    return "path id: " + std::to_string(id_) + " cyclic: " +
           std::to_string(cyclic_);
}
}
