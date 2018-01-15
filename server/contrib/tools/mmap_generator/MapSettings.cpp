#include "MapSettings.h"
#include "DetourNavMesh.h"
#include "MMapCommon.h"
#include <cstring>
#include <map>

static std::map<unsigned int, rcConfig> settings;

static rcConfig get_default();
static void fill_settings();

rcConfig get_config_for(unsigned int map)
{
    if (settings.empty())
        fill_settings();

    // Map specific options
    auto itr = settings.find(map);
    if (itr != settings.end())
        return itr->second;

    // Default options
    return get_default();
}

bool should_erode_walkable_areas(unsigned int map)
{
    switch (map)
    {
    // Blade's Edge Arena
    case 562:
        return false;
    }
    return true;
}

static rcConfig get_default()
{
    rcConfig config;
    memset(&config, 0, sizeof(config));

    config.maxVertsPerPoly = DT_VERTS_PER_POLYGON;
    config.cs = BASE_UNIT_DIM;
    config.ch = BASE_UNIT_DIM;
    config.walkableSlopeAngle = 70.0f;
    config.tileSize = VERTEX_PER_TILE;
    config.walkableRadius = 2;
    config.borderSize = config.walkableRadius + 3;
    config.maxEdgeLen = VERTEX_PER_TILE + 1; // anything bigger than tileSize
    config.walkableHeight = 6;
    config.walkableClimb = 8; // < walkableHeight
    config.minRegionArea = rcSqr(60);
    config.mergeRegionArea = rcSqr(50);
    config.maxSimplificationError =
        1.8f; // eliminates most jagged edges (tiny polygons)
    config.detailSampleDist = config.cs * 64;
    config.detailSampleMaxError = config.ch * 2;

    return config;
}

static void fill_settings()
{
    auto config = get_default();
    config.walkableHeight = 10;

    // Botanica
    settings[553] = config;
    // Old Hillsbrad
    settings[560] = config;
    // Ruins of Lordaeron
    settings[572] = config;
}
