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

#ifndef MANGOSSERVER_ITEM_H
#define MANGOSSERVER_ITEM_H

#include "Common.h"
#include "ItemPrototype.h"
#include "Object.h"
#include "inventory/slot.h"

class Bag;
class Field;
class QueryResult;
class Spell;
struct SpellEntry;
class Unit;

struct ItemSetEffect
{
    uint32 setid;
    uint32 item_count;
    SpellEntry const* spells[8];
};

enum InventoryResult
{
    EQUIP_ERR_OK = 0, // ""
    EQUIP_ERR_CANT_EQUIP_LEVEL_I =
        1, // "You must reach level %d to use that item."
    EQUIP_ERR_CANT_EQUIP_SKILL =
        2, // "You aren't skilled enough to use that item."
    EQUIP_ERR_ITEM_DOESNT_GO_TO_SLOT =
        3,                  // "That item doesn't go in that slot."
    EQUIP_ERR_BAG_FULL = 4, // "That bag is full."
    EQUIP_ERR_NONEMPTY_BAG_OVER_OTHER_BAG =
        5, // "Can't put non-empty bags in other bags."
    EQUIP_ERR_CANT_TRADE_EQUIP_BAGS = 6, // "You can't trade equipped bags."
    EQUIP_ERR_ONLY_AMMO_CAN_GO_HERE = 7, // "Only ammo can go there."
    EQUIP_ERR_NO_REQUIRED_PROFICIENCY =
        8, // "You do not have the required proficiency for that item."
    EQUIP_ERR_NO_EQUIPMENT_SLOT_AVAILABLE =
        9, // "No equipment slot is available for that item."
    EQUIP_ERR_YOU_CAN_NEVER_USE_THAT_ITEM =
        10, // "You can never use that item."
    EQUIP_ERR_YOU_CAN_NEVER_USE_THAT_ITEM2 =
        11, // "You can never use that item."
    EQUIP_ERR_NO_EQUIPMENT_SLOT_AVAILABLE2 =
        12, // "No equipment slot is available for that item."
    EQUIP_ERR_CANT_EQUIP_WITH_TWOHANDED =
        13, // "Cannot equip that with a two-handd weapon."
    EQUIP_ERR_CANT_DUAL_WIELD = 14, // "You cannot dual-wield."
    EQUIP_ERR_ITEM_DOESNT_GO_INTO_BAG =
        15, // "That item doesn't go in that container."
    EQUIP_ERR_ITEM_DOESNT_GO_INTO_BAG2 =
        16, // "That item doesn't go in that container."
    EQUIP_ERR_CANT_CARRY_MORE_OF_THIS =
        17, // "You can't carry any more of those items."
    EQUIP_ERR_NO_EQUIPMENT_SLOT_AVAILABLE3 =
        18, // "No equipment slot is avaliable for that item."
    EQUIP_ERR_ITEM_CANT_STACK = 19,       // "This item cann't stack."
    EQUIP_ERR_ITEM_CANT_BE_EQUIPPED = 20, // "This item cannot be equipped."
    EQUIP_ERR_ITEMS_CANT_BE_SWAPPED = 21, // "These item's can't be swapped."
    EQUIP_ERR_SLOT_IS_EMPTY = 22,         // "That slot is empty."
    EQUIP_ERR_ITEM_NOT_FOUND = 23,        // "The item was not found."
    EQUIP_ERR_CANT_DROP_SOULBOUND = 24,   // "You can't drop a soulbound item."
    EQUIP_ERR_OUT_OF_RANGE = 25,          // "Out of range."
    EQUIP_ERR_TRIED_TO_SPLIT_MORE_THAN_COUNT =
        26, // "Tried to split more than number in stack."
    EQUIP_ERR_COULDNT_SPLIT_ITEMS = 27, // "Couldn't split those items."
    EQUIP_ERR_MISSING_REAGENT = 28,     // "Missing reagent"
    EQUIP_ERR_NOT_ENOUGH_MONEY = 29,    // "You don't have enough money."
    EQUIP_ERR_NOT_A_BAG = 30,           // "Not a bag."
    EQUIP_ERR_CAN_ONLY_DO_WITH_EMPTY_BAGS =
        31, // "You can only do that with empty bags."
    EQUIP_ERR_DONT_OWN_THAT_ITEM = 32,     // "You don't own that item."
    EQUIP_ERR_CAN_EQUIP_ONLY1_QUIVER = 33, // "You can only equip one quiver."
    EQUIP_ERR_MUST_PURCHASE_THAT_BAG_SLOT =
        34, // "You must purchase that bag slot first."
    EQUIP_ERR_TOO_FAR_AWAY_FROM_BANK =
        35,                           // "You are too far away from a bank."
    EQUIP_ERR_ITEM_LOCKED = 36,       // "Item is locked."
    EQUIP_ERR_YOU_ARE_STUNNED = 37,   // "You are stunned"
    EQUIP_ERR_YOU_ARE_DEAD = 38,      // "You can't do that when you're dead."
    EQUIP_ERR_CANT_DO_RIGHT_NOW = 39, // "You can't do that right now."
    EQUIP_ERR_INT_BAG_ERROR = 40,     // "Internal Bag Error"
    EQUIP_ERR_CAN_EQUIP_ONLY1_BOLT = 41, // "You can only equip one quiver."
    EQUIP_ERR_CAN_EQUIP_ONLY1_AMMOPOUCH =
        42, // "You can only equip one ammo pouch."
    EQUIP_ERR_STACKABLE_CANT_BE_WRAPPED =
        43, // "Stackable item's can't be wrapped."
    EQUIP_ERR_EQUIPPED_CANT_BE_WRAPPED =
        44, // "Equipped items can't be wrapped."
    EQUIP_ERR_WRAPPED_CANT_BE_WRAPPED = 45, // "Wrapped items can't be wrapped."
    EQUIP_ERR_BOUND_CANT_BE_WRAPPED = 46,   // "Bound items can't be wrapped."
    EQUIP_ERR_UNIQUE_CANT_BE_WRAPPED = 47,  // "Unique items can't be wrapped."
    EQUIP_ERR_BAGS_CANT_BE_WRAPPED = 48,    // "Bags can't be wrapped"
    EQUIP_ERR_ALREADY_LOOTED = 49,          // "Already looted"
    EQUIP_ERR_INVENTORY_FULL = 50,          // "Inventory is full."
    EQUIP_ERR_BANK_FULL = 51,               // "Your bank is full"
    EQUIP_ERR_ITEM_IS_CURRENTLY_SOLD_OUT =
        52,                            // "That item is currently sold out."
    EQUIP_ERR_BAG_FULL3 = 53,          // "That bag is full."
    EQUIP_ERR_ITEM_NOT_FOUND2 = 54,    // "That item was not found."
    EQUIP_ERR_ITEM_CANT_STACK2 = 55,   // "This item cannot stack."
    EQUIP_ERR_BAG_FULL4 = 56,          // "That bag is full."
    EQUIP_ERR_ITEM_SOLD_OUT = 57,      // "That item is currently sold out."
    EQUIP_ERR_OBJECT_IS_BUSY = 58,     // "That object is busy."
    EQUIP_ERR_NONE = 59,               // ""
    EQUIP_ERR_NOT_IN_COMBAT = 60,      // "You can't do that while in combat"
    EQUIP_ERR_NOT_WHILE_DISARMED = 61, // "You can't do that while disarmed"
    EQUIP_ERR_BAG_FULL6 = 62,          // "That bag is full."
    EQUIP_ERR_CANT_EQUIP_RANK =
        63, // "You don't have the required rank for that item"
    EQUIP_ERR_CANT_EQUIP_REPUTATION =
        64, // "You don't have the required reputation for that item"
    EQUIP_ERR_TOO_MANY_SPECIAL_BAGS =
        65, // "You cannot equip another bag of that type"
    EQUIP_ERR_LOOT_CANT_LOOT_THAT_NOW = 66, // "You can't loot that item now."
    EQUIP_ERR_ITEM_UNIQUE_EQUIPABLE =
        67, // "You cannot equip more than one of those."
    EQUIP_ERR_VENDOR_MISSING_TURNINS =
        68, // "You do not have the required items for that purchase"
    EQUIP_ERR_NOT_ENOUGH_HONOR_POINTS =
        69, // "You don't have enough honor points"
    EQUIP_ERR_NOT_ENOUGH_ARENA_POINTS =
        70, // "You don't have enough arena points"
    EQUIP_ERR_ITEM_MAX_COUNT_SOCKETED =
        71, // "You have the maximum number of those gems in your inventory or
            // socketed into items." <-- This Error is unused, only four gems
            // have a MaxCount, and they're all named "zzOLD", in other words
            // unused
    EQUIP_ERR_MAIL_BOUND_ITEM = 72, // "You can't mail soulbound items."
    EQUIP_ERR_NO_SPLIT_WHILE_PROSPECTING = 73, // "Internal Bag Error"
    EQUIP_ERR_BAG_FULL7 = 74,                  // "That bag is full."
    EQUIP_ERR_ITEM_MAX_COUNT_EQUIPPED_SOCKETED =
        75, // "You have the maximum number of those gems socketed into equipped
            // items."
    EQUIP_ERR_ITEM_UNIQUE_EQUIPPABLE_SOCKETED = 76, // "You cannot socket more
                                                    // than one of those gems
                                                    // into a single item."
    EQUIP_ERR_TOO_MUCH_GOLD = 77,                   // "At gold limit"
    EQUIP_ERR_NOT_DURING_ARENA_MATCH =
        78, // "You can't do that while in an arena match."
    EQUIP_ERR_CANNOT_TRADE_THAT = 79, // "You can't trade a soulbound item."
    EQUIP_ERR_PERSONAL_ARENA_RATING_TOO_LOW =
        80, // "Your personal or team arena rating is too low for that item"
            // probably exist more
};

enum BuyResult
{
    BUY_ERR_CANT_FIND_ITEM = 0,
    BUY_ERR_ITEM_ALREADY_SOLD = 1,
    BUY_ERR_NOT_ENOUGHT_MONEY = 2,
    BUY_ERR_SELLER_DONT_LIKE_YOU = 4,
    BUY_ERR_DISTANCE_TOO_FAR = 5,
    BUY_ERR_ITEM_SOLD_OUT = 7,
    BUY_ERR_CANT_CARRY_MORE = 8,
    BUY_ERR_RANK_REQUIRE = 11,
    BUY_ERR_REPUTATION_REQUIRE = 12
};

enum SellResult
{
    SELL_ERR_CANT_FIND_ITEM = 1,
    SELL_ERR_CANT_SELL_ITEM = 2,         // merchant doesn't like that item
    SELL_ERR_CANT_FIND_VENDOR = 3,       // merchant doesn't like you
    SELL_ERR_YOU_DONT_OWN_THAT_ITEM = 4, // you don't own that item
    SELL_ERR_UNK = 5,                    // nothing appears...
    SELL_ERR_ONLY_EMPTY_BAG = 6          // can only do with empty bags
};

// -1 from client enchantment slot number
enum EnchantmentSlot
{
    PERM_ENCHANTMENT_SLOT = 0,
    TEMP_ENCHANTMENT_SLOT = 1,
    SOCK_ENCHANTMENT_SLOT = 2,
    SOCK_ENCHANTMENT_SLOT_2 = 3,
    SOCK_ENCHANTMENT_SLOT_3 = 4,
    BONUS_ENCHANTMENT_SLOT = 5,
    MAX_INSPECTED_ENCHANTMENT_SLOT = 6,

    PROP_ENCHANTMENT_SLOT_0 = 6,  // used with RandomSuffix
    PROP_ENCHANTMENT_SLOT_1 = 7,  // used with RandomSuffix
    PROP_ENCHANTMENT_SLOT_2 = 8,  // used with RandomSuffix and RandomProperty
    PROP_ENCHANTMENT_SLOT_3 = 9,  // used with RandomProperty
    PROP_ENCHANTMENT_SLOT_4 = 10, // used with RandomProperty
    MAX_ENCHANTMENT_SLOT = 11
};

#define MAX_VISIBLE_ITEM_OFFSET \
    16 // 16 fields per visible item (creator(2) + enchantments(12) +
       // properties(1) + pad(1))

#define MAX_GEM_SOCKETS \
    MAX_ITEM_PROTO_SOCKETS // (BONUS_ENCHANTMENT_SLOT-SOCK_ENCHANTMENT_SLOT) and
                           // item proto size, equal value expected

enum EnchantmentOffset
{
    ENCHANTMENT_ID_OFFSET = 0,
    ENCHANTMENT_DURATION_OFFSET = 1,
    ENCHANTMENT_CHARGES_OFFSET = 2
};

#define MAX_ENCHANTMENT_OFFSET 3

enum EnchantmentSlotMask
{
    ENCHANTMENT_CAN_SOULBOUND = 0x01,
    ENCHANTMENT_UNK1 = 0x02,
    ENCHANTMENT_UNK2 = 0x04,
    ENCHANTMENT_UNK3 = 0x08
};

enum ItemLootUpdateState
{
    ITEM_LOOT_NONE = 0, // loot not generated
    ITEM_LOOT_TEMPORARY =
        1, // generated loot is temporary (will deleted at loot window close)
    ITEM_LOOT_UNCHANGED = 2,
    ITEM_LOOT_CHANGED = 3,
    ITEM_LOOT_NEW = 4,
    ITEM_LOOT_REMOVED = 5
};

// masks for ITEM_FIELD_FLAGS field
enum ItemDynFlags
{
    ITEM_DYNFLAG_BINDED = 0x00000001, // set in game at binding
    ITEM_DYNFLAG_UNK1 = 0x00000002,
    ITEM_DYNFLAG_UNLOCKED = 0x00000004, // have meaning only for item with
                                        // proto->LockId, if not set show as
                                        // "Locked, req. lockpicking N"
    ITEM_DYNFLAG_WRAPPED =
        0x00000008, // mark item as wrapped into wrapper container
    ITEM_DYNFLAG_UNK4 = 0x00000010, // can't repeat old note: appears red icon
                                    // (like when item durability==0)
    ITEM_DYNFLAG_UNK5 = 0x00000020,
    ITEM_DYNFLAG_UNK6 = 0x00000040, // ? old note: usable
    ITEM_DYNFLAG_UNK7 = 0x00000080,
    ITEM_DYNFLAG_UNK8 = 0x00000100,
    ITEM_DYNFLAG_READABLE = 0x00000200, // can be open for read, it or item
                                        // proto pagetText make show "Right
                                        // click to read"
    ITEM_DYNFLAG_UNK10 = 0x00000400,
    ITEM_DYNFLAG_UNK11 = 0x00000800,
    ITEM_DYNFLAG_UNK12 = 0x00001000,
    ITEM_DYNFLAG_UNK13 = 0x00002000,
    ITEM_DYNFLAG_UNK14 = 0x00004000,
    ITEM_DYNFLAG_UNK15 = 0x00008000,
    ITEM_DYNFLAG_UNK16 = 0x00010000,
    ITEM_DYNFLAG_UNK17 = 0x00020000,
    ITEM_DYNFLAG_UNK18 = 0x00040000,
    ITEM_DYNFLAG_UNK19 = 0x00080000,
    ITEM_DYNFLAG_UNK20 = 0x00100000,
    ITEM_DYNFLAG_UNK21 = 0x00200000,
    ITEM_DYNFLAG_UNK22 = 0x00400000,
    ITEM_DYNFLAG_UNK23 = 0x00800000,
    ITEM_DYNFLAG_UNK24 = 0x01000000,
    ITEM_DYNFLAG_UNK25 = 0x02000000,
    ITEM_DYNFLAG_UNK26 = 0x04000000,
    ITEM_DYNFLAG_UNK27 = 0x08000000,
};

enum ItemRequiredTargetType
{
    ITEM_TARGET_TYPE_CREATURE = 1,
    ITEM_TARGET_TYPE_DEAD = 2
};

#define MAX_ITEM_REQ_TARGET_TYPE 2

struct ItemRequiredTarget
{
    ItemRequiredTarget(ItemRequiredTargetType uiType, uint32 uiTargetEntry)
      : m_uiType(uiType), m_uiTargetEntry(uiTargetEntry)
    {
    }
    ItemRequiredTargetType m_uiType;
    uint32 m_uiTargetEntry;

    // helpers
    bool IsFitToRequirements(Unit* pUnitTarget) const;
};

bool ItemCanGoIntoBag(
    ItemPrototype const* proto, ItemPrototype const* pBagProto);

class MANGOS_DLL_SPEC Item : public Object
{
public:
    static Item* CreateItem(uint32 item, uint32 count,
        Player const* player = nullptr, uint32 randomPropertyId = 0);
    Item* CloneItem(uint32 count, Player const* player = nullptr) const;

    // For bags we must specify a prototype when they're created
    Item(const ItemPrototype* prototype = nullptr);
    ~Item();

    virtual bool Create(uint32 guidlow, uint32 itemid, Player const* owner);

    ItemPrototype const* GetProto() const;

    ObjectGuid const& GetOwnerGuid() const
    {
        return GetGuidValue(ITEM_FIELD_OWNER);
    }
    void SetOwnerGuid(ObjectGuid guid) { SetGuidValue(ITEM_FIELD_OWNER, guid); }
    Player* GetOwner() const;

    void SetBinding(bool val)
    {
        ApplyModFlag(ITEM_FIELD_FLAGS, ITEM_DYNFLAG_BINDED, val);
    }
    bool IsSoulBound() const
    {
        return HasFlag(ITEM_FIELD_FLAGS, ITEM_DYNFLAG_BINDED);
    }

    // If this function returns false call db_remove() && delete the resources
    // you allocated.
    bool LoadFromDB(
        uint32 guidLow, Field* fields, ObjectGuid ownerGuid = ObjectGuid());
    void LoadLootFromDB(Field* fields);

    bool IsBag() const { return GetProto()->InventoryType == INVTYPE_BAG; }
    bool IsBroken() const
    {
        return GetUInt32Value(ITEM_FIELD_MAXDURABILITY) > 0 &&
               GetUInt32Value(ITEM_FIELD_DURABILITY) == 0;
    }

    bool IsFitToSpellRequirements(SpellEntry const* spellInfo) const;
    bool IsTargetValidForItemUse(Unit* pUnitTarget);
    bool IsLimitedToAnotherMapOrZone(uint32 cur_mapId, uint32 cur_zoneId) const;
    void update_gem_bonus();

    uint32 GetCount() const { return GetUInt32Value(ITEM_FIELD_STACK_COUNT); }
    void SetCount(uint32 value)
    {
        SetUInt32Value(ITEM_FIELD_STACK_COUNT, value);
    }
    uint32 GetMaxStackCount() const { return GetProto()->GetMaxStackSize(); }

    bool IsEquipped() const;

    uint32 GetSkill();
    uint32 GetSpell();

    // RandomPropertyId (signed but stored as unsigned)
    int32 GetItemRandomPropertyId() const
    {
        return GetInt32Value(ITEM_FIELD_RANDOM_PROPERTIES_ID);
    }
    uint32 GetItemSuffixFactor() const
    {
        return GetUInt32Value(ITEM_FIELD_PROPERTY_SEED);
    }
    void SetItemRandomProperties(int32 randomPropId);
    bool UpdateItemSuffixFactor();
    static int32 GenerateItemRandomPropertyId(uint32 item_id);
    void SetEnchantment(
        EnchantmentSlot slot, uint32 id, uint32 duration, uint32 charges);
    void SetEnchantmentDuration(EnchantmentSlot slot, uint32 duration);
    void SetEnchantmentCharges(EnchantmentSlot slot, uint32 charges);
    void ClearEnchantment(EnchantmentSlot slot);
    uint32 GetEnchantmentId(EnchantmentSlot slot) const
    {
        return GetUInt32Value(ITEM_FIELD_ENCHANTMENT_1_1 +
                              slot * MAX_ENCHANTMENT_OFFSET +
                              ENCHANTMENT_ID_OFFSET);
    }
    uint32 GetEnchantmentDuration(EnchantmentSlot slot);
    uint32 GetEnchantmentCharges(EnchantmentSlot slot) const
    {
        return GetUInt32Value(ITEM_FIELD_ENCHANTMENT_1_1 +
                              slot * MAX_ENCHANTMENT_OFFSET +
                              ENCHANTMENT_CHARGES_OFFSET);
    }

    void SendDuration(Player* owner);
    void UpdateDuration(Player* owner, uint32 diff);
    void SendEnchDurations(Player* owner);
    void UpdateEnchDurations(Player* owner, uint32 diff);

    // spell charges (signed but stored as unsigned)
    int32 GetSpellCharges(uint8 index /*0..5*/ = 0) const
    {
        return GetInt32Value(ITEM_FIELD_SPELL_CHARGES + index);
    }
    void SetSpellCharges(uint8 index /*0..5*/, int32 value)
    {
        SetInt32Value(ITEM_FIELD_SPELL_CHARGES + index, value);
    }
    // Returns true if item was depleted and can be destroyed (some items are
    // not destroyed when spell charges reach 0)
    bool DropSpellCharge();

    void SetLootState(ItemLootUpdateState state);
    bool HasGeneratedLoot() const
    {
        return m_lootState != ITEM_LOOT_NONE &&
               m_lootState != ITEM_LOOT_REMOVED;
    }
    bool HasTemporaryLoot() const { return m_lootState == ITEM_LOOT_TEMPORARY; }

    bool HasSavedLoot() const
    {
        return m_lootState != ITEM_LOOT_NONE && m_lootState != ITEM_LOOT_NEW &&
               m_lootState != ITEM_LOOT_TEMPORARY;
    }

    bool HasQuest(uint32 quest_id) const override
    {
        return GetProto()->StartQuest == quest_id;
    }
    bool HasInvolvedQuest(uint32 /*quest_id*/) const override { return false; }
    bool IsPotion() const { return GetProto()->IsPotion(); }
    bool IsConjuredConsumable() const
    {
        return GetProto()->IsConjuredConsumable();
    }

    void AddToClientUpdateList() override;
    void RemoveFromClientUpdateList() override;
    void BuildUpdateData(UpdateDataMapType& update_players) override;

    void OnLootOpen(LootType lootType, Player* looter) override;

    void mark_for_save() { should_save_ = true; }
    bool should_save() const { return should_save_; }

    inventory::slot slot() const { return slot_; }
    void slot(inventory::slot s) { slot_ = s; }

    bool can_put_in_trade() const;
    bool in_trade() const { return in_trade_; }
    void set_in_trade(bool in) { in_trade_ = in; }

    void db_save(Player* owner = nullptr)
    {
        in_db_ ? db_update(owner) : db_insert(owner);
    }
    void db_delete();
    void db_save_loot(Player* owner = nullptr);

    void add_referencing_spell(Spell* s) { referencing_spells_.push_back(s); }
    void remove_referencing_spell(Spell* s)
    {
        auto& v = referencing_spells_;
        v.erase(std::remove(std::begin(v), std::end(v), s), std::end(v));
    }
    bool already_referenced(Spell* ignore) const
    {
        auto& v = referencing_spells_;
        return v.size() > 1 ||
               (v.size() == 1 &&
                   std::find(std::begin(v), std::end(v), ignore) == v.end());
    }
    // callback to mod castitem of referencing spell when this item is consumed
    // by another item
    void on_stack_to(Item* target);

    // Visualize and gems makes them appear for OTHER players, when inspecting
    // the wearer
    void visualize_gems(Player* owner = nullptr) const;
    bool meta_toggled_on; // true if item has a meta-gem and it's toggled on

private:
    void db_insert(Player* owner = nullptr);
    void db_update(Player* owner = nullptr);

    bool should_save_;
    bool in_db_;    // true if we've been saved to the DB, if false we can just
                    // free resources
    bool in_trade_; // true if currently in a trade-window
    inventory::slot slot_;
    ItemLootUpdateState m_lootState;
    uint32 temp_ench_dur_; // remaining time on temporary enchantment
    std::vector<Spell*> referencing_spells_;
};

// Item Helpers

// Gets the update value's offset for the corresponding visible item slot, see
// EPlayerVisibleItemFields for data offsets
inline uint32 item_field_offset(uint32 item_slot)
{
    return PLAYER_VISIBLE_ITEM_1 + item_slot * MAX_VISIBLE_ITEM_OFFSET;
}

#endif
