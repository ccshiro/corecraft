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
SDName: boss_alar
SD%Complete: 100
SDComment:
SDCategory: Tempest Keep, The Eye
EndScriptData */

#include "precompiled.h"
#include "the_eye.h"

enum
{
    PHASE_ONE = 1,
    PHASE_ONE_MOVE,
    PHASE_ONE_ROOF,
    PHASE_ONE_REBIRTH,
    PHASE_TWO,
    PHASE_TWO_METEOR,

    NPC_ALAR_METEOR = 100036,

    // Phase one spells
    SPELL_FLAME_BUFFET = 34121,
    SPELL_SUMMON_EMBER = 18814,
    NPC_EMBER_OF_ALAR = 19551,
    SPELL_FLAME_QUILLS = 34229,
    SPELL_EMBER_BLAST = 34341,
    // Phase two spells
    SPELL_REBIRTH = 34342,
    SPELL_METEOR_PRE = 35367,
    SPELL_METEOR = 35181,
    SPELL_REBIRTH_METEOR = 35369,
    SPELL_MELT_ARMOR = 35410,
    SPELL_CHARGE = 35412,
    NPC_FLAME_PATCH = 20602,

    SPELL_SELF_ROOT = 42716,
};

#define ALAR_POSITION_COUNT 6
static const float alarPositions[ALAR_POSITION_COUNT][3] = {
    {335.6f, 59.3f, 17.8f}, {388.8f, 31.8f, 20.2f}, {388.6f, -33.0f, 20.2f},
    {339.8f, -60.5f, 17.8f}, {261.3f, -41.3f, 20.2f}, {262.4f, 40.9f, 20.2f},
};
static const float inbetweenPos[3] = {293.1f, 0.0f, 28.9f};

static const float roofPosition[3] = {331.0f, 0.0f, 41.0f};

static const float rebirthPosition[4] = {333.0f, 0.0f, -2.39f, 3.14f};

struct MANGOS_DLL_DECL boss_alarAI : public ScriptedAI
{
    boss_alarAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_instance = (ScriptedInstance*)pCreature->GetInstanceData();
        Reset();
        m_creature->AddInhabitType(INHABIT_AIR);
    }

    std::vector<ObjectGuid> m_spawns;

    ScriptedInstance* m_instance;
    uint32 m_phase;
    // Phase one
    uint32 m_move;
    uint32 m_nextQuill;
    uint32 m_lastPointId;
    uint32 m_buffet;
    // Phase Two
    uint32 m_rebirth;
    uint32 m_meteor;
    uint32 m_visibility;
    uint32 m_flamePatch;
    uint32 m_meltArmor;
    uint32 m_charge;

    void Reset() override
    {
        SetCombatMovement(false);
        m_phase = PHASE_ONE;
        m_move = urand(15000, 20000);
        m_nextQuill = 1; // First quill is at second move
        m_lastPointId = 0;
        m_buffet = 1000;
        m_rebirth = 0;
        m_meteor = 0;
        m_visibility = 0;
        m_flamePatch = urand(5000, 10000);
        m_meltArmor = urand(5000, 10000);
        m_charge = urand(5000, 10000);

        m_creature->SetVisibility(VISIBILITY_ON);
        m_creature->ApplySpellImmune(
            0, IMMUNITY_DAMAGE, SPELL_SCHOOL_MASK_FIRE, true);
    }

    void Aggro(Unit* /*pWho*/) override
    {
        // Always flies to first position when aggrod
        MoveAlarPos(1);
        if (m_instance)
            m_instance->SetData(TYPE_ALAR, IN_PROGRESS);
    }

    void EnterEvadeMode(bool by_group) override
    {
        m_creature->AddInhabitType(INHABIT_AIR);
        ScriptedAI::EnterEvadeMode(by_group);
    }

    void JustReachedHome() override
    {
        if (m_instance)
            m_instance->SetData(TYPE_ALAR, FAIL);
        DespawnSummons();
    }

    void JustDied(Unit* /*pKiller*/) override
    {
        if (m_instance)
            m_instance->SetData(TYPE_ALAR, DONE);
        DespawnSummons();
    }

    void DespawnSummons()
    {
        for (auto& elem : m_spawns)
            if (Creature* c = m_creature->GetMap()->GetCreature(elem))
                c->Kill(c);
        m_spawns.clear();
    }

    void JustSummoned(Creature* c) override
    {
        if (c->GetEntry() == NPC_EMBER_OF_ALAR ||
            c->GetEntry() == NPC_FLAME_PATCH)
            m_spawns.push_back(c->GetObjectGuid());
    }

    void MovementInform(movement::gen uiType, uint32 uiPointId) override
    {
        if (uiType == movement::gen::point)
        {
            if (uiPointId == 100)
                m_creature->movement_gens.push(
                    new movement::PointMovementGenerator(m_lastPointId,
                        alarPositions[m_lastPointId - 1][0],
                        alarPositions[m_lastPointId - 1][1],
                        alarPositions[m_lastPointId - 1][2], false, true),
                    movement::EVENT_LEAVE_COMBAT);
            else if (uiPointId == 200)
            {
                DoCastSpellIfCan(
                    m_creature, SPELL_FLAME_QUILLS, CAST_TRIGGERED);
                m_move = 12000;
            }
            else if (uiPointId == 300)
            {
                m_creature->AddAuraThroughNewHolder(
                    SPELL_SELF_ROOT, m_creature);
                DoCastSpellIfCan(m_creature, SPELL_METEOR_PRE);
                if (Creature* c = m_creature->SummonCreature(NPC_ALAR_METEOR,
                        m_creature->GetX(), m_creature->GetY(),
                        m_creature->GetZ(), 0.0f, TEMPSUMMON_TIMED_DESPAWN,
                        25000))
                    c->CastSpell(c, SPELL_METEOR_PRE, true);
                m_visibility = 1800;
            }
            else
            {
                if (m_phase != PHASE_ONE) // If we're landing from a
                                          // phase_one_roof phase, set phase to
                                          // phase_one
                    m_phase = PHASE_ONE;
                m_creature->CastSpell(m_creature, SPELL_SELF_ROOT, true);
                m_buffet = 2000;
            }
        }
    }

    void MoveAlarPos(uint32 point = 0)
    {
        m_phase = PHASE_ONE_MOVE;

        // Pick a point if default value is specified
        if (point == 0)
        {
            do
                point = urand(1, ALAR_POSITION_COUNT);
            while (point == m_lastPointId);
        }

        assert(point <= ALAR_POSITION_COUNT);

        // Fly to inbetween if we're flying from 5 -> 6 or vice versa
        if ((m_lastPointId == 5 || m_lastPointId == 6) &&
            (point == 5 || point == 6))
        {
            m_creature->movement_gens.push(
                new movement::PointMovementGenerator(100, inbetweenPos[0],
                    inbetweenPos[1], inbetweenPos[2], false, true),
                movement::EVENT_LEAVE_COMBAT);
        }
        else
        {
            m_creature->movement_gens.push(
                new movement::PointMovementGenerator(point,
                    alarPositions[point - 1][0], alarPositions[point - 1][1],
                    alarPositions[point - 1][2], false, true),
                movement::EVENT_LEAVE_COMBAT);
        }

        m_lastPointId = point;
    }

    void DamageTaken(Unit* /*pDealer*/, uint32& uiDamage) override
    {
        if (uiDamage < m_creature->GetHealth())
            return;

        if (m_phase != PHASE_TWO)
        {
            uiDamage = m_creature->GetHealth() - 1;
            if (m_phase == PHASE_ONE_REBIRTH)
                return; // Already begun rebirth

            // Start rebirth
            m_phase = PHASE_ONE_REBIRTH;
            m_rebirth = 15000;
            DoCastSpellIfCan(m_creature, SPELL_EMBER_BLAST, CAST_TRIGGERED);
        }
    }

    void UpdateAI(const uint32 diff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        if (m_phase == PHASE_ONE || m_phase == PHASE_ONE_ROOF)
        {
            if (m_move <= diff)
            {
                if (m_phase != PHASE_ONE_ROOF)
                {
                    // 1.0 version of Al'ar spawned 3 embers each time
                    DoCastSpellIfCan(
                        m_creature, SPELL_SUMMON_EMBER, CAST_TRIGGERED);
                }

                m_creature->remove_auras(SPELL_SELF_ROOT);
                if (!m_nextQuill)
                {
                    m_phase = PHASE_ONE_ROOF;
                    m_creature->movement_gens.push(
                        new movement::PointMovementGenerator(200,
                            roofPosition[0], roofPosition[1], roofPosition[2],
                            false, true),
                        movement::EVENT_LEAVE_COMBAT);
                    m_nextQuill = urand(1, 6);
                }
                else
                {
                    MoveAlarPos();
                    --m_nextQuill;
                }

                m_move = urand(30000, 40000);
            }
            else
                m_move -= diff;
        }
        else if (m_phase == PHASE_ONE_REBIRTH)
        {
            if (m_rebirth)
            {
                if (m_rebirth <= diff)
                {
                    m_creature->NearTeleportTo(rebirthPosition[0],
                        rebirthPosition[1], rebirthPosition[2],
                        rebirthPosition[3]);
                    if (DoCastSpellIfCan(m_creature, SPELL_REBIRTH) == CAST_OK)
                    {
                        m_creature->remove_auras(SPELL_EMBER_BLAST);
                        m_rebirth = 0;
                    }
                }
                else
                    m_rebirth -= diff;
            }

            // Wait until our health is restored with switching phase (so we
            // don't get killed)
            if (m_creature->GetHealthPercent() > 90.0f)
            {
                m_creature->RemoveInhabitType(INHABIT_AIR);
                m_creature->remove_auras(SPELL_SELF_ROOT);
                SetCombatMovement(true);
                m_phase = PHASE_TWO;
                m_move = 30000;
                m_rebirth = 0;
            }
        }
        else if (m_phase == PHASE_TWO)
        {
            if (m_move <= diff)
            {
                m_creature->AddInhabitType(INHABIT_AIR);
                SetCombatMovement(false);
                m_creature->movement_gens.push(
                    new movement::PointMovementGenerator(300, roofPosition[0],
                        roofPosition[1], roofPosition[2], false, true),
                    movement::EVENT_LEAVE_COMBAT);
                m_move = urand(50000, 60000);
                m_phase = PHASE_TWO_METEOR;
            }
            else
                m_move -= diff;

            if (m_meltArmor <= diff)
            {
                if (DoCastSpellIfCan(
                        m_creature->getVictim(), SPELL_MELT_ARMOR) == CAST_OK)
                    m_meltArmor = urand(60000, 70000);
            }
            else
                m_meltArmor -= diff;

            if (m_flamePatch <= diff)
            {
                if (Unit* tar = m_creature->SelectAttackingTarget(
                        ATTACKING_TARGET_RANDOM, 0))
                {
                    m_creature->SummonCreature(NPC_FLAME_PATCH, tar->GetX(),
                        tar->GetY(), tar->GetZ(), 0.0f,
                        TEMPSUMMON_TIMED_DESPAWN, 2 * MINUTE * IN_MILLISECONDS);
                    m_flamePatch = urand(20000, 30000);
                }
            }
            else
                m_flamePatch -= diff;

            if (m_charge <= diff)
            {
                if (Unit* tar = m_creature->SelectAttackingTarget(
                        ATTACKING_TARGET_RANDOM, 1, SPELL_CHARGE))
                    if (DoCastSpellIfCan(tar, SPELL_CHARGE) == CAST_OK)
                        m_charge = urand(45000, 55000);
            }
            else
                m_charge -= diff;
        }
        else if (m_phase == PHASE_TWO_METEOR)
        {
            if (m_visibility)
            {
                if (m_visibility <= diff)
                {
                    m_creature->SetVisibility(
                        m_creature->GetVisibility() == VISIBILITY_ON ?
                            VISIBILITY_OFF :
                            VISIBILITY_ON);
                    m_visibility = 0;
                }
                else
                    m_visibility -= diff;
            }

            if (m_meteor)
            {
                if (m_meteor <= diff)
                {
                    Creature* caster = GetClosestCreatureWithEntry(
                        m_creature, NPC_ALAR_METEOR, 25.0f);
                    Unit* tar = m_creature->SelectAttackingTarget(
                        ATTACKING_TARGET_RANDOM, 0, SPELL_METEOR);
                    if (tar && caster)
                    {
                        m_creature->remove_auras(SPELL_METEOR_PRE);
                        caster->CastSpell(tar, SPELL_METEOR, false);
                        caster->remove_auras(SPELL_METEOR_PRE);
                        m_creature->NearTeleportTo(tar->GetX(), tar->GetY(),
                            tar->GetZ(), m_creature->GetAngle(tar));
                        m_meteor = 0;
                        m_rebirth = 2000;
                        m_visibility = 1500;
                    }
                }
                else
                    m_meteor -= diff;
            }

            if (m_rebirth)
            {
                if (m_rebirth <= diff)
                {
                    if (DoCastSpellIfCan(m_creature, SPELL_REBIRTH_METEOR) ==
                        CAST_OK)
                    {
                        // In Al'ar 1.0 gained health when rebirthing, this
                        // amount was derived through simulation
                        m_creature->SetHealth(
                            m_creature->GetHealth() +
                            m_creature->GetMaxHealth() * 0.05f);

                        m_creature->RemoveInhabitType(INHABIT_AIR);
                        SetCombatMovement(true);
                        m_creature->remove_auras(SPELL_SELF_ROOT);
                        m_rebirth = 0;
                        m_buffet = 5000;
                        m_phase = PHASE_TWO;
                    }
                }
                else
                    m_rebirth -= diff;
            }
        }

        if (m_phase == PHASE_ONE || m_phase == PHASE_TWO)
        {
            // On retail he stops buffeting when someone is in his melee
            // range and has auto attack toggle on (even if said person is
            // not hitting Al'ar).
            for (auto& attacker : m_creature->getAttackers())
                if (m_creature->CanReachWithMeleeAttack(attacker))
                {
                    m_buffet = 1000;
                    break;
                }
            if (m_buffet <= diff)
            {
                if (DoCastSpellIfCan(
                        m_creature->getVictim(), SPELL_FLAME_BUFFET) == CAST_OK)
                    m_buffet = 2000;
            }
            else
                m_buffet -= diff;

            DoMeleeAttackIfReady();
        }
    }
};

CreatureAI* GetAI_boss_alar(Creature* pCreature)
{
    return new boss_alarAI(pCreature);
}

bool DummyAura_Alar(const Aura* aur, bool apply)
{
    if (aur->GetId() == SPELL_METEOR_PRE && apply)
        if (Unit* caster = aur->GetCaster())
            if (caster->GetTypeId() == TYPEID_UNIT)
                if (boss_alarAI* AI =
                        dynamic_cast<boss_alarAI*>(((Creature*)caster)->AI()))
                    AI->m_meteor = 2000;
    return true;
}

void AddSC_boss_alar()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "boss_alar";
    pNewScript->GetAI = &GetAI_boss_alar;
    pNewScript->pEffectAuraDummy = &DummyAura_Alar;
    pNewScript->RegisterSelf();
}
