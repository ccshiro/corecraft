/* Copyright (C) 2006 - 2012 ScriptDev2 <http://www.scriptdev2.com/>
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
SDName: Boss_Watchkeeper_Gargolmar
SD%Complete: 100
SDComment:
SDCategory: Hellfire Citadel, Hellfire Ramparts
EndScriptData */

#include "precompiled.h"

enum
{
    SAY_TAUNT = -1543000,
    SAY_HEAL = -1543001,
    SAY_SURGE = -1543002,
    SAY_AGGRO_1 = -1543003,
    SAY_AGGRO_2 = -1543004,
    SAY_AGGRO_3 = -1543005,
    SAY_KILL_1 = -1543006,
    SAY_KILL_2 = -1543007,
    SAY_DIE = -1543008,

    SPELL_MORTAL_WOUND = 30641,
    SPELL_MORTAL_WOUND_H = 36814,
    SPELL_SURGE = 34645,
    SPELL_RETALIATION = 22857,
    SPELL_OVERPOWER = 32154,
};

struct MANGOS_DLL_DECL boss_watchkeeper_gargolmarAI : public ScriptedAI
{
    boss_watchkeeper_gargolmarAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_bIsRegularMode = pCreature->GetMap()->IsRegularDifficulty();
        m_bHasTaunted = false;
        Reset();
    }

    bool m_bIsRegularMode;

    uint32 m_uiSurgeTimer;
    uint32 m_uiMortalWoundTimer;
    bool m_bRetaliated;
    uint32 m_uiOverpowerTimer;

    bool m_bHasTaunted;
    bool m_bYelledForHeal;

    void Reset() override
    {
        m_uiSurgeTimer = 3000;
        m_uiMortalWoundTimer = 10000;
        m_uiOverpowerTimer = 5000;

        m_bRetaliated = false;
        m_bYelledForHeal = false;
    }

    void Aggro(Unit* /*pWho*/) override
    {
        switch (urand(0, 2))
        {
        case 0:
            DoScriptText(SAY_AGGRO_1, m_creature);
            break;
        case 1:
            DoScriptText(SAY_AGGRO_2, m_creature);
            break;
        case 2:
            DoScriptText(SAY_AGGRO_3, m_creature);
            break;
        }
    }

    void MoveInLineOfSight(Unit* pWho) override
    {
        if (!m_bHasTaunted && m_creature->IsWithinDistInMap(pWho, 60.0f))
        {
            DoScriptText(SAY_TAUNT, m_creature);
            m_bHasTaunted = true;
        }

        ScriptedAI::MoveInLineOfSight(pWho);
    }

    void KilledUnit(Unit* victim) override
    {
        DoKillSay(m_creature, victim, SAY_KILL_1, SAY_KILL_2);
    }

    void JustDied(Unit* /*pKiller*/) override
    {
        DoScriptText(SAY_DIE, m_creature);
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        if (m_uiMortalWoundTimer < uiDiff)
        {
            if (DoCastSpellIfCan(m_creature->getVictim(),
                    m_bIsRegularMode ? SPELL_MORTAL_WOUND :
                                       SPELL_MORTAL_WOUND_H) == CAST_OK)
                m_uiMortalWoundTimer = urand(6000, 12000);
        }
        else
            m_uiMortalWoundTimer -= uiDiff;

        if (m_uiSurgeTimer < uiDiff)
        {
            if (Unit* pTarget = m_creature->SelectAttackingTarget(
                    ATTACKING_TARGET_RANDOM, 0, SPELL_SURGE))
            {
                if (DoCastSpellIfCan(pTarget, SPELL_SURGE, false) == CAST_OK)
                    m_uiSurgeTimer = 10000;
            }
        }
        else
            m_uiSurgeTimer -= uiDiff;

        if (!m_bRetaliated && m_creature->GetHealthPercent() <= 20.0f)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_RETALIATION) == CAST_OK)
                m_bRetaliated = true;
        }

        if (m_uiOverpowerTimer < uiDiff)
        {
            if (DoCastSpellIfCan(m_creature->getVictim(), SPELL_OVERPOWER) ==
                CAST_OK)
                m_uiOverpowerTimer = 5000;
        }
        else
            m_uiOverpowerTimer -= uiDiff;

        if (!m_bYelledForHeal)
        {
            if (m_creature->GetHealthPercent() < 40.0f)
            {
                DoScriptText(SAY_HEAL, m_creature);
                m_bYelledForHeal = true;
            }
        }

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_boss_watchkeeper_gargolmarAI(Creature* pCreature)
{
    return new boss_watchkeeper_gargolmarAI(pCreature);
}

void AddSC_boss_watchkeeper_gargolmar()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "boss_watchkeeper_gargolmar";
    pNewScript->GetAI = &GetAI_boss_watchkeeper_gargolmarAI;
    pNewScript->RegisterSelf();
}
