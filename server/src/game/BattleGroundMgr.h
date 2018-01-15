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

#ifndef __BATTLEGROUNDMGR_H
#define __BATTLEGROUNDMGR_H

#include "BattleGround.h"
#include "Common.h"
#include "DBCEnums.h"
#include "SharedDefines.h"
#include "battlefield_arena_rating.h"
#include "battlefield_container.h"
#include "battlefield_queue.h"
#include "Policies/Singleton.h"
#include "Utilities/EventProcessor.h"
#include <boost/thread/mutex.hpp>
#include <unordered_map>

class BattleGround;

typedef std::unordered_map<uint32, BattleGroundTypeId> BattleMastersMap;
typedef std::unordered_map<uint32, BattleGroundEventIdx>
    CreatureBattleEventIndexesMap;
typedef std::unordered_map<uint32, BattleGroundEventIdx>
    GameObjectBattleEventIndexesMap;

#define BATTLEGROUND_ARENA_POINT_DISTRIBUTION_DAY 86400 // seconds in a day

class BattleGroundMgr
{
public:
    /* Construction */
    BattleGroundMgr();
    ~BattleGroundMgr();
    void Update(uint32 diff);

    /* Packet Building */
    void BuildPlayerJoinedBattleGroundPacket(WorldPacket* data, Player* plr);
    void BuildPlayerLeftBattleGroundPacket(WorldPacket* data, ObjectGuid guid);
    void BuildBattleGroundListPacket(WorldPacket* data, ObjectGuid guid,
        Player* plr, BattleGroundTypeId bgTypeId);
    void BuildGroupJoinedBattlegroundPacket(
        WorldPacket* data, BattleGroundTypeId bgTypeId);
    void BuildUpdateWorldStatePacket(
        WorldPacket* data, uint32 field, uint32 value);
    void BuildPvpLogDataPacket(WorldPacket* data, BattleGround* bg);
    void BuildPlaySoundPacket(WorldPacket* data, uint32 soundid);
    void send_status_packets(Player* player, bool send_pending = false);

    /* Battlegrounds */
    BattleGround* GetBattleGround(uint32 InstanceID,
        BattleGroundTypeId bgTypeId); // there must be uint32 because
                                      // MAX_BATTLEGROUND_TYPE_ID means unknown

    BattleGround* create_battlefield(const battlefield::specification& spec);
    uint32 next_client_instance_id(
        BattleGroundTypeId bg_type, const battlefield::bracket& bracket);

    void DeleteAllBattleGrounds();

    uint32 GetMaxRatingDifference() const;
    uint32 GetRatingDiscardTimer() const;
    uint32 GetPrematureFinishTime() const;

    void InitAutomaticArenaPointDistribution();
    void DistributeArenaPoints();

    void LoadBattleMastersEntry();
    BattleGroundTypeId GetBattleMasterBG(uint32 entry) const
    {
        auto itr = mBattleMastersMap.find(entry);
        if (itr != mBattleMastersMap.end())
            return itr->second;
        return BATTLEGROUND_TYPE_NONE;
    }

    void LoadBattleEventIndexes();
    const BattleGroundEventIdx GetCreatureEventIndex(
        uint32 dbTableGuidLow) const
    {
        auto itr = m_CreatureBattleEventIndexMap.find(dbTableGuidLow);
        if (itr != m_CreatureBattleEventIndexMap.end())
            return itr->second;
        return m_CreatureBattleEventIndexMap.find(-1)->second;
    }
    const BattleGroundEventIdx GetGameObjectEventIndex(
        uint32 dbTableGuidLow) const
    {
        auto itr = m_GameObjectBattleEventIndexMap.find(dbTableGuidLow);
        if (itr != m_GameObjectBattleEventIndexMap.end())
            return itr->second;
        return m_GameObjectBattleEventIndexMap.find(-1)->second;
    }

    static bool IsArenaType(BattleGroundTypeId bgTypeId);
    static bool IsBattleGroundType(BattleGroundTypeId bgTypeId)
    {
        return !BattleGroundMgr::IsArenaType(bgTypeId);
    }

    static HolidayIds BGTypeToWeekendHolidayId(BattleGroundTypeId bgTypeId);
    static BattleGroundTypeId WeekendHolidayIdToBGType(HolidayIds holiday);
    static bool IsBGWeekend(BattleGroundTypeId bgTypeId);

    void attempt_accept_invite(Player* player);
    // Both for leaving queue and for declining an invite
    void attempt_leave_queue(
        Player* player, const battlefield::specification& spec);

    // Returns a pair of winner (first) and losers (second) rating change
    std::pair<int32, int32> distribute_arena_rating(
        uint32 instance_id, uint32 victorious_arena_team_id);

    bool has_pending_invite(ObjectGuid player) const
    {
        return invites_.find(player) != invites_.end();
    }
    BattleGround* get_pending_invite(ObjectGuid player) const;

    void set_debugging(bool on) { debugging_pvp_ = on; }
    bool debugging() const { return debugging_pvp_; }

    // Returns false if we're in a queue or a battleground
    bool can_leave_arena_team(ObjectGuid player) const
    {
        std::vector<ObjectGuid> vec;
        vec.push_back(player);
        return can_leave_arena_team(vec);
    }
    bool can_leave_arena_team(const std::vector<ObjectGuid>& players) const;

private:
    const uint32 invite_timeout = 80000;
    const uint32 invite_arena_timeout = 30000;
    const uint32 invite_reminder = 60000;
    struct player_invite
    {
        uint32 timestamp_ms;
        bool sent_twice;
        BattleGround* bg;
    };
    typedef std::map<ObjectGuid, player_invite> invites_map;
    void send_invite(
        BattleGround* bg, Player* player, Team invited_to = TEAM_NONE);
    void update_invites();
    invites_map invites_;

    battlefield::container battlefield_container_;

    BattleMastersMap mBattleMastersMap;
    CreatureBattleEventIndexesMap m_CreatureBattleEventIndexMap;
    GameObjectBattleEventIndexesMap m_GameObjectBattleEventIndexMap;

    /* Client Instance ID handling */
    static const int client_instance_id_expiry_seconds = 10800; // 3 hours
    struct client_instance_id
    {
        uint32 id;
        BattleGroundTypeId bg_type;
        int bracket_min_level;
        time_t expiry_timestamp; // 0 if still in use
    };
    typedef std::vector<client_instance_id> client_instance_ids;
    client_instance_ids occupied_client_ids_;
    void begin_instance_id_expiry(uint32 id, BattleGroundTypeId bg_type,
        const battlefield::bracket& bracket);
    void clean_old_instance_ids();

    uint32 m_NextRatingDiscardUpdate;
    time_t m_NextAutoDistributionTime;

    uint32 update_timer_;
    const int update_interval = 1000;

    // Functions for Inviting to, managing and popping new Battlegrounds
    void delete_finished_battlegrounds();
    void running_battlegrounds_invites();
    void invite_player_list(
        BattleGround* bg, const battlefield::queue::player_list& players);
    void pop_new_battlegrounds();
    void pop_all_brackets(const battlefield::specification& spec);
    bool pop_battleground(const battlefield::specification& spec);
    bool pop_rated_arena(const battlefield::specification& spec);
    bool pop_skirmish_arena(const battlefield::specification& spec);

    typedef std::map<uint32 /*instance id*/,
        battlefield::arena_rating::distributor*> rating_dist_map;
    rating_dist_map rating_distributors_;

    std::mutex distribute_arena_mutex_;

    bool debugging_pvp_;
};

#define sBattleGroundMgr MaNGOS::UnlockedSingleton<BattleGroundMgr>
#endif
