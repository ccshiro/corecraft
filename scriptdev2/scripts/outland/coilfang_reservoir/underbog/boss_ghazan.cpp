/* Copyright (C) 2012 Corecraft
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
SDName: Boss_Ghaz'an
SD%Complete: 95
SDComment: A visual bug (doesn't show) with Ghazan casting Acid Breath. Don't
know what's going wrong, to be honest (the spell shows fine when anyone but him
casts it)
SDCategory: Coilfang Resevoir, Underbog
EndScriptData */

#include "precompiled.h"

enum
{
    SPELL_ACID_BREATH = 34268, // In normal each 10 seconds. In heroic first
                               // time at 10 seconds, then randomly at between 1
                               // and 10 (or so it appears on videos)
    SPELL_ACID_SPIT = 34290,   // First time at engage, then seems like every 8
    // seconds on normal and between 4 and 8 on heroic.
    SPELL_ENRAGE = 15716,
    SPELL_TAIL_SWEEP = 34267, // Really don't know this timer, every group seem
                              // to have avoided it.
    SPELL_TAIL_SWEEP_H = 38737,
    SPELL_THRASH_AURA = 3417,
};

#define ASCEND_POINTS 9
DynamicWaypoint ascendPath[ASCEND_POINTS] = {
    DynamicWaypoint(209.1f, -480.4f, 54.9f),
    DynamicWaypoint(198.6f, -483.9f, 59.4f),
    DynamicWaypoint(176.6f, -486.7f, 67.4f),
    DynamicWaypoint(165.0f, -481.3f, 72.7f),
    DynamicWaypoint(158.7f, -468.8f, 75.4f),
    DynamicWaypoint(186.7f, -470.7f, 77.3f),
    DynamicWaypoint(186.7f, -470.7f, 77.3f),
    DynamicWaypoint(216.4f, -476.9f, 80.7f),
    DynamicWaypoint(252.49f, -460.9f, 81.37f, 3.54f),
};

struct MANGOS_DLL_DECL boss_ghazanAI : public ScriptedAI
{
    boss_ghazanAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_bAscended = false;
        m_bIsRegularMode = pCreature->GetMap()->IsRegularDifficulty();
        Reset();
    }

    bool m_bAscended;
    bool m_bIsRegularMode;
    bool m_bHasEnraged;
    uint32 m_uiAcidBreathTimer;
    uint32 m_uiAcidSpitTimer;
    uint32 m_uiTailSweepTimer;
    uint32 m_uiAscendCheckTimer;

    void Reset() override
    {
        m_uiAcidBreathTimer = 10000;
        m_uiAcidSpitTimer = 0;
        m_uiTailSweepTimer = 6000;
        m_bHasEnraged = false;

        DoCastSpellIfCan(m_creature, SPELL_THRASH_AURA, CAST_TRIGGERED);
    }

    void DoAscend()
    {
        m_bAscended = true;
        m_creature->movement_gens.remove_all(movement::gen::waypoint);
        std::vector<DynamicWaypoint> dwp(
            ascendPath, ascendPath + ASCEND_POINTS);
        m_creature->movement_gens.push(
            new movement::DynamicWaypointMovementGenerator(dwp, false), 0, 30);
        m_creature->movement_gens.push(new movement::RandomMovementGenerator(
            5.0f, G3D::Vector3(252.49f, -460.9f, 81.37f)));
    }

    void MovementInform(movement::gen uiType, uint32 uiPointId) override
    {
        if (uiType != movement::gen::waypoint)
            return;
        if (uiPointId == 0 && !m_bAscended)
        {
            if (GetClosestPlayer(m_creature, 120.0f) != nullptr)
                DoAscend();
        }
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        // Acid Breath
        if (m_uiAcidBreathTimer <= uiDiff)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_ACID_BREATH) == CAST_OK)
            {
                if (m_bIsRegularMode)
                    m_uiAcidBreathTimer = 10000;
                else
                    m_uiAcidBreathTimer =
                        urand(1000, 10000); // He can do it repeatedly in heroic
            }
        }
        else
            m_uiAcidBreathTimer -= uiDiff;

        // Acid Spit
        if (m_uiAcidSpitTimer <= uiDiff)
        {
            if (Unit* pTarget = m_creature->SelectAttackingTarget(
                    ATTACKING_TARGET_RANDOM, 0))
            {
                if (DoCastSpellIfCan(pTarget, SPELL_ACID_SPIT) == CAST_OK)
                {
                    if (m_bIsRegularMode)
                        m_uiAcidSpitTimer = 8000;
                    else
                        m_uiAcidSpitTimer = urand(4000, 8000);
                }
            }
        }
        else
            m_uiAcidSpitTimer -= uiDiff;

        // Tail Sweep
        if (m_uiTailSweepTimer <= uiDiff)
        {
            if (DoCastSpellIfCan(m_creature,
                    m_bIsRegularMode ? SPELL_TAIL_SWEEP : SPELL_TAIL_SWEEP_H) ==
                CAST_OK)
                m_uiTailSweepTimer = 12000;
        }
        else
            m_uiTailSweepTimer -= uiDiff;

        // Enrage
        if (m_creature->GetHealthPercent() <= 20 && !m_bHasEnraged)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_ENRAGE) == CAST_OK)
                m_bHasEnraged = true;
        }

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_boss_ghazan(Creature* pCreature)
{
    return new boss_ghazanAI(pCreature);
}

void AddSC_boss_ghazan()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "boss_ghazan";
    pNewScript->GetAI = &GetAI_boss_ghazan;
    pNewScript->RegisterSelf();
}
