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

#include "ObjectAccessor.h"
#include "Corpse.h"
#include "Creature.h"
#include "DynamicObject.h"
#include "GameObject.h"
#include "Item.h"
#include "Map.h"
#include "MapManager.h"
#include "ObjectGuid.h"
#include "ObjectMgr.h"
#include "Pet.h"
#include "Player.h"
#include "World.h"
#include "WorldPacket.h"
#include "Policies/Singleton.h"
#include <cmath>

ObjectAccessor::ObjectAccessor() : alliance_online(0), horde_online(0)
{
}

ObjectAccessor::~ObjectAccessor()
{
    for (Player2CorpsesMapType::const_iterator itr = i_player2corpse.begin();
         itr != i_player2corpse.end();)
    {
        auto cur = itr;
        ++itr;
        auto resources = cur->second;
        resources->RemoveFromWorld();
        delete resources;
    }
}

Unit* ObjectAccessor::GetUnit(WorldObject const& u, ObjectGuid guid)
{
    if (!guid)
        return nullptr;

    if (guid.IsPlayer())
        return FindPlayer(guid);

    if (!u.IsInWorld())
        return nullptr;

    return u.GetMap()->GetAnyTypeCreature(guid);
}

Corpse* ObjectAccessor::GetCorpseInMap(ObjectGuid guid, uint32 mapid)
{
    Corpse* ret = HashMapHolder<Corpse>::Find(guid);

    if (!ret)
        return nullptr;

    if (ret->GetMapId() != mapid)
        return nullptr;

    return ret;
}

Player* ObjectAccessor::FindPlayer(ObjectGuid guid, bool inWorld /*= true*/)
{
    if (!guid)
        return nullptr;

    Player* plr = HashMapHolder<Player>::Find(guid);
    if (!plr || (!plr->IsInWorld() && inWorld))
        return nullptr;

    return plr;
}

Player* ObjectAccessor::FindPlayerByName(const std::string& name, bool inWorld)
{
    return sObjectAccessor::Instance()->player_by_name(name, inWorld);
}

Player* ObjectAccessor::player_by_name(
    const std::string& name, bool in_world) const
{
    std::lock_guard<std::mutex> guard(name_mutex_);
    auto itr = player_name_map_.find(name);
    if (itr != player_name_map_.end())
    {
        if (!in_world || itr->second->IsInWorld())
            return itr->second;
    }
    return nullptr;
}

void ObjectAccessor::SaveAllPlayers()
{
    std::vector<Player*> players;

    {
        HashMapHolder<Player>::LockedContainer cont = GetPlayers();
        players.reserve(cont.get().size());
        for (auto& elem : cont.get())
            players.push_back(elem.second);
    }

    for (auto p : players)
        p->SaveToDB();
}

void ObjectAccessor::UpdateAllianceHordeCount()
{
    LoginDatabase.PExecute(
        "UPDATE realmlist SET alliance_online=%u, horde_online=%u WHERE id=%u",
        alliance_online, horde_online, realmID);
}

void ObjectAccessor::KickPlayer(ObjectGuid guid)
{
    if (Player* p = ObjectAccessor::FindPlayer(guid, false))
    {
        WorldSession* s = p->GetSession();
        s->KickPlayer(); // mark session to remove at next session list update
        s->LogoutPlayer(
            false); // logout player without waiting next session list update
    }
}

Corpse* ObjectAccessor::GetCorpseForPlayerGUID(ObjectGuid guid)
{
    std::lock_guard<std::mutex> g(corpse_mutex_);

    auto iter = i_player2corpse.find(guid);
    if (iter == i_player2corpse.end())
        return nullptr;

    assert(iter->second->GetType() != CORPSE_BONES);

    return iter->second;
}

void ObjectAccessor::RemoveCorpse(Corpse* corpse)
{
    assert(corpse && corpse->GetType() != CORPSE_BONES);

    std::lock_guard<std::mutex> g(corpse_mutex_);
    auto iter = i_player2corpse.find(corpse->GetOwnerGuid());
    if (iter == i_player2corpse.end())
        return;

    sObjectMgr::Instance()->remove_static_corpse(corpse);

    i_player2corpse.erase(iter);
}

void ObjectAccessor::AddCorpse(Corpse* corpse)
{
    assert(corpse && corpse->GetType() != CORPSE_BONES);

    std::lock_guard<std::mutex> g(corpse_mutex_);
    assert(
        i_player2corpse.find(corpse->GetOwnerGuid()) == i_player2corpse.end());
    i_player2corpse[corpse->GetOwnerGuid()] = corpse;

    // Don't add static corpse for battleground maps
    auto map_entry = sMapStore.LookupEntry(corpse->GetMapId());
    if (!map_entry || map_entry->IsBattleGroundOrArena())
        return;

    sObjectMgr::Instance()->add_static_corpse(corpse);
}

Corpse* ObjectAccessor::ConvertCorpseForPlayer(
    ObjectGuid player_guid, bool insignia)
{
    Corpse* corpse = GetCorpseForPlayerGUID(player_guid);
    if (!corpse)
        return nullptr;

    // Cannot process while corpse is being inserted into map
    if (corpse->pending_map_insert)
        return nullptr;

    LOG_DEBUG(logging, "Deleting Corpse and spawning bones.");

    // remove corpse from player_guid -> corpse map
    RemoveCorpse(corpse);

    // remove resurrectable corpse from grid object registry (loaded state
    // checked into call)
    // do not load the map if it's not loaded
    Map* map = sMapMgr::Instance()->FindMap(
        corpse->GetMapId(), corpse->GetInstanceId());

    // remove corpse from DB
    corpse->DeleteFromDB();

    Corpse* bones = nullptr;
    // create the bones only if the map and the grid is loaded at the corpse's
    // location
    // ignore bones creating option in case insignia
    if (map && (insignia || (map->IsBattleGroundOrArena() ?
                                    sWorld::Instance()->getConfig(
                                        CONFIG_BOOL_DEATH_BONES_BG_OR_ARENA) :
                                    sWorld::Instance()->getConfig(
                                        CONFIG_BOOL_DEATH_BONES_WORLD))))
    {
        // Create bones, don't change Corpse
        bones = new Corpse;
        bones->Create(corpse->GetGUIDLow());

        for (int i = 3; i < CORPSE_END;
             ++i) // don't overwrite guid and object type
            bones->SetUInt32Value(i, corpse->GetUInt32Value(i));

        bones->SetTime(WorldTimer::time_no_syscall());
        bones->SetType(CORPSE_BONES);
        bones->Relocate(corpse->GetX(), corpse->GetY(), corpse->GetZ());
        bones->SetOrientation(corpse->GetO());

        bones->SetUInt32Value(
            CORPSE_FIELD_FLAGS, CORPSE_FLAG_UNK2 | CORPSE_FLAG_BONES);
        bones->SetGuidValue(CORPSE_FIELD_OWNER, corpse->GetOwnerGuid());

        for (int i = inventory::equipment_start; i < inventory::equipment_end;
             ++i)
        {
            if (corpse->GetUInt32Value(CORPSE_FIELD_ITEM + i))
                bones->SetUInt32Value(CORPSE_FIELD_ITEM + i, 0);
        }

        // For insignia removal we must make the bones lootable
        if (insignia)
        {
            bones->SetFlag(CORPSE_FIELD_DYNAMIC_FLAGS, CORPSE_DYNFLAG_LOOTABLE);

            // We store the level of our player in the gold field
            // We retrieve this information at Player::SendLoot()
            if (auto player = ObjectAccessor::FindPlayer(player_guid, false))
                bones->SetOwnerLevel(player->getLevel());
            else
                bones->SetOwnerLevel(1);
        }
    }

    // Get rid of corpse
    if (map && corpse->IsInWorld())
        map->erase(corpse, true);
    else
        delete corpse;

    // Insert bones after corpse erased
    if (bones)
        map->insert(bones);

    return bones;
}

void ObjectAccessor::RemoveOldCorpses()
{
    time_t now = WorldTimer::time_no_syscall();
    Player2CorpsesMapType::iterator next;
    for (auto itr = i_player2corpse.begin(); itr != i_player2corpse.end();
         itr = next)
    {
        next = itr;
        ++next;

        if (!itr->second->IsExpired(now))
            continue;

        ConvertCorpseForPlayer(itr->first);
    }
}

void ObjectAccessor::add_player(Player* plr)
{
    HashMapHolder<Player>::Insert(plr);

    std::lock_guard<std::mutex> guard(name_mutex_);
    player_name_map_.insert(
        std::pair<std::string, Player*>(plr->GetName(), plr));
}

void ObjectAccessor::remove_player(Player* plr)
{
    HashMapHolder<Player>::Remove(plr);

    std::lock_guard<std::mutex> guard(name_mutex_);
    player_name_map_.erase(plr->GetName());
}

// IsInWorld() must be false before calling this
void ObjectAccessor::OnPlayerAddToWorld(Team team)
{
    if (team == ALLIANCE)
        alliance_online += 1;
    else if (team == HORDE)
        horde_online += 1;
}

// IsInWorld() must be true before calling this
void ObjectAccessor::OnPlayerRemoveFromWorld(Team team)
{
    if (team == ALLIANCE && alliance_online > 0)
        alliance_online -= 1;
    else if (team == HORDE && horde_online > 0)
        horde_online -= 1;
}

uint32 ObjectAccessor::GetPlayerCount()
{
    return HashMapHolder<Player>::GetSize();
}

/// Define the static members of HashMapHolder
template <class T>
typename HashMapHolder<T>::MapType HashMapHolder<T>::object_map_;
template <class T>
std::mutex HashMapHolder<T>::mutex_;
template <class T>
uint32 HashMapHolder<T>::size_;

/// Global definitions for the different hashmap storages
template class HashMapHolder<Player>;
template class HashMapHolder<Corpse>;
