/* Copyright (C) 2012 Corecraft */

/* ScriptData
SDName: Boss_Commander_Sarannis
SD%Complete: 100
SDComment:
SDCategory: Tempest Keep, The Botanica
EndScriptData */

#include "precompiled.h"
#include <vector>

enum
{
    SAY_AGGRO = -1501000,
    SAY_ARCANE_RESONANCE = -1501001,
    SAY_ARCANE_DEVASTATION = -1501002,
    SAY_SUMMON = -1501003,
    SAY_KILL_1 = -1501004,
    SAY_KILL_2 = -1501005,
    SAY_DEATH = -1501006,
    SAY_INTRO = -1501007,

    SPELL_ARCANE_RESONANCE = 34794,
    SPELL_ARCANE_DEVASTATION = 34799,
    SPELL_DOUBLE_ATTACK = 19818,

    NPC_BLOODWARDER_MENDER = 19633,
    NPC_BLOODWARDER_RESERVIST = 20078,

    NPC_BLOODWARDER_STEWARD = 18404,
    NPC_BLOODWARDER_PROTECTOR = 17993,

    SPELL_SIMPLE_TELEPORT = 12980,
};

struct IntroPos
{
    float X, Y, Z, O;
};

const IntroPos IntroMobsProtector[] = {
    // Salute one
    {151.958, 282.052, -4.4129, 1.59048}, // protector
    {154.023, 282.03, -4.09486, 1.86144}, // protector

    // Two
    {161.421, 285.544, -3.15519, 2.3515},   // protector
    {162.71, 286.907, -3.182072, 2.895766}, // protector

    // Three
    {164.693, 293.305, -4.18703, 2.77797}, // protector
    {165.081, 295.767, -4.50304, 2.96803}, // protector
};

const IntroPos IntroMobsSteward[] = {
    // Salute one
    {150.187, 281.684, -4.62663, 1.54336}, // steward

    // Two
    {160.296, 284.481, -3.17738, 2.20461}, // steward

    // Three
    {165.003, 298.302, -4.90127, 3.61206}, // Steward
};

const IntroPos ChillPoint = {150.7, 296.2, -4.56, 5.5};
const IntroPos MidPointForth = {136.3, 310.6, -1.6, 5.5};
const IntroPos MidPointBack = {136.3, 310.6, -1.6, 2.4};

const IntroPos SalutePointOne = {162.3, 295.8, -4.9, 6.2};
const IntroPos SalutePointTwo = {159.9, 287.3, -3.99, 5.4};
const IntroPos SalutePointThree = {152.5, 285.3, -4.82, 4.57};

const IntroPos CheerPoint = {156.0, 291.0, -4.87, 5.5};

const IntroPos HomePoint = {120.012, 327.677, -4.99033, 5.47973};

struct MANGOS_DLL_DECL boss_commander_sarannisAI : public ScriptedAI
{
    boss_commander_sarannisAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_bIsRegularMode = pCreature->GetMap()->IsRegularDifficulty();

        int32 grp_id[3];
        grp_id[0] = m_creature->GetMap()->GetCreatureGroupMgr().CreateNewGroup(
            "Sarannis Group One", true);
        grp_id[1] = m_creature->GetMap()->GetCreatureGroupMgr().CreateNewGroup(
            "Sarannis Group Two", true);
        grp_id[2] = m_creature->GetMap()->GetCreatureGroupMgr().CreateNewGroup(
            "Sarannis Group Three", true);
        for (auto& elem : grp_id)
            if (auto grp =
                    m_creature->GetMap()->GetCreatureGroupMgr().GetGroup(elem))
                grp->AddFlag(CREATURE_GROUP_FLAG_CANNOT_ASSIST_OTHER_GRPS);

        DoSummon(NPC_BLOODWARDER_STEWARD, IntroMobsSteward[0], m_SaluteListOne,
            grp_id[0]);
        DoSummon(NPC_BLOODWARDER_STEWARD, IntroMobsSteward[1], m_SaluteListTwo,
            grp_id[1]);
        DoSummon(NPC_BLOODWARDER_STEWARD, IntroMobsSteward[2],
            m_SaluteListThree, grp_id[2]);

        DoSummon(NPC_BLOODWARDER_PROTECTOR, IntroMobsProtector[0],
            m_SaluteListOne, grp_id[0]);
        DoSummon(NPC_BLOODWARDER_PROTECTOR, IntroMobsProtector[1],
            m_SaluteListOne, grp_id[0]);
        DoSummon(NPC_BLOODWARDER_PROTECTOR, IntroMobsProtector[2],
            m_SaluteListTwo, grp_id[1]);
        DoSummon(NPC_BLOODWARDER_PROTECTOR, IntroMobsProtector[3],
            m_SaluteListTwo, grp_id[1]);
        DoSummon(NPC_BLOODWARDER_PROTECTOR, IntroMobsProtector[4],
            m_SaluteListThree, grp_id[2]);
        DoSummon(NPC_BLOODWARDER_PROTECTOR, IntroMobsProtector[5],
            m_SaluteListThree, grp_id[2]);

        Reset();
    }

    void DoSummon(
        uint32 mobid, IntroPos pos, std::vector<ObjectGuid>& list, int32 grp_id)
    {
        Creature* pCreature = m_creature->SummonCreature(mobid, pos.X, pos.Y,
            pos.Z, pos.O, TEMPSUMMON_CORPSE_TIMED_DESPAWN, 2 * 60 * 1000);
        if (pCreature)
        {
            list.push_back(pCreature->GetObjectGuid());
            if (CreatureGroup* pGroup =
                    m_creature->GetMap()->GetCreatureGroupMgr().GetGroup(
                        grp_id))
                pGroup->AddMember(pCreature, false);
        }
    }

    void Aggro(Unit* who) override
    {
        DoScriptText(SAY_AGGRO, m_creature);

        // Pull in any still alive salute mobs
        std::vector<ObjectGuid> guids(
            m_SaluteListOne.begin(), m_SaluteListOne.end());
        guids.insert(
            guids.end(), m_SaluteListTwo.begin(), m_SaluteListTwo.end());
        guids.insert(
            guids.end(), m_SaluteListThree.begin(), m_SaluteListThree.end());
        for (auto guid : guids)
            if (Creature* c = m_creature->GetMap()->GetCreature(guid))
                c->AI()->AttackStart(who);
    }

    uint32 m_uiArcaneResonanceTimer;
    uint32 m_uiDevastationCD;
    uint32 m_uiDevastationTimer;
    uint32 m_uiCombatSummonTimer;

    std::vector<ObjectGuid> m_SaluteListOne;
    std::vector<ObjectGuid> m_SaluteListTwo;
    std::vector<ObjectGuid> m_SaluteListThree;

    bool m_bHasDoneSummons;
    bool m_bIsRegularMode;

    void Reset() override
    {
        m_uiArcaneResonanceTimer = 3000;
        m_uiDevastationCD = 0;
        m_uiDevastationTimer = urand(25000, 40000);

        m_bHasDoneSummons = false;
        m_uiCombatSummonTimer = 50 * 1000;

        // Intro
        m_uiCurrentIntroPhase = 0;
        m_uiStartIntroTimer = 8000;

        if (!m_creature->has_aura(SPELL_DOUBLE_ATTACK))
            m_creature->CastSpell(m_creature, SPELL_DOUBLE_ATTACK, true);
    }

    void KilledUnit(Unit* victim) override
    {
        DoKillSay(m_creature, victim, SAY_KILL_1, SAY_KILL_2);
    }

    void JustDied(Unit* /*pKiller*/) override
    {
        DoScriptText(SAY_DEATH, m_creature);
    }

    void SaluteList(std::vector<ObjectGuid>& list)
    {
        for (auto& elem : list)
        {
            if (Creature* pCreature = m_creature->GetMap()->GetCreature(elem))
                pCreature->HandleEmoteCommand(EMOTE_ONESHOT_SALUTE);
        }
    }

    bool IsGroupAlive(std::vector<ObjectGuid>& list)
    {
        for (auto& elem : list)
        {
            if (Creature* pCreature = m_creature->GetMap()->GetCreature(elem))
            {
                if (!pCreature->isAlive() || pCreature->isInCombat())
                    return false;
            }
        }
        return true;
    }

    void MovementInform(movement::gen uiData, uint32 uiPointId) override
    {
        if (uiData != movement::gen::point)
            return;

        m_creature->movement_gens.push(new movement::IdleMovementGenerator());

        switch (uiPointId)
        {
        case 1:
            m_creature->SetFacingTo(ChillPoint.O);
            m_uiCurrentIntroPhase = 1;
            break;
        case 2:
            m_creature->SetFacingTo(SalutePointOne.O);
            m_uiCurrentIntroPhase = 2;
            break;
        case 3:
            m_creature->SetFacingTo(SalutePointTwo.O);
            m_uiCurrentIntroPhase = 3;
            break;
        case 4:
            m_creature->SetFacingTo(SalutePointThree.O);
            m_uiCurrentIntroPhase = 4;
            break;
        case 5:
            m_creature->SetFacingTo(CheerPoint.O);
            m_uiCurrentIntroPhase = 5;
            break;
        case 15:
            m_creature->SetFacingTo(ChillPoint.O);
            m_uiCurrentIntroPhase = 15;
            m_uiWaitTimer = 2500;
            break;
        case 20:
            m_creature->SetFacingTo(HomePoint.O);
            m_uiCurrentIntroPhase = 0;
            m_uiStartIntroTimer = urand(12000, 18000);
            break;

        case 50:
            m_creature->movement_gens.remove_all(movement::gen::idle);
            m_creature->movement_gens.push(
                new movement::PointMovementGenerator(
                    1, ChillPoint.X, ChillPoint.Y, ChillPoint.Z, false, false),
                movement::EVENT_ENTER_COMBAT);
            break;

        case 100:
            m_creature->movement_gens.remove_all(movement::gen::idle);
            m_creature->movement_gens.push(
                new movement::PointMovementGenerator(
                    20, HomePoint.X, HomePoint.Y, HomePoint.Z, false, false),
                movement::EVENT_ENTER_COMBAT);
            break;
        }
    }

    uint32 m_uiCurrentIntroPhase;
    uint32 m_uiStartIntroTimer;
    uint32 m_uiSaluteTimer;
    uint32 m_uiWaitTimer;
    void DoIntroPhase(const uint32 uiDiff)
    {
        switch (m_uiCurrentIntroPhase)
        {
        case 0:
            if (m_uiStartIntroTimer <= uiDiff)
            {
                m_creature->movement_gens.remove_all(movement::gen::idle);
                m_creature->movement_gens.push(
                    new movement::PointMovementGenerator(50, MidPointForth.X,
                        MidPointForth.Y, MidPointForth.Z, false, false),
                    movement::EVENT_ENTER_COMBAT);
                m_uiCurrentIntroPhase = 100; // Moving phase
            }
            else
                m_uiStartIntroTimer -= uiDiff;
            break;

        case 1:
            switch (urand(1, 4))
            {
            case 1:
                if (!IsGroupAlive(m_SaluteListThree))
                    return;
                m_creature->movement_gens.remove_all(movement::gen::idle);
                m_creature->movement_gens.push(
                    new movement::PointMovementGenerator(2, SalutePointOne.X,
                        SalutePointOne.Y, SalutePointOne.Z, false, false),
                    movement::EVENT_ENTER_COMBAT);
                m_uiCurrentIntroPhase = 100; // Moving phase
                break;

            case 2:
                if (!IsGroupAlive(m_SaluteListTwo))
                    return;
                m_creature->movement_gens.remove_all(movement::gen::idle);
                m_creature->movement_gens.push(
                    new movement::PointMovementGenerator(3, SalutePointTwo.X,
                        SalutePointTwo.Y, SalutePointTwo.Z, false, false),
                    movement::EVENT_ENTER_COMBAT);
                m_uiCurrentIntroPhase = 100; // Moving phase
                break;

            case 3:
                if (!IsGroupAlive(m_SaluteListOne))
                    return;
                m_creature->movement_gens.remove_all(movement::gen::idle);
                m_creature->movement_gens.push(
                    new movement::PointMovementGenerator(4, SalutePointThree.X,
                        SalutePointThree.Y, SalutePointThree.Z, false, false),
                    movement::EVENT_ENTER_COMBAT);
                m_uiCurrentIntroPhase = 100; // Moving phase
                break;

            case 4:
                m_creature->movement_gens.remove_all(movement::gen::idle);
                m_creature->movement_gens.push(
                    new movement::PointMovementGenerator(5, CheerPoint.X,
                        CheerPoint.Y, CheerPoint.Z, false, false),
                    movement::EVENT_ENTER_COMBAT);
                m_uiCurrentIntroPhase = 100; // Moving phase
                break;
            }
            break;

        case 2:
            SaluteList(m_SaluteListThree);
            m_uiSaluteTimer = 1000;
            m_uiCurrentIntroPhase = 10; // Salute back phase
            break;

        case 3:
            SaluteList(m_SaluteListTwo);
            m_uiSaluteTimer = 1000;
            m_uiCurrentIntroPhase = 10; // Salute back phase
            break;

        case 4:
            SaluteList(m_SaluteListOne);
            m_uiSaluteTimer = 1000;
            m_uiCurrentIntroPhase = 10; // Salute back phase
            break;

        case 5:
            DoScriptText(SAY_INTRO, m_creature);
            m_creature->HandleEmoteCommand(EMOTE_ONESHOT_CHEER);
            m_uiWaitTimer = 5000;
            m_uiCurrentIntroPhase = 8; // Move back
            break;

        case 8:
            if (m_uiWaitTimer <= uiDiff)
            {
                m_creature->movement_gens.remove_all(movement::gen::idle);
                m_creature->movement_gens.push(
                    new movement::PointMovementGenerator(15, ChillPoint.X,
                        ChillPoint.Y, ChillPoint.Z, false, false),
                    movement::EVENT_ENTER_COMBAT);
                m_uiCurrentIntroPhase = 100; // Movement
                m_uiWaitTimer = 0;
            }
            else
                m_uiWaitTimer -= uiDiff;
            break;

        case 10:
            if (m_uiSaluteTimer <= uiDiff)
            {
                m_creature->HandleEmoteCommand(EMOTE_ONESHOT_SALUTE);
                m_uiSaluteTimer = 0;
                m_uiWaitTimer = 1800;
                m_uiCurrentIntroPhase = 8; // Move back
            }
            else
                m_uiSaluteTimer -= uiDiff;
            break;

        case 15:
            if (m_uiWaitTimer <= uiDiff)
            {
                if (urand(1, 3) == 1) // 33% chance to go home
                {
                    m_creature->movement_gens.remove_all(movement::gen::idle);
                    m_creature->movement_gens.push(
                        new movement::PointMovementGenerator(100,
                            MidPointBack.X, MidPointBack.Y, MidPointBack.Z,
                            false, false),
                        movement::EVENT_ENTER_COMBAT);
                    m_uiCurrentIntroPhase = 100; // Moving phase
                }
                else
                    m_uiCurrentIntroPhase = 1; // Start over
                m_uiWaitTimer = 0;
            }
            else
                m_uiWaitTimer -= uiDiff;
            break;

        case 100:
            break;
        }
    }

    void SummonCombatAdds()
    {
        float x = m_creature->GetX();
        float y = m_creature->GetY();
        float z = m_creature->GetZ();
        float o = m_creature->GetO();
        DoScriptText(SAY_SUMMON, m_creature);

        Creature* pS = m_creature->SummonCreature(NPC_BLOODWARDER_MENDER, x, y,
            z, o, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 3000);
        if (pS)
            pS->AI()->AttackStart(m_creature->getVictim());
        for (uint8 i = 0; i < 3; ++i)
        {
            if (Creature* pCreature =
                    m_creature->SummonCreature(NPC_BLOODWARDER_RESERVIST, x, y,
                        z, o, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 3000))
            {
                pCreature->AI()->AttackStart(m_creature->getVictim());
                pCreature->CastSpell(pCreature, SPELL_SIMPLE_TELEPORT, false);
            }
        }
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
        {
            DoIntroPhase(uiDiff);
            return;
        }

        if (m_uiArcaneResonanceTimer <= uiDiff)
        {
            if (DoCastSpellIfCan(
                    m_creature->getVictim(), SPELL_ARCANE_RESONANCE) == CAST_OK)
            {
                if (urand(1, 5) == 1)
                    DoScriptText(SAY_ARCANE_RESONANCE, m_creature);

                m_uiDevastationCD =
                    1500; // Doesn't seem to properly trigger GCD

                if (urand(1, 3) == 1)
                    m_uiArcaneResonanceTimer = 1500; // GCD
                else
                    m_uiArcaneResonanceTimer = urand(3000, 10000); // Normal CD
            }
        }
        else
            m_uiArcaneResonanceTimer -= uiDiff;

        if (m_uiDevastationCD)
        {
            if (m_uiDevastationCD <= uiDiff)
                m_uiDevastationCD = 0;
            else
                m_uiDevastationCD -= uiDiff;
        }

        // Do devastation preemptively if target has 3 stacks
        if (m_uiDevastationCD == 0)
        {
            if (AuraHolder* holder =
                    m_creature->getVictim()->get_aura(SPELL_ARCANE_RESONANCE))
            {
                if (holder->GetStackAmount() == 3)
                {
                    if (DoCastSpellIfCan(m_creature->getVictim(),
                            SPELL_ARCANE_DEVASTATION) == CAST_OK)
                    {
                        DoScriptText(SAY_ARCANE_DEVASTATION, m_creature);
                        m_uiDevastationCD = 8000;
                        m_uiDevastationTimer = urand(20000, 40000);
                    }
                }
            }
        }

        if (m_uiDevastationTimer <= uiDiff)
        {
            if (DoCastSpellIfCan(m_creature->getVictim(),
                    SPELL_ARCANE_DEVASTATION) == CAST_OK)
            {
                DoScriptText(SAY_ARCANE_DEVASTATION, m_creature);
                m_uiDevastationCD = 8000;
                m_uiDevastationTimer = urand(20000, 40000);
                ;
            }
        }
        else
            m_uiDevastationTimer -= uiDiff;

        if (m_bIsRegularMode)
        {
            if (!m_bHasDoneSummons && m_creature->GetHealthPercent() <= 55)
            {
                SummonCombatAdds();
                m_bHasDoneSummons = true;
            }
        }
        else
        {
            if (m_uiCombatSummonTimer <= uiDiff)
            {
                SummonCombatAdds();
                m_uiCombatSummonTimer = 60 * 1000;
            }
            else
                m_uiCombatSummonTimer -= uiDiff;
        }

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_boss_commander_sarannis(Creature* pCreature)
{
    return new boss_commander_sarannisAI(pCreature);
}

void AddSC_boss_commander_sarannis()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "boss_commander_sarannis";
    pNewScript->GetAI = &GetAI_boss_commander_sarannis;
    pNewScript->RegisterSelf();
}
