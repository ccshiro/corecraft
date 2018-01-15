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

#ifndef MANGOSSERVER_CAMERA_H
#define MANGOSSERVER_CAMERA_H

#include "Common.h"
#include "maps/map_grid.h"

class Player;
class UpdateData;
class ViewPoint;
class WorldObject;
class WorldPacket;

// Keep track of if camera/viewpoint is added to a cell, and which cell
struct camera_cell_ref
{
    int x = 0;
    int y = 0;
    bool added = false;
};

/// Camera - object-receiver. Receives broadcast packets from nearby
/// worldobjects, object visibility changes and sends them to client
class MANGOS_DLL_SPEC Camera
{
    friend class ViewPoint;

public:
    explicit Camera(Player* pl);
    ~Camera();

    WorldObject* GetBody() { return m_source; }
    Player* GetOwner() { return &m_owner; }

    float GetDistance(WorldObject* obj) const;
    float GetDistance2d(WorldObject* obj) const;
    float GetX() const;
    float GetY() const;
    float GetZ() const;
    float GetObjectBoundingRadius() const;
    Map* GetMap() const;

    // set camera's view to any worldobject
    // Note: this worldobject must be in same map, in same phase with camera's
    // owner(player)
    // client supports only unit and dynamic objects as farsight objects
    void SetView(WorldObject* obj, bool update_far_sight_field = true);

    // set view to camera's owner
    void ResetView(bool update_far_sight_field = true);

    template <class T>
    void UpdateVisibilityOf(T* obj, UpdateData& d, std::set<WorldObject*>& vis);
    void UpdateVisibilityOf(WorldObject* obj);

    // updates visibility of worldobjects around viewpoint for camera's owner
    void UpdateVisibilityForOwner();

    int CellX() const { return cell_.x; }
    int CellY() const { return cell_.y; }

private:
    // called when viewpoint changes visibility state
    void Event_AddedToWorld();
    void Event_RemovedFromWorld();
    void Event_Moved();
    void Event_ViewPointVisibilityChanged();

    Player& m_owner;
    WorldObject* m_source;

    camera_cell_ref cell_;

    void UpdateForCurrentViewPoint();
};

/// Object-observer, notifies farsight object state to cameras that attached to
/// it
class MANGOS_DLL_SPEC ViewPoint
{
    friend class Camera;

    typedef std::list<Camera*> CameraList;

    CameraList m_cameras;
    camera_cell_ref cell_;

    void Attach(Camera* c) { m_cameras.push_back(c); }
    void Detach(Camera* c) { m_cameras.remove(c); }

    void CameraCall(void (Camera::*handler)())
    {
        if (!m_cameras.empty())
        {
            for (auto itr = m_cameras.begin(); itr != m_cameras.end();)
            {
                Camera* c = *(itr++);
                (c->*handler)();
            }
        }
    }

public:
    ~ViewPoint();

    bool hasViewers() const { return !m_cameras.empty(); }

    // these events are called when viewpoint changes visibility state
    // x and y: cell coordinates
    void Event_AddedToWorld(int x, int y)
    {
        cell_.x = x;
        cell_.y = y;
        cell_.added = true;
        CameraCall(&Camera::Event_AddedToWorld);
    }

    void Event_RemovedFromWorld()
    {
        cell_.added = false;
        CameraCall(&Camera::Event_RemovedFromWorld);
    }

    void Event_CellChanged(int x, int y)
    {
        cell_.x = x;
        cell_.y = y;
        CameraCall(&Camera::Event_Moved);
    }

    void Event_ViewPointVisibilityChanged()
    {
        CameraCall(&Camera::Event_ViewPointVisibilityChanged);
    }

    void Call_UpdateVisibilityForOwner()
    {
        CameraCall(&Camera::UpdateVisibilityForOwner);
    }
};

#endif
