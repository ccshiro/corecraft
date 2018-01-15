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
SDName: boss_solarian
SD%Complete: 100
SDComment:
SDCategory: Tempest Keep, The Eye
EndScriptData */

#include "precompiled.h"
#include "the_eye.h"

enum
{
    SAY_AGGRO = -1550007,
    SAY_PHASE_2_ONE = -1550008,
    SAY_PHASE_2_TWO = -1550009,
    SAY_PHASE_3_ONE = -1550014, // MAYBE NOT USED
    SAY_PHASE_3_TWO = -1550015,
    SAY_KILL_1 = -1550010,
    SAY_KILL_2 = -1550011,
    SAY_KILL_3 = -1550012,
    SAY_DEATH = -1550013,

    NPC_SOLARIUM_AGENT = 18925,
    NPC_SOLARIUM_PRIEST = 18806,

    // Standard phase
    SPELL_MARK_OF_SOLARIAN = 33023,
    SPELL_BLINDING_LIGHT = 33009,
    SPELL_WRATH_OF_THE_ASTROMANCER = 33040,
    SPELL_ARCANE_MISSILES = 33031,
    // Her spotlight phase spells
    SPELL_TELEPORT_START_POS = 33244,
    SPELL_INVISIBILITY = 32754,        // Not sure if correct spell
    SPELL_ASTROMANCER_SPLIT_R = 33281, // Right, front, left
    SPELL_ASTROMANCER_SPLIT_F = 33282,
    SPELL_ASTROMANCER_SPLIT_L = 33348,
    // "Enrage" phase
    SPELL_SOLARIAN_TRANSFORM = 39117,
    SPELL_VOID_BOLT = 39329,
    SPELL_PSYCHIC_SCREAM = 34322,

    NPC_SPLIT = 18928,

    PHASE_SOLARIAN = 1,
    PHASE_INBETWEEN,
    PHASE_ADDS,
    PHASE_ENRAGE
};

struct MANGOS_DLL_DECL boss_high_astromancer_solarianAI : public ScriptedAI
{
    boss_high_astromancer_solarianAI(Creature* pCreature)
      : ScriptedAI(pCreature)
    {
        m_instance = (ScriptedInstance*)pCreature->GetInstanceData();
        Reset();
    }

    ScriptedInstance* m_instance;
    uint32 m_phase;
    uint32 m_phaseTimer;
    uint32 m_mark;
    uint32 m_wrath;
    uint32 m_missile;
    uint32 m_void;
    uint32 m_scream;

    void Reset() override
    {
        m_phase = PHASE_SOLARIAN;
        m_phaseTimer = 50000;
        m_mark = 30000;
        m_wrath = 22000;
        m_missile = urand(3000, 5000);
        m_void = 3000;
        m_scream = 7000;
    }

    void Aggro(Unit* /*pWho*/) override
    {
        if (m_instance)
            m_instance->SetData(TYPE_SOLARIAN, IN_PROGRESS);

        DoScriptText(SAY_AGGRO, m_creature);
    }

    void JustReachedHome() override
    {
        if (m_instance)
            m_instance->SetData(TYPE_SOLARIAN, FAIL);
        DespawnSummons();
    }

    void JustDied(Unit* /*pKiller*/) override
    {
        if (m_instance)
            m_instance->SetData(TYPE_SOLARIAN, DONE);

        DoScriptText(SAY_DEATH, m_creature);
        DespawnSummons();

        // Remove the wrath dot on everyone so it stops jumping around
        for (const auto& elem : m_creature->GetMap()->GetPlayers())
        {
            if (Player* p = elem.getSource())
                p->remove_auras(33045);
        }
    }

    void KilledUnit(Unit* victim) override
    {
        DoKillSay(m_creature, victim, SAY_KILL_1, SAY_KILL_2, SAY_KILL_3);
    }

    std::vector<ObjectGuid> m_spawns;
    void DespawnSummons()
    {
        for (auto& elem : m_spawns)
            if (Creature* c = m_creature->GetMap()->GetCreature(elem))
                c->Kill(c);
        m_spawns.clear();
    }

    void JustSummoned(Creature* c) override
    {
        m_spawns.push_back(c->GetObjectGuid());
    }

    void UpdateAI(const uint32 diff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim() ||
            m_phase == PHASE_INBETWEEN || m_phase == PHASE_ADDS)
        {
            if (m_phase == PHASE_INBETWEEN)
            {
                if (m_phaseTimer <= diff)
                {
                    m_creature->CastSpell(
                        m_creature, SPELL_ASTROMANCER_SPLIT_R, true);
                    m_creature->CastSpell(
                        m_creature, SPELL_ASTROMANCER_SPLIT_F, true);
                    m_creature->CastSpell(
                        m_creature, SPELL_ASTROMANCER_SPLIT_L, true);
                    m_creature->CastSpell(m_creature, SPELL_INVISIBILITY, true);
                    m_phase = PHASE_ADDS;
                    m_phaseTimer = 23000;
                    // These spells are right away after this phase
                    m_mark = 0;
                    m_wrath = 0;
                }
                else
                    m_phaseTimer -= diff;
            }
            else if (m_phase == PHASE_ADDS)
            {
                if (m_phaseTimer <= diff)
                {
                    Unit* victim = m_creature->SelectAttackingTarget(
                        ATTACKING_TARGET_TOPAGGRO, 0);
                    if (!victim)
                    {
                        EnterEvadeMode();
                        return;
                    }

                    auto splits = GetCreatureListWithEntryInGrid(
                        m_creature, NPC_SPLIT, 100.0f);
                    for (auto i = splits.begin(); i != splits.end();)
                    {
                        if (!(*i)->isAlive())
                            i = splits.erase(i);
                        else
                            ++i;
                    }
                    if (splits.size() != 3)
                    {
                        EnterEvadeMode();
                        return;
                    }
                    auto itr = splits.begin();
                    float ang = (*itr)->GetAngle(432.6f, -373.8f);
                    m_creature->SummonCreature(NPC_SOLARIUM_PRIEST,
                        (*itr)->GetX(), (*itr)->GetY(), (*itr)->GetZ(), ang,
                        TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 20000);
                    std::advance(itr, 1);
                    ang = (*itr)->GetAngle(432.6f, -373.8f);
                    m_creature->SummonCreature(NPC_SOLARIUM_PRIEST,
                        (*itr)->GetX(), (*itr)->GetY(), (*itr)->GetZ(), ang,
                        TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 20000);
                    std::advance(itr, 1);
                    ang = (*itr)->GetAngle(432.6f, -373.8f);
                    m_creature->NearTeleportTo(
                        (*itr)->GetX(), (*itr)->GetY(), (*itr)->GetZ(), ang);

                    m_phase = PHASE_SOLARIAN;
                    m_phaseTimer = 70000;
                    m_creature->remove_auras(SPELL_INVISIBILITY);

                    Pacify(false);
                    m_creature->movement_gens.remove_all(
                        movement::gen::stopped);
                }
                else
                    m_phaseTimer -= diff;
            }

            return;
        }

        if (m_phase == PHASE_SOLARIAN && m_creature->GetHealthPercent() < 21.0f)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_SOLARIAN_TRANSFORM) ==
                CAST_OK)
                m_phase = PHASE_ENRAGE;
            return;
        }

        if (m_phase == PHASE_SOLARIAN)
        {
            if (m_phaseTimer <= diff)
            {
                if (CanCastSpell(m_creature, SPELL_TELEPORT_START_POS, false) ==
                    CAST_OK)
                {
                    DoScriptText(
                        urand(0, 1) ? SAY_PHASE_2_ONE : SAY_PHASE_2_TWO,
                        m_creature);
                    Pacify(true);
                    m_creature->movement_gens.push(
                        new movement::StoppedMovementGenerator(),
                        movement::EVENT_LEAVE_COMBAT);
                    DoCastSpellIfCan(
                        m_creature, SPELL_TELEPORT_START_POS, CAST_TRIGGERED);
                    m_phase = PHASE_INBETWEEN;
                    m_phaseTimer = 2000;
                }
                return;
            }
            else
                m_phaseTimer -= diff;

            if (m_mark <= diff)
            {
                if (DoCastSpellIfCan(m_creature, SPELL_MARK_OF_SOLARIAN) ==
                    CAST_OK)
                {
                    DoCastSpellIfCan(
                        m_creature, SPELL_BLINDING_LIGHT, CAST_TRIGGERED);
                    m_mark = 30000;
                }
            }
            else
                m_mark -= diff;

            if (m_wrath <= diff)
            {
                if (Unit* tar = m_creature->SelectAttackingTarget(
                        ATTACKING_TARGET_RANDOM, 0))
                {
                    if (DoCastSpellIfCan(tar, SPELL_WRATH_OF_THE_ASTROMANCER,
                            false) == CAST_OK)
                        m_wrath = 45000;
                }
            }
            else
                m_wrath -= diff;

            if (m_missile <= diff)
            {
                if (Unit* tar = m_creature->SelectAttackingTarget(
                        ATTACKING_TARGET_RANDOM, 0, nullptr,
                        SELECT_FLAG_IN_FRONT))
                {
                    if (DoCastSpellIfCan(tar, SPELL_ARCANE_MISSILES) == CAST_OK)
                        m_missile = urand(3000, 5000);
                }
            }
            else
                m_missile -= diff;
        }
        else if (m_phase == PHASE_ENRAGE)
        {
            if (m_scream <= diff)
            {
                if (DoCastSpellIfCan(m_creature, SPELL_PSYCHIC_SCREAM) ==
                    CAST_OK)
                    m_scream = urand(15000, 25000);
            }
            else
                m_scream -= diff;

            if (m_void <= diff)
            {
                if (DoCastSpellIfCan(
                        m_creature->getVictim(), SPELL_VOID_BOLT) == CAST_OK)
                    m_void = urand(5000, 10000);
            }
            else
                m_void -= diff;
        }

        if (m_phase != PHASE_INBETWEEN && m_phase != PHASE_ADDS)
            DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_boss_high_astromancer_solarian(Creature* pCreature)
{
    return new boss_high_astromancer_solarianAI(pCreature);
}

void AddSC_boss_high_astromancer_solarian()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "boss_high_astromancer_solarian";
    pNewScript->GetAI = &GetAI_boss_high_astromancer_solarian;
    pNewScript->RegisterSelf();
}
