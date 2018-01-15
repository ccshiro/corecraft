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

#include "ArenaTeam.h"
#include "BattleGround.h"
#include "BattleGroundEY.h"
#include "BattleGroundMgr.h"
#include "BattleGroundWS.h"
#include "Chat.h"
#include "Common.h"
#include "Language.h"
#include "logging.h"
#include "Object.h"
#include "ObjectMgr.h"
#include "Opcodes.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SharedDefines.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "battlefield_queue.h"

void WorldSession::HandleBattlemasterHelloOpcode(WorldPacket& recv_data)
{
    ObjectGuid guid;
    recv_data >> guid;

    Creature* pCreature = GetPlayer()->GetMap()->GetCreature(guid);

    if (!pCreature)
        return;

    if (!pCreature->isBattleMaster()) // it's not battlemaster
        return;

    // Stop the npc if moving
    if (!pCreature->IsStopped())
        pCreature->StopMoving();

    BattleGroundTypeId bgTypeId =
        sBattleGroundMgr::Instance()->GetBattleMasterBG(pCreature->GetEntry());

    if (bgTypeId == BATTLEGROUND_TYPE_NONE)
        return;

    if (!_player->GetBGAccessByLevel(bgTypeId))
    {
        // temp, must be gossip message...
        SendNotification(LANG_YOUR_BG_LEVEL_REQ_ERROR);
        return;
    }

    SendBattlegGroundList(guid, bgTypeId);
}

void WorldSession::SendBattlegGroundList(
    ObjectGuid guid, BattleGroundTypeId bgTypeId)
{
    WorldPacket data;
    sBattleGroundMgr::Instance()->BuildBattleGroundListPacket(
        &data, guid, _player, bgTypeId);
    send_packet(std::move(data));
}

void WorldSession::HandleBattlemasterJoinOpcode(WorldPacket& recv_data)
{
    ObjectGuid guid; // battlemaster guid
    uint32 battleground_type;
    uint32 specific_id; // 0 for first available
    uint8 join_as_group;

    recv_data >> guid >> battleground_type >> specific_id >> join_as_group;

    switch (battleground_type)
    {
    case BATTLEGROUND_AV:
    case BATTLEGROUND_WS:
    case BATTLEGROUND_AB:
    case BATTLEGROUND_EY:
        // Valid, do nothing
        break;
    default:
        logging.error(
            "Player (%s) tried to join an invalid battleground type (%i)",
            _player->GetGuidStr().c_str(), battleground_type);
        return;
    }

    Creature* battlemaster = GetPlayer()->GetMap()->GetCreature(guid);
    if (!battlemaster || !battlemaster->isBattleMaster() ||
        !_player->IsWithinDistInMap(battlemaster, INTERACTION_DISTANCE) ||
        sBattleGroundMgr::Instance()->GetBattleMasterBG(
            battlemaster->GetEntry()) != battleground_type)
        return;

    Group* group = nullptr;
    if (join_as_group)
    {
        group = _player->GetGroup();
        if (!group || !group->IsLeader(_player->GetObjectGuid()))
            return;
    }

    std::set<const Player*> players;
    if (join_as_group)
    {
        for (auto member : group->members(false))
            players.insert(member);
    }

    const battlefield::specification spec = battlefield::specification(
        static_cast<BattleGroundTypeId>(battleground_type));
    battlefield::queue_entry entry(
        spec.get_type(), _player, players, join_as_group, specific_id);
    battlefield::queue::result res = sBattlefieldQueue::Instance()->push(entry);
    for (auto& re : res)
    {
        if (Player* player = ObjectAccessor::FindPlayer(re.first, false))
        {
            WorldPacket data(SMSG_GROUP_JOINED_BATTLEGROUND, 4);
            data << re.second;
            player->GetSession()->send_packet(std::move(data));

            if (re.second > 0)
            {
                // Push finished for this person (result > 0)
                sBattleGroundMgr::Instance()->send_status_packets(player);
                player->SetBattleGroundEntryPoint(_player); // _player is leader
            }
        }
    }

    // res is empty if the join worked out for a solo queuer
    if (!join_as_group && res.empty())
    {
        sBattleGroundMgr::Instance()->send_status_packets(_player);
        _player->SetBattleGroundEntryPoint();
    }
}

void WorldSession::HandleBattleGroundPlayerPositionsOpcode(
    WorldPacket& /*recv_data*/)
{
    BattleGround* bg = _player->GetBattleGround();
    if (!bg) // can't be received if player not in battleground
        return;

    switch (bg->GetTypeID())
    {
    case BATTLEGROUND_WS:
    {
        // This is not TBC-like. We went for the way it works in vanilla; you
        // can see your carrier
        // but not the enemy carrying your flag. It results in an over-all more
        // fun WSG.
        WorldPacket data(MSG_BATTLEGROUND_PLAYER_POSITIONS, 4 + 4 + 16);
        data << (uint32)0;

        Player* carrier = _player->GetTeam() == ALLIANCE ?
                              sObjectMgr::Instance()->GetPlayer(
                                  static_cast<BattleGroundWS*>(bg)
                                      ->GetHordeFlagPickerGuid()) :
                              sObjectMgr::Instance()->GetPlayer(
                                  static_cast<BattleGroundWS*>(bg)
                                      ->GetAllianceFlagPickerGuid());

        if (carrier)
            data << (uint32)1;
        else
            data << (uint32)0;

        if (carrier)
        {
            data << carrier->GetObjectGuid();
            data << carrier->GetX();
            data << carrier->GetY();
        }

        send_packet(std::move(data));
        break;
    }
    case BATTLEGROUND_EY:
    {
        uint32 flagCarrierCount = 0;
        Player* flagCarrier = sObjectMgr::Instance()->GetPlayer(
            static_cast<BattleGroundEY*>(bg)->GetFlagPickerGuid());
        if (flagCarrier)
            flagCarrierCount = 1;

        WorldPacket data(
            MSG_BATTLEGROUND_PLAYER_POSITIONS, 4 + 4 + 16 * flagCarrierCount);
        data << uint32(0);
        data << uint32(flagCarrierCount);

        if (flagCarrier)
        {
            data << flagCarrier->GetObjectGuid();
            data << flagCarrier->GetX();
            data << flagCarrier->GetY();
        }

        send_packet(std::move(data));
        break;
    }
    case BATTLEGROUND_AB:
    case BATTLEGROUND_AV:
    {
        // for other BG types - send default
        WorldPacket data(MSG_BATTLEGROUND_PLAYER_POSITIONS, (4 + 4));
        data << uint32(0);
        data << uint32(0);
        send_packet(std::move(data));
    }
    break;
    default:
        // maybe it is sent also in arena - do nothing
        break;
    }
}

void WorldSession::HandlePVPLogDataOpcode(WorldPacket& /*recv_data*/)
{
    BattleGround* bg = _player->GetBattleGround();
    if (!bg)
        return;

    // arena finish version will send in BattleGround::EndBattleGround directly
    if (bg->isArena())
        return;

    WorldPacket data;
    sBattleGroundMgr::Instance()->BuildPvpLogDataPacket(&data, bg);
    send_packet(std::move(data));
}

void WorldSession::HandleBattlefieldListOpcode(WorldPacket& recv_data)
{
    uint32 bgTypeId;
    recv_data >> bgTypeId; // id from DBC

    BattlemasterListEntry const* bl =
        sBattlemasterListStore.LookupEntry(bgTypeId);
    if (!bl)
    {
        logging.error("Battleground: invalid bgtype received.");
        return;
    }

    WorldPacket data;
    sBattleGroundMgr::Instance()->BuildBattleGroundListPacket(
        &data, _player->GetObjectGuid(), _player, BattleGroundTypeId(bgTypeId));
    send_packet(std::move(data));
}

void WorldSession::HandleBattleFieldPortOpcode(WorldPacket& recv_data)
{
    uint8 arena_type;
    uint8 unk;
    uint32 bg_type_id;
    uint16 unk2;
    uint8 action;

    recv_data >> arena_type >> unk >> bg_type_id >> unk2 >> action;

    // Verify arena type
    switch (arena_type)
    {
    case ARENA_TYPE_NONE:
    case ARENA_TYPE_2v2:
    case ARENA_TYPE_3v3:
    case ARENA_TYPE_5v5:
        // Valid, do nothing
        break;
    default:
        // Invalid arena type
        logging.error(
            "WorldSession::HandleBattleFieldPortOpcode: Player (%s) supplied "
            "invalid arena type (%i)",
            _player->GetGuidStr().c_str(), arena_type);
        return;
    }

    // Verify BG type
    switch (bg_type_id)
    {
    case BATTLEGROUND_AV:
    case BATTLEGROUND_WS:
    case BATTLEGROUND_AB:
    case BATTLEGROUND_EY:
        // Valid battleground, do nothing
        break;
    case BATTLEGROUND_AA:
    case BATTLEGROUND_NA: // Specific arenas are used for joining purposes
    case BATTLEGROUND_BE:
    case BATTLEGROUND_RL:
        // Verify that arena type also had a supplied arena size
        if (arena_type == ARENA_TYPE_NONE)
            return;
        break;
    default:
        // Invalid bg type id
        logging.error(
            "WorldSession::HandleBattleFieldPortOpcode: Player (%s) supplied "
            "invalid battleground type (%i)",
            _player->GetGuidStr().c_str(), bg_type_id);
        return;
    }

    const battlefield::specification spec = battlefield::specification(
        static_cast<BattleGroundTypeId>(bg_type_id), _player->getLevel(),
        static_cast<ArenaType>(arena_type),
        false // Note: Blizzard does not supply this information. To not add too
              // much code bloat the manager will adjust this accordingly
        );

    if (action == 0)
    {
        sBattleGroundMgr::Instance()->attempt_leave_queue(_player, spec);
    }
    else if (action == 1)
    {
        // Note: Our way of implementation has only one possible invite pending
        // at a time
        // which means we do not need to pass along which invite the player
        // tried to accept
        sBattleGroundMgr::Instance()->attempt_accept_invite(_player);
    }
}

void WorldSession::HandleLeaveBattlefieldOpcode(WorldPacket& recv_data)
{
    recv_data.read_skip<uint8>();  // unk1
    recv_data.read_skip<uint8>();  // unk2
    recv_data.read_skip<uint32>(); // BattleGroundTypeId
    recv_data.read_skip<uint16>(); // unk3

    _player->LeaveBattleground();
}

void WorldSession::HandleBattlefieldStatusOpcode(WorldPacket& /*recv_data*/)
{
    sBattleGroundMgr::Instance()->send_status_packets(
        _player, true); // Include pending invites
}

void WorldSession::HandleAreaSpiritHealerQueryOpcode(WorldPacket& recv_data)
{
    BattleGround* bg = _player->GetBattleGround();
    if (!bg)
        return;

    ObjectGuid guid;
    recv_data >> guid;

    Creature* unit = GetPlayer()->GetMap()->GetCreature(guid);
    if (!unit)
        return;

    if (!unit->isSpiritService()) // it's not spirit service
        return;

    unit->SendAreaSpiritHealerQueryOpcode(GetPlayer());
}

void WorldSession::HandleAreaSpiritHealerQueueOpcode(WorldPacket& recv_data)
{
    BattleGround* bg = _player->GetBattleGround();
    if (!bg)
        return;

    ObjectGuid guid;
    recv_data >> guid;

    Creature* unit = GetPlayer()->GetMap()->GetCreature(guid);
    if (!unit)
        return;

    if (!unit->isSpiritService()) // it's not spirit service
        return;

    sScriptMgr::Instance()->OnGossipHello(GetPlayer(), unit);
}

void WorldSession::HandleBattlemasterJoinArena(WorldPacket& recv_data)
{
    ObjectGuid guid; // arenamaster guid
    uint8 team_size;
    uint8 join_as_group;
    uint8 rated;

    recv_data >> guid >> team_size >> join_as_group >> rated;

    ArenaType arena_type;
    switch (team_size)
    {
    case 0:
        arena_type = ARENA_TYPE_2v2;
        break;
    case 1:
        arena_type = ARENA_TYPE_3v3;
        break;
    case 2:
        arena_type = ARENA_TYPE_5v5;
        break;
    default:
        // invalid user supplied value
        logging.error(
            "Player (%s) tried to join invalid arena team size index (%i)",
            _player->GetGuidStr().c_str(), team_size);
        return;
    }

    const battlefield::specification spec(
        BATTLEGROUND_AA, _player->getLevel(), arena_type, rated);

    Creature* battlemaster = GetPlayer()->GetMap()->GetCreature(guid);
    if (!battlemaster || !battlemaster->isBattleMaster() ||
        !_player->IsWithinDistInMap(battlemaster, INTERACTION_DISTANCE) ||
        sBattleGroundMgr::Instance()->GetBattleMasterBG(
            battlemaster->GetEntry()) != BATTLEGROUND_AA)
        return;

    if (rated && !join_as_group)
        return;

    Group* group = nullptr;
    if (join_as_group)
    {
        group = _player->GetGroup();
        if (!group || !group->IsLeader(_player->GetObjectGuid()))
            return;
    }

    std::set<const Player*> players;
    if (join_as_group)
    {
        for (auto member : group->members(false))
            players.insert(member);
    }

    battlefield::queue_entry entry(
        spec.get_type(), _player, players, join_as_group);
    battlefield::queue::result res = sBattlefieldQueue::Instance()->push(entry);
    for (auto& re : res)
    {
        if (Player* player = ObjectAccessor::FindPlayer(re.first, false))
        {
            WorldPacket data(SMSG_GROUP_JOINED_BATTLEGROUND, 4);
            data << re.second;
            player->GetSession()->send_packet(std::move(data));

            if (re.second > 0)
            {
                // Push finished for this person (result > 0)
                sBattleGroundMgr::Instance()->send_status_packets(player);
                player->SetBattleGroundEntryPoint(_player); // _player is leader
            }
        }
    }

    // res is empty if the join worked out for a solo queuer
    if (!join_as_group && res.empty())
    {
        sBattleGroundMgr::Instance()->send_status_packets(_player);
        _player->SetBattleGroundEntryPoint();
    }
}

void WorldSession::HandleReportPvPAFK(WorldPacket& recv_data)
{
    ObjectGuid playerGuid;
    recv_data >> playerGuid;
    Player* reportedPlayer = sObjectMgr::Instance()->GetPlayer(playerGuid);

    if (!reportedPlayer)
    {
        LOG_DEBUG(
            logging, "WorldSession::HandleReportPvPAFK: player not found");
        return;
    }

    LOG_DEBUG(logging, "WorldSession::HandleReportPvPAFK: %s reported %s",
        _player->GetName(), reportedPlayer->GetName());

    reportedPlayer->ReportedAfkBy(_player);
}

void WorldSession::SendBattleGroundOrArenaJoinError(uint8 err)
{
    WorldPacket data;
    int32 msg;
    switch (err)
    {
    case BG_JOIN_ERR_OFFLINE_MEMBER:
        msg = LANG_BG_GROUP_OFFLINE_MEMBER;
        break;
    case BG_JOIN_ERR_GROUP_TOO_MANY:
        msg = LANG_BG_GROUP_TOO_LARGE;
        break;
    case BG_JOIN_ERR_MIXED_FACTION:
        msg = LANG_BG_GROUP_MIXED_FACTION;
        break;
    case BG_JOIN_ERR_MIXED_LEVELS:
        msg = LANG_BG_GROUP_MIXED_LEVELS;
        break;
    case BG_JOIN_ERR_GROUP_MEMBER_ALREADY_IN_QUEUE:
        msg = LANG_BG_GROUP_MEMBER_ALREADY_IN_QUEUE;
        break;
    case BG_JOIN_ERR_GROUP_DESERTER:
        msg = LANG_BG_GROUP_MEMBER_DESERTER;
        break;
    case BG_JOIN_ERR_ALL_QUEUES_USED:
        msg = LANG_BG_GROUP_MEMBER_NO_FREE_QUEUE_SLOTS;
        break;
    case BG_JOIN_ERR_GROUP_NOT_ENOUGH:
    case BG_JOIN_ERR_MIXED_ARENATEAM:
    default:
        return;
        break;
    }
    ChatHandler::FillMessageData(&data, nullptr, CHAT_MSG_BG_SYSTEM_NEUTRAL,
        LANG_UNIVERSAL, GetMangosString(msg));
    send_packet(std::move(data));
    return;
}
