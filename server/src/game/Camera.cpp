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

#include "Camera.h"
#include "DynamicObject.h"
#include "logging.h"
#include "ObjectAccessor.h"
#include "Pet.h"
#include "Player.h"
#include "SpecialVisCreature.h"
#include "Totem.h"
#include "TemporarySummon.h"
#include "Transport.h"
#include "maps/visitors.h"

Camera::Camera(Player* pl) : m_owner(*pl), m_source(pl)
{
    m_source->GetViewPoint().Attach(this);
}

Camera::~Camera()
{
    // view of camera should be already reseted to owner (RemoveFromWorld ->
    // Event_RemovedFromWorld -> ResetView)
    assert(m_source == &m_owner);

    // for symmetry with constructor and way to make viewpoint's list empty
    m_source->GetViewPoint().Detach(this);
}

void Camera::UpdateForCurrentViewPoint()
{
    if (!m_owner.IsInWorld())
        return;

    if (cell_.added)
        m_owner.GetMap()->get_map_grid().erase_camera(
            this, false, cell_.x, cell_.y);

    if (m_source->GetViewPoint().cell_.added)
    {
        cell_ = m_source->GetViewPoint().cell_;
        m_owner.GetMap()->get_map_grid().insert_camera(this, cell_.x, cell_.y);
    }

    UpdateVisibilityForOwner();
}

float Camera::GetDistance(WorldObject* obj) const
{
    return m_source->GetDistance(obj);
}

float Camera::GetDistance2d(WorldObject* obj) const
{
    return m_source->GetDistance2d(obj);
}

float Camera::GetX() const
{
    return m_source->GetX();
}

float Camera::GetY() const
{
    return m_source->GetY();
}

float Camera::GetZ() const
{
    return m_source->GetZ();
}

float Camera::GetObjectBoundingRadius() const
{
    return m_source->GetObjectBoundingRadius();
}

Map* Camera::GetMap() const
{
    return m_source->GetMap();
}

void Camera::SetView(WorldObject* obj, bool update_far_sight_field /*= true*/)
{
    assert(obj);

    if (m_source == obj || !m_source->IsInWorld())
        return;

    if (!m_owner.IsInMap(obj))
    {
        logging.error(
            "Camera::SetView, viewpoint is not in map with camera's owner");
        return;
    }

    if (!obj->isType(TypeMask(TYPEMASK_DYNAMICOBJECT | TYPEMASK_UNIT)))
    {
        logging.error(
            "Camera::SetView, viewpoint type is not available for client");
        return;
    }

    // detach and deregister from active objects if there are no more reasons to
    // be active
    m_source->GetViewPoint().Detach(this);
    if (!m_source->GetViewPoint().hasViewers() && !m_source->isActiveObject())
        m_source->GetMap()->remove_active_entity(m_source);

    m_source = obj;

    if (!m_source->isActiveObject())
        m_source->GetMap()->add_active_entity(m_source);

    m_source->GetViewPoint().Attach(this);

    if (update_far_sight_field)
        m_owner.SetGuidValue(PLAYER_FARSIGHT,
            (m_source == &m_owner ? ObjectGuid() : m_source->GetObjectGuid()));

    UpdateForCurrentViewPoint();
}

void Camera::Event_ViewPointVisibilityChanged()
{
    if (!m_owner.HaveAtClient(m_source))
        ResetView();
}

void Camera::ResetView(bool update_far_sight_field /*= true*/)
{
    SetView(&m_owner, update_far_sight_field);
}

void Camera::Event_AddedToWorld()
{
    assert(m_source->GetViewPoint().cell_.added);
    assert(!cell_.added);

    cell_ = m_source->GetViewPoint().cell_;
    m_owner.GetMap()->get_map_grid().insert_camera(this, cell_.x, cell_.y);

    UpdateVisibilityForOwner();
}

void Camera::Event_RemovedFromWorld()
{
    assert(cell_.added);

    if (m_source == &m_owner)
    {
        cell_.added = false;
        m_owner.GetMap()->get_map_grid().erase_camera(
            this, false, cell_.x, cell_.y);
        return;
    }

    ResetView();
}

void Camera::Event_Moved()
{
    assert(m_source->GetViewPoint().cell_.added);
    assert(cell_.added);

    float x = cell_.x;
    float y = cell_.y;
    cell_ = m_source->GetViewPoint().cell_;
    m_owner.GetMap()->get_map_grid().relocate_camera(
        this, x, y, cell_.x, cell_.y);
}

void Camera::UpdateVisibilityOf(WorldObject* target)
{
    if (m_owner.IsInWorld())
        m_owner.UpdateVisibilityOf(m_source, target);
}

template <class T>
void Camera::UpdateVisibilityOf(
    T* target, UpdateData& data, std::set<WorldObject*>& vis)
{
    if (m_owner.IsInWorld())
        m_owner.template UpdateVisibilityOf<T>(m_source, target, data, vis);
}

template void Camera::UpdateVisibilityOf(
    Player*, UpdateData&, std::set<WorldObject*>&);
template void Camera::UpdateVisibilityOf(
    Creature*, UpdateData&, std::set<WorldObject*>&);
template void Camera::UpdateVisibilityOf(
    Pet*, UpdateData&, std::set<WorldObject*>&);
template void Camera::UpdateVisibilityOf(
    Corpse*, UpdateData&, std::set<WorldObject*>&);
template void Camera::UpdateVisibilityOf(
    GameObject*, UpdateData&, std::set<WorldObject*>&);
template void Camera::UpdateVisibilityOf(
    DynamicObject*, UpdateData&, std::set<WorldObject*>&);
template void Camera::UpdateVisibilityOf(
    SpecialVisCreature*, UpdateData&, std::set<WorldObject*>&);
template void Camera::UpdateVisibilityOf(
    TemporarySummon*, UpdateData&, std::set<WorldObject*>&);
template void Camera::UpdateVisibilityOf(
    Totem*, UpdateData&, std::set<WorldObject*>&);

void Camera::UpdateVisibilityForOwner()
{
    UpdateData data;
    std::set<WorldObject*> visible_now;
    Player* player = GetOwner();
    auto client_guids = player->m_clientGUIDs;

    maps::visitors::simple<Player, Creature, Pet, Corpse, GameObject,
        DynamicObject, TemporarySummon, Totem>().visit_2d(m_source,
        player->GetMap()->GetVisibilityDistance(),
        [this, &data, &visible_now, &client_guids](auto&& obj) mutable
        {
            // XXX: Explicit this due to bug in GCC.
            this->UpdateVisibilityOf(obj, data, visible_now);
            client_guids.erase(obj->GetObjectGuid());
        });

    maps::visitors::simple<SpecialVisCreature>().visit_2d(m_source, 300.0f,
        [this, &data, &visible_now, &client_guids](auto&& obj) mutable
        {
            // XXX: Explicit this due to bug in GCC.
            this->UpdateVisibilityOf(obj, data, visible_now);
            client_guids.erase(obj->GetObjectGuid());
        });

    // at this moment client_guids have guids that not iterate at grid level
    // checks
    // but exist one case when this possible and object not out of range:
    // transports
    if (Transport* transport = player->GetTransport())
    {
        Transport::PassengerSet copy =
            transport->GetPassengers(); // content can change in
                                        // BeforeVisibilityDestroy
        for (const auto& elem : copy)
        {
            if (client_guids.find((elem)->GetObjectGuid()) !=
                client_guids.end())
            {
                client_guids.erase((elem)->GetObjectGuid());

                switch ((elem)->GetTypeId())
                {
                case TYPEID_GAMEOBJECT:
                    player->UpdateVisibilityOf(player,
                        static_cast<GameObject*>(elem), data, visible_now);
                    break;
                case TYPEID_PLAYER:
                    static_cast<Player*>(elem)->UpdateVisibilityOf(
                        elem, player);
                    player->UpdateVisibilityOf(
                        player, static_cast<Player*>(elem), data, visible_now);
                    break;
                case TYPEID_UNIT:
                    player->UpdateVisibilityOf(player,
                        static_cast<Creature*>(elem), data, visible_now);
                    break;
                case TYPEID_DYNAMICOBJECT:
                    player->UpdateVisibilityOf(player,
                        static_cast<DynamicObject*>(elem), data, visible_now);
                    break;
                default:
                    break;
                }
            }
        }
    }

    // generate outOfRange for not iterate objects
    data.AddOutOfRangeGUID(client_guids);
    for (auto itr = client_guids.begin(); itr != client_guids.end(); ++itr)
    {
        player->m_clientGUIDs.erase(*itr);

        LOG_DEBUG(logging,
            "%s is out of range (no in active cells set) now for %s",
            itr->GetString().c_str(), player->GetGuidStr().c_str());
    }

    if (data.HasData())
    {
        // send create/outofrange packet to player (except player create updates
        // that already sent using SendUpdateToPlayer)
        data.SendPacket(player->GetSession());

        // send out of range to other players if need
        ObjectGuidSet const& oor = data.GetOutOfRangeGUIDs();
        for (const auto& elem : oor)
        {
            if (!elem.IsPlayer())
                continue;

            if (Player* plr = ObjectAccessor::FindPlayer(elem))
                plr->UpdateVisibilityOf(plr->GetCamera().GetBody(), player);
        }
    }

    // Now do operations that required done at object visibility change to
    // visible

    // send data at target visibility change (adding to client)
    for (const auto& elem : visible_now)
    {
        // target aura duration for caster show only if target exist at caster
        // client
        if ((elem) != player && (elem)->isType(TYPEMASK_UNIT))
            player->SendAuraDurationsForTarget((Unit*)(elem));
    }
}

//////////////////

ViewPoint::~ViewPoint()
{
    if (!m_cameras.empty())
    {
        logging.error(
            "ViewPoint destructor called, but some cameras referenced to it");
        assert(false);
    }
}
