/*
 * Copyright (C) 2008-2014 TrinityCore <http://www.trinitycore.org/>
 * Copyright (C) 2005-2009 MaNGOS <http://getmangos.com/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef TRANSPORTS_H
#define TRANSPORTS_H

#include "GameObject.h"
#include "TransportMgr.h"

struct CreatureData;

class Transport : public GameObject
{
    friend Transport* TransportMgr::CreateTransport(uint32, uint32, Map*);

    Transport();

public:
    typedef std::set<WorldObject*> PassengerSet;

    ~Transport();

    bool Create(uint32 guidlow, uint32 mapid, float x, float y, float z,
        float ang, uint32 animprogress);
    void CleanupsBeforeDelete() override;

    void Update(uint32 diff, uint32);

    void BuildUpdateData(UpdateDataMapType& data_map) override;

    void AddPassenger(WorldObject* passenger);
    void RemovePassenger(WorldObject* passenger);
    PassengerSet const& GetPassengers() const { return _passengers; }

    /// This method transforms supplied transport offsets into global
    /// coordinates
    void CalculatePassengerPosition(
        float& x, float& y, float& z, float* o = nullptr) const
    {
        CalculatePassengerPosition(x, y, z, o, GetX(), GetY(), GetZ(), GetO());
    }

    /// This method transforms supplied global coordinates into local offsets
    void CalculatePassengerOffset(
        float& x, float& y, float& z, float* o = nullptr) const
    {
        CalculatePassengerOffset(x, y, z, o, GetX(), GetY(), GetZ(), GetO());
    }

    uint32 GetPeriod() const { return GetUInt32Value(GAMEOBJECT_LEVEL); }
    void SetPeriod(uint32 period) { SetUInt32Value(GAMEOBJECT_LEVEL, period); }
    uint32 GetTimer() const { return GetGOValue()->Transport.PathProgress; }

    KeyFrameVec const& GetKeyFrames() const
    {
        return _transportInfo->keyFrames;
    }

    void UpdatePosition(float x, float y, float z, float o);

    void EnableMovement(bool enabled);

    TransportTemplate const* GetTransportTemplate() const
    {
        return _transportInfo;
    }

    bool IsInLineOfSight(const G3D::Vector3& p1, const G3D::Vector3& p2) const;
    float GetHeight(G3D::Vector3 pos) const;

private:
    static void CalculatePassengerPosition(float& x, float& y, float& z,
        float* o, float transX, float transY, float transZ, float transO);
    static void CalculatePassengerOffset(float& x, float& y, float& z, float* o,
        float transX, float transY, float transZ, float transO);

    void MoveToNextWaypoint();
    float CalculateSegmentPos(float perc);
    bool TeleportTransport(uint32 newMapid, float x, float y, float z, float o);
    void UpdatePassengerPositions(PassengerSet passengers);
    void DoEventIfAny(KeyFrame const& node, bool departure);

    //! Helpers to know if stop frame was reached
    bool IsMoving() const { return _isMoving; }
    void SetMoving(bool val) { _isMoving = val; }

    TransportTemplate const* _transportInfo;

    KeyFrameVec::const_iterator _currentFrame;
    KeyFrameVec::const_iterator _nextFrame;
    ShortTimeTracker _positionChangeTimer;
    bool _isMoving;
    bool _pendingStop;

    //! These are needed to properly control events triggering only once for
    // each frame
    bool _triggeredArrivalEvent;
    bool _triggeredDepartureEvent;

    PassengerSet _passengers;
    PassengerSet::iterator _passengerTeleportItr;
};

#endif
