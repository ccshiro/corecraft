#ifndef GAME__BUFF_STACKING_H
#define GAME__BUFF_STACKING_H

#include "Item.h"
#include "Player.h"
#include "SpellAuraDefines.h"
#include "SpellAuras.h"
#include "SpellMgr.h"
#include "Platform/Define.h"
#include <map>
#include <set>
#include <utility>

struct SpellEntry;
class Unit;

// DB Stacking Tables:
//
// `buffstack_groups`
// `id`: id of group
// `default`: selects the default case where no rules defined (BS_DEF_*)
// `name`: descriptive name of the group
// `comment`: explanation of its purpose
//
// `buffstack_group_spells`
// `gid`: buffstack_groups.id
// `sid`: first spell id in spell chain
// `comment`: comment
//
// `buffstack_rules`
// `id_one`: spell_dbc.Id or -buff_stack_groups.id
// `id_two`: spell_dbc.Id or -buff_stack_groups.id
// `stacks`: (BS_STACKS_*)

enum BuffStackDefults
{
    BS_DEF_NORMAL = 0, // Use normal buff stacking rules after DB rules
    BS_DEF_ALL = 1,    // Stacks with everything, unless DB overrides exist
};

enum BuffStackStacks
{
    BS_STACKS_OK = 0, // Buffs stack

    // Do not stack, resolution types:
    BS_STACKS_COLLISION_STRENGTH = 1,
    BS_STACKS_COLLISION_DURATION = 2,
    BS_STACKS_COLLISION_RANK = 3,

    BS_STACKS_IGNORE = 0xFF, // Not a DB value
};

// This class allows you to treat auras that are of the same type as different
// if they do indeed differ based on their misc values. Makes logic more simple
class logical_aura
{
public:
    logical_aura(const SpellEntry* info, SpellEffectIndex index,
        int32 scaled_base_points = 0)
      : type_(static_cast<AuraType>(info->EffectApplyAuraName[index])),
        spec_(static_cast<SpellSpecific>(info->SpellSpecific)),
        misc_(static_cast<AuraType>(info->EffectMiscValue[index])),
        base_points_(scaled_base_points), index_(index), info_(info)
    {
    }

    const SpellEntry* info() const { return info_; }
    SpellEffectIndex index() const { return index_; }
    bool positive() const { return IsPositiveSpell(info_); }
    int32 base_points() const
    {
        if (ignore_basepoints())
            return 0;
        return base_points_ ? base_points_ :
                              (info_->EffectBasePoints[index_] + 1);
    } // Base points in the SpellEntry are -1

    // Comparison operators
    bool operator==(const logical_aura& rhs) const
    {
        return type_ == rhs.type_ && misc_equal(rhs.misc_) &&
               specific_equal(rhs.spec_);
    }
    bool operator<(const logical_aura& rhs) const
    {
        if (type_ < rhs.type_)
            return true;
        else if (type_ > rhs.type_)
            return false;
        if (!misc_equal(rhs.misc_))
            return misc_ < rhs.misc_;
        if (!specific_equal(rhs.spec_))
            return spec_ < rhs.spec_;
        return false;
    }
    bool operator!=(const logical_aura& rhs) const { return *this != rhs; }
    bool operator>(const logical_aura& rhs) const
    {
        return !(*this < rhs || *this == rhs);
    }
    bool operator<=(const logical_aura& rhs) const
    {
        return *this < rhs || *this == rhs;
    }
    bool operator>=(const logical_aura& rhs) const
    {
        return *this > rhs || *this == rhs;
    }
    bool ignore_basepoints() const { return type_ == SPELL_AURA_MOD_STUN; }

private:
    AuraType type_;
    SpellSpecific spec_;
    uint32 misc_;
    int32 base_points_;
    SpellEffectIndex index_;
    const SpellEntry* info_;
    bool misc_equal(uint32 misc) const
    {
        switch (type_)
        {
        // Pure values
        case SPELL_AURA_MOD_STAT:
        case SPELL_AURA_TRACK_CREATURES:
        case SPELL_AURA_MECHANIC_IMMUNITY:
        case SPELL_AURA_ADD_FLAT_MODIFIER:
        case SPELL_AURA_ADD_PCT_MODIFIER:
        case SPELL_AURA_MOD_DAMAGE_PERCENT_TAKEN:
        case SPELL_AURA_SCHOOL_ABSORB: // These are bitmask values, but only
                                       // those that are exactly the same
                                       // bitmasks care about eachother
            return misc_ == misc;
        // Bitmask values
        case SPELL_AURA_MOD_RESISTANCE:
        case SPELL_AURA_MOD_DAMAGE_TAKEN:
        case SPELL_AURA_MOD_RESISTANCE_EXCLUSIVE:
        case SPELL_AURA_MOD_RATING:
            return (misc_ & misc) != 0;
        default:
            break;
        }
        // Misc irrelevant
        return true;
    }
    bool specific_equal(uint32 spec) const
    {
        // Foods and drinks must stack with auras without such specifics
        // For example, paladin's blessing of wisdom stacks with any water

        if (spec == 0 || spec_ == 0)
            return true;

        if (spec == spec_)
            return true;

        switch (spec)
        {
        case SPELL_FOOD:
            if (spec_ != SPELL_FOOD && spec_ != SPELL_FOOD_AND_DRINK)
                return false;
        case SPELL_DRINK:
            if (spec_ != SPELL_DRINK && spec_ != SPELL_FOOD_AND_DRINK)
                return false;
        case SPELL_FOOD_AND_DRINK:
            if (spec_ != SPELL_FOOD && spec_ != SPELL_DRINK &&
                spec_ != SPELL_FOOD_AND_DRINK)
                return false;
        default:
            break;
        }
        switch (spec_)
        {
        case SPELL_FOOD:
            if (spec != SPELL_FOOD && spec != SPELL_FOOD_AND_DRINK)
                return false;
        case SPELL_DRINK:
            if (spec != SPELL_DRINK && spec != SPELL_FOOD_AND_DRINK)
                return false;
        case SPELL_FOOD_AND_DRINK:
            if (spec != SPELL_FOOD && spec != SPELL_DRINK &&
                spec != SPELL_FOOD_AND_DRINK)
                return false;
        default:
            break;
        }

        // Spell spec irrelevant
        return true;
    }
};

// Function object for comparison if we're allowed to add our buff to the
// target.
class buff_stacks
{
public:
    buff_stacks(uint32 spell_id, Unit* caster, Unit* target,
        Item* cast_item = nullptr, AuraHolder* holder = nullptr);
    buff_stacks(const SpellEntry* spell_info, Unit* caster, Unit* target,
        Item* cast_item = nullptr, AuraHolder* holder = nullptr)
      : spell_info_(spell_info), caster_(caster), target_(target),
        cast_item_(cast_item), holder_(holder), duration_(0)
    {
        build_auras();
    }

    // The boolean indicates whether we can apply ourselves, the set are
    // the auras that needs to be removed in order for us to be added
    std::pair<bool, std::set<AuraHolder*>> operator()() const;

    // Does area aura stack with its own chain from multiple sources?
    static bool area_aura_stacks(uint32 spell_id);

private:
    // Data
    const SpellEntry* spell_info_;
    Unit* caster_;
    Unit* target_;
    Item* cast_item_;
    AuraHolder* holder_;
    int32 duration_;               // DiminishingReturn scaled value
    std::set<logical_aura> auras_; // Use an ordered container

    // Logic checks
    std::pair<bool, bool> handle_drink_buff(
        std::set<AuraHolder*>& remove) const;
    bool handle_weapons(std::set<AuraHolder*>& remove) const;
    bool handle_same_chain_caster(std::set<AuraHolder*>& remove) const;
    bool handle_same_chain_other(
        AuraHolder* other, std::set<AuraHolder*>& remove) const;
    bool handle_other(std::set<AuraHolder*>& remove) const;
    bool handle_area_aura_source(std::set<AuraHolder*>& remove) const;
    bool handle_collisions(std::set<AuraHolder*>& remove, AuraHolder* holder,
        const SpellEntry* other_info, bool force_test = false) const;
    bool handle_mechanics(const SpellEntry* other_info) const;
    bool handle_spell_specifics(AuraHolder* holder) const;
    // 0: break, 1: continue, 2: ignore
    int handle_db_stacking(std::set<AuraHolder*>& remove, AuraHolder* holder,
        const SpellEntry* other_info, bool& ret_val) const;

    // Helper functions
    AuraType get_substantial(bool fallback_to_any) const;
    void build_auras();
    void build_logical_auras(std::set<logical_aura>& auras,
        const SpellEntry* info, AuraHolder* holder = nullptr) const;
    bool ignored_due_to_castitem(AuraHolder* holder = nullptr) const;

    // Helper functions (inline)
    bool same_chain(uint32 other) const
    {
        if (sSpellMgr::Instance()->GetSpellChainNode(spell_info_->Id))
            return sSpellMgr::Instance()->GetFirstSpellInChain(
                       spell_info_->Id) ==
                   sSpellMgr::Instance()->GetFirstSpellInChain(other);
        return false;
    }
    bool addible(const SpellEntry* entry, SpellEffectIndex index) const
    {
        // these aura types are not ignored for stacking, but can be added
        // together, making them stack with the same type of auras
        uint32 aura = entry->EffectApplyAuraName[index];
        return aura == SPELL_AURA_MOD_MELEE_HASTE ||
               aura == SPELL_AURA_MOD_RANGED_HASTE ||
               aura == SPELL_AURA_RANGED_ATTACK_POWER_ATTACKER_BONUS;
    }
    bool ignore(const SpellEntry* entry, SpellEffectIndex index) const
    {
        uint32 aura = entry->EffectApplyAuraName[index];
        return aura == SPELL_AURA_NONE || aura == SPELL_AURA_DUMMY ||
               aura == SPELL_AURA_PERIODIC_DUMMY ||
               aura == SPELL_AURA_PERIODIC_TRIGGER_SPELL ||
               aura == SPELL_AURA_PROC_TRIGGER_SPELL;
    }
    // Cosmetic means that the aura is only there to affect how another aura
    // looks, and is completely ignored for stacking purposes
    bool is_cosmetic(const SpellEntry* info, uint32 aura) const
    {
        return aura == SPELL_AURA_MOD_DECREASE_SPEED &&
               info->HasApplyAuraName(SPELL_AURA_MOD_CONFUSE);
    }
    bool is_hot_dot(AuraType aura) const
    {
        return aura == SPELL_AURA_PERIODIC_DAMAGE ||
               aura == SPELL_AURA_PERIODIC_HEAL ||
               aura == SPELL_AURA_PERIODIC_LEECH ||
               aura == SPELL_AURA_PERIODIC_MANA_LEECH;
    }
    bool is_hot_dot(const SpellEntry* info) const
    {
        for (int i = 0; i < 3; ++i)
            if (is_hot_dot(static_cast<AuraType>(info->EffectApplyAuraName[i])))
                return true;
        return false;
    }
    bool is_weapon_handled_buff() const
    {
        // XXX:
        if (caster_->GetTypeId() == TYPEID_PLAYER && holder_ &&
            holder_->GetCastItemGuid() && caster_ == target_)
            if (Item* item = static_cast<Player*>(caster_)->GetItemByGuid(
                    holder_->GetCastItemGuid()))
                if (item->slot().main_hand() || item->slot().off_hand())
                    return true;
        return false;
    }
    typedef std::set<std::pair<logical_aura, logical_aura>> overlaps;
    overlaps get_overlaps(const std::set<logical_aura>& auras) const
    {
        overlaps overlap;
        // If any significant aura is the same these buffs overlap
        for (const auto& elem : auras_)
        {
            auto find = auras.find(elem);
            if (find != auras.end() && (find->positive() == elem.positive()) &&
                !addible(elem.info(), elem.index()))
                overlap.insert(std::make_pair(elem, *find));
        }
        return overlap;
    }
};

#endif
