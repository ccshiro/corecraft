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

#include "MapPersistentStateMgr.h"
#include "GameEventMgr.h"
#include "Group.h"
#include "InstanceData.h"
#include "logging.h"
#include "Map.h"
#include "MapManager.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "ProgressBar.h"
#include "SQLStorages.h"
#include "Timer.h"
#include "Transport.h"
#include "World.h"

static uint32 resetEventTypeDelay[MAX_RESET_EVENT_TYPE] = {
    0, 3600, 900, 300, 60};

//== MapPersistentState functions ==========================
MapPersistentState::MapPersistentState(
    uint16 MapId, uint32 InstanceId, Difficulty difficulty)
  : m_instanceid(InstanceId), m_mapid(MapId), m_difficulty(difficulty),
    m_usedByMap(nullptr)
{
    persistent_game_objects_.set_empty_key(0);
    persistent_game_objects_.set_deleted_key(1);
    persistent_creatures_.set_empty_key(0);
    persistent_creatures_.set_deleted_key(1);
}

MapPersistentState::~MapPersistentState()
{
}

MapEntry const* MapPersistentState::GetMapEntry() const
{
    return sMapStore.LookupEntry(m_mapid);
}

/* true if the instance state is still valid */
bool MapPersistentState::UnloadIfEmpty(bool db_delete)
{
    if (CanBeUnload())
    {
        auto state = dynamic_cast<DungeonPersistentState*>(this);
        if (state && db_delete)
            state->DeleteFromDB();

        if (GetInstanceId() && (db_delete || !state))
            sMapPersistentStateMgr::Instance()->free_instance_id(
                GetInstanceId());

        sMapPersistentStateMgr::Instance()->RemovePersistentState(
            GetMapId(), GetInstanceId());

        return false;
    }
    else
        return true;
}

void MapPersistentState::SaveCreatureRespawnTime(
    uint32 loguid, time_t t, uint32 spawntimesecs)
{
    SetCreatureRespawnTime(loguid, t);

    // BGs/Arenas always reset at server restart/unload, so no reason store in
    // DB
    if (GetMapEntry()->IsBattleGroundOrArena())
        return;

    // We only save creatures with a rather long respawn timer to the DB, after
    // all,
    // if it's less than 30 minutes it hardly matters if it respawns on server
    // restart
    if (spawntimesecs < SAVE_CREATURE_MIN_RESPAWN_DELAY)
        return;

    CharacterDatabase.BeginTransaction();

    static SqlStatementID delSpawnTime;
    static SqlStatementID insSpawnTime;

    SqlStatement stmt = CharacterDatabase.CreateStatement(delSpawnTime,
        "DELETE FROM creature_respawn WHERE guid = ? AND instance = ?");
    stmt.PExecute(loguid, m_instanceid);

    if (t > WorldTimer::time_no_syscall())
    {
        stmt = CharacterDatabase.CreateStatement(
            insSpawnTime, "INSERT INTO creature_respawn VALUES ( ?, ?, ?, ? )");
        stmt.PExecute(loguid, uint64(t), m_instanceid, m_mapid);
    }

    CharacterDatabase.CommitTransaction();
}

void MapPersistentState::SaveGORespawnTime(uint32 loguid, time_t t)
{
    SetGORespawnTime(loguid, t);

    // BGs/Arenas always reset at server restart/unload, so no reason store in
    // DB
    if (GetMapEntry()->IsBattleGroundOrArena())
        return;

    CharacterDatabase.BeginTransaction();

    static SqlStatementID delSpawnTime;
    static SqlStatementID insSpawnTime;

    SqlStatement stmt = CharacterDatabase.CreateStatement(delSpawnTime,
        "DELETE FROM gameobject_respawn WHERE guid = ? AND instance = ?");
    stmt.PExecute(loguid, m_instanceid);

    if (t > WorldTimer::time_no_syscall())
    {
        stmt = CharacterDatabase.CreateStatement(
            insSpawnTime, "INSERT INTO gameobject_respawn VALUES ( ?, ?, ? )");
        stmt.PExecute(loguid, uint64(t), m_instanceid);
    }

    CharacterDatabase.CommitTransaction();
}

void MapPersistentState::SetCreatureRespawnTime(uint32 loguid, time_t t)
{
    if (t > WorldTimer::time_no_syscall())
        m_creatureRespawnTimes[loguid] = t;
    else
    {
        m_creatureRespawnTimes.erase(loguid);
        UnloadIfEmpty();
    }
}

void MapPersistentState::SetGORespawnTime(uint32 loguid, time_t t)
{
    if (t > WorldTimer::time_no_syscall())
        m_goRespawnTimes[loguid] = t;
    else
    {
        m_goRespawnTimes.erase(loguid);
        UnloadIfEmpty();
    }
}

void MapPersistentState::ClearRespawnTimes()
{
    m_goRespawnTimes.clear();
    m_creatureRespawnTimes.clear();

    UnloadIfEmpty();
}

void MapPersistentState::add_persistent_creature(const CreatureData* data)
{
    auto id = maps::cell_id(data->posX, data->posY);
    auto itr = persistent_creatures_.find(id);

    if (unlikely(itr == persistent_creatures_.end()))
    {
        itr = persistent_creatures_
                  .insert(std::make_pair(
                      id, google::dense_hash_set<const CreatureData*>{}))
                  .first;
        itr->second.set_empty_key(nullptr);
        itr->second.set_deleted_key(reinterpret_cast<const CreatureData*>(1));
    }

    itr->second.insert(data);
}

void MapPersistentState::add_persistent_game_object(const GameObjectData* data)
{
    auto id = maps::cell_id(data->posX, data->posY);
    auto itr = persistent_game_objects_.find(id);

    if (unlikely(itr == persistent_game_objects_.end()))
    {
        itr = persistent_game_objects_
                  .insert(std::make_pair(
                      id, google::dense_hash_set<const GameObjectData*>{}))
                  .first;
        itr->second.set_empty_key(nullptr);
        itr->second.set_deleted_key(reinterpret_cast<const GameObjectData*>(1));
    }

    itr->second.insert(data);
}

void MapPersistentState::remove_persistent_creature(const CreatureData* data)
{
    auto id = maps::cell_id(data->posX, data->posY);
    auto itr = persistent_creatures_.find(id);

    if (unlikely(itr == persistent_creatures_.end()))
        return;

    itr->second.erase(data);
}

void MapPersistentState::remove_persistent_game_object(
    const GameObjectData* data)
{
    auto id = maps::cell_id(data->posX, data->posY);
    auto itr = persistent_game_objects_.find(id);

    if (unlikely(itr == persistent_game_objects_.end()))
        return;

    itr->second.erase(data);
}

const google::dense_hash_set<const CreatureData*>*
MapPersistentState::get_persistent_creatures(int x, int y) const
{
    auto id = maps::cell_id(x, y);
    auto itr = persistent_creatures_.find(id);

    if (unlikely(itr == persistent_creatures_.end()))
        return nullptr;

    return &itr->second;
}

const google::dense_hash_set<const GameObjectData*>*
MapPersistentState::get_persistent_game_objects(int x, int y) const
{
    auto id = maps::cell_id(x, y);
    auto itr = persistent_game_objects_.find(id);

    if (unlikely(itr == persistent_game_objects_.end()))
        return nullptr;

    return &itr->second;
}

void MapPersistentState::InitPools()
{
    // pool system initialized already for persistent state (can be shared by
    // map states)
    if (!GetSpawnedPoolData().IsInitialized())
    {
        GetSpawnedPoolData().SetInitialized();
        sPoolMgr::Instance()->Initialize(
            this); // init pool system data for map persistent state
        sGameEventMgr::Instance()->Initialize(
            this); // init pool system data for map persistent state
    }
}

//== WorldPersistentState functions ========================
SpawnedPoolData WorldPersistentState::m_sharedSpawnedPoolData;

bool WorldPersistentState::CanBeUnload() const
{
    // prevent unload if used for loaded map
    // prevent unload if respawn data still exist (will not prevent reset by
    // scheduler)
    // Note: non instanceable Map never unload until server shutdown and in
    // result for loaded non-instanceable maps map persistent states also not
    // unloaded
    //       but for proper work pool systems with shared pools state for
    //       non-instanceable maps need
    //       load persistent map states for any non-instanceable maps before Map
    //       loading and make sure that it never unloaded
    return /*MapPersistentState::CanBeUnload() && !HasRespawnTimes()*/ false;
}

//== DungeonPersistentState functions =====================

DungeonPersistentState::DungeonPersistentState(uint16 MapId, uint32 InstanceId,
    Difficulty difficulty, time_t resetTime, bool canReset)
  : MapPersistentState(MapId, InstanceId, difficulty), m_resetTime(resetTime),
    m_canReset(canReset)
{
}

DungeonPersistentState::~DungeonPersistentState()
{
    // m_boundPlayers is not modified by ClearInstanceBindOnDestruction
    for (auto& elem : m_boundPlayers)
        (elem)->ClearInstanceBindOnDestruction(this);
    // m_boundGroups is not modified by ClearInstanceBindOnDestruction
    for (auto& elem : m_boundGroups)
        (elem)->ClearInstanceBindOnDestruction(this);
}

void DungeonPersistentState::AddCoOwner(Player* player)
{
    if (std::find(m_coOwners.begin(), m_coOwners.end(), player) !=
        m_coOwners.end())
        return;
    m_coOwners.push_back(player);
}

void DungeonPersistentState::RemoveCoOwner(Player* player)
{
    auto itr = std::find(m_coOwners.begin(), m_coOwners.end(), player);
    if (itr != m_coOwners.end())
        m_coOwners.erase(itr);
}

void DungeonPersistentState::BindPlayer(Player* player)
{
    if (std::find(m_boundPlayers.begin(), m_boundPlayers.end(), player) !=
        m_boundPlayers.end())
        return;
    m_boundPlayers.push_back(player);
}

void DungeonPersistentState::UnbindPlayer(Player* player, bool can_db_delete)
{
    RemoveCoOwner(player);

    auto itr = std::find(m_boundPlayers.begin(), m_boundPlayers.end(), player);
    if (itr != m_boundPlayers.end())
        m_boundPlayers.erase(itr);

    UnloadIfEmpty(can_db_delete);
}

void DungeonPersistentState::UnbindAllPlayers()
{
    auto copy = m_boundPlayers;
    for (auto player : copy)
        player->UnbindFromInstance(GetMapId(), GetDifficulty());

    // NOTE: Uses UnbindPlayer, *this might be deleted now
}

// NOTE: Except for permanent instances, only one group can be bound to the
// instance
void DungeonPersistentState::BindGroup(Group* group)
{
    if (std::find(m_boundGroups.begin(), m_boundGroups.end(), group) !=
        m_boundGroups.end())
        return;

    if (CanReset())
    {
        Group* prev = m_boundGroups.empty() ? nullptr : m_boundGroups[0];

        // Bind new one before unbinding old, so state cannot be unloaded
        if (m_boundGroups.empty())
            m_boundGroups.push_back(group);
        else
            m_boundGroups[0] = group;

        if (prev)
            prev->UnbindFromInstance(this); // *this cannot be unloaded (won't
                                            // be found in UnbindGroup)
    }
    else
    {
        m_boundGroups.push_back(group);
    }
}

void DungeonPersistentState::UnbindGroup(Group* group)
{
    auto itr = std::find(m_boundGroups.begin(), m_boundGroups.end(), group);
    if (itr != m_boundGroups.end())
        m_boundGroups.erase(itr);

    UnloadIfEmpty(true);
}

bool DungeonPersistentState::InBoundGroup(Player* player) const
{
    Group* group = player->GetGroup();
    if (!group)
        return false;

    for (auto grp : m_boundGroups)
        if (grp == group)
            return true;

    return false;
}

bool DungeonPersistentState::IsCoOwner(Player* player) const
{
    return std::find(m_coOwners.begin(), m_coOwners.end(), player) !=
           m_coOwners.end();
}

bool DungeonPersistentState::CanBeUnload() const
{
    // prevent unload if any bounded groups or online bounded player still
    // exists
    return MapPersistentState::CanBeUnload() && !HasBounds() &&
           !HasRespawnTimes();
}

/*
    Called from AddPersistentState
*/
void DungeonPersistentState::SaveToDB()
{
    // state instance data too
    std::string data;

    if (Map* map = GetMap())
    {
        InstanceData* iData = map->GetInstanceData();
        if (iData && iData->Save())
        {
            data = iData->Save();
            CharacterDatabase.escape_string(data);
        }
    }

    CharacterDatabase.PExecute(
        "INSERT INTO instance VALUES ('%u', '%u', '" UI64FMTD "', '%u', '%s')",
        GetInstanceId(), GetMapId(), (uint64)GetResetTimeForDB(),
        GetDifficulty(), data.c_str());
}

void DungeonPersistentState::DeleteRespawnTimes()
{
    CharacterDatabase.BeginTransaction();
    CharacterDatabase.PExecute(
        "DELETE FROM creature_respawn WHERE instance = '%u'", GetInstanceId());
    CharacterDatabase.PExecute(
        "DELETE FROM gameobject_respawn WHERE instance = '%u'",
        GetInstanceId());
    CharacterDatabase.CommitTransaction();

    ClearRespawnTimes(); // state can be deleted at call if only respawn data
                         // prevent unload
}

void DungeonPersistentState::DeleteFromDB()
{
    MapPersistentStateManager::DeleteInstanceFromDB(GetInstanceId());
}

// to cache or not to cache, that is the question
InstanceTemplate const* DungeonPersistentState::GetTemplate() const
{
    return ObjectMgr::GetInstanceTemplate(GetMapId());
}

time_t DungeonPersistentState::GetResetTimeForDB() const
{
    // only state the reset time for normal instances
    const MapEntry* entry = sMapStore.LookupEntry(GetMapId());
    if (!entry || entry->map_type == MAP_RAID ||
        GetDifficulty() == DUNGEON_DIFFICULTY_HEROIC)
        return 0;
    else
        return GetResetTime();
}

//== BattleGroundPersistentState functions =================

bool BattleGroundPersistentState::CanBeUnload() const
{
    // prevent unload if used for loaded map
    // BGs/Arenas not locked by respawn data/etc
    return MapPersistentState::CanBeUnload();
}

//== DungeonResetScheduler functions ======================

uint32 DungeonResetScheduler::GetMaxResetTimeFor(InstanceTemplate const* temp)
{
    if (!temp)
        return 0;

    return temp->reset_delay * DAY;
}

time_t DungeonResetScheduler::CalculateNextResetTime(
    InstanceTemplate const* temp, time_t prevResetTime)
{
    uint32 diff =
        sWorld::Instance()->getConfig(CONFIG_UINT32_INSTANCE_RESET_TIME_HOUR) *
        HOUR;
    uint32 period = GetMaxResetTimeFor(temp);
    return ((prevResetTime + MINUTE) / DAY * DAY) + period + diff;
}

void DungeonResetScheduler::LoadResetTimes()
{
    time_t now = WorldTimer::time_no_syscall();
    time_t today = (now / DAY) * DAY;

    // NOTE: Use DirectPExecute for tables that will be queried later

    // get the current reset times for normal instances (these may need to be
    // updated)
    // these are only kept in memory for InstanceSaves that are loaded later
    // resettime = 0 in the DB for raid/heroic instances so those are skipped
    typedef std::map<uint32, std::pair<uint32, time_t>> ResetTimeMapType;
    ResetTimeMapType InstResetTime;

    std::unique_ptr<QueryResult> result(CharacterDatabase.Query(
        "SELECT id, map, resettime FROM instance WHERE resettime > 0"));
    if (result)
    {
        do
        {
            if (time_t resettime = time_t((*result)[2].GetUInt64()))
            {
                uint32 id = (*result)[0].GetUInt32();
                uint32 mapid = (*result)[1].GetUInt32();

                MapEntry const* mapEntry = sMapStore.LookupEntry(mapid);

                if (!mapEntry || !mapEntry->IsDungeon())
                {
                    sMapPersistentStateMgr::Instance()->DeleteInstanceFromDB(
                        id);
                    continue;
                }

                InstResetTime[id] = std::pair<uint32, uint64>(mapid, resettime);
            }
        } while (result->NextRow());

        // update reset time for normal instances with the max creature respawn
        // time + X hours
        result.reset(CharacterDatabase.Query(
            "SELECT MAX(respawntime), instance FROM creature_respawn WHERE "
            "instance > 0 GROUP BY instance"));
        if (result)
        {
            do
            {
                Field* fields = result->Fetch();

                time_t resettime = time_t(fields[0].GetUInt64() + 2 * HOUR);
                uint32 instance = fields[1].GetUInt32();

                auto itr = InstResetTime.find(instance);
                if (itr != InstResetTime.end() &&
                    itr->second.second != resettime)
                {
                    CharacterDatabase.DirectPExecute(
                        "UPDATE instance SET resettime = '" UI64FMTD
                        "' WHERE id = '%u'",
                        uint64(resettime), instance);
                    itr->second.second = resettime;
                }
            } while (result->NextRow());
        }

        // schedule the reset times
        for (auto& elem : InstResetTime)
            if (elem.second.second > now)
                ScheduleReset(true, elem.second.second,
                    DungeonResetEvent(RESET_EVENT_NORMAL_DUNGEON,
                                  elem.second.first, elem.first));
    }

    // load the global respawn times for raid/heroic instances
    uint32 diff =
        sWorld::Instance()->getConfig(CONFIG_UINT32_INSTANCE_RESET_TIME_HOUR) *
        HOUR;
    m_resetTimeByMapId.resize(sMapStore.GetNumRows() + 1);
    result.reset(
        CharacterDatabase.Query("SELECT mapid, resettime FROM instance_reset"));
    if (result)
    {
        do
        {
            Field* fields = result->Fetch();

            uint32 mapid = fields[0].GetUInt32();

            MapEntry const* mapEntry = sMapStore.LookupEntry(mapid);

            if (!mapEntry || !mapEntry->IsDungeon() ||
                !ObjectMgr::GetInstanceTemplate(mapid))
            {
                logging.error(
                    "MapPersistentStateManager::LoadResetTimes: invalid mapid "
                    "%u in instance_reset!",
                    mapid);
                CharacterDatabase.DirectPExecute(
                    "DELETE FROM instance_reset WHERE mapid = '%u'", mapid);
                continue;
            }

            // update the reset time if the hour in the configs changes
            uint64 oldresettime = fields[1].GetUInt64();
            uint64 newresettime = (oldresettime / DAY) * DAY + diff;
            if (oldresettime != newresettime)
                CharacterDatabase.DirectPExecute(
                    "UPDATE instance_reset SET resettime = '" UI64FMTD
                    "' WHERE mapid = '%u'",
                    newresettime, mapid);

            SetResetTimeFor(mapid, newresettime);
        } while (result->NextRow());
    }

    // clean expired instances, references to them will be deleted in
    // CleanupInstances
    // must be done before calculating new reset times
    m_InstanceSaves._CleanupExpiredInstancesAtTime(now);

    // calculate new global reset times for expired instances and those that
    // have never been reset yet
    // add the global reset times to the priority queue
    for (uint32 i = 0; i < sInstanceTemplate.MaxEntry; i++)
    {
        InstanceTemplate const* temp = ObjectMgr::GetInstanceTemplate(i);
        if (!temp)
            continue;

        // only raid/heroic maps have a global reset time
        MapEntry const* mapEntry = sMapStore.LookupEntry(temp->map);
        if (!mapEntry || !mapEntry->IsDungeon() || !mapEntry->HasResetTime())
            continue;

        uint32 period = GetMaxResetTimeFor(temp);
        time_t t = GetResetTimeFor(temp->map);
        if (!t)
        {
            // initialize the reset time
            t = today + period + diff;
            CharacterDatabase.DirectPExecute(
                "INSERT INTO instance_reset VALUES ('%u','" UI64FMTD "')",
                temp->map, (uint64)t);
        }

        if (t < now)
        {
            // assume that expired instances have already been cleaned
            // calculate the next reset time
            t = (t / DAY) * DAY;
            t += ((today - t) / period + 1) * period + diff;
            CharacterDatabase.DirectPExecute(
                "UPDATE instance_reset SET resettime = '" UI64FMTD
                "' WHERE mapid = '%u'",
                (uint64)t, temp->map);
        }

        SetResetTimeFor(temp->map, t);

        // schedule the global reset/warning
        ResetEventType type = RESET_EVENT_INFORM_1;
        for (; type < RESET_EVENT_INFORM_LAST; type = ResetEventType(type + 1))
            if (t - resetEventTypeDelay[type] > now)
                break;

        ScheduleReset(true, t - resetEventTypeDelay[type],
            DungeonResetEvent(type, temp->map, 0));
    }
}

void DungeonResetScheduler::ScheduleReset(
    bool add, time_t time, DungeonResetEvent event)
{
    if (add)
        m_resetTimeQueue.insert(
            std::pair<time_t, DungeonResetEvent>(time, event));
    else
    {
        // find the event in the queue and remove it
        ResetTimeQueue::iterator itr;
        std::pair<ResetTimeQueue::iterator, ResetTimeQueue::iterator> range;
        range = m_resetTimeQueue.equal_range(time);
        for (itr = range.first; itr != range.second; ++itr)
        {
            if (itr->second == event)
            {
                m_resetTimeQueue.erase(itr);
                return;
            }
        }
        // in case the reset time changed (should happen very rarely), we search
        // the whole queue
        if (itr == range.second)
        {
            for (itr = m_resetTimeQueue.begin(); itr != m_resetTimeQueue.end();
                 ++itr)
            {
                if (itr->second == event)
                {
                    m_resetTimeQueue.erase(itr);
                    return;
                }
            }

            if (itr == m_resetTimeQueue.end())
                logging.error(
                    "DungeonResetScheduler::ScheduleReset: cannot cancel the "
                    "reset, the event(%d,%d,%d) was not found!",
                    event.type, event.mapid, event.instanceId);
        }
    }
}

void DungeonResetScheduler::RemoveResetSchedule(uint32 instance_id)
{
    for (auto itr = m_resetTimeQueue.begin(); itr != m_resetTimeQueue.end();
         ++itr)
    {
        if (itr->second.instanceId == instance_id)
        {
            m_resetTimeQueue.erase(itr);
            return;
        }
    }
}

void DungeonResetScheduler::Update()
{
    time_t now = WorldTimer::time_no_syscall(), t;
    while (!m_resetTimeQueue.empty() &&
           (t = m_resetTimeQueue.begin()->first) < now)
    {
        DungeonResetEvent& event = m_resetTimeQueue.begin()->second;
        if (event.type == RESET_EVENT_NORMAL_DUNGEON)
        {
            // for individual normal instances, max creature respawn + X hours
            m_InstanceSaves._ResetInstance(event.instanceId);
        }
        else
        {
            // global reset/warning for a certain map
            time_t resetTime = GetResetTimeFor(event.mapid);
            m_InstanceSaves._ResetOrWarnAll(event.mapid,
                event.type != RESET_EVENT_INFORM_LAST, uint32(resetTime - now));
            if (event.type != RESET_EVENT_INFORM_LAST)
            {
                // schedule the next warning/reset
                event.type = ResetEventType(event.type + 1);
                ScheduleReset(
                    true, resetTime - resetEventTypeDelay[event.type], event);
            }
            else
            {
                // re-schedule the next/new global reset/warning
                // calculate the next reset time
                InstanceTemplate const* instanceTemplate =
                    ObjectMgr::GetInstanceTemplate(event.mapid);
                assert(instanceTemplate);

                time_t next_reset =
                    DungeonResetScheduler::CalculateNextResetTime(
                        instanceTemplate, resetTime);

                CharacterDatabase.DirectPExecute(
                    "UPDATE instance_reset SET resettime = '" UI64FMTD
                    "' WHERE mapid = '%u'",
                    uint64(next_reset), uint32(event.mapid));

                SetResetTimeFor(event.mapid, next_reset);

                ResetEventType type = RESET_EVENT_INFORM_1;
                for (; type < RESET_EVENT_INFORM_LAST;
                     type = ResetEventType(type + 1))
                    if (next_reset - resetEventTypeDelay[type] > now)
                        break;

                // add new scheduler event to the queue
                event.type = type;
                ScheduleReset(
                    true, next_reset - resetEventTypeDelay[event.type], event);
            }
        }
        m_resetTimeQueue.erase(m_resetTimeQueue.begin());
    }
}

//== MapPersistentStateManager functions =========================

MapPersistentStateManager::MapPersistentStateManager()
  : lock_instLists(false), m_Scheduler(*this)
{
}

MapPersistentStateManager::~MapPersistentStateManager()
{
    // it is undefined whether this or objectmgr will be unloaded first
    // so we must be prepared for both cases
    lock_instLists = true;
    for (auto& elem : m_instanceSaveByInstanceId)
    {
        // Don't delete DungeonPersistenStates, they are owned by
        // std::shared_ptrs
        if (dynamic_cast<DungeonPersistentState*>(elem.second) == nullptr)
            delete elem.second;
    }
    for (auto& elem : m_instanceSaveByMapId)
        delete elem.second;
}

/*
- adding instance into manager
- called from DungeonMap::Add, _LoadBoundInstances, LoadGroups
*/
MapPersistentState* MapPersistentStateManager::AddPersistentState(
    MapEntry const* mapEntry, uint32 instanceId, Difficulty difficulty,
    time_t resetTime, bool canReset, bool load /*=false*/,
    bool initPools /*= true*/)
{
    if (MapPersistentState* old_save =
            GetPersistentState(mapEntry->MapID, instanceId))
        return old_save;

    if (mapEntry->IsDungeon())
    {
        if (!resetTime)
        {
            // initialize reset time
            // for normal instances if no creatures are killed the instance will
            // reset in two hours
            if (mapEntry->map_type == MAP_RAID ||
                difficulty > DUNGEON_DIFFICULTY_NORMAL)
                resetTime = m_Scheduler.GetResetTimeFor(mapEntry->MapID);
            else
            {
                resetTime = WorldTimer::time_no_syscall() + 2 * HOUR;
                // normally this will be removed soon after in DungeonMap::Add,
                // prevent error
                m_Scheduler.ScheduleReset(true, resetTime,
                    DungeonResetEvent(RESET_EVENT_NORMAL_DUNGEON,
                                              mapEntry->MapID, instanceId));
            }
        }
    }

    LOG_DEBUG(logging,
        "MapPersistentStateManager::AddPersistentState: mapid = %d, instanceid "
        "= %d, reset time = " UI64FMTD ", canRset = %u",
        mapEntry->MapID, instanceId, resetTime, canReset ? 1 : 0);

    MapPersistentState* state;
    if (mapEntry->IsDungeon())
    {
        auto dungeonState = std::make_shared<DungeonPersistentState>(
            mapEntry->MapID, instanceId, difficulty, resetTime, canReset);
        if (!load)
            dungeonState->SaveToDB();
        state = dungeonState.get();
        m_dungeonStates[instanceId] = std::move(dungeonState);
    }
    else if (mapEntry->IsBattleGroundOrArena())
        state = new BattleGroundPersistentState(
            mapEntry->MapID, instanceId, difficulty);
    else
        state = new WorldPersistentState(mapEntry->MapID);

    if (instanceId)
        m_instanceSaveByInstanceId[instanceId] = state;
    else
        m_instanceSaveByMapId[mapEntry->MapID] = state;

    if (initPools)
        state->InitPools();

    return state;
}

MapPersistentState* MapPersistentStateManager::GetPersistentState(
    uint32 mapId, uint32 instanceId)
{
    if (instanceId)
    {
        auto itr = m_instanceSaveByInstanceId.find(instanceId);
        return itr != m_instanceSaveByInstanceId.end() ? itr->second : nullptr;
    }
    else
    {
        auto itr = m_instanceSaveByMapId.find(mapId);
        return itr != m_instanceSaveByMapId.end() ? itr->second : nullptr;
    }
}

std::shared_ptr<DungeonPersistentState>
MapPersistentStateManager::GetDungeonPersistentState(uint32 instance_id)
{
    auto itr = m_dungeonStates.find(instance_id);
    return itr != m_dungeonStates.end() ? itr->second : nullptr;
}

void MapPersistentStateManager::DeleteInstanceFromDB(uint32 instanceid)
{
    if (instanceid)
    {
        CharacterDatabase.PExecute(
            "DELETE FROM instance WHERE id = '%u'", instanceid);
        CharacterDatabase.PExecute(
            "DELETE FROM character_instance WHERE instance = '%u'", instanceid);
        CharacterDatabase.PExecute(
            "DELETE FROM group_instance WHERE instance = '%u'", instanceid);
        CharacterDatabase.PExecute(
            "DELETE FROM creature_respawn WHERE instance = '%u'", instanceid);
        CharacterDatabase.PExecute(
            "DELETE FROM gameobject_respawn WHERE instance = '%u'", instanceid);
    }
}

void MapPersistentStateManager::RemovePersistentState(
    uint32 mapId, uint32 instanceId)
{
    if (lock_instLists)
        return;

    if (instanceId)
    {
        auto itr = m_instanceSaveByInstanceId.find(instanceId);
        if (itr != m_instanceSaveByInstanceId.end())
        {
            // state the resettime for normal instances only when they get
            // unloaded
            if (itr->second->GetMapEntry()->IsDungeon())
                if (time_t resettime = ((DungeonPersistentState*)itr->second)
                                           ->GetResetTimeForDB())
                    CharacterDatabase.PExecute(
                        "UPDATE instance SET resettime = '" UI64FMTD
                        "' WHERE id = '%u'",
                        (uint64)resettime, instanceId);

            _ResetSave(m_instanceSaveByInstanceId, itr);
        }
    }
    else
    {
        auto itr = m_instanceSaveByMapId.find(mapId);
        if (itr != m_instanceSaveByMapId.end())
            _ResetSave(m_instanceSaveByMapId, itr);
    }
}

void MapPersistentStateManager::load_reserved_instance_ids()
{
    assert(reserved_instance_ids_.empty() && "Only call on startup");

    auto result = std::unique_ptr<QueryResult>(
        CharacterDatabase.PQuery("SELECT id FROM instance"));
    if (!result)
        return;
    do
    {
        Field* fields = result->Fetch();
        reserved_instance_ids_.emplace_back(fields[0].GetUInt32());
    } while (result->NextRow());

    std::sort(reserved_instance_ids_.begin(), reserved_instance_ids_.end());
    logging.info("Loaded %u reserved instance ids",
        (uint32)reserved_instance_ids_.size());
}

uint32 MapPersistentStateManager::reserve_instance_id()
{
    // Prune reserverd instance ids that have been freed for more than an hour
    for (auto itr = instance_ids_pending_release_.begin();
         itr != instance_ids_pending_release_.end();)
    {
        if (itr->second + HOUR > WorldTimer::time_no_syscall())
            break;

        auto reserved_itr = std::lower_bound(reserved_instance_ids_.begin(),
            reserved_instance_ids_.end(), itr->first);
        if (reserved_itr != reserved_instance_ids_.end())
            reserved_instance_ids_.erase(reserved_itr);

        itr = instance_ids_pending_release_.erase(itr);
    }

    // Reserve lowest unreserved instance id
    uint32 id = 1;

    for (auto reserved : reserved_instance_ids_)
    {
        if (reserved == id)
            ++id;
        else
            break;
    }

    auto itr = std::lower_bound(
        reserved_instance_ids_.begin(), reserved_instance_ids_.end(), id);
    reserved_instance_ids_.insert(itr, id);

    return id;
}

void MapPersistentStateManager::free_instance_id(uint32 id)
{
    instance_ids_pending_release_.emplace_back(
        id, WorldTimer::time_no_syscall());
}

void MapPersistentStateManager::_DelHelper(DatabaseType& db, const char* fields,
    const char* table, const char* queryTail, ...)
{
    Tokens fieldTokens = StrSplit(fields, ", ");
    assert(fieldTokens.size() != 0);

    va_list ap;
    char szQueryTail[MAX_QUERY_LEN];
    va_start(ap, queryTail);
    vsnprintf(szQueryTail, MAX_QUERY_LEN, queryTail, ap);
    va_end(ap);

    std::unique_ptr<QueryResult> result(
        db.PQuery("SELECT %s FROM %s %s", fields, table, szQueryTail));
    if (result)
    {
        do
        {
            Field* fields = result->Fetch();
            std::ostringstream ss;
            for (size_t i = 0; i < fieldTokens.size(); i++)
            {
                std::string fieldValue = fields[i].GetCppString();
                db.escape_string(fieldValue);
                ss << (i != 0 ? " AND " : "") << fieldTokens[i] << " = '"
                   << fieldValue << "'";
            }
            db.PExecute("DELETE FROM %s WHERE %s", table, ss.str().c_str());
        } while (result->NextRow());
    }
}

void MapPersistentStateManager::CleanupInstances()
{
    BarGoLink bar(2);
    bar.step();

    // load reset times and clean expired instances
    m_Scheduler.LoadResetTimes();

    CharacterDatabase.BeginTransaction();
    // clean character/group - instance binds with invalid group/characters
    _DelHelper(CharacterDatabase, "character_instance.guid, instance",
        "character_instance",
        "LEFT JOIN characters ON character_instance.guid = characters.guid "
        "WHERE characters.guid IS NULL");
    _DelHelper(CharacterDatabase, "group_instance.leaderGuid, instance",
        "group_instance",
        "LEFT JOIN characters ON group_instance.leaderGuid = characters.guid "
        "LEFT JOIN groups ON group_instance.leaderGuid = groups.leaderGuid "
        "WHERE characters.guid IS NULL OR groups.leaderGuid IS NULL");

    // clean instances that do not have any players or groups bound to them
    _DelHelper(CharacterDatabase, "id, map, difficulty", "instance",
        "LEFT JOIN character_instance ON character_instance.instance = id LEFT "
        "JOIN group_instance ON group_instance.instance = id WHERE "
        "character_instance.instance IS NULL AND group_instance.instance IS "
        "NULL");

    // clean invalid instance references in other tables
    _DelHelper(CharacterDatabase, "character_instance.guid, instance",
        "character_instance",
        "LEFT JOIN instance ON character_instance.instance = instance.id WHERE "
        "instance.id IS NULL");
    _DelHelper(CharacterDatabase, "group_instance.leaderGuid, instance",
        "group_instance",
        "LEFT JOIN instance ON group_instance.instance = instance.id WHERE "
        "instance.id IS NULL");

    // clean unused respawn data
    CharacterDatabase.Execute(
        "DELETE FROM creature_respawn WHERE instance <> 0 AND instance NOT IN "
        "(SELECT id FROM instance)");
    CharacterDatabase.Execute(
        "DELETE FROM gameobject_respawn WHERE instance <> 0 AND instance NOT "
        "IN (SELECT id FROM instance)");
    // execute transaction directly
    CharacterDatabase.CommitTransaction();

    bar.step();
    logging.info("Instances cleaned up\n");
}

void MapPersistentStateManager::_ResetSave(
    PersistentStateMap& holder, PersistentStateMap::iterator& itr)
{
    // unbind all players bound to the instance
    // do not allow UnbindInstance to automatically unload the InstanceSaves
    lock_instLists = true;
    if (auto map = itr->second->GetMap())
        map->ClearPersistentState();

    if (itr->second->GetInstanceId() &&
        m_dungeonStates.count(itr->second->GetInstanceId()))
    {
        m_Scheduler.RemoveResetSchedule(itr->second->GetInstanceId());
        m_dungeonStates.erase(itr->second->GetInstanceId());
    }
    else
        delete itr->second;

    holder.erase(itr++);
    lock_instLists = false;
}

void MapPersistentStateManager::_ResetInstance(uint32 instanceId)
{
    LOG_DEBUG(
        logging, "MapPersistentStateManager::_ResetInstance %u", instanceId);

    auto itr = m_instanceSaveByInstanceId.find(instanceId);
    if (itr != m_instanceSaveByInstanceId.end())
    {
        // delay reset until map unload for loaded map
        if (Map* iMap = itr->second->GetMap())
        {
            assert(iMap->IsDungeon());

            ((DungeonMap*)iMap)->Reset(INSTANCE_RESET_RESPAWN_DELAY);
            return;
        }

        _ResetSave(m_instanceSaveByInstanceId, itr);
    }

    DeleteInstanceFromDB(instanceId); // even if state not loaded
}

void MapPersistentStateManager::_ResetOrWarnAll(
    uint32 mapid, bool warn, uint32 timeLeft)
{
    // global reset for all instances of the given map
    MapEntry const* mapEntry = sMapStore.LookupEntry(mapid);
    if (!mapEntry->Instanceable())
        return;

    time_t now = WorldTimer::time_no_syscall();

    if (!warn)
    {
        // this is called one minute before the reset time
        InstanceTemplate const* temp = ObjectMgr::GetInstanceTemplate(mapid);
        if (!temp || !temp->reset_delay)
        {
            logging.error(
                "MapPersistentStateManager::ResetOrWarnAll: no instance "
                "template or reset delay for map %d",
                mapid);
            return;
        }

        // remove all binds to instances of the given map
        for (auto itr = m_instanceSaveByInstanceId.begin();
             itr != m_instanceSaveByInstanceId.end();)
        {
            if (itr->second->GetMapId() == mapid)
                _ResetSave(m_instanceSaveByInstanceId, itr);
            else
                ++itr;
        }

        // delete them from the DB, even if not loaded
        CharacterDatabase.BeginTransaction();
        CharacterDatabase.PExecute(
            "DELETE FROM character_instance USING character_instance LEFT JOIN "
            "instance ON character_instance.instance = id WHERE map = '%u'",
            mapid);
        CharacterDatabase.PExecute(
            "DELETE FROM group_instance USING group_instance LEFT JOIN "
            "instance ON group_instance.instance = id WHERE map = '%u'",
            mapid);
        CharacterDatabase.PExecute(
            "DELETE FROM instance WHERE map = '%u'", mapid);
        CharacterDatabase.CommitTransaction();

        // calculate the next reset time
        time_t next_reset =
            DungeonResetScheduler::CalculateNextResetTime(temp, now + timeLeft);
        // update it in the DB
        CharacterDatabase.PExecute(
            "UPDATE instance_reset SET resettime = '" UI64FMTD
            "' WHERE mapid = '%u'",
            (uint64)next_reset, mapid);
    }

    // note: this isn't fast but it's meant to be executed very rarely
    const MapManager::MapMapType& maps = sMapMgr::Instance()->Maps();

    auto iter_last = maps.lower_bound(MapID(mapid + 1));
    for (auto mitr = maps.lower_bound(MapID(mapid)); mitr != iter_last; ++mitr)
    {
        Map* map2 = mitr->second;
        if (map2->GetId() != mapid)
            break;

        if (warn)
            ((DungeonMap*)map2)->SendResetWarnings(timeLeft);
        else
            ((DungeonMap*)map2)->Reset(INSTANCE_RESET_GLOBAL);
    }
}

void MapPersistentStateManager::GetStatistics(
    uint32& numStates, uint32& numBoundPlayers, uint32& numBoundGroups)
{
    numStates = 0;
    numBoundPlayers = 0;
    numBoundGroups = 0;

    // only instanceable maps have bounds
    for (auto& elem : m_instanceSaveByInstanceId)
    {
        if (!elem.second->GetMapEntry()->IsDungeon())
            continue;

        ++numStates;
        numBoundPlayers += (uint32)((DungeonPersistentState*)elem.second)
                               ->GetBoundPlayers()
                               .size();
        numBoundGroups +=
            ((DungeonPersistentState*)elem.second)->GetBoundGroups().size();
    }
}

void MapPersistentStateManager::_CleanupExpiredInstancesAtTime(time_t t)
{
    _DelHelper(CharacterDatabase, "id, map, difficulty", "instance",
        "LEFT JOIN instance_reset ON mapid = map WHERE (instance.resettime < "
        "'" UI64FMTD
        "' AND instance.resettime > '0') OR (NOT instance_reset.resettime IS "
        "NULL AND instance_reset.resettime < '" UI64FMTD "')",
        (uint64)t, (uint64)t);
}

void MapPersistentStateManager::InitWorldMaps()
{
    MapPersistentState* state =
        nullptr; // need any from created for shared pool state
    for (uint32 mapid = 0; mapid < sMapStore.GetNumRows(); ++mapid)
        if (MapEntry const* entry = sMapStore.LookupEntry(mapid))
            if (!entry->Instanceable())
                state = AddPersistentState(
                    entry, 0, REGULAR_DIFFICULTY, 0, false, true, false);

    if (state)
        state->InitPools();
}

void MapPersistentStateManager::LoadCreatureRespawnTimes()
{
    // remove outdated data
    CharacterDatabase.DirectExecute(
        "DELETE FROM creature_respawn WHERE respawntime <= "
        "UNIX_TIMESTAMP(NOW())");

    uint32 count = 0;

    //                                                                      0
    //                                                                      1
    //                                                                      2
    //                                                                      3
    //                                                                      4 5
    std::unique_ptr<QueryResult> result(CharacterDatabase.Query(
        "SELECT guid, respawntime, creature_respawn.map, instance, difficulty, "
        "resettime FROM creature_respawn LEFT JOIN instance ON instance = id"));
    if (!result)
    {
        logging.info("Loaded 0 creature respawn time.\n");
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        Field* fields = result->Fetch();
        bar.step();

        uint32 loguid = fields[0].GetUInt32();
        uint64 respawn_time = fields[1].GetUInt64();
        uint32 mapId = fields[2].GetUInt32();
        uint32 instanceId = fields[3].GetUInt32();
        uint8 difficulty = fields[4].GetUInt8();
        time_t resetTime = (time_t)fields[5].GetUInt64();

        CreatureData const* data =
            sObjectMgr::Instance()->GetCreatureData(loguid);
        if (!data)
            continue;

        if (mapId != data->mapid)
            continue;

        MapEntry const* mapEntry = sMapStore.LookupEntry(mapId);
        if (!mapEntry || (mapEntry->Instanceable() != (instanceId != 0)))
            continue;

        if (difficulty >= (!mapEntry->Instanceable() ?
                                  DUNGEON_DIFFICULTY_HEROIC :
                                  MAX_DIFFICULTY))
            continue;

        MapPersistentState* state = AddPersistentState(mapEntry, instanceId,
            Difficulty(difficulty), resetTime, mapEntry->IsDungeon(), true);
        if (!state)
            continue;

        state->SetCreatureRespawnTime(loguid, time_t(respawn_time));

        ++count;

    } while (result->NextRow());

    logging.info("Loaded %u creature respawn times\n", count);
}

void MapPersistentStateManager::LoadGameobjectRespawnTimes()
{
    // remove outdated data
    CharacterDatabase.DirectExecute(
        "DELETE FROM gameobject_respawn WHERE respawntime <= "
        "UNIX_TIMESTAMP(NOW())");

    uint32 count = 0;

    //                                                    0     1            2
    //                                                    3         4 5
    QueryResult* result = CharacterDatabase.Query(
        "SELECT guid, respawntime, map, instance, difficulty, resettime FROM "
        "gameobject_respawn LEFT JOIN instance ON instance = id");

    if (!result)
    {
        logging.info("Loaded 0 gameobject respawn time.\n");
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        Field* fields = result->Fetch();
        bar.step();

        uint32 loguid = fields[0].GetUInt32();
        uint64 respawn_time = fields[1].GetUInt64();
        uint32 mapId = fields[2].GetUInt32();
        uint32 instanceId = fields[3].GetUInt32();
        uint8 difficulty = fields[4].GetUInt8();
        time_t resetTime = (time_t)fields[5].GetUInt64();

        GameObjectData const* data = sObjectMgr::Instance()->GetGOData(loguid);
        if (!data)
            continue;

        if (mapId != data->mapid)
            continue;

        MapEntry const* mapEntry = sMapStore.LookupEntry(mapId);
        if (!mapEntry || (mapEntry->Instanceable() != (instanceId != 0)))
            continue;

        if (difficulty >=
            (!mapEntry->Instanceable() ? REGULAR_DIFFICULTY : MAX_DIFFICULTY))
            continue;

        MapPersistentState* state = AddPersistentState(mapEntry, instanceId,
            Difficulty(difficulty), resetTime, mapEntry->IsDungeon(), true);
        if (!state)
            continue;

        state->SetGORespawnTime(loguid, time_t(respawn_time));

        ++count;

    } while (result->NextRow());

    delete result;

    logging.info("Loaded %u gameobject respawn times\n", count);
}
