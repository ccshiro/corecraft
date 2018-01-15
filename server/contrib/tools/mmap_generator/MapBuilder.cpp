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

#include "MapBuilder.h"
#include "DetourCommon.h"
#include "DetourNavMesh.h"
#include "DetourNavMeshBuilder.h"
#include "MMapCommon.h"
#include "MapSettings.h"
#include "MapTree.h"
#include "ModelInstance.h"
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include <vector>

using namespace VMAP;

namespace MMAP
{
MapBuilder::MapBuilder(int threads, bool skipLiquid, bool skipContinents,
    bool skipJunkMaps, bool skipBattlegrounds, bool debugOutput,
    const char* offMeshFilePath)
  : m_terrainBuilder(nullptr), m_threads(threads), m_debugOutput(debugOutput),
    m_offMeshFilePath(offMeshFilePath), m_skipContinents(skipContinents),
    m_skipJunkMaps(skipJunkMaps), m_skipBattlegrounds(skipBattlegrounds),
    m_rcContext(nullptr)
{
    m_terrainBuilder = new TerrainBuilder(skipLiquid);

    m_rcContext = new rcContext(false);

    discoverTiles();
}

/**************************************************************************/
MapBuilder::~MapBuilder()
{
    for (auto& elem : m_tiles)
    {
        (elem).second->clear();
        delete (elem).second;
    }

    delete m_terrainBuilder;
    delete m_rcContext;
}

/**************************************************************************/
void MapBuilder::discoverTiles()
{
    vector<string> files;
    uint32 mapID, tileX, tileY, tileID, count = 0;
    char filter[12];

    printf("Discovering maps... ");
    getDirContents(files, "maps");
    for (auto& file : files)
    {
        mapID = uint32(atoi(file.substr(0, 3).c_str()));
        if (m_tiles.find(mapID) == m_tiles.end())
        {
            m_tiles.insert(pair<uint32, set<uint32>*>(mapID, new set<uint32>));
            count++;
        }
    }

    files.clear();
    getDirContents(files, "vmaps", "*.vmtree");
    for (auto& file : files)
    {
        mapID = uint32(atoi(file.substr(0, 3).c_str()));
        m_tiles.insert(pair<uint32, set<uint32>*>(mapID, new set<uint32>));
        count++;
    }
    printf("found %u.\n", count);

    count = 0;
    printf("Discovering tiles... ");
    for (auto& elem : m_tiles)
    {
        set<uint32>* tiles = (elem).second;
        mapID = (elem).first;

        sprintf(filter, "%03u*.vmtile", mapID);
        files.clear();
        getDirContents(files, "vmaps", filter);
        for (auto& file : files)
        {
            tileX = uint32(atoi(file.substr(7, 2).c_str()));
            tileY = uint32(atoi(file.substr(4, 2).c_str()));
            tileID = StaticMapTree::packTileID(tileY, tileX);

            tiles->insert(tileID);
            count++;
        }

        sprintf(filter, "%03u*", mapID);
        files.clear();
        getDirContents(files, "maps", filter);
        for (auto& file : files)
        {
            tileY = uint32(atoi(file.substr(3, 2).c_str()));
            tileX = uint32(atoi(file.substr(5, 2).c_str()));
            tileID = StaticMapTree::packTileID(tileX, tileY);

            if (tiles->insert(tileID).second)
                count++;
        }
    }
    printf("found %u.\n\n", count);
}

/**************************************************************************/
set<uint32>* MapBuilder::getTileList(uint32 mapID)
{
    auto itr = m_tiles.find(mapID);
    if (itr != m_tiles.end())
        return (*itr).second;

    auto tiles = new set<uint32>();
    m_tiles.insert(pair<uint32, set<uint32>*>(mapID, tiles));
    return tiles;
}

/**************************************************************************/
void MapBuilder::buildAllMaps()
{
    buildTransports();

    if (m_threads > 1)
    {
        printf("Building all maps using %u threads...\n", m_threads);
        boost::this_thread::sleep(boost::posix_time::seconds(2));
        printf("Queued map ids: ");
    }

    MaNGOS::locked_queue<uint32> map_ids;

    for (auto& elem : m_tiles)
    {
        uint32 mapID = (elem).first;
        if (!shouldSkipMap(mapID))
        {
            if (m_threads > 1)
            {
                printf("%u ", mapID);
                map_ids.push(mapID);
            }
            else
            {
                printf("Building map %u single-threaded.\n", mapID);
                buildMap(mapID);
            }
        }
    }

    if (m_threads <= 1)
        return;

    printf("\n");

    std::vector<boost::shared_ptr<boost::thread>> threads;
    for (int i = 0; i < m_threads; ++i)
    {
        boost::shared_ptr<boost::thread> t(
            new boost::thread(worker_thread(i, this, &map_ids)));
        threads.push_back(t);
    }
    for (int i = 0; i < m_threads; ++i)
        threads[i]->join();
}

void worker_thread::operator()() const
{
    while (true)
    {
        uint32 map_id;
        if (!queue_->pop(map_id))
            return;

        printf("Thread %d building map %u.\n", id_, map_id);
        builder_->buildMap(map_id);
    }
}

/**************************************************************************/
void MapBuilder::getGridBounds(
    uint32 mapID, uint32& minX, uint32& minY, uint32& maxX, uint32& maxY)
{
    maxX = INT_MAX;
    maxY = INT_MAX;
    minX = INT_MIN;
    minY = INT_MIN;

    float bmin[3] = {0, 0, 0};
    float bmax[3] = {0, 0, 0};
    float lmin[3] = {0, 0, 0};
    float lmax[3] = {0, 0, 0};
    MeshData meshData;

    // make sure we process maps which don't have tiles
    // initialize the static tree, which loads WDT models
    if (!m_terrainBuilder->loadVMap(mapID, 64, 64, meshData))
        return;

    // get the coord bounds of the model data
    if (meshData.solidVerts.size() + meshData.liquidVerts.size() == 0)
        return;

    // get the coord bounds of the model data
    if (meshData.solidVerts.size() && meshData.liquidVerts.size())
    {
        rcCalcBounds(meshData.solidVerts.getCArray(),
            meshData.solidVerts.size() / 3, bmin, bmax);
        rcCalcBounds(meshData.liquidVerts.getCArray(),
            meshData.liquidVerts.size() / 3, lmin, lmax);
        rcVmin(bmin, lmin);
        rcVmax(bmax, lmax);
    }
    else if (meshData.solidVerts.size())
        rcCalcBounds(meshData.solidVerts.getCArray(),
            meshData.solidVerts.size() / 3, bmin, bmax);
    else
        rcCalcBounds(meshData.liquidVerts.getCArray(),
            meshData.liquidVerts.size() / 3, lmin, lmax);

    // convert coord bounds to grid bounds
    maxX = 32 - bmin[0] / GRID_SIZE;
    maxY = 32 - bmin[2] / GRID_SIZE;
    minX = 32 - bmax[0] / GRID_SIZE;
    minY = 32 - bmax[2] / GRID_SIZE;
}

void MapBuilder::buildMeshFromFile(char* name)
{
    FILE* file = fopen(name, "rb");
    if (!file)
        return;

    printf("Building mesh from file\n");
    int tileX, tileY, mapId;
    if (fread(&mapId, sizeof(int), 1, file) != 1)
    {
        fclose(file);
        return;
    }
    if (fread(&tileX, sizeof(int), 1, file) != 1)
    {
        fclose(file);
        return;
    }
    if (fread(&tileY, sizeof(int), 1, file) != 1)
    {
        fclose(file);
        return;
    }

    dtNavMesh* navMesh = nullptr;
    buildNavMesh(mapId, navMesh);
    if (!navMesh)
    {
        printf("[Map %03i] Failed creating navmesh!\n", mapId);
        fclose(file);
        return;
    }

    uint32 verticesCount, indicesCount;
    if (fread(&verticesCount, sizeof(uint32), 1, file) != 1)
    {
        fclose(file);
        return;
    }
    if (fread(&indicesCount, sizeof(uint32), 1, file) != 1)
    {
        fclose(file);
        return;
    }

    auto verts = new float[verticesCount];
    auto inds = new int[indicesCount];

    if (fread(verts, sizeof(float), verticesCount, file) != verticesCount)
    {
        fclose(file);
        delete[] verts;
        delete[] inds;
        return;
    }
    if (fread(inds, sizeof(int), indicesCount, file) != indicesCount)
    {
        fclose(file);
        delete[] verts;
        delete[] inds;
        return;
    }

    MeshData data;

    for (uint32 i = 0; i < verticesCount; ++i)
        data.solidVerts.append(verts[i]);
    delete[] verts;

    for (uint32 i = 0; i < indicesCount; ++i)
        data.solidTris.append(inds[i]);
    delete[] inds;

    TerrainBuilder::cleanVertices(data.solidVerts, data.solidTris);
    // get bounds of current tile
    float bmin[3], bmax[3];
    getTileBounds(tileX, tileY, data.solidVerts.getCArray(),
        data.solidVerts.size() / 3, bmin, bmax);

    // build navmesh tile
    buildMoveMapTile(mapId, tileX, tileY, data, bmin, bmax, navMesh);
    fclose(file);
}

void MapBuilder::buildTransports()
{
    puts("Building transports.");
    buildTransport("Transportship_A.wmo");
    buildTransport("Transportship_Ne_Stationary.wmo");
    buildTransport("Transportship_Ne.wmo");
    buildTransport("Transportship.wmo");
    buildTransport("Transport_Zeppelin.wmo");
    buildTransport("Transport_Shipwreck.wmo");
    buildTransport("Transport_Zeppelin.wmo");
}

/**************************************************************************/
void MapBuilder::buildSingleTile(uint32 mapID, uint32 tileX, uint32 tileY)
{
    dtNavMesh* navMesh = nullptr;
    buildNavMesh(mapID, navMesh);
    if (!navMesh)
    {
        printf("[Map %03u] Failed creating navmesh!\n", mapID);
        return;
    }

    buildTile(mapID, tileX, tileY, navMesh);
    dtFreeNavMesh(navMesh);
}

/**************************************************************************/
void MapBuilder::buildMap(uint32 mapID)
{
    printf("Building map %03u:\n", mapID);

    set<uint32>* tiles = getTileList(mapID);

    // make sure we process maps which don't have tiles
    if (!tiles->size())
    {
        // convert coord bounds to grid bounds
        uint32 minX, minY, maxX, maxY;
        getGridBounds(mapID, minX, minY, maxX, maxY);

        // add all tiles within bounds to tile list.
        for (uint32 i = minX; i <= maxX; ++i)
            for (uint32 j = minY; j <= maxY; ++j)
                tiles->insert(StaticMapTree::packTileID(i, j));
    }

    if (!tiles->empty())
    {
        // build navMesh
        dtNavMesh* navMesh = nullptr;
        buildNavMesh(mapID, navMesh);
        if (!navMesh)
        {
            printf("[Map %03u] Failed creating navmesh!\n", mapID);
            return;
        }

        // now start building mmtiles for each tile
        printf("[Map %03i] We have %u tiles.                          \n",
            mapID, (unsigned int)tiles->size());
        for (const auto& tile : *tiles)
        {
            uint32 tileX, tileY;

            // unpack tile coords
            StaticMapTree::unpackTileID((tile), tileX, tileY);

            if (shouldSkipTile(mapID, tileX, tileY))
                continue;

            buildTile(mapID, tileX, tileY, navMesh);
        }

        dtFreeNavMesh(navMesh);
    }

    printf("[Map %03i] Complete!\n", mapID);
}

/**************************************************************************/
void MapBuilder::buildTile(
    uint32 mapID, uint32 tileX, uint32 tileY, dtNavMesh* navMesh)
{
    printf("[Map %u] Building tile [%02u,%02u]\n", mapID, tileX, tileY);

    MeshData meshData;

    // get heightmap data
    m_terrainBuilder->loadMap(mapID, tileX, tileY, meshData);

    // get model data
    m_terrainBuilder->loadVMap(mapID, tileY, tileX, meshData);

    // if there is no data, give up now
    if (!meshData.solidVerts.size() && !meshData.liquidVerts.size())
        return;

    // remove unused vertices
    TerrainBuilder::cleanVertices(meshData.solidVerts, meshData.solidTris);
    TerrainBuilder::cleanVertices(meshData.liquidVerts, meshData.liquidTris);

    // gather all mesh data for final data check, and bounds calculation
    G3D::Array<float> allVerts;
    allVerts.append(meshData.liquidVerts);
    allVerts.append(meshData.solidVerts);

    if (!allVerts.size())
        return;

    // get bounds of current tile
    float bmin[3], bmax[3];
    getTileBounds(
        tileX, tileY, allVerts.getCArray(), allVerts.size() / 3, bmin, bmax);

    m_terrainBuilder->loadOffMeshConnections(
        mapID, tileX, tileY, meshData, m_offMeshFilePath);

    // build navmesh tile
    buildMoveMapTile(mapID, tileX, tileY, meshData, bmin, bmax, navMesh);
}

/**************************************************************************/
void MapBuilder::buildNavMesh(uint32 mapID, dtNavMesh*& navMesh)
{
    set<uint32>* tiles = getTileList(mapID);

    // old code for non-statically assigned bitmask sizes:
    ///*** calculate number of bits needed to store tiles & polys ***/
    // int tileBits = dtIlog2(dtNextPow2(tiles->size()));
    // if (tileBits < 1) tileBits = 1;                                     //
    // need at least one bit!
    // int polyBits = sizeof(dtPolyRef)*8 - SALT_MIN_BITS - tileBits;

    int polyBits = STATIC_POLY_BITS;

    int maxTiles = tiles->size();
    int maxPolysPerTile = 1 << polyBits;

    /***          calculate bounds of map         ***/

    uint32 tileXMin = 64, tileYMin = 64, tileXMax = 0, tileYMax = 0, tileX,
           tileY;
    for (const auto& tile : *tiles)
    {
        StaticMapTree::unpackTileID(tile, tileX, tileY);

        if (tileX > tileXMax)
            tileXMax = tileX;
        else if (tileX < tileXMin)
            tileXMin = tileX;

        if (tileY > tileYMax)
            tileYMax = tileY;
        else if (tileY < tileYMin)
            tileYMin = tileY;
    }

    // use Max because '32 - tileX' is negative for values over 32
    float bmin[3], bmax[3];
    getTileBounds(tileXMax, tileYMax, nullptr, 0, bmin, bmax);

    /***       now create the navmesh       ***/

    // navmesh creation params
    dtNavMeshParams navMeshParams;
    memset(&navMeshParams, 0, sizeof(dtNavMeshParams));
    navMeshParams.tileWidth = GRID_SIZE;
    navMeshParams.tileHeight = GRID_SIZE;
    rcVcopy(navMeshParams.orig, bmin);
    navMeshParams.maxTiles = maxTiles;
    navMeshParams.maxPolys = maxPolysPerTile;

    navMesh = dtAllocNavMesh();
    printf("Creating navMesh...                     \n");
    if (!navMesh->init(&navMeshParams))
    {
        printf("[Map %u] Failed creating navmesh!\n", mapID);
        return;
    }

    char fileName[25];
    sprintf(fileName, "mmaps/%03u.mmap", mapID);

    FILE* file = fopen(fileName, "wb");
    if (!file)
    {
        dtFreeNavMesh(navMesh);
        char message[1024];
        sprintf(message, "[Map %03u] Failed to open %s for writing!\n", mapID,
            fileName);
        perror(message);
        return;
    }

    // now that we know navMesh params are valid, we can write them to file
    fwrite(&navMeshParams, sizeof(dtNavMeshParams), 1, file);
    fclose(file);
}

/**************************************************************************/
void MapBuilder::buildTransportNavMesh(const std::string& out_filename,
    float bmin[3], float[3], dtNavMesh*& navMesh)
{
    // navmesh creation params
    dtNavMeshParams navMeshParams;
    memset(&navMeshParams, 0, sizeof(dtNavMeshParams));
    navMeshParams.tileWidth = GRID_SIZE;
    navMeshParams.tileHeight = GRID_SIZE;
    rcVcopy(navMeshParams.orig, bmin);
    navMeshParams.maxTiles = 1;
    navMeshParams.maxPolys = 1 << STATIC_POLY_BITS;

    navMesh = dtAllocNavMesh();
    puts("Creating transport navMesh...");
    if (!navMesh->init(&navMeshParams))
    {
        dtFreeNavMesh(navMesh);
        navMesh = nullptr;
        puts("Failed creating transport navmesh!");
        return;
    }

    std::string path =
        std::string("mmaps/transports/") + out_filename + ".mmap";

    FILE* file = fopen(path.c_str(), "wb");
    if (!file)
    {
        dtFreeNavMesh(navMesh);
        navMesh = nullptr;
        std::string s = std::string("Failed to open ") + path + " for writing!";
        perror(s.c_str());
        return;
    }

    // now that we know navMesh params are valid, we can write them to file
    fwrite(&navMeshParams, sizeof(dtNavMeshParams), 1, file);
    fclose(file);
}

/**************************************************************************/
void MapBuilder::buildMoveMapTile(uint32 mapID, uint32 tileX, uint32 tileY,
    MeshData& meshData, float bmin[3], float bmax[3], dtNavMesh* navMesh)
{
    // console output
    char tileString[20];
    sprintf(tileString, "[%02i,%02i]: ", tileX, tileY);
    printf(
        "%s Building movemap tiles...                        \n", tileString);

    IntermediateValues iv;

    float* tVerts = meshData.solidVerts.getCArray();
    int tVertCount = meshData.solidVerts.size() / 3;
    int* tTris = meshData.solidTris.getCArray();
    int tTriCount = meshData.solidTris.size() / 3;

    float* lVerts = meshData.liquidVerts.getCArray();
    int lVertCount = meshData.liquidVerts.size() / 3;
    int* lTris = meshData.liquidTris.getCArray();
    int lTriCount = meshData.liquidTris.size() / 3;
    uint8* lTriFlags = meshData.liquidType.getCArray();

    auto config = get_config_for(mapID);
    bool erode = should_erode_walkable_areas(mapID);

    rcVcopy(config.bmin, bmin);
    rcVcopy(config.bmax, bmax);

    // this sets the dimensions of the heightfield - should maybe happen before
    // border padding
    rcCalcGridSize(
        config.bmin, config.bmax, config.cs, &config.width, &config.height);

    // allocate subregions : tiles
    auto tiles = new Tile[TILES_PER_MAP * TILES_PER_MAP];

    // Initialize per tile config.
    rcConfig tileCfg = config;
    tileCfg.width = config.tileSize + config.borderSize * 2;
    tileCfg.height = config.tileSize + config.borderSize * 2;

    // merge per tile poly and detail meshes
    auto pmmerge = new rcPolyMesh* [TILES_PER_MAP * TILES_PER_MAP];

    auto dmmerge = new rcPolyMeshDetail* [TILES_PER_MAP * TILES_PER_MAP];

    int nmerge = 0;
    // build all tiles
    for (int y = 0; y < TILES_PER_MAP; ++y)
    {
        for (int x = 0; x < TILES_PER_MAP; ++x)
        {
            Tile& tile = tiles[x + y * TILES_PER_MAP];

            // Calculate the per tile bounding box.
            tileCfg.bmin[0] =
                config.bmin[0] +
                float(x * config.tileSize - config.borderSize) * config.cs;
            tileCfg.bmin[2] =
                config.bmin[2] +
                float(y * config.tileSize - config.borderSize) * config.cs;
            tileCfg.bmax[0] =
                config.bmin[0] +
                float((x + 1) * config.tileSize + config.borderSize) *
                    config.cs;
            tileCfg.bmax[2] =
                config.bmin[2] +
                float((y + 1) * config.tileSize + config.borderSize) *
                    config.cs;

            // build heightfield
            tile.solid = rcAllocHeightfield();
            if (!tile.solid ||
                !rcCreateHeightfield(m_rcContext, *tile.solid, tileCfg.width,
                    tileCfg.height, tileCfg.bmin, tileCfg.bmax, tileCfg.cs,
                    tileCfg.ch))
            {
                printf("%s Failed building heightfield!            \n",
                    tileString);
                continue;
            }

            // mark all walkable tiles, both liquids and solids
            auto triFlags = new unsigned char[tTriCount];
            memset(triFlags, NAV_GROUND, tTriCount * sizeof(unsigned char));
            rcClearUnwalkableTriangles(m_rcContext, tileCfg.walkableSlopeAngle,
                tVerts, tVertCount, tTris, tTriCount, triFlags);
            rcRasterizeTriangles(m_rcContext, tVerts, tVertCount, tTris,
                triFlags, tTriCount, *tile.solid, config.walkableClimb);
            delete[] triFlags;

            rcFilterLowHangingWalkableObstacles(
                m_rcContext, config.walkableClimb, *tile.solid);
            rcFilterLedgeSpans(m_rcContext, tileCfg.walkableHeight,
                tileCfg.walkableClimb, *tile.solid);
            rcFilterWalkableLowHeightSpans(
                m_rcContext, tileCfg.walkableHeight, *tile.solid);

            rcRasterizeTriangles(m_rcContext, lVerts, lVertCount, lTris,
                lTriFlags, lTriCount, *tile.solid, config.walkableClimb);

            // compact heightfield spans
            tile.chf = rcAllocCompactHeightfield();
            if (!tile.chf ||
                !rcBuildCompactHeightfield(m_rcContext, tileCfg.walkableHeight,
                    tileCfg.walkableClimb, *tile.solid, *tile.chf))
            {
                printf("%s Failed compacting heightfield!            \n",
                    tileString);
                continue;
            }

            // build polymesh intermediates
            if (erode &&
                !rcErodeWalkableArea(
                    m_rcContext, config.walkableRadius, *tile.chf))
            {
                printf("%s Failed eroding area!                    \n",
                    tileString);
                continue;
            }

            if (!rcBuildDistanceField(m_rcContext, *tile.chf))
            {
                printf("%s Failed building distance field!         \n",
                    tileString);
                continue;
            }

            if (!rcBuildRegions(m_rcContext, *tile.chf, tileCfg.borderSize,
                    tileCfg.minRegionArea, tileCfg.mergeRegionArea))
            {
                printf("%s Failed building regions!                \n",
                    tileString);
                continue;
            }

            tile.cset = rcAllocContourSet();
            if (!tile.cset ||
                !rcBuildContours(m_rcContext, *tile.chf,
                    tileCfg.maxSimplificationError, tileCfg.maxEdgeLen,
                    *tile.cset))
            {
                printf("%s Failed building contours!               \n",
                    tileString);
                continue;
            }

            // build polymesh
            tile.pmesh = rcAllocPolyMesh();
            if (!tile.pmesh ||
                !rcBuildPolyMesh(m_rcContext, *tile.cset,
                    tileCfg.maxVertsPerPoly, *tile.pmesh))
            {
                printf("%s Failed building polymesh!               \n",
                    tileString);
                continue;
            }

            tile.dmesh = rcAllocPolyMeshDetail();
            if (!tile.dmesh ||
                !rcBuildPolyMeshDetail(m_rcContext, *tile.pmesh, *tile.chf,
                    tileCfg.detailSampleDist, tileCfg.detailSampleMaxError,
                    *tile.dmesh))
            {
                printf("%s Failed building polymesh detail!        \n",
                    tileString);
                continue;
            }

            // free those up
            // we may want to keep them in the future for debug
            // but right now, we don't have the code to merge them
            rcFreeHeightField(tile.solid);
            tile.solid = nullptr;
            rcFreeCompactHeightfield(tile.chf);
            tile.chf = nullptr;
            rcFreeContourSet(tile.cset);
            tile.cset = nullptr;

            if (tile.pmesh)
            {
                pmmerge[nmerge] = tile.pmesh;
                dmmerge[nmerge] = tile.dmesh;
                ++nmerge;
            }
        }
    }

    iv.polyMesh = rcAllocPolyMesh();
    if (!iv.polyMesh)
    {
        printf("%s alloc iv.polyMesh FAILED!\n", tileString);
        delete[] pmmerge;
        delete[] dmmerge;
        delete[] tiles;
        return;
    }
    rcMergePolyMeshes(m_rcContext, pmmerge, nmerge, *iv.polyMesh);

    iv.polyMeshDetail = rcAllocPolyMeshDetail();
    if (!iv.polyMeshDetail)
    {
        printf("%s alloc m_dmesh FAILED!          \n", tileString);
        delete[] pmmerge;
        delete[] dmmerge;
        delete[] tiles;
        return;
    }
    rcMergePolyMeshDetails(m_rcContext, dmmerge, nmerge, *iv.polyMeshDetail);

    // free things up
    delete[] pmmerge;
    delete[] dmmerge;
    delete[] tiles;

    // set polygons as walkable
    // TODO: special flags for DYNAMIC polygons, ie surfaces that can be turned
    // on and off
    for (int i = 0; i < iv.polyMesh->npolys; ++i)
        if (iv.polyMesh->areas[i] & RC_WALKABLE_AREA)
            iv.polyMesh->flags[i] = iv.polyMesh->areas[i];

    // setup mesh parameters
    dtNavMeshCreateParams params;
    memset(&params, 0, sizeof(params));
    params.verts = iv.polyMesh->verts;
    params.vertCount = iv.polyMesh->nverts;
    params.polys = iv.polyMesh->polys;
    params.polyAreas = iv.polyMesh->areas;
    params.polyFlags = iv.polyMesh->flags;
    params.polyCount = iv.polyMesh->npolys;
    params.nvp = iv.polyMesh->nvp;
    params.detailMeshes = iv.polyMeshDetail->meshes;
    params.detailVerts = iv.polyMeshDetail->verts;
    params.detailVertsCount = iv.polyMeshDetail->nverts;
    params.detailTris = iv.polyMeshDetail->tris;
    params.detailTriCount = iv.polyMeshDetail->ntris;

    params.offMeshConVerts = meshData.offMeshConnections.getCArray();
    params.offMeshConCount = meshData.offMeshConnections.size() / 6;
    params.offMeshConRad = meshData.offMeshConnectionRads.getCArray();
    params.offMeshConDir = meshData.offMeshConnectionDirs.getCArray();
    params.offMeshConAreas = meshData.offMeshConnectionsAreas.getCArray();
    params.offMeshConFlags = meshData.offMeshConnectionsFlags.getCArray();

    params.walkableHeight =
        BASE_UNIT_DIM * config.walkableHeight; // agent height
    params.walkableRadius =
        BASE_UNIT_DIM * config.walkableRadius; // agent radius
    params.walkableClimb =
        BASE_UNIT_DIM *
        config
            .walkableClimb; // keep less that walkableHeight (aka agent height)!
    params.tileX =
        (((bmin[0] + bmax[0]) / 2) - navMesh->getParams()->orig[0]) / GRID_SIZE;
    params.tileY =
        (((bmin[2] + bmax[2]) / 2) - navMesh->getParams()->orig[2]) / GRID_SIZE;
    rcVcopy(params.bmin, bmin);
    rcVcopy(params.bmax, bmax);
    params.cs = config.cs;
    params.ch = config.ch;
    params.tileLayer = 0;
    params.buildBvTree = true;

    // will hold final navmesh
    unsigned char* navData = nullptr;
    int navDataSize = 0;

    do
    {
        // these values are checked within dtCreateNavMeshData - handle them
        // here
        // so we have a clear error message
        if (params.nvp > DT_VERTS_PER_POLYGON)
        {
            printf("%s Invalid verts-per-polygon value!        \n", tileString);
            continue;
        }
        if (params.vertCount >= 0xffff)
        {
            printf("%s Too many vertices!                      \n", tileString);
            continue;
        }
        if (!params.vertCount || !params.verts)
        {
            // occurs mostly when adjacent tiles have models
            // loaded but those models don't span into this tile

            // message is an annoyance
            // printf("%sNo vertices to build tile!              \n",
            // tileString);
            continue;
        }
        if (!params.polyCount || !params.polys ||
            TILES_PER_MAP * TILES_PER_MAP == params.polyCount)
        {
            // we have flat tiles with no actual geometry - don't build those,
            // its useless
            // keep in mind that we do output those into debug info
            // drop tiles with only exact count - some tiles may have geometry
            // while having less tiles
            printf(
                "%s No polygons to build on tile!              \n", tileString);
            continue;
        }
        if (!params.detailMeshes || !params.detailVerts || !params.detailTris)
        {
            printf("%s No detail mesh to build tile!           \n", tileString);
            continue;
        }

        printf("%s Building navmesh tile...                \n", tileString);
        if (!dtCreateNavMeshData(&params, &navData, &navDataSize))
        {
            printf("%s Failed building navmesh tile!           \n", tileString);
            continue;
        }

        dtTileRef tileRef = 0;
        printf("%s Adding tile to navmesh...                \n", tileString);
        // DT_TILE_FREE_DATA tells detour to unallocate memory when the tile
        // is removed via removeTile()
        dtStatus dtResult = navMesh->addTile(
            navData, navDataSize, DT_TILE_FREE_DATA, 0, &tileRef);
        if (!tileRef || dtResult != DT_SUCCESS)
        {
            printf(
                "%s Failed adding tile to navmesh!           \n", tileString);
            continue;
        }

        // file output
        char fileName[255];
        sprintf(fileName, "mmaps/%03u%02i%02i.mmtile", mapID, tileY, tileX);
        FILE* file = fopen(fileName, "wb");
        if (!file)
        {
            char message[1024];
            sprintf(message, "[Map %03u] Failed to open %s for writing!\n",
                mapID, fileName);
            perror(message);
            navMesh->removeTile(tileRef, nullptr, nullptr);
            continue;
        }

        printf("%s Writing to file...                      \n", tileString);

        // write header
        MmapTileHeader header;
        header.usesLiquids = m_terrainBuilder->usesLiquids();
        header.size = uint32(navDataSize);
        fwrite(&header, sizeof(MmapTileHeader), 1, file);

        // write data
        fwrite(navData, sizeof(unsigned char), navDataSize, file);
        fclose(file);

        // now that tile is written to disk, we can unload it
        navMesh->removeTile(tileRef, nullptr, nullptr);
    } while (0);

    if (m_debugOutput)
    {
        // restore padding so that the debug visualization is correct
        for (int i = 0; i < iv.polyMesh->nverts; ++i)
        {
            unsigned short* v = &iv.polyMesh->verts[i * 3];
            v[0] += (unsigned short)config.borderSize;
            v[2] += (unsigned short)config.borderSize;
        }

        iv.generateObjFile(mapID, tileX, tileY, meshData);
        iv.writeIV(mapID, tileX, tileY);
    }
}

void MapBuilder::buildMoveMapTransport(const std::string& out_filename,
    MeshData& meshData, float bmin[3], float bmax[3], dtNavMesh* navMesh)
{
#define MMT_ERRCHK(x)                                   \
    if (!x)                                             \
    {                                                   \
        puts("Failed building move map for transport"); \
        return;                                         \
    }

    /*
     * Build poly mesh
     */
    IntermediateValues iv;

    float* tVerts = meshData.solidVerts.getCArray();
    int tVertCount = meshData.solidVerts.size() / 3;
    int* tTris = meshData.solidTris.getCArray();
    int tTriCount = meshData.solidTris.size() / 3;

    rcConfig config;
    memset(&config, 0, sizeof(rcConfig));
    config.maxVertsPerPoly = DT_VERTS_PER_POLYGON;
    config.cs = 0.3f;
    config.ch = 0.2f;
    config.walkableSlopeAngle = 80; // degrees
    config.walkableHeight = (int)ceilf(2.0f / config.ch);
    config.walkableClimb = (int)floorf(0.9f / config.ch);
    config.walkableRadius = (int)ceilf(0.6f / config.cs);
    config.maxEdgeLen = (int)(12.0f / 0.3f);
    config.maxSimplificationError = 1.3f;
    config.minRegionArea = (int)rcSqr(8.0f);    // Note: area = size*size
    config.mergeRegionArea = (int)rcSqr(20.0f); // Note: area = size*size
    config.maxVertsPerPoly = (int)6;
    config.detailSampleDist = 0.3f * 6.0f;
    config.detailSampleMaxError = 0.3f * 1.0f;

    rcVcopy(config.bmin, bmin);
    rcVcopy(config.bmax, bmax);
    rcCalcGridSize(
        config.bmin, config.bmax, config.cs, &config.width, &config.height);

    iv.polyMesh = rcAllocPolyMesh();
    iv.polyMeshDetail = rcAllocPolyMeshDetail();
    auto heightfield = rcAllocHeightfield();
    auto compactHf = rcAllocCompactHeightfield();
    auto contourSet = rcAllocContourSet();

    // build heightfield
    MMT_ERRCHK(rcCreateHeightfield(m_rcContext, *heightfield, config.width,
        config.height, config.bmin, config.bmax, config.cs, config.ch));
    // rasterize input
    auto triFlags = new unsigned char[tTriCount];
    memset(triFlags, NAV_GROUND, tTriCount * sizeof(unsigned char));
    rcMarkWalkableTriangles(m_rcContext, config.walkableSlopeAngle, tVerts,
        tVertCount, tTris, tTriCount, triFlags);
    rcRasterizeTriangles(m_rcContext, tVerts, tVertCount, tTris, triFlags,
        tTriCount, *heightfield, config.walkableClimb);
    delete[] triFlags;
    // various filter passes
    rcFilterLowHangingWalkableObstacles(
        m_rcContext, config.walkableClimb, *heightfield);
    rcFilterLedgeSpans(
        m_rcContext, config.walkableHeight, config.walkableClimb, *heightfield);
    rcFilterWalkableLowHeightSpans(
        m_rcContext, config.walkableHeight, *heightfield);
    // build compact heightfield
    MMT_ERRCHK(rcBuildCompactHeightfield(m_rcContext, config.walkableHeight,
        config.walkableClimb, *heightfield, *compactHf));
    MMT_ERRCHK(
        rcErodeWalkableArea(m_rcContext, config.walkableRadius, *compactHf));
    // build polymesh intermediates
    MMT_ERRCHK(rcBuildDistanceField(m_rcContext, *compactHf));
    MMT_ERRCHK(rcBuildRegions(m_rcContext, *compactHf, config.borderSize,
        config.minRegionArea, config.mergeRegionArea));
    MMT_ERRCHK(rcBuildContours(m_rcContext, *compactHf,
        config.maxSimplificationError, config.maxEdgeLen, *contourSet));
    // build low-res and high-res poly mesh
    MMT_ERRCHK(rcBuildPolyMesh(
        m_rcContext, *contourSet, config.maxVertsPerPoly, *iv.polyMesh));
    MMT_ERRCHK(rcBuildPolyMeshDetail(m_rcContext, *iv.polyMesh, *compactHf,
        config.detailSampleDist, config.detailSampleMaxError,
        *iv.polyMeshDetail));

    // free memory
    rcFreeHeightField(heightfield);
    rcFreeCompactHeightfield(compactHf);
    rcFreeContourSet(contourSet);

    /*
     * Build nav mesh
     */

    // update poly flags for areas
    for (int i = 0; i < iv.polyMesh->npolys; ++i)
    {
        if (iv.polyMesh->areas[i] & RC_WALKABLE_AREA)
            iv.polyMesh->flags[i] = NAV_GROUND;
    }

    dtNavMeshCreateParams params;
    memset(&params, 0, sizeof(params));
    params.verts = iv.polyMesh->verts;
    params.vertCount = iv.polyMesh->nverts;
    params.polys = iv.polyMesh->polys;
    params.polyAreas = iv.polyMesh->areas;
    params.polyFlags = iv.polyMesh->flags;
    params.polyCount = iv.polyMesh->npolys;
    params.nvp = iv.polyMesh->nvp;
    params.detailMeshes = iv.polyMeshDetail->meshes;
    params.detailVerts = iv.polyMeshDetail->verts;
    params.detailVertsCount = iv.polyMeshDetail->nverts;
    params.detailTris = iv.polyMeshDetail->tris;
    params.detailTriCount = iv.polyMeshDetail->ntris;
    params.walkableHeight = 2.0f;
    params.walkableRadius = 0.6f;
    params.walkableClimb = 0.9f;
    rcVcopy(params.bmin, bmin);
    rcVcopy(params.bmax, bmax);
    params.cs = config.cs;
    params.ch = config.ch;
    params.buildBvTree = true;

    // will hold final navmesh
    unsigned char* navData = nullptr;
    int navDataSize = 0;

    if (!dtCreateNavMeshData(&params, &navData, &navDataSize))
    {
        puts("dtCreateNavMeshData() failed for transport");
        return;
    }

    dtStatus dtResult = navMesh->init(navData, navDataSize, DT_TILE_FREE_DATA);
    if (dtResult != DT_SUCCESS)
    {
        puts("navMesh::addTile failed for transport");
        return;
    }

    // file output
    std::string path =
        std::string("mmaps/transports/") + out_filename + ".mmtile";
    FILE* file = fopen(path.c_str(), "wb");
    if (!file)
    {
        std::string s = std::string("Failed to open ") + path + " for writing!";
        perror(s.c_str());
        return;
    }

    puts("Build done. Writing navmesh to file...");

    // write header
    MmapTileHeader header;
    header.usesLiquids = false;
    header.size = uint32(navDataSize);
    fwrite(&header, sizeof(MmapTileHeader), 1, file);

    // write data
    fwrite(navData, sizeof(unsigned char), navDataSize, file);
    fclose(file);

    // TODO: output debug data if m_outputDebug is true
}

/**************************************************************************/
void MapBuilder::getTileBounds(uint32 tileX, uint32 tileY, float* verts,
    int vertCount, float* bmin, float* bmax)
{
    // this is for elevation
    if (verts && vertCount)
        rcCalcBounds(verts, vertCount, bmin, bmax);
    else
    {
        bmin[1] = FLT_MIN;
        bmax[1] = FLT_MAX;
    }

    // this is for width and depth
    bmax[0] = (32 - int(tileX)) * GRID_SIZE;
    bmax[2] = (32 - int(tileY)) * GRID_SIZE;
    bmin[0] = bmax[0] - GRID_SIZE;
    bmin[2] = bmax[2] - GRID_SIZE;
}

/**************************************************************************/
bool MapBuilder::shouldSkipMap(uint32 mapID)
{
    if (m_skipContinents)
        switch (mapID)
        {
        case 0:
        case 1:
        case 530:
            return true;
        default:
            break;
        }

    if (m_skipJunkMaps)
        switch (mapID)
        {
        case 13:  // test.wdt
        case 25:  // ScottTest.wdt
        case 29:  // Test.wdt
        case 42:  // Colin.wdt
        case 169: // EmeraldDream.wdt (unused, and very large)
        case 451: // development.wdt
            return true;
        default:
            if (isTransportMap(mapID))
                return true;
            break;
        }

    if (m_skipBattlegrounds)
        switch (mapID)
        {
        case 30:  // AV
        case 37:  // ?
        case 489: // WSG
        case 529: // AB
        case 566: // EotS
            return true;
        default:
            break;
        }

    return false;
}

/**************************************************************************/
bool MapBuilder::isTransportMap(uint32 mapID)
{
    switch (mapID)
    {
    // transport maps
    case 582:
    case 584:
    case 586:
    case 587:
    case 588:
    case 589:
    case 590:
    case 591:
    case 593:
        return true;
    default:
        return false;
    }
}

/**************************************************************************/
bool MapBuilder::shouldSkipTile(uint32 mapID, uint32 tileX, uint32 tileY)
{
    char fileName[255];
    sprintf(fileName, "mmaps/%03u%02i%02i.mmtile", mapID, tileY, tileX);
    FILE* file = fopen(fileName, "rb");
    if (!file)
        return false;

    MmapTileHeader header;
    int count = fread(&header, sizeof(MmapTileHeader), 1, file);
    fclose(file);
    if (count != 1)
        return false;

    if (header.mmapMagic != MMAP_MAGIC ||
        header.dtVersion != uint32(DT_NAVMESH_VERSION))
        return false;

    if (header.mmapVersion != MMAP_VERSION)
        return false;

    return true;
}

void MapBuilder::buildTransport(const std::string& wmo_filename)
{
    std::unique_ptr<VMapManager2> vmapManager(new VMapManager2(true));
    auto model = vmapManager->acquireModelInstance("vmaps/", wmo_filename);

    if (!model)
    {
        printf("Failed building transport: '%s'!\n", wmo_filename.c_str());
        return;
    }

    // build mesh data for our transport
    MeshData meshData;
    m_terrainBuilder->buildSingleModel(meshData, model, false, 1.0f,
        G3D::Vector3(0.0f, 0.0f, 0.0f), G3D::Matrix3::identity(), true);

    // calculate bounding box of geometry
    float bmin[3] = {0}, bmax[3] = {0};
    rcCalcBounds(meshData.solidVerts.getCArray(),
        meshData.solidVerts.size() / 3, bmin, bmax);

    dtNavMesh* navMesh = nullptr;
    buildTransportNavMesh(wmo_filename, bmin, bmax, navMesh);
    if (!navMesh)
    {
        puts("Failed building transport: could not create nav mesh!");
        return;
    }

    buildMoveMapTransport(wmo_filename, meshData, bmin, bmax, navMesh);

    dtFreeNavMesh(navMesh);

    printf("Built transport: '%s'\n", wmo_filename.c_str());
}
}
