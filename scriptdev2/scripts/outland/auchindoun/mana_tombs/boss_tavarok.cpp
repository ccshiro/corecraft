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
SDName: Boss_Tavarok
SD%Complete: 100
SDComment:
SDCategory: Auchindoun, Mana Tombs
EndScriptData */

#include "mana_tombs.h"
#include "precompiled.h"

enum
{
    SPELL_EARTHQUAKE = 33919,
    SPELL_CRYSTAL_PRISON = 32361,
    SPELL_ARCING_SMASH_N = 8374,
    SPELL_ARCING_SMASH_H = 38761,
};

struct MANGOS_DLL_DECL boss_tavarokAI : public ScriptedAI
{
    boss_tavarokAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_pInstance = (instance_mana_tombs*)pCreature->GetInstanceData();
        m_bIsRegularMode = pCreature->GetMap()->IsRegularDifficulty();
        Reset();
    }

    instance_mana_tombs* m_pInstance;
    bool m_bIsRegularMode;

    uint32 m_uiArcingSmashTimer;
    uint32 m_uiCrystalPrisonTimer;
    uint32 m_uiEarthquakeTimer;

    void Reset() override
    {
        m_uiArcingSmashTimer = urand(7000, 10000);
        m_uiCrystalPrisonTimer = 15000;
        m_uiEarthquakeTimer = urand(7000, 10000);
    }

    void Aggro(Unit* /*pWho*/) override
    {
        if (m_pInstance)
            m_pInstance->SetData(TYPE_TAVAROK, IN_PROGRESS);
    }

    void JustReachedHome() override
    {
        if (m_pInstance)
            m_pInstance->SetData(TYPE_TAVAROK, FAIL);
    }

    void JustDied(Unit* /*pWho*/) override
    {
        if (m_pInstance)
            m_pInstance->SetData(TYPE_TAVAROK, DONE);
    }

    void SpellDamageCalculation(const Unit* /*pDoneTo*/, int32& iDamage,
        const SpellEntry* pSpell, SpellEffectIndex effectIndex) override
    {
        if (pSpell->Id == SPELL_EARTHQUAKE && effectIndex == EFFECT_INDEX_1)
        {
            // Not exact values, but ~about this: (gotten from videos)
            if (m_bIsRegularMode)
                iDamage = urand(640, 880);
            else
                iDamage = urand(1480, 1680);
        }
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        if (m_uiArcingSmashTimer <= uiDiff)
        {
            if (DoCastSpellIfCan(m_creature->getVictim(),
                    m_bIsRegularMode ? SPELL_ARCING_SMASH_N :
                                       SPELL_ARCING_SMASH_H) == CAST_OK)
                m_uiArcingSmashTimer = urand(7000, 10000);
        }
        else
            m_uiArcingSmashTimer -= uiDiff;

        if (m_uiCrystalPrisonTimer <= uiDiff)
        {
            Unit* pTarget =
                m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 1);
            if (DoCastSpellIfCan(pTarget ? pTarget : m_creature->getVictim(),
                    SPELL_CRYSTAL_PRISON) == CAST_OK)
                m_uiCrystalPrisonTimer = 15000;
        }
        else
            m_uiCrystalPrisonTimer -= uiDiff;

        if (m_uiEarthquakeTimer <= uiDiff)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_EARTHQUAKE) == CAST_OK)
                m_uiEarthquakeTimer = urand(20000, 30000);
        }
        else
            m_uiEarthquakeTimer -= uiDiff;

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_boss_tavarok(Creature* pCreature)
{
    return new boss_tavarokAI(pCreature);
}

void AddSC_boss_tavarok()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "boss_tavarok";
    pNewScript->GetAI = &GetAI_boss_tavarok;
    pNewScript->RegisterSelf();
}
