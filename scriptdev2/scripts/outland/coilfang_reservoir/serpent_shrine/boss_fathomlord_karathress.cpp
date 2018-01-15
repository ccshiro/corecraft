/* Copyright (C) 2013 Corecraft <https://www.worldofcorecraft.com/>
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
SDName: boss_fathomlord_karathress
SD%Complete: 100
SDComment:
SDCategory: Coilfang Resevoir, Serpentshrine Cavern
EndScriptData */

#include "precompiled.h"
#include "serpentshrine_cavern.h"

enum
{
    SAY_AGGRO = -1548021,
    SAY_BLESSING_OF_THE_TIDES = -1548022,
    SAY_KILL_1 = -1548026,
    SAY_KILL_2 = -1548027,
    SAY_KILL_3 = -1548028,
    SAY_DEATH = -1548029,

    SPELL_CATACLYSMIC_BOLT = 38441,
    SPELL_SEAR_NOVA = 38445,
    // SPELL_BEAST_WITHIN              = 38373,
    // SPELL_SPITFIRE_TOTEM            = 38236,
    // SPELL_TIDAL_SURGE               = 38358,
    SPELL_BLESSING_OF_THE_TIDES = 38449,
    SPELL_BERSERK = 27680,
};

struct MANGOS_DLL_DECL boss_fathomlord_karathressAI : public ScriptedAI
{
    boss_fathomlord_karathressAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_instance = (ScriptedInstance*)pCreature->GetInstanceData();
        Reset();
    }

    ScriptedInstance* m_instance;
    uint32 m_cataclysmic;
    uint32 m_nova;
    uint32 m_beast;
    uint32 m_spitfire;
    uint32 m_tidal;
    uint32 m_berserk;
    bool m_tides;

    void Reset() override
    {
        m_cataclysmic = urand(5000, 15000);
        m_nova = urand(15000, 30000);
        m_beast = 48000;
        m_spitfire = 27000;
        m_tidal = 22000;
        m_berserk = 10 * MINUTE * IN_MILLISECONDS;
        m_tides = false;
    }

    void Aggro(Unit* /*pWho*/) override
    {
        if (m_instance)
            m_instance->SetData(TYPE_KARATHRESS, IN_PROGRESS);
        DoScriptText(SAY_AGGRO, m_creature);
    }

    void JustReachedHome() override
    {
        if (m_instance)
            m_instance->SetData(TYPE_KARATHRESS, FAIL);
    }

    void JustDied(Unit* /*pKiller*/) override
    {
        if (m_instance)
            m_instance->SetData(TYPE_KARATHRESS, DONE);
        DoScriptText(SAY_DEATH, m_creature);
    }

    void KilledUnit(Unit* victim) override
    {
        DoKillSay(m_creature, victim, SAY_KILL_1, SAY_KILL_2, SAY_KILL_3);
    }

    void BlessingOfTides()
    {
        if (m_instance)
        {
            bool blessed = false;
            for (uint32 i = NPC_CARIDBIS; i <= NPC_SHARKKIS; ++i)
                if (Creature* c = m_instance->GetSingleCreatureFromStorage(i))
                    if (c->isAlive())
                    {
                        c->InterruptNonMeleeSpells(false);
                        c->CastSpell(c, SPELL_BLESSING_OF_THE_TIDES, true);
                        blessed = true;
                    }
            if (blessed)
                DoScriptText(SAY_BLESSING_OF_THE_TIDES, m_creature);
        }
    }

    void UpdateAI(const uint32 diff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        if (!m_tides && m_creature->GetHealthPercent() < 76.0f)
        {
            BlessingOfTides();
            m_tides = true;
        }

        if (m_cataclysmic <= diff)
        {
            if (Unit* tar =
                    m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM,
                        0, SPELL_CATACLYSMIC_BOLT, SELECT_FLAG_POWER_MANA))
                if (DoCastSpellIfCan(tar, SPELL_CATACLYSMIC_BOLT) == CAST_OK)
                    m_cataclysmic = urand(5000, 15000);
        }
        else
            m_cataclysmic -= diff;

        if (m_berserk)
        {
            if (m_berserk <= diff)
            {
                if (DoCastSpellIfCan(m_creature, SPELL_BERSERK) == CAST_OK)
                    m_berserk = 0;
            }
            else
                m_berserk -= diff;
        }

        if (m_nova <= diff)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_SEAR_NOVA) == CAST_OK)
                m_nova = urand(15000, 30000);
        }
        else
            m_nova -= diff;

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_boss_fathomlord_karathress(Creature* pCreature)
{
    return new boss_fathomlord_karathressAI(pCreature);
}

void AddSC_boss_fathomlord_karathress()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "boss_fathomlord_karathress";
    pNewScript->GetAI = &GetAI_boss_fathomlord_karathress;
    pNewScript->RegisterSelf();
}
