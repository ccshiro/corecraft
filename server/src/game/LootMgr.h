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

#ifndef MANGOS_LOOTMGR_H
#define MANGOS_LOOTMGR_H

#include "ByteBuffer.h"
#include "ItemEnchantmentMgr.h"
#include "ObjectGuid.h"
#include "SharedDefines.h"
#include "Utilities/LinkedReference/RefManager.h"
#include <map>
#include <set>
#include <unordered_map>
#include <vector>

class Group;
class LootStore;
class Player;
class WorldObject;
class loot_recipient_mgr;
// note: the client cannot show more than 16 items total
#define MAX_NR_LOOT_ITEMS 16
// unrelated to the number of quest items shown, just for reserve
#define MAX_NR_QUEST_ITEMS 32
// Timeout for rolling on loot
#define LOOT_ROLL_TIMEOUT (1 * MINUTE * IN_MILLISECONDS)

enum PermissionTypes
{
    ALL_PERMISSION = 0,
    GROUP_PERMISSION = 1,
    MASTER_PERMISSION = 2,
    OWNER_PERMISSION = 3, // for single player only loots
    NONE_PERMISSION = 4
};

enum LootType
{
    LOOT_NONE = 0,

    LOOT_CORPSE = 1,
    LOOT_PICKPOCKETING = 2,
    LOOT_FISHING = 3,
    LOOT_DISENCHANTING = 4,
    // ignored always by client
    LOOT_SKINNING =
        6, // unsupported by client, sending LOOT_PICKPOCKETING instead
    LOOT_PROSPECTING =
        7, // unsupported by client, sending LOOT_PICKPOCKETING instead

    LOOT_FISHINGHOLE =
        20, // unsupported by client, sending LOOT_FISHING instead
    LOOT_FISHING_FAIL =
        21,             // unsupported by client, sending LOOT_FISHING instead
    LOOT_INSIGNIA = 22, // unsupported by client, sending LOOT_CORPSE instead
    LOOT_NORMAL_ITEM =
        23, // unsupported by client, sending LOOT_PICKPOCKETING instead
    LOOT_LOCKPICKING =
        24 // unsupported by client, sending LOOT_SKINNING instead
};

enum LootSlotType
{
    LOOT_SLOT_NORMAL = 0, // can be looted
    LOOT_SLOT_VIEW = 1,   // can only be viewed
    LOOT_SLOT_MASTER = 2, // can be looted only be the master looter
    LOOT_SLOT_REQS = 3,   // can't be looted (error message about missing reqs)
    LOOT_SLOT_OWNER =
        4, // ignore binding confirmation and etc, for single player looting
    MAX_LOOT_SLOT_TYPE, //
    LOOT_SLOT_NONE // Server side indication to not send the loot to the client
};

enum LootMethod
{
    FREE_FOR_ALL = 0,
    ROUND_ROBIN = 1,
    MASTER_LOOT = 2,
    GROUP_LOOT = 3,
    NEED_BEFORE_GREED = 4
};

enum class LootTable
{
    creature,
    gameobject,
    item,
    mail,
    disenchant,
    fishing,
    pickpocketing,
    prospecting,
    skinning,
    reference
};

struct LootStoreItem
{
    uint32 entry;        // DB entry
    uint32 itemid;       // id of the item
    float chance;        // always positive, chance to drop for both quest and
                         // non-quest items, chance to be used for refs
    int32 mincountOrRef; // mincount for drop items (positive) or minus
                         // referenced TemplateleId (negative)
    uint8 group : 7;
    bool needs_quest : 1; // quest drop (negative ChanceOrQuestChance in DB)
    uint8 maxcount : 8; // max drop count for the item (mincountOrRef positive)
                        // or Ref multiplicator (mincountOrRef negative)
    uint16 conditionId : 16; // additional loot condition Id

    LootTable table;

    // Constructor, converting ChanceOrQuestChance -> (chance, needs_quest)
    // displayid is filled in IsValid() which must be called after
    LootStoreItem(uint32 _entry, uint32 _itemid, float _chanceOrQuestChance,
        int8 _group, uint16 _conditionId, int32 _mincountOrRef, uint8 _maxcount,
        LootTable type)
      : entry(_entry), itemid(_itemid), chance(fabs(_chanceOrQuestChance)),
        mincountOrRef(_mincountOrRef), group(_group),
        needs_quest(_chanceOrQuestChance < 0), maxcount(_maxcount),
        conditionId(_conditionId), table(type)
    {
    }

    bool Roll(bool rate)
        const; // Checks if the entry takes it's chance (at loot generation)
    bool IsValid(LootStore const& store, uint32 entry) const;
    // Checks correctness of values
};

struct LootItem
{
    uint32 entry;
    uint32 itemid;
    uint32 randomSuffix;
    int32 randomPropertyId;
    uint16 conditionId : 16; // allow compiler pack structure
    uint8 count : 8;
    bool is_looted : 1;
    bool is_blocked : 1;
    bool one_per_player : 1; // there exists one copy for each player
    bool is_underthreshold : 1;
    bool is_counted : 1;
    bool needs_quest : 1;    // quest drop
    bool has_conditions : 1; // True if ConditionMgr has conditions for this
    ItemQualities item_quality;
    LootTable table;

    // Constructor, copies most fields from LootStoreItem, generates random
    // count and random suffixes/properties
    // Should be called for non-reference LootStoreItem entries only
    // (mincountOrRef > 0)
    explicit LootItem(LootStoreItem const& li);

    LootItem(uint32 itemid_, uint32 count_, uint32 randomSuffix_ = 0,
        int32 randomPropertyId_ = 0, uint32 entry_ = 0,
        LootTable type = LootTable::creature);
};

typedef std::vector<LootItem> LootItemList;

struct QuestItem
{
    uint8 index; // position in quest_items;
    bool is_looted;

    QuestItem() : index(0), is_looted(false) {}

    QuestItem(uint8 _index, bool _islooted = false)
      : index(_index), is_looted(_islooted)
    {
    }
};

class Loot;
class LootTemplate;

typedef std::vector<LootStoreItem> LootStoreItemList;
typedef std::unordered_map<uint32, LootTemplate*> LootTemplateMap;

typedef std::set<uint32> LootIdSet;

class LootStore
{
public:
    explicit LootStore(char const* name, char const* entryName,
        bool ratesAllowed, LootTable type)
      : m_name(name), m_entryName(entryName), m_ratesAllowed(ratesAllowed),
        m_tableType(type)
    {
    }
    virtual ~LootStore() { Clear(); }

    void Verify() const;

    void LoadAndCollectLootIds(LootIdSet& ids_set);
    void CheckLootRefs(LootIdSet* ref_set = nullptr)
        const; // check existence reference and remove it from ref_set
    void ReportUnusedIds(LootIdSet const& ids_set) const;
    void ReportNotExistedId(uint32 id) const;

    bool HaveLootFor(uint32 loot_id) const
    {
        return m_LootTemplates.find(loot_id) != m_LootTemplates.end();
    }
    bool HaveQuestLootFor(uint32 loot_id) const;
    bool HaveQuestLootForPlayer(uint32 loot_id, Player* player) const;

    LootTemplate const* GetLootFor(uint32 loot_id) const;

    char const* GetName() const { return m_name; }
    char const* GetEntryName() const { return m_entryName; }
    bool IsRatesAllowed() const { return m_ratesAllowed; }

protected:
    void LoadLootTable();
    void Clear();

private:
    LootTemplateMap m_LootTemplates;
    char const* m_name;
    char const* m_entryName;
    bool m_ratesAllowed;
    LootTable m_tableType;
};

class LootTemplate
{
    class LootGroup; // A set of loot definitions for items (refs are not
                     // allowed inside)
    typedef std::vector<LootGroup> LootGroups;

public:
    // Adds an entry to the group (at loading stage)
    void AddEntry(LootStoreItem& item);
    // Rolls for every item in the template and adds the rolled items the the
    // loot
    void Process(
        Loot& loot, LootStore const& store, bool rate, uint8 GroupId = 0) const;

    // True if template includes at least 1 quest drop entry
    bool HasQuestDrop(LootTemplateMap const& store, uint8 GroupId = 0) const;
    // True if template includes at least 1 quest drop for an active quest of
    // the player
    bool HasQuestDropForPlayer(LootTemplateMap const& store,
        Player const* player, uint8 GroupId = 0) const;

    // Checks integrity of the template
    void Verify(LootStore const& store, uint32 Id) const;
    void CheckLootRefs(LootIdSet* ref_set) const;

private:
    LootStoreItemList Entries; // not grouped only
    LootGroups Groups; // groups have own (optimised) processing, grouped
                       // entries go there
};

//=====================================================

class LootValidatorRef : public Reference<Loot, LootValidatorRef>
{
public:
    LootValidatorRef() {}
    void targetObjectDestroyLink() override {}
    void sourceObjectDestroyLink() override {}
};

//=====================================================

class LootValidatorRefManager : public RefManager<Loot, LootValidatorRef>
{
public:
    typedef LinkedListHead::Iterator<LootValidatorRef> iterator;

    LootValidatorRef* getFirst()
    {
        return (
            LootValidatorRef*)RefManager<Loot, LootValidatorRef>::getFirst();
    }
    LootValidatorRef* getLast()
    {
        return (LootValidatorRef*)RefManager<Loot, LootValidatorRef>::getLast();
    }

    iterator begin() { return iterator(getFirst()); }
    iterator end() { return iterator(nullptr); }
    iterator rbegin() { return iterator(getLast()); }
    iterator rend() { return iterator(nullptr); }
};

//=====================================================

class Loot
{
public:
    typedef std::unordered_map<ObjectGuid, std::set<uint8 /* item index */>>
        PlayerLootSlotMap;
    friend class loot_distributor;

    Loot(uint32 gold = 0)
      : gold_(gold), unlooted_count_(0), loot_type_(LOOT_NONE)
    {
    }
    ~Loot() { clear(); }

    void clear()
    {
        looting_players_.clear();
        items_.clear();
        quest_items_.clear();
        conditional_item_map_.clear();
        quest_item_map_.clear();
        one_per_player_map_.clear();
        player_loot_slots_.clear();
        gold_ = 0;
        unlooted_count_ = 0;
    }

    void generate_money_loot(uint32 min, uint32 max);
    bool fill_loot(uint32 loot_id, LootStore const& store,
        loot_recipient_mgr* recipients, bool personal,
        bool noEmptyError = false);

    bool empty() const { return items_.empty() && gold_ == 0; }
    bool looted() const { return gold_ == 0 && unlooted_count_ == 0; }
    size_t size() const { return items_.size() + quest_items_.size(); }
    uint32 gold() const { return gold_; };

    // can return true even if no quest items are in the loot
    bool do_only_quest_items_remain() const;

    // Players viewing the loot (i.e. has the loot window open)
    void add_looter(ObjectGuid guid, std::set<uint8>& lootableIndices)
    {
        looting_players_.insert(guid);
        player_loot_slots_[guid] = lootableIndices;
    }
    void remove_looter(ObjectGuid guid)
    {
        looting_players_.erase(guid);
        looting_players_.erase(guid);
    }
    bool has_looter(ObjectGuid guid) const
    {
        return looting_players_.find(guid) != looting_players_.end();
    }
    uint32 looters_count() const { return looting_players_.size(); }

    // loot_slot is of range 0 ... items.size() + quest_items_.size() - 1
    void notify_item_removed(uint8 loot_slot);
    void notify_quest_item_removed(uint8 loot_slot);
    void notify_money_removed();

    void add_item(LootStoreItem const& item);
    // Only checks conditions & quest status. Do not use it as a sole method of
    // lootability.
    bool can_loot_item(
        LootItem* item, ObjectGuid looter, bool is_anyones_loot = false) const;
    void on_loot_item(LootItem* item, ObjectGuid looter)
    {
        if (item->one_per_player)
            one_per_player_map_[item].insert(looter);
    }

    inline bool can_see_item_slot(ObjectGuid looter, uint8 loot_slot) const;
    inline const LootItem* get_slot_item(uint8 loot_slot) const;
    inline LootItem* get_slot_item(uint8 loot_slot);

private:
    typedef std::map<LootItem*, std::set<ObjectGuid>> LootItemPlayerMap;
    typedef std::set<ObjectGuid> PlayersLooting;

    LootItemList items_;
    LootItemList quest_items_;
    uint32 gold_;
    int unlooted_count_;
    LootType loot_type_;
    PlayersLooting looting_players_;
    // Players mapped to loot
    LootItemPlayerMap conditional_item_map_;
    LootItemPlayerMap quest_item_map_;
    LootItemPlayerMap one_per_player_map_; // This is reversed. Player who have
                                           // already taken their
                                           // item->one_per_player copy are in
                                           // here and cannot take another
    PlayerLootSlotMap player_loot_slots_;  // Map of players that are looting,
                                           // and what slot indices they can see

    size_t map_conditional_item(LootItem* item, loot_recipient_mgr* recipients);
    size_t map_quest_item(LootItem* item, loot_recipient_mgr* recipients);

    // loot that anyone can view needs to check availability when the looting
    // happens
    bool can_loot_item_now(LootItem* item, ObjectGuid looter) const;
};

inline bool Loot::can_see_item_slot(ObjectGuid looter, uint8 loot_slot) const
{
    auto find = player_loot_slots_.find(looter);
    if (find == player_loot_slots_.end())
        return false;
    return find->second.find(loot_slot) != find->second.end();
}
inline const LootItem* Loot::get_slot_item(uint8 loot_slot) const
{
    if (loot_slot >= items_.size()) // Quest item?
    {
        if (loot_slot - items_.size() >= quest_items_.size())
            return nullptr; // Invalid loot slot
        else
            return &quest_items_[loot_slot - items_.size()]; // Quest Item
    }
    else
        return &items_[loot_slot]; // Normal Item
}
inline LootItem* Loot::get_slot_item(uint8 loot_slot)
{
    if (loot_slot >= items_.size()) // Quest item?
    {
        if (loot_slot - items_.size() >= quest_items_.size())
            return nullptr;
        else
            return &quest_items_[loot_slot - items_.size()]; // Quest Item
    }
    else
        return &items_[loot_slot]; // Normal Item
}

extern LootStore LootTemplates_Creature;
extern LootStore LootTemplates_Fishing;
extern LootStore LootTemplates_Gameobject;
extern LootStore LootTemplates_Item;
extern LootStore LootTemplates_Mail;
extern LootStore LootTemplates_Pickpocketing;
extern LootStore LootTemplates_Skinning;
extern LootStore LootTemplates_Disenchant;
extern LootStore LootTemplates_Prospecting;

void LoadLootTemplates_Creature();
void LoadLootTemplates_Fishing();
void LoadLootTemplates_Gameobject();
void LoadLootTemplates_Item();
void LoadLootTemplates_Mail();
void LoadLootTemplates_Pickpocketing();
void LoadLootTemplates_Skinning();
void LoadLootTemplates_Disenchant();
void LoadLootTemplates_Prospecting();

void LoadLootTemplates_Reference();

inline void LoadLootTables()
{
    LoadLootTemplates_Creature();
    LoadLootTemplates_Fishing();
    LoadLootTemplates_Gameobject();
    LoadLootTemplates_Item();
    LoadLootTemplates_Mail();
    LoadLootTemplates_Pickpocketing();
    LoadLootTemplates_Skinning();
    LoadLootTemplates_Disenchant();
    LoadLootTemplates_Prospecting();

    LoadLootTemplates_Reference();

    logging.info("Loaded 10 loot tables\n");
}

#endif
