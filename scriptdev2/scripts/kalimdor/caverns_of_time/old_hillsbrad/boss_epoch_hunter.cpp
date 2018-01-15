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
SDName: Boss_Epoch_Hunter
SD%Complete: 100
SDComment:
SDCategory: Caverns of Time, Old Hillsbrad Foothills
EndScriptData */

#include "old_hillsbrad.h"
#include "precompiled.h"

enum
{
    SAY_AGGRO1 = -1560016,
    SAY_AGGRO2 = -1560017,
    SAY_KILL_1 = -1560018,
    SAY_KILL_2 = -1560019,
    SAY_BREATH1 = -1560020,
    SAY_BREATH2 = -1560021,
    SAY_DEATH = -1560022,

    SPELL_SAND_BREATH = 31914,
    SPELL_IMPENDING_DEATH = 31916,
    SPELL_MAGIC_DISRUPTION_AURA = 33834,
    SPELL_WING_BUFFET = 31475,

    // Tarren Mill mobbs
    NPC_GUARD = 23175,
    NPC_LOOK = 23177,
    NPC_PROT = 23179,

    // Infinite mobs
    NPC_SLAYER = 18170,
    NPC_SABOTEUR = 18172,
    NPC_DEFILER = 18171,
};

struct MANGOS_DLL_DECL boss_epoch_hunterAI : public ScriptedAI
{
    boss_epoch_hunterAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_pInstance = (instance_old_hillsbrad*)pCreature->GetInstanceData();
        Reset();
    }

    instance_old_hillsbrad* m_pInstance;

    void MovementInform(movement::gen gen_type, uint32 uiData) override
    {
        if (gen_type == movement::gen::point)
        {
            if (uiData == 10000) // A weak attempt at inter-AI communication
                StartPreEvent();
            else if (uiData == 100)
            {
                m_creature->HandleEmoteCommand(EMOTE_ONESHOT_LAND);
                m_creature->RemoveInhabitType(INHABIT_AIR);
                m_creature->movement_gens.push(
                    new movement::FallMovementGenerator());
                m_preEventPhase = 6;
                m_uiPreEventTimer = 3000;
            }
        }
    }

    void StartPreEvent()
    {
        m_uiPreEventTimer = 1000;
        m_preEventPhase = 1;
    }

    void JustSummoned(Creature* pSummon) override
    {
        m_spawnedMobs.push_back(pSummon->GetObjectGuid());
    }

    void DoPreEvent(uint32 uiDiff)
    {
        if (m_uiPreEventTimer)
        {
            if (m_preEventPhase >= 2 && m_preEventPhase < 6 &&
                !m_spawnedMobs.empty())
            {
                bool allDead = true;
                for (auto& elem : m_spawnedMobs)
                    if (Creature* mob = m_creature->GetMap()->GetCreature(elem))
                        if (mob->isAlive())
                        {
                            allDead = false;
                            break;
                        }
                if (allDead)
                    m_uiPreEventTimer = 1;
            }

            if (m_uiPreEventTimer <= uiDiff)
            {
                switch (m_preEventPhase)
                {
                case 1:
                    if (Creature* s = m_creature->SummonCreature(NPC_GUARD,
                            2623.9f, 695.9f, 57.3f, 0.1f,
                            TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 25 * 1000))
                    {
                        s->SetFlag(UNIT_FIELD_FLAGS,
                            UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_PASSIVE);
                        std::vector<DynamicWaypoint> wps;
                        wps.push_back(DynamicWaypoint(
                            2632.2f, 696.0f, 56.1f, 4.6f, 20000));
                        s->movement_gens.remove_all(movement::gen::idle);
                        s->movement_gens.push(
                            new movement::DynamicWaypointMovementGenerator(
                                wps, false));
                    }
                    if (Creature* s = m_creature->SummonCreature(NPC_PROT,
                            2654.9f, 698.9f, 57.5f, 3.3f,
                            TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 25 * 1000))
                    {
                        s->SetFlag(UNIT_FIELD_FLAGS,
                            UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_PASSIVE);
                        std::vector<DynamicWaypoint> wps;
                        wps.push_back(DynamicWaypoint(
                            2643.4f, 697.7f, 55.8f, 4.5f, 20000));
                        s->movement_gens.remove_all(movement::gen::idle);
                        s->movement_gens.push(
                            new movement::DynamicWaypointMovementGenerator(
                                wps, false));
                    }
                    if (Creature* s = m_creature->SummonCreature(NPC_LOOK,
                            2635.0f, 718.3f, 56.4f, 4.8f,
                            TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 25 * 1000))
                    {
                        s->SetFlag(UNIT_FIELD_FLAGS,
                            UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_PASSIVE);
                        std::vector<DynamicWaypoint> wps;
                        wps.push_back(
                            DynamicWaypoint(2640.0f, 707.0f, 56.0f, 4.8f));
                        wps.push_back(DynamicWaypoint(
                            2637.4f, 695.5f, 55.8f, 4.6f, 20000));
                        s->movement_gens.remove_all(movement::gen::idle);
                        s->movement_gens.push(
                            new movement::DynamicWaypointMovementGenerator(
                                wps, false));
                    }
                    m_uiPreEventTimer = 8000;
                    break;
                case 2:
                    if (m_spawnedMobs.size() > 2)
                    {
                        if (Creature* s = m_creature->GetMap()->GetCreature(
                                m_spawnedMobs[0]))
                        {
                            s->RemoveFlag(UNIT_FIELD_FLAGS,
                                UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_PASSIVE);
                            s->UpdateEntry(NPC_SLAYER);
                            if (Creature* thrall = m_pInstance->GetThrall())
                                s->AI()->AttackStart(thrall);
                        }
                        if (Creature* s = m_creature->GetMap()->GetCreature(
                                m_spawnedMobs[1]))
                        {
                            s->RemoveFlag(UNIT_FIELD_FLAGS,
                                UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_PASSIVE);
                            s->UpdateEntry(NPC_DEFILER);
                            if (Creature* thrall = m_pInstance->GetThrall())
                                s->AI()->AttackStart(thrall);
                        }
                        if (Creature* s = m_creature->GetMap()->GetCreature(
                                m_spawnedMobs[2]))
                        {
                            s->RemoveFlag(UNIT_FIELD_FLAGS,
                                UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_PASSIVE);
                            s->UpdateEntry(NPC_SABOTEUR);
                            if (Creature* thrall = m_pInstance->GetThrall())
                                s->AI()->AttackStart(thrall);
                        }
                    }
                    m_uiPreEventTimer = 90000;
                    break;
                case 3:
                    m_spawnedMobs.clear();
                    m_creature->SummonCreature(NPC_SABOTEUR, 2596.5f, 676.7f,
                        56.4f, 6.0f, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT,
                        25 * 1000);
                    m_creature->SummonCreature(NPC_DEFILER, 2594.2f, 680.4f,
                        55.9f, 6.0f, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT,
                        25 * 1000);
                    m_creature->SummonCreature(NPC_SLAYER, 2598.7f, 686.0f,
                        55.9f, 6.0f, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT,
                        25 * 1000);
                    m_uiAttackTimer = 4000;
                    m_uiPreEventTimer = 90000;
                    break;
                case 4:
                    m_spawnedMobs.clear();
                    m_creature->SummonCreature(NPC_SLAYER, 2644.4f, 707.8f,
                        56.0f, 4.4f, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT,
                        25 * 1000);
                    m_creature->SummonCreature(NPC_SLAYER, 2638.0f, 709.8f,
                        56.0f, 4.4f, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT,
                        25 * 1000);
                    m_creature->SummonCreature(NPC_DEFILER, 2645.6f, 716.5f,
                        57.4f, 4.4f, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT,
                        25 * 1000);
                    m_creature->SummonCreature(NPC_SABOTEUR, 2637.5f, 714.9f,
                        56.3f, 4.4f, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT,
                        25 * 1000);
                    m_uiAttackTimer = 4000;
                    m_uiPreEventTimer = 120000;
                    break;
                case 5:
                    // Descend
                    m_creature->movement_gens.push(
                        new movement::PointMovementGenerator(
                            100, 2640.0f, 690.6f, 67.1f, false, true));
                    m_creature->movement_gens.remove_all(movement::gen::idle);
                    m_uiPreEventTimer = 0;
                    m_preEventPhase = 0;
                    break;
                case 6:
                    // We've descended; time to start combat
                    m_creature->RemoveFlag(UNIT_FIELD_FLAGS,
                        UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_PASSIVE);
                    if (Creature* thrall = m_pInstance->GetThrall())
                        m_creature->AI()->AttackStart(thrall);
                    m_uiPreEventTimer = 0;
                    m_preEventPhase = 0;
                    break;
                }
                ++m_preEventPhase;
            }
            else
                m_uiPreEventTimer -= uiDiff;
        }
    }

    std::vector<ObjectGuid> m_spawnedMobs;
    uint32 m_uiAttackTimer;
    uint32 m_uiPreEventTimer;
    uint32 m_preEventPhase;
    uint32 m_uiSandBreathTimer;
    uint32 m_uiImpendingDeathTimer;
    uint32 m_uiWingBuffetTimer;
    uint32 m_uiMagicAuraTimer;

    void Reset() override
    {
        m_uiAttackTimer = 0;
        m_uiPreEventTimer = 0;
        m_preEventPhase = 0;
        m_uiSandBreathTimer = urand(20000, 25000);
        m_uiImpendingDeathTimer = 10000;
        m_uiWingBuffetTimer = urand(15000, 20000);
        m_uiMagicAuraTimer = 0;
        m_spawnedMobs.clear(); // Mobs are despawned by themselves
    }

    void Aggro(Unit* /*who*/) override
    {
        DoScriptText(urand(0, 1) ? SAY_AGGRO1 : SAY_AGGRO2, m_creature);
    }

    void KilledUnit(Unit* victim) override
    {
        DoKillSay(m_creature, victim, SAY_KILL_1, SAY_KILL_2);
    }

    void JustDied(Unit* /*victim*/) override
    {
        DoScriptText(SAY_DEATH, m_creature);

        if (m_pInstance &&
            m_pInstance->GetData(TYPE_THRALL_EVENT) == IN_PROGRESS)
            m_pInstance->SetData(TYPE_THRALL_PART4, DONE);
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
        {
            if (m_uiAttackTimer)
            {
                if (m_uiAttackTimer <= uiDiff)
                {
                    if (Creature* thrall = m_pInstance->GetThrall())
                        for (auto& elem : m_spawnedMobs)
                            if (Creature* summon =
                                    m_creature->GetMap()->GetCreature(elem))
                                if (summon->AI())
                                    summon->AI()->AttackStart(thrall);
                    m_uiAttackTimer = 0;
                }
                else
                    m_uiAttackTimer -= uiDiff;
            }

            DoPreEvent(uiDiff);
            return;
        }

        if (m_uiMagicAuraTimer <= uiDiff)
        {
            if (!m_creature->has_aura(SPELL_MAGIC_DISRUPTION_AURA))
                DoCastSpellIfCan(m_creature, SPELL_MAGIC_DISRUPTION_AURA);
            m_uiMagicAuraTimer = 10000;
        }
        else
            m_uiMagicAuraTimer -= uiDiff;

        // Sand Breath
        if (m_uiSandBreathTimer <= uiDiff)
        {
            if (DoCastSpellIfCan(m_creature->getVictim(), SPELL_SAND_BREATH) ==
                CAST_OK)
            {
                DoScriptText(
                    urand(0, 1) ? SAY_BREATH1 : SAY_BREATH2, m_creature);
                m_uiSandBreathTimer = urand(20000, 25000);
            }
        }
        else
            m_uiSandBreathTimer -= uiDiff;

        // Impending Death
        if (m_uiImpendingDeathTimer <= uiDiff)
        {
            if (Unit* target = m_creature->SelectAttackingTarget(
                    ATTACKING_TARGET_RANDOM, 0))
                if (DoCastSpellIfCan(target, SPELL_IMPENDING_DEATH) == CAST_OK)
                    m_uiImpendingDeathTimer = urand(15000, 20000);
        }
        else
            m_uiImpendingDeathTimer -= uiDiff;

        // Wing Buffet
        if (m_uiWingBuffetTimer <= uiDiff)
        {
            if (DoCastSpellIfCan(m_creature->getVictim(), SPELL_WING_BUFFET) ==
                CAST_OK)
                m_uiWingBuffetTimer = 30000;
        }
        else
            m_uiWingBuffetTimer -= uiDiff;

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_boss_epoch_hunter(Creature* pCreature)
{
    return new boss_epoch_hunterAI(pCreature);
}

void AddSC_boss_epoch_hunter()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "boss_epoch_hunter";
    pNewScript->GetAI = &GetAI_boss_epoch_hunter;
    pNewScript->RegisterSelf();
}
