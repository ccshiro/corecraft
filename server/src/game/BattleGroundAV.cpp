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

#include "BattleGroundAV.h"
#include "BattleGround.h"
#include "BattleGroundMgr.h"
#include "Creature.h"
#include "GameObject.h"
#include "InstanceData.h"
#include "Language.h"
#include "Player.h"
#include "WorldPacket.h"
#include "battlefield_queue.h"
#include <G3D/Vector3.h>

// Constant creature Object Guids
namespace av_guids
{
// Warmasters
static const ObjectGuid icewing_warmaster(HIGHGUID_UNIT, 14774, 1003298);
static const ObjectGuid iceblood_warmaster(HIGHGUID_UNIT, 14773, 97142);
static const ObjectGuid db_north_warmaster(HIGHGUID_UNIT, 14770, 1003297);
static const ObjectGuid west_frost_warmaster(HIGHGUID_UNIT, 14777, 97145);
static const ObjectGuid east_frost_warmaster(HIGHGUID_UNIT, 14772, 97144);
static const ObjectGuid db_south_warmaster(HIGHGUID_UNIT, 14771, 1003296);
static const ObjectGuid tower_point_warmaster(HIGHGUID_UNIT, 14776, 97143);
static const ObjectGuid stonehearth_warmaster(HIGHGUID_UNIT, 14775, 1003295);

// Marshals
static const ObjectGuid db_south_marshal(HIGHGUID_UNIT, 14763, 97138);
static const ObjectGuid west_frost_marshal(HIGHGUID_UNIT, 14769, 54296);
static const ObjectGuid icewing_marshal(HIGHGUID_UNIT, 14764, 97140);
static const ObjectGuid tower_point_marshal(HIGHGUID_UNIT, 14767, 1003190);
static const ObjectGuid iceblood_marshal(HIGHGUID_UNIT, 14766, 54294);
static const ObjectGuid stonehearth_marshal(HIGHGUID_UNIT, 14765, 54293);
static const ObjectGuid east_frost_marshal(HIGHGUID_UNIT, 14768, 54295);
static const ObjectGuid db_north_marshal(HIGHGUID_UNIT, 14762, 54289);
}

BattleGroundAV::BattleGroundAV(const battlefield::bracket& bracket)
  : specification_(battlefield::alterac_valley, bracket),
    custom_honor_timer_(15 * IN_MILLISECONDS)
{
    SetMapId(30);
    SetTypeID(BATTLEGROUND_AV);
    SetArenaorBGType(false);
    SetMinPlayersPerTeam(20);
    SetMaxPlayersPerTeam(40);
    SetMinPlayers(40);
    SetMaxPlayers(80);
    SetName("Alterac Valley");
    SetTeamStartLoc(ALLIANCE, 873.002f, -491.284f, 96.5419f, 3.0425f);
    SetTeamStartLoc(HORDE, -1437.67f, -610.089f, 51.1619f, 0.636f);
    SetLevelRange(51, 70);

    m_StartMessageIds[BG_STARTING_EVENT_FIRST] = 0;
    m_StartMessageIds[BG_STARTING_EVENT_SECOND] = LANG_BG_AV_START_ONE_MINUTE;
    m_StartMessageIds[BG_STARTING_EVENT_THIRD] = LANG_BG_AV_START_HALF_MINUTE;
    m_StartMessageIds[BG_STARTING_EVENT_FOURTH] = LANG_BG_AV_HAS_BEGUN;

    // Stuff that was in BattleGroundAV::Reset() previously:
    // set the reputation and honor variables:
    bool isBGWeekend = BattleGroundMgr::IsBGWeekend(GetTypeID());

    m_HonorMapComplete = (isBGWeekend) ? BG_AV_KILL_MAP_COMPLETE_HOLIDAY :
                                         BG_AV_KILL_MAP_COMPLETE;
    m_RepTowerDestruction =
        (isBGWeekend) ? BG_AV_REP_TOWER_HOLIDAY : BG_AV_REP_TOWER;
    m_RepCaptain =
        (isBGWeekend) ? BG_AV_REP_CAPTAIN_HOLIDAY : BG_AV_REP_CAPTAIN;
    m_RepBoss = (isBGWeekend) ? BG_AV_REP_BOSS_HOLIDAY : BG_AV_REP_BOSS;
    m_RepOwnedGrave =
        (isBGWeekend) ? BG_AV_REP_OWNED_GRAVE_HOLIDAY : BG_AV_REP_OWNED_GRAVE;
    m_RepSurviveCaptain = (isBGWeekend) ? BG_AV_REP_SURVIVING_CAPTAIN_HOLIDAY :
                                          BG_AV_REP_SURVIVING_CAPTAIN;
    m_RepSurviveTower = (isBGWeekend) ? BG_AV_REP_SURVIVING_TOWER_HOLIDAY :
                                        BG_AV_REP_SURVIVING_TOWER;
    m_RepOwnedMine =
        (isBGWeekend) ? BG_AV_REP_OWNED_MINE_HOLIDAY : BG_AV_REP_OWNED_MINE;

    for (uint8 i = 0; i < BG_TEAMS_COUNT; i++)
    {
        m_TeamScores[i] = BG_AV_SCORE_INITIAL_POINTS;
        m_IsInformedNearLose[i] = false;
        m_ActiveEvents[BG_AV_NodeEventCaptainDead_A + i] = BG_EVENT_NONE;
    }

    for (uint8 i = 0; i < BG_AV_MAX_MINES; i++)
    {
        m_Mine_Owner[i] = BG_AV_TEAM_NEUTRAL;
        m_Mine_PrevOwner[i] = m_Mine_Owner[i];
        m_ActiveEvents[BG_AV_MINE_BOSSES + i] = BG_AV_TEAM_NEUTRAL;
        m_ActiveEvents[BG_AV_MINE_EVENT + i] = BG_AV_TEAM_NEUTRAL;
        m_ActiveEvents[BG_AV_MINE_PATROLS + i] = BG_AV_TEAM_NEUTRAL;
        m_Mine_Timer[i] = BG_AV_MINE_TICK_TIMER;
    }

    m_ActiveEvents[BG_AV_CAPTAIN_A] = 0;
    m_ActiveEvents[BG_AV_CAPTAIN_H] = 0;
    m_ActiveEvents[BG_AV_HERALD] = 0;
    m_ActiveEvents[BG_AV_BOSS_A] = 0;
    m_ActiveEvents[BG_AV_BOSS_H] = 0;
    for (BG_AV_Nodes i = BG_AV_NODES_DUNBALDAR_SOUTH;
         i <= BG_AV_NODES_FROSTWOLF_WTOWER; ++i) // towers
        m_ActiveEvents[BG_AV_MARSHAL_A_SOUTH + i -
                       BG_AV_NODES_DUNBALDAR_SOUTH] = 0;

    m_ActiveEvents[BG_AV_LAND_MINES_A] = 0;
    m_ActiveEvents[BG_AV_LAND_MINES_H] = 0;

    // mine supplies are always active, just not interactable
    m_ActiveEvents[BG_AV_MINE_SUPPLIES_N] = 0;
    m_ActiveEvents[BG_AV_MINE_SUPPLIES_S] = 0;

    for (BG_AV_Nodes i = BG_AV_NODES_FIRSTAID_STATION;
         i <= BG_AV_NODES_STONEHEART_GRAVE; ++i) // alliance graves
        InitNode(i, BG_AV_TEAM_ALLIANCE, false);
    for (BG_AV_Nodes i = BG_AV_NODES_DUNBALDAR_SOUTH;
         i <= BG_AV_NODES_STONEHEART_BUNKER; ++i) // alliance towers
        InitNode(i, BG_AV_TEAM_ALLIANCE, true);

    for (BG_AV_Nodes i = BG_AV_NODES_ICEBLOOD_GRAVE;
         i <= BG_AV_NODES_FROSTWOLF_HUT; ++i) // horde graves
        InitNode(i, BG_AV_TEAM_HORDE, false);
    for (BG_AV_Nodes i = BG_AV_NODES_ICEBLOOD_TOWER;
         i <= BG_AV_NODES_FROSTWOLF_WTOWER; ++i) // horde towers
        InitNode(i, BG_AV_TEAM_HORDE, true);

    InitNode(BG_AV_NODES_SNOWFALL_GRAVE, BG_AV_TEAM_NEUTRAL,
        false); // give snowfall neutral owner
                // End of Stuff that was in BattleGroundAV::Reset() previously
}

BattleGroundAV::~BattleGroundAV()
{
}

void BattleGroundAV::HandleKillPlayer(Player* player, Player* killer)
{
    if (GetStatus() != STATUS_IN_PROGRESS)
        return;

    BattleGround::HandleKillPlayer(player, killer);
    // UpdateScore(GetTeamIndexByTeamId(player->GetTeam()), -1);
}

void BattleGroundAV::HandleKillUnit(Creature* creature, Player* killer)
{
    LOG_DEBUG(
        logging, "BattleGroundAV: HandleKillUnit %i", creature->GetEntry());
    if (GetStatus() != STATUS_IN_PROGRESS)
        return;

    switch (creature->GetEntry())
    {
    // Commanders bonus honor
    case 13140:
    case 13152:
    case 13153:
    case 13154:
        RewardHonorToTeam(GetBonusHonorFromKill(2), ALLIANCE);
        break;
    case 13139:
    case 13318:
    case 13319:
    case 13320:
        RewardHonorToTeam(GetBonusHonorFromKill(2), HORDE);
        break;
    // Lieutenants bonus honor
    case 13145:
    case 13147:
    case 13146:
    case 13137:
    case 13143:
    case 13144:
        RewardHonorToTeam(GetBonusHonorFromKill(1), ALLIANCE);
        break;
    case 13300:
    case 13297:
    case 13298:
    case 13299:
    case 13296:
    case 13138:
        RewardHonorToTeam(GetBonusHonorFromKill(1), HORDE);
        break;
    }

    uint8 event1 = (sBattleGroundMgr::Instance()->GetCreatureEventIndex(
                        creature->GetGUIDLow())).event1;
    if (event1 == BG_EVENT_NONE)
        return;
    switch (event1)
    {
    case BG_AV_BOSS_A:
        CastSpellOnTeam(BG_AV_BOSS_KILL_QUEST_SPELL,
            HORDE); // this is a spell which finishes a quest where a player has
                    // to kill the boss
        RewardReputationToTeam(BG_AV_FACTION_H, m_RepBoss, HORDE);
        RewardHonorToTeam(GetBonusHonorFromKill(BG_AV_KILL_BOSS), HORDE);
        SendYellToAll(LANG_BG_AV_A_GENERAL_DEAD, LANG_UNIVERSAL,
            GetSingleCreatureGuid(BG_AV_HERALD, 0));
        EndBattleGround(HORDE);
        break;
    case BG_AV_BOSS_H:
        CastSpellOnTeam(BG_AV_BOSS_KILL_QUEST_SPELL,
            ALLIANCE); // this is a spell which finishes a quest where a player
                       // has to kill the boss
        RewardReputationToTeam(BG_AV_FACTION_A, m_RepBoss, ALLIANCE);
        RewardHonorToTeam(GetBonusHonorFromKill(BG_AV_KILL_BOSS), ALLIANCE);
        SendYellToAll(LANG_BG_AV_H_GENERAL_DEAD, LANG_UNIVERSAL,
            GetSingleCreatureGuid(BG_AV_HERALD, 0));
        EndBattleGround(ALLIANCE);
        break;
    case BG_AV_CAPTAIN_A:
        if (IsActiveEvent(BG_AV_NodeEventCaptainDead_A, 0))
            return;
        RewardReputationToTeam(BG_AV_FACTION_H, m_RepCaptain, HORDE);
        RewardHonorToTeam(GetBonusHonorFromKill(BG_AV_KILL_CAPTAIN), HORDE);
        // UpdateScore(BG_TEAM_ALLIANCE, (-1) * BG_AV_RES_CAPTAIN);
        // spawn destroyed aura
        SpawnEvent(BG_AV_NodeEventCaptainDead_A, 0, true);
        break;
    case BG_AV_CAPTAIN_H:
        if (IsActiveEvent(BG_AV_NodeEventCaptainDead_H, 0))
            return;
        RewardReputationToTeam(BG_AV_FACTION_A, m_RepCaptain, ALLIANCE);
        RewardHonorToTeam(GetBonusHonorFromKill(BG_AV_KILL_CAPTAIN), ALLIANCE);
        // UpdateScore(BG_TEAM_HORDE, (-1) * BG_AV_RES_CAPTAIN);
        // spawn destroyed aura
        SpawnEvent(BG_AV_NodeEventCaptainDead_H, 0, true);
        break;
    case BG_AV_MINE_BOSSES_NORTH:
        ChangeMineOwner(
            BG_AV_NORTH_MINE, GetAVTeamIndexByTeamId(killer->GetTeam()));
        break;
    case BG_AV_MINE_BOSSES_SOUTH:
        ChangeMineOwner(
            BG_AV_SOUTH_MINE, GetAVTeamIndexByTeamId(killer->GetTeam()));
        break;
    }
}

void BattleGroundAV::HandleQuestComplete(uint32 questid, Player* player)
{
    if (GetStatus() != STATUS_IN_PROGRESS)
        return;
    BattleGroundAVTeamIndex teamIdx = GetAVTeamIndexByTeamId(player->GetTeam());
    assert(teamIdx != BG_AV_TEAM_NEUTRAL);

    uint32 reputation = 0; // reputation for the whole team (other reputation
                           // must be done in db)

    switch (questid)
    {
    case BG_AV_QUEST_A_SCRAPS1:
    case BG_AV_QUEST_A_SCRAPS2:
    case BG_AV_QUEST_H_SCRAPS1:
    case BG_AV_QUEST_H_SCRAPS2:
    {
        uint32 data = player->GetTeam() == ALLIANCE ? ALLIANCE_ARMOR_SCRAPS :
                                                      HORDE_ARMOR_SCRAPS;
        reputation = 1;

        // Update instance data
        if (auto inst = GetBgMap()->GetInstanceData())
            inst->SetData(data, inst->GetData(data) + 20);
        break;
    }
    case BG_AV_QUEST_A_COMMANDER1:
        if (auto av = GetBgMap()->GetInstanceData())
            av->SetData(ALLIANCE_SLIDORE_TURNINS,
                av->GetData(ALLIANCE_SLIDORE_TURNINS) + 1);
        break;
    case BG_AV_QUEST_H_COMMANDER1:
        if (auto av = GetBgMap()->GetInstanceData())
            av->SetData(
                HORDE_GUSE_TURNINS, av->GetData(HORDE_GUSE_TURNINS) + 1);
        break;
    case BG_AV_QUEST_A_COMMANDER2:
        if (auto av = GetBgMap()->GetInstanceData())
            av->SetData(ALLIANCE_VIPORE_TURNINS,
                av->GetData(ALLIANCE_VIPORE_TURNINS) + 1);
        break;
    case BG_AV_QUEST_H_COMMANDER2:
        if (auto av = GetBgMap()->GetInstanceData())
            av->SetData(
                HORDE_JEZTOR_TURNINS, av->GetData(HORDE_JEZTOR_TURNINS) + 1);
        break;
    case BG_AV_QUEST_A_COMMANDER3:
        if (auto av = GetBgMap()->GetInstanceData())
            av->SetData(ALLIANCE_ICHMAN_TURNINS,
                av->GetData(ALLIANCE_ICHMAN_TURNINS) + 1);
        break;
    case BG_AV_QUEST_H_COMMANDER3:
        if (auto av = GetBgMap()->GetInstanceData())
            av->SetData(HORDE_MULVERICK_TURNINS,
                av->GetData(HORDE_MULVERICK_TURNINS) + 1);
        break;
    case BG_AV_QUEST_A_BOSS1:
        reputation = 5;
        if (auto av = GetBgMap()->GetInstanceData())
            av->SetData(
                STORM_CRYSTAL_COUNT, av->GetData(STORM_CRYSTAL_COUNT) + 5);
        break;
    case BG_AV_QUEST_H_BOSS1:
        reputation = 5;
        if (auto av = GetBgMap()->GetInstanceData())
            av->SetData(
                SOLDIER_BLOOD_COUNT, av->GetData(SOLDIER_BLOOD_COUNT) + 5);
        break;
    case BG_AV_QUEST_A_BOSS2:
        reputation = 1;
        if (auto av = GetBgMap()->GetInstanceData())
            av->SetData(
                STORM_CRYSTAL_COUNT, av->GetData(STORM_CRYSTAL_COUNT) + 1);
        break;
    case BG_AV_QUEST_H_BOSS2:
        reputation = 1;
        if (auto av = GetBgMap()->GetInstanceData())
            av->SetData(
                SOLDIER_BLOOD_COUNT, av->GetData(SOLDIER_BLOOD_COUNT) + 1);
        break;
    case BG_AV_QUEST_A_NEAR_MINE:
    case BG_AV_QUEST_H_NEAR_MINE:
    {
        reputation = 2;

        uint32 data = teamIdx == BG_AV_TEAM_ALLIANCE ? ALLIANCE_TOTAL_SUPPLIES :
                                                       HORDE_TOTAL_SUPPLIES;

        if (auto av = GetBgMap()->GetInstanceData())
            av->SetData(data, av->GetData(data) + 10);
        break;
    }
    case BG_AV_QUEST_A_OTHER_MINE:
    case BG_AV_QUEST_H_OTHER_MINE:
    {
        reputation = 3;

        uint32 data = teamIdx == BG_AV_TEAM_ALLIANCE ? ALLIANCE_TOTAL_SUPPLIES :
                                                       HORDE_TOTAL_SUPPLIES;

        // you only need 1/4'th of the turn-ins if you use coldtooth supply only
        if (auto av = GetBgMap()->GetInstanceData())
            av->SetData(data, av->GetData(data) + 40);
        break;
    }
    case BG_AV_QUEST_A_RIDER_HIDE:
    case BG_AV_QUEST_H_RIDER_HIDE:
    {
        reputation = 1;

        uint32 data = teamIdx == BG_AV_TEAM_ALLIANCE ?
                          ALLIANCE_COLLECTED_WOLF_HIDES :
                          HORDE_COLLECTED_RAM_HIDES;
        if (auto av = GetBgMap()->GetInstanceData())
            av->SetData(data, av->GetData(data) + 1);
        break;
    }
    case BG_AV_QUEST_A_RIDER_TAME:
    case BG_AV_QUEST_H_RIDER_TAME:
    {
        reputation = 1;

        uint32 data = teamIdx == BG_AV_TEAM_ALLIANCE ?
                          ALLIANCE_TAMED_ALTERAC_RAMS :
                          HORDE_TAMED_FROSTWOLVES;
        if (auto av = GetBgMap()->GetInstanceData())
            av->SetData(data, av->GetData(data) + 1);
        break;
    }
    case BG_AV_QUEST_A_BEGIN_ATTACK:
        if (auto av = GetBgMap()->GetInstanceData())
            av->OnEvent("alliance ground");
        break;
    case BG_AV_QUEST_H_LAUNCH_ATTACK:
        if (auto av = GetBgMap()->GetInstanceData())
            av->OnEvent("horde ground");
        break;
    default:
        LOG_DEBUG(logging,
            "BattleGroundAV: Quest %u completed but is not interesting for us",
            questid);
        return;
        break;
    }
    if (reputation)
        RewardReputationToTeam(
            (player->GetTeam() == ALLIANCE) ? BG_AV_FACTION_A : BG_AV_FACTION_H,
            reputation, player->GetTeam());
}

/*void BattleGroundAV::UpdateScore(BattleGroundTeamIndex teamIdx, int32 points )
{
    // note: to remove reinforcements points must be negative, for adding
reinforcements points must be positive
    assert( teamIdx == BG_TEAM_ALLIANCE || teamIdx == BG_TEAM_HORDE);
    m_TeamScores[teamIdx] += points;                      // m_TeamScores is
int32 - so no problems here

    if (points < 0)
    {
        if (m_TeamScores[teamIdx] < 1)
        {
            m_TeamScores[teamIdx] = 0;
            // other team will win:
            EndBattleGround((teamIdx == BG_TEAM_ALLIANCE)? HORDE : ALLIANCE);
        }
        else if (!m_IsInformedNearLose[teamIdx] && m_TeamScores[teamIdx] <
BG_AV_SCORE_NEAR_LOSE)
        {
            SendMessageToAll((teamIdx == BG_TEAM_HORDE) ? LANG_BG_AV_H_NEAR_LOSE
: LANG_BG_AV_A_NEAR_LOSE, CHAT_MSG_BG_SYSTEM_NEUTRAL);
            PlaySoundToAll(BG_AV_SOUND_NEAR_LOSE);
            m_IsInformedNearLose[teamIdx] = true;
        }
    }
    // must be called here, else it could display a negative value
    UpdateWorldState(((teamIdx == BG_TEAM_HORDE) ? BG_AV_Horde_Score :
BG_AV_Alliance_Score), m_TeamScores[teamIdx]);
}*/

void BattleGroundAV::Update(uint32 diff)
{
    BattleGround::Update(diff);

    if (GetStatus() != STATUS_IN_PROGRESS)
        return;

    // add points from mine owning, and look if the neutral team can reclaim the
    // mine
    for (uint8 mine = 0; mine < BG_AV_MAX_MINES; mine++)
    {
        if (m_Mine_Owner[mine] != BG_AV_TEAM_NEUTRAL)
        {
            m_Mine_Timer[mine] -= diff;
            if (m_Mine_Timer[mine] <= 0)
            {
                // UpdateScore(BattleGroundTeamIndex(m_Mine_Owner[mine]), 1);
                m_Mine_Timer[mine] = BG_AV_MINE_TICK_TIMER;
            }

            if (m_Mine_Reclaim_Timer[mine] > diff)
                m_Mine_Reclaim_Timer[mine] -= diff;
            else
                ChangeMineOwner(mine, BG_AV_TEAM_NEUTRAL);
        }
    }

    // looks for all timers of the nodes and destroy the building (for
    // graveyards the building wont get destroyed, it goes just to the other
    // team
    for (BG_AV_Nodes i = BG_AV_NODES_FIRSTAID_STATION; i < BG_AV_NODES_MAX; ++i)
    {
        if (m_Nodes[i].State == POINT_ASSAULTED)
        {
            if (m_Nodes[i].Timer > diff)
                m_Nodes[i].Timer -= diff;
            else
                EventPlayerDestroyedPoint(i);
        }
    }

    if (custom_honor_timer_ <= diff)
    {
        for (auto& p : custom_honor_)
        {
            // Custom bonus honor and mark every 30 minutes
            if (p.second + 30 * MINUTE < WorldTimer::time_no_syscall())
            {
                if (Player* plr = GetBgMap()->GetPlayer(p.first))
                {
                    RewardMark(plr, 1);
                    UpdatePlayerScore(plr, SCORE_BONUS_HONOR, 42);

                    p.second = WorldTimer::time_no_syscall();
                }
            }
        }

        custom_honor_timer_ = 15 * IN_MILLISECONDS;
    }
    else
        custom_honor_timer_ -= diff;
}

void BattleGroundAV::StartingEventCloseDoors()
{
    LOG_DEBUG(logging, "BattleGroundAV: entering state STATUS_WAIT_JOIN ...");
}

void BattleGroundAV::StartingEventOpenDoors()
{
    OpenDoorEvent(BG_EVENT_DOOR);
}

void BattleGroundAV::AddPlayer(Player* plr)
{
    BattleGround::AddPlayer(plr);
    // create score and add it to map, default values are set in constructor
    auto sc = new BattleGroundAVScore;
    m_PlayerScores[plr->GetObjectGuid()] = sc;

    custom_honor_[plr->GetObjectGuid()] = WorldTimer::time_no_syscall();
}

void BattleGroundAV::EndBattleGround(Team winner)
{
    // calculate bonuskills for both teams:
    uint32 tower_survived[BG_TEAMS_COUNT] = {0, 0};
    uint32 graves_owned[BG_TEAMS_COUNT] = {0, 0};
    uint32 mines_owned[BG_TEAMS_COUNT] = {0, 0};
    // towers all not destroyed:
    for (BG_AV_Nodes i = BG_AV_NODES_DUNBALDAR_SOUTH;
         i <= BG_AV_NODES_STONEHEART_BUNKER; ++i)
        if (m_Nodes[i].State == POINT_CONTROLLED)
            if (m_Nodes[i].TotalOwner == BG_AV_TEAM_ALLIANCE)
                ++tower_survived[BG_TEAM_ALLIANCE];
    for (BG_AV_Nodes i = BG_AV_NODES_ICEBLOOD_TOWER;
         i <= BG_AV_NODES_FROSTWOLF_WTOWER; ++i)
        if (m_Nodes[i].State == POINT_CONTROLLED)
            if (m_Nodes[i].TotalOwner == BG_AV_TEAM_HORDE)
                ++tower_survived[BG_TEAM_HORDE];

    // graves all controlled
    for (BG_AV_Nodes i = BG_AV_NODES_FIRSTAID_STATION; i < BG_AV_NODES_MAX; ++i)
        if (m_Nodes[i].State == POINT_CONTROLLED &&
            m_Nodes[i].Owner != BG_AV_TEAM_NEUTRAL)
            ++graves_owned[m_Nodes[i].Owner];

    for (auto& elem : m_Mine_Owner)
        if (elem != BG_AV_TEAM_NEUTRAL)
            ++mines_owned[elem];

    // now we have the values give the honor/reputation to the teams:
    Team team[BG_TEAMS_COUNT] = {ALLIANCE, HORDE};
    uint32 faction[BG_TEAMS_COUNT] = {BG_AV_FACTION_A, BG_AV_FACTION_H};
    for (uint32 i = 0; i < BG_TEAMS_COUNT; i++)
    {
        if (tower_survived[i])
        {
            RewardReputationToTeam(
                faction[i], tower_survived[i] * m_RepSurviveTower, team[i]);
            RewardHonorToTeam(GetBonusHonorFromKill(tower_survived[i] *
                                                    BG_AV_KILL_SURVIVING_TOWER),
                team[i]);
        }
        LOG_DEBUG(logging,
            "BattleGroundAV: EndbattleGround: bgteam: %u towers:%u honor:%u "
            "rep:%u",
            i, tower_survived[i],
            GetBonusHonorFromKill(
                      tower_survived[i] * BG_AV_KILL_SURVIVING_TOWER),
            tower_survived[i] * BG_AV_REP_SURVIVING_TOWER);
        if (graves_owned[i])
            RewardReputationToTeam(
                faction[i], graves_owned[i] * m_RepOwnedGrave, team[i]);
        if (mines_owned[i])
            RewardReputationToTeam(
                faction[i], mines_owned[i] * m_RepOwnedMine, team[i]);
        // captain survived?:
        if (!IsActiveEvent(
                BG_AV_NodeEventCaptainDead_A + GetTeamIndexByTeamId(team[i]),
                0))
        {
            RewardReputationToTeam(faction[i], m_RepSurviveCaptain, team[i]);
            RewardHonorToTeam(
                GetBonusHonorFromKill(BG_AV_KILL_SURVIVING_CAPTAIN), team[i]);
        }
    }

    // both teams:
    if (m_HonorMapComplete)
    {
        RewardHonorToTeam(m_HonorMapComplete, ALLIANCE);
        RewardHonorToTeam(m_HonorMapComplete, HORDE);
    }
    BattleGround::EndBattleGround(winner);
}

void BattleGroundAV::RemovePlayer(Player* /*plr*/, ObjectGuid guid)
{
    auto itr = custom_honor_.find(guid);
    if (itr != custom_honor_.end())
        custom_honor_.erase(itr);
}

void BattleGroundAV::HandleAreaTrigger(Player* Source, uint32 Trigger)
{
    // this is wrong way to implement these things. On official it done by
    // gameobject spell cast.
    switch (Trigger)
    {
    case 95:
    case 2608:
        if (Source->GetTeam() != ALLIANCE)
            Source->GetSession()->SendNotification(
                LANG_BATTLEGROUND_ONLY_ALLIANCE_USE);
        else
            Source->LeaveBattleground();
        break;
    case 2606:
        if (Source->GetTeam() != HORDE)
            Source->GetSession()->SendNotification(
                LANG_BATTLEGROUND_ONLY_HORDE_USE);
        else
            Source->LeaveBattleground();
        break;
    case 3326:
    case 3327:
    case 3328:
    case 3329:
    case 3330:
    case 3331:
        // Source->Unmount();
        break;
    default:
        LOG_DEBUG(logging,
            "BattleGroundAV: WARNING: Unhandled AreaTrigger in Battleground: "
            "%u",
            Trigger);
        //            Source->GetSession()->SendAreaTriggerMessage("Warning:
        //            Unhandled AreaTrigger in Battleground: %u", Trigger);
        break;
    }
}

void BattleGroundAV::UpdatePlayerScore(
    Player* Source, uint32 type, uint32 value)
{
    auto itr = m_PlayerScores.find(Source->GetObjectGuid());
    if (itr == m_PlayerScores.end()) // player not found...
        return;

    switch (type)
    {
    case SCORE_GRAVEYARDS_ASSAULTED:
        ((BattleGroundAVScore*)itr->second)->GraveyardsAssaulted += value;
        break;
    case SCORE_GRAVEYARDS_DEFENDED:
        ((BattleGroundAVScore*)itr->second)->GraveyardsDefended += value;
        break;
    case SCORE_TOWERS_ASSAULTED:
        ((BattleGroundAVScore*)itr->second)->TowersAssaulted += value;
        break;
    case SCORE_TOWERS_DEFENDED:
        ((BattleGroundAVScore*)itr->second)->TowersDefended += value;
        break;
    case SCORE_SECONDARY_OBJECTIVES:
        ((BattleGroundAVScore*)itr->second)->SecondaryObjectives += value;
        break;
    default:
        BattleGround::UpdatePlayerScore(Source, type, value);
        break;
    }
}

void BattleGroundAV::EventPlayerDestroyedPoint(BG_AV_Nodes node)
{
    LOG_DEBUG(logging, "BattleGroundAV: player destroyed point node %i", node);

    assert(m_Nodes[node].Owner != BG_AV_TEAM_NEUTRAL);
    BattleGroundTeamIndex ownerTeamIdx =
        BattleGroundTeamIndex(m_Nodes[node].Owner);
    Team ownerTeam = ownerTeamIdx == BG_TEAM_ALLIANCE ? ALLIANCE : HORDE;

    // despawn banner
    DestroyNode(node);
    PopulateNode(node);
    UpdateNodeWorldState(node);

    if (IsTower(node))
    {
        // UpdateScore(GetOtherTeamIndex(ownerTeamIdx), (-1) * BG_AV_RES_TOWER);
        RewardReputationToTeam(
            (ownerTeam == ALLIANCE) ? BG_AV_FACTION_A : BG_AV_FACTION_H,
            m_RepTowerDestruction, ownerTeam);
        RewardHonorToTeam(GetBonusHonorFromKill(BG_AV_KILL_TOWER), ownerTeam);
        SendYell2ToAll(LANG_BG_AV_TOWER_TAKEN, LANG_UNIVERSAL,
            GetSingleCreatureGuid(BG_AV_HERALD, 0), GetNodeName(node),
            (ownerTeam == ALLIANCE) ? LANG_BG_ALLY : LANG_BG_HORDE);

        // spawn/despawn marshals/war master
        switch (node)
        {
        case BG_AV_NODES_DUNBALDAR_SOUTH:
            SpawnBGCreature(av_guids::db_south_marshal, RESPAWN_ONE_WEEK);
            SpawnBGCreature(av_guids::db_south_warmaster, RESPAWN_IMMEDIATELY);
            break;
        case BG_AV_NODES_DUNBALDAR_NORTH:
            SpawnBGCreature(av_guids::db_north_marshal, RESPAWN_ONE_WEEK);
            SpawnBGCreature(av_guids::db_north_warmaster, RESPAWN_IMMEDIATELY);
            break;
        case BG_AV_NODES_ICEWING_BUNKER:
            SpawnBGCreature(av_guids::icewing_marshal, RESPAWN_ONE_WEEK);
            SpawnBGCreature(av_guids::icewing_warmaster, RESPAWN_IMMEDIATELY);
            break;
        case BG_AV_NODES_STONEHEART_BUNKER:
            SpawnBGCreature(av_guids::stonehearth_marshal, RESPAWN_ONE_WEEK);
            SpawnBGCreature(
                av_guids::stonehearth_warmaster, RESPAWN_IMMEDIATELY);
            break;
        case BG_AV_NODES_ICEBLOOD_TOWER:
            SpawnBGCreature(av_guids::iceblood_warmaster, RESPAWN_ONE_WEEK);
            SpawnBGCreature(av_guids::iceblood_marshal, RESPAWN_IMMEDIATELY);
            break;
        case BG_AV_NODES_TOWER_POINT:
            SpawnBGCreature(av_guids::tower_point_warmaster, RESPAWN_ONE_WEEK);
            SpawnBGCreature(av_guids::tower_point_marshal, RESPAWN_IMMEDIATELY);
            break;
        case BG_AV_NODES_FROSTWOLF_ETOWER:
            SpawnBGCreature(av_guids::east_frost_warmaster, RESPAWN_ONE_WEEK);
            SpawnBGCreature(av_guids::east_frost_marshal, RESPAWN_IMMEDIATELY);
            break;
        case BG_AV_NODES_FROSTWOLF_WTOWER:
            SpawnBGCreature(av_guids::west_frost_warmaster, RESPAWN_ONE_WEEK);
            SpawnBGCreature(av_guids::west_frost_marshal, RESPAWN_IMMEDIATELY);
            break;
        default:
            break;
        }
    }
    else
    {
        SendYell2ToAll(LANG_BG_AV_GRAVE_TAKEN, LANG_UNIVERSAL,
            GetSingleCreatureGuid(BG_AV_HERALD, 0), GetNodeName(node),
            (ownerTeam == ALLIANCE) ? LANG_BG_ALLY : LANG_BG_HORDE);
    }
}

void BattleGroundAV::ChangeMineOwner(
    uint8 mine, BattleGroundAVTeamIndex teamIdx)
{
    m_Mine_Timer[mine] = BG_AV_MINE_TICK_TIMER;

    // mine=0 northmine, mine=1 southmine

    assert(mine == BG_AV_NORTH_MINE || mine == BG_AV_SOUTH_MINE);
    if (m_Mine_Owner[mine] == teamIdx)
        return;

    if (teamIdx != BG_AV_TEAM_NEUTRAL)
        CompleteMineQuest(
            mine, teamIdx == BG_AV_TEAM_ALLIANCE ? ALLIANCE : HORDE);

    m_Mine_PrevOwner[mine] = m_Mine_Owner[mine];
    m_Mine_Owner[mine] = teamIdx;

    SendMineWorldStates(mine);

    SpawnEvent(BG_AV_MINE_EVENT + mine, teamIdx, true, false);
    SpawnEvent(BG_AV_MINE_BOSSES + mine, teamIdx, true);
    SpawnEvent(BG_AV_MINE_PATROLS + mine, teamIdx, true);

    if (teamIdx != BG_AV_TEAM_NEUTRAL)
    {
        PlaySoundToAll((teamIdx == BG_AV_TEAM_ALLIANCE) ?
                           BG_AV_SOUND_ALLIANCE_GOOD :
                           BG_AV_SOUND_HORDE_GOOD);
        m_Mine_Reclaim_Timer[mine] = BG_AV_MINE_RECLAIM_TIMER;
        SendYell2ToAll(LANG_BG_AV_MINE_TAKEN, LANG_UNIVERSAL,
            GetSingleCreatureGuid(BG_AV_HERALD, 0),
            (teamIdx == BG_AV_TEAM_ALLIANCE) ? LANG_BG_ALLY : LANG_BG_HORDE,
            (mine == BG_AV_NORTH_MINE) ? LANG_BG_AV_MINE_NORTH :
                                         LANG_BG_AV_MINE_SOUTH);
    }

    // Refresh dynflag of all supplies in the affected mine
    auto& supplies = m_EventObjects[MAKE_PAIR32(BG_AV_MINE_SUPPLIES + mine, 0)];
    for (auto& elem : supplies.gameobjects)
        if (GameObject* go = GetBgMap()->GetGameObject(elem))
            go->UpdateValueIndex(GAMEOBJECT_DYN_FLAGS);
}

void BattleGroundAV::CompleteMineQuest(uint8 mine, Team team)
{
    G3D::Vector3 pos;
    if (mine == BG_AV_NORTH_MINE)
        pos = G3D::Vector3(880, -440, 55);
    else
        pos = G3D::Vector3(-850, -95, 70);

    auto& pls = GetBgMap()->GetPlayers();
    for (auto& ref : pls)
    {
        Player* pl = ref.getSource();
        if (pl && pl->GetTeam() == team &&
            pl->GetDistance(pos.x, pos.y, pos.z) < 160.0f)
            pl->KilledMonsterCredit(13796);
    }
}

bool BattleGroundAV::PlayerCanDoMineQuest(int32 GOId, Team team)
{
    if (GOId == BG_AV_OBJECTID_MINE_N)
        return (m_Mine_Owner[BG_AV_NORTH_MINE] == GetAVTeamIndexByTeamId(team));
    if (GOId == BG_AV_OBJECTID_MINE_S)
        return (m_Mine_Owner[BG_AV_SOUTH_MINE] == GetAVTeamIndexByTeamId(team));
    return true; // cause it's no mine'object it is ok if this is true
}

/// will spawn and despawn creatures around a node
/// more a wrapper around spawnevent cause graveyards are special
void BattleGroundAV::PopulateNode(BG_AV_Nodes node)
{
    InstanceData* id = GetBgMap()->GetInstanceData();
    if (!id)
        return;

    BattleGroundAVTeamIndex teamIdx = m_Nodes[node].Owner;
    if (IsGrave(node) && teamIdx != BG_AV_TEAM_NEUTRAL)
    {
        uint32 upgrade = id->GetData(teamIdx == BG_AV_TEAM_ALLIANCE ?
                                         ALLIANCE_SOLDIERS_UPGRADE_LEVEL :
                                         HORDE_SOLDIERS_UPGRADE_LEVEL);
        uint32 graveDefenderType;
        if (upgrade == 0)
            graveDefenderType = 0;
        else if (upgrade == 1)
            graveDefenderType = 1;
        else if (upgrade == 2)
            graveDefenderType = 2;
        else
            graveDefenderType = 3;

        if (m_Nodes[node].State ==
            POINT_CONTROLLED) // we can spawn the current owner event
            SpawnEvent(BG_AV_NODES_MAX + node,
                teamIdx * BG_AV_MAX_GRAVETYPES + graveDefenderType, true);
        else // we despawn the event from the prevowner
            SpawnEvent(BG_AV_NODES_MAX + node,
                m_Nodes[node].PrevOwner * BG_AV_MAX_GRAVETYPES +
                    graveDefenderType,
                false);
    }
    SpawnEvent(node, (teamIdx * BG_AV_MAX_STATES) + m_Nodes[node].State, true);
}

/// called when using a banner
void BattleGroundAV::EventPlayerClickedOnFlag(
    Player* source, GameObject* target_obj)
{
    if (GetStatus() != STATUS_IN_PROGRESS)
        return;
    LOG_DEBUG(
        logging, "BattleGroundAV: using gameobject %i", target_obj->GetEntry());
    uint8 event = (sBattleGroundMgr::Instance()->GetGameObjectEventIndex(
                       target_obj->GetGUIDLow())).event1;
    if (event >= BG_AV_NODES_MAX) // not a node
        return;
    BG_AV_Nodes node = BG_AV_Nodes(event);
    switch ((sBattleGroundMgr::Instance()->GetGameObjectEventIndex(
                 target_obj->GetGUIDLow())).event2 %
            BG_AV_MAX_STATES)
    {
    case POINT_CONTROLLED:
        EventPlayerAssaultsPoint(source, node);
        break;
    case POINT_ASSAULTED:
        EventPlayerDefendsPoint(source, node);
        break;
    default:
        break;
    }
}

void BattleGroundAV::EventPlayerDefendsPoint(Player* player, BG_AV_Nodes node)
{
    assert(GetStatus() == STATUS_IN_PROGRESS);

    BattleGroundTeamIndex teamIdx = GetTeamIndexByTeamId(player->GetTeam());

    if (m_Nodes[node].Owner == BattleGroundAVTeamIndex(teamIdx) ||
        m_Nodes[node].State != POINT_ASSAULTED)
        return;
    if (m_Nodes[node].TotalOwner ==
        BG_AV_TEAM_NEUTRAL) // initial snowfall capture
    {
        // until snowfall doesn't belong to anyone it is better handled in
        // assault - code (best would be to have a special function
        // for neutral nodes.. but doing this just for snowfall will be a bit to
        // much i think
        assert(node ==
               BG_AV_NODES_SNOWFALL_GRAVE); // currently the only neutral grave
        EventPlayerAssaultsPoint(player, node);
        return;
    }

    LOG_DEBUG(logging, "BattleGroundAV: player defends node: %i", node);
    if (m_Nodes[node].PrevOwner != BattleGroundAVTeamIndex(teamIdx))
    {
        logging.error(
            "BattleGroundAV: player defends point which doesn't belong to his "
            "team %i",
            node);
        return;
    }

    DefendNode(node, teamIdx);  // set the right variables for nodeinfo
    PopulateNode(node);         // spawn node-creatures (defender for example)
    UpdateNodeWorldState(node); // send new mapicon to the player

    if (IsTower(node))
    {
        SendYell2ToAll(LANG_BG_AV_TOWER_DEFENDED, LANG_UNIVERSAL,
            GetSingleCreatureGuid(BG_AV_HERALD, 0), GetNodeName(node),
            (teamIdx == BG_TEAM_ALLIANCE) ? LANG_BG_ALLY : LANG_BG_HORDE);
        UpdatePlayerScore(player, SCORE_TOWERS_DEFENDED, 1);
        PlaySoundToAll(BG_AV_SOUND_BOTH_TOWER_DEFEND);
    }
    else
    {
        SendYell2ToAll(LANG_BG_AV_GRAVE_DEFENDED, LANG_UNIVERSAL,
            GetSingleCreatureGuid(BG_AV_HERALD, 0), GetNodeName(node),
            (teamIdx == BG_TEAM_ALLIANCE) ? LANG_BG_ALLY : LANG_BG_HORDE);
        UpdatePlayerScore(player, SCORE_GRAVEYARDS_DEFENDED, 1);
        // update the statistic for the defending player
        PlaySoundToAll((teamIdx == BG_TEAM_ALLIANCE) ?
                           BG_AV_SOUND_ALLIANCE_GOOD :
                           BG_AV_SOUND_HORDE_GOOD);
    }
}

void BattleGroundAV::EventPlayerAssaultsPoint(Player* player, BG_AV_Nodes node)
{
    BattleGroundTeamIndex teamIdx = GetTeamIndexByTeamId(player->GetTeam());
    LOG_DEBUG(logging, "BattleGroundAV: player assaults node %i", node);
    if (m_Nodes[node].Owner == BattleGroundAVTeamIndex(teamIdx) ||
        BattleGroundAVTeamIndex(teamIdx) == m_Nodes[node].TotalOwner)
        return;

    // Handle quests
    if (m_Nodes[node].Owner != BG_AV_TEAM_NEUTRAL)
    {
        if (IsTower(node))
        {
            // "Towers and Bunkers"
            player->KilledMonsterCredit(13778);
        }
        else
        {
            // "Alterac Valley Graveyards" & "The Graveyards of Alterac"
            player->KilledMonsterCredit(13756);
        }
    }

    AssaultNode(node, teamIdx); // update nodeinfo variables
    UpdateNodeWorldState(node); // send mapicon
    PopulateNode(node);

    if (IsTower(node))
    {
        SendYell2ToAll(LANG_BG_AV_TOWER_ASSAULTED, LANG_UNIVERSAL,
            GetSingleCreatureGuid(BG_AV_HERALD, 0), GetNodeName(node),
            (teamIdx == BG_TEAM_ALLIANCE) ? LANG_BG_ALLY : LANG_BG_HORDE);
        UpdatePlayerScore(player, SCORE_TOWERS_ASSAULTED, 1);
    }
    else
    {
        SendYell2ToAll(LANG_BG_AV_GRAVE_ASSAULTED, LANG_UNIVERSAL,
            GetSingleCreatureGuid(BG_AV_HERALD, 0), GetNodeName(node),
            (teamIdx == BG_TEAM_ALLIANCE) ? LANG_BG_ALLY : LANG_BG_HORDE);
        // update the statistic for the assaulting player
        UpdatePlayerScore(player, SCORE_GRAVEYARDS_ASSAULTED, 1);
    }

    PlaySoundToAll((teamIdx == BG_TEAM_ALLIANCE) ?
                       BG_AV_SOUND_ALLIANCE_ASSAULTS :
                       BG_AV_SOUND_HORDE_ASSAULTS);
}

void BattleGroundAV::FillInitialWorldStates(WorldPacket& data, uint32& count)
{
    bool stateok;
    for (uint32 i = BG_AV_NODES_FIRSTAID_STATION; i < BG_AV_NODES_MAX; ++i)
    {
        for (uint8 j = 0; j < BG_AV_MAX_STATES; j++)
        {
            stateok = (m_Nodes[i].State == j);
            FillInitialWorldState(data, count,
                BG_AV_NodeWorldStates[i][GetWorldStateType(
                    j, BG_AV_TEAM_ALLIANCE)],
                m_Nodes[i].Owner == BG_AV_TEAM_ALLIANCE && stateok);
            FillInitialWorldState(data, count,
                BG_AV_NodeWorldStates[i][GetWorldStateType(
                    j, BG_AV_TEAM_HORDE)],
                m_Nodes[i].Owner == BG_AV_TEAM_HORDE && stateok);
        }
    }

    if (m_Nodes[BG_AV_NODES_SNOWFALL_GRAVE].Owner ==
        BG_AV_TEAM_NEUTRAL) // cause neutral teams aren't handled generic
        FillInitialWorldState(data, count, AV_SNOWFALL_N, 1);

    /*FillInitialWorldState(data, count, BG_AV_Alliance_Score,
    m_TeamScores[BG_TEAM_ALLIANCE]);
    FillInitialWorldState(data, count, BG_AV_Horde_Score,
    m_TeamScores[BG_TEAM_HORDE]);
    if( GetStatus() == STATUS_IN_PROGRESS )                 // only if game is
    running the teamscores are displayed
    {
        FillInitialWorldState(data, count, BG_AV_SHOW_A_SCORE, 1);
        FillInitialWorldState(data, count, BG_AV_SHOW_H_SCORE, 1);
    }
    else
    {
        FillInitialWorldState(data, count, BG_AV_SHOW_A_SCORE, 0);
        FillInitialWorldState(data, count, BG_AV_SHOW_H_SCORE, 0);
    }*/
    FillInitialWorldState(data, count, BG_AV_Alliance_Score, 0);
    FillInitialWorldState(data, count, BG_AV_Horde_Score, 0);
    FillInitialWorldState(data, count, BG_AV_SHOW_A_SCORE, 0);
    FillInitialWorldState(data, count, BG_AV_SHOW_H_SCORE, 0);

    FillInitialWorldState(data, count,
        BG_AV_MineWorldStates[BG_AV_NORTH_MINE][m_Mine_Owner[BG_AV_NORTH_MINE]],
        1);
    if (m_Mine_Owner[BG_AV_NORTH_MINE] != m_Mine_PrevOwner[BG_AV_NORTH_MINE])
        FillInitialWorldState(data, count,
            BG_AV_MineWorldStates[BG_AV_NORTH_MINE]
                                 [m_Mine_PrevOwner[BG_AV_NORTH_MINE]],
            0);

    FillInitialWorldState(data, count,
        BG_AV_MineWorldStates[BG_AV_SOUTH_MINE][m_Mine_Owner[BG_AV_SOUTH_MINE]],
        1);
    if (m_Mine_Owner[BG_AV_SOUTH_MINE] != m_Mine_PrevOwner[BG_AV_SOUTH_MINE])
        FillInitialWorldState(data, count,
            BG_AV_MineWorldStates[BG_AV_SOUTH_MINE]
                                 [m_Mine_PrevOwner[BG_AV_SOUTH_MINE]],
            0);
}

void BattleGroundAV::UpdateNodeWorldState(BG_AV_Nodes node)
{
    UpdateWorldState(BG_AV_NodeWorldStates[node][GetWorldStateType(
                         m_Nodes[node].State, m_Nodes[node].Owner)],
        1);
    if (m_Nodes[node].PrevOwner == BG_AV_TEAM_NEUTRAL) // currently only
                                                       // snowfall is supported
                                                       // as neutral node
        UpdateWorldState(AV_SNOWFALL_N, 0);
    else
        UpdateWorldState(BG_AV_NodeWorldStates[node][GetWorldStateType(
                             m_Nodes[node].PrevState, m_Nodes[node].PrevOwner)],
            0);
}

void BattleGroundAV::SendMineWorldStates(uint32 mine)
{
    assert(mine == BG_AV_NORTH_MINE || mine == BG_AV_SOUTH_MINE);

    UpdateWorldState(BG_AV_MineWorldStates[mine][m_Mine_Owner[mine]], 1);
    if (m_Mine_Owner[mine] != m_Mine_PrevOwner[mine])
        UpdateWorldState(
            BG_AV_MineWorldStates[mine][m_Mine_PrevOwner[mine]], 0);
}

WorldSafeLocsEntry const* BattleGroundAV::GetClosestGraveYard(Player* plr)
{
    float x = plr->GetX();
    float y = plr->GetY();
    BattleGroundAVTeamIndex teamIdx = GetAVTeamIndexByTeamId(plr->GetTeam());
    WorldSafeLocsEntry const* good_entry = nullptr;
    if (GetStatus() == STATUS_IN_PROGRESS)
    {
        // Is there any occupied node for this team?
        float mindist = 9999999.0f;
        for (uint8 i = BG_AV_NODES_FIRSTAID_STATION;
             i <= BG_AV_NODES_FROSTWOLF_HUT; ++i)
        {
            if (m_Nodes[i].Owner != teamIdx ||
                m_Nodes[i].State != POINT_CONTROLLED)
                continue;
            WorldSafeLocsEntry const* entry =
                sWorldSafeLocsStore.LookupEntry(BG_AV_GraveyardIds[i]);
            if (!entry)
                continue;
            float dist = (entry->x - x) * (entry->x - x) +
                         (entry->y - y) * (entry->y - y);
            if (mindist > dist)
            {
                mindist = dist;
                good_entry = entry;
            }
        }
    }
    // If not, place ghost in the starting-cave
    if (!good_entry)
        good_entry =
            sWorldSafeLocsStore.LookupEntry(BG_AV_GraveyardIds[teamIdx + 7]);

    return good_entry;
}

uint32 BattleGroundAV::GetNodeName(BG_AV_Nodes node)
{
    switch (node)
    {
    case BG_AV_NODES_FIRSTAID_STATION:
        return LANG_BG_AV_NODE_GRAVE_STORM_AID;
    case BG_AV_NODES_DUNBALDAR_SOUTH:
        return LANG_BG_AV_NODE_TOWER_DUN_S;
    case BG_AV_NODES_DUNBALDAR_NORTH:
        return LANG_BG_AV_NODE_TOWER_DUN_N;
    case BG_AV_NODES_STORMPIKE_GRAVE:
        return LANG_BG_AV_NODE_GRAVE_STORMPIKE;
    case BG_AV_NODES_ICEWING_BUNKER:
        return LANG_BG_AV_NODE_TOWER_ICEWING;
    case BG_AV_NODES_STONEHEART_GRAVE:
        return LANG_BG_AV_NODE_GRAVE_STONE;
    case BG_AV_NODES_STONEHEART_BUNKER:
        return LANG_BG_AV_NODE_TOWER_STONE;
    case BG_AV_NODES_SNOWFALL_GRAVE:
        return LANG_BG_AV_NODE_GRAVE_SNOW;
    case BG_AV_NODES_ICEBLOOD_TOWER:
        return LANG_BG_AV_NODE_TOWER_ICE;
    case BG_AV_NODES_ICEBLOOD_GRAVE:
        return LANG_BG_AV_NODE_GRAVE_ICE;
    case BG_AV_NODES_TOWER_POINT:
        return LANG_BG_AV_NODE_TOWER_POINT;
    case BG_AV_NODES_FROSTWOLF_GRAVE:
        return LANG_BG_AV_NODE_GRAVE_FROST;
    case BG_AV_NODES_FROSTWOLF_ETOWER:
        return LANG_BG_AV_NODE_TOWER_FROST_E;
    case BG_AV_NODES_FROSTWOLF_WTOWER:
        return LANG_BG_AV_NODE_TOWER_FROST_W;
    case BG_AV_NODES_FROSTWOLF_HUT:
        return LANG_BG_AV_NODE_GRAVE_FROST_HUT;
    default:
        return 0;
        break;
    }
}

void BattleGroundAV::AssaultNode(
    BG_AV_Nodes node, BattleGroundTeamIndex teamIdx)
{
    assert(m_Nodes[node].TotalOwner != BattleGroundAVTeamIndex(teamIdx));
    assert(m_Nodes[node].Owner != BattleGroundAVTeamIndex(teamIdx));
    // only assault an assaulted node if no totalowner exists:
    assert(m_Nodes[node].State != POINT_ASSAULTED ||
           m_Nodes[node].TotalOwner == BG_AV_TEAM_NEUTRAL);
    // the timer gets another time, if the previous owner was 0 == Neutral
    m_Nodes[node].Timer = (m_Nodes[node].PrevOwner != BG_AV_TEAM_NEUTRAL) ?
                              BG_AV_CAPTIME :
                              BG_AV_SNOWFALL_FIRSTCAP;
    m_Nodes[node].PrevOwner = m_Nodes[node].Owner;
    m_Nodes[node].Owner = BattleGroundAVTeamIndex(teamIdx);
    m_Nodes[node].PrevState = m_Nodes[node].State;
    m_Nodes[node].State = POINT_ASSAULTED;
}

void BattleGroundAV::DestroyNode(BG_AV_Nodes node)
{
    assert(m_Nodes[node].State == POINT_ASSAULTED);

    m_Nodes[node].TotalOwner = m_Nodes[node].Owner;
    m_Nodes[node].PrevOwner = m_Nodes[node].Owner;
    m_Nodes[node].PrevState = m_Nodes[node].State;
    m_Nodes[node].State = POINT_CONTROLLED;
    m_Nodes[node].Timer = 0;
}

void BattleGroundAV::InitNode(
    BG_AV_Nodes node, BattleGroundAVTeamIndex teamIdx, bool tower)
{
    m_Nodes[node].TotalOwner = teamIdx;
    m_Nodes[node].Owner = teamIdx;
    m_Nodes[node].PrevOwner = teamIdx;
    m_Nodes[node].State = POINT_CONTROLLED;
    m_Nodes[node].PrevState = m_Nodes[node].State;
    m_Nodes[node].State = POINT_CONTROLLED;
    m_Nodes[node].Timer = 0;
    m_Nodes[node].Tower = tower;
    m_ActiveEvents[node] = teamIdx * BG_AV_MAX_STATES + m_Nodes[node].State;
    if (IsGrave(node)) // grave-creatures are special cause of a quest
        m_ActiveEvents[node + BG_AV_NODES_MAX] = teamIdx * BG_AV_MAX_GRAVETYPES;
}

void BattleGroundAV::DefendNode(BG_AV_Nodes node, BattleGroundTeamIndex teamIdx)
{
    assert(m_Nodes[node].TotalOwner == BattleGroundAVTeamIndex(teamIdx));
    assert(m_Nodes[node].Owner != BattleGroundAVTeamIndex(teamIdx));
    assert(m_Nodes[node].State != POINT_CONTROLLED);
    m_Nodes[node].PrevOwner = m_Nodes[node].Owner;
    m_Nodes[node].Owner = BattleGroundAVTeamIndex(teamIdx);
    m_Nodes[node].PrevState = m_Nodes[node].State;
    m_Nodes[node].State = POINT_CONTROLLED;
    m_Nodes[node].Timer = 0;
}

void BattleGroundAV::DoArmorScrapsUpgrade(BattleGroundAVTeamIndex team)
{
    InstanceData* id = GetBgMap()->GetInstanceData();
    if (!id)
        return;

    uint32 scraps =
        id->GetData(team == BG_AV_TEAM_ALLIANCE ? ALLIANCE_ARMOR_SCRAPS :
                                                  HORDE_ARMOR_SCRAPS);
    uint32 upgrades = id->GetData(team == BG_AV_TEAM_ALLIANCE ?
                                      ALLIANCE_SOLDIERS_UPGRADE_LEVEL :
                                      HORDE_SOLDIERS_UPGRADE_LEVEL);

    if (scraps < 500 * (upgrades + 1))
        return;

    id->SetData(team == BG_AV_TEAM_ALLIANCE ? ALLIANCE_SOLDIERS_UPGRADE_LEVEL :
                                              HORDE_SOLDIERS_UPGRADE_LEVEL,
        upgrades + 1);

    for (BG_AV_Nodes i = BG_AV_NODES_FIRSTAID_STATION;
         i <= BG_AV_NODES_FROSTWOLF_HUT; ++i)
        if (m_Nodes[i].Owner == team && m_Nodes[i].State == POINT_CONTROLLED)
            PopulateNode(i);
}

bool BattleGroundAV::SpawnedByDefault(Creature* c)
{
    auto guid = c->GetObjectGuid();

    if (guid == av_guids::db_north_warmaster)
        return false;
    if (guid == av_guids::db_south_warmaster)
        return false;
    if (guid == av_guids::icewing_warmaster)
        return false;
    if (guid == av_guids::stonehearth_warmaster)
        return false;

    if (guid == av_guids::east_frost_marshal)
        return false;
    if (guid == av_guids::west_frost_marshal)
        return false;
    if (guid == av_guids::tower_point_marshal)
        return false;
    if (guid == av_guids::iceblood_marshal)
        return false;

    return true;
}
