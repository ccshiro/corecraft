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

#include "MapManager.h"
#include "Corpse.h"
#include "logging.h"
#include "MapPersistentStateMgr.h"
#include "ObjectMgr.h"
#include "Transport.h"
#include "World.h"
#include "threaded_maps.h"
#include "Database/DatabaseEnv.h"
#include "maps/map_grid.h"
#include "Policies/Singleton.h"

MapManager::MapManager() : threaded_maps_(nullptr)
{
    i_timer.SetInterval(
        sWorld::Instance()->getConfig(CONFIG_UINT32_INTERVAL_MAPUPDATE));

    // Blizzlike value is 400 ms
    batch_timers_[(int)BatchUpdates::spells].SetInterval(290);
}

MapManager::~MapManager()
{
    for (auto& elem : i_maps)
        delete elem.second;

    delete threaded_maps_;
}

void MapManager::InitializeVisibilityDistanceInfo()
{
    for (auto& elem : i_maps)
        (elem).second->InitVisibilityDistance();
}

Map* MapManager::CreateMap(uint32 id, const WorldObject* obj)
{
    Map* m = nullptr;

    const MapEntry* entry = sMapStore.LookupEntry(id);
    if (!entry)
        return nullptr;

    if (entry->Instanceable())
    {
        assert(obj && obj->GetTypeId() == TYPEID_PLAYER);
        // create DungeonMap object
        m = CreateInstance(id, (Player*)obj);
    }
    else
    {
        // create regular non-instanceable map
        m = FindMap(id);
        if (m == nullptr)
        {
            m = new WorldMap(id);
            // add map into container
            i_maps[MapID(id)] = m;

            // non-instanceable maps always expected have saved state
            m->CreateInstanceData(true);
        }
    }

    return m;
}

Map* MapManager::CreateBgMap(uint32 mapid, BattleGround* bg)
{
    sTerrainMgr::Instance()->LoadTerrain(mapid);

    return CreateBattleGroundMap(
        mapid, sMapPersistentStateMgr::Instance()->reserve_instance_id(), bg);
}

Map* MapManager::FindMap(uint32 mapid, uint32 instanceId) const
{
    auto iter = i_maps.find(MapID(mapid, instanceId));
    if (iter == i_maps.end())
        return nullptr;

    // this is a small workaround for transports
    if (instanceId == 0 && iter->second->Instanceable())
    {
        assert(false);
        return nullptr;
    }

    return iter->second;
}

/*
    checks that do not require a map to be created
    will send transfer error messages on fail
*/
bool MapManager::CanPlayerEnter(uint32 mapid, Player* player)
{
    auto map_entry = sMapStore.LookupEntry(mapid);
    if (!map_entry)
        return false;

    const char* map_name =
        map_entry->name[player->GetSession()->GetSessionDbcLocale()];
    if (!map_name)
        map_name = map_entry->name[LOCALE_enUS];

    /*
     * Map Entry Requirements
     */

    auto map_req = sObjectMgr::Instance()->GetMapEntryRequirements(mapid);
    if (!player->isGameMaster() && map_req)
    {
        if (player->getLevel() < map_req->level)
        {
            player->GetSession()->SendAreaTriggerMessage(
                "You are too low level to enter this instance.");
            return false;
        }

        for (auto& item : map_req->items)
        {
            if (item.difficulty != player->GetDifficulty())
                continue;
            if (item.team != TEAM_NONE && item.team != player->GetTeam())
                continue;
            if (!player->HasItemCount(item.id, 1))
            {
                if (!item.failed_text.empty())
                    player->GetSession()->SendAreaTriggerMessage(
                        item.failed_text.c_str());
                else
                    player->GetSession()->SendAreaTriggerMessage(
                        "You do not meet the requirements to enter this "
                        "instance.");
                return false;
            }
        }

        for (auto& item : map_req->quests)
        {
            if (item.difficulty != player->GetDifficulty())
                continue;
            if (item.team != TEAM_NONE && item.team != player->GetTeam())
                continue;
            if (!player->GetQuestRewardStatus(item.id))
            {
                if (!item.failed_text.empty())
                    player->GetSession()->SendAreaTriggerMessage(
                        item.failed_text.c_str());
                else
                    player->GetSession()->SendAreaTriggerMessage(
                        "You do not meet the requirements to enter this "
                        "instance.");
                return false;
            }
        }
    }

    /*
     * Dungeon & Raid checks
     */

    if (!map_entry->IsDungeon())
        return true;

    auto instance = ObjectMgr::GetInstanceTemplate(mapid);
    if (!instance)
        return false;

    if (!player->isAlive())
    {
        uint32 corpse_mapid = 0;
        bool has_corpse = false;
        if (Corpse* corpse = player->GetCorpse())
        {
            corpse_mapid = corpse->GetMapId();
            has_corpse = true;
        }

        // allow ghosts to enter an instance that has a child instance which the
        // player's corpse is in
        do
        {
            if (corpse_mapid == map_entry->MapID)
                break;

            auto corpse_instance = ObjectMgr::GetInstanceTemplate(corpse_mapid);
            corpse_mapid = corpse_instance ? corpse_instance->parent : 0;
        } while (corpse_mapid);

        if (!corpse_mapid && has_corpse)
        {
            // No error. In Vanilla & TBC you got ported back to your graveyard.
            const WorldSafeLocsEntry* gy = nullptr;
            // Special handle for battleground maps
            if (BattleGround* bg = player->GetBattleGround())
                gy = bg->GetClosestGraveYard(player);
            else
                gy = sObjectMgr::Instance()->GetClosestGraveyard(player->GetX(),
                    player->GetY(), player->GetZ(), player->GetMapId(),
                    player->GetTeam());
            if (gy)
            {
                player->TeleportTo(
                    gy->map_id, gy->x, gy->y, gy->z, player->GetO());
                return false;
            }
        }

        // Player will resurrect even if remaining conditions fail
        player->ResurrectPlayer(0.5f, false);
        player->SpawnCorpseBones();
    }

    if (player->GetDifficulty() == DUNGEON_DIFFICULTY_HEROIC &&
        !map_entry->SupportsHeroicMode())
    {
        player->SendTransferAborted(
            mapid, TRANSFER_ABORT_DIFFICULTY, player->GetDifficulty());
        return false;
    }

    // Bypass remaining checks for GM
    if (player->isGameMaster())
        return true;

    Group* group = player->GetGroup();

    // can only enter in a raid group
    if (map_entry->IsRaid() && (!group || !group->isRaidGroup()))
    {
        player->SendRaidGroupError(ERR_RAID_GROUP_RAIDGRP);
        return false;
    }

    return true;
}

void MapManager::DeleteInstance(uint32 mapid, uint32 instanceId)
{
    auto iter = i_maps.find(MapID(mapid, instanceId));
    if (iter != i_maps.end())
    {
        Map* pMap = iter->second;
        if (pMap->Instanceable())
        {
            i_maps.erase(iter);

            pMap->UnloadAll();
            delete pMap;
        }
    }
}

void MapManager::Update(uint32 diff)
{
    // Update batch timers
    for (int i = 0; i < (int)BatchUpdates::count; ++i)
    {
        batch_timers_[i].Update(diff);
        if (batch_timers_[i].Passed())
        {
            World::batch_ready[i] = true;
            batch_timers_[i].Reset();
        }
    }

    i_timer.Update(diff);
    if (!i_timer.Passed())
        return;

    if (!threaded_maps_ &&
        sWorld::Instance()->getConfig(CONFIG_UINT32_MAP_THREADS) > 1)
    {
        threaded_maps_ = new threaded_maps;
        threaded_maps_->initalize();
    }

    if (threaded_maps_)
    {
        for (auto& elem : i_maps)
            threaded_maps_->push_map(elem.second);
        threaded_maps_->run(i_timer.GetCurrent());
    }
    else
    {
        for (auto& elem : i_maps)
            elem.second->Update(i_timer.GetCurrent());
    }

    // Update transports between map update and global ops.
    // NOTE: Needs to be in here as we must update maps transport lives in as
    // often as the transports themselves. Or map pending local and global
    // operations might be executed out of order, causing very bad things.
    sTransportMgr::Instance()->UpdateTransports(diff);

    // Execute global pending grid operations
    maps::map_grid::exec_global_pending_operations();

    // remove all maps which can be unloaded
    auto iter = i_maps.begin();
    while (iter != i_maps.end())
    {
        Map* pMap = iter->second;
        // check if map can be unloaded
        if (pMap->CanUnload((uint32)i_timer.GetCurrent()))
        {
            pMap->UnloadAll();
            delete pMap;

            i_maps.erase(iter++);
        }
        else
            ++iter;
    }

    i_timer.SetCurrent(0);

    // Remove batches being ready; update has gone through
    for (int i = 0; i < (int)BatchUpdates::count; ++i)
        World::batch_ready[i] = false;
}

void MapManager::RemoveAllObjectsInRemoveList()
{
    for (auto& elem : i_maps)
        elem.second->RemoveAllObjectsInRemoveList();
}

bool MapManager::ExistMapAndVMap(uint32 mapid, float x, float y)
{
    auto p = maps::world_coords_to_data_cell(x, y);

    // NOTE: This function is only used to check expected values to make sure
    //       VMaps is at least present to some extent.
    auto exists = GridMap::ExistMap(mapid, p.first, p.second) &&
                  GridMap::ExistVMap(mapid, p.first, p.second);
    if (!exists)
    {
        logging.error(
            "Expected coordinates (%f, %f) that map to data cell (%d, %d) to "
            "have valid Map and VMap data, but they don't!",
            x, y, p.first, p.second);
    }

    return exists;
}

bool MapManager::IsValidMAP(uint32 mapid)
{
    MapEntry const* mEntry = sMapStore.LookupEntry(mapid);
    return mEntry &&
           (!mEntry->IsDungeon() || ObjectMgr::GetInstanceTemplate(mapid));
    // TODO: add check for battleground template
}

void MapManager::UnloadAll()
{
    for (auto& elem : i_maps)
        elem.second->UnloadAll();

    while (!i_maps.empty())
    {
        delete i_maps.begin()->second;
        i_maps.erase(i_maps.begin());
    }

    sTerrainMgr::Instance()->UnloadAll();
}

uint32 MapManager::GetNumInstances()
{
    uint32 ret = 0;
    for (auto& elem : i_maps)
    {
        Map* map = elem.second;
        if (!map->IsDungeon())
            continue;
        ret += 1;
    }
    return ret;
}

uint32 MapManager::GetNumPlayersInInstances()
{
    uint32 ret = 0;
    for (auto& elem : i_maps)
    {
        Map* map = elem.second;
        if (!map->IsDungeon())
            continue;
        ret += map->GetPlayers().getSize();
    }
    return ret;
}

///// returns a new or existing Instance
///// in case of battlegrounds it will only return an existing map, those maps
/// are created by bg-system
Map* MapManager::CreateInstance(uint32 id, Player* player)
{
    Map* map = nullptr;
    Map* pNewMap = nullptr;
    uint32 NewInstanceId = 0; // instanceId of the resulting map
    const MapEntry* entry = sMapStore.LookupEntry(id);

    if (entry->IsBattleGroundOrArena())
    {
        // find existing bg map for player
        NewInstanceId = player->GetBattleGroundId();
        assert(NewInstanceId);
        map = FindMap(id, NewInstanceId);
        assert(map);
    }
    else if (DungeonPersistentState* pSave =
                 player->GetInstanceBindForZoning(id))
    {
        // solo/perm/group
        NewInstanceId = pSave->GetInstanceId();
        map = FindMap(id, NewInstanceId);
        // it is possible that the save exists but the map doesn't
        if (!map)
            pNewMap = CreateDungeonMap(
                id, NewInstanceId, pSave->GetDifficulty(), pSave);
    }
    else
    {
        // if no instanceId via group members or instance saves is found
        // the instance will be created for the first time
        NewInstanceId =
            sMapPersistentStateMgr::Instance()->reserve_instance_id();
        Difficulty diff = player->GetGroup() ?
                              player->GetGroup()->GetDifficulty() :
                              player->GetDifficulty();
        pNewMap = CreateDungeonMap(id, NewInstanceId, diff);
        if (!pNewMap)
            sMapPersistentStateMgr::Instance()->free_instance_id(NewInstanceId);
    }

    // add a new map object into the registry
    if (pNewMap)
    {
        i_maps[MapID(id, NewInstanceId)] = pNewMap;
        map = pNewMap;
    }

    return map;
}

DungeonMap* MapManager::CreateDungeonMap(uint32 id, uint32 InstanceId,
    Difficulty difficulty, DungeonPersistentState* save)
{
    // make sure we have a valid map id
    const MapEntry* entry = sMapStore.LookupEntry(id);
    if (!entry)
    {
        logging.error("CreateDungeonMap: no entry for map %d", id);
        assert(false);
    }
    if (!ObjectMgr::GetInstanceTemplate(id))
    {
        logging.error("CreateDungeonMap: no instance template for map %d", id);
        assert(false);
    }

    // some instances only have one difficulty
    if (entry && !entry->SupportsHeroicMode())
        difficulty = DUNGEON_DIFFICULTY_NORMAL;

    LOG_DEBUG(logging,
        "MapInstanced::CreateDungeonMap: %s map instance %d for %d created "
        "with difficulty %d",
        save ? "" : "new ", InstanceId, id, difficulty);

    auto map = new DungeonMap(id, InstanceId, difficulty);

    // Dungeons can have saved instance data
    bool load_data = save != nullptr;
    map->CreateInstanceData(load_data);

    return map;
}

BattleGroundMap* MapManager::CreateBattleGroundMap(
    uint32 id, uint32 InstanceId, BattleGround* bg)
{
    LOG_DEBUG(logging,
        "MapInstanced::CreateBattleGroundMap: instance:%d for map:%d and "
        "bgType:%d created.",
        InstanceId, id, bg->GetTypeID());

    auto map = new BattleGroundMap(id, InstanceId);
    assert(map->IsBattleGroundOrArena());
    map->SetBG(bg);
    bg->SetBgMap(map);

    // add map into map container
    i_maps[MapID(id, InstanceId)] = map;

    // BGs/Arenas not have saved instance data
    map->CreateInstanceData(false);

    return map;
}
