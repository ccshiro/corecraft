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

#ifndef MANGOS_MAP_H
#define MANGOS_MAP_H

#include "Common.h"
#include "CreatureGroupMgr.h"
#include "DBCStructure.h"
#include "GridMap.h"
#include "MapRefManager.h"
#include "Object.h"
#include "ScriptMgr.h"
#include "SharedDefines.h"
#include "Timer.h"
#include "maps/map_grid.h"
#include "Platform/Define.h"
#include "Utilities/TypeList.h"
#include <sparsehash/dense_hash_map>
#include <vector>

class BattleGround;
class BattleGroundPersistentState;
class Creature;
struct CreatureInfo;
class DungeonPersistentState;
class DynamicMapTree;
class GameObjectModel;
class GridMap;
class Group;
class InstanceData;
class MapPersistentState;
struct ScriptInfo;
class Unit;
class WorldPacket;
class WorldPersistentState;

// GCC have alternative #pragma pack(N) syntax and old gcc version not support
// pack(push,N), also any gcc version not support it at some platform
#if defined(__GNUC__)
#pragma pack(1)
#else
#pragma pack(push, 1)
#endif

struct InstanceTemplate
{
    uint32 map;    // instance map
    uint32 parent; // non-continent parent instance (for instance with entrance
                   // in another instances)
                   // or 0 (not related to continent 0 map id)
    uint32 levelMin;
    uint32 levelMax;
    uint32 maxPlayers;
    uint32 reset_delay; // in days
    uint32 script_id;
};

struct WorldTemplate
{
    uint32 map; // non-instance map
    uint32 script_id;
};

enum LevelRequirementVsMode
{
    LEVELREQUIREMENT_HEROIC = 70
};

#if defined(__GNUC__)
#pragma pack()
#else
#pragma pack(pop)
#endif

#define MIN_UNLOAD_DELAY 1 // immediate unload

class MANGOS_DLL_SPEC Map
{
    friend class maps::map_grid;
    friend class MapReference;
    friend class ObjectGridLoader;
    friend class ObjectWorldLoader;

protected:
    Map(uint32 id, uint32 InstanceId, uint8 SpawnMode);

public:
    maps::map_grid& get_map_grid() { return *grid_; }

    virtual ~Map();

    // currently unused for normal maps
    bool CanUnload(uint32 diff)
    {
        if (!m_unloadTimer)
            return false;
        if (m_unloadTimer <= diff)
            return true;
        m_unloadTimer -= diff;
        return false;
    }

    // Unload map prior to deletion
    // Remark: Call this from any overriden implementations
    virtual void UnloadAll();

    virtual void Update(const uint32);

    // Pre-conditions or behavior is undefined:
    // 1. You may not add the same entity twice
    // 2. The object must have a valid X and Y
    // Returns false if the object could not be added
    // NOTE: Object will not have a valid map just because it inserted. Unless
    //       IsInWorld() is true or you got the object from the map/grid system
    //       the object cannot be assumed to have a valid map!
    bool insert(WorldObject* t);
    bool insert(Transport* t);
    virtual bool insert(Player* t);

    // Player is already in map but relogged and needs data about objects etc
    void relog(Player* player);

    // Pre-conditions or behavior is undefined:
    // 1. The object must exist in the map (i.e. be inserted already)
    template <typename T>
    void erase(T* t, bool destroy);
    void erase(WorldObject*, bool) = delete;
    void erase(Transport* t, bool destroy);
    virtual void erase(Player* t, bool destroy);

    // Do not use for players
    void add_active_entity(WorldObject* object);
    // Do not use for players
    void remove_active_entity(WorldObject* object);

    // Move an entity to a new position
    // Object must already have been added to the map, or outcome is undefined
    void relocate(Creature* object, float x, float y, float z, float o);
    void relocate(Player* object, float x, float y, float z, float o);

    // dist of 0: use map's vis range
    void broadcast_message(WorldObject* src, WorldPacket* msg, bool include_src,
        float dist = 0, bool my_team_only = false, bool check3d = false,
        const Player* ignore = nullptr);

    float GetVisibilityDistance() const { return m_VisibleDistance; }
    float GetSpecialVisDistance() const { return m_SpecialVisDistance; }
    // Initialize visibility distance per map type
    virtual void InitVisibilityDistance();

    uint32 GetId(void) const { return i_id; }

    // some calls like isInWater should not use vmaps due to processor power
    // can return INVALID_HEIGHT if under z+2 z coord not found height

    virtual void RemoveAllObjectsInRemoveList();

    bool CreatureRespawnRelocation(
        Creature* c); // used only in CreatureRelocation and ObjectGridUnloader

    uint32 GetInstanceId() const { return i_InstanceId; }
    virtual bool CanEnter(Player* /*player*/, bool /*resurrect*/)
    {
        return true;
    }
    const char* GetMapName() const;

    // _currently_ spawnmode == difficulty, but this can be changes later, so
    // use appropriate spawnmode/difficult functions
    // for simplify later code support
    // regular difficulty = continent/dungeon normal/raid normal difficulty
    uint8 GetSpawnMode() const { return (i_spawnMode); }
    Difficulty GetDifficulty() const { return Difficulty(GetSpawnMode()); }
    bool IsRegularDifficulty() const
    {
        return GetDifficulty() == REGULAR_DIFFICULTY;
    }
    uint32 GetMaxPlayers() const;
    uint32 GetMaxResetDelay() const;

    bool Instanceable() const
    {
        return i_mapEntry && i_mapEntry->Instanceable();
    }
    // NOTE: this duplicate of Instanceable(), but Instanceable() can be changed
    // when BG also will be instanceable
    bool IsDungeon() const { return i_mapEntry && i_mapEntry->IsDungeon(); }
    bool IsRaid() const { return i_mapEntry && i_mapEntry->IsRaid(); }
    bool IsRaidOrHeroicDungeon() const
    {
        return IsRaid() || GetDifficulty() > DUNGEON_DIFFICULTY_NORMAL;
    }
    bool IsBattleGround() const
    {
        return i_mapEntry && i_mapEntry->IsBattleGround();
    }
    bool IsBattleArena() const
    {
        return i_mapEntry && i_mapEntry->IsBattleArena();
    }
    bool IsBattleGroundOrArena() const
    {
        return i_mapEntry && i_mapEntry->IsBattleGroundOrArena();
    }

    // Can be nullptr if instance is reset, and people are being ported out
    MapPersistentState* GetPersistentState() const { return m_persistentState; }
    void ClearPersistentState() { m_persistentState = nullptr; }

    void AddObjectToRemoveList(WorldObject* obj);

    void UpdateObjectVisibility(WorldObject* obj);

    bool HavePlayers() const { return !m_mapRefManager.isEmpty(); }
    uint32 GetPlayersCountExceptGMs() const;

    void SendToPlayers(WorldPacket const* data) const;

    typedef MapRefManager PlayerList;
    PlayerList const& GetPlayers() const { return m_mapRefManager; }

    // per-map script storage
    void ScriptsStart(ScriptMapMapName const& scripts, uint32 id,
        Object* source, Object* target);
    void ScriptCommandStart(
        ScriptInfo const& script, uint32 delay, Object* source, Object* target);

    /*
     * Map-Level Object Storage
     */
    // Insertion/deletion
    void add_to_object_storage(Creature* obj);
    void add_to_object_storage(Pet* obj);
    void add_to_object_storage(GameObject* obj);
    void add_to_object_storage(DynamicObject* obj);
    void remove_from_object_storage(Creature* obj);
    void remove_from_object_storage(Pet* obj);
    void remove_from_object_storage(GameObject* obj);
    void remove_from_object_storage(DynamicObject* obj);
    // Lookup
    Player* GetPlayer(ObjectGuid guid);
    Creature* GetCreature(ObjectGuid guid);
    Pet* GetPet(ObjectGuid guid);
    // Prefer GetCreature and GetPlayer
    Creature* GetAnyTypeCreature(ObjectGuid guid);
    GameObject* GetGameObject(ObjectGuid guid);
    DynamicObject* GetDynamicObject(ObjectGuid guid);
    Corpse* GetCorpse(ObjectGuid guid);
    // Prefer GetCreature and GetPlayer and GetPet
    Unit* GetUnit(ObjectGuid guid);
    // Prefer specific getters
    WorldObject* GetWorldObject(ObjectGuid guid);
    Transport* GetTransport(ObjectGuid guid);

    void AddUpdateObject(Object* obj) { i_objectsToClientUpdate.insert(obj); }

    void RemoveUpdateObject(Object* obj) { i_objectsToClientUpdate.erase(obj); }

    // DynObjects currently
    uint32 GenerateLocalLowGuid(HighGuid guidhigh);

    // get corresponding TerrainData object for this particular map
    const TerrainInfo* GetTerrain() const { return m_TerrainData; }

    void CreateInstanceData(bool load);
    InstanceData* GetInstanceData() { return i_data; }
    uint32 GetScriptId() const { return i_script_id; }

    void MonsterYellToMap(
        ObjectGuid guid, int32 textId, uint32 language, Unit* target);
    void MonsterYellToMap(CreatureInfo const* cinfo, int32 textId,
        uint32 language, Unit* target, uint32 senderLowGuid = 0);
    void PlayDirectSoundToMap(uint32 soundId, uint32 zoneId = 0);

    CreatureGroupMgr& GetCreatureGroupMgr() { return m_creatureGrpMgr; }

    float GetWaterOrGroundLevel(float x, float y, float z,
        float* ground = nullptr, bool swim = false) const;
    float GetHeight(float x, float y, float z) const;
    // Checks game objects & all models
    bool isInLineOfSight(
        float x1, float y1, float z1, float x2, float y2, float z2) const;
    // Checks game objects & limited models (that block spell LoS)
    bool isInWmoLineOfSight(
        float x1, float y1, float z1, float x2, float y2, float z2) const;
    // Checks game objects
    bool isInDynLineOfSight(
        float x1, float y1, float z1, float x2, float y2, float z2) const;
    DynamicMapTree* get_dyn_tree() { return m_dyn_tree; }
    void BalanceDynamicTree();
    void EraseModel(const GameObjectModel& mdl);
    void InsertModel(const GameObjectModel& mdl);
    bool ContainsModel(const GameObjectModel& mdl) const;

    bool IsCreatureInCombatList(Creature* creature);
    void CreatureEnterCombat(Creature* creature);
    void CreatureLeaveCombat(Creature* creature);

    Creature* SummonCreature(uint32 id, float x, float y, float z, float ang,
        TempSummonType spwtype, uint32 despwtime, bool asActiveObject = false);

private:
    void SendInitSelf(Player* player);

    void SendInitTransports(Player* player);
    void SendRemoveTransports(Player* player);

    void ScriptsProcess();

    void SendObjectUpdates();
    std::set<Object*> i_objectsToClientUpdate;

    // distribute paths generated with concurrent_pathgen.h
    void DistributeCompletedPaths();

protected:
    // Map Grid
    std::shared_ptr<maps::map_grid> grid_;

    // True if map has unloaded and is waiting for deletion
    // No ne entities may be spawned
    bool unloaded_;

    MapEntry const* i_mapEntry;
    uint32 m_clearActiveCombatObjs;
    uint8 i_spawnMode;
    uint32 i_id;
    uint32 i_InstanceId;
    uint32 m_unloadTimer;
    float m_VisibleDistance;
    float m_SpecialVisDistance;
    DynamicMapTree* m_dyn_tree;
    MapPersistentState* m_persistentState;

    MapRefManager m_mapRefManager;
    MapRefManager::iterator m_mapRefIter;

    // Update of transports happen in TransportMgr::UpdateTransports
    typedef std::set<Transport*> TransportsContainer;
    TransportsContainer m_transports;

private:
    bool destroyed_; // FIXME: This is an ugly hack, see FIXME in map_grid.cpp

    // Saved so we can remove them as active objects
    std::set<ObjectGuid> m_inCombatCreatures;

    // Shared geodata object with map coord info...
    TerrainInfo* const m_TerrainData;

    std::set<WorldObject*> i_objectsToRemove;

    typedef std::multimap<time_t, ScriptAction> ScriptScheduleMap;
    ScriptScheduleMap m_scriptSchedule;

    InstanceData* i_data;
    uint32 i_script_id;

    // Map local low guid counters
    ObjectGuidGenerator<HIGHGUID_UNIT> m_CreatureGuids;
    ObjectGuidGenerator<HIGHGUID_GAMEOBJECT> m_GameObjectGuids;
    ObjectGuidGenerator<HIGHGUID_DYNAMICOBJECT> m_DynObjectGuids;
    ObjectGuidGenerator<HIGHGUID_PET> m_PetGuids;

    CreatureGroupMgr m_creatureGrpMgr;

    // Hash-maps for quick lookup of game entities
    google::dense_hash_map<ObjectGuid, Creature*> creature_map_;
    google::dense_hash_map<ObjectGuid, Pet*> pet_map_;
    google::dense_hash_map<ObjectGuid, GameObject*> go_map_;
    google::dense_hash_map<ObjectGuid, DynamicObject*> do_map_;

    // Once grid insert/erase happens, these callbacks are executed
    template <typename T>
    void inserted_callback(T* obj);
    void inserted_callback(Player* player);
    template <typename T>
    void erased_callback(T* obj, bool destroy);
    void erased_callback(Player* player, bool destroy);

    // NOTE:  (x, y) is WoW cell coordinates, i.e. when the world is split up
    //        into 64x64 cells. This function is used by map_grids
    // NOTE2: It is okay to call this function more than once for the same cell,
    //        as it will only load the data once.
    void LoadMapAndVMap(int x, int y);

    void InCombatCreaturesClear();

    // Specialization for types that currently have a hash map provided in
    // Map.cpp
    template <typename T>
    void add_to_hash_lookup(T*)
    {
    }
    template <typename T>
    void remove_from_hash_lookup(T*)
    {
    }
};

class MANGOS_DLL_SPEC WorldMap : public Map
{
private:
    using Map::GetPersistentState; // hide in subclass for overwrite
public:
    WorldMap(uint32 id) : Map(id, 0, REGULAR_DIFFICULTY) {}
    ~WorldMap() {}

    // can't be NULL for loaded map
    WorldPersistentState* GetPersistanceState() const;
};

class MANGOS_DLL_SPEC DungeonMap : public Map
{
private:
    using Map::GetPersistentState; // hide in subclass for overwrite
public:
    DungeonMap(uint32 id, uint32 InstanceId, uint8 SpawnMode);
    ~DungeonMap();
    bool insert(Player*) override;
    void erase(Player*, bool) override;
    void Update(const uint32) override;
    void SendResetFailedNotify();
    bool Reset(InstanceResetMethod method);
    void PermBindAllPlayers(Creature* boss);
    void UnloadAll() override;
    bool CanEnter(Player* player, bool resurrect) override;
    void SendResetWarnings(uint32 timeLeft) const;
    void SetResetSchedule(bool on);

    // Can be null if dungeon has reset and map is having inhabitants ported out
    DungeonPersistentState* GetPersistanceState() const;

    virtual void InitVisibilityDistance() override;

private:
    ShortIntervalTimer m_coOwnersTimer;
    bool m_resetAfterUnload;
    bool m_unloadWhenEmpty;

    void BindOfflinePlayer(ObjectGuid guid, bool perm = true);
    void CheckPlayerCoOwnerStatus(Player* player);
};

class MANGOS_DLL_SPEC BattleGroundMap : public Map
{
private:
    using Map::GetPersistentState; // hide in subclass for overwrite
public:
    BattleGroundMap(uint32 id, uint32 InstanceId);
    ~BattleGroundMap();

    bool insert(Player*) override;
    void erase(Player*, bool) override;
    void Update(const uint32) override;
    bool CanEnter(Player* player, bool resurrect) override;
    void SetUnload();
    void UnloadAll() override;

    virtual void InitVisibilityDistance() override;
    BattleGround* GetBG() { return m_bg; }
    void SetBG(BattleGround* bg) { m_bg = bg; }

    // can't be NULL for loaded map
    BattleGroundPersistentState* GetPersistanceState() const;

private:
    BattleGround* m_bg;
};

#endif
