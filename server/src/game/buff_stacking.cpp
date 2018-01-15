#include "buff_stacking.h"
#include "Spell.h"
#include "Unit.h"
#include <algorithm>
#include <cmath>

namespace
{
struct DbRules
{
    // Spell & Group ids this rule stacks & dont stack with
    google::dense_hash_map<int, BuffStackStacks> method;

    DbRules() { method.set_empty_key(0); }

    BuffStackStacks stacks(int id) const
    {
        auto itr = method.find(id);
        if (itr != method.end())
            return itr->second;
        return BS_STACKS_IGNORE;
    }
};

struct DbGroup
{
    int id;
    uint32 def;
    DbRules rules;

    BuffStackStacks stacks(int id) const { return rules.stacks(id); }
};

google::dense_hash_map<uint32, DbGroup> groups;
google::dense_hash_map<uint32, DbGroup*> spell_groups;
google::dense_hash_map<uint32, DbRules> spell_rules;
google::dense_hash_set<uint32> db_involved_spells;

DbGroup* get_group(uint32 id)
{
    auto itr = spell_groups.find(id);
    return itr != spell_groups.end() ? itr->second : nullptr;
}

DbRules* get_rules(uint32 id)
{
    auto itr = spell_rules.find(id);
    return itr != spell_rules.end() ? &itr->second : nullptr;
}

// Tests stacking of two spell ids
BuffStackStacks test_db_stacking(uint32 id_one, uint32 id_two)
{
    id_one = sSpellMgr::Instance()->GetFirstSpellInChain(id_one);
    id_two = sSpellMgr::Instance()->GetFirstSpellInChain(id_two);

    auto rules1 = get_rules(id_one), rules2 = get_rules(id_two);
    auto grp1 = get_group(id_one), grp2 = get_group(id_two);

    if (rules1)
    {
        auto res = rules1->stacks(id_two);
        if (grp2 && res == BS_STACKS_IGNORE)
            res = rules1->stacks(-grp2->id);
        if (res != BS_STACKS_IGNORE)
            return res;
    }

    if (rules2)
    {
        auto res = rules2->stacks(id_one);
        if (grp1 && res == BS_STACKS_IGNORE)
            res = rules2->stacks(-grp1->id);
        if (res != BS_STACKS_IGNORE)
            return res;
    }

    if (grp1)
    {
        auto res = grp1->stacks(id_two);
        if (grp2 && res == BS_STACKS_IGNORE)
            res = grp1->stacks(-grp2->id);
        if (res != BS_STACKS_IGNORE)
            return res;
    }

    if (grp2)
    {
        auto res = grp2->stacks(id_one);
        if (grp1 && res == BS_STACKS_IGNORE)
            res = grp2->stacks(-grp1->id);
        if (res != BS_STACKS_IGNORE)
            return res;
    }

    if (grp1 && grp1->def == BS_DEF_ALL)
        return BS_STACKS_OK;
    if (grp2 && grp2->def == BS_DEF_ALL)
        return BS_STACKS_OK;

    return BS_STACKS_IGNORE;
}
}

void buff_stack_db_load()
{
    std::map<int, DbRules> tmp_group_rules;

    groups.set_empty_key(0);
    spell_groups.set_empty_key(0);
    spell_rules.set_empty_key(0);
    db_involved_spells.set_empty_key(0);

    auto res = std::unique_ptr<QueryResult>(WorldDatabase.PQuery(
        "SELECT id_one, id_two, stacks FROM buffstack_rules"));
    if (!res)
        return; // Other tables depends on this one
    do
    {
        auto fields = res->Fetch();
        int id_one = fields[0].GetInt32();
        int id_two = fields[1].GetInt32();
        uint8 stacks = fields[2].GetUInt8();

        if (stacks > BS_STACKS_COLLISION_RANK)
        {
            logging.error("buff_stack_rules: invalid stacks one=%u, two=%u",
                id_one, id_two);
            continue;
        }

        if (id_one < 0)
        {
            tmp_group_rules[-id_one].method[id_two] = (BuffStackStacks)stacks;
        }
        else
        {
            spell_rules[id_one].method[id_two] = (BuffStackStacks)stacks;
            db_involved_spells.insert(id_one);
            db_involved_spells.insert(id_two);
        }

    } while (res->NextRow());

    res = std::unique_ptr<QueryResult>(
        WorldDatabase.PQuery("SELECT id, `default` FROM buffstack_groups"));
    if (!res)
        return; // buff_stack_spells depends on this one
    do
    {
        auto fields = res->Fetch();
        uint32 id = fields[0].GetUInt32();
        uint32 def = fields[1].GetUInt32();

        groups[id].id = id;
        groups[id].def = def;
        groups[id].rules = tmp_group_rules[id];

    } while (res->NextRow());

    res = std::unique_ptr<QueryResult>(
        WorldDatabase.PQuery("SELECT gid, sid FROM buffstack_group_spells"));
    if (!res)
        return;
    do
    {
        auto fields = res->Fetch();
        uint32 gid = fields[0].GetUInt32();
        uint32 sid = fields[1].GetUInt32();

        if (!groups.count(gid))
        {
            logging.error(
                "buff_stack_spells: non existant group gid=%u, sid=%u", gid,
                sid);
            continue;
        }

        spell_groups[sid] = &groups[gid];
        db_involved_spells.insert(sid);

    } while (res->NextRow());
}

buff_stacks::buff_stacks(uint32 spell_id, Unit* caster, Unit* target,
    Item* cast_item, AuraHolder* holder)
  : spell_info_(nullptr), caster_(caster), target_(target),
    cast_item_(cast_item), holder_(holder), duration_(0)
{
    spell_info_ = sSpellStore.LookupEntry(spell_id);
    if (spell_info_)
        build_auras();
}

void buff_stacks::build_auras()
{
    // Build our logical auras
    build_logical_auras(auras_, spell_info_, holder_);

    if (holder_)
    {
        // If we have a holder, the diminishing is already applied to that
        duration_ = holder_->GetAuraDuration();
    }
    else
    {
        // We need to calculate what diminishing will be once aura is applied if
        // we have no holder
        // because that means the test is happening before the Aura is created
        DiminishingGroup group =
            GetDiminishingReturnsGroupForSpell(spell_info_, false);
        duration_ = GetSpellDuration(spell_info_);

        if (group != DIMINISHING_NONE)
            target_->ApplyDiminishingToDuration(group, duration_, caster_,
                target_->GetDiminishing(group), false);
    }
}

std::pair<bool, std::set<AuraHolder*>> buff_stacks::operator()() const
{
    using namespace std;

    if (!spell_info_)
        return make_pair(false, set<AuraHolder*>());

    if (spell_info_->HasAttribute(SPELL_ATTR_CUSTOM_IGNORE_ALL_BUFF_STACK))
        return make_pair(true, set<AuraHolder*>());

    // Spells that must be removed if we can stack
    set<AuraHolder*> remove;

    // Passive spells with SpellIconID 1 seem to all be gear auras that can
    // stack in any fashion
    if (spell_info_->HasAttribute(SPELL_ATTR_PASSIVE) &&
        spell_info_->SpellIconID == 1)
        return make_pair(true, set<AuraHolder*>());

    // Drink buffs need to be handled in a seperate fashion
    auto drink_res = handle_drink_buff(remove);
    if (drink_res.first)
        return make_pair(drink_res.second, remove);

    // Area Auras when the source is the target
    if (holder_ && holder_->IsAreaAura() && caster_ == target_)
    {
        if (!handle_area_aura_source(remove))
            return make_pair(false, remove);
    }

    bool handled_other = false;
    if (is_weapon_handled_buff())
    {
        // Handle weapon casted buffs (only applies to players buffing
        // themselves)
        if (!handle_weapons(remove))
            return make_pair(false, set<AuraHolder*>());
    }
    else if (!spell_info_->SpellSpecific &&
             (auras_.empty() || is_hot_dot(spell_info_) ||
                 spell_info_->HasAttribute(SPELL_ATTR_EX3_SEPARATE_STACKS) ||
                 spell_info_->HasAttribute(SPELL_ATTR_PASSIVE)))
    {
        // HoTs, DoTs, passive talents or dummy auras only care about their own
        // spell chain (note: if a spell has a SpellSpecific invalidated)
        if (!handle_same_chain_caster(remove))
            return make_pair(false, set<AuraHolder*>());
    }
    else
    {
        handled_other = true;
        if (!handle_other(remove))
            return make_pair(false, set<AuraHolder*>());
    }

    // We need to check if we're involved in any DB overrides
    if (!handled_other &&
        db_involved_spells.count(
            sSpellMgr::Instance()->GetFirstSpellInChain(spell_info_->Id)))
    {
        // Test DB stacking
        bool ret_val = true;
        target_->loop_auras([&remove, &ret_val, this](AuraHolder* holder)
            {
                const SpellEntry* other_info = holder->GetSpellProto();
                if (!other_info || other_info->HasAttribute(SPELL_ATTR_PASSIVE))
                    return true; // continue

                auto res =
                    handle_db_stacking(remove, holder, other_info, ret_val);
                if (!res)
                    return false; // break
                return true;      // continue
            });
        if (!ret_val)
            return make_pair(false, set<AuraHolder*>());
    }

    return make_pair(true, remove);
}

std::pair<bool, bool> buff_stacks::handle_drink_buff(
    std::set<AuraHolder*>& remove) const
{
    // Drink buffs have:
    // aura 0: mod_power_regen with formula 0
    // aura 1: periodic_dummy with formula of actual regen
    if (!(spell_info_->EffectApplyAuraName[0] == SPELL_AURA_MOD_POWER_REGEN &&
            spell_info_->EffectApplyAuraName[1] == SPELL_AURA_PERIODIC_DUMMY))
        return std::make_pair(false, false);

    auto bp = spell_info_->EffectBasePoints[1];
    bool res = true;
    remove.clear();

    target_->loop_auras([&remove, bp, &res](AuraHolder* holder)
        {
            auto info = holder->GetSpellProto();
            if (info->EffectApplyAuraName[0] == SPELL_AURA_MOD_POWER_REGEN &&
                info->EffectApplyAuraName[1] == SPELL_AURA_PERIODIC_DUMMY)
            {
                if (info->EffectBasePoints[1] > bp)
                {
                    res = false;
                    return false; // break
                }
                else
                    remove.insert(holder);
            }
            return true; // continue
        });

    return std::make_pair(true, res);
}

bool buff_stacks::handle_weapons(std::set<AuraHolder*>& remove) const
{
    assert(is_weapon_handled_buff());

    Item* our_item = static_cast<Player*>(caster_)->GetItemByGuid(
        holder_->GetCastItemGuid());
    if (!our_item)
        return false;

    // We only need to check auras with the exact same ID as us
    const_cast<const Unit*>(target_)->loop_auras(
        [&remove, our_item, this](AuraHolder* holder)
        {
            if (!holder->GetCastItemGuid())
                return true; // continue
            if (Item* other_item =
                    static_cast<const Player*>(caster_)->GetItemByGuid(
                        holder->GetCastItemGuid()))
                if (our_item->slot() != other_item->slot() &&
                    other_item->slot().equipment())
                    return true; // continue
            remove.insert(holder);
            return true; // continue
        },
        spell_info_->Id);

    return true;
}

bool buff_stacks::handle_same_chain_caster(std::set<AuraHolder*>& remove) const
{
    // We only need to find auras with the exact setup as ourselves
    AuraType first_aura = get_substantial(true);
    const Unit::Auras& auras = target_->GetAurasByType(first_aura);
    for (const auto& aura : auras)
    {
        if (caster_ == (aura)->GetCaster() &&
            (same_chain((aura)->GetId()) || (aura)->GetId() == spell_info_->Id))
        {
            // Rank based testing
            if (sSpellMgr::Instance()->GetSpellRank((aura)->GetId()) >
                sSpellMgr::Instance()->GetSpellRank(spell_info_->Id))
                return false;
            if (duration_ != -1 && duration_ < (aura)->GetAuraDuration())
                return false;
            remove.insert((aura)->GetHolder());
        }
    }
    return true;
}

bool buff_stacks::handle_other(std::set<AuraHolder*>& remove) const
{
    // We need to try all auras for this spell
    bool ret_val = true;
    target_->loop_auras([&remove, &ret_val, this](AuraHolder* holder)
        {
            const SpellEntry* other_info = holder->GetSpellProto();
            if (!other_info)
                return true; // continue

            // Don't test stacking if one is a debuff and the other a buff
            if (holder_ && holder->IsPositive() != holder_->IsPositive())
                return true; // continue

            // Don't test stacking if it's a spell specific that only cares
            // about specifics of the same type
            if (spell_info_->SpellSpecific != other_info->SpellSpecific &&
                (is_same_spell_spec_only(spell_info_->SpellSpecific) ||
                    is_same_spell_spec_only(other_info->SpellSpecific)))
                return true; // continue

            if (spell_info_->SpellSpecific && other_info->SpellSpecific &&
                !handle_spell_specifics(holder))
            {
                // Flasks should not be overwritable by elixirs
                if ((spell_info_->SpellSpecific == SPELL_BATTLE_ELIXIR ||
                        spell_info_->SpellSpecific == SPELL_GUARDIAN_ELIXIR) &&
                    other_info->SpellSpecific == SPELL_FLASK)
                {
                    ret_val = false;
                    return false; // break
                }
                else
                    remove.insert(holder);
                return true; // continue
            }

            // Don't test against non-spell specs for spell spec only buffs
            if (is_spell_spec_only(spell_info_->SpellSpecific) ||
                is_spell_spec_only(other_info->SpellSpecific))
                return true; // continue

            // Test DB stacking
            auto res = handle_db_stacking(remove, holder, other_info, ret_val);
            if (!res)
                return false; // break
            if (res == 1)
                return true; // continue

            // Same rank rules
            if (!spell_info_->SpellSpecific &&
                (same_chain(other_info->Id) ||
                    (other_info->Id == spell_info_->Id &&
                        !spell_info_->HasAttribute(SPELL_ATTR_PASSIVE))))
            {
                if (!handle_same_chain_other(holder, remove))
                {
                    ret_val = false;
                    return false; // break
                }
                return true; // continue
            }

            // If we have no logically important auras we do not check for
            // collisions
            if (auras_.empty())
                return true; // continue

            // If either spell is passive, HoT/DoT or an area aura (or
            // persistent area aura) we do not do collision checks
            // Procs from equipped items, or auras marked for ignoring buff
            // stacking, also skip collision checks
            // (Note: SpellSpecifics already checked at this point)
            if (spell_info_->HasAttribute(SPELL_ATTR_PASSIVE) ||
                is_hot_dot(spell_info_) ||
                other_info->HasAttribute(SPELL_ATTR_PASSIVE) ||
                is_hot_dot(other_info) || (holder_ && holder_->IsAreaAura()) ||
                holder->IsAreaAura() || (holder_ && holder_->IsPersistent()) ||
                holder->IsPersistent() ||
                spell_info_->HasAttribute(
                    SPELL_ATTR_CUSTOM_IGNORE_BUFF_STACKING) ||
                other_info->HasAttribute(
                    SPELL_ATTR_CUSTOM_IGNORE_BUFF_STACKING) ||
                ignored_due_to_castitem(holder) ||
                ignored_due_to_castitem(holder_))
                return true; // continue

            // Daze effects do not collide with slows (e.g. concussive shot and
            // wing clip stack)
            if (spell_info_->HasAttribute(SPELL_ATTR_CUSTOM_DAZE_EFFECT) !=
                other_info->HasAttribute(SPELL_ATTR_CUSTOM_DAZE_EFFECT))
                return true; // continue

            if (!handle_collisions(remove, holder, other_info))
            {
                ret_val = false;
                return false; // break
            }
            return true; // continue
        });
    return ret_val;
}

bool buff_stacks::handle_area_aura_source(std::set<AuraHolder*>& remove) const
{
    AuraType first_aura = get_substantial(true);
    const auto& auras = target_->GetAurasByType(first_aura);
    for (const auto& aura : auras)
    {
        if (sSpellMgr::Instance()->GetFirstSpellInChain(aura->GetId()) !=
            sSpellMgr::Instance()->GetFirstSpellInChain(spell_info_->Id))
            continue;

        if (area_aura_stacks(spell_info_->Id))
            continue;

        remove.insert(aura->GetHolder());
    }

    return true;
}

bool buff_stacks::handle_same_chain_other(
    AuraHolder* other, std::set<AuraHolder*>& remove) const
{
    // If they're the same chain, check which spell is strongest
    bool res;
    auto start_sz = remove.size();
    if (!spell_info_->StackAmount)
    {
        res = handle_collisions(remove, other, other->GetSpellProto());
        if (!res)
            return false;
    }

    if (spell_info_->StackAmount || (res && remove.size() == start_sz))
    {
        // Equal strength, go by rank
        auto rank = sSpellMgr::Instance()->GetSpellRank(spell_info_->Id);
        auto other_rank =
            sSpellMgr::Instance()->GetSpellRank(other->GetSpellProto()->Id);
        if (other_rank > rank)
            return false;
        else if (other_rank < rank)
            remove.insert(other);
        // Same rank, does aura have a stack amount?
        else if (spell_info_->StackAmount)
            remove.insert(
                other); // Unit::AddAuraHolder expects other marked remove
        // Same rank, check duration
        else if (duration_ != -1 && duration_ < other->GetAuraDuration())
            return false;
        else
            remove.insert(other);
    }

    return true;
}

bool buff_stacks::handle_collisions(std::set<AuraHolder*>& remove,
    AuraHolder* holder, const SpellEntry* other_info, bool force_test) const
{
    std::set<logical_aura> auras;

    // Note: Don't use scaling base points if holder_ is nullptr, or we end up
    // trying stacking of an aura that scales and one that hasn't had scaling
    // calculated yet.
    build_logical_auras(auras, other_info, holder_ ? holder : nullptr);
    if (!force_test && auras.empty())
        return true;

    overlaps clashes = get_overlaps(auras);
    if (!force_test && clashes.empty())
        return true;

    // Check if the spells' mechanic lets us ignore collisions
    if (!force_test && handle_mechanics(other_info))
        return true;

    if (clashes.size() == auras_.size() && clashes.size() == auras.size())
    {
        // TODO: What happens if a buff has an aura that's less potent and one
        // that is more potent? Can that happen? If so, do they both stay?

        // The buffs clash completely. All their auras overlap. The most potent
        // stays.
        bool equal_potency = true;
        for (const auto& clashe : clashes)
            if (abs(clashe.first.base_points()) <
                abs(clashe.second.base_points()))
                return false;
            else if (abs(clashe.first.base_points()) >
                     abs(clashe.second.base_points()))
                equal_potency = false;
        // If the potency was exactly the same, use duration to determine
        if (equal_potency && duration_ != -1 &&
            duration_ < holder->GetAuraDuration())
            return false;
        remove.insert(holder);
    }
    else if (clashes.size() == auras_.size() || clashes.size() == auras.size())
    {
        // One buff has some auras that do not clash. The one with all clashing
        // auras
        // should go, UNLESS he is more potent with those auras, at which point
        // they both stay.
        for (const auto& clashe : clashes)
        {
            if (auras_.size() > auras.size())
            {
                if (abs(clashe.first.base_points()) <
                    abs(clashe.second.base_points()))
                    return true; // both stay
            }
            else // auras_.size() < auras.size()
            {
                if (abs(clashe.first.base_points()) >
                    abs(clashe.second.base_points()))
                    return true; // both stay
            }
        }
        // One goes
        if (auras_.size() < auras.size())
            return false; // The other one has more auras
        else
            remove.insert(holder); // We have more auras
    }
    else
    {
        // Both buffs have auras that do not clash with the other, they both can
        // stay.
    }
    return true;
}

inline uint32 first_mechanic(const SpellEntry* info)
{
    if (info->Mechanic)
        return info->Mechanic;
    return info->EffectMechanic[0] != 0 ?
               info->EffectMechanic[0] :
               info->EffectMechanic[1] != 0 ? info->EffectMechanic[1] :
                                              info->EffectMechanic[2];
}

bool buff_stacks::handle_mechanics(const SpellEntry* other_info) const
{
    // From what we know no spell that matters for this will have multiple
    // mechanics (a mechanic mask)
    uint32 our_mechanic = first_mechanic(spell_info_);
    uint32 other_mechanic = first_mechanic(other_info);

    // Only some mechanics matter from a stacking PoV, make sure both auras has
    // at least one of them

    switch (our_mechanic)
    {
    case MECHANIC_FEAR:
    case MECHANIC_HORROR:
    case MECHANIC_STUN:
    case MECHANIC_KNOCKOUT:
    case MECHANIC_SAPPED:
        break;
    default:
        return false;
    }

    switch (other_mechanic)
    {
    case MECHANIC_FEAR:
    case MECHANIC_HORROR:
    case MECHANIC_STUN:
    case MECHANIC_KNOCKOUT:
    case MECHANIC_SAPPED:
        break;
    default:
        return false;
    }

    // If both auras has a significant mechanic, and they don't have the same,
    // they can stack
    return our_mechanic != other_mechanic;
}

bool buff_stacks::handle_spell_specifics(AuraHolder* holder) const
{
    if (spell_info_->SpellSpecific == holder->GetSpellProto()->SpellSpecific)
    {
        if (is_one_buff_per_caster_spell(spell_info_) &&
            caster_->GetObjectGuid() == holder->GetCasterGuid())
            return false;
        else if (is_exclusive_buff_spell(spell_info_))
            return false;
    }
    std::vector<SpellSpecific> out;
    if (is_group_exclusive_buff_spell(spell_info_, out))
        if (std::find(out.begin(), out.end(),
                holder->GetSpellProto()->SpellSpecific) != out.end())
            return false;
    return true;
}

int buff_stacks::handle_db_stacking(std::set<AuraHolder*>& remove,
    AuraHolder* holder, const SpellEntry* other_info, bool& ret_val) const
{
    auto res = test_db_stacking(spell_info_->Id, other_info->Id);
    if (res == BS_STACKS_COLLISION_STRENGTH)
    {
        if (!handle_collisions(remove, holder, other_info, true))
        {
            ret_val = false;
            return 0; // break
        }
        return 1; // continue
    }
    else if (res == BS_STACKS_COLLISION_DURATION)
    {
        if (duration_ != -1 && duration_ < holder->GetAuraDuration())
        {
            ret_val = false;
            return 0; // break
        }
        remove.insert(holder);
        return 1; // continue
    }
    else if (res == BS_STACKS_COLLISION_RANK)
    {
        // Rank based testing
        if (sSpellMgr::Instance()->GetSpellRank(other_info->Id) >
            sSpellMgr::Instance()->GetSpellRank(spell_info_->Id))
        {
            ret_val = false;
            return 0; // break
        }
        if (duration_ != -1 && duration_ < holder->GetAuraDuration())
        {
            ret_val = false;
            return 0; // break
        }
        remove.insert(holder);
        return 1; // continue
    }
    else if (res == BS_STACKS_OK)
        return 1; // continue
    return 2;     // ignore
}

/**
 * Helper Functions
 */

AuraType buff_stacks::get_substantial(bool fallback_to_any) const
{
    uint32 first_substantial = !ignore(spell_info_, EFFECT_INDEX_0) ?
                                   spell_info_->EffectApplyAuraName[0] :
                                   !ignore(spell_info_, EFFECT_INDEX_1) ?
                                   spell_info_->EffectApplyAuraName[1] :
                                   !ignore(spell_info_, EFFECT_INDEX_2) ?
                                   spell_info_->EffectApplyAuraName[2] :
                                   static_cast<uint32>(SPELL_AURA_NONE);

    if (fallback_to_any && first_substantial == SPELL_AURA_NONE)
        return static_cast<AuraType>(spell_info_->EffectApplyAuraName[0] ?
                                         spell_info_->EffectApplyAuraName[0] :
                                         spell_info_->EffectApplyAuraName[1] ?
                                         spell_info_->EffectApplyAuraName[1] :
                                         spell_info_->EffectApplyAuraName[2]);
    return static_cast<AuraType>(first_substantial);
}

void buff_stacks::build_logical_auras(std::set<logical_aura>& auras,
    const SpellEntry* info, AuraHolder* holder) const
{
    for (int i = 0; i < 3; ++i)
    {
        if (!ignore(info, static_cast<SpellEffectIndex>(i)) &&
            !is_cosmetic(info, info->EffectApplyAuraName[i]))
        {
            // Use scaling base points if holder is passed in
            Aura* a = nullptr;
            if (holder &&
                (a = holder->GetAura(static_cast<SpellEffectIndex>(i))))
                auras.insert(
                    logical_aura(info, static_cast<SpellEffectIndex>(i),
                        a->GetModifier() ? a->GetModifier()->m_amount :
                                           a->GetBasePoints()));
            else
                auras.insert(
                    logical_aura(info, static_cast<SpellEffectIndex>(i)));
        }
    }
}

bool buff_stacks::ignored_due_to_castitem(AuraHolder* holder) const
{
    if (caster_->GetTypeId() != TYPEID_PLAYER)
        return false;

    Item* item = nullptr;

    // NOTE: holder is null if this is pre-application for our aura, at which
    // point we use cast_item_
    if (holder)
        item = static_cast<Player*>(caster_)->GetItemByGuid(
            holder->GetCastItemGuid(), false);
    else
        item = cast_item_;

    if (!item)
        return false;

    if (!item->IsEquipped())
        return false;

    // Trinket auras seem to always be able to stack
    if (item->slot().index() == inventory::trinket1 ||
        item->slot().index() == inventory::trinket2)
        return true;

    // "Equip: Does X" proc effects stack as long as they are triggered by
    // another aura
    if (holder && holder->GetTriggeredByProto())
        return true;

    return false;
}

// Does area aura stack with its own chain from multiple sources?
bool buff_stacks::area_aura_stacks(uint32 spell_id)
{
    auto res = test_db_stacking(spell_id, spell_id);
    return res == BS_STACKS_OK;
}
