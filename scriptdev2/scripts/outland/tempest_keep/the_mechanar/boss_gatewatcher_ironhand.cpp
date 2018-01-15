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
SDName: Boss_Gatewatcher_Ironhand
SD%Complete: 100
SDComment:
SDCategory: Tempest Keep, The Mechanar
EndScriptData */

#include "mechanar.h"
#include "precompiled.h"

#define SAY_AGGRO_1 -1554006
#define SAY_HAMMER_1 -1554007
#define SAY_HAMMER_2 -1554008
#define SAY_KILL_1 -1554009
#define SAY_KILL_2 -1554010
#define SAY_DEATH_1 -1554011
#define EMOTE_HAMMER -1554012

#define SPELL_SHADOW_POWER 35322
#define SPELL_SHADOW_POWER_H 39193
#define SPELL_HAMMER_PUNCH 35326
#define SPELL_JACKHAMMER 35327
#define SPELL_JACKHAMMER_H 39194
#define SPELL_STREAM_OF_MACHINE_FLUID 35311

struct MANGOS_DLL_DECL boss_gatewatcher_iron_handAI : public ScriptedAI
{
    boss_gatewatcher_iron_handAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_pInstance = (instance_mechanar*)pCreature->GetInstanceData();
        m_bIsRegularMode = pCreature->GetMap()->IsRegularDifficulty();
        Reset();
    }

    instance_mechanar* m_pInstance;
    bool m_bIsRegularMode;

    uint32 Shadow_Power_Timer;
    uint32 Jackhammer_Timer;
    uint32 Stream_of_Machine_Fluid_Timer;
    uint32 m_uiJackhammerRemainingTimer;

    void Reset() override
    {
        Shadow_Power_Timer = 15000;
        Jackhammer_Timer = 30000;
        Stream_of_Machine_Fluid_Timer = 10000;
        m_uiJackhammerRemainingTimer = 0;
    }

    void Aggro(Unit* /*who*/) override
    {
        DoScriptText(SAY_AGGRO_1, m_creature);
    }

    void KilledUnit(Unit* victim) override
    {
        DoKillSay(m_creature, victim, SAY_KILL_1, SAY_KILL_2);
    }

    void JustDied(Unit* /*Killer*/) override
    {
        DoScriptText(SAY_DEATH_1, m_creature);

        if (!m_pInstance)
            return;

        if (GameObject* pGo =
                m_pInstance->GetSingleGameObjectFromStorage(GO_MOARG_1_DOOR))
            pGo->SetGoState(GO_STATE_ACTIVE);
        m_pInstance->SetData(TYPE_GATEWATCHER_IRONHAND, DONE);
    }

    void UpdateAI(const uint32 diff) override
    {
        // Return since we have no target
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        if (m_uiJackhammerRemainingTimer != 0)
        {
            if (m_uiJackhammerRemainingTimer <= diff)
            {
                if (urand(0, 3))
                    DoScriptText(
                        urand(0, 1) ? SAY_HAMMER_1 : SAY_HAMMER_2, m_creature);
                m_uiJackhammerRemainingTimer = 0;
            }
            else
            {
                m_uiJackhammerRemainingTimer -= diff;
                return;
            }
        }

        // Shadow Power
        if (Shadow_Power_Timer <= diff)
        {
            if (DoCastSpellIfCan(m_creature,
                    m_bIsRegularMode ? SPELL_SHADOW_POWER :
                                       SPELL_SHADOW_POWER_H) == CAST_OK)
                Shadow_Power_Timer = urand(18000, 25000);
        }
        else
            Shadow_Power_Timer -= diff;

        // Jack Hammer
        if (Jackhammer_Timer <= diff)
        {
            if (DoCastSpellIfCan(m_creature->getVictim(),
                    m_bIsRegularMode ? SPELL_JACKHAMMER : SPELL_JACKHAMMER_H) ==
                CAST_OK)
            {
                DoScriptText(EMOTE_HAMMER, m_creature);

                Jackhammer_Timer = 30000;
                m_uiJackhammerRemainingTimer = 9500;
            }
        }
        else
            Jackhammer_Timer -= diff;

        // Stream of Machine Fluid
        if (Stream_of_Machine_Fluid_Timer <= diff)
        {
            if (DoCastSpellIfCan(m_creature->getVictim(),
                    SPELL_STREAM_OF_MACHINE_FLUID) == CAST_OK)
                Stream_of_Machine_Fluid_Timer = urand(16000, 22000);
        }
        else
            Stream_of_Machine_Fluid_Timer -= diff;

        if (!m_creature->has_aura(
                m_bIsRegularMode ? SPELL_JACKHAMMER : SPELL_JACKHAMMER_H))
            DoMeleeAttackIfReady();
    }
};
CreatureAI* GetAI_boss_gatewatcher_iron_hand(Creature* pCreature)
{
    return new boss_gatewatcher_iron_handAI(pCreature);
}

void AddSC_boss_gatewatcher_iron_hand()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "boss_gatewatcher_iron_hand";
    pNewScript->GetAI = &GetAI_boss_gatewatcher_iron_hand;
    pNewScript->RegisterSelf();
}
