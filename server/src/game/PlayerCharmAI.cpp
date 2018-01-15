/*
 * Copyright (C) 2013 CoreCraft <https://www.worldofcorecraft.com/>
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

#include "PlayerCharmAI.h"
#include "Creature.h"
#include "movement/IdleMovementGenerator.h"
#include "movement/TargetedMovementGenerator.h"
#include "Pet.h"
#include "Player.h"
#include "PlayerCharmAIClassBehavior.h"
#include "SpecialVisCreature.h"
#include "Totem.h"
#include "TemporarySummon.h"
#include "Spell.h"
#include "SpellAuras.h"
#include "SpellMgr.h"
#include "ThreatManager.h"
#include "Unit.h"
#include "pet_behavior.h"
#include "maps/visitors.h"

// NOTE: CreatureAI does nothing with the pointer except saving it in Creature*
// m_creature
// so despite this reinterpret being completely idiotic at first look, it has no
// harming effect
PlayerCharmAI::PlayerCharmAI(Player* plr)
  : CreatureAI(reinterpret_cast<Creature*>(plr)), m_player(plr), m_gcd(0),
    m_meleeCastSpell(0), m_forceTargetSwapTimer(0)
{
    auto charmer = m_player->GetCharmer();
    bool can_become_healer = false;
    if (charmer)
    {
        m_player->movement_gens.push(
            new movement::FollowMovementGenerator(
                charmer, PET_FOLLOW_DIST, PET_FOLLOW_ANGLE),
            0, movement::get_default_priority(movement::gen::controlled) + 1);

        auto charm_aura =
            m_player->get_aura(SPELL_AURA_MOD_CHARM, charmer->GetObjectGuid());
        if (!charm_aura)
            charm_aura = m_player->get_aura(
                SPELL_AURA_AOE_CHARM, charmer->GetObjectGuid());
        if (charm_aura &&
            charm_aura->GetSpellProto()->HasAttribute(
                SPELL_ATTR_CUSTOM_CHARM_HEALERS_POSSIBLE))
            can_become_healer = true;
    }

    switch (m_player->getClass())
    {
    case CLASS_WARRIOR:
        m_classBehavior = new WarriorPCAIBehavior(m_player, can_become_healer);
        break;
    case CLASS_ROGUE:
        m_classBehavior = new RoguePCAIBehavior(m_player, can_become_healer);
        break;
    case CLASS_PALADIN:
        m_classBehavior = new PaladinPCAIBehavior(m_player, can_become_healer);
        break;
    case CLASS_MAGE:
        m_classBehavior = new MagePCAIBehavior(m_player, can_become_healer);
        break;
    case CLASS_WARLOCK:
        m_classBehavior = new WarlockPCAIBehavior(m_player, can_become_healer);
        break;
    case CLASS_PRIEST:
        m_classBehavior = new PriestPCAIBehavior(m_player, can_become_healer);
        break;
    case CLASS_SHAMAN:
        m_classBehavior = new ShamanPCAIBehavior(m_player, can_become_healer);
        break;
    case CLASS_HUNTER:
        m_classBehavior = new HunterPCAIBehavior(m_player, can_become_healer);
        break;
    case CLASS_DRUID:
        m_classBehavior = new DruidPCAIBehavior(m_player, can_become_healer);
        break;
    default:
        logging.error(
            "PlayerCharmAI: Created with NullPCAIBehavior. Player name: %s. "
            "Class: %i",
            m_player->GetName(), m_player->getClass());
        m_classBehavior = new NullPCAIBehavior(m_player, false);
        break;
    }
}

PlayerCharmAI::~PlayerCharmAI()
{
    delete m_classBehavior;
}

void PlayerCharmAI::UpdateAI(const uint32 diff)
{
    if (!m_player->GetCharmer() || !this->SelectHostileTarget() ||
        !m_player->getVictim())
        return;

    if (m_forceTargetSwapTimer > diff)
        m_forceTargetSwapTimer -= diff;
    else
        m_forceTargetSwapTimer = 0;

    if (Pet* pet = m_player->GetPet())
        if (pet->getVictim() != m_player->getVictim() && pet->behavior())
            pet->behavior()->attempt_attack(m_player->getVictim());

    // Update movement
    bool should_idle = false;
    switch (m_classBehavior->GetClassType())
    {
    case PCAI_CT_MELEE:
        break;
    case PCAI_CT_SPELLCASTER:
    {
        bool can_idle = m_player->IsWithinDistInMap(m_player->getVictim(),
                            m_classBehavior->GetMaxDist()) &&
                        m_player->IsWithinWmoLOSInMap(m_player->getVictim());

        if (can_idle && m_player->movement_gens.has(movement::gen::stopped))
            should_idle = true;
        else if (can_idle &&
                 !m_player->movement_gens.has(movement::gen::stopped))
            should_idle = m_player->IsWithinDistInMap(
                m_player->getVictim(), m_classBehavior->GetMaxDist() * 0.8f);
        break;
    }
    case PCAI_CT_HUNTER:
    {
        bool can_idle =
            m_player->IsWithinDistInMap(
                m_player->getVictim(), m_classBehavior->GetMaxDist()) &&
            !m_player->CanReachWithMeleeAttack(m_player->getVictim()) &&
            m_player->IsWithinWmoLOSInMap(m_player->getVictim());

        if (can_idle && m_player->movement_gens.has(movement::gen::stopped))
            should_idle = true;
        else if (can_idle &&
                 !m_player->movement_gens.has(movement::gen::stopped))
            should_idle = m_player->IsWithinDistInMap(
                m_player->getVictim(), m_classBehavior->GetMaxDist() * 0.8f);
        break;
    }
    case PCAI_CT_HEALER:
    {
        auto charmer = m_player->GetCharmer();
        bool can_idle = !charmer || (m_player->IsWithinDistInMap(charmer,
                                         m_classBehavior->GetMaxDist()) &&
                                        m_player->IsWithinWmoLOSInMap(charmer));

        if (can_idle && m_player->movement_gens.has(movement::gen::stopped))
            should_idle = true;
        else if (charmer && can_idle &&
                 !m_player->movement_gens.has(movement::gen::stopped))
            should_idle = m_player->IsWithinDistInMap(
                charmer, m_classBehavior->GetMaxDist() * 0.8f);
        break;
    }
    }

    if (m_meleeCastSpell)
        should_idle = true;

    if (should_idle)
    {
        if (m_player->movement_gens.top_id() != movement::gen::stopped)
        {
            m_creature->movement_gens.push(
                new movement::StoppedMovementGenerator(),
                movement::EVENT_LEAVE_COMBAT,
                movement::get_default_priority(movement::gen::controlled) + 3);
        }
    }
    else
    {
        if (m_player->movement_gens.top_id() == movement::gen::stopped)
        {
            // Interrupt auto-shot
            m_player->InterruptSpell(CURRENT_AUTOREPEAT_SPELL);
            m_creature->movement_gens.remove_all(movement::gen::stopped);
        }
    }

    auto facing_target = m_player->getVictim();
    if (m_classBehavior->GetClassType() == PCAI_CT_HEALER)
        facing_target = m_player->GetCharmer();

    if (!m_player->m_movementInfo.HasMovementFlag(MOVEFLAG_FORWARD) &&
        (m_player->CanReachWithMeleeAttack(facing_target) ||
            m_player->movement_gens.top_id() == movement::gen::stopped) &&
        !m_player->HasInArc(M_PI_F / 16, facing_target))
    {
        m_player->SetFacingTo(m_player->GetAngle(facing_target));
    }

    // To get around the fact that we're moving when the spell starts despite
    // cancelling movegens
    if (m_meleeCastSpell)
    {
        if (Unit* tar = m_player->GetMap()->GetUnit(m_meleeTarget))
            m_player->CastSpell(tar, m_meleeCastSpell, false);
        m_meleeCastSpell = 0;
        m_meleeTarget = ObjectGuid();
        m_gcd = 1600;
    }

    if (m_gcd <= diff)
    {
        if (CastAnySpellIfCan())
            m_gcd = 1600;
    }
    else
        m_gcd -= diff;

    if (m_classBehavior->GetClassType() == PCAI_CT_MELEE)
        m_player->UpdateMeleeAttackingState();
}

void PlayerCharmAI::AttackStart(Unit* who)
{
    if (who &&
        m_player->Attack(who,
            m_classBehavior->GetClassType() == PCAI_CT_MELEE ? true : false))
    {
        who->SetInCombatWith(m_player);
        m_player->SetInCombatWith(who);
        auto charmer = m_player->GetCharmer();
        auto guid = ObjectGuid();
        if (charmer && m_classBehavior->GetClassType() == PCAI_CT_HEALER)
            guid = charmer->GetObjectGuid();

        m_creature->movement_gens.push(new movement::ChaseMovementGenerator(),
            movement::EVENT_LEAVE_COMBAT,
            movement::get_default_priority(movement::gen::controlled) + 2);

        switch (m_classBehavior->GetClassType())
        {
        case PCAI_CT_MELEE:
            break;
        case PCAI_CT_SPELLCASTER:
            if (m_player->IsWithinDistInMap(who, m_classBehavior->GetMaxDist()))
                m_creature->movement_gens.push(
                    new movement::StoppedMovementGenerator(),
                    movement::EVENT_LEAVE_COMBAT,
                    movement::get_default_priority(movement::gen::controlled) +
                        3);
            break;
        case PCAI_CT_HUNTER:
            if (m_player->IsWithinDistInMap(
                    who, m_classBehavior->GetMaxDist()) &&
                !m_player->CanReachWithMeleeAttack(who))
            {
                m_creature->movement_gens.push(
                    new movement::StoppedMovementGenerator(),
                    movement::EVENT_LEAVE_COMBAT,
                    movement::get_default_priority(movement::gen::controlled) +
                        3);
            }
            break;
        case PCAI_CT_HEALER:
        {
            if (charmer &&
                m_player->IsWithinDistInMap(
                    charmer, m_classBehavior->GetMaxDist()))
                m_creature->movement_gens.push(
                    new movement::StoppedMovementGenerator(),
                    movement::EVENT_LEAVE_COMBAT,
                    movement::get_default_priority(movement::gen::controlled) +
                        3);
            break;
        }
        }
    }
}

void PlayerCharmAI::Reset()
{
    m_victim = ObjectGuid();
}

void PlayerCharmAI::EnterEvadeMode(bool)
{
    m_player->CombatStop(true);
    m_player->movement_gens.on_event(movement::EVENT_LEAVE_COMBAT);
    Reset();
}

bool PlayerCharmAI::CastAnySpellIfCan()
{
    if (m_player->IsNonMeleeSpellCasted(false, false, true))
        return false;

    std::vector<std::pair<uint32, Unit*>> possibleSpells;
    possibleSpells.reserve(4);

    // If we have a form spell we need to enter. We need to cast that before we
    // start normal combat logic
    bool formSpell = false;
    for (const auto& elem : m_classBehavior->GetFormSpells())
    {
        if (m_player->has_aura(
                elem)) // Skip as soon as we run into an aura we have
            break;
        if (SpellCastable(elem, m_player))
        {
            m_player->remove_auras(
                SPELL_AURA_MOD_SHAPESHIFT); // Remove other shapeshifts
            possibleSpells.push_back(std::pair<uint32, Unit*>(elem, m_player));
            formSpell = true;
            break;
        }
    }
    if (m_classBehavior->GetFormSpells().empty() &&
        m_player->GetShapeshiftForm() != FORM_NONE)
        m_player->remove_auras(SPELL_AURA_MOD_SHAPESHIFT);

    if (!formSpell)
    {
        // Find first castable spell of each container
        for (const auto& elem : m_classBehavior->GetCooldownSpells())
            if (uint32 high_id = SpellCastable(elem, m_player))
            {
                possibleSpells.push_back(
                    std::pair<uint32, Unit*>(high_id, m_player));
                break;
            }
        auto charmer = m_player->GetCharmer();
        if (m_classBehavior->GetClassType() == PCAI_CT_HEALER && charmer)
        {
            for (const auto& elem : m_classBehavior->GetHealerSpells())
                if (uint32 high_id = SpellCastable(elem, charmer))
                {
                    possibleSpells.push_back(
                        std::pair<uint32, Unit*>(high_id, charmer));
                    break;
                }
        }
        else
        {
            for (const auto& elem : m_classBehavior->GetDpsSpells())
                if (uint32 high_id = SpellCastable(elem, m_player->getVictim()))
                {
                    possibleSpells.push_back(std::pair<uint32, Unit*>(
                        high_id, m_player->getVictim()));
                    break;
                }
        }
        // Only include dumb spells 33% of the tries
        if (urand(1, 3) == 1)
        {
            for (const auto& elem : m_classBehavior->GetDumbSpells())
                if (uint32 high_id = SpellCastable(elem, m_player))
                {
                    possibleSpells.push_back(
                        std::pair<uint32, Unit*>(high_id, m_player));
                    break;
                }
        }
        for (const auto& elem : m_classBehavior->GetBeneficialSpells())
            if (uint32 high_id = SpellCastable(elem, m_player->GetCharmer()))
            {
                possibleSpells.push_back(
                    std::pair<uint32, Unit*>(high_id, m_player->GetCharmer()));
                break;
            }
    }

    if (!possibleSpells.empty())
    {
        uint32 i = urand(0, possibleSpells.size() - 1);

        if (const SpellEntry* info =
                sSpellStore.LookupEntry(possibleSpells[i].first))
        {
            // If we're a melee casting a non-instant spell we need to stop
            if (const SpellCastTimesEntry* time =
                    sSpellCastTimesStore.LookupEntry(info->CastingTimeIndex))
            {
                if (time->CastTime > 0 &&
                    m_player->movement_gens.top_id() == movement::gen::chase &&
                    m_classBehavior->GetClassType() == PCAI_CT_MELEE)
                {
                    m_creature->movement_gens.push(
                        new movement::StoppedMovementGenerator(),
                        movement::EVENT_LEAVE_COMBAT,
                        movement::get_default_priority(
                            movement::gen::controlled) +
                            3);
                    m_meleeCastSpell = possibleSpells[i].first;
                    m_meleeTarget = possibleSpells[i].second->GetObjectGuid();
                    return true;
                }
            }

            m_player->CastSpell(
                possibleSpells[i].second, possibleSpells[i].first, false);

            if (info->StartRecoveryTime == 0)
                return false; // No GCD
        }

        return true;
    }

    return false;
}

uint32 PlayerCharmAI::SpellCastable(uint32 spellId, Unit* target)
{
    if (!m_player->HasSpell(spellId))
        return 0;

    spellId = GetHighestSpellRank(spellId);
    const SpellEntry* info = sSpellStore.LookupEntry(spellId);
    if (!info)
        return 0;

    // We can only cast instant spells when moving (unless we're a melee based
    // class)
    if (const SpellCastTimesEntry* time =
            sSpellCastTimesStore.LookupEntry(info->CastingTimeIndex))
        if (time->CastTime > 0 &&
            m_player->movement_gens.top_id() != movement::gen::stopped &&
            m_classBehavior->GetClassType() != PCAI_CT_MELEE)
            return 0;
    // Applies aura spell
    if (info->HasEffect(SPELL_EFFECT_APPLY_AURA) &&
        (!info->HasEffect(SPELL_EFFECT_SCHOOL_DAMAGE) ||
            sSpellMgr::Instance()->GetFirstSpellInChain(spellId) ==
                348)) // Ignore if spell also does direct damage (and isn't
                      // immolate)
    {
        // Return false if we have already applied this aura on the target
        bool broke = false;
        target->loop_auras([&broke, this, spellId](AuraHolder* holder)
            {
                if (holder->GetId() == spellId &&
                    holder->GetCasterGuid() == m_player->GetObjectGuid())
                    broke = true;
                return !broke; // break out of loop when broke is true
            });
        if (broke)
            return 0;
    }
    // Channeled spell
    if ((info->HasAttribute(SPELL_ATTR_EX_CHANNELED_1) ||
            info->HasAttribute(SPELL_ATTR_EX_CHANNELED_2)) &&
        m_player->movement_gens.top_id() != movement::gen::stopped)
        return 0;
    // Requires stance spell
    if (!(info->HasAttribute(SPELL_ATTR_EX2_NOT_NEED_SHAPESHIFT) &&
            m_player->GetShapeshiftForm() == FORM_NONE))
    {
        bool inRightStance =
            info->Stances != 0 ?
                ((1 << (m_player->GetShapeshiftForm() - 1)) & info->Stances) :
                false;
        if (info->HasAttribute(SPELL_ATTR_NOT_SHAPESHIFT) &&
            m_player->GetShapeshiftForm() != FORM_NONE && !inRightStance)
            return false;
    }
    // Can't do in this stance spell
    if (info->StancesNot != 0 &&
        ((1 << (m_player->GetShapeshiftForm() - 1)) & info->StancesNot))
        return 0;
    // Requires item type spell
    if (!m_player->HasItemFitToSpellReqirements(info))
        return 0;
    // Behind target spells
    if (info->HasAttribute(SPELL_ATTR_CUSTOM_CAST_FROM_BEHIND) &&
        target->HasInArc(M_PI_F, m_player))
        return 0;
    // Combo point spells
    if ((info->HasAttribute(SPELL_ATTR_EX_REQ_COMBO_POINTS) ||
            info->HasAttribute(SPELL_ATTR_EX_REQ_TARGET_COMBO_POINTS)) &&
        m_player->GetComboPoints() < 4)
        return 0;
    // On next swing spells
    if (info->HasAttribute(SPELL_ATTR_ON_NEXT_SWING_1) &&
        m_player->GetCurrentSpell(CURRENT_MELEE_SPELL) &&
        sSpellMgr::Instance()->GetFirstSpellInChain(
            m_player->GetCurrentSpell(CURRENT_MELEE_SPELL)->m_spellInfo->Id) ==
            info->Id)
        return 0;
    // Caster Aura State (judgement e.g.)
    if (info->CasterAuraState &&
        !m_player->HasAuraState((AuraState)info->CasterAuraState))
        return 0;
    // Skip auto-repeat spells if already active
    if (info->HasAttribute(SPELL_ATTR_EX2_AUTOREPEAT_FLAG) &&
        m_player->GetCurrentSpell(CURRENT_AUTOREPEAT_SPELL) &&
        m_player->GetCurrentSpell(CURRENT_AUTOREPEAT_SPELL)->m_spellInfo->Id ==
            spellId)
        return false;
    // Standard checks
    if (info->PreventionType == SPELL_PREVENTION_TYPE_SILENCE &&
        m_player->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_SILENCED))
        return 0;
    if (info->PreventionType == SPELL_PREVENTION_TYPE_PACIFY &&
        m_player->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PACIFIED))
        return 0;
    uint32 power = info->powerType == POWER_HEALTH ?
                       m_player->GetHealth() :
                       m_player->GetPower((Powers)info->powerType);
    if (power < Spell::CalculatePowerCost(info, m_creature))
        return 0;
    if (m_player->HasSpellCooldown(spellId))
        return 0;

    if (target != m_player)
    {
        if (const SpellRangeEntry* range =
                sSpellRangeStore.LookupEntry(info->rangeIndex))
        {
            float dist = m_player->GetCombatDistance(target);
            float minRange =
                spellId == 75 ?
                    5.0f :
                    range->minRange; // Hard-coded range for auto-shot
            if (dist < minRange || dist > range->maxRange)
                return 0;
        }

        if (!sSpellMgr::Instance()->IgnoresLineOfSight(spellId) &&
            !m_player->IsWithinWmoLOSInMap(target))
            return 0;
    }

    return spellId;
}

uint32 PlayerCharmAI::GetHighestSpellRank(uint32 spellId)
{
    std::vector<uint32>::const_iterator lower, upper;
    while (sSpellMgr::Instance()->GetNextSpellInChainBoundaries(
        spellId, lower, upper))
    {
        if (!m_player->HasSpell(*lower))
            break;
        spellId =
            *lower; // We don't care about spells with multiple nexts for this
    }
    return spellId;
}

bool PlayerCharmAI::SelectHostileTarget()
{
    Unit* charmer = m_player->GetCharmer();
    if (!charmer)
        return false;

    if (m_player->hasUnitState(UNIT_STAT_STUNNED | UNIT_STAT_DIED) ||
        m_player->IsAffectedByThreatIgnoringCC() ||
        m_player->movement_gens.has(movement::gen::charge))
        return false;

    Unit* target = nullptr;
    Unit* oldTarget =
        m_victim ? m_player->GetMap()->GetUnit(m_victim) : nullptr;
    // If getVictim() and m_victim does not result in the same target we do not
    // have a proper previous target
    if (oldTarget != m_player->getVictim())
        oldTarget = nullptr;

    // Keep the old target if our force switch has not come up yet, and we can
    // still attack him
    if (oldTarget && m_forceTargetSwapTimer > 0 &&
        oldTarget->isTargetableForAttack() && m_player->IsHostileTo(oldTarget))
        return true;

    // Get the closest target
    bool foundPlayer = false; // Once true we stop accepting non-player targets
    float dist = 50000.0f;
    auto nearby = maps::visitors::yield_set<Unit, Player, Creature, Pet,
        SpecialVisCreature, TemporarySummon, Totem>{}(m_player,
        PCAI_HOSTILE_SEARCH_RANGE, [this](Unit* u)
        {
            return u->isAlive() && m_player->IsHostileTo(u) &&
                   u->can_be_seen_by(m_player, m_player, false);
        });
    for (auto& elem : nearby)
    {
        // Stop accepting non-players if we found a player
        if (foundPlayer && (elem)->GetTypeId() != TYPEID_PLAYER)
            continue;
        if (!(elem)->isTargetableForAttack() || !m_player->IsHostileTo(elem) ||
            !charmer->IsHostileTo(elem))
            continue;
        if (m_forceTargetSwapTimer == 0 && elem == oldTarget)
            continue;

        float d = m_player->GetDistance(elem);
        if (d < dist || (!foundPlayer && (elem)->GetTypeId() == TYPEID_PLAYER))
        {
            if ((elem)->GetTypeId() == TYPEID_PLAYER)
                foundPlayer = true;
            dist = d;
            target = elem;
        }
    }

    if (target)
    {
        // Stick to old target if the new target's proximity is not
        // significantly more beneficial to us
        if (oldTarget && oldTarget != target)
        {
            if (m_forceTargetSwapTimer != 0 &&
                m_player->GetDistance(oldTarget) > 5.0f &&
                m_player->GetDistance(oldTarget) *
                        PCAI_DIST_NEW_VICTIM_BREAKPOINT <
                    m_player->GetDistance(target))
                target = oldTarget;
        }

        if (oldTarget != target)
        {
            AttackStart(target);
            m_forceTargetSwapTimer = urand(4000, 10000);
        }

        m_victim = target->GetObjectGuid();

        return true;
    }

    // We can still use the old target if it's still attackable (this means it
    // was ignored because of force swap)
    if (oldTarget && oldTarget->isTargetableForAttack() &&
        m_player->IsHostileTo(oldTarget))
        return true;

    if (!m_player->isInCombat())
        return false; // If we're not in combat we shouldn't evade

    // HACK: If the MC is incite chaos we shouldn't evade
    if (m_player->has_aura(33684))
        return false;

    EnterEvadeMode();
    return false;
}

void PCAIClassBehavior::CalculateTalentPage(Player* plr)
{
    // Calculate which talent tree has the most talents in it
    uint32 const* talentTabIds = GetTalentTabPages(plr->getClass());
    uint32 talentCount[3] = {0, 0, 0};
    for (uint32 talentId = 0; talentId < sTalentStore.GetNumRows(); ++talentId)
    {
        TalentEntry const* talentInfo = sTalentStore.LookupEntry(talentId);
        if (!talentInfo)
            continue;

        if (talentInfo->TalentTab != talentTabIds[0] &&
            talentInfo->TalentTab != talentTabIds[1] &&
            talentInfo->TalentTab != talentTabIds[2])
            continue;

        uint32 rankOfTalent = 0;
        for (int j = MAX_TALENT_RANK; j > 0; --j)
        {
            if (talentInfo->RankID[j - 1] &&
                plr->HasSpell(talentInfo->RankID[j - 1]))
            {
                rankOfTalent = j;
                break;
            }
        }
        if (!rankOfTalent)
            continue;

        if (talentInfo->TalentTab == talentTabIds[0])
            talentCount[0] += rankOfTalent;
        else if (talentInfo->TalentTab == talentTabIds[1])
            talentCount[1] += rankOfTalent;
        else if (talentInfo->TalentTab == talentTabIds[2])
            talentCount[2] += rankOfTalent;
    }

    // If no talents at all
    if (!talentCount[0] && !talentCount[1] && !talentCount[2])
    {
        m_talentPage = PCAI_TALENT_PAGE_NONE;
        return;
    }

    // Select page with most spent talents, prioritizing ones to the left if
    // equal amounted
    if (talentCount[0] >= talentCount[1] && talentCount[0] >= talentCount[2])
        m_talentPage = PCAI_TALENT_PAGE_ONE;
    else if (talentCount[1] >= talentCount[2])
        m_talentPage = PCAI_TALENT_PAGE_TWO;
    else
        m_talentPage = PCAI_TALENT_PAGE_THREE;
}
