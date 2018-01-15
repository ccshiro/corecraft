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

#include "MoveMap.h"
#include "GridMap.h"
#include "logging.h"
#include "MoveMapSharedDefines.h"
#include "World.h"
#include <boost/filesystem.hpp>

namespace MMAP
{
// ######################## MMapFactory ########################
// our global singelton copy
MMapManager* g_MMapManager = nullptr;

// stores list of mapids which do not use pathfinding
std::set<uint32>* g_mmapDisabledIds = nullptr;

MMapManager* MMapFactory::createOrGetMMapManager()
{
    if (g_MMapManager == nullptr)
        g_MMapManager = new MMapManager();

    return g_MMapManager;
}

void MMapFactory::preventPathfindingOnMaps(const char* ignoreMapIds)
{
    if (!g_mmapDisabledIds)
        g_mmapDisabledIds = new std::set<uint32>();

    uint32 strLenght = strlen(ignoreMapIds) + 1;
    auto mapList = new char[strLenght];
    memcpy(mapList, ignoreMapIds, sizeof(char) * strLenght);

    char* idstr = strtok(mapList, ",");
    while (idstr)
    {
        g_mmapDisabledIds->insert(uint32(atoi(idstr)));
        idstr = strtok(nullptr, ",");
    }

    delete[] mapList;
}

bool MMapFactory::IsPathfindingEnabled(uint32 mapId)
{
    return sWorld::Instance()->getConfig(CONFIG_BOOL_MMAP_ENABLED) &&
           g_mmapDisabledIds->find(mapId) == g_mmapDisabledIds->end();
}

void MMapFactory::clear()
{
    if (g_mmapDisabledIds)
    {
        delete g_mmapDisabledIds;
        g_mmapDisabledIds = nullptr;
    }

    if (g_MMapManager)
    {
        delete g_MMapManager;
        g_MMapManager = nullptr;
    }
}

// ######################## MMapManager ########################
MMapManager::~MMapManager()
{
    for (auto& elem : loadedMMaps)
        delete elem.second;
    for (auto& elem : loadedTransports)
        delete elem.second;

    // by now we should not have maps loaded
    // if we had, tiles in MMapData->mmapLoadedTiles, their actual data is lost!
}

bool MMapManager::loadMapData(uint32 mapId)
{
    // we already have this map loaded?
    if (loadedMMaps.find(mapId) != loadedMMaps.end())
        return true;

    // load and init dtNavMesh - read parameters from file
    uint32 pathLen = sWorld::Instance()->GetDataPath().length() +
                     strlen("mmaps/%03i.mmap") + 1;
    auto fileName = new char[pathLen];
    snprintf(fileName, pathLen,
        (sWorld::Instance()->GetDataPath() + "mmaps/%03i.mmap").c_str(), mapId);

    FILE* file = fopen(fileName, "rb");
    if (!file)
    {
        LOG_DEBUG(logging,
            "MMAP:loadMapData: Error: Could not open mmap file '%s'", fileName);
        delete[] fileName;
        return false;
    }

    dtNavMeshParams params;
    fread(&params, sizeof(dtNavMeshParams), 1, file);
    fclose(file);

    dtNavMesh* mesh = dtAllocNavMesh();
    assert(mesh);
    if (dtStatusFailed(mesh->init(&params)))
    {
        dtFreeNavMesh(mesh);
        logging.error(
            "MMAP:loadMapData: Failed to initialize dtNavMesh for mmap %03u "
            "from file %s",
            mapId, fileName);
        delete[] fileName;
        return false;
    }

    delete[] fileName;

    LOG_DEBUG(logging, "MMAP:loadMapData: Loaded %03i.mmap", mapId);

    // store inside our map list
    auto mmap_data = new MMapData(mesh);
    mmap_data->mmapLoadedTiles.clear();

    loadedMMaps.insert(std::pair<uint32, MMapData*>(mapId, mmap_data));
    return true;
}

uint32 MMapManager::packTileID(int32 x, int32 y)
{
    return uint32(x << 16 | y);
}

bool MMapManager::loadMap(uint32 mapId, int32 x, int32 y)
{
    // make sure the mmap is loaded and ready to load tiles
    if (!loadMapData(mapId))
        return false;

    // get this mmap data
    MMapData* mmap = loadedMMaps[mapId];
    assert(mmap->navMesh);

    // check if we already have this tile loaded
    uint32 packedGridPos = packTileID(x, y);
    if (mmap->mmapLoadedTiles.find(packedGridPos) !=
        mmap->mmapLoadedTiles.end())
    {
        return false;
    }

    // load this tile :: mmaps/MMMXXYY.mmtile
    uint32 pathLen = sWorld::Instance()->GetDataPath().length() +
                     strlen("mmaps/%03i%02i%02i.mmtile") + 1;
    auto fileName = new char[pathLen];
    snprintf(fileName, pathLen, (sWorld::Instance()->GetDataPath() +
                                    "mmaps/%03i%02i%02i.mmtile").c_str(),
        mapId, x, y);

    FILE* file = fopen(fileName, "rb");
    if (!file)
    {
        LOG_DEBUG(
            logging, "MMAP:loadMap: Could not open mmtile file '%s'", fileName);
        delete[] fileName;
        return false;
    }
    delete[] fileName;

    // read header
    MmapTileHeader fileHeader;
    fread(&fileHeader, sizeof(MmapTileHeader), 1, file);

    if (fileHeader.mmapMagic != MMAP_MAGIC)
    {
        logging.error("MMAP:loadMap: Bad header in mmap %03u%02i%02i.mmtile",
            mapId, x, y);
        return false;
    }

    if (fileHeader.mmapVersion != MMAP_VERSION)
    {
        logging.error(
            "MMAP:loadMap: %03u%02i%02i.mmtile was built with generator v%i, "
            "expected v%i",
            mapId, x, y, fileHeader.mmapVersion, MMAP_VERSION);
        return false;
    }

    unsigned char* data =
        (unsigned char*)dtAlloc(fileHeader.size, DT_ALLOC_PERM);
    assert(data);

    size_t result = fread(data, fileHeader.size, 1, file);
    if (!result)
    {
        logging.error(
            "MMAP:loadMap: Bad header or data in mmap %03u%02i%02i.mmtile",
            mapId, x, y);
        fclose(file);
        return false;
    }

    fclose(file);

    dtTileRef tileRef = 0;

    // memory allocated for data is now managed by detour, and will be
    // deallocated when the tile is removed
    if (dtStatusSucceed(mmap->navMesh->addTile(
            data, fileHeader.size, DT_TILE_FREE_DATA, 0, &tileRef)))
    {
        mmap->mmapLoadedTiles.insert(
            std::pair<uint32, dtTileRef>(packedGridPos, tileRef));
        ++loadedTiles;
        LOG_DEBUG(logging,
            "MMAP:loadMap: Loaded mmtile %03i[%02i,%02i] into %03i[%02i,%02i]",
            mapId, x, y, mapId, ((dtMeshHeader*)data)->x,
            ((dtMeshHeader*)data)->y);
        return true;
    }
    else
    {
        logging.error(
            "MMAP:loadMap: Could not load %03u%02i%02i.mmtile into navmesh",
            mapId, x, y);
        dtFree(data);
        return false;
    }

    return false;
}

bool MMapManager::unloadMap(uint32 mapId, int32 x, int32 y)
{
    // check if we have this map loaded
    if (loadedMMaps.find(mapId) == loadedMMaps.end())
    {
        // file may not exist, therefore not loaded
        LOG_DEBUG(logging,
            "MMAP:unloadMap: Asked to unload not loaded navmesh map. "
            "%03u%02i%02i.mmtile",
            mapId, x, y);
        return false;
    }

    MMapData* mmap = loadedMMaps[mapId];

    // check if we have this tile loaded
    uint32 packedGridPos = packTileID(x, y);
    if (mmap->mmapLoadedTiles.find(packedGridPos) ==
        mmap->mmapLoadedTiles.end())
    {
        // file may not exist, therefore not loaded
        LOG_DEBUG(logging,
            "MMAP:unloadMap: Asked to unload not loaded navmesh tile. "
            "%03u%02i%02i.mmtile",
            mapId, x, y);
        return false;
    }

    dtTileRef tileRef = mmap->mmapLoadedTiles[packedGridPos];

    // unload, and mark as non loaded
    if (dtStatusFailed(mmap->navMesh->removeTile(tileRef, nullptr, nullptr)))
    {
        // this is technically a memory leak
        // if the grid is later reloaded, dtNavMesh::addTile will return error
        // but no extra memory is used
        // we cannot recover from this error - assert out
        logging.error(
            "MMAP:unloadMap: Could not unload %03u%02i%02i.mmtile from navmesh",
            mapId, x, y);
        assert(false);
    }
    else
    {
        mmap->mmapLoadedTiles.erase(packedGridPos);
        --loadedTiles;
        LOG_DEBUG(logging,
            "MMAP:unloadMap: Unloaded mmtile %03i[%02i,%02i] from %03i", mapId,
            x, y, mapId);
        return true;
    }

    return false;
}

bool MMapManager::unloadMap(uint32 mapId)
{
    if (loadedMMaps.find(mapId) == loadedMMaps.end())
    {
        // file may not exist, therefore not loaded
        LOG_DEBUG(logging,
            "MMAP:unloadMap: Asked to unload not loaded navmesh map %03u",
            mapId);
        return false;
    }

    // unload all tiles from given map
    MMapData* mmap = loadedMMaps[mapId];
    for (auto i = mmap->mmapLoadedTiles.begin();
         i != mmap->mmapLoadedTiles.end(); ++i)
    {
        uint32 x = (i->first >> 16);
        uint32 y = (i->first & 0x0000FFFF);
        if (dtStatusFailed(
                mmap->navMesh->removeTile(i->second, nullptr, nullptr)))
            logging.error(
                "MMAP:unloadMap: Could not unload %03u%02i%02i.mmtile from "
                "navmesh",
                mapId, x, y);
        else
        {
            --loadedTiles;
            LOG_DEBUG(logging,
                "MMAP:unloadMap: Unloaded mmtile %03i[%02i,%02i] from %03i",
                mapId, x, y, mapId);
        }
    }

    delete mmap;
    loadedMMaps.erase(mapId);
    LOG_DEBUG(logging, "MMAP:unloadMap: Unloaded %03i.mmap", mapId);

    return true;
}

bool MMapManager::unloadMapInstance(uint32 mapId, uint32 instanceId)
{
    // check if we have this map loaded
    if (loadedMMaps.find(mapId) == loadedMMaps.end())
    {
        // file may not exist, therefore not loaded
        LOG_DEBUG(logging,
            "MMAP:unloadMapInstance: Asked to unload not loaded navmesh map "
            "%03u",
            mapId);
        return false;
    }

    MMapData* mmap = loadedMMaps[mapId];

    auto itr = mmap->navMeshQueries.find(query_key(instanceId, true));
    if (itr != mmap->navMeshQueries.end())
    {
        dtFreeNavMeshQuery(itr->second);
        mmap->navMeshQueries.erase(itr);
    }

    itr = mmap->navMeshQueries.find(query_key(instanceId, false));
    if (itr != mmap->navMeshQueries.end())
    {
        dtFreeNavMeshQuery(itr->second);
        mmap->navMeshQueries.erase(itr);
    }

    LOG_DEBUG(logging,
        "MMAP:unloadMapInstance: Unloaded mapId %03u instanceId %u", mapId,
        instanceId);

    return true;
}

void MMapManager::loadTransports()
{
    namespace fs = boost::filesystem;
    fs::path dir(sWorld::Instance()->GetDataPath() + "mmaps/transports/");
    if (!fs::is_directory(dir))
    {
        logging.error("Missing 'mmaps/transports/' in data directory.");
        return;
    }

    for (fs::directory_iterator itr(dir); itr != fs::directory_iterator();
         ++itr)
    {
        if (!fs::is_regular_file(itr->status()))
            continue;
        if (itr->path().extension().string() != ".mmap")
            continue;

        FILE* file = fopen(itr->path().string().c_str(), "rb");
        if (!file)
        {
            std::string s =
                std::string("Unable to open ") + itr->path().string() + "!";
            perror(s.c_str());
            continue;
        }

        dtNavMeshParams params;
        fread(&params, sizeof(dtNavMeshParams), 1, file);
        fclose(file);

        dtNavMesh* mesh = dtAllocNavMesh();
        assert(mesh);
        if (dtStatusFailed(mesh->init(&params)))
        {
            dtFreeNavMesh(mesh);
            logging.error(
                "MMAP:loadMapData: Failed to initialize dtNavMesh for "
                "transport file %s",
                itr->path().string().c_str());
            continue;
        }

        auto transportModel = itr->path().stem().string();
        loadedTransports[transportModel] = new MMapData(mesh);
        loadTransportData(transportModel);
    }
}

void MMapManager::loadTransportData(const std::string& transportModel)
{
    auto itr = loadedTransports.find(transportModel);
    if (itr == loadedTransports.end())
        return;
    auto mmap = itr->second;

    auto path = sWorld::Instance()->GetDataPath() + "mmaps/transports/" +
                transportModel + ".mmtile";
    FILE* file = fopen(path.c_str(), "rb");
    if (!file)
    {
        std::string s = std::string("Could not load file: ") + path;
        perror(s.c_str());
        return;
    }

    // read header
    MmapTileHeader fileHeader;
    fread(&fileHeader, sizeof(MmapTileHeader), 1, file);

    if (fileHeader.mmapMagic != MMAP_MAGIC)
    {
        logging.error(
            "MMAP:loadTransportData: Bad header in '%s'.", path.c_str());
        fclose(file);
        return;
    }

    unsigned char* data =
        (unsigned char*)dtAlloc(fileHeader.size, DT_ALLOC_PERM);
    if (!data)
    {
        logging.error(
            "MMAP:loadTransportData: dtAlloc failed for '%s'.", path.c_str());
        fclose(file);
        return;
    }

    size_t result = fread(data, fileHeader.size, 1, file);
    if (!result)
    {
        logging.error("MMAP:loadTransportData: Header with wrong size in '%s'.",
            path.c_str());
        fclose(file);
        dtFree(data);
        return;
    }

    fclose(file);

    if (dtStatusFailed(
            mmap->navMesh->init(data, fileHeader.size, DT_TILE_FREE_DATA)))
    {
        logging.error(
            "MMAP:loadTransportData: Could not load '%s'.", path.c_str());
        dtFree(data);
    }

    LOG_DEBUG(logging, "MMAP: Successfully loaded '%s'.", path.c_str());
}

dtNavMesh const* MMapManager::GetNavMesh(uint32 mapId)
{
    if (loadedMMaps.find(mapId) == loadedMMaps.end())
        return nullptr;

    return loadedMMaps[mapId]->navMesh;
}

dtNavMeshQuery const* MMapManager::GetNavMeshQuery(
    uint32 mapId, uint32 instanceId, bool pathThread)
{
    if (loadedMMaps.find(mapId) == loadedMMaps.end())
        return nullptr;

    MMapData* mmap = loadedMMaps[mapId];
    if (mmap->navMeshQueries.find(query_key(instanceId, pathThread)) ==
        mmap->navMeshQueries.end())
    {
        // allocate mesh query
        dtNavMeshQuery* query = dtAllocNavMeshQuery();
        assert(query);
        if (dtStatusFailed(query->init(mmap->navMesh, 1024)))
        {
            dtFreeNavMeshQuery(query);
            logging.error(
                "MMAP:GetNavMeshQuery: Failed to initialize dtNavMeshQuery for "
                "mapId %03u instanceId %u",
                mapId, instanceId);
            return nullptr;
        }

        LOG_DEBUG(logging,
            "MMAP:GetNavMeshQuery: created dtNavMeshQuery for mapId %03u "
            "instanceId %u pathThreaD: %s",
            mapId, instanceId, pathThread ? "yes" : "no");
        mmap->navMeshQueries[query_key(instanceId, pathThread)] = query;
    }

    return mmap->navMeshQueries[query_key(instanceId, pathThread)];
}

const dtNavMesh* MMapManager::GetNavMesh(const std::string& transportModel)
{
    if (loadedTransports.find(transportModel) == loadedTransports.end())
        return nullptr;

    return loadedTransports[transportModel]->navMesh;
}

const dtNavMeshQuery* MMapManager::GetNavMeshQuery(
    const std::string& transportModel, uint32 instanceId, bool pathThread)
{
    if (loadedTransports.find(transportModel) == loadedTransports.end())
        return nullptr;

    MMapData* mmap = loadedTransports[transportModel];

    if (mmap->navMeshQueries.find(query_key(instanceId, pathThread)) ==
        mmap->navMeshQueries.end())
    {
        // allocate mesh query
        dtNavMeshQuery* query = dtAllocNavMeshQuery();
        assert(query);
        if (dtStatusFailed(query->init(mmap->navMesh, 1024)))
        {
            dtFreeNavMeshQuery(query);
            logging.error(
                "MMAP:GetNavMeshQuery: Failed to initialize dtNavMeshQuery for "
                "transport %s instance %u!",
                transportModel.c_str(), instanceId);
            return nullptr;
        }

        LOG_DEBUG(logging,
            "MMAP:GetNavMeshQuery: created dtNavMeshQuery for transport %s "
            "instance %u pathThread: %s",
            transportModel.c_str(), instanceId, pathThread ? "yes" : "no");
        mmap->navMeshQueries[query_key(instanceId, pathThread)] = query;
    }

    return mmap->navMeshQueries[query_key(instanceId, pathThread)];
}
}
