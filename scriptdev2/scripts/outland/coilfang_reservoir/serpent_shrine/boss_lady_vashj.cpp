/* Copyright (C) 2013-2015 Corecraft <https://www.worldofcorecraft.com/>
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA02111-1307USA
 */

/* ScriptData
SDName: boss_lady_vashj
SD%Complete: 100
SDComment:
SDCategory: Coilfang Resevoir, Serpentshrine Cavern
EndScriptData */

#include "Spell.h"
#include "precompiled.h"
#include "serpentshrine_cavern.h"

enum
{
    SAY_INTRO = -1548042,
    SAY_AGGRO_1 = -1548043,
    SAY_AGGRO_2 = -1548044,
    SAY_AGGRO_3 = -1548045,
    SAY_AGGRO_4 = -1548046,
    SAY_AGGRO_5 = -1548047,
    SAY_PHASE_TWO = -1548048,
    SAY_PHASE_THREE = -1548049,
    SAY_ARCHERY_1 = -1548050,
    SAY_ARCHERY_2 = -1548051,
    SAY_KILL_1 = -1548052,
    SAY_KILL_2 = -1548053,
    SAY_KILL_3 = -1548054,
    SAY_DEATH = -1548055,

    // SPELL_SHOCK_BLAST = 38509, timers: [10-20, 10-20]
    // SPELL_SHOOT = 38295, timers: [0.5, 1-4]
    SPELL_MULTI_SHOT = 38310,
    // SPELL_STATIC_CHARGE = 38280, timers: [5-15, 5-15] Random
    // SPELL_ENTANGLE = 38316, timers: [10-20, 10-20] AOE
    SPELL_FORKED_LIGHTNING = 38145,
    SPELL_PERSUASION = 38511,

    SPELL_MAGIC_BARRIER = 38112,
    SPELL_PARALYZE = 38132, // While holding the core
    SPELL_REMOVE_TAINTED_CORES = 39495,
    SPELL_THROW_KEY = 38134,
    SPELL_OPENING = 3366,
    SPELL_USE_KEY = 38165,

    NPC_ENCHANTED_ELEMENTAL = 21958,
    NPC_TAINTED_ELEMENTAL = 22009,
    NPC_COILFANG_ELITE = 22055,
    NPC_COILFANG_STRIDER = 22056,
    // SPELL_SURGE = 38044,
    // SPELL_POISON_BOLT = 38253,

    PHASE_ONE = 1,
    PHASE_TWO = 2,
    PHASE_THREE = 3,
    PHASE_TRANSIT = 4,

    // Stairs are grouped. Each 10 second an elemental spawns per group
    // In total there are 4 stair complexes
    STAIR_COMPLEX_ONE_SIZE = 4,
    STAIR_COMPLEX_TWO_SIZE = 3,
    STAIR_COMPLEX_THREE_SIZE = 4,
    STAIR_COMPLEX_FOUR_SIZE = 6,
    STAIR_COMPLEX_TOTAL_SIZE = STAIR_COMPLEX_ONE_SIZE + STAIR_COMPLEX_TWO_SIZE +
                               STAIR_COMPLEX_THREE_SIZE +
                               STAIR_COMPLEX_FOUR_SIZE
};

static float stairSpawnPoints[STAIR_COMPLEX_TOTAL_SIZE][3] = {
    // Complex One
    {118.4f, -937.5f, 22.6f}, {111.8f, -959.7f, 23.0f},
    {104.5f, -976.0f, 22.3f}, {84.4f, -995.5f, 22.5f},
    // Complex Two
    {66.5f, -1006.0f, 22.6f}, {26.7f, -1015.1f, 22.6f},
    {-9.3f, -1005.3f, 22.7f},
    // Complex Three
    {-41.5f, -978.2f, 22.8f}, {-61.6f, -929.2f, 22.4f},
    {-58.4f, -905.7f, 22.4f}, {-51.9f, -882.9f, 22.6f},
    // Complex Four
    {-1.8f, -839.4f, 22.7f}, {18.4f, -834.1f, 22.7f}, {40.9f, -833.4f, 22.4f},
    {58.4f, -838.0f, 22.3f}, {81.8f, -848.8f, 22.5f}, {103.0f, -869.2f, 22.2f}};

struct MANGOS_DLL_DECL boss_lady_vashjAI : public Scripted_BehavioralAI
{
    boss_lady_vashjAI(Creature* pCreature) : Scripted_BehavioralAI(pCreature)
    {
        m_instance = (ScriptedInstance*)pCreature->GetInstanceData();
        Reset();
        m_doneIntro = false;
    }

    std::vector<ObjectGuid> m_spawns;
    ScriptedInstance* m_instance;
    uint32 m_phase;
    uint32 m_multiShot;
    uint32 m_lightning;
    uint32 m_enchEle;
    uint32 m_taintedEle;
    uint32 m_elite;
    uint32 m_strider;
    uint32 m_mindControl;
    uint32 m_sporebatFull;
    uint32 m_sporebat;
    uint32 m_lookAround;
    bool m_doneIntro;

    void Reset() override
    {
        m_phase = PHASE_ONE;
        m_multiShot = urand(5000, 15000);
        m_lightning = urand(3000, 8000);
        m_enchEle = 10000;
        m_taintedEle = urand(50000, 80000);
        m_elite = urand(40000, 50000);
        m_strider = urand(60000, 65000);
        m_mindControl = 15000;
        m_sporebat = m_sporebatFull = 30000;
        m_lookAround = 1000; // In phase 2
        m_creature->SetFocusTarget(nullptr);

        ToggleBehavioralAI(true);
        Scripted_BehavioralAI::Reset();
    }

    void Aggro(Unit* /*pWho*/) override
    {
        if (m_instance)
            m_instance->SetData(TYPE_LADY_VASHJ, IN_PROGRESS);

        switch (urand(0, 4))
        {
        case 0:
            DoScriptText(SAY_AGGRO_1, m_creature);
            break;
        case 1:
            DoScriptText(SAY_AGGRO_2, m_creature);
            break;
        case 2:
            DoScriptText(SAY_AGGRO_3, m_creature);
            break;
        case 3:
            DoScriptText(SAY_AGGRO_4, m_creature);
            break;
        case 4:
            DoScriptText(SAY_AGGRO_5, m_creature);
            break;
        }
    }

    void EnterEvadeMode(bool by_group = false) override
    {
        auto generators = GetCreatureListWithEntryInGrid(
            m_creature, NPC_SHIELD_GENERATOR, 200.0f);
        for (auto& generator : generators)
            generator->InterruptNonMeleeSpells(false);
        Scripted_BehavioralAI::EnterEvadeMode(by_group);
    }

    void JustReachedHome() override
    {
        DespawnSummons();
        if (m_instance)
            m_instance->SetData(TYPE_LADY_VASHJ, FAIL);
        // Note: Encounter resetting done in
        // instance_serpentshrine_cavern::ResetLadyVashjEncounter()
    }

    void JustDied(Unit* /*pKiller*/) override
    {
        if (m_instance)
            m_instance->SetData(TYPE_LADY_VASHJ, DONE);
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
                // Start Phase Two (but leave it at transit until we are
                // affected by magic barrier)
                m_creature->movement_gens.push(
                    new movement::StoppedMovementGenerator(),
                    movement::EVENT_LEAVE_COMBAT);
                auto generators = GetCreatureListWithEntryInGrid(
                    m_creature, NPC_SHIELD_GENERATOR, 50.0f);
                for (auto& generator : generators)
                {
                    generator->CastSpell(
                        m_creature, SPELL_MAGIC_BARRIER, false);
                }
            }
        }
    }

    void JustSummoned(Creature* c) override
    {
        m_spawns.push_back(c->GetObjectGuid());
    }

    void DespawnSummons()
    {
        for (auto& elem : m_spawns)
            if (Creature* c = m_creature->GetMap()->GetCreature(elem))
                c->Kill(c);
        m_spawns.clear();
    }

    void SpawnEnchEle()
    {
        int min = 0, max = STAIR_COMPLEX_ONE_SIZE;
        int index = urand(min, max - 1);
        m_creature->SummonCreature(NPC_ENCHANTED_ELEMENTAL,
            stairSpawnPoints[index][0], stairSpawnPoints[index][1],
            stairSpawnPoints[index][2], 0.0f, TEMPSUMMON_CORPSE_TIMED_DESPAWN,
            5000);
        min = max;
        max += STAIR_COMPLEX_TWO_SIZE;
        index = urand(min, max - 1);
        m_creature->SummonCreature(NPC_ENCHANTED_ELEMENTAL,
            stairSpawnPoints[index][0], stairSpawnPoints[index][1],
            stairSpawnPoints[index][2], 0.0f, TEMPSUMMON_CORPSE_TIMED_DESPAWN,
            5000);
        min = max;
        max += STAIR_COMPLEX_THREE_SIZE;
        index = urand(min, max - 1);
        m_creature->SummonCreature(NPC_ENCHANTED_ELEMENTAL,
            stairSpawnPoints[index][0], stairSpawnPoints[index][1],
            stairSpawnPoints[index][2], 0.0f, TEMPSUMMON_CORPSE_TIMED_DESPAWN,
            5000);
        min = max;
        max += STAIR_COMPLEX_FOUR_SIZE;
        index = urand(min, max - 1);
        m_creature->SummonCreature(NPC_ENCHANTED_ELEMENTAL,
            stairSpawnPoints[index][0], stairSpawnPoints[index][1],
            stairSpawnPoints[index][2], 0.0f, TEMPSUMMON_CORPSE_TIMED_DESPAWN,
            5000);
    }

    void RandomSpawn(uint32 entry)
    {
        int index = urand(0, STAIR_COMPLEX_TOTAL_SIZE - 1);
        m_creature->SummonCreature(entry, stairSpawnPoints[index][0],
            stairSpawnPoints[index][1], stairSpawnPoints[index][2], 0.0f,
            TEMPSUMMON_CORPSE_TIMED_DESPAWN,
            entry == NPC_TAINTED_ELEMENTAL ? 30000 : 5000);
    }

    void SpawnToxicSporebat(); // Implemented below

    void SpellHit(Unit* /*pCaster*/, const SpellEntry* pSpell) override
    {
        if (m_phase == PHASE_TRANSIT && pSpell->Id == SPELL_MAGIC_BARRIER)
        {
            m_phase = PHASE_TWO;
            DoScriptText(SAY_PHASE_TWO, m_creature);
            // Toggle Shield Generators usable
            if (m_instance)
                if (instance_serpentshrine_cavern* inst =
                        dynamic_cast<instance_serpentshrine_cavern*>(
                            m_instance))
                    inst->ToggleShieldGenerators(true);
        }
    }

    void UpdateAI(const uint32 diff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
        {
            if (!m_doneIntro && GetClosestPlayer(m_creature, 180.0f) != nullptr)
            {
                DoScriptText(SAY_INTRO, m_creature);
                m_doneIntro = true;
            }
            return;
        }

        // Phase Switching
        if (m_phase == PHASE_ONE && m_creature->GetHealthPercent() < 71.0f)
        {
            ToggleBehavioralAI(false);
            m_creature->movement_gens.remove_all(movement::gen::stopped);
            m_creature->movement_gens.push(
                new movement::PointMovementGenerator(
                    100, 29.7f, -923.2f, 42.9f, true, true),
                movement::EVENT_LEAVE_COMBAT,
                movement::get_default_priority(movement::gen::stopped) + 1);
            m_phase = PHASE_TRANSIT;
        }

        if (m_phase == PHASE_ONE || m_phase == PHASE_THREE)
        {
            // Phase Three has an added mind control & sporebats
            if (m_phase == PHASE_THREE)
            {
                if (m_mindControl <= diff)
                {
                    if (DoCastSpellIfCan(m_creature, SPELL_PERSUASION) ==
                        CAST_OK)
                        m_mindControl = urand(20000, 30000);
                }
                else
                    m_mindControl -= diff;

                if (m_sporebat <= diff)
                {
                    // Cap sporebats at maximum 50 currently alive
                    if (m_instance &&
                        m_instance->GetData(TYPE_SPOREBAT_COUNT) < 50)
                    {
                        SpawnToxicSporebat();
                        if (m_sporebatFull >= 3000)
                            m_sporebatFull -= 2000;
                        else
                            m_sporebatFull = 1000;
                        m_sporebat = m_sporebatFull;
                    }
                }
                else
                    m_sporebat -= diff;
            }

            // Multi-Shot
            if (m_multiShot <= diff)
            {
                if (Unit* t = m_creature->SelectAttackingTarget(
                        ATTACKING_TARGET_RANDOM, 1, SPELL_MULTI_SHOT))
                    if (DoCastSpellIfCan(t, SPELL_MULTI_SHOT) == CAST_OK)
                    {
                        if (urand(1, 5) == 1)
                            DoScriptText(
                                urand(0, 1) ? SAY_ARCHERY_1 : SAY_ARCHERY_2,
                                m_creature);
                        m_multiShot = urand(5000, 15000);
                    }
            }
            else
                m_multiShot -= diff;
        }
        else if (m_phase == PHASE_TWO)
        {
            if (!m_creature->has_aura(SPELL_MAGIC_BARRIER))
            {
                m_phase = PHASE_THREE;
                DoScriptText(SAY_PHASE_THREE, m_creature);
                m_creature->SetFocusTarget(nullptr);
                DoResetThreat();
                // Toggle shield generators unusable
                if (m_instance)
                    if (instance_serpentshrine_cavern* inst =
                            dynamic_cast<instance_serpentshrine_cavern*>(
                                m_instance))
                        inst->ToggleShieldGenerators(false);
                m_creature->movement_gens.remove_all(movement::gen::stopped);
                ToggleBehavioralAI(true);
                return;
            }

            if (m_lookAround <= diff)
            {
                if (!m_creature->IsNonMeleeSpellCasted(false))
                {
                    if (Unit* t = m_creature->SelectAttackingTarget(
                            ATTACKING_TARGET_RANDOM, 0))
                        m_creature->SetFocusTarget(t);
                    m_lookAround = 1000;
                }
            }
            else
                m_lookAround -= diff;

            // Phase Two
            if (m_lightning <= diff)
            {
                if (Unit* tar = m_creature->SelectAttackingTarget(
                        ATTACKING_TARGET_RANDOM, 0))
                    if (DoCastSpellIfCan(tar, SPELL_FORKED_LIGHTNING) ==
                        CAST_OK)
                        m_lightning = urand(3000, 8000);
            }
            else
                m_lightning -= diff;

            // Elemental Spawn
            if (m_enchEle <= diff)
            {
                SpawnEnchEle();
                m_enchEle = 10000;
            }
            else
                m_enchEle -= diff;

            // Random Position Spawns

            if (m_taintedEle <= diff)
            {
                RandomSpawn(NPC_TAINTED_ELEMENTAL);
                m_taintedEle = urand(50000, 80000);
            }
            else
                m_taintedEle -= diff;

            if (m_elite <= diff)
            {
                RandomSpawn(NPC_COILFANG_ELITE);
                m_elite = urand(40000, 50000);
            }
            else
                m_elite -= diff;

            if (m_strider <= diff)
            {
                RandomSpawn(NPC_COILFANG_STRIDER);
                m_strider = urand(60000, 65000);
            }
            else
                m_strider -= diff;
        }

        Scripted_BehavioralAI::UpdateInCombatAI(diff);

        if (m_phase == PHASE_ONE || m_phase == PHASE_THREE)
            DoMeleeAttackIfReady();
    }
};

#define SPORE_Z 74.0f

#define SPORE_INTRO_SIZE 3
static float sporeIntroOne[SPORE_INTRO_SIZE][3] = {{-13.3f, -762.4f, 128.6f},
    {21.5f, -804.3f, SPORE_Z}, {25.9f, -917.0f, SPORE_Z}};
static float sporeIntroTwo[SPORE_INTRO_SIZE][3] = {{-29.3f, -761.9f, 95.0f},
    {21.5f, -804.3f, SPORE_Z}, {25.9f, -917.0f, SPORE_Z}};
static float sporeIntroThree[SPORE_INTRO_SIZE][3] = {{51.0f, -742.4f, 133.5f},
    {30.2f, -805.4f, SPORE_Z}, {35.9f, -917.7f, SPORE_Z}};
static float sporeIntroFour[SPORE_INTRO_SIZE][3] = {{84.5f, -743.9f, 102.0f},
    {30.2f, -805.4f, SPORE_Z}, {35.9f, -917.7f, SPORE_Z}};

#define SPORE_SPLINE_ONE_ID 11
#define SPORE_SPLINE_TWO_ID 12

void boss_lady_vashjAI::SpawnToxicSporebat()
{
    switch (urand(0, 3))
    {
    case 0:
        if (Creature* c = m_creature->SummonCreature(NPC_TOXIC_SPOREBAT,
                sporeIntroOne[0][0], sporeIntroOne[0][1], sporeIntroOne[0][2],
                0.0f, TEMPSUMMON_DEAD_DESPAWN, 0,
                SUMMON_OPT_ACTIVE | SUMMON_OPT_DESPAWN_ON_SUMMONER_DEATH))
            c->movement_gens.push(
                new movement::PointMovementGenerator(100, sporeIntroOne[1][0],
                    sporeIntroOne[1][1], sporeIntroOne[1][2], false, true));
        break;
    case 1:
        if (Creature* c = m_creature->SummonCreature(NPC_TOXIC_SPOREBAT,
                sporeIntroTwo[0][0], sporeIntroTwo[0][1], sporeIntroTwo[0][2],
                0.0f, TEMPSUMMON_DEAD_DESPAWN, 0,
                SUMMON_OPT_ACTIVE | SUMMON_OPT_DESPAWN_ON_SUMMONER_DEATH))
            c->movement_gens.push(
                new movement::PointMovementGenerator(100, sporeIntroTwo[1][0],
                    sporeIntroTwo[1][1], sporeIntroTwo[1][2], false, true));
        break;
    case 2:
        if (Creature* c = m_creature->SummonCreature(NPC_TOXIC_SPOREBAT,
                sporeIntroThree[0][0], sporeIntroThree[0][1],
                sporeIntroThree[0][2], 0.0f, TEMPSUMMON_DEAD_DESPAWN, 0,
                SUMMON_OPT_ACTIVE | SUMMON_OPT_DESPAWN_ON_SUMMONER_DEATH))
            c->movement_gens.push(
                new movement::PointMovementGenerator(200, sporeIntroThree[1][0],
                    sporeIntroThree[1][1], sporeIntroThree[1][2], false, true));
        break;
    case 3:
        if (Creature* c = m_creature->SummonCreature(NPC_TOXIC_SPOREBAT,
                sporeIntroFour[0][0], sporeIntroFour[0][1],
                sporeIntroFour[0][2], 0.0f, TEMPSUMMON_DEAD_DESPAWN, 0,
                SUMMON_OPT_ACTIVE | SUMMON_OPT_DESPAWN_ON_SUMMONER_DEATH))
            c->movement_gens.push(
                new movement::PointMovementGenerator(200, sporeIntroFour[1][0],
                    sporeIntroFour[1][1], sporeIntroFour[1][2], false, true));
        break;
    }
}

struct MANGOS_DLL_DECL mob_toxic_sporebatAI : public ScriptedAI
{
    mob_toxic_sporebatAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        Reset();
    }

    void MovementInform(movement::gen gen_type, uint32 uiData) override
    {
        if (gen_type == movement::gen::point)
        {
            if (uiData == 100)
                m_creature->movement_gens.push(
                    new movement::PointMovementGenerator(300,
                        sporeIntroOne[2][0], sporeIntroOne[2][1],
                        sporeIntroOne[2][2], false, true));
            else if (uiData == 200)
                m_creature->movement_gens.push(
                    new movement::PointMovementGenerator(400,
                        sporeIntroThree[2][0], sporeIntroThree[2][1],
                        sporeIntroThree[2][2], false, true));
            else if (uiData == 300)
            {
                m_creature->movement_gens.push(
                    new movement::SplineMovementGenerator(
                        SPORE_SPLINE_ONE_ID, true));
            }
            else if (uiData == 400)
            {
                m_creature->movement_gens.push(
                    new movement::SplineMovementGenerator(
                        SPORE_SPLINE_TWO_ID, true));
            }
        }
    }

    void AttackStart(Unit* /*pWho*/) override {}

    void UpdateAI(const uint32 /*diff*/) override {}
};

CreatureAI* GetAI_boss_lady_vashj(Creature* pCreature)
{
    return new boss_lady_vashjAI(pCreature);
}

CreatureAI* GetAI_toxic_sporebat(Creature* pCreature)
{
    return new mob_toxic_sporebatAI(pCreature);
}

bool ItemUse_TaintedCore(Player* p, Item* item, const SpellCastTargets& tar)
{
    if (GameObject* go = tar.getGOTarget())
    {
        if (p->HasItemCount(ITEM_TAINTED_CORE, 1))
        {
            if (Creature* c = GetClosestCreatureWithEntry(
                    go, NPC_SHIELD_GENERATOR, 15.0f, true))
            {
                if (c->IsNonMeleeSpellCasted(false))
                {
                    c->InterruptNonMeleeSpells(false);
                    if (p->GetInstanceData())
                        if (ScriptedInstance* m_instance =
                                dynamic_cast<ScriptedInstance*>(
                                    p->GetInstanceData()))
                            if (Creature* vashj =
                                    m_instance->GetSingleCreatureFromStorage(
                                        NPC_LADY_VASHJ))
                                vashj->DealDamage(vashj,
                                    vashj->GetMaxHealth() * 0.05f, nullptr,
                                    DIRECT_DAMAGE, SPELL_SCHOOL_MASK_NONE,
                                    nullptr, false, false);
                }
            }
        }

        if (auto info = sSpellStore.LookupEntry(SPELL_USE_KEY))
        {
            auto spell = new Spell(p, info, false);
            spell->set_cast_item(item);
            spell->m_cast_count = 1;
            spell->prepare(&tar);
        }
        return true;
    }
    else if (tar.getUnitTarget() &&
             tar.getUnitTarget()->GetTypeId() == TYPEID_PLAYER)
    {
        p->CastSpell(tar.getUnitTarget(), SPELL_THROW_KEY, false, item);
        return true;
    }

    return true;
}

void AddSC_boss_lady_vashj()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "boss_lady_vashj";
    pNewScript->GetAI = &GetAI_boss_lady_vashj;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "item_tainted_core";
    pNewScript->pItemUse = &ItemUse_TaintedCore;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "mob_toxic_sporebat";
    pNewScript->GetAI = &GetAI_toxic_sporebat;
    pNewScript->RegisterSelf();
}
