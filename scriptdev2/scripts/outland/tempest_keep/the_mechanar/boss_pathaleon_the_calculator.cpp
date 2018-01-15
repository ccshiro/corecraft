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
SDName: Boss Pathaleon the Calculator
SD%Complete: 100
SDComment:
SDCategory: Tempest Keep, The Mechanar
EndScriptData */

#include "mechanar.h"
#include "precompiled.h"

#define SAY_AGGRO -1554020
#define SAY_DOMINATION_1 -1554021
#define SAY_DOMINATION_2 -1554022
#define SAY_SUMMON -1554023
#define SAY_ENRAGE -1554024
#define SAY_KILL_1 -1554025
#define SAY_KILL_2 -1554026
#define SAY_DEATH -1554027

// Spells to be casted
#define SPELL_MANA_TAP 36021
#define SPELL_ARCANE_TORRENT 36022
#define SPELL_DOMINATION 35280
#define SPELL_ARCANE_EXPLOSION_H 15453
#define SPELL_FRENZY 36992

#define SPELL_SUMMON_NETHER_WRAITH_1 35285
#define SPELL_SUMMON_NETHER_WRAITH_2 35286
#define SPELL_SUMMON_NETHER_WRAITH_3 35287 // Not used by blizzard
#define SPELL_SUMMON_NETHER_WRAITH_4 35288

// Add Spells
#define SPELL_DETONATION 35058
#define SPELL_ARCANE_BOLT 20720

// This Value is Guess Work at the moment:
#define MANA_PER_MANA_TAP_CHARGE 500

struct MANGOS_DLL_DECL boss_pathaleon_the_calculatorAI : public ScriptedAI
{
    boss_pathaleon_the_calculatorAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_pInstance = (instance_mechanar*)pCreature->GetInstanceData();
        m_bIsRegularMode = pCreature->GetMap()->IsRegularDifficulty();
        Reset();
    }

    instance_mechanar* m_pInstance;

    bool m_bIsRegularMode;

    uint32 Summon_Timer;
    uint32 ManaTap_Timer;
    uint32 ArcaneTorrent_Timer;
    uint32 Domination_Timer;
    uint32 ArcaneExplosion_Timer;
    bool Enraged;

    void Reset() override
    {
        Summon_Timer = 25000;
        ManaTap_Timer = urand(12000, 20000);
        ArcaneTorrent_Timer = urand(16000, 25000);
        Domination_Timer = 15000;
        ArcaneExplosion_Timer = urand(8000, 13000);

        Enraged = false;

        m_creature->SetAggroDistance(
            90.0f); // Pathaleon has really friggin high aggro distance

        if (m_pInstance)
        {
            m_pInstance->SetData(TYPE_PATHALEON_THE_CALCULATOR, NOT_STARTED);
        }

        m_creature->SetVirtualItem(VIRTUAL_ITEM_SLOT_0, 20835);
        m_creature->SetVirtualItem(VIRTUAL_ITEM_SLOT_1, 20911);
    }

    void Aggro(Unit* /*who*/) override
    {
        DoScriptText(SAY_AGGRO, m_creature);

        if (m_pInstance)
        {
            m_pInstance->SetData(TYPE_PATHALEON_THE_CALCULATOR, IN_PROGRESS);
        }
    }

    void KilledUnit(Unit* victim) override
    {
        DoKillSay(m_creature, victim, SAY_KILL_1, SAY_KILL_2);
    }

    void JustDied(Unit* /*Killer*/) override
    {
        DoScriptText(SAY_DEATH, m_creature);

        if (m_pInstance)
        {
            m_pInstance->SetData(TYPE_PATHALEON_THE_CALCULATOR, DONE);
        }
    }

    void JustSummoned(Creature* pSummon) override
    {
        if (m_creature->getVictim() && pSummon->AI())
            pSummon->AI()->AttackStart(m_creature->getVictim());
    }

    void UpdateAI(const uint32 diff) override
    {
        // Return since we have no target
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        if (!Enraged)
        {
            if (Summon_Timer <= diff)
            {
                if (DoCastSpellIfCan(
                        m_creature, SPELL_SUMMON_NETHER_WRAITH_1) == CAST_OK)
                {
                    DoCastSpellIfCan(m_creature, SPELL_SUMMON_NETHER_WRAITH_2,
                        CAST_TRIGGERED);
                    // DoCastSpellIfCan(m_creature,
                    // SPELL_SUMMON_NETHER_WRAITH_3, CAST_TRIGGERED); // Not
                    // used by blizzard
                    DoCastSpellIfCan(m_creature, SPELL_SUMMON_NETHER_WRAITH_4,
                        CAST_TRIGGERED);

                    DoScriptText(SAY_SUMMON, m_creature);

                    Summon_Timer = 25000;
                }
            }
            else
                Summon_Timer -= diff;
        }

        if (ManaTap_Timer <= diff)
        {
            if (Unit* pTarget = m_creature->SelectAttackingTarget(
                    ATTACKING_TARGET_RANDOM, 0, SELECT_FLAG_POWER_MANA))
            {
                if (DoCastSpellIfCan(pTarget, SPELL_MANA_TAP) == CAST_OK)
                    ManaTap_Timer = m_bIsRegularMode ? urand(14000, 22000) :
                                                       urand(14000, 18000);
            }
            else
                ManaTap_Timer = 2500; // Try again later
        }
        else
            ManaTap_Timer -= diff;

        if (ArcaneTorrent_Timer <= diff)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_ARCANE_TORRENT) == CAST_OK)
            {
                // Arcane Torrent only has an Apply Aura: Silence effect
                // (i.e., missing dumby for the rest of the spell), so we
                // implement the rest here
                if (m_creature->has_aura(SPELL_MANA_TAP))
                {
                    if (AuraHolder* holder =
                            m_creature->get_aura(SPELL_MANA_TAP))
                    {
                        uint32 mana_increase =
                            MANA_PER_MANA_TAP_CHARGE * holder->GetStackAmount();
                        // Round it down if it's too high
                        uint32 missing = m_creature->GetMaxPower(POWER_MANA) -
                                         m_creature->GetPower(POWER_MANA);
                        mana_increase =
                            (mana_increase > missing) ? missing : mana_increase;

                        m_creature->ModifyPower(POWER_MANA, mana_increase);
                        m_creature->remove_auras(SPELL_MANA_TAP);
                    }
                }

                ArcaneTorrent_Timer = urand(16000, 25000);
            }
        }
        else
            ArcaneTorrent_Timer -= diff;

        if (Domination_Timer <= diff)
        {
            if (Unit* target = m_creature->SelectAttackingTarget(
                    ATTACKING_TARGET_RANDOM, 1))
            {
                if (DoCastSpellIfCan(target, SPELL_DOMINATION) == CAST_OK)
                {
                    DoScriptText(
                        urand(0, 1) ? SAY_DOMINATION_1 : SAY_DOMINATION_2,
                        m_creature);
                    Domination_Timer = urand(20000, 30000);
                }
            }
        }
        else
            Domination_Timer -= diff;

        // Only casting if Heroic Mode is used
        if (!m_bIsRegularMode)
        {
            if (ArcaneExplosion_Timer <= diff)
            {
                if (DoCastSpellIfCan(m_creature, SPELL_ARCANE_EXPLOSION_H) ==
                    CAST_OK)
                    ArcaneExplosion_Timer = urand(10000, 14000);
            }
            else
                ArcaneExplosion_Timer -= diff;
        }

        if (!Enraged && m_creature->GetHealthPercent() <= 20.0f)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_FRENZY) == CAST_OK)
            {
                m_creature->SetVirtualItem(VIRTUAL_ITEM_SLOT_1, EQUIP_UNEQUIP);
                m_creature->SetVirtualItem(VIRTUAL_ITEM_SLOT_0, 18803);
                DoScriptText(SAY_ENRAGE, m_creature);
                Enraged = true;

                // Kill all adds
                auto l =
                    GetCreatureListWithEntryInGrid(m_creature, 21062, 200.0f);
                for (auto c : l)
                    c->Kill(c, false);
            }
        }

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_boss_pathaleon_the_calculator(Creature* pCreature)
{
    return new boss_pathaleon_the_calculatorAI(pCreature);
}

/* PRE BOSS EVENT */

enum
{
    NPC_BLOODWARDER_CENTURION = 19510,
    NPC_SUNSEEKER_ASTROMAGE = 19168,
    NPC_BLOODWARDER_PHYSICIAN = 20990,
    NPC_SUNSEEKER_ENGINEER = 20988,
    NPC_SUNSEEKER_NETHERBINDER = 20059,
    NPC_TEMPEST_FORGE_DESTROYER = 19735,

    NPC_PATHALEON_THE_CALCULATOR = 19220,

    NPC_DESPAWN_TIME = 2 * 60 * 1000,
    BOSS_DESPAWN_TIME = 10 * 60 * 1000,
    EVENT_TIMEOUT_TIME = 45 * 60 * 1000,

    DATA_PATHALEON_IS_ALIVE = 1,

    SPELL_SIMPLE_TELEPORT = 12980,
};

struct sPathaleonSpawnStruct
{
    float X, Y, Z, O;
};

const sPathaleonSpawnStruct PathaleonSpawnStruct_TriggerOne[] = {
    {222.1, -17.1, 24.874, 6.1946}, {219.5, -20.5, 24.874, 0.0136},
    {219.5, -25.3, 24.874, 6.2457}, {222.1, -28.9, 24.874, 6.2339},
};
const sPathaleonSpawnStruct PathaleonSpawnSingle_TriggerOne = {
    234.9, -23.5, 26.3284, 0.04239};

const sPathaleonSpawnStruct PathaleonSpawnStruct_TriggerTwo[] = {
    {132.1, 67.7, 24.874, 4.6796}, {135.0, 65.0, 24.874, 4.6796},
    {139.7, 65.0, 24.874, 4.6796}, {143.3, 67.7, 24.874, 4.6796},
};
const sPathaleonSpawnStruct PathaleonSpawnSingle_TriggerTwo = {
    137.9, 71.2, 24.874, 4.6796};
const sPathaleonSpawnStruct PathaleonSpawnBoss_TriggerTwo = {
    138.447, 150.281, 25.5756, 4.63332};

struct MANGOS_DLL_DECL boss_pathaleon_event_trigger_oneAI : public ScriptedAI
{
    boss_pathaleon_event_trigger_oneAI(Creature* pCreature)
      : ScriptedAI(pCreature)
    {
        m_bIsRegularMode = pCreature->GetMap()->IsRegularDifficulty();
        m_pInstance = (ScriptedInstance*)pCreature->GetInstanceData();

        m_timeSinceEventStarted = 0;
        m_uiCheckTimer = 1500;
        m_bIsEventStarted = false;
        m_uiWalkTimer = 0;

        Reset();
    }

    ScriptedInstance* m_pInstance;

    bool m_bIsRegularMode;
    bool m_bIsEventStarted;
    uint32 m_timeSinceEventStarted; // After 30 minutes the event resets if
                                    // Pathaleon is not dead
    uint32 m_uiCurrentWave;
    uint32 m_uiCheckTimer;
    uint32 m_uiWalkTimer;

    std::list<ObjectGuid> m_currentWave;

    void Reset() override {}

    void MoveInLineOfSight(Unit* pWho) override
    {
        if (pWho->GetTypeId() != TYPEID_PLAYER ||
            ((Player*)pWho)->isGameMaster())
            return;

        // Start event if not already started (and pathaleon is not dead)
        if (m_pInstance &&
            m_pInstance->GetData(TYPE_PATHALEON_THE_CALCULATOR) != DONE &&
            !m_bIsEventStarted && m_creature->IsWithinDistInMap(pWho, 20.0f))
        {
            if (m_creature->GetDistanceZ(pWho) > 10.0f) // Not below us right?
                return;

            // Start it
            m_timeSinceEventStarted = 0;
            m_uiCurrentWave = 1;
            m_bIsEventStarted = true;

            // Spawn the first wave
            SpawnNextWave();
        }
    }

    void EventTimedOut() // Took you too long
    {
        m_bIsEventStarted = false;

        CreatureGroup* pGroup = NULL;

        // Despawn current wave
        if (m_currentWave.size() > 0)
        {
            for (auto& elem : m_currentWave)
            {
                if (Creature* pCurrent =
                        m_creature->GetMap()->GetCreature(elem))
                {
                    pCurrent->ForcedDespawn();
                    if (pCurrent->GetGroup())
                        pGroup = pCurrent->GetGroup();
                }
            }
        }
        m_timeSinceEventStarted = 0;
        m_uiCurrentWave = 0;

        if (pGroup)
            m_creature->GetMap()->GetCreatureGroupMgr().DeleteGroup(
                pGroup->GetId());
    }

    void MakeWaveWalk()
    {
        int32 id = m_creature->GetMap()->GetCreatureGroupMgr().CreateNewGroup(
            "Pathaleon Summon Group", true);
        for (auto& elem : m_currentWave)
        {
            if (Creature* pCurrent = m_creature->GetMap()->GetCreature(elem))
            {
                pCurrent->CastSpell(pCurrent, SPELL_SIMPLE_TELEPORT, false);
                float x = pCurrent->GetX() + 50.0f;
                float y = pCurrent->GetY();
                float z = pCurrent->GetZ() + 2.0f;
                pCurrent->movement_gens.push(
                    new movement::PointMovementGenerator(
                        0, x, y, z, true, false),
                    movement::EVENT_ENTER_COMBAT);
                pCurrent->movement_gens.remove_all(movement::gen::idle);
                pCurrent->movement_gens.push(
                    new movement::IdleMovementGenerator(
                        x, y, z, pCurrent->GetO()));
                if (CreatureGroup* pGroup =
                        m_creature->GetMap()->GetCreatureGroupMgr().GetGroup(
                            id))
                    pGroup->AddMember(pCurrent, false);
            }
        }
    }

    void SpawnMob(uint32 entry, sPathaleonSpawnStruct p)
    {
        if (Creature* pCreature = m_creature->SummonCreature(entry, p.X, p.Y,
                p.Z, p.O, TEMPSUMMON_CORPSE_TIMED_DESPAWN, NPC_DESPAWN_TIME))
        {
            m_currentWave.push_back(pCreature->GetObjectGuid());
        }
    }

    void SpawnNextWave()
    {
        m_currentWave.clear();

        switch (m_uiCurrentWave)
        {
        case 1:
        {
            SpawnMob(
                NPC_SUNSEEKER_ASTROMAGE, PathaleonSpawnStruct_TriggerOne[0]);
            SpawnMob(
                NPC_BLOODWARDER_CENTURION, PathaleonSpawnStruct_TriggerOne[1]);
            SpawnMob(
                NPC_BLOODWARDER_PHYSICIAN, PathaleonSpawnStruct_TriggerOne[2]);
            SpawnMob(
                NPC_SUNSEEKER_ASTROMAGE, PathaleonSpawnStruct_TriggerOne[3]);
            m_uiWalkTimer = 400;
        }
        break;
        case 2:
        {
            SpawnMob(
                NPC_TEMPEST_FORGE_DESTROYER, PathaleonSpawnSingle_TriggerOne);
            m_uiWalkTimer = 400;
        }
        break;
        case 3:
        {
            SpawnMob(
                NPC_SUNSEEKER_ENGINEER, PathaleonSpawnStruct_TriggerOne[0]);
            SpawnMob(
                NPC_BLOODWARDER_PHYSICIAN, PathaleonSpawnStruct_TriggerOne[1]);
            SpawnMob(
                NPC_SUNSEEKER_ENGINEER, PathaleonSpawnStruct_TriggerOne[2]);
            m_uiWalkTimer = 400;
        }
        break;
        }
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_bIsEventStarted)
            return;

        if (m_timeSinceEventStarted > EVENT_TIMEOUT_TIME)
        {
            EventTimedOut();
            return;
        }
        else
            m_timeSinceEventStarted += uiDiff;

        if (m_uiWalkTimer)
        {
            if (m_uiWalkTimer <= uiDiff)
            {
                m_uiWalkTimer = 0;
                MakeWaveWalk();
            }
            else
                m_uiWalkTimer -= uiDiff;
        }

        if (m_uiCheckTimer <= uiDiff)
            m_uiCheckTimer = 1500;
        else
        {
            m_uiCheckTimer -= uiDiff;
            return;
        }

        // Check if wave is dead:
        for (auto& elem : m_currentWave)
        {
            Creature* pCurrent = m_creature->GetMap()->GetCreature(elem);
            if (pCurrent && pCurrent->isAlive())
                return; // Wave is not dead yet
        }

        // Wave is dead:
        m_uiCurrentWave += 1;
        SpawnNextWave();
    }
};
CreatureAI* GetAI_boss_pathaleon_event_trigger_one(Creature* pCreature)
{
    return new boss_pathaleon_event_trigger_oneAI(pCreature);
}

struct MANGOS_DLL_DECL boss_pathaleon_event_trigger_twoAI : public ScriptedAI
{
    boss_pathaleon_event_trigger_twoAI(Creature* pCreature)
      : ScriptedAI(pCreature)
    {
        m_bIsRegularMode = pCreature->GetMap()->IsRegularDifficulty();
        m_pInstance = (ScriptedInstance*)pCreature->GetInstanceData();

        m_timeSinceEventStarted = 0;
        m_uiCheckTimer = 1500;
        m_bIsEventStarted = false;
        m_uiWalkTimer = 0;

        Reset();
    }

    ScriptedInstance* m_pInstance;

    bool m_bIsRegularMode;
    bool m_bIsEventStarted;
    uint32 m_timeSinceEventStarted; // After 30 minutes the event resets if
                                    // Pathaleon is not dead
    uint32 m_uiCurrentWave;
    uint32 m_uiCheckTimer;
    uint32 m_uiWalkTimer;

    std::list<ObjectGuid> m_currentWave;

    void Reset() override {}

    void MoveInLineOfSight(Unit* pWho) override
    {
        if (pWho->GetTypeId() != TYPEID_PLAYER ||
            ((Player*)pWho)->isGameMaster())
            return;

        // Start event if not already started (and pathaleon is not dead)
        if (m_pInstance &&
            m_pInstance->GetData(TYPE_PATHALEON_THE_CALCULATOR) != DONE &&
            !m_bIsEventStarted && m_creature->IsWithinDistInMap(pWho, 15.0f))
        {
            if (m_creature->GetDistanceZ(pWho) > 10.0f) // Not below us right?
                return;

            // Start it
            m_timeSinceEventStarted = 0;
            m_uiCurrentWave = 1;
            m_bIsEventStarted = true;

            // Spawn the first wave
            SpawnNextWave();
        }
    }

    void EventTimedOut() // Took you too long
    {
        m_bIsEventStarted = false;

        CreatureGroup* pGroup = NULL;

        // Despawn current wave
        if (m_currentWave.size() > 0)
        {
            for (auto& elem : m_currentWave)
            {
                if (Creature* pCurrent =
                        m_creature->GetMap()->GetCreature(elem))
                {
                    pCurrent->ForcedDespawn();
                    if (pCurrent->GetGroup())
                        pGroup = pCurrent->GetGroup();
                }
            }
        }
        m_timeSinceEventStarted = 0;
        m_uiCurrentWave = 0;

        if (pGroup)
            m_creature->GetMap()->GetCreatureGroupMgr().DeleteGroup(
                pGroup->GetId());
    }

    void MakeWaveWalk()
    {
        int32 id = m_creature->GetMap()->GetCreatureGroupMgr().CreateNewGroup(
            "Pathaleon Summon Group", true);
        for (auto& elem : m_currentWave)
        {
            if (Creature* pCurrent = m_creature->GetMap()->GetCreature(elem))
            {
                pCurrent->CastSpell(pCurrent, SPELL_SIMPLE_TELEPORT, false);

                float x = pCurrent->GetX();
                float y = pCurrent->GetY() - 40.0f;
                float z = pCurrent->GetZ();
                pCurrent->movement_gens.push(
                    new movement::PointMovementGenerator(
                        0, x, y, z, true, false),
                    movement::EVENT_ENTER_COMBAT);
                pCurrent->movement_gens.remove_all(movement::gen::idle);
                pCurrent->movement_gens.push(
                    new movement::IdleMovementGenerator(
                        x, y, z, pCurrent->GetO()));
                if (CreatureGroup* pGroup =
                        m_creature->GetMap()->GetCreatureGroupMgr().GetGroup(
                            id))
                    pGroup->AddMember(pCurrent, false);
            }
        }
    }

    void SpawnMob(uint32 entry, sPathaleonSpawnStruct p)
    {
        if (Creature* pCreature = m_creature->SummonCreature(entry, p.X, p.Y,
                p.Z, p.O, TEMPSUMMON_CORPSE_TIMED_DESPAWN, NPC_DESPAWN_TIME))
        {
            m_currentWave.push_back(pCreature->GetObjectGuid());
        }
    }

    void SpawnNextWave()
    {
        m_currentWave.clear();

        switch (m_uiCurrentWave)
        {
        case 1:
        {
            SpawnMob(
                NPC_SUNSEEKER_ASTROMAGE, PathaleonSpawnStruct_TriggerTwo[0]);
            SpawnMob(
                NPC_SUNSEEKER_ENGINEER, PathaleonSpawnStruct_TriggerTwo[1]);
            SpawnMob(
                NPC_BLOODWARDER_PHYSICIAN, PathaleonSpawnStruct_TriggerTwo[2]);
            m_uiWalkTimer = 400;
        }
        break;
        case 2:
        {
            SpawnMob(
                NPC_TEMPEST_FORGE_DESTROYER, PathaleonSpawnSingle_TriggerTwo);
            m_uiWalkTimer = 400;
        }
        break;
        case 3:
        {
            SpawnMob(
                NPC_SUNSEEKER_ENGINEER, PathaleonSpawnStruct_TriggerTwo[0]);
            SpawnMob(
                NPC_BLOODWARDER_PHYSICIAN, PathaleonSpawnStruct_TriggerTwo[1]);
            SpawnMob(
                NPC_SUNSEEKER_ENGINEER, PathaleonSpawnStruct_TriggerTwo[2]);
            m_uiWalkTimer = 400;
        }
        break;

        case 4:
        {
            sPathaleonSpawnStruct p = PathaleonSpawnBoss_TriggerTwo;
            Creature* pCreature = m_creature->SummonCreature(
                NPC_PATHALEON_THE_CALCULATOR, p.X, p.Y, p.Z, p.O,
                TEMPSUMMON_CORPSE_TIMED_DESPAWN, BOSS_DESPAWN_TIME);
            m_currentWave.push_back(pCreature->GetObjectGuid());
            pCreature->CastSpell(pCreature, SPELL_SIMPLE_TELEPORT, false);
            pCreature->HandleEmote(333); // Attack ready emote
        }
        break;

        case 5: /* Pathaleon killed */
        {
            m_bIsEventStarted = false;
        }
        break;
        }
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_bIsEventStarted)
            return;

        if (m_timeSinceEventStarted > EVENT_TIMEOUT_TIME)
        {
            EventTimedOut();
            return;
        }
        else
            m_timeSinceEventStarted += uiDiff;

        if (m_uiWalkTimer)
        {
            if (m_uiWalkTimer <= uiDiff)
            {
                m_uiWalkTimer = 0;
                MakeWaveWalk();
            }
            else
                m_uiWalkTimer -= uiDiff;
        }

        if (m_uiCheckTimer <= uiDiff)
            m_uiCheckTimer = 1500;
        else
        {
            m_uiCheckTimer -= uiDiff;
            return;
        }

        for (auto& elem : m_currentWave)
        {
            Creature* pCurrent = m_creature->GetMap()->GetCreature(elem);
            if (pCurrent && pCurrent->isAlive())
                return; // Wave is not dead yet
        }

        // Wave is dead:
        m_uiCurrentWave += 1;
        SpawnNextWave();
    }
};
CreatureAI* GetAI_boss_pathaleon_event_trigger_two(Creature* pCreature)
{
    return new boss_pathaleon_event_trigger_twoAI(pCreature);
}

void AddSC_boss_pathaleon_the_calculator()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "boss_pathaleon_the_calculator";
    pNewScript->GetAI = &GetAI_boss_pathaleon_the_calculator;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "boss_pathaleon_event_trigger_one";
    pNewScript->GetAI = &GetAI_boss_pathaleon_event_trigger_one;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "boss_pathaleon_event_trigger_two";
    pNewScript->GetAI = &GetAI_boss_pathaleon_event_trigger_two;
    pNewScript->RegisterSelf();
}
