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
SDName: Boss_Nazan_And_Vazruden
SD%Complete: 100
SDComment:
SDCategory: Hellfire Citadel, Hellfire Ramparts
EndScriptData */

#include "hellfire_ramparts.h"
#include "precompiled.h"

enum
{
    SAY_INTRO = -1543017,
    SAY_AGGRO1 = -1543018,
    SAY_AGGRO2 = -1543019,
    SAY_AGGRO3 = -1543020,
    SAY_TAUNT = -1543021,
    SAY_KILL_1 = -1543022,
    SAY_KILL_2 = -1543023,
    SAY_DEATH = -1543024,
    EMOTE_DESCEND = -1543025,

    SPELL_SUMMON_VAZRUDEN = 30717,

    // vazruden
    SPELL_REVENGE = 19130,
    SPELL_REVENGE_H = 40392,

    // nazan
    SPELL_FIREBALL = 34653,
    SPELL_FIREBALL_H = 36920,

    SPELL_LIQUID_FIRE = 23971, // Summons liquid fire
    SPELL_LIQUID_FIRE_H = 30928,

    SPELL_CONE_OF_FIRE = 30926,
    SPELL_CONE_OF_FIRE_H = 36921,

    SPELL_BELLOW_ROAR_H = 39427,

    // misc
    POINT_ID_CENTER = 100,
    POINT_ID_FLYING = 101,
    POINT_ID_COMBAT = 102,

    NPC_NAZAN = 17536,
};

const float afCenterPos[3] = {
    -1399.401f, 1736.365f, 87.008f}; // moves here to drop off nazan
const float afCombatPos[3] = {
    -1413.848f, 1754.019f, 83.146f}; // moves here when decending

// Creature fly around platform by default.
// After "dropping off" Vazruden, transforms to mount (Nazan) and are then ready
// to fight when
// Vazruden reach 30% HP
struct MANGOS_DLL_DECL boss_vazruden_heraldAI : public ScriptedAI
{
    boss_vazruden_heraldAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        pCreature->SetActiveObjectState(true);
        m_pInstance = (instance_ramparts*)pCreature->GetInstanceData();
        m_bIsRegularMode = pCreature->GetMap()->IsRegularDifficulty();

        m_uiCheckSentiresTimer = 1000;
        Reset();
    }

    instance_ramparts* m_pInstance;
    bool m_bIsRegularMode;

    bool m_bIsEventInProgress;
    uint32 m_uiMovementTimer;
    uint32 m_uiFireballTimer;
    uint32 m_uiConeOfFireTimer;
    uint32 m_uiBellowingRoarTimer;
    uint32 m_uiCheckSentiresTimer;
    float air_x;
    float air_y;
    float air_z;

    uint32 m_uiLiquidFireTimer;
    ObjectGuid m_liquidFireTarget;

    ObjectGuid m_vazrudenGuid;

    void Reset() override
    {
        if (m_creature->GetEntry() != NPC_VAZRUDEN_HERALD)
            m_creature->UpdateEntry(NPC_VAZRUDEN_HERALD);

        m_creature->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE);
        m_creature->addUnitState(UNIT_STAT_IGNORE_PATHFINDING);

        m_uiMovementTimer = 0;
        m_uiLiquidFireTimer = 0;
        m_uiFireballTimer = 0;
        m_bIsEventInProgress = false;
        m_vazrudenGuid.Clear();
        m_uiConeOfFireTimer = 7000;
        m_uiBellowingRoarTimer = 20000;

        // see boss_onyxia
        // sort of a hack, it is unclear how this really work but the values
        // appear to be valid
        m_creature->SetByteValue(UNIT_FIELD_BYTES_1, 3,
            UNIT_BYTE1_FLAG_ALWAYS_STAND | UNIT_BYTE1_FLAG_UNK_2);
        m_creature->SetLevitate(true);
    }

    /* Disable Combat if we're not yet in action with Nazan */
    void MoveInLineOfSight(Unit* pWho) override
    {
        if (m_pInstance && m_pInstance->GetData(TYPE_NAZAN) != IN_PROGRESS)
            return;

        ScriptedAI::MoveInLineOfSight(pWho);
    }
    void Aggro(Unit* pWho) override
    {
        if (m_pInstance && m_pInstance->GetData(TYPE_NAZAN) != IN_PROGRESS)
            return;

        ScriptedAI::Aggro(pWho);
    }
    void AttackStart(Unit* pWho) override
    {
        if (m_pInstance && m_pInstance->GetData(TYPE_NAZAN) != IN_PROGRESS)
            return;

        ScriptedAI::AttackStart(pWho);
    }
    void EnterCombat(Unit* pEnemy) override
    {
        if (m_pInstance && m_pInstance->GetData(TYPE_NAZAN) != IN_PROGRESS)
            return;

        ScriptedAI::EnterCombat(pEnemy);
    }
    void AttackedBy(Unit* pAttacker) override
    {
        if (m_pInstance && m_pInstance->GetData(TYPE_NAZAN) != IN_PROGRESS)
            return;

        ScriptedAI::AttackedBy(pAttacker);
    }
    /* End of Combat Disabling Code */

    /* Movement Logic */
    void MovementInform(movement::gen uiType, uint32 uiPointId) override
    {
        if (!m_pInstance)
            return;

        if (uiType == movement::gen::waypoint)
        {
            if (m_uiMovementTimer || m_bIsEventInProgress)
                return;
        }

        if (uiType == movement::gen::point)
        {
            switch (uiPointId)
            {
            case POINT_ID_CENTER:
                DoSplit();
                break;
            case POINT_ID_COMBAT:
            {
                m_creature->clearUnitState(UNIT_STAT_IGNORE_PATHFINDING);
                m_pInstance->SetData(TYPE_NAZAN, IN_PROGRESS);

                // Landing
                // undo flying
                m_creature->SetByteValue(UNIT_FIELD_BYTES_1, 3, 0);
                m_creature->SetLevitate(false);

                m_pInstance->AttackNearestPlayer(m_creature);

                // Initialize for combat
                m_uiFireballTimer = 8000;

                break;
            }
            case POINT_ID_FLYING:
                if (m_bIsEventInProgress) // Additional check for wipe case,
                                          // while nazan is flying to this point
                    m_uiFireballTimer = 1;
                break;
            }
        }
    }

    void DoMoveToCenter()
    {
        DoScriptText(SAY_INTRO, m_creature);
        m_creature->GetPosition(air_x, air_y, air_z);
        m_creature->movement_gens.push(
            new movement::PointMovementGenerator(POINT_ID_CENTER,
                afCenterPos[0], afCenterPos[1], afCenterPos[2], false, true));

        // Put us in combat and add base threat to everyone
        m_creature->SetInCombatWithZone();
        const ThreatList& tl = m_creature->getThreatManager().getThreatList();
        for (const auto& elem : tl)
        {
            if (Unit* tar =
                    m_creature->GetMap()->GetUnit((elem)->getUnitGuid()))
                m_creature->AddThreat(tar, 0);
        }
    }

    void DoMoveToAir()
    {
        m_creature->movement_gens.remove_all(movement::gen::stopped);
        m_creature->movement_gens.push(new movement::PointMovementGenerator(
            POINT_ID_FLYING, air_x, air_y, air_z, false, true));
    }

    void DoSplit()
    {
        m_creature->UpdateEntry(NPC_NAZAN);

        DoCastSpellIfCan(m_creature, SPELL_SUMMON_VAZRUDEN);

        m_uiMovementTimer = 3000;

        // Let him idle for now
        m_creature->movement_gens.push(new movement::StoppedMovementGenerator(),
            movement::EVENT_LEAVE_COMBAT);
    }

    void JustSummoned(Creature* pSummoned) override
    {
        if (pSummoned->GetEntry() != NPC_VAZRUDEN)
            return;

        if (m_pInstance)
            m_pInstance->AttackNearestPlayer(pSummoned);

        m_vazrudenGuid = pSummoned->GetObjectGuid();

        if (m_pInstance)
            m_pInstance->SetData(TYPE_VAZRUDEN, IN_PROGRESS);
    }

    void JustDied(Unit* /*pKiller*/) override
    {
        if (m_pInstance)
            m_pInstance->SetData(TYPE_NAZAN, DONE);
    }

    void JustReachedHome() override
    {
        if (m_pInstance)
            m_pInstance->SetData(TYPE_NAZAN, FAIL);

        m_uiCheckSentiresTimer = 4000;
    }

    /* AI logic for Nazan */
    void UpdateAI(const uint32 uiDiff) override
    {
        if (m_uiLiquidFireTimer)
        {
            if (m_uiLiquidFireTimer <= uiDiff)
            {
                if (Unit* pTarget =
                        m_creature->GetMap()->GetUnit(m_liquidFireTarget))
                {
                    pTarget->CastSpell(pTarget,
                        m_bIsRegularMode ? SPELL_LIQUID_FIRE :
                                           SPELL_LIQUID_FIRE_H,
                        true, NULL, NULL, m_creature->GetObjectGuid());
                    m_liquidFireTarget = ObjectGuid();
                }

                m_uiLiquidFireTimer = 0;
            }
            else
                m_uiLiquidFireTimer -= uiDiff;
        }

        // Do not attempt target select if nazan is not already in combat (or
        // we'll evade)
        if (!(m_pInstance && m_pInstance->GetData(TYPE_NAZAN) == IN_PROGRESS) ||
            !m_creature->SelectHostileTarget() || !m_creature->getVictim())
        {
            if (m_uiCheckSentiresTimer && m_pInstance)
            {
                if (m_uiCheckSentiresTimer <= uiDiff)
                {
                    if (m_pInstance->AreSentriesDead(m_creature))
                    {
                        m_creature->RemoveFlag(
                            UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE);
                        m_uiMovementTimer = 1000;
                        m_bIsEventInProgress = true;
                        m_uiCheckSentiresTimer = 0;
                    }
                    else
                        m_uiCheckSentiresTimer = 1000;
                }
                else
                    m_uiCheckSentiresTimer -= uiDiff;
            }

            if (m_uiMovementTimer)
            {
                if (m_uiMovementTimer <= uiDiff)
                {
                    if (m_pInstance)
                    {
                        if (m_pInstance->GetData(TYPE_VAZRUDEN) == IN_PROGRESS)
                            DoMoveToAir();
                        else
                            DoMoveToCenter();
                    }
                    m_uiMovementTimer = 0;
                }
                else
                    m_uiMovementTimer -= uiDiff;
            }

            if (m_vazrudenGuid && m_uiFireballTimer)
            {
                if (m_uiFireballTimer <= uiDiff)
                {
                    if (Creature* pVazruden =
                            m_creature->GetMap()->GetCreature(m_vazrudenGuid))
                    {
                        if (Unit* pEnemy = pVazruden->SelectAttackingTarget(
                                ATTACKING_TARGET_RANDOM, 0))
                        {
                            if (DoCastSpellIfCan(
                                    pEnemy, m_bIsRegularMode ?
                                                SPELL_FIREBALL :
                                                SPELL_FIREBALL_H) == CAST_OK)
                            {
                                m_liquidFireTarget = pEnemy->GetObjectGuid();
                                m_uiLiquidFireTimer = uint32(
                                    m_creature->GetDistance(pEnemy) * 50);
                                m_uiFireballTimer =
                                    m_uiLiquidFireTimer + urand(2000, 7000);
                            }
                        }
                    }
                }
                else
                    m_uiFireballTimer -= uiDiff;
            }
            return;
        }

        // In Combat
        if (m_uiFireballTimer <= uiDiff)
        {
            if (Unit* pEnemy = m_creature->SelectAttackingTarget(
                    ATTACKING_TARGET_RANDOM, 0))
            {
                if (DoCastSpellIfCan(pEnemy,
                        m_bIsRegularMode ? SPELL_FIREBALL : SPELL_FIREBALL_H) ==
                    CAST_OK)
                {
                    m_uiLiquidFireTimer =
                        uint32(m_creature->GetDistance(pEnemy) * 80);
                    m_liquidFireTarget = pEnemy->GetObjectGuid();
                    m_uiFireballTimer = urand(8000, 14000);
                }
            }
        }
        else
            m_uiFireballTimer -= uiDiff;

        if (m_uiConeOfFireTimer < uiDiff)
        {
            if (DoCastSpellIfCan(m_creature->getVictim(),
                    m_bIsRegularMode ? SPELL_CONE_OF_FIRE :
                                       SPELL_CONE_OF_FIRE_H) == CAST_OK)
            {
                m_uiConeOfFireTimer = urand(7000, 14000);
                m_liquidFireTarget = m_creature->getVictim()->GetObjectGuid();
                m_uiLiquidFireTimer = 1200;
            }
        }
        else
            m_uiConeOfFireTimer -= uiDiff;

        if (!m_bIsRegularMode)
        {
            if (m_uiBellowingRoarTimer < uiDiff)
            {
                if (DoCastSpellIfCan(m_creature, SPELL_BELLOW_ROAR_H) ==
                    CAST_OK)
                    m_uiBellowingRoarTimer = urand(20000, 30000);
            }
            else
                m_uiBellowingRoarTimer -= uiDiff;
        }

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_boss_vazruden_herald(Creature* pCreature)
{
    return new boss_vazruden_heraldAI(pCreature);
}

struct MANGOS_DLL_DECL boss_vazrudenAI : public ScriptedAI
{
    boss_vazrudenAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_pInstance = (ScriptedInstance*)pCreature->GetInstanceData();
        m_bIsRegularMode = pCreature->GetMap()->IsRegularDifficulty();
        Reset();
    }

    ScriptedInstance* m_pInstance;
    bool m_bIsRegularMode;

    uint32 m_uiRevengeTimer;
    bool m_bHealthBelow;

    void Reset() override
    {
        m_bHealthBelow = false;
        m_uiRevengeTimer = urand(5500, 8400);
    }

    void Aggro(Unit* /*pWho*/) override
    {
        switch (urand(0, 2))
        {
        case 0:
            DoScriptText(SAY_AGGRO1, m_creature);
            break;
        case 1:
            DoScriptText(SAY_AGGRO2, m_creature);
            break;
        case 2:
            DoScriptText(SAY_AGGRO3, m_creature);
            break;
        }
    }

    void JustDied(Unit* /*pKiller*/) override
    {
        DoScriptText(SAY_DEATH, m_creature);

        if (m_pInstance)
            m_pInstance->SetData(TYPE_VAZRUDEN, DONE);
    }

    void JustReachedHome() override
    {
        if (m_pInstance)
        {
            m_pInstance->SetData(TYPE_VAZRUDEN, FAIL);
            if (Creature* pCreature = m_pInstance->GetSingleCreatureFromStorage(
                    NPC_VAZRUDEN_HERALD))
            {
                if (boss_vazruden_heraldAI* pAI =
                        dynamic_cast<boss_vazruden_heraldAI*>(pCreature->AI()))
                {
                    pAI->m_uiCheckSentiresTimer = 4000;
                }
            }
        }
    }

    void KilledUnit(Unit* victim) override
    {
        DoKillSay(m_creature, victim, SAY_KILL_1, SAY_KILL_2);
    }

    void PrepareAndDescendMount()
    {
        if (!m_pInstance)
            return;
        if (Creature* pHerald =
                m_pInstance->GetSingleCreatureFromStorage(NPC_VAZRUDEN_HERALD))
        {
            pHerald->SetWalk(false);
            pHerald->movement_gens.push(new movement::PointMovementGenerator(
                POINT_ID_COMBAT, afCombatPos[0], afCombatPos[1], afCombatPos[2],
                false, true));
            DoScriptText(EMOTE_DESCEND, pHerald);
        }
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        if (!m_bHealthBelow && m_creature->GetHealthPercent() <= 45.0f)
        {
            if (m_pInstance)
                PrepareAndDescendMount();

            m_bHealthBelow = true;
        }

        if (m_uiRevengeTimer < uiDiff)
        {
            if (DoCastSpellIfCan(m_creature->getVictim(),
                    m_bIsRegularMode ? SPELL_REVENGE : SPELL_REVENGE_H) ==
                CAST_OK)
                m_uiRevengeTimer = urand(11400, 14300);
        }
        else
            m_uiRevengeTimer -= uiDiff;

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_boss_vazruden(Creature* pCreature)
{
    return new boss_vazrudenAI(pCreature);
}

void AddSC_boss_nazan_and_vazruden()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "boss_vazruden";
    pNewScript->GetAI = &GetAI_boss_vazruden;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "boss_vazruden_herald";
    pNewScript->GetAI = &GetAI_boss_vazruden_herald;
    pNewScript->RegisterSelf();
}
