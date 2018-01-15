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

#ifndef _VMAPMANAGER2_H
#define _VMAPMANAGER2_H

#include "Platform/Define.h"
#include <G3D/Vector3.h>
#include <map>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

//===========================================================

#define MAP_FILENAME_EXTENSION2 ".vmtree"

#define FILENAMEBUFFER_SIZE 500

#define VMAP_INVALID_HEIGHT -100000.0f // for check
#define VMAP_INVALID_HEIGHT_VALUE \
    -200000.0f // real assigned value in unknown height case

/**
This is the main Class to manage loading and unloading of maps, line of sight,
height calculation and so on.
For each map or map tile to load it reads a directory file that contains the
ModelContainer files used by this map or map tile.
Each global map or instance has its own dynamic BSP-Tree.
The loaded ModelContainers are included in one of these BSP-Trees.
Additionally a table to match map ids and map names is used.
*/

//===========================================================

namespace VMAP
{
enum VMAPLoadResult
{
    VMAP_LOAD_RESULT_ERROR,
    VMAP_LOAD_RESULT_OK,
    VMAP_LOAD_RESULT_IGNORED,
};

class StaticMapTree;
class WorldModel;

class ManagedModel
{
public:
    ManagedModel() : iModel(nullptr), iRefCount(0) {}
    void setModel(WorldModel* model) { iModel = model; }
    WorldModel* getModel() { return iModel; }
    void incRefCount() { ++iRefCount; }
    int decRefCount() { return --iRefCount; }

protected:
    WorldModel* iModel;
    int iRefCount;
};

typedef std::unordered_map<uint32, StaticMapTree*> InstanceTreeMap;
typedef std::unordered_map<std::string, ManagedModel> ModelFileMap;

class VMapManager2
{
private:
    bool iMmapGenerator; // True if tree is used in mmap generator

    // Tree to check collision
    ModelFileMap iLoadedModelFiles;
    InstanceTreeMap iInstanceMapM2Trees;  // Contains M2 models
    InstanceTreeMap iInstanceMapWmoTrees; // Contains WMO models

    // map<mapId, vector<area>>: cache of which areas have liquid data
    std::map<uint32, std::vector<uint32>> iLiquidCache;

    bool _loadMap(
        uint32 pMapId, const std::string& basePath, uint32 tileX, uint32 tileY);
    /* void _unloadMap(uint32 pMapId, uint32 x, uint32 y); */

public:
    // public for debug
    G3D::Vector3 convertPositionToInternalRep(float x, float y, float z) const;
    G3D::Vector3 convertPositionToMangosRep(float x, float y, float z) const;
    static std::string getMapFileName(unsigned int pMapId);

    VMapManager2(bool mmapGen = false);
    ~VMapManager2();

    bool isWmoModel(std::string name) const;

    VMAPLoadResult loadMap(
        const char* pBasePath, unsigned int pMapId, int x, int y);

    void unloadMap(unsigned int pMapId, int x, int y);
    void unloadMap(unsigned int pMapId);

    // debug func: get name of models the ray from (x1,y1,z1) to (x2,y2,z2)
    // passes through
    std::vector<std::string> getModelNames(unsigned int mapId, float x1,
        float y1, float z1, float x2, float y2, float z2);

    bool isInWmoLineOfSight(unsigned int mapId, float x1, float y1, float z1,
        float x2, float y2, float z2);
    bool isInM2LineOfSight(unsigned int pMapId, float x1, float y1, float z1,
        float x2, float y2, float z2);

    float getM2Height(
        unsigned int pMapId, float x, float y, float z, float maxSearchDist);
    float getWmoHeight(
        unsigned int map_id, float x, float y, float z, float search_dist);

    bool processCommand(char* /*pCommand*/)
    {
        return false;
    } // for debug and extensions

    bool getAreaInfo(unsigned int pMapId, float x, float y, float& z,
        uint32& flags, int32& adtId, int32& rootId, int32& groupId) const;

    // HasLiquidData: queries cached data and is very quick, use if you know map
    // & zone/area before you need liquid
    bool HasLiquidData(uint32 map, uint32 area) const;
    // GetLiquidLevel: queries WMO data and is relatively slow
    bool GetLiquidLevel(uint32 pMapId, float x, float y, float z,
        uint8 ReqLiquidType, float& level, float& floor, uint32& type) const;

    WorldModel* acquireModelInstance(
        const std::string& basepath, const std::string& filename);
    void releaseModelInstance(const std::string& filename);

    // what's the use of this? o.O
    std::string getDirFileName(unsigned int pMapId, int /*x*/, int /*y*/) const
    {
        return getMapFileName(pMapId);
    }
    bool existsMap(const char* pBasePath, unsigned int pMapId, int x, int y);

    std::vector<G3D::Vector3> getAllIntersections(uint32 map, bool wmo,
        G3D::Vector3 pos, const G3D::Vector3& direction) const;

    void getInstanceMapTree(InstanceTreeMap& instanceMapTree);

    void _AddLiquidWmoCache(uint32 map, uint32 area);
};
}

#endif
