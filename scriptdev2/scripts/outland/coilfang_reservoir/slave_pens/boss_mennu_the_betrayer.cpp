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
SDName: boss_mennu_the_betrayer
SD%Complete: 100%
SDComment:
SDCategory: Coilfang Resevoir, Slave Pens
EndScriptData */

#include "precompiled.h"

enum
{
    SAY_AGGRO_1 = -1611000,
    SAY_AGGRO_2 = -1611001,
    SAY_AGGRO_3 = -1611002,
    SAY_KILL_1 = -1611003,
    SAY_KILL_2 = -1611004,
    SAY_DEATH_1 = -1611005,

    SPELL_LIGHTNING_BOLT = 35010,
    SPELL_FIRE_NOVA_TOTEM = 31991,
    SPELL_STONESKIN_TOTEM = 31985,
    SPELL_EARTHGRAB_TOTEM = 31981,
    SPELL_HEALING_TOTEM = 34980,

    SPELL_FIRE_NOVA = 33132,
    SPELL_FIRE_NOVA_H = 43464,
    SPELL_INSTAKILL = 40450,
};

struct MANGOS_DLL_DECL boss_mennu_the_betrayerAI : public ScriptedAI
{
    boss_mennu_the_betrayerAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_bIsRegularMode = pCreature->GetMap()->IsRegularDifficulty();
        Reset();
    }

    bool m_bIsRegularMode;
    std::vector<uint32> m_remainingTotems;
    uint32 m_uiTotemTimer;
    uint32 m_uiLightningBoltTimer;

    void Reset() override
    {
        m_uiTotemTimer = 2000;
        m_uiLightningBoltTimer = 20000;
        GenerateRandomTotemOrder();
    }

    void GenerateRandomTotemOrder()
    {
        m_remainingTotems.clear();
        std::vector<uint32> temp;
        temp.push_back(SPELL_FIRE_NOVA_TOTEM);
        temp.push_back(SPELL_STONESKIN_TOTEM);
        temp.push_back(SPELL_EARTHGRAB_TOTEM);
        temp.push_back(SPELL_HEALING_TOTEM);
        for (uint32 i = 0; i < 4; ++i)
        {
            uint32 index = urand(0, temp.size() - 1);
            m_remainingTotems.push_back(temp[index]);
            temp.erase(temp.begin() + index);
        }
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

    void KilledUnit(Unit* victim) override
    {
        DoKillSay(m_creature, victim, SAY_KILL_1, SAY_KILL_2);
    }

    void JustDied(Unit* /*pKiller*/) override
    {
        DoScriptText(SAY_DEATH_1, m_creature);
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        // Drop a totem
        if (m_uiTotemTimer <= uiDiff)
        {
            if (m_remainingTotems.empty())
            {
                GenerateRandomTotemOrder();
                m_uiTotemTimer =
                    20000; // 30 seconds between start of each totem phase
            }
            else
            {
                if (DoCastSpellIfCan(m_creature, m_remainingTotems[0]) ==
                    CAST_OK)
                {
                    m_uiTotemTimer = 2000;
                    m_remainingTotems.erase(m_remainingTotems.begin());
                }
            }
        }
        else
            m_uiTotemTimer -= uiDiff;

        // Cast lightning bolt
        if (m_uiLightningBoltTimer <= uiDiff)
        {
            if (DoCastSpellIfCan(
                    m_creature->getVictim(), SPELL_LIGHTNING_BOLT) == CAST_OK)
                m_uiLightningBoltTimer = urand(20000, 25000);
        }
        else
            m_uiLightningBoltTimer -= uiDiff;

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_boss_mennu_the_betrayerAI(Creature* pCreature)
{
    return new boss_mennu_the_betrayerAI(pCreature);
}

struct MANGOS_DLL_DECL npc_corrupted_nova_totemAI : public ScriptedAI
{
    npc_corrupted_nova_totemAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_bIsRegularMode = pCreature->GetMap()->IsRegularDifficulty();
        Reset();
    }

    bool m_bIsRegularMode;
    bool m_bDidExplode;
    uint32 m_uiExplodeTimer;
    uint32 m_uiDestroyTimer;

    void Reset() override
    {
        m_creature->addUnitState(UNIT_STAT_CAN_NOT_REACT_OR_LOST_CONTROL);

        if (m_bIsRegularMode)
            m_uiExplodeTimer = 5000;
        else
            m_uiExplodeTimer = 15000;

        m_uiDestroyTimer = 0;
        m_bDidExplode = false;
    }

    void DamageTaken(Unit* /*pDoneBy*/, uint32& uiDmg) override
    {
        if (uiDmg >= m_creature->GetHealth() && !m_bIsRegularMode)
        {
            uiDmg = m_creature->GetHealth() - 1;
            if (!m_bDidExplode)
            {
                DoCast(
                    m_creature, SPELL_FIRE_NOVA_H); // Fire nova with ~6k damage
                m_bDidExplode = true;
                m_uiExplodeTimer = 0;
                m_uiDestroyTimer = 800;
            }
        }
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        if (m_uiExplodeTimer)
        {
            if (m_uiExplodeTimer <= uiDiff)
            {
                if (m_bIsRegularMode)
                    DoCast(m_creature,
                        SPELL_FIRE_NOVA); // Fire nova with ~2.5k damage
                else
                    DoCast(m_creature,
                        SPELL_FIRE_NOVA_H); // Fire nova with ~6k damage

                m_bDidExplode = true;
                m_uiExplodeTimer = 0;
                m_uiDestroyTimer = 800;
            }
            else
                m_uiExplodeTimer -= uiDiff;
        }

        if (m_uiDestroyTimer)
        {
            if (m_uiDestroyTimer <= uiDiff)
            {
                DoCast(m_creature, SPELL_INSTAKILL, true); // self-kill
                m_uiDestroyTimer = 0;
            }
            else
                m_uiDestroyTimer -= uiDiff;
        }
    }
};

CreatureAI* GetAI_npc_corrupted_nova_totem(Creature* pCreature)
{
    return new npc_corrupted_nova_totemAI(pCreature);
}

void AddSC_boss_mennu_the_betrayer()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "boss_mennu_the_betrayer";
    pNewScript->GetAI = &GetAI_boss_mennu_the_betrayerAI;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "npc_corrupted_nova_totem";
    pNewScript->GetAI = &GetAI_npc_corrupted_nova_totem;
    pNewScript->RegisterSelf();
}
