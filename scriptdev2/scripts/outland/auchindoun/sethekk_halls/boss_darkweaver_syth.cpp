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
SDName: Boss_Darkweaver_Syth
SD%Complete: 100
SDComment:
SDCategory: Auchindoun, Sethekk Halls
EndScriptData */

#include "precompiled.h"
#include "sethekk_halls.h"

#define SHOCK_COUNT 4
#define HEROIC_OFFSET 4
const static uint32 SythShocks[SHOCK_COUNT + HEROIC_OFFSET] = {
    33534, // Arcane
    15039, // Flame
    12548, // Frost
    33620, // Shadow

    38135, // Arcane
    15616, // Flame
    21401, // Frost
    38136, // Shadow
};

enum
{
    SAY_SUMMON = -1556000,
    SAY_AGGRO_1 = -1556001,
    SAY_AGGRO_2 = -1556002,
    SAY_AGGRO_3 = -1556003,
    SAY_KILL_1 = -1556004,
    SAY_KILL_2 = -1556005,
    SAY_DEATH = -1556006,

    SPELL_CHAIN_LIGHTNING_N = 15659,
    SPELL_CHAIN_LIGHTNING_H = 15305,

    SPELL_SUMMON_SYTH_FIRE = 33537,   // Spawns 19203
    SPELL_SUMMON_SYTH_ARCANE = 33538, // Spawns 19205
    SPELL_SUMMON_SYTH_FROST = 33539,  // Spawns 19204
    SPELL_SUMMON_SYTH_SHADOW = 33540, // Spawns 19206

    // Npc entries
    NPC_FIRE_ELEMENTAL = 19203,
    NPC_FROST_ELEMENTAL = 19204,
    NPC_ARCANE_ELEMENTAL = 19205,
    NPC_SHADOW_ELEMENTAL = 19206,
};

struct MANGOS_DLL_DECL boss_darkweaver_sythAI : public ScriptedAI
{
    boss_darkweaver_sythAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_pInstance = (ScriptedInstance*)pCreature->GetInstanceData();
        m_bIsRegularMode = pCreature->GetMap()->IsRegularDifficulty();
        Reset();
    }

    ScriptedInstance* m_pInstance;
    bool m_bIsRegularMode;
    uint32 m_uiChainlightningTimer;
    uint32 m_uiShockTimers[SHOCK_COUNT];
    std::vector<ObjectGuid> m_spawnedAdds;

    float m_fHpCheck;

    // Spell timers depend on eachother
    void SetSpellTimers()
    {
        m_uiChainlightningTimer = urand(10000, 15000);
        for (auto& elem : m_uiShockTimers)
            elem = urand(1, m_uiChainlightningTimer - 1500);
    }

    void DespawnAdds()
    {
        for (auto& elem : m_spawnedAdds)
        {
            if (Creature* summon = m_creature->GetMap()->GetPet(elem))
                summon->ForcedDespawn();
        }
        m_spawnedAdds.clear();
    }

    void Reset() override
    {
        SetSpellTimers();

        m_fHpCheck = 90.0f;

        DespawnAdds();
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
            m_pInstance->SetData(TYPE_SYTH, IN_PROGRESS);
    }

    void JustDied(Unit* /*pKiller*/) override
    {
        DoScriptText(SAY_DEATH, m_creature);

        DespawnAdds();

        if (m_pInstance)
            m_pInstance->SetData(TYPE_SYTH, DONE);
    }

    void JustReachedHome() override
    {
        if (m_pInstance)
            m_pInstance->SetData(TYPE_SYTH, FAIL);
    }

    void KilledUnit(Unit* victim) override
    {
        DoKillSay(m_creature, victim, SAY_KILL_1, SAY_KILL_2);
    }

    void JustSummoned(Creature* pSummoned) override
    {
        switch (pSummoned->GetEntry())
        {
        case NPC_FIRE_ELEMENTAL:
            pSummoned->ApplySpellImmune(
                0, IMMUNITY_SCHOOL, SPELL_SCHOOL_MASK_FIRE, true);
            break;
        case NPC_FROST_ELEMENTAL:
            pSummoned->ApplySpellImmune(
                0, IMMUNITY_SCHOOL, SPELL_SCHOOL_MASK_FROST, true);
            break;
        case NPC_ARCANE_ELEMENTAL:
            pSummoned->ApplySpellImmune(
                0, IMMUNITY_SCHOOL, SPELL_SCHOOL_MASK_ARCANE, true);
            break;
        case NPC_SHADOW_ELEMENTAL:
            pSummoned->ApplySpellImmune(
                0, IMMUNITY_SCHOOL, SPELL_SCHOOL_MASK_SHADOW, true);
            break;
        }

        m_spawnedAdds.push_back(pSummoned->GetObjectGuid());

        if (Unit* pTarget =
                m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0))
            pSummoned->AI()->AttackStart(pTarget);
    }

    // Wrapper to handle the elementals summon
    void SythSummoning()
    {
        DoScriptText(SAY_SUMMON, m_creature);

        DoCastSpellIfCan(
            m_creature, SPELL_SUMMON_SYTH_ARCANE, CAST_TRIGGERED); // front
        DoCastSpellIfCan(
            m_creature, SPELL_SUMMON_SYTH_FIRE, CAST_TRIGGERED); // back
        DoCastSpellIfCan(
            m_creature, SPELL_SUMMON_SYTH_FROST, CAST_TRIGGERED); // left
        DoCastSpellIfCan(
            m_creature, SPELL_SUMMON_SYTH_SHADOW, CAST_TRIGGERED); // right
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        // Summon elementals at 90%, 50% and 10% health
        if (m_creature->GetHealthPercent() <= m_fHpCheck)
        {
            if (!m_creature->IsNonMeleeSpellCasted(false))
            {
                SythSummoning();
                m_fHpCheck -= 40.0f;
            }
        }

        // Update lightning timer, but don't cast until shocks are depleted
        if (m_uiChainlightningTimer <= uiDiff)
            m_uiChainlightningTimer = 0;
        else
            m_uiChainlightningTimer -= uiDiff;

        // Cast shocks until we deplete them
        bool foundShock = false;
        for (uint32 i = 0; i < SHOCK_COUNT; ++i)
        {
            if (m_uiShockTimers[i])
            {
                foundShock = true;

                if (m_uiShockTimers[i] <= uiDiff)
                {
                    if (DoCastSpellIfCan(m_creature->getVictim(),
                            SythShocks[m_bIsRegularMode ? i :
                                                          i + HEROIC_OFFSET]) ==
                        CAST_OK)
                        m_uiShockTimers[i] = 0;
                }
                else
                    m_uiShockTimers[i] -= uiDiff;
            }
        }

        // Cast chain lightning when all shocks are casted and timer is over
        if (!foundShock && !m_uiChainlightningTimer)
        {
            if (DoCastSpellIfCan(m_creature->getVictim(),
                    m_bIsRegularMode ? SPELL_CHAIN_LIGHTNING_N :
                                       SPELL_CHAIN_LIGHTNING_H) == CAST_OK)
                SetSpellTimers(); // Set new spell timers
        }

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_boss_darkweaver_syth(Creature* pCreature)
{
    return new boss_darkweaver_sythAI(pCreature);
}

void AddSC_boss_darkweaver_syth()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "boss_darkweaver_syth";
    pNewScript->GetAI = &GetAI_boss_darkweaver_syth;
    pNewScript->RegisterSelf();
}
