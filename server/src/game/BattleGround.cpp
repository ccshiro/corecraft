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

#include "BattleGround.h"
#include "ArenaTeam.h"
#include "BattleGroundMgr.h"
#include "Creature.h"
#include "Formulas.h"
#include "Group.h"
#include "Language.h"
#include "Mail.h"
#include "MapManager.h"
#include "Object.h"
#include "ObjectGuid.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "SpecialVisCreature.h"
#include "Totem.h"
#include "TemporarySummon.h"
#include "SpellAuras.h"
#include "Util.h"
#include "WorldPacket.h"
#include "maps/callbacks.h"
#include "maps/visitors.h"

namespace MaNGOS
{
class BattleGroundChatBuilder
{
public:
    BattleGroundChatBuilder(ChatMsg msgtype, int32 textId, Player const* source,
        va_list* args = nullptr)
      : i_msgtype(msgtype), i_textId(textId), i_source(source), i_args(args)
    {
    }
    void operator()(WorldPacket& data, int32 loc_idx)
    {
        char const* text =
            sObjectMgr::Instance()->GetMangosString(i_textId, loc_idx);

        if (i_args)
        {
            // we need copy va_list before use or original va_list will
            // corrupted
            va_list ap;
            va_copy(ap, *i_args);

            char str[2048];
            vsnprintf(str, 2048, text, ap);
            va_end(ap);

            do_helper(data, &str[0]);
        }
        else
            do_helper(data, text);
    }

private:
    void do_helper(WorldPacket& data, char const* text)
    {
        ObjectGuid targetGuid =
            i_source ? i_source->GetObjectGuid() : ObjectGuid();

        data << uint8(i_msgtype);
        data << uint32(LANG_UNIVERSAL);
        data << ObjectGuid(targetGuid); // there 0 for BG messages
        data << uint32(0);              // can be chat msg group or something
        data << ObjectGuid(targetGuid);
        data << uint32(strlen(text) + 1);
        data << text;
        data << uint8(i_source ? i_source->chatTag() : uint8(0));
    }

    ChatMsg i_msgtype;
    int32 i_textId;
    Player const* i_source;
    va_list* i_args;
};

class BattleGroundYellBuilder
{
public:
    BattleGroundYellBuilder(uint32 language, int32 textId,
        Creature const* source, va_list* args = nullptr)
      : i_language(language), i_textId(textId), i_source(source), i_args(args)
    {
    }
    void operator()(WorldPacket& data, int32 loc_idx)
    {
        char const* text =
            sObjectMgr::Instance()->GetMangosString(i_textId, loc_idx);

        if (i_args)
        {
            // we need copy va_list before use or original va_list will
            // corrupted
            va_list ap;
            va_copy(ap, *i_args);

            char str[2048];
            vsnprintf(str, 2048, text, ap);
            va_end(ap);

            do_helper(data, &str[0]);
        }
        else
            do_helper(data, text);
    }

private:
    void do_helper(WorldPacket& data, char const* text)
    {
        // copyied from BuildMonsterChat
        data << uint8(CHAT_MSG_MONSTER_YELL);
        data << uint32(i_language);
        data << ObjectGuid(i_source->GetObjectGuid());
        data << uint32(0); // 2.1.0
        data << uint32(strlen(i_source->GetName()) + 1);
        data << i_source->GetName();
        data << ObjectGuid(); // Unit Target - isn't important for bgs
        data << uint32(strlen(text) + 1);
        data << text;
        data << uint8(0); // ChatTag - for bgs allways 0?
    }

    uint32 i_language;
    int32 i_textId;
    Creature const* i_source;
    va_list* i_args;
};

class BattleGround2ChatBuilder
{
public:
    BattleGround2ChatBuilder(ChatMsg msgtype, int32 textId,
        Player const* source, int32 arg1, int32 arg2)
      : i_msgtype(msgtype), i_textId(textId), i_source(source), i_arg1(arg1),
        i_arg2(arg2)
    {
    }
    void operator()(WorldPacket& data, int32 loc_idx)
    {
        char const* text =
            sObjectMgr::Instance()->GetMangosString(i_textId, loc_idx);
        char const* arg1str =
            i_arg1 ? sObjectMgr::Instance()->GetMangosString(i_arg1, loc_idx) :
                     "";
        char const* arg2str =
            i_arg2 ? sObjectMgr::Instance()->GetMangosString(i_arg2, loc_idx) :
                     "";

        char str[2048];
        snprintf(str, 2048, text, arg1str, arg2str);

        ObjectGuid targetGuid =
            i_source ? i_source->GetObjectGuid() : ObjectGuid();

        data << uint8(i_msgtype);
        data << uint32(LANG_UNIVERSAL);
        data << ObjectGuid(targetGuid); // there 0 for BG messages
        data << uint32(0);              // can be chat msg group or something
        data << ObjectGuid(targetGuid);
        data << uint32(strlen(str) + 1);
        data << str;
        data << uint8(i_source ? i_source->chatTag() : uint8(0));
    }

private:
    ChatMsg i_msgtype;
    int32 i_textId;
    Player const* i_source;
    int32 i_arg1;
    int32 i_arg2;
};

class BattleGround2YellBuilder
{
public:
    BattleGround2YellBuilder(uint32 language, int32 textId,
        Creature const* source, int32 arg1, int32 arg2)
      : i_language(language), i_textId(textId), i_source(source), i_arg1(arg1),
        i_arg2(arg2)
    {
    }
    void operator()(WorldPacket& data, int32 loc_idx)
    {
        char const* text =
            sObjectMgr::Instance()->GetMangosString(i_textId, loc_idx);
        char const* arg1str =
            i_arg1 ? sObjectMgr::Instance()->GetMangosString(i_arg1, loc_idx) :
                     "";
        char const* arg2str =
            i_arg2 ? sObjectMgr::Instance()->GetMangosString(i_arg2, loc_idx) :
                     "";

        char str[2048];
        snprintf(str, 2048, text, arg1str, arg2str);
        // copyied from BuildMonsterChat
        data << uint8(CHAT_MSG_MONSTER_YELL);
        data << uint32(i_language);
        data << ObjectGuid(i_source->GetObjectGuid());
        data << uint32(0); // 2.1.0
        data << uint32(strlen(i_source->GetName()) + 1);
        data << i_source->GetName();
        data << ObjectGuid(); // Unit Target - isn't important for bgs
        data << uint32(strlen(str) + 1);
        data << str;
        data << uint8(0); // ChatTag - for bgs allways 0?
    }

private:
    uint32 i_language;
    int32 i_textId;
    Creature const* i_source;
    int32 i_arg1;
    int32 i_arg2;
};
} // namespace MaNGOS

template <class Do>
void BattleGround::BroadcastWorker(Do& _do)
{
    for (BattleGroundPlayerMap::const_iterator itr = m_Players.begin();
         itr != m_Players.end(); ++itr)
        if (Player* plr = ObjectAccessor::FindPlayer(itr->first))
            _do(plr);
}

BattleGround::BattleGround()
{
    m_TypeID = BattleGroundTypeId(0);
    m_Status = STATUS_WAIT_JOIN;
    m_ClientInstanceID = 0;
    m_EndTime = 0;
    m_InvitedAlliance = 0;
    m_InvitedHorde = 0;
    m_ArenaType = ARENA_TYPE_NONE;
    m_IsArena = false;
    m_Winner = 2;
    m_StartTime = 0;
    m_TimeElapsed = 0;
    m_Events = 0;
    m_IsRated = false;
    m_BuffChange = false;
    m_Name = "";
    m_LevelMin = 0;
    m_LevelMax = 0;
    m_InBGFreeSlotQueue = false;
    m_StartDelayTime = 0;

    m_MaxPlayersPerTeam = 0;
    m_MaxPlayers = 0;
    m_MinPlayersPerTeam = 0;
    m_MinPlayers = 0;

    m_MapId = 0;
    m_Map = nullptr;

    m_TeamStartLocX[BG_TEAM_ALLIANCE] = 0;
    m_TeamStartLocX[BG_TEAM_HORDE] = 0;

    m_TeamStartLocY[BG_TEAM_ALLIANCE] = 0;
    m_TeamStartLocY[BG_TEAM_HORDE] = 0;

    m_TeamStartLocZ[BG_TEAM_ALLIANCE] = 0;
    m_TeamStartLocZ[BG_TEAM_HORDE] = 0;

    m_TeamStartLocO[BG_TEAM_ALLIANCE] = 0;
    m_TeamStartLocO[BG_TEAM_HORDE] = 0;

    m_ArenaTeamIds[BG_TEAM_ALLIANCE] = 0;
    m_ArenaTeamIds[BG_TEAM_HORDE] = 0;

    m_ArenaTeamRatingChanges[BG_TEAM_ALLIANCE] = 0;
    m_ArenaTeamRatingChanges[BG_TEAM_HORDE] = 0;

    m_BgRaids[BG_TEAM_ALLIANCE] = nullptr;
    m_BgRaids[BG_TEAM_HORDE] = nullptr;

    m_PlayersCount[BG_TEAM_ALLIANCE] = 0;
    m_PlayersCount[BG_TEAM_HORDE] = 0;

    m_TeamScores[BG_TEAM_ALLIANCE] = 0;
    m_TeamScores[BG_TEAM_HORDE] = 0;

    m_PrematureCountDown = false;
    m_PrematureCountDownTimer = 0;

    if (sBattleGroundMgr::Instance()->debugging())
    {
        m_StartDelayTimes[BG_STARTING_EVENT_FIRST] =
            static_cast<BattleGroundStartTimeIntervals>(3000);
        m_StartDelayTimes[BG_STARTING_EVENT_SECOND] =
            static_cast<BattleGroundStartTimeIntervals>(2000);
        m_StartDelayTimes[BG_STARTING_EVENT_THIRD] =
            static_cast<BattleGroundStartTimeIntervals>(1000);
        m_StartDelayTimes[BG_STARTING_EVENT_FOURTH] = BG_START_DELAY_NONE;
    }
    else
    {
        m_StartDelayTimes[BG_STARTING_EVENT_FIRST] = BG_START_DELAY_2M;
        m_StartDelayTimes[BG_STARTING_EVENT_SECOND] = BG_START_DELAY_1M;
        m_StartDelayTimes[BG_STARTING_EVENT_THIRD] = BG_START_DELAY_30S;
        m_StartDelayTimes[BG_STARTING_EVENT_FOURTH] = BG_START_DELAY_NONE;
    }

    // we must set to some default existing values
    m_StartMessageIds[BG_STARTING_EVENT_FIRST] = 0;
    m_StartMessageIds[BG_STARTING_EVENT_SECOND] = LANG_BG_WS_START_ONE_MINUTE;
    m_StartMessageIds[BG_STARTING_EVENT_THIRD] = LANG_BG_WS_START_HALF_MINUTE;
    m_StartMessageIds[BG_STARTING_EVENT_FOURTH] = LANG_BG_WS_HAS_BEGUN;

    // Stuff that was in BattleGround::Reset() previously:
    SetWinner(WINNER_NONE);
    // SetStatus(STATUS_WAIT_QUEUE); -- Put to wait_join above
    SetStartTime(0);
    SetEndTime(0);
    SetArenaType(ARENA_TYPE_NONE);
    SetRated(false);

    m_Events = 0;

    // door-event2 is always 0
    m_ActiveEvents[BG_EVENT_DOOR] = 0;
    m_ActiveEvents[ARENA_BUFF_EVENT] = BG_EVENT_NONE;
    m_ArenaBuffSpawned = false;

    if (m_InvitedAlliance > 0 || m_InvitedHorde > 0)
        logging.error(
            "BattleGround system: bad counter, m_InvitedAlliance: %d, "
            "m_InvitedHorde: %d",
            m_InvitedAlliance, m_InvitedHorde);

    m_InvitedAlliance = 0;
    m_InvitedHorde = 0;
    m_InBGFreeSlotQueue = false;

    m_Players.clear();

    for (BattleGroundScoreMap::const_iterator itr = m_PlayerScores.begin();
         itr != m_PlayerScores.end(); ++itr)
        delete itr->second;
    m_PlayerScores.clear();
    // End of stuff that was in BattleGround::Reset() previously
}

BattleGround::~BattleGround()
{
    // remove objects and creatures
    // (this is done automatically in mapmanager update, when the instance is
    // reset after the reset time)
    int size = m_BgObjects.size();
    for (int i = 0; i < size; ++i)
        DelObject(i);

    for (BattleGroundScoreMap::const_iterator itr = m_PlayerScores.begin();
         itr != m_PlayerScores.end(); ++itr)
        delete itr->second;
}

void BattleGround::Update(uint32 diff)
{
    // remove offline players from bg after 5 minutes
    if (!m_OfflineQueue.empty())
    {
        auto itr = m_Players.find(*(m_OfflineQueue.begin()));
        if (itr != m_Players.end())
        {
            if (itr->second.OfflineRemoveTime <= WorldTimer::time_no_syscall())
            {
                RemovePlayerAtLeave(itr->first, true); // remove player from BG
                m_OfflineQueue.pop_front(); // remove from offline queue
                // do not use itr for anything, because it is erased in
                // RemovePlayerAtLeave()
            }
        }
    }

    /*********************************************************/
    /***           BATTLEGROUND BALLANCE SYSTEM            ***/
    /*********************************************************/

    // if less then minimum players are in on one side, then start premature
    // finish timer
    if (GetStatus() == STATUS_IN_PROGRESS && !isArena() &&
        sBattleGroundMgr::Instance()->GetPrematureFinishTime() &&
        (GetPlayersCountByTeam(ALLIANCE) < GetMinPlayersPerTeam() ||
            GetPlayersCountByTeam(HORDE) < GetMinPlayersPerTeam()))
    {
        if (!m_PrematureCountDown)
        {
            m_PrematureCountDown = true;
            m_PrematureCountDownTimer =
                sBattleGroundMgr::Instance()->GetPrematureFinishTime();
        }
        else if (m_PrematureCountDownTimer < diff)
        {
            // time's up!
            Team winner = TEAM_NONE;
            if (GetPlayersCountByTeam(ALLIANCE) >= GetMinPlayersPerTeam())
                winner = ALLIANCE;
            else if (GetPlayersCountByTeam(HORDE) >= GetMinPlayersPerTeam())
                winner = HORDE;

            EndBattleGround(winner);
            m_PrematureCountDown = false;
        }
        else if (!sBattleGroundMgr::Instance()->debugging())
        {
            uint32 newtime = m_PrematureCountDownTimer - diff;
            // announce every minute
            if (newtime > (MINUTE * IN_MILLISECONDS))
            {
                if (newtime / (MINUTE * IN_MILLISECONDS) !=
                    m_PrematureCountDownTimer / (MINUTE * IN_MILLISECONDS))
                    PSendMessageToAll(
                        LANG_BATTLEGROUND_PREMATURE_FINISH_WARNING,
                        CHAT_MSG_SYSTEM, nullptr,
                        (uint32)(m_PrematureCountDownTimer /
                                 (MINUTE * IN_MILLISECONDS)));
            }
            else
            {
                // announce every 15 seconds
                if (newtime / (15 * IN_MILLISECONDS) !=
                    m_PrematureCountDownTimer / (15 * IN_MILLISECONDS))
                    PSendMessageToAll(
                        LANG_BATTLEGROUND_PREMATURE_FINISH_WARNING_SECS,
                        CHAT_MSG_SYSTEM, nullptr,
                        (uint32)(m_PrematureCountDownTimer / IN_MILLISECONDS));
            }
            m_PrematureCountDownTimer = newtime;
        }
    }
    else if (m_PrematureCountDown)
        m_PrematureCountDown = false;

    /*********************************************************/
    /***           ARENA BUFF OBJECT SPAWNING              ***/
    /*********************************************************/
    if (isArena() && !m_ArenaBuffSpawned)
    {
        // 90 seconds after start the buffobjects in arena should get spawned
        if (m_StartTime > uint32(m_StartDelayTimes[BG_STARTING_EVENT_FIRST] +
                                 ARENA_SPAWN_BUFF_OBJECTS * 1000))
        {
            SpawnEvent(ARENA_BUFF_EVENT, 0, true);
            m_ArenaBuffSpawned = true;
        }
    }

    /*********************************************************/
    /***           BATTLEGROUND STARTING SYSTEM            ***/
    /*********************************************************/
    if (GetStatus() == STATUS_WAIT_JOIN && GetPlayersSize())
    {
        ModifyStartDelayTime(diff);
        if (!(m_Events & BG_STARTING_EVENT_1))
        {
            m_Events |= BG_STARTING_EVENT_1;

            // setup here, only when at least one player has ported to the map
            if (!SetupBattleGround())
            {
                EndNow();
                return;
            }

            StartingEventCloseDoors();
            SetStartDelayTime(m_StartDelayTimes[BG_STARTING_EVENT_FIRST]);
            // first start warning - 2 or 1 minute, only if defined
            if (m_StartMessageIds[BG_STARTING_EVENT_FIRST])
                SendMessageToAll(m_StartMessageIds[BG_STARTING_EVENT_FIRST],
                    CHAT_MSG_BG_SYSTEM_NEUTRAL);
        }
        // After 1 minute or 30 seconds, warning is signalled
        else if (GetStartDelayTime() <=
                     m_StartDelayTimes[BG_STARTING_EVENT_SECOND] &&
                 !(m_Events & BG_STARTING_EVENT_2))
        {
            m_Events |= BG_STARTING_EVENT_2;
            SendMessageToAll(m_StartMessageIds[BG_STARTING_EVENT_SECOND],
                CHAT_MSG_BG_SYSTEM_NEUTRAL);
        }
        // After 30 or 15 seconds, warning is signalled
        else if (GetStartDelayTime() <=
                     m_StartDelayTimes[BG_STARTING_EVENT_THIRD] &&
                 !(m_Events & BG_STARTING_EVENT_3))
        {
            m_Events |= BG_STARTING_EVENT_3;
            SendMessageToAll(m_StartMessageIds[BG_STARTING_EVENT_THIRD],
                CHAT_MSG_BG_SYSTEM_NEUTRAL);
        }
        // delay expired (atfer 2 or 1 minute)
        else if (GetStartDelayTime() <= 0 && !(m_Events & BG_STARTING_EVENT_4))
        {
            m_Events |= BG_STARTING_EVENT_4;

            StartingEventOpenDoors();

            SendMessageToAll(m_StartMessageIds[BG_STARTING_EVENT_FOURTH],
                CHAT_MSG_BG_SYSTEM_NEUTRAL);
            SetStatus(STATUS_IN_PROGRESS);
            SetStartDelayTime(m_StartDelayTimes[BG_STARTING_EVENT_FOURTH]);

            // remove preparation
            if (isArena())
            {
                for (BattleGroundPlayerMap::const_iterator itr =
                         m_Players.begin();
                     itr != m_Players.end(); ++itr)
                {
                    if (Player* player =
                            sObjectMgr::Instance()->GetPlayer(itr->first))
                        player->remove_auras(SPELL_ARENA_PREPARATION);
                }

                CheckArenaWinConditions();
            }
            else
            {
                PlaySoundToAll(SOUND_BG_START);

                for (BattleGroundPlayerMap::const_iterator itr =
                         m_Players.begin();
                     itr != m_Players.end(); ++itr)
                    if (Player* plr =
                            sObjectMgr::Instance()->GetPlayer(itr->first))
                        plr->remove_auras(SPELL_PREPARATION);
                // Announce BG starting
                if (sWorld::Instance()->getConfig(
                        CONFIG_BOOL_BATTLEGROUND_QUEUE_ANNOUNCER_START))
                {
                    sWorld::Instance()->SendWorldText(
                        LANG_BG_STARTED_ANNOUNCE_WORLD, GetName(),
                        GetMinLevel(), GetMaxLevel());
                }
            }
        }
    }

    /*********************************************************/
    /***           BATTLEGROUND ENDING SYSTEM              ***/
    /*********************************************************/

    if (GetStatus() == STATUS_WAIT_LEAVE)
    {
        // remove all players from battleground after 2 minutes
        m_EndTime -= diff;
        if (m_EndTime <= 0)
        {
            m_EndTime = 0;
            BattleGroundPlayerMap::iterator itr, next;
            for (itr = m_Players.begin(); itr != m_Players.end(); itr = next)
            {
                next = itr;
                ++next;
                // itr is erased here!
                RemovePlayerAtLeave(itr->first, true); // remove player from BG
                // do not change any battleground's private variables
            }
        }
    }

    // update start time
    m_StartTime += diff;

    // update time elapsed
    m_TimeElapsed += diff;
}

void BattleGround::SetTeamStartLoc(
    Team team, float X, float Y, float Z, float O)
{
    BattleGroundTeamIndex teamIdx = GetTeamIndexByTeamId(team);
    m_TeamStartLocX[teamIdx] = X;
    m_TeamStartLocY[teamIdx] = Y;
    m_TeamStartLocZ[teamIdx] = Z;
    m_TeamStartLocO[teamIdx] = O;
}

void BattleGround::SendPacketToAll(WorldPacket* packet)
{
    for (BattleGroundPlayerMap::const_iterator itr = m_Players.begin();
         itr != m_Players.end(); ++itr)
    {
        if (itr->second.OfflineRemoveTime)
            continue;

        if (Player* plr = sObjectMgr::Instance()->GetPlayer(itr->first))
            plr->GetSession()->send_packet(packet);
        else
            logging.error("BattleGround:SendPacketToAll: %s not found!",
                itr->first.GetString().c_str());
    }
}

void BattleGround::SendPacketToTeam(
    Team teamId, WorldPacket* packet, Player* sender, bool self)
{
    for (BattleGroundPlayerMap::const_iterator itr = m_Players.begin();
         itr != m_Players.end(); ++itr)
    {
        if (itr->second.OfflineRemoveTime)
            continue;

        Player* plr = sObjectMgr::Instance()->GetPlayer(itr->first);
        if (!plr)
        {
            logging.error("BattleGround:SendPacketToTeam: %s not found!",
                itr->first.GetString().c_str());
            continue;
        }

        if (!self && sender == plr)
            continue;

        Team team = itr->second.PlayerTeam;
        if (!team)
            team = plr->GetTeam();

        if (team == teamId)
            plr->GetSession()->send_packet(packet);
    }
}

void BattleGround::PlaySoundToAll(uint32 SoundID)
{
    WorldPacket data;
    sBattleGroundMgr::Instance()->BuildPlaySoundPacket(&data, SoundID);
    SendPacketToAll(&data);
}

void BattleGround::PlaySoundToTeam(uint32 SoundID, Team teamId)
{
    for (BattleGroundPlayerMap::const_iterator itr = m_Players.begin();
         itr != m_Players.end(); ++itr)
    {
        if (itr->second.OfflineRemoveTime)
            continue;

        Player* plr = sObjectMgr::Instance()->GetPlayer(itr->first);
        if (!plr)
        {
            logging.error("BattleGround:PlaySoundToTeam: %s not found!",
                itr->first.GetString().c_str());
            continue;
        }

        Team team = itr->second.PlayerTeam;
        if (!team)
            team = plr->GetTeam();

        if (team == teamId)
        {
            WorldPacket data;
            sBattleGroundMgr::Instance()->BuildPlaySoundPacket(&data, SoundID);
            plr->GetSession()->send_packet(std::move(data));
        }
    }
}

void BattleGround::CastSpellOnTeam(uint32 SpellID, Team teamId)
{
    for (BattleGroundPlayerMap::const_iterator itr = m_Players.begin();
         itr != m_Players.end(); ++itr)
    {
        if (itr->second.OfflineRemoveTime)
            continue;

        Player* plr = sObjectMgr::Instance()->GetPlayer(itr->first);

        if (!plr)
        {
            logging.error("BattleGround:CastSpellOnTeam: %s not found!",
                itr->first.GetString().c_str());
            continue;
        }

        Team team = itr->second.PlayerTeam;
        if (!team)
            team = plr->GetTeam();

        if (team == teamId)
            plr->CastSpell(plr, SpellID, true);
    }
}

void BattleGround::RewardHonorToTeam(uint32 Honor, Team teamId)
{
    for (BattleGroundPlayerMap::const_iterator itr = m_Players.begin();
         itr != m_Players.end(); ++itr)
    {
        if (itr->second.OfflineRemoveTime)
            continue;

        Player* plr = sObjectMgr::Instance()->GetPlayer(itr->first);

        if (!plr)
        {
            logging.error("BattleGround:RewardHonorToTeam: %s not found!",
                itr->first.GetString().c_str());
            continue;
        }

        Team team = itr->second.PlayerTeam;
        if (!team)
            team = plr->GetTeam();

        if (team == teamId)
            UpdatePlayerScore(plr, SCORE_BONUS_HONOR, Honor);
    }
}

void BattleGround::RewardReputationToTeam(
    uint32 faction_id, uint32 Reputation, Team teamId)
{
    FactionEntry const* factionEntry = sFactionStore.LookupEntry(faction_id);

    if (!factionEntry)
        return;

    for (BattleGroundPlayerMap::const_iterator itr = m_Players.begin();
         itr != m_Players.end(); ++itr)
    {
        if (itr->second.OfflineRemoveTime)
            continue;

        Player* plr = sObjectMgr::Instance()->GetPlayer(itr->first);

        if (!plr)
        {
            logging.error("BattleGround:RewardReputationToTeam: %s not found!",
                itr->first.GetString().c_str());
            continue;
        }

        Team team = itr->second.PlayerTeam;
        if (!team)
            team = plr->GetTeam();

        if (team == teamId)
            plr->GetReputationMgr().ModifyReputation(factionEntry, Reputation);
    }
}

void BattleGround::UpdateWorldState(uint32 Field, uint32 Value)
{
    WorldPacket data;
    sBattleGroundMgr::Instance()->BuildUpdateWorldStatePacket(
        &data, Field, Value);
    SendPacketToAll(&data);
}

void BattleGround::UpdateWorldStateForPlayer(
    uint32 Field, uint32 Value, Player* Source)
{
    WorldPacket data;
    sBattleGroundMgr::Instance()->BuildUpdateWorldStatePacket(
        &data, Field, Value);
    Source->GetSession()->send_packet(std::move(data));
}

void BattleGround::EndBattleGround(Team winner)
{
    int32 winmsg_id = 0;
    bool distribute_rating = true;

    if (winner == ALLIANCE)
    {
        winmsg_id = isBattleGround() ? LANG_BG_A_WINS : LANG_ARENA_GOLD_WINS;

        PlaySoundToAll(SOUND_ALLIANCE_WINS); // alliance wins sound

        SetWinner(WINNER_ALLIANCE);
    }
    else if (winner == HORDE)
    {
        winmsg_id = isBattleGround() ? LANG_BG_H_WINS : LANG_ARENA_GREEN_WINS;

        PlaySoundToAll(SOUND_HORDE_WINS); // horde wins sound

        SetWinner(WINNER_HORDE);
    }
    else
    {
        SetWinner(WINNER_NONE);
        distribute_rating = false;
    }

    SetStatus(STATUS_WAIT_LEAVE);
    SetEndTime(TIME_TO_AUTOREMOVE);

    // Send status packet to all participents so that "Battleground closing in"
    // will render correctly
    for (auto& p : m_Players)
    {
        if (p.second.OfflineRemoveTime)
            continue;
        if (auto plr = sObjectMgr::Instance()->GetPlayer(p.first))
            sBattleGroundMgr::Instance()->send_status_packets(plr);
    }

    // arena rating calculation
    if (isArena() && isRated() && distribute_rating)
    {
        std::pair<int32, int32> result =
            sBattleGroundMgr::Instance()->distribute_arena_rating(
                GetInstanceID(), GetArenaTeamIdForTeam(winner));

        SetArenaTeamRatingChangeForTeam(winner, result.first);
        SetArenaTeamRatingChangeForTeam(GetOtherTeam(winner), result.second);
    }

    for (auto& elem : m_Players)
    {
        Team team = elem.second.PlayerTeam;

        if (elem.second.OfflineRemoveTime)
            continue;

        Player* plr = sObjectMgr::Instance()->GetPlayer(elem.first);
        if (!plr)
        {
            logging.error("BattleGround:EndBattleGround %s not found!",
                elem.first.GetString().c_str());
            continue;
        }

        // should remove spirit of redemption
        if (plr->HasAuraType(SPELL_AURA_SPIRIT_OF_REDEMPTION))
            plr->remove_auras(SPELL_AURA_MOD_SHAPESHIFT);

        if (!plr->isAlive())
        {
            plr->ResurrectPlayer(1.0f);
            plr->SpawnCorpseBones();
        }
        else
        {
            // needed cause else in av some creatures will kill the players at
            // the end
            plr->CombatStop();
            plr->getHostileRefManager().deleteReferences();
        }

        // this line is obsolete - team is set ALWAYS
        // if(!team) team = plr->GetTeam();

        if (team == winner)
        {
            RewardMark(plr, ITEM_WINNER_COUNT);
            RewardQuestComplete(plr);
        }
        else
            RewardMark(plr, ITEM_LOSER_COUNT);

        plr->CombatStopWithPets(true);

        BlockMovement(plr);

        WorldPacket data;
        sBattleGroundMgr::Instance()->BuildPvpLogDataPacket(&data, this);
        plr->GetSession()->send_packet(std::move(data));
    }

    if (winmsg_id)
        SendMessageToAll(winmsg_id, CHAT_MSG_BG_SYSTEM_NEUTRAL);
}

uint32 BattleGround::GetBonusHonorFromKill(uint32 kills) const
{
    // variable kills means how many honorable kills you scored (so we need
    // kills * honor_for_one_kill)
    return (uint32)MaNGOS::Honor::hk_honor_at_level(GetMaxLevel(), kills);
}

uint32 BattleGround::GetBattlemasterEntry() const
{
    switch (GetTypeID())
    {
    case BATTLEGROUND_AV:
        return 15972;
    case BATTLEGROUND_WS:
        return 14623;
    case BATTLEGROUND_AB:
        return 14879;
    case BATTLEGROUND_EY:
        return 22516;
    case BATTLEGROUND_NA:
        return 20200;
    default:
        return 0;
    }
}

void BattleGround::RewardMark(Player* plr, uint32 count)
{
    switch (GetTypeID())
    {
    case BATTLEGROUND_AV:
        RewardItem(plr, ITEM_AV_MARK_OF_HONOR, count);
        break;
    case BATTLEGROUND_WS:
        RewardItem(plr, ITEM_WS_MARK_OF_HONOR, count);
        break;
    case BATTLEGROUND_AB:
        RewardItem(plr, ITEM_AB_MARK_OF_HONOR, count);
        break;
    case BATTLEGROUND_EY:
        RewardItem(plr, ITEM_EY_MARK_OF_HONOR, count);
        break;
    default:
        break;
    }
}

void BattleGround::RewardSpellCast(Player* plr, uint32 spell_id)
{
    // 'Inactive' this aura prevents the player from gaining honor points and
    // battleground tokens
    if (plr->has_aura(SPELL_AURA_PLAYER_INACTIVE, SPELL_AURA_DUMMY))
        return;

    SpellEntry const* spellInfo = sSpellStore.LookupEntry(spell_id);
    if (!spellInfo)
    {
        logging.error(
            "Battleground reward casting spell %u not exist.", spell_id);
        return;
    }

    plr->CastSpell(plr, spellInfo, true);
}

void BattleGround::RewardItem(Player* plr, uint32 item_id, uint32 count)
{
    /* XXX */
    // 'Inactive' this aura prevents the player from gaining honor points and
    // battleground tokens
    if (plr->has_aura(SPELL_AURA_PLAYER_INACTIVE, SPELL_AURA_DUMMY))
        return;

    inventory::transaction trans;
    trans.add(item_id, count);
    if (!plr->storage().finalize(trans))
    {
        uint32 failed = count - trans.add_failures()[0];
        if (failed > 0)
            SendRewardMarkByMail(plr, item_id, failed);
    }
}

void BattleGround::SendRewardMarkByMail(Player* plr, uint32 mark, uint32 count)
{
    /* XXX*/
    uint32 bmEntry = GetBattlemasterEntry();
    if (!bmEntry)
        return;

    ItemPrototype const* markProto = ObjectMgr::GetItemPrototype(mark);
    if (!markProto)
        return;

    if (Item* markItem = Item::CreateItem(mark, count, plr))
    {
        // save new item before send
        markItem->db_save(); // save for prevent lost at next mail load, if send
                             // fail then item will deleted

        int loc_idx = plr->GetSession()->GetSessionDbLocaleIndex();

        // subject: item name
        std::string subject = markProto->Name1;
        sObjectMgr::Instance()->GetItemLocaleStrings(
            markProto->ItemId, loc_idx, &subject);

        // text
        std::string textFormat =
            plr->GetSession()->GetMangosString(LANG_BG_MARK_BY_MAIL);
        char textBuf[300];
        snprintf(textBuf, 300, textFormat.c_str(), GetName(), GetName());

        MailDraft(subject, textBuf)
            .AddItem(markItem)
            .SendMailTo(plr, MailSender(MAIL_CREATURE, bmEntry));
    }
}

void BattleGround::RewardQuestComplete(Player* plr)
{
    uint32 quest;
    switch (GetTypeID())
    {
    case BATTLEGROUND_AV:
        quest = SPELL_AV_QUEST_REWARD;
        break;
    case BATTLEGROUND_WS:
        quest = SPELL_WS_QUEST_REWARD;
        break;
    case BATTLEGROUND_AB:
        quest = SPELL_AB_QUEST_REWARD;
        break;
    case BATTLEGROUND_EY:
        quest = SPELL_EY_QUEST_REWARD;
        break;
    default:
        return;
    }

    RewardSpellCast(plr, quest);
}

void BattleGround::BlockMovement(Player* plr)
{
    plr->SetClientControl(plr, 0); // movement disabled NOTE: the effect will be
                                   // automatically removed by client when the
                                   // player is teleported from the
                                   // battleground, so no need to send with
                                   // uint8(1) in RemovePlayerAtLeave()
}

void BattleGround::RemovePlayerAtLeave(ObjectGuid guid, bool Transport)
{
    Team team = GetPlayerTeam(guid);
    bool participant = false;
    // Remove from lists/maps
    auto itr = m_Players.find(guid);
    if (itr != m_Players.end())
    {
        UpdatePlayersCountByTeam(team, true); // -1 player
        m_Players.erase(itr);
        // check if the player was a participant of the match, or only entered
        // through gm command (goname)
        participant = true;
    }

    // Delete from player teams if not arena and not in wait_leave phase
    if (!isArena() && m_Status != STATUS_WAIT_LEAVE)
    {
        auto find = player_teams_.find(guid);
        if (find != player_teams_.end())
            player_teams_.erase(find);
    }

    // Don't delete scores if it's an arena or we're in the wait_leave phase
    if (!isArena() && m_Status != STATUS_WAIT_LEAVE)
    {
        auto itr2 = m_PlayerScores.find(guid);
        if (itr2 != m_PlayerScores.end())
        {
            delete itr2->second; // delete player's score
            m_PlayerScores.erase(itr2);
        }
    }

    Player* plr = sObjectMgr::Instance()->GetPlayer(guid);

    if (plr)
    {
        // should remove spirit of redemption
        if (plr->HasAuraType(SPELL_AURA_SPIRIT_OF_REDEMPTION))
            plr->remove_auras(SPELL_AURA_MOD_SHAPESHIFT);

        if (!plr->isAlive()) // resurrect on exit
        {
            plr->ResurrectPlayer(1.0f);
            plr->SpawnCorpseBones();
        }
    }
    else // spawn corpse for dead, offline players (does nothing if alive)
    {
        sObjectAccessor::Instance()->ConvertCorpseForPlayer(guid);
    }

    RemovePlayer(plr, guid); // BG subclass specific code

    if (participant) // if the player was a match participant, remove auras,
                     // calc rating, update queue
    {
        if (plr)
        {
            plr->ClearAfkReports();

            if (!team)
                team = plr->GetTeam();

            // if arena, remove the specific arena auras
            if (isArena())
                plr->RemoveArenaAuras(true); // removes debuffs / dots etc., we
                                             // don't want the player to die
                                             // after porting out
        }

        // remove from raid group if player is member
        if (Group* group = GetBgRaid(team))
        {
            if (!group->RemoveMember(guid, 0)) // group was disbanded
            {
                SetBgRaid(team, nullptr);
                delete group;
            }
        }
        DecreaseInvitedCount(team);

        // Let others know
        WorldPacket data;
        sBattleGroundMgr::Instance()->BuildPlayerLeftBattleGroundPacket(
            &data, guid);
        SendPacketToTeam(team, &data, plr, false);
    }

    if (plr)
    {
        // Do next only if found in battleground
        plr->SetBattleGroundId(0, BATTLEGROUND_TYPE_NONE); // We're not in BG.
        // reset destination bg team
        plr->SetBGTeam(TEAM_NONE);
        // clear combo points
        plr->ClearComboPointHolders();

        if (Transport)
            plr->TeleportToBGEntryPoint();

        // Clear BG data once player's left (must be after the above functions
        // since they set save to true)
        plr->ClearBGData();

        LOG_DEBUG(logging, "BATTLEGROUND: Removed player %s from BattleGround.",
            plr->GetName());
    }

    // battleground object will be deleted next BattleGround::Update() call
}

void BattleGround::StartBattleGround()
{
    SetStartTime(0);
}

void BattleGround::AddPlayer(Player* plr)
{
    // remove afk from player
    if (plr->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_AFK))
        plr->ToggleAFK();

    // score struct must be created in inherited class

    ObjectGuid guid = plr->GetObjectGuid();
    Team team = plr->GetBGTeam();

    if (m_Players.find(guid) != m_Players.end())
    {
        logging.error(
            "BattleGround::AddPlayer() called for Player %s whom was already "
            "in the Battleground.",
            guid.GetString().c_str());
        return;
    }

    BattleGroundPlayer bp;
    bp.OfflineRemoveTime = 0;
    bp.PlayerTeam = team;

    // Add to list/maps
    m_Players[guid] = bp;
    player_teams_[guid] = team;

    UpdatePlayersCountByTeam(team, false); // +1 player

    WorldPacket data;
    sBattleGroundMgr::Instance()->BuildPlayerJoinedBattleGroundPacket(
        &data, plr);
    SendPacketToTeam(team, &data, plr, false);

    // add arena specific auras
    if (isArena())
    {
        // We need to remove auras after add to map, for correct speeds
        // Move all actions to after player has been added to map for arenas
        auto status = GetStatus();
        plr->queue_action(0, [plr, team, status]()
            {
                plr->RemoveArenaSpellCooldowns();
                plr->RemoveArenaAuras();
                plr->RemoveTempEnchantsOnArenaEntry();
                if (team == ALLIANCE) // gold
                {
                    if (plr->GetTeam() == HORDE)
                        plr->CastSpell(plr, SPELL_HORDE_GOLD_FLAG, true);
                    else
                        plr->CastSpell(plr, SPELL_ALLIANCE_GOLD_FLAG, true);
                }
                else // green
                {
                    if (plr->GetTeam() == HORDE)
                        plr->CastSpell(plr, SPELL_HORDE_GREEN_FLAG, true);
                    else
                        plr->CastSpell(plr, SPELL_ALLIANCE_GREEN_FLAG, true);
                }

                // XXX:
                inventory::destroy_conjured_items(plr->storage());

                if (status == STATUS_WAIT_JOIN) // not started yet
                    plr->CastSpell(plr, SPELL_ARENA_PREPARATION, true);
            });
    }
    else
    {
        if (GetStatus() == STATUS_WAIT_JOIN) // not started yet
            plr->CastSpell(plr, SPELL_PREPARATION,
                true); // reduces all mana cost of spells.
    }

    // setup BG group membership
    PlayerAddedToBGCheckIfBGIsRunning(plr);
    AddOrSetPlayerToCorrectBgGroup(plr, guid, team);

    // Log
    LOG_DEBUG(
        logging, "BATTLEGROUND: Player %s joined the battle.", plr->GetName());
}

/* this method adds player to his team's bg group, or sets his correct group if
 * player is already in bg group */
void BattleGround::AddOrSetPlayerToCorrectBgGroup(
    Player* plr, ObjectGuid plr_guid, Team team)
{
    if (Group* group = GetBgRaid(team)) // raid already exist
    {
        if (group->IsMember(plr_guid))
        {
            uint8 subgroup = group->GetMemberGroup(plr_guid);
            plr->SetBattleGroundRaid(group, subgroup);
        }
        else
        {
            group->AddMember(plr_guid, plr->GetName());
            if (Group* originalGroup = plr->GetOriginalGroup())
                if (originalGroup->IsLeader(plr_guid))
                    group->ChangeLeader(plr_guid);
        }
    }
    else // first player joined
    {
        group = new Group;
        SetBgRaid(team, group);
        group->Create(plr_guid, plr->GetName());
    }
}

// This method should be called when player logs into running battleground
void BattleGround::EventPlayerLoggedIn(Player* player, ObjectGuid plr_guid)
{
    // player is correct pointer
    for (auto itr = m_OfflineQueue.begin(); itr != m_OfflineQueue.end(); ++itr)
    {
        if (*itr == plr_guid)
        {
            m_OfflineQueue.erase(itr);
            break;
        }
    }
    m_Players[plr_guid].OfflineRemoveTime = 0;
    PlayerAddedToBGCheckIfBGIsRunning(player);
    // if battleground is starting, then add preparation aura
    // we don't have to do that, because preparation aura isn't removed when
    // player logs out
}

// This method should be called when player logs out from running battleground
void BattleGround::EventPlayerLoggedOut(Player* player)
{
    // player is correct pointer, it is checked in WorldSession::LogoutPlayer()
    m_OfflineQueue.push_back(player->GetObjectGuid());
    m_Players[player->GetObjectGuid()].OfflineRemoveTime =
        WorldTimer::time_no_syscall() + MAX_OFFLINE_TIME;
    if (GetStatus() == STATUS_IN_PROGRESS)
    {
        // drop flag and handle other cleanups
        RemovePlayer(player, player->GetObjectGuid());

        // End arena if no more alive players on player's team
        if (isArena())
        {
            if (GetAlivePlayersCountByTeam(player->GetBGTeam()) == 0 &&
                GetPlayersCountByTeam(GetOtherTeam(player->GetBGTeam())) > 0)
                EndBattleGround(GetOtherTeam(player->GetBGTeam()));
        }
    }
}

// get the number of free slots for team
// returns the number how many players can join battleground to
// MaxPlayersPerTeam
uint32 BattleGround::GetFreeSlotsForTeam(Team team) const
{
    // return free slot count to MaxPlayerPerTeam
    if (GetStatus() == STATUS_WAIT_JOIN || GetStatus() == STATUS_IN_PROGRESS)
        return (GetInvitedCount(team) < GetMaxPlayersPerTeam()) ?
                   GetMaxPlayersPerTeam() - GetInvitedCount(team) :
                   0;

    return 0;
}

bool BattleGround::IsTerminated() const
{
    // GetPlayersSize() can be 0 while the map still has players. This happens
    // when players are being ported out and existent in an in-betwen state.
    if (m_Map)
    {
        if (m_Map->HavePlayers())
            return false;
    }
    return !GetPlayersSize() && !GetInvitedCount(HORDE) &&
           !GetInvitedCount(ALLIANCE);
}

bool BattleGround::HasFreeSlots() const
{
    return GetPlayersSize() < GetMaxPlayers();
}

void BattleGround::UpdatePlayerScore(Player* Source, uint32 type, uint32 value)
{
    // this procedure is called from virtual function implemented in bg subclass
    BattleGroundScoreMap::const_iterator itr =
        m_PlayerScores.find(Source->GetObjectGuid());

    if (itr == m_PlayerScores.end()) // player not found...
        return;

    switch (type)
    {
    case SCORE_KILLING_BLOWS: // Killing blows
        itr->second->KillingBlows += value;
        break;
    case SCORE_DEATHS: // Deaths
        itr->second->Deaths += value;
        break;
    case SCORE_HONORABLE_KILLS: // Honorable kills
        itr->second->HonorableKills += value;
        break;
    case SCORE_BONUS_HONOR: // Honor bonus
        // do not add honor in arenas
        if (isBattleGround())
        {
            // reward honor instantly
            if (Source->RewardHonor(nullptr, 1, (float)value))
                itr->second->BonusHonor += value;
        }
        break;
    // used only in EY, but in MSG_PVP_LOG_DATA opcode
    case SCORE_DAMAGE_DONE: // Damage Done
        itr->second->DamageDone += value;
        break;
    case SCORE_HEALING_DONE: // Healing Done
        itr->second->HealingDone += value;
        break;
    default:
        logging.error("BattleGround: Unknown player score type %u", type);
        break;
    }
}

bool BattleGround::AddObject(uint32 type, uint32 entry, float x, float y,
    float z, float o, float rotation0, float rotation1, float rotation2,
    float rotation3, uint32 /*respawnTime*/)
{
    // must be created this way, adding to godatamap would add it to the base
    // map of the instance
    // and when loading it (in go::LoadFromDB()), a new guid would be assigned
    // to the object, and a new object would be created
    // so we must create it specific for this instance
    auto go = new GameObject;
    if (!go->Create(GetBgMap()->GenerateLocalLowGuid(HIGHGUID_GAMEOBJECT),
            entry, GetBgMap(), x, y, z, o, rotation0, rotation1, rotation2,
            rotation3))
    {
        logging.error(
            "Gameobject template %u not found in database! BattleGround not "
            "created!",
            entry);
        logging.error(
            "Cannot create gameobject template %u! BattleGround not created!",
            entry);
        delete go;
        return false;
    }
    /*
        uint32 guid = go->GetGUIDLow();

        // without this, UseButtonOrDoor caused the crash, since it tried to get
       go info from godata
        // iirc that was changed, so adding to go data map is no longer required
       if that was the only function using godata from GameObject without
       checking if it existed
        GameObjectData& data = sObjectMgr::Instance()->NewGOData(guid);

        data.id             = entry;
        data.mapid          = GetMapId();
        data.posX           = x;
        data.posY           = y;
        data.posZ           = z;
        data.orientation    = o;
        data.rotation0      = rotation0;
        data.rotation1      = rotation1;
        data.rotation2      = rotation2;
        data.rotation3      = rotation3;
        data.spawntimesecs  = respawnTime;
        data.spawnMask      = 1;
        data.animprogress   = 100;
        data.go_state       = 1;
    */
    // add to world, so it can be later looked up from HashMapHolder
    go->AddToWorld();
    m_BgObjects[type] = go->GetObjectGuid();
    return true;
}

// some doors aren't despawned so we cannot handle their closing in
// gameobject::update()
// it would be nice to correctly implement GO_ACTIVATED state and open/close
// doors in gameobject code
void BattleGround::DoorClose(ObjectGuid guid)
{
    GameObject* obj = GetBgMap()->GetGameObject(guid);
    if (obj)
    {
        // if doors are open, close it
        if (obj->getLootState() == GO_ACTIVATED &&
            obj->GetGoState() != GO_STATE_READY)
        {
            // change state to allow door to be closed
            obj->SetLootState(GO_READY);
            obj->UseDoorOrButton(RESPAWN_ONE_WEEK);
        }
    }
    else
        logging.error("BattleGround: Door %s not found (cannot close doors)",
            guid.GetString().c_str());
}

void BattleGround::DoorOpen(ObjectGuid guid)
{
    GameObject* obj = GetBgMap()->GetGameObject(guid);
    if (obj)
    {
        // Change state to be sure they will be opened
        obj->SetLootState(GO_READY);
        obj->UseDoorOrButton(RESPAWN_ONE_WEEK);
        // Remove door from map in ~3 seconds
        obj->queue_action(3000, [obj]()
            {
                obj->GetMap()->erase(obj, true);
            });
    }
    else
        logging.error(
            "BattleGround: Door %s not found! - doors will be closed.",
            guid.GetString().c_str());
}

void BattleGround::OnObjectDBLoad(Creature* creature)
{
    const BattleGroundEventIdx eventId =
        sBattleGroundMgr::Instance()->GetCreatureEventIndex(
            creature->GetGUIDLow());
    if (eventId.event1 == BG_EVENT_NONE)
    {
        if (!SpawnedByDefault(creature))
            SpawnBGCreature(creature->GetObjectGuid(), RESPAWN_ONE_WEEK);

        return;
    }
    m_EventObjects[MAKE_PAIR32(eventId.event1, eventId.event2)]
        .creatures.push_back(creature->GetObjectGuid());
    if (!IsActiveEvent(eventId.event1, eventId.event2))
        SpawnBGCreature(creature->GetObjectGuid(), RESPAWN_ONE_WEEK);
}

ObjectGuid BattleGround::GetSingleCreatureGuid(uint8 event1, uint8 event2)
{
    BGCreatures::const_iterator itr =
        m_EventObjects[MAKE_PAIR32(event1, event2)].creatures.begin();
    if (itr != m_EventObjects[MAKE_PAIR32(event1, event2)].creatures.end())
        return *itr;
    return ObjectGuid();
}

void BattleGround::OnObjectDBLoad(GameObject* obj)
{
    const BattleGroundEventIdx eventId =
        sBattleGroundMgr::Instance()->GetGameObjectEventIndex(
            obj->GetGUIDLow());
    if (eventId.event1 == BG_EVENT_NONE)
        return;
    m_EventObjects[MAKE_PAIR32(eventId.event1, eventId.event2)]
        .gameobjects.push_back(obj->GetObjectGuid());
    if (!IsActiveEvent(eventId.event1, eventId.event2))
    {
        SpawnBGObject(obj->GetObjectGuid(), RESPAWN_ONE_WEEK);
    }
    else
    {
        // it's possible, that doors aren't spawned anymore (wsg)
        if (GetStatus() >= STATUS_IN_PROGRESS &&
            IsDoor(eventId.event1, eventId.event2))
            DoorOpen(obj->GetObjectGuid());
    }
}

bool BattleGround::IsDoor(uint8 event1, uint8 event2)
{
    if (event1 == BG_EVENT_DOOR)
    {
        if (event2 > 0)
        {
            logging.error("BattleGround too high event2 for event1:%i", event1);
            return false;
        }
        return true;
    }
    return false;
}

void BattleGround::OpenDoorEvent(uint8 event1, uint8 event2 /*=0*/)
{
    if (!IsDoor(event1, event2))
    {
        logging.error(
            "BattleGround:OpenDoorEvent this is no door event1:%u event2:%u",
            event1, event2);
        return;
    }
    if (!IsActiveEvent(event1, event2)) // maybe already despawned (eye)
    {
        logging.error(
            "BattleGround:OpenDoorEvent this event isn't active event1:%u "
            "event2:%u",
            event1, event2);
        return;
    }
    BGObjects::const_iterator itr =
        m_EventObjects[MAKE_PAIR32(event1, event2)].gameobjects.begin();
    for (; itr != m_EventObjects[MAKE_PAIR32(event1, event2)].gameobjects.end();
         ++itr)
        DoorOpen(*itr);
}

void BattleGround::SpawnEvent(
    uint8 event1, uint8 event2, bool spawn, bool despawn_other)
{
    // stop if we want to spawn something which was already spawned
    // or despawn something which was already despawned
    if (event2 == BG_EVENT_NONE ||
        (spawn && m_ActiveEvents[event1] == event2) ||
        (!spawn && m_ActiveEvents[event1] != event2))
        return;

    if (spawn)
    {
        // despawn the current active event
        if (despawn_other)
        {
            SpawnEvent(event1, m_ActiveEvents[event1], false);
        }
        else
        {
            uint32 e1 = event1, e2 = m_ActiveEvents[event1];
            for (auto& elem : m_EventObjects[MAKE_PAIR32(e1, e2)].creatures)
            {
                if (Creature* c = GetBgMap()->GetCreature(elem))
                    c->SetRespawnDelay(RESPAWN_ONE_WEEK);
            }
        }
        m_ActiveEvents[event1] = event2; // set this event to active
    }
    else
        m_ActiveEvents[event1] =
            BG_EVENT_NONE; // no event active if event2 gets despawned

    BGCreatures::const_iterator itr =
        m_EventObjects[MAKE_PAIR32(event1, event2)].creatures.begin();
    for (; itr != m_EventObjects[MAKE_PAIR32(event1, event2)].creatures.end();
         ++itr)
        SpawnBGCreature(*itr, (spawn) ? RESPAWN_IMMEDIATELY : RESPAWN_ONE_WEEK);
    BGObjects::const_iterator itr2 =
        m_EventObjects[MAKE_PAIR32(event1, event2)].gameobjects.begin();
    for (;
         itr2 != m_EventObjects[MAKE_PAIR32(event1, event2)].gameobjects.end();
         ++itr2)
        SpawnBGObject(*itr2, (spawn) ? RESPAWN_IMMEDIATELY : RESPAWN_ONE_WEEK);
}

void BattleGround::SpawnBGObject(ObjectGuid guid, uint32 respawntime)
{
    Map* map = GetBgMap();

    GameObject* obj = map->GetGameObject(guid);
    if (!obj)
        return;
    if (respawntime == 0)
    {
        // we need to change state from GO_JUST_DEACTIVATED to GO_READY in case
        // battleground is starting again
        if (obj->getLootState() == GO_JUST_DEACTIVATED)
            obj->SetLootState(GO_READY);
        obj->SetRespawnTime(0);
        map->erase(obj, false);
        map->insert(obj);
    }
    else
    {
        map->erase(obj, false);
        map->insert(obj);
        obj->SetRespawnTime(respawntime);
        obj->SetLootState(GO_JUST_DEACTIVATED);
    }
}

void BattleGround::SpawnBGCreature(ObjectGuid guid, uint32 respawntime)
{
    Map* map = GetBgMap();

    Creature* obj = map->GetCreature(guid);
    if (!obj)
        return;
    if (respawntime == 0)
    {
        obj->Respawn(true);

        // Reset the RespawnDelay to be creature.spawntimesecs
        const CreatureData* data =
            sObjectMgr::Instance()->GetCreatureData(obj->GetGUIDLow());
        if (data)
            obj->SetRespawnDelay(data->spawntimesecs);
    }
    else
    {
        map->erase(obj, false);
        map->insert(obj);
        obj->SetRespawnDelay(respawntime);
        obj->SetDeathState(JUST_DIED);
        obj->RemoveCorpse();
    }
}

bool BattleGround::DelObject(uint32 type)
{
    if (!m_BgObjects[type])
        return true;

    GameObject* obj = GetBgMap()->GetGameObject(m_BgObjects[type]);
    if (!obj)
    {
        logging.error(
            "Can't find gobject: %s", m_BgObjects[type].GetString().c_str());
        return false;
    }

    obj->SetRespawnTime(0); // not save respawn time
    obj->Delete();
    m_BgObjects[type].Clear();
    return true;
}

void BattleGround::SendMessageToAll(
    int32 entry, ChatMsg type, Player const* source)
{
    MaNGOS::BattleGroundChatBuilder bg_builder(type, entry, source);
    auto bg_do = maps::callbacks::make_localize_packet(bg_builder);
    BroadcastWorker(bg_do);
}

void BattleGround::SendYellToAll(int32 entry, uint32 language, ObjectGuid guid)
{
    Creature* source = GetBgMap()->GetCreature(guid);
    if (!source)
        return;
    MaNGOS::BattleGroundYellBuilder bg_builder(language, entry, source);
    auto bg_do = maps::callbacks::make_localize_packet(bg_builder);
    BroadcastWorker(bg_do);
}

void BattleGround::PSendMessageToAll(
    int32 entry, ChatMsg type, Player const* source, ...)
{
    va_list ap;
    va_start(ap, source);

    MaNGOS::BattleGroundChatBuilder bg_builder(type, entry, source, &ap);
    auto bg_do = maps::callbacks::make_localize_packet(bg_builder);
    BroadcastWorker(bg_do);

    va_end(ap);
}

void BattleGround::SendMessage2ToAll(
    int32 entry, ChatMsg type, Player const* source, int32 arg1, int32 arg2)
{
    MaNGOS::BattleGround2ChatBuilder bg_builder(
        type, entry, source, arg1, arg2);
    auto bg_do = maps::callbacks::make_localize_packet(bg_builder);
    BroadcastWorker(bg_do);
}

void BattleGround::SendYell2ToAll(
    int32 entry, uint32 language, ObjectGuid guid, int32 arg1, int32 arg2)
{
    Creature* source = GetBgMap()->GetCreature(guid);
    if (!source)
        return;
    MaNGOS::BattleGround2YellBuilder bg_builder(
        language, entry, source, arg1, arg2);
    auto bg_do = maps::callbacks::make_localize_packet(bg_builder);
    BroadcastWorker(bg_do);
}

void BattleGround::EndNow()
{
    SetStatus(STATUS_WAIT_LEAVE);
    SetEndTime(0);
}

/*
important notice:
buffs aren't spawned/despawned when players captures anything
buffs are in their positions when battleground starts
*/
void BattleGround::HandleTriggerBuff(ObjectGuid go_guid, Player* took_buff)
{
    GameObject* obj = GetBgMap()->GetGameObject(go_guid);
    if (!obj || obj->GetGoType() != GAMEOBJECT_TYPE_TRAP || !obj->isSpawned())
        return;

    // static buffs are already handled just by database and don't need
    // battleground code
    if (!m_BuffChange)
    {
        if (obj->GetEntry() == BG_OBJECTID_SHADOW_SIGHT_ONE ||
            obj->GetEntry() == BG_OBJECTID_SHADOW_SIGHT_TWO)
        {
            obj->SetRespawnTime(ARENA_SPAWN_BUFF_OBJECTS);
            if (took_buff)
                took_buff->remove_auras_on_event(AURA_INTERRUPT_FLAG_INTERACT);
        }

        obj->SetLootState(GO_JUST_DEACTIVATED); // can be despawned or destroyed
        return;
    }

    // change buff type, when buff is used:
    // TODO this can be done when poolsystem works for instances
    int32 index = m_BgObjects.size() - 1;
    while (index >= 0 && m_BgObjects[index] != go_guid)
        index--;
    if (index < 0)
    {
        logging.error(
            "BattleGround (Type: %u) has buff trigger %s GOType: %u but it "
            "hasn't that object in its internal data",
            GetTypeID(), go_guid.GetString().c_str(), obj->GetGoType());
        return;
    }

    // randomly select new buff
    uint8 buff = urand(0, 2);
    uint32 entry = obj->GetEntry();
    if (m_BuffChange && entry != Buff_Entries[buff])
    {
        // despawn current buff
        SpawnBGObject(m_BgObjects[index], RESPAWN_ONE_WEEK);
        // set index for new one
        for (uint8 currBuffTypeIndex = 0; currBuffTypeIndex < 3;
             ++currBuffTypeIndex)
        {
            if (entry == Buff_Entries[currBuffTypeIndex])
            {
                index -= currBuffTypeIndex;
                index += buff;
            }
        }
    }

    SpawnBGObject(m_BgObjects[index], BUFF_RESPAWN_TIME);
}

void BattleGround::HandleKillPlayer(Player* player, Player* killer)
{
    // add +1 deaths
    UpdatePlayerScore(player, SCORE_DEATHS, 1);

    // add +1 kills to group and +1 killing_blows to killer
    if (killer)
    {
        UpdatePlayerScore(killer, SCORE_HONORABLE_KILLS, 1);
        UpdatePlayerScore(killer, SCORE_KILLING_BLOWS, 1);

        for (BattleGroundPlayerMap::const_iterator itr = m_Players.begin();
             itr != m_Players.end(); ++itr)
        {
            Player* plr = sObjectMgr::Instance()->GetPlayer(itr->first);

            if (!plr || plr == killer)
                continue;

            if (plr->GetTeam() == killer->GetTeam() &&
                plr->IsAtGroupRewardDistance(player))
                UpdatePlayerScore(plr, SCORE_HONORABLE_KILLS, 1);
        }
    }

    // to be able to remove insignia -- ONLY IN BattleGrounds
    if (!isArena())
        player->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_SKINNABLE);
}

// return the player's team
// used in same faction arena matches mainly
Team BattleGround::GetPlayerTeam(ObjectGuid guid)
{
    std::map<ObjectGuid, Team>::const_iterator itr = player_teams_.find(guid);
    if (itr != player_teams_.end())
        return itr->second;
    return TEAM_NONE;
}

bool BattleGround::IsPlayerInBattleGround(ObjectGuid guid)
{
    BattleGroundPlayerMap::const_iterator itr = m_Players.find(guid);
    if (itr != m_Players.end())
        return true;
    return false;
}

void BattleGround::PlayerAddedToBGCheckIfBGIsRunning(Player* plr)
{
    if (GetStatus() != STATUS_WAIT_LEAVE)
        return;

    BlockMovement(plr);

    WorldPacket data;
    sBattleGroundMgr::Instance()->BuildPvpLogDataPacket(&data, this);
    plr->GetSession()->send_packet(std::move(data));
}

uint32 BattleGround::GetAlivePlayersCountByTeam(Team team) const
{
    int count = 0;
    for (const auto& elem : m_Players)
    {
        if (elem.second.PlayerTeam == team)
        {
            Player* pl = sObjectMgr::Instance()->GetPlayer(elem.first);
            if (pl && pl->isAlive())
                ++count;
        }
    }
    return count;
}

void BattleGround::CheckArenaWinConditions()
{
    auto alive_ally = GetAlivePlayersCountByTeam(ALLIANCE);
    auto alive_horde = GetAlivePlayersCountByTeam(HORDE);
    auto count_ally = GetPlayersCountByTeam(ALLIANCE);
    auto count_horde = GetPlayersCountByTeam(HORDE);

    if (alive_ally == 0 && alive_horde == 0 && count_ally > 0 && count_horde > 0)
        EndBattleGround(TEAM_NONE);
    else if (alive_ally == 0 && count_horde > 0)
        EndBattleGround(HORDE);
    else if (alive_horde == 0 && count_ally > 0)
        EndBattleGround(ALLIANCE);
}

void BattleGround::SetBgRaid(Team team, Group* bg_raid)
{
    Group*& old_raid = m_BgRaids[GetTeamIndexByTeamId(team)];

    if (old_raid)
        old_raid->SetBattlegroundGroup(nullptr);

    if (bg_raid)
        bg_raid->SetBattlegroundGroup(this);

    old_raid = bg_raid;
}

WorldSafeLocsEntry const* BattleGround::GetClosestGraveYard(Player* player)
{
    return sObjectMgr::Instance()->GetClosestGraveyard(player->GetX(),
        player->GetY(), player->GetZ(), player->GetMapId(), player->GetTeam());
}

void BattleGround::SendToBattleGround(Player* player)
{
    uint32 mapid = GetMapId();
    float x, y, z, O;
    Team team = player->GetBGTeam();
    if (team == 0)
        team = player->GetTeam();
    GetTeamStartLoc(team, x, y, z, O);

    // The client expects to receive the status update before the new world
    // packet
    // as proven by the fact that the Gladius addon generates an error otherwise
    sBattleGroundMgr::Instance()->send_status_packets(player);

    LOG_DEBUG(logging,
        "BATTLEGROUND: Sending %s to map %u, X %f, Y %f, Z %f, O %f",
        player->GetName(), mapid, x, y, z, O);
    player->TeleportTo(mapid, x, y, z, O);
}

bool BattleGround::add_invite(ObjectGuid guid)
{
    if (Player* player = ObjectAccessor::FindPlayer(guid, false))
    {
        invitee_teams_[guid] = player->GetBGTeam();
        return true;
    }
    return false;
}
bool BattleGround::remove_invite(ObjectGuid guid)
{
    auto itr = invitee_teams_.find(guid);
    if (itr == invitee_teams_.end())
        return false;
    invitee_teams_.erase(itr);
    return true;
}

Team BattleGround::invitee_team(ObjectGuid guid) const
{
    auto itr = invitee_teams_.find(guid);
    if (itr == invitee_teams_.end())
        return TEAM_NONE;
    return itr->second;
}
