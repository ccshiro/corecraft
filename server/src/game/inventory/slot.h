#ifndef MANGOS__GAME__INVENTORY__SLOT_H
#define MANGOS__GAME__INVENTORY__SLOT_H

#include "Common.h"
#include "SharedDefines.h"

struct ItemPrototype;

namespace inventory
{
// TODO: introduce enum classes when C++11 becomes possible
// some enums are commented out due to lack of scoping

enum bag_id : uint8
{
    // The main bag is used for:
    // - Main inventory bag
    // - 19 equipment slots
    // - 28 main bank slots
    // - 4 bag slots
    // - 7 bank bag slots
    // - 12 vendor buyback slots
    // - 32 keyring slots
    main_bag = 255,

    // 4 extra bags
    bags_start = 19,
    bags_end = 23,

    // 7 extra bank bags
    bank_bags_start = 67,
    bank_bags_end = 74
};

// The main bag is special (see enum bag_id) and slots have
// special meanings
enum main_bag_index : uint8
{
    // 19 equipment slots
    equipment_start = 0,
    head = 0,
    neck = 1,
    shoulders = 2,
    shirt = 3,
    chest = 4,
    waist = 5,
    legs = 6,
    feet = 7,
    wrists = 8,
    hands = 9,
    finger1 = 10,
    finger2 = 11,
    trinket1 = 12,
    trinket2 = 13,
    back = 14,
    main_hand_e = 15,
    off_hand_e = 16,
    ranged_e = 17,
    tabard = 18,
    equipment_end = 19,

    // 4 bag slots (SEE TODO AT TOP)
    // bags_start      = 19,
    // bags_end        = 23,

    // 16 actual slots in the main bag
    slot_start = 23,
    slot_end = 39,

    // 28 main bank slots
    bank_slot_start = 39,
    bank_slot_end = 67,

    // 7 bank bag slots (SEE TODO AT TOP)
    // bank_bags_start = 67,
    // bank_bags_end   = 74,

    // 12 buyback slots
    buyback_start = 74,
    buyback_end = 86,

    // 32 keyring slots
    keyring_start = 86,
    keyring_end = 118,

    main_bag_end = 118
};

enum slot_type
{
    personal_slot,
    guild_slot,
    invalid_slot
};

const uint8 max_bag_size = 36;
const uint8 max_guild_tab = 6;
const uint8 max_guild_tab_size = 98;

class slot
{
public:
    // creates an invalid slot that is guaranteed to fail every function called
    // on it
    slot() : bag_(0), index_(255), type_(invalid_slot) {}

    slot(slot_type t, uint8 bag, uint8 index)
      : bag_(bag), index_(index), type_(t)
    {
    }

    explicit operator bool() const { return valid(); }

    uint8 bag() const { return bag_; }

    uint8 index() const { return index_; }

    // returns true if the slot is a personal slot, false if it's a guild slot
    bool personal() const { return type_ == personal_slot; }

    // returns true if the bag/index is possibly valid
    // makes no guarantee about if the underlying storage has the slot available
    bool valid() const
    {
        switch (type_)
        {
        case personal_slot:
            return valid_personal();
        case guild_slot:
            return valid_guild();
        default:
            return false;
        }
    }

    bool valid_guild() const
    {
        return bag_ < max_guild_tab && index_ < max_guild_tab_size;
    }

    bool valid_personal() const
    {
        if (!valid_bag())
            return false;

        if (bag_ == main_bag)
            return index_ < main_bag_end;
        else
            return index_ < max_bag_size;
    }

    // True if item is equipped on a character's body, as opposed to in a bag,
    // for example
    bool on_body() const
    {
        if (!personal() || !valid())
            return false;
        return backpack() || keyring() || bagslot() || extra_bag() ||
               equipment();
    }

    // Returns true when the bag id is possibly valid
    // does not guarantee that the player has a bag in that bagslot
    // this function is only valid for personal slots
    bool valid_bag() const
    {
        if (bag_ == main_bag)
            return true;
        if (bag_ >= bags_start && bag_ < bags_end)
            return true;
        if (bag_ >= bank_bags_start && bag_ < bank_bags_end)
            return true;
        return false;
    }

    // returns true when the slot is in the keyring
    // does not guarantee that the slot is accessible to the player
    // this function is only valid for personal slots
    bool keyring() const
    {
        if (bag_ == main_bag && index_ >= keyring_start && index_ < keyring_end)
            return true;
        return false;
    }

    // returns true if the slot is in the main bag ("backpack")
    // this function is only valid for personal slots
    bool backpack() const
    {
        if (bag_ == main_bag && index_ >= slot_start && index_ < slot_end)
            return true;
        return false;
    }

    // returns true if the slot is a bag slot
    // this function is only valid for personal slots
    bool bagslot() const
    {
        if (bag_ == main_bag && index_ >= bags_start && index_ < bags_end)
            return true;
        return false;
    }

    // returns true if the slot is a bank bag slot
    // this function is only valid for personal slots
    bool bank_bagslot() const
    {
        if (bag_ == main_bag && index_ >= bank_bags_start &&
            index_ < bank_bags_end)
            return true;
        return false;
    }

    // returns true if the slot is in one of the 4 extra bag slots
    // guarantees the index in the bag is not higher than the maximum bag size
    // does not guarantee the index in that bag slot is valid
    // does not guarantee that the bag is actually equipped
    // this function is only valid for personal slots
    bool extra_bag() const
    {
        if (bag_ >= bags_start && bag_ < bags_end && index_ < max_bag_size)
            return true;
        return false;
    }

    // returns true if the slot is in the main bank
    // this function is only valid for personal slots
    bool main_bank() const
    {
        if (bag_ == main_bag && index_ >= bank_slot_start &&
            index_ < bank_slot_end)
            return true;
        return false;
    }

    // returns true if the slot is in the extra bank bags
    // guarantees the index in the bag is not higher than the maximum bag size
    // does not guarantee the index in that bag slot is valid
    // does not guarantee that the bag is actually equipped
    // this function is only valid for personal slots
    bool extra_bank_bag() const
    {
        if (bag_ >= bank_bags_start && bag_ < bank_bags_end &&
            index_ < max_bag_size)
            return true;
        return false;
    }

    // returns true if the slot is an equipment slot
    // this function is only valid for personal slots
    bool equipment() const
    {
        if (bag_ == main_bag && index_ < equipment_end)
            return true;
        return false;
    }

    // returns true if the slot is a buyback slot
    // this function is only valid for personal slots
    bool buyback() const
    {
        if (bag_ == main_bag && index_ >= buyback_start && index_ < buyback_end)
            return true;
        return false;
    }

    bool main_hand() const { return bag_ == main_bag && index_ == main_hand_e; }

    bool off_hand() const { return bag_ == main_bag && index_ == off_hand_e; }

    bool ranged() const { return bag_ == main_bag && index_ == ranged_e; }

    bool bank() const
    {
        return main_bank() || bank_bagslot() || extra_bank_bag();
    }

    // returns WeaponAttackType or MAX_ATTACK
    int attack_type() const
    {
        if (main_hand())
            return BASE_ATTACK;
        else if (off_hand())
            return OFF_ATTACK;
        else if (ranged())
            return RANGED_ATTACK;
        else
            return MAX_ATTACK;
    }

    // returns true if the slot can hold the item
    // this function is only valid for personal slots
    bool can_hold(const ItemPrototype* prototype) const;

    bool operator==(const slot& rhs) const
    {
        return bag_ == rhs.bag_ && index_ == rhs.index_ && type_ == rhs.type_;
    }

    bool operator!=(const slot& rhs) const { return !operator==(rhs); }

private:
    uint8 bag_;
    uint8 index_;
    uint8 type_;
};

// Function Object to sorts slots in order you wish to add to them. Which means
// that if you have a "std::set<slot, slot_sort_cmp> slots" then *slots.begin()
// is a more attractive place to store into than *++slots.begin()
struct slot_sort_cmp
{
    bool operator()(const slot& x, const slot& y) const
    {
        // XXX: Below only applies for personal slots

        if (x.bag() == y.bag())
        {
            return x.index() < y.index();
        }

        // main_bag is always to be considered less than any other bag
        if (x.bag() == main_bag)
            return true;
        if (y.bag() == main_bag)
            return false;

        // For extra bags & extra bank bags
        return x.bag() < y.bag();
    }
};

} // namespace inventory

#endif // MANGOS__GAME__INVENTORY__SLOT_H
