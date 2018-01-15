/* Copyright (C) 2012-2013 CoreCraft */

/* ScriptData
SDName: Boss_Nightbane
SD%Complete: 100
SDComment:
SDCategory: Karazhan
EndScriptData */

// NOTE ABOUT RESTLESS SKELETONS:
// We do not include the immunities to all but holy
// attacks.
// Reason: This was added in patch 2.0.10 (6th march
// 2007)
// and then removed in a hotfix 3 days laters (9th march
// 2007)

#include "karazhan.h"
#include "precompiled.h"

enum
{
    SAY_AWAKEN_EMOTE = -1512001,
    SAY_AGGRO = -1512002,
    SAY_AIR_PHASE = -1512003,
    SAY_GROUND_PHASE_1 = -1512004,
    SAY_GROUND_PHASE_2 = -1512005,
    SAY_FEAR_EMOTE = -1512006,
    SAY_AIR_PHASE_OOR = -1512007,
    SAY_DEEP_BREATH_EMOTE = -1512008,

    // PHASE 1:
    SPELL_BELLOWING_ROAR = 36922,
    SPELL_CLEAVE = 30131,
    SPELL_SMOLDERING_BREATH = 30210,
    SPELL_CHARRED_EARTH = 30129,
    SPELL_DISTRACTING_ASH_REAL = 30130,
    SPELL_DISTRACTING_ASH_GRND = 30280,
    SPELL_TAIL_SWEEP = 25653,

    // PHASE 2:
    SPELL_RAIN_OF_BONES = 37098,
    SPELL_SMOKING_BLAST = 30128,
    NPC_RESTLESS_SKELETON = 17261,
    SPELL_FIREBALL_BARRAGE = 30282,

    // Flight splines
    SPLINE_FLY_IN_ID = 13,
    SPLINE_P2_LAND_ID = 14,
};

enum FlightType
{
    FLIGHT_TYPE_NONE = 0,
    FLIGHT_TYPE_INTRO,
    FLIGHT_TYPE_PHASE_TWO,
    FLIGHT_TYPE_LAND_PHASE_TWO
};

const float IntroLand[3] = {-11143.5f, -1894.3f, 91.4738f};

const float PhaseTwoStand[4] = {
    -11165.3f, -1870.9f, 102.4f, 5.4f}; // Stands here and shoots at the raid

struct MANGOS_DLL_DECL boss_nightbaneAI : public ScriptedAI
{
    boss_nightbaneAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_instance = pCreature->GetMapId() == KARAZHAN_MAP_ID ?
                         (instance_karazhan*)pCreature->GetInstanceData() :
                         NULL;
        if (!m_instance)
        {
            m_creature->ForcedDespawn();
            return;
        }

        if (m_instance->GetData(TYPE_NIGHTBANE) == IN_PROGRESS ||
            m_instance->GetData(TYPE_NIGHTBANE) == DONE)
        {
            m_creature->ForcedDespawn();
            return;
        }
        m_instance->SetData(TYPE_NIGHTBANE, IN_PROGRESS);

        if (Creature* helper = m_instance->GetSingleCreatureFromStorage(
                NPC_NIGHTBANE_HELPER_TARGET))
            DoScriptText(SAY_AWAKEN_EMOTE, helper);

        m_hasIntroFlight = false;
        Reset();
    }

    instance_karazhan* m_instance;
    FlightType m_flightType;
    bool m_hasIntroFlight;
    uint32 m_phase;
    int32 m_lastPhaseSwitchPct;
    uint32 m_phaseTwoEndTimer;
    uint32 m_removePacifyTimer;
    std::vector<ObjectGuid> m_skeletons;
    // Phase One:
    uint32 m_fearTimer;
    uint32 m_cleaveTimer;
    uint32 m_smolderingBreathTimer;
    uint32 m_charredEarthTimer;
    uint32 m_tailSweepTimer;
    // Phase Two:
    bool m_rainOfBones;
    uint32 m_distractingAshTimer;
    uint32 m_smokingBlastTimer;
    uint32 m_RoBSummonCount;
    uint32 m_RoBSummonTimer;
    uint32 m_fireballBarrageTimer;
    bool m_saidFireballBarrage;
    ObjectGuid m_RoBBombardedTarget;
    G3D::Vector3 m_RoBPos;

    void Aggro(Unit* /*pWho*/) override { DoScriptText(SAY_AGGRO, m_creature); }

    void JustDied(Unit* /*pKiller*/) override
    {
        if (m_instance)
            m_instance->SetData(TYPE_NIGHTBANE, DONE);
        DespawnSkeletons();
    }

    void JustReachedHome() override
    {
        m_creature->ForcedDespawn();
        if (m_instance)
            m_instance->SetData(TYPE_NIGHTBANE, FAIL);
        DespawnSkeletons();
    }

    void JustSummoned(Creature* c) override
    {
        if (c->GetEntry() == NPC_RESTLESS_SKELETON)
            m_skeletons.push_back(c->GetObjectGuid());
    }

    void DespawnSkeletons()
    {
        for (auto& elem : m_skeletons)
            if (Creature* c = m_creature->GetMap()->GetCreature(elem))
                c->ForcedDespawn();
        m_skeletons.clear();
    }

    void UpdateSkeletons()
    {
        for (std::vector<ObjectGuid>::iterator itr = m_skeletons.begin();
             itr != m_skeletons.end();)
        {
            if (Creature* c = m_creature->GetMap()->GetCreature(*itr))
            {
                if (c->isAlive())
                    ++itr;
                else
                    itr = m_skeletons.erase(itr);
            }
            else
                itr = m_skeletons.erase(itr);
        }
    }

    void Reset() override
    {
        m_phase = 1;
        m_flightType = FLIGHT_TYPE_NONE;
        m_lastPhaseSwitchPct = 100;
        m_phaseTwoEndTimer = 0;
        m_removePacifyTimer = 0;
        // Both Phases:
        m_distractingAshTimer = urand(5000, 15000);
        // Phase One:
        m_fearTimer = urand(30000, 60000);
        m_cleaveTimer = urand(10000, 15000);
        m_smolderingBreathTimer = urand(30000, 45000);
        m_charredEarthTimer = urand(20000, 30000);
        m_tailSweepTimer = urand(15000, 30000);
        // Phase Two:
        m_rainOfBones = false;
        m_smokingBlastTimer = 1000;
        m_RoBSummonCount = 0;
        m_RoBSummonTimer = 0;
        m_fireballBarrageTimer = 1500;
        m_saidFireballBarrage = false;
    }

    void MovementInform(movement::gen type, uint32 id) override
    {
        if (m_flightType == FLIGHT_TYPE_INTRO && type == movement::gen::spline)
        {
            m_creature->m_movementInfo.AddMovementFlag(MOVEFLAG_FLYING);
            m_creature->movement_gens.push(
                new movement::PointMovementGenerator(
                    10, IntroLand[0], IntroLand[1], IntroLand[2], false, true),
                0, movement::get_default_priority(movement::gen::stopped) + 1);
        }
        else if (m_flightType == FLIGHT_TYPE_INTRO &&
                 type == movement::gen::point && id == 10)
        {
            m_creature->m_movementInfo.RemoveMovementFlag(MOVEFLAG_FLYING);
            m_creature->HandleEmoteCommand(EMOTE_ONESHOT_LAND);
            m_removePacifyTimer = 2000;
            m_creature->RemoveFlag(
                UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_PASSIVE);
            m_creature->SetInCombatWithZone();
            m_flightType = FLIGHT_TYPE_NONE;
        }
        else if (m_flightType == FLIGHT_TYPE_PHASE_TWO &&
                 type == movement::gen::point && id == 100)
        {
            m_creature->m_movementInfo.AddMovementFlag(MOVEFLAG_LEVITATING);
            m_flightType = FLIGHT_TYPE_NONE;
        }
        else if (m_flightType == FLIGHT_TYPE_LAND_PHASE_TWO &&
                 type == movement::gen::spline)
        {
            m_creature->HandleEmote(EMOTE_ONESHOT_LAND);
            m_removePacifyTimer = 2000;
            m_flightType = FLIGHT_TYPE_NONE;
        }
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_hasIntroFlight)
        {
            m_creature->UpdateSpeed(MOVE_RUN, false, 1.5f);
            m_creature->SetFlag(
                UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_PASSIVE);
            m_creature->movement_gens.push(
                new movement::StoppedMovementGenerator,
                movement::EVENT_LEAVE_COMBAT);
            m_creature->movement_gens.push(
                new movement::SplineMovementGenerator(SPLINE_FLY_IN_ID));
            Pacify(true);
            m_flightType = FLIGHT_TYPE_INTRO;
            m_hasIntroFlight = true;
        }

        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        if (m_removePacifyTimer)
        {
            if (m_removePacifyTimer <= uiDiff)
            {
                m_creature->UpdateSpeed(MOVE_RUN, false, 1.0f);
                Pacify(false);
                m_creature->movement_gens.remove_all(movement::gen::stopped);
                m_removePacifyTimer = 0;
            }
            else
                m_removePacifyTimer -= uiDiff;
            return;
        }

        if (m_flightType != FLIGHT_TYPE_NONE)
            return;

        // Fear timer keeps updating in the air pre patch 2.1
        if (m_fearTimer <= uiDiff)
            m_fearTimer = 0;
        else
            m_fearTimer -= uiDiff;

        if (m_phase == 1)
        {
            if ((m_creature->GetHealthPercent() < 76 &&
                    m_lastPhaseSwitchPct > 75) ||
                (m_creature->GetHealthPercent() < 51 &&
                    m_lastPhaseSwitchPct > 50) ||
                (m_creature->GetHealthPercent() < 26 &&
                    m_lastPhaseSwitchPct > 25))
            {
                // Enter Phase 2
                m_phase = 2;
                m_skeletons.clear();
                DoResetThreat();
                m_flightType = FLIGHT_TYPE_PHASE_TWO;
                m_creature->HandleEmoteCommand(EMOTE_ONESHOT_LIFTOFF);
                m_creature->m_movementInfo.AddMovementFlag(MOVEFLAG_FLYING);
                m_creature->movement_gens.push(
                    new movement::StoppedMovementGenerator,
                    movement::EVENT_LEAVE_COMBAT);
                m_creature->movement_gens.push(
                    new movement::PointMovementGenerator(100, PhaseTwoStand[0],
                        PhaseTwoStand[1], PhaseTwoStand[2], false, true),
                    0,
                    movement::get_default_priority(movement::gen::stopped) + 1);
                DoScriptText(SAY_AIR_PHASE, m_creature);
                m_rainOfBones = false;
                m_fireballBarrageTimer = 1500;
                m_saidFireballBarrage = false;
                m_phaseTwoEndTimer = 57 * 1000;
                m_lastPhaseSwitchPct -= 25;
                return; // Do not do more combat logic
            }

            /* Combat Logic */
            if (m_fearTimer == 0 &&
                m_creature->CanReachWithMeleeAttack(m_creature->getVictim()))
            {
                if (DoCastSpellIfCan(m_creature, SPELL_BELLOWING_ROAR) ==
                    CAST_OK)
                    m_fearTimer = urand(30000, 60000);
            }

            if (m_cleaveTimer <= uiDiff)
            {
                if (DoCastSpellIfCan(m_creature->getVictim(), SPELL_CLEAVE) ==
                    CAST_OK)
                    m_cleaveTimer = urand(10000, 15000);
            }
            else
                m_cleaveTimer -= uiDiff;

            if (m_smolderingBreathTimer <= uiDiff)
            {
                if (DoCastSpellIfCan(m_creature, SPELL_SMOLDERING_BREATH) ==
                    CAST_OK)
                    m_smolderingBreathTimer = urand(30000, 45000);
            }
            else
                m_smolderingBreathTimer -= uiDiff;

            if (m_charredEarthTimer <= uiDiff)
            {
                if (Unit* target = m_creature->SelectAttackingTarget(
                        ATTACKING_TARGET_RANDOM, 0))
                {
                    if (DoCastSpellIfCan(target, SPELL_CHARRED_EARTH) ==
                        CAST_OK)
                        m_charredEarthTimer = urand(20000, 30000);
                }
            }
            else
                m_charredEarthTimer -= uiDiff;

            if (m_tailSweepTimer <= uiDiff)
            {
                if (DoCastSpellIfCan(m_creature, SPELL_TAIL_SWEEP) == CAST_OK)
                    m_tailSweepTimer = urand(15000, 30000);
            }
            else
                m_tailSweepTimer -= uiDiff;

            if (m_distractingAshTimer <= uiDiff)
            {
                Unit* target = m_creature->getVictim()->has_aura(
                                   SPELL_DISTRACTING_ASH_REAL) ?
                                   m_creature->SelectAttackingTarget(
                                       ATTACKING_TARGET_RANDOM, 1) :
                                   m_creature->getVictim();
                if (target)
                {
                    if (DoCastSpellIfCan(target, SPELL_DISTRACTING_ASH_GRND) ==
                        CAST_OK)
                        m_distractingAshTimer = urand(5000, 15000);
                }
            }
            else
                m_distractingAshTimer -= uiDiff;

            DoMeleeAttackIfReady();
        }
        else
        {
            if (m_phaseTwoEndTimer <= uiDiff)
                m_phaseTwoEndTimer = 0;
            else
                m_phaseTwoEndTimer -= uiDiff;

            if (m_rainOfBones && !m_RoBSummonTimer)
            {
                UpdateSkeletons();
                // If all skeletons are dead, or if the timer has run out we
                // enter phase 1
                if (m_skeletons.empty() || !m_phaseTwoEndTimer)
                {
                    // Enter Phase 1
                    m_phase = 1;
                    m_flightType = FLIGHT_TYPE_LAND_PHASE_TWO;
                    DoResetThreat();
                    m_creature->m_movementInfo.RemoveMovementFlag(
                        MOVEFLAG_LEVITATING);
                    m_creature->movement_gens.push(
                        new movement::SplineMovementGenerator(
                            SPLINE_P2_LAND_ID));
                    Pacify(true);
                    m_creature->m_movementInfo.RemoveMovementFlag(
                        MOVEFLAG_FLYING);
                    DoScriptText(
                        urand(0, 1) ? SAY_GROUND_PHASE_1 : SAY_GROUND_PHASE_2,
                        m_creature);
                    return; // Do not do more combat logic
                }
            }

            /* Combat Logic */
            if (!m_rainOfBones)
            {
                if (Unit* target = m_creature->SelectAttackingTarget(
                        ATTACKING_TARGET_RANDOM, 0))
                {
                    if (DoCastSpellIfCan(target, SPELL_RAIN_OF_BONES) ==
                        CAST_OK)
                    {
                        DoScriptText(SAY_DEEP_BREATH_EMOTE, m_creature);

                        m_rainOfBones = true;
                        m_RoBSummonTimer = 2000;
                        m_RoBSummonCount = 0;
                        m_RoBBombardedTarget = target->GetObjectGuid();
                        target->GetPosition(m_RoBPos.x, m_RoBPos.y, m_RoBPos.z);
                    }
                }
            }

            if (m_RoBSummonTimer)
            {
                if (m_RoBSummonTimer <= uiDiff)
                {
                    Player* plr =
                        m_creature->GetMap()->GetPlayer(m_RoBBombardedTarget);

                    // Keep renewing position; rain of bones tracks target
                    if (plr)
                        plr->GetPosition(m_RoBPos.x, m_RoBPos.y, m_RoBPos.z);

                    auto pos = m_creature->GetPointXYZ(
                        m_RoBPos, frand(0, 2 * M_PI_F), frand(0, 8), true);

                    m_creature->SummonCreature(NPC_RESTLESS_SKELETON, pos.x,
                        pos.y, pos.z, frand(0, 6),
                        TEMPSUMMON_CORPSE_TIMED_DESPAWN, 5000);
                    m_RoBSummonTimer = 2000;
                    ++m_RoBSummonCount;
                    if (m_RoBSummonCount >= 5)
                        m_RoBSummonTimer = 0;
                }
                else
                    m_RoBSummonTimer -= uiDiff;
            }

            if (m_distractingAshTimer <= uiDiff)
            {
                Unit* target = m_creature->getVictim()->has_aura(
                                   SPELL_DISTRACTING_ASH_REAL) ?
                                   m_creature->SelectAttackingTarget(
                                       ATTACKING_TARGET_RANDOM, 1) :
                                   m_creature->getVictim();
                if (target)
                {
                    if (DoCastSpellIfCan(target, SPELL_DISTRACTING_ASH_REAL) ==
                        CAST_OK)
                        m_distractingAshTimer = urand(5000, 15000);
                }
            }
            else
                m_distractingAshTimer -= uiDiff;

            if (m_fireballBarrageTimer <= uiDiff)
            {
                if (m_creature->getVictim()->GetDistance(m_creature) >= 80.0f)
                {
                    if (DoCastSpellIfCan(m_creature->getVictim(),
                            SPELL_FIREBALL_BARRAGE) == CAST_OK)
                    {
                        if (!m_saidFireballBarrage)
                        {
                            DoScriptText(SAY_AIR_PHASE_OOR, m_creature);
                            m_saidFireballBarrage = true;
                        }
                        m_fireballBarrageTimer = 1500;
                    }
                }
            }
            else
                m_fireballBarrageTimer -= uiDiff;

            if (m_smokingBlastTimer <= uiDiff)
            {
                if (DoCastSpellIfCan(m_creature->getVictim(),
                        SPELL_SMOKING_BLAST) == CAST_OK)
                    m_smokingBlastTimer = 1000;
            }
            else
                m_smokingBlastTimer -= uiDiff;
        }
    }
};

CreatureAI* GetAI_boss_nightbane(Creature* pCreature)
{
    return new boss_nightbaneAI(pCreature);
}

void AddSC_boss_nightbane()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "boss_nightbane";
    pNewScript->GetAI = &GetAI_boss_nightbane;
    pNewScript->RegisterSelf();
}
