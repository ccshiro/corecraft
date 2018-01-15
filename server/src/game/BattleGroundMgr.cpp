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

#include "BattleGroundMgr.h"
#include "ArenaTeam.h"
#include "BattleGroundAA.h"
#include "BattleGroundAB.h"
#include "BattleGroundAV.h"
#include "BattleGroundBE.h"
#include "BattleGroundEY.h"
#include "BattleGroundNA.h"
#include "BattleGroundRL.h"
#include "BattleGroundWS.h"
#include "Chat.h"
#include "Common.h"
#include "GameEventMgr.h"
#include "Map.h"
#include "MapManager.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "ProgressBar.h"
#include "SharedDefines.h"
#include "World.h"
#include "WorldPacket.h"
#include "battlefield_queue.h"
#include "Policies/Singleton.h"

/*********************************************************/
/***            BATTLEGROUND MANAGER                   ***/
/*********************************************************/

BattleGroundMgr::BattleGroundMgr() : update_timer_(0), debugging_pvp_(false)
{
    m_NextRatingDiscardUpdate =
        sWorld::Instance()->getConfig(CONFIG_UINT32_ARENA_RATING_DISCARD_TIMER);
}

BattleGroundMgr::~BattleGroundMgr()
{
    DeleteAllBattleGrounds();
}

void BattleGroundMgr::DeleteAllBattleGrounds()
{
    for (auto& elem : battlefield_container_)
    {
        delete elem;
    }
    battlefield_container_.clear();
}

void BattleGroundMgr::send_status_packets(Player* player, bool send_pending)
{
    /*
    SMSG_BATTLEFIELD_STATUS documentation:
    uint32: slot (0, 1, 2)
    uint64: Arena type and bg type packed in using some magic values. (NOTE: IF
    0 rest of the packet should not be sent)
    uint32: client instance id (Warsong Gulch >2<, Alterac Valley >4<, etc)
    uint8:  rated
    uint32: BattleGroundStatus.
        STATUS_WAIT_QUEUE:  uint32 average wait time + uint32 time in queue
    (both in milliseconds)
        STATUS_WAIT_JOIN:   uint32 map id + uint32 time left (milliseconds)
        STATUS_IN_PROGRESS: uint32 map id + uint32 end time + uint32 start time
    + uint8 0x1(?)
    */

    uint32 current_slot = 0;

    // Pending invites or current battlegrounds are always first
    auto found = invites_.find(player->GetObjectGuid());
    if (found != invites_.end())
    {
        if (send_pending)
        {
            player_invite invite = found->second;
            BattleGround* bg = invite.bg;

            const uint32 now = WorldTimer::getMSTime();
            const uint32 expire_time =
                invite.timestamp_ms +
                (bg->isArena() ? invite_arena_timeout : invite_timeout);
            const uint32 time_left = now < expire_time ? expire_time - now : 0;

            WorldPacket data(SMSG_BATTLEFIELD_STATUS);
            data << uint32(current_slot)
                 << uint64(uint64(bg->GetArenaType()) | (uint64(0x0D) << 8) |
                           (uint64(bg->GetTypeID()) << 16) |
                           (uint64(0x1F90) << 48))
                 << uint32(bg->GetClientInstanceID()) << uint8(bg->isRated())
                 << uint32(STATUS_WAIT_JOIN) << uint32(bg->GetMapId())
                 << uint32(time_left);
            player->GetSession()->send_packet(std::move(data));
        }
        ++current_slot; // Increase even if we don't send pending (to not
                        // overwrite this slot)
    }
    else if (BattleGround* bg = player->GetBattleGround())
    {
        WorldPacket data(SMSG_BATTLEFIELD_STATUS);
        data << uint32(current_slot++)
             << uint64(uint64(bg->GetArenaType()) | (uint64(0x0D) << 8) |
                       (uint64(bg->GetTypeID()) << 16) | (uint64(0x1F90) << 48))
             << uint32(bg->GetClientInstanceID()) << uint8(bg->isRated())
             << uint32(STATUS_IN_PROGRESS) << uint32(bg->GetMapId())
             << uint32(bg->GetEndTime()) << uint32(bg->GetTimeElapsed())
             << uint8(0x1);
        player->GetSession()->send_packet(std::move(data));
    }

    // Send all our queues
    battlefield::queue::entry_list queues =
        sBattlefieldQueue::Instance()->current_queues(player->GetObjectGuid());
    for (auto entry : queues)
    {
        const battlefield::specification& spec = entry->get_specification();
        uint32 delta = WorldTimer::time_no_syscall() - entry->timestamp();

        time_t average_wait_time =
            sBattlefieldQueue::Instance()->average_wait_time(spec);

        WorldPacket data(SMSG_BATTLEFIELD_STATUS, 4 + 8 + 4 + 1 + 4 + 4 + 4);
        data << uint32(current_slot++)
             << uint64(uint64(spec.arena_size()) | (uint64(0x0D) << 8) |
                       (uint64(spec.blizzard_id()) << 16) |
                       (uint64(0x1F90) << 48)) << uint32(0)
             << uint8(spec.rated()) << uint32(STATUS_WAIT_QUEUE)
             << uint32(average_wait_time * 1000) << uint32(delta * 1000);
        player->GetSession()->send_packet(std::move(data));
    }

    while (current_slot < 3)
    {
        // Send status none packets
        WorldPacket data(SMSG_BATTLEFIELD_STATUS, 4 + 8);
        data << uint32(current_slot++) << uint64(0);
        player->GetSession()->send_packet(std::move(data));
    }
}

void BattleGroundMgr::BuildPvpLogDataPacket(WorldPacket* data, BattleGround* bg)
{
    uint8 type = (bg->isArena() ? 1 : 0);
    // last check on 2.4.1
    data->initialize(
        MSG_PVP_LOG_DATA, (1 + 1 + 4 + 40 * bg->GetPlayerScoresSize()));
    *data << uint8(type); // type (battleground=0/arena=1)

    if (type) // arena
    {
        // it seems this must be according to BG_WINNER_A/H and _NOT_
        // BG_TEAM_A/H
        for (int i = 1; i >= 0; --i)
        {
            *data << uint32(
                3000 - bg->m_ArenaTeamRatingChanges[i]); // rating change:
                                                         // showed value - 3000
            *data << uint32(3999); // huge thanks for TOM_RUS for this!
            LOG_DEBUG(
                logging, "rating change: %d", bg->m_ArenaTeamRatingChanges[i]);
        }
        for (int i = 1; i >= 0; --i)
        {
            uint32 at_id = bg->m_ArenaTeamIds[i];
            ArenaTeam* at = sObjectMgr::Instance()->GetArenaTeamById(at_id);
            if (at)
                *data << at->GetName();
            else
                *data << (uint8)0;
        }
    }

    if (bg->GetStatus() != STATUS_WAIT_LEAVE)
    {
        *data << uint8(0); // bg not ended
    }
    else
    {
        *data << uint8(1);               // bg ended
        *data << uint8(bg->GetWinner()); // who win
    }

    *data << (int32)(bg->GetPlayerScoresSize());

    for (auto itr = bg->GetPlayerScoresBegin(); itr != bg->GetPlayerScoresEnd();
         ++itr)
    {
        *data << ObjectGuid(itr->first);
        *data << (int32)itr->second->KillingBlows;
        if (type == 0)
        {
            *data << (int32)itr->second->HonorableKills;
            *data << (int32)itr->second->Deaths;
            *data << (int32)(itr->second->BonusHonor);
        }
        else
        {
            Team team = bg->GetPlayerTeam(itr->first);
            *data << uint8(team == ALLIANCE ? 1 : 0);
        }
        *data << (int32)itr->second->DamageDone;  // damage done
        *data << (int32)itr->second->HealingDone; // healing done
        switch (bg->GetTypeID()) // battleground specific things
        {
        case BATTLEGROUND_AV:
            *data << (uint32)0x00000005; // count of next fields
            *data << (uint32)((BattleGroundAVScore*)itr->second)
                         ->GraveyardsAssaulted; // GraveyardsAssaulted
            *data << (uint32)((BattleGroundAVScore*)itr->second)
                         ->GraveyardsDefended; // GraveyardsDefended
            *data << (uint32)((BattleGroundAVScore*)itr->second)
                         ->TowersAssaulted; // TowersAssaulted
            *data << (uint32)((BattleGroundAVScore*)itr->second)
                         ->TowersDefended; // TowersDefended
            *data << (uint32)((BattleGroundAVScore*)itr->second)
                         ->SecondaryObjectives; // SecondaryObjectives - free
                                                // some of the Lieutnants
            break;
        case BATTLEGROUND_WS:
            *data << (uint32)0x00000002; // count of next fields
            *data << (uint32)((BattleGroundWGScore*)itr->second)
                         ->FlagCaptures; // flag captures
            *data << (uint32)((BattleGroundWGScore*)itr->second)
                         ->FlagReturns; // flag returns
            break;
        case BATTLEGROUND_AB:
            *data << (uint32)0x00000002; // count of next fields
            *data << (uint32)((BattleGroundABScore*)itr->second)
                         ->BasesAssaulted; // bases asssulted
            *data << (uint32)((BattleGroundABScore*)itr->second)
                         ->BasesDefended; // bases defended
            break;
        case BATTLEGROUND_EY:
            *data << (uint32)0x00000001; // count of next fields
            *data << (uint32)((BattleGroundEYScore*)itr->second)
                         ->FlagCaptures; // flag captures
            break;
        case BATTLEGROUND_NA:
        case BATTLEGROUND_BE:
        case BATTLEGROUND_AA:
        case BATTLEGROUND_RL:
            *data << (int32)0;
            break;
        default:
            *data << (int32)0;
            break;
        }
    }
}

void BattleGroundMgr::BuildGroupJoinedBattlegroundPacket(
    WorldPacket* data, BattleGroundTypeId bgTypeId)
{
    /*bgTypeId is:
    0 - Your group has joined a battleground queue, but you are not eligible
    1 - Your group has joined the queue for AV
    2 - Your group has joined the queue for WS
    3 - Your group has joined the queue for AB
    4 - Your group has joined the queue for NA
    5 - Your group has joined the queue for BE Arena
    6 - Your group has joined the queue for All Arenas
    7 - Your group has joined the queue for EotS*/
    data->initialize(SMSG_GROUP_JOINED_BATTLEGROUND, 4);
    *data << uint32(bgTypeId);
}

void BattleGroundMgr::BuildUpdateWorldStatePacket(
    WorldPacket* data, uint32 field, uint32 value)
{
    data->initialize(SMSG_UPDATE_WORLD_STATE, 4 + 4);
    *data << uint32(field);
    *data << uint32(value);
}

void BattleGroundMgr::BuildPlaySoundPacket(WorldPacket* data, uint32 soundid)
{
    data->initialize(SMSG_PLAY_SOUND, 4);
    *data << uint32(soundid);
}

void BattleGroundMgr::BuildPlayerLeftBattleGroundPacket(
    WorldPacket* data, ObjectGuid guid)
{
    data->initialize(SMSG_BATTLEGROUND_PLAYER_LEFT, 8);
    *data << ObjectGuid(guid);
}

void BattleGroundMgr::BuildPlayerJoinedBattleGroundPacket(
    WorldPacket* data, Player* plr)
{
    data->initialize(SMSG_BATTLEGROUND_PLAYER_JOINED, 8);
    *data << plr->GetObjectGuid();
}

BattleGround* BattleGroundMgr::GetBattleGround(
    uint32 InstanceID, BattleGroundTypeId /*bgTypeId*/)
{
    // TODO: Cleanup + revise method (kinda stinks atm)
    return battlefield_container_.get_battleground(InstanceID);
}

void BattleGroundMgr::Update(uint32 diff)
{
    update_invites();

    delete_finished_battlegrounds();

    // Limit updates of the queue
    update_timer_ += diff;
    if (update_timer_ < static_cast<uint32>(update_interval))
        return;
    update_timer_ -= static_cast<uint32>(update_interval);

    running_battlegrounds_invites();

    pop_new_battlegrounds();
}

void BattleGroundMgr::delete_finished_battlegrounds()
{
    for (auto itr = battlefield_container_.begin();
         itr != battlefield_container_.end();)
    {
        BattleGround* bg = *itr;
        if (bg->IsTerminated())
        {
            uint32 map_id = bg->GetMapId(), instance_id = bg->GetInstanceID();
            itr = battlefield_container_.erase(itr);

            // Start expiry timer of our client instance ID (to not reuse it
            // right away)
            begin_instance_id_expiry(
                bg->GetClientInstanceID(), bg->GetTypeID(), bg->get_bracket());

            sBattlefieldQueue::Instance()->upgrade_battlefield(
                bg->get_specification(), bg->GetClientInstanceID());

            // If rating was never distributed we count both teams as losers.
            // This function does nothing if rating has already been distributed
            // for this game
            if (bg->isArena() && bg->isRated())
                distribute_arena_rating(bg->GetInstanceID(), 0);

            sMapMgr::Instance()->DeleteInstance(map_id, instance_id);
            delete bg; // must be deleted absolutely last
        }
        else
            ++itr;
    }
}

void BattleGroundMgr::running_battlegrounds_invites()
{
    for (auto bg : battlefield_container_)
    {
        if (bg->isArena())
            continue; // Cannot invite to running arena

        uint32 alliance = bg->GetFreeSlotsForTeam(ALLIANCE);
        uint32 horde = bg->GetFreeSlotsForTeam(HORDE);
        if (bg->get_specification().get_type() == battlefield::alterac_valley &&
            !sBattleGroundMgr::Instance()
                 ->debugging()) // don't use AV pop logic when debugging
        {
            battlefield::queue::player_list list =
                sBattlefieldQueue::Instance()->av_pop(bg->get_specification(),
                    alliance, horde, bg->GetClientInstanceID());
            invite_player_list(bg, list);
        }
        else
        {
            if (alliance)
                invite_player_list(bg,
                    sBattlefieldQueue::Instance()->pop(bg->get_specification(),
                        battlefield::alliance, alliance,
                        bg->GetClientInstanceID()));
            if (horde)
                invite_player_list(bg,
                    sBattlefieldQueue::Instance()->pop(bg->get_specification(),
                        battlefield::horde, horde, bg->GetClientInstanceID()));
        }
    }
}

void BattleGroundMgr::invite_player_list(
    BattleGround* bg, const battlefield::queue::player_list& players)
{
    for (const auto& players_itr : players)
    {
        if (Player* player = ObjectAccessor::FindPlayer(players_itr))
            send_invite(bg, player);
    }
}

void BattleGroundMgr::pop_new_battlegrounds()
{
    // TODO: Exmaine how long it'd take to pop all types at once in a big sized
    // queue
    // Battlegrounds
    pop_all_brackets(battlefield::specification(battlefield::alterac_valley));
    pop_all_brackets(battlefield::specification(battlefield::warsong_gulch));
    pop_all_brackets(battlefield::specification(battlefield::arathi_basin));
    pop_all_brackets(battlefield::specification(battlefield::eye_of_the_storm));
    // Arenas
    pop_all_brackets(battlefield::specification(battlefield::skirmish_2v2));
    pop_all_brackets(battlefield::specification(battlefield::skirmish_3v3));
    pop_all_brackets(battlefield::specification(battlefield::skirmish_5v5));
    pop_all_brackets(battlefield::specification(battlefield::rated_2v2));
    pop_all_brackets(battlefield::specification(battlefield::rated_3v3));
    pop_all_brackets(battlefield::specification(battlefield::rated_5v5));
}

void BattleGroundMgr::pop_all_brackets(const battlefield::specification& spec)
{
    battlefield::bracket bracket = spec.min_bracket();
    battlefield::bracket max_bracket = spec.max_bracket();
    while (bracket <= max_bracket)
    {
        if (spec.arena())
        {
            if (spec.rated())
                pop_rated_arena(
                    battlefield::specification(spec.get_type(), bracket));
            else
                pop_skirmish_arena(
                    battlefield::specification(spec.get_type(), bracket));
        }
        else
        {
            pop_battleground(
                battlefield::specification(spec.get_type(), bracket));
        }
        ++bracket;
    }
}

bool BattleGroundMgr::pop_battleground(const battlefield::specification& spec)
{
    battlefield::queue::player_list pops =
        sBattlefieldQueue::Instance()->dual_pop(spec);
    if (pops.empty())
        return false;

    BattleGround* bg = create_battlefield(spec);
    if (!bg)
        return false;

    for (auto& pop : pops)
    {
        Player* player = ObjectAccessor::FindPlayer(pop, false);
        if (!player)
            continue;
        send_invite(bg, player);
    }

    bg->StartBattleGround();
    battlefield_container_.insert(bg);

    return true;
}

bool BattleGroundMgr::pop_rated_arena(const battlefield::specification& spec)
{
    battlefield::arena_rating::distributor* dist =
        sBattlefieldQueue::Instance()->pop_rated_arena(spec);
    if (!dist)
        return false;

    BattleGround* bg = create_battlefield(spec);
    if (!bg)
        return false;

    // Invite team one
    for (auto itr = dist->team_one()->players.begin();
         itr != dist->team_one()->players.end(); ++itr)
    {
        Player* player = ObjectAccessor::FindPlayer(*itr, false);
        if (!player)
            continue;
        send_invite(bg, player, dist->team_one()->team);
    }
    // Invite team two
    for (auto itr = dist->team_two()->players.begin();
         itr != dist->team_two()->players.end(); ++itr)
    {
        Player* player = ObjectAccessor::FindPlayer(*itr, false);
        if (!player)
            continue;
        send_invite(bg, player, dist->team_two()->team);
    }

    bg->SetArenaTeamIdForTeam(
        dist->team_one()->team, dist->team_one()->arena_team_id);
    bg->SetArenaTeamIdForTeam(
        dist->team_two()->team, dist->team_two()->arena_team_id);

    bg->StartBattleGround();
    battlefield_container_.insert(bg);
    rating_distributors_[bg->GetInstanceID()] = dist;

    return true;
}

bool BattleGroundMgr::pop_skirmish_arena(const battlefield::specification& spec)
{
    // Note: The first in the pair is always invited as Alliance and the second
    // in the pair as Horde
    std::pair<battlefield::queue::player_list, battlefield::queue::player_list>
        pops = sBattlefieldQueue::Instance()->pop_skirmish(spec);
    if (pops.first.empty() || pops.first.size() != pops.second.size())
        return false;

    BattleGround* bg = create_battlefield(spec);
    if (!bg)
        return false;

    // Invite team one
    for (auto& elem : pops.first)
    {
        Player* player = ObjectAccessor::FindPlayer(elem, false);
        if (!player)
            continue;
        send_invite(bg, player, ALLIANCE); // See note above
    }
    // Invite team two
    for (auto& elem : pops.second)
    {
        Player* player = ObjectAccessor::FindPlayer(elem, false);
        if (!player)
            continue;
        send_invite(bg, player, HORDE); // See note above
    }

    bg->StartBattleGround();
    battlefield_container_.insert(bg);

    return true;
}

void BattleGroundMgr::send_invite(
    BattleGround* bg, Player* player, Team invite_to)
{
    assert(invites_.find(player->GetObjectGuid()) == invites_.end());

    player->SetBGTeam(invite_to == TEAM_NONE ? player->GetTeam() : invite_to);
    bg->add_invite(player->GetObjectGuid());
    bg->IncreaseInvitedCount(player->GetBGTeam());
    player_invite invite = {WorldTimer::getMSTime(), false, bg};
    invites_[player->GetObjectGuid()] = invite;
    send_status_packets(player, true);
}

BattleGround* BattleGroundMgr::get_pending_invite(ObjectGuid player) const
{
    auto itr = invites_.find(player);
    if (itr != invites_.end())
        return itr->second.bg;
    return nullptr;
}

void BattleGroundMgr::attempt_accept_invite(Player* player)
{
    auto itr = invites_.find(player->GetObjectGuid());
    if (itr != invites_.end())
    {
        BattleGround* bg = itr->second.bg;

        if (!player->isAlive())
        {
            player->ResurrectPlayer(1.0f);
            player->SpawnCorpseBones();
        }
        if (player->IsTaxiFlying())
        {
            player->movement_gens.reset();
            player->m_taxi.ClearTaxiDestinations();
        }
        player->SetBattleGroundId(bg->GetInstanceID(), bg->GetTypeID());

        bg->remove_invite(player->GetObjectGuid());
        invites_.erase(itr);

        bg->SendToBattleGround(player);
    }
}

void BattleGroundMgr::attempt_leave_queue(
    Player* player, const battlefield::specification& spec)
{
    // Check if what we're leaving is a pending invite (a queue that has popped)
    auto itr = invites_.find(player->GetObjectGuid());
    if (itr != invites_.end())
    {
        BattleGround* bg = itr->second.bg;
        // We cannot leave the queue for a popped arena, wheter it's skirmish or
        // rated
        if (!bg->isArena())
        {
            if (spec == bg->get_specification())
            {
                bg->DecreaseInvitedCount(player->GetBGTeam());
                invites_.erase(itr);
                bg->remove_invite(player->GetObjectGuid());
                player->SetBGTeam(TEAM_NONE);
                send_status_packets(player);
                return;
            }
        }
    }

    battlefield::specification actual_spec = spec;
    if (spec.arena())
    {
        // Figure out if the arena queue is rated or skirmish
        // Blizzard does not supply this value in their network packet.
        // We need to adhere it from the queue
        battlefield::queue::entry_list players_queues =
            sBattlefieldQueue::Instance()->current_queues(
                player->GetObjectGuid());
        auto itr = players_queues.begin();
        if (itr != players_queues.end())
        {
            battlefield::specification inner_spec = (*itr)->get_specification();
            if (inner_spec.arena() && inner_spec.rated())
                actual_spec = battlefield::specification(spec.blizzard_id(),
                    spec.get_bracket(), spec.arena_size(), true);
            else
                actual_spec = battlefield::specification(spec.blizzard_id(),
                    spec.get_bracket(), spec.arena_size(), false);
        }
    }

    // We're leaving a queue
    std::vector<ObjectGuid> dequeued_players; // In case of rated arenas
    sBattlefieldQueue::Instance()->remove_player(
        player->GetObjectGuid(), actual_spec.get_type(), &dequeued_players);
    for (auto& dequeued_player : dequeued_players)
    {
        if (Player* player = ObjectAccessor::FindPlayer(dequeued_player))
        {
            send_status_packets(player);
            if (actual_spec.arena() && actual_spec.rated())
            {
                WorldPacket data(SMSG_GROUP_JOINED_BATTLEGROUND, 4);
                data << battlefield::queue_result::error_team_left;
                player->GetSession()->send_packet(std::move(data));
            }
        }
    }
    // Note: It seems the client does not need/expect us to send a new status
    // packets for it if we left a queue
}

void BattleGroundMgr::update_invites()
{
    uint32 timestamp = WorldTimer::getMSTime();
    for (auto itr = invites_.begin(); itr != invites_.end();)
    {
        auto current = itr++;
        if (timestamp - current->second.timestamp_ms >=
            (current->second.bg->isArena() ? invite_arena_timeout :
                                             invite_timeout))
        {
            ObjectGuid guid = current->first;
            Player* player = ObjectAccessor::FindPlayer(guid);
            BattleGround* bg = current->second.bg;

            bg->remove_invite(guid);
            invites_.erase(current);
            if (player)
            {
                bg->DecreaseInvitedCount(player->GetBGTeam());
                player->SetBGTeam(TEAM_NONE);
                send_status_packets(player);
            }
        }
        else if (!current->second.sent_twice &&
                 timestamp - current->second.timestamp_ms >= invite_reminder)
        {
            if (Player* player = ObjectAccessor::FindPlayer(current->first))
                send_status_packets(player, true);
            current->second.sent_twice = true;
        }
    }
}

BattleGround* BattleGroundMgr::create_battlefield(
    const battlefield::specification& spec)
{
    BattleGround* bg = nullptr;
    switch (spec.get_type())
    {
    case battlefield::alterac_valley:
        bg = new BattleGroundAV(spec.get_bracket());
        break;
    case battlefield::warsong_gulch:
        bg = new BattleGroundWS(spec.get_bracket());
        break;
    case battlefield::arathi_basin:
        bg = new BattleGroundAB(spec.get_bracket());
        break;
    case battlefield::eye_of_the_storm:
        bg = new BattleGroundEY(spec.get_bracket());
        break;
    case battlefield::skirmish_2v2:
    case battlefield::skirmish_3v3:
    case battlefield::skirmish_5v5:
    case battlefield::rated_2v2:
    case battlefield::rated_3v3:
    case battlefield::rated_5v5:
    {
        switch (urand(0, 2))
        {
        case 0:
            bg = new BattleGroundBE(spec);
            break;
        case 1:
            bg = new BattleGroundNA(spec);
            break;
        case 2:
            bg = new BattleGroundRL(spec);
            break;
        }
        break;
    }
    default:
        return nullptr;
    }

    if (!bg)
        return bg;

    sMapMgr::Instance()->CreateBgMap(bg->GetMapId(), bg);
    bg->SetClientInstanceID(
        next_client_instance_id(spec.blizzard_id(), spec.get_bracket()));

    return bg;
}

uint32 BattleGroundMgr::next_client_instance_id(
    BattleGroundTypeId bg_type, const battlefield::bracket& bracket)
{
    // Arenas don't have client instance IDs
    if (bg_type != BATTLEGROUND_AV && bg_type != BATTLEGROUND_WS &&
        bg_type != BATTLEGROUND_AB && bg_type != BATTLEGROUND_EY)
        return 0;

    clean_old_instance_ids();

    // Get the lowest available client instance id for this battleground type
    // and bracket
    uint32 lowest_found_id = 1;
    bool found = false;
    while (!found)
    {
        found = true;
        for (auto& elem : occupied_client_ids_)
        {
            if (elem.bg_type != bg_type ||
                elem.bracket_min_level != bracket.min())
                continue;

            if (lowest_found_id == elem.id)
            {
                ++lowest_found_id;
                found = false;
                break; // for-loop
            }
        }
    }

    // Save our found id as occupied with no expiry (expiry will be activated
    // when BG is deleted)
    client_instance_id new_entry = {lowest_found_id, bg_type, bracket.min(), 0};
    occupied_client_ids_.push_back(new_entry);

    return lowest_found_id;
}

void BattleGroundMgr::begin_instance_id_expiry(
    uint32 id, BattleGroundTypeId bg_type, const battlefield::bracket& bracket)
{
    for (auto& elem : occupied_client_ids_)
    {
        if (elem.bg_type == bg_type &&
            elem.bracket_min_level == bracket.min() && elem.id == id)
        {
            time_t now = WorldTimer::time_no_syscall();
            elem.expiry_timestamp = now + client_instance_id_expiry_seconds;
            break;
        }
    }
}

void BattleGroundMgr::clean_old_instance_ids()
{
    time_t now = WorldTimer::time_no_syscall();
    for (auto itr = occupied_client_ids_.begin();
         itr != occupied_client_ids_.end();)
    {
        if (itr->expiry_timestamp != 0 &&
            now >= itr->expiry_timestamp) // 0 means still in use
            itr = occupied_client_ids_.erase(itr);
        else
            ++itr;
    }
}

std::pair<int32, int32> BattleGroundMgr::distribute_arena_rating(
    uint32 instance_id, uint32 victorious_arena_team_id)
{
    std::lock_guard<std::mutex> guard(distribute_arena_mutex_);

    auto itr = rating_distributors_.find(instance_id);
    if (itr == rating_distributors_.end())
        return std::make_pair(0, 0);

    battlefield::arena_rating::distributor* dist = itr->second;
    std::pair<int32, int32> result = dist->match_over(victorious_arena_team_id);

    delete dist;
    rating_distributors_.erase(itr);

    return result;
}

bool BattleGroundMgr::can_leave_arena_team(
    const std::vector<ObjectGuid>& players) const
{
    // If we're queued for a rated arena, has a pending invite to, or is inside
    // of one
    // we just flat out deny leaving arena teams, to simplify the logic
    for (const auto& players_itr : players)
    {
        auto find = invites_.find(players_itr);
        if (find != invites_.end() && find->second.bg->isArena() &&
            find->second.bg->isRated())
            return false;
        battlefield::queue::entry_list list =
            sBattlefieldQueue::Instance()->current_queues(players_itr);
        for (auto& elem : list)
        {
            battlefield::specification spec = (elem)->get_specification();
            if (spec.arena() && spec.rated())
                return false;
        }
        Player* player = sObjectMgr::Instance()->GetPlayer(players_itr, false);
        if (player)
        {
            BattleGround* bg = player->GetBattleGround();
            if (bg && bg->isArena() && bg->isRated())
                return false;
        }
    }
    return true;
}

void BattleGroundMgr::InitAutomaticArenaPointDistribution()
{
    if (sWorld::Instance()->getConfig(CONFIG_BOOL_ARENA_AUTO_DISTRIBUTE_POINTS))
    {
        LOG_DEBUG(logging, "Initializing Automatic Arena Point Distribution");
        QueryResult* result = CharacterDatabase.Query(
            "SELECT NextArenaPointDistributionTime FROM saved_variables");
        if (!result)
        {
            LOG_DEBUG(logging,
                "Battleground: Next arena point distribution time not found in "
                "SavedVariables, reseting it now.");
            m_NextAutoDistributionTime = time_t(
                WorldTimer::time_no_syscall() +
                BATTLEGROUND_ARENA_POINT_DISTRIBUTION_DAY *
                    sWorld::Instance()->getConfig(
                        CONFIG_UINT32_ARENA_AUTO_DISTRIBUTE_INTERVAL_DAYS));
            CharacterDatabase.PExecute(
                "INSERT INTO saved_variables (NextArenaPointDistributionTime) "
                "VALUES ('" UI64FMTD "')",
                uint64(m_NextAutoDistributionTime));
        }
        else
        {
            m_NextAutoDistributionTime = time_t((*result)[0].GetUInt64());
            delete result;
        }
        LOG_DEBUG(logging, "Automatic Arena Point Distribution initialized.");
    }
}

void BattleGroundMgr::DistributeArenaPoints()
{
    // used to distribute arena points based on last week's stats
    sWorld::Instance()->SendWorldText(LANG_DIST_ARENA_POINTS_START);

    sWorld::Instance()->SendWorldText(LANG_DIST_ARENA_POINTS_ONLINE_START);

    // temporary structure for storing maximum points to add values for all
    // players
    std::map<uint32, uint32> PlayerPoints;

    // at first update all points for all team members
    {
        sObjectMgr::instance objMgr =
            sObjectMgr::Instance(); // Lock ObjectMgr until we go out of scope
        for (ObjectMgr::ArenaTeamMap::const_iterator team_itr =
                 objMgr->GetArenaTeamMapBegin();
             team_itr != objMgr->GetArenaTeamMapEnd(); ++team_itr)
        {
            if (ArenaTeam* at = team_itr->second)
            {
                at->UpdateArenaPointsHelper(PlayerPoints);
            }
        }
    }

    // cycle that gives points to all players
    for (auto& PlayerPoint : PlayerPoints)
    {
        // update to database
        CharacterDatabase.PExecute(
            "UPDATE characters SET arenaPoints = arenaPoints + '%u' WHERE guid "
            "= '%u'",
            PlayerPoint.second, PlayerPoint.first);
        // add points if player is online
        if (Player* pl = sObjectMgr::Instance()->GetPlayer(
                ObjectGuid(HIGHGUID_PLAYER, PlayerPoint.first)))
            pl->ModifyArenaPoints(PlayerPoint.second);
    }

    PlayerPoints.clear();

    sWorld::Instance()->SendWorldText(LANG_DIST_ARENA_POINTS_ONLINE_END);

    sWorld::Instance()->SendWorldText(LANG_DIST_ARENA_POINTS_TEAM_START);
    {
        sObjectMgr::instance objMgr =
            sObjectMgr::Instance(); // Lock ObjectMgr until we go out of scope
        for (ObjectMgr::ArenaTeamMap::const_iterator titr =
                 objMgr->GetArenaTeamMapBegin();
             titr != objMgr->GetArenaTeamMapEnd(); ++titr)
        {
            if (ArenaTeam* at = titr->second)
            {
                at->FinishWeek(); // set played this week etc values to 0 in
                                  // memory, too
                at->SaveToDB();   // save changes
                at->NotifyStatsChanged(); // notify the players of the changes
            }
        }
    }

    sWorld::Instance()->SendWorldText(LANG_DIST_ARENA_POINTS_TEAM_END);

    sWorld::Instance()->SendWorldText(LANG_DIST_ARENA_POINTS_END);
}

void BattleGroundMgr::BuildBattleGroundListPacket(WorldPacket* data,
    ObjectGuid guid, Player* plr, BattleGroundTypeId bgTypeId)
{
    if (!plr)
        return;

    switch (bgTypeId)
    {
    case BATTLEGROUND_AV:
    case BATTLEGROUND_WS:
    case BATTLEGROUND_AB:
    case BATTLEGROUND_EY:
    case BATTLEGROUND_AA:
        break;
    default:
        return;
    }

    data->initialize(SMSG_BATTLEFIELD_LIST);
    *data << guid;                   // battlemaster guid
    *data << uint32(bgTypeId);       // battleground id
    if (bgTypeId == BATTLEGROUND_AA) // arena
    {
        *data << uint8(5);  // unk
        *data << uint32(0); // unk
    }
    else // battleground
    {
        *data << uint8(0x00); // unk

        battlefield::specification player_spec(
            battlefield::specification(bgTypeId).get_type(), plr->getLevel());
        const battlefield::container::battlegrounds* bgs =
            battlefield_container_.find(player_spec);
        if (!bgs)
        {
            *data << uint32(0);
            return;
        }

        *data << uint32(bgs->size());
        for (const auto& bg : *bgs)
        {
            *data << uint32((bg)->GetClientInstanceID());
        }
    }
}

bool BattleGroundMgr::IsArenaType(BattleGroundTypeId bgTypeId)
{
    switch (bgTypeId)
    {
    case BATTLEGROUND_NA:
    case BATTLEGROUND_BE:
    case BATTLEGROUND_RL:
    case BATTLEGROUND_AA:
        return true;
    default:
        return false;
    };
}

uint32 BattleGroundMgr::GetMaxRatingDifference() const
{
    // this is for stupid people who can't use brain and set max rating
    // difference to 0
    uint32 diff = sWorld::Instance()->getConfig(
        CONFIG_UINT32_ARENA_MAX_RATING_DIFFERENCE);
    if (diff == 0)
        diff = 5000;
    return diff;
}

uint32 BattleGroundMgr::GetRatingDiscardTimer() const
{
    return sWorld::Instance()->getConfig(
        CONFIG_UINT32_ARENA_RATING_DISCARD_TIMER);
}

uint32 BattleGroundMgr::GetPrematureFinishTime() const
{
    return sWorld::Instance()->getConfig(
        CONFIG_UINT32_BATTLEGROUND_PREMATURE_FINISH_TIMER);
}

void BattleGroundMgr::LoadBattleMastersEntry()
{
    mBattleMastersMap.clear(); // need for reload case

    QueryResult* result =
        WorldDatabase.Query("SELECT entry,bg_template FROM battlemaster_entry");

    uint32 count = 0;

    if (!result)
    {
        logging.info("Loaded 0 battlemaster entries - table is empty!\n");
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        ++count;
        bar.step();

        Field* fields = result->Fetch();

        uint32 entry = fields[0].GetUInt32();
        uint32 bgTypeId = fields[1].GetUInt32();
        if (!sBattlemasterListStore.LookupEntry(bgTypeId))
        {
            logging.error(
                "Table `battlemaster_entry` contain entry %u for nonexistent "
                "battleground type %u, ignored.",
                entry, bgTypeId);
            continue;
        }

        mBattleMastersMap[entry] = BattleGroundTypeId(bgTypeId);

    } while (result->NextRow());

    delete result;

    logging.info("Loaded %u battlemaster entries\n", count);
}

HolidayIds BattleGroundMgr::BGTypeToWeekendHolidayId(
    BattleGroundTypeId bgTypeId)
{
    switch (bgTypeId)
    {
    case BATTLEGROUND_AV:
        return HOLIDAY_CALL_TO_ARMS_AV;
    case BATTLEGROUND_EY:
        return HOLIDAY_CALL_TO_ARMS_EY;
    case BATTLEGROUND_WS:
        return HOLIDAY_CALL_TO_ARMS_WS;
    case BATTLEGROUND_AB:
        return HOLIDAY_CALL_TO_ARMS_AB;
    default:
        return HOLIDAY_NONE;
    }
}

BattleGroundTypeId BattleGroundMgr::WeekendHolidayIdToBGType(HolidayIds holiday)
{
    switch (holiday)
    {
    case HOLIDAY_CALL_TO_ARMS_AV:
        return BATTLEGROUND_AV;
    case HOLIDAY_CALL_TO_ARMS_EY:
        return BATTLEGROUND_EY;
    case HOLIDAY_CALL_TO_ARMS_WS:
        return BATTLEGROUND_WS;
    case HOLIDAY_CALL_TO_ARMS_AB:
        return BATTLEGROUND_AB;
    default:
        return BATTLEGROUND_TYPE_NONE;
    }
}

bool BattleGroundMgr::IsBGWeekend(BattleGroundTypeId bgTypeId)
{
    return sGameEventMgr::Instance()->IsActiveHoliday(
        BGTypeToWeekendHolidayId(bgTypeId));
}

void BattleGroundMgr::LoadBattleEventIndexes()
{
    BattleGroundEventIdx events;
    events.event1 = BG_EVENT_NONE;
    events.event2 = BG_EVENT_NONE;
    m_GameObjectBattleEventIndexMap.clear(); // need for reload case
    m_GameObjectBattleEventIndexMap[-1] = events;
    m_CreatureBattleEventIndexMap.clear(); // need for reload case
    m_CreatureBattleEventIndexMap[-1] = events;

    uint32 count = 0;

    QueryResult* result =
        //                           0         1           2                3
        //                           4              5           6
        WorldDatabase.Query(
            "SELECT data.typ, data.guid1, data.ev1 AS ev1, data.ev2 AS ev2, "
            "data.map AS m, data.guid2, description.map, "
            //                              7                  8 9
            "description.event1, description.event2, description.description "
            "FROM "
            "(SELECT '1' AS typ, a.guid AS guid1, a.event1 AS ev1, a.event2 AS "
            "ev2, b.map AS map, b.guid AS guid2 "
            "FROM gameobject_battleground AS a "
            "LEFT OUTER JOIN gameobject AS b ON a.guid = b.guid "
            "UNION "
            "SELECT '2' AS typ, a.guid AS guid1, a.event1 AS ev1, a.event2 AS "
            "ev2, b.map AS map, b.guid AS guid2 "
            "FROM creature_battleground AS a "
            "LEFT OUTER JOIN creature AS b ON a.guid = b.guid "
            ") data "
            "RIGHT OUTER JOIN battleground_events AS description ON data.map = "
            "description.map "
            "AND data.ev1 = description.event1 AND data.ev2 = "
            "description.event2 "
            // full outer join doesn't work in mysql :-/ so just UNION-select
            // the same again and add a left outer join
            "UNION "
            "SELECT data.typ, data.guid1, data.ev1, data.ev2, data.map, "
            "data.guid2, description.map, "
            "description.event1, description.event2, description.description "
            "FROM "
            "(SELECT '1' AS typ, a.guid AS guid1, a.event1 AS ev1, a.event2 AS "
            "ev2, b.map AS map, b.guid AS guid2 "
            "FROM gameobject_battleground AS a "
            "LEFT OUTER JOIN gameobject AS b ON a.guid = b.guid "
            "UNION "
            "SELECT '2' AS typ, a.guid AS guid1, a.event1 AS ev1, a.event2 AS "
            "ev2, b.map AS map, b.guid AS guid2 "
            "FROM creature_battleground AS a "
            "LEFT OUTER JOIN creature AS b ON a.guid = b.guid "
            ") data "
            "LEFT OUTER JOIN battleground_events AS description ON data.map = "
            "description.map "
            "AND data.ev1 = description.event1 AND data.ev2 = "
            "description.event2 "
            "ORDER BY m, ev1, ev2");
    if (!result)
    {
        logging.error("Loaded 0 battleground eventindexes.\n");
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        bar.step();
        Field* fields = result->Fetch();
        if (fields[2].GetUInt8() == BG_EVENT_NONE ||
            fields[3].GetUInt8() == BG_EVENT_NONE)
            continue; // we don't need to add those to the eventmap

        bool gameobject = (fields[0].GetUInt8() == 1);
        uint32 dbTableGuidLow = fields[1].GetUInt32();
        events.event1 = fields[2].GetUInt8();
        events.event2 = fields[3].GetUInt8();
        uint32 map = fields[4].GetUInt32();

        uint32 desc_map = fields[6].GetUInt32();
        uint8 desc_event1 = fields[7].GetUInt8();
        uint8 desc_event2 = fields[8].GetUInt8();
        const char* description = fields[9].GetString();

        // checking for NULL - through right outer join this will mean
        // following:
        if (fields[5].GetUInt32() != dbTableGuidLow)
        {
            logging.error(
                "BattleGroundEvent: %s with nonexistent guid %u for event: "
                "map:%u, event1:%u, event2:%u (\"%s\")",
                (gameobject) ? "gameobject" : "creature", dbTableGuidLow, map,
                events.event1, events.event2, description);
            continue;
        }

        // checking for NULL - through full outer join this can mean 2 things:
        if (desc_map != map)
        {
            // there is an event missing
            if (dbTableGuidLow == 0)
            {
                logging.error(
                    "BattleGroundEvent: missing db-data for map:%u, event1:%u, "
                    "event2:%u (\"%s\")",
                    desc_map, desc_event1, desc_event2, description);
                continue;
            }
            // we have an event which shouldn't exist
            else
            {
                logging.error(
                    "BattleGroundEvent: %s with guid %u is registered, for a "
                    "nonexistent event: map:%u, event1:%u, event2:%u",
                    (gameobject) ? "gameobject" : "creature", dbTableGuidLow,
                    map, events.event1, events.event2);
                continue;
            }
        }

        if (gameobject)
            m_GameObjectBattleEventIndexMap[dbTableGuidLow] = events;
        else
            m_CreatureBattleEventIndexMap[dbTableGuidLow] = events;

        ++count;

    } while (result->NextRow());

    logging.info("Loaded %u battleground eventindexes\n", count);
    delete result;
}
