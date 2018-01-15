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

#ifndef _OBJECT_H
#define _OBJECT_H

#include "Camera.h"
#include "Common.h"
#include "LootMgr.h"
#include "ObjectGuid.h"
#include "Timer.h"
#include "UpdateData.h"
#include "UpdateFields.h"
#include "G3D/Vector3.h"
#include <set>
#include <string>
#include <unordered_map>

class ByteBuffer;
class Creature;
class Group;
class InstanceData;
class Map;
class Player;
class TerrainInfo;
class Transport;
class Unit;
class UpdateData;
class UpdateMask;
class WorldPacket;
class WorldSession;
class loot_distributor;
#define CONTACT_DISTANCE 0.5f
#define INTERACTION_DISTANCE 5.0f
#define ATTACK_DISTANCE 5.0f
#define MAX_VISIBILITY_DISTANCE \
    333.0f // max distance for visible object show, limited in 333 yards
#define DEFAULT_VISIBILITY_DISTANCE \
    90.0f // default visible distance, 90 yards on continents
#define DEFAULT_VISIBILITY_INSTANCE \
    120.0f // default visible distance in instances, 120 yards
#define DEFAULT_VISIBILITY_BGARENAS \
    180.0f // default visible distance in BG/Arenas, 180 yards
#define DEFAULT_SPECIALVIS_DISTANCE \
    300.0f // default visible MAX distance for special vis creature, 300 yards

#define DEFAULT_BOUNDING_RADIUS \
    0.388999998569489f // this is the default combat reached used for players &
                       // non-creatures (corresponds to
                       // creature_model_info.bounding_radius)
#define DEFAULT_COMBAT_REACH \
    1.5f // this is the default combat reached used for players (corresponds to
         // creature_model_info.combat_reach)
#define DEFAULT_OBJECT_SCALE \
    1.0f // player/item scale as default, npc/go from database, pets from dbc

#define MAX_STEALTH_DETECT_RANGE 30.0f

enum TempSummonType
{
    TEMPSUMMON_TIMED_OR_DEAD_DESPAWN =
        1, // despawns after a specified time OR when the creature disappears
    TEMPSUMMON_TIMED_OR_CORPSE_DESPAWN =
        2, // despawns after a specified time OR when the creature dies
    TEMPSUMMON_TIMED_DESPAWN = 3, // despawns after a specified time
    TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT = 4, // despawns after a specified
                                                // time after the creature is
                                                // out of combat
    TEMPSUMMON_CORPSE_DESPAWN = 5, // despawns instantly after death
    TEMPSUMMON_CORPSE_TIMED_DESPAWN =
        6,                       // despawns after a specified time after death
    TEMPSUMMON_DEAD_DESPAWN = 7, // despawns when the creature disappears
    TEMPSUMMON_MANUAL_DESPAWN = 8, // despawns when UnSummon() is called
    TEMPSUMMON_TIMED_DEATH = 9     // dies when duration ends, then despawns.
};

enum SummonOptions
{
    SUMMON_OPT_ACTIVE = 0x01,
    SUMMON_OPT_NO_LOOT = 0x02,
    SUMMON_OPT_NOT_COMBAT_SUMMON = 0x04,
    SUMMON_OPT_DESPAWN_ON_SUMMONER_DEATH = 0x08,
};

typedef std::unordered_map<Player*, UpdateData> UpdateDataMapType;

struct Position
{
    Position() : x(0.0f), y(0.0f), z(0.0f), o(0.0f) {}
    float x, y, z, o;

    void Get(float& x_, float& y_, float& z_) const
    {
        x_ = x;
        y_ = y;
        z_ = z;
    }
    void Get(float& x_, float& y_, float& z_, float& o_) const
    {
        x_ = x;
        y_ = y;
        z_ = z;
        o_ = o;
    }

    void Set(float x_, float y_, float z_, float o_)
    {
        x = x_;
        y = y_;
        z = z_;
        o = o_;
    }

    // modulos a radian orientation to the range of 0..2PI
    static float NormalizeOrientation(float o)
    {
        // fmod only supports positive numbers. Thus we have
        // to emulate negative numbers
        if (o < 0)
        {
            float mod = o * -1;
            mod = std::fmod(mod, 2.0f * static_cast<float>(M_PI));
            mod = -mod + 2.0f * static_cast<float>(M_PI);
            return mod;
        }
        return std::fmod(o, 2.0f * static_cast<float>(M_PI));
    }
};

struct WorldLocation
{
    uint32 mapid;
    float coord_x;
    float coord_y;
    float coord_z;
    float orientation;
    explicit WorldLocation(uint32 _mapid = 0, float _x = 0, float _y = 0,
        float _z = 0, float _o = 0)
      : mapid(_mapid), coord_x(_x), coord_y(_y), coord_z(_z), orientation(_o)
    {
    }
    WorldLocation(WorldLocation const& loc)
      : mapid(loc.mapid), coord_x(loc.coord_x), coord_y(loc.coord_y),
        coord_z(loc.coord_z), orientation(loc.orientation)
    {
    }
};

// use this class to measure time between world update ticks
// essential for units updating their spells after cells become active
class WorldUpdateCounter
{
public:
    WorldUpdateCounter() : m_tmStart(0) {}

    time_t timeElapsed()
    {
        if (!m_tmStart)
            m_tmStart = WorldTimer::tickPrevTime();

        return WorldTimer::getMSTimeDiff(m_tmStart, WorldTimer::tickTime());
    }

    void Reset() { m_tmStart = WorldTimer::tickTime(); }

private:
    uint32 m_tmStart;
};

class MANGOS_DLL_SPEC Object
{
public:
    virtual ~Object();

    virtual bool IsInWorld() const { return m_inWorld; }
    virtual void AddToWorld()
    {
        if (m_inWorld)
            return;

        m_inWorld = true;

        // synchronize values mirror with values array (changes will send in
        // updatecreate opcode any way
        ClearUpdateMask(false); // false - we can't have update data in update
                                // queue before adding to world
    }
    virtual void RemoveFromWorld()
    {
        // if we remove from world then sending changes not required
        ClearUpdateMask(true);
        m_inWorld = false;
    }

    ObjectGuid const& GetObjectGuid() const
    {
        return GetGuidValue(OBJECT_FIELD_GUID);
    }
    uint32 GetGUIDLow() const { return GetObjectGuid().GetCounter(); }
    PackedGuid const& GetPackGUID() const { return m_PackGUID; }
    std::string GetGuidStr() const { return GetObjectGuid().GetString(); }

    uint32 GetEntry() const { return GetUInt32Value(OBJECT_FIELD_ENTRY); }
    void SetEntry(uint32 entry) { SetUInt32Value(OBJECT_FIELD_ENTRY, entry); }

    float GetObjectScale() const
    {
        return m_floatValues[OBJECT_FIELD_SCALE_X] ?
                   m_floatValues[OBJECT_FIELD_SCALE_X] :
                   DEFAULT_OBJECT_SCALE;
    }

    void SetObjectScale(float newScale);

    uint8 GetTypeId() const { return m_objectTypeId; }
    bool isType(TypeMask mask) const { return (mask & m_objectType); }

    virtual void BuildCreateUpdateBlockForPlayer(
        UpdateData* data, Player* target) const;
    void SendCreateUpdateToPlayer(Player* player);

    // must be overwrite in appropriate subclasses (WorldObject, Item
    // currently), or will crash
    virtual void AddToClientUpdateList();
    virtual void RemoveFromClientUpdateList();
    virtual void BuildUpdateData(UpdateDataMapType& update_players);
    void MarkForClientUpdate();
    void SendForcedObjectUpdate(bool ignore_normal_update = false);

    void BuildValuesUpdateBlockForPlayer(
        UpdateData* data, Player* target) const;
    void BuildOutOfRangeUpdateBlock(UpdateData* data) const;
    void BuildMovementUpdateBlock(UpdateData* data, uint8 flags = 0) const;

    virtual void DestroyForPlayer(Player* target) const;

    const int32& GetInt32Value(uint16 index) const
    {
        assert(index < m_valuesCount || PrintIndexError(index, false));
        return m_int32Values[index];
    }

    const uint32& GetUInt32Value(uint16 index) const
    {
        assert(index < m_valuesCount || PrintIndexError(index, false));
        return m_uint32Values[index];
    }

    const uint64& GetUInt64Value(uint16 index) const
    {
        assert(index + 1 < m_valuesCount || PrintIndexError(index, false));
        return *((uint64*)&(m_uint32Values[index]));
    }

    const float& GetFloatValue(uint16 index) const
    {
        assert(index < m_valuesCount || PrintIndexError(index, false));
        return m_floatValues[index];
    }

    uint8 GetByteValue(uint16 index, uint8 offset) const
    {
        assert(index < m_valuesCount || PrintIndexError(index, false));
        assert(offset < 4);
        return *(((uint8*)&m_uint32Values[index]) + offset);
    }

    uint16 GetUInt16Value(uint16 index, uint8 offset) const
    {
        assert(index < m_valuesCount || PrintIndexError(index, false));
        assert(offset < 2);
        return *(((uint16*)&m_uint32Values[index]) + offset);
    }

    ObjectGuid const& GetGuidValue(uint16 index) const
    {
        return *reinterpret_cast<ObjectGuid const*>(&GetUInt64Value(index));
    }

    void SetInt32Value(uint16 index, int32 value);
    void SetUInt32Value(uint16 index, uint32 value);
    void SetUInt64Value(uint16 index, const uint64& value);
    void SetFloatValue(uint16 index, float value);
    void SetByteValue(uint16 index, uint8 offset, uint8 value);
    void SetUInt16Value(uint16 index, uint8 offset, uint16 value);
    void SetInt16Value(uint16 index, uint8 offset, int16 value)
    {
        SetUInt16Value(index, offset, (uint16)value);
    }
    void SetGuidValue(uint16 index, ObjectGuid const& value)
    {
        SetUInt64Value(index, value.GetRawValue());
    }
    void SetStatFloatValue(uint16 index, float value);
    void SetStatInt32Value(uint16 index, int32 value);

    void ApplyModUInt32Value(uint16 index, int32 val, bool apply);
    void ApplyModInt32Value(uint16 index, int32 val, bool apply);
    void ApplyModUInt64Value(uint16 index, int32 val, bool apply);
    void ApplyModPositiveFloatValue(uint16 index, float val, bool apply);
    void ApplyModSignedFloatValue(uint16 index, float val, bool apply);

    // The following function forces the index to be marked for update without
    // changing the value
    void UpdateValueIndex(uint16 index);

    void ApplyPercentModFloatValue(uint16 index, float val, bool apply)
    {
        val = val != -100.0f ? val : -99.9f;
        SetFloatValue(index,
            GetFloatValue(index) *
                (apply ? (100.0f + val) / 100.0f : 100.0f / (100.0f + val)));
    }

    void SetFlag(uint16 index, uint32 newFlag);
    void RemoveFlag(uint16 index, uint32 oldFlag);

    void ToggleFlag(uint16 index, uint32 flag)
    {
        if (HasFlag(index, flag))
            RemoveFlag(index, flag);
        else
            SetFlag(index, flag);
    }

    bool HasFlag(uint16 index, uint32 flag) const
    {
        assert(index < m_valuesCount || PrintIndexError(index, false));
        return (m_uint32Values[index] & flag) != 0;
    }

    void ApplyModFlag(uint16 index, uint32 flag, bool apply)
    {
        if (apply)
            SetFlag(index, flag);
        else
            RemoveFlag(index, flag);
    }

    void SetByteFlag(uint16 index, uint8 offset, uint8 newFlag);
    void RemoveByteFlag(uint16 index, uint8 offset, uint8 newFlag);

    void ToggleByteFlag(uint16 index, uint8 offset, uint8 flag)
    {
        if (HasByteFlag(index, offset, flag))
            RemoveByteFlag(index, offset, flag);
        else
            SetByteFlag(index, offset, flag);
    }

    bool HasByteFlag(uint16 index, uint8 offset, uint8 flag) const
    {
        assert(index < m_valuesCount || PrintIndexError(index, false));
        assert(offset < 4);
        return (((uint8*)&m_uint32Values[index])[offset] & flag) != 0;
    }

    void ApplyModByteFlag(uint16 index, uint8 offset, uint32 flag, bool apply)
    {
        if (apply)
            SetByteFlag(index, offset, flag);
        else
            RemoveByteFlag(index, offset, flag);
    }

    void SetShortFlag(uint16 index, bool highpart, uint16 newFlag);
    void RemoveShortFlag(uint16 index, bool highpart, uint16 oldFlag);

    void ToggleShortFlag(uint16 index, bool highpart, uint8 flag)
    {
        if (HasShortFlag(index, highpart, flag))
            RemoveShortFlag(index, highpart, flag);
        else
            SetShortFlag(index, highpart, flag);
    }

    bool HasShortFlag(uint16 index, bool highpart, uint8 flag) const
    {
        assert(index < m_valuesCount || PrintIndexError(index, false));
        return (((uint16*)&m_uint32Values[index])[highpart ? 1 : 0] & flag) !=
               0;
    }

    void ApplyModShortFlag(uint16 index, bool highpart, uint32 flag, bool apply)
    {
        if (apply)
            SetShortFlag(index, highpart, flag);
        else
            RemoveShortFlag(index, highpart, flag);
    }

    void SetFlag64(uint16 index, uint64 newFlag)
    {
        uint64 oldval = GetUInt64Value(index);
        uint64 newval = oldval | newFlag;
        SetUInt64Value(index, newval);
    }

    void RemoveFlag64(uint16 index, uint64 oldFlag)
    {
        uint64 oldval = GetUInt64Value(index);
        uint64 newval = oldval & ~oldFlag;
        SetUInt64Value(index, newval);
    }

    void ToggleFlag64(uint16 index, uint64 flag)
    {
        if (HasFlag64(index, flag))
            RemoveFlag64(index, flag);
        else
            SetFlag64(index, flag);
    }

    bool HasFlag64(uint16 index, uint64 flag) const
    {
        assert(index < m_valuesCount || PrintIndexError(index, false));
        return (GetUInt64Value(index) & flag) != 0;
    }

    void ApplyModFlag64(uint16 index, uint64 flag, bool apply)
    {
        if (apply)
            SetFlag64(index, flag);
        else
            RemoveFlag64(index, flag);
    }

    void ClearUpdateMask(bool remove);

    bool LoadValues(const char* data);

    uint16 GetValuesCount() const { return m_valuesCount; }

    void InitValues() { _InitValues(); }

    virtual bool HasQuest(uint32 /* quest_id */) const { return false; }
    virtual bool HasInvolvedQuest(uint32 /* quest_id */) const { return false; }

    // A Loot Distributor distributes the loot we drop; not all objects has one
    // (in which case there's no need to override this)
    virtual loot_distributor* GetLootDistributor() const
    {
        return m_lootDistributor;
    }
    virtual void OnLootOpen(LootType /*lootType*/, Player* /*looter*/) {}
    void DeleteLootDistributor();

    // FIXME: This is a hack. See function definition for more info.
    void ForceUpdateDynflag();
    void ForceUpdateDynflagForPlayer(Player* plr);

    virtual void resend_health();

protected:
    Object();

    void _InitValues();
    void _Create(uint32 guidlow, uint32 entry, HighGuid guidhigh);

    virtual void _SetUpdateBits(UpdateMask* updateMask, Player* target) const;

    virtual void _SetCreateBits(UpdateMask* updateMask, Player* target) const;

    void BuildMovementUpdate(ByteBuffer* data, uint8 updateFlags) const;
    void BuildValuesUpdate(uint8 updatetype, ByteBuffer* data,
        UpdateMask* updateMask, Player* target) const;
    void BuildUpdateDataForPlayer(
        Player* pl, UpdateDataMapType& update_players);

    uint16 m_objectType;

    uint8 m_objectTypeId;
    uint8 m_updateFlag;

    union
    {
        int32* m_int32Values;
        uint32* m_uint32Values;
        float* m_floatValues;
    };

    bool* _changedFields;

    uint16 m_valuesCount;

    bool m_objectUpdated;

    // Distributes the loot we drop
    loot_distributor* m_lootDistributor;

private:
    bool m_inWorld;

    PackedGuid m_PackGUID;

    Object(const Object&);            // prevent generation copy constructor
    Object& operator=(Object const&); // prevent generation assigment operator

    // FIXME: This is a hack. See ForceUpdateDynflagForPlayer()
    mutable std::set<ObjectGuid> m_forceUpdateDynflags;
    bool m_forceDynForEveryone;

public:
    // for output helpfull error messages from ASSERTs
    bool PrintIndexError(uint32 index, bool set) const;
    bool PrintEntryError(char const* descr) const;
};

struct MovementInfo
{
    // common
    uint32 moveFlags; // see enum MovementFlags
    uint8 moveFlags2;
    uint32 time;
    Position pos;

    // transport
    struct TransportInfo
    {
        void Reset()
        {
            guid.Clear();
            pos.Set(0.0f, 0.0f, 0.0f, 0.0f);
            time = 0;
        }

        ObjectGuid guid;
        Position pos;
        uint32 time;
    } transport;

    // swimming/fling
    float pitch;

    // falling
    uint32 fallTime;

    // jumping
    struct JumpInfo
    {
        void Reset() { zspeed = sinAngle = cosAngle = xyspeed = 0.0f; }

        float zspeed, sinAngle, cosAngle, xyspeed;
    } jump;

    // spline
    float splineElevation;

    MovementInfo()
      : moveFlags(0), moveFlags2(0), time(0), pitch(0.0f), fallTime(0),
        splineElevation(0.0f)
    {
        transport.Reset();
        jump.Reset();
    }

    // Read/Write methods
    void Read(ByteBuffer& data);
    void Write(ByteBuffer& data) const;

    // Movement flags manipulations
    void AddMovementFlag(uint32 f) { moveFlags |= f; }
    void RemoveMovementFlag(uint32 f) { moveFlags &= ~f; }
    bool HasMovementFlag(uint32 f) const { return moveFlags & f; }
    uint32 GetMovementFlags() const { return moveFlags; }
    void SetMovementFlags(uint32 f) { moveFlags = f; }
};

inline ByteBuffer& operator<<(ByteBuffer& buf, MovementInfo const& mi)
{
    mi.Write(buf);
    return buf;
}

inline ByteBuffer& operator>>(ByteBuffer& buf, MovementInfo& mi)
{
    mi.Read(buf);
    return buf;
}

class MANGOS_DLL_SPEC WorldObject : public Object
{
public:
    virtual ~WorldObject() {}

    bool IsInWorld() const override { return Object::IsInWorld() && m_currMap; }

    void _Create(uint32 guidlow, HighGuid guidhigh);

    // Relocate may only be called directly BEFORE an object is added to a map.
    // After that use Map::Relocate, which will invoke this automatically.
    // Calling this functions after Map::Add (except from the grid system) is
    // undefined behavior.
    // Also any non-Unit (i.e. not a NPC or Player) cannot relocate after
    // they're added to the map.
    void Relocate(float x, float y, float z);

    void SetOrientation(float orientation);

    float GetX() const { return m_position.x; }
    float GetY() const { return m_position.y; }
    float GetZ() const { return m_position.z; }
    float GetO() const { return m_position.o; }
    void GetPosition(float& x, float& y, float& z) const
    {
        x = m_position.x;
        y = m_position.y;
        z = m_position.z;
    }
    void GetPosition(WorldLocation& loc) const
    {
        loc.mapid = m_mapId;
        GetPosition(loc.coord_x, loc.coord_y, loc.coord_z);
        loc.orientation = GetO();
    }

    // Searches for a point with a relative orientation, and a maximum distance
    // dist: exact distance, bounding box radius not added
    G3D::Vector2 GetPoint2d(float rel_ori, float dist) const;

    // Searches for a point in a relative orientation, with a maximum distance
    // dist: exact distance, bounding box radius not added
    // normalize_z: ONLY set to true if the point will be used by a player
    // extended_z: set to true when we need to look further than "normal" to
    // find the ground; for example if blink while falling
    // local: if true, returns coordinates in object space if on a transport
    // NOTE: Will always return a valid position
    G3D::Vector3 GetPoint(float rel_ori, float dist, bool normalize_z = false,
        bool extended_z = false, bool local = false) const;
    // obj: will use angle between obj and *this to determine orientation of
    // point
    // dist: now + bounding radius of obj and *this, as it's assumed to be
    // wanted in most cases
    G3D::Vector3 GetPoint(WorldObject* obj, float dist = 0.0f,
        bool normalize_z = false, bool extended_z = false,
        bool local = false) const;

    // Use a different start location than *this for getting a point
    // WARNING: X,Y,Z has to be a valid location!!! If in doubt, use normal
    // GetPoint
    // WARNING2: If on a transport, coordinates must be in object space!!! If in
    // doubt, use normal GetPoint
    G3D::Vector3 GetPointXYZ(const G3D::Vector3& start, float abs_angle,
        float dist, bool normalize_z = false, bool extended_z = false,
        bool local = false) const;

    bool same_floor(const WorldObject* obj) const;

    virtual float GetObjectBoundingRadius() const
    {
        return DEFAULT_BOUNDING_RADIUS;
    }

    bool IsPositionValid() const;
    void UpdateGroundPositionZ(float x, float y, float& z) const;
    void UpdateAllowedPositionZ(float x, float y, float& z) const;

    uint32 GetMapId() const { return m_mapId; }
    uint32 GetInstanceId() const { return m_InstanceId; }

    virtual void UpdateZoneAreaCache();
    uint32 GetZoneId() const { return cached_zone_; }
    uint32 GetAreaId() const { return cached_area_; }
    void GetZoneAndAreaId(uint32& zoneid, uint32& areaid) const
    {
        zoneid = cached_zone_;
        areaid = cached_area_;
    }

    InstanceData* GetInstanceData() const;

    const char* GetName() const { return m_name.c_str(); }
    void SetName(const std::string& newname) { m_name = newname; }

    virtual const char* GetNameForLocaleIdx(int32 /*locale_idx*/) const
    {
        return GetName();
    }

    float GetDistance(const WorldObject* obj) const;
    float GetDistance(float x, float y, float z) const;
    float GetDistance2d(const WorldObject* obj) const;
    float GetDistance2d(float x, float y) const;
    float GetDistanceZ(const WorldObject* obj) const;
    bool IsInMap(const WorldObject* obj) const
    {
        return IsInWorld() && obj->IsInWorld() && (GetMap() == obj->GetMap());
    }
    bool IsWithinDist3d(float x, float y, float z, float dist2compare,
        bool inclBounding = true) const;
    bool IsWithinDist2d(
        float x, float y, float dist2compare, bool inclBounding = true) const;
    bool _IsWithinDist(WorldObject const* obj, float dist2compare, bool is3D,
        bool inclBoundingBox = true) const;

    // use only if you will sure about placing both object at same map
    bool IsWithinDist(WorldObject const* obj, float dist2compare,
        bool is3D = true, bool inclBoundingBox = true) const
    {
        return obj && _IsWithinDist(obj, dist2compare, is3D, inclBoundingBox);
    }

    bool IsWithinDistInMap(WorldObject const* obj, float dist2compare,
        bool is3D = true, bool inclBoundingBox = true) const
    {
        return obj && IsInMap(obj) &&
               _IsWithinDist(obj, dist2compare, is3D, inclBoundingBox);
    }
    bool IsWithinLOSInMap(const WorldObject* obj) const;
    bool IsWithinLOS(float x, float y, float z) const;
    bool IsWithinWmoLOSInMap(const WorldObject* obj) const;
    bool IsWithinWmoLOS(float x, float y, float z) const;
    bool GetDistanceOrder(WorldObject const* obj1, WorldObject const* obj2,
        bool is3D = true) const;
    bool IsInRange(WorldObject const* obj, float minRange, float maxRange,
        bool is3D = true) const;
    bool IsInRange2d(float x, float y, float minRange, float maxRange) const;
    bool IsInRange3d(
        float x, float y, float z, float minRange, float maxRange) const;

    float GetAngle(const WorldObject* obj) const;
    float GetAngle(const float x, const float y) const;
    bool HasInArc(const float arcangle, const WorldObject* obj) const;
    bool HasInArc(const float arcangle, const float posAngle) const;
    bool isInFrontInMap(
        WorldObject const* target, float distance, float arc = M_PI) const;
    bool isInBackInMap(
        WorldObject const* target, float distance, float arc = M_PI) const;
    bool isInFront(
        WorldObject const* target, float distance, float arc = M_PI) const;
    bool isInBack(
        WorldObject const* target, float distance, float arc = M_PI) const;

    virtual void CleanupsBeforeDelete(); // used in destructor or explicitly
                                         // before mass creature delete to
                                         // remove cross-references to already
                                         // deleted units

    void SendMessageToSet(WorldPacket* data, bool self);
    void SendMessageToSetInRange(WorldPacket* data, float dist, bool self);
    void SendMessageToSetExcept(WorldPacket* data, const Player* skip);

    void MonsterSay(const char* text, uint32 language, Unit* target = nullptr);
    void MonsterSayF(const char* format, uint32 language, Unit* target, ...)
        ATTR_PRINTF(2, 5);
    void MonsterYell(const char* text, uint32 language, Unit* target = nullptr);
    void MonsterTextEmote(
        const char* text, Unit* target, bool IsBossEmote = false);
    void MonsterWhisper(
        const char* text, Unit* target, bool IsBossWhisper = false);
    void MonsterSay(int32 textId, uint32 language, Unit* target = nullptr);
    void MonsterYell(int32 textId, uint32 language, Unit* target = nullptr);
    void MonsterTextEmote(int32 textId, Unit* target, bool IsBossEmote = false);
    void MonsterWhisper(
        int32 textId, Unit* receiver, bool IsBossWhisper = false);
    void MonsterYellToZone(int32 textId, uint32 language, Unit* target);
    static void BuildMonsterChat(WorldPacket* data, ObjectGuid senderGuid,
        uint8 msgtype, char const* text, uint32 language, char const* name,
        ObjectGuid targetGuid, char const* targetName);

    void PlayDistanceSound(uint32 sound_id, Player* target = nullptr);
    void PlayDirectSound(uint32 sound_id, Player* target = nullptr);

    void SendObjectDeSpawnAnim(ObjectGuid guid);
    void SendGameObjectCustomAnim(ObjectGuid guid, uint32 animId = 0);

    virtual bool IsHostileTo(Unit const* unit) const = 0;
    virtual bool IsFriendlyTo(Unit const* unit) const = 0;
    bool IsControlledByPlayer() const;

    virtual void SaveRespawnTime() {}
    void AddObjectToRemoveList();

    void UpdateObjectVisibility();
    virtual void UpdateVisibilityAndView(); // update visibility for object and
                                            // object for all around

    // main visibility check function in normal case (ignore grey zone distance
    // check)
    bool isVisibleFor(Player const* u, WorldObject const* viewPoint) const
    {
        return isVisibleForInState(u, viewPoint, false);
    }

    // low level function for visibility change code, must be define in all main
    // world object subclasses
    virtual bool isVisibleForInState(Player const* u,
        WorldObject const* viewPoint, bool inVisibleList) const = 0;

    void SetMap(Map* map);
    Map* GetMap() const
    {
        assert(m_currMap);
        return m_currMap;
    }
    // used to check all object's GetMap() calls when object is not in world!
    void ResetMap() { m_currMap = nullptr; }

    // obtain terrain data for map where this object belong...
    TerrainInfo const* GetTerrain() const;

    void AddToClientUpdateList() override;
    void RemoveFromClientUpdateList() override;
    void BuildUpdateData(UpdateDataMapType&) override;

    // Transports
    Transport* GetTransport() const { return m_transport; }
    float GetTransOffsetX() const { return m_movementInfo.transport.pos.x; }
    float GetTransOffsetY() const { return m_movementInfo.transport.pos.y; }
    float GetTransOffsetZ() const { return m_movementInfo.transport.pos.z; }
    float GetTransOffsetO() const { return m_movementInfo.transport.pos.o; }
    uint32 GetTransTime() const { return m_movementInfo.transport.time; }
    void SetTransport(Transport* t) { m_transport = t; }

    MovementInfo m_movementInfo;

    // level -1: don't override level (use db data)
    // summon_options: enum SummonOptions
    Creature* SummonCreature(uint32 id, float x, float y, float z, float ang,
        TempSummonType spwtype, uint32 despwtime, uint32 summon_options = 0,
        int level = -1);
    GameObject* SummonGameObject(uint32 entry, float x, float y, float z,
        float ang, float rotation0, float rotation1, float rotation2,
        float rotation3, uint32 respawnTime, bool summonWild = false);

    Creature* FindNearestCreature(
        uint32 entry, float range, bool alive = true) const;
    GameObject* FindNearestGameObject(uint32 entry, float range) const;
    Player* FindNearestPlayer(float dist) const;

    bool isActiveObject() const
    {
        return m_isActiveObject || m_viewPoint.hasViewers();
    }
    void SetActiveObjectState(bool active);

    ViewPoint& GetViewPoint() { return m_viewPoint; }

    // ASSERT print helper
    bool PrintCoordinatesError(
        float x, float y, float z, char const* descr) const;

    virtual void StartGroupLoot(Group* /*group*/, uint32 /*timer*/) {}

    // Used by grid system to correctly adjust update diff
    WorldUpdateCounter& GetUpdateTracker() { return m_updateTracker; }

    void queue_action(uint32 delay, std::function<void()> func)
    {
        queued_actions_.emplace_back(std::move(func), delay);
    }

    // ticks: minimum # of server ticks until action is executed
    void queue_action_ticks(uint32 ticks, std::function<void()> func)
    {
        queued_tick_actions_.emplace_back(std::move(func), ticks);
    }

    float GetLosHeight() const { return m_losHeight; }
    void SetLosHeight(float height) { m_losHeight = height; }

protected:
    explicit WorldObject();

    bool has_queued_actions() const
    {
        return !queued_actions_.empty() || !queued_tick_actions_.empty();
    }
    void update_queued_actions(uint32 diff);
    struct queued_action_t
    {
        queued_action_t(std::function<void()> f_, uint32 t_)
          : func(std::move(f_)), timer(t_)
        {
        }

        std::function<void()> func;
        uint32 timer;
    };
    std::vector<queued_action_t> queued_actions_;
    std::vector<queued_action_t> queued_tick_actions_;

    // these functions are used mostly for Relocate() and Corpse/Player specific
    // stuff...
    // use them ONLY in LoadFromDB()/Create() funcs and nowhere else!
    // mapId/instanceId should be set in SetMap() function!
    void SetLocationMapId(uint32 _mapId) { m_mapId = _mapId; }
    void SetLocationInstanceId(uint32 _instanceId)
    {
        m_InstanceId = _instanceId;
    }

    std::string m_name;

    uint32 cached_zone_;
    uint32 cached_area_;

private:
    Map* m_currMap; // current object's Map location

    uint32 m_mapId;      // object at map with map_id
    uint32 m_InstanceId; // in map copy with instance id

    Position m_position;
    ViewPoint m_viewPoint;
    float m_losHeight;
    WorldUpdateCounter m_updateTracker;
    bool m_isActiveObject;

    Transport* m_transport;

public:
    bool pending_map_insert; // True between Map::insert and
                             // Map::inserted_callback
};

#endif
