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

#ifndef MANGOS_PATH_FINDER_H
#define MANGOS_PATH_FINDER_H

#include "DetourNavMesh.h"
#include "DetourNavMeshQuery.h"
#include "MoveMapSharedDefines.h"
#include "VMapFactory.h"
#include "movement/MoveSplineInitArgs.h"

class Unit;
class WorldObject;
using movement::Vector3;
using movement::PointsArray;

// 74*4.0f=296y  number_of_points*interval = max_path_len
// this is way more than actual evade range
// I think we can safely cut those down even more
#define MAX_PATH_LENGTH 74
#define MAX_POINT_PATH_LENGTH 74

#define SMOOTH_PATH_STEP_SIZE 4.0f
#define SMOOTH_PATH_SLOP 0.3f

#define VERTEX_SIZE 3
#define INVALID_POLYREF 0

enum PathType
{
    PATHFIND_BLANK = 0x00,  // path not built yet
    PATHFIND_NORMAL = 0x01, // normal path
    PATHFIND_SHORTCUT =
        0x02, // travel through obstacles, terrain, air, etc (old behavior)
    PATHFIND_INCOMPLETE =
        0x04, // we have partial path to follow - getting closer to target
    PATHFIND_NOPATH = 0x08, // no valid path at all or error in generating one
    PATHFIND_NOT_USING_PATH =
        0x10, // used when we are either flying/swiming or on map w/o mmaps
    PATHFIND_SHORT = 0x20, // path is longer or equal to its limited path length
};

class PathFinder
{
public:
    PathFinder();
    explicit PathFinder(Unit* owner);
    ~PathFinder();

    // delayedInit: must invoke if pathfinder has no Unit owner; this is used
    // for concurrent pathfinding
    void delayedInit(Unit* source, uint32 map_id);

    // finalize: invoke if you have no Unit owner; this is used for concurrent
    // pathfinding
    void finalize(Unit* source);

    // Calculate the path from owner to given destination
    // return: true if new path was calculated, false otherwise (no change
    // needed)
    bool calculate(float destX, float destY, float destZ,
        bool forceDest = false, bool straightLine = false);
    // Calculate the path from given position to given destination
    // return: true if new path was calculated, false otherwise (no change
    // needed)
    bool calculate(float srcX, float srcY, float srcZ, float destX, float destY,
        float destZ, bool forceDest = false, bool straightLine = false);

    // option setters - use optional
    void setUseStraightPath(bool useStraightPath)
    {
        _useStraightPath = useStraightPath;
    }
    void setPathLengthLimit(float distance)
    {
        _pointPathLimit = std::min<uint32>(
            uint32(distance / SMOOTH_PATH_STEP_SIZE), MAX_POINT_PATH_LENGTH);
    }

    // result getters
    G3D::Vector3 const& getStartPosition() const { return _startPosition; }
    G3D::Vector3 const& getEndPosition() const { return _endPosition; }
    G3D::Vector3 const& getActualEndPosition() const
    {
        return _actualEndPosition;
    }

    movement::PointsArray& getPath() { return _pathPoints; }

    PathType getPathType() const { return _type; }

    int getEndOfPathIndex() const
    {
        return _endOfPathIdx >= (int)_pathPoints.size() ? -1 : _endOfPathIdx;
    }

private:
    dtPolyRef
        _pathPolyRefs[MAX_PATH_LENGTH]; // array of detour polygon references
    uint32 _polyLength;                 // number of polygons in the path

    movement::PointsArray _pathPoints; // our actual (x,y,z) path to the target
    PathType _type;                    // tells what kind of path this is

    bool _useStraightPath;  // type of path will be generated
    bool _forceDestination; // when set, we will always arrive at given point
    uint32 _pointPathLimit; // limit point path size; min(this,
                            // MAX_POINT_PATH_LENGTH)
    bool _straightLine;     // use raycast if true for a straight line path

    G3D::Vector3 _startPosition;     // {x, y, z} of current location
    G3D::Vector3 _endPosition;       // {x, y, z} of the destination
    G3D::Vector3 _actualEndPosition; // {x, y, z} of the closest possible point
                                     // to given destination

    Unit* _sourceUnit;         // the unit that is moving
    dtNavMesh const* _navMesh; // the nav mesh
    dtNavMeshQuery const*
        _navMeshQuery; // the nav mesh query used to find the path

    dtQueryFilter
        _filter; // use single filter for all movements, update it when needed
    int _endOfPathIdx; // _pathPoints index when DT_STRAIGHTPATH_END was
                       // encountered (-1 if not found)

    bool _invalidPoly;
    bool _farAwayPoly;
    bool _canSwimOrFly;

    void SetStartPosition(G3D::Vector3 const& point) { _startPosition = point; }
    void SetEndPosition(G3D::Vector3 const& point)
    {
        _actualEndPosition = point;
        _endPosition = point;
    }
    void SetActualEndPosition(G3D::Vector3 const& point)
    {
        _actualEndPosition = point;
    }
    void NormalizePath();

    void Clear()
    {
        _polyLength = 0;
        _pathPoints.clear();
    }

    bool InRange(
        G3D::Vector3 const& p1, G3D::Vector3 const& p2, float r, float h) const;
    float Dist3DSqr(G3D::Vector3 const& p1, G3D::Vector3 const& p2) const;
    bool InRangeYZX(float const* v1, float const* v2, float r, float h) const;

    dtPolyRef GetPathPolyByPosition(dtPolyRef const* polyPath,
        uint32 polyPathSize, float const* Point,
        float* Distance = nullptr) const;
    dtPolyRef GetPolyByLocation(float const* Point, float* Distance) const;
    bool HaveTile(G3D::Vector3 const& p) const;

    void BuildPolyPath(
        G3D::Vector3 const& startPos, G3D::Vector3 const& endPos);
    void BuildPointPath(float const* startPoint, float const* endPoint);
    void BuildShortcut();

    NavTerrain GetNavTerrain();
    void CreateFilter();
    void UpdateFilter();

    // smooth path aux functions
    uint32 FixupCorridor(dtPolyRef* path, uint32 npath, uint32 maxPath,
        dtPolyRef const* visited, uint32 nvisited);
    bool GetSteerTarget(float const* startPos, float const* endPos,
        float minTargetDist, dtPolyRef const* path, uint32 pathSize,
        float* steerPos, unsigned char& steerPosFlag, dtPolyRef& steerPosRef);
    dtStatus FindSmoothPath(float const* startPos, float const* endPos,
        dtPolyRef const* polyPath, uint32 polyPathSize, float* smoothPath,
        int* smoothPathSize, uint32 smoothPathMaxSize);
};

// TODO: Make pathfinder part of movement namespace and move into movement/
namespace movement
{
// BuildRetailLikePath: Main functionality to build a path from one object to
// another
// max_len: this only applies if a smooth path (as opposed to a direct) is built
// points: returned path; vector with size()>=2, vector is empty() if no path
// was constructed
// receiver: function that will receive the path; leave as nullptr if it should
// be distributed to movement gens
// returns: id of queued path, 0 if built in place
uint32 BuildRetailLikePath(std::vector<G3D::Vector3>& path, Unit* src_obj,
    Unit* dst_obj, float max_len = 0,
    std::function<void(Unit*, std::vector<G3D::Vector3>, uint32)> receiver =
        nullptr);

// BuildRetailLikePath: Main functionality to build a path from an object to a
// point
// points: returned path; vector with size()>=2, vector is empty() if no path
// was constructed
// receiver: function that will receive the path; leave as nullptr if it should
// be distributed to movement gens
// returns: true if the path was built, false if it will be delivered sometime
// in the future
uint32 BuildRetailLikePath(std::vector<G3D::Vector3>& path, Unit* src_obj,
    G3D::Vector3 dst,
    std::function<void(Unit*, std::vector<G3D::Vector3>, uint32)> receiver =
        nullptr);

// class that builds retail-like paths
// don't use directly, instead use the helper functions BuildRetailLikePath()
class retail_like_path_builder
{
public:
    void set_src(Unit* s) { src_obj = s; }
    void set_dst(Unit* d) { dst_obj = d; }
    void set_dst(G3D::Vector3 d) { dst = d; }
    void set_maxlen(float len) { max_len = len; }
    void set_receiver(
        std::function<void(Unit*, std::vector<G3D::Vector3>, uint32)> recv)
    {
        receiver = recv;
    }
    void set_path(const std::vector<G3D::Vector3>& path) { points = path; }
    void set_eop_index(int index) { endofpath_index = index; }

    Unit* get_src_obj() const { return src_obj; }
    Unit* get_dst_obj() const { return dst_obj; }
    const G3D::Vector3& get_src() const { return src; }
    const G3D::Vector3& get_dst() const { return dst; }
    std::function<void(Unit*, std::vector<G3D::Vector3>, uint32)>
    get_receiver() const
    {
        return receiver;
    }
    uint32 get_path_id() const { return path_id; }

    float get_maxlen() const { return max_len; }

    bool build();

    std::vector<G3D::Vector3> path() const { return points; }
    bool do_finish_phase(bool nav_mesh);

private:
    // Initialization data
    Unit* src_obj = nullptr;
    Unit* dst_obj = nullptr;
    G3D::Vector3 dst = {0, 0, 0};
    float max_len = 0;

    std::function<void(Unit*, std::vector<G3D::Vector3>, uint32)> receiver =
        nullptr;

    // Resulting path
    std::vector<G3D::Vector3> points;
    uint32 path_id =
        0; // If path gets queued, this is the id given to the callback
    int endofpath_index = -1; // as returned by PathFinder::getEndOfPathIndex()

    // Internal state data
    G3D::Vector3 src;
    float bounding_radius = 0;
    float vmap_src_height = VMAP_INVALID_HEIGHT_VALUE;
    float vmap_dst_height = VMAP_INVALID_HEIGHT_VALUE;
    float src_ground = VMAP_INVALID_HEIGHT_VALUE;
    float dst_ground = VMAP_INVALID_HEIGHT_VALUE;
    bool wmo_water = false;    // true if we need to do expensive water
                               // calculations for all of the path
    bool dst_in_water = false; // true if end point of path is in water

    // Main functionality
    bool handle_transport_pathing();
    bool handle_full_water_pathing();
    bool handle_air_pathing();
    void handle_partial_water_pathing();

    // Helper functions
    void draw_lerp_path(const G3D::Vector3& src, const G3D::Vector3& dst,
        std::vector<G3D::Vector3>* out = nullptr);
    bool draw_navmesh_path();
    void draw_adt_path();
    void draw_cheat_path();
    void navmesh_path_bounding_fix();
    void calc_heights();
    // returns: true if path generation was queued with sConcurrentPathgen
    bool should_navmesh_path();
    void pet_door_fix();
};
}

#endif
