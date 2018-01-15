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

#include "VMapManager2.h"
#include "MapTree.h"
#include "ModelInstance.h"
#include "VMapDefinitions.h"
#include "WorldModel.h"
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

using G3D::Vector3;

namespace VMAP
{
//=========================================================

VMapManager2::VMapManager2(bool mmapGen) : iMmapGenerator(mmapGen)
{
}

//=========================================================

VMapManager2::~VMapManager2()
{
    for (auto& elem : iInstanceMapWmoTrees)
    {
        delete elem.second;
    }
    for (auto& elem : iInstanceMapM2Trees)
    {
        delete elem.second;
    }
    for (auto& elem : iLoadedModelFiles)
    {
        delete elem.second.getModel();
    }
}

//=========================================================

bool VMapManager2::isWmoModel(std::string name) const
{
    // .m2:  never blocks spell LoS
    // .wmo: always blocks spell LoS
    return name.rfind(".wmo") != std::string::npos;
}

//=========================================================

Vector3 VMapManager2::convertPositionToInternalRep(
    float x, float y, float z) const
{
    Vector3 pos;
    const float mid = 0.5f * 64.0f * 533.33333333f;
    pos.x = mid - x;
    pos.y = mid - y;
    pos.z = z;

    return pos;
}

//=========================================================

Vector3 VMapManager2::convertPositionToMangosRep(
    float x, float y, float z) const
{
    Vector3 pos;
    const float mid = 0.5f * 64.0f * 533.33333333f;
    pos.x = mid - x;
    pos.y = mid - y;
    pos.z = z;

    return pos;
}
//=========================================================

// move to MapTree too?
std::string VMapManager2::getMapFileName(unsigned int pMapId)
{
    std::stringstream fname;
    fname.width(3);
    fname << std::setfill('0') << pMapId
          << std::string(MAP_FILENAME_EXTENSION2);
    return fname.str();
}

//=========================================================

VMAPLoadResult VMapManager2::loadMap(
    const char* pBasePath, unsigned int pMapId, int x, int y)
{
    if (_loadMap(pMapId, pBasePath, x, y))
        return VMAP_LOAD_RESULT_OK;
    else
        return VMAP_LOAD_RESULT_ERROR;
}

//=========================================================
// load one tile (internal use only)

bool VMapManager2::_loadMap(unsigned int pMapId, const std::string& basePath,
    uint32 tileX, uint32 tileY)
{
    auto instanceM2Tree = iInstanceMapM2Trees.find(pMapId);
    auto instanceWmoTree = iInstanceMapWmoTrees.find(pMapId);
    if (instanceM2Tree == iInstanceMapM2Trees.end())
    {
        std::string mapFileName = getMapFileName(pMapId);
        auto newM2Tree = new StaticMapTree(pMapId, basePath);
        if (!newM2Tree->InitMap(mapFileName, this, false, iMmapGenerator))
        {
            delete newM2Tree;
            return false;
        }
        StaticMapTree* newWmoTree;
        if (!iMmapGenerator)
        {
            newWmoTree = new StaticMapTree(pMapId, basePath);
            if (!newWmoTree->InitMap(mapFileName, this, true))
            {
                delete newM2Tree;
                delete newWmoTree;
                return false;
            }
        }
        instanceM2Tree =
            iInstanceMapM2Trees.insert(InstanceTreeMap::value_type(
                                           pMapId, newM2Tree)).first;
        if (!iMmapGenerator)
            instanceWmoTree =
                iInstanceMapWmoTrees.insert(InstanceTreeMap::value_type(
                                                pMapId, newWmoTree)).first;
    }
    return instanceM2Tree->second->LoadMapTile(
               tileX, tileY, this, false, iMmapGenerator) &&
           (iMmapGenerator ||
               instanceWmoTree->second->LoadMapTile(tileX, tileY, this, true));
}

//=========================================================

void VMapManager2::unloadMap(unsigned int pMapId)
{
    auto instanceM2Tree = iInstanceMapM2Trees.find(pMapId);
    auto instanceWmoTree = iInstanceMapWmoTrees.find(pMapId);
    if (instanceM2Tree != iInstanceMapM2Trees.end())
    {
        instanceM2Tree->second->UnloadMap(this);
        if (!iMmapGenerator)
            instanceWmoTree->second->UnloadMap(this);
        if (instanceM2Tree->second->numLoadedTiles() == 0)
        {
            delete instanceM2Tree->second;
            iInstanceMapM2Trees.erase(pMapId);
        }
        if (!iMmapGenerator && instanceWmoTree->second->numLoadedTiles() == 0)
        {
            delete instanceWmoTree->second;
            iInstanceMapWmoTrees.erase(pMapId);
        }
    }
}

//=========================================================

void VMapManager2::unloadMap(unsigned int pMapId, int x, int y)
{
    auto instanceM2Tree = iInstanceMapM2Trees.find(pMapId);
    auto instanceWmoTree = iInstanceMapWmoTrees.find(pMapId);
    if (instanceM2Tree != iInstanceMapM2Trees.end())
    {
        instanceM2Tree->second->UnloadMapTile(
            x, y, this, false, iMmapGenerator);
        if (!iMmapGenerator)
            instanceWmoTree->second->UnloadMapTile(x, y, this, true);
        if (instanceM2Tree->second->numLoadedTiles() == 0)
        {
            delete instanceM2Tree->second;
            iInstanceMapM2Trees.erase(pMapId);
        }
        if (!iMmapGenerator && instanceWmoTree->second->numLoadedTiles() == 0)
        {
            delete instanceWmoTree->second;
            iInstanceMapWmoTrees.erase(pMapId);
        }
    }
}

//==========================================================

// debug func: get name of models the ray from (x1,y1,z1) to (x2,y2,z2) passes
// through
std::vector<std::string> VMapManager2::getModelNames(unsigned int mapId,
    float x1, float y1, float z1, float x2, float y2, float z2)
{
    std::vector<std::string> v;

    Vector3 pos1 = convertPositionToInternalRep(x1, y1, z1);
    Vector3 pos2 = convertPositionToInternalRep(x2, y2, z2);

    auto instanceM2Tree = iInstanceMapM2Trees.find(mapId);
    if (instanceM2Tree != iInstanceMapM2Trees.end())
        v = instanceM2Tree->second->getModelNames(pos1, pos2);

    auto instanceWmoTree = iInstanceMapWmoTrees.find(mapId);
    if (instanceWmoTree != iInstanceMapWmoTrees.end())
    {
        std::vector<std::string> tmp;
        tmp = instanceWmoTree->second->getModelNames(pos1, pos2);
        v.insert(v.end(), tmp.begin(), tmp.end());
    }

    return v;
}

//==========================================================

bool VMapManager2::isInWmoLineOfSight(unsigned int mapId, float x1, float y1,
    float z1, float x2, float y2, float z2)
{
    bool result = true;
    auto instanceTree = iInstanceMapWmoTrees.find(mapId);
    if (instanceTree != iInstanceMapWmoTrees.end())
    {
        Vector3 pos1 = convertPositionToInternalRep(x1, y1, z1);
        Vector3 pos2 = convertPositionToInternalRep(x2, y2, z2);
        if (pos1 != pos2)
            result = instanceTree->second->isInLineOfSight(pos1, pos2);
    }
    return result;
}

//==========================================================

bool VMapManager2::isInM2LineOfSight(unsigned int pMapId, float x1, float y1,
    float z1, float x2, float y2, float z2)
{
    bool result = true;
    auto instanceTree = iInstanceMapM2Trees.find(pMapId);
    if (instanceTree != iInstanceMapM2Trees.end())
    {
        Vector3 pos1 = convertPositionToInternalRep(x1, y1, z1);
        Vector3 pos2 = convertPositionToInternalRep(x2, y2, z2);
        if (pos1 != pos2)
        {
            result = instanceTree->second->isInLineOfSight(pos1, pos2);
        }
    }
    return result;
}

//=========================================================
/**
get height or INVALID_HEIGHT if no height available
*/

float VMapManager2::getM2Height(
    unsigned int pMapId, float x, float y, float z, float maxSearchDist)
{
    float height = VMAP_INVALID_HEIGHT_VALUE; // no height
    auto instanceTree = iInstanceMapM2Trees.find(pMapId);
    if (instanceTree != iInstanceMapM2Trees.end())
    {
        Vector3 pos = convertPositionToInternalRep(x, y, z);
        height = instanceTree->second->getHeight(pos, maxSearchDist);
        if (!(height < G3D::inf()))
        {
            height = VMAP_INVALID_HEIGHT_VALUE; // no height
        }
    }
    return height;
}

//=========================================================
/**
get height or INVALID_HEIGHT if no height available
*/

float VMapManager2::getWmoHeight(
    unsigned int pMapId, float x, float y, float z, float maxSearchDist)
{
    float height = VMAP_INVALID_HEIGHT_VALUE; // no height
    auto instanceTree = iInstanceMapWmoTrees.find(pMapId);
    if (instanceTree != iInstanceMapWmoTrees.end())
    {
        Vector3 pos = convertPositionToInternalRep(x, y, z);
        height = instanceTree->second->getHeight(pos, maxSearchDist);
        if (!(height < G3D::inf()))
        {
            height = VMAP_INVALID_HEIGHT_VALUE; // no height
        }
    }
    return height;
}

//=========================================================

bool VMapManager2::getAreaInfo(unsigned int pMapId, float x, float y, float& z,
    uint32& flags, int32& adtId, int32& rootId, int32& groupId) const
{
    bool result = false;
    auto instanceTree = iInstanceMapWmoTrees.find(pMapId);
    if (instanceTree != iInstanceMapWmoTrees.end())
    {
        Vector3 pos = convertPositionToInternalRep(x, y, z);
        result = instanceTree->second->getAreaInfo(
            pos, flags, adtId, rootId, groupId);
        // z is not touched by convertPositionToMangosRep(), so just copy
        z = pos.z;
    }
    return (result);
}

bool VMapManager2::HasLiquidData(uint32 map, uint32 area) const
{
    auto itr = iLiquidCache.find(map);
    if (itr != iLiquidCache.end())
    {
        if (std::find(itr->second.begin(), itr->second.end(), area) !=
            itr->second.end())
            return true;
    }
    return false;
}

bool VMapManager2::GetLiquidLevel(uint32 pMapId, float x, float y, float z,
    uint8 ReqLiquidType, float& level, float& floor, uint32& type) const
{
    auto instanceTree = iInstanceMapWmoTrees.find(pMapId);
    if (instanceTree != iInstanceMapWmoTrees.end())
    {
        LocationInfo info;
        Vector3 pos = convertPositionToInternalRep(x, y, z);
        if (instanceTree->second->GetLocationInfo(pos, info))
        {
            floor = info.ground_Z;
            type = info.hitModel->GetLiquidType();
            if (ReqLiquidType && !(type & ReqLiquidType))
                return false;
            if (info.hitInstance->GetLiquidLevel(pos, info, level))
                return true;
        }
    }
    return false;
}

//=========================================================

WorldModel* VMapManager2::acquireModelInstance(
    const std::string& basepath, const std::string& filename)
{
    auto model = iLoadedModelFiles.find(filename);
    if (model == iLoadedModelFiles.end())
    {
        auto worldmodel = new WorldModel();
        if (!worldmodel->readFile(basepath + filename + ".vmo"))
        {
            ERROR_LOG("VMapManager2: could not load '%s%s.vmo'!",
                basepath.c_str(), filename.c_str());
            delete worldmodel;
            return nullptr;
        }
        LOG_DEBUG(logging, "VMapManager2: loading file '%s%s'.",
            basepath.c_str(), filename.c_str());
        model = iLoadedModelFiles.insert(std::pair<std::string, ManagedModel>(
                                             filename, ManagedModel())).first;
        model->second.setModel(worldmodel);
    }
    model->second.incRefCount();
    return model->second.getModel();
}

void VMapManager2::releaseModelInstance(const std::string& filename)
{
    auto model = iLoadedModelFiles.find(filename);
    if (model == iLoadedModelFiles.end())
    {
        ERROR_LOG("VMapManager2: trying to unload non-loaded file '%s'!",
            filename.c_str());
        return;
    }
    if (model->second.decRefCount() == 0)
    {
        LOG_DEBUG(
            logging, "VMapManager2: unloading file '%s'", filename.c_str());
        delete model->second.getModel();
        iLoadedModelFiles.erase(model);
    }
}

//=========================================================

bool VMapManager2::existsMap(
    const char* pBasePath, unsigned int pMapId, int x, int y)
{
    return StaticMapTree::CanLoadMap(std::string(pBasePath), pMapId, x, y);
}

//=========================================================

std::vector<G3D::Vector3> VMapManager2::getAllIntersections(
    uint32 map, bool wmo, G3D::Vector3 pos, const G3D::Vector3& direction) const
{
    if (wmo)
    {
        auto tree = iInstanceMapWmoTrees.find(map);
        if (tree != iInstanceMapWmoTrees.end())
        {
            pos = convertPositionToInternalRep(pos.x, pos.y, pos.z);
            auto v = tree->second->getAllIntersections(pos, direction);
            for (auto& p : v)
                p = convertPositionToMangosRep(p.x, p.y, p.z);
            return v;
        }
    }
    else
    {
        auto tree = iInstanceMapM2Trees.find(map);
        if (tree != iInstanceMapM2Trees.end())
        {
            pos = convertPositionToInternalRep(pos.x, pos.y, pos.z);
            auto v = tree->second->getAllIntersections(pos, direction);
            for (auto& p : v)
                p = convertPositionToMangosRep(p.x, p.y, p.z);
            return v;
        }
    }
    return std::vector<G3D::Vector3>();
}

//=========================================================

void VMapManager2::_AddLiquidWmoCache(uint32 map, uint32 area)
{
    auto& v = iLiquidCache[map];
    if (std::find(v.begin(), v.end(), area) == v.end())
    {
        LOG_DEBUG(logging,
            "VMapManager2: added map %u area %u to Liquid Wmo Cache.", map,
            area);
        v.push_back(area);
    }
}

} // namespace VMAP
