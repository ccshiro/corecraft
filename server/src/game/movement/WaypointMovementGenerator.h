/*
 * Copyright (C) 2005-2012 MaNGOS <http://getmangos.com/>
 * Copyright (C) 2015 corecraft <https://www.worldofcorecraft.com>
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

#ifndef GAME__MOVEMENT__WAYPOINT_MOVEMENT_GENERATOR_H
#define GAME__MOVEMENT__WAYPOINT_MOVEMENT_GENERATOR_H

#include "generators.h"
#include "Player.h"
#include "Unit.h"
#include "WaypointManager.h"
#include <set>
#include <vector>

#define FLIGHT_TRAVEL_UPDATE 100
#define STOP_TIME_FOR_PLAYER 3 * MINUTE* IN_MILLISECONDS // 3 Minutes

/** WaypointMovementGenerator loads a series of way points
 * from the DB and apply it to the creature's movement generator.
 * Hence, the creature will move according to its predefined way points.
 */

namespace movement
{
class MANGOS_DLL_SPEC WaypointMovementGenerator : public Generator
{
public:
    WaypointMovementGenerator()
      : i_path(nullptr), i_currentNode(0), i_nextMoveTime(0),
        m_interruptTimestamp(0), m_isArrivalDone(false), i_castedSpell(false),
        i_loaded(false)
    {
    }
    ~WaypointMovementGenerator() { i_path = nullptr; }

    gen id() const override { return gen::waypoint; }

    void start() override;
    void stop() override;
    bool update(uint32 diff, uint32) override;
    std::string debug_str() const override;

    void LoadPath();

private:
    const WaypointPath* i_path;
    uint32 i_currentNode;
    ShortTimeTracker i_nextMoveTime;
    uint32 m_interruptTimestamp;
    bool m_isArrivalDone;
    bool i_castedSpell;
    bool i_loaded;

    void Stop(int32 time) { i_nextMoveTime.Reset(time); }

    bool Stopped() { return !i_nextMoveTime.Passed(); }

    bool CanMove(int32 diff)
    {
        i_nextMoveTime.Update(diff);
        return i_nextMoveTime.Passed();
    }

    void OnArrived();
    void StartMove();
};

/** FlightPathMovementGenerator generates movement of the player for the paths
 * and hence generates ground and activities for the player.
 */
class MANGOS_DLL_SPEC FlightPathMovementGenerator : public Generator
{
public:
    FlightPathMovementGenerator(TaxiPathNodeList const& pathnodes,
        uint32 startNode = 0, uint32 mountDisplayId = 0)
    {
        i_path = &pathnodes;
        i_currentNode = startNode;
        i_mountDisplayId = mountDisplayId;
    }

    gen id() const override { return gen::flight; }

    void start() override;
    void stop() override;
    bool update(uint32 diff, uint32) override;

    TaxiPathNodeList const& GetPath() { return *i_path; }
    uint32 GetPathAtMapEnd() const;
    bool HasArrived() const { return (i_currentNode >= i_path->size()); }
    uint32 GetCurrentNode() const { return i_currentNode; }
    void SetCurrentNodeAfterTeleport();
    void SkipCurrentNode() { ++i_currentNode; }
    void DoEventIfAny(TaxiPathNodeEntry const& node, bool departure);

private:
    const TaxiPathNodeList* i_path;
    uint32 i_currentNode;
    uint32 i_mountDisplayId;
};

/** DynamicWaypointMovementGenerator uses an std::vector
 *  to define its path rather than waypoints from the database.
 *  The use for this could be either for a summone mob, which
 *  has no strict GUID in the database, or for a script
 *  which generats its waypoints on-the-fly
 */
class MANGOS_DLL_SPEC DynamicWaypointMovementGenerator : public Generator
{
public:
    DynamicWaypointMovementGenerator(
        const std::vector<DynamicWaypoint>& wpList, bool repeatPath);

    gen id() const override { return gen::waypoint; }

    void start() override;
    void stop() override;
    bool update(uint32 diff, uint32) override;
    std::string debug_str() const override;

    void LoadPath();

private:
    ShortTimeTracker i_nextMoveTime;
    std::vector<DynamicWaypoint> i_path;
    uint32 i_currentNode;
    bool m_repeatPath;
    bool m_waitingForMove;
    bool m_isArrivalDone;
    bool i_castedSpell;

    void Stop(int32 time) { i_nextMoveTime.Reset(time); }

    bool Stopped() { return !i_nextMoveTime.Passed(); }

    bool CanMove(int32 diff)
    {
        i_nextMoveTime.Update(diff);
        return i_nextMoveTime.Passed();
    }

    void OnArrived();
    void StartMove();

    void StartMoveNow()
    {
        i_nextMoveTime.Reset(0);
        StartMove();
    }
};

class MANGOS_DLL_SPEC SplineMovementGenerator : public Generator
{
public:
    SplineMovementGenerator(uint32 id, bool cyclic = false)
      : spline_(nullptr), id_(id), cyclic_(cyclic)
    {
    }

    gen id() const override { return gen::spline; }

    void start() override;
    void stop() override;
    bool update(uint32, uint32) override;
    std::string debug_str() const override;

private:
    const std::vector<G3D::Vector3>* spline_;
    uint32 id_;
    bool cyclic_;
};
}

#endif
