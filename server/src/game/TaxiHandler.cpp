/*
 * Copyright (C) 2005-2012 MaNGOS <http://getmangos.com/>
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

#include "Common.h"
#include "logging.h"
#include "movement/WaypointMovementGenerator.h"
#include "ObjectMgr.h"
#include "Opcodes.h"
#include "Path.h"
#include "Player.h"
#include "UpdateMask.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "Database/DatabaseEnv.h"

void WorldSession::HandleTaxiNodeStatusQueryOpcode(WorldPacket& recv_data)
{
    ObjectGuid guid;

    recv_data >> guid;
    SendTaxiStatus(guid);
}

void WorldSession::SendTaxiStatus(ObjectGuid guid)
{
    // cheating checks
    Creature* unit = GetPlayer()->GetMap()->GetCreature(guid);
    if (!unit)
    {
        LOG_DEBUG(logging,
            "WorldSession::SendTaxiStatus - %s not found or you can't interact "
            "with it.",
            guid.GetString().c_str());
        return;
    }

    uint32 curloc = sObjectMgr::Instance()->GetNearestTaxiNode(unit->GetX(),
        unit->GetY(), unit->GetZ(), unit->GetMapId(), GetPlayer()->GetTeam());

    // not found nearest
    if (curloc == 0)
        return;

    LOG_DEBUG(logging, "WORLD: current location %u ", curloc);

    WorldPacket data(SMSG_TAXINODE_STATUS, 9);
    data << ObjectGuid(guid);
    data << uint8(GetPlayer()->m_taxi.IsTaximaskNodeKnown(curloc) ? 1 : 0);
    send_packet(std::move(data));

    // Taxi must be express (set in Player::ActivateTaxiPathTo)
    if (GetPlayer()->m_taxi.IsExpress())
    {
        uint32 currSource = GetPlayer()->m_taxi.GetTaxiSource();
        uint32 currDest = GetPlayer()->m_taxi.GetTaxiDestination();
        // Sanity null-check, If we do not have a source or destination then
        // return, as NextTaxiDestination will cause crash
        if (!currSource || !currDest)
            return;
        // First, are we at the destination?
        if (curloc == currDest)
        {
            // Secondly, check if we have another destination
            uint32 destinationnode = GetPlayer()->m_taxi.NextTaxiDestination();
            if (destinationnode > 0)
            {
                // current source node for next destination
                uint32 sourcenode = GetPlayer()->m_taxi.GetTaxiSource();

                // Add to taximask middle hubs in taxicheat mode (to prevent
                // having player with disabled taxicheat and not having back
                // flight path)
                if (GetPlayer()->isTaxiCheater())
                {
                    if (GetPlayer()->m_taxi.SetTaximaskNode(sourcenode))
                    {
                        WorldPacket data(SMSG_NEW_TAXI_PATH, 0);
                        _player->GetSession()->send_packet(std::move(data));
                    }
                }

                LOG_DEBUG(logging, "WORLD: Taxi has to go from %u to %u",
                    sourcenode, destinationnode);

                // We should not change mounts mid-flight (this is retail-like
                // in TBC), use mount from the 1st path
                uint32 mountDisplayId =
                    GetPlayer()->m_taxi.GetOriginalMountDisplayId();
                // Sanity check, if original is unavailable then use the current
                // path's mount (Shouldn't happen)
                if (!mountDisplayId)
                    mountDisplayId =
                        sObjectMgr::Instance()->GetTaxiMountDisplayId(
                            sourcenode, GetPlayer()->GetTeam());

                uint32 path, cost;
                sObjectMgr::Instance()->GetTaxiPath(
                    sourcenode, destinationnode, path, cost);
                if (path && mountDisplayId)
                {
                    // XXX (Mangos never checked availability of gold, do we
                    // need to?)
                    inventory::transaction trans;
                    trans.remove(cost);
                    GetPlayer()->storage().finalize(trans);
                    SendDoFlight(
                        mountDisplayId, path, 1); // skip start fly node
                }
                else
                    GetPlayer()->m_taxi.ClearTaxiDestinations(); // clear
                                                                 // problematic
                                                                 // path and
                                                                 // next
            }
            // If not....
            else
            {
                // we have no other destination so we need to reset our
                // destinations to reflect our current path (sanity check - to
                // prevent crashes)
                GetPlayer()->m_taxi.ClearTaxiDestinations();
                GetPlayer()->m_taxi.AddTaxiDestination(currSource);
                GetPlayer()->m_taxi.AddTaxiDestination(currDest);
            }
        }
    }
}

void WorldSession::HandleTaxiQueryAvailableNodes(WorldPacket& recv_data)
{
    ObjectGuid guid;
    recv_data >> guid;

    // cheating checks
    Creature* unit =
        GetPlayer()->GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_FLIGHTMASTER);
    if (!unit)
    {
        LOG_DEBUG(logging,
            "WORLD: HandleTaxiQueryAvailableNodes - %s not found or you can't "
            "interact with him.",
            guid.GetString().c_str());
        return;
    }

    // remove fake death
    if (GetPlayer()->hasUnitState(UNIT_STAT_DIED))
        GetPlayer()->remove_auras(SPELL_AURA_FEIGN_DEATH);

    // unknown taxi node case
    if (SendLearnNewTaxiNode(unit))
        return;

    // known taxi node case
    SendTaxiMenu(unit);
}

void WorldSession::SendTaxiMenu(Creature* unit)
{
    // find current node
    uint32 curloc = sObjectMgr::Instance()->GetNearestTaxiNode(unit->GetX(),
        unit->GetY(), unit->GetZ(), unit->GetMapId(), GetPlayer()->GetTeam());

    if (curloc == 0)
        return;

    WorldPacket data(SMSG_SHOWTAXINODES, (4 + 8 + 4 + 8 * 4));
    data << uint32(1);
    data << unit->GetObjectGuid();
    data << uint32(curloc);
    GetPlayer()->m_taxi.AppendTaximaskTo(data, GetPlayer()->isTaxiCheater());
    send_packet(std::move(data));
}

void WorldSession::SendDoFlight(
    uint32 mountDisplayId, uint32 path, uint32 pathNode)
{
    if (path >= sTaxiPathNodesByPath.size())
        return;

    // remove fake death
    if (GetPlayer()->hasUnitState(UNIT_STAT_DIED))
        GetPlayer()->remove_auras(SPELL_AURA_FEIGN_DEATH);

    _player->movement_gens.remove_all(movement::gen::flight);

    _player->movement_gens.push(new movement::FlightPathMovementGenerator(
        sTaxiPathNodesByPath[path], pathNode, mountDisplayId));
}

bool WorldSession::SendLearnNewTaxiNode(Creature* unit)
{
    // find current node
    uint32 curloc = sObjectMgr::Instance()->GetNearestTaxiNode(unit->GetX(),
        unit->GetY(), unit->GetZ(), unit->GetMapId(), GetPlayer()->GetTeam());

    if (curloc == 0)
        return true; // `true` send to avoid WorldSession::SendTaxiMenu call
                     // with one more curlock seartch with same false result.

    if (GetPlayer()->m_taxi.SetTaximaskNode(curloc))
    {
        WorldPacket msg(SMSG_NEW_TAXI_PATH, 0);
        send_packet(&msg);

        WorldPacket update(SMSG_TAXINODE_STATUS, 9);
        update << ObjectGuid(unit->GetObjectGuid());
        update << uint8(1);
        send_packet(&update);

        return true;
    }
    else
        return false;
}

void WorldSession::HandleActivateTaxiExpressOpcode(WorldPacket& recv_data)
{
    ObjectGuid guid;
    uint32 node_count, _totalcost;

    recv_data >> guid >> _totalcost >> node_count;

    Creature* npc =
        GetPlayer()->GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_FLIGHTMASTER);
    if (!npc)
    {
        LOG_DEBUG(logging,
            "WORLD: HandleActivateTaxiExpressOpcode - %s not found or you "
            "can't interact with it.",
            guid.GetString().c_str());
        return;
    }
    std::vector<uint32> nodes;

    for (uint32 i = 0; i < node_count; ++i)
    {
        uint32 node;
        recv_data >> node;
        nodes.push_back(node);
    }

    if (nodes.empty())
        return;

    GetPlayer()->ActivateTaxiPathTo(nodes, npc, 0, true);
}

void WorldSession::HandleMoveSplineDoneOpcode(WorldPacket& recv_data)
{
    MovementInfo movementInfo; // used only for proper packet read

    recv_data >> movementInfo;
    recv_data >> Unused<uint32>(); // unk

    // in taxi flight packet received in 2 case:
    // 1) end taxi path in far (multi-node) flight
    // 2) switch from one map to other in case multi-map taxi path
    // we need process only (1)
    uint32 curDest = GetPlayer()->m_taxi.GetTaxiDestination();
    if (!curDest)
        return;

    TaxiNodesEntry const* curDestNode = sTaxiNodesStore.LookupEntry(curDest);

    // far teleport case
    if (curDestNode && curDestNode->map_id != GetPlayer()->GetMapId())
    {
        if (_player->movement_gens.top_id() == movement::gen::flight)
        {
            movement::FlightPathMovementGenerator* flight =
                (movement::FlightPathMovementGenerator*)(_player->movement_gens
                                                             .top());

            flight->stop(); // will reset at map landing

            flight->SetCurrentNodeAfterTeleport();
            TaxiPathNodeEntry const& node =
                flight->GetPath()[flight->GetCurrentNode()];
            flight->SkipCurrentNode();

            GetPlayer()->TeleportTo(curDestNode->map_id, node.x, node.y, node.z,
                GetPlayer()->GetO());
        }
        return;
    }

    uint32 destinationnode = GetPlayer()->m_taxi.NextTaxiDestination();
    if (destinationnode > 0) // if more destinations to go
    {
        /*
        // current source node for next destination
        uint32 sourcenode = GetPlayer()->m_taxi.GetTaxiSource();

        // Add to taximask middle hubs in taxicheat mode (to prevent having
        player with disabled taxicheat and not having back flight path)
        if (GetPlayer()->isTaxiCheater())
        {
            if(GetPlayer()->m_taxi.SetTaximaskNode(sourcenode))
            {
                WorldPacket data(SMSG_NEW_TAXI_PATH, 0);
                _player->GetSession()->SendPacket(std::move(data));
            }
        }

        LOG_DEBUG(logging, "WORLD: Taxi has to go from %u to %u", sourcenode,
        destinationnode );

        uint32 mountDisplayId =
        sObjectMgr::Instance()->GetTaxiMountDisplayId(sourcenode,
        GetPlayer()->GetTeam());

        uint32 path, cost;
        sObjectMgr::Instance()->GetTaxiPath( sourcenode, destinationnode, path,
        cost);

        if(path && mountDisplayId)
        {
            GetPlayer()->ModifyMoney(-(int32)cost);
            SendDoFlight( mountDisplayId, path, 1 );        // skip start fly
        node
        }
        else
            GetPlayer()->m_taxi.ClearTaxiDestinations();    // clear problematic
        path and next
            */
    }
    else
    {
        // we are not at the location we should be...
        if (curDestNode && (GetPlayer()->GetX() != curDestNode->x ||
                               GetPlayer()->GetY() != curDestNode->y ||
                               GetPlayer()->GetZ() != curDestNode->z))
        {
            GetPlayer()->NearTeleportTo(curDestNode->x, curDestNode->y,
                curDestNode->z, GetPlayer()->GetO());
        }

        GetPlayer()->m_taxi.ClearTaxiDestinations(); // not destinations, clear
                                                     // source node
    }
}

void WorldSession::HandleActivateTaxiOpcode(WorldPacket& recv_data)
{
    ObjectGuid guid;
    std::vector<uint32> nodes;
    nodes.resize(2);

    recv_data >> guid >> nodes[0] >> nodes[1];

    Creature* npc =
        GetPlayer()->GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_FLIGHTMASTER);
    if (!npc)
    {
        LOG_DEBUG(logging,
            "WORLD: HandleActivateTaxiOpcode - %s not found or you can't "
            "interact with it.",
            guid.GetString().c_str());
        return;
    }

    GetPlayer()->ActivateTaxiPathTo(nodes, npc);
}
