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
SDName: Boss_Temporus
SD%Complete: 100
SDComment:
SDCategory: Caverns of Time, The Dark Portal
EndScriptData */

#include "dark_portal.h"
#include "precompiled.h"

enum
{
    SAY_INTRO = -1269000,
    SAY_AGGRO = -1269001,
    SAY_BANISH = -1269002,
    SAY_KILL_1 = -1269003,
    SAY_KILL_2 = -1269004,
    SAY_DEATH = -1269005,

    SPELL_HASTE = 31458,
    SPELL_MORTAL_WOUND = 31464,
    SPELL_WING_BUFFET = 31475,
    SPELL_WING_BUFFET_H = 38593,
    SPELL_REFLECT = 38592
};

struct MANGOS_DLL_DECL boss_temporusAI : public ScriptedAI
{
    boss_temporusAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_pInstance = (ScriptedInstance*)pCreature->GetInstanceData();
        m_bIsRegularMode = pCreature->GetMap()->IsRegularDifficulty();
        Reset();

        m_uiIntroText = 500;
    }

    ScriptedInstance* m_pInstance;
    bool m_bIsRegularMode;

    uint32 m_uiIntroText;
    uint32 m_uiHasteTimer;
    uint32 m_uiSpellReflectionTimer;
    uint32 m_uiMortalWoundTimer;
    uint32 m_uiWingBuffetTimer;

    void Reset() override
    {
        m_uiHasteTimer = urand(10000, 15000);
        m_uiSpellReflectionTimer = urand(10000, 15000);
        m_uiMortalWoundTimer = 6000;
        m_uiWingBuffetTimer = 15000;
    }

    void Aggro(Unit* /*pWho*/) override
    {
        DoScriptText(SAY_AGGRO, m_creature);

        // Only Temporus change the instance status, his copy does not
        if (m_creature->GetEntry() == NPC_TEMPORUS && m_pInstance)
            m_pInstance->SetData(TYPE_TEMPORUS, IN_PROGRESS);
    }

    void JustReachedHome() override
    {
        // Only Temporus change the instance status, his copy does not
        if (m_creature->GetEntry() == NPC_TEMPORUS && m_pInstance)
            m_pInstance->SetData(TYPE_TEMPORUS, FAIL);
    }

    void KilledUnit(Unit* victim) override
    {
        DoKillSay(m_creature, victim, SAY_KILL_1, SAY_KILL_2);
    }

    void JustDied(Unit* /*pKiller*/) override
    {
        DoScriptText(SAY_DEATH, m_creature);

        // Only Temporus change the instance status, his copy does not
        if (m_creature->GetEntry() == NPC_TEMPORUS && m_pInstance)
            m_pInstance->SetData(TYPE_TEMPORUS, DONE);
    }

    void MoveInLineOfSight(Unit* pWho) override
    {
        // Despawn Time Keeper
        if (pWho->GetTypeId() == TYPEID_UNIT &&
            pWho->GetEntry() == NPC_TIME_KEEPER)
        {
            if (m_creature->IsWithinDistInMap(pWho, 30.0f))
            {
                DoScriptText(SAY_BANISH, m_creature);
                ((Creature*)pWho)->ForcedDespawn();
            }
        }

        ScriptedAI::MoveInLineOfSight(pWho);
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        if (m_uiIntroText)
        {
            if (m_uiIntroText <= uiDiff)
            {
                DoScriptText(SAY_INTRO, m_creature);
                m_uiIntroText = 0;
            }
            else
                m_uiIntroText -= uiDiff;
        }

        // Return since we have no target
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        // Attack Haste
        if (m_uiHasteTimer < uiDiff)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_HASTE) == CAST_OK)
                m_uiHasteTimer = urand(10000, 15000);
        }
        else
            m_uiHasteTimer -= uiDiff;

        // MortalWound_Timer
        if (m_uiMortalWoundTimer < uiDiff)
        {
            if (DoCastSpellIfCan(m_creature->getVictim(), SPELL_MORTAL_WOUND) ==
                CAST_OK)
                m_uiMortalWoundTimer = urand(6000, 7000);
        }
        else
            m_uiMortalWoundTimer -= uiDiff;

        // Wing ruffet
        if (m_uiWingBuffetTimer < uiDiff)
        {
            if (DoCastSpellIfCan(m_creature,
                    m_bIsRegularMode ? SPELL_WING_BUFFET :
                                       SPELL_WING_BUFFET_H) == CAST_OK)
                m_uiWingBuffetTimer = urand(20000, 25000);
        }
        else
            m_uiWingBuffetTimer -= uiDiff;

        // Spell reflection
        if (!m_bIsRegularMode)
        {
            if (m_uiSpellReflectionTimer < uiDiff)
            {
                if (DoCastSpellIfCan(m_creature, SPELL_REFLECT) == CAST_OK)
                    m_uiSpellReflectionTimer = urand(25000, 30000);
            }
            else
                m_uiSpellReflectionTimer -= uiDiff;
        }

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_boss_temporus(Creature* pCreature)
{
    return new boss_temporusAI(pCreature);
}

void AddSC_boss_temporus()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "boss_temporus";
    pNewScript->GetAI = &GetAI_boss_temporus;
    pNewScript->RegisterSelf();
}
