#ifndef MANGOS__GAME__INVENTORY__STORAGE_H
#define MANGOS__GAME__INVENTORY__STORAGE_H

#include "Common.h"
#include "Item.h"
#include "inventory/copper.h"
#include "inventory/slot.h"
#include "inventory/transaction.h"
#include <map>
#include <vector>

class Player;
class QueryResult;

namespace inventory
{
class storage
{
public:
    storage() : copper_(0) {}
    virtual ~storage();

    // Error types differ depending on storage type
    // personal_storage returns values from the InventoryType enum
    // guild_storage returns values from the CommandError enum
    // virtual uint32 swap(slot dst, slot src);

    Item* get(slot s) const;

    copper money() const { return copper_; }
    void money(copper c) { copper_ = c; }

    virtual bool verify(transaction& trans) const;
    virtual bool finalize(transaction& trans);

    uint32 item_count(uint32 item_id, bool exclude_bank = false,
        Item* exclude_item = nullptr, bool exclude_buyback = false) const;

    // std::vector<uint8> available_bags: extra bags for personal_storage, tab
    // ids for guild bank. Order matters (e.g. to search backpack first, 255
    // should be first)
    // if available_bags contains main_bag (255) the boolean bank decideds if we
    // search the bank or the backpack (& keyrings)
    std::vector<slot> autostore_slots(Player* player, Item* item, uint32 count,
        std::vector<uint8> available_bags, bool bank = false) const;

protected:
    enum delete_state
    {
        delete_remove,
        delete_destroy
    };
    typedef std::map<uint8 /*bag*/, std::vector<Item*>> storage_map;
    typedef std::pair<Item*, delete_state> delete_map_entry;
    typedef std::map<uint32 /*guid*/, delete_map_entry> delete_map;

    virtual slot_type get_slot_type() const = 0;
    virtual InventoryResult can_store(
        slot dst, Item* item, Player* player, bool skip_unique_check) const = 0;
    bool has_item(Item* item) const;

    void send_received_item(Player* player, Item* item, uint32 item_id,
        uint32 count, bool from_npc, bool created, bool broadcast);

    // This function is added to not litter the code with find calls when you
    // are certain that a bag exist.
    // NOTE: You can only use this one when you're 100% sure the bag exists
    // Using it for a non-existant bag will cause an exception to be thrown
    // FIXME: Replace with std::map::at when we have C++11
    const std::vector<Item*>& items_subscript(uint8 bag) const;

    storage_map items_;
    delete_map deleted_items_; // Items that have been removed/destroyed and
                               // need cleanup on saving
    copper copper_;
};

// err_type storage_swap(storage& dst_storage, slot dst_slot, storage&
// src_storage, slot src_slot);

} // namespace inventory
#endif // MANGOS__GAME__INVENTORY__STORAGE_H
