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
SDName: Boss_Captain_Skarloc
SD%Complete: 100
SDComment:
SDCategory: Caverns of Time, Old Hillsbrad Foothills
EndScriptData */

#include "old_hillsbrad.h"
#include "precompiled.h"

enum
{
    SAY_ENTER = -1560000,
    SAY_TAUNT1 = -1560002,
    SAY_TAUNT2 = -1560001,
    SAY_KILL_1 = -1560003,
    SAY_KILL_2 = -1560004,
    SAY_DEATH = -1560005,

    //
    // All spells are handled by behavioral AI
    //
    /*SPELL_HOLY_LIGHT            = 29427,
    SPELL_CLEANSE               = 29380,
    SPELL_HAMMER_OF_JUSTICE     = 13005,
    SPELL_HOLY_SHIELD           = 31904,
    SPELL_DEVOTION_AURA         = 8258,
    SPELL_CONSECRATION          = 38385,*/
};

struct MANGOS_DLL_DECL boss_captain_skarlocAI : public Scripted_BehavioralAI
{
    boss_captain_skarlocAI(Creature* pCreature)
      : Scripted_BehavioralAI(pCreature)
    {
        m_pInstance = (ScriptedInstance*)pCreature->GetInstanceData();
        Reset();
    }

    ScriptedInstance* m_pInstance;
    uint32 m_uiTauntPhase;

    void Reset() override
    {
        m_uiTauntPhase = 0;

        Scripted_BehavioralAI::Reset();
    }

    void KilledUnit(Unit* victim) override
    {
        DoKillSay(m_creature, victim, SAY_KILL_1, SAY_KILL_2);
    }

    void JustDied(Unit* /*victim*/) override
    {
        DoScriptText(SAY_DEATH, m_creature);

        if (m_pInstance)
            m_pInstance->SetData(TYPE_THRALL_PART1, DONE);
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        // Haven't actually verified how taunting works in retail, but I imagine
        // it's something like this
        if (m_uiTauntPhase == 0 && m_creature->GetHealthPercent() <= 80.0f)
        {
            DoScriptText(SAY_TAUNT1, m_creature);
            ++m_uiTauntPhase;
        }
        else if (m_uiTauntPhase == 1 && m_creature->GetHealthPercent() <= 40.0f)
        {
            DoScriptText(SAY_TAUNT2, m_creature);
            ++m_uiTauntPhase;
        }

        // All spells are handled by behavioral AI
        Scripted_BehavioralAI::UpdateInCombatAI(uiDiff);

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_boss_captain_skarloc(Creature* pCreature)
{
    return new boss_captain_skarlocAI(pCreature);
}

void AddSC_boss_captain_skarloc()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "boss_captain_skarloc";
    pNewScript->GetAI = &GetAI_boss_captain_skarloc;
    pNewScript->RegisterSelf();
}
