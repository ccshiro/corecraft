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
SDName: boss_lieutenant_drake
SD%Complete: 100
SDComment: Script for GO (barrels) quest 10283 in instance script
SDCategory: Caverns of Time, Old Hillsbrad Foothills
EndScriptData */

#include "escort_ai.h"
#include "old_hillsbrad.h"
#include "precompiled.h"

/*######
## go_barrel_old_hillsbrad
######*/

bool GOUse_go_barrel_old_hillsbrad(Player* pPlayer, GameObject* pGo)
{
    if (ScriptedInstance* pInstance = (ScriptedInstance*)pGo->GetInstanceData())
    {
        pInstance->SetData(TYPE_BARREL_DIVERSION, SPECIAL);
        pGo->SetFlag(GAMEOBJECT_FLAGS, GO_FLAG_NO_INTERACT);
        pPlayer->destroy_item(ITEM_ENTRY_INCENDIARY_BOMBS, 1);
    }
    return false;
}

/*######
## boss_lieutenant_drake
######*/

enum
{
    SAY_ENTER = -1560006,
    SAY_AGGRO = -1560007,
    SAY_KILL_1 = -1560008,
    SAY_KILL_2 = -1560009,
    SAY_WHIRLWIND = -1560010,
    SAY_FEAR = -1560011,
    SAY_DEATH = -1560012,

    SPELL_WHIRLWIND = 31909,
    SPELL_HAMSTRING = 9080,
    SPELL_MORTAL_STRIKE = 31911,
    SPELL_FRIGHTENING_SHOUT = 33789,
    SPELL_EXPLODING_SHOT = 33792,
};

struct MANGOS_DLL_DECL boss_lieutenant_drakeAI : public ScriptedAI
{
    boss_lieutenant_drakeAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_uiEnterTimer = 1000;
        m_bHasWalkedDownStairs = false;
        Reset();
    }

    bool m_bHasWalkedDownStairs;
    uint32 m_uiEnterTimer;
    uint32 m_uiWhirlwindTimer;
    uint32 m_uiFearTimer;
    uint32 m_uiHamstringTimer;
    uint32 m_uiMortalStrikeTimer;
    uint32 m_uiExplodingShotTimer;

    void Reset() override
    {
        m_uiWhirlwindTimer = urand(10000, 15000);
        m_uiFearTimer = urand(15000, 30000);
        m_uiHamstringTimer = 8000;
        m_uiMortalStrikeTimer = 10000;
        m_uiExplodingShotTimer = urand(15000, 20000);
    }

    void Aggro(Unit* /*who*/) override { DoScriptText(SAY_AGGRO, m_creature); }

    void KilledUnit(Unit* victim) override
    {
        DoKillSay(m_creature, victim, SAY_KILL_1, SAY_KILL_2);
    }

    void JustDied(Unit* /*victim*/) override
    {
        DoScriptText(SAY_DEATH, m_creature);
    }

    void MovementInform(movement::gen gen_type, uint32 uiData) override
    {
        if (gen_type == movement::gen::waypoint && !m_bHasWalkedDownStairs &&
            uiData == 4)
            m_bHasWalkedDownStairs = true;
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
        {
            if (m_uiEnterTimer)
            {
                if (m_uiEnterTimer <= uiDiff)
                {
                    std::vector<DynamicWaypoint> wps;
                    wps.push_back(DynamicWaypoint(2130.2f, 71.1f, 64.6f));
                    wps.push_back(DynamicWaypoint(2128.1f, 70.9f, 64.4f));
                    wps.push_back(DynamicWaypoint(2127.0f, 78.9f, 60.3f));
                    wps.push_back(DynamicWaypoint(2125.4f, 88.7f, 54.8f));
                    wps.push_back(DynamicWaypoint(2116.5f, 96.6f, 52.5f));
                    m_creature->movement_gens.push(
                        new movement::DynamicWaypointMovementGenerator(
                            wps, false));
                    m_creature->movement_gens.remove_all(movement::gen::idle);
                    m_creature->movement_gens.push(
                        new movement::IdleMovementGenerator(
                            2116.4f, 93.1f, 52.5f, m_creature->GetO()));
                    DoScriptText(SAY_ENTER, m_creature);
                    m_uiEnterTimer = 0;
                }
                else
                    m_uiEnterTimer -= uiDiff;
            }

            if (m_bHasWalkedDownStairs &&
                !m_creature->movement_gens.has(movement::gen::waypoint) &&
                !m_creature->movement_gens.has(movement::gen::home))
            {
                std::vector<DynamicWaypoint> wps;
                wps.push_back(DynamicWaypoint(2110.8f, 128.6f, 52.4f));
                wps.push_back(DynamicWaypoint(2107.7f, 149.7f, 52.4f));
                wps.push_back(DynamicWaypoint(2130.6f, 188.7f, 52.5f));
                wps.push_back(DynamicWaypoint(2176.5f, 227.0f, 52.4f));
                // Headed back
                wps.push_back(DynamicWaypoint(2130.6f, 188.7f, 52.5f));
                wps.push_back(DynamicWaypoint(2107.7f, 149.7f, 52.4f));
                wps.push_back(DynamicWaypoint(2110.8f, 128.6f, 52.4f));
                wps.push_back(DynamicWaypoint(2116.5f, 96.6f, 52.5f));
                wps.push_back(DynamicWaypoint(2119.2f, 70.0f, 52.8f));
                m_creature->movement_gens.push(
                    new movement::DynamicWaypointMovementGenerator(wps, true));
            }

            return;
        }

        // Whirlwind
        if (m_uiWhirlwindTimer <= uiDiff)
        {
            if (DoCastSpellIfCan(m_creature->getVictim(), SPELL_WHIRLWIND) ==
                CAST_OK)
            {
                DoScriptText(SAY_WHIRLWIND, m_creature);
                m_uiWhirlwindTimer = urand(15000, 25000);
            }
        }
        else
            m_uiWhirlwindTimer -= uiDiff;

        // Fear
        if (m_uiFearTimer <= uiDiff)
        {
            if (DoCastSpellIfCan(m_creature->getVictim(),
                    SPELL_FRIGHTENING_SHOUT) == CAST_OK)
            {
                DoScriptText(SAY_FEAR, m_creature);
                m_uiFearTimer = urand(15000, 30000);
            }
        }
        else
            m_uiFearTimer -= uiDiff;

        // Hamstring
        if (m_uiHamstringTimer <= uiDiff)
        {
            if (DoCastSpellIfCan(m_creature->getVictim(), SPELL_HAMSTRING) ==
                CAST_OK)
                m_uiHamstringTimer = urand(10000, 20000);
        }
        else
            m_uiHamstringTimer -= uiDiff;

        // Moral Strike
        if (m_uiMortalStrikeTimer <= uiDiff)
        {
            if (DoCastSpellIfCan(
                    m_creature->getVictim(), SPELL_MORTAL_STRIKE) == CAST_OK)
                m_uiMortalStrikeTimer = urand(15000, 20000);
        }
        else
            m_uiMortalStrikeTimer -= uiDiff;

        // Exploding Shot
        if (m_uiExplodingShotTimer <= uiDiff)
        {
            if (!m_creature->CanReachWithMeleeAttack(m_creature->getVictim()))
            {
                if (DoCastSpellIfCan(m_creature->getVictim(),
                        SPELL_EXPLODING_SHOT) == CAST_OK)
                    m_uiExplodingShotTimer = urand(15000, 20000);
            }
        }
        else
            m_uiExplodingShotTimer -= uiDiff;

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_boss_lieutenant_drake(Creature* pCreature)
{
    return new boss_lieutenant_drakeAI(pCreature);
}

void AddSC_boss_lieutenant_drake()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "go_barrel_old_hillsbrad";
    pNewScript->pGOUse = &GOUse_go_barrel_old_hillsbrad;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "boss_lieutenant_drake";
    pNewScript->GetAI = &GetAI_boss_lieutenant_drake;
    pNewScript->RegisterSelf();
}
