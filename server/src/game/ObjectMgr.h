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

#ifndef _OBJECTMGR_H
#define _OBJECTMGR_H

#include "Common.h"
#include "Corpse.h"
#include "Creature.h"
#include "GameObject.h"
#include "ItemPrototype.h"
#include "logging.h"
#include "Map.h"
#include "MapPersistentStateMgr.h"
#include "NPCHandler.h"
#include "Object.h"
#include "ObjectAccessor.h"
#include "ObjectGuid.h"
#include "Player.h"
#include "QuestDef.h"
#include "SQLStorages.h"
#include "Policies/Singleton.h"
#include <sparsehash/dense_hash_map>
#include <sparsehash/dense_hash_set>
#include <limits>
#include <map>
#include <string>
#include <unordered_map>

class ArenaTeam;
class Group;
class Item;

struct GameTele
{
    float position_x;
    float position_y;
    float position_z;
    float orientation;
    uint32 mapId;
    std::string name;
    std::wstring wnameLow;
};

typedef std::unordered_map<uint32, GameTele> GameTeleMap;

struct AreaTrigger
{
    uint8 requiredLevel;
    uint32 requiredQuest;
    uint32 target_mapId;
    float target_X;
    float target_Y;
    float target_Z;
    float target_Orientation;
};

// mangos string ranges
#define MIN_MANGOS_STRING_ID 1 // 'mangos_string'
#define MAX_MANGOS_STRING_ID 2000000000
#define MIN_DB_SCRIPT_STRING_ID MAX_MANGOS_STRING_ID // 'db_script_string'
#define MAX_DB_SCRIPT_STRING_ID 2000010000
#define MIN_CREATURE_AI_TEXT_STRING_ID (-1) // 'creature_ai_texts'
#define MAX_CREATURE_AI_TEXT_STRING_ID (-1000000)

struct MangosStringLocale
{
    std::vector<std::string> Content; // 0 -> default, i -> i-1 locale index
};

typedef std::unordered_map<uint32, CreatureData> CreatureDataMap;
typedef CreatureDataMap::value_type CreatureDataPair;

class FindCreatureData
{
public:
    FindCreatureData(uint32 id, Player* player)
      : i_id(id), i_player(player), i_anyData(nullptr), i_mapData(nullptr),
        i_mapDist(0.0f), i_spawnedData(nullptr), i_spawnedDist(0.0f)
    {
    }

    bool operator()(CreatureDataPair const& dataPair);
    CreatureDataPair const* GetResult() const;

private:
    uint32 i_id;
    Player* i_player;

    CreatureDataPair const* i_anyData;
    CreatureDataPair const* i_mapData;
    float i_mapDist;
    CreatureDataPair const* i_spawnedData;
    float i_spawnedDist;
};

typedef std::unordered_map<uint32, GameObjectData> GameObjectDataMap;
typedef GameObjectDataMap::value_type GameObjectDataPair;

class FindGOData
{
public:
    FindGOData(uint32 id, Player* player)
      : i_id(id), i_player(player), i_anyData(nullptr), i_mapData(nullptr),
        i_mapDist(0.0f), i_spawnedData(nullptr), i_spawnedDist(0.0f)
    {
    }

    bool operator()(GameObjectDataPair const& dataPair);
    GameObjectDataPair const* GetResult() const;

private:
    uint32 i_id;
    Player* i_player;

    GameObjectDataPair const* i_anyData;
    GameObjectDataPair const* i_mapData;
    float i_mapDist;
    GameObjectDataPair const* i_spawnedData;
    float i_spawnedDist;
};

typedef std::unordered_map<uint32, CreatureLocale> CreatureLocaleMap;
typedef std::unordered_map<uint32, GameObjectLocale> GameObjectLocaleMap;
typedef std::unordered_map<uint32, ItemLocale> ItemLocaleMap;
typedef std::unordered_map<uint32, QuestLocale> QuestLocaleMap;
typedef std::unordered_map<uint32, NpcTextLocale> NpcTextLocaleMap;
typedef std::unordered_map<uint32, PageTextLocale> PageTextLocaleMap;
typedef std::unordered_map<int32, MangosStringLocale> MangosStringLocaleMap;
typedef std::unordered_map<uint32, GossipMenuItemsLocale>
    GossipMenuItemsLocaleMap;
typedef std::unordered_map<uint32, PointOfInterestLocale>
    PointOfInterestLocaleMap;

typedef std::multimap<int32, uint32> ExclusiveQuestGroupsMap;
typedef std::multimap<uint32, ItemRequiredTarget> ItemRequiredTargetMap;
typedef std::multimap<uint32, uint32> QuestRelationsMap;
typedef std::pair<ExclusiveQuestGroupsMap::const_iterator,
    ExclusiveQuestGroupsMap::const_iterator> ExclusiveQuestGroupsMapBounds;
typedef std::pair<ItemRequiredTargetMap::const_iterator,
    ItemRequiredTargetMap::const_iterator> ItemRequiredTargetMapBounds;
typedef std::pair<QuestRelationsMap::const_iterator,
    QuestRelationsMap::const_iterator> QuestRelationsMapBounds;

struct PetLevelInfo
{
    PetLevelInfo() : health(0), mana(0)
    {
        for (auto& elem : stats)
            elem = 0;
    }

    uint16 stats[MAX_STATS];
    uint16 health;
    uint16 mana;
    uint16 armor;
};

struct MailLevelReward
{
    MailLevelReward() : raceMask(0), mailTemplateId(0), senderEntry(0) {}
    MailLevelReward(
        uint32 _raceMask, uint32 _mailTemplateId, uint32 _senderEntry)
      : raceMask(_raceMask), mailTemplateId(_mailTemplateId),
        senderEntry(_senderEntry)
    {
    }

    uint32 raceMask;
    uint32 mailTemplateId;
    uint32 senderEntry;
};

typedef std::list<MailLevelReward> MailLevelRewardList;
typedef std::unordered_map<uint8, MailLevelRewardList> MailLevelRewardMap;

// We assume the rate is in general the same for all three types below, but
// chose to keep three for scalability and customization
struct RepRewardRate
{
    float quest_rate;    // We allow rate = 0.0 in database. For this case,
    float creature_rate; // it means that no reputation are given at all
    float spell_rate;    // for this faction/rate type.
};

struct ReputationOnKillEntry
{
    uint32 repfaction1;
    uint32 repfaction2;
    bool is_teamaward1;
    uint32 reputation_max_cap1;
    int32 repvalue1;
    bool is_teamaward2;
    uint32 reputation_max_cap2;
    int32 repvalue2;
    bool team_dependent;
};

struct RepSpilloverTemplate
{
    uint32 faction[MAX_SPILLOVER_FACTIONS];
    float faction_rate[MAX_SPILLOVER_FACTIONS];
    uint32 faction_rank[MAX_SPILLOVER_FACTIONS];
};

struct PointOfInterest
{
    uint32 entry;
    float x;
    float y;
    uint32 icon;
    uint32 flags;
    uint32 data;
    std::string icon_name;
};

struct GossipMenuItems
{
    uint32 menu_id;
    uint32 id;
    uint8 option_icon;
    std::string option_text;
    uint32 option_id;
    uint32 npc_option_npcflag;
    int32 action_menu_id;
    uint32 action_poi_id;
    uint32 action_script_id;
    bool box_coded;
    uint32 box_money;
    std::string box_text;
    uint16 cond_1;
    uint16 cond_2;
    uint16 cond_3;
    uint16 conditionId;
};

struct GossipMenus
{
    uint32 entry;
    uint32 text_id;
    uint32 script_id;
    uint16 cond_1;
    uint16 cond_2;
    uint16 conditionId;
    // An entry with ordering bigger than others can only be chosen if their
    // conditions fail,
    // entries with ordering == 0 are not relevant to the selection and can
    // always be chosen
    uint32 ordering;
};

typedef std::multimap<uint32, GossipMenus> GossipMenusMap;
typedef std::pair<GossipMenusMap::const_iterator,
    GossipMenusMap::const_iterator> GossipMenusMapBounds;
typedef std::multimap<uint32, GossipMenuItems> GossipMenuItemsMap;
typedef std::pair<GossipMenuItemsMap::const_iterator,
    GossipMenuItemsMap::const_iterator> GossipMenuItemsMapBounds;

struct PetCreateSpellEntry
{
    uint32 spellid[4];
    bool auto_cast[4];
};

#define WEATHER_SEASONS 4
struct WeatherSeasonChances
{
    uint32 rainChance;
    uint32 snowChance;
    uint32 stormChance;
};

struct WeatherZoneChances
{
    WeatherSeasonChances data[WEATHER_SEASONS];
};

struct PetDbData
{
    // character_pet
    uint32 guid;
    uint32 id;
    uint32 owner_guid;
    uint32 model_id;
    uint32 create_spell;
    uint8 pet_type;
    uint32 level;
    uint32 exp;
    uint32 react_state;
    int32 loyalty_points;
    uint32 loyalty;
    int32 training_points;
    std::string name;
    bool renamed;
    uint32 slot;
    uint32 health;
    uint32 mana;
    uint32 happiness;
    uint64 save_time;
    uint32 reset_talents_cost;
    uint32 reset_talents_time;
    std::string action_bar_raw;
    std::string teach_spells_raw;
    bool dead;
    DeclinedName declined_name;

    // pet_spell
    struct Spell
    {
        uint32 id;
        ActiveStates active;
    };
    std::vector<Spell> spells;

    // pet_spell_cooldown
    struct SpellCooldown
    {
        uint32 id;
        uint64 time;
    };
    std::vector<SpellCooldown> spell_cooldowns;

    struct Aura
    {
        uint64 caster_guid;
        uint32 item_guid;
        uint32 spell_id;
        uint32 stacks;
        uint32 charges;
        int32 bp[3];
        uint32 periodic_time[3];
        int32 max_duration;
        int32 duration;
        uint32 eff_mask;
    };
    std::vector<Aura> auras;

    bool needs_save = false;
    bool deleted = false;
};

struct GraveYardData
{
    uint32 safeLocId;
    Team team;
    bool corpse_safeloc;
};
typedef std::multimap<uint32, GraveYardData> GraveYardMap;
typedef std::pair<GraveYardMap::const_iterator, GraveYardMap::const_iterator>
    GraveYardMapBounds;

enum ConditionType
{                      // value1       value2  for the Condition enumed
    CONDITION_OR = -2, // cond-id-1    cond-id-2  returns cond-id-1 OR cond-id-2
    CONDITION_AND =
        -1, // cond-id-1    cond-id-2  returns cond-id-1 AND cond-id-2
    CONDITION_NONE = 0, // 0            0
    CONDITION_AURA = 1, // spell_id     effindex
    CONDITION_ITEM =
        2, // item_id      count   check present req. amount items in inventory
    CONDITION_ITEM_EQUIPPED = 3, // item_id      0
    CONDITION_AREAID =
        4, // area_id      0, 1 (0: in (sub)area, 1: not in (sub)area)
    CONDITION_REPUTATION_RANK = 5, // faction_id   min_rank
    CONDITION_TEAM = 6,  // player_team  0,      (469 - Alliance 67 - Horde)
    CONDITION_SKILL = 7, // skill_id     skill_value
    CONDITION_QUESTREWARDED = 8, // quest_id     0
    CONDITION_QUESTTAKEN = 9, // quest_id     0,1,2   for condition true while
                              // quest active (0 any state, 1 if quest
                              // incomplete, 2 if quest completed).
    CONDITION_AD_COMMISSION_AURA = 10, // 0            0,      for condition
                                       // true while one from AD commission aura
                                       // active
    CONDITION_NO_AURA = 11,            // spell_id     effindex
    CONDITION_ACTIVE_GAME_EVENT = 12,  // event_id     0
    CONDITION_AREA_FLAG = 13,          // area_flag    area_flag_not
    CONDITION_RACE_CLASS = 14,         // race_mask    class_mask
    CONDITION_LEVEL = 15,  // player_level 0, 1 or 2 (0: equal to, 1: equal or
                           // higher than, 2: equal or less than)
    CONDITION_NOITEM = 16, // item_id      count   check not present req. amount
                           // items in inventory
    CONDITION_SPELL = 17,  // spell_id     0, 1 (0: has spell, 1: hasn't spell)
    CONDITION_INSTANCE_SCRIPT = 18, // map_id       instance_condition_id
                                    // (instance script specific enum)
    CONDITION_QUESTAVAILABLE = 19,  // quest_id     0       for case when
                                    // loot/gossip possible only if player can
                                    // start quest
    CONDITION_RESERVED_1 = 20,      // reserved for 3.x and later
    CONDITION_RESERVED_2 = 21,      // reserved for 3.x and later
    CONDITION_QUEST_NONE =
        22, // quest_id     0 (quest did not take and not rewarded)
    CONDITION_ITEM_WITH_BANK = 23,   // item_id      count   check present req.
                                     // amount items in inventory or bank
    CONDITION_NOITEM_WITH_BANK = 24, // item_id      count   check not present
                                     // req. amount items in inventory or bank
    CONDITION_NOT_ACTIVE_GAME_EVENT = 25, // event_id     0
    CONDITION_ACTIVE_HOLIDAY = 26, // holiday_id   0       preferred use instead
                                   // CONDITION_ACTIVE_GAME_EVENT when possible
    CONDITION_NOT_ACTIVE_HOLIDAY =
        27, // holiday_id   0       preferred use instead
            // CONDITION_NOT_ACTIVE_GAME_EVENT when possible
    CONDITION_LEARNABLE_ABILITY = 28, // spell_id     0 or item_id
    // True when player can learn ability (using min skill value from
    // SkillLineAbility).
    // Item_id can be defined in addition, to check if player has one (1) item
    // in inventory or bank.
    // When player has spell or has item (when defined), condition return false.
    CONDITION_SKILL_BELOW = 29, // skill_id     skill_value
    // True if player has skill skill_id and skill less than (and not equal)
    // skill_value (for skill_value > 1)
    // If skill_value == 1, then true if player has not skill skill_id
    CONDITION_OBJECTIVE_NOT_COMPLETE = 30, // quest_id      objective_index
    CONDITION_OBJECTIVE_COMPLETE = 31,     // quest_id      objective_index
};

class PlayerCondition
{
public:
    // Default constructor, required for SQL Storage (Will give errors if used
    // elsewise)
    PlayerCondition()
      : m_entry(0), m_condition(CONDITION_AND), m_value1(0), m_value2(0)
    {
    }

    PlayerCondition(
        uint16 _entry, int16 _condition, uint32 _value1, uint32 _value2)
      : m_entry(_entry), m_condition(ConditionType(_condition)),
        m_value1(_value1), m_value2(_value2)
    {
    }

    // Checks correctness of values
    bool IsValid() const
    {
        return IsValid(m_entry, m_condition, m_value1, m_value2);
    }
    static bool IsValid(
        uint16 entry, ConditionType condition, uint32 value1, uint32 value2);

    bool Meets(Player const* pPlayer)
        const; // Checks if the player meets the condition

    // TODO: old system, remove soon!
    bool operator==(PlayerCondition const& lc) const
    {
        return (lc.m_condition == m_condition && lc.m_value1 == m_value1 &&
                lc.m_value2 == m_value2);
    }

private:
    uint16 m_entry;            // entry of the condition
    ConditionType m_condition; // additional condition type
    uint32 m_value1; // data for the condition - see ConditionType definition
    uint32 m_value2;
};

// NPC gossip text id
typedef std::unordered_map<uint32, uint32> CacheNpcTextIdMap;

typedef std::unordered_map<uint32, VendorItemData> CacheVendorItemMap;
typedef std::unordered_map<uint32, TrainerSpellData> CacheTrainerSpellMap;

enum SkillRangeType
{
    SKILL_RANGE_LANGUAGE, // 300..300
    SKILL_RANGE_LEVEL,    // 1..max skill for level
    SKILL_RANGE_MONO,     // 1..1, grey monolite bar
    SKILL_RANGE_RANK,     // 1..skill for known rank
    SKILL_RANGE_NONE,     // 0..0 always
};

SkillRangeType GetSkillRangeType(SkillLineEntry const* pSkill, bool racial);

#define MAX_PLAYER_NAME 12 // max allowed by client name length
#define MAX_INTERNAL_PLAYER_NAME \
    15 // max server internal player name length ( > MAX_PLAYER_NAME for support
       // declined names )
#define MAX_PET_NAME 12     // max allowed by client name length
#define MAX_CHARTER_NAME 24 // max allowed by client name length

bool normalizePlayerName(std::string& name);

struct MANGOS_DLL_SPEC LanguageDesc
{
    Language lang_id;
    uint32 spell_id;
    uint32 skill_id;
};

extern LanguageDesc lang_description[LANGUAGES_COUNT];
MANGOS_DLL_SPEC LanguageDesc const* GetLanguageDescByID(uint32 lang);

class PlayerDumpReader;

template <typename T>
class IdGenerator
{
public: // constructors
    explicit IdGenerator(char const* _name) : m_name(_name), m_nextGuid(1) {}

public: // modifiers
    void Set(T val) { m_nextGuid = val; }
    T Generate();

public: // accessors
    T GetNextAfterMaxUsed() const { return m_nextGuid; }

private: // fields
    char const* m_name;
    T m_nextGuid;
};

struct MapEntryRequirements
{
    uint32 level;
    struct req_entry
    {
        // in db : 'a|h|b n|h itemid[ "failed text"]'
        Team team;
        Difficulty difficulty;
        uint32 id; // item or quest id
        std::string failed_text;
    };
    std::vector<req_entry> items;
    std::vector<req_entry> quests;
};

class ObjectMgr
{
    friend class PlayerDumpReader;

public:
    ObjectMgr();
    ~ObjectMgr();

    typedef std::unordered_map<uint32, Item*> ItemMap;

    typedef std::unordered_map<uint32, Group*> GroupMap;

    typedef std::unordered_map<uint32, ArenaTeam*> ArenaTeamMap;

    typedef std::unordered_map<uint32, Quest*> QuestMap;

    typedef std::unordered_map<uint32, AreaTrigger> AreaTriggerMap;

    typedef std::unordered_map<uint32, RepRewardRate> RepRewardRateMap;
    typedef std::unordered_map<uint32, ReputationOnKillEntry> RepOnKillMap;
    typedef std::unordered_map<uint32, RepSpilloverTemplate>
        RepSpilloverTemplateMap;

    typedef std::unordered_map<uint32, PointOfInterest> PointOfInterestMap;

    typedef std::unordered_map<uint32, WeatherZoneChances> WeatherZoneMap;

    typedef std::unordered_map<uint32, PetCreateSpellEntry> PetCreateSpellMap;

    typedef std::vector<ObjectGuid> linked_mobs_vector;
    typedef std::unordered_map<ObjectGuid, linked_mobs_vector> aggro_link_map;

    static Player* GetPlayer(const char* name, bool inWorld = true)
    {
        return ObjectAccessor::FindPlayerByName(name, inWorld);
    }
    static Player* GetPlayer(ObjectGuid guid, bool inWorld = true)
    {
        return ObjectAccessor::FindPlayer(guid, inWorld);
    }

    static GameObjectInfo const* GetGameObjectInfo(uint32 id)
    {
        return sGOStorage.LookupEntry<GameObjectInfo>(id);
    }

    void LoadGameobjectInfo();
    void AddGameobjectInfo(GameObjectInfo* goinfo);

    // All group methods are thread UNSAFE
    void PackGroupIds();
    Group* GetGroupById(uint32 id) const;
    void AddGroup(Group* group);
    void RemoveGroup(Group* group);
    void AddOfflineLeaderGroup(Group* group);
    bool IsOfflineLeaderGroup(Group* group) const;
    void UpdateGroupsWithOfflineLeader();

    ArenaTeam* GetArenaTeamById(uint32 arenateamid) const;
    ArenaTeam* GetArenaTeamByName(const std::string& arenateamname) const;
    ArenaTeam* GetArenaTeamByCaptain(ObjectGuid guid) const;
    void AddArenaTeam(ArenaTeam* team);
    void RemoveArenaTeam(ArenaTeam* team);
    ArenaTeamMap::iterator GetArenaTeamMapBegin()
    {
        return mArenaTeamMap.begin();
    }
    ArenaTeamMap::iterator GetArenaTeamMapEnd() { return mArenaTeamMap.end(); }
    ArenaTeamMap::const_iterator GetArenaTeamMapBegin() const
    {
        return mArenaTeamMap.begin();
    }
    ArenaTeamMap::const_iterator GetArenaTeamMapEnd() const
    {
        return mArenaTeamMap.end();
    }
    uint32 get_arena_team_rank(ArenaTeam* team) const;
    // Must call this whenever rating of any ArenaTeam changes
    void update_arena_rankings();

    static CreatureInfo const* GetCreatureTemplate(uint32 id);
    CreatureModelInfo const* GetCreatureModelInfo(uint32 modelid) const;
    CreatureModelInfo const* GetCreatureModelRandomGender(
        uint32 display_id) const;
    uint32 GetCreatureModelAlternativeModel(uint32 modelId) const;

    EquipmentInfo const* GetEquipmentInfo(uint32 entry) const;
    EquipmentInfoRaw const* GetEquipmentInfoRaw(uint32 entry) const;
    static CreatureDataAddon const* GetCreatureAddon(uint32 lowguid)
    {
        return sCreatureDataAddonStorage.LookupEntry<CreatureDataAddon>(
            lowguid);
    }

    static CreatureDataAddon const* GetCreatureTemplateAddon(uint32 entry)
    {
        return sCreatureInfoAddonStorage.LookupEntry<CreatureDataAddon>(entry);
    }

    static ItemPrototype const* GetItemPrototype(uint32 id)
    {
        return sItemStorage.LookupEntry<ItemPrototype>(id);
    }

    static InstanceTemplate const* GetInstanceTemplate(uint32 map)
    {
        return sInstanceTemplate.LookupEntry<InstanceTemplate>(map);
    }

    static WorldTemplate const* GetWorldTemplate(uint32 map)
    {
        return sWorldTemplate.LookupEntry<WorldTemplate>(map);
    }

    PetLevelInfo const* GetPetLevelInfo(uint32 creature_id, uint32 level) const;

    PlayerClassInfo const* GetPlayerClassInfo(uint32 class_) const
    {
        if (class_ >= MAX_CLASSES)
            return nullptr;
        return &playerClassInfo[class_];
    }
    void GetPlayerClassLevelInfo(
        uint32 class_, uint32 level, PlayerClassLevelInfo* info) const;

    PlayerInfo const* GetPlayerInfo(uint32 race, uint32 class_) const
    {
        if (race >= MAX_RACES)
            return nullptr;
        if (class_ >= MAX_CLASSES)
            return nullptr;
        PlayerInfo const* info = &playerInfo[race][class_];
        if (info->displayId_m == 0 || info->displayId_f == 0)
            return nullptr;
        return info;
    }
    void GetPlayerLevelInfo(
        uint32 race, uint32 class_, uint32 level, PlayerLevelInfo* info) const;

    ObjectGuid GetPlayerGuidByName(std::string name) const;
    bool GetPlayerNameByGUID(ObjectGuid guid, std::string& name) const;
    Team GetPlayerTeamByGUID(ObjectGuid guid) const;
    uint32 GetPlayerAccountIdByGUID(ObjectGuid guid) const;
    uint32 GetPlayerAccountIdByPlayerName(const std::string& name) const;

    uint32 GetNearestTaxiNode(
        float x, float y, float z, uint32 mapid, Team team) const;
    void GetTaxiPath(
        uint32 source, uint32 destination, uint32& path, uint32& cost) const;
    uint32 GetTaxiMountDisplayId(
        uint32 id, Team team, bool allowed_alt_team = false) const;

    Quest const* GetQuestTemplate(uint32 quest_id) const
    {
        auto itr = mQuestTemplates.find(quest_id);
        return itr != mQuestTemplates.end() ? itr->second : nullptr;
    }
    QuestMap const& GetQuestTemplates() const { return mQuestTemplates; }

    uint32 GetQuestForAreaTrigger(uint32 Trigger_ID) const
    {
        auto itr = mQuestAreaTriggerMap.find(Trigger_ID);
        if (itr != mQuestAreaTriggerMap.end())
            return itr->second;
        return 0;
    }
    bool IsTavernAreaTrigger(uint32 Trigger_ID) const
    {
        return mTavernAreaTriggerSet.find(Trigger_ID) !=
               mTavernAreaTriggerSet.end();
    }

    bool IsGameObjectForQuests(uint32 entry) const
    {
        return mGameObjectForQuestSet.find(entry) !=
               mGameObjectForQuestSet.end();
    }

    GossipText const* GetGossipText(uint32 Text_ID) const;

    const WorldSafeLocsEntry* GetClosestGraveyard(
        float x, float y, float z, uint32 MapId, Team team);
    const WorldSafeLocsEntry* GetCorpseSafeLoc(
        float x, float y, float z, uint32 MapId, Team team);
    bool AddGraveYardLink(uint32 id, uint32 zone, Team team, bool inDB = true,
        bool corpse_safeloc = false);
    void LoadGraveyardZones();
    GraveYardData const* FindGraveYardData(uint32 id, uint32 zone) const;

    AreaTrigger const* GetAreaTrigger(uint32 trigger) const
    {
        auto itr = mAreaTriggers.find(trigger);
        if (itr != mAreaTriggers.end())
            return &itr->second;
        return nullptr;
    }

    AreaTrigger const* GetGoBackTrigger(uint32 Map) const;
    AreaTrigger const* GetMapEntranceTrigger(uint32 Map) const;

    RepRewardRate const* GetRepRewardRate(uint32 factionId) const
    {
        auto itr = m_RepRewardRateMap.find(factionId);
        if (itr != m_RepRewardRateMap.end())
            return &itr->second;

        return nullptr;
    }

    ReputationOnKillEntry const* GetReputationOnKillEntry(uint32 id) const
    {
        auto itr = mRepOnKill.find(id);
        if (itr != mRepOnKill.end())
            return &itr->second;
        return nullptr;
    }

    RepSpilloverTemplate const* GetRepSpilloverTemplate(uint32 factionId) const
    {
        auto itr = m_RepSpilloverTemplateMap.find(factionId);
        if (itr != m_RepSpilloverTemplateMap.end())
            return &itr->second;

        return nullptr;
    }

    PointOfInterest const* GetPointOfInterest(uint32 id) const
    {
        auto itr = mPointsOfInterest.find(id);
        if (itr != mPointsOfInterest.end())
            return &itr->second;
        return nullptr;
    }

    PetCreateSpellEntry const* GetPetCreateSpellEntry(uint32 id) const
    {
        auto itr = mPetCreateSpell.find(id);
        if (itr != mPetCreateSpell.end())
            return &itr->second;
        return nullptr;
    }

    void LoadArenaTeams();
    void LoadGroups();
    void LoadQuests();
    void LoadQuestRelations()
    {
        LoadGameobjectQuestRelations();
        LoadGameobjectInvolvedRelations();
        LoadCreatureQuestRelations();
        LoadCreatureInvolvedRelations();
    }
    void LoadGameobjectQuestRelations();
    void LoadGameobjectInvolvedRelations();
    void LoadCreatureQuestRelations();
    void LoadCreatureInvolvedRelations();

    bool LoadMangosStrings(
        DatabaseType& db, char const* table, int32 min_value, int32 max_value);
    bool LoadMangosStrings()
    {
        return LoadMangosStrings(WorldDatabase, "mangos_string",
            MIN_MANGOS_STRING_ID, MAX_MANGOS_STRING_ID);
    }
    void LoadPetCreateSpells();
    void LoadCreatureLocales();
    void LoadCreatureTemplates();
    void LoadCreatures();
    void LoadCreatureAddons();
    void LoadCreatureModelInfo();
    void LoadCreatureModelRace();
    void LoadEquipmentTemplates();
    void LoadGameObjectLocales();
    void LoadGameObjects();
    void LoadItemPrototypes();
    void LoadItemRequiredTarget();
    void LoadItemLocales();
    void LoadQuestLocales();
    void LoadGossipTextLocales();
    void LoadPageTextLocales();
    void LoadGossipMenuItemsLocales();
    void LoadPointOfInterestLocales();
    void LoadInstanceTemplate();
    void LoadWorldTemplate();
    void LoadConditions();
    void LoadMailLevelRewards();

    void LoadGossipText();

    void LoadAreaTriggerTeleports();
    void LoadQuestAreaTriggers();
    void LoadTavernAreaTriggers();
    void LoadGameObjectForQuests();

    void LoadItemTexts();
    void LoadPageTexts();

    void LoadPlayerInfo();
    void LoadPetLevelInfo();
    void LoadPetScaling();
    void LoadExplorationBaseXP();
    void LoadPetNames();
    void LoadPetNumber();
    void LoadCorpses();
    void LoadFishingBaseSkillLevel();

    void LoadReputationRewardRate();
    void LoadReputationOnKill();
    void LoadReputationSpilloverTemplate();

    void LoadPointsOfInterest();

    void LoadWeatherZoneChances();
    void LoadGameTele();

    void LoadNpcGossips();

    void LoadGossipMenus();

    void LoadVendorTemplates();
    void LoadVendors() { LoadVendors("npc_vendor", false); }
    void LoadTrainerTemplates();
    void LoadTrainers() { LoadTrainers("npc_trainer", false); }

    void LoadNpcAggroLink();

    void LoadMapEntryRequirements();

    void LoadExtendedItemCost();

    std::string GeneratePetName(uint32 entry);
    uint32 GetBaseXP(uint32 level) const;
    uint32 GetXPForLevel(uint32 level) const;
    uint32 GetXPForPetLevel(uint32 level) const
    {
        return GetXPForLevel(level) / 4;
    }

    int32 GetFishingBaseSkillLevel(uint32 entry) const
    {
        auto itr = mFishingBaseForArea.find(entry);
        return itr != mFishingBaseForArea.end() ? itr->second : 0;
    }

    void ReturnOrDeleteOldMails(bool serverUp);

    void SetHighestGuids();

    // used for set initial guid counter for map local guids
    uint32 GetFirstTemporaryCreatureLowGuid() const
    {
        return m_FirstTemporaryCreatureGuid;
    }
    uint32 GetFirstTemporaryGameObjectLowGuid() const
    {
        return m_FirstTemporaryGameObjectGuid;
    }

    // used in .npc add/.gobject add commands for adding static spawns
    uint32 GenerateStaticCreatureLowGuid()
    {
        if (m_StaticCreatureGuids.GetNextAfterMaxUsed() >=
            m_FirstTemporaryCreatureGuid)
            return 0;
        return m_StaticCreatureGuids.Generate();
    }
    uint32 GenerateStaticGameObjectLowGuid()
    {
        if (m_StaticGameObjectGuids.GetNextAfterMaxUsed() >=
            m_FirstTemporaryGameObjectGuid)
            return 0;
        return m_StaticGameObjectGuids.Generate();
    }

    uint32 GeneratePlayerLowGuid() { return m_CharGuids.Generate(); }
    uint32 GenerateItemLowGuid() { return m_ItemGuids.Generate(); }
    uint32 GenerateCorpseLowGuid() { return m_CorpseGuids.Generate(); }
    uint32 GenerateGroupLowGuid() { return m_GroupGuids.Generate(); }

    uint32 GenerateArenaTeamId() { return m_ArenaTeamIds.Generate(); }
    uint32 GenerateAuctionID() { return m_AuctionIds.Generate(); }
    uint32 GenerateGuildId() { return m_GuildIds.Generate(); }
    uint32 GenerateItemTextID() { return m_ItemGuids.Generate(); }
    uint32 GenerateMailID() { return m_MailIds.Generate(); }
    uint32 GeneratePetNumber() { return m_PetNumbers.Generate(); }

    uint32 CreateItemText(std::string text);
    void AddItemText(uint32 itemTextId, std::string text)
    {
        mItemTexts[itemTextId] = text;
    }
    std::string GetItemText(uint32 id) const
    {
        auto itr = mItemTexts.find(id);
        if (itr != mItemTexts.end())
            return itr->second;
        else
            return "There is no info for this item";
    }

    MailLevelReward const* GetMailLevelReward(uint32 level, uint32 raceMask)
    {
        MailLevelRewardMap::const_iterator map_itr =
            m_mailLevelRewardMap.find(level);
        if (map_itr == m_mailLevelRewardMap.end())
            return nullptr;

        for (const auto& elem : map_itr->second)
            if (elem.raceMask & raceMask)
                return &elem;

        return nullptr;
    }

    WeatherZoneChances const* GetWeatherChances(uint32 zone_id) const
    {
        auto itr = mWeatherZoneMap.find(zone_id);
        if (itr != mWeatherZoneMap.end())
            return &itr->second;
        else
            return nullptr;
    }

    CreatureDataPair const* GetCreatureDataPair(uint32 guid) const
    {
        auto itr = mCreatureDataMap.find(guid);
        if (itr == mCreatureDataMap.end())
            return nullptr;
        return &*itr;
    }

    CreatureData const* GetCreatureData(uint32 guid) const
    {
        CreatureDataPair const* dataPair = GetCreatureDataPair(guid);
        return dataPair ? &dataPair->second : nullptr;
    }

    CreatureData& NewOrExistCreatureData(uint32 guid)
    {
        return mCreatureDataMap[guid];
    }
    void DeleteCreatureData(uint32 guid);

    template <typename Worker>
    void DoCreatureData(Worker& worker) const
    {
        for (const auto& elem : mCreatureDataMap)
            if (worker(elem))
                break;
    }

    CreatureLocale const* GetCreatureLocale(uint32 entry) const
    {
        auto itr = mCreatureLocaleMap.find(entry);
        if (itr == mCreatureLocaleMap.end())
            return nullptr;
        return &itr->second;
    }

    void GetCreatureLocaleStrings(uint32 entry, int32 loc_idx,
        char const** namePtr, char const** subnamePtr = nullptr) const;

    GameObjectLocale const* GetGameObjectLocale(uint32 entry) const
    {
        auto itr = mGameObjectLocaleMap.find(entry);
        if (itr == mGameObjectLocaleMap.end())
            return nullptr;
        return &itr->second;
    }

    ItemLocale const* GetItemLocale(uint32 entry) const
    {
        auto itr = mItemLocaleMap.find(entry);
        if (itr == mItemLocaleMap.end())
            return nullptr;
        return &itr->second;
    }

    void GetItemLocaleStrings(uint32 entry, int32 loc_idx, std::string* namePtr,
        std::string* descriptionPtr = nullptr) const;

    QuestLocale const* GetQuestLocale(uint32 entry) const
    {
        auto itr = mQuestLocaleMap.find(entry);
        if (itr == mQuestLocaleMap.end())
            return nullptr;
        return &itr->second;
    }

    void GetQuestLocaleStrings(
        uint32 entry, int32 loc_idx, std::string* titlePtr) const;

    NpcTextLocale const* GetNpcTextLocale(uint32 entry) const
    {
        auto itr = mNpcTextLocaleMap.find(entry);
        if (itr == mNpcTextLocaleMap.end())
            return nullptr;
        return &itr->second;
    }

    typedef std::string NpcTextArray[MAX_GOSSIP_TEXT_OPTIONS];
    void GetNpcTextLocaleStringsAll(uint32 entry, int32 loc_idx,
        NpcTextArray* text0_Ptr, NpcTextArray* text1_Ptr) const;
    void GetNpcTextLocaleStrings0(uint32 entry, int32 loc_idx,
        std::string* text0_0_Ptr, std::string* text1_0_Ptr) const;

    PageTextLocale const* GetPageTextLocale(uint32 entry) const
    {
        auto itr = mPageTextLocaleMap.find(entry);
        if (itr == mPageTextLocaleMap.end())
            return nullptr;
        return &itr->second;
    }

    GossipMenuItemsLocale const* GetGossipMenuItemsLocale(uint32 entry) const
    {
        auto itr = mGossipMenuItemsLocaleMap.find(entry);
        if (itr == mGossipMenuItemsLocaleMap.end())
            return nullptr;
        return &itr->second;
    }

    PointOfInterestLocale const* GetPointOfInterestLocale(uint32 poi_id) const
    {
        auto itr = mPointOfInterestLocaleMap.find(poi_id);
        if (itr == mPointOfInterestLocaleMap.end())
            return nullptr;
        return &itr->second;
    }

    GameObjectDataPair const* GetGODataPair(uint32 guid) const
    {
        auto itr = mGameObjectDataMap.find(guid);
        if (itr == mGameObjectDataMap.end())
            return nullptr;
        return &*itr;
    }

    GameObjectData const* GetGOData(uint32 guid) const
    {
        GameObjectDataPair const* dataPair = GetGODataPair(guid);
        return dataPair ? &dataPair->second : nullptr;
    }

    GameObjectData& NewGOData(uint32 guid) { return mGameObjectDataMap[guid]; }
    void DeleteGOData(uint32 guid);

    template <typename Worker>
    void DoGOData(Worker& worker) const
    {
        for (const auto& elem : mGameObjectDataMap)
            if (worker(elem)) // arg = GameObjectDataPair
                break;
    }

    MangosStringLocale const* GetMangosStringLocale(int32 entry) const
    {
        auto itr = mMangosStringLocaleMap.find(entry);
        if (itr == mMangosStringLocaleMap.end())
            return nullptr;
        return &itr->second;
    }

    const char* GetMangosString(int32 entry, int locale_idx) const;
    const char* GetMangosStringForDBCLocale(int32 entry) const
    {
        return GetMangosString(entry, DBCLocaleIndex);
    }
    int32 GetDBCLocaleIndex() const { return DBCLocaleIndex; }
    void SetDBCLocaleIndex(uint32 lang)
    {
        DBCLocaleIndex = GetIndexForLocale(LocaleConstant(lang));
    }

    // Add/Remove static entities (these entities are automatically spawned when
    // their owning map and cell is loaded). Only add things actually in the DB.
    // DB management is not handled by this, these are callbacks exposed to the
    // DB code. For instance specific, persistent entities see
    // MapPersistentStateMgr.h.
    void add_static_creature(const CreatureData* data);
    void remove_static_creature(const CreatureData* data);
    // If gameobject is an elevator, it gets added to static elevators
    void add_static_game_object(const GameObjectData* data);
    void remove_static_game_object(const GameObjectData* data);
    void add_static_corpse(Corpse* corpse);
    void remove_static_corpse(Corpse* corpse);
    // Returns nullptr if no such data exists for cell
    const google::dense_hash_set<const CreatureData*>* get_static_creatures(
        int map_id, int x, int y) const;
    const google::dense_hash_set<const GameObjectData*>*
    get_static_game_objects(int map_id, int x, int y) const;
    const google::dense_hash_map<uint32, uint32>* get_static_corpses(
        int map_id, int x, int y) const;
    const google::dense_hash_set<const GameObjectData*>* get_static_elevators(
        int map_id) const;

    // reserved names
    void LoadReservedPlayersNames();
    bool IsReservedName(const std::string& name) const;

    // name with valid structure and symbols
    static uint8 CheckPlayerName(const std::string& name, bool create = false);
    static PetNameInvalidReason CheckPetName(const std::string& name);
    static bool IsValidCharterName(const std::string& name);

    static bool CheckDeclinedNames(
        std::wstring mainpart, DeclinedName const& names);

    int GetIndexForLocale(LocaleConstant loc);
    LocaleConstant GetLocaleForIndex(int i);

    // TODO: Outdated version, rename NEW and remove soon
    uint16 GetConditionId(
        ConditionType condition, uint32 value1, uint32 value2);
    bool IsPlayerMeetToCondition(
        Player const* player, uint16 condition_id) const
    {
        if (condition_id >= mConditions.size())
            return false;

        return mConditions[condition_id].Meets(player);
    }

    // Check if a player meets condition conditionId
    bool IsPlayerMeetToNEWCondition(
        Player const* pPlayer, uint16 conditionId) const
    {
        if (!pPlayer)
            return false; // player not present, return false

        if (const PlayerCondition* condition =
                sConditionStorage.LookupEntry<PlayerCondition>(conditionId))
            return condition->Meets(pPlayer);

        return false;
    }

    GameTele const* GetGameTele(uint32 id) const
    {
        auto itr = m_GameTeleMap.find(id);
        if (itr == m_GameTeleMap.end())
            return nullptr;
        return &itr->second;
    }

    GameTele const* GetGameTele(
        const std::string& name, bool partialMatch) const;
    GameTeleMap const& GetGameTeleMap() const { return m_GameTeleMap; }
    bool AddGameTele(GameTele& data);
    bool DeleteGameTele(const std::string& name);

    uint32 GetNpcGossip(uint32 entry) const
    {
        auto iter = m_mCacheNpcTextIdMap.find(entry);
        if (iter == m_mCacheNpcTextIdMap.end())
            return 0;

        return iter->second;
    }

    TrainerSpellData const* GetNpcTrainerSpells(uint32 entry) const
    {
        auto iter = m_mCacheTrainerSpellMap.find(entry);
        if (iter == m_mCacheTrainerSpellMap.end())
            return nullptr;

        return &iter->second;
    }

    TrainerSpellData const* GetNpcTrainerTemplateSpells(uint32 entry) const
    {
        auto iter = m_mCacheTrainerTemplateSpellMap.find(entry);
        if (iter == m_mCacheTrainerTemplateSpellMap.end())
            return nullptr;

        return &iter->second;
    }

    VendorItemData const* GetNpcVendorItemList(uint32 entry) const
    {
        auto iter = m_mCacheVendorItemMap.find(entry);
        if (iter == m_mCacheVendorItemMap.end())
            return nullptr;

        return &iter->second;
    }

    VendorItemData const* GetNpcVendorTemplateItemList(uint32 entry) const
    {
        auto iter = m_mCacheVendorTemplateItemMap.find(entry);
        if (iter == m_mCacheVendorTemplateItemMap.end())
            return nullptr;

        return &iter->second;
    }

    void AddVendorItem(uint32 entry, uint32 item, uint32 maxcount,
        uint32 incrtime, uint32 ExtendedCost, uint32 weight);
    bool RemoveVendorItem(uint32 entry, uint32 item);
    bool IsVendorItemValid(bool isTemplate, char const* tableName,
        uint32 vendor_entry, uint32 item, uint32 maxcount, uint32 ptime,
        uint32 ExtendedCost, Player* pl = nullptr,
        std::set<uint32>* skip_vendors = nullptr) const;

    int GetOrNewIndexForLocale(LocaleConstant loc);

    ItemRequiredTargetMapBounds GetItemRequiredTargetMapBounds(
        uint32 uiItemEntry) const
    {
        return m_ItemRequiredTarget.equal_range(uiItemEntry);
    }

    GossipMenusMapBounds GetGossipMenusMapBounds(uint32 uiMenuId) const
    {
        return m_mGossipMenusMap.equal_range(uiMenuId);
    }

    GossipMenuItemsMapBounds GetGossipMenuItemsMapBounds(uint32 uiMenuId) const
    {
        return m_mGossipMenuItemsMap.equal_range(uiMenuId);
    }

    ExclusiveQuestGroupsMapBounds GetExclusiveQuestGroupsMapBounds(
        int32 groupId) const
    {
        return m_ExclusiveQuestGroups.equal_range(groupId);
    }

    QuestRelationsMapBounds GetCreatureQuestRelationsMapBounds(
        uint32 entry) const
    {
        return m_CreatureQuestRelations.equal_range(entry);
    }

    QuestRelationsMapBounds GetCreatureQuestInvolvedRelationsMapBounds(
        uint32 entry) const
    {
        return m_CreatureQuestInvolvedRelations.equal_range(entry);
    }

    QuestRelationsMapBounds GetGOQuestRelationsMapBounds(uint32 entry) const
    {
        return m_GOQuestRelations.equal_range(entry);
    }

    QuestRelationsMapBounds GetGOQuestInvolvedRelationsMapBounds(
        uint32 entry) const
    {
        return m_GOQuestInvolvedRelations.equal_range(entry);
    }

    QuestRelationsMap& GetCreatureQuestRelationsMap()
    {
        return m_CreatureQuestRelations;
    }

    // returns: points of health per stamina
    float GetPetStaminaScaling(uint32 cid) const;
    // returns: points of mana per intellect
    float GetPetIntellectScaling(uint32 cid) const;

    uint32 GetModelForRace(uint32 sourceModelId, uint32 racemask) const;

    void AddLocaleString(const std::string& s, LocaleConstant locale,
        std::vector<std::string>& data);
    // FIXME: This is retarded (shouldn't be a part of the objectmgr class)
    inline void GetLocaleString(const std::vector<std::string>& data,
        int loc_idx, std::string& value) const
    {
        if (data.size() > size_t(loc_idx) && !data[loc_idx].empty())
            value = data[loc_idx];
    }

    void SetGraveYardLinkTeam(uint32 id, uint32 zoneId, Team team);

    const linked_mobs_vector* get_defenders(ObjectGuid boss) const
    {
        auto itr = aggro_links_.find(boss);
        if (itr != aggro_links_.end())
            return &itr->second;
        return nullptr;
    }

    const MapEntryRequirements* GetMapEntryRequirements(uint32 map) const
    {
        auto itr = map_entry_req_.find(map);
        if (itr != map_entry_req_.end())
            return &itr->second;
        return nullptr;
    }

    const ItemExtendedCostEntry* GetExtendedCostOverride(uint32 id) const
    {
        auto itr = extended_cost_.find(id);
        if (itr != extended_cost_.end())
            return &itr->second;
        return nullptr;
    }

protected:
    // first free id for selected id type
    IdGenerator<uint32> m_ArenaTeamIds;
    IdGenerator<uint32> m_AuctionIds;
    IdGenerator<uint32> m_GuildIds;
    IdGenerator<uint32> m_ItemTextIds;
    IdGenerator<uint32> m_MailIds;
    IdGenerator<uint32> m_PetNumbers;

    // initial free low guid for selected guid type for map local guids
    uint32 m_FirstTemporaryCreatureGuid;
    uint32 m_FirstTemporaryGameObjectGuid;

    // guids from reserved range for use in .npc add/.gobject add commands for
    // adding new static spawns (saved in DB) from client.
    ObjectGuidGenerator<HIGHGUID_UNIT> m_StaticCreatureGuids;
    ObjectGuidGenerator<HIGHGUID_GAMEOBJECT> m_StaticGameObjectGuids;

    // first free low guid for selected guid type
    ObjectGuidGenerator<HIGHGUID_PLAYER> m_CharGuids;
    ObjectGuidGenerator<HIGHGUID_ITEM> m_ItemGuids;
    ObjectGuidGenerator<HIGHGUID_CORPSE> m_CorpseGuids;
    ObjectGuidGenerator<HIGHGUID_GROUP> m_GroupGuids;

    QuestMap mQuestTemplates;

    typedef std::unordered_map<uint32, GossipText> GossipTextMap;
    typedef std::unordered_map<uint32, uint32> QuestAreaTriggerMap;
    typedef std::unordered_map<uint32, std::string> ItemTextMap;
    typedef std::set<uint32> TavernAreaTriggerSet;
    typedef std::set<uint32> GameObjectForQuestSet;

    typedef std::multimap<uint32, CreatureModelRace> CreatureModelRaceMap;
    typedef std::pair<CreatureModelRaceMap::const_iterator,
        CreatureModelRaceMap::const_iterator> CreatureModelRaceMapBounds;

    GroupMap mGroupMap;
    std::vector<std::pair<uint32, time_t>> mOfflineLeaderGroups;

    std::vector<ArenaTeam*> arena_rank_map_; // Sorted in asc order of rating
    ArenaTeamMap mArenaTeamMap;

    ItemTextMap mItemTexts;

    QuestAreaTriggerMap mQuestAreaTriggerMap;
    TavernAreaTriggerSet mTavernAreaTriggerSet;
    GameObjectForQuestSet mGameObjectForQuestSet;
    GossipTextMap mGossipText;
    AreaTriggerMap mAreaTriggers;

    RepRewardRateMap m_RepRewardRateMap;
    RepOnKillMap mRepOnKill;
    RepSpilloverTemplateMap m_RepSpilloverTemplateMap;

    GossipMenusMap m_mGossipMenusMap;
    GossipMenuItemsMap m_mGossipMenuItemsMap;
    PointOfInterestMap mPointsOfInterest;

    WeatherZoneMap mWeatherZoneMap;

    PetCreateSpellMap mPetCreateSpell;

    // character reserved names
    typedef std::set<std::wstring> ReservedNamesMap;
    ReservedNamesMap m_ReservedNames;

    GraveYardMap mGraveYardMap;

    GameTeleMap m_GameTeleMap;

    ItemRequiredTargetMap m_ItemRequiredTarget;

    typedef std::vector<LocaleConstant> LocalForIndex;
    LocalForIndex m_LocalForIndex;

    ExclusiveQuestGroupsMap m_ExclusiveQuestGroups;

    QuestRelationsMap m_CreatureQuestRelations;
    QuestRelationsMap m_CreatureQuestInvolvedRelations;
    QuestRelationsMap m_GOQuestRelations;
    QuestRelationsMap m_GOQuestInvolvedRelations;

    int DBCLocaleIndex;

private:
    void LoadCreatureAddons(
        SQLStorage& creatureaddons, char const* entryName, char const* comment);
    void ConvertCreatureAddonAuras(
        CreatureDataAddon* addon, char const* table, char const* guidEntryStr);
    void LoadQuestRelationsHelper(QuestRelationsMap& map, char const* table);
    void LoadVendors(char const* tableName, bool isTemplates);
    void LoadTrainers(char const* tableName, bool isTemplates);

    void LoadGossipMenu(std::set<uint32>& gossipScriptSet);
    void LoadGossipMenuItems(std::set<uint32>& gossipScriptSet);

    std::vector<ArenaTeam*>::const_iterator get_arena_team_rank_itr(
        ArenaTeam* team) const;

    MailLevelRewardMap m_mailLevelRewardMap;

    typedef std::map<uint32, PetLevelInfo*> PetLevelInfoMap;
    // PetLevelInfoMap[creature_id][level]
    PetLevelInfoMap petInfo; // [creature_id][level]
    struct pet_scaling_entry
    {
        float stamina;
        float intellect;
    };
    std::map<uint32, pet_scaling_entry> pet_scaling_; // indexed on creature id

    PlayerClassInfo playerClassInfo[MAX_CLASSES];

    void BuildPlayerLevelInfo(
        uint8 race, uint8 class_, uint8 level, PlayerLevelInfo* plinfo) const;
    PlayerInfo playerInfo[MAX_RACES][MAX_CLASSES];

    typedef std::vector<uint32> PlayerXPperLevel; // [level]
    PlayerXPperLevel mPlayerXPperLevel;

    typedef std::map<uint32, uint32> BaseXPMap; // [area level][base xp]
    BaseXPMap mBaseXPTable;

    typedef std::map<uint32, int32>
        FishingBaseSkillMap; // [areaId][base skill level]
    FishingBaseSkillMap mFishingBaseForArea;

    typedef std::map<uint32, std::vector<std::string>> HalfNameMap;
    HalfNameMap PetHalfName0;
    HalfNameMap PetHalfName1;

    // Staticly spawned enitites
    // key (map << 32 | cell), value: set(data)
    google::dense_hash_map<uint64, google::dense_hash_set<const CreatureData*>>
        static_creatures_;
    // key (map << 32 | cell), value: set(data)
    google::dense_hash_map<uint64,
        google::dense_hash_set<const GameObjectData*>> static_game_objects_;
    // grid,
    // key (map << 32 | cell), value: map(corpse low guid, instance id)
    google::dense_hash_map<uint64, google::dense_hash_map<uint32, uint32>>
        static_corpses_;
    // key map, value: set(data)
    google::dense_hash_map<int, google::dense_hash_set<const GameObjectData*>>
        static_elevators_;

    CreatureDataMap mCreatureDataMap;
    CreatureLocaleMap mCreatureLocaleMap;
    GameObjectDataMap mGameObjectDataMap;
    GameObjectLocaleMap mGameObjectLocaleMap;
    ItemLocaleMap mItemLocaleMap;
    QuestLocaleMap mQuestLocaleMap;
    NpcTextLocaleMap mNpcTextLocaleMap;
    PageTextLocaleMap mPageTextLocaleMap;
    MangosStringLocaleMap mMangosStringLocaleMap;
    GossipMenuItemsLocaleMap mGossipMenuItemsLocaleMap;
    PointOfInterestLocaleMap mPointOfInterestLocaleMap;

    // Storage for Conditions. First element (index 0) is reserved for
    // zero-condition (nothing required)
    typedef std::vector<PlayerCondition> ConditionStore;
    ConditionStore mConditions;

    CreatureModelRaceMap m_mCreatureModelRaceMap;

    CacheNpcTextIdMap m_mCacheNpcTextIdMap;
    CacheVendorItemMap m_mCacheVendorTemplateItemMap;
    CacheVendorItemMap m_mCacheVendorItemMap;
    CacheTrainerSpellMap m_mCacheTrainerTemplateSpellMap;
    CacheTrainerSpellMap m_mCacheTrainerSpellMap;

    aggro_link_map aggro_links_;

    std::map<uint32, MapEntryRequirements> map_entry_req_;

    std::map<uint32, ItemExtendedCostEntry> extended_cost_;
};

#define sObjectMgr MaNGOS::UnlockedSingleton<ObjectMgr>

// scripting access functions
MANGOS_DLL_SPEC bool LoadMangosStrings(DatabaseType& db, char const* table,
    int32 start_value = MAX_CREATURE_AI_TEXT_STRING_ID,
    int32 end_value = std::numeric_limits<int32>::min());
MANGOS_DLL_SPEC CreatureInfo const* GetCreatureTemplateStore(uint32 entry);
MANGOS_DLL_SPEC Quest const* GetQuestTemplateStore(uint32 entry);

#endif
