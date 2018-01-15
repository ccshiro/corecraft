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

#include "GridMap.h"
#include "DBCEnums.h"
#include "DBCStores.h"
#include "logging.h"
#include "Map.h"
#include "MapManager.h"
#include "MoveMap.h"
#include "Util.h"
#include "VMapFactory.h"
#include "World.h"
#include "Policies/Singleton.h"
#include <boost/filesystem.hpp>
#include <iomanip>
#include <sstream>

#define MAP_RESOLUTION 128
#define SIZE_OF_GRIDS 533.3333f

char const* MAP_MAGIC = "MAPS";
char const* MAP_VERSION_MAGIC = "b1.3";
char const* MAP_AREA_MAGIC = "AREA";
char const* MAP_HEIGHT_MAGIC = "MHGT";
char const* MAP_LIQUID_MAGIC = "MLIQ";

GridMap::GridMap()
{
    m_flags = 0;

    // Area data
    m_gridArea = 0;
    m_area_map = nullptr;

    // Height level data
    m_gridHeight = INVALID_HEIGHT_VALUE;
    m_gridGetHeight = &GridMap::getHeightFromFlat;
    m_V9 = nullptr;
    m_V8 = nullptr;

    // Liquid data
    m_liquidType = 0;
    m_liquid_offX = 0;
    m_liquid_offY = 0;
    m_liquid_width = 0;
    m_liquid_height = 0;
    m_liquidLevel = INVALID_HEIGHT_VALUE;
    m_liquidFlags = nullptr;
    m_liquidEntry = nullptr;
    m_liquid_map = nullptr;
}

GridMap::~GridMap()
{
    unloadData();
}

bool GridMap::loadData(char* filename)
{
    // Unload old data if exist
    unloadData();

    GridMapFileHeader header;
    // Not return error if file not found
    FILE* in = fopen(filename, "rb");
    if (!in)
        return true;

    fread(&header, sizeof(header), 1, in);
    if (header.mapMagic == *((uint32 const*)(MAP_MAGIC)) &&
        header.versionMagic == *((uint32 const*)(MAP_VERSION_MAGIC)))
    {
        // loadup area data
        if (header.areaMapOffset &&
            !loadAreaData(in, header.areaMapOffset, header.areaMapSize))
        {
            logging.error("Error loading map area data\n");
            fclose(in);
            return false;
        }

        // loadup height data
        if (header.heightMapOffset &&
            !loadHeightData(in, header.heightMapOffset, header.heightMapSize))
        {
            logging.error("Error loading map height data\n");
            fclose(in);
            return false;
        }

        // loadup liquid data
        if (header.liquidMapOffset &&
            !loadGridMapLiquidData(
                in, header.liquidMapOffset, header.liquidMapSize))
        {
            logging.error("Error loading map liquids data\n");
            fclose(in);
            return false;
        }

        fclose(in);
        return true;
    }

    logging.error(
        "Map file '%s' is non-compatible version (outdated?). Please, create "
        "new using ad.exe program.",
        filename);
    fclose(in);
    return false;
}

void GridMap::unloadData()
{
    delete[] m_area_map;
    delete[] m_V9;
    delete[] m_V8;
    delete[] m_liquidEntry;
    delete[] m_liquidFlags;
    delete[] m_liquid_map;

    m_area_map = nullptr;
    m_V9 = nullptr;
    m_V8 = nullptr;
    m_liquidEntry = nullptr;
    m_liquidFlags = nullptr;
    m_liquid_map = nullptr;
    m_gridGetHeight = &GridMap::getHeightFromFlat;
}

bool GridMap::loadAreaData(FILE* in, uint32 offset, uint32 /*size*/)
{
    GridMapAreaHeader header;
    fseek(in, offset, SEEK_SET);
    fread(&header, sizeof(header), 1, in);
    if (header.fourcc != *((uint32 const*)(MAP_AREA_MAGIC)))
        return false;

    m_gridArea = header.gridArea;
    if (!(header.flags & MAP_AREA_NO_AREA))
    {
        m_area_map = new uint16[16 * 16];
        fread(m_area_map, sizeof(uint16), 16 * 16, in);
    }

    return true;
}

bool GridMap::loadHeightData(FILE* in, uint32 offset, uint32 /*size*/)
{
    GridMapHeightHeader header;
    fseek(in, offset, SEEK_SET);
    fread(&header, sizeof(header), 1, in);
    if (header.fourcc != *((uint32 const*)(MAP_HEIGHT_MAGIC)))
        return false;

    m_gridHeight = header.gridHeight;
    if (!(header.flags & MAP_HEIGHT_NO_HEIGHT))
    {
        if ((header.flags & MAP_HEIGHT_AS_INT16))
        {
            m_uint16_V9 = new uint16[129 * 129];
            m_uint16_V8 = new uint16[128 * 128];
            fread(m_uint16_V9, sizeof(uint16), 129 * 129, in);
            fread(m_uint16_V8, sizeof(uint16), 128 * 128, in);
            m_gridIntHeightMultiplier =
                (header.gridMaxHeight - header.gridHeight) / 65535;
            m_gridGetHeight = &GridMap::getHeightFromUint16;
        }
        else if ((header.flags & MAP_HEIGHT_AS_INT8))
        {
            m_uint8_V9 = new uint8[129 * 129];
            m_uint8_V8 = new uint8[128 * 128];
            fread(m_uint8_V9, sizeof(uint8), 129 * 129, in);
            fread(m_uint8_V8, sizeof(uint8), 128 * 128, in);
            m_gridIntHeightMultiplier =
                (header.gridMaxHeight - header.gridHeight) / 255;
            m_gridGetHeight = &GridMap::getHeightFromUint8;
        }
        else
        {
            m_V9 = new float[129 * 129];
            m_V8 = new float[128 * 128];
            fread(m_V9, sizeof(float), 129 * 129, in);
            fread(m_V8, sizeof(float), 128 * 128, in);
            m_gridGetHeight = &GridMap::getHeightFromFloat;
        }
    }
    else
        m_gridGetHeight = &GridMap::getHeightFromFlat;

    return true;
}

bool GridMap::loadGridMapLiquidData(FILE* in, uint32 offset, uint32 /*size*/)
{
    GridMapLiquidHeader header;
    fseek(in, offset, SEEK_SET);
    fread(&header, sizeof(header), 1, in);
    if (header.fourcc != *((uint32 const*)(MAP_LIQUID_MAGIC)))
        return false;

    m_liquidType = header.liquidType;
    m_liquid_offX = header.offsetX;
    m_liquid_offY = header.offsetY;
    m_liquid_width = header.width;
    m_liquid_height = header.height;
    m_liquidLevel = header.liquidLevel;

    if (!(header.flags & MAP_LIQUID_NO_TYPE))
    {
        m_liquidEntry = new uint16[16 * 16];
        fread(m_liquidEntry, sizeof(uint16), 16 * 16, in);

        m_liquidFlags = new uint8[16 * 16];
        fread(m_liquidFlags, sizeof(uint8), 16 * 16, in);
    }

    if (!(header.flags & MAP_LIQUID_NO_HEIGHT))
    {
        m_liquid_map = new float[m_liquid_width * m_liquid_height];
        fread(
            m_liquid_map, sizeof(float), m_liquid_width * m_liquid_height, in);
    }

    return true;
}

uint16 GridMap::getArea(float x, float y)
{
    if (!m_area_map)
        return m_gridArea;

    x = 16 * (32 - x / SIZE_OF_GRIDS);
    y = 16 * (32 - y / SIZE_OF_GRIDS);
    int lx = (int)x & 15;
    int ly = (int)y & 15;
    return m_area_map[lx * 16 + ly];
}

float GridMap::getHeightFromFlat(float /*x*/, float /*y*/) const
{
    return m_gridHeight;
}

float GridMap::getHeightFromFloat(float x, float y) const
{
    if (!m_V8 || !m_V9)
        return m_gridHeight;

    x = MAP_RESOLUTION * (32 - x / SIZE_OF_GRIDS);
    y = MAP_RESOLUTION * (32 - y / SIZE_OF_GRIDS);

    int x_int = (int)x;
    int y_int = (int)y;
    x -= x_int;
    y -= y_int;
    x_int &= (MAP_RESOLUTION - 1);
    y_int &= (MAP_RESOLUTION - 1);

    // Height stored as: h5 - its v8 grid, h1-h4 - its v9 grid
    // +--------------> X
    // | h1-------h2     Coordinates is:
    // | | \  1  / |     h1 0,0
    // | |  \   /  |     h2 0,1
    // | | 2  h5 3 |     h3 1,0
    // | |  /   \  |     h4 1,1
    // | | /  4  \ |     h5 1/2,1/2
    // | h3-------h4
    // V Y
    // For find height need
    // 1 - detect triangle
    // 2 - solve linear equation from triangle points
    // Calculate coefficients for solve h = a*x + b*y + c

    float a, b, c;
    // Select triangle:
    if (x + y < 1)
    {
        if (x > y)
        {
            // 1 triangle (h1, h2, h5 points)
            float h1 = m_V9[(x_int)*129 + y_int];
            float h2 = m_V9[(x_int + 1) * 129 + y_int];
            float h5 = 2 * m_V8[x_int * 128 + y_int];
            a = h2 - h1;
            b = h5 - h1 - h2;
            c = h1;
        }
        else
        {
            // 2 triangle (h1, h3, h5 points)
            float h1 = m_V9[x_int * 129 + y_int];
            float h3 = m_V9[x_int * 129 + y_int + 1];
            float h5 = 2 * m_V8[x_int * 128 + y_int];
            a = h5 - h1 - h3;
            b = h3 - h1;
            c = h1;
        }
    }
    else
    {
        if (x > y)
        {
            // 3 triangle (h2, h4, h5 points)
            float h2 = m_V9[(x_int + 1) * 129 + y_int];
            float h4 = m_V9[(x_int + 1) * 129 + y_int + 1];
            float h5 = 2 * m_V8[x_int * 128 + y_int];
            a = h2 + h4 - h5;
            b = h4 - h2;
            c = h5 - h4;
        }
        else
        {
            // 4 triangle (h3, h4, h5 points)
            float h3 = m_V9[(x_int)*129 + y_int + 1];
            float h4 = m_V9[(x_int + 1) * 129 + y_int + 1];
            float h5 = 2 * m_V8[x_int * 128 + y_int];
            a = h4 - h3;
            b = h3 + h4 - h5;
            c = h5 - h4;
        }
    }
    // Calculate height
    return a * x + b * y + c;
}

float GridMap::getHeightFromUint8(float x, float y) const
{
    if (!m_uint8_V8 || !m_uint8_V9)
        return m_gridHeight;

    x = MAP_RESOLUTION * (32 - x / SIZE_OF_GRIDS);
    y = MAP_RESOLUTION * (32 - y / SIZE_OF_GRIDS);

    int x_int = (int)x;
    int y_int = (int)y;
    x -= x_int;
    y -= y_int;
    x_int &= (MAP_RESOLUTION - 1);
    y_int &= (MAP_RESOLUTION - 1);

    int32 a, b, c;
    uint8* V9_h1_ptr = &m_uint8_V9[x_int * 128 + x_int + y_int];
    if (x + y < 1)
    {
        if (x > y)
        {
            // 1 triangle (h1, h2, h5 points)
            int32 h1 = V9_h1_ptr[0];
            int32 h2 = V9_h1_ptr[129];
            int32 h5 = 2 * m_uint8_V8[x_int * 128 + y_int];
            a = h2 - h1;
            b = h5 - h1 - h2;
            c = h1;
        }
        else
        {
            // 2 triangle (h1, h3, h5 points)
            int32 h1 = V9_h1_ptr[0];
            int32 h3 = V9_h1_ptr[1];
            int32 h5 = 2 * m_uint8_V8[x_int * 128 + y_int];
            a = h5 - h1 - h3;
            b = h3 - h1;
            c = h1;
        }
    }
    else
    {
        if (x > y)
        {
            // 3 triangle (h2, h4, h5 points)
            int32 h2 = V9_h1_ptr[129];
            int32 h4 = V9_h1_ptr[130];
            int32 h5 = 2 * m_uint8_V8[x_int * 128 + y_int];
            a = h2 + h4 - h5;
            b = h4 - h2;
            c = h5 - h4;
        }
        else
        {
            // 4 triangle (h3, h4, h5 points)
            int32 h3 = V9_h1_ptr[1];
            int32 h4 = V9_h1_ptr[130];
            int32 h5 = 2 * m_uint8_V8[x_int * 128 + y_int];
            a = h4 - h3;
            b = h3 + h4 - h5;
            c = h5 - h4;
        }
    }

    // Calculate height
    return (float)((a * x) + (b * y) + c) * m_gridIntHeightMultiplier +
           m_gridHeight;
}

float GridMap::getHeightFromUint16(float x, float y) const
{
    if (!m_uint16_V8 || !m_uint16_V9)
        return m_gridHeight;

    x = MAP_RESOLUTION * (32 - x / SIZE_OF_GRIDS);
    y = MAP_RESOLUTION * (32 - y / SIZE_OF_GRIDS);

    int x_int = (int)x;
    int y_int = (int)y;
    x -= x_int;
    y -= y_int;
    x_int &= (MAP_RESOLUTION - 1);
    y_int &= (MAP_RESOLUTION - 1);

    int32 a, b, c;
    uint16* V9_h1_ptr = &m_uint16_V9[x_int * 128 + x_int + y_int];
    if (x + y < 1)
    {
        if (x > y)
        {
            // 1 triangle (h1, h2, h5 points)
            int32 h1 = V9_h1_ptr[0];
            int32 h2 = V9_h1_ptr[129];
            int32 h5 = 2 * m_uint16_V8[x_int * 128 + y_int];
            a = h2 - h1;
            b = h5 - h1 - h2;
            c = h1;
        }
        else
        {
            // 2 triangle (h1, h3, h5 points)
            int32 h1 = V9_h1_ptr[0];
            int32 h3 = V9_h1_ptr[1];
            int32 h5 = 2 * m_uint16_V8[x_int * 128 + y_int];
            a = h5 - h1 - h3;
            b = h3 - h1;
            c = h1;
        }
    }
    else
    {
        if (x > y)
        {
            // 3 triangle (h2, h4, h5 points)
            int32 h2 = V9_h1_ptr[129];
            int32 h4 = V9_h1_ptr[130];
            int32 h5 = 2 * m_uint16_V8[x_int * 128 + y_int];
            a = h2 + h4 - h5;
            b = h4 - h2;
            c = h5 - h4;
        }
        else
        {
            // 4 triangle (h3, h4, h5 points)
            int32 h3 = V9_h1_ptr[1];
            int32 h4 = V9_h1_ptr[130];
            int32 h5 = 2 * m_uint16_V8[x_int * 128 + y_int];
            a = h4 - h3;
            b = h3 + h4 - h5;
            c = h5 - h4;
        }
    }

    // Calculate height
    return (float)((a * x) + (b * y) + c) * m_gridIntHeightMultiplier +
           m_gridHeight;
}

float GridMap::getLiquidLevel(float x, float y)
{
    if (!m_liquid_map)
        return m_liquidLevel;

    x = MAP_RESOLUTION * (32 - x / SIZE_OF_GRIDS);
    y = MAP_RESOLUTION * (32 - y / SIZE_OF_GRIDS);

    int cx_int = ((int)x & (MAP_RESOLUTION - 1)) - m_liquid_offY;
    int cy_int = ((int)y & (MAP_RESOLUTION - 1)) - m_liquid_offX;

    if (cx_int < 0 || cx_int >= m_liquid_height)
        return INVALID_HEIGHT_VALUE;

    if (cy_int < 0 || cy_int >= m_liquid_width)
        return INVALID_HEIGHT_VALUE;

    return m_liquid_map[cx_int * m_liquid_width + cy_int];
}

uint8 GridMap::getTerrainType(float x, float y)
{
    if (!m_liquidFlags)
        return (uint8)m_liquidType;

    x = 16 * (32 - x / SIZE_OF_GRIDS);
    y = 16 * (32 - y / SIZE_OF_GRIDS);
    int lx = (int)x & 15;
    int ly = (int)y & 15;
    return m_liquidFlags[lx * 16 + ly];
}

// Get water state on map
GridMapLiquidStatus GridMap::getLiquidStatus(
    float x, float y, float z, uint8 ReqLiquidType, GridMapLiquidData* data)
{
    // Check water type (if no water return)
    if (!m_liquidFlags && !m_liquidType)
        return LIQUID_MAP_NO_WATER;

    // Get cell
    float cx = MAP_RESOLUTION * (32 - x / SIZE_OF_GRIDS);
    float cy = MAP_RESOLUTION * (32 - y / SIZE_OF_GRIDS);

    int x_int = (int)cx & (MAP_RESOLUTION - 1);
    int y_int = (int)cy & (MAP_RESOLUTION - 1);

    // Check water type in cell
    int idx = (x_int >> 3) * 16 + (y_int >> 3);
    uint8 type = m_liquidFlags ? m_liquidFlags[idx] : 1 << m_liquidType;
    uint32 entry = 0;
    if (m_liquidEntry)
    {
        if (LiquidTypeEntry const* liquidEntry =
                sLiquidTypeStore.LookupEntry(m_liquidEntry[idx]))
        {
            entry = liquidEntry->Id;
            uint32 liqTypeIdx = liquidEntry->Type;
            if ((entry < 21) && (type & MAP_LIQUID_TYPE_WATER))
            {
                if (AreaTableEntry const* area =
                        sAreaStore.LookupEntry(getArea(x, y)))
                {
                    uint32 overrideLiquid = area->LiquidTypeOverride;
                    if (!overrideLiquid && area->zone)
                    {
                        area = GetAreaEntryByAreaID(area->zone);
                        if (area)
                            overrideLiquid = area->LiquidTypeOverride;
                    }

                    if (LiquidTypeEntry const* liq =
                            sLiquidTypeStore.LookupEntry(overrideLiquid))
                    {
                        entry = overrideLiquid;
                        liqTypeIdx = liq->Type;
                    }
                }
            }

            type |= (1 << liqTypeIdx) | (type & MAP_LIQUID_TYPE_DARK_WATER);
        }
    }

    // Check req liquid type mask
    if (ReqLiquidType && !(ReqLiquidType & type))
        return LIQUID_MAP_NO_WATER;

    // Check water level:
    // Check water height map
    int lx_int = x_int - m_liquid_offY;
    if (lx_int < 0 || lx_int >= m_liquid_height)
        return LIQUID_MAP_NO_WATER;

    int ly_int = y_int - m_liquid_offX;
    if (ly_int < 0 || ly_int >= m_liquid_width)
        return LIQUID_MAP_NO_WATER;

    // Get water level
    float liquid_level = m_liquid_map ?
                             m_liquid_map[lx_int * m_liquid_width + ly_int] :
                             m_liquidLevel;

    // Get ground level (sub 0.2 for fix some errors)
    float ground_level = getHeight(x, y);

    // Check water level and ground level
    if (liquid_level < ground_level || z < ground_level - 2)
        return LIQUID_MAP_NO_WATER;

    // All ok in water -> store data
    if (data)
    {
        data->entry = entry;
        data->type_flags = type;
        data->level = liquid_level;
        data->depth_level = ground_level;
    }

    float delta = liquid_level - z;

    // Get position delta
    if (delta > 2.0f) // Under water
        return LIQUID_MAP_UNDER_WATER;

    if (delta > 0.0f) // In water
        return LIQUID_MAP_IN_WATER;

    if (delta > -1.0f) // Walk on water
        return LIQUID_MAP_WATER_WALK;

    return LIQUID_MAP_ABOVE_WATER; // Above water
}

bool GridMap::ExistMap(uint32 mapid, int gx, int gy)
{
    int len = sWorld::Instance()->GetDataPath().length() +
              strlen("maps/%03u%02u%02u.map") + 1;
    auto tmp = new char[len];
    snprintf(tmp, len, (char*)(sWorld::Instance()->GetDataPath() +
                               "maps/%03u%02u%02u.map").c_str(),
        mapid, gx, gy);

    FILE* pf = fopen(tmp, "rb");

    if (!pf)
    {
        logging.error("Check existing of map file '%s': not exist!", tmp);
        delete[] tmp;
        return false;
    }

    GridMapFileHeader header;
    fread(&header, sizeof(header), 1, pf);
    if (header.mapMagic != *((uint32 const*)(MAP_MAGIC)) ||
        header.versionMagic != *((uint32 const*)(MAP_VERSION_MAGIC)))
    {
        logging.error(
            "Map file '%s' is non-compatible version (outdated?). Please, "
            "create new using ad.exe program.",
            tmp);
        delete[] tmp;
        fclose(pf); // close file before return
        return false;
    }

    delete[] tmp;
    fclose(pf);
    return true;
}

bool GridMap::ExistVMap(uint32 mapid, int gx, int gy)
{
    if (auto vmgr = VMAP::VMapFactory::createOrGetVMapManager())
    {
        // x and y are swapped !! => fixed now
        bool exists = vmgr->existsMap(
            (sWorld::Instance()->GetDataPath() + "vmaps").c_str(), mapid, gx,
            gy);
        if (!exists)
        {
            std::string name = vmgr->getDirFileName(mapid, gx, gy);
            logging.error(
                "VMap file '%s' is missing or point to wrong version vmap "
                "file, redo vmaps with latest vmap_assembler.exe program",
                (sWorld::Instance()->GetDataPath() + "vmaps/" + name).c_str());
            return false;
        }
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
TerrainInfo::TerrainInfo(uint32 mapid) : m_mapId(mapid)
{
    for (int k = 0; k < MAP_DATA_FMT_CELL_COUNT; ++k)
        for (int i = 0; i < MAP_DATA_FMT_CELL_COUNT; ++i)
            m_GridMaps[i][k] = nullptr;
}

TerrainInfo::~TerrainInfo()
{
    for (int k = 0; k < MAP_DATA_FMT_CELL_COUNT; ++k)
        for (auto& elem : m_GridMaps)
            delete elem[k];

    VMAP::VMapFactory::createOrGetVMapManager()->unloadMap(m_mapId);
    MMAP::MMapFactory::createOrGetMMapManager()->unloadMap(m_mapId);
}

GridMap* TerrainInfo::Load(const uint32 x, const uint32 y)
{
    assert(x < MAP_DATA_FMT_CELL_COUNT);
    assert(y < MAP_DATA_FMT_CELL_COUNT);

    // quick check if GridMap already loaded
    GridMap* pMap = m_GridMaps[x][y];
    if (!pMap)
        pMap = LoadMapAndVMap(x, y);

    return pMap;
}

float TerrainInfo::GetHeightStatic(float x, float y, float z,
    bool useVmaps /*=true*/,
    float maxSearchDist /*=DEFAULT_HEIGHT_SEARCH*/) const
{
    float height = INVALID_HEIGHT_VALUE;

    if (GridMap* gmap = const_cast<TerrainInfo*>(this)->GetGrid(x, y))
        height = gmap->getHeight(x, y);

    if (useVmaps)
    {
        auto vmgr = VMAP::VMapFactory::createOrGetVMapManager();
        float vmap_height =
            vmgr->getM2Height(GetMapId(), x, y, z + 2.0f, maxSearchDist);
        if (vmap_height < VMAP_INVALID_HEIGHT)
            vmap_height =
                vmgr->getWmoHeight(GetMapId(), x, y, z + 2.0f, maxSearchDist);

        if (vmap_height > VMAP_INVALID_HEIGHT)
        {
            if (!(height > vmap_height && z - 2.0f > vmap_height))
                height = vmap_height;
        }
    }

    return height;
}

inline bool IsOutdoorWMO(uint32 mogpFlags, uint32 mapId)
{
    // in flyable areas mounting up is also allowed if 0x0008 flag is set
    if (mapId == 530)
        return mogpFlags & 0x8008;

    return mogpFlags & 0x8000;
}

bool TerrainInfo::IsOutdoors(float x, float y, float z) const
{
    uint32 mogpFlags;
    int32 adtId, rootId, groupId;

    // no wmo found? -> outside by default
    if (!GetAreaInfo(x, y, z, mogpFlags, adtId, rootId, groupId))
        return true;

    return IsOutdoorWMO(mogpFlags, GetMapId());
}

bool TerrainInfo::GetAreaInfo(float x, float y, float z, uint32& flags,
    int32& adtId, int32& rootId, int32& groupId) const
{
    float vmap_z = z;
    auto vmgr = VMAP::VMapFactory::createOrGetVMapManager();
    if (vmgr->getAreaInfo(
            GetMapId(), x, y, vmap_z, flags, adtId, rootId, groupId))
    {
        // check if there's terrain between player height and object height
        if (GridMap* gmap = const_cast<TerrainInfo*>(this)->GetGrid(x, y))
        {
            float _mapheight = gmap->getHeight(x, y);
            // z + 2.0f condition taken from GetHeightStatic(), not sure if it's
            // such a great choice...
            if (z + 2.0f > _mapheight && _mapheight > vmap_z)
                return false;
        }
        return true;
    }
    return false;
}

uint16 TerrainInfo::GetAreaFlag(
    float x, float y, float z, bool* isOutdoors) const
{
    uint32 mogpFlags;
    int32 adtId, rootId, groupId;
    WMOAreaTableEntry const* wmoEntry = nullptr;
    AreaTableEntry const* atEntry = nullptr;
    bool haveAreaInfo = false;

    if (GetAreaInfo(x, y, z, mogpFlags, adtId, rootId, groupId))
    {
        haveAreaInfo = true;
        wmoEntry = GetWMOAreaTableEntryByTripple(rootId, adtId, groupId);
        if (wmoEntry)
            atEntry = GetAreaEntryByAreaID(wmoEntry->areaId);
    }

    uint16 areaflag;
    if (atEntry)
        areaflag = atEntry->exploreFlag;
    else
    {
        if (GridMap* gmap = const_cast<TerrainInfo*>(this)->GetGrid(x, y))
            areaflag = gmap->getArea(x, y);
        // this used while not all *.map files generated (instances)
        else
            areaflag = GetAreaFlagByMapId(GetMapId());
    }

    if (isOutdoors)
    {
        if (haveAreaInfo)
            *isOutdoors = IsOutdoorWMO(mogpFlags, GetMapId());
        else
            *isOutdoors = true;
    }
    return areaflag;
}

uint8 TerrainInfo::GetTerrainType(float x, float y) const
{
    if (GridMap* gmap = const_cast<TerrainInfo*>(this)->GetGrid(x, y))
        return gmap->getTerrainType(x, y);
    else
        return 0;
}

uint32 TerrainInfo::GetAreaId(float x, float y, float z) const
{
    return TerrainManager::GetAreaIdByAreaFlag(GetAreaFlag(x, y, z), m_mapId);
}

uint32 TerrainInfo::GetZoneId(float x, float y, float z) const
{
    return TerrainManager::GetZoneIdByAreaFlag(GetAreaFlag(x, y, z), m_mapId);
}

void TerrainInfo::GetZoneAndAreaId(
    uint32& zoneid, uint32& areaid, float x, float y, float z) const
{
    TerrainManager::GetZoneAndAreaIdByAreaFlag(
        zoneid, areaid, GetAreaFlag(x, y, z), m_mapId);
}

GridMapLiquidStatus TerrainInfo::getLiquidStatus(float x, float y, float z,
    uint8 ReqLiquidType, bool check_vmap, GridMapLiquidData* data) const
{
    GridMapLiquidStatus result = LIQUID_MAP_NO_WATER;
    auto vmgr = VMAP::VMapFactory::createOrGetVMapManager();
    float liquid_level = INVALID_HEIGHT_VALUE;
    float ground_level = INVALID_HEIGHT_VALUE;
    uint32 liquid_type = 0;

    if (check_vmap &&
        vmgr->GetLiquidLevel(GetMapId(), x, y, z, ReqLiquidType, liquid_level,
            ground_level, liquid_type))
    {
        // Check water level and ground level
        if (liquid_level > ground_level && z > ground_level - 2)
        {
            // All ok in water -> store data
            if (data)
            {
                uint32 liquidFlagType = 0;
                if (LiquidTypeEntry const* liq =
                        sLiquidTypeStore.LookupEntry(liquid_type))
                    liquidFlagType = 1 << liq->Type;

                data->level = liquid_level;
                data->depth_level = ground_level;

                data->entry = liquid_type;
                data->type_flags = liquidFlagType;
            }

            float delta = liquid_level - z;

            // Get position delta
            if (delta > 2.0) // Under water
                return LIQUID_MAP_UNDER_WATER;
            if (delta > 0.0f) // In water
                return LIQUID_MAP_IN_WATER;
            if (delta > -1.0f) // Walk on water
                return LIQUID_MAP_WATER_WALK;
            result = LIQUID_MAP_ABOVE_WATER;
        }
    }
    else if (GridMap* gmap = const_cast<TerrainInfo*>(this)->GetGrid(x, y))
    {
        GridMapLiquidData map_data;
        GridMapLiquidStatus map_result =
            gmap->getLiquidStatus(x, y, z, ReqLiquidType, &map_data);
        // Not override LIQUID_MAP_ABOVE_WATER with LIQUID_MAP_NO_WATER:
        if (map_result != LIQUID_MAP_NO_WATER)
        {
            if (data)
                *data = map_data;
            return map_result;
        }
    }
    return result;
}

bool TerrainInfo::IsInWater(
    float x, float y, float pZ, GridMapLiquidData* data) const
{
    // Check surface in x, y point for liquid
    if (const_cast<TerrainInfo*>(this)->GetGrid(x, y))
    {
        GridMapLiquidData liquid_status;
        GridMapLiquidData* liquid_ptr = data ? data : &liquid_status;
        if (getLiquidStatus(x, y, pZ, MAP_ALL_LIQUIDS, true, liquid_ptr))
        {
            // if (liquid_prt->level - liquid_prt->depth_level > 2) //???
            return true;
        }
    }
    return false;
}

bool TerrainInfo::IsUnderWater(float x, float y, float z) const
{
    if (const_cast<TerrainInfo*>(this)->GetGrid(x, y))
    {
        if (getLiquidStatus(
                x, y, z, MAP_LIQUID_TYPE_WATER | MAP_LIQUID_TYPE_OCEAN) &
            LIQUID_MAP_UNDER_WATER)
            return true;
    }
    return false;
}

/**
 * Function find higher form water or ground height for current floor
 *
 * @param x, y, z    Coordinates original point at floor level
 *
 * @param pGround    optional arg for retrun calculated by function work ground
 *height, it let avoid in caller code recalculate height for point if it need
 *
 * @param swim       z coordinate can be calculated for select above/at or under
 *z coordinate (for fly or swim/walking by bottom)
 *                   in last cases for in water returned under water height for
 *avoid client set swimming unit as saty at water.
 *
 * @return           calculated z coordinate
 */
float TerrainInfo::GetWaterOrGroundLevel(float x, float y, float z,
    float* pGround /*= NULL*/, bool swim /*= false*/) const
{
    if (const_cast<TerrainInfo*>(this)->GetGrid(x, y))
    {
        // we need ground level (including grid height version) for proper
        // return water level in point
        float ground_z = GetHeightStatic(x, y, z, true, DEFAULT_WATER_SEARCH);
        if (pGround)
            *pGround = ground_z;

        GridMapLiquidData liquid_status;

        GridMapLiquidStatus res = getLiquidStatus(
            x, y, ground_z, MAP_ALL_LIQUIDS, true, &liquid_status);
        return res ? (swim ? liquid_status.level - 2.0f : liquid_status.level) :
                     ground_z;
    }

    return VMAP_INVALID_HEIGHT_VALUE;
}

GridMap* TerrainInfo::GetGrid(float x, float y)
{
    auto pair = maps::world_coords_to_data_cell(x, y);

    // quick check if GridMap already loaded
    GridMap* pMap = m_GridMaps[pair.first][pair.second];
    if (!pMap)
        pMap = LoadMapAndVMap(pair.second, pair.second);

    return pMap;
}

GridMap* TerrainInfo::LoadMapAndVMap(uint32 x, uint32 y)
{
    // double checked lock pattern
    if (!m_GridMaps[x][y])
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!m_GridMaps[x][y])
        {
            auto map = new GridMap();

            // map file name
            char* tmp = nullptr;
            int len = sWorld::Instance()->GetDataPath().length() +
                      strlen("maps/%03u%02u%02u.map") + 1;
            tmp = new char[len];
            snprintf(tmp, len, (char*)(sWorld::Instance()->GetDataPath() +
                                       "maps/%03u%02u%02u.map").c_str(),
                m_mapId, x, y);
            LOG_DEBUG(logging, "Loading map %s", tmp);

            if (!map->loadData(tmp))
            {
                logging.error("Error load map file: \n %s\n", tmp);
                // ASSERT(false);
            }

            delete[] tmp;
            m_GridMaps[x][y] = map;

            // load VMAPs for current map/grid...
            int vmapLoadResult =
                VMAP::VMapFactory::createOrGetVMapManager()->loadMap(
                    (sWorld::Instance()->GetDataPath() + "vmaps").c_str(),
                    m_mapId, x, y);
            switch (vmapLoadResult)
            {
            case VMAP::VMAP_LOAD_RESULT_OK:
                LOG_DEBUG(logging,
                    "VMAP loaded id:%d, x:%d, y:%d (vmap rep.: x:%d, y:%d)",
                    m_mapId, x, y, x, y);
                break;
            case VMAP::VMAP_LOAD_RESULT_ERROR:
                LOG_DEBUG(logging,
                    "Could not load VMAP id:%d, x:%d, y:%d (vmap "
                    "rep.: x:%d, y:%d)",
                    m_mapId, x, y, x, y);
                break;
            case VMAP::VMAP_LOAD_RESULT_IGNORED:
                LOG_DEBUG(logging,
                    "Ignored VMAP id:%d, x:%d, y:%d (vmap rep.: x:%d, y:%d)",
                    m_mapId, x, y, x, y);
                break;
            }

            // load navmesh
            MMAP::MMapFactory::createOrGetMMapManager()->loadMap(m_mapId, x, y);
        }
    }

    return m_GridMaps[x][y];
}

float TerrainInfo::GetWaterLevel(
    float x, float y, float z, float* pGround /*= NULL*/) const
{
    if (const_cast<TerrainInfo*>(this)->GetGrid(x, y))
    {
        // we need ground level (including grid height version) for proper
        // return water level in point
        float ground_z = GetHeightStatic(x, y, z, true, DEFAULT_WATER_SEARCH);
        if (pGround)
            *pGround = ground_z;

        GridMapLiquidData liquid_status;

        GridMapLiquidStatus res = getLiquidStatus(
            x, y, ground_z, MAP_ALL_LIQUIDS, true, &liquid_status);
        if (!res)
            return VMAP_INVALID_HEIGHT_VALUE;

        return liquid_status.level;
    }

    return VMAP_INVALID_HEIGHT_VALUE;
}

//////////////////////////////////////////////////////////////////////////

TerrainManager::TerrainManager()
{
}

TerrainManager::~TerrainManager()
{
    for (auto& elem : i_TerrainMap)
        delete elem.second;
}

TerrainInfo* TerrainManager::LoadTerrain(const uint32 mapId)
{
    TerrainInfo* ptr = nullptr;
    TerrainDataMap::const_iterator iter = i_TerrainMap.find(mapId);
    if (iter == i_TerrainMap.end())
    {
        ptr = new TerrainInfo(mapId);
        i_TerrainMap[mapId] = ptr;
    }
    else
        ptr = (*iter).second;

    return ptr;
}

void TerrainManager::LoadAll()
{
    logging.info(
        "Loading all Map, VMap and MMap data into memory, this is going to "
        "take a while...");
    printf("Maps loaded: ");
    fflush(stdout);

    // Looping through all map-ids is non-trivial at the moment as all we know
    // are
    // the amount of map entries
    uint32 maps_count = sMapStore.GetNumRows(), found_maps = 0;

    for (uint32 map_id = 0; found_maps < maps_count && map_id < 1000; ++map_id)
    {
        const MapEntry* map_entry = sMapStore.LookupEntry(map_id);
        if (!map_entry)
            continue;

        auto terrain = LoadTerrain(map_id);
        if (!terrain)
            continue;

        for (int y = 0; y < MAP_DATA_FMT_CELL_COUNT; ++y)
            for (int x = 0; x < MAP_DATA_FMT_CELL_COUNT; ++x)
            {
                // Check if map file for grid exists
                std::stringstream ss;
                ss << sWorld::Instance()->GetDataPath() << "maps/"
                   << std::setfill('0') << std::setw(3) << map_id
                   << std::setw(2) << y << x << ".map";
                if (!boost::filesystem::exists(ss.str()))
                    continue;
                // Load it
                terrain->Load(y, x);
                MMAP::MMapFactory::createOrGetMMapManager()->loadMap(
                    map_id, y, x);
            }

        printf("%u ", map_id);
        fflush(stdout);
        ++found_maps;
    }
    printf("\n");
}

void TerrainManager::UnloadAll()
{
    for (auto& elem : i_TerrainMap)
        delete elem.second;

    i_TerrainMap.clear();
}

uint32 TerrainManager::GetAreaIdByAreaFlag(uint16 areaflag, uint32 map_id)
{
    AreaTableEntry const* entry =
        GetAreaEntryByAreaFlagAndMap(areaflag, map_id);

    if (entry)
        return entry->ID;
    else
        return 0;
}

uint32 TerrainManager::GetZoneIdByAreaFlag(uint16 areaflag, uint32 map_id)
{
    AreaTableEntry const* entry =
        GetAreaEntryByAreaFlagAndMap(areaflag, map_id);

    if (entry)
        return (entry->zone != 0) ? entry->zone : entry->ID;
    else
        return 0;
}

void TerrainManager::GetZoneAndAreaIdByAreaFlag(
    uint32& zoneid, uint32& areaid, uint16 areaflag, uint32 map_id)
{
    AreaTableEntry const* entry =
        GetAreaEntryByAreaFlagAndMap(areaflag, map_id);

    areaid = entry ? entry->ID : 0;
    zoneid = entry ? ((entry->zone != 0) ? entry->zone : entry->ID) : 0;
}
