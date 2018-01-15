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

#include "Transport.h"
#include "Common.h"
#include "DBCStores.h"
#include "GameObjectModel.h"
#include "MapManager.h"
#include "MapReference.h"
#include "ObjectMgr.h"
#include "Path.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "Totem.h"
#include "World.h"
#include "WorldPacket.h"
#include "G3D/Ray.h"

Transport::Transport()
  : GameObject(), _transportInfo(nullptr), _isMoving(true), _pendingStop(false),
    _triggeredArrivalEvent(false), _triggeredDepartureEvent(false),
    _passengerTeleportItr(_passengers.begin())
{
    // 2.3.2 - 0x5A
    m_updateFlag = (UPDATEFLAG_TRANSPORT | UPDATEFLAG_LOWGUID |
                    UPDATEFLAG_HIGHGUID | UPDATEFLAG_HAS_POSITION);
}

Transport::~Transport()
{
    assert(_passengers.empty());
}

bool Transport::Create(uint32 guidlow, uint32 mapid, float x, float y, float z,
    float ang, uint32 animprogress)
{
    // guidlow == entry

    Relocate(x, y, z);
    SetOrientation(ang);

    if (!IsPositionValid())
    {
        logging.error(
            "Transport (GUID: %u) not created. Suggested coordinates isn't "
            "valid (X: %f Y: %f)",
            guidlow, x, y);
        return false;
    }

    Object::_Create(guidlow, 0, HIGHGUID_MO_TRANSPORT);

    GameObjectInfo const* goinfo =
        sObjectMgr::Instance()->GetGameObjectInfo(guidlow);

    if (!goinfo)
    {
        logging.error(
            "Transport not created: entry in `gameobject_template` not found, "
            "guidlow: %u map: %u  (X: %f Y: %f Z: %f) ang: %f",
            guidlow, mapid, x, y, z, ang);
        return false;
    }

    m_goInfo = goinfo;

    TransportTemplate const* tInfo =
        sTransportMgr::Instance()->GetTransportTemplate(guidlow);
    if (!tInfo)
    {
        logging.error(
            "Transport %u (name: %s) will not be created, missing "
            "`transport_template` entry.",
            guidlow, goinfo->name);
        return false;
    }

    _transportInfo = tInfo;

    // initialize waypoints
    _nextFrame = tInfo->keyFrames.begin();
    _currentFrame = _nextFrame++;
    _triggeredArrivalEvent = false;
    _triggeredDepartureEvent = false;

    m_goValue.Transport.PathProgress = 0;
    SetObjectScale(goinfo->size);
    // SetFaction(goinfo->faction);
    SetUInt32Value(GAMEOBJECT_FLAGS, goinfo->flags);
    SetPeriod(tInfo->pathTime);
    SetEntry(goinfo->id);
    SetDisplayId(goinfo->displayId);
    SetGoState(
        !goinfo->moTransport.canBeStopped ? GO_STATE_READY : GO_STATE_ACTIVE);
    SetGoType(GAMEOBJECT_TYPE_MO_TRANSPORT);
    SetGoAnimProgress(animprogress);
    SetName(goinfo->name);
    UpdateRotationFields(0.0f, 1.0f);

    if (!m_model)
        m_model = GameObjectModel::Create(*this);

    return true;
}

void Transport::CleanupsBeforeDelete()
{
    while (!_passengers.empty())
    {
        WorldObject* obj = *_passengers.begin();
        RemovePassenger(obj);
    }

    GameObject::CleanupsBeforeDelete();
}

void Transport::Update(uint32 diff, uint32)
{
    uint32 const positionUpdateDelay = 200;

    if (GetKeyFrames().size() <= 1)
        return;

    if (IsMoving() || !_pendingStop)
        m_goValue.Transport.PathProgress += diff;

    uint32 timer = m_goValue.Transport.PathProgress % GetPeriod();

    // Set current waypoint
    // Desired outcome: _currentFrame->DepartureTime < timer <
    // _nextFrame->ArriveTime
    // ... arrive | ... delay ... | departure
    //      event /         event /
    for (;;)
    {
        if (timer >= _currentFrame->ArriveTime)
        {
            if (!_triggeredArrivalEvent)
            {
                DoEventIfAny(*_currentFrame, false);
                _triggeredArrivalEvent = true;
            }

            if (timer < _currentFrame->DepartureTime)
            {
                SetMoving(false);
                if (_pendingStop && GetGoState() != GO_STATE_READY)
                {
                    SetGoState(GO_STATE_READY);
                    m_goValue.Transport.PathProgress =
                        (m_goValue.Transport.PathProgress / GetPeriod());
                    m_goValue.Transport.PathProgress *= GetPeriod();
                    m_goValue.Transport.PathProgress +=
                        _currentFrame->ArriveTime;
                }
                break; // its a stop frame and we are waiting
            }
        }

        if (timer >= _currentFrame->DepartureTime && !_triggeredDepartureEvent)
        {
            DoEventIfAny(*_currentFrame, true); // departure event
            _triggeredDepartureEvent = true;
        }

        // not waiting anymore
        SetMoving(true);

        // Enable movement
        if (GetGOInfo()->moTransport.canBeStopped)
            SetGoState(GO_STATE_ACTIVE);

        if (timer >= _currentFrame->DepartureTime &&
            timer < _currentFrame->NextArriveTime)
            break; // found current waypoint

        MoveToNextWaypoint();

        // LOG_DEBUG(logging,"Transport %u (%s) moved to node %u %u %f %f %f",
        // GetEntry(), GetName(), _currentFrame->Node->index,
        // _currentFrame->Node->mapid, _currentFrame->Node->x,
        // _currentFrame->Node->y, _currentFrame->Node->z);

        // Departure event
        if (_currentFrame->IsTeleportFrame())
            if (TeleportTransport(_nextFrame->Node->mapid, _nextFrame->Node->x,
                    _nextFrame->Node->y, _nextFrame->Node->z,
                    _nextFrame->InitialOrientation))
                return; // Update more in new map thread
    }

    // Set position
    _positionChangeTimer.Update(diff);
    if (_positionChangeTimer.Passed())
    {
        _positionChangeTimer.Reset(positionUpdateDelay);
        if (IsMoving())
        {
            float t = CalculateSegmentPos(float(timer) * 0.001f);
            G3D::Vector3 pos, dir;
            _currentFrame->Spline->evaluate_percent(
                _currentFrame->Index, t, pos);
            _currentFrame->Spline->evaluate_derivative(
                _currentFrame->Index, t, dir);
            UpdatePosition(
                pos.x, pos.y, pos.z, std::atan2(dir.y, dir.x) + float(M_PI));
        }
    }
}

void Transport::AddPassenger(WorldObject* passenger)
{
    if (!IsInWorld())
        return;

    if (_passengers.insert(passenger).second)
    {
        LOG_DEBUG(logging, "Object %s boarded transport %s.",
            passenger->GetName(), GetName());

        passenger->SetTransport(this);
        passenger->m_movementInfo.AddMovementFlag(MOVEFLAG_ONTRANSPORT);
        passenger->m_movementInfo.transport.guid = GetObjectGuid();

        if (passenger->GetTypeId() == TYPEID_PLAYER &&
            static_cast<Player*>(passenger)->GetPet())
        {
            Pet* pet = static_cast<Player*>(passenger)->GetPet();
            AddPassenger(pet);

            // Calculate and set position in object space
            Position tmp;
            pet->GetPosition(tmp.x, tmp.y, tmp.z);
            tmp.o = pet->GetO();
            CalculatePassengerOffset(tmp.x, tmp.y, tmp.z, &tmp.o);
            pet->m_movementInfo.transport.pos = tmp;
        }
    }
}

void Transport::RemovePassenger(WorldObject* passenger)
{
    bool erased = false;
    if (_passengerTeleportItr != _passengers.end())
    {
        auto itr = _passengers.find(passenger);
        if (itr != _passengers.end())
        {
            if (itr == _passengerTeleportItr)
                ++_passengerTeleportItr;

            _passengers.erase(itr);
            erased = true;
        }
    }
    else
        erased = _passengers.erase(passenger) > 0;

    if (erased)
    {
        LOG_DEBUG(logging, "Object %s removed from transport %s.",
            passenger->GetName(), GetName());

        passenger->SetTransport(nullptr);
        passenger->m_movementInfo.RemoveMovementFlag(MOVEFLAG_ONTRANSPORT);
        passenger->m_movementInfo.transport.Reset();

        if (passenger->GetTypeId() == TYPEID_PLAYER &&
            static_cast<Player*>(passenger)->GetPet())
        {
            Pet* pet = static_cast<Player*>(passenger)->GetPet();

            // Calculate position in world space
            Position pos;
            pet->m_movementInfo.transport.pos.Get(pos.x, pos.y, pos.z, pos.o);
            CalculatePassengerPosition(pos.x, pos.y, pos.z, &pos.o);

            RemovePassenger(pet);

            // Move to position in world space
            pet->Relocate(pos.x, pos.y, pos.z);
            pet->SetOrientation(pos.o);
        }
    }
}

void Transport::UpdatePosition(float x, float y, float z, float o)
{
    Relocate(x, y, z);
    SetOrientation(o);
    UpdatePassengerPositions(_passengers);
}

void Transport::EnableMovement(bool enabled)
{
    if (!GetGOInfo()->moTransport.canBeStopped)
        return;

    _pendingStop = !enabled;
}

bool Transport::IsInLineOfSight(
    const G3D::Vector3& p1, const G3D::Vector3& p2) const
{
    if (!m_model)
        return false;

    float max_dist = (p2 - p1).magnitude();

    if (!G3D::fuzzyGt(max_dist, 0))
        return true;

    G3D::Ray r(p1, (p2 - p1) / max_dist);

    return !m_model->intersectRay(r, max_dist, true);
}

float Transport::GetHeight(G3D::Vector3 pos) const
{
    if (!m_model)
        return INVALID_HEIGHT_VALUE;

    pos.z += 0.5f; // avoid imprecision errors

    G3D::Ray r(pos, G3D::Vector3(0, 0, -1));
    float max_dist = 50.0f;

    if (m_model->intersectRay(r, max_dist, true))
    {
        float height = pos.z - max_dist;
        return height;
    }

    return INVALID_HEIGHT_VALUE;
}

void Transport::MoveToNextWaypoint()
{
    // Clear events flagging
    _triggeredArrivalEvent = false;
    _triggeredDepartureEvent = false;

    // Set frames
    _currentFrame = _nextFrame++;
    if (_nextFrame == GetKeyFrames().end())
        _nextFrame = GetKeyFrames().begin();
}

float Transport::CalculateSegmentPos(float now)
{
    KeyFrame const& frame = *_currentFrame;
    const float speed = float(m_goInfo->moTransport.moveSpeed);
    const float accel = float(m_goInfo->moTransport.accelRate);
    float timeSinceStop =
        frame.TimeFrom + (now - (1.0f / IN_MILLISECONDS) * frame.DepartureTime);
    float timeUntilStop =
        frame.TimeTo - (now - (1.0f / IN_MILLISECONDS) * frame.DepartureTime);
    float segmentPos, dist;
    float accelTime = _transportInfo->accelTime;
    float accelDist = _transportInfo->accelDist;
    // calculate from nearest stop, less confusing calculation...
    if (timeSinceStop < timeUntilStop)
    {
        if (timeSinceStop < accelTime)
            dist = 0.5f * accel * timeSinceStop * timeSinceStop;
        else
            dist = accelDist + (timeSinceStop - accelTime) * speed;
        segmentPos = dist - frame.DistSinceStop;
    }
    else
    {
        if (timeUntilStop < _transportInfo->accelTime)
            dist = 0.5f * accel * timeUntilStop * timeUntilStop;
        else
            dist = accelDist + (timeUntilStop - accelTime) * speed;
        segmentPos = frame.DistUntilStop - dist;
    }

    return segmentPos / frame.NextDistFromPrev;
}

bool Transport::TeleportTransport(
    uint32 newMapid, float x, float y, float z, float o)
{
    Map const* oldMap = GetMap();

    // We need to always do a new world packet for transport teleports in TBC
    bool new_map = false;
    if (oldMap->GetId() != newMapid)
    {
        Map* newMap = sMapMgr::Instance()->CreateMap(newMapid, nullptr);
        GetMap()->erase(this, false);
        SetMap(newMap);
        new_map = true;
    }

    for (_passengerTeleportItr = _passengers.begin();
         _passengerTeleportItr != _passengers.end();)
    {
        WorldObject* obj = (*_passengerTeleportItr++);

        // remove auras preventing proper teleport
        if (obj->GetTypeId() == TYPEID_UNIT ||
            obj->GetTypeId() == TYPEID_PLAYER)
        {
            static_cast<Unit*>(obj)->remove_auras_if([](AuraHolder* holder)
                {
                    auto info = holder->GetSpellProto();
                    return info->HasApplyAuraName(SPELL_AURA_MOD_FEAR) ||
                           info->HasApplyAuraName(SPELL_AURA_MOD_CONFUSE) ||
                           info->HasApplyAuraName(SPELL_AURA_MOD_ROOT) ||
                           info->HasApplyAuraName(SPELL_AURA_MOD_STUN) ||
                           info->HasApplyAuraName(SPELL_AURA_MOD_CHARM) ||
                           info->HasApplyAuraName(SPELL_AURA_MOD_POSSESS) ||
                           info->HasApplyAuraName(SPELL_AURA_MOD_POSSESS_PET);
                });
        }

        float destX, destY, destZ, destO;
        obj->m_movementInfo.transport.pos.Get(destX, destY, destZ, destO);
        CalculatePassengerPosition(destX, destY, destZ, &destO, x, y, z, o);

        switch (obj->GetTypeId())
        {
        case TYPEID_PLAYER:
            if (!static_cast<Player*>(obj)->TeleportTo(newMapid, destX, destY,
                    destZ, destO, TELE_TO_NOT_LEAVE_TRANSPORT))
                RemovePassenger(obj);
            break;
        default:
            RemovePassenger(obj);
            break;
        }
    }

    Relocate(x, y, z);
    SetOrientation(o);
    if (new_map)
        GetMap()->insert(this);

    return true;
}

void Transport::UpdatePassengerPositions(PassengerSet passengers)
{
    for (auto passenger : passengers)
    {
        // transport teleported but passenger not yet (can happen for players)
        if (!passenger->IsInWorld() || passenger->GetMap() != GetMap())
            continue;

        // Do not use Unit::UpdatePosition here, we don't want to remove auras
        // as if regular movement occurred
        float x, y, z, o;
        passenger->m_movementInfo.transport.pos.Get(x, y, z, o);
        CalculatePassengerPosition(x, y, z, &o);
        switch (passenger->GetTypeId())
        {
        case TYPEID_UNIT:
        {
            Creature* creature = static_cast<Creature*>(passenger);
            if (creature->IsPet() && GetOwnerGuid().IsPlayer())
            {
                // For pets, make sure the owning player is in world
                Unit* owner = GetOwner();
                if (!owner || !owner->IsInWorld())
                    break;
            }
            GetMap()->relocate(creature, x, y, z, o);
            break;
        }
        case TYPEID_PLAYER:
            // NOTE: IsInWorld() checked above
            GetMap()->relocate(static_cast<Player*>(passenger), x, y, z, o);
            break;
        default:
            break;
        }
    }
}

void Transport::DoEventIfAny(KeyFrame const& node, bool departure)
{
    if (uint32 eventid =
            departure ? node.Node->departureEventID : node.Node->arrivalEventID)
    {
        LOG_DEBUG(logging, "Taxi %s event %u of node %u of %s \"%s\") path",
            departure ? "departure" : "arrival", eventid, node.Node->index,
            GetGuidStr().c_str(), GetName());

        if (!sScriptMgr::Instance()->OnProcessEvent(
                eventid, this, this, departure))
            GetMap()->ScriptsStart(sEventScripts, eventid, this, this);
    }
}

void Transport::BuildUpdateData(UpdateDataMapType& data_map)
{
    Map::PlayerList const& players = GetMap()->GetPlayers();
    if (players.isEmpty())
        return;

    for (const auto& player : players)
        BuildUpdateDataForPlayer(player.getSource(), data_map);

    ClearUpdateMask(true);
}

void Transport::CalculatePassengerPosition(float& x, float& y, float& z,
    float* o, float transX, float transY, float transZ, float transO)
{
    float inx = x, iny = y, inz = z;
    if (o)
        *o = Position::NormalizeOrientation(transO + *o);

    x = transX + inx * std::cos(transO) - iny * std::sin(transO);
    y = transY + iny * std::cos(transO) + inx * std::sin(transO);
    z = transZ + inz;
}

void Transport::CalculatePassengerOffset(float& x, float& y, float& z, float* o,
    float transX, float transY, float transZ, float transO)
{
    if (o)
        *o = Position::NormalizeOrientation(*o - transO);

    z -= transZ;
    y -= transY; // y = searchedY * std::cos(o) + searchedX * std::sin(o)
    x -= transX; // x = searchedX * std::cos(o) + searchedY * std::sin(o + pi)
    float inx = x, iny = y;
    y = (iny - inx * std::tan(transO)) /
        (std::cos(transO) + std::sin(transO) * std::tan(transO));
    x = (inx + iny * std::tan(transO)) /
        (std::cos(transO) + std::sin(transO) * std::tan(transO));
}
