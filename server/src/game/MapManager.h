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

#ifndef MANGOS_MAPMANAGER_H
#define MANGOS_MAPMANAGER_H

#include "Common.h"
#include "Map.h"
#include "SharedDefines.h"
#include "Platform/Define.h"
#include "Policies/Singleton.h"

class BattleGround;
class threaded_maps;

struct MANGOS_DLL_DECL MapID
{
    explicit MapID(uint32 id) : nMapId(id), nInstanceId(0) {}
    MapID(uint32 id, uint32 instid) : nMapId(id), nInstanceId(instid) {}

    bool operator<(const MapID& val) const
    {
        if (nMapId == val.nMapId)
            return nInstanceId < val.nInstanceId;

        return nMapId < val.nMapId;
    }

    bool operator==(const MapID& val) const
    {
        return nMapId == val.nMapId && nInstanceId == val.nInstanceId;
    }

    uint32 nMapId;
    uint32 nInstanceId;
};

class MANGOS_DLL_DECL MapManager
{
    friend class MaNGOS::Singleton<MapManager>;
    friend class World;
    MapManager* get_inst() { return this; }

public:
    ~MapManager();

    typedef std::map<MapID, Map*> MapMapType;

    Map* CreateMap(uint32, const WorldObject* obj);
    Map* CreateBgMap(uint32 mapid, BattleGround* bg);
    Map* FindMap(uint32 mapid, uint32 instanceId = 0) const;

    // only const version for outer users
    void DeleteInstance(uint32 mapid, uint32 instanceId);

    void Update(uint32);

    void SetMapUpdateInterval(uint32 t)
    {
        if (t > MIN_MAP_UPDATE_DELAY)
            t = MIN_MAP_UPDATE_DELAY;

        i_timer.SetInterval(t);
        i_timer.Reset();
    }

    // void LoadGrid(int mapid, int instId, float x, float y, const WorldObject*
    // obj, bool no_unload = false);
    void UnloadAll();

    static bool ExistMapAndVMap(uint32 mapid, float x, float y);
    static bool IsValidMAP(uint32 mapid);

    // modulos a radian orientation to the range of 0..2PI
    static float NormalizeOrientation(float o)
    {
        // fmod only supports positive numbers. Thus we have
        // to emulate negative numbers
        if (o < 0)
        {
            float mod = o * -1;
            mod = fmod(mod, 2.0f * M_PI_F);
            mod = -mod + 2.0f * M_PI_F;
            return mod;
        }
        return fmod(o, 2.0f * M_PI_F);
    }

    void RemoveAllObjectsInRemoveList();

    bool CanPlayerEnter(uint32 mapid, Player* player);
    void InitializeVisibilityDistanceInfo();

    /* statistics */
    uint32 GetNumInstances();
    uint32 GetNumPlayersInInstances();

    // get list of all maps
    const MapMapType& Maps() const { return i_maps; }

    template <typename Do>
    void DoForAllMapsWithMapId(uint32 mapId, Do& _do);

    threaded_maps* get_threaded_maps() { return threaded_maps_; }

private:
    MapManager();

    MapManager(const MapManager&);
    MapManager& operator=(const MapManager&);

    Map* CreateInstance(uint32 id, Player* player);
    DungeonMap* CreateDungeonMap(uint32 id, uint32 InstanceId,
        Difficulty difficulty, DungeonPersistentState* save = nullptr);
    BattleGroundMap* CreateBattleGroundMap(
        uint32 id, uint32 InstanceId, BattleGround* bg);

    MapMapType i_maps;
    IntervalTimer i_timer;

    threaded_maps* threaded_maps_;
    ShortIntervalTimer batch_timers_[(int)BatchUpdates::count];
};

template <typename Do>
inline void MapManager::DoForAllMapsWithMapId(uint32 mapId, Do& _do)
{
    MapMapType::const_iterator start = i_maps.lower_bound(MapID(mapId, 0));
    MapMapType::const_iterator end = i_maps.lower_bound(MapID(mapId + 1, 0));
    for (auto itr = start; itr != end; ++itr)
        _do(itr->second);
}

#define sMapMgr MaNGOS::Singleton<MapManager>

#endif
