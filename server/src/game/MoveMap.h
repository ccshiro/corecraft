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

#ifndef _MOVE_MAP_H
#define _MOVE_MAP_H

#include "DetourAlloc.h"
#include "DetourNavMesh.h"
#include "DetourNavMeshQuery.h"
#include "Platform/Define.h"
#include <map>
#include <string>
#include <unordered_map>

//  memory management
inline void* dtCustomAlloc(int size, dtAllocHint /*hint*/)
{
    return (void*)new unsigned char[size];
}

inline void dtCustomFree(void* ptr)
{
    delete[](unsigned char*)ptr;
}

struct query_key
{
    query_key() : instance(0), pathgen_thread(false) {}
    query_key(uint32 i, bool p) : instance(i), pathgen_thread(p) {}

    uint32 instance;
    bool pathgen_thread;

    bool operator==(const query_key& other) const
    {
        return instance == other.instance &&
               pathgen_thread == other.pathgen_thread;
    }
};
// specialize std::hash<query_key>
namespace std
{
template <>
struct hash<query_key>
{
    std::size_t operator()(const query_key& k) const
    {
        return hash<uint32>()(k.instance << 1 | k.pathgen_thread);
    }
};
}

//  move map related classes
namespace MMAP
{
typedef std::unordered_map<uint32, dtTileRef> MMapTileSet;
typedef std::unordered_map<query_key, dtNavMeshQuery*> NavMeshQuerySet;

// dummy struct to hold map's mmap data
struct MMapData
{
    MMapData(dtNavMesh* mesh) : navMesh(mesh) {}
    ~MMapData()
    {
        for (auto& elem : navMeshQueries)
            dtFreeNavMeshQuery(elem.second);

        if (navMesh)
            dtFreeNavMesh(navMesh);
    }

    dtNavMesh* navMesh;

    // we have to use single dtNavMeshQuery for every instance, since those are
    // not thread safe
    NavMeshQuerySet navMeshQueries; // query_key to query
    MMapTileSet mmapLoadedTiles;    // maps [map grid coords] to [dtTile]
};

typedef std::unordered_map<uint32, MMapData*> MMapDataSet;

// singelton class
// holds all all access to mmap loading unloading and meshes
class MMapManager
{
public:
    MMapManager() : loadedTiles(0) {}
    ~MMapManager();

    bool loadMap(uint32 mapId, int32 x, int32 y);
    bool unloadMap(uint32 mapId, int32 x, int32 y);
    bool unloadMap(uint32 mapId);
    bool unloadMapInstance(uint32 mapId, uint32 instanceId);
    void loadTransports();

    // the returned [dtNavMeshQuery const*] is NOT threadsafe
    dtNavMeshQuery const* GetNavMeshQuery(
        uint32 mapId, uint32 instanceId, bool pathThread);
    dtNavMesh const* GetNavMesh(uint32 mapId);

    const dtNavMesh* GetNavMesh(const std::string& transportModel);
    const dtNavMeshQuery* GetNavMeshQuery(
        const std::string& transportModel, uint32 instanceId, bool pathThread);

    uint32 getLoadedTilesCount() const { return loadedTiles; }
    uint32 getLoadedMapsCount() const { return loadedMMaps.size(); }

private:
    bool loadMapData(uint32 mapId);
    void loadTransportData(const std::string& transportModel);
    uint32 packTileID(int32 x, int32 y);

    MMapDataSet loadedMMaps;
    std::map<std::string, MMapData*> loadedTransports;
    uint32 loadedTiles;
};

// static class
// holds all mmap global data
// access point to MMapManager singelton
class MMapFactory
{
public:
    static MMapManager* createOrGetMMapManager();
    static void clear();
    static void preventPathfindingOnMaps(const char* ignoreMapIds);
    static bool IsPathfindingEnabled(uint32 mapId);
};
}

#endif // _MOVE_MAP_H
