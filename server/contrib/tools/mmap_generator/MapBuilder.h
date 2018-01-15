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

#ifndef _MAP_BUILDER_H
#define _MAP_BUILDER_H

#include "DetourNavMesh.h"
#include "IntermediateValues.h"
#include "LockedQueue.h"
#include "Recast.h"
#include "TerrainBuilder.h"
#include "VMapManager2.h"
#include "WorldModel.h"
#include <map>
#include <set>
#include <vector>

using namespace std;
using namespace VMAP;

namespace G3D
{
class AABox;
}

namespace MMAP
{
typedef map<uint32, set<uint32>*> TileList;
struct Tile
{
    Tile()
      : chf(nullptr), solid(nullptr), cset(nullptr), pmesh(nullptr),
        dmesh(nullptr)
    {
    }
    ~Tile()
    {
        rcFreeCompactHeightfield(chf);
        rcFreeContourSet(cset);
        rcFreeHeightField(solid);
        rcFreePolyMesh(pmesh);
        rcFreePolyMeshDetail(dmesh);
    }
    rcCompactHeightfield* chf;
    rcHeightfield* solid;
    rcContourSet* cset;
    rcPolyMesh* pmesh;
    rcPolyMeshDetail* dmesh;
};

class MapBuilder
{
public:
    MapBuilder(int threads = 1, bool skipLiquid = false,
        bool skipContinents = false, bool skipJunkMaps = true,
        bool skipBattlegrounds = false, bool debugOutput = false,
        const char* offMeshFilePath = nullptr);

    ~MapBuilder();

    // builds all mmap tiles for the specified map id (ignores skip settings)
    void buildMap(uint32 mapID);
    void buildMeshFromFile(char* name);

    // build navmesh for all transports
    void buildTransports();

    // builds an mmap tile for the specified map and its mesh
    void buildSingleTile(uint32 mapID, uint32 tileX, uint32 tileY);

    // builds list of maps, then builds all of mmap tiles (based on the skip
    // settings)
    void buildAllMaps();

private:
    // detect maps and tiles
    void discoverTiles();
    set<uint32>* getTileList(uint32 mapID);

    void buildNavMesh(uint32 mapID, dtNavMesh*& navMesh);
    void buildTransportNavMesh(const std::string& out_filename, float bmin[3],
        float bmax[3], dtNavMesh*& navMesh);

    void buildTile(
        uint32 mapID, uint32 tileX, uint32 tileY, dtNavMesh* navMesh);

    // move map building
    void buildMoveMapTile(uint32 mapID, uint32 tileX, uint32 tileY,
        MeshData& meshData, float bmin[3], float bmax[3], dtNavMesh* navMesh);

    void buildMoveMapTransport(const std::string& out_filename,
        MeshData& meshData, float bmin[3], float bmax[3], dtNavMesh* navMesh);

    void getTileBounds(uint32 tileX, uint32 tileY, float* verts, int vertCount,
        float* bmin, float* bmax);
    void getGridBounds(
        uint32 mapID, uint32& minX, uint32& minY, uint32& maxX, uint32& maxY);

    bool shouldSkipMap(uint32 mapID);
    bool isTransportMap(uint32 mapID);
    bool shouldSkipTile(uint32 mapID, uint32 tileX, uint32 tileY);

    void buildTransport(const std::string& wmo_filename);

    TerrainBuilder* m_terrainBuilder;

    int m_threads;

    TileList m_tiles;

    bool m_debugOutput;

    const char* m_offMeshFilePath;
    bool m_skipContinents;
    bool m_skipJunkMaps;
    bool m_skipBattlegrounds;

    // build performance - not really used for now
    rcContext* m_rcContext;
};

class worker_thread
{
public:
    worker_thread(
        int thread_id, MapBuilder* builder, MaNGOS::locked_queue<uint32>* queue)
      : id_(thread_id), queue_(queue), builder_(builder)
    {
    }
    void operator()() const;

private:
    int id_;
    MaNGOS::locked_queue<uint32>* queue_;
    MapBuilder* builder_;
};
}

#endif
