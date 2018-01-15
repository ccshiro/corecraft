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
SDName: Boss_The_Maker
SD%Complete: 100
SDComment:
SDCategory: Hellfire Citadel, Blood Furnace
EndScriptData */

#include "blood_furnace.h"
#include "precompiled.h"

enum
{
    SAY_AGGRO_1 = -1542009,
    SAY_AGGRO_2 = -1542010,
    SAY_AGGRO_3 = -1542011,
    SAY_KILL_1 = -1542012,
    SAY_KILL_2 = -1542013,
    SAY_DIE = -1542014,

    SPELL_EXPLODING_BREAKER = 30925,
    SPELL_EXPLODING_BREAKER_H = 40059,
    SPELL_DOMINATION = 30923,
};

struct MANGOS_DLL_DECL boss_the_makerAI : public ScriptedAI
{
    boss_the_makerAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_pInstance = (ScriptedInstance*)pCreature->GetInstanceData();
        m_bIsRegularMode = pCreature->GetMap()->IsRegularDifficulty();
        Reset();
    }

    ScriptedInstance* m_pInstance;
    bool m_bIsRegularMode;

    uint32 m_uiExplodingBreakerTimer;
    uint32 m_uiDominationTimer;

    void Reset() override
    {
        m_uiExplodingBreakerTimer = 6000;
        m_uiDominationTimer = 20000;
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

        if (m_pInstance)
            m_pInstance->SetData(TYPE_THE_MAKER_EVENT, IN_PROGRESS);
    }

    void JustReachedHome() override
    {
        if (m_pInstance)
            m_pInstance->SetData(TYPE_THE_MAKER_EVENT, FAIL);
    }

    void KilledUnit(Unit* victim) override
    {
        DoKillSay(m_creature, victim, SAY_KILL_1, SAY_KILL_2);
    }

    void JustDied(Unit* /*pKiller*/) override
    {
        DoScriptText(SAY_DIE, m_creature);

        if (m_pInstance)
            m_pInstance->SetData(TYPE_THE_MAKER_EVENT, DONE);
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        if (m_uiExplodingBreakerTimer < uiDiff)
        {
            if (DoCastSpellIfCan(m_creature->getVictim(),
                    m_bIsRegularMode ? SPELL_EXPLODING_BREAKER :
                                       SPELL_EXPLODING_BREAKER_H) == CAST_OK)
                m_uiExplodingBreakerTimer = urand(10000, 14000);
        }
        else
            m_uiExplodingBreakerTimer -= uiDiff;

        if (m_uiDominationTimer < uiDiff)
        {
            if (Unit* pTarget = m_creature->SelectAttackingTarget(
                    ATTACKING_TARGET_RANDOM, 0))
            {
                if (DoCastSpellIfCan(pTarget, SPELL_DOMINATION) == CAST_OK)
                    m_uiDominationTimer = urand(20000, 30000);
            }
        }
        else
            m_uiDominationTimer -= uiDiff;

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_boss_the_makerAI(Creature* pCreature)
{
    return new boss_the_makerAI(pCreature);
}

void AddSC_boss_the_maker()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "boss_the_maker";
    pNewScript->GetAI = &GetAI_boss_the_makerAI;
    pNewScript->RegisterSelf();
}
