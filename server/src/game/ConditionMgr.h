/*
 * Copyright (C) 2008-2012 TrinityCore <http://www.trinitycore.org/>
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

#ifndef TRINITY_CONDITIONMGR_H
#define TRINITY_CONDITIONMGR_H

#include "LootMgr.h"
#include "Policies/Singleton.h"

struct Condition;
class LootTemplate;
class Player;
class Unit;
class WorldObject;

enum ConditionTypes
{                          // value1           value2         value3
    TC_CONDITION_NONE = 0, // 0                0              0 always true
    TC_CONDITION_AURA = 1, // spell_id         0 or count
    TC_CONDITION_ITEM = 2, // item_id          count          bank
                           // true if has #count of item_ids (if 'bank' is set
                           // it searches in bank slots too)
    TC_CONDITION_ITEM_EQUIPPED = 3,   // item_id          0              0
                                      // true if has item_id equipped
    TC_CONDITION_ZONEID = 4,          // zone_id          0              0
                                      // true if in zone_id
    TC_CONDITION_REPUTATION_RANK = 5, // faction_id       rankMask       0
                                      // true if has min_rank for faction_id
    TC_CONDITION_TEAM = 6,            // player_team      0,             0
                                      // 469 - Alliance, 67 - Horde)
    TC_CONDITION_SKILL = 7,           // skill_id         skill_value    0
                                      // true if has skill_value for skill_id
    TC_CONDITION_QUESTREWARDED = 8,   // quest_id         0              0
                                      // true if quest_id was rewarded before
    TC_CONDITION_QUESTTAKEN = 9,      // quest_id         0,             0
                                      // true while quest active
    TC_CONDITION_DRUNKENSTATE = 10,   // DrunkenState     0,             0
                                      // true if player is drunk enough
    TC_CONDITION_WORLD_STATE = 11,    // index            value          0
    // true if world has the value for the index
    TC_CONDITION_ACTIVE_EVENT = 12,  // event_id         0              0
                                     // true if event is active
    TC_CONDITION_INSTANCE_DATA = 13, // entry            data           0
                                     // true if data is set in current instance
    TC_CONDITION_QUEST_NONE = 14,    // quest_id         0              0
                                     // true if doesn't have quest saved
    TC_CONDITION_CLASS = 15,         // class            0              0
                                     // true if player's class is equal to class
    TC_CONDITION_RACE = 16,          // race             0              0
                                     // true if player's race is equal to race
    TC_CONDITION_ACHIEVEMENT = 17,   // achievement_id   0              0
                                     // true if achievement is complete
    TC_CONDITION_TITLE = 18,         // title id         0              0
                                     // true if player has title
    TC_CONDITION_SPAWNMASK = 19,     // spawnMask        0              0
                                     // true if in spawnMask
    TC_CONDITION_POWER = 20,         // enum Powers, value, ComparisonType
    TC_CONDITION_UNIT_GO_FLAG = 21,  // flags
                                     // true if unit flags or go flags match
    TC_CONDITION_MAPID = 22,         // map_id           0              0
                                     // true if in map_id
    TC_CONDITION_AREAID = 23,        // area_id          0              0
                                     // true if in area_id
    TC_CONDITION_UNUSED_24 = 24,     //
    TC_CONDITION_SPELL = 25,         // spell_id         0              0
                                     // true if player has learned spell
    TC_CONDITION_PHASEMASK = 26,     // phasemask        0              0
                                     // true if object is in phasemask
    TC_CONDITION_LEVEL = 27,         // level            ComparisonType 0
    // true if unit's level is equal to param1 (param2
    // can modify the statement)
    TC_CONDITION_QUEST_COMPLETE =
        28, // quest_id         0              0                  true if player
            // has quest_id with all objectives complete, but not yet rewarded
    TC_CONDITION_NEAR_CREATURE = 29, // creature entry   distance       0
                                     // true if there is a creature of entry in
                                     // range
    TC_CONDITION_NEAR_GAMEOBJECT = 30, // gameobject entry distance       0
                                       // true if there is a gameobject of entry
                                       // in range
    TC_CONDITION_OBJECT_ENTRY = 31,    // TypeID           entry          0
                                       // true if object is type TypeID and the
    // entry is 0 or matches entry of the object
    TC_CONDITION_TYPE_MASK = 32,   // TypeMask         0              0
                                   // true if object is type object's TypeMask
                                   // matches provided TypeMask
    TC_CONDITION_RELATION_TO = 33, // ConditionTarget  RelationType   0
                                   // true if object is in given relation with
                                   // object specified by ConditionTarget
    TC_CONDITION_REACTION_TO = 34, // ConditionTarget  rankMask       0
                                   // true if object's reaction matches rankMask
                                   // object specified by ConditionTarget
    TC_CONDITION_DISTANCE_TO =
        35, // ConditionTarget  distance       ComparisonType     true if object
            // and ConditionTarget are within distance given by parameters
    TC_CONDITION_ALIVE = 36,  // guid or 0        entry or 0     0
                              // true if unit is alive (if condition 1 is not
                              // zero we check that creature's alive status
                              // instead of our own) P.S.: Enter entry for hash
                              // based lookup rather than near creature search
    TC_CONDITION_HP_VAL = 37, // hpVal            ComparisonType 0
                              // true if unit's hp matches given value
    TC_CONDITION_HP_PCT = 38, // hpPct            ComparisonType 0
                              // true if unit's hp matches given pct
    TC_CONDITION_COMBAT =
        39, //                                                    true if unit
            //                                                    is in combat
    TC_CONDITION_OBJECTIVE_COMPLETE = 40, // quest_id         objectiveIndex
                                          // true if that objective is complete
                                          // (index 0 to 3)
    TC_CONDITION_MOVEGEN =
        41, // movement::gen type (see movement/generators.h), must be top?
    TC_CONDITION_AURA_TYPE = 42,       // enum AuraType
    TC_CONDITION_MELEE_REACHABLE = 43, // ConditionTarget
    TC_CONDITION_MAX                   // MAX
};

enum ConditionSourceType
{
    // See bottom of file for documentation of various types
    CONDITION_SOURCE_TYPE_NONE = 0,
    CONDITION_SOURCE_TYPE_CREATURE_LOOT_TEMPLATE = 1,
    CONDITION_SOURCE_TYPE_DISENCHANT_LOOT_TEMPLATE = 2,
    CONDITION_SOURCE_TYPE_FISHING_LOOT_TEMPLATE = 3,
    CONDITION_SOURCE_TYPE_GAMEOBJECT_LOOT_TEMPLATE = 4,
    CONDITION_SOURCE_TYPE_ITEM_LOOT_TEMPLATE = 5,
    CONDITION_SOURCE_TYPE_MAIL_LOOT_TEMPLATE = 6,
    // CONDITION_SOURCE_TYPE_MILLING_LOOT_TEMPLATE          = 7,
    CONDITION_SOURCE_TYPE_PICKPOCKETING_LOOT_TEMPLATE = 8,
    CONDITION_SOURCE_TYPE_PROSPECTING_LOOT_TEMPLATE = 9,
    CONDITION_SOURCE_TYPE_REFERENCE_LOOT_TEMPLATE = 10,
    CONDITION_SOURCE_TYPE_SKINNING_LOOT_TEMPLATE = 11,

    CONDITION_SOURCE_TYPE_SPELL_CAST = 12,
    CONDITION_SOURCE_TYPE_SPELL_TARGET_SELECTION = 13,

    CONDITION_SOURCE_TYPE_GOSSIP_MENU = 14,
    CONDITION_SOURCE_TYPE_GOSSIP_MENU_OPTION = 15,

    CONDITION_SOURCE_TYPE_QUEST_AVAILABLE = 19,

    CONDITION_SOURCE_TYPE_SMART_EVENT = 22,
    CONDITION_SOURCE_TYPE_NPC_VENDOR = 23,
    CONDITION_SOURCE_TYPE_SMART_TARGET = 24,
    CONDITION_SOURCE_TYPE_MAX
};

enum ComparisionType
{
    COMP_TYPE_EQ = 0,
    COMP_TYPE_HIGH,
    COMP_TYPE_LOW,
    COMP_TYPE_HIGH_EQ,
    COMP_TYPE_LOW_EQ,
    COMP_TYPE_MAX
};

enum RelationType
{
    RELATION_SELF = 0,
    RELATION_IN_PARTY,
    RELATION_IN_RAID_OR_PARTY,
    RELATION_OWNED_BY,
    RELATION_PASSENGER_OF,
    RELATION_CREATED_BY,
    RELATION_MAX
};

enum
{
    MAX_CONDITION_TARGETS = 3
};

struct ConditionSourceInfo
{
    WorldObject* mConditionTargets[MAX_CONDITION_TARGETS]; // an array of
                                                           // targets available
                                                           // for conditions
    const Condition* mLastFailedCondition;
    ConditionSourceInfo(WorldObject* target0, WorldObject* target1 = nullptr,
        WorldObject* target2 = nullptr)
    {
        mConditionTargets[0] = target0;
        mConditionTargets[1] = target1;
        mConditionTargets[2] = target2;
        mLastFailedCondition = nullptr;
    }
};

struct Condition
{
    ConditionSourceType SourceType; // SourceTypeOrReferenceId
    uint32 SourceGroup;
    int32 SourceEntry;
    uint32 SourceId; // So far, only used in CONDITION_SOURCE_TYPE_SMART_EVENT
    uint32 ElseGroup;
    ConditionTypes ConditionType; // ConditionTypeOrReference
    uint32 ConditionValue1;
    uint32 ConditionValue2;
    uint32 ConditionValue3;
    uint32 ErrorTextId;
    uint32 ReferenceId;
    uint32 ScriptId;
    uint8 ConditionTarget;
    bool NegativeCondition;

    Condition()
    {
        SourceType = CONDITION_SOURCE_TYPE_NONE;
        SourceGroup = 0;
        SourceEntry = 0;
        ElseGroup = 0;
        ConditionType = TC_CONDITION_NONE;
        ConditionTarget = 0;
        ConditionValue1 = 0;
        ConditionValue2 = 0;
        ConditionValue3 = 0;
        ReferenceId = 0;
        ErrorTextId = 0;
        ScriptId = 0;
        NegativeCondition = false;
    }

    bool Meets(ConditionSourceInfo& sourceInfo) const;
    bool isLoaded() const
    {
        return static_cast<uint32>(ConditionType) > TC_CONDITION_NONE ||
               ReferenceId;
    } // NOTE: Not the nicest solution, but we cast to uint32 to suppress
      // warning
    uint32 GetMaxAvailableConditionTargets() const;
};

typedef std::vector<Condition> ConditionList;
typedef std::map<uint32, ConditionList> ConditionTypeContainer;
typedef std::map<ConditionSourceType, ConditionTypeContainer>
    ConditionContainer;

typedef std::map<ConditionSourceType,
    std::map<uint32 /*entry*/, std::map<uint32 /*itemid*/, ConditionList>>>
    LootConditionContainer;
typedef std::map<uint32, ConditionTypeContainer> GossipConditionContainer;
typedef std::map<std::pair<int32, uint32 /*SAI source_type*/>,
    ConditionTypeContainer> SmartConditionContainer;
typedef std::map<uint32, ConditionTypeContainer> NpcVendorConditionContainer;

typedef std::map<uint32, ConditionList>
    ConditionReferenceContainer; // only used for references

class ConditionMgr
{
    friend class MaNGOS::UnlockedSingleton<ConditionMgr>; // Private constructor

    ConditionMgr();

public:
    ~ConditionMgr();

    void LoadConditions(bool isReload = false);
    bool isConditionTypeValid(Condition* cond);
    const ConditionList* GetConditionReferences(uint32 refId);

    bool IsObjectMeetToConditions(
        WorldObject* object, const ConditionList* conditions);
    bool IsObjectMeetToConditions(WorldObject* object1, WorldObject* object2,
        const ConditionList* conditions);
    bool IsObjectMeetToConditions(
        ConditionSourceInfo& sourceInfo, const ConditionList* conditions);
    bool CanHaveSourceGroupSet(ConditionSourceType sourceType) const;
    bool CanHaveSourceIdSet(ConditionSourceType sourceType) const;

    const ConditionList* GetLootConditions(
        ConditionSourceType type, uint32 entry, uint32 itemid);
    const ConditionList* GetSpellCastConditions(uint32 spell_id);
    const ConditionList* GetSpellTargetSelectionConditions(uint32 spell_id);
    const ConditionList* GetGossipMenuConditions(
        uint32 menu_id, uint32 text_id);
    const ConditionList* GetGossipOptionConditions(
        uint32 menu_id, uint32 option_id);
    const ConditionList* GetQuestAvailableConditions(uint32 quest_id);
    const ConditionList* GetConditionsForSmartEvent(
        int32 entryOrGuid, uint32 eventId, uint32 sourceType);
    const ConditionList* GetConditionsForSmartTarget(
        int32 entryOrGuid, uint32 eventId, uint32 sourceType);
    const ConditionList* GetVendorItemConditions(
        uint32 creatureId, uint32 itemId);

private:
    bool isSourceTypeValid(Condition* cond);
    bool IsObjectMeetToConditionList(
        ConditionSourceInfo& sourceInfo, const ConditionList& conditions);

    void Clean(); // free up resources

    ConditionContainer ConditionStore;
    ConditionReferenceContainer ConditionReferenceStore;
    LootConditionContainer LootConditions;
    GossipConditionContainer GossipMenuConditionStore;
    GossipConditionContainer GossipOptionConditionStore;
    SmartConditionContainer SmartEventConditionStore;
    SmartConditionContainer SmartTargetConditionStore;
    NpcVendorConditionContainer NpcVendorConditions;
};

template <class T>
bool CompareValues(ComparisionType type, T val1, T val2)
{
    switch (type)
    {
    case COMP_TYPE_EQ:
        return val1 == val2;
    case COMP_TYPE_HIGH:
        return val1 > val2;
    case COMP_TYPE_LOW:
        return val1 < val2;
    case COMP_TYPE_HIGH_EQ:
        return val1 >= val2;
    case COMP_TYPE_LOW_EQ:
        return val1 <= val2;
    default:
        // incorrect parameter
        assert(false);
        return false;
    }
}

#define sConditionMgr MaNGOS::UnlockedSingleton<ConditionMgr>

#endif

/* SourceType documentation
 *
 * X_LOOT_TEMPLATE
 *   Condition to determine if a specific item of a loot table can drop
 * SourceType:
 *   Id of the source type matching the intended loot template table
 * SourceGroup:
 *   Id of the loot template table entry
 * SourceEntry:
 *   Item Id the condition applies for
 * Target:
 *   0 - The Player
 *
 * CONDITION_SOURCE_TYPE_GOSSIP_MENU
 *   Condition to determine if we can read gossip menu
 * SourceGroup:
 *   gossip_menu.entry
 * SourceEntry:
 *   gossip_menu.text_id
 * Target:
 *   0 - The Player
 *
 * CONDITION_SOURCE_TYPE_SPELL_CAST
 *   Condition to determine if we can cast spell
 * SourceEntry:
 *   Spell Id
 * Target:
 *   0 - Caster
 *   1 - Explicitly Selected Target
 * ErrorTextId:
 *   0 or SpellCastResult + 1
 *
 * CONDITION_SOURCE_TYPE_SPELL_TARGET_SELECTION
 *   Condition to determine if unit can be chosen for spell target selection
 * SourceEntry:
 *   Spell Id
 * Target:
 *   0 - Caster
 *   1 - Tested Target
 *
 * CONDITION_SOURCE_TYPE_GOSSIP_MENU_OPTION
 *   Condition to determine if we can see and click gossip menu option
 * SourceGroup:
 *   gossip_menu_option.menu_id
 * SourceEntry:
 *   gossip_menu_option.id
 * Target:
 *   0 - The Player
 *
 * CONDITION_SOURCE_TYPE_QUEST_AVAILABLE
 *   Condition to determine if we can pick up a quest
 * SourceEntry:
 *   Quest Id
 * Target:
 *   0 - The Player
 *   1 - The Quest Giver
 *
 * CONDITION_SOURCE_TYPE_SMART_EVENT
 * SourceGroup:
 *   smart_scripts.id + 1
 * SourceEntry:
 *   smart_scripts.entryorguid
 * SourceId:
 *   smart_scripts.source_type
 * Target:
 *   0 - Invoker of Event
 *   1 - The owner of the script
 *
 * CONDITION_SOURCE_TYPE_NPC_VENDOR
 *   Condition to determine if we can buy an item from vendor
 * SourceGroup:
 *   Npc Id
 * SourceEntry:
 *   Item Id
 * Target:
 *   0 - The Player
 *   1 - The NPC
 *
 * CONDITION_SOURCE_TYPE_SMART_TARGET
 * SourceGroup:
 *   smart_scripts.id + 1
 * SourceEntry:
 *   smart_scripts.entryorguid
 * SourceId:
 *   smart_scripts.source_type
 * Target:
 *   0 - The Target
 *   1 - The NPC
 */
