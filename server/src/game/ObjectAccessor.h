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

#ifndef MANGOS_OBJECTACCESSOR_H
#define MANGOS_OBJECTACCESSOR_H

#include "Common.h"
#include "Corpse.h"
#include "Object.h"
#include "Player.h"
#include "UpdateData.h"
#include "Platform/Define.h"
#include "Policies/Singleton.h"
#include "maps/map_grid.h"
#include <boost/thread.hpp>
#include <list>
#include <set>
#include <string>
#include <unordered_map>

class Map;
class Unit;
class WorldObject;

template <class T>
class HashMapHolder
{
public:
    typedef std::unordered_map<ObjectGuid, T*> MapType;

    static void Insert(T* o)
    {
        std::lock_guard<std::mutex> guard(mutex_);
        object_map_[o->GetObjectGuid()] = o;
        ++size_;
    }

    static void Remove(T* o)
    {
        std::lock_guard<std::mutex> guard(mutex_);
        object_map_.erase(o->GetObjectGuid());
        if (size_ > 0)
            --size_;
    }

    static T* Find(ObjectGuid guid)
    {
        std::lock_guard<std::mutex> guard(mutex_);
        auto itr = object_map_.find(guid);
        return (itr != object_map_.end()) ? itr->second : nullptr;
    }

    class LockedContainer
    {
        std::shared_ptr<std::lock_guard<std::mutex>> guard_;
        MapType& m_;

    public:
        LockedContainer(MapType& m, std::mutex& mutex)
          : guard_(new std::lock_guard<std::mutex>(mutex)), m_(m)
        {
        }
        MapType& get() { return m_; }
    };
    static LockedContainer GetLockedContainer()
    {
        return LockedContainer(object_map_, mutex_);
    }

    static uint32 GetSize()
    {
        std::lock_guard<std::mutex> guard(mutex_);
        return size_;
    }

private:
    // Non instanceable only static
    HashMapHolder() {}

    static std::mutex mutex_;
    static MapType object_map_;
    static uint32 size_;
};

class MANGOS_DLL_DECL ObjectAccessor
{
    friend class MaNGOS::UnlockedSingleton<ObjectAccessor>;

    ObjectAccessor();
    ObjectAccessor(const ObjectAccessor&);
    ObjectAccessor& operator=(const ObjectAccessor&);

public:
    ~ObjectAccessor();

    typedef std::unordered_map<ObjectGuid, Corpse*> Player2CorpsesMapType;

    // Search player at any map in world and other objects at same map with
    // `obj`
    // Note: recommended use Map::GetUnit version if player also expected at
    // same map only
    static Unit* GetUnit(WorldObject const& obj, ObjectGuid guid);

    // Player access
    static Player* FindPlayer(ObjectGuid guid,
        bool inWorld =
            true); // if need player at specific map better use Map::GetPlayer
    static Player* FindPlayerByName(
        const std::string& name, bool inWorld = true);
    static void KickPlayer(ObjectGuid guid);

    Player* player_by_name(const std::string& name, bool in_world) const;

    HashMapHolder<Player>::LockedContainer GetPlayers()
    {
        return HashMapHolder<Player>::GetLockedContainer();
    }

    void SaveAllPlayers();
    void UpdateAllianceHordeCount();

    // Corpse access
    Corpse* GetCorpseForPlayerGUID(ObjectGuid guid);
    Corpse* GetCorpseInMap(ObjectGuid guid, uint32 mapid);
    void RemoveCorpse(Corpse* corpse);
    void AddCorpse(Corpse* corpse);
    Corpse* ConvertCorpseForPlayer(
        ObjectGuid player_guid, bool insignia = false);
    void RemoveOldCorpses();

    // Call when object is added to world
    static void AddObject(Player* object);
    static void RemoveObject(Player* object);
    static void AddObject(Corpse* object);
    static void RemoveObject(Corpse* object);
    // Specialization for player
    void add_player(Player* plr);
    void remove_player(Player* plr);

    // Player count & Alliance / Horde count
    static uint32 GetPlayerCount();
    // IsInWorld() must be false before calling this
    void OnPlayerAddToWorld(Team team);
    // IsInWorld() must be true before calling this
    void OnPlayerRemoveFromWorld(Team team);

    uint32 alliance_online;
    uint32 horde_online;

private:
    Player2CorpsesMapType i_player2corpse;
    std::mutex corpse_mutex_;

    mutable std::mutex name_mutex_;
    typedef std::unordered_map<std::string, Player*> player_name_map;
    player_name_map player_name_map_;
};

#define sObjectAccessor MaNGOS::UnlockedSingleton<ObjectAccessor>

inline void ObjectAccessor::AddObject(Player* object)
{
    sObjectAccessor::Instance()->add_player(object);
}

inline void ObjectAccessor::RemoveObject(Player* object)
{
    sObjectAccessor::Instance()->remove_player(object);
}

inline void ObjectAccessor::AddObject(Corpse* object)
{
    HashMapHolder<Corpse>::Insert(object);
}

inline void ObjectAccessor::RemoveObject(Corpse* object)
{
    HashMapHolder<Corpse>::Remove(object);
}

#endif
