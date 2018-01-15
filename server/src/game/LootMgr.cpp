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

#include "LootMgr.h"
#include "ConditionMgr.h"
#include "logging.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "ProgressBar.h"
#include "SharedDefines.h"
#include "Util.h"
#include "World.h"
#include "loot_distributor.h"

LootStore LootTemplates_Creature(
    "creature_loot_template", "creature entry", true, LootTable::creature);
LootStore LootTemplates_Disenchant("disenchant_loot_template",
    "item disenchant id", true, LootTable::disenchant);
LootStore LootTemplates_Fishing(
    "fishing_loot_template", "area id", true, LootTable::fishing);
LootStore LootTemplates_Gameobject("gameobject_loot_template",
    "gameobject lootid", true, LootTable::gameobject);
LootStore LootTemplates_Item("item_loot_template",
    "item entry with ITEM_FLAG_LOOTABLE", true, LootTable::item);
LootStore LootTemplates_Mail(
    "mail_loot_template", "mail template id", false, LootTable::mail);
LootStore LootTemplates_Pickpocketing("pickpocketing_loot_template",
    "creature pickpocket lootid", true, LootTable::pickpocketing);
LootStore LootTemplates_Prospecting("prospecting_loot_template",
    "item entry (ore)", true, LootTable::prospecting);
LootStore LootTemplates_Reference(
    "reference_loot_template", "reference id", false, LootTable::reference);
LootStore LootTemplates_Skinning("skinning_loot_template",
    "creature skinning id", true, LootTable::skinning);

namespace
{
ConditionSourceType table_to_condition_type(LootTable table_type)
{
    switch (table_type)
    {
    case LootTable::creature:
        return CONDITION_SOURCE_TYPE_CREATURE_LOOT_TEMPLATE;
    case LootTable::gameobject:
        return CONDITION_SOURCE_TYPE_GAMEOBJECT_LOOT_TEMPLATE;
    case LootTable::item:
        return CONDITION_SOURCE_TYPE_ITEM_LOOT_TEMPLATE;
    case LootTable::mail:
        return CONDITION_SOURCE_TYPE_MAIL_LOOT_TEMPLATE;
    case LootTable::disenchant:
        return CONDITION_SOURCE_TYPE_DISENCHANT_LOOT_TEMPLATE;
    case LootTable::fishing:
        return CONDITION_SOURCE_TYPE_FISHING_LOOT_TEMPLATE;
    case LootTable::pickpocketing:
        return CONDITION_SOURCE_TYPE_PICKPOCKETING_LOOT_TEMPLATE;
    case LootTable::prospecting:
        return CONDITION_SOURCE_TYPE_PROSPECTING_LOOT_TEMPLATE;
    case LootTable::skinning:
        return CONDITION_SOURCE_TYPE_SKINNING_LOOT_TEMPLATE;
    case LootTable::reference:
        return CONDITION_SOURCE_TYPE_REFERENCE_LOOT_TEMPLATE;
    }
    return CONDITION_SOURCE_TYPE_NONE;
}

const ConditionList* get_conditions_for_item(LootItem* item)
{
    auto type = table_to_condition_type(item->table);
    if (type != CONDITION_SOURCE_TYPE_NONE)
        return sConditionMgr::Instance()->GetLootConditions(
            type, item->entry, item->itemid);
    return nullptr;
}

bool meets_conditions(Player* player, LootItem* item)
{
    auto conds = get_conditions_for_item(item);
    if (conds == nullptr)
    {
        item->has_conditions = false;
        return true;
    }
    return sConditionMgr::Instance()->IsObjectMeetToConditions(player, conds);
}
}

class LootTemplate::LootGroup // A set of loot definitions for items (refs are
                              // not allowed)
{
public:
    void AddEntry(
        LootStoreItem& item); // Adds an entry to the group (at loading stage)
    bool
    HasQuestDrop() const; // True if group includes at least 1 quest drop entry
    bool HasQuestDropForPlayer(Player const* player) const;
    // The same for active quests of the player
    void Process(Loot& loot) const; // Rolls an item from the group (if any) and
                                    // adds the item to the loot
    float RawTotalChance()
        const; // Overall chance for the group (without equal chanced items)
    float TotalChance() const; // Overall chance for the group

    void Verify(LootStore const& lootstore, uint32 id, uint32 group_id) const;
    void CollectLootIds(LootIdSet& set) const;
    void CheckLootRefs(LootIdSet* ref_set) const;

private:
    LootStoreItemList ExplicitlyChanced; // Entries with chances defined in DB
    LootStoreItemList
        EqualChanced; // Zero chances - every entry takes the same chance

    LootStoreItem const* Roll() const; // Rolls an item from the group, returns
                                       // NULL if all miss their chances
};

// Remove all data and free all memory
void LootStore::Clear()
{
    for (LootTemplateMap::const_iterator itr = m_LootTemplates.begin();
         itr != m_LootTemplates.end(); ++itr)
        delete itr->second;
    m_LootTemplates.clear();
}

// Checks validity of the loot store
// Actual checks are done within LootTemplate::Verify() which is called for
// every template
void LootStore::Verify() const
{
    for (const auto& elem : m_LootTemplates)
        elem.second->Verify(*this, elem.first);
}

// Loads a *_loot_template DB table into loot store
// All checks of the loaded template are called from here, no error reports at
// loot generation required
void LootStore::LoadLootTable()
{
    LootTemplateMap::const_iterator tab;
    uint32 count = 0;

    // Clearing store (for reloading case)
    Clear();

    logging.info("%s :", GetName());

    std::unique_ptr<QueryResult> result(WorldDatabase.PQuery(
        "SELECT entry, item, ChanceOrQuestChance, groupid, mincountOrRef, "
        "maxcount, condition_id FROM %s",
        GetName()));

    if (result)
    {
        BarGoLink bar(result->GetRowCount());

        do
        {
            Field* fields = result->Fetch();
            bar.step();

            uint32 entry = fields[0].GetUInt32();
            uint32 item = fields[1].GetUInt32();
            float chanceOrQuestChance = fields[2].GetFloat();
            uint8 group = fields[3].GetUInt8();
            int32 mincountOrRef = fields[4].GetInt32();
            uint32 maxcount = fields[5].GetUInt32();
            uint16 conditionId = fields[6].GetUInt16();

            if (maxcount > std::numeric_limits<uint8>::max())
            {
                logging.error(
                    "Table '%s' entry %d item %d: maxcount value (%u) to "
                    "large. must be less %u - skipped",
                    GetName(), entry, item, maxcount,
                    std::numeric_limits<uint8>::max());
                continue; // error already printed to log/console.
            }

            if (mincountOrRef < 0 && conditionId)
            {
                logging.error(
                    "Table '%s' entry %u mincountOrRef %i < 0 and not allowed "
                    "has condition, skipped",
                    GetName(), entry, mincountOrRef);
                continue;
            }

            if (conditionId)
            {
                const PlayerCondition* condition =
                    sConditionStorage.LookupEntry<PlayerCondition>(conditionId);
                if (!condition)
                {
                    logging.error(
                        "Table `%s` for entry %u, item %u has condition_id %u "
                        "that does not exist in `conditions`, ignoring",
                        GetName(), entry, item, conditionId);
                    conditionId = 0;
                }
            }

            LootStoreItem storeitem =
                LootStoreItem(entry, item, chanceOrQuestChance, group,
                    conditionId, mincountOrRef, maxcount, m_tableType);

            if (!storeitem.IsValid(*this, entry)) // Validity checks
                continue;

            // Looking for the template of the entry
            // often entries are put together
            if (m_LootTemplates.empty() || tab->first != entry)
            {
                // Searching the template (in case template Id changed)
                tab = m_LootTemplates.find(entry);
                if (tab == m_LootTemplates.end())
                {
                    std::pair<LootTemplateMap::iterator, bool> pr =
                        m_LootTemplates.insert(LootTemplateMap::value_type(
                            entry, new LootTemplate));
                    tab = pr.first;
                }
            }
            // else is empty - template Id and iter are the same
            // finally iter refers to already existing or just created <entry,
            // LootTemplate>

            // Adds current row to the template
            tab->second->AddEntry(storeitem);
            ++count;

        } while (result->NextRow());

        Verify(); // Checks validity of the loot store
    }
    logging.info("Loaded %u loot definitions (%lu templates)", count,
        (unsigned long)m_LootTemplates.size());
}

bool LootStore::HaveQuestLootFor(uint32 loot_id) const
{
    auto itr = m_LootTemplates.find(loot_id);
    if (itr == m_LootTemplates.end())
        return false;

    // scan loot for quest items
    return itr->second->HasQuestDrop(m_LootTemplates);
}

bool LootStore::HaveQuestLootForPlayer(uint32 loot_id, Player* player) const
{
    auto tab = m_LootTemplates.find(loot_id);
    if (tab != m_LootTemplates.end())
        if (tab->second->HasQuestDropForPlayer(m_LootTemplates, player))
            return true;

    return false;
}

LootTemplate const* LootStore::GetLootFor(uint32 loot_id) const
{
    auto tab = m_LootTemplates.find(loot_id);

    if (tab == m_LootTemplates.end())
        return nullptr;

    return tab->second;
}

void LootStore::LoadAndCollectLootIds(LootIdSet& ids_set)
{
    LoadLootTable();

    for (LootTemplateMap::const_iterator tab = m_LootTemplates.begin();
         tab != m_LootTemplates.end(); ++tab)
        ids_set.insert(tab->first);
}

void LootStore::CheckLootRefs(LootIdSet* ref_set) const
{
    for (const auto& elem : m_LootTemplates)
        elem.second->CheckLootRefs(ref_set);
}

void LootStore::ReportUnusedIds(LootIdSet const& ids_set) const
{
    // all still listed ids isn't referenced
    for (const auto& elem : ids_set)
        logging.error(
            "Table '%s' entry %d isn't %s and not referenced from loot, and "
            "then useless.",
            GetName(), elem, GetEntryName());
}

void LootStore::ReportNotExistedId(uint32 id) const
{
    logging.error(
        "Table '%s' entry %d (%s) not exist but used as loot id in DB.",
        GetName(), id, GetEntryName());
}

//
// --------- LootStoreItem ---------
//

// Checks if the entry (quest, non-quest, reference) takes it's chance (at loot
// generation)
// RATE_DROP_ITEMS is no longer used for all types of entries
bool LootStoreItem::Roll(bool /*rate*/) const
{
    if (chance >= 100.0f)
        return true;

    if (mincountOrRef < 0) // reference case
        return roll_chance_f(chance);

    if (!ObjectMgr::GetItemPrototype(itemid))
        return false;

    return roll_chance_f(chance);
}

// Checks correctness of values
bool LootStoreItem::IsValid(LootStore const& store, uint32 entry) const
{
    if (group >= 1 << 7) // it stored in 7 bit field
    {
        logging.error(
            "Table '%s' entry %d item %d: group (%u) must be less %u - skipped",
            store.GetName(), entry, itemid, group, 1 << 7);
        return false;
    }

    if (mincountOrRef == 0)
    {
        logging.error(
            "Table '%s' entry %d item %d: wrong mincountOrRef (%d) - skipped",
            store.GetName(), entry, itemid, mincountOrRef);
        return false;
    }

    if (mincountOrRef > 0) // item (quest or non-quest) entry, maybe grouped
    {
        ItemPrototype const* proto = ObjectMgr::GetItemPrototype(itemid);
        if (!proto)
        {
            logging.error(
                "Table '%s' entry %d item %d: item entry not listed in "
                "`item_template` - skipped",
                store.GetName(), entry, itemid);
            return false;
        }

        if (chance == 0 &&
            group == 0) // Zero chance is allowed for grouped entries only
        {
            logging.error(
                "Table '%s' entry %d item %d: equal-chanced grouped entry, but "
                "group not defined - skipped",
                store.GetName(), entry, itemid);
            return false;
        }

        if (chance != 0 && chance < 0.000001f) // loot with low chance
        {
            logging.error(
                "Table '%s' entry %d item %d: low chance (%f) - skipped",
                store.GetName(), entry, itemid, chance);
            return false;
        }

        if (maxcount < mincountOrRef) // wrong max count
        {
            logging.error(
                "Table '%s' entry %d item %d: max count (%u) less that min "
                "count (%i) - skipped",
                store.GetName(), entry, itemid, uint32(maxcount),
                mincountOrRef);
            return false;
        }
    }
    else // mincountOrRef < 0
    {
        if (needs_quest)
        {
            logging.error(
                "Table '%s' entry %d item %d: negative chance is specified for "
                "a reference, skipped",
                store.GetName(), entry, itemid);
            return false;
        }
        else if (chance == 0) // no chance for the reference
        {
            logging.error(
                "Table '%s' entry %d item %d: zero chance is specified for a "
                "reference, skipped",
                store.GetName(), entry, itemid);
            return false;
        }
    }
    return true; // Referenced template existence is checked at whole store
                 // level
}

//
// --------- LootItem ---------
//

// Constructor, copies most fields from LootStoreItem and generates random count
LootItem::LootItem(LootStoreItem const& li)
{
    entry = li.entry;
    itemid = li.itemid;
    conditionId = li.conditionId;

    ItemPrototype const* proto = ObjectMgr::GetItemPrototype(itemid);
    one_per_player = proto && (proto->Flags & ITEM_FLAG_PARTY_LOOT);

    needs_quest = li.needs_quest;

    count = urand(li.mincountOrRef,
        li.maxcount); // constructor called for mincountOrRef > 0 only
    randomSuffix = GenerateEnchSuffixFactor(itemid);
    randomPropertyId = Item::GenerateItemRandomPropertyId(itemid);
    is_looted = 0;
    is_blocked = 0;
    is_underthreshold = 0;
    is_counted = 0;
    item_quality = static_cast<ItemQualities>(proto->Quality);
    table = li.table;

    // Must be after entry, itemid and table are filled in
    has_conditions = get_conditions_for_item(this);
}

LootItem::LootItem(uint32 itemid_, uint32 count_, uint32 randomSuffix_,
    int32 randomPropertyId_, uint32 entry_, LootTable type)
{
    entry = entry_;
    itemid = itemid_;
    conditionId = 0;

    ItemPrototype const* proto = ObjectMgr::GetItemPrototype(itemid);
    one_per_player = proto && (proto->Flags & ITEM_FLAG_PARTY_LOOT);

    needs_quest = false;

    count = count_;
    randomSuffix =
        randomSuffix_ ? randomSuffix_ : GenerateEnchSuffixFactor(itemid);
    randomPropertyId = randomPropertyId_ ?
                           randomPropertyId_ :
                           Item::GenerateItemRandomPropertyId(itemid);
    is_looted = 0;
    is_blocked = 0;
    is_underthreshold = 0;
    is_counted = 0;
    item_quality = static_cast<ItemQualities>(proto->Quality);
    table = type;

    // Must be after entry, itemid and table are filled in
    if (entry == 0)
        has_conditions = false;
    else
        has_conditions = get_conditions_for_item(this);
}

//
// --------- Loot ---------
//

// Inserts the item into the loot
void Loot::add_item(LootStoreItem const& item)
{
    if (item.needs_quest) // Quest drop
    {
        if (quest_items_.size() < MAX_NR_QUEST_ITEMS)
            quest_items_.push_back(LootItem(item));
    }
    else if (items_.size() < MAX_NR_LOOT_ITEMS) // Non-quest drop
    {
        items_.push_back(LootItem(item));

        if (!item.conditionId && !items_.back().has_conditions)
        {
            ItemPrototype const* proto =
                ObjectMgr::GetItemPrototype(item.itemid);
            if (!proto || !(proto->Flags & ITEM_FLAG_PARTY_LOOT))
                ++unlooted_count_;
        }
    }
}

// Calls processor of corresponding LootTemplate (which handles everything
// including references)
bool Loot::fill_loot(uint32 loot_id, LootStore const& store,
    loot_recipient_mgr* recipients, bool /*personal*/, bool noEmptyError)
{
    // Must be provided
    if (!recipients)
        return false;

    const LootTemplate* tab = store.GetLootFor(loot_id);

    if (!tab)
    {
        if (!noEmptyError)
            logging.error(
                "Table '%s' loot id #%u used but it doesn't have records.",
                store.GetName(), loot_id);
        return false;
    }

    items_.reserve(MAX_NR_LOOT_ITEMS);
    quest_items_.reserve(MAX_NR_QUEST_ITEMS);

    tab->Process(*this, store, store.IsRatesAllowed()); // Processing is done
                                                        // there, callback via
                                                        // loot::add_item()

    uint32 ffa_items = 0;
    uint32 ffa_item_copies = 0;

    // Calculate and save access status for quest items & conditional items
    // (i.e. if you get a quest after the loot has happened, you do not get to
    // loot it)
    for (auto& elem : items_)
    {
        if (elem.is_looted)
            continue;
        if (elem.conditionId || elem.has_conditions)
        {
            size_t map_count = map_conditional_item(&elem, recipients);
            if (map_count && elem.one_per_player)
            {
                ++ffa_items;
                ffa_item_copies += map_count;
                one_per_player_map_.insert(
                    std::pair<LootItem*, std::set<ObjectGuid>>(
                        &elem, std::set<ObjectGuid>()));
            }
        }
        else if (elem.one_per_player)
        {
            ++ffa_items;
            ffa_item_copies += recipients->taps()->size();
            one_per_player_map_.insert(
                std::pair<LootItem*, std::set<ObjectGuid>>(
                    &elem, std::set<ObjectGuid>()));
        }
    }
    for (auto& elem : quest_items_)
    {
        if (elem.is_looted)
            continue;
        size_t map_count = map_quest_item(&elem, recipients);
        if (elem.one_per_player)
        {
            ++ffa_items;
            ffa_item_copies += map_count;
            one_per_player_map_.insert(
                std::pair<LootItem*, std::set<ObjectGuid>>(
                    &elem, std::set<ObjectGuid>()));
        }
    }

    unlooted_count_ =
        items_.size() + quest_items_.size() - ffa_items + ffa_item_copies;
    return true;
}

size_t Loot::map_conditional_item(
    LootItem* item, loot_recipient_mgr* recipients)
{
    // Add any player that fullfills conditions at the drop of loot to our map
    // of players who can loot this item
    size_t count = 0;
    for (const auto& elem : *recipients->taps())
    {
        if (Player* plr = sObjectAccessor::Instance()->FindPlayer(elem))
        {
            if (item->has_conditions && !meets_conditions(plr, item))
                continue;
            if (item->conditionId &&
                !sObjectMgr::Instance()->IsPlayerMeetToNEWCondition(
                    plr, item->conditionId))
                continue;
            ++count;
            conditional_item_map_[item].insert(plr->GetObjectGuid());
        }
    }
    return count;
}

size_t Loot::map_quest_item(LootItem* item, loot_recipient_mgr* recipients)
{
    // Add any player that has quest and fullfills conditions at the drop of
    // loot to our map of players who can loot this item
    size_t count = 0;
    for (const auto& elem : *recipients->taps())
    {
        if (Player* plr = sObjectAccessor::Instance()->FindPlayer(elem))
        {
            // Must have quest
            if (!plr->HasQuestForItem(item->itemid))
                continue;

            // Quest Items can have conditions too
            if (item->conditionId || item->has_conditions)
            {
                if (item->has_conditions && !meets_conditions(plr, item))
                    continue;
                if (item->conditionId &&
                    !sObjectMgr::Instance()->IsPlayerMeetToNEWCondition(
                        plr, item->conditionId))
                    continue;
                ++count;
                quest_item_map_[item].insert(plr->GetObjectGuid());
                conditional_item_map_[item].insert(plr->GetObjectGuid());
            }
            else
            {
                ++count;
                quest_item_map_[item].insert(plr->GetObjectGuid());
            }
        }
    }
    return count;
}

bool Loot::do_only_quest_items_remain() const
{
    if (gold_ > 0)
        return false;

    for (auto& item : items_)
        if (!item.is_looted)
            return false;

    return true;
}

bool Loot::can_loot_item(
    LootItem* item, ObjectGuid looter, bool is_anyones_loot) const
{
    // Need to check one_per_player before can_loot_item_now
    if (item->one_per_player)
    {
        auto find = one_per_player_map_.find(item);
        if (find == one_per_player_map_.end())
            return false;
        if (find->second.find(looter) != find->second.end()) // This map is
                                                             // reversed; anyone
                                                             // present cannot
                                                             // loot the item
            return false;
    }

    if (is_anyones_loot)
        return can_loot_item_now(item, looter);

    // Quests continue on to the condition check as well (some quests have
    // conditions)
    if (item->needs_quest)
    {
        auto find = quest_item_map_.find(item);
        if (find == quest_item_map_.end())
            return false;
        if (find->second.find(looter) == find->second.end())
            return false;
    }

    if (item->conditionId || item->has_conditions)
    {
        auto find = conditional_item_map_.find(item);
        if (find == conditional_item_map_.end())
            return false;
        if (find->second.find(looter) == find->second.end())
            return false;
    }

    // Even if we passed quest check and conditions when it dropped, we must
    // recheck them now that it's time to actually loot the item
    if (!can_loot_item_now(item, looter))
        return false;

    return true; // Quests get here
}

// loot that anyone can view needs to check availability when the looting
// happens
bool Loot::can_loot_item_now(LootItem* item, ObjectGuid looter) const
{
    Player* p = sObjectMgr::Instance()->GetPlayer(looter);
    if (!p)
        return false;

    if (item->needs_quest)
    {
        if (!p->HasQuestForItem(item->itemid))
            return false;
    }

    if (item->conditionId || item->has_conditions)
    {
        if (item->has_conditions && !meets_conditions(p, item))
            return false;
        if (item->conditionId &&
            !sObjectMgr::Instance()->IsPlayerMeetToNEWCondition(
                p, item->conditionId))
            return false;
    }

    return true;
}

//===================================================

void Loot::notify_item_removed(uint8 loot_slot)
{
    // notify all players that are looting this that the item was removed

    LootItem* item = get_slot_item(loot_slot);
    if (!item)
        return;

    for (auto itr = looting_players_.begin(); itr != looting_players_.end();)
    {
        auto current = itr++;
        if (Player* plr = ObjectAccessor::FindPlayer(*current))
        {
            // Only notify players that can see that item
            if (!can_see_item_slot(plr->GetObjectGuid(), loot_slot))
                continue;

            plr->SendNotifyLootItemRemoved(loot_slot);
        }
        else
            remove_looter(*current);
    }
}

void Loot::notify_money_removed()
{
    for (auto itr = looting_players_.begin(); itr != looting_players_.end();)
    {
        auto current = itr++;
        if (Player* plr = ObjectAccessor::FindPlayer(*current))
            plr->SendNotifyLootMoneyRemoved();
        else
            remove_looter(*current);
    }
}

void Loot::notify_quest_item_removed(uint8 loot_slot)
{
    LootItem* item = get_slot_item(loot_slot);
    if (!item || !item->needs_quest)
        return;

    for (auto itr = looting_players_.begin(); itr != looting_players_.end();)
    {
        auto current = itr++;
        if (Player* plr = ObjectAccessor::FindPlayer(*current))
        {
            // Only notify players that can loot that quest item
            if (!can_see_item_slot(plr->GetObjectGuid(), loot_slot))
                return;

            plr->SendNotifyLootItemRemoved(loot_slot);
        }
        else
            remove_looter(*current);
    }
}

void Loot::generate_money_loot(uint32 min, uint32 max)
{
    if (max > 0)
    {
        if (max <= min)
            gold_ = uint32(
                max *
                sWorld::Instance()->getConfig(CONFIG_FLOAT_RATE_DROP_MONEY));
        else if ((max - min) < 32700)
            gold_ = uint32(
                urand(min, max) *
                sWorld::Instance()->getConfig(CONFIG_FLOAT_RATE_DROP_MONEY));
        else
            gold_ = uint32(urand(min >> 8, max >> 8) *
                           sWorld::Instance()->getConfig(
                               CONFIG_FLOAT_RATE_DROP_MONEY))
                    << 8;
    }
}

//
// --------- LootTemplate::LootGroup ---------
//

// Adds an entry to the group (at loading stage)
void LootTemplate::LootGroup::AddEntry(LootStoreItem& item)
{
    if (item.chance != 0)
        ExplicitlyChanced.push_back(item);
    else
        EqualChanced.push_back(item);
}

// Rolls an item from the group, returns NULL if all miss their chances
LootStoreItem const* LootTemplate::LootGroup::Roll() const
{
    if (!ExplicitlyChanced
             .empty()) // First explicitly chanced entries are checked
    {
        float Roll = rand_chance_f();

        for (auto& elem : ExplicitlyChanced) // check each explicitly chanced
                                             // entry in the template and
        // modify its chance based on quality.
        {
            if (elem.chance >= 100.0f)
                return &elem;

            Roll -= elem.chance;
            if (Roll < 0)
                return &elem;
        }
    }
    if (!EqualChanced.empty()) // If nothing selected yet - an item is taken
                               // from equal-chanced part
        return &EqualChanced[irand(0, EqualChanced.size() - 1)];

    return nullptr; // Empty drop from the group
}

// True if group includes at least 1 quest drop entry
bool LootTemplate::LootGroup::HasQuestDrop() const
{
    for (const auto& elem : ExplicitlyChanced)
        if (elem.needs_quest)
            return true;
    for (const auto& elem : EqualChanced)
        if (elem.needs_quest)
            return true;
    return false;
}

// True if group includes at least 1 quest drop entry for active quests of the
// player
bool LootTemplate::LootGroup::HasQuestDropForPlayer(Player const* player) const
{
    for (const auto& elem : ExplicitlyChanced)
        if (player->HasQuestForItem(elem.itemid))
            return true;
    for (const auto& elem : EqualChanced)
        if (player->HasQuestForItem(elem.itemid))
            return true;
    return false;
}

// Rolls an item from the group (if any takes its chance) and adds the item to
// the loot
void LootTemplate::LootGroup::Process(Loot& loot) const
{
    LootStoreItem const* item = Roll();
    if (item != nullptr)
        loot.add_item(*item);
}

// Overall chance for the group without equal chanced items
float LootTemplate::LootGroup::RawTotalChance() const
{
    float result = 0;

    for (const auto& elem : ExplicitlyChanced)
        if (!elem.needs_quest)
            result += elem.chance;

    return result;
}

// Overall chance for the group
float LootTemplate::LootGroup::TotalChance() const
{
    float result = RawTotalChance();

    if (!EqualChanced.empty() && result < 100.0f)
        return 100.0f;

    return result;
}

void LootTemplate::LootGroup::Verify(
    LootStore const& lootstore, uint32 id, uint32 group_id) const
{
    float chance = RawTotalChance();
    if (chance > 101.0f) // TODO: replace with 100% when DBs will be ready
    {
        logging.error(
            "Table '%s' entry %u group %d has total chance > 100%% (%f)",
            lootstore.GetName(), id, group_id, chance);
    }

    if (chance >= 100.0f && !EqualChanced.empty())
    {
        logging.error(
            "Table '%s' entry %u group %d has items with chance=0%% but group "
            "total chance >= 100%% (%f)",
            lootstore.GetName(), id, group_id, chance);
    }
}

void LootTemplate::LootGroup::CheckLootRefs(LootIdSet* ref_set) const
{
    for (const auto& elem : ExplicitlyChanced)
    {
        if (elem.mincountOrRef < 0)
        {
            if (!LootTemplates_Reference.GetLootFor(-elem.mincountOrRef))
                LootTemplates_Reference.ReportNotExistedId(-elem.mincountOrRef);
            else if (ref_set)
                ref_set->erase(-elem.mincountOrRef);
        }
    }

    for (const auto& elem : EqualChanced)
    {
        if (elem.mincountOrRef < 0)
        {
            if (!LootTemplates_Reference.GetLootFor(-elem.mincountOrRef))
                LootTemplates_Reference.ReportNotExistedId(-elem.mincountOrRef);
            else if (ref_set)
                ref_set->erase(-elem.mincountOrRef);
        }
    }
}

//
// --------- LootTemplate ---------
//

// Adds an entry to the group (at loading stage)
void LootTemplate::AddEntry(LootStoreItem& item)
{
    if (item.group > 0 && item.mincountOrRef > 0) // Group
    {
        if (item.group >= Groups.size())
            Groups.resize(
                item.group); // Adds new group the the loot template if needed
        Groups[item.group - 1].AddEntry(item); // Adds new entry to the group
    }
    else // Non-grouped entries and references are stored together
        Entries.push_back(item);
}

// Rolls for every item in the template and adds the rolled items the the loot
void LootTemplate::Process(
    Loot& loot, LootStore const& store, bool rate, uint8 groupId) const
{
    if (groupId) // Group reference uses own processing of the group
    {
        if (groupId > Groups.size())
            return; // Error message already printed at loading stage

        Groups[groupId - 1].Process(loot);
        return;
    }

    // Rolling non-grouped items
    for (const auto& elem : Entries)
    {
        if (!elem.Roll(rate))
            continue; // Bad luck for the entry

        if (elem.mincountOrRef < 0) // References processing
        {
            LootTemplate const* Referenced =
                LootTemplates_Reference.GetLootFor(-elem.mincountOrRef);

            if (!Referenced)
                continue; // Error message already printed at loading stage

            for (uint32 loop = 0; loop < elem.maxcount;
                 ++loop) // Ref multiplicator
                Referenced->Process(loot, store, rate, elem.group);
        }
        else                     // Plain entries (not a reference, not grouped)
            loot.add_item(elem); // Chance is already checked, just add
    }

    // Now processing groups
    for (const auto& elem : Groups)
        elem.Process(loot);
}

// True if template includes at least 1 quest drop entry
bool LootTemplate::HasQuestDrop(
    LootTemplateMap const& store, uint8 groupId) const
{
    if (groupId) // Group reference
    {
        if (groupId > Groups.size())
            return false; // Error message [should be] already printed at
                          // loading stage
        return Groups[groupId - 1].HasQuestDrop();
    }

    for (const auto& elem : Entries)
    {
        if (elem.mincountOrRef < 0) // References
        {
            auto Referenced = store.find(-elem.mincountOrRef);
            if (Referenced == store.end())
                continue; // Error message [should be] already printed at
                          // loading stage
            if (Referenced->second->HasQuestDrop(store, elem.group))
                return true;
        }
        else if (elem.needs_quest)
            return true; // quest drop found
    }

    // Now processing groups
    for (const auto& elem : Groups)
        if (elem.HasQuestDrop())
            return true;

    return false;
}

// True if template includes at least 1 quest drop for an active quest of the
// player
bool LootTemplate::HasQuestDropForPlayer(
    LootTemplateMap const& store, Player const* player, uint8 groupId) const
{
    if (groupId) // Group reference
    {
        if (groupId > Groups.size())
            return false; // Error message already printed at loading stage
        return Groups[groupId - 1].HasQuestDropForPlayer(player);
    }

    // Checking non-grouped entries
    for (const auto& elem : Entries)
    {
        if (elem.mincountOrRef < 0) // References processing
        {
            auto Referenced = store.find(-elem.mincountOrRef);
            if (Referenced == store.end())
                continue; // Error message already printed at loading stage
            if (Referenced->second->HasQuestDropForPlayer(
                    store, player, elem.group))
                return true;
        }
        else if (player->HasQuestForItem(elem.itemid))
            return true; // active quest drop found
    }

    // Now checking groups
    for (const auto& elem : Groups)
        if (elem.HasQuestDropForPlayer(player))
            return true;

    return false;
}

// Checks integrity of the template
void LootTemplate::Verify(LootStore const& lootstore, uint32 id) const
{
    // Checking group chances
    for (uint32 i = 0; i < Groups.size(); ++i)
        Groups[i].Verify(lootstore, id, i + 1);

    // TODO: References validity checks
}

void LootTemplate::CheckLootRefs(LootIdSet* ref_set) const
{
    for (const auto& elem : Entries)
    {
        if (elem.mincountOrRef < 0)
        {
            if (!LootTemplates_Reference.GetLootFor(-elem.mincountOrRef))
                LootTemplates_Reference.ReportNotExistedId(-elem.mincountOrRef);
            else if (ref_set)
                ref_set->erase(-elem.mincountOrRef);
        }
    }

    for (const auto& elem : Groups)
        elem.CheckLootRefs(ref_set);
}

void LoadLootTemplates_Creature()
{
    LootIdSet ids_set, ids_setUsed;
    LootTemplates_Creature.LoadAndCollectLootIds(ids_set);

    // remove real entries and check existence loot
    for (uint32 i = 1; i < sCreatureStorage.MaxEntry; ++i)
    {
        if (CreatureInfo const* cInfo =
                sCreatureStorage.LookupEntry<CreatureInfo>(i))
        {
            if (uint32 lootid = cInfo->lootid)
            {
                if (ids_set.find(lootid) == ids_set.end())
                    LootTemplates_Creature.ReportNotExistedId(lootid);
                else
                    ids_setUsed.insert(lootid);
            }
        }
    }
    for (const auto& elem : ids_setUsed)
        ids_set.erase(elem);

    // for alterac valley we've defined Player-loot inside
    // creature_loot_template id=0
    // this hack is used, so that we won't need to create an extra table
    // player_loot_template for just one case
    ids_set.erase(0);

    // output error for any still listed (not referenced from appropriate table)
    // ids
    LootTemplates_Creature.ReportUnusedIds(ids_set);
}

void LoadLootTemplates_Disenchant()
{
    LootIdSet ids_set, ids_setUsed;
    LootTemplates_Disenchant.LoadAndCollectLootIds(ids_set);

    // remove real entries and check existence loot
    for (uint32 i = 1; i < sItemStorage.MaxEntry; ++i)
    {
        if (ItemPrototype const* proto =
                sItemStorage.LookupEntry<ItemPrototype>(i))
        {
            if (uint32 lootid = proto->DisenchantID)
            {
                if (ids_set.find(lootid) == ids_set.end())
                    LootTemplates_Disenchant.ReportNotExistedId(lootid);
                else
                    ids_setUsed.insert(lootid);
            }
        }
    }
    for (const auto& elem : ids_setUsed)
        ids_set.erase(elem);
    // output error for any still listed (not referenced from appropriate table)
    // ids
    LootTemplates_Disenchant.ReportUnusedIds(ids_set);
}

void LoadLootTemplates_Fishing()
{
    LootIdSet ids_set;
    LootTemplates_Fishing.LoadAndCollectLootIds(ids_set);

    // remove real entries and check existence loot
    for (uint32 i = 1; i < sAreaStore.GetNumRows(); ++i)
    {
        if (AreaTableEntry const* areaEntry = sAreaStore.LookupEntry(i))
            if (ids_set.find(areaEntry->ID) != ids_set.end())
                ids_set.erase(areaEntry->ID);
    }

    // by default (look config options) fishing at fail provide junk loot, entry
    // 0 use for store this loot
    ids_set.erase(0);

    // output error for any still listed (not referenced from appropriate table)
    // ids
    LootTemplates_Fishing.ReportUnusedIds(ids_set);
}

void LoadLootTemplates_Gameobject()
{
    LootIdSet ids_set, ids_setUsed;
    LootTemplates_Gameobject.LoadAndCollectLootIds(ids_set);

    // remove real entries and check existence loot
    for (uint32 i = 1; i < sGOStorage.MaxEntry; ++i)
    {
        if (GameObjectInfo const* gInfo =
                sGOStorage.LookupEntry<GameObjectInfo>(i))
        {
            if (uint32 lootid = gInfo->GetLootId())
            {
                if (ids_set.find(lootid) == ids_set.end())
                    LootTemplates_Gameobject.ReportNotExistedId(lootid);
                else
                    ids_setUsed.insert(lootid);
            }
        }
    }
    for (const auto& elem : ids_setUsed)
        ids_set.erase(elem);

    // output error for any still listed (not referenced from appropriate table)
    // ids
    LootTemplates_Gameobject.ReportUnusedIds(ids_set);
}

void LoadLootTemplates_Item()
{
    LootIdSet ids_set;
    LootTemplates_Item.LoadAndCollectLootIds(ids_set);

    // remove real entries and check existence loot
    for (uint32 i = 1; i < sItemStorage.MaxEntry; ++i)
    {
        if (ItemPrototype const* proto =
                sItemStorage.LookupEntry<ItemPrototype>(i))
        {
            if (!(proto->Flags & ITEM_FLAG_LOOTABLE))
                continue;

            if (ids_set.find(proto->ItemId) != ids_set.end() ||
                proto->MaxMoneyLoot > 0)
                ids_set.erase(proto->ItemId);
            // wdb have wrong data cases, so skip by default
            else
                LootTemplates_Item.ReportNotExistedId(proto->ItemId);
        }
    }

    // output error for any still listed (not referenced from appropriate table)
    // ids
    LootTemplates_Item.ReportUnusedIds(ids_set);
}

void LoadLootTemplates_Pickpocketing()
{
    LootIdSet ids_set, ids_setUsed;
    LootTemplates_Pickpocketing.LoadAndCollectLootIds(ids_set);

    // remove real entries and check existence loot
    for (uint32 i = 1; i < sCreatureStorage.MaxEntry; ++i)
    {
        if (CreatureInfo const* cInfo =
                sCreatureStorage.LookupEntry<CreatureInfo>(i))
        {
            if (uint32 lootid = cInfo->pickpocketLootId)
            {
                if (ids_set.find(lootid) == ids_set.end())
                    LootTemplates_Pickpocketing.ReportNotExistedId(lootid);
                else
                    ids_setUsed.insert(lootid);
            }
        }
    }
    for (const auto& elem : ids_setUsed)
        ids_set.erase(elem);

    // output error for any still listed (not referenced from appropriate table)
    // ids
    LootTemplates_Pickpocketing.ReportUnusedIds(ids_set);
}

void LoadLootTemplates_Prospecting()
{
    LootIdSet ids_set;
    LootTemplates_Prospecting.LoadAndCollectLootIds(ids_set);

    // remove real entries and check existence loot
    for (uint32 i = 1; i < sItemStorage.MaxEntry; ++i)
    {
        ItemPrototype const* proto = sItemStorage.LookupEntry<ItemPrototype>(i);
        if (!proto)
            continue;

        if (!(proto->Flags & ITEM_FLAG_PROSPECTABLE))
            continue;

        if (ids_set.find(proto->ItemId) != ids_set.end())
            ids_set.erase(proto->ItemId);
        // else -- exist some cases that possible can be prospected but not
        // expected have any result loot
        //    LootTemplates_Prospecting.ReportNotExistedId(proto->ItemId);
    }

    // output error for any still listed (not referenced from appropriate table)
    // ids
    LootTemplates_Prospecting.ReportUnusedIds(ids_set);
}

void LoadLootTemplates_Mail()
{
    LootIdSet ids_set;
    LootTemplates_Mail.LoadAndCollectLootIds(ids_set);

    // remove real entries and check existence loot
    for (uint32 i = 1; i < sMailTemplateStore.GetNumRows(); ++i)
        if (sMailTemplateStore.LookupEntry(i))
            if (ids_set.find(i) != ids_set.end())
                ids_set.erase(i);

    // output error for any still listed (not referenced from appropriate table)
    // ids
    LootTemplates_Mail.ReportUnusedIds(ids_set);
}

void LoadLootTemplates_Skinning()
{
    LootIdSet ids_set, ids_setUsed;
    LootTemplates_Skinning.LoadAndCollectLootIds(ids_set);

    // remove real entries and check existence loot
    for (uint32 i = 1; i < sCreatureStorage.MaxEntry; ++i)
    {
        if (CreatureInfo const* cInfo =
                sCreatureStorage.LookupEntry<CreatureInfo>(i))
        {
            if (uint32 lootid = cInfo->SkinLootId)
            {
                if (ids_set.find(lootid) == ids_set.end())
                    LootTemplates_Skinning.ReportNotExistedId(lootid);
                else
                    ids_setUsed.insert(lootid);
            }
        }
    }
    for (const auto& elem : ids_setUsed)
        ids_set.erase(elem);

    // output error for any still listed (not referenced from appropriate table)
    // ids
    LootTemplates_Skinning.ReportUnusedIds(ids_set);
}

void LoadLootTemplates_Reference()
{
    LootIdSet ids_set;
    LootTemplates_Reference.LoadAndCollectLootIds(ids_set);

    // check references and remove used
    LootTemplates_Creature.CheckLootRefs(&ids_set);
    LootTemplates_Fishing.CheckLootRefs(&ids_set);
    LootTemplates_Gameobject.CheckLootRefs(&ids_set);
    LootTemplates_Item.CheckLootRefs(&ids_set);
    LootTemplates_Pickpocketing.CheckLootRefs(&ids_set);
    LootTemplates_Skinning.CheckLootRefs(&ids_set);
    LootTemplates_Disenchant.CheckLootRefs(&ids_set);
    LootTemplates_Prospecting.CheckLootRefs(&ids_set);
    LootTemplates_Mail.CheckLootRefs(&ids_set);
    LootTemplates_Reference.CheckLootRefs(&ids_set);

    // output error for any still listed ids (not referenced from any loot
    // table)
    LootTemplates_Reference.ReportUnusedIds(ids_set);
}
