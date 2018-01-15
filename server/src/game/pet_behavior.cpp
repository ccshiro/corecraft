#include "pet_behavior.h"
#include "movement/IdleMovementGenerator.h"
#include "movement/PointMovementGenerator.h"
#include "movement/TargetedMovementGenerator.h"
#include "Pet.h"
#include "Player.h"
#include "Spell.h"
#include "SpellMgr.h"
#include "Util.h"
#include "pet_template.h"

#define PET_CHASE_SPELL_PRIO \
    movement::get_default_priority(movement::gen::chase) + 5

pet_behavior::pet_behavior(Creature* owner)
  : pet_(owner), stay_gen_(nullptr), paused_(false), stay_(false),
    chasing_(false), issued_command_(petai_cmd::none), update_friendly_(0),
    queued_id_(0)
{
}

pet_behavior::~pet_behavior()
{
    // Remove stay gen for enslaved demons & other temporary controlled pets
    if (stay_gen_)
        pet_->movement_gens.remove(stay_gen_);
}

void pet_behavior::evade(bool target_died)
{
    issued_command_ = petai_cmd::none;

    if (target_)
    {
        // It's possible the target in Unit::attacking has gone invalid, in
        // which case we don't use attack stop
        if (pet_->GetMap()->GetUnit(target_))
        {
            Unit* new_target = nullptr;

            // go on new target if current one died
            if (target_died && !is_passive() && !pet_->getAttackers().empty())
                new_target = *pet_->getAttackers().begin();

            pet_->AttackStop(new_target != nullptr, false, true);
            pet_->InterruptNonMeleeSpells(
                false); // AttackStop() only interrupts melee spells

            if (new_target)
            {
                if (try_attack(new_target))
                    return; // we got a new target, don't process further
            }
        }
        else
        {
            pet_->clear_attacking();
            pet_->InterruptNonMeleeSpells(false);
            pet_->SetTargetGuid(ObjectGuid());
            pet_->clearUnitState(UNIT_STAT_MELEE_ATTACKING);
        }

        target_ = ObjectGuid();
    }

    // Remove stay idle movement generator if stay is disabled
    if (!stay_ && stay_gen_)
    {
        pet_->movement_gens.remove(stay_gen_);
        stay_gen_ = nullptr;
    }

    pet_->movement_gens.on_event(movement::EVENT_LEAVE_COMBAT);
}

void pet_behavior::attacked(Unit* attacker)
{
    ReactStates react = pet_->GetCharmInfo()->GetReactState();
    if ((react != REACT_DEFENSIVE && react != REACT_AGGRESSIVE) ||
        is_passive() || target_ || issued_command_ != petai_cmd::none)
        return;
    try_attack(attacker);
}

bool pet_behavior::in_control() const
{
    if (pet_->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_CONFUSED) ||
        pet_->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_FLEEING) ||
        pet_->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_STUNNED) ||
        pet_->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_DISABLE_MOVE))
        return false;
    if (paused_)
        return false;
    return true;
}

void pet_behavior::update(const uint32 diff)
{
    if (queued_id_ &&
        (!in_control() ||
            pet_->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PLAYER_CONTROLLED)))
        clear_queued_spell();

    // If we have a mind control (such as eyes of the beast) on us, we need
    // special updating
    if (pet_->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PLAYER_CONTROLLED))
    {
        update_player_controlled();
        return;
    }

    /* We need to update validity of target every update,
       as to keep pet_->getVictim() valid for other parts of the core */
    Unit* target = target_ ? pet_->GetMap()->GetUnit(target_) : nullptr;
    if (!target && target_)
    {
        evade();
    }
    else if (target && !can_attack(target))
    {
        target = nullptr;
        evade();
    }

    // Don't update while charging
    if (pet_->movement_gens.top_id() == movement::gen::charge)
        return;

    // Needs to be after evade() to clear our target etc if it goes missing
    // while we're not in control
    if (!in_control())
        return;

    // We need to update facing and melee attacking state every tick
    if (target)
    {
        // If we're not chasing we need to spin around manually
        if (!chasing_)
            pet_->SetInFront(target);

        if (pet_->get_template()->behavior != PET_BEHAVIOR_RANGED_NO_MELEE &&
            pet_->get_template()->behavior != PET_BEHAVIOR_MELEE_NO_AUTO_ATTACK)
            pet_->UpdateMeleeAttackingState();
    }

    if (update_friendly_ <= diff)
    {
        if (Unit* owner = get_owner())
        {
            update_friendly_ = PET_UPDATE_FRIENDLY_TARGETS;
            update_friendly_targets(owner);
        }
    }
    else
        update_friendly_ -= diff;

    // Handle queued spells if we have any
    if (queued_id_)
        update_queued_spells();

    Unit* owner = get_owner();
    if (!owner)
        return;

    // Command states must be updated before combat states
    if (issued_command_ != petai_cmd::none)
        update_command_state(owner);

    // Command states changing might also invalidate our target
    if (!target_)
        target = nullptr;

    if (target)
        update_victim(owner, target);
    else
        update_no_victim(owner);
}

void pet_behavior::update_command_state(Unit* /*owner*/)
{
    switch (issued_command_)
    {
    case petai_cmd::stay:
        stay_ = true;
        chasing_ = false;
        // Idle movement generator
        if (stay_gen_ == nullptr)
        {
            stay_gen_ = new movement::IdleMovementGenerator();
            pet_->movement_gens.push(
                stay_gen_, 0, get_default_priority(movement::gen::chase) - 1);
        }
        // Clear chase movement generator
        pet_->movement_gens.remove_all(movement::gen::chase);
        pet_->GetCharmInfo()->SetCommandState(COMMAND_STAY);
        pet_->GetCharmInfo()->SaveStayPosition(
            pet_->GetX(), pet_->GetY(), pet_->GetZ());
        clear_queued_spell();
        break;
    case petai_cmd::follow:
    {
        stay_ = false;
        // nobreak
    }
    case petai_cmd::passive:
    {
        evade();
        pet_->GetCharmInfo()->SetCommandState(COMMAND_FOLLOW);

        // Interrupt spells when we're made to follow
        pet_->InterruptNonMeleeSpells(false);
        clear_queued_spell();

        break;
    }
    case petai_cmd::attack:
        if (Unit* target = pet_->GetMap()->GetUnit(target_))
            attack(target);
        else
            evade();
        break;
    default:
        break;
    }

    issued_command_ = petai_cmd::none;
}

void pet_behavior::update_victim(Unit* owner, Unit* target)
{
    if (pet_->get_template()->behavior != PET_BEHAVIOR_MELEE)
    {
        bool can_afford =
            pet_->GetPower(pet_->getPowerType()) >= offensive_spell_cost();

        // If we're ranged we need to act if we can't afford our main spell
        if (!can_afford)
        {
            // If we don't have a melee attack we evade
            if (pet_->get_template()->behavior == PET_BEHAVIOR_RANGED_NO_MELEE)
            {
                evade();
                return;
            }
            // Otherwise we begin chasing (assuming stay has not been issued)
            else if (!chasing_ && !stay_)
            {
                set_chase_on();
            }
        }
        // Or if target is OORing/LoSing us
        else if (!pet_->IsWithinDistInMap(
                     target, offensive_spell_dist() * 0.95) ||
                 !pet_->IsWithinWmoLOSInMap(target))
        {
            if (!chasing_)
                set_chase_on();
        }
        // If we are chasing but can afford our main spell; and reach with it,
        // we can stop chasing
        else if (can_afford && chasing_ &&
                 pet_->IsWithinDistInMap(
                     target, offensive_spell_dist() * 0.8) &&
                 pet_->IsWithinWmoLOSInMap(target))
        {
            chasing_ = false;
            if (!pet_->movement_gens.has(movement::gen::stopped))
            {
                pet_->movement_gens.push(
                    new movement::StoppedMovementGenerator(),
                    movement::EVENT_LEAVE_COMBAT);
            }
        }
    }

    // Move to back of target NPC if it's stationary and not attacking pet_
    if (chasing_ && !pet_->IsNonMeleeSpellCasted(false) &&
        target->GetTypeId() == TYPEID_UNIT && !target->GetOwnerGuid() &&
        target->getVictim() != pet_ &&
        pet_->get_template()->behavior != PET_BEHAVIOR_RANGED_NO_MELEE &&
        target->HasInArc(M_PI_F, pet_) &&
        pet_->CanReachWithMeleeAttack(target) &&
        target->GetDistance(
            target_last_pos_.x, target_last_pos_.y, target_last_pos_.z) < 0.5f)
    {
        auto pos =
            target->GetPoint(M_PI_F, target->GetObjectBoundingRadius(), true);
        pet_->movement_gens.push(
            new movement::PointMovementGenerator(
                0xDEADBEEF, pos.x, pos.y, pos.z, false, true),
            movement::EVENT_LEAVE_COMBAT);
    }
    else if (chasing_)
    {
        target->GetPosition(
            target_last_pos_.x, target_last_pos_.y, target_last_pos_.z);
    }

    // Cast in-combat available spells
    if (!pet_->IsNonMeleeSpellCasted(false))
    {
        std::vector<const SpellEntry*> available_spells =
            build_available_spells();
        if (!available_spells.empty())
        {
            const SpellEntry* spell_info =
                available_spells[urand(0, available_spells.size() - 1)];
            if (Unit* target = find_target(spell_info))
            {
                cast(spell_info, target);
            }
            else
            {
                // No target found for this spell, ban it temporarily
                banned_spells_[spell_info->Id] = WorldTimer::time_no_syscall();
            }
        }
    }
}

void pet_behavior::update_no_victim(Unit* owner)
{
    bool casting = pet_->IsNonMeleeSpellCasted(false);

    if (!casting && !stay_ &&
        (pet_->get_template()->pet_flags & PET_FLAGS_DISABLE_FOLLOW) == 0)
    {
        if (!pet_->movement_gens.has(movement::gen::follow))
            pet_->movement_gens.push(
                new movement::FollowMovementGenerator(owner));
    }

    // If we're aggressive nearby enemies will cause us to queue an aggro
    // command
    if (pet_->GetCharmInfo()->GetReactState() == REACT_AGGRESSIVE)
    {
        if (issued_command_ == petai_cmd::none && !target_ &&
            !is_passive()) // Can be passive despite reactstate being aggressive
        {
            if (Unit* target = pet_->SelectNearestPetTarget(nullptr, 30.0f))
                try_attack(target);
        }
    }

    if (!casting)
        cast_ooc_spells();
}

void pet_behavior::update_player_controlled()
{
    pet_->UpdateMeleeAttackingState();

    if (issued_command_ == petai_cmd::attack)
    {
        if (Unit* target = pet_->GetMap()->GetUnit(target_))
            pet_->Attack(target, true);
    }
}

void pet_behavior::update_queued_spells()
{
    auto info = sSpellStore.LookupEntry(queued_id_);
    if (!info)
    {
        clear_queued_spell();
        return;
    }

    auto range = sSpellRangeStore.LookupEntry(info->rangeIndex);
    float min = GetSpellMinRange(range);
    float max = GetSpellMaxRange(range) -
                3; // reduce the max a bit so we have a bit of a buffer
    if (max < min)
        max = min > 0 ? min : 2; // in case min and max are very close

    if (Unit* target = pet_->GetMap()->GetUnit(queued_target_))
    {
        // If still not in range or sight, try again later
        if (!pet_->IsInRange(target, min, max) ||
            !pet_->IsWithinWmoLOSInMap(target))
            return;

        clear_queued_spell();

        SpellCastTargets targets;
        targets.setUnitTarget(target);
        Spell::attempt_pet_cast(pet_, info, targets, false);
    }
    else
        clear_queued_spell();
}

void pet_behavior::attempt_attack(Unit* target)
{
    Unit* owner = get_owner();
    if (!owner)
        return;

    if (!target->can_be_seen_by(owner, pet_))
        return;

    if (try_attack(target))
        clear_queued_spell();
}

bool pet_behavior::can_initiate_attack(Unit* target) const
{
    if (!can_attack(target))
        return false;

    if (!target->can_be_seen_by(pet_, pet_))
        return false;

    return true;
}

bool pet_behavior::can_attack(Unit* target) const
{
    Unit* owner = get_owner();
    if (!owner)
        return false;

    if (pet_ == target || owner == target)
        return false;

    if (!target->isAlive())
        return false;

    if (target->IsFriendlyTo(owner) || target->IsFriendlyTo(pet_))
        return false;

    // Players or player pets must be hostile, not just non-friendly
    if (target->player_or_pet() && !target->IsHostileTo(owner))
        return false;

    if (!target->isTargetableForAttack())
        return false;

    if (owner->GetTypeId() == TYPEID_PLAYER &&
        owner->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_SANCTUARY) &&
        target->player_or_pet())
        return false;

    if (target->GetTypeId() == TYPEID_PLAYER &&
        target->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_SANCTUARY))
        return false;

    if ((pet_->GetTransport() || target->GetTransport() ||
            owner->GetTransport()) &&
        (pet_->GetTransport() != target->GetTransport() ||
            owner->GetTransport() != target->GetTransport()))
        return false;

    if (target->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_PLAYER_ATTACKABLE))
        return false;

    return true;
}

bool pet_behavior::try_evade()
{
    if (target_)
    {
        evade();
        return true;
    }
    return false;
}

bool pet_behavior::try_attack(Unit* target)
{
    if (!can_attack(target))
        return false;

    if (target_ && target_ != target->GetObjectGuid())
        pet_->InterruptNonMeleeSpells(false);

    issued_command_ = petai_cmd::attack;
    target_ = target->GetObjectGuid();

    return true;
}

void pet_behavior::attempt_follow()
{
    if ((pet_->get_template()->pet_flags & PET_FLAGS_DISABLE_FOLLOW) != 0)
        return;

    issued_command_ = petai_cmd::follow;
}

void pet_behavior::attempt_stay()
{
    issued_command_ = petai_cmd::stay;
}

void pet_behavior::attempt_passive()
{
    issued_command_ = petai_cmd::passive;
}

bool pet_behavior::attempt_queue_spell(
    const SpellEntry* info, Unit* target, bool switch_target)
{
    bool friendly = pet_->IsFriendlyTo(target);

    if (!IsPositiveSpell(info) && friendly)
        return false;

    if (!friendly && !can_attack(target))
        return false;

    if (switch_target)
    {
        try_attack(target);
    }
    else
    {
        pet_->movement_gens.push(
            new movement::ChaseMovementGenerator(target->GetObjectGuid()), 0,
            PET_CHASE_SPELL_PRIO);
        pet_->SetTargetGuid(target->GetObjectGuid());
    }

    queued_id_ = info->Id;
    queued_target_ = target->GetObjectGuid();

    return true;
}

void pet_behavior::attack(Unit* target)
{
    if (target == pet_->getVictim() ||
        pet_->Attack(target,
            pet_->get_template()->behavior != PET_BEHAVIOR_RANGED_NO_MELEE ?
                true :
                false))
    {
        // Auras that get removed on the MELEE_ATTACK event, also get removed
        // for pets when they turn attack on (see imp's phase shift)
        pet_->remove_auras_on_event(AURA_INTERRUPT_FLAG_MELEE_ATTACK);

        target_ = target->GetObjectGuid();

        if (!pet_->movement_gens.has(movement::gen::chase))
        {
            pet_->movement_gens.push(new movement::ChaseMovementGenerator(),
                movement::EVENT_LEAVE_COMBAT);
            chasing_ = true;
        }
    }
}

std::vector<const SpellEntry*> pet_behavior::build_available_spells()
{
    std::vector<const SpellEntry*> available;

    for (unsigned int i = 0; i < pet_->GetPetAutoSpellSize(); ++i)
    {
        const SpellEntry* info =
            sSpellStore.LookupEntry(pet_->GetPetAutoSpellOnPos(i));
        if (!info)
            continue;

        if (pet_->GetCharmInfo()->GetGlobalCooldownMgr().HasGlobalCooldown(
                info) ||
            pet_->HasSpellCooldown(info->Id))
            continue;

        if (!can_use(info))
            continue;

        // Make sure hostile spells are castable on our main target, so we don't
        // spend updates trying non-able spells
        if (!IsPositiveSpell(info))
        {
            Unit* target = pet_->GetMap()->GetUnit(target_);
            if (!target)
                continue;
            if (!can_cast_on(info, target))
                continue;
        }

        available.push_back(info);
    }

    // If we can't find any available combat spells, we clear the banned spells
    // to retry them next update
    // (so we're not stalling spells for 10 seconds, for example if we were out
    // of range and now are in range again)
    if (available.empty())
        banned_spells_.clear();

    return available;
}

Unit* pet_behavior::find_target(const SpellEntry* spell_info) const
{
    // Devour magic should always be considered hostile when auto-casting it
    bool is_devour_magic = spell_info->SpellFamilyName == SPELLFAMILY_WARLOCK &&
                           spell_info->SpellIconID == 47;

    if (IsPositiveSpell(spell_info) && !is_devour_magic)
    {
        if (spell_info->HasTargetType(TARGET_SELF))
            return can_cast_on(spell_info, pet_) ? pet_ : nullptr;

        std::vector<Unit*> valid_targets;
        for (const auto& elem : friendly_targets_)
        {
            if (Unit* target = pet_->GetMap()->GetUnit(elem))
                if (can_cast_on(spell_info, target))
                    valid_targets.push_back(target);
        }

        if (valid_targets.empty())
            return can_cast_on(spell_info, pet_) ? pet_ : nullptr;

        return valid_targets[urand(0, valid_targets.size() - 1)];
    }
    else
    {
        if (Unit* target = pet_->GetMap()->GetUnit(target_))
            if (can_cast_on(spell_info, target))
                return target;
    }

    return nullptr;
}

bool pet_behavior::can_cast_on(const SpellEntry* spell_info, Unit* target) const
{
    if (!target->isAlive())
        return false;

    if (pet_ != target && !pet_->IsWithinWmoLOSInMap(target))
        return false;

    float min_range = 0.0f, max_range = 5.0f;
    if (IsAreaOfEffectSpell(spell_info))
    {
        max_range = GetSpellRadius(spell_info);
    }
    else
    {
        if (const SpellRangeEntry* range_entry =
                sSpellRangeStore.LookupEntry(spell_info->rangeIndex))
        {
            min_range = range_entry->minRange;
            max_range = range_entry->maxRange;
        }
    }

    float dist = pet_->GetDistance(target);
    if (dist < min_range || dist > max_range)
        return false;

    // Can only cast area auras on yourself
    if (HasAreaAuraEffect(spell_info))
        return target == pet_ && !pet_->has_aura(spell_info->Id);

    // Check stack amount for positive spells, as to avoid continuously
    // refreshing it
    if (spell_info->HasEffect(SPELL_EFFECT_APPLY_AURA) &&
        IsPositiveSpell(spell_info))
    {
        uint32 max_stack = spell_info->StackAmount;
        if (max_stack <= 1)
        {
            if (target->has_aura(spell_info->Id))
                return false;
        }
        else
        {
            if (target->GetAuraCount(spell_info->Id) >= max_stack)
                return false;
        }
    }

    return true;
}

bool pet_behavior::cast(
    const SpellEntry* spell_info, Unit* target, bool can_ban)
{
    SpellCastTargets targets;
    targets.setUnitTarget(target);
    if (Spell::attempt_pet_cast(pet_, spell_info, targets, false))
        return true;

    if (can_ban)
    {
        // Spell was thought to be castable, ban it temporarily
        banned_spells_[spell_info->Id] = WorldTimer::time_no_syscall();
    }

    return false;
}

void pet_behavior::cast_ooc_spells()
{
    for (unsigned int i = 0; i < pet_->GetPetAutoSpellSize(); ++i)
    {
        uint32 id = pet_->GetPetAutoSpellOnPos(i);

        const SpellEntry* info = sSpellStore.LookupEntry(id);
        if (!info)
            continue;

        bool ok = false;

        // List of known OOC spells using auto-cast
        switch (info->SpellFamilyName)
        {
        case SPELLFAMILY_WARLOCK:
            // Blood Pact
            if (info->SpellFamilyFlags & 0x800000 && info->SpellIconID == 541)
                ok = true;
            break;
        default:
            break;
        }

        switch (info->Id)
        {
        // Phase Shift
        case 4511:
        // Lesser Invisibility
        case 7870:
        // Paranoia
        case 19480:
            ok = true;
        default:
            break;
        }

        if (!ok)
            continue;

        if (pet_->has_aura(info->Id))
            continue;

        if (pet_->GetCharmInfo()->GetGlobalCooldownMgr().HasGlobalCooldown(
                info) ||
            pet_->HasSpellCooldown(info->Id))
            continue;

        cast(info, pet_, false);
        break; // Only cast one spell
    }
}

uint32 pet_behavior::offensive_spell_cost() const
{
    int32 oom = pet_->get_template()->spell_oom;

    // < 0: Use as spell id (first in chain); and figure out our oom mark from
    // that
    if (oom < 0)
    {
        // Find the highest rank we have of this spell
        uint32 spell_id = std::abs(oom);
        uint32 current = sSpellMgr::Instance()->GetFirstSpellInChain(spell_id);
        const SpellChainNode* node;
        while ((node = sSpellMgr::Instance()->GetSpellChainNode(current)) !=
                   nullptr &&
               !node->next.empty() && pet_->HasSpell(node->next[0]))
            current = node->next[0];

        if (!pet_->HasSpell(current))
            return 0; // Didn't have any of the ranks

        // Use that spell to determine the cost
        return Spell::CalculatePowerCost(
            sSpellStore.LookupEntry(current), pet_);
    }

    return static_cast<uint32>(
        oom); // Suppress warnings; we know what we're doing
}

float pet_behavior::offensive_spell_dist() const
{
    return pet_->get_template()->spell_dist;
}

// Player pets have conditions for certain spells
bool pet_behavior::can_use(const SpellEntry* spell_info) const
{
    // Spells can be temporarily banned for 10 seconds
    auto itr = banned_spells_.find(spell_info->Id);
    if (itr != banned_spells_.end() &&
        itr->second + 10 > WorldTimer::time_no_syscall())
        return false;

    // Ranged pets can only cast instant spells while moving (melee ones can any
    // spell)
    if (pet_->get_template()->behavior != PET_BEHAVIOR_MELEE &&
        GetSpellCastTime(spell_info, nullptr) != 0 && chasing_)
        return false;

    Unit* victim = pet_->GetMap()->GetUnit(target_); // Can be NULL

    // Smart Autocast - Some Pet Spells should only be casted under certain
    // conditions
    switch (spell_info->SpellFamilyName)
    {
    // Warlock pets
    case SPELLFAMILY_WARLOCK:
        // Voildwalker & Felguard Spells
        if (spell_info->SpellFamilyFlags & 0x2000000)
        {
            // Anguish & Torment can only be used on units
            if (spell_info->SpellIconID == 173)
                return victim ? victim->GetTypeId() == TYPEID_UNIT : false;

            // Suffering -- Must be at least 2 unfriendly non-players nearby
            else if (spell_info->SpellIconID == 9)
            {
                float dist = GetSpellRadius(sSpellRadiusStore.LookupEntry(
                                 spell_info->EffectRadiusIndex[0])) -
                             0.5f;
                if (dist <= 0)
                    return false;
                std::vector<Unit*> targets;
                pet_->SelectUnfriendlyInRange(targets, dist);
                int count = 0;
                for (size_t i = 0; i < targets.size() && count < 2; ++i)
                    if (targets[i]->GetTypeId() == TYPEID_UNIT)
                        ++count;
                if (count < 2)
                    return false;
            }
        }
        // Succubus Spells
        else if (spell_info->SpellFamilyFlags & 0x40000000)
        {
            // Seduction
            if (spell_info->Id == 6358 && victim &&
                (victim->GetDiminishing(DIMINISHING_FEAR) ==
                        DIMINISHING_LEVEL_IMMUNE ||
                    victim->IsImmuneToSpell(spell_info)))
                return false;

            // Lesser Invisibility
            else if (spell_info->Id == 7870 &&
                     (target_ || pet_->isInCombat() ||
                         (pet_->HasAuraType(SPELL_AURA_PERIODIC_DAMAGE) ||
                             (pet_->GetOwner() &&
                                 pet_->GetOwner()->HasAuraType(
                                     SPELL_AURA_PERIODIC_DAMAGE) &&
                                 pet_->GetOwner()->HasAuraType(
                                     SPELL_AURA_SPLIT_DAMAGE_PCT)))))
                return false;

            // Soothing Kiss (Rank 5 can auto-cast on players, previous ranks
            // can't)
            else if (spell_info->SpellIconID == 694)
            {
                const SpellChainNode* node =
                    sSpellMgr::Instance()->GetSpellChainNode(spell_info->Id);
                bool can_soothe =
                    victim && (victim->GetTypeId() == TYPEID_UNIT ||
                                  (node && node->rank >= 5));
                if (!can_soothe || victim->getVictim() != pet_)
                    return false;
            }
        }
        // Felhunter - Devour Magic
        else if (spell_info->SpellIconID == 47)
        {
            if (!(victim && !pet_->IsFriendlyTo(victim) &&
                    victim->HasDispellableBuff(DISPEL_MAGIC)))
                return false;
        }
        // Felhunter - Spell Lock
        else if (spell_info->SpellIconID == 77)
        {
            // GetPowerType() returns mana even if mob has no power type, so we
            // check max mana instead
            if (!(victim && victim->GetMaxPower(POWER_MANA) > 0))
                return false;
        }
        // Imp - Fire Shield
        else if (spell_info->SpellFamilyFlags & 0x800000 &&
                 spell_info->SpellIconID == 16)
        {
            // Implemented in pet_behavior::struck_party_member
            return false;
        }

        break; // Warlock end
    case SPELLFAMILY_GENERIC:
    {
        switch (spell_info->Id)
        {
        // Imp's Phase Shift
        case 4511:
            if (target_ || pet_->isInCombat())
                return false;
            break;
        // Tained Blood (handled in pet_behavior::damaged())
        case 19478:
        case 19655:
        case 19656:
        case 19660:
        case 27280:
            return false;
        // Shell Shield
        case 20601:
            if (pet_->GetHealthPercent() > 50.0f)
                return false;
            break;
        }
        if (spell_info->HasApplyAuraName(SPELL_AURA_MOD_INCREASE_SPEED))
        {
            if (!victim || pet_->IsWithinDistInMap(victim, 8.0f))
                return false;
        }
        break; // Hunter end
    }

    default:
        break;
    }

    return true;
}

bool pet_behavior::is_passive() const
{
    return pet_->GetCharmInfo()->GetReactState() == REACT_PASSIVE;
}

void pet_behavior::update_friendly_targets(Unit* owner)
{
    friendly_targets_.clear();
    friendly_targets_.push_back(owner->GetObjectGuid());

    Group* group;
    if (owner->GetTypeId() == TYPEID_PLAYER &&
        (group = static_cast<Player*>(owner)->GetGroup()))
    {
        // Add all members in our owner's sub-group
        for (auto member : group->members(true))
        {
            if (!group->SameSubGroup(static_cast<Player*>(owner), member) ||
                member == owner || member->GetMap() != owner->GetMap())
                continue;

            friendly_targets_.push_back(member->GetObjectGuid());
        }
    }
}

Unit* pet_behavior::get_owner() const
{
    if (Unit* owner = pet_->GetCharmerOrOwner())
    {
        // If our owner is a totem, we need to get the owner of the totem (for
        // shaman's earth & fire elementals)
        if (owner->GetTypeId() == TYPEID_UNIT &&
            static_cast<Creature*>(owner)->IsTotem())
            return owner->GetOwner();
        return owner;
    }

    return nullptr;
}

void pet_behavior::clear_queued_spell()
{
    if (pet_->GetTargetGuid() == queued_target_ && queued_target_ != target_)
        pet_->SetTargetGuid(ObjectGuid());

    queued_id_ = 0;
    queued_target_.Clear();

    pet_->movement_gens.remove_if([](auto* gen)
        {
            return gen->id() == movement::gen::chase &&
                   gen->priority() == PET_CHASE_SPELL_PRIO;
        });
}

void pet_behavior::died()
{
    clear_queued_spell();

    target_ = ObjectGuid();
    issued_command_ = petai_cmd::none;
    chasing_ = false;
}

// Callback used to implement Imp's Fire Shield, is only invoked for
// pet_behavior (not CreatureAI)
void pet_behavior::struck_party_member(Unit* /*attacker*/, Player* attackee)
{
    // Imp's fire shield
    if (pet_->GetEntry() == 416 && pet_->IsWithinDistInMap(attackee, 30.0f))
    {
        for (unsigned int i = 0; i < pet_->GetPetAutoSpellSize(); ++i)
        {
            const SpellEntry* info =
                sSpellStore.LookupEntry(pet_->GetPetAutoSpellOnPos(i));
            if (!info)
                continue;
            if (info->SpellFamilyName == SPELLFAMILY_WARLOCK &&
                info->SpellFamilyFlags & 0x800000 && info->SpellIconID == 16)
            {
                if (!attackee->has_aura(info->Id))
                    cast(info, attackee, false);

                break;
            }
        }
    }
}

void pet_behavior::damaged(Unit* /*by*/, uint32 /*damage*/)
{
    if (pet_->GetEntry() == 417)
    {
        for (unsigned int i = 0; i < pet_->GetPetAutoSpellSize(); ++i)
        {
            const SpellEntry* info =
                sSpellStore.LookupEntry(pet_->GetPetAutoSpellOnPos(i));
            if (!info)
                continue;
            if (info->SpellIconID == 153)
            {
                if (!pet_->has_aura(info->Id))
                    cast(info, pet_, false);

                break;
            }
        }
    }
}

void pet_behavior::set_chase_on()
{
    // TODO: This shouldn't really be needed (that's why movement is a prio
    // queue after all!)
    if (pet_->movement_gens.has(movement::gen::stopped))
        pet_->movement_gens.remove_all(movement::gen::stopped);
    if (!pet_->movement_gens.has(movement::gen::chase))
        pet_->movement_gens.push(new movement::ChaseMovementGenerator(),
            movement::EVENT_LEAVE_COMBAT);
    chasing_ = true;
}

std::string pet_behavior::debug() const
{
    std::ostringstream ss;
    ss << "Data:\n";
    ss << "Creature id: ";
    if (pet_->get_template()->cid == 0)
        ss << "default template";
    else
        ss << pet_->get_template()->cid;
    ss << " (pet flags: " << pet_->get_template()->pet_flags << ")\n";
    ss << "Behavior: " << pet_->get_template()->behavior
       << " (flags: " << pet_->get_template()->behavior_flags << ")\n";
    ss << "Creature template flags: " << pet_->get_template()->ctemplate_flags
       << "\n";
    ss << "Debug Data: target_: " << target_.GetString() << " | "
       << " stay_: " << stay_ << " | chasing_: " << chasing_
       << " | issued_command_: " << issued_command_;
    if (pet_->get_template()->behavior != PET_BEHAVIOR_MELEE)
        ss << " | spell_dist: " << offensive_spell_dist()
           << " | spell_oom: " << offensive_spell_cost();
    ss << " | command_state: " << pet_->GetCharmInfo()->GetCommandState()
       << " | react_state: " << pet_->GetCharmInfo()->GetReactState()
       << " | queued_id_: " << queued_id_
       << " | queued_target_: " << queued_target_.GetString();
    ss << "\nAuto cast spells: ";
    for (unsigned int i = 0; i < pet_->GetPetAutoSpellSize(); ++i)
        if (const SpellEntry* spell =
                sSpellStore.LookupEntry(pet_->GetPetAutoSpellOnPos(i)))
            ss << spell->SpellName[0] << " (" << spell->Id << ") ";
    ss << "\n";
    return ss.str();
}

std::ostream& operator<<(std::ostream& os, const petai_cmd cmd)
{
    switch (cmd)
    {
    case petai_cmd::none:
        os << "None";
        break;
    case petai_cmd::stay:
        os << "Stay";
        break;
    case petai_cmd::follow:
        os << "Follow";
        break;
    case petai_cmd::attack:
        os << "Attack";
        break;
    case petai_cmd::passive:
        os << "Passive";
        break;
    }
    return os;
}
