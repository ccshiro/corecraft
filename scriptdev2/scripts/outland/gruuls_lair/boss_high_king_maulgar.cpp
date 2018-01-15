/* Copyright (C) 2013 Corecraft <https://www.worldofcorecraft.com>
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
SDName: boss_high_king_maulgar
SD%Complete: 100
SDComment:
SDCategory: Gruul's Lair
EndScriptData */

#include "gruuls_lair.h"
#include "precompiled.h"

enum
{
    SAY_AGGRO = -1565000,
    SAY_PHASE_TWO = -1565001,
    SAY_KILL_1 = -1565006,
    SAY_KILL_2 = -1565007,
    SAY_KILL_3 = -1565008,
    SAY_DEATH = -1565009,

    SPELL_ARCING_SMASH = 39144,
    SPELL_MIGHTY_BLOW = 33230,
    SPELL_WHIRLWIND = 33238,
    // Phase 2 only spells
    SPELL_FLURRY = 33232,
    SPELL_BERSERKER_CHARGE = 26561,
    SPELL_INTIMIDATING_ROAR = 16508,
};

struct MANGOS_DLL_DECL boss_high_king_maulgarAI : public ScriptedAI
{
    boss_high_king_maulgarAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_instance = (ScriptedInstance*)pCreature->GetInstanceData();
        Reset();
    }

    ScriptedInstance* m_instance;
    uint32 m_phase;
    uint32 m_blow;
    uint32 m_smash;
    uint32 m_whirlwind;
    uint32 m_charge;
    uint32 m_roar;

    void Reset() override
    {
        SetEquipmentSlots(true); // Load default
        m_phase = 1;
        m_blow = urand(15000, 20000);
        m_smash = urand(10000, 15000);
        m_whirlwind = 45000;
        m_charge = urand(5000, 15000);
        m_roar = urand(5000, 10000);
    }

    void JustReachedHome() override
    {
        if (m_instance)
            m_instance->SetData(TYPE_MAULGAR, FAIL);
    }

    void KilledUnit(Unit* victim) override
    {
        DoKillSay(m_creature, victim, SAY_KILL_1, SAY_KILL_2, SAY_KILL_3);
    }

    void JustDied(Unit* /*pKiller*/) override
    {
        if (m_instance)
            m_instance->SetData(TYPE_MAULGAR, DONE);

        DoScriptText(SAY_DEATH, m_creature);
    }

    void Aggro(Unit* /*pWho*/) override
    {
        if (m_instance)
            m_instance->SetData(TYPE_MAULGAR, IN_PROGRESS);

        DoScriptText(SAY_AGGRO, m_creature);
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        if (m_phase == 1)
        {
            if (m_creature->GetHealthPercent() < 51.0f)
            {
                if (DoCastSpellIfCan(m_creature, SPELL_FLURRY) == CAST_OK)
                {
                    DoScriptText(SAY_PHASE_TWO, m_creature);
                    SetEquipmentSlots(
                        false, EQUIP_UNEQUIP); // Discard the weapon
                    m_phase = 2;
                    m_whirlwind = 45000; // Resets Timer in phase swap
                }
                return;
            }
        }
        else
        {
            if (m_charge <= uiDiff)
            {
                if (Unit* target = m_creature->SelectAttackingTarget(
                        ATTACKING_TARGET_RANDOM, 0, SPELL_BERSERKER_CHARGE,
                        SELECT_FLAG_FARTHEST_AWAY))
                    if (DoCastSpellIfCan(target, SPELL_BERSERKER_CHARGE) ==
                        CAST_OK)
                    {
                        DoResetThreat();
                        m_charge = urand(30000, 40000);
                    }
            }
            else
                m_charge -= uiDiff;

            if (m_roar <= uiDiff)
            {
                if (DoCastSpellIfCan(m_creature->getVictim(),
                        SPELL_INTIMIDATING_ROAR) == CAST_OK)
                    m_roar = urand(20000, 30000);
            }
            else
                m_roar -= uiDiff;
        }

        // Spells used in all phases:

        if (m_whirlwind <= uiDiff)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_WHIRLWIND) == CAST_OK)
            {
                m_whirlwind = 60000;
                return;
            }
        }
        else
            m_whirlwind -= uiDiff;

        if (m_smash <= uiDiff)
        {
            if (DoCastSpellIfCan(m_creature->getVictim(), SPELL_ARCING_SMASH) ==
                CAST_OK)
                m_smash = urand(10000, 15000);
        }
        else
            m_smash -= uiDiff;

        if (m_blow <= uiDiff)
        {
            if (DoCastSpellIfCan(m_creature->getVictim(), SPELL_MIGHTY_BLOW) ==
                CAST_OK)
                m_blow = urand(15000, 20000);
        }
        else
            m_blow -= uiDiff;

        if (!m_creature->has_aura(SPELL_WHIRLWIND))
            DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_boss_high_king_maulgar(Creature* pCreature)
{
    return new boss_high_king_maulgarAI(pCreature);
}

void AddSC_boss_high_king_maulgar()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "boss_high_king_maulgar";
    pNewScript->GetAI = &GetAI_boss_high_king_maulgar;
    pNewScript->RegisterSelf();
}
