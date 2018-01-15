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
SDName: Boss_Warchief_Kargath_Bladefist
SD%Complete: 100
SDComment:
SDCategory: Hellfire Citadel, Shattered Halls
EndScriptData */

/* ContentData
boss_warchief_kargath_bladefist
EndContentData */

#include "precompiled.h"
#include "shattered_halls.h"

enum
{
    SAY_AGGRO1 = -1540042,
    SAY_AGGRO2 = -1540043,
    SAY_AGGRO3 = -1540044,
    SAY_KILL_1 = -1540045,
    SAY_KILL_2 = -1540046,
    SAY_DEATH = -1540047,
    SAY_EVADE = -1540048,

    SPELL_BLADE_DANCE = 30739,
    SPELL_CHARGE_H = 25821,
    SPELL_SWEEPING_STRIKES = 35429,

    TARGET_NUM = 5,

    NPC_SHATTERED_ASSASSIN = 17695,
    NPC_HEARTHEN_GUARD = 17621,
    NPC_SHARPSHOOTER_GUARD = 17622,
    NPC_REAVER_GUARD = 17623,
};

float AssaEntry[4] = {287.3f, -79.9f, 1.974f, 3.14f}; // y -8
float AssaExit[4] = {179.2f, -79.9f, 1.811f, 0.00f};  // y -8
float AddsSpawn[4] = {322.7f, -84.4f, 1.931f, 3.14f};

float InitialBlade[4][3] = {{246.7f, -101.3f, 4.938f},
    {212.0f, -101.6f, 4.938f}, {213.1f, -67.13f, 4.938f},
    {246.0f, -67.08f, 4.938f}};

// MMaps bugs if he has to run all this way unassisted.
#define OMROGG_PATH_LEN 10
static DynamicWaypoint OmroggPath[OMROGG_PATH_LEN] = {
    DynamicWaypoint(374.69f, 29.37f, -8.0541f, 100, 0, true),
    DynamicWaypoint(374.64f, 17.37f, -0.0017f, 100, 0, true),
    DynamicWaypoint(374.04f, 13.68f, 1.0579f, 100, 0, true),
    DynamicWaypoint(373.86f, -32.86f, 1.9191f, 100, 0, true),
    DynamicWaypoint(372.67f, -70.83f, 1.9103f, 100, 0, true),
    DynamicWaypoint(357.64f, -83.77f, 1.9275f, 100, 0, true),
    DynamicWaypoint(303.04f, -84.12f, 1.9348f, 100, 0, true),
    DynamicWaypoint(274.22f, -84.00f, 2.3063f, 100, 0, true),
    DynamicWaypoint(255.79f, -84.15f, 4.6605f, 100, 0, true),
    DynamicWaypoint(232.53f, -83.40f, 3.1072f, 100, 0, true)};

struct MANGOS_DLL_DECL boss_warchief_kargath_bladefistAI : public ScriptedAI
{
    boss_warchief_kargath_bladefistAI(Creature* pCreature)
      : ScriptedAI(pCreature)
    {
        m_pInstance = (ScriptedInstance*)pCreature->GetInstanceData();
        m_bIsRegularMode = pCreature->GetMap()->IsRegularDifficulty();
        m_grpIds[0] = m_grpIds[1] = 0;
        Reset();
    }

    ScriptedInstance* m_pInstance;
    bool m_bIsRegularMode;

    GUIDVector m_vAddGuids;
    GUIDVector m_vAssassinGuids;

    uint32 m_uiResetPosTimer;
    uint32 m_uiChargeTimer;
    uint32 m_uiBladeDanceTimer;
    uint32 m_uiSummonAssistantTimer;
    uint32 m_uiWaitTimer;
    uint32 m_uiBladeHitTimer;
    uint32 m_uiSweepingStrikesTimer;
    int32 m_grpIds[2];

    uint32 m_uiAssassinsTimer;

    bool m_bInBlade;

    uint32 m_uiTargetNum;

    void AttackStart(Unit* pWho) override
    {
        if (m_bInBlade)
            return;

        ScriptedAI::AttackStart(pWho);
    }

    void Reset() override
    {
        m_bInBlade = false;
        m_uiWaitTimer = 0;

        m_uiChargeTimer = 0;
        m_uiBladeDanceTimer = 30000;
        m_uiBladeHitTimer = 0;
        m_uiSummonAssistantTimer = 20000;
        m_uiAssassinsTimer = 1500; // Basically right away @ retail
        m_uiResetPosTimer = 3000;
        m_uiSweepingStrikesTimer = 10000;

        // Reset O'mrogg if he's still alive
        if (Creature* omrogg = m_creature->GetMap()->GetCreature(ObjectGuid(
                HIGHGUID_UNIT, (uint32)NPC_OMROGG, (uint32)NPC_OMROGG_GUID)))
            if (omrogg->isAlive() && omrogg->AI())
                omrogg->AI()->Reset();
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

        if (m_pInstance)
            m_pInstance->SetData(TYPE_BLADEFIST, IN_PROGRESS);

        // O'mrogg has increased visibility, so this'll actually work:
        if (Creature* omrogg = m_creature->GetMap()->GetCreature(ObjectGuid(
                HIGHGUID_UNIT, (uint32)NPC_OMROGG, (uint32)NPC_OMROGG_GUID)))
            if (omrogg->isAlive())
            {
                omrogg->SetAggroDistance(60.0f);
                std::vector<DynamicWaypoint> path(
                    OmroggPath, OmroggPath + (OMROGG_PATH_LEN - 1));
                omrogg->movement_gens.push(
                    new movement::DynamicWaypointMovementGenerator(path, true));
            }
    }

    void JustSummoned(Creature* pSummoned) override
    {
        switch (pSummoned->GetEntry())
        {
        case NPC_HEARTHEN_GUARD:
        case NPC_SHARPSHOOTER_GUARD:
        case NPC_REAVER_GUARD:
            if (Unit* pTarget = m_creature->SelectAttackingTarget(
                    ATTACKING_TARGET_RANDOM, 0))
                pSummoned->AI()->AttackStart(pTarget);

            m_vAddGuids.push_back(pSummoned->GetObjectGuid());
            break;
        case NPC_SHATTERED_ASSASSIN:
            m_vAssassinGuids.push_back(pSummoned->GetObjectGuid());
            break;
        }
    }

    void KilledUnit(Unit* victim) override
    {
        DoKillSay(m_creature, victim, SAY_KILL_1, SAY_KILL_2);
    }

    void JustDied(Unit* /*pKiller*/) override
    {
        DoScriptText(SAY_DEATH, m_creature);
        DoDespawnAdds();

        if (m_pInstance)
            m_pInstance->SetData(TYPE_BLADEFIST, DONE);
    }

    void JustReachedHome() override
    {
        m_creature->SetSpeedRate(MOVE_RUN, 2.0f);

        DoDespawnAdds();

        if (m_pInstance)
            m_pInstance->SetData(TYPE_BLADEFIST, FAIL);

        if (m_grpIds[0] < 0 && m_grpIds[1] < 0)
        {
            m_creature->GetMap()->GetCreatureGroupMgr().DeleteGroup(
                m_grpIds[0]);
            m_creature->GetMap()->GetCreatureGroupMgr().DeleteGroup(
                m_grpIds[1]);
            m_grpIds[0] = m_grpIds[1] = 0;
        }
    }

    void MovementInform(movement::gen uiType, uint32 uiPointId) override
    {
        if (m_bInBlade)
        {
            if (uiType != movement::gen::point)
                return;

            if (uiPointId == 1)
            {
                // Get closest intial position
                uint32 index = 0;
                float dist = 1000.0f;
                for (uint32 i = 0; i < 4; ++i)
                {
                    float d = m_creature->GetDistance2d(
                        InitialBlade[i][0], InitialBlade[i][1]);
                    if (d < dist)
                    {
                        dist = d;
                        index = i;
                    }
                }
                float* pos = InitialBlade[index];
                m_creature->movement_gens.push(
                    new movement::PointMovementGenerator(
                        2, pos[0], pos[1], pos[2], false, true),
                    movement::EVENT_LEAVE_COMBAT);
            }
            else if (uiPointId == 2)
            {
                m_uiWaitTimer = 1; // Take a new point in our update code
            }
        }
    }

    void DoDespawnAdds()
    {
        for (GUIDVector::const_iterator itr = m_vAddGuids.begin();
             itr != m_vAddGuids.end(); ++itr)
        {
            if (Creature* pTemp = m_creature->GetMap()->GetCreature(*itr))
                pTemp->ForcedDespawn();
        }

        m_vAddGuids.clear();

        // Do not despawn assassins if they're in combat
        for (GUIDVector::const_iterator itr = m_vAssassinGuids.begin();
             itr != m_vAssassinGuids.end(); ++itr)
        {
            if (Creature* pTemp = m_creature->GetMap()->GetCreature(*itr))
            {
                if (!pTemp->isInCombat())
                    pTemp->ForcedDespawn();
            }
        }

        m_vAssassinGuids.clear();
    }

    void SpawnAssassins()
    {
        m_grpIds[0] =
            m_creature->GetMap()->GetCreatureGroupMgr().CreateNewGroup(
                "Assassins Kargath One Temp", true);
        CreatureGroup* grp1 =
            m_creature->GetMap()->GetCreatureGroupMgr().GetGroup(m_grpIds[0]);
        if (grp1)
            grp1->AddFlag(CREATURE_GROUP_FLAG_CANNOT_ASSIST);
        m_grpIds[1] =
            m_creature->GetMap()->GetCreatureGroupMgr().CreateNewGroup(
                "Assassins Kargath Two Temp", true);
        CreatureGroup* grp2 =
            m_creature->GetMap()->GetCreatureGroupMgr().GetGroup(m_grpIds[1]);
        if (grp2)
            grp2->AddFlag(CREATURE_GROUP_FLAG_CANNOT_ASSIST);
        if (Creature* c = m_creature->SummonCreature(NPC_SHATTERED_ASSASSIN,
                AssaEntry[0], AssaEntry[1], AssaEntry[2], AssaEntry[3],
                TEMPSUMMON_MANUAL_DESPAWN, 24000))
            if (grp1)
                grp1->AddMember(c, false);
        if (Creature* c = m_creature->SummonCreature(NPC_SHATTERED_ASSASSIN,
                AssaEntry[0], AssaEntry[1] - 8.0f, AssaEntry[2], AssaEntry[3],
                TEMPSUMMON_MANUAL_DESPAWN, 24000))
            if (grp1)
                grp1->AddMember(c, false);
        if (Creature* c = m_creature->SummonCreature(NPC_SHATTERED_ASSASSIN,
                AssaExit[0], AssaExit[1], AssaExit[2], AssaExit[3],
                TEMPSUMMON_MANUAL_DESPAWN, 24000))
            if (grp2)
                grp2->AddMember(c, false);
        if (Creature* c = m_creature->SummonCreature(NPC_SHATTERED_ASSASSIN,
                AssaExit[0], AssaExit[1] - 8.0f, AssaExit[2], AssaExit[3],
                TEMPSUMMON_MANUAL_DESPAWN, 24000))
            if (grp2)
                grp2->AddMember(c, false);
    }

    Player* GetBladeTarget()
    {
        auto players = GetAllPlayersInObjectRangeCheckInCell(m_creature, 40.0f);
        std::vector<Player*> ableTargets;
        ableTargets.reserve(players.size());
        for (auto& player : players)
            if ((player)->isAlive() && !(player)->isGameMaster() &&
                m_creature->IsWithinWmoLOSInMap(player))
                ableTargets.push_back(player);

        if (ableTargets.empty())
            return m_creature->FindNearestPlayer(
                120.0f); // Just pick someone and do it on them (won't look
                         // awesome but we shouldn't be able to avoid this
                         // attack)

        return ableTargets[urand(0, ableTargets.size() - 1)];
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        // Check if out of range
        if (EnterEvadeIfOutOfCombatArea(uiDiff))
        {
            DoScriptText(SAY_EVADE, m_creature);
            return;
        }

        if (m_uiAssassinsTimer)
        {
            if (m_uiAssassinsTimer <= uiDiff)
            {
                SpawnAssassins();
                m_uiAssassinsTimer = 0;
            }
            else
                m_uiAssassinsTimer -= uiDiff;
        }

        if (m_uiSummonAssistantTimer < uiDiff)
        {
            switch (urand(0, 2))
            {
            case 0:
                m_creature->SummonCreature(NPC_HEARTHEN_GUARD, AddsSpawn[0],
                    AddsSpawn[1], AddsSpawn[2], AddsSpawn[3],
                    TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 20000);
                break;
            case 1:
                m_creature->SummonCreature(NPC_SHARPSHOOTER_GUARD, AddsSpawn[0],
                    AddsSpawn[1], AddsSpawn[2], AddsSpawn[3],
                    TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 20000);
                break;
            case 2:
                m_creature->SummonCreature(NPC_REAVER_GUARD, AddsSpawn[0],
                    AddsSpawn[1], AddsSpawn[2], AddsSpawn[3],
                    TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 20000);
                break;
            }

            m_uiSummonAssistantTimer = 20000;
        }
        else
            m_uiSummonAssistantTimer -= uiDiff;

        if (m_bInBlade)
        {
            if (m_uiBladeHitTimer)
            {
                if (m_uiBladeHitTimer <= uiDiff)
                {
                    DoCastSpellIfCan(m_creature, SPELL_BLADE_DANCE);
                    m_uiBladeHitTimer = 1000;
                }
                else
                    m_uiBladeHitTimer -= uiDiff;
            }

            if (m_uiWaitTimer)
            {
                if (m_uiWaitTimer <= uiDiff)
                {
                    if (m_uiTargetNum == 0)
                    {
                        // stop bladedance
                        m_bInBlade = false;
                        m_creature->SetSpeedRate(MOVE_RUN, 2.0f);
                        m_creature->movement_gens.remove_all(
                            movement::gen::stopped);
                        m_uiWaitTimer = 0;
                        if (!m_bIsRegularMode)
                            m_uiChargeTimer = urand(2000, 4000);
                        m_uiSweepingStrikesTimer = 10000;
                    }
                    else
                    {
                        // pick a random player in LoS as charge target
                        Player* target = GetBladeTarget();
                        if (!target)
                        {
                            EnterEvadeMode();
                            return;
                        }
                        m_uiBladeHitTimer = 1000;
                        float dist = 35.0f - m_creature->GetDistance(target);
                        if (dist < 0)
                            dist = 0;
                        auto pos = target->GetPoint(
                            target->GetAngle(m_creature) + M_PI_F, dist);
                        m_creature->movement_gens.push(
                            new movement::PointMovementGenerator(
                                1, pos.x, pos.y, pos.z, false, true),
                            movement::EVENT_LEAVE_COMBAT);
                        m_uiWaitTimer = 3000; // Timeout
                        --m_uiTargetNum;
                    }
                }
                else
                    m_uiWaitTimer -= uiDiff;
            }
        }
        else // !m_bInBlade
        {
            if (m_uiBladeDanceTimer < uiDiff)
            {
                m_uiTargetNum = TARGET_NUM;
                m_uiWaitTimer = 3000; // Timeout
                m_bInBlade = true;
                m_uiBladeDanceTimer = 30000;
                m_uiBladeHitTimer = 0;
                m_creature->SetSpeedRate(MOVE_RUN, 4.0f);

                // Move to a random start position
                float* pos = InitialBlade[urand(0, 3)];
                m_creature->movement_gens.push(
                    new movement::StoppedMovementGenerator(),
                    movement::EVENT_LEAVE_COMBAT);
                m_creature->movement_gens.push(
                    new movement::PointMovementGenerator(
                        2, pos[0], pos[1], pos[2], false, true),
                    movement::EVENT_LEAVE_COMBAT);
                return;
            }
            else
                m_uiBladeDanceTimer -= uiDiff;

            if (m_uiChargeTimer)
            {
                if (m_uiChargeTimer <= uiDiff)
                {
                    if (Unit* pTarget = m_creature->SelectAttackingTarget(
                            ATTACKING_TARGET_RANDOM, 0))
                        DoCastSpellIfCan(pTarget, SPELL_CHARGE_H);

                    m_uiChargeTimer = 0;
                }
                else
                    m_uiChargeTimer -= uiDiff;
            }

            if (m_uiSweepingStrikesTimer)
            {
                if (m_uiSweepingStrikesTimer <= uiDiff)
                {
                    if (DoCastSpellIfCan(m_creature, SPELL_SWEEPING_STRIKES) ==
                        CAST_OK)
                        m_uiSweepingStrikesTimer = 0;
                }
                else
                    m_uiSweepingStrikesTimer -= uiDiff;
            }

            DoMeleeAttackIfReady();
        }
    }
};

CreatureAI* GetAI_boss_warchief_kargath_bladefist(Creature* pCreature)
{
    return new boss_warchief_kargath_bladefistAI(pCreature);
}

void AddSC_boss_warchief_kargath_bladefist()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "boss_warchief_kargath_bladefist";
    pNewScript->GetAI = &GetAI_boss_warchief_kargath_bladefist;
    pNewScript->RegisterSelf();
}
