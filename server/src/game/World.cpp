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

/** \file
    \ingroup world
*/

#include "World.h"
#include "AccountMgr.h"
#include "AuctionHouseMgr.h"
#include "BattleGroundMgr.h"
#include "BehavioralAI.h"
#include "CharacterDatabaseCleaner.h"
#include "Chat.h"
#include "ConditionMgr.h"
#include "CreatureEventAIMgr.h"
#include "CreatureGroupMgr.h"
#include "CreatureTextMgr.h"
#include "DBCStores.h"
#include "FirstKills.h"
#include "GMTicketMgr.h"
#include "GameEventMgr.h"
#include "GuildMgr.h"
#include "ItemEnchantmentMgr.h"
#include "logging.h"
#include "LootMgr.h"
#include "MapManager.h"
#include "MapPersistentStateMgr.h"
#include "MassMailMgr.h"
#include "MoveMap.h"
#include "ObjectMgr.h"
#include "Opcodes.h"
#include "Player.h"
#include "PoolManager.h"
#include "ScriptMgr.h"
#include "SkillDiscovery.h"
#include "SkillExtraItems.h"
#include "SmartScriptMgr.h"
#include "SpellMgr.h"
#include "SystemConfig.h"
#include "TemporarySummon.h"
#include "TransportMgr.h"
#include "Util.h"
#include "VMapFactory.h"
#include "WardenDataStorage.h"
#include "Weather.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "action_limit.h"
#include "ban_wave.h"
#include "loot_selection.h"
#include "netstat.h"
#include "pet_template.h"
#include "Config/Config.h"
#include "Database/DatabaseEnv.h"
#include "Database/DatabaseImpl.h"
#include "maps/callbacks.h"
#include "movement/WaypointManager.h"
#include "OutdoorPvP/OutdoorPvP.h"
#include "Platform/Define.h"
#include "Policies/Singleton.h"

volatile bool World::m_stopEvent = false;
uint8 World::m_ExitCode = SHUTDOWN_EXIT_CODE;
volatile uint32 World::m_worldLoopCounter = 0;

bool World::batch_ready[(int)BatchUpdates::count] = {false};

float World::m_MaxVisibleDistanceOnContinents = DEFAULT_VISIBILITY_DISTANCE;
float World::m_MaxVisibleDistanceInInstances = DEFAULT_VISIBILITY_INSTANCE;
float World::m_MaxVisibleDistanceInBGArenas = DEFAULT_VISIBILITY_BGARENAS;

float World::m_MaxVisibleDistanceInFlight = DEFAULT_VISIBILITY_DISTANCE;
float World::m_VisibleUnitGreyDistance = 0;
float World::m_VisibleObjectGreyDistance = 0;

float World::m_relocation_lower_limit = 10.f;
float World::m_relocation_lower_limit_sq = 10.f * 10.f;
uint32 World::m_relocation_ai_notify_delay = 1000u;

#define DEFMODE_REMINDER_TIME 60000

/// World constructor
World::World()
{
    m_playerLimit = 0;
    m_allowMovement = true;
    m_ShutdownMask = 0;
    m_ShutdownTimer = 0;
    WorldTimer::curr_sys_time = time(nullptr);
    m_lastFacPlCountTime = m_startTime = WorldTimer::time_no_syscall();
    m_maxActiveSessionCount = 0;
    m_maxQueuedSessionCount = 0;
    m_NextDailyQuestReset = 0;

    // Time it takes us to consider a disconnected player to have lost their
    // spot in the queue.
    recent_logoff_threshold_ =
        (sConfig::Instance()->GetIntDefault("RecentLogoffThreshold", 5) * 60);

    m_defaultDbcLocale = LOCALE_enUS;
    m_availableDbcLocaleMask = 0;

    warden_debug_ = false;

    for (auto& elem : m_configUint32Values)
        elem = 0;

    for (auto& elem : m_configInt32Values)
        elem = 0;

    for (auto& elem : m_configFloatValues)
        elem = 0.0f;

    for (auto& elem : m_configBoolValues)
        elem = false;

    defensive_mode_level_ = 0;
    defensive_mode_kicked_ = 0;
    defensive_mode_accepted_ = 0;
    defmode_last_reminder_ = 0;
}

/// World destructor
World::~World()
{
    ///- Empty the kicked session set
    while (!m_sessions.empty())
    {
        // not remove from queue, prevent loading new sessions
        m_sessions.erase(m_sessions.begin());
    }

    ///- Empty the WeatherMap
    for (WeatherMap::const_iterator itr = m_weathers.begin();
         itr != m_weathers.end(); ++itr)
        delete itr->second;

    m_weathers.clear();

    cli_command* command;
    while (cli_cmd_queue.pop(command))
        delete command;

    VMAP::VMapFactory::clear();
    MMAP::MMapFactory::clear();
}

/// Find a player in a specified zone
Player* World::FindPlayerInZone(uint32 zone)
{
    ///- circle through active sessions and return the first player found in the
    /// zone
    SessionMap::const_iterator itr;
    for (itr = m_sessions.begin(); itr != m_sessions.end(); ++itr)
    {
        if (!itr->second)
            continue;
        Player* player = itr->second->GetPlayer();
        if (!player)
            continue;
        if (player->IsInWorld() && player->GetZoneId() == zone)
        {
            // Used by the weather system. We return the player to broadcast the
            // change weather message to him and all players in the zone.
            return player;
        }
    }
    return nullptr;
}

/// Find a session by its id
WorldSession* World::FindSession(uint32 id) const
{
    auto itr = m_sessions.find(id);

    if (itr != m_sessions.end())
        return itr->second.get(); // also can return NULL for kicked session
    else
        return nullptr;
}

/// Remove a given session
bool World::RemoveSession(uint32 id)
{
    auto itr = m_sessions.find(id);

    if (itr != m_sessions.end() && itr->second)
    {
        if (itr->second->PlayerLoading())
            return false;
        itr->second->DisconnectPlayer();
        if (itr->second->GetPlayer() && itr->second->GetPlayer()->IsInWorld())
            expired_sessions_.push_back(itr->second);
        else
            itr->second->LogoutPlayer(true);
        m_sessions.erase(itr);
    }

    return true;
}

void World::AddSession(std::shared_ptr<WorldSession> s)
{
    addSessQueue.push(std::move(s));
}

void World::AddSession_(std::shared_ptr<WorldSession> s)
{
    assert(s);

    // NOTE - Still there is race condition in WorldSession* being used in the
    // Sockets

    ///- kick already loaded player with same account (if any) and remove
    /// session
    ///- if player is in loading and want to load again, return
    if (!RemoveSession(s->GetAccountId()))
    {
        s->KickPlayer();
        return;
    }

    // decrease session counts only at not reconnection case
    bool decrease_session = true;

    // if session already exist, prepare to it deleting at next world update
    // NOTE - KickPlayer() should be called on "old" in RemoveSession()
    {
        SessionMap::const_iterator old = m_sessions.find(s->GetAccountId());

        if (old != m_sessions.end())
        {
            // prevent decrease sessions count if session queued
            if (RemoveQueuedSession(old->second.get()))
                decrease_session = false;
        }
    }

    // MUST call this before adding recently_loggedoff_accts_.size() to
    // Sessions, below!!!
    prune_recently_logged_off();

    m_sessions[s->GetAccountId()] = s;

    uint32 Sessions =
        (GetActiveAndQueuedSessionCount() + recently_loggedoff_accts_.size());
    uint32 pLimit = GetPlayerAmountLimit();

    // so we don't count the user trying to
    // login as a session and queue the socket that we are using
    if (decrease_session)
        --Sessions;

    if (pLimit > 0 && Sessions >= pLimit && s->GetSecurity() == SEC_PLAYER &&
        !may_skip_queue(s->GetAccountId()))
    {
        AddQueuedSession(s);
        UpdateMaxSessionCounters();
        LOG_DEBUG(logging,
            "PlayerQueue: Account id %u is in Queue Position (%u).",
            s->GetAccountId(), GetQueuedSessionCount() + 1);
        return;
    }

    WorldPacket packet(SMSG_AUTH_RESPONSE, 1 + 4 + 1 + 4 + 1);
    packet << uint8(AUTH_OK);
    packet << uint32(0);             // BillingTimeRemaining
    packet << uint8(0);              // BillingPlanFlags
    packet << uint32(0);             // BillingTimeRested
    packet << uint8(s->Expansion()); // 0 - normal, 1 - TBC. Must be set in
                                     // database manually for each account.
    s->send_packet(std::move(packet));

    UpdateMaxSessionCounters();

    // Updates the population
    if (pLimit > 0)
    {
        float popu = float(
            GetActiveSessionCount()); // updated number of users on the server
        popu /= pLimit;
        popu *= 2;

        static SqlStatementID id;

        SqlStatement stmt = LoginDatabase.CreateStatement(
            id, "UPDATE realmlist SET population = ? WHERE id = ?");
        stmt.PExecute(popu, realmID);

        LOG_DEBUG(logging, "Server Population (%f).", popu);
    }
}

int32 World::GetQueuedSessionPos(WorldSession* sess)
{
    uint32 position = 1;

    for (Queue::const_iterator iter = m_QueuedSessions.begin();
         iter != m_QueuedSessions.end(); ++iter, ++position)
        if (iter->get() == sess)
            return position;

    return 0;
}

void World::AddQueuedSession(std::shared_ptr<WorldSession> sess)
{
    sess->SetInQueue(true);

    // The 1st SMSG_AUTH_RESPONSE needs to contain other info too.
    WorldPacket packet(SMSG_AUTH_RESPONSE, 1 + 4 + 1 + 4 + 1 + 4);
    packet << uint8(AUTH_WAIT_QUEUE);
    packet << uint32(0);                // BillingTimeRemaining
    packet << uint8(0);                 // BillingPlanFlags
    packet << uint32(0);                // BillingTimeRested
    packet << uint8(sess->Expansion()); // 0 - normal, 1 - TBC, must be set in
                                        // database manually for each account
    packet << uint32(GetQueuedSessionPos(sess.get())); // position in queue
    sess->send_packet(std::move(packet));

    m_QueuedSessions.push_back(std::move(sess));
}

bool World::RemoveQueuedSession(WorldSession* sess)
{
    uint32 position = 1;
    auto iter = m_QueuedSessions.begin();

    // search to remove and count skipped positions
    bool found = false;

    for (; iter != m_QueuedSessions.end(); ++iter, ++position)
    {
        if (iter->get() == sess)
        {
            sess->SetInQueue(false);
            iter = m_QueuedSessions.erase(iter);
            found = true; // removing queued session
            break;
        }
    }

    check_and_release_queue(found);

    return found;
}

void World::check_and_release_queue(bool found)
{
    // Before adding recently_loggedoff_accts_.size() below!
    prune_recently_logged_off();

    // MUST lock after prune_recently_logged_off(), as it locks the same mutex
    std::lock_guard<std::mutex> guard(recently_loggedoff_mutex_);

    // sessions count including queued to remove (if removed_session set)
    uint32 sessions =
        (GetActiveSessionCount() + recently_loggedoff_accts_.size());

    // if session not queued then we need decrease sessions count
    if (!found && sessions)
        --sessions;

    uint32 position = 1;

    if ((!m_playerLimit || (int32)sessions < m_playerLimit) &&
        !m_QueuedSessions.empty())
    {
        m_QueuedSessions.front()->SetInQueue(false);
        m_QueuedSessions.front()->SendAuthWaitQue(0);
        m_QueuedSessions.pop_front();
    }

    // update position from iter to end()
    // iter point to first not updated socket, position store new position
    for (auto iter = m_QueuedSessions.begin(); iter != m_QueuedSessions.end();
         ++iter, ++position)
        (*iter)->SendAuthWaitQue(position);
}

/// Find a Weather object by the given zoneid
Weather* World::FindWeather(uint32 id) const
{
    auto itr = m_weathers.find(id);

    if (itr != m_weathers.end())
        return itr->second;
    else
        return nullptr;
}

/// Remove a Weather object for the given zoneid
void World::RemoveWeather(uint32 id)
{
    // not called at the moment. Kept for completeness
    auto itr = m_weathers.find(id);

    if (itr != m_weathers.end())
    {
        delete itr->second;
        m_weathers.erase(itr);
    }
}

/// Add a Weather object to the list
Weather* World::AddWeather(uint32 zone_id)
{
    WeatherZoneChances const* weatherChances =
        sObjectMgr::Instance()->GetWeatherChances(zone_id);

    // zone not have weather, ignore
    if (!weatherChances)
        return nullptr;

    auto w = new Weather(zone_id, weatherChances);
    m_weathers[w->GetZone()] = w;
    w->ReGenerate();
    w->UpdateWeather();
    return w;
}

/// Initialize config values
void World::LoadConfigSettings(bool reload)
{
    ///- Read the version of the configuration file and warn the user in case of
    /// emptiness or mismatch
    uint32 confVersion = sConfig::Instance()->GetIntDefault("ConfVersion", 0);
    if (!confVersion)
    {
        logging.error("No ConfVersion field found in config file.");

        exit(1);
    }
    else
    {
        if (confVersion < _MANGOSDCONFVERSION)
        {
            logging.error("Config is out of date, please update it!");

            exit(1);
        }
    }

    ///- Read the player limit and the Message of the day from the config file
    SetPlayerLimit(
        sConfig::Instance()->GetIntDefault("PlayerLimit", DEFAULT_PLAYER_LIMIT),
        true);
    SetMotd(sConfig::Instance()->GetStringDefault(
        "Motd", "Welcome to the Massive Network Game Object Server."));

    ///- Read all rates from the config file
    setConfigPos(CONFIG_FLOAT_RATE_HEALTH, "Rate.Health", 1.0f);
    setConfigPos(CONFIG_FLOAT_RATE_POWER_MANA, "Rate.Mana", 1.0f);
    setConfig(CONFIG_FLOAT_RATE_POWER_RAGE_INCOME, "Rate.Rage.Income", 1.0f);
    setConfigPos(CONFIG_FLOAT_RATE_POWER_RAGE_LOSS, "Rate.Rage.Loss", 1.0f);
    setConfig(CONFIG_FLOAT_RATE_POWER_FOCUS, "Rate.Focus", 1.0f);
    setConfigPos(CONFIG_FLOAT_RATE_LOYALTY, "Rate.Loyalty", 1.0f);
    setConfig(CONFIG_FLOAT_RATE_POWER_ENERGY, "Rate.Energy", 1.0f);
    setConfigPos(
        CONFIG_FLOAT_RATE_SKILL_DISCOVERY, "Rate.Skill.Discovery", 1.0f);
    setConfigPos(CONFIG_FLOAT_RATE_DROP_MONEY, "Rate.Drop.Money", 1.0f);
    setConfig(CONFIG_FLOAT_RATE_XP_KILL, "Rate.XP.Kill", 1.0f);
    setConfig(CONFIG_FLOAT_RATE_XP_QUEST, "Rate.XP.Quest", 1.0f);
    setConfig(CONFIG_FLOAT_RATE_XP_EXPLORE, "Rate.XP.Explore", 1.0f);
    setConfig(CONFIG_FLOAT_RATE_REPUTATION_GAIN, "Rate.Reputation.Gain", 1.0f);
    setConfig(CONFIG_FLOAT_RATE_REPUTATION_LOWLEVEL_KILL,
        "Rate.Reputation.LowLevel.Kill", 1.0f);
    setConfig(CONFIG_FLOAT_RATE_REPUTATION_LOWLEVEL_QUEST,
        "Rate.Reputation.LowLevel.Quest", 1.0f);
    setConfigPos(CONFIG_FLOAT_RATE_CREATURE_NORMAL_DAMAGE,
        "Rate.Creature.Normal.Damage", 1.0f);
    setConfigPos(CONFIG_FLOAT_RATE_CREATURE_ELITE_ELITE_DAMAGE,
        "Rate.Creature.Elite.Elite.Damage", 1.0f);
    setConfigPos(CONFIG_FLOAT_RATE_CREATURE_ELITE_RAREELITE_DAMAGE,
        "Rate.Creature.Elite.RAREELITE.Damage", 1.0f);
    setConfigPos(CONFIG_FLOAT_RATE_CREATURE_ELITE_WORLDBOSS_DAMAGE,
        "Rate.Creature.Elite.WORLDBOSS.Damage", 1.0f);
    setConfigPos(CONFIG_FLOAT_RATE_CREATURE_ELITE_RARE_DAMAGE,
        "Rate.Creature.Elite.RARE.Damage", 1.0f);
    setConfigPos(
        CONFIG_FLOAT_RATE_CREATURE_NORMAL_HP, "Rate.Creature.Normal.HP", 1.0f);
    setConfigPos(CONFIG_FLOAT_RATE_CREATURE_ELITE_ELITE_HP,
        "Rate.Creature.Elite.Elite.HP", 1.0f);
    setConfigPos(CONFIG_FLOAT_RATE_CREATURE_ELITE_RAREELITE_HP,
        "Rate.Creature.Elite.RAREELITE.HP", 1.0f);
    setConfigPos(CONFIG_FLOAT_RATE_CREATURE_ELITE_WORLDBOSS_HP,
        "Rate.Creature.Elite.WORLDBOSS.HP", 1.0f);
    setConfigPos(CONFIG_FLOAT_RATE_CREATURE_ELITE_RARE_HP,
        "Rate.Creature.Elite.RARE.HP", 1.0f);
    setConfigPos(CONFIG_FLOAT_RATE_CREATURE_NORMAL_SPELLDAMAGE,
        "Rate.Creature.Normal.SpellDamage", 1.0f);
    setConfigPos(CONFIG_FLOAT_RATE_CREATURE_ELITE_ELITE_SPELLDAMAGE,
        "Rate.Creature.Elite.Elite.SpellDamage", 1.0f);
    setConfigPos(CONFIG_FLOAT_RATE_CREATURE_ELITE_RAREELITE_SPELLDAMAGE,
        "Rate.Creature.Elite.RAREELITE.SpellDamage", 1.0f);
    setConfigPos(CONFIG_FLOAT_RATE_CREATURE_ELITE_WORLDBOSS_SPELLDAMAGE,
        "Rate.Creature.Elite.WORLDBOSS.SpellDamage", 1.0f);
    setConfigPos(CONFIG_FLOAT_RATE_CREATURE_ELITE_RARE_SPELLDAMAGE,
        "Rate.Creature.Elite.RARE.SpellDamage", 1.0f);
    setConfig(CONFIG_FLOAT_RATE_REST_INGAME, "Rate.Rest.InGame", 1.0f);
    setConfig(CONFIG_FLOAT_RATE_REST_OFFLINE_IN_TAVERN_OR_CITY,
        "Rate.Rest.Offline.InTavernOrCity", 1.0f);
    setConfig(CONFIG_FLOAT_RATE_REST_OFFLINE_IN_WILDERNESS,
        "Rate.Rest.Offline.InWilderness", 1.0f);
    setConfigPos(CONFIG_FLOAT_RATE_DAMAGE_FALL, "Rate.Damage.Fall", 1.0f);
    setConfigPos(CONFIG_FLOAT_RATE_AUCTION_TIME, "Rate.Auction.Time", 1.0f);
    setConfig(CONFIG_FLOAT_RATE_AUCTION_DEPOSIT, "Rate.Auction.Deposit", 1.0f);
    setConfig(CONFIG_FLOAT_RATE_AUCTION_CUT, "Rate.Auction.Cut", 1.0f);
    setConfig(CONFIG_UINT32_AUCTION_DEPOSIT_MIN, "Auction.Deposit.Min", 0);
    setConfig(CONFIG_FLOAT_RATE_HONOR, "Rate.Honor", 1.0f);
    setConfigPos(CONFIG_FLOAT_RATE_MINING_AMOUNT, "Rate.Mining.Amount", 1.0f);
    setConfigPos(CONFIG_FLOAT_RATE_MINING_NEXT, "Rate.Mining.Next", 1.0f);
    setConfigPos(
        CONFIG_FLOAT_RATE_INSTANCE_RESET_TIME, "Rate.InstanceResetTime", 1.0f);
    setConfigPos(CONFIG_FLOAT_RATE_TALENT, "Rate.Talent", 1.0f);

    setConfigPos(CONFIG_FLOAT_RATE_GEAR_DURABILITY_LOSS_DAMAGE,
        "DurabilityLoss.GearDamageChance", 0.03f);
    setConfigPos(CONFIG_FLOAT_RATE_WEAP_DURABILITY_LOSS_DAMAGE,
        "DurabilityLoss.WeapDamageChance", 0.015f);
    setConfig(CONFIG_UINT32_DURABILITY_LOSS_COMBAT_MIN,
        "DurabilityLoss.CombatMin", 30);
    setConfig(CONFIG_UINT32_DURABILITY_LOSS_COMBAT_MAX,
        "DurabilityLoss.CombatMax", 70);

    setConfigPos(CONFIG_FLOAT_LISTEN_RANGE_SAY, "ListenRange.Say", 40.0f);
    setConfigPos(CONFIG_FLOAT_LISTEN_RANGE_YELL, "ListenRange.Yell", 200.0f);
    setConfigPos(
        CONFIG_FLOAT_LISTEN_RANGE_TEXTEMOTE, "ListenRange.TextEmote", 40.0f);

    setConfigPos(CONFIG_FLOAT_GROUP_XP_DISTANCE, "MaxGroupXPDistance", 74.0f);
    setConfigPos(CONFIG_FLOAT_SIGHT_GUARDER, "GuarderSight", 50.0f);
    setConfigPos(CONFIG_FLOAT_SIGHT_MONSTER, "MonsterSight", 50.0f);

    setConfigPos(CONFIG_FLOAT_CREATURE_FAMILY_FLEE_ASSISTANCE_RADIUS,
        "CreatureFamilyFleeAssistanceRadius", 30.0f);

    ///- Read other configuration items from the config file
    setConfigMinMax(CONFIG_UINT32_COMPRESSION, "Compression", 1, 1, 9);
    setConfig(CONFIG_BOOL_ADDON_CHANNEL, "AddonChannel", true);
    setConfig(CONFIG_BOOL_CLEAN_CHARACTER_DB, "CleanCharacterDB", true);
    setConfig(CONFIG_UINT32_INTERVAL_SAVE, "PlayerSave.Interval",
        15 * MINUTE * IN_MILLISECONDS);
    setConfigMinMax(CONFIG_UINT32_MIN_LEVEL_STAT_SAVE,
        "PlayerSave.Stats.MinLevel", 0, 0, MAX_LEVEL);
    setConfig(CONFIG_BOOL_STATS_SAVE_ONLY_ON_LOGOUT,
        "PlayerSave.Stats.SaveOnlyOnLogout", true);
    setConfig(CONFIG_BOOL_LOAD_DATA_ON_STARTUP, "LoadDataOnStartup", false);

    setConfigMin(CONFIG_UINT32_INTERVAL_MAPUPDATE, "MapUpdateInterval", 100,
        MIN_MAP_UPDATE_DELAY);
    if (reload)
        sMapMgr::Instance()->SetMapUpdateInterval(
            getConfig(CONFIG_UINT32_INTERVAL_MAPUPDATE));

    setConfig(CONFIG_UINT32_INTERVAL_CHANGEWEATHER, "ChangeWeatherInterval",
        10 * MINUTE * IN_MILLISECONDS);

    if (configNoReload(reload, CONFIG_UINT32_PORT_WORLD, "WorldServerPort",
            DEFAULT_WORLDSERVER_PORT))
        setConfig(CONFIG_UINT32_PORT_WORLD, "WorldServerPort",
            DEFAULT_WORLDSERVER_PORT);

    if (configNoReload(reload, CONFIG_UINT32_GAME_TYPE, "GameType", 0))
        setConfig(CONFIG_UINT32_GAME_TYPE, "GameType", 0);

    if (configNoReload(reload, CONFIG_UINT32_REALM_ZONE, "RealmZone",
            REALM_ZONE_DEVELOPMENT))
        setConfig(
            CONFIG_UINT32_REALM_ZONE, "RealmZone", REALM_ZONE_DEVELOPMENT);

    setConfig(
        CONFIG_BOOL_ALLOW_TWO_SIDE_ACCOUNTS, "AllowTwoSide.Accounts", false);
    setConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_CHAT,
        "AllowTwoSide.Interaction.Chat", false);
    setConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_CHANNEL,
        "AllowTwoSide.Interaction.Channel", false);
    setConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_GROUP,
        "AllowTwoSide.Interaction.Group", false);
    setConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_GUILD,
        "AllowTwoSide.Interaction.Guild", false);
    setConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_AUCTION,
        "AllowTwoSide.Interaction.Auction", false);
    setConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_MAIL,
        "AllowTwoSide.Interaction.Mail", false);
    setConfig(
        CONFIG_BOOL_ALLOW_TWO_SIDE_WHO_LIST, "AllowTwoSide.WhoList", false);
    setConfig(
        CONFIG_BOOL_ALLOW_TWO_SIDE_ADD_FRIEND, "AllowTwoSide.AddFriend", false);

    setConfig(CONFIG_UINT32_STRICT_PLAYER_NAMES, "StrictPlayerNames", 0);
    setConfig(CONFIG_UINT32_STRICT_CHARTER_NAMES, "StrictCharterNames", 0);
    setConfig(CONFIG_UINT32_STRICT_PET_NAMES, "StrictPetNames", 0);

    setConfigMinMax(
        CONFIG_UINT32_MIN_PLAYER_NAME, "MinPlayerName", 2, 1, MAX_PLAYER_NAME);
    setConfigMinMax(CONFIG_UINT32_MIN_CHARTER_NAME, "MinCharterName", 2, 1,
        MAX_CHARTER_NAME);
    setConfigMinMax(
        CONFIG_UINT32_MIN_PET_NAME, "MinPetName", 2, 1, MAX_PET_NAME);

    setConfig(CONFIG_UINT32_CHARACTERS_CREATING_DISABLED,
        "CharactersCreatingDisabled", 0);

    setConfigMinMax(
        CONFIG_UINT32_CHARACTERS_PER_REALM, "CharactersPerRealm", 10, 1, 10);

    // must be after CONFIG_UINT32_CHARACTERS_PER_REALM
    setConfigMin(CONFIG_UINT32_CHARACTERS_PER_ACCOUNT, "CharactersPerAccount",
        50, getConfig(CONFIG_UINT32_CHARACTERS_PER_REALM));

    setConfigMinMax(CONFIG_UINT32_SKIP_CINEMATICS, "SkipCinematics", 0, 0, 2);

    if (configNoReload(reload, CONFIG_UINT32_MAX_PLAYER_LEVEL, "MaxPlayerLevel",
            DEFAULT_MAX_LEVEL))
        setConfigMinMax(CONFIG_UINT32_MAX_PLAYER_LEVEL, "MaxPlayerLevel",
            DEFAULT_MAX_LEVEL, 1, DEFAULT_MAX_LEVEL);

    setConfigMinMax(CONFIG_UINT32_START_PLAYER_LEVEL, "StartPlayerLevel", 1, 1,
        getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL));

    setConfigMinMax(CONFIG_UINT32_START_PLAYER_MONEY, "StartPlayerMoney", 0, 0,
        MAX_MONEY_AMOUNT);

    setConfig(CONFIG_UINT32_MAX_HONOR_POINTS, "MaxHonorPoints", 75000);

    setConfigMinMax(CONFIG_UINT32_START_HONOR_POINTS, "StartHonorPoints", 0, 0,
        getConfig(CONFIG_UINT32_MAX_HONOR_POINTS));

    setConfig(CONFIG_UINT32_MAX_ARENA_POINTS, "MaxArenaPoints", 5000);

    setConfigMinMax(CONFIG_UINT32_START_ARENA_POINTS, "StartArenaPoints", 0, 0,
        getConfig(CONFIG_UINT32_MAX_ARENA_POINTS));

    setConfig(CONFIG_BOOL_ALL_TAXI_PATHS, "AllFlightPaths", false);

    setConfig(CONFIG_BOOL_INSTANCE_IGNORE_LEVEL, "Instance.IgnoreLevel", false);
    setConfig(CONFIG_BOOL_INSTANCE_IGNORE_RAID, "Instance.IgnoreRaid", false);

    setConfig(CONFIG_BOOL_CAST_UNSTUCK, "CastUnstuck", true);
    setConfig(
        CONFIG_UINT32_MAX_SPELL_CASTS_IN_CHAIN, "MaxSpellCastsInChain", 10);
    setConfig(
        CONFIG_UINT32_INSTANCE_RESET_TIME_HOUR, "Instance.ResetTimeHour", 4);
    setConfig(CONFIG_UINT32_INSTANCE_UNLOAD_DELAY, "Instance.UnloadDelay",
        30 * MINUTE * IN_MILLISECONDS);

    setConfigMinMax(CONFIG_UINT32_MAX_PRIMARY_TRADE_SKILL,
        "MaxPrimaryTradeSkill", 2, 0, 10);

    setConfigMinMax(CONFIG_UINT32_TRADE_SKILL_GMIGNORE_MAX_PRIMARY_COUNT,
        "TradeSkill.GMIgnore.MaxPrimarySkillsCount", SEC_CONSOLE, SEC_PLAYER,
        SEC_CONSOLE);
    setConfigMinMax(CONFIG_UINT32_TRADE_SKILL_GMIGNORE_LEVEL,
        "TradeSkill.GMIgnore.Level", SEC_CONSOLE, SEC_PLAYER, SEC_CONSOLE);
    setConfigMinMax(CONFIG_UINT32_TRADE_SKILL_GMIGNORE_SKILL,
        "TradeSkill.GMIgnore.Skill", SEC_CONSOLE, SEC_PLAYER, SEC_CONSOLE);

    setConfigMinMax(
        CONFIG_UINT32_MIN_PETITION_SIGNS, "MinPetitionSigns", 9, 0, 9);

    setConfig(CONFIG_UINT32_GM_LOGIN_STATE, "GM.LoginState", 2);
    setConfig(CONFIG_UINT32_GM_VISIBLE_STATE, "GM.Visible", 2);
    setConfig(CONFIG_UINT32_GM_CHAT, "GM.Chat", 2);

    setConfig(
        CONFIG_UINT32_GM_LEVEL_IN_GM_LIST, "GM.InGMList.Level", SEC_FULL_GM);
    setConfig(
        CONFIG_UINT32_GM_LEVEL_IN_WHO_LIST, "GM.InWhoList.Level", SEC_FULL_GM);
    setConfig(CONFIG_BOOL_GM_LOG_TRADE, "GM.LogTrade", false);

    setConfigMinMax(CONFIG_UINT32_START_GM_LEVEL, "GM.StartLevel", 1,
        getConfig(CONFIG_UINT32_START_PLAYER_LEVEL), MAX_LEVEL);
    setConfig(CONFIG_BOOL_GM_LOWER_SECURITY, "GM.LowerSecurity", false);

    setConfig(CONFIG_UINT32_GROUP_VISIBILITY, "Visibility.GroupMode", 0);

    setConfig(CONFIG_UINT32_MAIL_DELIVERY_DELAY, "MailDeliveryDelay", HOUR);

    setConfigMin(CONFIG_UINT32_MASS_MAILER_SEND_PER_TICK,
        "MassMailer.SendPerTick", 10, 1);

    setConfig(CONFIG_UINT32_UPTIME_UPDATE, "UpdateUptimeInterval", 10);
    if (reload)
    {
        m_timers[WUPDATE_UPTIME].SetInterval(
            getConfig(CONFIG_UINT32_UPTIME_UPDATE) * MINUTE * IN_MILLISECONDS);
        m_timers[WUPDATE_UPTIME].Reset();
    }

    setConfig(CONFIG_UINT32_SKILL_CHANCE_ORANGE, "SkillChance.Orange", 100);
    setConfig(CONFIG_UINT32_SKILL_CHANCE_YELLOW, "SkillChance.Yellow", 75);
    setConfig(CONFIG_UINT32_SKILL_CHANCE_GREEN, "SkillChance.Green", 25);
    setConfig(CONFIG_UINT32_SKILL_CHANCE_GREY, "SkillChance.Grey", 0);

    setConfig(
        CONFIG_UINT32_SKILL_CHANCE_MINING_STEPS, "SkillChance.MiningSteps", 75);
    setConfig(CONFIG_UINT32_SKILL_CHANCE_SKINNING_STEPS,
        "SkillChance.SkinningSteps", 75);

    setConfig(CONFIG_BOOL_SKILL_PROSPECTING, "SkillChance.Prospecting", false);

    setConfig(CONFIG_UINT32_SKILL_GAIN_CRAFTING, "SkillGain.Crafting", 1);
    setConfig(CONFIG_UINT32_SKILL_GAIN_DEFENSE, "SkillGain.Defense", 1);
    setConfig(CONFIG_UINT32_SKILL_GAIN_GATHERING, "SkillGain.Gathering", 1);
    setConfig(CONFIG_UINT32_SKILL_GAIN_WEAPON, "SkillGain.Weapon", 1);

    setConfig(
        CONFIG_BOOL_SKILL_FAIL_LOOT_FISHING, "SkillFail.Loot.Fishing", false);
    setConfig(
        CONFIG_BOOL_SKILL_FAIL_GAIN_FISHING, "SkillFail.Gain.Fishing", false);
    setConfig(CONFIG_BOOL_SKILL_FAIL_POSSIBLE_FISHINGPOOL,
        "SkillFail.Possible.FishingPool", true);

    setConfig(CONFIG_UINT32_MAX_OVERSPEED_PINGS, "MaxOverspeedPings", 2);
    if (getConfig(CONFIG_UINT32_MAX_OVERSPEED_PINGS) != 0 &&
        getConfig(CONFIG_UINT32_MAX_OVERSPEED_PINGS) < 2)
    {
        logging.error(
            "MaxOverspeedPings (%i) must be in range 2..infinity (or 0 to "
            "disable check). Set to 2.",
            getConfig(CONFIG_UINT32_MAX_OVERSPEED_PINGS));
        setConfig(CONFIG_UINT32_MAX_OVERSPEED_PINGS, 2);
    }

    setConfig(CONFIG_BOOL_WEATHER, "ActivateWeather", true);

    setConfig(CONFIG_BOOL_ALWAYS_MAX_SKILL_FOR_LEVEL, "AlwaysMaxSkillForLevel",
        false);

    if (configNoReload(
            reload, CONFIG_UINT32_EXPANSION, "Expansion", MAX_EXPANSION))
        setConfigMinMax(CONFIG_UINT32_EXPANSION, "Expansion", MAX_EXPANSION, 0,
            MAX_EXPANSION);

    setConfig(
        CONFIG_UINT32_CHATFLOOD_MESSAGE_COUNT, "ChatFlood.MessageCount", 10);
    setConfig(
        CONFIG_UINT32_CHATFLOOD_MESSAGE_DELAY, "ChatFlood.MessageDelay", 1);
    setConfig(CONFIG_UINT32_CHATFLOOD_MUTE_TIME, "ChatFlood.MuteTime", 10);

    setConfig(CONFIG_BOOL_EVENT_ANNOUNCE, "Event.Announce", false);

    setConfig(CONFIG_UINT32_CREATURE_FAMILY_ASSISTANCE_DELAY,
        "CreatureFamilyAssistanceDelay", 1500);
    setConfig(CONFIG_UINT32_CREATURE_FAMILY_FLEE_DELAY,
        "CreatureFamilyFleeDelay", 7000);

    setConfig(CONFIG_UINT32_WORLD_BOSS_LEVEL_DIFF, "WorldBossLevelDiff", 3);

    // note: disable value (-1) will assigned as 0xFFFFFFF, to prevent overflow
    // at calculations limit it to max possible player level MAX_LEVEL(100)
    setConfigMinMax(CONFIG_INT32_QUEST_LOW_LEVEL_HIDE_DIFF,
        "Quests.LowLevelHideDiff", 4, -1, MAX_LEVEL);
    setConfigMinMax(CONFIG_INT32_QUEST_HIGH_LEVEL_HIDE_DIFF,
        "Quests.HighLevelHideDiff", 7, -1, MAX_LEVEL);

    setConfigMinMax(CONFIG_UINT32_QUEST_DAILY_RESET_HOUR,
        "Quests.Daily.ResetHour", 6, 0, 23);

    setConfig(CONFIG_BOOL_QUEST_IGNORE_RAID, "Quests.IgnoreRaid", false);

    setConfig(
        CONFIG_BOOL_RESTRICTED_LFG_CHANNEL, "Channel.RestrictedLfg", true);
    setConfig(CONFIG_BOOL_SILENTLY_GM_JOIN_TO_CHANNEL, "Channel.SilentlyGMJoin",
        false);

    setConfig(CONFIG_BOOL_TALENTS_INSPECTING, "TalentsInspecting", true);
    setConfig(CONFIG_BOOL_CHAT_FAKE_MESSAGE_PREVENTING,
        "ChatFakeMessagePreventing", false);

    setConfig(CONFIG_UINT32_CHAT_STRICT_LINK_CHECKING_SEVERITY,
        "ChatStrictLinkChecking.Severity", 0);
    setConfig(CONFIG_UINT32_CHAT_STRICT_LINK_CHECKING_KICK,
        "ChatStrictLinkChecking.Kick", 0);

    setConfig(CONFIG_BOOL_CORPSE_EMPTY_LOOT_SHOW, "Corpse.EmptyLootShow", true);
    setConfig(CONFIG_UINT32_CORPSE_DECAY_NORMAL, "Corpse.Decay.NORMAL", 300);
    setConfig(CONFIG_UINT32_CORPSE_DECAY_RARE, "Corpse.Decay.RARE", 900);
    setConfig(CONFIG_UINT32_CORPSE_DECAY_ELITE, "Corpse.Decay.ELITE", 600);
    setConfig(
        CONFIG_UINT32_CORPSE_DECAY_RAREELITE, "Corpse.Decay.RAREELITE", 1200);
    setConfig(
        CONFIG_UINT32_CORPSE_DECAY_WORLDBOSS, "Corpse.Decay.WORLDBOSS", 3600);
    setConfig(CONFIG_UINT32_CORPSE_DECAY_LOOTED, "Corpse.Decay.Looted", 120);

    setConfig(CONFIG_INT32_DEATH_SICKNESS_LEVEL, "Death.SicknessLevel", 11);

    setConfig(CONFIG_BOOL_DEATH_CORPSE_RECLAIM_DELAY_PVP,
        "Death.CorpseReclaimDelay.PvP", true);
    setConfig(CONFIG_BOOL_DEATH_CORPSE_RECLAIM_DELAY_PVE,
        "Death.CorpseReclaimDelay.PvE", true);
    setConfig(CONFIG_BOOL_DEATH_BONES_WORLD, "Death.Bones.World", true);
    setConfig(CONFIG_BOOL_DEATH_BONES_BG_OR_ARENA,
        "Death.Bones.BattlegroundOrArena", true);
    setConfigMinMax(CONFIG_FLOAT_GHOST_RUN_SPEED_WORLD,
        "Death.Ghost.RunSpeed.World", 1.0f, 0.1f, 10.0f);
    setConfigMinMax(CONFIG_FLOAT_GHOST_RUN_SPEED_BG,
        "Death.Ghost.RunSpeed.Battleground", 1.0f, 0.1f, 10.0f);

    // always use declined names in the russian client
    if (getConfig(CONFIG_UINT32_REALM_ZONE) == REALM_ZONE_RUSSIAN)
        setConfig(CONFIG_BOOL_DECLINED_NAMES_USED, true);
    else
        setConfig(CONFIG_BOOL_DECLINED_NAMES_USED, "DeclinedNames", false);

    setConfig(CONFIG_BOOL_BATTLEGROUND_CAST_DESERTER,
        "Battleground.CastDeserter", true);
    setConfigMinMax(CONFIG_UINT32_BATTLEGROUND_QUEUE_ANNOUNCER_JOIN,
        "Battleground.QueueAnnouncer.Join", 0, 0, 2);
    setConfig(CONFIG_BOOL_BATTLEGROUND_QUEUE_ANNOUNCER_START,
        "Battleground.QueueAnnouncer.Start", false);
    setConfig(CONFIG_UINT32_BATTLEGROUND_INVITATION_TYPE,
        "Battleground.InvitationType", 0);
    setConfig(CONFIG_UINT32_BATTLEGROUND_PREMATURE_FINISH_TIMER,
        "BattleGround.PrematureFinishTimer", 5 * MINUTE * IN_MILLISECONDS);
    setConfig(CONFIG_UINT32_BATTLEGROUND_PREMADE_GROUP_WAIT_FOR_MATCH,
        "BattleGround.PremadeGroupWaitForMatch", 30 * MINUTE * IN_MILLISECONDS);
    setConfig(CONFIG_UINT32_ARENA_MAX_RATING_DIFFERENCE,
        "Arena.MaxRatingDifference", 150);
    setConfig(CONFIG_UINT32_ARENA_RATING_DISCARD_TIMER,
        "Arena.RatingDiscardTimer", 10 * MINUTE * IN_MILLISECONDS);
    setConfig(CONFIG_BOOL_ARENA_AUTO_DISTRIBUTE_POINTS,
        "Arena.AutoDistributePoints", false);
    setConfig(CONFIG_UINT32_ARENA_AUTO_DISTRIBUTE_INTERVAL_DAYS,
        "Arena.AutoDistributeInterval", 7);
    setConfig(CONFIG_BOOL_ARENA_QUEUE_ANNOUNCER_JOIN,
        "Arena.QueueAnnouncer.Join", false);
    setConfig(CONFIG_BOOL_ARENA_QUEUE_ANNOUNCER_EXIT,
        "Arena.QueueAnnouncer.Exit", false);
    setConfig(CONFIG_UINT32_ARENA_SEASON_ID, "Arena.ArenaSeason.ID", 1);
    setConfigMin(CONFIG_INT32_ARENA_STARTRATING, "Arena.StartRating", -1, -1);
    setConfigMin(CONFIG_INT32_ARENA_STARTPERSONALRATING,
        "Arena.StartPersonalRating", -1, -1);

    setConfig(CONFIG_BOOL_OUTDOORPVP_SI_ENABLED, "OutdoorPvp.SIEnabled", true);
    setConfig(CONFIG_BOOL_OUTDOORPVP_EP_ENABLED, "OutdoorPvp.EPEnabled", true);
    setConfig(CONFIG_BOOL_OUTDOORPVP_HP_ENABLED, "OutdoorPvp.HPEnabled", true);
    setConfig(CONFIG_BOOL_OUTDOORPVP_ZM_ENABLED, "OutdoorPvp.ZMEnabled", true);
    setConfig(CONFIG_BOOL_OUTDOORPVP_TF_ENABLED, "OutdoorPvp.TFEnabled", true);
    setConfig(CONFIG_BOOL_OUTDOORPVP_NA_ENABLED, "OutdoorPvp.NAEnabled", true);

    setConfig(CONFIG_BOOL_KICK_PLAYER_ON_BAD_PACKET, "Network.KickOnBadPacket",
        false);

    setConfig(CONFIG_UINT32_INSTANT_LOGOUT, "InstantLogout", SEC_TICKET_GM);

    setConfigMin(CONFIG_UINT32_GUILD_EVENT_LOG_COUNT,
        "Guild.EventLogRecordsCount", GUILD_EVENTLOG_MAX_RECORDS,
        GUILD_EVENTLOG_MAX_RECORDS);
    setConfigMin(CONFIG_UINT32_GUILD_BANK_EVENT_LOG_COUNT,
        "Guild.BankEventLogRecordsCount", GUILD_BANK_MAX_LOGS,
        GUILD_BANK_MAX_LOGS);

    setConfig(CONFIG_UINT32_TIMERBAR_FATIGUE_GMLEVEL,
        "TimerBar.Fatigue.GMLevel", SEC_CONSOLE);
    setConfig(CONFIG_UINT32_TIMERBAR_FATIGUE_MAX, "TimerBar.Fatigue.Max", 60);
    setConfig(CONFIG_UINT32_TIMERBAR_BREATH_GMLEVEL, "TimerBar.Breath.GMLevel",
        SEC_CONSOLE);
    setConfig(CONFIG_UINT32_TIMERBAR_BREATH_MAX, "TimerBar.Breath.Max", 180);
    setConfig(CONFIG_UINT32_TIMERBAR_FIRE_GMLEVEL, "TimerBar.Fire.GMLevel",
        SEC_CONSOLE);
    setConfig(CONFIG_UINT32_TIMERBAR_FIRE_MAX, "TimerBar.Fire.Max", 1);

    m_relocation_ai_notify_delay = sConfig::Instance()->GetIntDefault(
        "Visibility.AIRelocationNotifyDelay", 1000u);
    m_relocation_lower_limit_sq =
        pow(sConfig::Instance()->GetFloatDefault(
                "Visibility.RelocationLowerLimit", 10),
            2);

    m_VisibleUnitGreyDistance = sConfig::Instance()->GetFloatDefault(
        "Visibility.Distance.Grey.Unit", 1);
    if (m_VisibleUnitGreyDistance > MAX_VISIBILITY_DISTANCE)
    {
        logging.error("Visibility.Distance.Grey.Unit can't be greater %f",
            MAX_VISIBILITY_DISTANCE);
        m_VisibleUnitGreyDistance = MAX_VISIBILITY_DISTANCE;
    }
    m_VisibleObjectGreyDistance = sConfig::Instance()->GetFloatDefault(
        "Visibility.Distance.Grey.Object", 10);
    if (m_VisibleObjectGreyDistance > MAX_VISIBILITY_DISTANCE)
    {
        logging.error("Visibility.Distance.Grey.Object can't be greater %f",
            MAX_VISIBILITY_DISTANCE);
        m_VisibleObjectGreyDistance = MAX_VISIBILITY_DISTANCE;
    }

    // visibility on continents
    m_MaxVisibleDistanceOnContinents = sConfig::Instance()->GetFloatDefault(
        "Visibility.Distance.Continents", DEFAULT_VISIBILITY_DISTANCE);
    if (m_MaxVisibleDistanceOnContinents < 45)
    {
        logging.error(
            "Visibility.Distance.Continents can't be less max aggro radius 45");
        m_MaxVisibleDistanceOnContinents = 45;
    }
    else if (m_MaxVisibleDistanceOnContinents + m_VisibleUnitGreyDistance >
             MAX_VISIBILITY_DISTANCE)
    {
        logging.error("Visibility.Distance.Continents can't be greater %f",
            MAX_VISIBILITY_DISTANCE - m_VisibleUnitGreyDistance);
        m_MaxVisibleDistanceOnContinents =
            MAX_VISIBILITY_DISTANCE - m_VisibleUnitGreyDistance;
    }

    // visibility in instances
    m_MaxVisibleDistanceInInstances = sConfig::Instance()->GetFloatDefault(
        "Visibility.Distance.Instances", DEFAULT_VISIBILITY_INSTANCE);
    if (m_MaxVisibleDistanceInInstances < 45)
    {
        logging.error(
            "Visibility.Distance.Instances can't be less max aggro radius 45");
        m_MaxVisibleDistanceInInstances = 45;
    }
    else if (m_MaxVisibleDistanceInInstances + m_VisibleUnitGreyDistance >
             MAX_VISIBILITY_DISTANCE)
    {
        logging.error("Visibility.Distance.Instances can't be greater %f",
            MAX_VISIBILITY_DISTANCE - m_VisibleUnitGreyDistance);
        m_MaxVisibleDistanceInInstances =
            MAX_VISIBILITY_DISTANCE - m_VisibleUnitGreyDistance;
    }

    // visibility in BG/Arenas
    m_MaxVisibleDistanceInBGArenas = sConfig::Instance()->GetFloatDefault(
        "Visibility.Distance.BGArenas", DEFAULT_VISIBILITY_BGARENAS);
    if (m_MaxVisibleDistanceInBGArenas < 45)
    {
        logging.error(
            "Visibility.Distance.BGArenas can't be less max aggro radius 45");
        m_MaxVisibleDistanceInBGArenas = 45;
    }
    else if (m_MaxVisibleDistanceInBGArenas + m_VisibleUnitGreyDistance >
             MAX_VISIBILITY_DISTANCE)
    {
        logging.error("Visibility.Distance.BGArenas can't be greater %f",
            MAX_VISIBILITY_DISTANCE - m_VisibleUnitGreyDistance);
        m_MaxVisibleDistanceInBGArenas =
            MAX_VISIBILITY_DISTANCE - m_VisibleUnitGreyDistance;
    }

    m_MaxVisibleDistanceInFlight = sConfig::Instance()->GetFloatDefault(
        "Visibility.Distance.InFlight", DEFAULT_VISIBILITY_DISTANCE);
    if (m_MaxVisibleDistanceInFlight + m_VisibleObjectGreyDistance >
        MAX_VISIBILITY_DISTANCE)
    {
        logging.error("Visibility.Distance.InFlight can't be greater %f",
            MAX_VISIBILITY_DISTANCE - m_VisibleObjectGreyDistance);
        m_MaxVisibleDistanceInFlight =
            MAX_VISIBILITY_DISTANCE - m_VisibleObjectGreyDistance;
    }

    ///- Load the CharDelete related config options
    setConfigMinMax(
        CONFIG_UINT32_CHARDELETE_METHOD, "CharDelete.Method", 0, 0, 1);
    setConfigMinMax(CONFIG_UINT32_CHARDELETE_MIN_LEVEL, "CharDelete.MinLevel",
        0, 0, getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL));
    setConfig(CONFIG_UINT32_CHARDELETE_KEEP_DAYS, "CharDelete.KeepDays", 30);

    if (configNoReload(reload, CONFIG_UINT32_GUID_RESERVE_SIZE_CREATURE,
            "GuidReserveSize.Creature", 100))
        setConfig(CONFIG_UINT32_GUID_RESERVE_SIZE_CREATURE,
            "GuidReserveSize.Creature", 100);
    if (configNoReload(reload, CONFIG_UINT32_GUID_RESERVE_SIZE_GAMEOBJECT,
            "GuidReserveSize.GameObject", 100))
        setConfig(CONFIG_UINT32_GUID_RESERVE_SIZE_GAMEOBJECT,
            "GuidReserveSize.GameObject", 100);

    ///- Read the "Data" directory from the config file
    std::string dataPath =
        sConfig::Instance()->GetStringDefault("DataDir", "./");

    // for empty string use current dir as for absent case
    if (dataPath.empty())
        dataPath = "./";
    // normalize dir path to path/ or path\ form
    else if (dataPath.at(dataPath.length() - 1) != '/' &&
             dataPath.at(dataPath.length() - 1) != '\\')
        dataPath.append("/");

    if (reload)
    {
        if (dataPath != m_dataPath)
            logging.error(
                "DataDir option can't be changed at mangosd.conf reload, using "
                "current value (%s).",
                m_dataPath.c_str());
    }
    else
    {
        m_dataPath = dataPath;
        logging.info("Using DataDir %s", m_dataPath.c_str());
    }

    logging.info("WORLD: VMap data directory is: %svmaps", m_dataPath.c_str());

    setConfig(CONFIG_BOOL_MMAP_ENABLED, "mmap.enabled", true);
    std::string ignoreMapIds =
        sConfig::Instance()->GetStringDefault("mmap.ignoreMapIds", "");
    MMAP::MMapFactory::preventPathfindingOnMaps(ignoreMapIds.c_str());
    // Show big warnings if mmaps is disabled
    if (!getConfig(CONFIG_BOOL_MMAP_ENABLED))
        logging.warning(
            "MMaps is turned off, server behavior will be very poor.");

    // Warden
    setConfig(CONFIG_BOOL_WARDEN_ENABLED, "Warden.Enabled", false);
    setConfig(CONFIG_UINT32_WARDEN_NUM_CHECKS, "Warden.NumChecks", 3);
    setConfig(CONFIG_UINT32_WARDEN_NUM_DYN_CHECKS, "Warden.NumDynChecks", 5);
    setConfig(CONFIG_UINT32_WARDEN_CLIENT_RESPONSE_DELAY,
        "Warden.ClientResponseDelay", 15);
    setConfig(CONFIG_UINT32_WARDEN_CLIENT_CHECK_HOLDOFF,
        "Warden.ClientCheckHoldOff", 30);
    setConfig(CONFIG_UINT32_WARDEN_BAN_TIME, "Warden.BanLength", 0);
    setConfig(
        CONFIG_UINT32_WARDEN_SLOW_CHECKS_DELAY, "Warden.SlowChecksDelay", 300);

    setConfig(
        CONFIG_BOOL_ANTI_CHEAT_KICK_ENABLED, "AntiCheat.KickEnabled", false);

    // World Drop Percentages
    setConfig(CONFIG_FLOAT_WORLD_DROP_ARMOR_SLOT_CHANCE,
        "WorldDrop.NormalMob.ArmorSlotChance", 0.1f);
    setConfig(CONFIG_FLOAT_WORLD_DROP_WEAPON_SLOT_CHANCE,
        "WorldDrop.NormalMob.WeaponSlotChance", 0.05f);
    setConfig(CONFIG_FLOAT_WORLD_DROP_EPIC_QUALITY_CHANCE,
        "WorldDrop.NormalMob.EpicQualityChance", 0.0008f);
    setConfig(CONFIG_FLOAT_WORLD_DROP_RARE_QUALITY_CHANCE,
        "WorldDrop.NormalMob.RareQualityChance", 0.009f);
    setConfig(CONFIG_FLOAT_WORLD_DROP_UNCOMMON_QUALITY_CHANCE,
        "WorldDrop.NormalMob.UncommonQualityChance", 0.15f);
    setConfig(CONFIG_FLOAT_WORLD_DROP_COMMON_QUALITY_CHANCE,
        "WorldDrop.NormalMob.CommonQualityChance", 0.4f);
    setConfig(CONFIG_FLOAT_WORLD_DROP_ARMOR_SLOT_CHANCE_ELITE,
        "WorldDrop.EliteMob.ArmorSlotChance", 0.15f);
    setConfig(CONFIG_FLOAT_WORLD_DROP_WEAPON_SLOT_CHANCE_ELITE,
        "WorldDrop.EliteMob.WeaponSlotChance", 0.075f);
    setConfig(CONFIG_FLOAT_WORLD_DROP_EPIC_QUALITY_CHANCE_ELITE,
        "WorldDrop.EliteMob.EpicQualityChance", 0.0008f);
    setConfig(CONFIG_FLOAT_WORLD_DROP_RARE_QUALITY_CHANCE_ELITE,
        "WorldDrop.EliteMob.RareQualityChance", 0.009f);
    setConfig(CONFIG_FLOAT_WORLD_DROP_UNCOMMON_QUALITY_CHANCE_ELITE,
        "WorldDrop.EliteMob.UncommonQualityChance", 0.2f);
    setConfig(CONFIG_FLOAT_WORLD_DROP_COMMON_QUALITY_CHANCE_ELITE,
        "WorldDrop.EliteMob.CommonQualityChance", 0.4f);

    setConfig(
        CONFIG_UINT32_CHARACTER_SCREEN_TIMEOUT, "ChararacterSelectTimeout", 15);

    setConfig(CONFIG_BOOL_TICKET_SYSTEM_ENABLED, "TicketSystemEnabled", true);

    // Concurrency settings
    setConfig(CONFIG_UINT32_MAP_THREADS, "Concurrency.MapThreadsNum", 3);
    // Concurrency.UsePathgenThread in mangosd/WorldThread.cpp: only changeable
    // at startup
    // Concurrency.NetworkThreads in WorldServer.cpp: only changeable at startup
}

extern void LoadGameObjectModelList();
extern void buff_stack_db_load();

/// Initialize the World
void World::SetInitialWorldSettings()
{
    ///- Initialize the random number generator
    srand((unsigned int)time(nullptr));

    ///- Time server startup
    uint32 uStartTime = WorldTimer::getMSTime();

    ///- Initialize detour memory management
    dtAllocSetCustom(dtCustomAlloc, dtCustomFree);

    ///- Initialize config settings
    LoadConfigSettings();

    ///- Check the existence of the map files for all races start areas.
    if (!MapManager::ExistMapAndVMap(0, -6240.32f, 331.033f) ||
        !MapManager::ExistMapAndVMap(0, -8949.95f, -132.493f) ||
        !MapManager::ExistMapAndVMap(0, -8949.95f, -132.493f) ||
        !MapManager::ExistMapAndVMap(1, -618.518f, -4251.67f) ||
        !MapManager::ExistMapAndVMap(0, 1676.35f, 1677.45f) ||
        !MapManager::ExistMapAndVMap(1, 10311.3f, 832.463f) ||
        !MapManager::ExistMapAndVMap(1, -2917.58f, -257.98f) ||
        (m_configUint32Values[CONFIG_UINT32_EXPANSION] &&
            (!MapManager::ExistMapAndVMap(530, 10349.6f, -6357.29f) ||
                !MapManager::ExistMapAndVMap(530, -3961.64f, -13931.2f))))
    {
        logging.error(
            "Correct *.map files not found in path '%smaps' or "
            "*.vmtree/*.vmtile files in '%svmaps'. Please place *.map and vmap "
            "files in appropriate directories or correct the DataDir value in "
            "the mangosd.conf file.",
            m_dataPath.c_str(), m_dataPath.c_str());

        exit(1);
    }

    if (logging.get_logger().get_level() <= LogLevel::info)
        printf("\n");

    ///- Loading strings. Getting no records means core load has to be canceled
    /// because no error message can be output.
    logging.info("Loading MaNGOS strings...");
    if (!sObjectMgr::Instance()->LoadMangosStrings())
        exit(1); // Error message displayed in function already

    ///- Update the realm entry in the database with the realm type from the
    /// config file
    // No SQL injection as values are treated as integers

    // not send custom type REALM_FFA_PVP to realm list
    uint32 server_type = IsFFAPvPRealm() ? static_cast<uint32>(REALM_TYPE_PVP) :
                                           getConfig(CONFIG_UINT32_GAME_TYPE);
    uint32 realm_zone = getConfig(CONFIG_UINT32_REALM_ZONE);
    LoginDatabase.PExecute(
        "UPDATE realmlist SET icon = %u, timezone = %u WHERE id = '%u'",
        server_type, realm_zone, realmID);

    ///- Remove the bones (they should not exist in DB though) and old corpses
    /// after a restart
    CharacterDatabase.PExecute(
        "DELETE FROM corpse WHERE corpse_type = '0' OR time < "
        "(UNIX_TIMESTAMP()-'%u')",
        3 * DAY);

    ///- Load the DBC files
    logging.info("Initialize data stores...");
    LoadDBCStores(m_dataPath);
    DetectDBCLang();
    sObjectMgr::Instance()->SetDBCLocaleIndex(
        GetDefaultDbcLocale()); // Get once for all the locale index of DBC
                                // language (console/broadcasts)

    LoadGameObjectModelList();

    logging.info("Loading Script Names...");
    sScriptMgr::Instance()->LoadScriptNames();

    logging.info("Loading WorldTemplate...");
    sObjectMgr::Instance()->LoadWorldTemplate();

    logging.info("Loading InstanceTemplate...");
    sObjectMgr::Instance()->LoadInstanceTemplate();

    logging.info("Loading SkillLineAbilityMultiMap Data...");
    sSpellMgr::Instance()->LoadSkillLineAbilityMap();

    logging.info("Loading SkillRaceClassInfoMultiMap Data...");
    sSpellMgr::Instance()->LoadSkillRaceClassInfoMap();

    ///- Clean up and pack instances
    // Must be called before `creature_respawn`/`gameobject_respawn` tables
    logging.info("Cleaning up instances...");
    sMapPersistentStateMgr::Instance()->CleanupInstances();

    logging.info("Loading reserved instance ids...");
    sMapPersistentStateMgr::Instance()->load_reserved_instance_ids();

    // Must be after CleanupInstances
    logging.info("Packing groups...");
    sObjectMgr::Instance()->PackGroupIds();

    ///- Init highest guids before any guid using table loading to prevent using
    /// not initialized guids in some code.
    // Must be after PackInstances() and PackGroupIds()
    sObjectMgr::Instance()->SetHighestGuids();

    logging.info("Loading Page Texts...");
    sObjectMgr::Instance()->LoadPageTexts();

    // Must be after LoadPageTexts
    logging.info("Loading Game Object Templates...");
    sObjectMgr::Instance()->LoadGameobjectInfo();

    logging.info("Loading Spell Chain Data...");
    sSpellMgr::Instance()->LoadSpellChains();

    logging.info("Loading Spell Elixir types...");
    sSpellMgr::Instance()->LoadSpellElixirs();

    // Must be after LoadSpellChains
    logging.info("Loading Spell Learn Skills...");
    sSpellMgr::Instance()->LoadSpellLearnSkills();

    logging.info("Loading Spell Learn Spells...");
    sSpellMgr::Instance()->LoadSpellLearnSpells();

    logging.info("Loading Spell Proc Event conditions...");
    sSpellMgr::Instance()->LoadSpellProcEvents();

    logging.info("Loading Spell Bonus Data...");
    sSpellMgr::Instance()->LoadSpellBonuses();

    // Must be after LoadSpellChains
    logging.info("Loading Spell Proc Item Enchant...");
    sSpellMgr::Instance()->LoadSpellProcItemEnchant();

    logging.info("Loading Spell Dependencies...");
    sSpellMgr::Instance()->LoadSpellDependencies();

    logging.info("Loading Spell Proc Exceptions...");
    sSpellMgr::Instance()->LoadSpellProcExceptions();

    logging.info("Loading Spell Multipart Auras...");
    sSpellMgr::Instance()->LoadSpellMultipartAuras();

    logging.info("Loading Aggro Spells Definitions...");
    sSpellMgr::Instance()->LoadSpellThreats();

    logging.info("Loading NPC Texts...");
    sObjectMgr::Instance()->LoadGossipText();

    logging.info("Loading Item Random Enchantments Table...");
    LoadRandomEnchantmentsTable();

    // Must be after LoadRandomEnchantmentsTable and LoadPageTexts
    logging.info("Loading Items...");
    sObjectMgr::Instance()->LoadItemPrototypes();

    logging.info("Loading Item Texts...");
    sObjectMgr::Instance()->LoadItemTexts();

    logging.info("Loading Creature Model Based Info Data...");
    sObjectMgr::Instance()->LoadCreatureModelInfo();

    logging.info("Loading Equipment templates...");
    sObjectMgr::Instance()->LoadEquipmentTemplates();

    logging.info("Loading Creature templates...");
    sObjectMgr::Instance()->LoadCreatureTemplates();

    // Must be after creature templates
    logging.info("Loading Creature Model for race...");
    sObjectMgr::Instance()->LoadCreatureModelRace();

    // Must be after LoadCreatureTemplates and LoadGameobjectInfo
    logging.info("Loading SpellsScriptTarget...");
    sSpellMgr::Instance()->LoadSpellScriptTarget();

    logging.info("Loading ItemRequiredTarget...");
    sObjectMgr::Instance()->LoadItemRequiredTarget();

    logging.info("Loading Reputation Reward Rates...");
    sObjectMgr::Instance()->LoadReputationRewardRate();

    logging.info("Loading Creature Reputation OnKill Data...");
    sObjectMgr::Instance()->LoadReputationOnKill();

    logging.info("Loading Reputation Spillover Data...");
    sObjectMgr::Instance()->LoadReputationSpilloverTemplate();

    logging.info("Loading Points Of Interest Data...");
    sObjectMgr::Instance()->LoadPointsOfInterest();

    logging.info("Loading Pet Create Spells...");
    sObjectMgr::Instance()->LoadPetCreateSpells();

    logging.info("Loading Creature Data...");
    sObjectMgr::Instance()->LoadCreatures();

    // Must be after LoadCreatureTemplates() and LoadCreatures()
    logging.info("Loading Creature Addon Data...");
    sObjectMgr::Instance()->LoadCreatureAddons();

    logging.info("Loading Creature Group System...");
    CreatureGroupMgr::LoadAllGroupsFromDb();

    logging.info("Loading Gameobject Data...");
    sObjectMgr::Instance()->LoadGameObjects();

    logging.info("Loading Objects Pooling Data...");
    sPoolMgr::Instance()->LoadFromDB();

    logging.info("Loading Weather Data...");
    sObjectMgr::Instance()->LoadWeatherZoneChances();

    // Must be loaded after DBCs, creature_template, item_template, gameobject
    // tables
    logging.info("Loading Quests...");
    sObjectMgr::Instance()->LoadQuests();

    // Must be after quest load
    logging.info("Loading Quests Relations...");
    sObjectMgr::Instance()->LoadQuestRelations();

    // Must be after sPoolMgr::Instance()->LoadFromDB and quests to properly
    // load pool events and quests for events
    logging.info("Loading Game Event Data...");
    sGameEventMgr::Instance()->LoadFromDB();

    logging.info("Loading Npc Aggro Linkage...");
    sObjectMgr::Instance()->LoadNpcAggroLink();

    // Load Conditions
    logging.info("Loading Conditions...");
    sObjectMgr::Instance()->LoadConditions();

    // Must be after:
    // PackInstances(),
    // LoadCreatures(),
    // sPoolMgr::Instance()->LoadFromDB(),
    // sGameEventMgr::Instance()->LoadFromDB();
    logging.info("Creating map persistent states for non-instanceable maps...");
    sMapPersistentStateMgr::Instance()->InitWorldMaps();

    // Must be after LoadCreatures() and
    // sMapPersistentStateMgr::Instance()->InitWorldMaps()
    logging.info("Loading Creature Respawn Data...");
    sMapPersistentStateMgr::Instance()->LoadCreatureRespawnTimes();

    // Must be after LoadGameObjects() and
    // sMapPersistentStateMgr::Instance()->InitWorldMaps()
    logging.info("Loading Gameobject Respawn Data...");
    sMapPersistentStateMgr::Instance()->LoadGameobjectRespawnTimes();

    // Must be after quest load
    logging.info("Loading SpellArea Data...");
    sSpellMgr::Instance()->LoadSpellAreas();

    // Must be after item template load
    logging.info("Loading AreaTrigger definitions...");
    sObjectMgr::Instance()->LoadAreaTriggerTeleports();

    // Must be after LoadQuests
    logging.info("Loading Quest Area Triggers...");
    sObjectMgr::Instance()->LoadQuestAreaTriggers();

    logging.info("Loading Tavern Area Triggers...");
    sObjectMgr::Instance()->LoadTavernAreaTriggers();

    logging.info("Loading AreaTrigger script names...");
    sScriptMgr::Instance()->LoadAreaTriggerScripts();

    logging.info("Loading event id script names...");
    sScriptMgr::Instance()->LoadEventIdScripts();

    logging.info("Loading Graveyard-zone links...");
    sObjectMgr::Instance()->LoadGraveyardZones();

    logging.info("Loading spell target destination coordinates...");
    sSpellMgr::Instance()->LoadSpellTargetPositions();

    logging.info("Loading SpellAffect definitions...");
    sSpellMgr::Instance()->LoadSpellAffects();

    logging.info("Loading spell pet auras...");
    sSpellMgr::Instance()->LoadSpellPetAuras();

    logging.info("Loading Player Create Info & Level Stats...");
    sObjectMgr::Instance()->LoadPlayerInfo();

    logging.info("Loading Exploration BaseXP Data...");
    sObjectMgr::Instance()->LoadExplorationBaseXP();

    logging.info("Loading Pet Name Parts...");
    sObjectMgr::Instance()->LoadPetNames();

    logging.info("Loading Spell Level Calc...");
    sSpellMgr::Instance()->LoadSpellLevelCalc();

    CharacterDatabaseCleaner::CleanDatabase();

    logging.info("Loading the max pet number...");
    sObjectMgr::Instance()->LoadPetNumber();

    logging.info("Loading pet level stats...");
    sObjectMgr::Instance()->LoadPetLevelInfo();

    logging.info("Loading Pet Scaling...");
    sObjectMgr::Instance()->LoadPetScaling();

    logging.info("Loading Player Corpses...");
    sObjectMgr::Instance()->LoadCorpses();

    logging.info("Loading Player level dependent mail rewards...");
    sObjectMgr::Instance()->LoadMailLevelRewards();

    logging.info("Loading Loot Tables...");
    LoadLootTables();

    logging.info("Loading Skill Discovery Table...");
    LoadSkillDiscoveryTable();

    logging.info("Loading Skill Extra Item Table...");
    LoadSkillExtraItemTable();

    logging.info("Loading Skill Fishing base level requirements...");
    sObjectMgr::Instance()->LoadFishingBaseSkillLevel();

    // Must be after load Creature and LoadGossipText
    logging.info("Loading Npc Text Id...");
    sObjectMgr::Instance()->LoadNpcGossips();

    // Must be before gossip menu options
    logging.info("Loading Gossip scripts...");
    sScriptMgr::Instance()->LoadGossipScripts();

    sObjectMgr::Instance()->LoadGossipMenus();

    // Extended cost must be loaded before vendors
    logging.info("Loading Extended Item Cost overrides...");
    sObjectMgr::Instance()->LoadExtendedItemCost();

    logging.info("Loading Vendors...");
    // Must be after load ItemTemplate
    sObjectMgr::Instance()->LoadVendorTemplates();
    // Must be after load CreatureTemplate, VendorTemplate, and ItemTemplate
    sObjectMgr::Instance()->LoadVendors();

    logging.info("Loading Trainers...");
    // Must be after load CreatureTemplate
    sObjectMgr::Instance()->LoadTrainerTemplates();
    // Must be after load CreatureTemplate, TrainerTemplate
    sObjectMgr::Instance()->LoadTrainers();

    // Before loading from creature_movement
    logging.info("Loading Waypoint scripts...");
    sScriptMgr::Instance()->LoadCreatureMovementScripts();

    logging.info("Loading Waypoints...");
    sWaypointMgr::Instance()->Load();

    ///- Loading localization data
    logging.info("Loading Localization strings...");
    // Must be after CreatureInfo loading
    sObjectMgr::Instance()->LoadCreatureLocales();
    // Must be after GameobjectInfo loading
    sObjectMgr::Instance()->LoadGameObjectLocales();
    // Must be after ItemPrototypes loading
    sObjectMgr::Instance()->LoadItemLocales();
    // Must be after QuestTemplates loading
    sObjectMgr::Instance()->LoadQuestLocales();
    // Must be after LoadGossipText
    sObjectMgr::Instance()->LoadGossipTextLocales();
    // Must be after PageText loading
    sObjectMgr::Instance()->LoadPageTextLocales();
    // Must be after gossip menu items loading
    sObjectMgr::Instance()->LoadGossipMenuItemsLocales();
    // Must be after POI loading
    sObjectMgr::Instance()->LoadPointOfInterestLocales();

    ///- Load dynamic data tables from the database
    logging.info("Loading Auctions...");
    sAuctionMgr::Instance()->LoadAuctionItems();
    sAuctionMgr::Instance()->LoadAuctions();

    logging.info("Loading Guilds...");
    sGuildMgr::Instance()->LoadGuilds();

    logging.info("Loading ArenaTeams...");
    sObjectMgr::Instance()->LoadArenaTeams();

    logging.info("Loading Groups...");
    sObjectMgr::Instance()->LoadGroups();

    logging.info("Loading ReservedNames...");
    sObjectMgr::Instance()->LoadReservedPlayersNames();

    logging.info("Loading GameObjects for quests...");
    sObjectMgr::Instance()->LoadGameObjectForQuests();

    logging.info("Loading BattleMasters...");
    sBattleGroundMgr::Instance()->LoadBattleMastersEntry();

    logging.info("Loading BattleGround event indexes...");
    sBattleGroundMgr::Instance()->LoadBattleEventIndexes();

    logging.info("Loading GameTeleports...");
    sObjectMgr::Instance()->LoadGameTele();

    logging.info("Loading GM ticket data...");
    ticket_mgr::instance().load();

    ///- Handle outdated emails (delete/return)
    logging.info("Returning old mails...");
    sObjectMgr::Instance()->ReturnOrDeleteOldMails(false);

    ///- Load and initialize scripts
    logging.info("Loading Scripts...");
    // Must be after loading Creature/Gameobject(Template/Data) and
    // QuestTemplate
    sScriptMgr::Instance()->LoadQuestStartScripts();
    // Must be after loading Creature/Gameobject(Template/Data) and
    // QuestTemplate
    sScriptMgr::Instance()->LoadQuestEndScripts();
    // Must be after loading Creature/Gameobject(Template/Data)
    sScriptMgr::Instance()->LoadSpellScripts();
    // Must be after loading Creature/Gameobject(Template/Data)
    sScriptMgr::Instance()->LoadGameObjectScripts();
    // Must be after loading Creature/Gameobject(Template/Data)
    sScriptMgr::Instance()->LoadEventScripts();

    // Must be after Load*Scripts calls
    logging.info("Loading Scripts text locales...");
    sScriptMgr::Instance()->LoadDbScriptStrings();

    // false, will checked in LoadCreatureEventAI_Scripts
    logging.info("Loading CreatureEventAI Texts...");
    sEventAIMgr::Instance()->LoadCreatureEventAI_Texts(false);

    // false, will checked in LoadCreatureEventAI_Scripts
    logging.info("Loading CreatureEventAI Summons...");
    sEventAIMgr::Instance()->LoadCreatureEventAI_Summons(false);

    logging.info("Loading CreatureEventAI Scripts...");
    sEventAIMgr::Instance()->LoadCreatureEventAI_Scripts();

    logging.info("Loading Creature AI Spells...");
    BehavioralAI::LoadBehaviors();

    logging.info("Loading buffstacking data...\n");
    buff_stack_db_load();

    logging.info("Initializing Scripts...");
    switch (sScriptMgr::Instance()->LoadScriptLibrary(MANGOS_SCRIPT_NAME))
    {
    case SCRIPT_LOAD_OK:
        logging.info("Scripting library loaded.\n");
        break;
    case SCRIPT_LOAD_ERR_NOT_FOUND:
        logging.error("Scripting library not found or not accessible.\n");
        break;
    case SCRIPT_LOAD_ERR_WRONG_API:
        logging.error(
            "Scripting library has an unexpected ABI. Is it outdated?\n");
        break;
    case SCRIPT_LOAD_ERR_OUTDATED:
        logging.error(
            "Scripting library build for old mangosd revision. You need "
            "to rebuild it.\n");
        break;
    }

    logging.info("Loading Smart AI waypoints...");
    sSmartWaypointMgr::Instance()->LoadFromDB();

    logging.info("Loading Smart AI group waypoints...");
    sSmartGroupWaypointMgr::Instance()->LoadFromDB();

    logging.info("Loading Smart AI scripts...");
    sSmartScriptMgr::Instance()->LoadSmartAIFromDB();

    logging.info("Loading Creature Texts...");
    sCreatureTextMgr::Instance()->LoadCreatureTexts();

    logging.info("Loading Conditions...");
    sConditionMgr::Instance()->LoadConditions();

    logging.info("Loading all First Kills...");
    sFirstKills::Instance()->LoadAllFirstKillsFromDB();

    logging.info("Loading Ban Wave...");
    sBanWave::Instance()->load_from_db();

    logging.info("Loading Loot Selection data...");
    sLootSelection::Instance()->load_from_db();

    logging.info("Loading Pet Template data...");
    sPetTemplates::Instance()->load();

    logging.info("Loading Map Entry Requirements data...");
    sObjectMgr::Instance()->LoadMapEntryRequirements();

    ///- Initialize game time and timers
    logging.info("DEBUG:: Initialize game time and timers");
    WorldTimer::curr_sys_time = time(nullptr);
    m_startTime = WorldTimer::curr_sys_time;

    tm local;
    time_t curr;
    time(&curr);
    local = *(localtime(&curr)); // dereference and assign
    char isoDate[128];
    sprintf(isoDate, "%04d-%02d-%02d %02d:%02d:%02d", local.tm_year + 1900,
        local.tm_mon + 1, local.tm_mday, local.tm_hour, local.tm_min,
        local.tm_sec);

    LoginDatabase.PExecute(
        "INSERT INTO uptime (realmid, starttime, startstring, uptime) "
        "VALUES('%u', " UI64FMTD ", '%s', 0)",
        realmID, uint64(m_startTime), isoDate);

    m_timers[WUPDATE_WEATHERS].SetInterval(1 * IN_MILLISECONDS);
    m_timers[WUPDATE_AUCTIONS].SetInterval(MINUTE * IN_MILLISECONDS);
    m_timers[WUPDATE_UPTIME].SetInterval(
        getConfig(CONFIG_UINT32_UPTIME_UPDATE) * MINUTE * IN_MILLISECONDS);
    // Update "uptime" table based on configuration entry in minutes.
    m_timers[WUPDATE_CORPSES].SetInterval(20 * MINUTE * IN_MILLISECONDS);
    m_timers[WUPDATE_DELETECHARS].SetInterval(
        DAY * IN_MILLISECONDS); // check for chars to delete every day
    m_timers[WUPDATE_ACTIONLIMIT].SetInterval(120 * IN_MILLISECONDS);
    m_timers[WUPDATE_TICKET_MGR].SetInterval(5 * IN_MILLISECONDS);
    m_timers[WUPDATE_LEADERLESS_GROUP].SetInterval(15 * IN_MILLISECONDS);

    // to set mailtimer to return mails every day between 4 and 5 am
    // mailtimer is increased when updating auctions
    // one second is 1000 -(tested on win system)
    mail_timer =
        uint32((((localtime(&WorldTimer::curr_sys_time)->tm_hour + 20) % 24) *
                   HOUR * IN_MILLISECONDS) /
               m_timers[WUPDATE_AUCTIONS].GetInterval());
    // 1440
    mail_timer_expires = uint32(
        (DAY * IN_MILLISECONDS) / (m_timers[WUPDATE_AUCTIONS].GetInterval()));
    LOG_DEBUG(logging,
        "Mail timer set to: %u, mail return is called every %u minutes",
        mail_timer, mail_timer_expires);

    ///- Initialize static helper structures
    Player::InitVisibleBits();

    ///- Load Warden Data
    logging.info("Loading Warden Data...");
    WardenDataStorage.Init();

    ///- Initialize Battlegrounds
    logging.info("Starting BattleGround System");
    sBattleGroundMgr::Instance()->InitAutomaticArenaPointDistribution();

    ///- Initialize Outdoor PvP
    logging.info("Starting Outdoor PvP System");
    sOutdoorPvPMgr::Instance()->InitOutdoorPvP();

    // Not sure if this can be moved up in the sequence (with static data
    // loading) as it uses MapManager
    logging.info("Loading Transports...");
    sTransportMgr::Instance()->LoadTransportTemplates();
    sTransportMgr::Instance()->SpawnContinentTransports();

    logging.info("Deleting expired bans...");
    LoginDatabase.Execute(
        "DELETE FROM ip_banned WHERE unbandate<=UNIX_TIMESTAMP() AND "
        "unbandate<>bandate");

    logging.info("Calculate next daily quest reset time...");
    InitDailyQuestResetTime();

    logging.info("Starting Game Event system...");
    uint32 nextGameEvent = sGameEventMgr::Instance()->Initialize();
    m_timers[WUPDATE_EVENTS].SetInterval(nextGameEvent); // depend on next event

    // Delete all characters which have been deleted X days before
    Player::DeleteOldCharacters();

    // Load transport navmeshes
    MMAP::MMapFactory::createOrGetMMapManager()->loadTransports();

    // Load Map, VMap and MMap data on startup if this option is on
    if (getConfig(CONFIG_BOOL_LOAD_DATA_ON_STARTUP))
        sTerrainMgr::Instance()->LoadAll();

    logging.info("WORLD: World initialized");

    uint32 uStartInterval =
        WorldTimer::getMSTimeDiff(uStartTime, WorldTimer::getMSTime());
    logging.info("SERVER STARTUP TIME: %i minutes %i seconds",
        uStartInterval / 60000, (uStartInterval % 60000) / 1000);

    // FIXME: Hack. This faction template is broken in the DBC.
    if (auto t = sFactionTemplateStore.LookupEntry(1761))
    {
        const_cast<FactionTemplateEntry*>(t)->enemyFaction[0] = 995;
        const_cast<FactionTemplateEntry*>(t)->enemyFaction[1] = 996;
        const_cast<FactionTemplateEntry*>(t)->enemyFaction[2] = 997;
        const_cast<FactionTemplateEntry*>(t)->enemyFaction[3] = 998;
    }
}

void World::DetectDBCLang()
{
    uint32 m_lang_confid =
        sConfig::Instance()->GetIntDefault("DBC.Locale", 255);

    if (m_lang_confid != 255 && m_lang_confid >= MAX_LOCALE)
    {
        logging.error("Incorrect DBC.Locale! Must be >= 0 and < %d (set to 0)",
            MAX_LOCALE);
        m_lang_confid = LOCALE_enUS;
    }

    ChrRacesEntry const* race = sChrRacesStore.LookupEntry(RACE_HUMAN);
    assert(race);

    std::string availableLocalsStr;

    uint32 default_locale = MAX_LOCALE;
    for (int i = MAX_LOCALE - 1; i >= 0; --i)
    {
        if (strlen(race->name[i]) > 0) // check by race names
        {
            default_locale = i;
            m_availableDbcLocaleMask |= (1 << i);
            availableLocalsStr += localeNames[i];
            availableLocalsStr += " ";
        }
    }

    if (default_locale != m_lang_confid && m_lang_confid < MAX_LOCALE &&
        (m_availableDbcLocaleMask & (1 << m_lang_confid)))
    {
        default_locale = m_lang_confid;
    }

    if (default_locale >= MAX_LOCALE)
    {
        logging.error("Unable to determine your DBC Locale! (corrupt DBC?)");

        exit(1);
    }

    m_defaultDbcLocale = LocaleConstant(default_locale);

    logging.info(
        "Using %s DBC Locale as default. All available DBC locales: %s\n",
        localeNames[m_defaultDbcLocale],
        availableLocalsStr.empty() ? "<none>" : availableLocalsStr.c_str());
}

/// Update the World !
void World::Update(uint32 diff)
{
    ///- Update the different timers
    for (auto& elem : m_timers)
    {
        if (elem.GetCurrent() >= 0)
            elem.Update(diff);
        else
            elem.SetCurrent(0);
    }

    ///- Update the game time and check for shutdown time
    _UpdateGameTime();

    ///-Update mass mailer tasks if any
    sMassMailMgr::Instance()->Update();

    /// Handle daily quests reset time
    if (WorldTimer::curr_sys_time > m_NextDailyQuestReset)
        ResetDailyQuests();

    /// <ul><li> Handle auctions when the timer has passed
    if (m_timers[WUPDATE_AUCTIONS].Passed())
    {
        m_timers[WUPDATE_AUCTIONS].Reset();

        ///- Update mails (return old mails with item, or delete them)
        //(tested... works on win)
        if (++mail_timer > mail_timer_expires)
        {
            mail_timer = 0;
            sObjectMgr::Instance()->ReturnOrDeleteOldMails(true);
        }

        ///- Handle expired auctions
        sAuctionMgr::Instance()->Update();
    }

    // Handle resetting of storage limits in guild banks (once per day)
    sGuildMgr::Instance()->reset_storages_if_midnight();

    if (defensive_mode_level_ != 0 &&
        defmode_last_reminder_ + DEFMODE_REMINDER_TIME <
            WorldTimer::getMSTime())
        defmode_gm_reminder(); // resets timer

    // clean action limits
    if (m_timers[WUPDATE_ACTIONLIMIT].Passed())
    {
        sActionLimit::Instance()->clean();
        m_timers[WUPDATE_ACTIONLIMIT].Reset();
    }

    // update the ticket manager
    if (m_timers[WUPDATE_TICKET_MGR].Passed())
    {
        ticket_mgr::instance().update();
        m_timers[WUPDATE_TICKET_MGR].Reset();
    }

    // Update groups were leader went offline
    if (m_timers[WUPDATE_LEADERLESS_GROUP].Passed())
    {
        sObjectMgr::Instance()->UpdateGroupsWithOfflineLeader();
        m_timers[WUPDATE_LEADERLESS_GROUP].Reset();
    }

    NETSTAT_WRITE_LOGS();

    /// <li> Handle session updates
    UpdateSessions(diff);

    /** Check for players in the pending queue -- in Update() because we allow
     * recently disconnected
     *  players to log back on within a specified timeframe. so we need to
     * utilize the heartbeat poll. */
    check_and_release_queue(true);

    /// <li> Handle weather updates when the timer has passed
    if (m_timers[WUPDATE_WEATHERS].Passed())
    {
        ///- Send an update signal to Weather objects
        for (auto itr = m_weathers.begin(); itr != m_weathers.end();)
        {
            ///- and remove Weather objects for zones with no player
            // As interval > WorldTick
            if (!itr->second->Update(m_timers[WUPDATE_WEATHERS].GetInterval()))
            {
                delete itr->second;
                m_weathers.erase(itr++);
            }
            else
                ++itr;
        }

        m_timers[WUPDATE_WEATHERS].SetCurrent(0);
    }
    /// <li> Update uptime table
    if (m_timers[WUPDATE_UPTIME].Passed())
    {
        uint32 tmpDiff = uint32(WorldTimer::curr_sys_time - m_startTime);
        uint32 maxClientsNum = GetMaxActiveSessionCount();

        m_timers[WUPDATE_UPTIME].Reset();
        LoginDatabase.PExecute(
            "UPDATE uptime SET uptime = %u, maxplayers = %u WHERE realmid = %u "
            "AND starttime = " UI64FMTD,
            tmpDiff, maxClientsNum, realmID, uint64(m_startTime));
    }

    /// <li> Handle all other objects
    ///- Update objects (maps, creatures,...)
    MapManager* mm = sMapMgr::Instance()->get_inst(); // FIXME: Ugly hack
                                                      // dealing with the fact
                                                      // that we shouldn't lock
                                                      // this to call update,
    mm->Update(diff); // Comes about because everything in this core is a
                      // friggin singleton
    sBattleGroundMgr::Instance()->Update(diff);
    sOutdoorPvPMgr::Instance()->Update(diff);

    ///- Delete all characters which have been deleted X days before
    if (m_timers[WUPDATE_DELETECHARS].Passed())
    {
        m_timers[WUPDATE_DELETECHARS].Reset();
        Player::DeleteOldCharacters();
    }

    // execute callbacks from sql queries that were queued recently
    UpdateResultQueue();

    ///- Erase corpses once every 20 minutes
    if (m_timers[WUPDATE_CORPSES].Passed())
    {
        m_timers[WUPDATE_CORPSES].Reset();

        sObjectAccessor::Instance()->RemoveOldCorpses();
    }

    ///- Process Game events when necessary
    if (m_timers[WUPDATE_EVENTS].Passed())
    {
        m_timers[WUPDATE_EVENTS]
            .Reset(); // to give time for Update() to be processed
        uint32 nextGameEvent = sGameEventMgr::Instance()->Update();
        m_timers[WUPDATE_EVENTS].SetInterval(nextGameEvent);
        m_timers[WUPDATE_EVENTS].Reset();
    }

    // Update Alliance and Horde online count each minute
    if (m_lastFacPlCountTime < WorldTimer::curr_sys_time - 60)
    {
        sObjectAccessor::Instance()->UpdateAllianceHordeCount();
        m_lastFacPlCountTime = WorldTimer::curr_sys_time;
    }

    /// </ul>
    ///- Move all creatures with "delayed move" and remove and delete all
    /// objects with "delayed remove"
    sMapMgr::Instance()->RemoveAllObjectsInRemoveList();

    // update the instance reset times
    sMapPersistentStateMgr::Instance()->Update();

    // And last, but not least handle the issued cli commands
    process_cli_commands();

    // cleanup spells that were not deletable, but now are
    for (auto itr = spell_backburner_.begin(); itr != spell_backburner_.end();)
    {
        if ((*itr)->IsDeletable())
        {
            delete *itr;
            itr = spell_backburner_.erase(itr);
        }
        else
        {
            ++itr;
        }
    }
}

/// Send a packet to all players (except self if mentioned)
void World::SendGlobalMessage(
    WorldPacket* packet, WorldSession* self, uint32 team)
{
    SessionMap::const_iterator itr;
    for (itr = m_sessions.begin(); itr != m_sessions.end(); ++itr)
    {
        if (itr->second && itr->second->GetPlayer() &&
            itr->second->GetPlayer()->IsInWorld() &&
            itr->second.get() != self &&
            (team == 0 || itr->second->GetPlayer()->GetTeam() == team))
        {
            itr->second->send_packet(packet);
        }
    }
}

namespace MaNGOS
{
class WorldWorldTextBuilder
{
public:
    typedef std::vector<WorldPacket*> WorldPacketList;
    explicit WorldWorldTextBuilder(int32 textId, va_list* args = nullptr)
      : i_textId(textId), i_args(args)
    {
    }
    void operator()(WorldPacketList& data_list, int32 loc_idx)
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

            do_helper(data_list, &str[0]);
        }
        else
            do_helper(data_list, (char*)text);
    }

private:
    char* lineFromMessage(char*& pos)
    {
        char* start = strtok(pos, "\n");
        pos = nullptr;
        return start;
    }
    void do_helper(WorldPacketList& data_list, char* text)
    {
        char* pos = text;

        while (char* line = lineFromMessage(pos))
        {
            auto data = new WorldPacket();

            uint32 lineLength = (line ? strlen(line) : 0) + 1;

            data->initialize(SMSG_MESSAGECHAT, 100); // guess size
            *data << uint8(CHAT_MSG_SYSTEM);
            *data << uint32(LANG_UNIVERSAL);
            *data << uint64(0);
            *data << uint32(0); // can be chat msg group or something
            *data << uint64(0);
            *data << uint32(lineLength);
            *data << line;
            *data << uint8(0);

            data_list.push_back(data);
        }
    }

    int32 i_textId;
    va_list* i_args;
};
} // namespace MaNGOS

/// Send a System Message to all players (except self if mentioned)
void World::SendWorldText(int32 string_id, ...)
{
    va_list ap;
    va_start(ap, string_id);

    MaNGOS::WorldWorldTextBuilder wt_builder(string_id, &ap);
    auto wt_do = maps::callbacks::make_localize_packets(wt_builder);
    for (SessionMap::const_iterator itr = m_sessions.begin();
         itr != m_sessions.end(); ++itr)
    {
        if (!itr->second || !itr->second->GetPlayer() ||
            !itr->second->GetPlayer()->IsInWorld())
            continue;

        wt_do(itr->second->GetPlayer());
    }

    va_end(ap);
}

/// DEPRICATED, only for debug purpose. Send a System Message to all players
/// (except self if mentioned)
void World::SendGlobalText(const char* text, WorldSession* self)
{
    WorldPacket data;

    // need copy to prevent corruption by strtok call in LineFromMessage
    // original string
    char* buf = mangos_strdup(text);
    char* pos = buf;

    while (char* line = ChatHandler::LineFromMessage(pos))
    {
        ChatHandler::FillMessageData(
            &data, nullptr, CHAT_MSG_SYSTEM, LANG_UNIVERSAL, line);
        SendGlobalMessage(&data, self);
    }

    delete[] buf;
}

/// Send a packet to all players (or players selected team) in the zone (except
/// self if mentioned)
void World::SendZoneMessage(
    uint32 zone, WorldPacket* packet, WorldSession* self, uint32 team)
{
    SessionMap::const_iterator itr;
    for (itr = m_sessions.begin(); itr != m_sessions.end(); ++itr)
    {
        if (itr->second && itr->second->GetPlayer() &&
            itr->second->GetPlayer()->IsInWorld() &&
            itr->second->GetPlayer()->GetZoneId() == zone &&
            itr->second.get() != self &&
            (team == 0 || itr->second->GetPlayer()->GetTeam() == team))
        {
            itr->second->send_packet(packet);
        }
    }
}

/// Send a System Message to all players in the zone (except self if mentioned)
void World::SendZoneText(
    uint32 zone, const char* text, WorldSession* self, uint32 team)
{
    WorldPacket data;
    ChatHandler::FillMessageData(
        &data, nullptr, CHAT_MSG_SYSTEM, LANG_UNIVERSAL, text);
    SendZoneMessage(zone, &data, self, team);
}

/// Kick (and save) all players
void World::KickAll()
{
    m_QueuedSessions
        .clear(); // prevent send queue update packet and login queued sessions

    // session not removed at kick and will removed in next update tick
    for (SessionMap::const_iterator itr = m_sessions.begin();
         itr != m_sessions.end(); ++itr)
        itr->second->KickPlayer();
}

/// Kick (and save) all players with security level less `sec`
void World::KickAllLess(AccountTypes sec)
{
    // session not removed at kick and will removed in next update tick
    for (SessionMap::const_iterator itr = m_sessions.begin();
         itr != m_sessions.end(); ++itr)
        if (itr->second->GetSecurity() < sec)
            itr->second->KickPlayer();
}

/// Ban an account or ban an IP address, duration_secs if it is positive used,
/// otherwise permban
BanReturn World::BanAccount(BanMode mode, std::string nameOrIP,
    uint32 duration_secs, std::string reason, std::string author)
{
    LoginDatabase.escape_string(nameOrIP);
    LoginDatabase.escape_string(reason);
    std::string safe_author = author;
    LoginDatabase.escape_string(safe_author);

    QueryResult* resultAccounts = nullptr; // used for kicking

    ///- Update the database with ban information
    switch (mode)
    {
    case BAN_IP:
        // No SQL injection as strings are escaped
        resultAccounts = LoginDatabase.PQuery(
            "SELECT id FROM account WHERE last_ip = '%s'", nameOrIP.c_str());
        LoginDatabase.PExecute(
            "INSERT INTO ip_banned VALUES "
            "('%s',UNIX_TIMESTAMP(),UNIX_TIMESTAMP()+%u,'%s','%s')",
            nameOrIP.c_str(), duration_secs, safe_author.c_str(),
            reason.c_str());
        break;
    case BAN_ACCOUNT:
        // No SQL injection as string is escaped
        resultAccounts = LoginDatabase.PQuery(
            "SELECT id FROM account WHERE username = '%s'", nameOrIP.c_str());
        break;
    case BAN_CHARACTER:
        // No SQL injection as string is escaped
        resultAccounts = CharacterDatabase.PQuery(
            "SELECT account FROM characters WHERE name = '%s'",
            nameOrIP.c_str());
        break;
    default:
        return BAN_SYNTAX_ERROR;
    }

    if (!resultAccounts)
    {
        if (mode == BAN_IP)
            return BAN_SUCCESS; // ip correctly banned but nobody affected (yet)
        else
            return BAN_NOTFOUND; // Nobody to ban
    }

    ///- Disconnect all affected players (for IP it can be several)
    do
    {
        Field* fieldsAccount = resultAccounts->Fetch();
        uint32 account = fieldsAccount->GetUInt32();

        if (mode != BAN_IP)
        {
            // No SQL injection as strings are escaped
            LoginDatabase.PExecute(
                "INSERT INTO account_banned VALUES ('%u', UNIX_TIMESTAMP(), "
                "UNIX_TIMESTAMP()+%u, '%s', '%s', '1')",
                account, duration_secs, safe_author.c_str(), reason.c_str());
        }

        if (WorldSession* sess = FindSession(account))
            if (std::string(sess->GetPlayerName()) != author)
                sess->KickPlayer();
    } while (resultAccounts->NextRow());

    delete resultAccounts;
    return BAN_SUCCESS;
}

/// Remove a ban from an account or IP address
bool World::RemoveBanAccount(BanMode mode, std::string nameOrIP)
{
    if (mode == BAN_IP)
    {
        LoginDatabase.escape_string(nameOrIP);
        LoginDatabase.PExecute(
            "DELETE FROM ip_banned WHERE ip = '%s'", nameOrIP.c_str());
    }
    else
    {
        uint32 account = 0;
        if (mode == BAN_ACCOUNT)
            account = sAccountMgr::Instance()->GetId(nameOrIP);
        else if (mode == BAN_CHARACTER)
            account = sObjectMgr::Instance()->GetPlayerAccountIdByPlayerName(
                nameOrIP);

        if (!account)
            return false;

        // NO SQL injection as account is uint32
        LoginDatabase.PExecute(
            "UPDATE account_banned SET active = '0' WHERE id = '%u'", account);
    }
    return true;
}

/// Update the game time
void World::_UpdateGameTime()
{
    ///- update the time
    time_t thisTime = time(nullptr);
    uint32 elapsed = uint32(thisTime - WorldTimer::curr_sys_time);
    WorldTimer::curr_sys_time = thisTime;

    ///- if there is a shutdown timer
    if (!m_stopEvent && m_ShutdownTimer > 0 && elapsed > 0)
    {
        ///- ... and it is overdue, stop the world (set m_stopEvent)
        if (m_ShutdownTimer <= elapsed)
        {
            if (!(m_ShutdownMask & SHUTDOWN_MASK_IDLE) ||
                GetActiveAndQueuedSessionCount() == 0)
                m_stopEvent = true; // exist code already set
            else
                m_ShutdownTimer = 1; // minimum timer value to wait idle state
        }
        ///- ... else decrease it and if necessary display a shutdown countdown
        /// to the users
        else
        {
            m_ShutdownTimer -= elapsed;

            ShutdownMsg();
        }
    }
}

/// Shutdown the server
void World::ShutdownServ(uint32 time, uint32 options, uint8 exitcode)
{
    // ignore if server shutdown at next tick
    if (m_stopEvent)
        return;

    m_ShutdownMask = options;
    m_ExitCode = exitcode;

    ///- If the shutdown time is 0, set m_stopEvent (except if shutdown is
    ///'idle' with remaining sessions)
    if (time == 0)
    {
        if (!(options & SHUTDOWN_MASK_IDLE) ||
            GetActiveAndQueuedSessionCount() == 0)
            m_stopEvent = true; // exist code already set
        else
            m_ShutdownTimer = 1; // So that the session count is re-evaluated at
                                 // next world tick
    }
    ///- Else set the shutdown timer and warn users
    else
    {
        m_ShutdownTimer = time;
        ShutdownMsg(true);
    }
}

/// Display a shutdown message to the user(s)
void World::ShutdownMsg(bool show, Player* player)
{
    // not show messages for idle shutdown mode
    if (m_ShutdownMask & SHUTDOWN_MASK_IDLE)
        return;

    ///- Display a message every 12 hours, 1 hour, 5 minutes, 1 minute and 15
    /// seconds
    if (show || (m_ShutdownTimer < 5 * MINUTE &&
                    (m_ShutdownTimer % 15) == 0) || // < 5 min; every 15 sec
        (m_ShutdownTimer < 15 * MINUTE &&
            (m_ShutdownTimer % MINUTE) == 0) || // < 15 min; every 1 min
        (m_ShutdownTimer < 30 * MINUTE &&
            (m_ShutdownTimer % (5 * MINUTE)) == 0) || // < 30 min; every 5 min
        (m_ShutdownTimer < 12 * HOUR &&
            (m_ShutdownTimer % HOUR) == 0) || // < 12 h; every 1 h
        (m_ShutdownTimer >= 12 * HOUR &&
            (m_ShutdownTimer % (12 * HOUR)) == 0)) // >= 12 h; every 12 h
    {
        std::string str = secsToTimeString(m_ShutdownTimer);

        ServerMessageType msgid = (m_ShutdownMask & SHUTDOWN_MASK_RESTART) ?
                                      SERVER_MSG_RESTART_TIME :
                                      SERVER_MSG_SHUTDOWN_TIME;

        SendServerMessage(msgid, str.c_str(), player);
        LOG_DEBUG(logging, "Server is %s in %s",
            (m_ShutdownMask & SHUTDOWN_MASK_RESTART ? "restart" :
                                                      "shutting down"),
            str.c_str());
    }
}

/// Cancel a planned server shutdown
void World::ShutdownCancel()
{
    // nothing cancel or too later
    if (!m_ShutdownTimer || m_stopEvent)
        return;

    ServerMessageType msgid = (m_ShutdownMask & SHUTDOWN_MASK_RESTART) ?
                                  SERVER_MSG_RESTART_CANCELLED :
                                  SERVER_MSG_SHUTDOWN_CANCELLED;

    m_ShutdownMask = 0;
    m_ShutdownTimer = 0;
    m_ExitCode = SHUTDOWN_EXIT_CODE; // to default value
    SendServerMessage(msgid);

    LOG_DEBUG(logging, "Server %s cancelled.",
        (m_ShutdownMask & SHUTDOWN_MASK_RESTART ? "restart" : "shutdown"));
}

/// Send a server message to the user(s)
void World::SendServerMessage(
    ServerMessageType type, const char* text, Player* player)
{
    WorldPacket data(SMSG_SERVER_MESSAGE, 50); // guess size
    data << uint32(type);
    data << text;

    if (player)
        player->GetSession()->send_packet(std::move(data));
    else
        SendGlobalMessage(&data);
}

void World::UpdateSessions(uint32 diff)
{
    ///- Add new sessions
    std::shared_ptr<WorldSession> sess;
    while (addSessQueue.pop(sess))
        AddSession_(std::move(sess));

    ///- Then send an update signal to remaining ones
    for (SessionMap::iterator itr = m_sessions.begin(), next;
         itr != m_sessions.end(); itr = next)
    {
        next = itr;
        ++next;
        ///- and remove not active sessions from the list
        WorldSessionFilter updater(itr->second.get());

        if (!itr->second->Update(updater, diff))
        {
            RemoveQueuedSession(itr->second.get());
            m_sessions.erase(itr);
        }
    }

    for (auto itr = expired_sessions_.begin(); itr != expired_sessions_.end();)
    {
        WorldSessionFilter updater(itr->get());
        if (!(*itr)->Update(updater, diff))
        {
            // Can't be in queue, only in-game sessions can 'expire'
            itr = expired_sessions_.erase(itr);
        }
        else
            ++itr;
    }
}

// This handles the issued and queued CLI/RA commands
void World::process_cli_commands()
{
    cli_command* command = nullptr;
    while (cli_cmd_queue.pop(command))
    {
        LOG_DEBUG(
            logging, "World::process_cli_commands: Processing CLI Command");

        cli_cmd_handler handler(*command);
        handler.handle();

        command->complete();
        delete command;
    }
}

void World::InitResultQueue()
{
}

void World::UpdateResultQueue()
{
    // process async result queues
    CharacterDatabase.ProcessResultQueue();
    WorldDatabase.ProcessResultQueue();
    LoginDatabase.ProcessResultQueue();
}

void World::UpdateRealmCharCount(uint32 accountId)
{
    CharacterDatabase.AsyncPQuery(this, &World::_UpdateRealmCharCount,
        accountId, "SELECT COUNT(guid) FROM characters WHERE account = '%u'",
        accountId);
}

void World::_UpdateRealmCharCount(
    QueryResult* resultCharCount, uint32 accountId)
{
    if (resultCharCount)
    {
        Field* fields = resultCharCount->Fetch();
        uint32 charCount = fields[0].GetUInt32();
        delete resultCharCount;

        LoginDatabase.BeginTransaction();
        LoginDatabase.PExecute(
            "DELETE FROM realmcharacters WHERE acctid= '%u' AND realmid = '%u'",
            accountId, realmID);
        LoginDatabase.PExecute(
            "INSERT INTO realmcharacters (numchars, acctid, realmid) VALUES "
            "(%u, %u, %u)",
            charCount, accountId, realmID);
        LoginDatabase.CommitTransaction();
    }
}

void World::InitDailyQuestResetTime()
{
    QueryResult* result = CharacterDatabase.Query(
        "SELECT NextDailyQuestResetTime FROM saved_variables");
    if (!result)
        m_NextDailyQuestReset = time_t(time(nullptr)); // game time not yet init
    else
        m_NextDailyQuestReset = time_t((*result)[0].GetUInt64());

    // generate time by config
    time_t curTime = time(nullptr);
    tm localTm = *localtime(&curTime);
    localTm.tm_hour = getConfig(CONFIG_UINT32_QUEST_DAILY_RESET_HOUR);
    localTm.tm_min = 0;
    localTm.tm_sec = 0;

    // current day reset time
    time_t nextDayResetTime = mktime(&localTm);

    // next reset time before current moment
    if (curTime >= nextDayResetTime)
        nextDayResetTime += DAY;

    // normalize reset time
    m_NextDailyQuestReset = m_NextDailyQuestReset < curTime ?
                                nextDayResetTime - DAY :
                                nextDayResetTime;

    if (!result)
        CharacterDatabase.PExecute(
            "INSERT INTO saved_variables (NextDailyQuestResetTime) VALUES "
            "('" UI64FMTD "')",
            uint64(m_NextDailyQuestReset));
    else
        delete result;
}

void World::ResetDailyQuests()
{
    LOG_DEBUG(logging, "Daily quests reset for all characters.");
    CharacterDatabase.Execute("DELETE FROM character_queststatus_daily");
    for (SessionMap::const_iterator itr = m_sessions.begin();
         itr != m_sessions.end(); ++itr)
        if (itr->second->GetPlayer())
            itr->second->GetPlayer()->ResetDailyQuestStatus();

    m_NextDailyQuestReset = time_t(m_NextDailyQuestReset + DAY);
    CharacterDatabase.PExecute(
        "UPDATE saved_variables SET NextDailyQuestResetTime = '" UI64FMTD "'",
        uint64(m_NextDailyQuestReset));
}

void World::SetPlayerLimit(int32 limit, bool needUpdate)
{
    if (limit < -SEC_FULL_GM)
        limit = -SEC_FULL_GM;

    // lock update need
    bool db_update_need =
        needUpdate || (limit < 0) != (m_playerLimit < 0) ||
        (limit < 0 && m_playerLimit < 0 && limit != m_playerLimit);

    m_playerLimit = limit;

    if (db_update_need)
        LoginDatabase.PExecute(
            "UPDATE realmlist SET allowedSecurityLevel = '%u' WHERE id = '%u'",
            uint32(GetPlayerSecurityLimit()), realmID);
}

void World::UpdateMaxSessionCounters()
{
    m_maxActiveSessionCount = std::max(m_maxActiveSessionCount,
        uint32(m_sessions.size() - m_QueuedSessions.size()));
    m_maxQueuedSessionCount =
        std::max(m_maxQueuedSessionCount, uint32(m_QueuedSessions.size()));
}

void World::setConfig(
    eConfigUInt32Values index, char const* fieldname, uint32 defvalue)
{
    setConfig(index, sConfig::Instance()->GetIntDefault(fieldname, defvalue));
    if (int32(getConfig(index)) < 0)
    {
        logging.error("%s (%i) can't be negative. Using %u instead.", fieldname,
            int32(getConfig(index)), defvalue);
        setConfig(index, defvalue);
    }
}

void World::setConfig(
    eConfigInt32Values index, char const* fieldname, int32 defvalue)
{
    setConfig(index, sConfig::Instance()->GetIntDefault(fieldname, defvalue));
}

void World::setConfig(
    eConfigFloatValues index, char const* fieldname, float defvalue)
{
    setConfig(index, sConfig::Instance()->GetFloatDefault(fieldname, defvalue));
}

void World::setConfig(
    eConfigBoolValues index, char const* fieldname, bool defvalue)
{
    setConfig(index, sConfig::Instance()->GetBoolDefault(fieldname, defvalue));
}

void World::setConfigPos(
    eConfigFloatValues index, char const* fieldname, float defvalue)
{
    setConfig(index, fieldname, defvalue);
    if (getConfig(index) < 0.0f)
    {
        logging.error("%s (%f) can't be negative. Using %f instead.", fieldname,
            getConfig(index), defvalue);
        setConfig(index, defvalue);
    }
}

void World::setConfigMin(eConfigUInt32Values index, char const* fieldname,
    uint32 defvalue, uint32 minvalue)
{
    setConfig(index, fieldname, defvalue);
    if (getConfig(index) < minvalue)
    {
        logging.error("%s (%u) must be >= %u. Using %u instead.", fieldname,
            getConfig(index), minvalue, minvalue);
        setConfig(index, minvalue);
    }
}

void World::setConfigMin(eConfigInt32Values index, char const* fieldname,
    int32 defvalue, int32 minvalue)
{
    setConfig(index, fieldname, defvalue);
    if (getConfig(index) < minvalue)
    {
        logging.error("%s (%i) must be >= %i. Using %i instead.", fieldname,
            getConfig(index), minvalue, minvalue);
        setConfig(index, minvalue);
    }
}

void World::setConfigMin(eConfigFloatValues index, char const* fieldname,
    float defvalue, float minvalue)
{
    setConfig(index, fieldname, defvalue);
    if (getConfig(index) < minvalue)
    {
        logging.error("%s (%f) must be >= %f. Using %f instead.", fieldname,
            getConfig(index), minvalue, minvalue);
        setConfig(index, minvalue);
    }
}

void World::setConfigMinMax(eConfigUInt32Values index, char const* fieldname,
    uint32 defvalue, uint32 minvalue, uint32 maxvalue)
{
    setConfig(index, fieldname, defvalue);
    if (getConfig(index) < minvalue)
    {
        logging.error("%s (%u) must be in range %u...%u. Using %u instead.",
            fieldname, getConfig(index), minvalue, maxvalue, minvalue);
        setConfig(index, minvalue);
    }
    else if (getConfig(index) > maxvalue)
    {
        logging.error("%s (%u) must be in range %u...%u. Using %u instead.",
            fieldname, getConfig(index), minvalue, maxvalue, maxvalue);
        setConfig(index, maxvalue);
    }
}

void World::setConfigMinMax(eConfigInt32Values index, char const* fieldname,
    int32 defvalue, int32 minvalue, int32 maxvalue)
{
    setConfig(index, fieldname, defvalue);
    if (getConfig(index) < minvalue)
    {
        logging.error("%s (%i) must be in range %i...%i. Using %i instead.",
            fieldname, getConfig(index), minvalue, maxvalue, minvalue);
        setConfig(index, minvalue);
    }
    else if (getConfig(index) > maxvalue)
    {
        logging.error("%s (%i) must be in range %i...%i. Using %i instead.",
            fieldname, getConfig(index), minvalue, maxvalue, maxvalue);
        setConfig(index, maxvalue);
    }
}

void World::setConfigMinMax(eConfigFloatValues index, char const* fieldname,
    float defvalue, float minvalue, float maxvalue)
{
    setConfig(index, fieldname, defvalue);
    if (getConfig(index) < minvalue)
    {
        logging.error("%s (%f) must be in range %f...%f. Using %f instead.",
            fieldname, getConfig(index), minvalue, maxvalue, minvalue);
        setConfig(index, minvalue);
    }
    else if (getConfig(index) > maxvalue)
    {
        logging.error("%s (%f) must be in range %f...%f. Using %f instead.",
            fieldname, getConfig(index), minvalue, maxvalue, maxvalue);
        setConfig(index, maxvalue);
    }
}

bool World::configNoReload(bool reload, eConfigUInt32Values index,
    char const* fieldname, uint32 defvalue)
{
    if (!reload)
        return true;

    uint32 val = sConfig::Instance()->GetIntDefault(fieldname, defvalue);
    if (val != getConfig(index))
        logging.error(
            "%s option can't be changed at mangosd.conf reload, using current "
            "value (%u).",
            fieldname, getConfig(index));

    return false;
}

bool World::configNoReload(bool reload, eConfigInt32Values index,
    char const* fieldname, int32 defvalue)
{
    if (!reload)
        return true;

    int32 val = sConfig::Instance()->GetIntDefault(fieldname, defvalue);
    if (val != getConfig(index))
        logging.error(
            "%s option can't be changed at mangosd.conf reload, using current "
            "value (%i).",
            fieldname, getConfig(index));

    return false;
}

bool World::configNoReload(bool reload, eConfigFloatValues index,
    char const* fieldname, float defvalue)
{
    if (!reload)
        return true;

    float val = sConfig::Instance()->GetFloatDefault(fieldname, defvalue);
    if (val != getConfig(index))
        logging.error(
            "%s option can't be changed at mangosd.conf reload, using current "
            "value (%f).",
            fieldname, getConfig(index));

    return false;
}

bool World::configNoReload(
    bool reload, eConfigBoolValues index, char const* fieldname, bool defvalue)
{
    if (!reload)
        return true;

    bool val = sConfig::Instance()->GetBoolDefault(fieldname, defvalue);
    if (val != getConfig(index))
        logging.error(
            "%s option can't be changed at mangosd.conf reload, using current "
            "value (%s).",
            fieldname, getConfig(index) ? "'true'" : "'false'");

    return false;
}

void World::add_recently_logged_off(uint32 accountId)
{
    // 0 == feature disabled
    if (recent_logoff_threshold_ == 0)
        return;

    std::lock_guard<std::mutex> guard(recently_loggedoff_mutex_);

    time_t timeout = WorldTimer::time_no_syscall() + recent_logoff_threshold_;

    // In case they were already in the list... std::map::insert won't update an
    // existing value if the key already exists.
    recently_loggedoff_accts_.erase(accountId);

    recently_loggedoff_accts_.insert(
        std::pair<uint32, time_t>(accountId, timeout));
}

void World::prune_recently_logged_off()
{
    if (recent_logoff_threshold_ == 0)
        return;

    std::lock_guard<std::mutex> guard(recently_loggedoff_mutex_);

    for (auto itr = recently_loggedoff_accts_.begin();
         itr != recently_loggedoff_accts_.end();
        /*empty*/)
    {
        if (itr->second <= WorldTimer::time_no_syscall())
            itr = recently_loggedoff_accts_.erase(itr);
        else
            ++itr;
    }
}

bool World::may_skip_queue(uint32 accountId)
{
    if (recent_logoff_threshold_ == 0)
        return false;

    std::lock_guard<std::mutex> guard(recently_loggedoff_mutex_);

    auto itr = recently_loggedoff_accts_.find(accountId);

    if (itr != recently_loggedoff_accts_.end())
        if (itr->second > WorldTimer::time_no_syscall())
            return true;

    return false;
}

/// Sends a world defense message to all players not in an instance
void World::SendDefenseMessage(uint32 zoneId, int32 textId)
{
    for (SessionMap::const_iterator itr = m_sessions.begin();
         itr != m_sessions.end(); ++itr)
    {
        if (itr->second && itr->second->GetPlayer() &&
            itr->second->GetPlayer()->IsInWorld() &&
            !itr->second->GetPlayer()->GetMap()->Instanceable())
        {
            char const* message = itr->second->GetMangosString(textId);
            uint32 messageLength = strlen(message) + 1;

            WorldPacket data(SMSG_DEFENSE_MESSAGE, 4 + 4 + messageLength);
            data << uint32(zoneId);
            data << uint32(messageLength);
            data << message;
            itr->second->send_packet(std::move(data));
        }
    }
}

void World::defmode_level(uint32 level)
{
    defensive_mode_level_ = level;
    defensive_mode_kicked_ = 0;
    defensive_mode_accepted_ = 0;
    defmode_last_reminder_ = WorldTimer::getMSTime();
}

void World::defmode_stats(uint32 add_kicked, uint32 add_let_through)
{
    std::lock_guard<std::mutex> guard(defmode_stats_mutex_);

    defensive_mode_kicked_ += add_kicked;
    defensive_mode_accepted_ += add_let_through;
}

std::pair<uint32, uint32> World::defmode_stats() const
{
    std::lock_guard<std::mutex> guard(defmode_stats_mutex_);
    return std::make_pair(defensive_mode_kicked_, defensive_mode_accepted_);
}

void World::defmode_gm_reminder()
{
    auto pair = defmode_stats();

    HashMapHolder<Player>::LockedContainer m =
        sObjectAccessor::Instance()->GetPlayers();
    for (HashMapHolder<Player>::MapType::const_iterator itr = m.get().begin();
         itr != m.get().end(); ++itr)
    {
        AccountTypes itr_sec = itr->second->GetSession()->GetSecurity();
        if (itr_sec > SEC_PLAYER)
        {
            ChatHandler(itr->second)
                .PSendSysMessage(
                    "Defensive Mode is still activated (blocked: %u, let in: "
                    "%u).",
                    pair.first, pair.second);
        }
    }

    defmode_last_reminder_ = WorldTimer::getMSTime();
}
