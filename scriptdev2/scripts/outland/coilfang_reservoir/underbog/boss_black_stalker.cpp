/* Copyright (C) Corecraft
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
SDName: Boss_black_stalker
SD%Complete: 100
SDComment:
SDCategory: Coilfang Resevoir, Underbog
EndScriptData */

#include "precompiled.h"

enum
{
    SPELL_LEVITATE = 31704,
    SPELL_KNOCKBACK = 24199,
    SPELL_SUSPENSION = 31719,

    SPELL_CHAIN_LIGHTNING = 31717, // First CL at pull then 15 seconds interval
                                   // (normal). On heroic it seems between 4 and
                                   // 15 seconds
    SPELL_STATIC_CHARGE = 31715,   // 35 seconds interval

    NPC_SPORE_STRIDER = 22299, // Each 10 seconds
};

struct sSporeStriderSpawnPos
{
    float X, Y, Z;
};

const sSporeStriderSpawnPos SporeStriderSpawnPos[] = {
    {175.3, 32.8, 26.803}, {178.0, 19.0, 26.692}, {179.4, 0.32, 26.792},
    {170.4, -6.8, 26.792}, {153.9, -9.2, 26.792}, {138.6, -6.1, 26.792},
    {121.8, 11.6, 26.874}, {122.5, 27.9, 27.313}, {128.4, 43.3, 27.088},
    {141.9, 50.1, 27.079}, {155.2, 50.1, 27.299},
};
const uint32 SporeStriderSpawnSize = 11;

struct MANGOS_DLL_DECL boss_black_stalkerAI : public ScriptedAI
{
    boss_black_stalkerAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_bIsRegularMode = pCreature->GetMap()->IsRegularDifficulty();
        Reset();
    }

    ObjectGuid m_pulledTarget;
    bool m_bIsRegularMode;
    uint32 m_uiLevitateTimer;
    uint32 m_uiPullTimer;
    uint32 m_uiSuspendTimer;
    uint32 m_uiChainLightningTimer;
    uint32 m_uiStaticChargeTimer;
    uint32 m_uiSpawnSporeTimer;

    void Reset() override
    {
        m_uiLevitateTimer = 15000;
        m_uiChainLightningTimer = 1000;
        m_uiStaticChargeTimer = 25000;
        m_uiSpawnSporeTimer = 10000;
        m_uiPullTimer = 0;
        m_uiSuspendTimer = 0;
        m_pulledTarget = ObjectGuid();
    }

    void JustSummoned(Creature* pSummoned) override
    {
        if (pSummoned->AI())
            if (Player* target = pSummoned->FindNearestPlayer(120))
                pSummoned->AI()->AttackStart(target);
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        // Levitate
        if (m_uiLevitateTimer <= uiDiff)
        {
            if (Unit* pTarget = m_creature->SelectAttackingTarget(
                    ATTACKING_TARGET_RANDOM, 0))
            {
                if (DoCastSpellIfCan(pTarget, SPELL_LEVITATE, CAST_TRIGGERED) ==
                    CAST_OK)
                {
                    m_pulledTarget = pTarget->GetObjectGuid();
                    m_uiLevitateTimer = 24000;
                    m_uiPullTimer = 3000;
                    m_uiSuspendTimer = 4000;
                }
            }
        }
        else
            m_uiLevitateTimer -= uiDiff;

        if (m_uiPullTimer)
        {
            if (m_uiPullTimer <= uiDiff)
            {
                if (Unit* pTarget =
                        m_creature->GetMap()->GetUnit(m_pulledTarget))
                    DoCastSpellIfCan(pTarget, SPELL_KNOCKBACK, CAST_TRIGGERED);
                m_uiPullTimer = 0;
            }
            else
                m_uiPullTimer -= uiDiff;
        }

        if (m_uiSuspendTimer)
        {
            if (m_uiSuspendTimer <= uiDiff)
            {
                if (Unit* pTarget =
                        m_creature->GetMap()->GetUnit(m_pulledTarget))
                    pTarget->AddAuraThroughNewHolder(
                        SPELL_SUSPENSION, m_creature);
                m_uiSuspendTimer = 0;
                m_pulledTarget = ObjectGuid();
            }
            else
                m_uiSuspendTimer -= uiDiff;
        }

        // Cast Chain Lightning
        if (m_uiChainLightningTimer <= uiDiff)
        {
            if (DoCastSpellIfCan(
                    m_creature->getVictim(), SPELL_CHAIN_LIGHTNING) == CAST_OK)
                m_uiChainLightningTimer =
                    m_bIsRegularMode ? 15000 : urand(4000, 15000);
        }
        else
            m_uiChainLightningTimer -= uiDiff;

        // Cast Static Charge
        if (m_uiStaticChargeTimer <= uiDiff)
        {
            if (Unit* pTarget = m_creature->SelectAttackingTarget(
                    ATTACKING_TARGET_RANDOM, 0))
            {
                if (DoCastSpellIfCan(pTarget, SPELL_STATIC_CHARGE) == CAST_OK)
                    m_uiStaticChargeTimer = 35000;
            }
        }
        else
            m_uiStaticChargeTimer -= uiDiff;

        // Spawn Adds
        if (!m_bIsRegularMode)
        {
            if (m_uiSpawnSporeTimer <= uiDiff)
            {
                std::vector<sSporeStriderSpawnPos> availablePos(
                    SporeStriderSpawnPos,
                    SporeStriderSpawnPos + (SporeStriderSpawnSize - 1));

                // Spawn 3 adds at different locations
                for (int i = 0; i < 3; ++i)
                {
                    uint32 index = urand(0, availablePos.size() - 1);
                    sSporeStriderSpawnPos pos = availablePos[index];
                    availablePos.erase(availablePos.begin() + index);

                    // Calculate angle so we're looking directly at Black
                    // Stalker:
                    float angle = atan2(
                        pos.X - m_creature->GetX(), pos.Y - m_creature->GetY());
                    m_creature->SummonCreature(NPC_SPORE_STRIDER, pos.X, pos.Y,
                        pos.Z, angle, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT,
                        5000);
                }
                m_uiSpawnSporeTimer = urand(10000, 15000);
            }
            else
                m_uiSpawnSporeTimer -= uiDiff;
        }

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_boss_black_stalker(Creature* pCreature)
{
    return new boss_black_stalkerAI(pCreature);
}

void AddSC_boss_black_stalker()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "boss_black_stalker";
    pNewScript->GetAI = &GetAI_boss_black_stalker;
    pNewScript->RegisterSelf();
}
