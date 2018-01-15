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

/// \addtogroup world The World
/// @{
/// \file

#ifndef __WORLD_H
#define __WORLD_H

#include "Common.h"
#include "SharedDefines.h"
#include "Timer.h"
#include "Policies/Singleton.h"
#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/thread.hpp>
#include <list>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>

class Object;
class WorldPacket;
class WorldSession;
class Player;
class Weather;
class SqlResultQueue;
class QueryResult;
class Spell;

// ServerMessages.dbc
enum ServerMessageType
{
    SERVER_MSG_SHUTDOWN_TIME = 1,
    SERVER_MSG_RESTART_TIME = 2,
    SERVER_MSG_CUSTOM = 3,
    SERVER_MSG_SHUTDOWN_CANCELLED = 4,
    SERVER_MSG_RESTART_CANCELLED = 5,
};

enum ShutdownMask
{
    SHUTDOWN_MASK_RESTART = 1,
    SHUTDOWN_MASK_IDLE = 2,
};

enum ShutdownExitCode
{
    SHUTDOWN_EXIT_CODE = 0,
    ERROR_EXIT_CODE = 1,
    RESTART_EXIT_CODE = 2,
};

/// Timers for different object refresh rates
enum WorldTimers
{
    WUPDATE_AUCTIONS = 0,
    WUPDATE_WEATHERS,
    WUPDATE_UPTIME,
    WUPDATE_CORPSES,
    WUPDATE_EVENTS,
    WUPDATE_DELETECHARS,
    WUPDATE_ACTIONLIMIT,
    WUPDATE_TICKET_MGR,
    WUPDATE_LEADERLESS_GROUP,
    WUPDATE_COUNT
};

/// Configuration elements
enum eConfigUInt32Values
{
    CONFIG_UINT32_COMPRESSION = 0,
    CONFIG_UINT32_INTERVAL_SAVE,
    CONFIG_UINT32_INTERVAL_MAPUPDATE,
    CONFIG_UINT32_INTERVAL_CHANGEWEATHER,
    CONFIG_UINT32_PORT_WORLD,
    CONFIG_UINT32_GAME_TYPE,
    CONFIG_UINT32_REALM_ZONE,
    CONFIG_UINT32_STRICT_PLAYER_NAMES,
    CONFIG_UINT32_STRICT_CHARTER_NAMES,
    CONFIG_UINT32_STRICT_PET_NAMES,
    CONFIG_UINT32_MIN_PLAYER_NAME,
    CONFIG_UINT32_MIN_CHARTER_NAME,
    CONFIG_UINT32_MIN_PET_NAME,
    CONFIG_UINT32_CHARACTERS_CREATING_DISABLED,
    CONFIG_UINT32_CHARACTERS_PER_ACCOUNT,
    CONFIG_UINT32_CHARACTERS_PER_REALM,
    CONFIG_UINT32_SKIP_CINEMATICS,
    CONFIG_UINT32_MAX_PLAYER_LEVEL,
    CONFIG_UINT32_START_PLAYER_LEVEL,
    CONFIG_UINT32_START_PLAYER_MONEY,
    CONFIG_UINT32_MAX_HONOR_POINTS,
    CONFIG_UINT32_START_HONOR_POINTS,
    CONFIG_UINT32_MAX_ARENA_POINTS,
    CONFIG_UINT32_START_ARENA_POINTS,
    CONFIG_UINT32_INSTANCE_RESET_TIME_HOUR,
    CONFIG_UINT32_INSTANCE_UNLOAD_DELAY,
    CONFIG_UINT32_MAX_SPELL_CASTS_IN_CHAIN,
    CONFIG_UINT32_MAX_PRIMARY_TRADE_SKILL,
    CONFIG_UINT32_TRADE_SKILL_GMIGNORE_MAX_PRIMARY_COUNT,
    CONFIG_UINT32_TRADE_SKILL_GMIGNORE_LEVEL,
    CONFIG_UINT32_TRADE_SKILL_GMIGNORE_SKILL,
    CONFIG_UINT32_MIN_PETITION_SIGNS,
    CONFIG_UINT32_GM_LOGIN_STATE,
    CONFIG_UINT32_GM_VISIBLE_STATE,
    CONFIG_UINT32_GM_CHAT,
    CONFIG_UINT32_GM_LEVEL_IN_GM_LIST,
    CONFIG_UINT32_GM_LEVEL_IN_WHO_LIST,
    CONFIG_UINT32_START_GM_LEVEL,
    CONFIG_UINT32_GROUP_VISIBILITY,
    CONFIG_UINT32_MAIL_DELIVERY_DELAY,
    CONFIG_UINT32_MASS_MAILER_SEND_PER_TICK,
    CONFIG_UINT32_UPTIME_UPDATE,
    CONFIG_UINT32_AUCTION_DEPOSIT_MIN,
    CONFIG_UINT32_SKILL_CHANCE_ORANGE,
    CONFIG_UINT32_SKILL_CHANCE_YELLOW,
    CONFIG_UINT32_SKILL_CHANCE_GREEN,
    CONFIG_UINT32_SKILL_CHANCE_GREY,
    CONFIG_UINT32_SKILL_CHANCE_MINING_STEPS,
    CONFIG_UINT32_SKILL_CHANCE_SKINNING_STEPS,
    CONFIG_UINT32_SKILL_GAIN_CRAFTING,
    CONFIG_UINT32_SKILL_GAIN_DEFENSE,
    CONFIG_UINT32_SKILL_GAIN_GATHERING,
    CONFIG_UINT32_SKILL_GAIN_WEAPON,
    CONFIG_UINT32_MAX_OVERSPEED_PINGS,
    CONFIG_UINT32_EXPANSION,
    CONFIG_UINT32_CHATFLOOD_MESSAGE_COUNT,
    CONFIG_UINT32_CHATFLOOD_MESSAGE_DELAY,
    CONFIG_UINT32_CHATFLOOD_MUTE_TIME,
    CONFIG_UINT32_CREATURE_FAMILY_ASSISTANCE_DELAY,
    CONFIG_UINT32_CREATURE_FAMILY_FLEE_DELAY,
    CONFIG_UINT32_WORLD_BOSS_LEVEL_DIFF,
    CONFIG_UINT32_QUEST_DAILY_RESET_HOUR,
    CONFIG_UINT32_CHAT_STRICT_LINK_CHECKING_SEVERITY,
    CONFIG_UINT32_CHAT_STRICT_LINK_CHECKING_KICK,
    CONFIG_UINT32_CORPSE_DECAY_NORMAL,
    CONFIG_UINT32_CORPSE_DECAY_RARE,
    CONFIG_UINT32_CORPSE_DECAY_ELITE,
    CONFIG_UINT32_CORPSE_DECAY_RAREELITE,
    CONFIG_UINT32_CORPSE_DECAY_WORLDBOSS,
    CONFIG_UINT32_CORPSE_DECAY_LOOTED,
    CONFIG_UINT32_INSTANT_LOGOUT,
    CONFIG_UINT32_BATTLEGROUND_INVITATION_TYPE,
    CONFIG_UINT32_BATTLEGROUND_PREMATURE_FINISH_TIMER,
    CONFIG_UINT32_BATTLEGROUND_PREMADE_GROUP_WAIT_FOR_MATCH,
    CONFIG_UINT32_BATTLEGROUND_QUEUE_ANNOUNCER_JOIN,
    CONFIG_UINT32_ARENA_MAX_RATING_DIFFERENCE,
    CONFIG_UINT32_ARENA_RATING_DISCARD_TIMER,
    CONFIG_UINT32_ARENA_AUTO_DISTRIBUTE_INTERVAL_DAYS,
    CONFIG_UINT32_ARENA_SEASON_ID,
    CONFIG_UINT32_GUILD_EVENT_LOG_COUNT,
    CONFIG_UINT32_GUILD_BANK_EVENT_LOG_COUNT,
    CONFIG_UINT32_TIMERBAR_FATIGUE_GMLEVEL,
    CONFIG_UINT32_TIMERBAR_FATIGUE_MAX,
    CONFIG_UINT32_TIMERBAR_BREATH_GMLEVEL,
    CONFIG_UINT32_TIMERBAR_BREATH_MAX,
    CONFIG_UINT32_TIMERBAR_FIRE_GMLEVEL,
    CONFIG_UINT32_TIMERBAR_FIRE_MAX,
    CONFIG_UINT32_MIN_LEVEL_STAT_SAVE,
    CONFIG_UINT32_CHARDELETE_KEEP_DAYS,
    CONFIG_UINT32_CHARDELETE_METHOD,
    CONFIG_UINT32_CHARDELETE_MIN_LEVEL,
    CONFIG_UINT32_GUID_RESERVE_SIZE_CREATURE,
    CONFIG_UINT32_GUID_RESERVE_SIZE_GAMEOBJECT,
    CONFIG_UINT32_WARDEN_NUM_CHECKS,
    CONFIG_UINT32_WARDEN_CLIENT_CHECK_HOLDOFF,
    CONFIG_UINT32_WARDEN_CLIENT_RESPONSE_DELAY,
    CONFIG_UINT32_WARDEN_BAN_TIME,
    CONFIG_UINT32_WARDEN_NUM_DYN_CHECKS,
    CONFIG_UINT32_WARDEN_SLOW_CHECKS_DELAY,
    CONFIG_UINT32_MAP_THREADS,
    CONFIG_UINT32_DURABILITY_LOSS_COMBAT_MIN,
    CONFIG_UINT32_DURABILITY_LOSS_COMBAT_MAX,
    CONFIG_UINT32_CHARACTER_SCREEN_TIMEOUT,
    CONFIG_UINT32_VALUE_COUNT
};

/// Configuration elements
enum eConfigInt32Values
{
    CONFIG_INT32_DEATH_SICKNESS_LEVEL = 0,
    CONFIG_INT32_ARENA_STARTRATING,
    CONFIG_INT32_ARENA_STARTPERSONALRATING,
    CONFIG_INT32_QUEST_LOW_LEVEL_HIDE_DIFF,
    CONFIG_INT32_QUEST_HIGH_LEVEL_HIDE_DIFF,
    CONFIG_INT32_VALUE_COUNT
};

/// Server config
enum eConfigFloatValues
{
    CONFIG_FLOAT_RATE_HEALTH = 0,
    CONFIG_FLOAT_RATE_POWER_MANA,
    CONFIG_FLOAT_RATE_POWER_RAGE_INCOME,
    CONFIG_FLOAT_RATE_POWER_RAGE_LOSS,
    CONFIG_FLOAT_RATE_POWER_FOCUS,
    CONFIG_FLOAT_RATE_POWER_ENERGY,
    CONFIG_FLOAT_RATE_SKILL_DISCOVERY,
    CONFIG_FLOAT_RATE_DROP_MONEY,
    CONFIG_FLOAT_RATE_XP_KILL,
    CONFIG_FLOAT_RATE_XP_QUEST,
    CONFIG_FLOAT_RATE_XP_EXPLORE,
    CONFIG_FLOAT_RATE_REPUTATION_GAIN,
    CONFIG_FLOAT_RATE_REPUTATION_LOWLEVEL_KILL,
    CONFIG_FLOAT_RATE_REPUTATION_LOWLEVEL_QUEST,
    CONFIG_FLOAT_RATE_CREATURE_NORMAL_HP,
    CONFIG_FLOAT_RATE_CREATURE_ELITE_ELITE_HP,
    CONFIG_FLOAT_RATE_CREATURE_ELITE_RAREELITE_HP,
    CONFIG_FLOAT_RATE_CREATURE_ELITE_WORLDBOSS_HP,
    CONFIG_FLOAT_RATE_CREATURE_ELITE_RARE_HP,
    CONFIG_FLOAT_RATE_CREATURE_NORMAL_DAMAGE,
    CONFIG_FLOAT_RATE_CREATURE_ELITE_ELITE_DAMAGE,
    CONFIG_FLOAT_RATE_CREATURE_ELITE_RAREELITE_DAMAGE,
    CONFIG_FLOAT_RATE_CREATURE_ELITE_WORLDBOSS_DAMAGE,
    CONFIG_FLOAT_RATE_CREATURE_ELITE_RARE_DAMAGE,
    CONFIG_FLOAT_RATE_CREATURE_NORMAL_SPELLDAMAGE,
    CONFIG_FLOAT_RATE_CREATURE_ELITE_ELITE_SPELLDAMAGE,
    CONFIG_FLOAT_RATE_CREATURE_ELITE_RAREELITE_SPELLDAMAGE,
    CONFIG_FLOAT_RATE_CREATURE_ELITE_WORLDBOSS_SPELLDAMAGE,
    CONFIG_FLOAT_RATE_CREATURE_ELITE_RARE_SPELLDAMAGE,
    CONFIG_FLOAT_RATE_REST_INGAME,
    CONFIG_FLOAT_RATE_REST_OFFLINE_IN_TAVERN_OR_CITY,
    CONFIG_FLOAT_RATE_REST_OFFLINE_IN_WILDERNESS,
    CONFIG_FLOAT_RATE_DAMAGE_FALL,
    CONFIG_FLOAT_RATE_AUCTION_TIME,
    CONFIG_FLOAT_RATE_AUCTION_DEPOSIT,
    CONFIG_FLOAT_RATE_AUCTION_CUT,
    CONFIG_FLOAT_RATE_HONOR,
    CONFIG_FLOAT_RATE_MINING_AMOUNT,
    CONFIG_FLOAT_RATE_MINING_NEXT,
    CONFIG_FLOAT_RATE_TALENT,
    CONFIG_FLOAT_RATE_LOYALTY,
    CONFIG_FLOAT_RATE_INSTANCE_RESET_TIME,
    CONFIG_FLOAT_RATE_GEAR_DURABILITY_LOSS_DAMAGE,
    CONFIG_FLOAT_RATE_WEAP_DURABILITY_LOSS_DAMAGE,
    CONFIG_FLOAT_SIGHT_GUARDER,
    CONFIG_FLOAT_SIGHT_MONSTER,
    CONFIG_FLOAT_LISTEN_RANGE_SAY,
    CONFIG_FLOAT_LISTEN_RANGE_YELL,
    CONFIG_FLOAT_LISTEN_RANGE_TEXTEMOTE,
    CONFIG_FLOAT_CREATURE_FAMILY_FLEE_ASSISTANCE_RADIUS,
    CONFIG_FLOAT_GROUP_XP_DISTANCE,
    CONFIG_FLOAT_GHOST_RUN_SPEED_WORLD,
    CONFIG_FLOAT_GHOST_RUN_SPEED_BG,
    CONFIG_FLOAT_WORLD_DROP_ARMOR_SLOT_CHANCE,
    CONFIG_FLOAT_WORLD_DROP_ARMOR_SLOT_CHANCE_ELITE,
    CONFIG_FLOAT_WORLD_DROP_WEAPON_SLOT_CHANCE,
    CONFIG_FLOAT_WORLD_DROP_WEAPON_SLOT_CHANCE_ELITE,
    CONFIG_FLOAT_WORLD_DROP_EPIC_QUALITY_CHANCE,
    CONFIG_FLOAT_WORLD_DROP_EPIC_QUALITY_CHANCE_ELITE,
    CONFIG_FLOAT_WORLD_DROP_RARE_QUALITY_CHANCE,
    CONFIG_FLOAT_WORLD_DROP_RARE_QUALITY_CHANCE_ELITE,
    CONFIG_FLOAT_WORLD_DROP_UNCOMMON_QUALITY_CHANCE,
    CONFIG_FLOAT_WORLD_DROP_UNCOMMON_QUALITY_CHANCE_ELITE,
    CONFIG_FLOAT_WORLD_DROP_COMMON_QUALITY_CHANCE,
    CONFIG_FLOAT_WORLD_DROP_COMMON_QUALITY_CHANCE_ELITE,
    CONFIG_FLOAT_VALUE_COUNT
};

/// Configuration elements
enum eConfigBoolValues
{
    CONFIG_BOOL_LOAD_DATA_ON_STARTUP = 0,
    CONFIG_BOOL_ALLOW_TWO_SIDE_ACCOUNTS,
    CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_CHAT,
    CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_CHANNEL,
    CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_GROUP,
    CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_GUILD,
    CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_AUCTION,
    CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_MAIL,
    CONFIG_BOOL_ALLOW_TWO_SIDE_WHO_LIST,
    CONFIG_BOOL_ALLOW_TWO_SIDE_ADD_FRIEND,
    CONFIG_BOOL_INSTANCE_IGNORE_LEVEL,
    CONFIG_BOOL_INSTANCE_IGNORE_RAID,
    CONFIG_BOOL_CAST_UNSTUCK,
    CONFIG_BOOL_GM_LOG_TRADE,
    CONFIG_BOOL_GM_LOWER_SECURITY,
    CONFIG_BOOL_SKILL_PROSPECTING,
    CONFIG_BOOL_ALWAYS_MAX_SKILL_FOR_LEVEL,
    CONFIG_BOOL_WEATHER,
    CONFIG_BOOL_EVENT_ANNOUNCE,
    CONFIG_BOOL_QUEST_IGNORE_RAID,
    CONFIG_BOOL_RESTRICTED_LFG_CHANNEL,
    CONFIG_BOOL_SILENTLY_GM_JOIN_TO_CHANNEL,
    CONFIG_BOOL_TALENTS_INSPECTING,
    CONFIG_BOOL_CHAT_FAKE_MESSAGE_PREVENTING,
    CONFIG_BOOL_CHAT_STRICT_LINK_CHECKING_SEVERITY,
    CONFIG_BOOL_CHAT_STRICT_LINK_CHECKING_KICK,
    CONFIG_BOOL_ADDON_CHANNEL,
    CONFIG_BOOL_CORPSE_EMPTY_LOOT_SHOW,
    CONFIG_BOOL_DEATH_CORPSE_RECLAIM_DELAY_PVP,
    CONFIG_BOOL_DEATH_CORPSE_RECLAIM_DELAY_PVE,
    CONFIG_BOOL_DEATH_BONES_WORLD,
    CONFIG_BOOL_DEATH_BONES_BG_OR_ARENA,
    CONFIG_BOOL_ALL_TAXI_PATHS,
    CONFIG_BOOL_DECLINED_NAMES_USED,
    CONFIG_BOOL_SKILL_FAIL_LOOT_FISHING,
    CONFIG_BOOL_SKILL_FAIL_GAIN_FISHING,
    CONFIG_BOOL_SKILL_FAIL_POSSIBLE_FISHINGPOOL,
    CONFIG_BOOL_BATTLEGROUND_CAST_DESERTER,
    CONFIG_BOOL_BATTLEGROUND_QUEUE_ANNOUNCER_START,
    CONFIG_BOOL_ARENA_AUTO_DISTRIBUTE_POINTS,
    CONFIG_BOOL_ARENA_QUEUE_ANNOUNCER_JOIN,
    CONFIG_BOOL_ARENA_QUEUE_ANNOUNCER_EXIT,
    CONFIG_BOOL_KICK_PLAYER_ON_BAD_PACKET,
    CONFIG_BOOL_STATS_SAVE_ONLY_ON_LOGOUT,
    CONFIG_BOOL_CLEAN_CHARACTER_DB,
    CONFIG_BOOL_MMAP_ENABLED,
    CONFIG_BOOL_WARDEN_ENABLED,
    CONFIG_BOOL_OUTDOORPVP_SI_ENABLED,
    CONFIG_BOOL_OUTDOORPVP_EP_ENABLED,
    CONFIG_BOOL_OUTDOORPVP_HP_ENABLED,
    CONFIG_BOOL_OUTDOORPVP_ZM_ENABLED,
    CONFIG_BOOL_OUTDOORPVP_TF_ENABLED,
    CONFIG_BOOL_OUTDOORPVP_NA_ENABLED,
    CONFIG_BOOL_ANTI_CHEAT_KICK_ENABLED,
    CONFIG_BOOL_TICKET_SYSTEM_ENABLED,
    CONFIG_BOOL_VALUE_COUNT
};

/// Can be used in SMSG_AUTH_RESPONSE packet
enum BillingPlanFlags
{
    SESSION_NONE = 0x00,
    SESSION_UNUSED = 0x01,
    SESSION_RECURRING_BILL = 0x02,
    SESSION_FREE_TRIAL = 0x04,
    SESSION_IGR = 0x08,
    SESSION_USAGE = 0x10,
    SESSION_TIME_MIXTURE = 0x20,
    SESSION_RESTRICTED = 0x40,
    SESSION_ENABLE_CAIS = 0x80,
};

/// Type of server, this is values from second column of Cfg_Configs.dbc (1.12.1
/// have another numeration)
enum RealmType
{
    REALM_TYPE_NORMAL = 0,
    REALM_TYPE_PVP = 1,
    REALM_TYPE_NORMAL2 = 4,
    REALM_TYPE_RP = 6,
    REALM_TYPE_RPPVP = 8,
    REALM_TYPE_FFA_PVP = 16 // custom, free for all pvp mode like arena PvP in
                            // all zones except rest activated places and
                            // sanctuaries
                            // replaced by REALM_PVP in realm list
};

/// This is values from first column of Cfg_Categories.dbc (1.12.1 have another
/// numeration)
enum RealmZone
{
    REALM_ZONE_UNKNOWN = 0,        // any language
    REALM_ZONE_DEVELOPMENT = 1,    // any language
    REALM_ZONE_UNITED_STATES = 2,  // extended-Latin
    REALM_ZONE_OCEANIC = 3,        // extended-Latin
    REALM_ZONE_LATIN_AMERICA = 4,  // extended-Latin
    REALM_ZONE_TOURNAMENT_5 = 5,   // basic-Latin at create, any at login
    REALM_ZONE_KOREA = 6,          // East-Asian
    REALM_ZONE_TOURNAMENT_7 = 7,   // basic-Latin at create, any at login
    REALM_ZONE_ENGLISH = 8,        // extended-Latin
    REALM_ZONE_GERMAN = 9,         // extended-Latin
    REALM_ZONE_FRENCH = 10,        // extended-Latin
    REALM_ZONE_SPANISH = 11,       // extended-Latin
    REALM_ZONE_RUSSIAN = 12,       // Cyrillic
    REALM_ZONE_TOURNAMENT_13 = 13, // basic-Latin at create, any at login
    REALM_ZONE_TAIWAN = 14,        // East-Asian
    REALM_ZONE_TOURNAMENT_15 = 15, // basic-Latin at create, any at login
    REALM_ZONE_CHINA = 16,         // East-Asian
    REALM_ZONE_CN1 = 17,           // basic-Latin at create, any at login
    REALM_ZONE_CN2 = 18,           // basic-Latin at create, any at login
    REALM_ZONE_CN3 = 19,           // basic-Latin at create, any at login
    REALM_ZONE_CN4 = 20,           // basic-Latin at create, any at login
    REALM_ZONE_CN5 = 21,           // basic-Latin at create, any at login
    REALM_ZONE_CN6 = 22,           // basic-Latin at create, any at login
    REALM_ZONE_CN7 = 23,           // basic-Latin at create, any at login
    REALM_ZONE_CN8 = 24,           // basic-Latin at create, any at login
    REALM_ZONE_TOURNAMENT_25 = 25, // basic-Latin at create, any at login
    REALM_ZONE_TEST_SERVER = 26,   // any language
    REALM_ZONE_TOURNAMENT_27 = 27, // basic-Latin at create, any at login
    REALM_ZONE_QA_SERVER = 28,     // any language
    REALM_ZONE_CN9 = 29,           // basic-Latin at create, any at login
    REALM_ZONE_TEST_SERVER_2 = 30, // any language
    // in 3.x
    REALM_ZONE_CN10 = 31, // basic-Latin at create, any at login
    REALM_ZONE_CTC = 32,
    REALM_ZONE_CNC = 33,
    REALM_ZONE_CN1_4 = 34,   // basic-Latin at create, any at login
    REALM_ZONE_CN2_6_9 = 35, // basic-Latin at create, any at login
    REALM_ZONE_CN3_7 = 36,   // basic-Latin at create, any at login
    REALM_ZONE_CN5_8 = 37    // basic-Latin at create, any at login
};

// CLI commands to be executed by the class cli_cmd_handler
class cli_command
{
public:
    typedef boost::function<void(const std::string& str)>
        completion_handler_type;

    cli_command(uint32 acc_id, AccountTypes access, std::string command,
        completion_handler_type completion)
      : account_id_(acc_id), access_level_(access),
        command_(std::move(command)), completion_handler_(std::move(completion))
    {
    }

    const char* command() const { return command_.c_str(); }
    uint32 acc_id() const { return account_id_; }
    AccountTypes access() const { return access_level_; }

    void print(const std::string& str) { print_str_ += str + "\n"; }
    void complete() { completion_handler_(print_str_); }

private:
    uint32 account_id_;
    AccountTypes access_level_;
    std::string command_;
    std::string print_str_;
    completion_handler_type completion_handler_;
};

inline cli_command* make_cli_command(uint32 acc_id, AccountTypes access,
    const std::string& command, cli_command::completion_handler_type completion)
{
    return new cli_command(acc_id, access, command, completion);
}

/// The World
class World
{
public:
    typedef std::unordered_map<uint32, std::shared_ptr<WorldSession>>
        SessionMap;

    static volatile uint32 m_worldLoopCounter;

    static bool batch_ready[(int)BatchUpdates::count];

    World();
    ~World();

    WorldSession* FindSession(uint32 id) const;
    void AddSession(std::shared_ptr<WorldSession> s);
    bool RemoveSession(uint32 id);
    /// Get the number of current active sessions
    void UpdateMaxSessionCounters();
    const SessionMap& GetAllSessions() const { return m_sessions; }
    uint32 GetActiveAndQueuedSessionCount() const { return m_sessions.size(); }
    uint32 GetActiveSessionCount() const
    {
        return m_sessions.size() - m_QueuedSessions.size();
    }
    uint32 GetQueuedSessionCount() const { return m_QueuedSessions.size(); }
    /// Get the maximum number of parallel sessions on the server since last
    /// reboot
    uint32 GetMaxQueuedSessionCount() const { return m_maxQueuedSessionCount; }
    uint32 GetMaxActiveSessionCount() const { return m_maxActiveSessionCount; }
    Player* FindPlayerInZone(uint32 zone);

    Weather* FindWeather(uint32 id) const;
    Weather* AddWeather(uint32 zone_id);
    void RemoveWeather(uint32 zone_id);

    /// Get the active session server limit (or security level limitations)
    uint32 GetPlayerAmountLimit() const
    {
        return m_playerLimit >= 0 ? m_playerLimit : 0;
    }
    AccountTypes GetPlayerSecurityLimit() const
    {
        return m_playerLimit <= 0 ? AccountTypes(-m_playerLimit) : SEC_PLAYER;
    }

    /// Set the active session server limit (or security level limitation)
    void SetPlayerLimit(int32 limit, bool needUpdate = false);

    // player Queue
    typedef std::list<std::shared_ptr<WorldSession>> Queue;
    void AddQueuedSession(std::shared_ptr<WorldSession> session);
    bool RemoveQueuedSession(WorldSession* session);
    int32 GetQueuedSessionPos(WorldSession* session);

    /// \todo Actions on m_allowMovement still to be implemented
    /// Is movement allowed?
    bool getAllowMovement() const { return m_allowMovement; }
    /// Allow/Disallow object movements
    void SetAllowMovement(bool allow) { m_allowMovement = allow; }

    /// Set a new Message of the Day
    void SetMotd(const std::string& motd) { m_motd = motd; }
    /// Get the current Message of the Day
    const char* GetMotd() const { return m_motd.c_str(); }

    LocaleConstant GetDefaultDbcLocale() const { return m_defaultDbcLocale; }

    /// Get the path where data (dbc, maps) are stored on disk
    std::string GetDataPath() const { return m_dataPath; }

    /// When server started?
    time_t const& GetStartTime() const { return m_startTime; }
    /// Uptime (in secs)
    uint32 GetUptime() const
    {
        return uint32(WorldTimer::curr_sys_time - m_startTime);
    }
    /// Next daily quests reset time
    time_t GetNextDailyQuestsResetTime() const { return m_NextDailyQuestReset; }

    /// Get the maximum skill level a player can reach
    uint16 GetConfigMaxSkillValue() const
    {
        uint32 lvl = getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL);
        return lvl > 60 ? 300 + ((lvl - 60) * 75) / 10 : lvl * 5;
    }

    void SetInitialWorldSettings();
    void LoadConfigSettings(bool reload = false);

    void SendWorldText(int32 string_id, ...);
    void SendGlobalText(const char* text, WorldSession* self);
    void SendGlobalMessage(
        WorldPacket* packet, WorldSession* self = nullptr, uint32 team = 0);
    void SendZoneMessage(uint32 zone, WorldPacket* packet,
        WorldSession* self = nullptr, uint32 team = 0);
    void SendZoneText(uint32 zone, const char* text,
        WorldSession* self = nullptr, uint32 team = 0);
    void SendServerMessage(ServerMessageType type, const char* text = "",
        Player* player = nullptr);

    /// Are we in the middle of a shutdown?
    bool IsShutdowning() const { return m_ShutdownTimer > 0; }
    void ShutdownServ(uint32 time, uint32 options, uint8 exitcode);
    void ShutdownCancel();
    void ShutdownMsg(bool show = false, Player* player = nullptr);
    static uint8 GetExitCode() { return m_ExitCode; }
    static void StopNow(uint8 exitcode)
    {
        m_stopEvent = true;
        m_ExitCode = exitcode;
    }
    static bool IsStopped() { return m_stopEvent; }

    void Update(uint32 diff);

    void UpdateSessions(uint32 diff);

    /// Get a server configuration element (see #eConfigFloatValues)
    void setConfig(eConfigFloatValues index, float value)
    {
        m_configFloatValues[index] = value;
    }
    /// Get a server configuration element (see #eConfigFloatValues)
    float getConfig(eConfigFloatValues rate) const
    {
        return m_configFloatValues[rate];
    }

    /// Set a server configuration element (see #eConfigUInt32Values)
    void setConfig(eConfigUInt32Values index, uint32 value)
    {
        m_configUint32Values[index] = value;
    }
    /// Get a server configuration element (see #eConfigUInt32Values)
    uint32 getConfig(eConfigUInt32Values index) const
    {
        return m_configUint32Values[index];
    }

    /// Set a server configuration element (see #eConfigInt32Values)
    void setConfig(eConfigInt32Values index, int32 value)
    {
        m_configInt32Values[index] = value;
    }
    /// Get a server configuration element (see #eConfigInt32Values)
    int32 getConfig(eConfigInt32Values index) const
    {
        return m_configInt32Values[index];
    }

    /// Set a server configuration element (see #eConfigBoolValues)
    void setConfig(eConfigBoolValues index, bool value)
    {
        m_configBoolValues[index] = value;
    }
    /// Get a server configuration element (see #eConfigBoolValues)
    bool getConfig(eConfigBoolValues index) const
    {
        return m_configBoolValues[index];
    }

    /// Are we on a "Player versus Player" server?
    bool IsPvPRealm()
    {
        return (getConfig(CONFIG_UINT32_GAME_TYPE) == REALM_TYPE_PVP ||
                getConfig(CONFIG_UINT32_GAME_TYPE) == REALM_TYPE_RPPVP ||
                getConfig(CONFIG_UINT32_GAME_TYPE) == REALM_TYPE_FFA_PVP);
    }
    bool IsFFAPvPRealm()
    {
        return getConfig(CONFIG_UINT32_GAME_TYPE) == REALM_TYPE_FFA_PVP;
    }

    void KickAll();
    void KickAllLess(AccountTypes sec);
    BanReturn BanAccount(BanMode mode, std::string nameOrIP,
        uint32 duration_secs, std::string reason, std::string author);
    bool RemoveBanAccount(BanMode mode, std::string nameOrIP);

    // for max speed access
    static float GetMaxVisibleDistanceOnContinents()
    {
        return m_MaxVisibleDistanceOnContinents;
    }
    static float GetMaxVisibleDistanceInInstances()
    {
        return m_MaxVisibleDistanceInInstances;
    }
    static float GetMaxVisibleDistanceInBGArenas()
    {
        return m_MaxVisibleDistanceInBGArenas;
    }

    static float GetMaxVisibleDistanceInFlight()
    {
        return m_MaxVisibleDistanceInFlight;
    }
    static float GetVisibleUnitGreyDistance()
    {
        return m_VisibleUnitGreyDistance;
    }
    static float GetVisibleObjectGreyDistance()
    {
        return m_VisibleObjectGreyDistance;
    }

    static float GetRelocationLowerLimit() { return m_relocation_lower_limit; }
    static float GetRelocationLowerLimitSq()
    {
        return m_relocation_lower_limit_sq;
    }
    static uint32 GetRelocationAINotifyDelay()
    {
        return m_relocation_ai_notify_delay;
    }

    void process_cli_commands();
    void queue_cli_command(cli_command* command)
    {
        cli_cmd_queue.push(command);
    }

    void UpdateResultQueue();
    void InitResultQueue();

    void UpdateRealmCharCount(uint32 accid);

    LocaleConstant GetAvailableDbcLocale(LocaleConstant locale) const
    {
        if (m_availableDbcLocaleMask & (1 << locale))
            return locale;
        else
            return m_defaultDbcLocale;
    }

    bool debugging_warden() const { return warden_debug_; }
    // Only toggle debugging in single-threaded environment
    void set_warden_debug(bool on) { warden_debug_ = on; }

    //! Adds a user to the list of recently logged out users for purposes of
    // skipping the queue when logging back in.
    void add_recently_logged_off(uint32 accountId);

    //! Remove expired recent logouts
    void prune_recently_logged_off();

    //! Checks the recently logged off users and returns whether the user is
    // allowed to skip the login queue.
    bool may_skip_queue(uint32 accountId);

    //! Checks the pending queue for players waiting and releases them if
    // possible
    void check_and_release_queue(bool found);

    void SendDefenseMessage(uint32 zoneId, int32 textId);

    // Defensive mode means the server only permitts accounts to login that have
    // a
    // character that is at least this level. This limit serves as a "trusted"
    // threshold
    // in the event of an attack.
    void defmode_level(uint32 level); // This function should only be called in
                                      // a single-threaded scenario,
    // such as the ChatHandler for processing GM commands
    uint32 defmode_level() const { return defensive_mode_level_; }
    void defmode_stats(uint32 add_kicked, uint32 add_let_through);
    std::pair<uint32, uint32> defmode_stats() const;

    // add a spell that was still referenced (& therefore not deletable) for
    // later cleanup
    void backburn_spell(Spell* spell) { spell_backburner_.push_back(spell); }

protected:
    void _UpdateGameTime();
    // callback for UpdateRealmCharacters
    void _UpdateRealmCharCount(QueryResult* resultCharCount, uint32 accountId);

    void InitDailyQuestResetTime();
    void ResetDailyQuests();

private:
    void setConfig(
        eConfigUInt32Values index, char const* fieldname, uint32 defvalue);
    void setConfig(
        eConfigInt32Values index, char const* fieldname, int32 defvalue);
    void setConfig(
        eConfigFloatValues index, char const* fieldname, float defvalue);
    void setConfig(
        eConfigBoolValues index, char const* fieldname, bool defvalue);
    void setConfigPos(
        eConfigFloatValues index, char const* fieldname, float defvalue);
    void setConfigMin(eConfigUInt32Values index, char const* fieldname,
        uint32 defvalue, uint32 minvalue);
    void setConfigMin(eConfigInt32Values index, char const* fieldname,
        int32 defvalue, int32 minvalue);
    void setConfigMin(eConfigFloatValues index, char const* fieldname,
        float defvalue, float minvalue);
    void setConfigMinMax(eConfigUInt32Values index, char const* fieldname,
        uint32 defvalue, uint32 minvalue, uint32 maxvalue);
    void setConfigMinMax(eConfigInt32Values index, char const* fieldname,
        int32 defvalue, int32 minvalue, int32 maxvalue);
    void setConfigMinMax(eConfigFloatValues index, char const* fieldname,
        float defvalue, float minvalue, float maxvalue);
    bool configNoReload(bool reload, eConfigUInt32Values index,
        char const* fieldname, uint32 defvalue);
    bool configNoReload(bool reload, eConfigInt32Values index,
        char const* fieldname, int32 defvalue);
    bool configNoReload(bool reload, eConfigFloatValues index,
        char const* fieldname, float defvalue);
    bool configNoReload(bool reload, eConfigBoolValues index,
        char const* fieldname, bool defvalue);

    static volatile bool m_stopEvent;
    static uint8 m_ExitCode;
    uint32 m_ShutdownTimer;
    uint32 m_ShutdownMask;

    time_t m_startTime;
    IntervalTimer m_timers[WUPDATE_COUNT];
    uint32 mail_timer;
    uint32 mail_timer_expires;

    time_t m_lastFacPlCountTime;

    typedef std::unordered_map<uint32, Weather*> WeatherMap;
    WeatherMap m_weathers;
    SessionMap m_sessions;
    // session with a player still in-game
    std::vector<std::shared_ptr<WorldSession>> expired_sessions_;
    uint32 m_maxActiveSessionCount;
    uint32 m_maxQueuedSessionCount;

    uint32 m_configUint32Values[CONFIG_UINT32_VALUE_COUNT];
    int32 m_configInt32Values[CONFIG_INT32_VALUE_COUNT];
    float m_configFloatValues[CONFIG_FLOAT_VALUE_COUNT];
    bool m_configBoolValues[CONFIG_BOOL_VALUE_COUNT];

    int32 m_playerLimit;
    LocaleConstant
        m_defaultDbcLocale; // from config for one from loaded DBC locales
    uint32 m_availableDbcLocaleMask; // by loaded DBC
    void DetectDBCLang();
    bool m_allowMovement;
    std::string m_motd;
    std::string m_dataPath;

    // for max speed access
    static float m_MaxVisibleDistanceOnContinents;
    static float m_MaxVisibleDistanceInInstances;
    static float m_MaxVisibleDistanceInBGArenas;

    static float m_MaxVisibleDistanceInFlight;
    static float m_VisibleUnitGreyDistance;
    static float m_VisibleObjectGreyDistance;

    static float m_relocation_lower_limit;
    static float m_relocation_lower_limit_sq;
    static uint32 m_relocation_ai_notify_delay;

    // CLI command holder to be thread safe
    MaNGOS::locked_queue<cli_command*> cli_cmd_queue;

    // next daily quests reset time
    time_t m_NextDailyQuestReset;

    // Player Queue
    Queue m_QueuedSessions;

    // sessions that are added async
    void AddSession_(std::shared_ptr<WorldSession> s);
    MaNGOS::locked_queue<std::shared_ptr<WorldSession>> addSessQueue;

    bool warden_debug_;

    //! A mutex for the recently logged off map
    std::mutex recently_loggedoff_mutex_;
    //! A list of recently logged out accounts, used to allow players to
    // reconnect without going through the login queue again.
    std::map<uint32, time_t> recently_loggedoff_accts_;
    //! Time it takes us to consider a disconnected player to have lost their
    // spot in the queue.
    uint32 recent_logoff_threshold_;

    // Defensive mode lowest accepted level, 0 if defensive mode is turned off
    void defmode_gm_reminder();
    uint32 defmode_last_reminder_;
    uint32 defensive_mode_level_;
    mutable std::mutex defmode_stats_mutex_;
    uint32 defensive_mode_kicked_;
    uint32 defensive_mode_accepted_;

    std::vector<Spell*> spell_backburner_;
};

extern uint32 realmID;

#define sWorld MaNGOS::UnlockedSingleton<World>
#endif
/// @}
