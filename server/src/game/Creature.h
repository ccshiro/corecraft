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

#ifndef MANGOSSERVER_CREATURE_H
#define MANGOSSERVER_CREATURE_H

#include "Common.h"
#include "DBCEnums.h"
#include "ItemPrototype.h"
#include "LootMgr.h"
#include "SharedDefines.h"
#include "Unit.h"
#include "UpdateMask.h"
#include <list>
#include <unordered_map>

class CreatureAI;
class CreatureGroup;
struct GameEventCreatureData;
class Group;
class Player;
class Quest;
struct SpellEntry;
class WorldSession;
class pet_behavior;
struct pet_template;

enum CreatureFlagsExtra
{
    CREATURE_FLAG_EXTRA_INSTANCE_BIND = 0x00000001, // creature kill bind
                                                    // instance with killer and
                                                    // killer's group
    CREATURE_FLAG_EXTRA_CIVILIAN =
        0x00000002, // not aggro (ignore faction/reputation hostility)
    CREATURE_FLAG_EXTRA_NO_PARRY = 0x00000004, // creature can't parry
    CREATURE_FLAG_EXTRA_NO_PARRY_HASTEN =
        0x00000008, // creature can't counter-attack at parry
    CREATURE_FLAG_EXTRA_NO_BLOCK = 0x00000010, // creature can't block
    CREATURE_FLAG_EXTRA_NO_CRUSH =
        0x00000020, // creature can't do crush attacks
    CREATURE_FLAG_EXTRA_NO_XP_AT_KILL =
        0x00000040,                             // creature kill not provide XP
    CREATURE_FLAG_EXTRA_INVISIBLE = 0x00000080, // creature is always invisible
                                                // for player (mostly trigger
                                                // creatures)
    CREATURE_FLAG_EXTRA_NOT_TAUNTABLE =
        0x00000100, // creature is immune to taunt auras and effect attack me
    CREATURE_FLAG_EXTRA_AGGRO_ZONE =
        0x00000200, // creature sets itself in combat with zone on aggro
    CREATURE_FLAG_EXTRA_GUARD = 0x00000400, // creature is a guard
    CREATURE_FLAG_EXTRA_NO_TALKTO_CREDIT =
        0x00000800, // creature doesn't give quest-credits when talked to
                    // (temporarily flag)
    CREATURE_FLAG_EXTRA_DUNGEON_BOSS = 0x00001000, // creature is a dungeon boss
    CREATURE_FLAG_NO_LOOT_SELECTION = 0x00002000,  // creature does not drop
                                                   // anything from the standard
                                                   // loot_selection.h
    CREATURE_FLAG_USE_DUAL_WIELD = 0x00004000,     // Allows creatures without a
                                                   // visual weapon in their
                                                   // off-hand to dual wield
    CREATURE_FLAG_IGNORED_BY_NPCS =
        0x00008000, // creature is not attacked by other creatures
    CREATURE_FLAG_EXTRA_NO_AGGRO_PULSE =
        0x00010000, // creature does not invoke Creature::AggroPulse
    CREATURE_FLAG_EXTRA_NO_NEGATIVE_STAT_MODS = 0x00020000,
    // NPC will only attack targets on its threat-list, or those that are PVP
    // flagged; used for Warlock's Infernal and Doomguard
    CREATURE_FLAG_EXTRA_AGGRESSIVE_PLAYER_DEMON = 0x00040000,
    CREATURE_FLAG_EXTRA_CHASE_GEN_NO_BACKING = 0x00080000,
    // Casting speed not slowed down by mind-numbing, curse of tongues, etc
    CREATURE_FLAG_EXTRA_IMMUNE_TO_HASTE_DECREASE = 0x00100000,
    // Immune to all spells with DispelType=poison
    CREATURE_FLAG_EXTRA_IMMUNE_TO_POISON = 0x00200000,
    // Immune to all spells with DispelType=disease
    CREATURE_FLAG_EXTRA_IMMUNE_TO_DISEASE = 0x00400000,
    // Immune to life drain effects
    CREATURE_FLAG_EXTRA_IMMUNE_TO_LIFE_STEAL = 0x00800000,
    // Immune to mana drain & burn effects
    CREATURE_FLAG_EXTRA_IMMUNE_TO_MANA_BURN = 0x01000000,
};

// GCC have alternative #pragma pack(N) syntax and old gcc version not support
// pack(push,N), also any gcc version not support it at some platform
#if defined(__GNUC__)
#pragma pack(1)
#else
#pragma pack(push, 1)
#endif

#define MAX_KILL_CREDIT 2
#define MAX_CREATURE_MODEL 4

// if a mob is incapable of pathing to the target for this period of time, it
// will reset fully
#define MOB_EVADE_FULL_RESET_TIMER 25 * IN_MILLISECONDS
// how often the mob regenerates health when evading
#define MOB_EVADE_HP_TICK_TIMER 5 * IN_MILLISECONDS

#define KITING_LEASH_RADIUS 60.0f
#define KITING_LEASH_TELEPORT 200.0f

// from `creature_template` table
struct CreatureInfo
{
    uint32 Entry;
    uint32 HeroicEntry;
    uint32 KillCredit[MAX_KILL_CREDIT];
    uint32 ModelId[MAX_CREATURE_MODEL];
    char* Name;
    char* SubName;
    char* IconName;
    uint32 GossipMenuId;
    uint32 minlevel;
    uint32 maxlevel;
    uint32 minhealth;
    uint32 maxhealth;
    uint32 minmana;
    uint32 maxmana;
    uint32 armor;
    uint32 faction_A;
    uint32 faction_H;
    uint32 npcflag;
    float speed_walk;
    float speed_run;
    float scale;
    uint32 rank;
    float mindmg;
    float maxdmg;
    uint32 dmgschool;
    uint32 attackpower;
    float dmg_multiplier;
    uint32 baseattacktime;
    uint32 rangeattacktime;
    uint32 unit_class; // enum Classes. Note only 4 classes are known for
                       // creatures.
    uint32 unit_flags; // enum UnitFlags mask values
    uint32 dynamicflags;
    uint32 family; // enum CreatureFamily values (optional)
    uint32 trainer_type;
    uint32 trainer_spell;
    uint32 trainer_class;
    uint32 trainer_race;
    float minrangedmg;
    float maxrangedmg;
    uint32 rangedattackpower;
    uint32 type;       // enum CreatureType values
    uint32 type_flags; // enum CreatureTypeFlags mask values
    uint32 lootid;
    uint32 pickpocketLootId;
    uint32 SkinLootId;
    int32 resistance1;
    int32 resistance2;
    int32 resistance3;
    int32 resistance4;
    int32 resistance5;
    int32 resistance6;
    uint32 spells[CREATURE_MAX_SPELLS];
    uint32 PetSpellDataId;
    uint32 mingold;
    uint32 maxgold;
    char const* AIName;
    uint32 MovementType;
    uint32 InhabitType;
    float unk16;
    float unk17;
    bool RacialLeader;
    bool RegenHealth;
    bool RegenMana;
    uint32 equipmentId;
    uint32 trainerId;
    uint32 vendorId;
    uint32 MechanicImmuneMask;
    uint32 flags_extra;
    uint32 special_visibility;
    float aggro_radius;
    float chain_radius;
    uint32 ScriptID;

    // helpers
    static HighGuid GetHighGuid()
    {
        return HIGHGUID_UNIT; // in pre-3.x always HIGHGUID_UNIT
    }

    ObjectGuid GetObjectGuid(uint32 lowguid) const
    {
        return ObjectGuid(GetHighGuid(), Entry, lowguid);
    }

    SkillType GetRequiredLootSkill() const
    {
        if (type_flags & CREATURE_TYPEFLAGS_HERBLOOT)
            return SKILL_HERBALISM;
        else if (type_flags & CREATURE_TYPEFLAGS_MININGLOOT)
            return SKILL_MINING;
        else
            return SKILL_SKINNING; // normal case
    }

    bool isTameable() const
    {
        return type == CREATURE_TYPE_BEAST && family != 0 &&
               (type_flags & CREATURE_TYPEFLAGS_TAMEABLE);
    }

    uint32 GetRandomValidModelId() const;
    uint32 GetFirstValidModelId() const;
};

struct EquipmentInfo
{
    uint32 entry;
    uint32 equipentry[3];
};

// depricated old way
struct EquipmentInfoRaw
{
    uint32 entry;
    uint32 equipmodel[3];
    uint32 equipinfo[3];
    uint32 equipslot[3];
};

// from `creature` table
struct CreatureData
{
    uint32 id;   // entry in creature_template
    uint32 guid; // guid in creature
    uint16 mapid;
    uint32 modelid_override; // overrides any model defined in creature_template
    int32 equipmentId;
    float posX;
    float posY;
    float posZ;
    float orientation;
    uint32 spawntimesecs;
    float spawndist;
    uint32 currentwaypoint;
    uint32 curhealth;
    uint32 curmana;
    bool is_dead;
    uint8 movementType;
    uint8 spawnMask;
    uint32 boss_link_entry;
    uint32 boss_link_guid;
    float leash_x;
    float leash_y;
    float leash_z;
    float leash_radius;
    float aggro_radius;
    float chain_radius;
    float special_visibility;

    // helper function
    ObjectGuid GetObjectGuid(uint32 lowguid) const
    {
        return ObjectGuid(CreatureInfo::GetHighGuid(), id, lowguid);
    }
};

enum SplineFlags
{
    SPLINEFLAG_WALKMODE = 0x0000100,
    SPLINEFLAG_FLYING = 0x0000200,
};

// from `creature_addon` and `creature_template_addon`tables
struct CreatureDataAddon
{
    uint32 guidOrEntry;
    uint32 mount;
    uint32 bytes1;
    uint8 sheath_state; // SheathState
    uint8 flags;        // UnitBytes2_Flags
    uint32 emote;
    uint32 move_flags;
    const char* quest_vis; // Check Creature::MeetsQuestVisibility(Player*
                           // player) for documentation
    uint32 const* auras;   // loaded as char* "spell1 spell2 ... "
};

struct CreatureModelInfo
{
    uint32 modelid;
    float bounding_radius;
    float combat_reach;
    float los_height;
    uint8 gender;
    uint32 modelid_other_gender; // The oposite gender for this modelid
                                 // (male/female)
    uint32
        modelid_alternative; // An alternative model. Generally same gender(2)
};

struct CreatureModelRace
{
    uint32 modelid;  // Native model/base model the selection is for
    uint32 racemask; // Races it applies to (and then a player source must exist
                     // for selection)
    uint32
        creature_entry; // Modelid from creature_template.entry will be selected
    uint32 modelid_racial; // Explicit modelid. Used if creature_template entry
                           // is not defined
};

// GCC have alternative #pragma pack() syntax and old gcc version not support
// pack(pop), also any gcc version not support it at some platform
#if defined(__GNUC__)
#pragma pack()
#else
#pragma pack(pop)
#endif

struct CreatureLocale
{
    std::vector<std::string> Name;
    std::vector<std::string> SubName;
};

struct GossipMenuItemsLocale
{
    std::vector<std::string> OptionText;
    std::vector<std::string> BoxText;
};

struct PointOfInterestLocale
{
    std::vector<std::string> IconName;
};

enum InhabitTypeValues
{
    INHABIT_GROUND = 1,
    INHABIT_WATER = 2,
    INHABIT_AIR = 4,
    INHABIT_ANYWHERE = INHABIT_GROUND | INHABIT_WATER | INHABIT_AIR
};

// Enums used by StringTextData::Type (CreatureEventAI)
enum ChatType
{
    CHAT_TYPE_SAY = 0,
    CHAT_TYPE_YELL = 1,
    CHAT_TYPE_TEXT_EMOTE = 2,
    CHAT_TYPE_BOSS_EMOTE = 3,
    CHAT_TYPE_WHISPER = 4,
    CHAT_TYPE_BOSS_WHISPER = 5,
    CHAT_TYPE_ZONE_YELL = 6
};

// Selection method used by SelectAttackingTarget
enum AttackingTarget
{
    ATTACKING_TARGET_RANDOM = 0,  // Just selects a random target
    ATTACKING_TARGET_TOPAGGRO,    // Selects targes from top aggro to bottom
    ATTACKING_TARGET_BOTTOMAGGRO, // Selects targets from bottom aggro to top
};

enum SelectFlags
{
    SELECT_FLAG_IN_LOS =
        0x001, // Default Selection Requirement for Spell-targets
    SELECT_FLAG_PLAYER = 0x002,
    SELECT_FLAG_POWER_MANA = 0x004, // For Energy based spells, like manaburn
    SELECT_FLAG_POWER_RAGE = 0x008,
    SELECT_FLAG_POWER_ENERGY = 0x010,
    SELECT_FLAG_IN_MELEE_RANGE = 0x040,
    SELECT_FLAG_NOT_IN_MELEE_RANGE = 0x080,
    SELECT_FLAG_FARTHEST_AWAY = 0x100,
    SELECT_FLAG_IN_FRONT = 0x200,
    SELECT_FLAG_IGNORE_TARGETS_WITH_AURA = 0x400,
};

// Vendors
struct VendorItem
{
    VendorItem(uint32 _item, uint32 _maxcount, uint32 _incrtime,
        uint32 _extendedCost, int32 _weight)
      : item(_item), maxcount(_maxcount), incrtime(_incrtime),
        ExtendedCost(_extendedCost), weight(_weight)
    {
    }

    uint32 item;
    uint32 maxcount;     // 0 for infinity item amount
    uint32 incrtime;     // time for restore items amount if maxcount != 0
    uint32 ExtendedCost; // index in ItemExtendedCost.dbc
    int32 weight;        // For sorting purposes
};

struct VendorItemData
{
    typedef std::vector<VendorItem> VendorItemList;
    VendorItemList m_items;

    const VendorItem* GetItem(uint32 slot) const
    {
        if (slot >= m_items.size())
            return nullptr;
        return &m_items[slot];
    }
    bool Empty() const { return m_items.empty(); }
    uint8 GetItemCount() const { return m_items.size(); }
    void AddItem(uint32 item, uint32 maxcount, uint32 ptime,
        uint32 ExtendedCost, uint32 weight);
    bool RemoveItem(uint32 item_id);
    const VendorItem* FindItem(uint32 item_id) const;
    size_t FindItemSlot(uint32 item_id) const;

    void Clear() { m_items.clear(); }
};

struct VendorItemCount
{
    explicit VendorItemCount(uint32 _item, uint32 _count)
      : itemId(_item), count(_count),
        lastIncrementTime(WorldTimer::time_no_syscall())
    {
    }

    uint32 itemId;
    uint32 count;
    time_t lastIncrementTime;
};

typedef std::list<VendorItemCount> VendorItemCounts;

struct TrainerSpell
{
    TrainerSpell()
      : spell(0), spellCost(0), reqSkill(0), reqSkillValue(0), reqLevel(0),
        isProvidedReqLevel(false)
    {
    }

    TrainerSpell(uint32 _spell, uint32 _spellCost, uint32 _reqSkill,
        uint32 _reqSkillValue, uint32 _reqLevel, bool _isProvidedReqLevel)
      : spell(_spell), spellCost(_spellCost), reqSkill(_reqSkill),
        reqSkillValue(_reqSkillValue), reqLevel(_reqLevel),
        isProvidedReqLevel(_isProvidedReqLevel)
    {
    }

    uint32 spell;
    uint32 spellCost;
    uint32 reqSkill;
    uint32 reqSkillValue;
    uint32 reqLevel;
    bool isProvidedReqLevel;
};

typedef std::unordered_map<uint32 /*spellid*/, TrainerSpell> TrainerSpellMap;

struct TrainerSpellData
{
    TrainerSpellData() : trainerType(0) {}

    TrainerSpellMap spellList;
    uint32 trainerType; // trainer type based at trainer spells, can be
                        // different from creature_template value.
    // req. for correct show non-prof. trainers like weaponmaster, allowed
    // values 0 and 2.
    TrainerSpell const* Find(uint32 spell_id) const;
    void Clear() { spellList.clear(); }
};

typedef std::map<uint32, time_t> CreatureSpellCooldowns;

// max different by z coordinate for creature aggro reaction
#define CREATURE_Z_ATTACK_RANGE 3

#define MAX_VENDOR_ITEMS \
    255 // Limitation in item count field size in SMSG_LIST_INVENTORY

enum VirtualItemSlot
{
    VIRTUAL_ITEM_SLOT_0 = 0,
    VIRTUAL_ITEM_SLOT_1 = 1,
    VIRTUAL_ITEM_SLOT_2 = 2,
};

#define MAX_VIRTUAL_ITEM_SLOT 3

enum VirtualItemInfoByteOffset
{
    VIRTUAL_ITEM_INFO_0_OFFSET_CLASS = 0,
    VIRTUAL_ITEM_INFO_0_OFFSET_SUBCLASS = 1,
    VIRTUAL_ITEM_INFO_0_OFFSET_UNK0 = 2,
    VIRTUAL_ITEM_INFO_0_OFFSET_MATERIAL = 3,

    VIRTUAL_ITEM_INFO_1_OFFSET_INVENTORYTYPE = 0,
    VIRTUAL_ITEM_INFO_1_OFFSET_SHEATH = 1,
};

struct CreatureCreatePos
{
public:
    // exactly coordinates used
    CreatureCreatePos(Map* map, float x, float y, float z, float o)
      : m_map(map), m_closeObject(nullptr), m_angle(0.0f), m_dist(0.0f)
    {
        m_pos.x = x;
        m_pos.y = y;
        m_pos.z = z;
        m_pos.o = o;
    }
    // if dist == 0.0f -> exactly object coordinates used, in other case close
    // point to object (CONTACT_DIST can be used as minimal distances)
    CreatureCreatePos(WorldObject* closeObject, float ori, float dist = 0.0f,
        float angle = 0.0f)
      : m_map(closeObject->GetMap()), m_closeObject(closeObject),
        m_angle(angle), m_dist(dist)
    {
        m_pos.o = ori;
    }

public:
    Map* GetMap() const { return m_map; }
    void SelectFinalPoint(Creature* cr);
    bool Relocate(Creature* cr) const;
    void AddBoundingRadius(float radius)
    {
        m_dist += (m_dist > 0) ? radius : 0;
    }

    // read only after SelectFinalPoint
    Position m_pos;

private:
    Map* m_map;
    WorldObject* m_closeObject;
    float m_angle;
    float m_dist;
};

enum CreatureSubtype
{
    CREATURE_SUBTYPE_GENERIC,          // new Creature
    CREATURE_SUBTYPE_PET,              // new Pet
    CREATURE_SUBTYPE_TOTEM,            // new Totem
    CREATURE_SUBTYPE_TEMPORARY_SUMMON, // new TemporarySummon
    CREATURE_SUBTYPE_SPECIAL_VIS,      // new SpecialVisCreature
};

enum TemporaryFactionFlags // Used at real faction changes
{
    TEMPFACTION_NONE = 0x00, // When no flag is used in temporary faction
                             // change, faction will be persistent. It will then
                             // require manual change back to default/another
                             // faction when changed once
    TEMPFACTION_RESTORE_RESPAWN =
        0x01, // Default faction will be restored at respawn
    TEMPFACTION_RESTORE_COMBAT_STOP = 0x02, // ... at CombatStop() (happens at
                                            // creature death, at evade or
                                            // custom scripte among others)
    TEMPFACTION_RESTORE_REACH_HOME = 0x04,  // ... at reaching home in home
                                            // movement (evade), if not already
                                            // done at CombatStop()
    TEMPFACTION_ALL,
};

class MANGOS_DLL_SPEC Creature : public Unit
{
protected:
    CreatureAI* i_AI;

public:
    explicit Creature(CreatureSubtype subtype = CREATURE_SUBTYPE_GENERIC);
    virtual ~Creature();

    void AddToWorld() override;
    void RemoveFromWorld() override;

    bool Create(uint32 guidlow, CreatureCreatePos& cPos,
        CreatureInfo const* cinfo, Team team = TEAM_NONE,
        const CreatureData* data = nullptr,
        GameEventCreatureData const* eventData = nullptr);
    bool LoadCreatureAddon(bool reload);
    void SelectLevel(const CreatureInfo* cinfo, float percentHealth = 100.0f,
        float percentMana = 100.0f, bool keepLevel = false);
    void LoadEquipment(uint32 equip_entry, bool force = false);

    bool HasStaticDBSpawnData()
        const; // listed in `creature` table and have fixed in DB guid

    char const* GetSubName() const { return GetCreatureInfo()->SubName; }

    void Update(uint32 update_diff, uint32 time);

    virtual void RegenerateAll(uint32 update_diff);
    void GetRespawnCoord(float& x, float& y, float& z, float* ori = nullptr,
        float* dist = nullptr) const;
    uint32 GetEquipmentId() const { return m_equipmentId; }

    CreatureSubtype GetSubtype() const { return m_subtype; }
    bool IsPet() const { return m_subtype == CREATURE_SUBTYPE_PET; }
    bool IsPlayerPet() const;
    bool IsTotem() const { return m_subtype == CREATURE_SUBTYPE_TOTEM; }
    bool IsTemporarySummon() const
    {
        return m_subtype == CREATURE_SUBTYPE_TEMPORARY_SUMMON;
    }
    bool IsSpecialVisCreature() const
    {
        return m_subtype == CREATURE_SUBTYPE_SPECIAL_VIS;
    }

    bool IsCorpse() const { return getDeathState() == CORPSE; }
    bool IsDespawned() const { return getDeathState() == DEAD; }
    void SetCorpseDelay(uint32 delay) { m_corpseDelay = delay; }
    bool IsRacialLeader() const { return GetCreatureInfo()->RacialLeader; }
    bool IsCivilian() const
    {
        return !IsPlayerPet() &&
               (GetCreatureInfo()->flags_extra & CREATURE_FLAG_EXTRA_CIVILIAN);
    }
    bool IsGuard() const
    {
        return GetCreatureInfo()->flags_extra & CREATURE_FLAG_EXTRA_GUARD;
    }

    bool CanWalk() const { return m_inhabitType & INHABIT_GROUND; }
    bool CanSwim() const { return m_inhabitType & INHABIT_WATER; }
    bool CanFly() const { return m_inhabitType & INHABIT_AIR; }
    void AddInhabitType(InhabitTypeValues inhabit);
    void RemoveInhabitType(InhabitTypeValues inhabit);
    void ResetInhabitType(uint32 inhabit = 0);

    bool IsTrainerOf(Player* player, bool msg) const;
    bool CanInteractWithBattleMaster(Player* player, bool msg) const;
    bool CanTrainAndResetTalentsOf(Player* pPlayer) const;

    bool IsOutOfThreatArea(Unit* pVictim) const;
    void FillGuidsListFromThreatList(
        std::vector<ObjectGuid>& guids, uint32 maxamount = 0);

    bool IsImmuneToSpell(SpellEntry const* spellInfo) override;
    // redefine Unit::IsImmuneToSpell
    bool IsImmuneToSpellEffect(
        SpellEntry const* spellInfo, SpellEffectIndex index) const override;
    // redefine Unit::IsImmuneToSpellEffect
    bool IsElite() const
    {
        if (IsPet())
            return false;
        uint32 rank = GetCreatureInfo()->rank;
        return rank != CREATURE_ELITE_NORMAL && rank != CREATURE_ELITE_RARE;
    }

    bool IsRare() const
    {
        if (IsPet())
            return false;
        uint32 rank = GetCreatureInfo()->rank;
        return rank == CREATURE_ELITE_RAREELITE || rank == CREATURE_ELITE_RARE;
    }

    bool IsWorldBoss() const
    {
        if (IsPet())
            return false;
        return GetCreatureInfo()->rank == CREATURE_ELITE_WORLDBOSS;
    }

    bool IsDungeonBoss() const
    {
        if (IsPet())
            return false;
        return GetCreatureInfo()->flags_extra &
               CREATURE_FLAG_EXTRA_DUNGEON_BOSS;
    }

    uint32 GetLevelForTarget(Unit const* target) const
        override; // overwrite Unit::GetLevelForTarget for boss level support

    bool IsInEvadeMode() const;

    virtual bool AIM_Initialize();

    CreatureAI* AI() { return i_AI; }

    void SetWalk(bool enable);
    void SetLevitate(bool enable);
    void SetSwim(bool enable);
    void SetCanRun(bool canRun) { m_canRun = canRun; }
    bool CanRun() const { return m_canRun; }

    uint32 GetShieldBlockValue() const override // dunno mob block value
    {
        return (getLevel() / 2 + uint32(GetStat(STAT_STRENGTH) / 20));
    }

    SpellSchoolMask GetMeleeDamageSchoolMask() const override
    {
        return m_meleeDamageSchoolMask;
    }
    void SetMeleeDamageSchool(SpellSchools school)
    {
        m_meleeDamageSchoolMask = SpellSchoolMask(1 << school);
    }

    void _AddCreatureSpellCooldown(uint32 spell_id, time_t end_time);
    void _AddCreatureCategoryCooldown(uint32 category, time_t apply_time);
    void AddCreatureSpellCooldown(uint32 spellid);
    bool HasSpellCooldown(uint32 spell_id) const;
    bool HasCategoryCooldown(uint32 spell_id) const;
    void ProhibitSpellSchool(
        SpellSchoolMask idSchoolMask, uint32 unTimeMs) override;
    inline bool IsSpellSchoolLocked(SpellSchoolMask idSchoolMask)
    {
        return (m_spellSchoolCooldownMask & idSchoolMask);
    }

    bool HasSpell(uint32 spellID) const override;

    bool UpdateEntry(uint32 entry, Team team = ALLIANCE,
        const CreatureData* data = nullptr,
        GameEventCreatureData const* eventData = nullptr,
        bool preserveHPAndPower = true);

    void ApplyGameEventSpells(
        GameEventCreatureData const* eventData, bool activated);
    bool UpdateStats(Stats stat) override;
    bool UpdateAllStats() override;
    void UpdateResistances(uint32 school) override;
    void UpdateArmor() override;
    void UpdateMaxHealth() override;
    void UpdateMaxPower(Powers power) override;
    void UpdateAttackPowerAndDamage(bool ranged = false) override;
    void UpdateDamagePhysical(WeaponAttackType attType) override;
    uint32 GetCurrentEquipmentId() { return m_equipmentId; }
    void SetCurrentEquipmentId(uint32 entry) { m_equipmentId = entry; }
    float GetSpellDamageMod(int32 Rank);

    VendorItemData const* GetVendorItems() const;
    VendorItemData const* GetVendorTemplateItems() const;
    uint32 GetVendorItemCurrentCount(VendorItem const* vItem);
    uint32 UpdateVendorItemCurrentCount(
        VendorItem const* vItem, uint32 used_count);

    TrainerSpellData const* GetTrainerTemplateSpells() const;
    TrainerSpellData const* GetTrainerSpells() const;

    CreatureInfo const* GetCreatureInfo() const { return m_creatureInfo; }
    CreatureDataAddon const* GetCreatureAddon() const;

    static uint32 ChooseDisplayId(const CreatureInfo* cinfo,
        const CreatureData* data = nullptr,
        GameEventCreatureData const* eventData = nullptr);

    std::string GetAIName() const;
    std::string GetScriptName() const;
    uint32 GetScriptId() const;

    // overwrite WorldObject function for proper name localization
    const char* GetNameForLocaleIdx(int32 locale_idx) const override;

    void SetDeathState(
        DeathState s) override; // overwrite virtual Unit::SetDeathState

    bool LoadFromDB(uint32 guid, Map* map);
    void SaveToDB();
    void SaveToDB(uint32 mapid, uint8 spawnMask);
    void DeleteFromDB();
    static void DeleteFromDB(uint32 lowguid, CreatureData const* data);

    loot_distributor* GetLootDistributor() const override;
    loot_distributor* GetCorpseLootDist() const { return m_lootDistributor; }
    void OnLootOpen(LootType lootType, Player* looter) override;
    void PrepareCorpseLoot();
    void PrepareSkinningLoot();
    void FinishedLooting();
    void RemoveLootFlags();
    void ResetLootRecipients();
    bool IsTappedBy(const Player* player) const;
    // Callback from Threat Manager
    // When player leaves threat list, they might end up losing their tap
    void abandon_taps(ObjectGuid guid) const;

    SpellEntry const* ReachWithSpellAttack(Unit* pVictim);
    SpellEntry const* ReachWithSpellCure(Unit* pVictim);

    uint32 m_spells[CREATURE_MAX_SPELLS];
    CreatureSpellCooldowns m_CreatureSpellCooldowns;
    CreatureSpellCooldowns m_CreatureCategoryCooldowns;

    bool IsWithinAggroDistance(const Unit* target) const
    {
        return IsWithinDistInMap(target, GetAggroDistance(target), true, false);
    }
    float GetAggroDistance(const Unit* target) const;

    void SendAIReaction(AiReaction reactionType);

    void RunAwayInFear(bool do_emote = true);
    void CallForHelp(float fRadius);
    bool CanAssist(const Unit* u, const Unit* enemy, bool sparring) const;

    bool CanInitiateAttack() const;
    bool CanStartAttacking(const Unit* who) const;

    movement::gen get_default_movement_gen() const
    {
        return m_defaultMovementGen;
    }
    void set_default_movement_gen(movement::gen gen)
    {
        m_defaultMovementGen = gen;
    }

    bool IsVisibleInGridForPlayer(Player* pl) const override;

    void RemoveCorpse();
    bool IsDeadByDefault() const { return m_isDeadByDefault; };

    void ForcedDespawn(uint32 timeMSToDespawn = 0);

    time_t const& GetRespawnTime() const { return m_respawnTime; }
    time_t GetRespawnTimeEx() const;
    void SetRespawnTime(uint32 respawn)
    {
        m_respawnTime = respawn ? WorldTimer::time_no_syscall() + respawn : 0;
    }
    void Respawn(bool force = false);
    void SaveRespawnTime() override;

    uint32 GetRespawnDelay() const { return m_respawnDelay; }
    void SetRespawnDelay(uint32 delay) { m_respawnDelay = delay; }

    float GetRespawnRadius() const { return m_respawnradius; }
    void SetRespawnRadius(float dist) { m_respawnradius = dist; }

    // Functions spawn/remove creature with DB guid in all loaded map copies (if
    // point grid loaded in map)
    static void AddToRemoveListInMaps(uint32 db_guid, CreatureData const* data);
    static void SpawnInMaps(uint32 db_guid, CreatureData const* data);

    void SendZoneUnderAttackMessage(Player* attacker);

    void SetInCombatWithZone();
    void SetAggroPulseTimer(uint32 timer) { m_aggroPulseTimer = timer; }
    float GetAggroPulsateRange() const;
    void AggroPulse();
    void AggroPulsateOnCreature(Creature* target);

    float GetLowHealthSpeedRate() const;
    void UpdateLowHealthSpeed();
    void ResetLowHealthSpeed() { m_lastLowHpUpdate = 0; }

    Unit* SelectAttackingTarget(AttackingTarget target, uint32 position,
        uint32 uiSpellEntry, uint32 selectFlags = 0) const;
    Unit* SelectAttackingTarget(AttackingTarget target, uint32 position,
        SpellEntry const* pSpellInfo = nullptr, uint32 selectFlags = 0) const;

    bool HasQuest(uint32 quest_id) const override;
    bool HasInvolvedQuest(uint32 quest_id) const override;

    bool IsRegeneratingHealth() const { return m_regenHealth; }
    bool IsRegeneratingMana() const { return m_regenMana; }
    void SetRegeneratingHealth(bool regenHealth)
    {
        m_regenHealth = regenHealth;
    }
    void SetRegeneratingMana(bool regenMana) { m_regenHealth = regenMana; }
    virtual uint8 GetPetAutoSpellSize() const { return CREATURE_MAX_SPELLS; }
    virtual uint32 GetPetAutoSpellOnPos(uint8 pos) const
    {
        if (pos >= CREATURE_MAX_SPELLS ||
            m_charmInfo->GetCharmSpell(pos)->GetType() != ACT_ENABLED)
            return 0;
        else
            return m_charmInfo->GetCharmSpell(pos)->GetAction();
    }

    void ResetKitingLeashPos()
    {
        GetPosition(m_lastCombatX, m_lastCombatY, m_lastCombatZ);
    }
    void KitingLeashTeleportHome();

    void SetSummonPoint(CreatureCreatePos const& pos)
    {
        m_summonPos = pos.m_pos;
    }
    void GetSummonPoint(float& fX, float& fY, float& fZ, float& fOrient) const
    {
        fX = m_summonPos.x;
        fY = m_summonPos.y;
        fZ = m_summonPos.z;
        fOrient = m_summonPos.o;
    }

    void SetDeadByDefault(bool death_state) { m_isDeadByDefault = death_state; }

    void SetFactionTemporary(
        uint32 factionId, uint32 tempFactionFlags = TEMPFACTION_ALL);
    void ClearTemporaryFaction();
    uint32 GetTemporaryFactionFlags() { return m_temporaryFactionFlags; }

    void SendAreaSpiritHealerQueryOpcode(Player* pl);

    void SetVirtualItem(VirtualItemSlot slot, uint32 item_id);
    void SetVirtualItemRaw(
        VirtualItemSlot slot, uint32 display_id, uint32 info0, uint32 info1);

    void SetKeepTargetEmptyDueToCC(bool apply);

    void SetAggroDistance(float dist);

    void SetGroup(CreatureGroup* group) { creature_group_ = group; }
    CreatureGroup* GetGroup() const { return creature_group_; }

    bool GetSpawnPosition(float& X, float& Y, float& Z, float& O) const;

    void OnMapCreatureRelocation();

    ObjectGuid GetBossLink() const { return m_bossLink; }

    // This target will be returned by the threat manager until focus is
    // cleared.
    // It has highest priority (higher than taunt & root).
    // Pass NULL to clear. Make sure to clear it when the creature Resets; if
    // that's the effect you want.
    void SetFocusTarget(Unit* unit);

    // Cancels auras that need to be removed when NPC evades
    void remove_auras_on_evade();

    // Things that must be done when evading
    void OnEvadeActions(bool by_group);

    void SetFocusSpellTarget(Unit* target, const SpellEntry* spellInfo);
    static void StopFocusSpellCallback(void* spellInfo, Unit* caster);

    bool MeetsQuestVisibility(Player* player) const;

    // Expansion mob was added in. 0 -- Vanilla, 1 -- TBC
    uint8 expansion_level() const;

    bool evading() const { return in_evade_; }
    void start_evade();
    void stop_evade();

    // Team that has tapped this creature
    Team tapping_team() const;

    std::vector<ObjectGuid> combat_summons; // Mobs we summoned while in combat

    uint32 aggro_delay() const { return aggro_delay_; }
    void aggro_delay(uint32 a) { aggro_delay_ = a; }

    bool special_vis_mob() const;
    float special_vis_dist() const;

    // for summoned mobs, static mobs should set it in the creature template
    void set_leash(float x, float y, float z, float radius)
    {
        leash_coords[0] = x;
        leash_coords[1] = y;
        leash_coords[2] = z;
        leash_radius = radius;
    }

    // reacts to a stealthed target getting close to aggroing us, with a
    // turn-around and "grunt"-like soumd
    void stealth_reaction(const Unit* target);

    // Methods used by pet and enslaved creatures
    pet_behavior* behavior() { return pet_behavior_; }
    const pet_behavior* behavior() const { return pet_behavior_; }
    const pet_template* get_template() const { return pet_template_; }
    // Method to install/uninstall pet behavior for a non-pet
    // Use AIM_Initialize() if the creature is an actual pet
    void install_pet_behavior();
    void uninstall_pet_behavior();

    void AddBossLinkedMob(ObjectGuid guid);

    uint32 total_dmg_taken;  // damage taken by all sources combined
    uint32 legit_dmg_taken;  // damage taken by level appropriate dealers
    uint32 player_dmg_taken; // damage taken by players

    ObjectGuid GetCastedAtTarget() const { return m_castedAtTarget; }

protected:
    bool MeetsSelectAttackingRequirement(
        Unit* pTarget, SpellEntry const* pSpellInfo, uint32 selectFlags) const;

    bool CreateFromProto(uint32 guidlow, CreatureInfo const* cinfo, Team team,
        const CreatureData* data = nullptr,
        GameEventCreatureData const* eventData = nullptr);
    bool InitEntry(uint32 entry, const CreatureData* data = nullptr,
        GameEventCreatureData const* eventData = nullptr);

    // vendor items
    VendorItemCounts m_vendorItemCounts;

    void _RealtimeSetCreatureInfo();

    static float _GetHealthMod(int32 Rank);
    static float _GetDamageMod(int32 Rank);

    /// Timers
    uint32 m_corpseDecayTimer; // (msecs)timer for death or corpse disappearance
    time_t m_respawnTime;      // (secs) time of next respawn
    uint32 m_respawnDelay;     // (secs) delay between corpse disappearance and
                               // respawning
    uint32 m_corpseDelay; // (secs) delay between death and corpse disappearance
    float m_respawnradius;

    CreatureSubtype m_subtype; // set in Creatures subclasses for fast it detect
                               // without dynamic_cast use
    void RegenerateMana();
    void RegenerateHealth();
    movement::gen m_defaultMovementGen;
    uint32 m_equipmentId;

    // below fields has potential for optimization
    bool m_regenHealth;
    bool m_regenMana;
    bool m_AI_locked;
    bool m_isDeadByDefault;
    uint32 m_temporaryFactionFlags; // used for real faction changes (not auras
                                    // etc)

    SpellSchoolMask m_meleeDamageSchoolMask;
    uint32 m_originalEntry;

    // X,Y,Z where mob was last damaged
    float m_lastCombatX;
    float m_lastCombatY;
    float m_lastCombatZ;

    Position m_summonPos;

    bool m_canRespawn;
    bool m_forcedRespawn;

    // nullptr if *this does not belong to a creature group
    CreatureGroup* creature_group_;

    bool m_canRun; // If false chase movement will walk, not run

    bool CanRespawn();
    ObjectGuid m_bossLink;
    bool m_checkBossLink;
    std::vector<ObjectGuid> m_linkedMobs; // mobs that have us as m_bossLink

    ObjectGuid m_castedAtTarget;
    uint32 m_focusSpellId;

    uint32 m_aggroPulseTimer;
    void CustomAggroPulse(float range);
    float m_lastLowHpUpdate;

    uint32 m_inhabitType;

    // Data used by pets and enslaved creatures
    const pet_template* pet_template_;
    pet_behavior* pet_behavior_;

private:
    void update_evade(const uint32 diff);

    CreatureInfo const* m_creatureInfo; // in heroic mode can different from
    // sObjectMgr::GetCreatureTemplate(GetEntry())
    std::vector<uint32> m_spellSchoolCooldowns;
    uint32 m_spellSchoolCooldownMask;

    bool lootPickpocket;
    bool lootSkin;
    bool lootBody;
    loot_distributor* m_skinningLootDist;
    loot_distributor* m_pickpocketLootDist;

    bool in_evade_;
    uint32 evade_timer_;
    uint32 evade_tick_timer_;
    uint32 aggro_delay_; // ms timestamp of when we can first aggro, 0 if not in
                         // effect

    float leash_coords[3];
    float leash_radius;
    float aggro_radius;
    float chain_radius;
};

class ForcedDespawnDelayEvent : public BasicEvent
{
public:
    ForcedDespawnDelayEvent(Creature& owner) : BasicEvent(), m_owner(owner) {}
    bool Execute(uint64 e_time, uint32 p_time) override;

private:
    Creature& m_owner;
};

#endif
