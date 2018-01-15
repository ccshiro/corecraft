#include "BehavioralAI.h"
#include "Creature.h"
#include "CreatureAI.h"
#include "movement/IdleMovementGenerator.h"
#include "movement/PointMovementGenerator.h"
#include "movement/TargetedMovementGenerator.h"
#include "Player.h"
#include "ProgressBar.h"
#include "SpecialVisCreature.h"
#include "Totem.h"
#include "TemporarySummon.h"
#include "Spell.h"
#include "Database/DatabaseEnv.h"
#include "maps/checks.h"
#include "maps/visitors.h"
#include <algorithm>
#include <set>

#define BEHAVEAI_STOPPED_PRIO 71

static std::string spell_type_str(SpellType type);

const BehavioralAI::AISpellMap BehavioralAI::aiSpellMap_;

float BehavioralAI::GetSpellMinimumRange(const CreatureAISpell& spell) const
{
    uint32 spellId = GetSpellId(spell);
    if (!spellId)
        return 0.0f;
    if (const SpellEntry* info = sSpellStore.LookupEntry(spellId))
        if (const SpellRangeEntry* range =
                sSpellRangeStore.LookupEntry(info->rangeIndex))
            return GetSpellMinRange(range);
    return 0.0f;
}

float BehavioralAI::GetSpellMaximumRange(const CreatureAISpell& spell) const
{
    uint32 spellId = GetSpellId(spell);
    if (!spellId)
        return 0.0f;
    if (const SpellEntry* info = sSpellStore.LookupEntry(spellId))
        if (const SpellRangeEntry* range =
                sSpellRangeStore.LookupEntry(info->rangeIndex))
            return GetSpellMaxRange(range);
    return 0.0f;
}

uint32 BehavioralAI::NearbyEnemiesCount(float radius) const
{
    uint32 count = 0;
    maps::visitors::simple<Player, Creature, Pet, SpecialVisCreature,
        TemporarySummon>{}(owner_, radius, [this, radius, &count](auto&& elem)
        {
            if (owner_->IsWithinDist(elem, radius, true, false) &&
                maps::checks::friendly_status{
                    owner_, maps::checks::friendly_status::hostile}(elem))
                ++count;
        });
    return count;
}

void BehavioralAI::LoadBehaviors()
{
    // Can only be loaded once at startup, if we wish to change this we need to
    // pay attention to
    // the cooldownEndTimestamps_ (its indices are built during construction) so
    // we don't run out of bounds
    assert(aiSpellMap_.empty());

    uint32 loadedTotal = 0;
    std::unique_ptr<QueryResult> result(WorldDatabase.PQuery(
        "SELECT creature_id, spell_id, heroic_spell_id, type, priority, "
        "cooldown_min, cooldown_max, target_settings, phase_mask "
        "FROM creature_ai_spells"));
    if (result)
    {
        BarGoLink bar(result->GetRowCount());
        do
        {
            bar.step();
            Field* fields = result->Fetch();
            CreatureAISpell spellData;
            uint32 creatureId = fields[0].GetUInt32();
            spellData.spellId = fields[1].GetUInt32();
            spellData.heroicSpellId = fields[2].GetUInt32();
            spellData.type = (SpellType)fields[3].GetUInt32();
            spellData.priority = fields[4].GetUInt32();
            spellData.cooldown_min = fields[5].GetUInt32();
            spellData.cooldown_max = fields[6].GetUInt32();
            spellData.target_settings = fields[7].GetUInt32();
            spellData.phase_mask = fields[8].GetUInt32();

            if (spellData.type >= AI_SPELL_MAX)
            {
                logging.error(
                    "Creature %u has invalid spell data for spell %u in "
                    "creature_ai_spells.",
                    creatureId, spellData.spellId);
                continue;
            }

            if (spellData.type == AI_SPELL_MY_PET && spellData.phase_mask != 0)
            {
                logging.warning(
                    "Creature %u has phase mask for spell %u in "
                    "creature_ai_spells, with type MY_PET, which cannot have a "
                    "phase requirement.",
                    creatureId, spellData.spellId);
                spellData.phase_mask = 0;
                // no continue
            }

            ++loadedTotal;

            const_cast<AISpellMap&>(aiSpellMap_)[creatureId].push_back(
                spellData);

        } while (result->NextRow());

        // Sort all entries according to their priority
        for (auto& elem : const_cast<AISpellMap&>(aiSpellMap_))
            std::sort(
                elem.second.rbegin(), elem.second.rend()); // Sort descendingly
    }
    logging.info("Loaded %u Creature BehavioralAI Spells\n", loadedTotal);
}

BehavioralAI::BehavioralAI(Creature* creature)
  : toggledOn_(false), owner_(creature), shouldDoMovement_(false),
    isHeroic_(!creature->GetMap()->IsRegularDifficulty()), globalCooldown_(0),
    maxRange_(0), runX_(0), runY_(0), runZ_(0), phase_(0), minMana_(0),
    behavior_(BEHAVIOR_WARRIOR), chaseMana_(false), chaseLoS_(false),
    chaseNear_(false), chaseRange_(false), chaseSchoolsLocked_(false),
    running_(false)
{
    // Fill out the cooldown map with all zeros
    auto find = aiSpellMap_.find(owner_->GetEntry());
    if (find != aiSpellMap_.end())
    {
        toggledOn_ = true;

        cooldownEndTimestamps_.reserve(find->second.size());
        for (auto& elem : find->second)
        {
            cooldownEndTimestamps_.push_back(0);

            // Do not save non-offensive spells in our available schools mask
            if (elem.type != AI_SPELL_OFFENSIVE)
                continue;
            // Add any non physical only school to our available schools
            const SpellEntry* info = sSpellStore.LookupEntry(elem.spellId);
            if (info && info->SchoolMask &&
                info->SchoolMask != SPELL_SCHOOL_MASK_NORMAL)
            {
                bool add = true;
                for (auto& _j : availableSchools_)
                    if (_j & info->SchoolMask)
                    {
                        add = false;
                        break;
                    }
                if (add)
                    availableSchools_.push_back(info->SchoolMask);
            }
        }

        // Adopt the behavior pattern associated with our creature's class
        switch (creature->getClass())
        {
        case CLASS_WARRIOR:
            behavior_ = BEHAVIOR_WARRIOR;
            break;
        case CLASS_PALADIN:
            behavior_ = BEHAVIOR_PALADIN;
            break;
        case CLASS_MAGE:
            behavior_ = BEHAVIOR_MAGE;
            break;
        case CLASS_ROGUE:
            behavior_ = BEHAVIOR_ROGUE;
            break;
        default:
            logging.error(
                "Creature with entry %u using Behavioral AI has an invalid "
                "creature_template.unit_class.",
                creature->GetEntry());
            break;
        }
    }
}

bool BehavioralAI::OnAttackStart()
{
    if (toggledOn_)
    {
        InitMovement();
        shouldDoMovement_ = true;
        return true;
    }
    else
    {
        shouldDoMovement_ = false;
        return false;
    }
}

void BehavioralAI::OnReset()
{
    for (auto& elem : cooldownEndTimestamps_)
        elem = 0;
    // Reset all combat related flags
    globalCooldown_ = 0;
    shouldDoMovement_ = false;
    chaseMana_ = false;
    chaseLoS_ = false;
    chaseNear_ = false;
    chaseRange_ = false;
    chaseSchoolsLocked_ = false;
    running_ = false;
    oldVictim_ = ObjectGuid();

    // Handle AI_SPELL_MY_PET spells
    if (owner_->IsInWorld())
        SummonMyPet();
}

bool BehavioralAI::IgnoreTarget(Unit* target) const
{
    if (behavior_ == BEHAVIOR_MAGE)
    {
        uint32 mask = 0;
        if (HaveAvailableSpells(false, &mask))
            return target->IsImmunedToDamage((SpellSchoolMask)mask);
    }

    return target->IsImmunedToDamage(owner_->GetMeleeDamageSchoolMask());
}

void BehavioralAI::SetCooldown(
    uint32 spell_id, uint32 phase_mask, uint32 cooldown)
{
    auto find = aiSpellMap_.find(owner_->GetEntry());
    if (find == aiSpellMap_.end())
        return;

    uint32 timestamp = WorldTimer::getMSTime() + cooldown;
    for (AISpellVector::size_type i = 0; i < find->second.size(); ++i)
    {
        uint32 spell = GetSpellId(find->second[i]);
        if (spell_id != spell)
            continue;

        int spell_phase_mask = find->second[i].phase_mask;
        if (phase_mask != 0 && (spell_phase_mask & phase_mask) == 0)
            continue;

        cooldownEndTimestamps_[i] = timestamp;
    }
}

void BehavioralAI::InternalUpdate(uint32 diff)
{
    if (!toggledOn_)
        return;

    if (owner_->IsAffectedByThreatIgnoringCC())
        return;

    if (owner_->AI() && owner_->AI()->IsPacified())
        return;

    if (shouldDoMovement_)
        UpdateMovement();

    if (globalCooldown_ < diff)
        globalCooldown_ = 0;
    else
    {
        globalCooldown_ -= diff;
        return;
    }

    CastSpell();
}

void BehavioralAI::InitMovement()
{
    // Warriors and paladins are melee mobs, as are mobs that somehow have no
    // spells
    if (behavior_ == BEHAVIOR_PALADIN || behavior_ == BEHAVIOR_WARRIOR ||
        aiSpellMap_.find(owner_->GetEntry()) == aiSpellMap_.end())
    {
        maxRange_ = 0.0f;
        minMana_ = 0;
    }
    else
    {
        const AISpellVector& spells =
            aiSpellMap_.find(owner_->GetEntry())->second;
        maxRange_ = 100.0f;
        minMana_ = 0;

        for (const auto& spell : spells)
        {
            if (GetSpellId(spell) == 0)
                continue;

            if (spell.type == AI_SPELL_OFFENSIVE &&
                !(spell.target_settings & AI_SPELL_IGNORE_FOR_MIN_MANA) &&
                GetSpellCost(spell) > minMana_)
                minMana_ = GetSpellCost(spell);
            if (spell.type == AI_SPELL_OFFENSIVE &&
                !(spell.target_settings & AI_SPELL_IGNORE_FOR_MAX_RANGE) &&
                GetSpellMaximumRange(spell) < maxRange_)
                maxRange_ = GetSpellMaximumRange(spell);
        }

        // min mana cannot be more than 25% of creature's mana
        if (behavior_ == BEHAVIOR_MAGE && minMana_ &&
            owner_->GetMaxPower(POWER_MANA) * 0.25f < minMana_)
        {
            logging.error(
                "Creature %s using behavioral AI as a mage and min mana is "
                "more than 25%% of creature's mana pool!",
                owner_->GetObjectGuid().GetString().c_str());
            minMana_ = owner_->GetMaxPower(POWER_MANA) * 0.25f;
        }
    }

    if (maxRange_ > 2.0f)
        maxRange_ -= 2.0f;

    if (!owner_->movement_gens.has(movement::gen::chase))
        owner_->movement_gens.push(new movement::ChaseMovementGenerator(),
            movement::EVENT_LEAVE_COMBAT);

    if (maxRange_ != 0.0f)
        if (!owner_->movement_gens.has(movement::gen::stopped))
            owner_->movement_gens.push(new movement::StoppedMovementGenerator(),
                movement::EVENT_LEAVE_COMBAT, BEHAVEAI_STOPPED_PRIO);
}

void BehavioralAI::UpdateMovement()
{
    // If we have no target weird stuff will happen
    if (owner_->getVictim() == nullptr)
        return;

    switch (behavior_)
    {
    // Melee mobs don't give a shit
    case BEHAVIOR_PALADIN:
    case BEHAVIOR_WARRIOR:
        return;
    case BEHAVIOR_ROGUE:
    {
        // Some rogues are melee only
        if (maxRange_ == 0.0f || owner_->IsNonMeleeSpellCasted(false))
            return;

        // Rogues start chasing when they get close to the target
        float combat_dist = owner_->GetCombatDistance(owner_->getVictim());
        if (chaseNear_ && combat_dist > 10.0f)
            chaseNear_ = false;
        else if (!chaseNear_ && combat_dist < 5.5f)
            chaseNear_ = true;

        break;
    }
    case BEHAVIOR_MAGE:
    {
        // Mages move in melee range when oom
        if (chaseMana_ &&
            ((float)owner_->GetPower(POWER_MANA) /
                (float)owner_->GetMaxPower(POWER_MANA)) > 0.25f)
            chaseMana_ = false;
        else if (!chaseMana_ && owner_->GetPower(POWER_MANA) < minMana_)
            chaseMana_ = true;

        // When all spell schools are locked or when we are silenced we should
        // run to the target
        if (!owner_->IsNonMeleeSpellCasted(false))
        {
            if (chaseSchoolsLocked_ && HaveAvailableSpells(true))
                chaseSchoolsLocked_ = false;
            else if (!chaseSchoolsLocked_ && !HaveAvailableSpells(false))
                chaseSchoolsLocked_ = true;
        }

        break;
    }
    default:
        assert(false);
        return;
    }

    // Mages and rogues run away from in melee-range targets if they're locked
    // down (rooted, stunned or confused)
    if ((behavior_ == BEHAVIOR_ROGUE || behavior_ == BEHAVIOR_MAGE) &&
        !chaseSchoolsLocked_ && !chaseMana_ &&
        !owner_->IsNonMeleeSpellCasted(false))
    {
        bool targetStuck = owner_->getVictim()->hasUnitState(
            UNIT_STAT_STUNNED | UNIT_STAT_ROOT | UNIT_STAT_CONFUSED);
        if (!running_ && targetStuck &&
            owner_->CanReachWithMeleeAttack(owner_->getVictim()))
        {
            float range =
                behavior_ == BEHAVIOR_ROGUE ? maxRange_ * 0.66f : 8.0f;
            auto pos = owner_->getVictim()->GetPoint(owner_, range);
            runX_ = pos.x;
            runY_ = pos.y;
            runZ_ = pos.z;
            if (!owner_->IsWithinDist3d(runX_, runY_, runZ_,
                    owner_->GetMeleeReach(owner_->getVictim(), 1.0f)))
                running_ = true;
        }
        else if (running_ &&
                 (!targetStuck ||
                     owner_->movement_gens.top_id() != movement::gen::point))
            running_ = false;
    }

    // Every mob should start chasing when it's out of range
    if (chaseRange_ &&
        owner_->IsWithinDistInMap(owner_->getVictim(), maxRange_ * 0.66f, true))
        chaseRange_ = false;
    else if (!chaseRange_ &&
             !owner_->IsWithinDistInMap(owner_->getVictim(), maxRange_, true))
        chaseRange_ = true;

    // Every mob should start chasing when LOS is broken
    if (chaseLoS_ && owner_->IsWithinWmoLOSInMap(owner_->getVictim()))
        chaseLoS_ = false;
    else if (!chaseLoS_ && !owner_->IsWithinWmoLOSInMap(owner_->getVictim()))
        chaseLoS_ = true;

    // Run away movement generator logic:
    // Case: We need to run away, push on point move gen
    if (running_ && !owner_->movement_gens.has(movement::gen::point, 75))
    {
        owner_->movement_gens.push(
            new movement::PointMovementGenerator(
                88001240, runX_, runY_, runZ_, true, true),
            movement::EVENT_LEAVE_COMBAT, 75);
    }
    // Case: We ran way, pop the point one
    else if (!running_ && owner_->movement_gens.has(movement::gen::point))
    {
        owner_->movement_gens.remove_if([](const movement::Generator* gen)
            {
                if (auto point_gen =
                        dynamic_cast<const movement::PointMovementGenerator*>(
                            gen))
                    return point_gen->GetId() == 88001240;
                return false;
            });
    }

    movement::Generator* stopped_gen = nullptr;
    for (auto& gen : owner_->movement_gens)
        if (gen->id() == movement::gen::stopped &&
            gen->priority() == BEHAVEAI_STOPPED_PRIO)
        {
            stopped_gen = gen;
            break;
        }
    // Stopped movement generator logic:
    // Case: If we have a stopped gen, and we wanna chase, pop it
    if (IsChasing() && stopped_gen)
    {
        owner_->movement_gens.remove(stopped_gen);
    }
    // Case: We wanna be stopped, push a stopped gen
    else if (!IsChasing() && !stopped_gen)
    {
        owner_->movement_gens.push(new movement::StoppedMovementGenerator(),
            movement::EVENT_LEAVE_COMBAT, BEHAVEAI_STOPPED_PRIO);
    }

    oldVictim_ = owner_->getVictim()->GetObjectGuid();
}

bool BehavioralAI::HaveAvailableSpells(bool now, uint32* mask_out) const
{
    auto find = aiSpellMap_.find(owner_->GetEntry());
    if (find == aiSpellMap_.end())
        return false;

    // If now is true we will check if we can cast a spell right now,
    // otherwise we will check if we can cast a spell in 4 seconds
    uint32 currentTimestamp = WorldTimer::getMSTime();
    if (!now)
        currentTimestamp += 4000;

    for (AISpellVector::size_type i = 0; i < find->second.size(); ++i)
    {
        uint32 spellId = GetSpellId(find->second[i]);
        if (!spellId)
            continue;

        if (find->second[i].type != AI_SPELL_OFFENSIVE)
            continue;

        int phase_mask = find->second[i].phase_mask;
        if (phase_mask != 0 &&
            (!phase_ || ((1 << (phase_ - 1)) & phase_mask) == 0))
            continue;

        if (cooldownEndTimestamps_[i] > currentTimestamp)
            continue;

        const SpellEntry* info =
            sSpellStore.LookupEntry(find->second[i].spellId);
        if (!info)
            continue;

        if (info->PreventionType == SPELL_PREVENTION_TYPE_SILENCE)
        {
            if (owner_->HasAuraType(SPELL_AURA_MOD_SILENCE))
                continue;

            if (owner_->IsSpellSchoolLocked((SpellSchoolMask)info->SchoolMask))
                continue;
        }

        if (mask_out)
            *mask_out |= info->SchoolMask;

        return true;
    }
    return false;
}

void BehavioralAI::CastSpell()
{
    auto find = aiSpellMap_.find(owner_->GetEntry());
    if (find == aiSpellMap_.end())
        return;

    if (owner_->IsNonMeleeSpellCasted(false))
        return;

    if (running_)
        return;

    uint32 currentTimestamp = WorldTimer::getMSTime();

    Unit* target = nullptr;
    const CreatureAISpell* spell = nullptr;
    AISpellVector::size_type spellIndex = 0;

    // Note: Spells are already sorted according to priority
    for (; spellIndex < find->second.size(); ++spellIndex)
    {
        uint32 spellId = GetSpellId(find->second[spellIndex]);
        int phase_mask = find->second[spellIndex].phase_mask;
        if (!spellId)
            continue;

        // Skip spells on cooldown
        if (cooldownEndTimestamps_[spellIndex] > currentTimestamp)
            continue;

        if (phase_mask != 0 &&
            (!phase_ || ((1 << (phase_ - 1)) & phase_mask) == 0))
            continue;

        // If we're chasing we can only use instant or healing spells
        if (IsChasing())
        {
            if (const SpellEntry* info = sSpellStore.LookupEntry(spellId))
                if (GetSpellCastTime(info, nullptr) != 0 &&
                    find->second[spellIndex].type != AI_SPELL_HEALING)
                    continue;
        }

        // Spells with cooldown longer than the initial cooldown breakpoint are
        // given an initial cooldown
        if ((find->second[spellIndex].cooldown_min >=
                    INTIAL_COOLDOWN_BREAK_POINT ||
                find->second[spellIndex].cooldown_max >=
                    INTIAL_COOLDOWN_BREAK_POINT) &&
            cooldownEndTimestamps_[spellIndex] == 0)
        {
            // Use half of cooldown as initial cooldown
            uint32 lowerEnd = find->second[spellIndex].cooldown_min / 2;
            uint32 higherEnd = find->second[spellIndex].cooldown_max / 2;
            cooldownEndTimestamps_[spellIndex] =
                currentTimestamp + urand(lowerEnd, higherEnd);
            continue;
        }

        // GetSpellTarget returns NULL if no target found or if we should not
        // cast that spell
        Unit* temp = GetSpellTarget(find->second[spellIndex]);
        if (temp && owner_->AI() &&
            owner_->AI()->CanCastSpell(temp, spellId, false) == CAST_OK)
        {
            target = temp;
            spell = &find->second[spellIndex];
            break;
        }
    }

    if (!target || !spell)
        return;
    uint32 spellId = GetSpellId(*spell);
    auto info = sSpellStore.LookupEntry(spellId);
    if (!info)
        return;

    // Cast spell and put it on cooldown
    if (spell->target_settings & AI_SPELL_USE_TARGET_COORDS)
        owner_->CastSpell(
            target->GetX(), target->GetY(), target->GetZ(), info, false);
    else
        owner_->CastSpell(target, info, false);
    cooldownEndTimestamps_[spellIndex] =
        currentTimestamp + urand(spell->cooldown_min, spell->cooldown_max);
    // Only set GCD for instant casts
    if (GetSpellCastTime(info, nullptr, true) == 0)
        globalCooldown_ = 1500;
}

Unit* BehavioralAI::GetSpellTarget(const CreatureAISpell& spell) const
{
    switch (spell.type)
    {
    case AI_SPELL_OFFENSIVE:
    case AI_SPELL_DOT:
    {
        AttackingTarget at = ATTACKING_TARGET_TOPAGGRO;
        uint32 offset = 0;
        SelectFlags sf = SELECT_FLAG_IN_LOS; // Added by default (possible to
                                             // remove it by specifying the
                                             // ignore LoS flag)

        if (spell.target_settings & AI_SPELL_TS_RANDOM_AGGRO)
            at = ATTACKING_TARGET_RANDOM;
        else if (spell.target_settings & AI_SPELL_TS_SECOND_AGGRO)
        {
            at = ATTACKING_TARGET_TOPAGGRO;
            offset = 1;
        }
        else if (spell.target_settings & AI_SPELL_TS_RANDOM_NOT_TOP_AGGRO)
        {
            at = ATTACKING_TARGET_RANDOM;
            offset = 1;
        }
        else if (spell.target_settings & AI_SPELL_TS_BOTTOM_AGGRO)
        {
            at = ATTACKING_TARGET_RANDOM;
            offset = 1;
        }

        if (spell.target_settings & AI_SPELL_TS_MANA_TARGETS)
            sf = SELECT_FLAG_POWER_MANA;
        else if (spell.target_settings & AI_SPELL_TS_RAGE_TARGETS)
            sf = SELECT_FLAG_POWER_RAGE;
        else if (spell.target_settings & AI_SPELL_TS_ENERGY_TARGETS)
            sf = SELECT_FLAG_POWER_ENERGY;

        if (spell.target_settings & AI_SPELL_TS_IN_MELEE_RANGE)
            sf = SelectFlags(sf | SELECT_FLAG_IN_MELEE_RANGE);
        else if (spell.target_settings & AI_SPELL_TS_NOT_IN_MELEE_RANGE)
            sf = SelectFlags(sf | SELECT_FLAG_NOT_IN_MELEE_RANGE);

        if (spell.target_settings & AI_SPELL_IGNORE_LOS)
            sf = SelectFlags(sf & ~SELECT_FLAG_IN_LOS);

        if (spell.target_settings & AI_SPELL_TS_IN_FRONT)
            sf = SelectFlags(sf | SELECT_FLAG_IN_FRONT);

        if (spell.target_settings & AI_SPELL_AOE_SPELL)
        {
            float range = GetSpellMaximumRange(spell);
            range = (range ? range : 5.0f);
            if (NearbyEnemiesCount(range) < 2)
                return nullptr;
        }

        uint32 spellId = GetSpellId(spell);
        if (at == ATTACKING_TARGET_TOPAGGRO && offset == 0)
        {
            // We cannot use select main tank; highest aggro might be
            // unselectable by SelectAttackingTarget

            Unit* target = owner_->getVictim();
            if (!target)
                return nullptr;

            // Check flags that apply even to main tank
            if (spell.target_settings &
                (AI_SPELL_TS_MANA_TARGETS | AI_SPELL_TS_RAGE_TARGETS |
                    AI_SPELL_TS_ENERGY_TARGETS))
            {
                if (sf & SELECT_FLAG_POWER_MANA &&
                    target->getPowerType() != POWER_MANA)
                    return nullptr;
                if (sf & SELECT_FLAG_POWER_RAGE &&
                    target->getPowerType() != POWER_RAGE)
                    return nullptr;
                if (sf & SELECT_FLAG_POWER_ENERGY &&
                    target->getPowerType() != POWER_ENERGY)
                    return nullptr;
            }
            if (spell.target_settings &
                (AI_SPELL_TS_IN_MELEE_RANGE | AI_SPELL_TS_NOT_IN_MELEE_RANGE))
            {
                if (spell.target_settings & AI_SPELL_TS_IN_MELEE_RANGE &&
                    !owner_->CanReachWithMeleeAttack(target))
                    return nullptr;
                if (spell.target_settings & AI_SPELL_TS_NOT_IN_MELEE_RANGE &&
                    owner_->CanReachWithMeleeAttack(target))
                    return nullptr;
            }

            if (spell.type == AI_SPELL_DOT && target->has_aura(spell.spellId))
                return nullptr;

            return target;
        }
        if (spell.type == AI_SPELL_DOT)
            sf = SelectFlags(sf | SELECT_FLAG_IGNORE_TARGETS_WITH_AURA);
        return owner_->SelectAttackingTarget(at, offset, spellId, sf);
    }

    case AI_SPELL_HEALING:
    {
        Unit* target = nullptr;
        float range = GetSpellMaximumRange(spell);

        // Don't cast any heals if everyone's above 80%
        maps::checks::hurt_friend check{owner_,
            static_cast<bool>(spell.target_settings & AI_SPELL_EXCLUDE_SELF),
            0.8f};

        target = maps::visitors::yield_best_match<Creature, Creature,
            SpecialVisCreature, TemporarySummon>{}(owner_, range, check);

        return target;
    }

    case AI_SPELL_BENEFICIAL:
    {
        uint32 spellId = GetSpellId(spell);
        float range = GetSpellMaximumRange(spell);

        auto proto = sSpellStore.LookupEntry(spellId);
        if (!proto)
            return nullptr;

        auto check = [proto, this, &spell, spellId](Unit* u)
        {
            if ((proto->HasAttribute(SPELL_ATTR_EX_CANT_TARGET_SELF) ||
                    spell.target_settings & AI_SPELL_EXCLUDE_SELF) &&
                u == owner_)
                return false;

            return !u->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE) &&
                   !u->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE) &&
                   u->isAlive() && u->isInCombat() && !owner_->IsHostileTo(u) &&
                   !u->has_aura(spellId);
        };

        return maps::visitors::yield_single<Creature, Creature,
            SpecialVisCreature, TemporarySummon>{}(owner_, range, check);
    }
    case AI_SPELL_CROWD_CONTROL:
    {
        float range = GetSpellMaximumRange(spell);
        auto targets =
            maps::visitors::yield_set<Player>{}(owner_, range, [](Player* p)
                {
                    return p->isAlive();
                });

        uint32 spellId = GetSpellId(spell);
        const SpellEntry* spellInfo = sSpellStore.LookupEntry(spellId);
        if (!spellInfo)
            return nullptr;

        if (spell.target_settings & AI_SPELL_CC_ONLY_MAIN_TANK)
            return owner_->getVictim();

        std::vector<Player*> possible_targets;
        for (auto& target : targets)
            if ((target)->IsHostileTo(owner_) &&
                !(target)->IsImmuneToSpell(spellInfo) &&
                !(target)->HasBreakOnDamageCCAura() // FIXME:
                // (*itr)->HasBreakOnDamageCCAura()
                // should be replaced by a more
                // deliberate CC check
                &&
                owner_->IsWithinWmoLOSInMap(target) &&
                (owner_->getVictim() != target ||
                    spell.target_settings & AI_SPELL_CC_INCLUDE_MAIN_TANK))
                possible_targets.push_back(target);

        if (!possible_targets.empty())
            return possible_targets[urand(0, possible_targets.size() - 1)];

        return nullptr;
    }

    case AI_SPELL_SELF_ENHANCEMENT:
    {
        uint32 spellId = GetSpellId(spell);
        const SpellEntry* spellInfo = sSpellStore.LookupEntry(spellId);
        if (!spellInfo)
            return nullptr;
        if (spellInfo->HasEffect(SPELL_EFFECT_APPLY_AURA) ||
            spellInfo->HasEffect(SPELL_EFFECT_APPLY_AREA_AURA_PARTY))
        {
            if (spellInfo->StackAmount > 1)
            {
                // Can spell stack further?
                if (AuraHolder* holder = owner_->get_aura(spellId))
                    if (holder->GetStackAmount() >= spellInfo->StackAmount)
                        return nullptr;
            }
            else if (owner_->has_aura(spellId))
                return nullptr;
        }

        if (spell.target_settings &
            (AI_SPELL_AOE_SPELL | AI_SPELL_VICTIM_IN_RANGE))
        {
            float range = GetSpellMaximumRange(spell);
            range = (range ? range : 5.0f);
            if (spell.target_settings & AI_SPELL_AOE_SPELL &&
                NearbyEnemiesCount(range) < 2)
                return nullptr;
            else if (spell.target_settings & AI_SPELL_VICTIM_IN_RANGE &&
                     owner_->getVictim())
            {
                if (range == 5.0f &&
                    !owner_->CanReachWithMeleeAttack(owner_->getVictim()))
                    return nullptr;
                else if (range != 5.0f &&
                         !owner_->IsWithinDistInMap(owner_->getVictim(), range))
                    return nullptr;
            }
        }

        return owner_;
    }

    case AI_SPELL_SILENCE:
    {
        float range = GetSpellMaximumRange(spell);
        auto targets =
            maps::visitors::yield_set<Player>{}(owner_, range, [](Player* p)
                {
                    return p->isAlive();
                });

        for (auto& target : targets)
            if ((target)->IsHostileTo(owner_) &&
                (target)->IsNonMeleeSpellCasted(false))
                return target;
        return nullptr;
    }

    case AI_SPELL_DISPEL:
    {
        uint32 spellId = GetSpellId(spell);
        if (!spellId)
            return nullptr;

        // Calculate the dispel types this spell can remove
        std::set<DispelType> dispelTypes;
        if (const SpellEntry* spellInfo = sSpellStore.LookupEntry(spellId))
        {
            for (uint32 i = 0; i < MAX_EFFECT_INDEX; ++i)
                if (spellInfo->Effect[i] == SPELL_EFFECT_DISPEL)
                    dispelTypes.insert(
                        (DispelType)spellInfo->EffectMiscValue[i]);
        }
        if (dispelTypes.empty())
            return nullptr;

        // What target type do we want?
        std::vector<Unit*> targets;
        bool useFriendly = true, useHostile = false; // Default: friendly
        if (spell.target_settings & AI_SPELL_DISPEL_HOSTILE)
        {
            useHostile = true;
            useFriendly =
                false; // Can be reset to true in next if, having both is fine
        }
        if (spell.target_settings & AI_SPELL_DISPEL_FRIENDLY)
            useFriendly = true;

        // Lookup all nearby targets
        float range = GetSpellMaximumRange(spell);
        auto allTargets = maps::visitors::yield_set<Unit, Player, Creature, Pet,
            SpecialVisCreature, TemporarySummon>{}(owner_, range,
            [](auto&& elem)
            {
                return elem->isAlive();
            });

        // Check which ones fit our criteria
        for (auto& allTarget : allTargets)
        {
            Unit* u = allTarget;
            bool friendly = u->IsFriendlyTo(owner_) ||
                            owner_->getFaction() == u->getFaction();
            bool hostile = u->IsHostileTo(owner_);
            if (friendly && !useFriendly)
                continue;
            if (hostile && !useHostile)
                continue;

            for (const auto& dispelType : dispelTypes)
            {
                if (friendly && u->HasDispellableDebuff(dispelType))
                    targets.push_back(allTarget);
                if (hostile && owner_->getThreatManager().hasTarget(u) &&
                    u->HasDispellableBuff(dispelType))
                    targets.push_back(allTarget);
            }
        }

        if (targets.empty())
            return nullptr;

        // Pick a random target if multiple exists
        return targets[urand(0, targets.size() - 1)];
    }

    case AI_SPELL_AREA_OF_EFFECT:
    {
        uint32 spell_id = GetSpellId(spell);
        if (!spell_id)
            return nullptr;
        const SpellEntry* info = sSpellStore.LookupEntry(spell_id);
        if (!info)
            return nullptr;

        bool positive = IsPositiveSpell(info);
        float radius = GetSpellRadius(info);

        if (spell.target_settings & AI_SPELL_AOE_MAIN_TARGET)
        {
            if (!owner_->getVictim())
                return nullptr;
            return owner_->IsWithinDist(
                       owner_->getVictim(), radius, true, false) ?
                       owner_ :
                       nullptr;
        }

        auto all_units = maps::visitors::yield_set<Unit, Player, Creature, Pet,
            SpecialVisCreature, TemporarySummon>{}(owner_, radius,
            [](auto&& elem)
            {
                return elem->isAlive();
            });

        uint32 found_targets = 0;

        for (auto u : all_units)
        {
            if (u == owner_)
                continue;
            if (!owner_->IsWithinDist(u, radius, true, false))
                continue;

            if (positive && !u->IsHostileTo(owner_))
                ++found_targets;
            else if (!positive && u->IsHostileTo(owner_))
                ++found_targets;
        }

        // For friendly AoEs owner_ counts as one
        if (positive)
            ++found_targets;

        int min = 2;
        if (spell.target_settings & AI_SPELL_AOE_ANYONE_IN_RANGE)
            min = 1;

        if (found_targets >= min)
            return owner_;
        return nullptr;
    }

    default:
        break;
    }

    return nullptr;
}

void BehavioralAI::ToggleBehavior(uint32 state)
{
    toggledOn_ = state;

    // Remove our stopped generator if toggling off
    if (!state)
    {
        owner_->movement_gens.remove_if([](const movement::Generator* gen)
            {
                return gen->id() == movement::gen::stopped &&
                       gen->priority() == BEHAVEAI_STOPPED_PRIO;
            });
    }
}

void BehavioralAI::ChangeBehavior(uint32 behavior)
{
    if (behavior != BEHAVIOR_MAGE)
    {
        chaseMana_ = false;
        chaseSchoolsLocked_ = false;
    }
    if (behavior != BEHAVIOR_ROGUE)
        chaseNear_ = false;

    if (behavior < AI_BEHAVIOR_MAX)
        behavior_ = (BehaviorAIType)behavior;
    else
        logging.error(
            "Creature with id %i tried setting behavior pattern to unallowed "
            "number",
            owner_->GetEntry());
}

uint32 BehavioralAI::GetSpellCost(const CreatureAISpell& spell) const
{
    uint32 spellId = GetSpellId(spell);
    if (const SpellEntry* info = sSpellStore.LookupEntry(spellId))
        return Spell::CalculatePowerCost(info, owner_);
    return 0;
}

void BehavioralAI::SummonMyPet()
{
    auto find = aiSpellMap_.find(owner_->GetEntry());
    if (find == aiSpellMap_.end())
        return;

    for (auto& spell : find->second)
    {
        if (spell.type == AI_SPELL_MY_PET)
        {
            owner_->CastSpell(owner_, spell.spellId, true);
            auto owner = owner_;
            owner->queue_action_ticks(3, [this, owner]()
                {
                    auto pet = owner->GetPet();
                    if (pet && !pet->movement_gens.has(movement::gen::follow))
                        pet->movement_gens.push(
                            new movement::FollowMovementGenerator(owner));
                });

            return;
        }
    }
}

std::string BehavioralAI::debug() const
{
    /* : toggledOn_(false), owner_(creature), shouldDoMovement_(false),
      isHeroic_(!creature->GetMap()->IsRegularDifficulty()), globalCooldown_(0),
      maxRange_(0), runX_(0), runY_(0), runZ_(0), phase_(0), minMana_(0),
      behavior_(BEHAVIOR_WARRIOR), chaseMana_(false), chaseLoS_(false),
      chaseNear_(false), chaseRange_(false), chaseSchoolsLocked_(false),
      running_(false) */
    std::string s;

    auto find = aiSpellMap_.find(owner_->GetEntry());
    if (find == aiSpellMap_.end())
        return "No Behavioral AI";

    auto bprint = [](bool b)
    {
        return b ? "true" : "false";
    };

    s = std::string("on: ") + bprint(toggledOn_) + ", movement: " +
        bprint(shouldDoMovement_) + ", class: " +
        std::to_string(owner_->getClass()) + ", phase: " +
        std::to_string(phase_) + "\n";

    uint32 currentTimestamp = WorldTimer::getMSTime();

    // Note: Spells are already sorted according to priority
    for (AISpellVector::size_type spellIndex = 0;
         spellIndex < find->second.size(); ++spellIndex)
    {
        uint32 spellId = GetSpellId(find->second[spellIndex]);
        int phase_mask = find->second[spellIndex].phase_mask;
        if (!spellId)
            continue;
        auto info = sSpellStore.LookupEntry(spellId);
        if (!info)
            continue;

        auto cd_secs = int32(((int64)cooldownEndTimestamps_[spellIndex] -
                                 (int64)currentTimestamp) /
                             1000);
        if (cd_secs < 0)
            cd_secs = 0;

        s += std::string(info->SpellName[0]) + "(" + std::to_string(spellId) +
             "): type=" + std::to_string(find->second[spellIndex].type) +
             ", cd=" + std::to_string(cd_secs) + " sec, phase=" +
             std::to_string(phase_mask) + ", settings=" +
             std::to_string(find->second[spellIndex].target_settings) + "\n";
    }

    return s;
}

static std::string spell_type_str(SpellType type)
{
    std::string s;
    switch (type)
    {
    case AI_SPELL_OFFENSIVE:
        s = "offensive";
        break;
    case AI_SPELL_HEALING:
        s = "healing";
        break;
    case AI_SPELL_BENEFICIAL:
        s = "beneficial";
        break;
    case AI_SPELL_CROWD_CONTROL:
        s = "crowd control";
        break;
    case AI_SPELL_SELF_ENHANCEMENT:
        s = "self enhancement";
        break;
    case AI_SPELL_SILENCE:
        s = "silence";
        break;
    case AI_SPELL_DISPEL:
        s = "dispel";
        break;
    case AI_SPELL_AREA_OF_EFFECT:
        s = "are of effect";
        break;
    case AI_SPELL_MY_PET:
        s = "pet";
        break;
    case AI_SPELL_DOT:
        s = "dot";
        break;
    default:
        s = "INVALID";
        break;
    }
    s += " (" + std::to_string((int)type) + ")";
    return s;
}
