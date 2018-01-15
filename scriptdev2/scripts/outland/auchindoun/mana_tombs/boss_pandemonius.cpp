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
SDName: Boss_Pandemonius
SD%Complete: 100
SDComment:
SDCategory: Auchindoun, Mana Tombs
EndScriptData */

#include "mana_tombs.h"
#include "precompiled.h"

enum
{
    SAY_AGGRO_1 = -1557008,
    SAY_AGGRO_2 = -1557009,
    SAY_AGGRO_3 = -1557010,
    SAY_KILL_1 = -1557011,
    SAY_KILL_2 = -1557012,
    SAY_DEATH = -1557013,
    EMOTE_DARK_SHELL = -1557014, // Added in 2.1, we skip it

    SPELL_VOID_BLAST = 32325,
    SPELL_VOID_BLAST_H = 38760,
    SPELL_DARK_SHELL =
        32358, // These 2 spells do too little damage in 2.4.3, we up them
    SPELL_DARK_SHELL_H = 38759, // Cast on 3 random targets, chosen targets not
                                // excluded (potentially they could all hit the
                                // same guy)
    DARK_SHELL_TARGET_COUNT = 3,
};

struct MANGOS_DLL_DECL boss_pandemoniusAI : public ScriptedAI
{
    boss_pandemoniusAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_pInstance = (instance_mana_tombs*)pCreature->GetInstanceData();
        m_bIsRegularMode = pCreature->GetMap()->IsRegularDifficulty();
        Reset();
    }

    instance_mana_tombs* m_pInstance;
    bool m_bIsRegularMode;

    uint32 m_uiVoidBlastTimer;
    uint32 m_uiDarkShellTimer;
    uint8 m_uiVoidBlastCounter;

    void SpellDamageCalculation(const Unit* /*pDoneTo*/, int32& iDamage,
        const SpellEntry* pSpell, SpellEffectIndex effectIndex) override
    {
        if (pSpell->Id == SPELL_VOID_BLAST && effectIndex == EFFECT_INDEX_0)
            iDamage = urand(1800, 2100);
        else if (pSpell->Id == SPELL_VOID_BLAST_H &&
                 effectIndex == EFFECT_INDEX_0)
            iDamage = urand(2700, 3000);
    }

    void Reset() override
    {
        m_uiVoidBlastTimer = 14000;
        m_uiDarkShellTimer = 18000;
        m_uiVoidBlastCounter = 0;
    }

    void JustDied(Unit* /*pKiller*/) override
    {
        DoScriptText(SAY_DEATH, m_creature);

        if (m_pInstance)
            m_pInstance->SetData(TYPE_PANDEMONIUS, DONE);
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
            m_pInstance->SetData(TYPE_PANDEMONIUS, IN_PROGRESS);
    }

    void JustReachedHome() override
    {
        if (m_pInstance)
            m_pInstance->SetData(TYPE_PANDEMONIUS, FAIL);
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        if (m_uiVoidBlastTimer <= uiDiff)
        {
            if (Unit* pTarget = m_creature->SelectAttackingTarget(
                    ATTACKING_TARGET_RANDOM, 0, SPELL_VOID_BLAST))
            {
                if (DoCastSpellIfCan(pTarget,
                        m_bIsRegularMode ? SPELL_VOID_BLAST :
                                           SPELL_VOID_BLAST_H) == CAST_OK)
                {
                    if (m_uiVoidBlastCounter < DARK_SHELL_TARGET_COUNT)
                    {
                        m_uiVoidBlastTimer = 500;
                        ++m_uiVoidBlastCounter;
                    }
                    else
                    {
                        m_uiVoidBlastTimer = urand(25000, 30000);
                        m_uiVoidBlastCounter = 0;
                    }
                }
            }
        }
        else
            m_uiVoidBlastTimer -= uiDiff;

        if (m_uiDarkShellTimer <= uiDiff)
        {
            if (DoCastSpellIfCan(m_creature,
                    m_bIsRegularMode ? SPELL_DARK_SHELL : SPELL_DARK_SHELL_H) ==
                CAST_OK)
                m_uiDarkShellTimer = urand(25000, 30000);
        }
        else
            m_uiDarkShellTimer -= uiDiff;

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_boss_pandemonius(Creature* pCreature)
{
    return new boss_pandemoniusAI(pCreature);
}

void AddSC_boss_pandemonius()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "boss_pandemonius";
    pNewScript->GetAI = &GetAI_boss_pandemonius;
    pNewScript->RegisterSelf();
}
