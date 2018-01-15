#ifndef MANGOS__GAME__INVENTORY__TRANSACTION_H
#define MANGOS__GAME__INVENTORY__TRANSACTION_H

#include "Common.h"
#include "inventory/copper.h"
#include "inventory/slot.h"
#include <utility>
#include <vector>

class Item;
// XXX: Transaction are only used for the personal_storage, we should remove
// their intended support for the guild_storage

namespace inventory
{
class storage;

class transaction
{
    friend class personal_storage;
    friend class guild_storage;

public:
    enum send_result
    {
        send_none,
        send_self,
        send_party
    };
    enum add_method
    {
        add_loot,
        add_vendor,
        add_craft
    };

    // If include_bank is true it is ONLY included for deleting. You can never
    // directly add to a bank.
    transaction(bool user_action = true, send_result r = send_self,
        add_method m = add_loot, bool include_bank = false)
      : error_(0), finalized_(false), storage_stage_(0),
        user_action_(user_action), send_result_(r), add_method_(m),
        include_bank_(include_bank), finalized_gold_(0)
    {
    }

    uint32 error() const { return error_; }
    bool empty() const { return actions_.empty(); }

    transaction& add(Item* item);
    transaction& add(uint32 id, uint32 count, uint32 rand_prop = 0);
    transaction& add(copper c);

    transaction& remove(Item* item);
    transaction& remove(uint32 id, uint32 count);
    transaction& remove(copper c);

    transaction& destroy(Item* item);
    transaction& destroy(uint32 id, uint32 count);
    transaction& destroy(copper c) { return remove(c); }

    bool has(Item* item, int* op = nullptr) const;

    // Once storage::finalize() has been called, returning true, these vectors
    // are populated with all item pointers that we've touched (added,
    // modified, or removed from). (Note: Destroys are not included.)
    const std::vector<Item*>& added_items() const { return added_items_; }
    const std::vector<Item*>& removed_items() const { return removed_items_; }

    // This vector is guaranteed to have as many entries as transaction::add()
    // calls you have done
    // For every item or copper you have added, it holds how many of them failed
    // to add. This vector
    // is only valid after this transaction was finalized or verified.
    // example:
    //     transaction t;
    //     t.add(copper(100));
    //     t.add(by_item_ptr);
    //     t.add(by_item_id, 10);
    //     if (!storage.finalize(t))
    //     {
    //         t.add_failures()[0]; // holds how many copper we failed to add
    //         because we are at the gold limit
    //         t.add_failures()[1]; // holds how many of the by_item_ptr item we
    //         failed to add, note that this
    //                              // can be more than 1 if the item object
    //                              represents a stack
    //         t.add_failures()[2]; // holds how many of the by_item_id item we
    //         failed to add
    //     }
    const std::vector<uint32>& add_failures() const { return add_failures_; }

private:
    enum operation
    {
        op_add,
        op_remove,
        op_destroy
    };
    struct operand
    {
        enum op_type
        {
            by_item,
            by_id,
            by_gold
        };
        op_type type;
        union
        {
            struct
            {
                Item* item;
            };
            struct
            {
                uint32 id;
                uint32 count;
                uint32 rand_prop;
            };
            struct
            {
                uint32 gold;
            };
        };
    };
    typedef std::pair<operation, operand> action;

    std::vector<action> actions_;
    std::vector<Item*> added_items_;
    std::vector<Item*> removed_items_;
    std::vector<uint32> add_failures_;
    uint32 error_;
    bool finalized_;
    uint32 storage_stage_; // Used by the storage to know if verify() needs to
                           // be called in finalize()
    bool user_action_; // If true stricter checking is applied. Should ALWAYS be
                       // used when the action is a user-initiated one
    send_result send_result_; // To whom add()s should be reported
    add_method
        add_method_;    // What type of add this was (received, crafted, etc)
    bool include_bank_; // If bank is to be included for deletions or not

    // Finalize data. storage::verify() writes this into us so that
    // storage::finalize() can use it
    enum finalize_action_types
    {
        fa_remove,
        fa_destroy,
        fa_mod_stack,
        fa_create_item,
        fa_set_item_ptr,
        fa_drop_item_ptr
    };
    struct finalize_action
    {
        finalize_action(finalize_action_types t, slot s)
          : type(t), target_slot(std::move(s))
        {
        } // No-data constructor
        finalize_action(finalize_action_types t, slot s, uint32 i, uint32 c)
          : type(t), target_slot(std::move(s)), id(i), count(c)
        {
        }
        finalize_action(
            finalize_action_types t, slot s, uint32 i, uint32 c, uint32 p)
          : type(t), target_slot(std::move(s)), id(i), count(c), prop(p)
        {
        }
        finalize_action(finalize_action_types t, slot s, Item* i)
          : type(t), target_slot(std::move(s)), item(i)
        {
        }
        finalize_action_types type; // Sort of action to do
        slot target_slot;           // Target slot
        union                       // Action data
        {
            struct
            {
                uint32 id;
                uint32 count;
                uint32 prop;
            };
            struct
            {
                Item* item;
            };
        };
    };
    // Finalize actions are ordered so that delete and remove are first, and add
    // afterwards.
    // They are presented in the order ::verify() put them, and ::finalize()
    // will run
    // them in the same order, creating the same effect.
    typedef std::vector<finalize_action> finalize_vector;
    finalize_vector finalize_actions_;
    copper finalized_gold_; // The resulting gold after the transaction
                            // (absolute value)
};

} // namespace inventoy

#endif // MANGOS__GAME__INVENTORY__TRANSACTION_H
