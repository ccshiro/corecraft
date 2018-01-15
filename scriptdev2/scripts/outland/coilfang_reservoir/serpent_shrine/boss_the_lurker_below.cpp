/* Copyright (C) 2013-2015 Corecraft <https://www.worldofcorecraft.com/>
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
SDName: boss_the_lurker_below
SD%Complete: 100
SDComment:
SDCategory: Coilfang Resevoir, Serpentshrine Cavern
EndScriptData */

#include "precompiled.h"
#include "serpentshrine_cavern.h"

enum
{
    SPELL_SPOUT_AURA = 37430,
    SPELL_SPOUT_CAST = 37431,
    SPELL_GEYSER = 37478,
    SPELL_WATER_BOLT = 37138,
    SPELL_WHIRL = 37660,
    SPELL_BIRTH = 37745,
    SPELL_SUBMERGE = 37550,
    SPELL_STUN_SELF = 48342,

    NPC_GUARDIAN = 21873,
    NPC_AMBUSHER = 21865,

    LURKER_SPIN_DURATION = 16000 // ms it takes to spin one lap
};

#define GUARDIAN_COUNT 3
static float guardianPositions[GUARDIAN_COUNT][3][3] = {
    {{-13.5f, -420.8f, -21.3f}, {7.7f, -428.5f, -21.3f},
     {11.2f, -427.5f, -19.7f}},
    {{49.7f, -369.9f, -21.4f}, {46.2f, -386.4f, -21.3f},
     {45.0f, -389.6f, -19.4f}},
    {{92.1f, -440.7f, -21.3f}, {66.9f, -433.2f, -21.3f},
     {63.9f, -432.1f, -19.5f}}};

#define AMBUSHER_COUNT 6
static float ambusherPositions[AMBUSHER_COUNT][3][3] = {
    {{61.4f, -484.9f, -22.3f}, {57.1f, -478.9f, -21.3f},
     {50.0f, -458.9f, -19.8f}},
    {{69.8f, -480.9f, -21.5f}, {68.2f, -474.0f, -21.0f},
     {62.7f, -456.1f, -19.8f}},
    {{7.2f, -487.3f, -21.3f}, {11.f, -476.7f, -21.3f},
     {16.9f, -457.8f, -19.8f}},
    {{-5.9f, -482.3f, -21.3f}, {-1.6f, -472.6f, -21.3f},
     {6.1f, -457.2f, -19.8f}},
    {{59.9f, -360.9f, -21.3f}, {62.4f, -377.0f, -21.2f},
     {64.1f, -379.4f, -19.7f}},
    {{93.3f, -387.5f, -21.5f}, {79.5f, -386.7f, -21.3f},
     {76.6f, -384.4f, -19.7f}}};

struct MANGOS_DLL_DECL boss_the_lurker_belowAI : public ScriptedAI
{
    boss_the_lurker_belowAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_instance = (ScriptedInstance*)pCreature->GetInstanceData();
        Reset();
    }

    std::vector<ObjectGuid> m_spawns;
    ScriptedInstance* m_instance;
    bool m_spoutCast; // Don't target anything while we're casting spout
    uint32 m_spout;
    int m_spouting;
    float m_startOrientation;
    bool m_passedTwoPi;
    bool m_positiveDir;
    uint32 m_spoutRetarget; // Delay between spout and re-starting attack
    uint32 m_geyser;
    uint32 m_whirl;
    uint32 m_whirlCount; // Only does 2 whirls until spout/submerge resets it
    uint32 m_submerge;
    uint32 m_emerge;
    uint32 m_waterBolt;

    void DespawnSummons()
    {
        for (auto& elem : m_spawns)
            if (Creature* c = m_creature->GetMap()->GetCreature(elem))
                c->Kill(c);
        m_spawns.clear();
    }

    void Reset() override
    {
        SetCombatMovement(false);
        m_spoutCast = false;
        m_spout = 40000;
        m_spouting = 0;
        m_passedTwoPi = false;
        m_positiveDir = false;
        m_spoutRetarget = 0;
        m_geyser = 10000;
        m_whirl = urand(15000, 20000);
        m_whirlCount = 0;
        m_submerge = 90000;
        m_emerge = 0;
        m_waterBolt = 8000;
        Pacify(true);
    }

    void Aggro(Unit* /*pWho*/) override
    {
        // Already IN_PROGRESS
    }

    void JustReachedHome() override
    {
        if (m_instance)
            m_instance->SetData(TYPE_LURKER_BELOW, FAIL);
        m_creature->ForcedDespawn();
        DespawnSummons();
    }

    void JustDied(Unit* /*pKiller*/) override
    {
        if (m_instance)
            m_instance->SetData(TYPE_LURKER_BELOW, DONE);
    }

    void SpoutCastCallback()
    {
        Pacify(true);
        m_creature->CastSpell(m_creature, SPELL_SPOUT_AURA, true);
        m_spoutCast = false;
        m_spouting = LURKER_SPIN_DURATION; // Duration of a full lapse
        m_positiveDir = (bool)urand(0, 1);
        m_creature->CastSpell(m_creature, SPELL_STUN_SELF, true);
        m_creature->SetFacingTo(m_startOrientation);
        m_creature->SetOrientation(m_startOrientation);
        // Reset timers
        m_geyser = 5000;
        m_whirl = 3000;
        m_whirlCount = 0;
        m_waterBolt = 5000;
    }

    void SummonedBy(WorldObject*) override
    {
        m_creature->SetInCombatWithZone();
        m_creature->CastSpell(m_creature, SPELL_BIRTH, false);
    }

    void JustSummoned(Creature* c) override
    {
        m_spawns.push_back(c->GetObjectGuid());
        c->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PASSIVE);
    }

    void SummonedMovementInform(
        Creature* pSummoned, movement::gen type, uint32 uiData) override
    {
        if (type == movement::gen::point)
        {
            switch (uiData)
            {
            case 0:
            case 1:
            case 2:
                pSummoned->movement_gens.push(
                    new movement::PointMovementGenerator(100,
                        guardianPositions[uiData][2][0],
                        guardianPositions[uiData][2][1],
                        guardianPositions[uiData][2][2], false, true),
                    movement::EVENT_LEAVE_COMBAT);
                pSummoned->RemoveInhabitType(INHABIT_WATER);
                pSummoned->AddInhabitType(INHABIT_GROUND);
                break;
            case 3:
            case 4:
            case 5:
            case 6:
            case 7:
            case 8:
                pSummoned->movement_gens.push(
                    new movement::PointMovementGenerator(100,
                        ambusherPositions[uiData - GUARDIAN_COUNT][2][0],
                        ambusherPositions[uiData - GUARDIAN_COUNT][2][1],
                        ambusherPositions[uiData - GUARDIAN_COUNT][2][2], false,
                        true),
                    movement::EVENT_LEAVE_COMBAT);
                pSummoned->RemoveInhabitType(INHABIT_WATER);
                pSummoned->AddInhabitType(INHABIT_GROUND);
                break;

            case 100:
                pSummoned->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PASSIVE);
                pSummoned->SetInCombatWithZone();
                break;
            }
        }
    }

    void SubmergeCallback()
    {
        for (int i = 0; i < GUARDIAN_COUNT; ++i)
            if (Creature* c = m_creature->SummonCreature(NPC_GUARDIAN,
                    guardianPositions[i][0][0], guardianPositions[i][0][1],
                    guardianPositions[i][0][2], 0, TEMPSUMMON_DEAD_DESPAWN, 0))
            {
                c->movement_gens.push(
                    new movement::PointMovementGenerator(i,
                        guardianPositions[i][1][0], guardianPositions[i][1][1],
                        guardianPositions[i][1][2], false, true),
                    movement::EVENT_LEAVE_COMBAT);
            }
        for (int i = 0; i < AMBUSHER_COUNT; ++i)
            if (Creature* c = m_creature->SummonCreature(NPC_AMBUSHER,
                    ambusherPositions[i][0][0], ambusherPositions[i][0][1],
                    ambusherPositions[i][0][2], 0, TEMPSUMMON_DEAD_DESPAWN, 0))
            {
                c->movement_gens.push(
                    new movement::PointMovementGenerator(GUARDIAN_COUNT + i,
                        ambusherPositions[i][1][0], ambusherPositions[i][1][1],
                        ambusherPositions[i][1][2], false, true),
                    movement::EVENT_LEAVE_COMBAT);
            }
        m_emerge = 60000;
        // Reset timers
        m_geyser = 5000;
        m_whirl = 3000;
        m_whirlCount = 0;
        m_waterBolt = 8000;
        m_spout = 0; // Spout right away afterwards
    }

    void BirthCallback() { Pacify(false); }

    void UpdateAI(const uint32 diff) override
    {
        if (m_spoutCast)
        {
            // Wait for spout callback but update timers that do not reset after
            // spout
            if (m_spout > diff)
                m_spout -= diff;
            if (m_submerge > diff)
                m_submerge -= diff;
            m_creature->SetTargetGuid(
                ObjectGuid()); // Will be reset when stun goes out
            m_creature->SetFacingTo(m_startOrientation);
            m_creature->SetOrientation(m_startOrientation);
            return;
        }

        if (m_spouting)
        {
            m_spouting -= diff;
            if (m_spouting <= 0)
            {
                m_creature->remove_auras(SPELL_STUN_SELF);
                m_creature->remove_auras(SPELL_SPOUT_AURA);
                m_spoutRetarget = 1000;
                m_spouting = 0;
                // Reset timers
                m_geyser = 5000;
                m_whirl = 3000; // Use whirl right after spout
                m_whirlCount = 0;
                m_waterBolt = 5000;
            }
            else if (!m_creature->movement_gens.has(movement::gen::facing))
            {
                // Update orientation
                float ori = m_startOrientation +
                            (m_positiveDir ? 1.0f : -1.0f) *
                                (LURKER_SPIN_DURATION - m_spouting) *
                                ((2 * M_PI_F) / LURKER_SPIN_DURATION);
                m_creature->SetFacingTo(ori);
                m_creature->SetOrientation(ori);
            }
        }

        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        if (m_emerge)
        {
            if (m_emerge <= diff)
            {
                m_creature->RemoveFlag(UNIT_FIELD_FLAGS,
                    UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE);
                m_creature->remove_auras(SPELL_SUBMERGE);
                m_emerge = 0;
                m_creature->CastSpell(m_creature, SPELL_BIRTH, false);
            }
            else
                m_emerge -= diff;
            return;
        }

        if (m_spoutRetarget)
        {
            if (m_spoutRetarget <= diff)
            {
                Pacify(false);
                m_spoutRetarget = 0;
            }
            else
                m_spoutRetarget -= diff;
        }

        // Don't do combat stuff while pacified
        if (IsPacified())
            return;

        if (!m_creature->CanReachWithMeleeAttack(m_creature->getVictim()))
        {
            if (m_waterBolt <= diff)
            {
                if (Unit* tar = m_creature->SelectAttackingTarget(
                        ATTACKING_TARGET_RANDOM, 0))
                    if (DoCastSpellIfCan(tar, SPELL_WATER_BOLT) == CAST_OK)
                        m_waterBolt = 1000;
            }
            else
                m_waterBolt -= diff;
        }
        else
            m_waterBolt = 5000;

        if (m_submerge <= diff)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_SUBMERGE) == CAST_OK)
            {
                Pacify(true);
                m_creature->SetFlag(UNIT_FIELD_FLAGS,
                    UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE);
                m_submerge = 90000; // Timers are not updated while submerged
            }
        }
        else
            m_submerge -= diff;

        if (m_spout <= diff)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_SPOUT_CAST) == CAST_OK)
            {
                m_startOrientation = m_creature->GetO();
                m_spoutCast = true;
                m_spout = urand(50000, 60000);
            }
        }
        else
            m_spout -= diff;

        if (m_geyser <= diff)
        {
            if (Unit* tar = m_creature->SelectAttackingTarget(
                    ATTACKING_TARGET_RANDOM, 0))
                if (DoCastSpellIfCan(tar, SPELL_GEYSER) == CAST_OK)
                    m_geyser = 10000;
        }
        else
            m_geyser -= diff;

        if (m_whirlCount < 2)
        {
            if (m_whirl <= diff)
            {
                if (DoCastSpellIfCan(m_creature, SPELL_WHIRL) == CAST_OK)
                {
                    m_whirl = urand(15000, 20000);
                    ++m_whirlCount;
                }
            }
            else
                m_whirl -= diff;
        }

        DoMeleeAttackIfReady();
    }
};

bool Lurker_DummyEffect(
    Unit* /*tar*/, uint32 id, SpellEffectIndex /*eff_id*/, Creature* c)
{
    if (id == SPELL_SPOUT_CAST)
    {
        if (boss_the_lurker_belowAI* ai =
                dynamic_cast<boss_the_lurker_belowAI*>(c->AI()))
            ai->SpoutCastCallback();
    }
    else if (id == SPELL_BIRTH)
    {
        if (boss_the_lurker_belowAI* ai =
                dynamic_cast<boss_the_lurker_belowAI*>(c->AI()))
            ai->BirthCallback();
    }
    return true;
}

bool Lurker_DummyAura(const Aura* aura, bool apply)
{
    if (apply && aura->GetId() == SPELL_SUBMERGE)
        if (Unit* c = aura->GetCaster())
            if (c->GetTypeId() == TYPEID_UNIT &&
                c->GetEntry() == NPC_LURKER_BELOW)
                if (boss_the_lurker_belowAI* ai =
                        dynamic_cast<boss_the_lurker_belowAI*>(
                            ((Creature*)c)->AI()))
                    ai->SubmergeCallback();
    return true;
}

CreatureAI* GetAI_boss_the_lurker_below(Creature* pCreature)
{
    return new boss_the_lurker_belowAI(pCreature);
}

bool GOUse_strange_pool(Player* p, GameObject* go)
{
    // ~1% chance to fish him up (gives ~5 minutes for 5 fishers)
    // guarantee catch for GM on
    if (!p || (urand(1, 100) > 100 && !p->isGameMaster()))
        return true;

    if (ScriptedInstance* inst = (ScriptedInstance*)go->GetInstanceData())
    {
        if (inst->GetData(TYPE_LURKER_BELOW) != IN_PROGRESS &&
            inst->GetData(TYPE_LURKER_BELOW) != DONE)
        {
            go->SummonCreature(NPC_LURKER_BELOW, 38.2f, -415.6f, -21.4f, 3.0f,
                TEMPSUMMON_MANUAL_DESPAWN, 0);
            inst->SetData(TYPE_LURKER_BELOW, IN_PROGRESS);
        }
    }

    return true;
}

void AddSC_boss_the_lurker_below()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "boss_the_lurker_below";
    pNewScript->GetAI = &GetAI_boss_the_lurker_below;
    pNewScript->pEffectDummyNPC = &Lurker_DummyEffect;
    pNewScript->pEffectAuraDummy = &Lurker_DummyAura;
    pNewScript->RegisterSelf();

    pNewScript->Name = "go_strange_pool";
    pNewScript->pGOUse = GOUse_strange_pool;
    pNewScript->RegisterSelf();
}
