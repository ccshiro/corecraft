#include "inventory/personal_storage.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "World.h"
#include "Database/Database.h"
#include <algorithm>
#include <memory>

// XXX: Optimize verify, some suggestions:
// XXX: Optimize away inventory loop for copper-only transactions.
// XXX: Only build as many empty slots as we truly need <- this might be the
// biggest slow down factor, we should try this by comparing an empty
//      inventory character with a lot of bag space versus one with only a few
//      empty slots -- i.e., does the build up of this set cause a lot of
//      cycles? I think it might
// XXX: Transaction should probably have some booleans to indicate if it has
// certain actions, for speeding up verify
//      e.g., if we only have delets, why process adds? and if we only have
//      remove by pointer, why process remove by ids as well?

// XXX: Cleanup & improve structure on code in this file

// XXX: Make sure equipment effects are always correct (see XXXs in Player)
// XXX: A repair always ends up cheaper than what the client says (this is true
// for mangos as well), either that means
//      the client does not include the discounts from reputation in its
//      calculation, or that the discounts are wrong for the server.

bool sort_slot_fn(const inventory::slot& lhs, const inventory::slot& rhs)
{
    int bagl = lhs.backpack() ? 0 : lhs.bag();
    int bagr = rhs.backpack() ? 0 : rhs.bag();
    if (bagl == bagr)
        return lhs.index() < rhs.index();
    else
        return bagl < bagr;
}

// Note: storage_stage must start at 1, not 0, as 0 is equal to a non-verified
// transaction; would affect Player::Create() poorly
inventory::personal_storage::personal_storage(Player* player)
  : player_(player), storage_stage_(1), loading_(false)
{
    items_[main_bag].resize(main_bag_end, nullptr);

    for (uint8 i = bags_start; i < bags_end; ++i)
        items_.insert(std::make_pair(i, std::vector<Item*>()));
    for (uint8 i = bank_bags_start; i < bank_bags_end; ++i)
        items_.insert(std::make_pair(i, std::vector<Item*>()));
}

std::vector<Item*> inventory::personal_storage::load(
    QueryResult* result, uint32 money, uint32 time_since_logout)
{
    // NOTE: It's important that the select query sorts on the bag in reverse
    // order because the main bag has id 255 which is the highest possible
    //       the main bag must be loaded first because all the extra bags are
    //       contained in the main bag, and we need all these extra bags and
    //       their size
    //       before we can load any items from those bags

    // QueryResult* result = CharacterDatabase.PQuery("SELECT data, bag, index,
    // item, item_template FROM character_inventory JOIN
    // item_instance ON character_inventory.item = item_instance.guid WHERE
    // character_inventory.guid = '%u' ORDER BY bag DESC", PLAYER_GUID);

    // Mail items that are not storable in the
    std::vector<Item*> mail_items;

    copper_ = copper(money);
    player_->SetUInt32Value(PLAYER_FIELD_COINAGE, copper_.get());

    if (!result)
    {
        ++storage_stage_;
        return std::vector<Item*>();
    }

    loading_ = true;

    do
    {
        Field* fields = result->Fetch();
        uint8 bag = fields[1].GetUInt8();
        uint8 index = fields[2].GetUInt8();
        uint32 item_lowguid = fields[3].GetUInt32();
        uint32 item_id = fields[4].GetUInt32();

        slot item_slot(personal_slot, bag, index);

        // It's possible an item_template was deleted while the server was
        // offline
        // This would then result in some characters possibly having items that
        // can't really exist anymore
        // we check for this when loading those items, so you can always rely on
        // the prototype being valid
        // for any item after this point.
        const ItemPrototype* prototype = ObjectMgr::GetItemPrototype(item_id);
        if (!prototype)
        {
            // NOTE: If more item related tables are added you need to fix both
            // this function, the guild one _AND_ Item::db_* functions
            CharacterDatabase.PExecute(
                "DELETE FROM character_inventory WHERE item = '%u'",
                item_lowguid);
            CharacterDatabase.PExecute(
                "DELETE FROM item_instance WHERE guid = '%u'", item_lowguid);
            CharacterDatabase.PExecute(
                "DELETE FROM item_text WHERE guid = '%u'", item_lowguid);
            CharacterDatabase.PExecute(
                "DELETE FROM character_gifts WHERE item_guid = '%u'",
                item_lowguid);
            CharacterDatabase.PExecute(
                "DELETE FROM item_loot WHERE guid = '%u'", item_lowguid);
            continue;
        }

        auto item = new Item(prototype);

        // Load all the fields into the item
        if (!item->LoadFromDB(item_lowguid, fields, player_->GetObjectGuid()))
        {
            CharacterDatabase.PExecute(
                "DELETE FROM character_inventory WHERE item = '%u'",
                item_lowguid);
            item->db_delete();
            delete item;
            continue;
        }

        // Buyback slots do not persist when logging out, we delete them on
        // loading
        if (item_slot.buyback())
        {
            CharacterDatabase.PExecute(
                "DELETE FROM character_inventory WHERE item = '%u'",
                item_lowguid);
            item->db_delete();
            delete item;
            continue;
        }

        // If the bag slot is invalid we remove this item
        // OR if the bag slot is in an extra bag, and the attached bag does not
        // exist we also remove the bag
        if (!item_slot.valid_bag() ||
            (item_slot.extra_bag() && !items_[main_bag][item_slot.bag()]))
        {
            CharacterDatabase.PExecute(
                "DELETE FROM character_inventory WHERE item = '%u'",
                item_lowguid);
            item->db_delete();
            delete item;
            continue;
        }

        // Verify that items placed in bagslots are actually bags
        if ((item_slot.bagslot() || item_slot.bank_bagslot()))
        {
            if (!item->IsBag())
            {
                CharacterDatabase.PExecute(
                    "DELETE FROM character_inventory WHERE item = '%u'",
                    item_lowguid);
                item->db_delete();
                delete item;
                continue;
            }
            items_[item_slot.index()].resize(prototype->ContainerSlots);
        }

        // Verify that the item can exist in this zone (only apply if we're
        // alive; in case we're corpse running)
        // NOTE: zone cache not yet loaded at this point, the deletion of the
        //       item will happen in UpdateZone anyway
        /* if (player_->isAlive() &&
            item->IsLimitedToAnotherMapOrZone(
                player_->GetMapId(), player_->GetZoneId()))
        {
            CharacterDatabase.PExecute(
                "DELETE FROM character_inventory WHERE item = '%u'",
                item_lowguid);
            item->db_delete();
            delete item;
            continue;
        } */

        // Conjured items disappear if we're logged out for longer than 15
        // minutes
        if (time_since_logout > 15 * MINUTE &&
            (prototype->Flags & ITEM_FLAG_CONJURED))
        {
            CharacterDatabase.PExecute(
                "DELETE FROM character_inventory WHERE item = '%u'",
                item_lowguid);
            item->db_delete();
            delete item;
            continue;
        }

        // Verify that the bag is big enough to hold the index specified in the
        // table
        if (item_slot.index() >= items_[item_slot.bag()].size())
        {
            CharacterDatabase.PExecute(
                "DELETE FROM character_inventory WHERE item = '%u'",
                item_lowguid);
            item->db_delete();
            delete item;
            continue;
        }

        if (!item_slot.can_hold(prototype))
        {
            CharacterDatabase.PExecute(
                "DELETE FROM character_inventory WHERE item = '%u'",
                item_lowguid);
            item->db_delete();
            delete item;
            continue;
        }

        // If item is placed in an equipment slot, we need to verify that the
        // player still has the required
        // skills to equip that item. This could change on a talent reset while
        // offline, for example.
        if (item_slot.equipment() &&
            player_->can_use_item(prototype) != EQUIP_ERR_OK)
        {
            // Mail the item, and remove it from the player's inventory.
            CharacterDatabase.PExecute(
                "DELETE FROM character_inventory WHERE item = '%u'",
                item_lowguid);
            mail_items.push_back(item);
            continue;
        }

        item->SetOwnerGuid(player_->GetObjectGuid());
        put_item(item, item_slot, false);
    } while (result->NextRow());

    loading_ = false;

    ++storage_stage_;
    return mail_items;
}

void inventory::personal_storage::save()
{
    // Save any items in our inventory that is marked for saving
    for (auto& elem : items_)
    {
        std::vector<Item*>& items = elem.second;
        for (auto i_itr = items.begin(); i_itr != items.end(); ++i_itr)
        {
            Item* item = *i_itr;
            if (item == nullptr)
                continue;

            if (item->should_save())
            {
                item->db_save(player_);
                save_storage_item(
                    item, slot(personal_slot, elem.first,
                              std::distance(items.begin(), i_itr)));
            }
        }
    }

    // Delete any items that have been marked for deletion
    static SqlStatementID delete_storage;
    for (auto& elem : deleted_items_)
    {
        // With remove we no longer own the resources, and dereferencing the
        // item pointer is undefined behavior
        if (elem.second.second == delete_remove)
        {
            SqlStatement stmt =
                CharacterDatabase.CreateStatement(delete_storage,
                    "DELETE FROM character_inventory WHERE item = ?");
            stmt.PExecute(elem.first);
        }
        // With destroy we are expected to release the resources
        else if (elem.second.second == delete_destroy)
        {
            SqlStatement stmt =
                CharacterDatabase.CreateStatement(delete_storage,
                    "DELETE FROM character_inventory WHERE item = ?");
            stmt.PExecute(elem.first);
            elem.second.first->db_delete();
            delete elem.second.first;
        }
    }

    deleted_items_.clear();
}

void inventory::personal_storage::save_storage_item(Item* item, slot item_slot)
{
    // NOTE: These are part of the BeginTransaction() & CommitTransaction()
    // scope in Player::SaveToDB(), which means
    // that even though they would've been potentially unordered (asynchronous)
    // as they currently appear, the fact that
    // they happen while we have an open transaction in Player::SaveToDB()
    // guarantees that they are in order
    static SqlStatementID delete_storage;
    static SqlStatementID insert_storage;

    SqlStatement stmt = CharacterDatabase.CreateStatement(
        delete_storage, "DELETE FROM character_inventory WHERE item = ?");
    stmt.addUInt32(item->GetGUIDLow());
    stmt.Execute();

    stmt = CharacterDatabase.CreateStatement(insert_storage,
        "INSERT INTO character_inventory (guid, bag, `index`, item, "
        "item_template) VALUES (?, ?, ?, ?, ?)");
    stmt.addUInt32(player_->GetGUIDLow());
    stmt.addUInt8(item_slot.bag());
    stmt.addUInt8(item_slot.index());
    stmt.addUInt32(item->GetGUIDLow());
    stmt.addUInt32(item->GetEntry());
    stmt.Execute();
}

bool inventory::personal_storage::gems_unique_equipped_check(
    Item* equipping, inventory::slot dst) const
{
    // Check if this item has any unique-equipped gems
    uint32 unique_gems[MAX_GEM_SOCKETS] = {0};
    bool has_unique_gem = false;
    for (int i = 0; i < MAX_GEM_SOCKETS; ++i)
        if (uint32 ench_id = equipping->GetEnchantmentId(
                static_cast<EnchantmentSlot>(SOCK_ENCHANTMENT_SLOT + i)))
            if (ObjectMgr::GetItemPrototype(
                    (sSpellItemEnchantmentStore.LookupEntry(ench_id)->GemID))
                    ->Flags &
                ITEM_FLAG_UNIQUE_EQUIPPED)
            {
                has_unique_gem = true;
                unique_gems[i] = ench_id;
            }

    if (!has_unique_gem)
        return true;

    for (iterator itr = begin(iterator::equipment); itr != end(); ++itr)
    {
        // Skip checking item slot we're replacing
        if ((*itr)->slot() == dst)
            continue;

        for (int i = 0; i < MAX_GEM_SOCKETS; ++i)
        {
            if (uint32 ench_id = (*itr)->GetEnchantmentId(
                    static_cast<EnchantmentSlot>(SOCK_ENCHANTMENT_SLOT + i)))
                for (auto& unique_gem : unique_gems)
                    if (unique_gem == ench_id)
                        return false;
        }
    }

    return true;
}

InventoryResult inventory::personal_storage::can_equip_bag(
    Item* bag, inventory::slot dst) const
{
    if (!bag->IsBag())
        return EQUIP_ERR_NOT_A_BAG;

    // Some bags are only equippable one of its kind at a time
    if (dst.bagslot())
    {
        for (unsigned int i = bags_start; i < bags_end; ++i)
        {
            if (dst.index() == i)
                continue;
            Item* other_bag = get(slot(personal_slot, main_bag, i));
            if (!other_bag || bag == other_bag)
                continue;
            // Quivers and Ammo Pouches
            if (bag->GetProto()->Class == ITEM_CLASS_QUIVER &&
                other_bag->GetProto()->Class == ITEM_CLASS_QUIVER)
                return bag->GetProto()->SubClass !=
                               other_bag->GetProto()->SubClass ?
                           EQUIP_ERR_TOO_MANY_SPECIAL_BAGS :
                           bag->GetProto()->SubClass == ITEM_SUBCLASS_QUIVER ?
                           EQUIP_ERR_CAN_EQUIP_ONLY1_QUIVER :
                           EQUIP_ERR_CAN_EQUIP_ONLY1_AMMOPOUCH;
            // For special bags (soul pouch, mining sack, etc) there can
            // only exist one of that particular type
            if (bag->GetProto()->Class == ITEM_CLASS_CONTAINER &&
                bag->GetProto()->SubClass != ITEM_SUBCLASS_CONTAINER)
                if (bag->GetProto()->Class == other_bag->GetProto()->Class &&
                    bag->GetProto()->SubClass ==
                        other_bag->GetProto()->SubClass)
                    return EQUIP_ERR_TOO_MANY_SPECIAL_BAGS;
        }
    }

    // Checks that apply only if dst is a bag in the bank
    if (dst.bank_bagslot())
    {
        // Must own the bank slot
        if (player_->GetBankBagSlotCount() <= (dst.index() - bank_bags_start))
            return EQUIP_ERR_MUST_PURCHASE_THAT_BAG_SLOT;
        // Can't store quivers in the bank
        if (bag->GetProto()->Class == ITEM_CLASS_QUIVER)
            return EQUIP_ERR_NOT_A_BAG;
    }

    return EQUIP_ERR_OK;
}

InventoryResult inventory::personal_storage::can_store(
    slot dst, Item* item, Player* /*player*/, bool skip_unique_check) const
{
    const ItemPrototype* prototype = item->GetProto();
    uint32 item_id = prototype->ItemId;
    uint32 count = item->GetCount();

    // Note: We check equip related checks in here, as one can never equip
    // an item by just an id and count.

    // Verify unique-equip.
    if (dst.equipment() && (prototype->Flags & ITEM_FLAG_UNIQUE_EQUIPPED))
    {
        const std::vector<Item*>& gear = items_subscript(main_bag);
        for (int i = equipment_start; i < equipment_end; ++i)
        {
            if (!gear[i])
                continue;
            // Skip items not the same as us
            if (gear[i]->GetProto()->ItemId != item_id)
                continue;
            // If it's not the same item or slot we can't equip it
            if (gear[i] != item && dst.index() != i)
                return EQUIP_ERR_ITEM_UNIQUE_EQUIPABLE;
        }
    }

    // Verify that the player can equip an equipment item
    if (dst.equipment() || dst.bagslot() || dst.bank_bagslot())
    {
        InventoryResult res = player_->can_perform_equip(item);
        if (res != EQUIP_ERR_OK)
            return res;

        // Checks that only apply to equipment, not bagslots
        if (dst.equipment())
        {
            if (!gems_unique_equipped_check(item, dst))
                return EQUIP_ERR_ITEM_MAX_COUNT_EQUIPPED_SOCKETED;

            if (dst.off_hand())
            {
                // Can't have two-handed in off-hand or main-hand weapon
                if (prototype->InventoryType == INVTYPE_2HWEAPON ||
                    prototype->InventoryType == INVTYPE_WEAPONMAINHAND)
                    return EQUIP_ERR_ITEM_DOESNT_GO_TO_SLOT;

                // Check so we can dual-wield if the type is a 1-handed weapon
                if ((prototype->InventoryType == INVTYPE_WEAPON ||
                        prototype->InventoryType == INVTYPE_WEAPONOFFHAND) &&
                    !player_->CanDualWield())
                    return EQUIP_ERR_CANT_DUAL_WIELD;

                // If we have a 2-handed in the main-hand we cannot equip this
                if (Item* mh = get(slot(personal_slot, main_bag, main_hand_e)))
                {
                    if (mh->GetProto()->InventoryType == INVTYPE_2HWEAPON)
                        return EQUIP_ERR_CANT_EQUIP_WITH_TWOHANDED;
                }
            }
        }
    }

    return can_store(dst, item_id, count, skip_unique_check);
}

InventoryResult inventory::personal_storage::can_store(
    slot dst, uint32 item_id, uint32 count, bool skip_unique_check) const
{
    const ItemPrototype* prototype = ObjectMgr::GetItemPrototype(item_id);
    assert(prototype != nullptr);

    // Verify the slot can hold this item
    if (!dst.can_hold(prototype))
        return EQUIP_ERR_ITEM_DOESNT_GO_TO_SLOT;

    // Verify max-count (includes uniqueness). TODO: If item is in our bag
    if (!skip_unique_check && prototype->MaxCount > 0)
    {
        if (item_count(item_id, false, nullptr, true) + count >
            prototype->MaxCount)
            return EQUIP_ERR_CANT_CARRY_MORE_OF_THIS;
    }

    // Verify that we really have this bag equipped
    auto bag_itr = items_.find(dst.bag());
    if (bag_itr == items_.end())
        return EQUIP_ERR_INT_BAG_ERROR;

    // When trying to store in an "extra bag", Verify that bag exists, and that
    // we can store in it.
    if (dst.extra_bag() || dst.extra_bank_bag())
    {
        Item* bag = items_subscript(main_bag)[dst.bag()];
        if (!bag || !bag->GetProto())
            return EQUIP_ERR_INT_BAG_ERROR;
        if (!ItemCanGoIntoBag(prototype, bag->GetProto()))
            return EQUIP_ERR_ITEM_DOESNT_GO_INTO_BAG;
    }

    // Verify that the bag is big enough for the index we've specified
    if (dst.index() >= bag_itr->second.size())
        return EQUIP_ERR_INT_BAG_ERROR;

    // We're able to store this item in the specified slot
    return EQUIP_ERR_OK;
}

bool inventory::personal_storage::verify(transaction& trans) const
{
    if (trans.finalized_)
        return false;

    uint32 error = 0, temp_err;
    trans.finalize_actions_.clear();
    trans.add_failures_.clear();

    std::map<uint32 /*itemid*/, std::vector<slot>>
        targets;                               // Slots that can hold itemid
    std::set<slot, slot_sort_cmp> empty_slots; // Slots that are empty. See
                                               // struct slot_sort_cmp to see
                                               // notes on ordering.
    std::map<slot, uint32 /*count*/, slot_sort_cmp> overwritten; // Slots where
                                                                 // count has
                                                                 // changed from
                                                                 // what it is
                                                                 // in our
                                                                 // inventory.

    copper gold(money().get());

    // Copy the deletions and removals into a temporary vector (skip gold), so
    // we know which are left to be processed
    std::vector<transaction::action> delete_actions;
    for (auto& elem : trans.actions_)
        if (elem.second.type != transaction::operand::by_gold &&
            (elem.first == transaction::op_remove ||
                elem.first == transaction::op_destroy))
            delete_actions.push_back(elem);

    // Loop through the inventory and build up targets
    for (const auto& elem : items_)
    {
        uint8 bag = elem.first;

        slot s(personal_slot, bag, 0);
        if ((s.bank() || s.extra_bank_bag() || s.bank_bagslot()) &&
            !trans.include_bank_)
            continue;

        const std::vector<Item*>& items = elem.second;
        for (size_t i = 0; i < items.size(); ++i)
        {
            slot s(personal_slot, bag, i);

            // Buyback slots are not manipulatable
            if (s.buyback())
                continue;

            // Process deletion of this item if we should. Returns true if
            // deletion was processed.
            if (items[i])
            {
                std::pair<bool, InventoryResult> res =
                    process_iterating_deletion(trans, s, delete_actions,
                        targets, empty_slots, overwritten, gold);
                if (res.first)
                {
                    if (error == EQUIP_ERR_OK && res.second != EQUIP_ERR_OK)
                        error = res.second;
                    continue;
                }
            }

            // Bank slots cannot be processed for adds, only for deletions. And
            // past this point is only adding logic.
            if (s.bank() || s.extra_bank_bag() || s.bank_bagslot())
                continue;

            // Insert empty slots into empty_slots (skip bank slots)
            if (items[i] == nullptr)
            {
                empty_slots.insert(s);
                continue;
            }

            // Insert into targets

            // Insert slots into targets that holster an item with the same Id
            // as one we're trying to add
            uint32 our_id = items[i]->GetProto()->ItemId;
            bool found = false;
            for (auto& _itr : trans.actions_)
                if (_itr.first == transaction::op_add &&
                    ((_itr.second.type == transaction::operand::by_id &&
                         our_id == _itr.second.id) ||
                        (_itr.second.type == transaction::operand::by_item &&
                            our_id == _itr.second.item->GetProto()->ItemId)))
                {
                    found = true;
                    break;
                }

            // If we found: Make sure slot isn't in empty_slots already (it can
            // be if the count was overwritten in a previous iteration)
            if (found && empty_slots.find(s) == empty_slots.end())
            {
                targets[items[i]->GetProto()->ItemId].push_back(s);
            }
        }
    }

    // Finish up the processing of removal actions
    if ((temp_err = process_deletions(trans, delete_actions, targets,
             empty_slots, overwritten, gold)) != EQUIP_ERR_OK &&
        error == 0)
        error = temp_err; // Only overwrite if error was 0 previously

    // Targets is now built as well. It's time to process adding of items
    if ((temp_err = process_adds(
             trans, targets, empty_slots, overwritten, gold)) != EQUIP_ERR_OK &&
        error == 0)
        error = temp_err; // Only overwrite if error was 0 previously

    // Save the resulting copper. This is an absolute value where transactions
    // have been applied on a copy of the personal_storage's copper_.
    trans.finalized_gold_ = gold;

    // Error == 0 means the transaction went through fully, otherwise error is
    // the first encountered error.
    if (error != 0)
    {
        trans.storage_stage_ = 0;
        trans.error_ = error;
        return false;
    }

    // Transaction has been verified. trans.finalize_actions_ holds what will be
    // carried out in ::finalize(trans)
    trans.storage_stage_ = storage_stage_;
    return true;
}

std::pair<bool, InventoryResult>
inventory::personal_storage::process_iterating_deletion(transaction& trans,
    slot current_slot, std::vector<transaction::action>& delete_actions,
    std::map<uint32, std::vector<slot>>& targets,
    std::set<slot, slot_sort_cmp>& empty_slots,
    std::map<slot, uint32, slot_sort_cmp>& /*overwritten*/,
    copper& /*gold*/) const
{
    // This function processes deletions while we're looping the inventory,
    // and is called once for each slot that has an item

    Item* item = items_subscript(current_slot.bag())[current_slot.index()];

    // First check if the item is contained as a pointer in the actions
    bool found_by_id = false;
    for (auto itr = delete_actions.begin(); itr != delete_actions.end(); ++itr)
    {
        if (itr->second.type == transaction::operand::by_item &&
            itr->second.item == item)
        {
            // If this item is not deletable, we return the error that prevents
            // us from doing so
            InventoryResult delete_res =
                can_delete_item(current_slot, trans.user_action_);
            if (delete_res != EQUIP_ERR_OK)
            {
                return std::make_pair(true, delete_res);
            }

            // Register item for deletion once ::finalize() is called
            trans.finalize_actions_.push_back(transaction::finalize_action(
                itr->first == transaction::op_remove ? transaction::fa_remove :
                                                       transaction::fa_destroy,
                current_slot));

            // Erase from actions & mark slot as empty
            delete_actions.erase(itr);
            empty_slots.insert(current_slot);
            return std::make_pair(true, EQUIP_ERR_OK);
        }
        if (itr->second.type == transaction::operand::by_id &&
            item->GetProto()->ItemId == itr->second.id)
            found_by_id = true;
    }

    // If we need to delete this item by id, and we didn't delete it by pointer
    // we need to keep
    // track of the position to delete by id later. Targets therefore has both
    // itemids which are
    // related to our adds, and also related to our deletions.
    if (found_by_id)
    {
        targets[item->GetProto()->ItemId].push_back(current_slot);
        return std::make_pair(true, EQUIP_ERR_OK);
    }

    return std::make_pair(false, EQUIP_ERR_OK);
}

InventoryResult inventory::personal_storage::process_deletions(
    transaction& trans, std::vector<transaction::action>& delete_actions,
    std::map<uint32, std::vector<slot>>& targets,
    std::set<slot, slot_sort_cmp>& empty_slots,
    std::map<slot, uint32, slot_sort_cmp>& overwritten, copper& gold) const
{
    // Note: Removing is smart, it removes from the item stack that has the
    // lowest count

    // This function is called after the iteration is done, in here
    // we need to process removal actions that happened by id,
    // as well as removal of any gold.

    InventoryResult err = EQUIP_ERR_OK;

    // Process removed gold
    for (auto& elem : trans.actions_)
    {
        if (elem.first == transaction::op_remove &&
            elem.second.type == transaction::operand::by_gold)
        {
            gold -= elem.second.gold;
            if (gold.spill() && err == EQUIP_ERR_OK)
                err = EQUIP_ERR_NOT_ENOUGH_MONEY;
        }
    }

    // Process remaining deletion actions
    for (auto& delete_action : delete_actions)
    {
        // We need to check for item pointers in case someone
        // tried to delete one that doesn't exist in the target storage
        if (delete_action.second.type == transaction::operand::by_item)
        {
            if (err == EQUIP_ERR_OK)
                err = EQUIP_ERR_ITEM_NOT_FOUND;
            continue;
        }

        uint32 count = delete_action.second.count;
        std::vector<slot>& slots = targets[delete_action.second.id];
        // Sort slots so backpack is before extra bags. In case all possible
        // stacks are of equal count, deletes will prefer backpack over
        // extra-bags.
        std::sort(slots.begin(), slots.end(), sort_slot_fn);
        // Drop charges from items in our bags while we still have a count > 0
        while (count > 0 && slots.size() > 0)
        {
            std::pair<slot, uint32 /*slot_count*/> lowest_slot(slot(), 0);
            size_t lowest_index;

            // Find the slot that has the stack with the lowest count of this
            // item type
            for (size_t i = 0; i < slots.size(); ++i)
            {
                uint32 slot_count =
                    items_subscript(
                        slots[i].bag())[slots[i].index()]->GetCount();
                if (overwritten[slots[i]] > 0)
                    slot_count = overwritten[slots[i]];
                // Check if this one is lower
                if (!lowest_slot.first.valid() ||
                    slot_count < lowest_slot.second)
                {
                    lowest_slot.first = slots[i];
                    lowest_slot.second = slot_count;
                    lowest_index = i;
                }
            }
            assert(lowest_slot.first != slot());

            // Remove the slot and try the next one if we can't delete the item
            // in it
            if (can_delete_item(lowest_slot.first, trans.user_action_) !=
                EQUIP_ERR_OK)
            {
                slots.erase(slots.begin() + lowest_index);
                continue;
            }

            // Subtract count from this slot, consuming it if we deplete it
            if (count >= lowest_slot.second)
            {
                count -= lowest_slot.second;
                empty_slots.insert(lowest_slot.first);
                slots.erase(
                    slots.begin() + lowest_index); // The slot was consumed

                // Register destroying of item in transaction for once
                // ::finalize() is called
                trans.finalize_actions_.push_back(transaction::finalize_action(
                    transaction::fa_destroy, lowest_slot.first));
            }
            else
            {
                // Update count in overwritten
                overwritten[lowest_slot.first] = lowest_slot.second - count;
                count = 0;
                // Register modding of stack count of item in transaction for
                // once ::finalize() is called
                trans.finalize_actions_.push_back(
                    transaction::finalize_action(transaction::fa_mod_stack,
                        lowest_slot.first, 0, overwritten[lowest_slot.first]));
            }
        }

        if (count > 0 && err == EQUIP_ERR_OK)
            err = EQUIP_ERR_MISSING_REAGENT; // No error that better describes
                                             // this problem is found
    }

    delete_actions.clear();

    return err;
}

InventoryResult inventory::personal_storage::process_adds(transaction& trans,
    std::map<uint32, std::vector<slot>>& targets,
    std::set<slot, slot_sort_cmp>& empty_slots,
    std::map<slot, uint32, slot_sort_cmp>& overwritten, copper& gold) const
{
    // Note: Adding takes the first available slot, aka it's not smart.
    // XXX: This function should be split up into more functions, it's way too
    // big atm.

    InventoryResult err = EQUIP_ERR_OK;

    for (auto itr = trans.actions_.begin(); itr != trans.actions_.end(); ++itr)
    {
        // Skip actions that aren't adds
        if (itr->first != transaction::op_add)
            continue;

        // Process gold adds and then continue
        if (itr->second.type == transaction::operand::by_gold)
        {
            gold += itr->second.gold;
            trans.add_failures_.push_back(gold.spill());
            if (err == 0 && gold.spill())
                err = EQUIP_ERR_TOO_MUCH_GOLD;
            continue;
        }

        // Process item adds (treat both as add by id and count)
        uint32 id = itr->second.type == transaction::operand::by_id ?
                        itr->second.id :
                        itr->second.item->GetProto()->ItemId;
        uint32 count = itr->second.type == transaction::operand::by_id ?
                           itr->second.count :
                           itr->second.item->GetCount();

        const ItemPrototype* prototype = ObjectMgr::GetItemPrototype(id);
        if (!prototype)
        {
            trans.add_failures_.push_back(count);
            if (err == 0)
                err = EQUIP_ERR_INT_BAG_ERROR;
            continue;
        }

        uint32 max_stack_size = prototype->Stackable;

        // If the transaction tried to add an item with count 0 the transaction
        // fails
        if (count == 0)
        {
            trans.add_failures_.push_back(0);
            if (err == 0)
                err = EQUIP_ERR_INT_BAG_ERROR;
            continue;
        }

        // We check uniqueness only once, instead of for each can_store() call
        if (prototype->MaxCount > 0)
        {
            uint32 has_count = item_count(id, false, nullptr, true);
            if (has_count + count > prototype->MaxCount)
            {
                trans.add_failures_.push_back(
                    count - (prototype->MaxCount - has_count));
                if (err == 0)
                    err = EQUIP_ERR_CANT_CARRY_MORE_OF_THIS;
                continue;
            }
        }

        // Process items that do not stack individually
        if (max_stack_size <= 1)
        {
            for (; empty_slots.size() > 0 && count > 0; --count)
            {
                // Find an empty slot (skipping equipment slots & once we can't
                // store in); item does not stack
                slot s;
                bool tried_special_bag = false;
                do
                {
                    if (!tried_special_bag)
                    {
                        tried_special_bag = true;
                        if (store_special_bag(s, prototype))
                            continue; // Escapes loop if slot was truly valid
                    }
                    s = *empty_slots.begin();
                    empty_slots.erase(empty_slots.begin());
                } while (empty_slots.size() > 0 &&
                         ((!s.backpack() && !s.extra_bag() &&
                              !s.keyring()) || // Skip slots we cannot add into
                             can_store(s, id, count, true) !=
                                 EQUIP_ERR_OK)); // Note: We call id,
                // count can_store() even for item ptrs because we can never
                // add() into an equipment-slot

                // If our slot is still invalid we're trying to add to an
                // inventory that's full
                if (!s.valid() || s.equipment() ||
                    can_store(s, id, count, true) != EQUIP_ERR_OK)
                {
                    if (err == 0)
                        err = EQUIP_ERR_INVENTORY_FULL;
                    break;
                }

                // Register where it was added for once ::finalize() is called
                if (itr->second.type == transaction::operand::by_id)
                {
                    // Create a new item if we added by ID
                    trans.finalize_actions_.push_back(
                        transaction::finalize_action(
                            transaction::fa_create_item, s, id, count,
                            itr->second.rand_prop));
                }
                else
                {
                    // Store our item if we added by item ptr
                    trans.finalize_actions_.push_back(
                        transaction::finalize_action(
                            transaction::fa_set_item_ptr, s, itr->second.item));
                }
            }

            trans.add_failures_.push_back(count);
            if (err == 0 && count > 0)
                err = EQUIP_ERR_INVENTORY_FULL;
            continue; // The rest of the code is for items that do stack
        }

        // If we get here we're processing an item that stacks. We first try to
        // fill up items of this type that we already have before we occupy a
        // new slot.
        std::vector<slot>& slots = targets[id];
        // Sort slots so backpack is before extra bags. In case all possible
        // stacks are of equal count, adds will prefer backpack over extra-bags.
        std::sort(slots.begin(), slots.end(), sort_slot_fn);
        for (size_t i = 0; i < slots.size() && count > 0; ++i)
        {
            slot s = slots[i];
            // Skip any slots we can't add to
            if (!s.backpack() && !s.extra_bag() && !s.keyring())
                continue;

            uint32 other_count;
            if (overwritten[s] > 0)
                other_count = overwritten[s];
            else
                other_count = get(s)->GetCount();

            if (other_count == max_stack_size)
                continue;

            uint32 new_count = count + other_count >= max_stack_size ?
                                   max_stack_size :
                                   count + other_count;
            uint32 added = new_count - other_count;
            count -= added;

            // Remeber this slot and how many to store into it, for when we call
            // ::finalize()
            trans.finalize_actions_.push_back(transaction::finalize_action(
                transaction::fa_mod_stack, s, 0, new_count));
            overwritten[s] = new_count; // Update for further adds
        }

        // If everything fits into already existing items we are done with this
        // addition
        if (count == 0)
        {
            trans.add_failures_.push_back(0);
            continue;
        }

        // We still have stuff to store that will need to go into an empty slot
        slot s;
        bool tried_special_bag = false;
        while (empty_slots.size() > 0 && count > 0)
        {
            if (!tried_special_bag)
            {
                tried_special_bag = true;
                if (!store_special_bag(s, prototype))
                    continue; // No special bag, continue loop and get a
                              // "normal" slot this time
            }
            else
            {
                s = *empty_slots.begin();
                empty_slots.erase(empty_slots.begin());
            }

            uint32 stack_size = count < max_stack_size ? count : max_stack_size;

            // Check if we can store in slot
            if ((!s.backpack() && !s.extra_bag() && !s.keyring()) ||
                can_store(s, id, stack_size, true) != EQUIP_ERR_OK)
                continue;

            // Register us for storing in this slot
            if (itr->second.type == transaction::operand::by_id)
            {
                // Create a new item if we added by ID
                trans.finalize_actions_.push_back(
                    transaction::finalize_action(transaction::fa_create_item, s,
                        id, stack_size, itr->second.rand_prop));
            }
            else
            {
                // Store our item if we added by item ptr
                trans.finalize_actions_.push_back(transaction::finalize_action(
                    transaction::fa_set_item_ptr, s, itr->second.item));
                // Also mark it for having its count changed
                trans.finalize_actions_.push_back(transaction::finalize_action(
                    transaction::fa_mod_stack, s, 0, stack_size));
            }

            count -= stack_size;

            // We need to insert the slot of the newly added item into our
            // targets,
            // if it was not a full stack (later additions might add ontop of
            // it)
            if (stack_size != max_stack_size)
            {
                targets[id].push_back(s);
                overwritten[s] = stack_size; // Note: We must insert into
                                             // overwritten, since no item
                                             // actually exists at this slot yet
            }
        }

        trans.add_failures_.push_back(count);
        if (err == 0 && count > 0)
            err = EQUIP_ERR_INVENTORY_FULL;
    }

    return err;
}

bool inventory::personal_storage::finalize(transaction& trans)
{
    if (trans.storage_stage_ != storage_stage_)
    {
        if (!verify(trans))
            return false;
    }

    // Carry out all finalized actions
    if (copper_.get() != trans.finalized_gold_.get())
    {
        LOG_DEBUG(logging,
            "personal_storage::finalize(): previous copper: %s. new copper: %s",
            copper_.str().c_str(), trans.finalized_gold_.str().c_str());
        copper_ = trans.finalized_gold_;
        player_->SetUInt32Value(PLAYER_FIELD_COINAGE, copper_.get());
        // Update factors that happen when money changed (e.g. quests)
        on_money_changed();
    }

    // We have a map of all items added and how many were added, so
    // that we can properly report what items were received
    typedef std::map<uint32 /*item_id*/,
        std::pair<uint32 /*count*/, std::vector<Item*>>> adds_map;
    adds_map adds;

    // We need to save removal of items & the count, so that we can update
    // things such as quests
    typedef std::set<uint32 /*item_id*/> removal_map;
    removal_map removals;

    // Carry out each finalize_action in this transaction. ::verify() has
    // ordered
    // them accordingly and we're free to simply carry them out.
    for (auto itr = trans.finalize_actions_.begin();
         itr != trans.finalize_actions_.end(); ++itr)
    {
        switch (itr->type)
        {
        case transaction::fa_remove:
        {
            Item* item =
                items_[itr->target_slot.bag()][itr->target_slot.index()];
            removals.insert(item->GetEntry());

            pop_item(item, false);
            deleted_items_[item->GetGUIDLow()] =
                std::make_pair(static_cast<Item*>(nullptr),
                    delete_remove); // Note: NULL because
                                    // Item's resources no
                                    // longer belong to us
            trans.removed_items_.push_back(item);

            LOG_DEBUG(logging,
                "personal_storage::finalize(): removed item with id %u and "
                "count %u",
                item->GetProto()->ItemId, item->GetCount());
            break;
        }
        case transaction::fa_destroy:
        {
            Item* item =
                items_[itr->target_slot.bag()][itr->target_slot.index()];
            removals.insert(item->GetEntry());

            pop_item(item, false);
            deleted_items_[item->GetGUIDLow()] =
                std::make_pair(item, delete_destroy);

            LOG_DEBUG(logging,
                "personal_storage::finalize(): destroyed item with id %u and "
                "count %u",
                item->GetProto()->ItemId, item->GetCount());
            break;
        }
        case transaction::fa_mod_stack:
        {
            Item* item =
                items_[itr->target_slot.bag()][itr->target_slot.index()];
            uint32 previous_count = item->GetCount();
            item->SetCount(itr->count);
            item->mark_for_save();
            if (previous_count < itr->count)
            {
                adds[item->GetProto()->ItemId].first +=
                    itr->count - previous_count;
                adds[item->GetProto()->ItemId].second.push_back(
                    nullptr); // Adds to stack
            }
            else if (previous_count > itr->count)
            {
                removals.insert(item->GetEntry());
            }

            LOG_DEBUG(logging,
                "personal_storage::finalize(): modded stack of item with guid "
                "%u from %u to %u",
                item->GetGUIDLow(), previous_count, itr->count);
            break;
        }
        case transaction::fa_create_item:
        {
            // Create Item can only fail if the prototype is invalid, but we
            // check that so there's no risk of it failing
            Item* item =
                Item::CreateItem(itr->id, itr->count, player_, itr->prop);
            if (item == nullptr)
            {
                throw std::runtime_error(
                    "The line \"Item* item = Item::CreateItem(itr->id, "
                    "itr->count, player_)\" in inventory/personal_storage.cpp "
                    "resulted in a NULL pointer."
                    "This should never be possible.");
            }

            put_item(item, itr->target_slot, false);
            trans.added_items_.push_back(item);
            adds[item->GetProto()->ItemId].first += item->GetCount();
            adds[item->GetProto()->ItemId].second.push_back(item);

            LOG_DEBUG(logging,
                "personal_storage::finalize(): created new item with id %u and "
                "count %u (slot: bag:%u index:%u)",
                item->GetProto()->ItemId, item->GetCount(),
                itr->target_slot.bag(), itr->target_slot.index());
            break;
        }
        case transaction::fa_set_item_ptr:
        {
            put_item(itr->item, itr->target_slot, false);
            trans.added_items_.push_back(itr->item);
            adds[itr->item->GetProto()->ItemId].first += itr->item->GetCount();
            adds[itr->item->GetProto()->ItemId].second.push_back(itr->item);

            // We need to make sure this item isn't scheduled for a destroyal or
            // removal (in case it was removed and then added back in)
            auto find = deleted_items_.find(itr->item->GetGUIDLow());
            if (find != deleted_items_.end())
                deleted_items_.erase(find);

            LOG_DEBUG(logging,
                "personal_storage::finalize(): put item with guid %u and count "
                "%u in slot: bag:%u, index:%u",
                itr->item->GetGUIDLow(), itr->item->GetCount(),
                itr->target_slot.bag(), itr->target_slot.index());
            break;
        }
        case transaction::fa_drop_item_ptr: // Does not affect the storage. Item
                                            // ptr was a part of the transaction
                                            // but got consumed into stacks.
            itr->item->db_delete();
            delete itr->item;
            break;
        }
    }

    for (auto& add : adds)
    {
        // Call the item added callback (updates things such as quests)
        on_item_added(add.first, add.second.first);

        // Send the adding of the item to the client
        if (trans.send_result_ != transaction::send_none)
        {
            if (add.second.second.size() > 1 || add.second.second[0] == nullptr)
            {
                // Send as if they were added as a stack, to not get multiple
                // loot messages
                send_received_item(player_, nullptr, add.first,
                    add.second.first,
                    trans.add_method_ == transaction::add_vendor ? true : false,
                    trans.add_method_ == transaction::add_craft ? true : false,
                    trans.send_result_ == transaction::send_party ? true :
                                                                    false);
            }
            else
            {
                // Only one new item was added, send that item
                send_received_item(player_, add.second.second[0], add.first,
                    add.second.first,
                    trans.add_method_ == transaction::add_vendor ? true : false,
                    trans.add_method_ == transaction::add_craft ? true : false,
                    trans.send_result_ == transaction::send_party ? true :
                                                                    false);
            }
        }
    }

    // Call the item removed callback (updates things such as quests)
    for (const auto& removal : removals)
        on_item_removed(removal);

    trans.finalized_ = true;
    ++storage_stage_;
    return true;
}

bool inventory::personal_storage::iterator::slot_valid() const
{
    if (!slot_.valid())
        return false;
    auto itr = storage_->items_.find(slot_.bag());
    if (itr == storage_->items_.end())
        return false;
    const std::vector<Item*>& items = itr->second;
    if (slot_.index() >= items.size())
        return false;
    if (items[slot_.index()] == nullptr)
        return false;

    // Mask checks
    if (slot_.equipment() && (mask_ & equipment) == 0)
        return false;
    if ((slot_.backpack() || slot_.extra_bag()) && (mask_ & inventory) == 0)
        return false;
    if ((slot_.main_bank() || slot_.extra_bank_bag()) && (mask_ & bank) == 0)
        return false;
    if (slot_.keyring() && (mask_ & keyring) == 0)
        return false;
    if (slot_.bagslot() && (mask_ & bags) == 0)
        return false;
    if (slot_.bank_bagslot() && (mask_ & (bank | bags)) == 0)
        return false;

    return true;
}

void inventory::personal_storage::iterator::next_slot()
{
    do
    {
        if (slot_.bag() == main_bag)
        {
            // Main Bag

            slot current = slot_;
        // If we're at a start, we jump slots until we get into a container type
        // we're checking for.
        main_bag_switch_label:
            switch (slot_.index())
            {
            case equipment_start: // EQUIPMENT
                if ((mask_ & equipment) == 0)
                {
                    slot_ = slot(personal_slot, main_bag, bags_start);
                    goto main_bag_switch_label;
                }
                break;
            case bags_start: // EXTRA BAGS (the slots that hold the bags, not
                             // the bags' content)
                if ((mask_ & bags) == 0)
                {
                    slot_ = slot(personal_slot, main_bag, slot_start);
                    goto main_bag_switch_label;
                }
                break;
            case slot_start: // INVENTORY
                if ((mask_ & inventory) == 0)
                {
                    slot_ = slot(personal_slot, main_bag, bank_slot_start);
                    goto main_bag_switch_label;
                }
                break;
            case bank_slot_start: // BANK
                if ((mask_ & bank) == 0)
                {
                    slot_ = slot(personal_slot, main_bag, bank_bags_start);
                    goto main_bag_switch_label;
                }
                break;
            case bank_bags_start: // BANK BAGS
                if ((mask_ & (bank | bags)) == 0)
                {
                    slot_ = slot(personal_slot, main_bag, buyback_start);
                    goto main_bag_switch_label;
                }
                break;
            case buyback_start: // BUY BACK
                if ((mask_ & buy_back) == 0)
                {
                    slot_ = slot(personal_slot, main_bag, keyring_start);
                    goto main_bag_switch_label;
                }
                break;
            case keyring_start: // KEYRING
                if ((mask_ & keyring) == 0)
                {
                    slot_ = slot(personal_slot, main_bag, main_bag_end);
                    goto main_bag_check_end_label;
                }
                break;
            default:
                break;
            }

            if (slot_ != current) // Don't advance index if we already advanced
                                  // it to a new start
                continue;
            // Advance index by 1
            slot_ = slot(personal_slot, main_bag, slot_.index() + 1);

        main_bag_check_end_label:
            // Check if we're at the end of the main bag, if so start at the
            // extra bags,
            // or if that does not fit our mask_, just end preemptively
            if (slot_.index() == main_bag_end)
            {
                if ((mask_ & bags) == 0)
                    *this = storage_->end();
                else
                    slot_ = slot(personal_slot, bags_start, 0);
            }
        }
        else
        {
            // Extra bags

            if (slot_.bag() < bags_end &&
                (mask_ & inventory) == 0) // EXTRA INVENTORY BAGS
                slot_ = slot(personal_slot, bank_bags_start, 0);
            else if (slot_.bag() >= bank_bags_start &&
                     (mask_ & bank) == 0) // EXTRA BANK BAGS
                *this = storage_->end();
            else
            {
                auto itr = storage_->items_.find(slot_.bag());
                if (itr == storage_->items_.end())
                {
                    assert(false &&
                           "invalid bag found where valid bag was expected");
                    *this = storage_->end();
                    break;
                }
                const std::vector<Item*>& bag_content = itr->second;

                if (slot_.index() >= bag_content.size())
                    slot_ = slot(personal_slot, slot_.bag() + 1, 0);
                else
                    slot_ = slot(personal_slot, slot_.bag(), slot_.index() + 1);
                // Adjust for the gap in extra inventory bags -> extra bank bags
                if (slot_.bag() < bank_bags_start && slot_.bag() >= bags_end)
                    slot_ = slot(personal_slot, bank_bags_start, 0);
                // Check if we're at the end of the bags
                if (slot_.bag() >= bank_bags_end)
                    *this = storage_->end();
            }
        }
    } while (*this != storage_->end() && !slot_valid());
}

void inventory::personal_storage::put_item(Item* item, slot dst, bool swap)
{
    assert(dst.valid() && items_.find(dst.bag()) != items_.end() &&
           items_[dst.bag()].size() > dst.index());

    ++storage_stage_;

    if (dst.buyback())
    {
        uint32 buyback_dist = dst.index() - buyback_start;
        player_->SetGuidValue(
            PLAYER_FIELD_VENDORBUYBACK_SLOT_1 + (2 * buyback_dist),
            item->GetObjectGuid());
        player_->SetUInt32Value(PLAYER_FIELD_BUYBACK_PRICE_1 + buyback_dist,
            item->GetProto()->SellPrice * item->GetCount());
        player_->SetUInt32Value(PLAYER_FIELD_BUYBACK_TIMESTAMP_1 + buyback_dist,
            WorldTimer::getMSTime());
        item->SetGuidValue(ITEM_FIELD_CONTAINED, player_->GetObjectGuid());
    }
    else if (dst.bag() == main_bag)
    {
        player_->SetGuidValue(PLAYER_FIELD_INV_SLOT_HEAD + (2 * dst.index()),
            item->GetObjectGuid());
        item->SetGuidValue(ITEM_FIELD_CONTAINED, player_->GetObjectGuid());
    }
    else
    {
        Item* bag = items_[main_bag][dst.bag()];
        bag->SetGuidValue(
            CONTAINER_FIELD_SLOT_1 + (2 * dst.index()), item->GetObjectGuid());
        item->SetGuidValue(ITEM_FIELD_CONTAINED, bag->GetObjectGuid());
    }

    item->SetGuidValue(ITEM_FIELD_OWNER, player_->GetObjectGuid());

    // Bind item if necessary
    if (!item->IsSoulBound())
    {
        ItemBondingType bt =
            static_cast<ItemBondingType>(item->GetProto()->Bonding);
        if (bt == BIND_QUEST_ITEM)
            item->SetBinding(true);
        else if ((bt == BIND_WHEN_EQUIPPED && dst.equipment()) ||
                 dst.bagslot() || dst.bank_bagslot())
            item->SetBinding(true);
        else if (bt == BIND_WHEN_PICKED_UP)
            item->SetBinding(true);
    }

    if (dst.equipment())
    {
        uint32 base = item_field_offset(dst.index());
        player_->SetGuidValue(base + PLAYER_VISIBLE_ITEM_CREATOR,
            item->GetGuidValue(ITEM_FIELD_CREATOR));
        player_->SetUInt32Value(
            base + PLAYER_VISIBLE_ITEM_ENTRY, item->GetEntry());
        // Use SetInt16Value to prevent set high part to FFFF for negative value
        player_->SetInt16Value(
            item_field_offset(dst.index()) + PLAYER_VISIBLE_ITEM_PROPERTIES, 0,
            item->GetItemRandomPropertyId());
        player_->SetUInt32Value(
            item_field_offset(dst.index()) + PLAYER_VISIBLE_ITEM_PROPERTIES,
            item->GetItemSuffixFactor());
    }

    if (player_->IsInWorld() && !item->IsInWorld())
    {
        item->AddToWorld();
        item->SendCreateUpdateToPlayer(player_);
    }
    else
        item->AddToClientUpdateList();

    slot src = item->slot();

    items_[dst.bag()][dst.index()] = item;
    item->slot(dst);
    item->mark_for_save();

    // on_item_relocation needs to be called after the move happens in put
    // dont invoke relocation if a swap takes place; swap() will do that
    if (!swap)
        on_item_relocation(item, dst, src);

    // Track enchantment and item duration if necessary
    player_->TrackItemDurations(item, true);
    player_->TrackEnchDurations(item, true);
}

void inventory::personal_storage::pop_item(Item* item, bool swap)
{
    assert(item->slot().valid() &&
           items_.find(item->slot().bag()) != items_.end() &&
           items_[item->slot().bag()].size() > item->slot().index());

    ++storage_stage_;

    if (item->slot().buyback())
    {
        uint32 buyback_dist = item->slot().index() - buyback_start;
        player_->SetGuidValue(
            PLAYER_FIELD_VENDORBUYBACK_SLOT_1 + (2 * buyback_dist),
            ObjectGuid());
        player_->SetUInt32Value(PLAYER_FIELD_BUYBACK_PRICE_1 + buyback_dist, 0);
        player_->SetUInt32Value(
            PLAYER_FIELD_BUYBACK_TIMESTAMP_1 + buyback_dist, 0);
        item->SetGuidValue(ITEM_FIELD_CONTAINED, ObjectGuid());
    }
    else if (item->slot().bag() == main_bag)
    {
        player_->SetGuidValue(
            PLAYER_FIELD_INV_SLOT_HEAD + (2 * item->slot().index()),
            ObjectGuid());
        item->SetGuidValue(ITEM_FIELD_CONTAINED, ObjectGuid());
    }
    else
    {
        Item* bag = items_[main_bag][item->slot().bag()];
        bag->SetGuidValue(
            CONTAINER_FIELD_SLOT_1 + (2 * item->slot().index()), ObjectGuid());
        item->SetGuidValue(ITEM_FIELD_CONTAINED, ObjectGuid());
    }

    item->RemoveFromClientUpdateList();
    item->SetGuidValue(ITEM_FIELD_OWNER, ObjectGuid());

    if (item->slot().equipment())
    {
        uint32 base = item_field_offset(item->slot().index());
        for (uint32 i = 0; i < 16; ++i)
            player_->SetUInt32Value(base + i, 0);
    }

    // Remove tracking of enchantment & item duration in the player object for
    // this item
    player_->TrackItemDurations(item, false);
    player_->TrackEnchDurations(item, false);

    if (player_->IsInWorld() && !swap)
    {
        item->RemoveFromWorld();
        item->DestroyForPlayer(player_);
    }

    // on_item_relocation needs to be called before the move happens in pop
    // dont invoke relocation if a swap takes place; swap() will do that
    if (!swap)
        on_item_relocation(item, slot(), item->slot());

    items_[item->slot().bag()][item->slot().index()] = nullptr;
    // if item is about to be swapped it need not be put in an invalid inventory
    // state
    if (!swap)
        item->slot(slot()); // Invalid slot
}

InventoryResult inventory::personal_storage::swap(
    slot dst, slot src, uint32 src_count)
{
    if (!dst.valid() || !src.valid())
        return EQUIP_ERR_INT_BAG_ERROR;

    if (src == dst)
        return EQUIP_ERR_ITEMS_CANT_BE_SWAPPED;

    if (src.bank() || src.bank_bagslot() || src.extra_bank_bag() ||
        dst.bank() || dst.bank_bagslot() || dst.extra_bank_bag())
    {
        if (!player_->last_interacted_banker() ||
            !player_->GetNPCIfCanInteractWith(
                player_->last_interacted_banker(), UNIT_NPC_FLAG_BANKER))
            return EQUIP_ERR_TOO_FAR_AWAY_FROM_BANK;
    }

    Item* dst_item = get(dst); // Can be NULL
    Item* src_item = get(src); // Cannot be NULL

    if (!src_item)
        return EQUIP_ERR_INT_BAG_ERROR;

    // Neither item can be looted currently
    if (src_item->HasGeneratedLoot() ||
        (dst_item && dst_item->HasGeneratedLoot()))
        return EQUIP_ERR_CANT_DO_RIGHT_NOW;

    // Neither item can be in a trade window (or locked by spell)
    if (src_item->in_trade() || (dst_item && dst_item->in_trade()) ||
        src_item->already_referenced(nullptr) ||
        (dst_item && dst_item->already_referenced(nullptr)))
        return EQUIP_ERR_CANT_DO_RIGHT_NOW;

    // If we're trying to split a stack off, the src_item must have a bigger
    // count
    if (src_count > 0 && src_count >= src_item->GetCount())
        return EQUIP_ERR_TRIED_TO_SPLIT_MORE_THAN_COUNT;

    // Checks related to swapping with equipment slots
    if (dst.equipment() || src.equipment())
    {
        // NOTE: This code does not apply in 2.4.3, but stays here for
        // compatibility if needed in the future
        // We can't swap a main-hand weapon while we are disarmed
        // if ((dst.main_hand() || src.main_hand()) &&
        // player_->HasAuraType(SPELL_AURA_MOD_DISARM))
        // return EQUIP_ERR_NOT_WHILE_DISARMED;

        if (src.equipment() && !dst.equipment())
        {
            InventoryResult err = player_->can_perform_unequip(src_item);
            if (err != EQUIP_ERR_OK)
                return err;
        }
        else if (dst_item && dst.equipment() && !src.equipment())
        {
            InventoryResult err = player_->can_perform_unequip(dst_item);
            if (err != EQUIP_ERR_OK)
                return err;
        }
    }

    InventoryResult err = can_store(dst, src_item, player_, true);
    if (err != EQUIP_ERR_OK)
        return err;

    // Checks that only apply if our dst has an item in it
    if (dst_item)
    {
        if (src_count > 0)
        {
            // Checks for splitting item into a destination item,
            // src's split will be stacked onto dst_item

            // Items must be the same type
            if (dst_item->GetProto()->ItemId != src_item->GetProto()->ItemId)
                return EQUIP_ERR_COULDNT_SPLIT_ITEMS;

            // Together they cannot exceed the max stackable count of this item
            if (dst_item->GetCount() + src_count >
                src_item->GetProto()->Stackable)
                return EQUIP_ERR_COULDNT_SPLIT_ITEMS;
        }
        else
        {
            // dst_item will swap place with src_item, we must check so dst_item
            // goes into src's slot
            InventoryResult err = can_store(src, dst_item, player_, true);
            if (err != EQUIP_ERR_OK)
                return EQUIP_ERR_ITEMS_CANT_BE_SWAPPED; // We probably don't
                                                        // want to report the
                                                        // error, but instead
                                                        // just that they can't
                                                        // swap
        }
    }

    // Some extra checks related to bags
    // NOTE: Checks do not apply for when an equipped bag is moved to another
    // slot that is still equipped (its content just follows along then)
    if (src_item->IsBag() &&
        !((src.bagslot() || src.bank_bagslot()) &&
            (dst.bagslot() || dst.bank_bagslot())))
    {
        // Bags cannot be stored in themselves
        if (dst.bag() != main_bag &&
            (src_item->slot().bagslot() || src_item->slot().bank_bagslot()) &&
            dst.bag() == src_item->slot().index())
            return EQUIP_ERR_NONEMPTY_BAG_OVER_OTHER_BAG;

        // src cannot be unequipped if it has items in it
        if ((src.bagslot() || src.bank_bagslot()) && !dst.bagslot() &&
            !dst.bank_bagslot())
        {
            const std::vector<Item*>& content = items_subscript(src.index());
            for (const auto& elem : content)
                if (elem != nullptr)
                    return EQUIP_ERR_NONEMPTY_BAG_OVER_OTHER_BAG;
        }

        // Checks that apply if dst is an equipped bag:
        if (dst_item && dst_item->IsBag() &&
            (dst_item->slot().bagslot() || dst_item->slot().bank_bagslot()))
        {
            // src cannot be stored (prior to the swap) in the bag it's
            // replacing
            if (dst_item->slot().index() == src_item->slot().bag())
                return EQUIP_ERR_NONEMPTY_BAG_OVER_OTHER_BAG;

            // If src would replace an already equipped bag src must be able to
            // holster all its items
            uint32 item_count = 0;
            const std::vector<Item*>& content =
                items_subscript(dst_item->slot().index());
            for (const auto& elem : content)
            {
                if (elem != nullptr)
                {
                    if (!ItemCanGoIntoBag(
                            (elem)->GetProto(), src_item->GetProto()))
                        return EQUIP_ERR_NONEMPTY_BAG_OVER_OTHER_BAG;
                    ++item_count;
                }
            }
            if (src_item->GetProto()->ContainerSlots < item_count)
                return EQUIP_ERR_NONEMPTY_BAG_OVER_OTHER_BAG;
        }
    }

    // If src/dst is a bag going into a bagslot: can we equip it?
    if (dst.bagslot() || dst.bank_bagslot())
    {
        auto res = can_equip_bag(src_item, dst);
        if (res != EQUIP_ERR_OK)
            return res;
    }
    if (dst_item && (src.bagslot() || src.bank_bagslot()))
    {
        auto res = can_equip_bag(dst_item, src);
        if (res != EQUIP_ERR_OK)
            return res;
    }

    if (src_count > 0)
    {
        // We're swapping a split of an item

        if (dst_item)
        {
            dst_item->SetCount(dst_item->GetCount() + src_count);
            dst_item->mark_for_save();
        }
        else
        {
            Item* item = src_item->CloneItem(src_count, player_);
            if (!item)
                return EQUIP_ERR_INT_BAG_ERROR;
            put_item(item, dst, false);
        }

        src_item->SetCount(src_item->GetCount() - src_count);
        src_item->mark_for_save();
    }
    else
    {
        // We're swapping an entire item

        // If we're swapping items that actually stack they end up stacking
        // (if combined count is lower than max stack size)
        if (!src.equipment() && !dst.equipment() && dst_item &&
            dst_item->GetProto()->ItemId == src_item->GetProto()->ItemId)
        {
            uint32 total_count = dst_item->GetCount() + src_item->GetCount();
            if (total_count <= src_item->GetProto()->Stackable)
            {
                dst_item->SetCount(total_count);
                dst_item->mark_for_save();

                // update spells referencing the src_item to reference dst_item
                src_item->on_stack_to(dst_item);

                pop_item(src_item, false);
                deleted_items_[src_item->GetGUIDLow()] =
                    std::make_pair(src_item, delete_destroy);

                return EQUIP_ERR_OK;
            }
            // If total count exceeds the amount of a full stack we should make
            // the dst_item have the max count
            // and src_item have the remaining count.
            else if (dst_item->GetCount() < dst_item->GetProto()->Stackable)
            {
                dst_item->SetCount(dst_item->GetProto()->Stackable);
                dst_item->mark_for_save();

                src_item->SetCount(
                    total_count - dst_item->GetProto()->Stackable);
                src_item->mark_for_save();

                return EQUIP_ERR_OK;
            }
        }

        // SUCCESS. We're able to swap items (Note: The 2-hander fail condition
        // just below we need to have below this point, as we can only go
        // through with the off-hand swap if the 2-hand swap can't fail from
        // that point on)

        // If we're equipping a 2-hander we need to automatically store our
        // off-hand (if we have one) into an empty slot in our inventory. If we
        // can't; this swap fails as well.
        if ((dst.main_hand() &&
                src_item->GetProto()->InventoryType == INVTYPE_2HWEAPON) ||
            (src.main_hand() && dst_item &&
                dst_item->GetProto()->InventoryType == INVTYPE_2HWEAPON))
        {
            slot off_hand_slot = slot(personal_slot, main_bag, off_hand_e);
            if (Item* off_hand = get(off_hand_slot))
            {
                slot store_to = first_empty_slot_for(off_hand);
                if (!store_to.valid())
                    return EQUIP_ERR_INVENTORY_FULL;

                // Attempt to swap our off-hand into that slot
                InventoryResult res = swap(store_to, off_hand_slot);
                if (res != EQUIP_ERR_OK)
                    return res; // Off-hand swap failed, therefore so did the
                                // current swap
            }
        }

        // If we're equpping a bag we need to update the size of that in items_
        if ((dst.bagslot() || dst.bank_bagslot()) && !src.bagslot() &&
            !src.bank_bagslot()) // See note above about swapping equipped bags
        {
            // If we had a bag previously we need to transfer its content to
            // src_item
            // NOTE: That all items fit into the new bag is checked above
            if (dst_item)
            {
                std::vector<Item*>& content = items_[dst.index()];

                // If our container is shrinking and items would be outside the
                // bound, we need to move them inside. We simply move them into
                // first available slot
                int new_size = src_item->GetProto()->ContainerSlots;
                for (std::size_t i = new_size - 1; i < content.size(); ++i)
                {
                    if (content[i] != nullptr)
                    {
                        for (int j = 0; j < new_size; ++j)
                        {
                            if (content[j] == nullptr)
                            {
                                // Swap item into boundary of our new container
                                Item* item = content[i];
                                pop_item(item, true);
                                put_item(item,
                                    slot(personal_slot, dst.index(), j), false);
                                break;
                            }
                        }
                    }
                }

                // Update the src_item's content values, as well as mark items
                // as contained by src_item
                for (auto& elem : content)
                {
                    if (elem == nullptr)
                        continue;
                    (elem)->SetGuidValue(
                        ITEM_FIELD_CONTAINED, src_item->GetObjectGuid());
                    src_item->SetGuidValue(
                        CONTAINER_FIELD_SLOT_1 + (2 * (elem)->slot().index()),
                        (elem)->GetObjectGuid());
                    src_item->mark_for_save();
                }

                // Zero out the content of dst_item, since it no longer holds
                // any items
                for (uint32 i = 0; i < dst_item->GetProto()->ContainerSlots;
                     ++i)
                {
                    dst_item->SetGuidValue(
                        CONTAINER_FIELD_SLOT_1 + (2 * i), ObjectGuid());
                    dst_item->mark_for_save();
                }
            }

            items_[dst.index()].resize(
                src_item->GetProto()->ContainerSlots, nullptr);
        }
        // If we're unequipping a bag we need to resize that vector to be of
        // size 0
        else if ((src.bagslot() || src.bank_bagslot()) && !dst.bagslot() &&
                 !dst.bank_bagslot())
        {
            items_[src.index()].resize(0);
        }

        // Swap the items and update their visual & position
        std::swap(
            items_[dst.bag()][dst.index()], items_[src.bag()][src.index()]);

        // Remove both items from their slots
        pop_item(src_item, true);
        if (dst_item)
            pop_item(dst_item, true);

        // Add both items to their new slots
        put_item(src_item, dst, true);
        if (dst_item)
            put_item(dst_item, src, true);

        // Handle relocations. Flip order so that the item getting equipped (if
        // the other is unequipped),
        // gets processed last. In other words, the unequip needs to be
        // processed before the equip.
        // This is to handle weapon damage correctly (it'd change to the
        // equipped weapon's, and then change it
        // to be your unarmed damage on the unequip). Anything else would result
        // in the same final stats, no matter order.
        if (dst.equipment())
        {
            if (dst_item)
                on_item_relocation(dst_item, src, dst);
            on_item_relocation(src_item, dst, src);
        }
        else
        {
            on_item_relocation(src_item, dst, src);
            if (dst_item)
                on_item_relocation(dst_item, src, dst);
        }

        // If we're swapping an equipped bag (either to an empty slot or with
        // another bag) we need to update its content
        // We don't need to use pop_item or put_item as the items are contained
        // in bags and already correctly rendered
        if ((src.bagslot() || src.bank_bagslot()) &&
            (dst.bagslot() || dst.bank_bagslot()))
        {
            // We start by adjusting all contained items' slots
            const std::vector<Item*>& src_content = items_[src.index()];
            const std::vector<Item*>& dst_content = items_[dst.index()];
            size_t size = src_content.size() > dst_content.size() ?
                              src_content.size() :
                              dst_content.size();
            for (size_t i = 0; i < size; ++i)
            {
                if (src_content.size() > i && src_content[i])
                {
                    src_content[i]->slot(slot(personal_slot, dst.index(), i));
                    src_content[i]->mark_for_save();
                }
                if (dst_item && dst_content.size() > i && dst_content[i])
                {
                    dst_content[i]->slot(slot(personal_slot, src.index(), i));
                    dst_content[i]->mark_for_save();
                }
            }

            // Then we need to actually swap the containers' content
            std::swap(items_[src.index()], items_[dst.index()]);
        }
    }

    return EQUIP_ERR_OK;
}

inventory::slot inventory::personal_storage::find_auto_equip_slot(
    slot src) const
{
    // Skip empty src slot, or a src slot that already is an equipment slot
    Item* item = get(src);
    if (!item || (src.equipment() || src.bagslot() || src.bank_bagslot()))
        return slot();

    switch (item->GetProto()->InventoryType)
    {
    case INVTYPE_HEAD:
        return slot(personal_slot, main_bag, head);
    case INVTYPE_NECK:
        return slot(personal_slot, main_bag, neck);
    case INVTYPE_SHOULDERS:
        return slot(personal_slot, main_bag, shoulders);
    case INVTYPE_BODY:
        return slot(personal_slot, main_bag, shirt);
    case INVTYPE_CHEST:
        return slot(personal_slot, main_bag, chest);
    case INVTYPE_WAIST:
        return slot(personal_slot, main_bag, waist);
    case INVTYPE_LEGS:
        return slot(personal_slot, main_bag, legs);
    case INVTYPE_FEET:
        return slot(personal_slot, main_bag, feet);
    case INVTYPE_WRISTS:
        return slot(personal_slot, main_bag, wrists);
    case INVTYPE_HANDS:
        return slot(personal_slot, main_bag, hands);
    case INVTYPE_FINGER:
        if (items_subscript(main_bag)[finger1] != nullptr &&
            items_subscript(main_bag)[finger2] == nullptr)
            return slot(personal_slot, main_bag, finger2);
        return slot(personal_slot, main_bag, finger1);
    case INVTYPE_TRINKET:
        if (items_subscript(main_bag)[trinket1] != nullptr &&
            items_subscript(main_bag)[trinket2] == nullptr)
            return slot(personal_slot, main_bag, trinket2);
        return slot(personal_slot, main_bag, trinket1);
    case INVTYPE_WEAPON:
    {
        auto m = get(slot(personal_slot, main_bag, main_hand_e));
        if (m == nullptr ||
            m->GetProto()->InventoryType ==
                INVTYPE_2HWEAPON) // Prefer main hand if it's empty, or has a
                                  // two-hander in it
            return slot(personal_slot, main_bag, main_hand_e);
        if (get(slot(personal_slot, main_bag, off_hand_e)) ==
                nullptr && // Settle for off-hand if it's empty and we know how
                           // to
                           // dual-wield
            player_->CanDualWield())
            return slot(personal_slot, main_bag, off_hand_e);
        return slot(personal_slot, main_bag,
            main_hand_e); // Always go with main-hand if they're both occupied
    }
    case INVTYPE_SHIELD:
        return slot(personal_slot, main_bag, off_hand_e);
    case INVTYPE_RANGED:
        return slot(personal_slot, main_bag, ranged_e);
    case INVTYPE_CLOAK:
        return slot(personal_slot, main_bag, back);
    case INVTYPE_2HWEAPON:
        return slot(personal_slot, main_bag, main_hand_e);
    case INVTYPE_BAG:
        if (item->GetProto()->Class == ITEM_CLASS_QUIVER)
        {
            for (int i = bags_end - 1; i >= bags_start;
                 --i) // Quivers prefer going in the last bag slot
            {
                if (items_subscript(main_bag)[i] == nullptr)
                    return slot(personal_slot, main_bag, i);
            }
        }
        else
        {
            for (int i = bags_start; i < bags_end;
                 ++i) // Other bags go into the first best slot
            {
                if (items_subscript(main_bag)[i] == nullptr)
                    return slot(personal_slot, main_bag, i);
            }
        }
        break;
    case INVTYPE_TABARD:
        return slot(personal_slot, main_bag, tabard);
    case INVTYPE_ROBE:
        return slot(personal_slot, main_bag, chest);
    case INVTYPE_WEAPONMAINHAND:
        return slot(personal_slot, main_bag, main_hand_e);
    case INVTYPE_WEAPONOFFHAND:
        return slot(personal_slot, main_bag, off_hand_e);
    case INVTYPE_HOLDABLE:
        return slot(personal_slot, main_bag, off_hand_e);
    case INVTYPE_AMMO:
        break;
    case INVTYPE_THROWN:
        return slot(personal_slot, main_bag, ranged_e);
    case INVTYPE_RANGEDRIGHT:
        return slot(personal_slot, main_bag, ranged_e);
    case INVTYPE_QUIVER: // There is actually no item that has this inventory
                         // type. All quivers go as bags
        break;
    case INVTYPE_RELIC:
        return slot(personal_slot, main_bag, ranged_e);
    }

    return slot(); // Invalid slot
}

InventoryResult inventory::personal_storage::can_delete_item(
    slot s, bool user_action) const
{
    if (!s.valid() || !s.personal())
        return EQUIP_ERR_INT_BAG_ERROR;

    Item* item = get(s);
    if (!item)
        return EQUIP_ERR_INT_BAG_ERROR;

    if (user_action && s.equipment())
    {
        InventoryResult err = player_->can_perform_unequip(item);
        if (err != EQUIP_ERR_OK)
            return err;
    }

    if (user_action && item->GetProto()->Flags & ITEM_FLAG_INDESTRUCTIBLE)
        return EQUIP_ERR_INT_BAG_ERROR; // No error exists for this

    // Can't delete an equipped bag that has items in it
    if (s.bagslot() || s.bank_bagslot())
    {
        const std::vector<Item*>& content = items_subscript(s.index());
        for (const auto& elem : content)
            if (elem != nullptr)
                return EQUIP_ERR_CAN_ONLY_DO_WITH_EMPTY_BAGS;
    }

    // Can't delete items we're currently looting
    if (player_->GetLootGuid() == item->GetObjectGuid())
        return EQUIP_ERR_CANT_DO_RIGHT_NOW;

    // Can't delete items currently in a trade window
    if (item->in_trade())
        return EQUIP_ERR_CANT_DO_RIGHT_NOW;

    return EQUIP_ERR_OK;
}

inventory::slot inventory::personal_storage::find_empty_slot_in_bag(
    uint8 bag, bool bank) const
{
    if (bag == main_bag)
    {
        // If bag is main_bag, we either go through our backpack or the main
        // bank
        for (int i = (bank ? bank_slot_start : slot_start);
             i < (bank ? bank_slot_end : slot_end); ++i)
            if (items_subscript(main_bag)[i] == nullptr)
                return slot(personal_slot, main_bag, i);
    }
    else
    {
        auto find = items_.find(bag);
        if (find != items_.end())
        {
            const std::vector<Item*>& content = find->second;
            for (std::size_t i = 0; i < content.size(); ++i)
                if (content[i] == nullptr)
                    return slot(personal_slot, bag, i);
        }
    }
    return slot();
}

int inventory::personal_storage::sell_item(slot src, uint32 src_split)
{
    Item* item = get(src);
    if (!item || (!src.backpack() && !src.extra_bag() && !src.keyring() &&
                     !src.bagslot()))
        return SELL_ERR_CANT_FIND_ITEM;

    InventoryResult err = can_delete_item(src);
    if (err != EQUIP_ERR_OK)
        return err == EQUIP_ERR_CAN_ONLY_DO_WITH_EMPTY_BAGS ?
                   SELL_ERR_ONLY_EMPTY_BAG :
                   SELL_ERR_CANT_SELL_ITEM;

    uint32 sell_price = item->GetProto()->SellPrice;
    if (sell_price == 0)
        return SELL_ERR_CANT_SELL_ITEM;

    uint32 money = sell_price * (src_split ? src_split : item->GetCount());
    inventory::transaction trans;
    trans.add(money);
    if (!verify(trans))
        return SELL_ERR_CANT_SELL_ITEM;

    Item* sell_item = item;

    if (src_split != 0)
    {
        if (src_split >= item->GetCount())
            return SELL_ERR_CANT_SELL_ITEM;
        Item* new_item = item->CloneItem(src_split, player_);
        if (!new_item)
            return SELL_ERR_CANT_SELL_ITEM;
        item->SetCount(item->GetCount() - src_split);
        item->mark_for_save();
        sell_item = new_item;
    }

    // Finalize the money (verified above)
    finalize(trans);

    // Put buyback in empty slot, or replace oldest
    int index = 0;
    uint32 min = 0xFFFFFFFF;
    for (int i = 0; i < (buyback_end - buyback_start); ++i)
    {
        auto t = player_->GetUInt32Value(PLAYER_FIELD_BUYBACK_TIMESTAMP_1 + i);
        if (t == 0)
        {
            index = i;
            break;
        }
        else if (t < min)
        {
            min = t;
            index = i;
        }
    }
    index += buyback_start;

    // Update the sold item
    if (sell_item == item)
        pop_item(sell_item, true);

    // Remove item currently in slot if any
    if (auto item = items_[main_bag][index])
    {
        pop_item(item, false);
        deleted_items_[item->GetGUIDLow()] =
            std::make_pair(item, delete_destroy);
        items_[main_bag][index] = nullptr;
    }

    // Put sold item in buyback
    slot buyback_slot = slot(personal_slot, main_bag, index);
    put_item(sell_item, buyback_slot, false);

    return -1;
}

void inventory::personal_storage::buyback_item(slot src)
{
    Item* item = get(src);
    if (!item || !src.buyback())
        return;

    copper price = item->GetProto()->SellPrice * item->GetCount();

    // We use the transaction to auto-store the item back
    inventory::transaction trans(
        true, transaction::send_self, transaction::add_vendor);
    trans.remove(price);
    trans.add(item);
    if (!verify(trans))
    {
        player_->SendEquipError(
            static_cast<InventoryResult>(trans.error()), item);
        return;
    }

    pop_item(item, false);
    finalize(trans);
}

void inventory::personal_storage::on_item_relocation(
    Item* item, slot dst, slot src) const
{
    const ItemPrototype* proto = item->GetProto();

    // If the item is broken we should not apply any relocation changes, as the
    // breaking
    // of the item has already removed all effects associated with the equipping
    // of the item
    if (item->IsBroken())
        return;

    // Go through all spells and applies the one we need to apply
    for (int i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
    {
        const _Spell& spell_data = item->GetProto()->Spells[i];
        if (spell_data.SpellId == 0)
            continue;

        const SpellEntry* spell_info =
            sSpellStore.LookupEntry(spell_data.SpellId);
        if (!spell_info)
            continue;

        // Equip cooldown
        if (dst.equipment() &&
            spell_data.SpellTrigger == ITEM_SPELLTRIGGER_ON_USE &&
            (proto->Flags & ITEM_FLAG_NO_EQUIP_COOLDOWN) == 0 && !loading_)
        {
            // Make sure we don't overwrite the current cooldown with our
            // equipment cooldown, in case that's longer
            if (player_->GetSpellCooldownDelay(spell_data.SpellId) < 30)
            {
                player_->AddSpellCooldown(spell_data.SpellId, item->GetEntry(),
                    WorldTimer::time_no_syscall() + 30);
                WorldPacket data(SMSG_ITEM_COOLDOWN, 12);
                data << ObjectGuid(item->GetObjectGuid());
                data << uint32(spell_data.SpellId);
                player_->GetSession()->send_packet(std::move(data));
            }
        }

        if (spell_data.SpellTrigger == ITEM_SPELLTRIGGER_ON_STORE)
            player_->ApplyEquipSpell(
                spell_info, item, dst.valid_personal() ? true : false);

        if (spell_data.SpellTrigger == ITEM_SPELLTRIGGER_ON_EQUIP)
        {
            if ((dst.equipment() || dst.bagslot()) &&
                (src.equipment() || src.bagslot()))
                continue; // No need to update if we're still equipped post-swap
            if (dst.equipment() || dst.bagslot())
                player_->ApplyEquipSpell(spell_info, item, true);
            else if (src.equipment() || src.bagslot())
                player_->ApplyEquipSpell(spell_info, item, false);
        }
    }

    // No need to do anything of the below if we're just moving the item around
    // in the bags
    if (!dst.equipment() && !src.equipment())
        return;

    bool update_meta_gems = proto->Socket[0].Color != 0;
    int dst_attack_type = dst.attack_type();
    int src_attack_type = src.attack_type();

    if (src.equipment()) // Remove...
    {
        // Item enchantments (this includes gems, and meta-gem if we have a
        // meta-gem that's toggled on)
        player_->ApplyEnchantment(
            item, false, src, item->meta_toggled_on ? false : true);
        if (item->meta_toggled_on)
        {
            item->meta_toggled_on = false;
            update_meta_gems = false; // don't update meta gems below
        }
        // Raw stats (e.g., armor, agility, haste rating)
        player_->_ApplyItemBonuses(item, false, src);
        // Wepon-related effects (such as critical strike for that weapon type)
        if (src_attack_type < MAX_ATTACK)
            player_->_ApplyWeaponDependentAuraMods(
                item, static_cast<WeaponAttackType>(src_attack_type), false);
        // Item-set
        if (proto->ItemSet)
            RemoveItemsSetItem(player_, item->GetProto());
        // Remove auras casted by this item (unless item remains equipped)
        if (!dst.equipment())
            player_->RemoveAurasCastedBy(item);
    }

    if (dst.equipment()) // Apply...
    {
        // Item enchantments (this includes gems, but not meta-gems)
        player_->ApplyEnchantment(item, true, dst, true);
        // Raw stats (e.g., armor, agility, haste rating)
        player_->_ApplyItemBonuses(item, true, dst);
        // Wepon-related effects (such as critical strike for that weapon type)
        if (dst_attack_type < MAX_ATTACK)
            player_->_ApplyWeaponDependentAuraMods(
                item, static_cast<WeaponAttackType>(dst_attack_type), true);
        // Item-set
        if (proto->ItemSet)
            AddItemsSetItem(player_, item);
        // Make gems in item appear visually for other players inspecting us
        if (proto->Socket[0].Color != 0)
            item->visualize_gems(player_);
        if (dst.ranged())
            player_->_ApplyAmmoBonuses();
    }

    // Equipping to a weapon slot in combat triggers the global cooldown &
    // resets swing timer
    if (player_->isInCombat() &&
        (dst.main_hand() || dst.off_hand() || dst.ranged()))
    {
        // Global cooldown
        uint32 combat_swap_spell =
            player_->getClass() == CLASS_ROGUE ? 6123 : 6119;
        if (const SpellEntry* spell_info =
                sSpellStore.LookupEntry(combat_swap_spell))
        {
            uint32 gcd = spell_info->StartRecoveryTime;
            player_->GetGlobalCooldownMgr().AddGlobalCooldown(spell_info, gcd);

            // Then in standard Mangos-fashion we also need to send the cooldown
            // packet ourselves
            WorldPacket data(SMSG_SPELL_COOLDOWN, 16);
            data << player_->GetObjectGuid();
            data << uint8(1);
            data << uint32(combat_swap_spell);
            data << uint32(0);
            player_->GetSession()->send_packet(std::move(data));
        }
        // Reset swing timers
        player_->resetAttackTimer(BASE_ATTACK);
        player_->resetAttackTimer(OFF_ATTACK);
        player_->resetAttackTimer(RANGED_ATTACK);
    }

    // Remove self-cast, postivie auras that no longer meet equipment
    // requirements
    if (src.equipment() && !dst.equipment())
        player_->UpdateEquipmentRequiringAuras();

    if (update_meta_gems)
        player_->update_meta_gem();
}

void inventory::personal_storage::on_item_added(uint32 id, uint32 count) const
{
    // We need to iterate quests and see if this item is needed for any of them
    for (int i = 0; i < MAX_QUEST_LOG_SIZE; ++i)
    {
        uint32 quest_id = player_->GetQuestSlotQuestId(i);
        const Quest* quest_info;
        QuestStatusData* quest_status;
        if (quest_id == 0 ||
            (quest_status = &player_->getQuestStatusMap()[quest_id])
                    ->m_status != QUEST_STATUS_INCOMPLETE ||
            (quest_info = sObjectMgr::Instance()->GetQuestTemplate(quest_id)) ==
                nullptr ||
            !quest_info->HasSpecialFlag(QUEST_SPECIAL_FLAG_DELIVER))
            continue;

        for (int j = 0; j < QUEST_ITEM_OBJECTIVES_COUNT; ++j)
        {
            if (quest_info->ReqItemId[j] != id ||
                quest_status->m_itemcount[j] == quest_info->ReqItemCount[j])
                continue;
            uint32 added = count;
            if (quest_status->m_itemcount[j] + count >
                quest_info->ReqItemCount[j])
                added =
                    quest_info->ReqItemCount[j] - quest_status->m_itemcount[j];
            quest_status->m_itemcount[j] += added;
            if (quest_status->uState != QUEST_NEW)
                quest_status->uState = QUEST_CHANGED;

            // NOTE: The client handler for quest updates does this:
            // if (op == SMSG_QUESTUPDATE_ADD_ITEM) return;
            // In other words, that opcode is outdated and unnecessary to send
            // FIXME: figure out how to inform the client a quest item was added

            if (quest_status->m_itemcount[j] == quest_info->ReqItemCount[j] &&
                player_->CanCompleteQuest(quest_id))
                player_->CompleteQuest(quest_id);
        }
    }
    // player_->UpdateForQuestWorldObjects(); XXX: This was called in mangos'
    // Player::ItemAddedQuestCheck() -- why?? It updates GOs
}

void inventory::personal_storage::on_item_removed(uint32 id) const
{
    // We need to to iterate quests and see if any of them has changed due to
    // this removal
    for (int i = 0; i < MAX_QUEST_LOG_SIZE; ++i)
    {
        uint32 quest_id = player_->GetQuestSlotQuestId(i);
        const Quest* quest_info;
        if (quest_id == 0 ||
            (quest_info = sObjectMgr::Instance()->GetQuestTemplate(quest_id)) ==
                nullptr ||
            !quest_info->HasSpecialFlag(QUEST_SPECIAL_FLAG_DELIVER))
            continue;
        QuestStatusData& quest_status = player_->getQuestStatusMap()[quest_id];

        for (int j = 0; j < QUEST_ITEM_OBJECTIVES_COUNT; ++j)
        {
            if (quest_info->ReqItemId[j] != id)
                continue;
            uint32 count =
                item_count(id); // This is WITHOUT the items we just removed
            if (count >= quest_info->ReqItemCount[j])
                continue; // Nothing changed, count still bigger than required
            quest_status.m_itemcount[j] = count;
            if (quest_status.uState != QUEST_NEW)
                quest_status.uState = QUEST_CHANGED;
            if (quest_status.m_status == QUEST_STATUS_COMPLETE)
                player_->IncompleteQuest(quest_id);
        }
    }
    // player_->UpdateForQuestWorldObjects(); XXX: This was called in mangos'
    // Player::ItemAddedQuestCheck() -- why?? It updates GOs
}

void inventory::personal_storage::on_money_changed() const
{
    // We need to to iterate quests and see if any of them has changed due to
    // the change in money
    for (int i = 0; i < MAX_QUEST_LOG_SIZE; ++i)
    {
        uint32 quest_id = player_->GetQuestSlotQuestId(i);
        const Quest* quest_info;
        if (quest_id == 0 ||
            (quest_info = sObjectMgr::Instance()->GetQuestTemplate(quest_id)) ==
                nullptr ||
            quest_info->GetRewOrReqMoney() >= 0)
            continue;
        QuestStatusData& quest_status = player_->getQuestStatusMap()[quest_id];
        if (quest_status.m_status == QUEST_STATUS_COMPLETE &&
            copper_.get() <
                static_cast<uint32>(std::abs(quest_info->GetRewOrReqMoney())))
            player_->IncompleteQuest(quest_id);
        else if (quest_status.m_status == QUEST_STATUS_INCOMPLETE &&
                 copper_.get() >= static_cast<uint32>(std::abs(
                                      quest_info->GetRewOrReqMoney())) &&
                 player_->CanCompleteQuest(quest_id))
            player_->CompleteQuest(quest_id);
    }
}

InventoryResult inventory::personal_storage::remove_count(
    Item* item, uint32 count_to_remove)
{
    // Make sure that item is actually in our storage
    if (get(item->slot()) != item)
        return EQUIP_ERR_ITEM_NOT_FOUND;

    if (item->GetCount() <= count_to_remove)
    {
        // Removal of count causes the item to be deleted
        bool bank_slot = item->slot().bank() || item->slot().bank_bagslot() ||
                         item->slot().extra_bank_bag();
        inventory::transaction trans(true, inventory::transaction::send_self,
            inventory::transaction::add_loot, bank_slot);
        trans.destroy(item);
        if (!finalize(trans))
            return static_cast<InventoryResult>(trans.error());
    }
    else
    {
        // The item still exists, its count just went down
        InventoryResult err = can_delete_item(item->slot());
        if (err != EQUIP_ERR_OK)
            return err;
        item->SetCount(item->GetCount() - count_to_remove);
        item->mark_for_save();
        on_item_removed(item->GetEntry());
    }

    return EQUIP_ERR_OK;
}

bool inventory::personal_storage::can_reach_item(Item* item) const
{
    // Make sure that item is actually in our storage
    if (get(item->slot()) != item)
        return false;

    if (item->slot().bank() || item->slot().extra_bank_bag() ||
        item->slot().bank_bagslot())
    {
        if (!player_->GetNPCIfCanInteractWith(
                player_->last_interacted_banker(), UNIT_NPC_FLAG_BANKER))
            return false;
    }

    return true;
}

bool inventory::personal_storage::store_special_bag(
    inventory::slot& out, const ItemPrototype* prototype) const
{
    if (prototype->BagFamily == 0)
        return false;

    // Go through our bags and check if any of them are special.
    // We can only have one special bag at a time
    for (int i = bags_start; i < bags_end; ++i)
    {
        slot s = slot(personal_slot, main_bag, i);
        if (Item* bag = get(s))
            if (bag->GetProto()->SubClass != ITEM_SUBCLASS_CONTAINER &&
                ItemCanGoIntoBag(prototype, bag->GetProto()))
            {
                // Find the first available free slot in this bag (no need to
                // check for items to stack onto)
                const std::vector<Item*>& content = items_subscript(i);
                for (std::size_t j = 0; j < content.size(); ++j)
                {
                    s = slot(personal_slot, i, j);
                    if (get(s) == nullptr)
                    {
                        out = s;
                        return true;
                    }
                }
            }
    }

    return false;
}

inventory::slot inventory::personal_storage::first_empty_slot_for(
    Item* item) const
{
    // Check backpack
    for (int i = slot_start; i < slot_end; ++i)
    {
        if (items_subscript(main_bag)[i] == nullptr)
        {
            slot s = slot(personal_slot, main_bag, i);
            if (can_store(s, item, player_, true) == EQUIP_ERR_OK)
                return s;
        }
    }

    // Check extra bags
    for (int i = bags_start; i < bags_end; ++i)
    {
        if (items_subscript(main_bag)[i] !=
            nullptr) // Not NULL if we have an equipped bag in that slot
        {
            const std::vector<Item*>& content = items_subscript(i);
            for (size_t j = 0; j < content.size(); ++j)
                if (content[j] == nullptr)
                {
                    slot s = slot(personal_slot, i, j);
                    if (can_store(s, item, player_, true) == EQUIP_ERR_OK)
                        return s;
                }
        }
    }

    return slot();
}

Item* inventory::personal_storage::stack_onto(Item* source, Item* target)
{
    assert(source->GetEntry() == target->GetEntry());

    int free_slots = (int)target->GetMaxStackCount() - (int)target->GetCount();
    if (free_slots > 0)
    {
        if ((int)source->GetCount() <= free_slots)
        {
            target->SetCount(target->GetCount() + source->GetCount());
            pop_item(source, false);
            deleted_items_[source->GetGUIDLow()] =
                std::make_pair(source, storage::delete_destroy);
            target->mark_for_save();
            return nullptr;
        }
        else
        {
            target->SetCount(target->GetCount() + free_slots);
            target->mark_for_save();
            source->SetCount(source->GetCount() - free_slots);
            source->mark_for_save();
        }
    }

    return source;
}

void inventory::personal_storage::auto_store_item(Item* item, bool bank)
{
    int begin = bank ? bank_slot_start : slot_start;
    int end = bank ? bank_slot_end : slot_end;
    bool stackable = item->GetMaxStackCount() > 1;
    uint32 bag_family = item->GetProto()->BagFamily;

    slot first_empty, perfect_empty;

    // Go through main bag
    for (int i = begin; i < end; ++i)
    {
        auto inner = items_subscript(main_bag)[i];

        // Stack onto other item
        if (stackable && inner && inner->GetEntry() == item->GetEntry())
            if (stack_onto(item, inner) == nullptr)
                return; // Stacked fully

        // Already found a perfect empty slot? Onto next iteration it is!
        if (perfect_empty)
            continue;

        // Save empty slot in case we need one at the end
        if (!inner)
        {
            if (bag_family == 0)
            {
                auto s = slot(personal_slot, main_bag, i);
                if (can_store(s, item, player_, true) == EQUIP_ERR_OK)
                {
                    perfect_empty = s;
                    if (!stackable)
                        goto found_perfect;
                }
            }
            // NOTE: Maing bag can never be a perfect match for a bag-family
            //       item
            else if (!first_empty)
            {
                auto s = slot(personal_slot, main_bag, i);
                if (can_store(s, item, player_, true) == EQUIP_ERR_OK)
                    first_empty = s;
            }
        }
    }

    begin = bank ? bank_bags_start : bags_start;
    end = bank ? bank_bags_end : bags_end;
    // Go through extra bags
    for (int i = begin; i < end; ++i)
    {
        auto bag = items_subscript(main_bag)[i];
        if (bag == nullptr)
            continue;
        const std::vector<Item*>& content = items_subscript(i);
        for (size_t j = 0; j < content.size(); ++j)
        {
            auto inner = content[j];

            // Stack onto other item
            if (stackable && inner && inner->GetEntry() == item->GetEntry())
                if (stack_onto(item, inner) == nullptr)
                    return; // Stacked fully

            // Already found a perfect empty slot? Onto next iteration it is!
            if (perfect_empty)
                continue;

            // Save empty slot in case we need one at the end
            if (!inner)
            {
                if (bag_family == 0)
                {
                    auto s = slot(personal_slot, i, j);
                    if (can_store(s, item, player_, true) == EQUIP_ERR_OK)
                    {
                        perfect_empty = s;
                        if (!stackable)
                            goto found_perfect;
                    }
                }
                else
                {
                    auto s = slot(personal_slot, i, j);
                    if (bag->GetProto()->BagFamily & bag_family)
                    {
                        if (can_store(s, item, player_, true) == EQUIP_ERR_OK)
                        {
                            perfect_empty = s;
                            if (!stackable)
                                goto found_perfect;
                        }
                    }
                    else if (!first_empty)
                    {
                        if (can_store(s, item, player_, true) == EQUIP_ERR_OK)
                            first_empty = s;
                    }
                }
            }
        }
    }

    if (!first_empty && !perfect_empty)
    {
        if (bank)
            player_->SendEquipError(EQUIP_ERR_BANK_FULL, item);
        else
            player_->SendEquipError(EQUIP_ERR_INVENTORY_FULL, item);
        return;
    }

found_perfect:
    swap(perfect_empty ? perfect_empty : first_empty, item->slot());
}

// TODO:
// This is used for ItemHandler::HandleBuyItemInSlotOpcode, this is
// functionality
// that we did not consider when creating our transaction, and should be worked
// in later
InventoryResult inventory::personal_storage::add_to_slot(
    uint32 id, uint32 count, slot s)
{
    // Errors that happen in here all respond with:
    // "No equipment slot is available for that item."
    // at retail, which makes no sense at all. We choose our own errors instead

    // Only our inventory can be the target of this operation
    if (!s.backpack() && !s.extra_bag())
        return EQUIP_ERR_ITEM_DOESNT_GO_TO_SLOT;

    const ItemPrototype* prototype = ObjectMgr::GetItemPrototype(id);
    if (!prototype)
        return EQUIP_ERR_INT_BAG_ERROR;

    uint32 max_stack_size = prototype->Stackable;

    // Can't get more than one stack through this method
    if (count > max_stack_size)
        return EQUIP_ERR_ITEM_DOESNT_GO_TO_SLOT;

    // The target slot must either be empty, or the same id with a resulting
    // count less than max count
    if (Item* item = get(s))
    {
        if (item->GetEntry() != id)
            return EQUIP_ERR_ITEM_CANT_STACK;
        if (item->GetCount() + count > max_stack_size)
            return EQUIP_ERR_ITEM_CANT_STACK;
    }

    // We use a transaction to verify we can get this item & count at all
    transaction trans;
    trans.add(id, count);
    if (!verify(trans))
        return static_cast<InventoryResult>(trans.error());

    // Operation has been verfied, go through with it
    if (Item* item = get(s))
    {
        item->SetCount(item->GetCount() + count);
        item->mark_for_save();
    }
    else
    {
        std::unique_ptr<Item> new_item(Item::CreateItem(id, count, player_));
        if (!new_item)
            return EQUIP_ERR_INT_BAG_ERROR;

        InventoryResult res = can_store(s, id, count, true);
        if (res != EQUIP_ERR_OK)
            return res;

        Item* i = new_item.release();
        put_item(i, s, false);
        send_received_item(player_, i, id, count, true, false, false);
    }

    return EQUIP_ERR_OK;
}

void inventory::destroy_conjured_items(personal_storage& s)
{
    inventory::transaction trans;

    for (auto itr = s.begin(personal_storage::iterator::all_body);
         itr != s.end(); ++itr)
    {
        Item* item = *itr;
        if (item->GetProto()->Flags & ITEM_FLAG_CONJURED)
            trans.destroy(item);
    }

// there should be no possibility for this finalize to fail as all
// items added have been verified to be in the player's inventory
#ifndef NDEBUG
    bool b = s.finalize(trans);
#else
    s.finalize(trans);
#endif
    assert(b && "assumption in inventory::destroy_conjured_items failed");
}
