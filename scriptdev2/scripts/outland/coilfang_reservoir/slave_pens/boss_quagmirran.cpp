/* Copyright (C) 2012 CoreCraft
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

/* ScriptData
SDName: Boss_Quagmirran
SD%Complete: 100%
SDComment:
SDCategory: Coilfang Resevoir, Slave Pens
EndScriptData */

#include "precompiled.h"

enum
{
    SPELL_CLEAVE = 40504,
    SPELL_UPPERCUT = 32055,
    SPELL_ACID_SPRAY = 38153,
    SPELL_ACID_SPRAY_PROC = 38163,
    SPELL_POISON_VOLLEY = 34780,
    SPELL_POISON_VOLLEY_H = 39340,
};

struct MANGOS_DLL_DECL boss_quagmirranAI : public ScriptedAI
{
    boss_quagmirranAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_ascendTimer = 5000;
        m_bIsRegularMode = pCreature->GetMap()->IsRegularDifficulty();
        Reset();
    }

    void SpellDamageCalculation(const Unit*, int32& iDamage,
        const SpellEntry* pSpell, SpellEffectIndex) override
    {
        if (pSpell->Id == SPELL_ACID_SPRAY_PROC)
            iDamage = m_bIsRegularMode ? urand(300, 350) : urand(900, 1000);
    }

    bool m_bIsRegularMode;
    uint32 m_uiCleaveTimer;
    uint32 m_uiUppercutTimer;
    uint32 m_uiAcidSprayTimer;
    uint32 m_uiPoisonVolleyTimer;
    uint32 m_ascendTimer;

    void Reset() override
    {
        m_uiCleaveTimer = urand(10000, 20000);
        m_uiUppercutTimer = 15000;
        m_uiAcidSprayTimer = 22000;
        m_uiPoisonVolleyTimer = 30000;
    }

    void AscendIfNeeded(uint32 diff)
    {
        if (m_ascendTimer)
        {
            if (m_ascendTimer <= diff)
                m_ascendTimer = 5000;
            else
            {
                m_ascendTimer -= diff;
                return;
            }
        }
        else
            return;

        if (GetClosestPlayer(m_creature, 120.0f) == nullptr)
            return;
        m_ascendTimer = 0;

        // Ascend
        std::vector<DynamicWaypoint> wps;
        wps.push_back(DynamicWaypoint(-240.9f, -684.8f, 17.4f));
        wps.push_back(DynamicWaypoint(-224.5f, -692.4f, 23.6f));
        wps.push_back(DynamicWaypoint(-214.4f, -698.6f, 30.0f));
        wps.push_back(DynamicWaypoint(-199.9f, -706.8f, 37.8f));
        m_creature->movement_gens.push(
            new movement::DynamicWaypointMovementGenerator(wps, false));
        m_creature->movement_gens.remove_all(movement::gen::idle);
        m_creature->movement_gens.push(
            new movement::IdleMovementGenerator(-199.9f, -706.8f, 37.8f, 5.6f));
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
        {
            AscendIfNeeded(uiDiff);
            return;
        }

        // Acid Spray
        if (m_uiAcidSprayTimer < uiDiff)
        {
            if (Unit* pTarget = m_creature->SelectAttackingTarget(
                    ATTACKING_TARGET_RANDOM, 0))
            {
                if (DoCastSpellIfCan(pTarget, SPELL_ACID_SPRAY) == CAST_OK)
                    m_uiAcidSprayTimer = urand(35000, 40000);
            }
        }
        else
            m_uiAcidSprayTimer -= uiDiff;

        // Cleave
        if (m_uiCleaveTimer < uiDiff)
        {
            if (DoCastSpellIfCan(m_creature->getVictim(), SPELL_CLEAVE) ==
                CAST_OK)
                m_uiCleaveTimer = urand(15000, 20000);
        }
        else
            m_uiCleaveTimer -= uiDiff;

        // Uppercut
        if (m_uiUppercutTimer < uiDiff)
        {
            if (DoCastSpellIfCan(m_creature->getVictim(), SPELL_UPPERCUT) ==
                CAST_OK)
                m_uiUppercutTimer = urand(20000, 30000);
        }
        else
            m_uiUppercutTimer -= uiDiff;

        // Poison Volley
        if (m_uiPoisonVolleyTimer < uiDiff)
        {
            if (DoCastSpellIfCan(m_creature,
                    m_bIsRegularMode ? SPELL_POISON_VOLLEY :
                                       SPELL_POISON_VOLLEY_H) == CAST_OK)
                m_uiPoisonVolleyTimer = urand(20000, 40000);
        }
        else
            m_uiPoisonVolleyTimer -= uiDiff;

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_boss_quagmirran(Creature* pCreature)
{
    return new boss_quagmirranAI(pCreature);
}

void AddSC_boss_quagmirran()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "boss_quagmirran";
    pNewScript->GetAI = &GetAI_boss_quagmirran;
    pNewScript->RegisterSelf();
}
