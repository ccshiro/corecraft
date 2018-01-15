/* Copyright (C) 2013 Corecraft <https://www.worldofcorecraft.com/>
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
SDName: boss_kaelthas
SD%Complete: 100
SDComment:
SDCategory: Tempest Keep, The Eye
EndScriptData */

#include "precompiled.h"
#include "the_eye.h"

enum
{
    // -1550016 -> -1550034 -> -1550043
    SAY_INTRO = -1550016,
    SAY_PHASE_TWO = -1550024,
    SAY_KILL_1 = -1550025,
    SAY_KILL_2 = -1550026,
    SAY_KILL_3 = -1550027,
    SAY_MIND_CONTROL_1 = -1550028,
    SAY_MIND_CONTROL_2 = -1550029,
    SAY_GRAVITY_LAPSE_1 = -1550030,
    SAY_GRAVITY_LAPSE_2 = -1550031,
    SAY_PHOENIX_1 = -1550032,
    SAY_PHOENIX_2 = -1550033,
    SAY_DEATH = -1550034,

    NPC_NETHER_VAPOR = 21002,

    SPELL_FIREBALL = 36805,
    SPELL_MIND_CONTROL = 36797,
    SPELL_ARCANE_DISRUPTION = 36834,
    SPELL_PHOENIX = 36723,
    SPELL_SHOCK_BARRIER = 36815,
    SPELL_PYROBLAST = 36819,
    SPELL_FLAME_STRIKE = 36735,

    SPELL_KAEL_GAIN_POWER = 36091,
    NPC_NETHER_BEAM = 100037,
    SPELL_NETHERBEAM_1 = 36089,
    SPELL_NETHERBEAM_2 = 36090,
    SPELL_NETHERBEAM_ENHA_1 = 36364,
    SPELL_NETHERBEAM_ENHA_2 = 36370,
    SPELL_NETHERBEAM_ENHA_3 = 36371,
    SPELL_KAEL_EXPLOSION = 36092,
    SPELL_LIFE_LESS = 36545,
    SPELL_FULL_POWER = 36187,

    SPELL_GRAVITY_LAPSE = 35941, // Implemented in this script, not core
    SPELL_GRAVITY_LAPSE_FLY = 39432,
    SPELL_GRAVITY_LAPSE_KNOCK = 34480,
    SPELL_NETHER_VAPOR = 35865,
    SPELL_NETHER_BEAM = 35869,
    SPELL_SELF_ROOT = 42716,
};

struct boss_kaelthasAI : public ScriptedAI
{
    boss_kaelthasAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_instance = (ScriptedInstance*)pCreature->GetInstanceData();
        Reset();

        m_creature->movement_gens.push(new movement::StoppedMovementGenerator(),
            movement::EVENT_LEAVE_COMBAT);
    }

    std::vector<ObjectGuid> m_spawns;

    ScriptedInstance* m_instance;
    time_t m_lastTargetSwitch;
    uint32 m_rpTimer;
    uint32 m_rpStage;
    // Weapon Phase
    uint32 m_releaseWeapons;
    // Kael Phase One
    uint32 m_fireball;
    uint32 m_mc;
    uint32 m_disrupt;
    uint32 m_phoenix;
    uint32 m_barrier;
    uint32 m_flameStrike;
    // Kael Phase Transition
    uint32 m_transTimer;
    uint32 m_transStage;
    uint32 m_pureNether;
    // Kael Phase Two
    uint32 m_gravityLapse;
    bool m_vapor;
    bool m_endedGravity;
    uint32 m_gravityEnd;
    uint32 m_shieldRemain;

    void Reset() override
    {
        m_creature->SetTargetGuid(ObjectGuid());
        m_creature->SetObjectScale(1.0f);
        m_rpTimer = 0;
        m_rpStage = 0;
        m_releaseWeapons = 0;
        // Kael Phase One
        m_fireball = 0;
        m_mc = 15000;
        m_disrupt = urand(15000, 35000);
        m_phoenix = urand(50000, 60000);
        m_barrier = urand(55000, 65000);
        m_flameStrike = urand(15000, 35000);
        // Kael Phase Transition
        m_transTimer = 0;
        m_transStage = 0;
        m_pureNether = 0;
        // Kael Phase Two
        m_gravityLapse = 10000;
        m_vapor = false;
        m_endedGravity = true;
        m_gravityEnd = 0;
        m_shieldRemain = 0;

        Pacify(true);
    }

    void DespawnSummons()
    {
        for (auto& elem : m_spawns)
            if (Creature* c = m_creature->GetMap()->GetCreature(elem))
                c->ForcedDespawn();
        m_spawns.clear();
    }

    void JustSummoned(Creature* pCreature) override
    {
        uint32 e = pCreature->GetEntry();
        if (e == NPC_PHOENIX)
            return; // Handled in instance script

        m_spawns.push_back(pCreature->GetObjectGuid());

        if (e != NPC_NETHER_BEAM && e != NPC_NETHER_VAPOR)
        {
            m_releaseWeapons = 4000;
            pCreature->SetFlag(
                UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_PASSIVE);
        }
    }

    void MoveInLineOfSight(Unit* pWho) override
    {
        if (m_instance && m_instance->GetData(TYPE_KAELTHAS) == IN_PROGRESS &&
            m_instance->GetData(DATA_KAELTHAS_PHASE) < KAEL_PHASE_KAELTHAS_P1)
        {
            // Look around while waiting to engage
            if (m_creature->IsWithinDistInMap(pWho, 30.0f) &&
                pWho->isTargetableForAttack() &&
                m_creature->IsHostileTo(pWho) &&
                m_lastTargetSwitch + 2 < WorldTimer::time_no_syscall())
            {
                m_creature->SetTargetGuid(pWho->GetObjectGuid());
                m_lastTargetSwitch =
                    WorldTimer::time_no_syscall(); // Prevent target switching
                                                   // too often
            }
        }
        else
            ScriptedAI::MoveInLineOfSight(pWho);
    }

    void Aggro(Unit* /*pWho*/) override
    {
        if (m_instance)
        {
            m_instance->SetData(TYPE_KAELTHAS, IN_PROGRESS);
            m_creature->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
        }
    }

    void JustReachedHome() override
    {
        if (m_instance)
            m_instance->SetData(TYPE_KAELTHAS, FAIL);

        DespawnSummons();

        if (!m_creature->movement_gens.has(movement::gen::stopped))
            m_creature->movement_gens.push(
                new movement::StoppedMovementGenerator(),
                movement::EVENT_LEAVE_COMBAT);
    }

    void JustDied(Unit* /*pKiller*/) override
    {
        if (m_instance)
            m_instance->SetData(TYPE_KAELTHAS, DONE);

        DoScriptText(SAY_DEATH, m_creature);
    }

    void KilledUnit(Unit* victim) override
    {
        DoKillSay(m_creature, victim, SAY_KILL_1, SAY_KILL_2, SAY_KILL_3);
    }

    void MovementInform(movement::gen uiMoveType, uint32 uiPointId) override
    {
        if (uiMoveType == movement::gen::point)
        {
            if (uiPointId == 100)
            {
                m_creature->SetFacingTo(3.1f);
                DoScriptText(SAY_PHASE_TWO, m_creature);
                m_transTimer = 4000;
                m_transStage = 1;
                m_creature->SetFlag(UNIT_FIELD_FLAGS,
                    UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE);
            }
            else if (uiPointId == 200)
            {
                m_transTimer = 5000;
            }
            else if (uiPointId == 300)
            {
                m_creature->remove_auras(SPELL_FULL_POWER);
                m_transTimer = 2000;
            }
        }
    }

    void GravityLapseCallback()
    {
        m_creature->CastSpell(m_creature, SPELL_SELF_ROOT, true);
        m_vapor = true;

        uint32 spellId = 35966;
        for (ThreatList::const_iterator itr =
                 m_creature->getThreatManager().getThreatList().begin();
             itr != m_creature->getThreatManager().getThreatList().end() &&
                 spellId <= 35990;
             ++itr)
        {
            if ((*itr)->getUnitGuid().IsPlayer())
            {
                Player* pl =
                    m_creature->GetMap()->GetPlayer((*itr)->getUnitGuid());
                if (pl && pl->isAlive())
                {
                    m_creature->CastSpell(pl, spellId++, true);
                    pl->CastSpell(pl, SPELL_GRAVITY_LAPSE_KNOCK, true);
                    pl->CastSpell(pl, SPELL_GRAVITY_LAPSE_FLY, true);
                }
            }
        }

        m_gravityEnd = 30000;
        m_endedGravity = false;
    }

    void UpdateAI(const uint32 diff) override
    {
        if (!m_instance)
            return;

        const uint32 phase = m_instance->GetData(DATA_KAELTHAS_PHASE);

        if (phase == KAEL_PHASE_INTRO_RP)
        {
            if (m_rpTimer <= diff)
            {
                switch (m_rpStage)
                {
                case 0:
                    DoScriptText(SAY_INTRO, m_creature);
                    m_rpTimer = 25000;
                    break;
                case 1:
                    DoScriptText(SAY_KAEL_THALADRED_RELEASE, m_creature);
                    m_rpTimer = 7000;
                    break;
                case 2:
                    m_instance->SetData(
                        DATA_KAELTHAS_PHASE, KAEL_PHASE_ADVISOR);
                    return; // We don't want to increase stage. This above might
                            // cause a reset, and the next try would then skip
                            // the intro say.
                }
                ++m_rpStage;
            }
            else
                m_rpTimer -= diff;
        }
        else if (phase == KAEL_PHASE_WEAPONS)
        {
            if (m_releaseWeapons)
            {
                if (m_releaseWeapons <= diff)
                {
                    for (auto& elem : m_spawns)
                        if (Creature* c =
                                m_creature->GetMap()->GetCreature(elem))
                        {
                            if (c->isAlive() &&
                                c->HasFlag(
                                    UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE))
                            {
                                c->RemoveFlag(
                                    UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE |
                                                          UNIT_FLAG_PASSIVE);
                                c->SetInCombatWithZone();
                            }
                        }
                    m_releaseWeapons = 0;
                }
                else
                    m_releaseWeapons -= diff;
            }
        }

        if (phase == KAEL_PHASE_KAELTHAS_P1_TO_P2)
        {
            if (m_pureNether)
            {
                if (m_pureNether <= diff)
                {
                    switch (urand(0, 2))
                    {
                    case 0:
                        if (Creature* c = m_creature->SummonCreature(
                                NPC_NETHER_BEAM, 817.0f, 13.0f, 90.0f, 0.0f,
                                TEMPSUMMON_TIMED_DESPAWN, 500))
                        {
                            c->queue_action(0, [c]()
                                {
                                    c->CastSpell(c, 36196, false);
                                });
                        }
                        break;
                    case 1:
                        if (Creature* c = m_creature->SummonCreature(
                                NPC_NETHER_BEAM, 817.0f, 3.0f, 90.0f, 0.0f,
                                TEMPSUMMON_TIMED_DESPAWN, 500))
                        {
                            c->queue_action(0, [c]()
                                {
                                    c->CastSpell(c, 36197, false);
                                });
                        }
                        break;
                    case 2:
                        if (Creature* c = m_creature->SummonCreature(
                                NPC_NETHER_BEAM, 817.0f, -8.0f, 90.0f, 0.0f,
                                TEMPSUMMON_TIMED_DESPAWN, 500))
                        {
                            c->queue_action(0, [c]()
                                {
                                    c->CastSpell(c, 36198, false);
                                });
                        }
                        break;
                    }
                    m_pureNether = urand(0, 500);
                }
                else
                    m_pureNether -= diff;
            }

            if (m_transTimer)
            {
                if (m_transTimer <= diff)
                {
                    switch (m_transStage)
                    {
                    case 1:
                        DoCastSpellIfCan(m_creature, SPELL_KAEL_GAIN_POWER);
                        m_transTimer = 1000;
                        break;
                    case 2:
                        m_creature->AddInhabitType(INHABIT_AIR);
                        m_creature->movement_gens.push(
                            new movement::PointMovementGenerator(200,
                                m_creature->GetX(), m_creature->GetY(),
                                m_creature->GetZ() + 20.0f, false, false),
                            movement::EVENT_LEAVE_COMBAT);
                        m_transTimer = 2000;
                        break;
                    case 3:
                        if (Creature* c = m_creature->SummonCreature(
                                NPC_NETHER_BEAM, 800.0f, 34.0f, 82.0f, 0.0f,
                                TEMPSUMMON_TIMED_DESPAWN, 40000))
                        {
                            c->queue_action(0, [c]()
                                {
                                    c->CastSpell(c, SPELL_NETHERBEAM_1, false);
                                });
                        }
                        if (Creature* c = m_creature->SummonCreature(
                                NPC_NETHER_BEAM, 799.0f, -30.0f, 82.0f, 0.0f,
                                TEMPSUMMON_TIMED_DESPAWN, 40000))
                        {
                            c->queue_action(0, [c]()
                                {
                                    c->CastSpell(c, SPELL_NETHERBEAM_1, true);
                                });
                        }
                        m_creature->SetObjectScale(1.25f);
                        m_transTimer = 2000;
                        break;
                    case 4:
                        if (Creature* c = m_creature->SummonCreature(
                                NPC_NETHER_BEAM, 847.5f, -16.5f, 64.0f, 0.0f,
                                TEMPSUMMON_TIMED_DESPAWN, 40000))
                        {
                            c->queue_action(0, [c]()
                                {
                                    c->CastSpell(c, SPELL_NETHERBEAM_2, true);
                                });
                        }
                        if (Creature* c = m_creature->SummonCreature(
                                NPC_NETHER_BEAM, 847.5f, 15.0f, 64.0f, 0.0f,
                                TEMPSUMMON_TIMED_DESPAWN, 40000))
                        {
                            c->queue_action(0, [c]()
                                {
                                    c->CastSpell(c, SPELL_NETHERBEAM_2, true);
                                });
                        }
                        DoCastSpellIfCan(m_creature, SPELL_NETHERBEAM_ENHA_1);
                        m_creature->SetObjectScale(1.5f);
                        m_transTimer = 2000;
                        break;
                    case 5:
                        if (Creature* c = m_creature->SummonCreature(
                                NPC_NETHER_BEAM, 845.5f, 6.5f, 67.4f, 0.0f,
                                TEMPSUMMON_TIMED_DESPAWN, 40000))
                        {
                            c->queue_action(0, [c]()
                                {
                                    c->CastSpell(c, SPELL_NETHERBEAM_2, true);
                                });
                        }
                        if (Creature* c = m_creature->SummonCreature(
                                NPC_NETHER_BEAM, 844.5f, -8.0f, 67.2f, 0.0f,
                                TEMPSUMMON_TIMED_DESPAWN, 40000))
                        {
                            c->queue_action(0, [c]()
                                {
                                    c->CastSpell(c, SPELL_NETHERBEAM_2, true);
                                });
                        }
                        DoCastSpellIfCan(m_creature, SPELL_NETHERBEAM_ENHA_2);
                        m_creature->SetObjectScale(1.75f);
                        m_transTimer = 4000;
                        break;
                    case 6:
                        DoCastSpellIfCan(m_creature, SPELL_NETHERBEAM_ENHA_3);
                        m_creature->SetObjectScale(2.0f);
                        // Wait for point gen to end if it hasn't already
                        m_transTimer = m_creature->movement_gens.has(
                                           movement::gen::point) ?
                                           0 :
                                           1;
                        break;
                    case 7:
                    {
                        m_creature->CastSpell(
                            m_creature, SPELL_KAEL_EXPLOSION, true);
                        // Remove all auras added so far
                        m_creature->remove_auras(SPELL_NETHERBEAM_ENHA_1);
                        m_creature->remove_auras(SPELL_NETHERBEAM_ENHA_2);
                        m_creature->remove_auras(SPELL_NETHERBEAM_ENHA_3);
                        // Despawn all spawned nether beams
                        auto cl = GetCreatureListWithEntryInGrid(
                            m_creature, NPC_NETHER_BEAM, 100.0f);
                        for (auto& elem : cl)
                            (elem)->ForcedDespawn();
                        // Hang life-less
                        DoCastSpellIfCan(
                            m_creature, SPELL_LIFE_LESS, CAST_TRIGGERED);
                        // Explode all game object
                        if (GameObject* go =
                                m_instance->GetSingleGameObjectFromStorage(
                                    GO_STATUE_LEFT))
                            go->SetGoState(GO_STATE_ACTIVE);
                        if (GameObject* go =
                                m_instance->GetSingleGameObjectFromStorage(
                                    GO_STATUE_RIGHT))
                            go->SetGoState(GO_STATE_ACTIVE);
                        if (GameObject* go =
                                m_instance->GetSingleGameObjectFromStorage(
                                    GO_TEMPEST_BRIDGE_WINDOW))
                            go->SetGoState(GO_STATE_ACTIVE);
                        m_transTimer = 5000;
                        break;
                    }
                    case 8:
                    {
                        m_pureNether = 100;
                        m_transTimer = 5000;
                        break;
                    }
                    case 9:
                        m_pureNether = 0;
                        m_creature->remove_auras(SPELL_LIFE_LESS);
                        m_creature->remove_auras(SPELL_KAEL_GAIN_POWER);
                        m_transTimer = 500;
                        break;
                    case 10:
                        m_creature->CastSpell(
                            m_creature, SPELL_FULL_POWER, false);
                        m_transTimer = 4000;
                        break;
                    case 11:
                        m_creature->movement_gens.push(
                            new movement::PointMovementGenerator(300,
                                m_creature->GetX(), m_creature->GetY(),
                                m_creature->GetZ() - 20.0f, false, false),
                            movement::EVENT_LEAVE_COMBAT);
                        m_transTimer = 0; // Wait for movement
                        break;
                    case 12:
                        m_creature->RemoveFlag(
                            UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE |
                                                  UNIT_FLAG_NOT_SELECTABLE);
                        m_instance->SetData(
                            DATA_KAELTHAS_PHASE, KAEL_PHASE_KAELTHAS_P2);
                        m_creature->RemoveInhabitType(INHABIT_AIR);

                        m_creature->movement_gens.remove_all(
                            movement::gen::stopped);
                        Pacify(false);
                        m_transTimer = 0;
                        break;
                    }
                    ++m_transStage;
                }
                else
                    m_transTimer -= diff;
            }
        }

        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        if (phase < KAEL_PHASE_KAELTHAS_P1)
            return;

        if ((phase == KAEL_PHASE_KAELTHAS_P1 ||
                phase == KAEL_PHASE_KAELTHAS_P2) &&
            IsPacified())
        {
            if (phase == KAEL_PHASE_KAELTHAS_P1)
                DoResetThreat();
            Pacify(false);
            m_creature->movement_gens.remove_all(movement::gen::stopped);
            m_creature->RemoveFlag(UNIT_FIELD_FLAGS,
                UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE);
        }

        if (IsPacified())
            return;

        if (phase == KAEL_PHASE_KAELTHAS_P1)
        {
            if (m_creature->GetHealthPercent() < 51.0f)
            {
                m_creature->InterruptNonMeleeSpells(false);
                m_instance->SetData(
                    DATA_KAELTHAS_PHASE, KAEL_PHASE_KAELTHAS_P1_TO_P2);
                m_creature->movement_gens.push(
                    new movement::PointMovementGenerator(
                        100, 794.4f, -0.46f, 48.7f, true, true),
                    movement::EVENT_LEAVE_COMBAT);
                Pacify(true);
                m_creature->movement_gens.push(
                    new movement::StoppedMovementGenerator(),
                    movement::EVENT_LEAVE_COMBAT);
                return;
            }

            if (m_creature->has_aura(SPELL_SHOCK_BARRIER))
                DoCastSpellIfCan(m_creature->getVictim(), SPELL_PYROBLAST);

            if (m_barrier <= diff)
            {
                if (DoCastSpellIfCan(m_creature, SPELL_SHOCK_BARRIER) ==
                    CAST_OK)
                    m_barrier = urand(55000, 65000);
            }
            else
                m_barrier -= diff;

            if (m_mc <= diff)
            {
                if (DoCastSpellIfCan(m_creature, SPELL_MIND_CONTROL) == CAST_OK)
                {
                    DoScriptText(
                        urand(0, 1) ? SAY_MIND_CONTROL_1 : SAY_MIND_CONTROL_2,
                        m_creature);
                    m_mc = 30000;
                }
            }
            else
                m_mc -= diff;
        }
        else if (phase == KAEL_PHASE_KAELTHAS_P2)
        {
            if (m_gravityLapse <= diff)
            {
                if (!m_creature->has_aura(SPELL_SHOCK_BARRIER))
                {
                    DoCastSpellIfCan(m_creature, SPELL_SHOCK_BARRIER);
                    m_shieldRemain = 10000;
                }
                else if (DoCastSpellIfCan(m_creature, SPELL_GRAVITY_LAPSE) ==
                         CAST_OK)
                {
                    DoScriptText(
                        urand(0, 1) ? SAY_GRAVITY_LAPSE_1 : SAY_GRAVITY_LAPSE_2,
                        m_creature);
                    m_gravityLapse = 90000;
                }
            }
            else
                m_gravityLapse -= diff;

            if (m_vapor)
            {
                if (DoCastSpellIfCan(m_creature, SPELL_NETHER_VAPOR) == CAST_OK)
                    m_vapor = false;
            }

            // Update time-left on shock barrier
            if (m_shieldRemain)
            {
                if (m_shieldRemain <= diff)
                    m_shieldRemain = 0;
                else
                    m_shieldRemain -= diff;
            }

            if (m_gravityEnd)
            {
                // Keep reducing timer while spamming netherbeam and keeping
                // shock barrier up
                if (m_gravityEnd <= diff)
                    m_gravityEnd = 0;
                else
                    m_gravityEnd -= diff;

                if (m_shieldRemain <= 3000)
                {
                    if (DoCastSpellIfCan(m_creature, SPELL_SHOCK_BARRIER) ==
                        CAST_OK)
                        m_shieldRemain = 10000;
                }
                else
                    DoCastSpellIfCan(m_creature, SPELL_NETHER_BEAM);
            }

            if (!m_gravityEnd && !m_endedGravity)
            {
                // Remove root and shock barrier when gravity lapse is over
                m_creature->remove_auras(SPELL_SELF_ROOT);
                m_creature->remove_auras(SPELL_SHOCK_BARRIER);
                m_endedGravity = true;
                m_shieldRemain = 0;
                // Let people fall down before arcane disrupting
                if (m_disrupt < 3000)
                    m_disrupt = 3000;
            }
        }

        // Spells that happen in both phases:

        if (m_disrupt <= diff)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_ARCANE_DISRUPTION) ==
                CAST_OK)
                m_disrupt = urand(15000, 35000);
        }
        else
            m_disrupt -= diff;

        if (m_phoenix <= diff)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_PHOENIX) == CAST_OK)
            {
                DoScriptText(
                    urand(0, 1) ? SAY_PHOENIX_1 : SAY_PHOENIX_2, m_creature);
                m_phoenix = urand(50000, 60000);
            }
        }
        else
            m_phoenix -= diff;

        if (m_flameStrike <= diff)
        {
            if (Unit* tar = m_creature->SelectAttackingTarget(
                    ATTACKING_TARGET_RANDOM, 0, SPELL_FLAME_STRIKE))
                if (DoCastSpellIfCan(tar, SPELL_FLAME_STRIKE) == CAST_OK)
                    m_flameStrike = urand(15000, 35000);
        }
        else
            m_flameStrike -= diff;

        if (m_fireball <= diff)
        {
            if (DoCastSpellIfCan(m_creature->getVictim(), SPELL_FIREBALL) ==
                CAST_OK)
                m_fireball = urand(5000, 10000);
        }
        else
            m_fireball -= diff;

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_boss_kaelthas(Creature* pCreature)
{
    return new boss_kaelthasAI(pCreature);
}

bool EffDummy_Kaelthas(
    Unit* /*caster*/, uint32 id, SpellEffectIndex /*effIdx*/, Creature* target)
{
    if (id == SPELL_GRAVITY_LAPSE && target->GetEntry() == NPC_KAELTHAS)
    {
        if (boss_kaelthasAI* kaelAI =
                dynamic_cast<boss_kaelthasAI*>(target->AI()))
            kaelAI->GravityLapseCallback();
        return true;
    }
    return false;
}

void AddSC_boss_kaelthas()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "boss_kaelthas";
    pNewScript->GetAI = &GetAI_boss_kaelthas;
    pNewScript->pEffectDummyNPC = &EffDummy_Kaelthas;
    pNewScript->RegisterSelf();
}
