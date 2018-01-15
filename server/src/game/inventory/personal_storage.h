#ifndef MANGOS__GAME__INVENTORY__PERSONAL_STORAGE_H
#define MANGOS__GAME__INVENTORY__PERSONAL_STORAGE_H

#include "Common.h"
#include "inventory/storage.h"
#include "inventory/transaction.h"
#include <iterator>
#include <map>
#include <stdexcept>
#include <vector>

class Item;
struct ItemPrototype;
class Player;
class QueryResult;

namespace inventory
{
class personal_storage : public storage
{
    friend class iterator;
    friend class guild_storage; // guild_storage needs to be able to change our
                                // content since it has no state and cannot be a
                                // part of normal saving
public:
    // Note: Iterator does not meet all forward_iterator properties, as some are
    // non-sensical for us.
    // More precisely:
    //                - It's not default-constructible
    //                - "*a == *b; *++a == *++b;" only holds true if a.mask ==
    //                b.mask
    //                - *a is valid, but a-> is not
    // Note2: An iterator only stops over valid items. As long as the iterator
    // is not == end()
    // using the dereferenced item without checking against NULL is defined
    // behavior.
    class iterator
        : public std::iterator<std::forward_iterator_tag, inventory::slot>
    {
    public:
        enum
        {
            equipment = 0x0001,
            inventory = 0x0002,
            bank = 0x0004,
            keyring = 0x0008,
            bags = 0x0010,
            buy_back = 0x0020,

            all_body = equipment | inventory | keyring | bags,
            all_personal = equipment | inventory | bank | keyring | bags,
            all = equipment | inventory | bank | keyring | bags | buy_back,
        };

        iterator(slot s, int mask, const personal_storage* stor)
          : mask_(mask), slot_(std::move(s)), storage_(stor)
        {
            // Default slot() represents an invalid slot, and is also how we
            // denote our end iterator
            if (!slot_valid() && slot_ != slot())
                next_slot();
        }
        iterator(const iterator& rhs)
          : mask_(rhs.mask_), slot_(rhs.slot_), storage_(rhs.storage_)
        {
        }
        iterator& operator=(const iterator& rhs)
        {
            mask_ = rhs.mask_;
            slot_ = rhs.slot_;
            storage_ = rhs.storage_;
            return *this;
        }

        Item* operator*()
        {
            auto itr = storage_->items_.find(slot_.bag());
            if (itr == storage_->items_.end())
                throw std::runtime_error(
                    "personal_storage::iterator: dereferenced end iterator");
            return itr->second[slot_.index()];
        }
        bool operator==(const iterator& rhs) const
        {
            return slot_ == rhs.slot_;
        }
        bool operator!=(const iterator& rhs) const { return !operator==(rhs); }
        iterator& operator++()
        {
            next_slot();
            return *this;
        }
        iterator operator++(int)
        {
            iterator temp(*this);
            ++*this;
            return temp;
        }

    private:
        bool slot_valid() const; // Checks if slot is valid for our mask
        void next_slot();

        int mask_;
        slot slot_;
        const personal_storage* storage_;
    };

    personal_storage(Player* player);

    // Returns items that did not get loaded properly. Advised action: send them
    // by mail.
    std::vector<Item*> load(
        QueryResult* result, uint32 money, uint32 time_since_logout);

    // CharacterDatabase.BeginTransaction() must be called before save()
    // And CharacterDatabase.CommitTransaction() must be called after,
    // rollback should not be called, as saving changes the state of the
    // storage.
    void save();

    bool verify(transaction& trans) const override;
    bool finalize(transaction& trans) override;

    // The iterators are const as they cannot change our inventory. Any change
    // to a dereference
    // iterator (the item) is acceptable, with the exception of deleting the
    // resources.
    iterator begin(int mask = iterator::all) const
    {
        return iterator(slot(personal_slot, main_bag, 0), mask, this);
    }
    iterator end() const { return iterator(slot(), 0, this); }

    InventoryResult swap(slot dst, slot src, uint32 src_count = 0);
    slot find_auto_equip_slot(slot src) const;
    // main_bag looks only in backpack for this function
    slot find_empty_slot_in_bag(uint8 bag, bool bank = false) const;

    InventoryResult can_delete_item(slot s, bool user_action = true) const;

    // -1 means success, for errors see the Sell/Buy Result enum
    int sell_item(slot src, uint32 src_split = 0);
    void buyback_item(slot src);

    InventoryResult remove_count(Item* item, uint32 count_to_remove);
    // The core is able to reach items in the bank even when not near the bank,
    // a player action cannot do that however. This function checks if a player
    // is in a reachable distance to an item slot
    bool can_reach_item(Item* item) const;

    // Finds first empty-slot that item fits into. Checks backpack & extra bags.
    slot first_empty_slot_for(Item* item) const;

    // Stack onto item (must be same id and stackable!). Returns nullptr if
    // item disappeared into target, or otherwise pointer to source.
    // REMARK: Both items must already be in inventory
    Item* stack_onto(Item* source, Item* target);

    // Auto-store item in the most compact manner, stacking onto other such
    // items and prioritizing special bags.
    // bank: stores in personal bank if true, in inventory otherwise
    // REMARK: Item must already be in inventory
    void auto_store_item(Item* item, bool bank);

    // TODO:
    // This is used for ItemHandler::HandleBuyItemInSlotOpcode, this is
    // functionality
    // that we did not consider when creating our transaction, and should be
    // worked in later
    // Adds an item to the specified slot, if possible
    InventoryResult add_to_slot(uint32 id, uint32 count, slot s);

private:
    typedef std::pair<slot, InventoryResult> add_slot;

    InventoryResult can_store(slot dst, Item* item, Player* player,
        bool skip_unique_check) const override;
    InventoryResult can_store(
        slot dst, uint32 item_id, uint32 count, bool skip_unique_check) const;
    slot_type get_slot_type() const override { return personal_slot; }

    uint32 equipped_count(uint32 item_id, Item* exclude = nullptr) const;
    void save_storage_item(Item* item, slot item_slot);
    void put_item(Item* item, slot dst, bool swap);
    void pop_item(Item* item, bool swap);

    // Transaction helper functions. Used by ::verify()
    std::pair<bool, InventoryResult> process_iterating_deletion(
        transaction& trans, slot current_slot,
        std::vector<transaction::action>& delete_actions,
        std::map<uint32, std::vector<slot>>& targets,
        std::set<slot, slot_sort_cmp>& empty_slots,
        std::map<slot, uint32, slot_sort_cmp>& overwritten, copper& gold) const;
    InventoryResult process_deletions(transaction& trans,
        std::vector<transaction::action>& delete_actions,
        std::map<uint32, std::vector<slot>>& targets,
        std::set<slot, slot_sort_cmp>& empty_slots,
        std::map<slot, uint32, slot_sort_cmp>& overwritten, copper& gold) const;
    InventoryResult process_adds(transaction& trans,
        std::map<uint32, std::vector<slot>>& targets,
        std::set<slot, slot_sort_cmp>& empty_slots,
        std::map<slot, uint32, slot_sort_cmp>& overwritten, copper& gold) const;
    bool store_special_bag(
        inventory::slot& out, const ItemPrototype* prototype) const;

    // Checks if the item can't be equipped due to unique gems
    bool gems_unique_equipped_check(Item* equipping, inventory::slot dst) const;

    InventoryResult can_equip_bag(Item* bag, inventory::slot dst) const;

    // src_item is non-null when a swap happens (where item goes from src to
    // dst, and what was in dst goes to src)
    void on_item_relocation(Item* item, slot dst, slot src) const;
    void on_item_added(uint32 id, uint32 count) const;
    void on_item_removed(uint32 id) const;
    void on_money_changed() const;

    Player* player_;       // Player that owns this storage
    uint32 storage_stage_; // Each modification to the storage increases this
                           // number. Used to know if a transaction verify() is
                           // still valid
    bool loading_;
};

void destroy_conjured_items(personal_storage& s);

} // namespace inventory
#endif // MANGOS__GAME__INVENTORY__PERSONAL STORAGE_H
