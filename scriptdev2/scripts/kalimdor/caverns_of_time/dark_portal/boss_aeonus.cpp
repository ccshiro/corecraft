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
SDName: Boss_Aeonus
SD%Complete: 100
SDComment:
SDCategory: Caverns of Time, The Dark Portal
EndScriptData */

#include "dark_portal.h"
#include "precompiled.h"

enum
{
    SAY_INTRO = -1269012,
    SAY_AGGRO = -1269013,
    SAY_BANISH = -1269014,
    SAY_KILL_1 = -1269015,
    SAY_KILL_2 = -1269016,
    SAY_DEATH = -1269017,
    EMOTE_GENERIC_FRENZY = -1000002,

    SPELL_CLEAVE = 40504,
    SPELL_TIME_STOP = 31422,
    SPELL_ENRAGE = 37605,
    SPELL_SAND_BREATH = 31473,
    SPELL_SAND_BREATH_H = 39049,
    SPELL_THRASH = 3417 // Added through creature_template_addon
};

struct MANGOS_DLL_DECL boss_aeonusAI : public ScriptedAI
{
    boss_aeonusAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_pInstance = (ScriptedInstance*)pCreature->GetInstanceData();
        m_bIsRegularMode = pCreature->GetMap()->IsRegularDifficulty();
        Reset();

        m_uiIntroText = 500;
    }

    ScriptedInstance* m_pInstance;
    bool m_bIsRegularMode;

    uint32 m_uiIntroText;
    uint32 m_uiSandBreathTimer;
    uint32 m_uiTimeStopTimer;
    uint32 m_uiFrenzyTimer;
    uint32 m_uiCleaveTimer;

    void Reset() override
    {
        m_uiSandBreathTimer = urand(5000, 15000);
        m_uiTimeStopTimer = urand(10000, 15000);
        m_uiFrenzyTimer = 20000;
        m_uiCleaveTimer = 18000;
    }

    void AttackStart(Unit* pWho) override
    {
        m_creature->InterruptNonMeleeSpells(false);
        ScriptedAI::AttackStart(pWho);
    }

    void Aggro(Unit* /*pWho*/) override
    {
        DoScriptText(SAY_AGGRO, m_creature);

        if (m_pInstance)
            m_pInstance->SetData(TYPE_AEONUS, IN_PROGRESS);
    }

    void JustReachedHome() override
    {
        if (m_pInstance)
            m_pInstance->SetData(TYPE_AEONUS, FAIL);
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

    void JustDied(Unit* /*pKiller*/) override
    {
        DoScriptText(SAY_DEATH, m_creature);

        if (m_pInstance)
            m_pInstance->SetData(TYPE_AEONUS, DONE);
    }

    void KilledUnit(Unit* victim) override
    {
        DoKillSay(m_creature, victim, SAY_KILL_1, SAY_KILL_2);
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

        // Need to do it like this, _addon or reset won't work for this boss
        if (!m_creature->has_aura(SPELL_THRASH))
            m_creature->CastSpell(m_creature, SPELL_THRASH, true);

        // Sand Breath
        if (m_uiSandBreathTimer < uiDiff)
        {
            if (DoCastSpellIfCan(m_creature->getVictim(),
                    m_bIsRegularMode ? SPELL_SAND_BREATH :
                                       SPELL_SAND_BREATH_H) == CAST_OK)
                m_uiSandBreathTimer = urand(10000, 30000);
        }
        else
            m_uiSandBreathTimer -= uiDiff;

        // Time Stop
        if (m_uiTimeStopTimer < uiDiff)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_TIME_STOP) == CAST_OK)
                m_uiTimeStopTimer = urand(10000, 20000);
        }
        else
            m_uiTimeStopTimer -= uiDiff;

        // Cleave
        if (m_uiCleaveTimer < uiDiff)
        {
            if (DoCastSpellIfCan(m_creature->getVictim(), SPELL_CLEAVE) ==
                CAST_OK)
                m_uiCleaveTimer = urand(10000, 20000);
        }
        else
            m_uiCleaveTimer -= uiDiff;

        // Frenzy
        if (m_uiFrenzyTimer < uiDiff)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_ENRAGE) == CAST_OK)
            {
                DoScriptText(EMOTE_GENERIC_FRENZY, m_creature);
                m_uiFrenzyTimer = 30000;
            }
        }
        else
            m_uiFrenzyTimer -= uiDiff;

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_boss_aeonus(Creature* pCreature)
{
    return new boss_aeonusAI(pCreature);
}

void AddSC_boss_aeonus()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "boss_aeonus";
    pNewScript->GetAI = &GetAI_boss_aeonus;
    pNewScript->RegisterSelf();
}
