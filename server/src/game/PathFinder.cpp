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

#include "PathFinder.h"
#include "Creature.h"
#include "DetourCommon.h"
#include "GameObjectModel.h"
#include "GridMap.h"
#include "logging.h"
#include "Map.h"
#include "MoveMap.h"
#include "Player.h"
#include "Transport.h"
#include "World.h"
#include "concurrent_pathgen.h"

////////////////// PathFinder //////////////////
PathFinder::PathFinder()
  : _polyLength(0), _type(PATHFIND_BLANK), _useStraightPath(false),
    _forceDestination(false), _pointPathLimit(MAX_POINT_PATH_LENGTH),
    _straightLine(false), _endPosition(G3D::Vector3::zero()),
    _sourceUnit(nullptr), _navMesh(nullptr), _navMeshQuery(nullptr),
    _endOfPathIdx(-1), _invalidPoly(false), _farAwayPoly(false),
    _canSwimOrFly(false)
{
    memset(_pathPolyRefs, 0, sizeof(_pathPolyRefs));
}

PathFinder::PathFinder(Unit* owner) : PathFinder()
{
    _sourceUnit = owner;

    _canSwimOrFly = _sourceUnit->GetTypeId() == TYPEID_UNIT &&
                    (static_cast<Creature*>(_sourceUnit)->CanSwim() ||
                        static_cast<Creature*>(_sourceUnit)->CanFly());

    LOG_DEBUG(logging, "++ PathFinder::PathFinder for %u \n",
        _sourceUnit->GetGUIDLow());

    uint32 mapId = _sourceUnit->GetMapId();
    if (MMAP::MMapFactory::IsPathfindingEnabled(mapId))
    {
        MMAP::MMapManager* mmap = MMAP::MMapFactory::createOrGetMMapManager();
        auto t = _sourceUnit->GetTransport();
        _navMesh =
            t ? mmap->GetNavMesh(t->m_model->name) : mmap->GetNavMesh(mapId);
        _navMeshQuery = t ?
                            mmap->GetNavMeshQuery(t->m_model->name,
                                _sourceUnit->GetInstanceId(), false) :
                            mmap->GetNavMeshQuery(
                                mapId, _sourceUnit->GetInstanceId(), false);
    }

    CreateFilter();
}

void PathFinder::delayedInit(Unit* source, uint32 map_id)
{
    // cannot build concurrent paths for transports
    assert(source->GetTransport() == nullptr);

    if (MMAP::MMapFactory::IsPathfindingEnabled(map_id))
    {
        MMAP::MMapManager* mmap = MMAP::MMapFactory::createOrGetMMapManager();
        auto t = source->GetTransport();
        _navMesh =
            t ? mmap->GetNavMesh(t->m_model->name) : mmap->GetNavMesh(map_id);
        _navMeshQuery =
            t ?
                mmap->GetNavMeshQuery(
                    t->m_model->name, source->GetInstanceId(), true) :
                mmap->GetNavMeshQuery(map_id, source->GetInstanceId(), true);
    }

    _canSwimOrFly = source->GetTypeId() == TYPEID_UNIT &&
                    (static_cast<Creature*>(source)->CanSwim() ||
                        static_cast<Creature*>(source)->CanFly());

    _sourceUnit = source; // Used by CreateFilter
    CreateFilter();
    _sourceUnit = nullptr; // Unset again as it might go invalid
}

void PathFinder::finalize(Unit* source)
{
    _sourceUnit = source; // Used in invoked functions

    if (_invalidPoly)
    {
        BuildShortcut();
        bool airPath = source->GetTypeId() == TYPEID_UNIT &&
                       static_cast<const Creature*>(source)->CanFly();
        bool waterPath = source->GetTypeId() == TYPEID_UNIT &&
                         static_cast<const Creature*>(source)->CanSwim();
        if (_pathPoints.size() >= 2 && waterPath &&
            !_sourceUnit->GetTransport())
        {
            // Check both start and end points, if they're both in water, then
            // we can *safely* let the creature move
            if (source->GetLiquidStatus(_pathPoints[0].x, _pathPoints[0].y,
                    _pathPoints[0].z, MAP_ALL_LIQUIDS) == LIQUID_MAP_NO_WATER)
                waterPath = false;
            else if (source->GetLiquidStatus(
                         _pathPoints[_pathPoints.size() - 1].x,
                         _pathPoints[_pathPoints.size() - 1].y,
                         _pathPoints[_pathPoints.size() - 1].z,
                         MAP_ALL_LIQUIDS) == LIQUID_MAP_NO_WATER)
                waterPath = false;
        }

        _type = (airPath || waterPath) ?
                    PathType(PATHFIND_NORMAL | PATHFIND_NOT_USING_PATH) :
                    PATHFIND_NOPATH;
        return;
    }
    else if (_farAwayPoly && source->GetTypeId() == TYPEID_UNIT)
    {
        BuildShortcut();
        _type = PathType(PATHFIND_NORMAL | PATHFIND_NOT_USING_PATH);
    }

    NormalizePath();
}

PathFinder::~PathFinder()
{
#ifndef OPTIMIZED_BUILD
    if (_sourceUnit)
        LOG_DEBUG(logging, "++ PathFinder::~PathFinder() for %u \n",
            _sourceUnit->GetGUIDLow());
    else
        LOG_DEBUG(logging, "++ PathFinder::~PathFinder() without owner\n");
#endif
}

bool PathFinder::calculate(
    float destX, float destY, float destZ, bool forceDest, bool straightLine)
{
    assert(_sourceUnit);

    float x, y, z;

    // use transport coordinates if we're on a transport
    if (_sourceUnit->GetTransport() != nullptr)
        _sourceUnit->m_movementInfo.transport.pos.Get(x, y, z);
    else
        _sourceUnit->GetPosition(x, y, z);

    return calculate(x, y, z, destX, destY, destZ, forceDest, straightLine);
}

bool PathFinder::calculate(float srcX, float srcY, float srcZ, float destX,
    float destY, float destZ, bool forceDest, bool straightLine)
{
    if (!maps::verify_coords(destX, destY) || !maps::verify_coords(srcX, srcY))
        return false;

    G3D::Vector3 dest(destX, destY, destZ);
    SetEndPosition(dest);

    G3D::Vector3 start(srcX, srcY, srcZ);
    SetStartPosition(start);

    _forceDestination = forceDest;
    _straightLine = straightLine;

    LOG_DEBUG(logging, "++ PathFinder::CalculatePath()\n");

    // make sure navMesh works - we can run on map w/o mmap
    // check if the start and end point have a .mmtile loaded (can we pass via
    // not loaded tile on the way?)
    if (!_navMesh || !_navMeshQuery || !HaveTile(start) || !HaveTile(dest) ||
        (_sourceUnit &&
            _sourceUnit->hasUnitState(UNIT_STAT_IGNORE_PATHFINDING)))
    {
        BuildShortcut();
        _type = PathType(PATHFIND_NORMAL | PATHFIND_NOT_USING_PATH);
        return true;
    }

    BuildPolyPath(start, dest);

    if (_sourceUnit)
        finalize(_sourceUnit);

    return true;
}

dtPolyRef PathFinder::GetPathPolyByPosition(dtPolyRef const* polyPath,
    uint32 polyPathSize, float const* point, float* distance) const
{
    if (!polyPath || !polyPathSize)
        return INVALID_POLYREF;

    dtPolyRef nearestPoly = INVALID_POLYREF;
    float minDist2d = FLT_MAX;
    float minDist3d = 0.0f;

    for (uint32 i = 0; i < polyPathSize; ++i)
    {
        float closestPoint[VERTEX_SIZE];
        if (dtStatusFailed(_navMeshQuery->closestPointOnPoly(
                polyPath[i], point, closestPoint, nullptr)))
            continue;

        float d = dtVdist2DSqr(point, closestPoint);
        if (d < minDist2d)
        {
            minDist2d = d;
            nearestPoly = polyPath[i];
            minDist3d = dtVdistSqr(point, closestPoint);
        }

        if (minDist2d < 1.0f) // shortcut out - close enough for us
            break;
    }

    if (distance)
        *distance = dtSqrt(minDist3d);

    return (minDist2d < 3.0f) ? nearestPoly : INVALID_POLYREF;
}

dtPolyRef PathFinder::GetPolyByLocation(
    float const* point, float* distance) const
{
    // first we check the current path
    // if the current path doesn't contain the current poly,
    // we need to use the expensive navMesh.findNearestPoly
    dtPolyRef polyRef =
        GetPathPolyByPosition(_pathPolyRefs, _polyLength, point, distance);
    if (polyRef != INVALID_POLYREF)
        return polyRef;

    // we don't have it in our old path
    // try to get it by findNearestPoly()
    // first try with low search box
    float extents[VERTEX_SIZE] = {
        3.0f, 5.0f, 3.0f}; // bounds of poly search area
    float closestPoint[VERTEX_SIZE] = {0.0f, 0.0f, 0.0f};
    if (dtStatusSucceed(_navMeshQuery->findNearestPoly(
            point, extents, &_filter, &polyRef, closestPoint)) &&
        polyRef != INVALID_POLYREF)
    {
        *distance = dtVdist(closestPoint, point);
        return polyRef;
    }

    // still nothing ..
    // try with bigger search box
    // Note that the extent should not overlap more than 128 polygons in the
    // navmesh (see dtNavMeshQuery::findNearestPoly)
    extents[1] = 50.0f;

    if (dtStatusSucceed(_navMeshQuery->findNearestPoly(
            point, extents, &_filter, &polyRef, closestPoint)) &&
        polyRef != INVALID_POLYREF)
    {
        *distance = dtVdist(closestPoint, point);
        return polyRef;
    }

    return INVALID_POLYREF;
}

void PathFinder::BuildPolyPath(
    G3D::Vector3 const& startPos, G3D::Vector3 const& endPos)
{
    // *** getting start/end poly logic ***

    float distToStartPoly, distToEndPoly;
    float startPoint[VERTEX_SIZE] = {startPos.y, startPos.z, startPos.x};
    float endPoint[VERTEX_SIZE] = {endPos.y, endPos.z, endPos.x};

    dtPolyRef startPoly = GetPolyByLocation(startPoint, &distToStartPoly);
    dtPolyRef endPoly = GetPolyByLocation(endPoint, &distToEndPoly);

    // we have a hole in our mesh; if so make shortcut and mark as NOPATH (done
    // in finalize())
    if (startPoly == INVALID_POLYREF || endPoly == INVALID_POLYREF)
    {
        LOG_DEBUG(
            logging, "++ BuildPolyPath :: (startPoly == 0 || endPoly == 0)\n");

        // If we force destination, build a shortcut and pretend it's valid
        // instead
        if (_forceDestination)
        {
            BuildShortcut();
            _type = PathType(PATHFIND_NORMAL | PATHFIND_NOT_USING_PATH);
            return;
        }

        _invalidPoly = true;
        return;
    }

    // we may need a better number here
    /*bool farFromPoly = (distToStartPoly > 7.0f || distToEndPoly > 7.0f);
    if (farFromPoly)
    {
        LOG_DEBUG(logging, "++ BuildPolyPath ::
    farFromPoly distToStartPoly=%.3f distToEndPoly=%.3f\n", distToStartPoly,
    distToEndPoly);

        // if we can swim or fly, build a shortcut in finalize()
        if (_canSwimOrFly)
        {
            _farAwayPoly = true;
            return;
        }

        float closestPoint[VERTEX_SIZE];
        // we may want to use closestPointOnPolyBoundary instead
        if (dtStatusSucceed(_navMeshQuery->closestPointOnPoly(endPoly, endPoint,
    closestPoint, NULL)))
        {
            dtVcopy(endPoint, closestPoint);
            SetActualEndPosition(G3D::Vector3(endPoint[2], endPoint[0],
    endPoint[1]));
        }

        _type = PATHFIND_INCOMPLETE;
    }*/

    // *** poly path generating logic ***

    // start and end are on same polygon
    // just need to move in straight line
    if (startPoly == endPoly)
    {
        LOG_DEBUG(logging, "++ BuildPolyPath :: (startPoly == endPoly)\n");

        BuildShortcut();

        _pathPolyRefs[0] = startPoly;
        _polyLength = 1;

        _type = /*farFromPoly ? PATHFIND_INCOMPLETE :*/ PATHFIND_NORMAL;
        LOG_DEBUG(logging, "++ BuildPolyPath :: path type %d\n", _type);
        return;
    }

    // look for startPoly/endPoly in current path
    /// @todo we can merge it with getPathPolyByPosition() loop
    bool startPolyFound = false;
    bool endPolyFound = false;
    uint32 pathStartIndex = 0;
    uint32 pathEndIndex = 0;

    if (_polyLength)
    {
        for (; pathStartIndex < _polyLength; ++pathStartIndex)
        {
            // here to catch few bugs
            if (_pathPolyRefs[pathStartIndex] == INVALID_POLYREF)
            {
                logging.error(
                    "Invalid poly ref in BuildPolyPath. _polyLength: %u, "
                    "pathStartIndex: %u,"
                    " startPos: %s, endPos: %s",
                    _polyLength, pathStartIndex, startPos.toString().c_str(),
                    endPos.toString().c_str());

                break;
            }

            if (_pathPolyRefs[pathStartIndex] == startPoly)
            {
                startPolyFound = true;
                break;
            }
        }

        for (pathEndIndex = _polyLength - 1; pathEndIndex > pathStartIndex;
             --pathEndIndex)
            if (_pathPolyRefs[pathEndIndex] == endPoly)
            {
                endPolyFound = true;
                break;
            }
    }

    if (startPolyFound && endPolyFound)
    {
        LOG_DEBUG(
            logging, "++ BuildPolyPath :: (startPolyFound && endPolyFound)\n");

        // we moved along the path and the target did not move out of our old
        // poly-path
        // our path is a simple subpath case, we have all the data we need
        // just "cut" it out

        _polyLength = pathEndIndex - pathStartIndex + 1;
        memmove(_pathPolyRefs, _pathPolyRefs + pathStartIndex,
            _polyLength * sizeof(dtPolyRef));
    }
    else if (startPolyFound && !endPolyFound)
    {
        LOG_DEBUG(
            logging, "++ BuildPolyPath :: (startPolyFound && !endPolyFound)\n");

        // we are moving on the old path but target moved out
        // so we have atleast part of poly-path ready

        _polyLength -= pathStartIndex;

        // try to adjust the suffix of the path instead of recalculating entire
        // length
        // at given interval the target cannot get too far from its last
        // location
        // thus we have less poly to cover
        // sub-path of optimal path is optimal

        // take ~80% of the original length
        /// @todo play with the values here
        uint32 prefixPolyLength = uint32(_polyLength * 0.8f + 0.5f);
        memmove(_pathPolyRefs, _pathPolyRefs + pathStartIndex,
            prefixPolyLength * sizeof(dtPolyRef));

        dtPolyRef suffixStartPoly = _pathPolyRefs[prefixPolyLength - 1];

        // we need any point on our suffix start poly to generate poly-path, so
        // we need last poly in prefix data
        float suffixEndPoint[VERTEX_SIZE];
        if (dtStatusFailed(_navMeshQuery->closestPointOnPoly(
                suffixStartPoly, endPoint, suffixEndPoint, nullptr)))
        {
            // we can hit offmesh connection as last poly - closestPointOnPoly()
            // don't like that
            // try to recover by using prev polyref
            --prefixPolyLength;
            suffixStartPoly = _pathPolyRefs[prefixPolyLength - 1];
            if (dtStatusFailed(_navMeshQuery->closestPointOnPoly(
                    suffixStartPoly, endPoint, suffixEndPoint, nullptr)))
            {
                // suffixStartPoly is still invalid, error state
                BuildShortcut();
                _type = PATHFIND_NOPATH;
                return;
            }
        }

        // generate suffix
        uint32 suffixPolyLength = 0;

        dtStatus dtResult;
        if (_straightLine)
        {
            float hit = 0;
            float hitNormal[3];
            memset(hitNormal, 0, sizeof(hitNormal));

            dtResult = _navMeshQuery->raycast(suffixStartPoly, suffixEndPoint,
                endPoint, &_filter, &hit, hitNormal,
                _pathPolyRefs + prefixPolyLength - 1, (int*)&suffixPolyLength,
                MAX_PATH_LENGTH - prefixPolyLength);

            // raycast() sets hit to FLT_MAX if there is a ray between start and
            // end
            if (hit != FLT_MAX)
            {
                // the ray hit something, return no path instead of the
                // incomplete one
                _type = PATHFIND_NOPATH;
                return;
            }
        }
        else
        {
            dtResult = _navMeshQuery->findPath(suffixStartPoly, // start polygon
                endPoly,                                        // end polygon
                suffixEndPoint,                       // start position
                endPoint,                             // end position
                &_filter,                             // polygon search filter
                _pathPolyRefs + prefixPolyLength - 1, // [out] path
                (int*)&suffixPolyLength,
                MAX_PATH_LENGTH -
                    prefixPolyLength); // max number of polygons in output path
        }

        if (!suffixPolyLength || dtStatusFailed(dtResult))
        {
            // this is probably an error state, but we'll leave it
            // and hopefully recover on the next Update
            // we still need to copy our preffix
            logging.error("Path Build failed: 0 length path");
        }

        LOG_DEBUG(logging,
            "++  m_polyLength=%u prefixPolyLength=%u suffixPolyLength=%u \n",
            _polyLength, prefixPolyLength, suffixPolyLength);

        // new path = prefix + suffix - overlap
        _polyLength = prefixPolyLength + suffixPolyLength - 1;
    }
    else
    {
        LOG_DEBUG(logging,
            "++ BuildPolyPath :: (!startPolyFound && !endPolyFound)\n");

        // either we have no path at all -> first run
        // or something went really wrong -> we aren't moving along the path to
        // the target
        // just generate new path

        // free and invalidate old path data
        Clear();

        dtStatus dtResult;
        if (_straightLine)
        {
            float hit = 0;
            float hitNormal[3];
            memset(hitNormal, 0, sizeof(hitNormal));

            dtResult = _navMeshQuery->raycast(startPoly, startPoint, endPoint,
                &_filter, &hit, hitNormal, _pathPolyRefs, (int*)&_polyLength,
                MAX_PATH_LENGTH);

            // raycast() sets hit to FLT_MAX if there is a ray between start and
            // end
            if (hit != FLT_MAX)
            {
                // the ray hit something, return no path instead of the
                // incomplete one
                _type = PATHFIND_NOPATH;
                return;
            }
        }
        else
        {
            dtResult = _navMeshQuery->findPath(startPoly, // start polygon
                endPoly,                                  // end polygon
                startPoint,                               // start position
                endPoint,                                 // end position
                &_filter,      // polygon search filter
                _pathPolyRefs, // [out] path
                (int*)&_polyLength,
                MAX_PATH_LENGTH); // max number of polygons in output path
        }

        if (!_polyLength || dtStatusFailed(dtResult))
        {
            // only happens if we passed bad data to findPath(), or navmesh is
            // messed up
            logging.error("Path Build failed: 0 length path");
            BuildShortcut();
            _type = PATHFIND_NOPATH;
            return;
        }
    }

    // by now we know what type of path we can get
    if (_pathPolyRefs[_polyLength - 1] == endPoly &&
        !(_type & PATHFIND_INCOMPLETE))
        _type = PATHFIND_NORMAL;
    else
        _type = PATHFIND_INCOMPLETE;

    // generate the point-path out of our up-to-date poly-path
    BuildPointPath(startPoint, endPoint);
}

void PathFinder::BuildPointPath(const float* startPoint, const float* endPoint)
{
    float pathPoints[MAX_POINT_PATH_LENGTH * VERTEX_SIZE];
    uint32 pointCount = 0;
    dtStatus dtResult = DT_FAILURE;
    if (_straightLine)
    {
        // if the path is a straight line then start and end position are enough
        dtResult = DT_SUCCESS;
        pointCount = 2;
        memcpy(&pathPoints[0], startPoint, sizeof(float) * 3);
        memcpy(&pathPoints[3], endPoint, sizeof(float) * 3);
    }
    else if (_useStraightPath)
    {
        dtResult = _navMeshQuery->findStraightPath(startPoint, // start position
            endPoint,                                          // end position
            _pathPolyRefs,                                     // current path
            _polyLength, // lenth of current path
            pathPoints,  // [out] path corner points
            nullptr,     // [out] flags
            nullptr,     // [out] shortened path
            (int*)&pointCount,
            _pointPathLimit); // maximum number of points/polygons to use
    }
    else
    {
        dtResult = FindSmoothPath(startPoint, // start position
            endPoint,                         // end position
            _pathPolyRefs,                    // current path
            _polyLength,                      // length of current path
            pathPoints,                       // [out] path corner points
            (int*)&pointCount,
            _pointPathLimit); // maximum number of points
    }

    if (pointCount < 2 || dtStatusFailed(dtResult))
    {
        // only happens if pass bad data to findStraightPath or navmesh is
        // broken
        // single point paths can be generated here
        /// @todo check the exact cases
        LOG_DEBUG(logging,
            "++ PathFinder::BuildPointPath FAILED! path sized %d returned\n",
            pointCount);
        BuildShortcut();
        if (_forceDestination)
            _type = PathType(PATHFIND_NORMAL | PATHFIND_NOT_USING_PATH);
        else
            _type = PATHFIND_NOPATH;
        return;
    }
    else if (pointCount == _pointPathLimit)
    {
        LOG_DEBUG(logging,
            "++ PathFinder::BuildPointPath FAILED! path sized %d returned, "
            "lower than limit set to %d\n",
            pointCount, _pointPathLimit);
        BuildShortcut();
        if (_forceDestination)
            _type = PathType(PATHFIND_NORMAL | PATHFIND_NOT_USING_PATH);
        else
            _type = PATHFIND_SHORT;
        return;
    }

    _pathPoints.resize(pointCount);
    for (uint32 i = 0; i < pointCount; ++i)
        _pathPoints[i] = G3D::Vector3(pathPoints[i * VERTEX_SIZE + 2],
            pathPoints[i * VERTEX_SIZE], pathPoints[i * VERTEX_SIZE + 1]);

    SetActualEndPosition(_pathPoints[pointCount - 1]);

    // force the given destination, if needed
    if (_forceDestination &&
        (!(_type & PATHFIND_NORMAL) ||
            !InRange(getEndPosition(), getActualEndPosition(), 1.0f, 1.0f)))
    {
        // we may want to keep partial subpath
        if (Dist3DSqr(getActualEndPosition(), getEndPosition()) <
            0.3f * Dist3DSqr(getStartPosition(), getEndPosition()))
        {
            SetActualEndPosition(getEndPosition());
            _pathPoints[_pathPoints.size() - 1] = getEndPosition();
        }
        else
        {
            SetActualEndPosition(getEndPosition());
            BuildShortcut();
        }

        _type = PathType(PATHFIND_NORMAL | PATHFIND_NOT_USING_PATH);
    }

    LOG_DEBUG(logging,
        "++ PathFinder::BuildPointPath path type %d size %d poly-size %d\n",
        _type, pointCount, _polyLength);
}

void PathFinder::NormalizePath()
{
    assert(_sourceUnit);

    if (!_sourceUnit->IsInWorld())
        return;

    auto normalize = [this](auto& elem)
    {
        if (Transport* trans = _sourceUnit->GetTransport())
        {
            float z = trans->GetHeight(elem);
            if (z < INVALID_HEIGHT)
                elem.z = z;
        }
        else
        {
            float z = _sourceUnit->GetMap()->GetTerrain()->GetHeightStatic(
                elem.x, elem.y, elem.z, true, 8.0f);
            if (z < INVALID_HEIGHT)
                elem.z = z;
        }
    };

    // Only normalize first and last point for NPCs
    if (_sourceUnit->GetTypeId() != TYPEID_PLAYER)
    {
        if (!_pathPoints.empty())
        {
            normalize(_pathPoints[0]);
            if (_pathPoints.size() > 1)
                normalize(_pathPoints[_pathPoints.size() - 1]);
        }
        return;
    }

    // Normalize all points for players
    for (auto& elem : _pathPoints)
        normalize(elem);
}

void PathFinder::BuildShortcut()
{
    LOG_DEBUG(logging, "++ BuildShortcut :: making shortcut\n");

    Clear();

    // make two point path, our curr pos is the start, and dest is the end
    _pathPoints.resize(2);

    // set start and a default next position
    _pathPoints[0] = getStartPosition();
    _pathPoints[1] = getActualEndPosition();

    _type = PATHFIND_SHORTCUT;
}

void PathFinder::CreateFilter()
{
    assert(_sourceUnit);

    uint16 includeFlags = 0;
    uint16 excludeFlags = 0;
    _filter.setIncludeFlags(NAV_GROUND);
    _filter.setExcludeFlags(0);
    return;

    if (_sourceUnit->GetTypeId() == TYPEID_UNIT)
    {
        Creature* creature = (Creature*)_sourceUnit;
        if (creature->CanWalk())
            includeFlags |= NAV_GROUND; // walk

        // creatures don't take environmental damage
        if (creature->CanSwim())
            includeFlags |= (NAV_WATER | NAV_MAGMA | NAV_SLIME); // swim
    }
    else // assume Player
    {
        // perfect support not possible, just stay 'safe'
        includeFlags |= (NAV_GROUND | NAV_WATER | NAV_MAGMA | NAV_SLIME);
    }

    _filter.setIncludeFlags(includeFlags);
    _filter.setExcludeFlags(excludeFlags);

    UpdateFilter();
}

void PathFinder::UpdateFilter()
{
    // allow creatures to cheat and use different movement types if they are
    // moved
    // forcefully into terrain they can't normally move in
    auto status = _sourceUnit->GetLiquidStatus();
    if ((status & LIQUID_MAP_IN_WATER) || (status & LIQUID_MAP_UNDER_WATER))
    {
        uint16 includedFlags = _filter.getIncludeFlags();
        includedFlags |= GetNavTerrain();

        _filter.setIncludeFlags(includedFlags);
    }
}

NavTerrain PathFinder::GetNavTerrain()
{
    GridMapLiquidData data;
    GridMapLiquidStatus liquidStatus =
        _sourceUnit->GetLiquidStatus(MAP_ALL_LIQUIDS, &data);
    if (liquidStatus == LIQUID_MAP_NO_WATER)
        return NAV_GROUND;

    switch (data.type_flags)
    {
    case MAP_LIQUID_TYPE_WATER:
    case MAP_LIQUID_TYPE_OCEAN:
        return NAV_WATER;
    case MAP_LIQUID_TYPE_MAGMA:
        return NAV_MAGMA;
    case MAP_LIQUID_TYPE_SLIME:
        return NAV_SLIME;
    default:
        return NAV_GROUND;
    }
}

bool PathFinder::HaveTile(const G3D::Vector3& p) const
{
    int tx = -1, ty = -1;
    float point[VERTEX_SIZE] = {p.y, p.z, p.x};

    _navMesh->calcTileLoc(point, &tx, &ty);

    /// Workaround
    /// For some reason, often the tx and ty variables wont get a valid value
    /// Use this check to prevent getting negative tile coords and crashing on
    /// getTileAt
    if (tx < 0 || ty < 0)
        return false;

    return (_navMesh->getTileAt(tx, ty, 0) != nullptr);
}

uint32 PathFinder::FixupCorridor(dtPolyRef* path, uint32 npath, uint32 maxPath,
    dtPolyRef const* visited, uint32 nvisited)
{
    int32 furthestPath = -1;
    int32 furthestVisited = -1;

    // Find furthest common polygon.
    for (int32 i = npath - 1; i >= 0; --i)
    {
        bool found = false;
        for (int32 j = nvisited - 1; j >= 0; --j)
        {
            if (path[i] == visited[j])
            {
                furthestPath = i;
                furthestVisited = j;
                found = true;
            }
        }
        if (found)
            break;
    }

    // If no intersection found just return current path.
    if (furthestPath == -1 || furthestVisited == -1)
        return npath;

    // Concatenate paths.

    // Adjust beginning of the buffer to include the visited.
    uint32 req = nvisited - furthestVisited;
    uint32 orig = uint32(furthestPath + 1) < npath ? furthestPath + 1 : npath;
    uint32 size = npath > orig ? npath - orig : 0;
    if (req + size > maxPath)
        size = maxPath - req;

    if (size)
        memmove(path + req, path + orig, size * sizeof(dtPolyRef));

    // Store visited
    for (uint32 i = 0; i < req; ++i)
        path[i] = visited[(nvisited - 1) - i];

    return req + size;
}

bool PathFinder::GetSteerTarget(float const* startPos, float const* endPos,
    float minTargetDist, dtPolyRef const* path, uint32 pathSize,
    float* steerPos, unsigned char& steerPosFlag, dtPolyRef& steerPosRef)
{
    // Find steer target.
    static const uint32 MAX_STEER_POINTS = 3;
    float steerPath[MAX_STEER_POINTS * VERTEX_SIZE];
    unsigned char steerPathFlags[MAX_STEER_POINTS];
    dtPolyRef steerPathPolys[MAX_STEER_POINTS];
    uint32 nsteerPath = 0;
    dtStatus dtResult = _navMeshQuery->findStraightPath(startPos, endPos, path,
        pathSize, steerPath, steerPathFlags, steerPathPolys, (int*)&nsteerPath,
        MAX_STEER_POINTS);
    if (!nsteerPath || dtStatusFailed(dtResult))
        return false;

    // Find vertex far enough to steer to.
    uint32 ns = 0;
    while (ns < nsteerPath)
    {
        // Stop at Off-Mesh link or when point is further than slop away.
        if ((steerPathFlags[ns] & DT_STRAIGHTPATH_OFFMESH_CONNECTION) ||
            !InRangeYZX(
                &steerPath[ns * VERTEX_SIZE], startPos, minTargetDist, 1000.0f))
            break;
        ns++;
    }
    // Failed to find good point to steer to.
    if (ns >= nsteerPath)
        return false;

    dtVcopy(steerPos, &steerPath[ns * VERTEX_SIZE]);
    steerPos[1] = startPos[1]; // keep Z value
    steerPosFlag = steerPathFlags[ns];
    steerPosRef = steerPathPolys[ns];

    return true;
}

dtStatus PathFinder::FindSmoothPath(float const* startPos, float const* endPos,
    dtPolyRef const* polyPath, uint32 polyPathSize, float* smoothPath,
    int* smoothPathSize, uint32 maxSmoothPathSize)
{
    *smoothPathSize = 0;
    uint32 nsmoothPath = 0;

    dtPolyRef polys[MAX_PATH_LENGTH];
    memcpy(polys, polyPath, sizeof(dtPolyRef) * polyPathSize);
    uint32 npolys = polyPathSize;

    float iterPos[VERTEX_SIZE], targetPos[VERTEX_SIZE];
    if (dtStatusFailed(_navMeshQuery->closestPointOnPolyBoundary(
            polys[0], startPos, iterPos)))
        return DT_FAILURE;

    if (dtStatusFailed(_navMeshQuery->closestPointOnPolyBoundary(
            polys[npolys - 1], endPos, targetPos)))
        return DT_FAILURE;

    dtVcopy(&smoothPath[nsmoothPath * VERTEX_SIZE], iterPos);
    nsmoothPath++;

    // Move towards target a small advancement at a time until target reached or
    // when ran out of memory to store the path.
    while (npolys && nsmoothPath < maxSmoothPathSize)
    {
        // Find location to steer towards.
        float steerPos[VERTEX_SIZE];
        unsigned char steerPosFlag;
        dtPolyRef steerPosRef = INVALID_POLYREF;

        if (!GetSteerTarget(iterPos, targetPos, SMOOTH_PATH_SLOP, polys, npolys,
                steerPos, steerPosFlag, steerPosRef))
            break;

        bool endOfPath = (steerPosFlag & DT_STRAIGHTPATH_END);
        bool offMeshConnection =
            (steerPosFlag & DT_STRAIGHTPATH_OFFMESH_CONNECTION);

        // Save point index we found endOfPath at
        if (endOfPath && _endOfPathIdx == -1)
            _endOfPathIdx = nsmoothPath;

        // Find movement delta.
        float delta[VERTEX_SIZE];
        dtVsub(delta, steerPos, iterPos);
        float len = dtSqrt(dtVdot(delta, delta));
        // If the steer target is end of path or off-mesh link, do not move past
        // the location.
        if ((endOfPath || offMeshConnection) && len < SMOOTH_PATH_STEP_SIZE)
            len = 1.0f;
        else
            len = SMOOTH_PATH_STEP_SIZE / len;

        float moveTgt[VERTEX_SIZE];
        dtVmad(moveTgt, iterPos, delta, len);

        // Move
        float result[VERTEX_SIZE];
        const static uint32 MAX_VISIT_POLY = 16;
        dtPolyRef visited[MAX_VISIT_POLY];

        uint32 nvisited = 0;
        _navMeshQuery->moveAlongSurface(polys[0], iterPos, moveTgt, &_filter,
            result, visited, (int*)&nvisited, MAX_VISIT_POLY);
        npolys =
            FixupCorridor(polys, npolys, MAX_PATH_LENGTH, visited, nvisited);

        _navMeshQuery->getPolyHeight(polys[0], result, &result[1]);
        result[1] += 0.5f;
        dtVcopy(iterPos, result);

        // Handle end of path and off-mesh links when close enough.
        if (endOfPath && InRangeYZX(iterPos, steerPos, SMOOTH_PATH_SLOP, 1.0f))
        {
            // Reached end of path.
            dtVcopy(iterPos, targetPos);
            if (nsmoothPath < maxSmoothPathSize)
            {
                dtVcopy(&smoothPath[nsmoothPath * VERTEX_SIZE], iterPos);
                nsmoothPath++;
            }
            break;
        }
        else if (offMeshConnection &&
                 InRangeYZX(iterPos, steerPos, SMOOTH_PATH_SLOP, 1.0f))
        {
            // Advance the path up to and over the off-mesh connection.
            dtPolyRef prevRef = INVALID_POLYREF;
            dtPolyRef polyRef = polys[0];
            uint32 npos = 0;
            while (npos < npolys && polyRef != steerPosRef)
            {
                prevRef = polyRef;
                polyRef = polys[npos];
                npos++;
            }

            for (uint32 i = npos; i < npolys; ++i)
                polys[i - npos] = polys[i];

            npolys -= npos;

            // Handle the connection.
            float startPos[VERTEX_SIZE], endPos[VERTEX_SIZE];
            if (dtStatusSucceed(_navMesh->getOffMeshConnectionPolyEndPoints(
                    prevRef, polyRef, startPos, endPos)))
            {
                if (nsmoothPath < maxSmoothPathSize)
                {
                    dtVcopy(&smoothPath[nsmoothPath * VERTEX_SIZE], startPos);
                    nsmoothPath++;
                }
                // Move position at the other side of the off-mesh link.
                dtVcopy(iterPos, endPos);
                _navMeshQuery->getPolyHeight(polys[0], iterPos, &iterPos[1]);
                iterPos[1] += 0.5f;
            }
        }

        // Store results.
        if (nsmoothPath < maxSmoothPathSize)
        {
            dtVcopy(&smoothPath[nsmoothPath * VERTEX_SIZE], iterPos);
            nsmoothPath++;
        }
    }

    *smoothPathSize = nsmoothPath;

    // this is most likely a loop
    return nsmoothPath < MAX_POINT_PATH_LENGTH ? DT_SUCCESS : DT_FAILURE;
}

bool PathFinder::InRangeYZX(
    const float* v1, const float* v2, float r, float h) const
{
    const float dx = v2[0] - v1[0];
    const float dy = v2[1] - v1[1]; // elevation
    const float dz = v2[2] - v1[2];
    return (dx * dx + dz * dz) < r * r && fabsf(dy) < h;
}

bool PathFinder::InRange(
    G3D::Vector3 const& p1, G3D::Vector3 const& p2, float r, float h) const
{
    G3D::Vector3 d = p1 - p2;
    return (d.x * d.x + d.y * d.y) < r * r && fabsf(d.z) < h;
}

float PathFinder::Dist3DSqr(
    G3D::Vector3 const& p1, G3D::Vector3 const& p2) const
{
    return (p1 - p2).squaredLength();
}

uint32 movement::BuildRetailLikePath(std::vector<G3D::Vector3>& path,
    Unit* src_obj, Unit* dst_obj, float max_len,
    std::function<void(Unit*, std::vector<G3D::Vector3>, uint32)> receiver)
{
    retail_like_path_builder b;
    b.set_src(src_obj);
    b.set_dst(dst_obj);
    b.set_maxlen(max_len);
    if (receiver)
        b.set_receiver(receiver);

    if (!b.build())
        return b.get_path_id(); // Path not built yet; it will be delivered
                                // sometime in the future

    path = b.path();

    return 0;
}

uint32 movement::BuildRetailLikePath(std::vector<G3D::Vector3>& path,
    Unit* src_obj, G3D::Vector3 dst,
    std::function<void(Unit*, std::vector<G3D::Vector3>, uint32)> receiver)
{
    retail_like_path_builder b;
    b.set_src(src_obj);
    b.set_dst(dst);
    if (receiver)
        b.set_receiver(receiver);

    if (!b.build())
        return b.get_path_id(); // Path not built yet; it will be delivered
                                // sometime in the future

    path = b.path();

    return 0;
}

bool movement::retail_like_path_builder::build()
{
    src_obj->GetPosition(src.x, src.y, src.z);
    if (dst_obj)
    {
        dst_obj->GetPosition(dst.x, dst.y, dst.z);
        // NOTE: Bounding radius does not apply if we were given a dst position
        if (dst_obj->GetTypeId() == TYPEID_PLAYER &&
            src_obj->GetTypeId() == TYPEID_PLAYER)
            bounding_radius = 0.8f * src_obj->GetMeleeReach(dst_obj);
        else
            bounding_radius = src_obj->GetObjectBoundingRadius() +
                              dst_obj->GetObjectBoundingRadius();
    }

    // If we're within bounding radius we need not move
    if (0.0f > G3D::distance(dst.x - src.x, dst.y - src.y, dst.z - src.z) -
                   bounding_radius - 0.1f)
    {
        points.push_back(src);
        points.push_back(src);
        return true;
    }

    // Pathing on transports need to be handled before anything else
    if (handle_transport_pathing())
        return true;

    // Can only stand still in place if target is flying and we can't fly
    if (dst_obj && src_obj->GetTypeId() == TYPEID_UNIT &&
        !src_obj->can_pathing_cheat() &&
        dst_obj->m_movementInfo.HasMovementFlag(MOVEFLAG_FLYING2) &&
        !static_cast<Creature*>(src_obj)->CanFly())
    {
        points.push_back(src);
        points.push_back(src);
        return true;
    }

    // Full water paths
    if (handle_full_water_pathing())
    {
        if (!points.empty())
            points.emplace(points.begin(), src_obj->GetX(), src_obj->GetY(),
                src_obj->GetZ());
        else if (src_obj->can_pathing_cheat())
            draw_cheat_path();
        return true;
    }

    // If we cannot leave water and handle_full_water_pathing did not find a
    // path then there is no path for us
    if (src_obj->GetTypeId() == TYPEID_UNIT &&
        !static_cast<Creature*>(src_obj)->CanWalk() &&
        static_cast<Creature*>(src_obj)->CanSwim())
    {
        points.clear();
        return true;
    }

    // Only do vmap height calculation for src and dst once as it's fairly
    // expensive
    calc_heights();

    // Air pathing
    if (handle_air_pathing())
    {
        if (!points.empty())
            points.emplace(points.begin(), src_obj->GetX(), src_obj->GetY(),
                src_obj->GetZ());
        else if (src_obj->can_pathing_cheat())
            draw_cheat_path();
        return true;
    }

    // Use NavMesh (detour) navigation if there are WMOs or M2s as part of our
    // path
    bool navmesh_path = should_navmesh_path();
    if (navmesh_path)
    {
        if (draw_navmesh_path())
            return false; // Path queued, will be delivered sometime in the
                          // future

        if (points.empty())
            return true;
    }

    // Do ADT pathing if we didn't need a NavMesh path
    if (points.empty())
        draw_adt_path();

    // Finish phase can either be done when navmesh callback finishes, or
    // in-place here
    if (!do_finish_phase(navmesh_path))
        return true;

    // Add our own position as first point (if ADT pathing)
    if (!navmesh_path && !points.empty())
        points.emplace(
            points.begin(), src_obj->GetX(), src_obj->GetY(), src_obj->GetZ());

    return true;
}

bool movement::retail_like_path_builder::do_finish_phase(bool nav_mesh)
{
    if (nav_mesh)
    {
        // Move end point to an outer part of bounding box
        navmesh_path_bounding_fix();
    }

    // Correct points that are in water (if any)
    if (!points.empty())
        handle_partial_water_pathing();

    // Check if end point of path is close enough to intended dst
    if (src_obj->GetTypeId() == TYPEID_UNIT && !points.empty())
    {
        // We're okay with a path below in Z (on the ground while target is
        // flying), but still in line 2D wise if we:
        // a) don't have a dst_obj
        // or
        // b) have a dst object that's falling or flying
        const auto& end = points.back();
        if (G3D::distance(end.x - dst.x, end.y - dst.y, end.z - dst.z) >
            bounding_radius + 2.0f)
        {
            if (!(G3D::distance(end.x - dst.x, end.y - dst.y) <
                        bounding_radius + 1.0f &&
                    (dst.z + 0.5f > end.z ||
                        (dst_obj &&
                            dst_obj->m_movementInfo.HasMovementFlag(
                                MOVEFLAG_GRAVITY | MOVEFLAG_FLYING |
                                MOVEFLAG_FLYING2)))))
            {
                points.clear();
                return false;
            }
        }
    }

    // Check so end point is not in the air for mobs that cannot fly
    if (!points.empty() && !dst_in_water &&
        src_obj->GetTypeId() == TYPEID_UNIT && !src_obj->can_pathing_cheat() &&
        !static_cast<Creature*>(src_obj)->CanFly())
    {
        const auto& end = points.back();
        if (dst_ground + 2.0f < end.z)
        {
            points.clear();
            return false;
        }
    }

    pet_door_fix();

    return true;
}

bool movement::retail_like_path_builder::handle_transport_pathing()
{
    if (!src_obj->GetTransport() && !(dst_obj && dst_obj->GetTransport()))
        return false;

    // dst object and src need to be on the same transport
    if (dst_obj && dst_obj->GetTransport() != src_obj->GetTransport())
        return true; // Found no path

    // NOTE: we cannot build transport paths concurrently as the transport moves
    // between maps

    PathFinder finder(src_obj);

    if (max_len > 0)
        finder.setPathLengthLimit(max_len);

    if (dst_obj)
        dst_obj->m_movementInfo.transport.pos.Get(dst.x, dst.y, dst.z);

    if (!finder.calculate(dst.x, dst.y, dst.z, src_obj->can_pathing_cheat()))
        return true; // Found no path

    if (finder.getPathType() &
        (PATHFIND_NOPATH | PATHFIND_INCOMPLETE | PATHFIND_SHORT))
        return true; // Found no path

    points = finder.getPath();
    return true;
}

bool movement::retail_like_path_builder::handle_full_water_pathing()
{
    // Mobs that cannot swim does not seem to draw paths if the target is in
    // water
    if (src_obj->GetTypeId() == TYPEID_UNIT &&
        !static_cast<Creature*>(src_obj)->CanSwim())
    {
        if (dst_obj && dst_obj->GetTypeId() == TYPEID_PLAYER)
        {
            // Look a bit further down for player targets (decrease Z even more
            // if jumping)
            bool falling =
                static_cast<Player*>(dst_obj)->m_movementInfo.HasMovementFlag(
                    MOVEFLAG_GRAVITY);
            if (dst_obj->GetLiquidStatus(dst.x, dst.y,
                    dst.z - (falling ? 3.0f : 1.0f), MAP_ALL_LIQUIDS) &
                LIQUID_MAP_UNDER_WATER)
                return true; // no path possible
        }
        else
        {
            if (dst_obj)
            {
                if (dst_obj->GetLiquidStatus(
                        dst.x, dst.y, dst.z, MAP_ALL_LIQUIDS) &
                    LIQUID_MAP_UNDER_WATER)
                    return true; // no path possible
            }
            else
            {
                if (src_obj->GetLiquidStatus(
                        dst.x, dst.y, dst.z, MAP_ALL_LIQUIDS) &
                    LIQUID_MAP_UNDER_WATER)
                    return true; // no path possible
            }
        }
        return false;
    }

    bool src_in_water = false, src_shallow = false;
    auto liq = src_obj->GetLiquidStatus(src.x, src.y, src.z, MAP_ALL_LIQUIDS);
    if (liq & (LIQUID_MAP_UNDER_WATER | LIQUID_MAP_IN_WATER))
    {
        src_in_water = true;
        if ((liq & LIQUID_MAP_UNDER_WATER) == 0)
            src_shallow = true;
    }

    bool dst_in_water = false, dst_shallow = false;
    if (dst_obj)
        liq = dst_obj->GetLiquidStatus(dst.x, dst.y, dst.z, MAP_ALL_LIQUIDS);
    else
        liq = src_obj->GetLiquidStatus(dst.x, dst.y, dst.z, MAP_ALL_LIQUIDS);

    if (liq & (LIQUID_MAP_UNDER_WATER | LIQUID_MAP_IN_WATER))
    {
        dst_in_water = true;
        if ((liq & LIQUID_MAP_UNDER_WATER) == 0)
            dst_shallow = true;
    }

    // If we have dst or src in water, but not full path, we need to do
    // expensive WMO checks later on
    if (dst_in_water || src_in_water)
        wmo_water = true;

    // For full water pathing, both dst and src needs to be in water
    if (!src_in_water || !dst_in_water)
        return false;

    // If water is shallow, make sure no M2 object stands between points
    if (src_shallow && dst_shallow &&
        !VMAP::VMapFactory::createOrGetVMapManager()->isInM2LineOfSight(
            src_obj->GetMapId(), src.x, src.y, src.z + 1.0f, dst.x, dst.y,
            dst.z + 1.0f))
    {
        wmo_water = true;
        return false;
    }

    // Draw a simple point to point path
    // NOTE: This will go straight through WMOs, but it works the same on retail
    draw_lerp_path(src, dst);

    return true;
}

bool movement::retail_like_path_builder::handle_air_pathing()
{
    // NOTE: Air pathing uses NO pathing at all, it goes from point to point (as
    // tested on retail)
    //       It even goes straight through mountains and other ADT terrain & WMO
    //       objects.

    // Creatures either air path fully, or not at all
    if (src_obj->GetTypeId() == TYPEID_UNIT &&
        !static_cast<Creature*>(src_obj)->CanFly() &&
        (!src_obj->m_movementInfo.HasMovementFlag(MOVEFLAG_LEVITATING) ||
            src_obj->m_movementInfo.HasMovementFlag(MOVEFLAG_SWIMMING)))
        return false;

    // Players have special rules for when they can air path
    if (src_obj->GetTypeId() == TYPEID_PLAYER)
    {
        bool air_path = false;

        // If src player is falling, and not too close to the ground, we can air
        // path
        if (src_obj->m_movementInfo.HasMovementFlag(MOVEFLAG_GRAVITY) &&
            fabs(src_ground - src.z) > 5.0f)
            air_path = true;
        else if (dst_obj && dst_obj->GetTypeId() == TYPEID_PLAYER)
        {
            // If target is a flying player...
            if (static_cast<Player*>(dst_obj)->m_movementInfo.HasMovementFlag(
                    MOVEFLAG_FLYING2))
                air_path = true;
            // ...or falling and not too close to the ground
            else if (static_cast<Player*>(dst_obj)
                         ->m_movementInfo.HasMovementFlag(MOVEFLAG_GRAVITY) &&
                     fabs(dst_ground - dst.z) > 5.0f)
                air_path = true;
        }
        else if (dst_obj && dst_obj->GetTypeId() == TYPEID_UNIT)
        {
            // If target is a flying creature
            if (static_cast<Creature*>(dst_obj)->m_movementInfo.HasMovementFlag(
                    MOVEFLAG_LEVITATING) &&
                !static_cast<Creature*>(dst_obj)
                     ->m_movementInfo.HasMovementFlag(MOVEFLAG_SWIMMING))
                air_path = true;
        }

        if (!air_path)
            return false;
    }

    // As the NOTE explains above, we do a simple point to point path
    draw_lerp_path(src, dst);

    // For players we need to correct height points to not path through terrain
    if (src_obj->GetTypeId() == TYPEID_PLAYER)
    {
        // Skip if first point is below grid height (in case of underground WMO)
        auto grid = const_cast<TerrainInfo*>(src_obj->GetMap()->GetTerrain())
                        ->GetGrid(src.x, src.y);
        if (grid && grid->getHeight(src.x, src.y) < src.z)
        {
            // NOTE: We only need to use grid height; you cannot charge through
            // WMO objects as they block LoS,
            //       but you can charge through terrain as they don't block LoS
            for (auto& p : points)
            {
                if (auto grid =
                        const_cast<TerrainInfo*>(
                            src_obj->GetMap()->GetTerrain())->GetGrid(p.x, p.y))
                {
                    float grid_height = grid->getHeight(p.x, p.y);
                    if (grid_height > p.z)
                        p.z = grid_height + 1.0f;
                }
            }
        }
    }

    return true;
}

void movement::retail_like_path_builder::handle_partial_water_pathing()
{
    std::vector<size_t> water_points;

    // Figure out which points are under water
    for (size_t i = 0; i < points.size(); ++i)
    {
        auto& p = points[i];
        if (wmo_water)
        {
            if (src_obj->GetLiquidStatus(p.x, p.y, p.z, MAP_ALL_LIQUIDS) &
                LIQUID_MAP_UNDER_WATER)
                water_points.push_back(i);
        }
        else
        {
            if (auto grid =
                    const_cast<TerrainInfo*>(src_obj->GetMap()->GetTerrain())
                        ->GetGrid(p.x, p.y))
                if (grid->getLiquidStatus(p.x, p.y, p.z, MAP_ALL_LIQUIDS) &
                    LIQUID_MAP_UNDER_WATER)
                    water_points.push_back(i);
        }
    }

    // Cannot path at all if we have a water point and we can't swim
    // FIXME: This is not correct, on retail it will path around bodies of
    //        water if there's a path around it.
    if (!water_points.empty() && src_obj->GetTypeId() == TYPEID_UNIT &&
        !static_cast<Creature*>(src_obj)->CanSwim())
    {
        points.clear();
        return;
    }

    if (!water_points.empty() &&
        water_points[water_points.size() - 1] == (points.size() - 1))
        dst_in_water = true;

    float prev_bound = bounding_radius;

    auto itr = water_points.begin();
    while (itr != water_points.end())
    {
        size_t start = *itr;
        size_t end = *itr;
        G3D::Vector3 start_pos, end_pos;

        // Move start to land path if possible, for smoother result
        if (start != 0)
        {
            --start;
            start_pos = points[start];
        }
        else
            start_pos = src;

        while (++itr != water_points.end() && *itr == (end + 1))
            ++end;

        // Move end to land path if possible, for smoother result
        if ((end + 1) < points.size())
        {
            ++end;
            end_pos = points[end];
        }
        else
            end_pos = dst;
        ++end; // one-of-the-end iterator

        // Draw a straight path from point to point and replace the current
        // water path
        std::vector<G3D::Vector3> tmp;
        bounding_radius = 0;
        draw_lerp_path(start_pos, end_pos, &tmp);

        int prev_size = (int)points.size();
        points.erase(points.begin() + start, points.begin() + end);
        points.insert(points.begin() + start, tmp.begin(), tmp.end());

        // Indices have changed now, we need to adjust saved indices after *itr
        int diff = (int)points.size() - prev_size;
        for (auto itr_copy = itr; itr_copy != water_points.end(); ++itr_copy)
            *itr_copy = size_t((int)*itr_copy + diff);
    }

    bounding_radius =
        prev_bound; // changed inside of previous loop, reset to original value
}

void movement::retail_like_path_builder::draw_lerp_path(const G3D::Vector3& src,
    const G3D::Vector3& dst, std::vector<G3D::Vector3>* out)
{
    // get direction vector from dst to src, and move it bounding radius yards
    // away; that is our new dst
    auto new_dst = dst + (src - dst).direction() * bounding_radius;

    float dist = G3D::distance(
        new_dst.x - src.x, new_dst.y - new_dst.y, new_dst.z - src.z);

    if (dist < 2.0f)
    {
        if (out)
            out->push_back(new_dst);
        else
            points.push_back(new_dst);
        return;
    }

    float max_dist = dist;
    while (dist > 0.1f)
    {
        dist -= dist >= 4 ? 4 : dist;

        // if next point would be really close, just consume it as well instead
        if (dist < 2.0f)
            dist = 0;

        float progress = 1.0f - (dist / max_dist);
        if (out)
            out->push_back(
                src.lerp(new_dst, progress)); // lerp: Linear Interpolation
        else
            points.push_back(
                src.lerp(new_dst, progress)); // lerp: Linear Interpolation
    }
}

bool movement::retail_like_path_builder::draw_navmesh_path()
{
    // NOTE: navmesh paths cannot be used for dst above ground if we can't
    // pathing cheat (flying
    //       already handled at this point), correct dst z in those cases.
    if (!src_obj->can_pathing_cheat() && dst.z > dst_ground)
        dst.z = dst_ground;

    // Players standing at the ledge of something will have ground far below
    // them, but actually not be falling towards is. Basically the mid point of
    // the player is hovering in the air. There's one redeeming quality: if the
    // player jumps in this scenario, he will fall all the way down, so a player
    // not affected by gravity, and not flying, far above the target can safely
    // be pathed to.
    if (dst_obj && dst_obj->GetZ() > dst.z + 3 &&
        dst_obj->GetTypeId() == TYPEID_PLAYER &&
        !dst_obj->m_movementInfo.HasMovementFlag(
            MOVEFLAG_GRAVITY | MOVEFLAG_FLYING2))
        dst.z = dst_obj->GetZ();

    if (sConcurrentPathgen::Instance()->in_use())
    {
        path_id = sConcurrentPathgen::Instance()->queue_path(
            src_obj->GetMap(), *this, src_obj->can_pathing_cheat());
        return true;
    }

    PathFinder f(src_obj);

    if (max_len > 0)
        f.setPathLengthLimit(max_len);

    if (!f.calculate(dst.x, dst.y, dst.z, src_obj->can_pathing_cheat()))
        return false; // Navmesh path was built, but 0 points

    if (f.getPathType() &
        (PATHFIND_NOPATH | PATHFIND_INCOMPLETE | PATHFIND_SHORT))
        return false; // Navmesh path was built, but 0 points

    points = f.getPath();
    endofpath_index = f.getEndOfPathIndex();

    return false;
}

void movement::retail_like_path_builder::draw_adt_path()
{
    // For ADT pathing distance, we only care about 2D
    float dist = G3D::distance(dst.x - src.x, dst.y - src.y) - bounding_radius;
    // Make sure we draw at least one point
    if (dist <= 0)
        dist = 0.5f;
    float o = src_obj->GetAngle(dst.x, dst.y);

    auto src_copy = src; // don't modify this->src

    while (dist > 0.1f) // 0.1 in case of precision errors
    {
        // We want about ~4 yards apart in 3D distance, so for steep places
        // we'll need to try multiple times
        float point_dist = dist > 4 ? 4 : dist;
        G3D::Vector3 tmp_src;
        while (true)
        {
            tmp_src = src_copy;

            tmp_src.x = tmp_src.x + point_dist * cos(o);
            tmp_src.y = tmp_src.y + point_dist * sin(o);
            if (auto grid =
                    const_cast<TerrainInfo*>(src_obj->GetMap()->GetTerrain())
                        ->GetGrid(tmp_src.x, tmp_src.y))
                tmp_src.z = grid->getHeight(tmp_src.x, tmp_src.y);

            if (G3D::distance(src_copy.x - tmp_src.x, src_copy.y - tmp_src.y,
                    src_copy.z - tmp_src.z) < 6.0f)
                break;

            point_dist -= point_dist > 1.5f ? 1.0f : 0.5f;

            if (point_dist < 0.1f)
                break;
        }

        dist -= G3D::distance(src_copy.x - tmp_src.x, src_copy.y - tmp_src.y);

        // If Z-incline is really high, add some arbitrary Z to resulting
        // position. This occurs when going up-hill.
        if (tmp_src.z - 2.0f > src_copy.z)
            tmp_src.z += 1.5f;

        src_copy = tmp_src;
        points.push_back(src_copy);
    }

    auto& end = points.empty() ? src : points[points.size() - 1];

    // Add another point if we're outside of bounding radius
    if (G3D::distance(end.x - dst.x, end.y - dst.y, end.z - dst.z) >
        bounding_radius)
    {
        float tmp_bounding =
            bounding_radius -
            (G3D::distance(end.x - dst.x, end.y - dst.y, end.z - dst.z) -
                bounding_radius) -
            0.5f;
        G3D::Vector3 pos;
        if (tmp_bounding > 0)
        {
            pos.x = dst.x + tmp_bounding * cos(o - M_PI_F);
            pos.y = dst.y + tmp_bounding * sin(o - M_PI_F);
        }
        else
        {
            pos.x = dst.x;
            pos.y = dst.y;
        }
        if (auto grid =
                const_cast<TerrainInfo*>(src_obj->GetMap()->GetTerrain())
                    ->GetGrid(pos.x, pos.y))
            pos.z = grid->getHeight(pos.x, pos.y);
        points.push_back(pos);
    }

    // correct dst_ground
    if (!points.empty())
        dst_ground = points[points.size() - 1].z;
}

void movement::retail_like_path_builder::draw_cheat_path()
{
    points.clear();
    points.push_back(src);
    points.push_back(dst);
}

void movement::retail_like_path_builder::navmesh_path_bounding_fix()
{
    // TODO: This could be fixed better if we work it into PathFinder

    if (points.size() < 2 || !dst_obj || bounding_radius <= 0 ||
        dst.z > points.back().z + 3)
        return;

    // Case #1: we have no end of path index
    if (endofpath_index == -1 || (int)points.size() <= endofpath_index)
    {
        // Case #1.a: src and dst are very close to eachother, use a lerp path
        // (rather than go into the target)
        if (G3D::distance(dst.x - src.x, dst.y - src.y, dst.z - src.z) -
                bounding_radius <
            6.0f)
        {
            points.clear();
            points.push_back(src);
            draw_lerp_path(src, dst);
            return;
        }

        // Case #1.b: path generation was unable to establish a straight path,
        // do nothing
        return;
    }

    // Case #2: end of path index is within bounding radius, just remove rest of
    // path
    auto& eop = points[endofpath_index];
    if (G3D::distance(eop.x - dst.x, eop.y - dst.y, eop.z - dst.z) <=
        bounding_radius)
    {
        dst_ground = eop.z;
        points.erase(points.begin() + (endofpath_index + 1), points.end());
    }
    // Case #3: we need to manually compute contact point
    else
    {
        auto end = dst_obj->GetPointXYZ(dst, dst_obj->GetAngle(eop.x, eop.y),
            bounding_radius, src_obj->GetTypeId() == TYPEID_PLAYER);

        // If end is too far away, keep current path without fixing bounding box
        if (G3D::distance(end.x - dst.x, end.y - dst.y, end.z - dst.z) >=
            bounding_radius + 2.0f)
            return;

        int i = endofpath_index;
        while (i < (int)points.size())
        {
            auto& curr = points[i];
            if (G3D::distance(curr.x - dst.x, curr.y - dst.y, curr.z - dst.z) <=
                bounding_radius)
                break;
            ++i;
        }
        dst_ground = end.z;
        points.erase(points.begin() + i, points.end());
        points.push_back(end);
    }

    // If path got too small due to our erases:
    if (points.empty())
    {
        // Path removed completely
        points.push_back(src);
        points.push_back(src);
    }
    else if (points.size() == 1)
    {
        // Only one point remaining
        points.push_back(points.back());
    }
}

void movement::retail_like_path_builder::calc_heights()
{
    auto vmgr = VMAP::VMapFactory::createOrGetVMapManager();
    vmap_src_height = vmgr->getWmoHeight(
        src_obj->GetMapId(), src.x, src.y, src.z + 2.0f, 50.0f);
    if (vmap_src_height < VMAP_INVALID_HEIGHT)
        vmap_src_height = vmgr->getM2Height(
            src_obj->GetMapId(), src.x, src.y, src.z + 2.0f, 50.0f);
    vmap_dst_height = vmgr->getWmoHeight(
        src_obj->GetMapId(), dst.x, dst.y, dst.z + 2.0f, 50.0f);
    if (vmap_dst_height < VMAP_INVALID_HEIGHT)
        vmap_dst_height = vmgr->getM2Height(
            src_obj->GetMapId(), dst.x, dst.y, dst.z + 2.0f, 50.0f);

    // Get grid height data
    auto src_grid = const_cast<TerrainInfo*>(src_obj->GetMap()->GetTerrain())
                        ->GetGrid(src.x, src.y);
    if (!src_grid)
        return;
    auto dst_grid = const_cast<TerrainInfo*>(src_obj->GetMap()->GetTerrain())
                        ->GetGrid(dst.x, dst.y);
    if (!dst_grid)
        return;

    // Calculate ground height
    src_ground = src_grid->getHeight(src.x, src.y);
    dst_ground = dst_grid->getHeight(dst.x, dst.y);
    if (vmap_src_height > VMAP_INVALID_HEIGHT &&
        (vmap_src_height > src_ground || vmap_src_height > src.z - 2.0f))
        src_ground = vmap_src_height;
    if (vmap_dst_height > VMAP_INVALID_HEIGHT &&
        (vmap_dst_height > dst_ground || vmap_dst_height > dst.z - 2.0f))
        dst_ground = vmap_dst_height;
}

bool movement::retail_like_path_builder::should_navmesh_path()
{
    // If src/dst is on a WMO/M2, or there's a WMO/M2 blocking LoS from src to
    // dst, use navmesh pathing
    auto vmgr = VMAP::VMapFactory::createOrGetVMapManager();
    return vmap_src_height > VMAP_INVALID_HEIGHT ||
           vmap_dst_height > VMAP_INVALID_HEIGHT ||
           !(vmgr->isInWmoLineOfSight(src_obj->GetMapId(), src.x, src.y,
                 src_ground + 2.0f, dst.x, dst.y, dst_ground + 2.0f) &&
               vmgr->isInM2LineOfSight(src_obj->GetMapId(), src.x, src.y,
                   src_ground + 2.0f, dst.x, dst.y, dst_ground + 2.0f));
}

void movement::retail_like_path_builder::pet_door_fix()
{
    // Make sure player pets can't go through doors.
    if (src_obj->GetTypeId() != TYPEID_UNIT ||
        !static_cast<Creature*>(src_obj)->IsPet() ||
        !src_obj->GetCharmerOrOwnerPlayerOrPlayerItself())
        return;

    bool hit_door = false;
    for (size_t i = 1; i < points.size() && !hit_door; ++i)
        hit_door = !src_obj->GetMap()->isInDynLineOfSight(points[i - 1].x,
            points[i - 1].y, points[i - 1].z + 2.0f, points[i].x, points[i].y,
            points[i].z + 2.0f);

    if (hit_door)
    {
        points.clear();
        if (src_obj->can_pathing_cheat())
        {
            points.push_back(src);
            points.push_back(dst);
        }
    }
}
