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
SDName: Boss_Hydromancer_Thespia
SD%Complete: 100
SDComment:
SDCategory: Coilfang Resevoir, The Steamvault
EndScriptData */

/* ContentData
boss_hydromancer_thespia
mob_coilfang_waterelemental
EndContentData */

#include "precompiled.h"
#include "steam_vault.h"

enum
{
    SAY_SUMMON = -1545000, // Not used, just as her summon water elemental spell
    SAY_CLOUD = -1545024,
    SAY_AGGRO_1 = -1545001,
    SAY_AGGRO_2 = -1545002,
    SAY_AGGRO_3 = -1545003,
    SAY_KILL_1 = -1545004,
    SAY_KILL_2 = -1545005,
    SAY_DEAD = -1545006,

    SPELL_LIGHTNING_CLOUD = 25033,
    SPELL_LUNG_BURST = 31481,
    SPELL_ENVELOPING_WINDS = 31718,

    // Water elemental spells
    SPELL_WATER_BOLT_VOLLEY_N = 34449,
    SPELL_WATER_BOLT_VOLLEY_H = 37924,
};

struct MANGOS_DLL_DECL boss_thespiaAI : public ScriptedAI
{
    boss_thespiaAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_pInstance = (ScriptedInstance*)pCreature->GetInstanceData();
        m_bIsRegularMode = pCreature->GetMap()->IsRegularDifficulty();
        Reset();
    }

    ScriptedInstance* m_pInstance;
    bool m_bIsRegularMode;

    uint32 m_uiLightningCloudTimer;
    uint32 m_uiLungBurstTimer;
    uint32 m_uiEnvelopingWindsTimer;

    void Reset() override
    {
        m_uiLightningCloudTimer = 10000;
        m_uiLungBurstTimer = urand(10000, 15000);
        m_uiEnvelopingWindsTimer = urand(10000, 15000);
    }

    void JustReachedHome() override
    {
        if (m_pInstance)
            m_pInstance->SetData(TYPE_HYDROMANCER_THESPIA, FAIL);
    }

    void JustDied(Unit* /*pKiller*/) override
    {
        DoScriptText(SAY_DEAD, m_creature);

        if (m_pInstance)
            m_pInstance->SetData(TYPE_HYDROMANCER_THESPIA, DONE);
    }

    void KilledUnit(Unit* victim) override
    {
        DoKillSay(m_creature, victim, SAY_KILL_1, SAY_KILL_2);
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
            m_pInstance->SetData(TYPE_HYDROMANCER_THESPIA, IN_PROGRESS);
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        if (m_uiLightningCloudTimer <= uiDiff)
        {
            if (Unit* pTarget = m_creature->SelectAttackingTarget(
                    ATTACKING_TARGET_RANDOM, 0))
            {
                if (DoCastSpellIfCan(pTarget, SPELL_LIGHTNING_CLOUD) == CAST_OK)
                {
                    if (urand(0, 1))
                        DoScriptText(SAY_CLOUD, m_creature);
                    m_uiLightningCloudTimer = m_bIsRegularMode ? 30000 : 10000;
                }
            }
        }
        else
            m_uiLightningCloudTimer -= uiDiff;

        if (m_uiLungBurstTimer <= uiDiff)
        {
            if (Unit* pTarget = m_creature->SelectAttackingTarget(
                    ATTACKING_TARGET_RANDOM, 0))
            {
                if (DoCastSpellIfCan(pTarget, SPELL_LUNG_BURST) == CAST_OK)
                    m_uiLungBurstTimer = urand(10000, 20000);
            }
        }
        else
            m_uiLungBurstTimer -= uiDiff;

        if (m_uiEnvelopingWindsTimer <= uiDiff)
        {
            if (Unit* pTarget = m_creature->SelectAttackingTarget(
                    ATTACKING_TARGET_RANDOM, 0))
            {
                if (DoCastSpellIfCan(pTarget, SPELL_ENVELOPING_WINDS) ==
                    CAST_OK)
                    m_uiEnvelopingWindsTimer = 30000;
            }
        }
        else
            m_uiEnvelopingWindsTimer -= uiDiff;

        DoMeleeAttackIfReady();
    }
};

struct MANGOS_DLL_DECL mob_coilfang_waterelementalAI : public ScriptedAI
{
    mob_coilfang_waterelementalAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_bIsRegularMode = pCreature->GetMap()->IsRegularDifficulty();
        Reset();
    }

    bool m_bIsRegularMode;
    uint32 m_uiWaterBoltVolleyTimer;

    void Reset() override { m_uiWaterBoltVolleyTimer = 10000; }

    void UpdateAI(const uint32 diff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        if (m_uiWaterBoltVolleyTimer <= diff)
        {
            DoCastSpellIfCan(m_creature->getVictim(),
                m_bIsRegularMode ? SPELL_WATER_BOLT_VOLLEY_N :
                                   SPELL_WATER_BOLT_VOLLEY_H);
            m_uiWaterBoltVolleyTimer = urand(8000, 14000);
        }
        else
            m_uiWaterBoltVolleyTimer -= diff;

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_boss_thespiaAI(Creature* pCreature)
{
    return new boss_thespiaAI(pCreature);
}

CreatureAI* GetAI_mob_coilfang_waterelementalAI(Creature* pCreature)
{
    return new mob_coilfang_waterelementalAI(pCreature);
}

void AddSC_boss_hydromancer_thespia()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "boss_hydromancer_thespia";
    pNewScript->GetAI = &GetAI_boss_thespiaAI;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "mob_coilfang_waterelemental";
    pNewScript->GetAI = &GetAI_mob_coilfang_waterelementalAI;
    pNewScript->RegisterSelf();
}
