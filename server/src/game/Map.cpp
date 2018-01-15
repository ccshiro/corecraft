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

#include "Map.h"
#include "BattleGroundMgr.h"
#include "DBCEnums.h"
#include "DynamicObject.h"
#include "Group.h"
#include "InstanceData.h"
#include "logging.h"
#include "MapManager.h"
#include "MapPersistentStateMgr.h"
#include "MapRefManager.h"
#include "MoveMap.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "SpecialVisCreature.h"
#include "Totem.h"
#include "TemporarySummon.h"
#include "Transport.h"
#include "VMapFactory.h"
#include "World.h"
#include "concurrent_pathgen.h"
#include "loot_distributor.h"
#include "profiling/map_updates.h"
#include "maps/callbacks.h"
#include "maps/visitors.h"
#include "vmap/DynamicTree.h"
#include "vmap/GameObjectModel.h"

#ifdef PERF_SAMPLING_MAP_UPDATE
namespace profiling
{
extern map_update map_update_;
}
#define PROFILE_BLOCK_START(id) \
    profiling::map_update_.block_time_start(GetId(), id)
#define PROFILE_BLOCK_END(id) \
    profiling::map_update_.block_time_stop(GetId(), id)
#else
#define PROFILE_BLOCK_START(id)
#define PROFILE_BLOCK_END(id)
#endif

Map::Map(uint32 id, uint32 InstanceId, uint8 SpawnMode)
  : grid_{new maps::map_grid(this)}, i_mapEntry(sMapStore.LookupEntry(id)),
    m_clearActiveCombatObjs(0), i_spawnMode(SpawnMode), i_id(id),
    i_InstanceId(InstanceId), m_unloadTimer(0),
    m_VisibleDistance(DEFAULT_VISIBILITY_DISTANCE),
    m_SpecialVisDistance(DEFAULT_SPECIALVIS_DISTANCE),
    m_persistentState(nullptr), destroyed_{false},
    m_TerrainData(sTerrainMgr::Instance()->LoadTerrain(id)), i_data(nullptr),
    i_script_id(0), m_creatureGrpMgr(this)
{
    m_dyn_tree = new DynamicMapTree;

    m_CreatureGuids.Set(
        sObjectMgr::Instance()->GetFirstTemporaryCreatureLowGuid());
    m_GameObjectGuids.Set(
        sObjectMgr::Instance()->GetFirstTemporaryGameObjectLowGuid());

    // lets initialize visibility distance for map
    Map::InitVisibilityDistance();

    // add reference for TerrainData object
    m_TerrainData->AddRef();

    m_persistentState = sMapPersistentStateMgr::Instance()->AddPersistentState(
        i_mapEntry, GetInstanceId(), GetDifficulty(), 0, IsDungeon());
    m_persistentState->SetUsedByMapState(this);

    // Note 0 is an invalid GUID, 1 is valid but only for players
    creature_map_.set_empty_key(ObjectGuid(0));
    creature_map_.set_deleted_key(ObjectGuid(1));
    pet_map_.set_empty_key(ObjectGuid(0));
    pet_map_.set_deleted_key(ObjectGuid(1));
    go_map_.set_empty_key(ObjectGuid(0));
    go_map_.set_deleted_key(ObjectGuid(1));
    do_map_.set_empty_key(ObjectGuid(0));
    do_map_.set_deleted_key(ObjectGuid(1));

    // TODO: Make this more general
    if (id == 530)
    {
        // Cells that must start loaded in Outland (dark portal event)
        grid_->load_cell(252, 272);
        grid_->load_cell(253, 270);
        grid_->load_cell(250, 270);
        grid_->load_cell(253, 281);
    }

    grid_->spawn_elevators();
}

Map::~Map()
{
    if (!destroyed_)
        UnloadAll();

    if (!m_scriptSchedule.empty())
        sScriptMgr::Instance()->DecreaseScheduledScriptCount(
            m_scriptSchedule.size());

    if (m_persistentState)
        m_persistentState->SetUsedByMapState(
            nullptr); // field pointer can be deleted after this

    if (i_data)
    {
        delete i_data;
        i_data = nullptr;
    }

    // unload instance specific navigation data
    MMAP::MMapFactory::createOrGetMMapManager()->unloadMapInstance(
        m_TerrainData->GetMapId(), GetInstanceId());

    delete m_dyn_tree;
}

void Map::UnloadAll()
{
    destroyed_ = true;
    RemoveAllObjectsInRemoveList();
    grid_->destroy();
}

void Map::LoadMapAndVMap(int x, int y)
{
    m_TerrainData->Load(x, y);
}

void Map::InitVisibilityDistance()
{
    // init visibility for continents
    m_VisibleDistance = World::GetMaxVisibleDistanceOnContinents();
}

template <>
void Map::add_to_hash_lookup(Creature* c)
{
    creature_map_[c->GetObjectGuid()] = c;
}

template <>
void Map::add_to_hash_lookup(SpecialVisCreature* c)
{
    creature_map_[c->GetObjectGuid()] = c;
}

template <>
void Map::add_to_hash_lookup(TemporarySummon* c)
{
    creature_map_[c->GetObjectGuid()] = c;
}

template <>
void Map::add_to_hash_lookup(Totem* c)
{
    creature_map_[c->GetObjectGuid()] = c;
}

template <>
void Map::add_to_hash_lookup(Pet* p)
{
    pet_map_[p->GetObjectGuid()] = p;
}

template <>
void Map::add_to_hash_lookup(GameObject* go)
{
    go_map_[go->GetObjectGuid()] = go;
}

template <>
void Map::add_to_hash_lookup(DynamicObject* o)
{
    do_map_[o->GetObjectGuid()] = o;
}

template <>
void Map::remove_from_hash_lookup(Creature* c)
{
    creature_map_.erase(c->GetObjectGuid());
}

template <>
void Map::remove_from_hash_lookup(SpecialVisCreature* c)
{
    creature_map_.erase(c->GetObjectGuid());
}

template <>
void Map::remove_from_hash_lookup(TemporarySummon* c)
{
    creature_map_.erase(c->GetObjectGuid());
}

template <>
void Map::remove_from_hash_lookup(Totem* c)
{
    creature_map_.erase(c->GetObjectGuid());
}

template <>
void Map::remove_from_hash_lookup(Pet* p)
{
    pet_map_.erase(p->GetObjectGuid());
}

template <>
void Map::remove_from_hash_lookup(GameObject* go)
{
    go_map_.erase(go->GetObjectGuid());
}

template <>
void Map::remove_from_hash_lookup(DynamicObject* o)
{
    do_map_.erase(o->GetObjectGuid());
}

bool Map::insert(WorldObject* obj)
{
    assert(!obj->IsInWorld());
    assert(maps::verify_coords(obj->GetX(), obj->GetY()));

    // This condition can happen, and there's not much we can do about it. When
    // a mob is removed or a spell is, that might, in the game logic, trigger
    // the addition of a new world entity.
    // NOTE: Only affects creatures, not players.
    if (destroyed_)
        return false;

    auto p = maps::coords_to_cell_pair(obj->GetX(), obj->GetY());
    obj->GetViewPoint().Event_AddedToWorld(p.first, p.second);

    grid_->insert(obj);

    return true;
}

template <typename T>
void Map::inserted_callback(T* obj)
{
    add_to_hash_lookup(obj);

    obj->SetMap(this);
    obj->UpdateZoneAreaCache();
    obj->AddToWorld();

    UpdateObjectVisibility(obj);
}

template void Map::inserted_callback(Corpse*);
template void Map::inserted_callback(Creature*);
template void Map::inserted_callback(DynamicObject*);
template void Map::inserted_callback(GameObject*);
template void Map::inserted_callback(Pet*);
template void Map::inserted_callback(SpecialVisCreature*);
template void Map::inserted_callback(Totem*);
template void Map::inserted_callback(TemporarySummon*);

bool Map::insert(Transport* transport)
{
    assert(!transport->IsInWorld());
    assert(maps::verify_coords(transport->GetX(), transport->GetY()));

    add_to_hash_lookup(transport);
    m_transports.insert(transport);
    transport->AddToWorld();

    // Broadcast creation to players
    for (const auto& elem : GetPlayers())
    {
        if (elem.getSource()->GetTransport() != transport)
        {
            UpdateData data;
            transport->BuildCreateUpdateBlockForPlayer(&data, elem.getSource());
            data.SendPacket(elem.getSource()->GetSession(), true);
        }
    }

    return true;
}

bool Map::insert(Player* player)
{
    assert(!player->IsInWorld());
    assert(maps::verify_coords(player->GetX(), player->GetY()));
    assert(!destroyed_);

    // We need to send this data now rather than in the callback. The client
    // expects a certain ordering of packets that's not fulfilled othwerise.
    player->AddToWorld();
    SendInitSelf(player);
    SendInitTransports(player);

    // Bind player to map right away, to make sure the map can't go away
    player->GetMapRef().link(this, player);

    // Initiate moving camera as well
    auto p = maps::coords_to_cell_pair(player->GetX(), player->GetY());
    player->GetViewPoint().Event_AddedToWorld(p.first, p.second);

    grid_->insert(player);

    return true;
}

void Map::relog(Player* player)
{
    assert(player->IsInWorld() && player->GetMap() == this);

    player->m_clientGUIDs.clear();
    player->AddToWorld();
    SendInitSelf(player);
    SendInitTransports(player);

    player->GetCamera().UpdateVisibilityForOwner();
}

void Map::inserted_callback(Player* player)
{
    add_to_hash_lookup(player);

    player->SetMap(this);
    player->UpdateZoneAreaCache();

    UpdateObjectVisibility(player);

    if (i_data)
        i_data->OnPlayerEnter(player);
}

template <typename T>
void Map::erase(T* t, bool destroy)
{
    assert(t->IsInWorld());
    assert(maps::verify_coords(t->GetX(), t->GetY()));

    t->RemoveFromWorld();
    remove_from_hash_lookup(t);

    grid_->erase(t, destroy);
}

template void Map::erase(Corpse*, bool);
template void Map::erase(Creature*, bool);
template void Map::erase(DynamicObject*, bool);
template void Map::erase(GameObject*, bool);
template void Map::erase(Pet*, bool);
template void Map::erase(SpecialVisCreature*, bool);
template void Map::erase(Totem*, bool);
template void Map::erase(TemporarySummon*, bool);

template <typename T>
void Map::erased_callback(T* obj, bool destroy)
{
    if (destroy)
        obj->CleanupsBeforeDelete();

    UpdateObjectVisibility(obj);
    obj->ResetMap();
}

template void Map::erased_callback(Corpse*, bool);
template void Map::erased_callback(Creature*, bool);
template void Map::erased_callback(DynamicObject*, bool);
template void Map::erased_callback(GameObject*, bool);
template void Map::erased_callback(Pet*, bool);
template void Map::erased_callback(SpecialVisCreature*, bool);
template void Map::erased_callback(Totem*, bool);
template void Map::erased_callback(TemporarySummon*, bool);

void Map::erase(Transport* obj, bool /*remove*/)
{
    obj->RemoveFromWorld();
    remove_from_hash_lookup(obj);

    Map::PlayerList const& players = GetPlayers();
    for (const auto& player : players)
        if (player.getSource()->GetTransport() != obj)
        {
            UpdateData data;
            obj->BuildOutOfRangeUpdateBlock(&data);
            data.SendPacket(player.getSource()->GetSession(), true);
        }

    m_transports.erase(obj);

    obj->ResetMap();
}

void Map::erase(Player* player, bool destroy)
{
    assert(player->IsInWorld());
    assert(maps::verify_coords(player->GetX(), player->GetY()));

    player->RemoveFromWorld();
    remove_from_hash_lookup(player);

    grid_->erase(player, destroy);
}

void Map::erased_callback(Player* player, bool destroy)
{
    if (i_data)
        i_data->OnPlayerLeave(player);

    // Schedule clearing all active creature objects; we cannot do it right
    // away as they need time to reset properly
    if (m_mapRefManager.getSize() == 1)
        m_clearActiveCombatObjs = 10 * IN_MILLISECONDS;

    if (destroy)
        player->CleanupsBeforeDelete();
    else
        player->RemoveFromWorld();

    SendRemoveTransports(player);
    UpdateObjectVisibility(player);

    // this may be called during Map::Update
    // after decrement+unlink, ++m_mapRefIter will continue correctly
    // when the first element of the list is being removed
    // nocheck_prev will return the padding element of the RefManager
    // instead of NULL in the case of prev
    if (m_mapRefIter == player->GetMapRef())
        m_mapRefIter = m_mapRefIter->nocheck_prev();
    player->GetMapRef().unlink();

    player->ResetMap();

    if (destroy)
        sObjectAccessor::Instance()->RemoveObject(player);
}

void Map::add_active_entity(WorldObject* object)
{
    assert(object->GetTypeId() != TYPEID_PLAYER);

    grid_->add_active(object);
}

void Map::remove_active_entity(WorldObject* object)
{
    assert(object->GetTypeId() != TYPEID_PLAYER);

    grid_->remove_active(object);
}

void Map::relocate(Creature* creature, float x, float y, float z, float o)
{
    assert(maps::verify_coords(x, y));
    assert(creature->GetMap() == this);

    grid_->relocate(creature, x, y);

    creature->Relocate(x, y, z);
    creature->SetOrientation(o);

    creature->OnRelocated();
    creature->OnMapCreatureRelocation();
}

void Map::relocate(Player* player, float x, float y, float z, float o)
{
    assert(maps::verify_coords(x, y));
    assert(player->GetMap() == this);

    grid_->relocate(player, x, y);

    player->Relocate(x, y, z);
    player->SetOrientation(o);

    player->OnRelocated();
}

void Map::broadcast_message(WorldObject* src, WorldPacket* msg,
    bool include_src, float dist, bool my_team_only, bool check3d,
    const Player* ignore)
{
    if (dist == 0)
    {
        if (unlikely(src->GetTypeId() == TYPEID_UNIT &&
                     static_cast<Creature*>(src)->special_vis_mob()))
            dist = static_cast<Creature*>(src)->special_vis_dist();
        else
            dist = GetVisibilityDistance();

        dist += World::GetRelocationLowerLimit() * 2.0f;
    }

    maps::visitors::simple<Camera>().visit_2d(src, dist,
        [src, msg, include_src, my_team_only, dist, check3d, ignore](Camera* c)
        {
            auto p = c->GetOwner();

            if (unlikely(!include_src && src == p))
                return;

            if (unlikely(ignore && ignore == p))
                return;

            if (check3d && !src->IsWithinDist(p, dist))
                return;

            if (unlikely(my_team_only && src->GetTypeId() == TYPEID_PLAYER &&
                         static_cast<Player*>(src)->GetTeam() == p->GetTeam()))
                return;

            if (unlikely(!p->HaveAtClient(src)))
            {
                // Always send chat messages (not for invis GMs though)
                if (msg->opcode() == SMSG_MESSAGECHAT)
                {
                    if ((src->GetTypeId() == TYPEID_UNIT ||
                            src->GetTypeId() == TYPEID_PLAYER) &&
                        static_cast<Unit*>(src)->GetVisibility() ==
                            VISIBILITY_OFF)
                        return;
                }
                else
                    return;
            }

            WorldSession* session = p->GetSession();
            if (unlikely(session == nullptr))
                return;

            session->send_packet(msg);
        });
}

void Map::Update(uint32 t_diff)
{
#ifdef PERF_SAMPLING_MAP_UPDATE
    profiling::map_update_.map_start(GetId());
#endif

    // Clear active state of all creatures made active due to combat if such a
    // clear is scheduled
    if (m_clearActiveCombatObjs)
    {
        if (m_clearActiveCombatObjs <= t_diff)
        {
            InCombatCreaturesClear();
            m_clearActiveCombatObjs = 0;
        }
        else
        {
            m_clearActiveCombatObjs -= t_diff;
        }
    }

    PROFILE_BLOCK_START("dynamic tree");
    m_dyn_tree->update(t_diff);
    PROFILE_BLOCK_END("dynamic tree");

    /// update worldsessions for existing players
    PROFILE_BLOCK_START("world sessions");
    for (m_mapRefIter = m_mapRefManager.begin();
         m_mapRefIter != m_mapRefManager.end(); ++m_mapRefIter)
    {
        Player* plr = m_mapRefIter->getSource();
        if (plr && plr->IsInWorld())
        {
            WorldSession* pSession = plr->GetSession();
            MapSessionFilter updater(pSession);

            pSession->Update(updater, t_diff);
        }
    }
    PROFILE_BLOCK_END("world sessions");

    // Update grid, i.e. call Update() for all objects in this map's grid
    PROFILE_BLOCK_START("grid update");
    grid_->update_objects(t_diff);
    PROFILE_BLOCK_END("grid update");

    ///- Process necessary scripts
    PROFILE_BLOCK_START("scripts");
    if (!m_scriptSchedule.empty())
        ScriptsProcess();
    PROFILE_BLOCK_END("scripts");

    if (i_data)
        i_data->Update(t_diff);

    PROFILE_BLOCK_START("creature groups");
    m_creatureGrpMgr.UpdateGroupMgr(t_diff);
    PROFILE_BLOCK_END("creature groups");

    // Distribute completed paths after object update. This makes spline
    // movements the most fresh.
    if (sConcurrentPathgen::Instance()->in_use())
        DistributeCompletedPaths();

#ifdef PERF_SAMPLING_MAP_UPDATE
    profiling::map_update_.map_end(GetId());
#endif
}

uint32 Map::GetMaxPlayers() const
{
    InstanceTemplate const* iTemplate = ObjectMgr::GetInstanceTemplate(GetId());
    if (!iTemplate)
        return 0;
    return iTemplate->maxPlayers;
}

uint32 Map::GetMaxResetDelay() const
{
    return DungeonResetScheduler::GetMaxResetTimeFor(
        ObjectMgr::GetInstanceTemplate(GetId()));
}

const char* Map::GetMapName() const
{
    return i_mapEntry ?
               i_mapEntry->name[sWorld::Instance()->GetDefaultDbcLocale()] :
               "UNNAMEDMAP\x0";
}

void Map::UpdateObjectVisibility(WorldObject* obj)
{
    float dist = 0;

    if (unlikely(obj->GetTypeId() == TYPEID_UNIT &&
                 static_cast<Creature*>(obj)->special_vis_mob()))
        dist = static_cast<Creature*>(obj)->special_vis_dist();
    else
        dist = GetVisibilityDistance();

    dist += World::GetRelocationLowerLimit() * 2.0f;

    maps::visitors::simple<Camera>().visit_2d(obj, dist, [obj](auto&& elem)
        {
            elem->UpdateVisibilityOf(obj);
        });
}

void Map::SendInitSelf(Player* player)
{
    LOG_DEBUG(
        logging, "Creating player data for himself %u", player->GetGUIDLow());

    UpdateData data;

    bool hasTransport = false;

    // attach to player data current transport data
    if (Transport* transport = player->GetTransport())
    {
        hasTransport = true;
        transport->BuildCreateUpdateBlockForPlayer(&data, player);
    }

    // build data for self presence in world at own client (one time for map)
    player->BuildCreateUpdateBlockForPlayer(&data, player);

    // build other passengers at transport also (they always visible and marked
    // as visible and will not send at visibility update at add to map
    if (Transport* transport = player->GetTransport())
    {
        for (const auto& elem : transport->GetPassengers())
        {
            if (player != elem && player->HaveAtClient(elem))
            {
                hasTransport = true;
                (elem)->BuildCreateUpdateBlockForPlayer(&data, player);
            }
        }
    }

    data.SendPacket(player->GetSession(), hasTransport);
}

void Map::SendInitTransports(Player* player)
{
    // Add all continent transports in map to update packet
    UpdateData transData;
    for (const auto& elem : m_transports)
        if (elem != player->GetTransport())
            (elem)->BuildCreateUpdateBlockForPlayer(&transData, player);

    // Add all elevator transports in map to update packet
    for (auto& elevator : grid_->elevators())
        elevator->BuildCreateUpdateBlockForPlayer(&transData, player);

    transData.SendPacket(player->GetSession(), true);
}

void Map::SendRemoveTransports(Player* player)
{
    // Remove all continent transports in map to update packet
    UpdateData transData;
    for (const auto& elem : m_transports)
        if (elem != player->GetTransport())
            (elem)->BuildOutOfRangeUpdateBlock(&transData);

    // Remove all elevator transports in map to update packet
    for (auto& elevator : grid_->elevators())
        elevator->BuildOutOfRangeUpdateBlock(&transData);

    transData.SendPacket(player->GetSession(), true);
}

void Map::AddObjectToRemoveList(WorldObject* obj)
{
    // grid_.erase is not immediate, so should be no need to have a remove list
    // FIXME: If this turns out true, remove the remove-list code altogether
    switch (obj->GetTypeId())
    {
    case TYPEID_CORPSE:
        erase(static_cast<Corpse*>(obj), true);
        break;
    case TYPEID_DYNAMICOBJECT:
        erase(static_cast<DynamicObject*>(obj), true);
        break;
    case TYPEID_GAMEOBJECT:
        erase(static_cast<GameObject*>(obj), true);
        break;
    case TYPEID_UNIT:
    {
        auto creature = static_cast<Creature*>(obj);

        if (auto group = creature->GetGroup())
            group->RemoveMember(creature, false);

        if (creature->IsPet())
            erase(static_cast<Pet*>(creature), true);
        else if (creature->IsSpecialVisCreature())
            erase(static_cast<SpecialVisCreature*>(creature), true);
        else if (creature->IsTemporarySummon())
            erase(static_cast<TemporarySummon*>(creature), true);
        else if (creature->IsTotem())
            erase(static_cast<Totem*>(creature), true);
        else
            erase(creature, true);
        break;
    }
    default:
        logging.error(
            "Map::RemoveAllObjectsInRemoveList(): tried to remove "
            "unexpected "
            "object (TypeId: %u), ignored.",
            obj->GetTypeId());
        break;
    }
    /*assert(
        obj->GetMapId() == GetId() && obj->GetInstanceId() == GetInstanceId());

    obj->CleanupsBeforeDelete(); // remove or simplify at least cross referenced
                                 // links

    i_objectsToRemove.insert(obj);*/
}

void Map::RemoveAllObjectsInRemoveList()
{
    /*while (!i_objectsToRemove.empty())
    {
        WorldObject* obj = *i_objectsToRemove.begin();
        i_objectsToRemove.erase(i_objectsToRemove.begin());

        switch (obj->GetTypeId())
        {
        case TYPEID_CORPSE: erase(static_cast<Corpse*>(obj), true); break;
        case TYPEID_DYNAMICOBJECT:
            erase(static_cast<DynamicObject*>(obj), true);
            break;
        case TYPEID_GAMEOBJECT:
            erase(static_cast<GameObject*>(obj), true);
            break;
        case TYPEID_UNIT:
        {
            auto creature = static_cast<Creature*>(obj);
            if (creature->IsPet())
                erase(static_cast<Pet*>(creature), true);
            else if (creature->IsSpecialVisCreature())
                erase(static_cast<SpecialVisCreature*>(creature), true);
            else if (creature->IsTemporarySummon())
                erase(static_cast<TemporarySummon*>(creature), true);
            else if (creature->IsTotem())
                erase(static_cast<Totem*>(creature), true);
            else
                erase(creature, true);
            break;
        }
        default:
            logging.error(
                "Map::RemoveAllObjectsInRemoveList(): tried to remove "
                "unexpected "
                "object (TypeId: %u), ignored.",
                obj->GetTypeId());
            break;
        }
    }*/
}

uint32 Map::GetPlayersCountExceptGMs() const
{
    uint32 count = 0;
    for (const auto& elem : m_mapRefManager)
        if (!elem.getSource()->isGameMaster())
            ++count;
    return count;
}

void Map::SendToPlayers(WorldPacket const* data) const
{
    for (const auto& elem : m_mapRefManager)
        elem.getSource()->GetSession()->send_packet(data);
}

void Map::CreateInstanceData(bool load)
{
    if (i_data != nullptr)
        return;

    if (Instanceable())
    {
        if (InstanceTemplate const* mInstance =
                ObjectMgr::GetInstanceTemplate(GetId()))
            i_script_id = mInstance->script_id;
    }
    else
    {
        if (WorldTemplate const* mInstance =
                ObjectMgr::GetWorldTemplate(GetId()))
            i_script_id = mInstance->script_id;
    }

    if (!i_script_id)
        return;

    i_data = sScriptMgr::Instance()->CreateInstanceData(this);
    if (!i_data)
        return;

    if (load)
    {
        // TODO: make a global storage for this
        QueryResult* result;

        if (Instanceable())
            result = CharacterDatabase.PQuery(
                "SELECT data FROM instance WHERE id = '%u'", i_InstanceId);
        else
            result = CharacterDatabase.PQuery(
                "SELECT data FROM world WHERE map = '%u'", GetId());

        if (result)
        {
            Field* fields = result->Fetch();
            const char* data = fields[0].GetString();
            if (data)
            {
                LOG_DEBUG(logging,
                    "Loading instance data for `%s` (Map: %u Instance: %u)",
                    sScriptMgr::Instance()->GetScriptName(i_script_id), GetId(),
                    i_InstanceId);
                i_data->Load(data);
            }
            delete result;
        }
        else
        {
            // for non-instanceable map always add data to table if not found,
            // later code expected that for map in `word` exist always after
            // load
            if (!Instanceable())
                CharacterDatabase.PExecute(
                    "INSERT INTO world VALUES ('%u', '')", GetId());
        }
    }
    else
    {
        LOG_DEBUG(logging, "New instance data, \"%s\" ,initialized!",
            sScriptMgr::Instance()->GetScriptName(i_script_id));
        i_data->Initialize();
    }
}

/* ******* World Maps ******* */

WorldPersistentState* WorldMap::GetPersistanceState() const
{
    return (WorldPersistentState*)Map::GetPersistentState();
}

/* ******* Dungeon Instance Maps ******* */

DungeonMap::DungeonMap(uint32 id, uint32 InstanceId, uint8 SpawnMode)
  : Map(id, InstanceId, SpawnMode), m_resetAfterUnload(false),
    m_unloadWhenEmpty(false)
{
    assert(i_mapEntry->IsDungeon());

    // lets initialize visibility distance for dungeons
    DungeonMap::InitVisibilityDistance();

    // the timer is started by default, and stopped when the first player joins
    // this make sure it gets unloaded if for some reason no player joins
    m_unloadTimer = std::max(
        sWorld::Instance()->getConfig(CONFIG_UINT32_INSTANCE_UNLOAD_DELAY),
        (uint32)MIN_UNLOAD_DELAY);

    m_coOwnersTimer.SetInterval(5000);
}

DungeonMap::~DungeonMap()
{
}

void DungeonMap::InitVisibilityDistance()
{
    // Most raid dungeons have increased visibility distance
    switch (i_id)
    {
    case 249: // Onyxia's Lair
    case 309: // Zul'Gurub
    case 409: // Molten Core
    case 469: // Blackwing Lair
    case 509: // AQ20
    case 531: // AQ40
    case 533: // Naxxramas
    case 534: // Mount Hyjal
    case 548: // SSC
    case 550: // TK
    case 564: // Black Temple
    case 580: // Sunwell
        m_VisibleDistance = 300;
        break;
    default:
        // Default instance value
        m_VisibleDistance = World::GetMaxVisibleDistanceInInstances();
        break;
    }
}

/*
    Do map specific checks to see if the player can enter
*/
bool DungeonMap::CanEnter(Player* player, bool resurrect)
{
    if (!m_persistentState)
        return false;

    if (player->GetMapRef().getTarget() == this)
    {
        logging.error(
            "DungeonMap::CanEnter - player %s(%u) already in map %d,%d,%d!",
            player->GetName(), player->GetGUIDLow(), GetId(), GetInstanceId(),
            GetSpawnMode());
        assert(false);
        return false;
    }

    // cannot enter if the instance is full (player cap), GMs don't count
    uint32 maxPlayers = GetMaxPlayers();
    if (!player->isGameMaster() && GetPlayersCountExceptGMs() >= maxPlayers)
    {
        LOG_DEBUG(logging,
            "MAP: Instance '%u' of map '%s' cannot have more than '%u' "
            "players. Player '%s' rejected",
            GetInstanceId(), GetMapName(), maxPlayers, player->GetName());
        player->SendTransferAborted(GetId(), TRANSFER_ABORT_MAX_PLAYERS);
        return false;
    }

    // cannot enter while an encounter in the instance is in progress
    if (!resurrect && !player->isGameMaster() && GetInstanceData() &&
        GetInstanceData()->IsEncounterInProgress() &&
        player->GetMapId() != GetId())
    {
        player->SendTransferAborted(GetId(), TRANSFER_ABORT_ZONE_IN_COMBAT);
        return false;
    }

    // Player cannot be bound to another instance of this map & difficulty
    auto bind = player->GetInstanceBind(GetId(), GetDifficulty());
    if (!player->isGameMaster() && bind)
    {
        auto state = bind->state.lock();
        if (state && state.get() != m_persistentState)
        {
            player->SendTransferAborted(GetId(), TRANSFER_ABORT_NOT_FOUND);
            return false;
        }
    }

    return Map::CanEnter(player, resurrect);
}

/*
    Do map specific checks and add the player to the map if successful.
*/
bool DungeonMap::insert(Player* player)
{
    if (!CanEnter(player, player->GetTeleportOptions() & TELE_TO_RESURRECT))
        return false;

    player->BindToInstance(GetPersistanceState()->shared_from_this(),
        !GetPersistanceState()->CanReset());

    // If player is in a group, that does not have a bind for this instance
    // type, bind that group as well
    if (Group* group = player->GetGroup())
    {
        if (!group->isBGGroup() &&
            group->GetInstanceBind(GetId(), GetDifficulty()) == nullptr)
            group->BindToInstance(GetPersistanceState()->shared_from_this(),
                !GetPersistanceState()->CanReset());
    }

    // for normal instances cancel the reset schedule when the
    // first player enters (no players yet)
    SetResetSchedule(false);

    LOG_DEBUG(logging, "MAP: Player '%s' is entering instance '%u' of map '%s'",
        player->GetName(), GetInstanceId(), GetMapName());
    // initialize unload state
    m_unloadTimer = 0;
    m_resetAfterUnload = false;
    m_unloadWhenEmpty = false;

    Map::insert(player);

    player->AddRecentDungeon(GetId(), GetInstanceId());

    return true;
}

void DungeonMap::Update(const uint32 t_diff)
{
    // Update co-owners, also toggle the port out of instance timer if need be
    // NOTE: It's possible that the instance has expired, the state been deleted
    //       and all that's left for this map is to have people be ported out.
    if (m_persistentState)
    {
        m_coOwnersTimer.Update(t_diff);
        if (m_coOwnersTimer.Passed())
        {
            for (auto& elem : m_mapRefManager)
                CheckPlayerCoOwnerStatus(elem.getSource());
            m_coOwnersTimer.Reset();
        }
    }

    Map::Update(t_diff);
}

void DungeonMap::erase(Player* player, bool remove)
{
    LOG_DEBUG(logging,
        "MAP: Removing player '%s' from instance '%u' of map '%s' before "
        "relocating to other map",
        player->GetName(), GetInstanceId(), GetMapName());

    // if last player set unload timer
    if (!m_unloadTimer && m_mapRefManager.getSize() == 1)
        m_unloadTimer = m_unloadWhenEmpty ?
                            MIN_UNLOAD_DELAY :
                            std::max(sWorld::Instance()->getConfig(
                                         CONFIG_UINT32_INSTANCE_UNLOAD_DELAY),
                                (uint32)MIN_UNLOAD_DELAY);

    Map::erase(player, remove);

    // Clear instance bind if we were being ported out (not for log out case)
    if (player->GetSession() && !player->GetSession()->PlayerLogout())
    {
        if (auto bind = player->GetInstanceBind(GetId(), GetDifficulty()))
            if (!bind->perm)
            {
                // Re-check co-owner status
                CheckPlayerCoOwnerStatus(player);
                // Unbind if not valid
                if (!player->m_InstanceValid)
                    player->UnbindFromInstance(GetId(), GetDifficulty());
            }
    }

    // for normal instances schedule the reset after all players have left
    SetResetSchedule(true);
}

void DungeonMap::SendResetFailedNotify()
{
    // notify the players to leave the instance so it can be reset
    for (auto& elem : m_mapRefManager)
        elem.getSource()->SendResetFailedNotify(GetId());
}

/*
    Returns true if there are no players in the instance
*/
bool DungeonMap::Reset(InstanceResetMethod method)
{
    // note: since the map may not be loaded when the instance needs to be reset
    // the instance must be deleted from the DB by InstanceSaveManager

    if (HavePlayers())
    {
        if (method == INSTANCE_RESET_ALL)
            SendResetFailedNotify();
        else
        {
            if (method == INSTANCE_RESET_GLOBAL)
            {
                // set the homebind timer for players inside (1 minute)
                for (auto& elem : m_mapRefManager)
                    elem.getSource()->m_InstanceValid = false;
            }

            // the unload timer is not started
            // instead the map will unload immediately after the players have
            // left
            m_unloadWhenEmpty = true;
            m_resetAfterUnload = true;
        }
    }
    else
    {
        // unloaded at next update
        m_unloadTimer = MIN_UNLOAD_DELAY;
        m_resetAfterUnload = true;
    }

    return m_mapRefManager.isEmpty();
}

void DungeonMap::PermBindAllPlayers(Creature* boss)
{
    if (!m_persistentState)
        return;

    GetPersistanceState()->SetCanReset(false);

    // Everyone on the tap list of the boss gets bound
    if (boss->GetLootDistributor() &&
        boss->GetLootDistributor()->recipient_mgr()->taps())
    {
        auto taps = boss->GetLootDistributor()->recipient_mgr()->taps();
        for (const auto& tap : *taps)
        {
            auto player = ObjectAccessor::FindPlayer(tap, false);
            if (!player)
                BindOfflinePlayer(tap);
            else
                player->BindToInstance(
                    GetPersistanceState()->shared_from_this(), true);
        }
    }

    // Everyone in the map gets bound as well
    for (auto& elem : m_mapRefManager)
        elem.getSource()->BindToInstance(
            GetPersistanceState()->shared_from_this(), true);

    // Rebind all groups to permanent that are temporarily bound to the instance
    for (auto grp : GetPersistanceState()->GetBoundGroups())
        grp->BindToInstance(GetPersistanceState()->shared_from_this(), true);
}

void DungeonMap::UnloadAll()
{
    // Insert any pending player, so we can teleport him out, in case
    grid_->exec_pending_operations();

    if (HavePlayers())
    {
        logging.error(
            "DungeonMap::UnloadAll: there are still players in the instance at "
            "unload, should not happen!");
        for (auto& elem : m_mapRefManager)
        {
            Player* plr = elem.getSource();
            plr->TeleportToHomebind();
        }
    }

    if (m_resetAfterUnload == true && m_persistentState)
        GetPersistanceState()->DeleteRespawnTimes();

    Map::UnloadAll();
}

void DungeonMap::SendResetWarnings(uint32 timeLeft) const
{
    for (const auto& elem : m_mapRefManager)
        elem.getSource()->SendInstanceResetWarning(GetId(), timeLeft);
}

void DungeonMap::SetResetSchedule(bool on)
{
    if (!m_persistentState)
        return;

    // only for normal instances
    // the reset time is only scheduled when there are no payers inside
    // it is assumed that the reset time will rarely (if ever) change while the
    // reset is scheduled
    if (!HavePlayers() && !IsRaidOrHeroicDungeon())
        sMapPersistentStateMgr::Instance()->GetScheduler().ScheduleReset(
            on, GetPersistanceState()->GetResetTime(),
            DungeonResetEvent(
                RESET_EVENT_NORMAL_DUNGEON, GetId(), GetInstanceId()));
}

DungeonPersistentState* DungeonMap::GetPersistanceState() const
{
    return (DungeonPersistentState*)Map::GetPersistentState();
}

void DungeonMap::BindOfflinePlayer(ObjectGuid guid, bool perm)
{
    CharacterDatabase.PExecute(
        "INSERT INTO character_instance (guid, instance, permanent) VALUES(%u, "
        "%u, %u)"
        " ON DUPLICATE KEY UPDATE instance=%u, permanent=%u",
        guid.GetCounter(), GetInstanceId(), perm, GetInstanceId(), perm);
}

void DungeonMap::CheckPlayerCoOwnerStatus(Player* player)
{
    assert(m_persistentState);

    auto state = GetPersistanceState();

    // Skip checking status if we're already registered as co-owner, or a GM
    if (state->IsCoOwner(player) || player->isGameMaster())
        return;

    // You obtain co-owner if:
    // a) You have a perm bind to the instance
    // b) You enter an instance with no co-owners, no perm binds and no group
    // binds
    // c) The instance has a group bind, and you're part of that group
    // d) You're in the same group as someone who co-owns it already
    bool promoteToCoOwner = false;

    auto bind = player->GetInstanceBind(GetId(), GetDifficulty());
    auto& coOwners = state->GetCoOwners();

    // a) You have a perm bind to the instance
    if (bind && bind->perm)
    {
        promoteToCoOwner = true;
        goto coowner_check_done;
    }

    // b) You enter an instance with no co-owners, no perm binds and no group
    // binds
    if (state->CanReset() && state->GetCoOwners().empty() &&
        state->GetBoundGroups().empty())
    {
        promoteToCoOwner = true;
        goto coowner_check_done;
    }

    // c) You're part of a group that's bound to the instance
    if (state->InBoundGroup(player))
    {
        promoteToCoOwner = true;
        goto coowner_check_done;
    }

    // d) You're in the same group as someone who co-owns it already (XXX: Is
    // this check needed?)
    for (const auto& coOwner : coOwners)
    {
        if ((coOwner)->GetGroup() != nullptr &&
            (coOwner)->GetGroup() == player->GetGroup())
        {
            promoteToCoOwner = true;
            goto coowner_check_done;
        }
    }

coowner_check_done:

    if (promoteToCoOwner)
    {
        state->AddCoOwner(player);
        player->m_InstanceValid = true;
    }
    else
    {
        player->m_InstanceValid = false;
    }
}

/* ******* Battleground Instance Maps ******* */

BattleGroundMap::BattleGroundMap(uint32 id, uint32 InstanceId)
  : Map(id, InstanceId, REGULAR_DIFFICULTY)
{
    // lets initialize visibility distance for BG/Arenas
    BattleGroundMap::InitVisibilityDistance();
}

BattleGroundMap::~BattleGroundMap()
{
}

void BattleGroundMap::Update(const uint32 diff)
{
    Map::Update(diff);

    GetBG()->Update(diff);
}

BattleGroundPersistentState* BattleGroundMap::GetPersistanceState() const
{
    return (BattleGroundPersistentState*)Map::GetPersistentState();
}

void BattleGroundMap::InitVisibilityDistance()
{
    // init visibility distance for BG/Arenas
    m_VisibleDistance = World::GetMaxVisibleDistanceInBGArenas();
}

bool BattleGroundMap::CanEnter(Player* player, bool resurrect)
{
    if (player->GetMapRef().getTarget() == this)
    {
        logging.error("BGMap::CanEnter - player %u already in map!",
            player->GetGUIDLow());
        assert(false);
        return false;
    }

    if (player->GetBattleGroundId() != GetInstanceId())
        return false;

    // player number limit is checked in bgmgr, no need to do it here

    return Map::CanEnter(player, resurrect);
}

bool BattleGroundMap::insert(Player* player)
{
    if (!CanEnter(player, false))
        return false;

    // reset instance validity, battleground maps do not homebind
    player->m_InstanceValid = true;

    return Map::insert(player);
}

void BattleGroundMap::erase(Player* player, bool remove)
{
    LOG_DEBUG(logging,
        "MAP: Removing player '%s' from bg '%u' of map '%s' before relocating "
        "to other map",
        player->GetName(), GetInstanceId(), GetMapName());
    Map::erase(player, remove);
}

void BattleGroundMap::SetUnload()
{
    m_unloadTimer = MIN_UNLOAD_DELAY;
}

void BattleGroundMap::UnloadAll()
{
    // Insert any pending player, so we can teleport him out
    grid_->exec_pending_operations();

    while (HavePlayers())
    {
        if (Player* plr = m_mapRefManager.getFirst()->getSource())
        {
            plr->TeleportTo(plr->GetBattleGroundEntryPoint());
            // TeleportTo removes the player from this map (if the map exists)
            // -> calls BattleGroundMap::Remove -> invalidates the iterator.
            // just in case, remove the player from the list explicitly here as
            // well to prevent a possible infinite loop
            // note that this remove is not needed if the code works well in
            // other places
            plr->GetMapRef().unlink();
        }
    }

    Map::UnloadAll();
}

/// Put scripts in the execution queue
void Map::ScriptsStart(
    ScriptMapMapName const& scripts, uint32 id, Object* source, Object* target)
{
    ///- Find the script map
    auto s = scripts.second.find(id);
    if (s == scripts.second.end())
        return;

    // prepare static data
    ObjectGuid sourceGuid = source->GetObjectGuid();
    ObjectGuid targetGuid = target ? target->GetObjectGuid() : ObjectGuid();
    ObjectGuid ownerGuid = source->isType(TYPEMASK_ITEM) ?
                               ((Item*)source)->GetOwnerGuid() :
                               ObjectGuid();

    ///- Schedule script execution for all scripts in the script map
    ScriptMap const* s2 = &(s->second);
    bool immedScript = false;
    for (const auto& elem : *s2)
    {
        ScriptAction sa(scripts.first, this, sourceGuid, targetGuid, ownerGuid,
            &elem.second);

        m_scriptSchedule.insert(ScriptScheduleMap::value_type(
            time_t(WorldTimer::time_no_syscall() + elem.first), sa));
        if (elem.first == 0)
            immedScript = true;

        sScriptMgr::Instance()->IncreaseScheduledScriptsCount();
    }
    ///- If one of the effects should be immediate, launch the script execution
    if (immedScript)
        ScriptsProcess();
}

void Map::ScriptCommandStart(
    ScriptInfo const& script, uint32 delay, Object* source, Object* target)
{
    // NOTE: script record _must_ exist until command executed

    // prepare static data
    ObjectGuid sourceGuid = source->GetObjectGuid();
    ObjectGuid targetGuid = target ? target->GetObjectGuid() : ObjectGuid();
    ObjectGuid ownerGuid = source->isType(TYPEMASK_ITEM) ?
                               ((Item*)source)->GetOwnerGuid() :
                               ObjectGuid();

    ScriptAction sa("Internal Activate Command used for spell", this,
        sourceGuid, targetGuid, ownerGuid, &script);

    m_scriptSchedule.insert(ScriptScheduleMap::value_type(
        time_t(WorldTimer::time_no_syscall() + delay), sa));

    sScriptMgr::Instance()->IncreaseScheduledScriptsCount();

    ///- If effects should be immediate, launch the script execution
    if (delay == 0)
        ScriptsProcess();
}

void Map::add_to_object_storage(Creature* obj)
{
    creature_map_[obj->GetObjectGuid()] = obj;
}
void Map::add_to_object_storage(Pet* obj)
{
    pet_map_[obj->GetObjectGuid()] = obj;
}

void Map::add_to_object_storage(GameObject* obj)
{
    go_map_[obj->GetObjectGuid()] = obj;
}

void Map::add_to_object_storage(DynamicObject* obj)
{
    do_map_[obj->GetObjectGuid()] = obj;
}

void Map::remove_from_object_storage(Creature* obj)
{
    creature_map_.erase(obj->GetObjectGuid());
}

void Map::remove_from_object_storage(Pet* obj)
{
    pet_map_.erase(obj->GetObjectGuid());
}

void Map::remove_from_object_storage(GameObject* obj)
{
    go_map_.erase(obj->GetObjectGuid());
}

void Map::remove_from_object_storage(DynamicObject* obj)
{
    do_map_.erase(obj->GetObjectGuid());
}

/// Process queued scripts
void Map::ScriptsProcess()
{
    if (m_scriptSchedule.empty())
        return;

    ///- Process overdue queued scripts
    auto iter = m_scriptSchedule.begin();
    // ok as multimap is a *sorted* associative container
    while (!m_scriptSchedule.empty() &&
           (iter->first <= WorldTimer::time_no_syscall()))
    {
        iter->second.HandleScriptStep();

        m_scriptSchedule.erase(iter);
        iter = m_scriptSchedule.begin();

        sScriptMgr::Instance()->DecreaseScheduledScriptCount();
    }
}

Player* Map::GetPlayer(ObjectGuid guid)
{
    Player* plr =
        ObjectAccessor::FindPlayer(guid); // return only in world players
    return plr && plr->GetMap() == this ? plr : nullptr;
}

Creature* Map::GetCreature(ObjectGuid guid)
{
    auto itr = creature_map_.find(guid);
    return itr != creature_map_.end() ? itr->second : nullptr;
}

Pet* Map::GetPet(ObjectGuid guid)
{
    auto itr = pet_map_.find(guid);
    return itr != pet_map_.end() ? itr->second : nullptr;
}

Corpse* Map::GetCorpse(ObjectGuid guid)
{
    Corpse* ret = sObjectAccessor::Instance()->GetCorpseInMap(guid, GetId());
    return ret && ret->GetInstanceId() == GetInstanceId() ? ret : nullptr;
}

Creature* Map::GetAnyTypeCreature(ObjectGuid guid)
{
    switch (guid.GetHigh())
    {
    case HIGHGUID_UNIT:
        return GetCreature(guid);
    case HIGHGUID_PET:
        return GetPet(guid);
    default:
        break;
    }

    return nullptr;
}

GameObject* Map::GetGameObject(ObjectGuid guid)
{
    auto itr = go_map_.find(guid);
    return itr != go_map_.end() ? itr->second : nullptr;
}

DynamicObject* Map::GetDynamicObject(ObjectGuid guid)
{
    auto itr = do_map_.find(guid);
    return itr != do_map_.end() ? itr->second : nullptr;
}

Unit* Map::GetUnit(ObjectGuid guid)
{
    if (guid.IsPlayer())
        return GetPlayer(guid);

    return GetAnyTypeCreature(guid);
}

WorldObject* Map::GetWorldObject(ObjectGuid guid)
{
    switch (guid.GetHigh())
    {
    case HIGHGUID_PLAYER:
        return GetPlayer(guid);
    case HIGHGUID_GAMEOBJECT:
        return GetGameObject(guid);
    case HIGHGUID_UNIT:
        return GetCreature(guid);
    case HIGHGUID_PET:
        return GetPet(guid);
    case HIGHGUID_DYNAMICOBJECT:
        return GetDynamicObject(guid);
    case HIGHGUID_CORPSE:
    {
        // corpse special case, it can be not in world
        Corpse* corpse = GetCorpse(guid);
        return corpse && corpse->IsInWorld() ? corpse : nullptr;
    }
    case HIGHGUID_MO_TRANSPORT:
    case HIGHGUID_TRANSPORT:
    default:
        break;
    }

    return nullptr;
}

Transport* Map::GetTransport(ObjectGuid guid)
{
    if (!guid.IsMOTransport())
        return nullptr;

    for (const auto& elem : m_transports)
        if ((elem)->GetObjectGuid() == guid)
            return elem;

    return nullptr;
}

void Map::SendObjectUpdates()
{
    UpdateDataMapType update_players;

    while (!i_objectsToClientUpdate.empty())
    {
        Object* obj = *i_objectsToClientUpdate.begin();
        i_objectsToClientUpdate.erase(i_objectsToClientUpdate.begin());
        obj->BuildUpdateData(update_players);
    }

    for (auto& update_player : update_players)
        update_player.second.SendPacket(update_player.first->GetSession());
}

void Map::DistributeCompletedPaths()
{
    auto paths = sConcurrentPathgen::Instance()->get_completed_paths(this);
    for (auto& path : paths)
    {
        if (path.receiver)
            path.receiver(path.src, std::move(path.path), path.path_id);
        else
            path.src->finished_path(std::move(path.path), path.path_id);
    }
}

uint32 Map::GenerateLocalLowGuid(HighGuid guidhigh)
{
    // TODO: for map local guid counters possible force reload map instead
    // shutdown server at guid counter overflow
    switch (guidhigh)
    {
    case HIGHGUID_UNIT:
        return m_CreatureGuids.Generate();
    case HIGHGUID_GAMEOBJECT:
        return m_GameObjectGuids.Generate();
    case HIGHGUID_DYNAMICOBJECT:
        return m_DynObjectGuids.Generate();
    case HIGHGUID_PET:
        return m_PetGuids.Generate();
    default:
        assert(false);
        return 0;
    }
}

/**
 * Helper structure for building static chat information
 *
 */
class StaticMonsterChatBuilder
{
public:
    StaticMonsterChatBuilder(CreatureInfo const* cInfo, ChatMsg msgtype,
        int32 textId, uint32 language, Unit* target, uint32 senderLowGuid = 0)
      : i_cInfo(cInfo), i_msgtype(msgtype), i_textId(textId),
        i_language(language), i_target(target)
    {
        // 0 lowguid not used in core, but accepted fine in this case by client
        i_senderGuid = i_cInfo->GetObjectGuid(senderLowGuid);
    }
    void operator()(WorldPacket& data, int32 loc_idx)
    {
        char const* text =
            sObjectMgr::Instance()->GetMangosString(i_textId, loc_idx);

        char const* nameForLocale = i_cInfo->Name;
        sObjectMgr::Instance()->GetCreatureLocaleStrings(
            i_cInfo->Entry, loc_idx, &nameForLocale);

        WorldObject::BuildMonsterChat(&data, i_senderGuid, i_msgtype, text,
            i_language, nameForLocale,
            i_target ? i_target->GetObjectGuid() : ObjectGuid(),
            i_target ? i_target->GetNameForLocaleIdx(loc_idx) : "");
    }

private:
    ObjectGuid i_senderGuid;
    CreatureInfo const* i_cInfo;
    ChatMsg i_msgtype;
    int32 i_textId;
    uint32 i_language;
    Unit* i_target;
};

/**
 * Function simulates yell of creature
 *
 * @param guid must be creature guid of whom to Simulate the yell, non-creature
 *guids not supported at this moment
 * @param textId Id of the simulated text
 * @param language language of the text
 * @param target, can be NULL
 */
void Map::MonsterYellToMap(
    ObjectGuid guid, int32 textId, uint32 language, Unit* target)
{
    if (guid.IsAnyTypeCreature())
    {
        CreatureInfo const* cInfo =
            ObjectMgr::GetCreatureTemplate(guid.GetEntry());
        if (!cInfo)
        {
            logging.error(
                "Map::MonsterYellToMap: Called for nonexistent creature entry "
                "in guid: %s",
                guid.GetString().c_str());
            return;
        }

        MonsterYellToMap(cInfo, textId, language, target, guid.GetCounter());
    }
    else
    {
        logging.error("Map::MonsterYellToMap: Called for non creature guid: %s",
            guid.GetString().c_str());
        return;
    }
}

/**
 * Function simulates yell of creature
 *
 * @param cinfo must be entry of Creature of whom to Simulate the yell
 * @param textId Id of the simulated text
 * @param language language of the text
 * @param target, can be NULL
 * @param senderLowGuid provide way proper show yell for near spawned creature
 *with known lowguid,
 *        0 accepted by client else if this not important
 */
void Map::MonsterYellToMap(CreatureInfo const* cinfo, int32 textId,
    uint32 language, Unit* target, uint32 senderLowGuid /*= 0*/)
{
    StaticMonsterChatBuilder say_build(
        cinfo, CHAT_MSG_MONSTER_YELL, textId, language, target, senderLowGuid);
    auto say_do = maps::callbacks::make_localize_packet(say_build);

    Map::PlayerList const& pList = GetPlayers();
    for (const auto& elem : pList)
        say_do(elem.getSource());
}

/**
 * Function to play sound to all players in map
 *
 * @param soundId Played Sound
 * @param zoneId Id of the Zone to which the sound should be restricted
 */
void Map::PlayDirectSoundToMap(uint32 soundId, uint32 zoneId /*=0*/)
{
    WorldPacket data(SMSG_PLAY_SOUND, 4);
    data << uint32(soundId);

    Map::PlayerList const& pList = GetPlayers();
    for (const auto& elem : pList)
        if (!zoneId || elem.getSource()->GetZoneId() == zoneId)
            elem.getSource()->GetSession()->send_packet(&data);
}

float Map::GetHeight(float x, float y, float z) const
{
    return m_TerrainData->GetHeightStatic(x, y, z);
}

bool Map::isInLineOfSight(
    float x1, float y1, float z1, float x2, float y2, float z2) const
{
    return VMAP::VMapFactory::createOrGetVMapManager()->isInM2LineOfSight(
               GetId(), x1, y1, z1, x2, y2, z2) &&
           VMAP::VMapFactory::createOrGetVMapManager()->isInWmoLineOfSight(
               GetId(), x1, y1, z1, x2, y2, z2) &&
           m_dyn_tree->isInLineOfSight(x1, y1, z1, x2, y2, z2);
}

bool Map::isInWmoLineOfSight(
    float x1, float y1, float z1, float x2, float y2, float z2) const
{
    return VMAP::VMapFactory::createOrGetVMapManager()->isInWmoLineOfSight(
               GetId(), x1, y1, z1, x2, y2, z2) &&
           m_dyn_tree->isInLineOfSight(x1, y1, z1, x2, y2, z2);
}

bool Map::isInDynLineOfSight(
    float x1, float y1, float z1, float x2, float y2, float z2) const
{
    return m_dyn_tree->isInLineOfSight(x1, y1, z1, x2, y2, z2);
}

void Map::BalanceDynamicTree()
{
    m_dyn_tree->balance();
}

void Map::EraseModel(const GameObjectModel& mdl)
{
    m_dyn_tree->remove(mdl);
}

void Map::InsertModel(const GameObjectModel& mdl)
{
    m_dyn_tree->insert(mdl);
}

bool Map::ContainsModel(const GameObjectModel& mdl) const
{
    return m_dyn_tree->contains(mdl);
}

bool Map::IsCreatureInCombatList(Creature* pCreature)
{
    return m_inCombatCreatures.find(pCreature->GetObjectGuid()) !=
           m_inCombatCreatures.end();
}

void Map::CreatureEnterCombat(Creature* pCreature)
{
    m_inCombatCreatures.insert(pCreature->GetObjectGuid());
}

void Map::CreatureLeaveCombat(Creature* pCreature)
{
    auto find = m_inCombatCreatures.find(pCreature->GetObjectGuid());
    if (find != m_inCombatCreatures.end())
        m_inCombatCreatures.erase(find);
}

void Map::InCombatCreaturesClear()
{
    for (const auto& elem : m_inCombatCreatures)
    {
        if (Creature* c = GetCreature(elem))
            c->SetActiveObjectState(false);
    }
    m_inCombatCreatures.clear();
}

Creature* Map::SummonCreature(uint32 id, float x, float y, float z, float ang,
    TempSummonType spwtype, uint32 despwtime, bool asActiveObject)
{
    const CreatureInfo* cinfo = ObjectMgr::GetCreatureTemplate(id);
    if (!cinfo)
    {
        logging.error(
            "Map::SummonCreature tried to summon creature with id %u. No such "
            "creature exists.",
            id);
        return nullptr;
    }

    auto creature = new TemporarySummon;

    CreatureCreatePos pos(this, x, y, z, ang);

    if (!creature->Create(
            GenerateLocalLowGuid(cinfo->GetHighGuid()), pos, cinfo, ALLIANCE))
    {
        delete creature;
        return nullptr;
    }

    creature->SetSummonPoint(pos);

    // Active state set before added to map
    // NOTE: Must be set to true so SummonedBy always triggers, gets toggled off
    //       next map update if NPC is not active.
    creature->SetActiveObjectState(true);

    if (!creature->Summon(spwtype, despwtime))
    {
        delete creature;
        return nullptr;
    }

    creature->queue_action(0, [creature, asActiveObject]()
        {
            if (!asActiveObject)
                creature->SetActiveObjectState(false);
            if (creature->AI())
                creature->AI()->SummonedBy(creature);
        });

    return creature;
}
