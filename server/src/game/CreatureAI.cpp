/*
 * Copyright (C) 2005-2012 MaNGOS <http://getmangos.com/>
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

#include "CreatureAI.h"
#include "Creature.h"
#include "critter_ai.h"
#include "DBCStores.h"
#include "GuardAI.h"
#include "SmartAI.h"
#include "Spell.h"
#include "SpellMgr.h"
#include "TotemAI.h"

CreatureAI::~CreatureAI()
{
}

void CreatureAI::AttackedBy(Unit* attacker)
{
    if (!m_creature->getVictim())
        AttackStart(attacker);
}

void CreatureAI::InitializeAI()
{
    if (!m_creature->isDead())
        Reset();
}

CanCastResult CreatureAI::CanCastSpell(
    Unit* pTarget, const SpellEntry* pSpell, bool isTriggered)
{
    // If not triggered, we check
    if (!isTriggered)
    {
        // State does not allow
        if (m_creature->hasUnitState(UNIT_STAT_CAN_NOT_REACT_OR_LOST_CONTROL))
            return CAST_FAIL_STATE;

        if (pSpell->PreventionType == SPELL_PREVENTION_TYPE_SILENCE)
        {
            // Check if silenced
            if (m_creature->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_SILENCED))
                return CAST_FAIL_STATE;
            // Check if spell school is locked
            if (m_creature->IsSpellSchoolLocked(
                    (SpellSchoolMask)pSpell->SchoolMask))
                return CAST_FAIL_STATE;
        }

        if (pSpell->PreventionType == SPELL_PREVENTION_TYPE_PACIFY &&
            m_creature->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PACIFIED))
            return CAST_FAIL_STATE;

        // Check for power (also done by Spell::CheckCast())
        if (m_creature->GetPower((Powers)pSpell->powerType) <
            Spell::CalculatePowerCost(pSpell, m_creature))
            return CAST_FAIL_POWER;

        // Overpower
        if (pSpell->HasAttribute(SPELL_ATTR_EX_REQ_TARGET_COMBO_POINTS) &&
            pTarget && m_creature->overpower_target != pTarget->GetObjectGuid())
            return CAST_FAIL_STATE;

        // Caster Aura State (e.g., revenge)
        if (pSpell->CasterAuraState &&
            !m_creature->HasAuraState(
                static_cast<AuraState>(pSpell->CasterAuraState)))
            return CAST_FAIL_STATE;
    }

    if (const SpellRangeEntry* pSpellRange =
            sSpellRangeStore.LookupEntry(pSpell->rangeIndex))
    {
        if (pTarget != m_creature)
        {
            // pTarget is out of range of this spell (also done by
            // Spell::CheckCast())
            float fDistance = m_creature->GetCombatDistance(pTarget);

            if (fDistance > pSpellRange->maxRange)
                return CAST_FAIL_TOO_FAR;

            float fMinRange = pSpellRange->minRange;

            if (fMinRange && fDistance < fMinRange)
                return CAST_FAIL_TOO_CLOSE;
        }

        // Check LOS for spell
        if (pTarget && pTarget != m_creature &&
            !sSpellMgr::Instance()->IgnoresLineOfSight(pSpell->Id) &&
            !m_creature->IsWithinWmoLOSInMap(pTarget))
        {
            return CAST_FAIL_LOS;
        }

        return CAST_OK;
    }
    else
        return CAST_FAIL_OTHER;
}

CanCastResult CreatureAI::DoCastSpellIfCan(Unit* pTarget, uint32 uiSpell,
    uint32 uiCastFlags, ObjectGuid uiOriginalCasterGUID)
{
    Unit* pCaster = m_creature;

    if (uiCastFlags & CAST_FORCE_TARGET_SELF)
        pCaster = pTarget;

    // Allowed to cast only if not casting (unless we interrupt ourself) or if
    // spell is triggered
    if (!pCaster->IsNonMeleeSpellCasted(false) ||
        (uiCastFlags & (CAST_TRIGGERED | CAST_INTERRUPT_PREVIOUS)))
    {
        if (const SpellEntry* pSpell = sSpellStore.LookupEntry(uiSpell))
        {
            // If cast flag CAST_AURA_NOT_PRESENT is active, check if target
            // already has aura on them
            if (uiCastFlags & CAST_AURA_NOT_PRESENT)
            {
                if (!pTarget || pTarget->has_aura(uiSpell))
                    return CAST_FAIL_TARGET_AURA;
            }

            // Check if cannot cast spell
            if (!(uiCastFlags & (CAST_FORCE_TARGET_SELF | CAST_FORCE_CAST)))
            {
                CanCastResult castResult =
                    CanCastSpell(pTarget, pSpell, uiCastFlags & CAST_TRIGGERED);

                if (castResult != CAST_OK)
                    return castResult;
            }

            // Interrupt any previous spell
            if (uiCastFlags & CAST_INTERRUPT_PREVIOUS &&
                pCaster->IsNonMeleeSpellCasted(false))
                pCaster->InterruptNonMeleeSpells(false);

            pCaster->CastSpell(pTarget, pSpell,
                (bool)(uiCastFlags & CAST_TRIGGERED), nullptr, nullptr,
                uiOriginalCasterGUID);
            return CAST_OK;
        }
        else
        {
            logging.error(
                "DoCastSpellIfCan by creature entry %u attempt to cast spell "
                "%u but spell does not exist.",
                m_creature->GetEntry(), uiSpell);
            return CAST_FAIL_OTHER;
        }
    }
    else
        return CAST_FAIL_IS_CASTING;
}

CanCastResult CreatureAI::CanCastSpell(
    Unit* pTarget, uint32 uiSpell, bool /*isTriggered*/, uint32 uiCastFlags)
{
    Unit* pCaster = m_creature;

    // Allowed to cast only if not casting (unless we interrupt ourself) or if
    // spell is triggered
    if (!pCaster->IsNonMeleeSpellCasted(false) ||
        (uiCastFlags & (CAST_TRIGGERED | CAST_INTERRUPT_PREVIOUS)))
    {
        if (const SpellEntry* pSpell = sSpellStore.LookupEntry(uiSpell))
        {
            // If cast flag CAST_AURA_NOT_PRESENT is active, check if target
            // already has aura on them
            if (uiCastFlags & CAST_AURA_NOT_PRESENT)
            {
                if (!pTarget || pTarget->has_aura(uiSpell))
                    return CAST_FAIL_TARGET_AURA;
            }

            // Check if cannot cast spell
            if (!(uiCastFlags & (CAST_FORCE_TARGET_SELF | CAST_FORCE_CAST)))
            {
                CanCastResult castResult =
                    CanCastSpell(pTarget, pSpell, uiCastFlags & CAST_TRIGGERED);

                if (castResult != CAST_OK)
                    return castResult;
            }

            // Check LOS for spell
            if (pTarget && pTarget != m_creature &&
                !sSpellMgr::Instance()->IgnoresLineOfSight(uiSpell) &&
                !m_creature->IsWithinWmoLOSInMap(pTarget))
            {
                return CAST_FAIL_LOS;
            }

            return CAST_OK;
        }
        else
        {
            return CAST_FAIL_OTHER;
        }
    }
    else
        return CAST_FAIL_IS_CASTING;
}

void CreatureAI::Pacify(bool state)
{
    if (IsPacified() == state)
        return;
    pacified_ = state;

    if (m_creature->getThreatManager().isThreatListEmpty())
        return;
    if (!m_creature->SelectHostileTarget())
        return;
    auto victim = m_creature->getVictim();
    if (victim == nullptr)
        return;

    if (state)
    {
        m_creature->SetTargetGuid(ObjectGuid());
        m_creature->clearUnitState(UNIT_STAT_MELEE_ATTACKING);
        m_creature->SendMeleeAttackStop(victim);
    }
    else
    {
        m_creature->SetTargetGuid(victim->GetObjectGuid());
        m_creature->addUnitState(UNIT_STAT_MELEE_ATTACKING);
        m_creature->SendMeleeAttackStart(victim);
        m_creature->SetStandState(UNIT_STAND_STATE_STAND);
    }
}

bool CreatureAI::IgnoreTarget(Unit* target) const
{
    return target->IsImmunedToDamage(m_creature->GetMeleeDamageSchoolMask());
}

bool CreatureAI::DoMeleeAttackIfReady()
{
    if (IsPacified())
        return false;

    return m_creature->UpdateMeleeAttackingState();
}

CreatureAI* make_ai_for(Creature* c)
{
    auto ai = c->GetAIName();

    if (auto script_ai = sScriptMgr::Instance()->GetCreatureAI(c))
        return script_ai;
    else if (ai.compare("SmartAI") == 0)
        return new SmartAI(c);
    else if (ai.compare("EventAI") == 0)
        return new SmartAI(c);
    else if (ai.compare("TotemAI") == 0 || c->IsTotem())
        return new TotemAI(c);
    else if (ai.compare("GuardAI") == 0 || c->IsGuard())
        return new GuardAI(c);
    else if (c->GetCreatureType() == CREATURE_TYPE_CRITTER)
        return new critter_ai(c);

    return new SmartAI(c);
}
