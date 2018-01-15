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

#include "MapTree.h"
#include "DBCStores.h"
#include "ModelInstance.h"
#include "VMapDefinitions.h"
#include "VMapManager2.h"
#include "World.h"
#include "WorldModel.h"
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>

using G3D::Vector3;

namespace VMAP
{
class MapRayCallback
{
public:
    MapRayCallback(ModelInstance* val) : prims(val), hit(false) {}
    bool operator()(const G3D::Ray& ray, uint32 entry, float& distance,
        bool pStopAtFirstHit = true)
    {
        bool result = prims[entry].intersectRay(ray, distance, pStopAtFirstHit);
        if (result)
            hit = true;
        return result;
    }
    bool didHit() { return hit; }

protected:
    ModelInstance* prims;
    bool hit;
};

class AreaInfoCallback
{
public:
    AreaInfoCallback(ModelInstance* val) : prims(val) {}
    void operator()(const Vector3& point, uint32 entry)
    {
#ifdef VMAP_DEBUG
        LOG_DEBUG(
            logging, "trying to intersect '%s'", prims[entry].name.c_str());
#endif
        prims[entry].intersectPoint(point, aInfo);
    }

    ModelInstance* prims;
    AreaInfo aInfo;
};

class LocationInfoCallback
{
public:
    LocationInfoCallback(ModelInstance* val, LocationInfo& info)
      : prims(val), locInfo(info), result(false)
    {
    }
    void operator()(const Vector3& point, uint32 entry)
    {
#ifdef VMAP_DEBUG
        LOG_DEBUG(
            logging, "trying to intersect '%s'", prims[entry].name.c_str());
#endif
        if (prims[entry].GetLocationInfo(point, locInfo))
            result = true;
    }

    ModelInstance* prims;
    LocationInfo& locInfo;
    bool result;
};

class AllIntersectionsCallback
{
public:
    AllIntersectionsCallback(ModelInstance* val) : prims(val) {}
    bool operator()(const G3D::Ray& ray, uint32 entry, float dist, bool)
    {
        G3D::Ray ray_copy = ray;
        bool hit = false;
        while (true)
        {
            if (!prims[entry].intersectRay(ray_copy, dist, true))
                break;
            hit = true;

            bool use = false;
            if (dist <= 0.01f)
                dist = 0.01f;
            else
                use = true;

            ray_copy = ray_copy.bump(dist);

            if (use)
                points.push_back(ray_copy.origin());

            // reset max distance
            dist = 10000.0f;
        }

        return hit;
    }
    std::vector<G3D::Vector3> points;

protected:
    ModelInstance* prims;
};

//=========================================================

std::string StaticMapTree::getTileFileName(
    uint32 mapID, uint32 tileX, uint32 tileY)
{
    std::stringstream tilefilename;
    tilefilename.fill('0');
    tilefilename << std::setw(3) << mapID << "_";
    // tilefilename << std::setw(2) << tileX << "_" << std::setw(2) << tileY <<
    // ".vmtile";
    tilefilename << std::setw(2) << tileY << "_" << std::setw(2) << tileX
                 << ".vmtile";
    return tilefilename.str();
}

bool StaticMapTree::getAreaInfo(Vector3& pos, uint32& flags, int32& adtId,
    int32& rootId, int32& groupId) const
{
    AreaInfoCallback intersectionCallBack(iTreeValues);
    iTree.intersectPoint(pos, intersectionCallBack);
    if (intersectionCallBack.aInfo.result)
    {
        flags = intersectionCallBack.aInfo.flags;
        adtId = intersectionCallBack.aInfo.adtId;
        rootId = intersectionCallBack.aInfo.rootId;
        groupId = intersectionCallBack.aInfo.groupId;
        pos.z = intersectionCallBack.aInfo.ground_Z;
        return true;
    }
    return false;
}

bool StaticMapTree::GetLocationInfo(
    const Vector3& pos, LocationInfo& info) const
{
    LocationInfoCallback intersectionCallBack(iTreeValues, info);
    iTree.intersectPoint(pos, intersectionCallBack);
    return intersectionCallBack.result;
}

StaticMapTree::StaticMapTree(uint32 mapID, std::string basePath)
  : iMapID(mapID), iTreeValues(nullptr), iBasePath(std::move(basePath))
{
    if (iBasePath.length() > 0 &&
        (iBasePath[iBasePath.length() - 1] != '/' ||
            iBasePath[iBasePath.length() - 1] != '\\'))
    {
        iBasePath.append("/");
    }
}

//=========================================================
//! Make sure to call unloadMap() to unregister acquired model references before
// destroying
StaticMapTree::~StaticMapTree()
{
    delete[] iTreeValues;
}

//=========================================================
/**
If intersection is found within pMaxDist, sets pMaxDist to intersection distance
and returns true.
Else, pMaxDist is not modified and returns false;
*/

bool StaticMapTree::getIntersectionTime(
    const G3D::Ray& pRay, float& pMaxDist, bool pStopAtFirstHit) const
{
    float distance = pMaxDist;
    MapRayCallback intersectionCallBack(iTreeValues);
    iTree.intersectRay(pRay, intersectionCallBack, distance, pStopAtFirstHit);
    if (intersectionCallBack.didHit())
        pMaxDist = distance;
    return intersectionCallBack.didHit();
}
//=========================================================

std::vector<std::string> StaticMapTree::getModelNames(
    const G3D::Vector3& pos1, const G3D::Vector3& pos2) const
{
    std::vector<std::string> v;

    float dist = (pos2 - pos1).magnitude();
    if (dist < 0.1f || dist > 1000.0f)
        return v;

    G3D::Ray ray = G3D::Ray::fromOriginAndDirection(pos1, (pos2 - pos1) / dist);

    auto callback =
        [this, &v](const G3D::Ray& ray, uint32 entry, float& dist, bool stop)
    {
        bool hit = iTreeValues[entry].intersectRay(ray, dist, stop);
        if (hit)
            v.push_back(iTreeValues[entry].name);
        return hit;
    };

    iTree.intersectRay(ray, callback, dist, false);
    return v;
}
//=========================================================

bool StaticMapTree::isInLineOfSight(
    const Vector3& pos1, const Vector3& pos2) const
{
    float maxDist = (pos2 - pos1).magnitude();
    // valid map coords should *never ever* produce float overflow, but this
    // would produce NaNs too:
    assert(maxDist < std::numeric_limits<float>::max());
    // prevent NaN values which can cause BIH intersection to enter infinite
    // loop
    if (maxDist < 1e-10f)
        return true;
    // direction with length of 1
    G3D::Ray ray =
        G3D::Ray::fromOriginAndDirection(pos1, (pos2 - pos1) / maxDist);
    if (getIntersectionTime(ray, maxDist, true))
        return false;

    return true;
}

//=========================================================

float StaticMapTree::getHeight(const Vector3& pPos, float maxSearchDist) const
{
    float height = G3D::inf();
    Vector3 dir = Vector3(0, 0, -1);
    G3D::Ray ray(pPos, dir); // direction with length of 1
    float maxDist = maxSearchDist;
    if (getIntersectionTime(ray, maxDist, false))
    {
        height = pPos.z - maxDist;
    }
    return (height);
}

//=========================================================

bool StaticMapTree::CanLoadMap(
    const std::string& vmapPath, uint32 mapID, uint32 tileX, uint32 tileY)
{
    std::string basePath = vmapPath;
    if (basePath.length() > 0 && (basePath[basePath.length() - 1] != '/' ||
                                     basePath[basePath.length() - 1] != '\\'))
        basePath.append("/");
    std::string fullname = basePath + VMapManager2::getMapFileName(mapID);
    bool success = true;
    FILE* rf = fopen(fullname.c_str(), "rb");
    if (!rf)
        return false;
    // TODO: check magic number when implemented...
    char tiled;
    char chunk[8];
    if (!readChunk(rf, chunk, VMAP_MAGIC, 8) ||
        fread(&tiled, sizeof(char), 1, rf) != 1)
    {
        fclose(rf);
        return false;
    }
    if (tiled)
    {
        std::string tilefile = basePath + getTileFileName(mapID, tileX, tileY);
        FILE* tf = fopen(tilefile.c_str(), "rb");
        if (!tf)
            success = false;
        else
        {
            if (!readChunk(tf, chunk, VMAP_MAGIC, 8))
                success = false;
            fclose(tf);
        }
    }
    fclose(rf);
    return success;
}

//=========================================================

bool StaticMapTree::InitMap(
    const std::string& fname, VMapManager2* vm, bool wmoTree, bool mmapGen)
{
    LOG_DEBUG(logging, "Initializing StaticMapTree '%s'", fname.c_str());
    bool success = true;
    std::string fullname = iBasePath + fname;
    FILE* rf = fopen(fullname.c_str(), "rb");
    if (!rf)
        return false;
    else
    {
        char chunk[8];
        // general info
        if (!readChunk(rf, chunk, VMAP_MAGIC, 8))
            success = false;
        char tiled = 0;
        if (success && fread(&tiled, sizeof(char), 1, rf) != 1)
            success = false;
        iIsTiled = bool(tiled);
        // Nodes
        if (success && !readChunk(rf, chunk, "NODE", 4))
            success = false;
        if (success)
            success = iTree.readFromFile(rf);
        if (success)
        {
            iNTreeValues = iTree.primCount();
            iTreeValues = new ModelInstance[iNTreeValues];
        }

        if (success && !readChunk(rf, chunk, "GOBJ", 4))
            success = false;
        // global model spawns
        // only non-tiled maps have them, and if so exactly one (so far at
        // least...)
        ModelSpawn spawn;
#ifdef VMAP_DEBUG
        LOG_DEBUG(logging, "Map isTiled: %u", static_cast<uint32>(iIsTiled));
#endif
        if (!iIsTiled && ModelSpawn::readFromFile(rf, spawn))
        {
            // either we fill with .WMO or .M2
            // TODO: Probably better to check (spawn.flags & MOD_M2)?
            if (!mmapGen && !(wmoTree == vm->isWmoModel(spawn.name)))
            {
                fseek(rf, 4, SEEK_CUR);
                goto end_of_initmap;
            }

            WorldModel* model = vm->acquireModelInstance(iBasePath, spawn.name);
            LOG_DEBUG(logging, "StaticMapTree::InitMap(): loading %s",
                spawn.name.c_str());
            if (model)
            {
                // assume that global model always is the first and only tree
                // value (could be improved...)
                iTreeValues[0] = ModelInstance(spawn, model);
                iLoadedSpawns[0] = 1;

                // Register in Liquid Cache if WMO has liquid
                if (spawn.flags & MOD_HAS_LIQUID)
                {
                    // NOTE: linked to a dummy function if we're building tools
                    // and not the game
                    // Implementation exists in:
                    // mmap_generator/VMapExtensions.cpp & DBCStores.cpp
                    auto& v = model->getGroupModels();
                    for (auto& group_model : v)
                    {
                        int area =
                            WmoLiquidCacheArea(iMapID, model->getRootWmoID(),
                                spawn.adtId, group_model.GetWmoID());
                        if (area != -1)
                            vm->_AddLiquidWmoCache(iMapID, area);
                    }
                }
            }
            else
            {
                success = false;
                ERROR_LOG(
                    "StaticMapTree::InitMap() could not acquire WorldModel "
                    "pointer for '%s'!",
                    spawn.name.c_str());
            }
        }
    end_of_initmap:

        fclose(rf);
    }
    return success;
}

//=========================================================

void StaticMapTree::UnloadMap(VMapManager2* vm)
{
    for (auto& elem : iLoadedSpawns)
    {
        iTreeValues[elem.first].setUnloaded();
        for (uint32 refCount = 0; refCount < elem.second; ++refCount)
            vm->releaseModelInstance(iTreeValues[elem.first].name);
    }
    iLoadedSpawns.clear();
    iLoadedTiles.clear();
}

//=========================================================

bool StaticMapTree::LoadMapTile(
    uint32 tileX, uint32 tileY, VMapManager2* vm, bool wmoTree, bool mmapGen)
{
    if (!iIsTiled)
    {
        // currently, core creates grids for all maps, whether it has terrain
        // tiles or not
        // so we need "fake" tile loads to know when we can unload map geometry
        iLoadedTiles[packTileID(tileX, tileY)] = false;
        return true;
    }
    if (!iTreeValues)
    {
        ERROR_LOG(
            "StaticMapTree::LoadMapTile(): Tree has not been initialized! "
            "[%u,%u]",
            tileX, tileY);
        return false;
    }

    // Don't reload tile if it's already loaded
    if (iLoadedTiles[packTileID(tileX, tileY)] == true)
        return true;

    bool result = true;

    std::string tilefile = iBasePath + getTileFileName(iMapID, tileX, tileY);
    FILE* tf = fopen(tilefile.c_str(), "rb");
    if (tf)
    {
        char chunk[8];
        if (!readChunk(tf, chunk, VMAP_MAGIC, 8))
            result = false;
        uint32 numSpawns = 0;
        if (result && fread(&numSpawns, sizeof(uint32), 1, tf) != 1)
            result = false;
        for (uint32 i = 0; i < numSpawns && result; ++i)
        {
            // read model spawns
            ModelSpawn spawn;
            result = ModelSpawn::readFromFile(tf, spawn);
            if (result)
            {
                // either we fill with .WMO or .M2
                // TODO: Probably better to check (spawn.flags & MOD_M2)?
                if (!mmapGen && !(wmoTree == vm->isWmoModel(spawn.name)))
                {
                    fseek(tf, 4, SEEK_CUR);
                    continue;
                }

                // acquire model instance
                WorldModel* model =
                    vm->acquireModelInstance(iBasePath, spawn.name);
                if (!model)
                    ERROR_LOG(
                        "StaticMapTree::LoadMapTile() could not acquire "
                        "WorldModel pointer for '%s'!",
                        spawn.name.c_str());

                // Register in Liquid Cache if WMO has liquid
                if (spawn.flags & MOD_HAS_LIQUID)
                {
                    // NOTE: linked to a dummy function if we're building tools
                    // and not the game
                    // Implementation exists in:
                    // mmap_generator/VMapExtensions.cpp & DBCStores.cpp
                    auto& v = model->getGroupModels();
                    for (auto& group_model : v)
                    {
                        int area =
                            WmoLiquidCacheArea(iMapID, model->getRootWmoID(),
                                spawn.adtId, group_model.GetWmoID());
                        if (area != -1)
                            vm->_AddLiquidWmoCache(iMapID, area);
                    }
                }

                // update tree
                uint32 referencedVal;

                fread(&referencedVal, sizeof(uint32), 1, tf);
                if (!iLoadedSpawns.count(referencedVal))
                {
#ifdef VMAP_DEBUG
                    if (referencedVal > iNTreeValues)
                    {
                        LOG_DEBUG(logging, "invalid tree element! (%u/%u)",
                            referencedVal, iNTreeValues);
                        continue;
                    }
#endif
                    iTreeValues[referencedVal] = ModelInstance(spawn, model);
                    iLoadedSpawns[referencedVal] = 1;
                }
                else
                {
                    ++iLoadedSpawns[referencedVal];
#ifdef VMAP_DEBUG
                    if (iTreeValues[referencedVal].ID != spawn.ID)
                        LOG_DEBUG(logging,
                            "Error: trying to load wrong spawn in node!");
                    else if (iTreeValues[referencedVal].name != spawn.name)
                        LOG_DEBUG(logging, "Error: name mismatch on GUID=%u",
                            spawn.ID);
#endif
                }
            }
        }
        iLoadedTiles[packTileID(tileX, tileY)] = true;
        fclose(tf);
    }
    else
        iLoadedTiles[packTileID(tileX, tileY)] = false;
    return result;
}

//=========================================================

void StaticMapTree::UnloadMapTile(
    uint32 tileX, uint32 tileY, VMapManager2* vm, bool wmoTree, bool mmapGen)
{
    uint32 tileID = packTileID(tileX, tileY);
    auto tile = iLoadedTiles.find(tileID);
    if (tile == iLoadedTiles.end())
    {
        ERROR_LOG(
            "StaticMapTree::UnloadMapTile(): Trying to unload non-loaded tile. "
            "Map:%u X:%u Y:%u",
            iMapID, tileX, tileY);
        return;
    }
    if (tile->second) // file associated with tile
    {
        std::string tilefile =
            iBasePath + getTileFileName(iMapID, tileX, tileY);
        FILE* tf = fopen(tilefile.c_str(), "rb");
        if (tf)
        {
            bool result = true;
            char chunk[8];
            if (!readChunk(tf, chunk, VMAP_MAGIC, 8))
                result = false;
            uint32 numSpawns;
            if (fread(&numSpawns, sizeof(uint32), 1, tf) != 1)
                result = false;
            for (uint32 i = 0; i < numSpawns && result; ++i)
            {
                // read model spawns
                ModelSpawn spawn;
                result = ModelSpawn::readFromFile(tf, spawn);
                if (result)
                {
                    if (!mmapGen && !(wmoTree == vm->isWmoModel(spawn.name)))
                    {
                        fseek(tf, 4, SEEK_CUR);
                        continue;
                    }

                    // release model instance
                    vm->releaseModelInstance(spawn.name);

                    // update tree
                    uint32 referencedNode;

                    fread(&referencedNode, sizeof(uint32), 1, tf);
                    if (!iLoadedSpawns.count(referencedNode))
                    {
                        ERROR_LOG(
                            "Trying to unload non-referenced model '%s' "
                            "(ID:%u)",
                            spawn.name.c_str(), spawn.ID);
                    }
                    else if (--iLoadedSpawns[referencedNode] == 0)
                    {
                        iTreeValues[referencedNode].setUnloaded();
                        iLoadedSpawns.erase(referencedNode);
                    }
                }
            }
            fclose(tf);
        }
    }
    iLoadedTiles.erase(tile);
}

//=========================================================

std::vector<G3D::Vector3> StaticMapTree::getAllIntersections(
    const G3D::Vector3& pos, const G3D::Vector3& direction) const
{
    float dist = G3D::inf();
    G3D::Ray r(pos, direction);
    AllIntersectionsCallback callback(iTreeValues);
    iTree.intersectRay(r, callback, dist, false);
    return callback.points;
}
}
