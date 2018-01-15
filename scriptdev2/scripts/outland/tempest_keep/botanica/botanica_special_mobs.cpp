/* Copyright (C) 2012 Corecraft */

/* ScriptData
SDName: mob_bloodwarder_falconer, mob_bloodwarder_protector
SD%Complete: 100
SDComment:
SDCategory: Tempest Keep, The Botanica
EndScriptData */

#include "precompiled.h"
#include "sc_grid_searchers.h"
#include <string>
#include <vector>

/***
 Bloodwarder Falcon and his pets
 ***/
enum
{
    FALCONER_NR_TWO_GUID = 83007,
    FALCONER_NR_THREE_GUID = 1006503,
    SPELL_MULTI_SHOT = 34879,
    SPELL_CALL_OF_THE_FALCON = 34852, // Implemented in this script too
    SPELL_DETERRENCE = 31567,
    SPELL_ARCANE_SHOT = 35401, // Doesn't seem to use this on retail...
    SPELL_WING_CLIP = 32908,
};

const DynamicWaypoint nr_one_points[] = {
    DynamicWaypoint(-20.0f, 287.8f, -1.82f, 6.3f, 0, true),
    DynamicWaypoint(1.2f, 287.9f, 1.0f, 6.3f, 0, true),
    DynamicWaypoint(-20.0f, 287.8f, -1.82f, 3.2f, 0, true),
    DynamicWaypoint(-31.7f, 291.1f, -1.84f, 5.8f, 0, true),
    DynamicWaypoint(-38.1f, 291.2f, -1.84f, 3.7f, 0, true),
    DynamicWaypoint(-38.1f, 285.0f, -1.84f, 5.0f, 0, true),
    DynamicWaypoint(-32.5f, 284.1f, -1.84f, 0.0f, 0, true),
    DynamicWaypoint(-31.7f, 291.1f, -1.84f, 5.8f, 0, true),
};

const DynamicWaypoint nr_two_points[] = {
    DynamicWaypoint(-36.1f, 300.9f, -1.9f, 1.3f, 0, true),
    DynamicWaypoint(-32.3f, 311.5f, -2.9f, 1.0f, 0, true),
    DynamicWaypoint(-25.1f, 319.0f, -3.8f, 0.5f, 1250, true),
    DynamicWaypoint(-32.3f, 311.5f, -2.9f, 4.3f, 0, true),
    DynamicWaypoint(-36.1f, 300.9f, -1.9f, 4.4f, 0, true),
    DynamicWaypoint(-39.5f, 288.0f, -1.84f, 4.5f, 0, true),
    DynamicWaypoint(-34.4f, 282.4f, -1.84f, 5.9f, 0, true),
    DynamicWaypoint(-29.4f, 288.5f, -1.84f, 1.3f, 0, true),
    DynamicWaypoint(-36.2f, 293.4f, -1.84f, 2.7f, 0, true),
    DynamicWaypoint(-39.3f, 287.6f, -1.84f, 3.8f, 0, true),
};

const DynamicWaypoint nr_three_points[] = {
    DynamicWaypoint(-34.8f, 271.8f, -2.3f, 4.9f, 0, true),
    DynamicWaypoint(-30.0f, 262.4f, -3.2f, 5.4f, 1250, true),
    DynamicWaypoint(-34.8f, 271.8f, -2.3f, 1.6f, 0, true),
    DynamicWaypoint(-32.6f, 282.8f, -1.84f, 1.1f, 0, true),
    DynamicWaypoint(-30.3f, 291.0f, -1.84f, 1.6f, 0, true),
    DynamicWaypoint(-40.3f, 289.6f, -1.84f, 3.8f, 0, true),
    DynamicWaypoint(-34.2f, 283.2f, -1.84f, 5.6f, 0, true),
};

const DynamicWaypoint nr_four_points[] = {
    DynamicWaypoint(91.0f, 283.0f, -5.4f, 2.9f, 0, true),
    DynamicWaypoint(77.3f, 278.5f, -5.35f, 3.8f, 0, true),
    DynamicWaypoint(74.2f, 257.9f, -5.5f, 5.0f, 0, true),
    DynamicWaypoint(88.9f, 254.4f, -5.4f, 1.0f, 0, true),
    DynamicWaypoint(94.5f, 267.6f, -5.9f, 1.1f, 0, true),
    DynamicWaypoint(101.1f, 280.0f, -6.8f, 2.1f, 0, true),
};

const DynamicWaypoint nr_five_points[] = {
    DynamicWaypoint(114.4f, 287.8f, -6.0f, 0.17f, 0, true),
    DynamicWaypoint(134.9f, 281.8f, -5.4f, 5.8f, 0, true),
    DynamicWaypoint(129.6f, 272.7f, -5.4f, 3.4f, 0, true),
    DynamicWaypoint(117.0f, 271.9f, -5.9f, 3.3f, 0, true),
    DynamicWaypoint(104.5f, 273.9f, -6.9f, 2.0f, 0, true),
    DynamicWaypoint(107.7f, 281.2f, -6.8f, 1.1f, 0, true),
};

const DynamicWaypoint nr_six_points[] = {
    DynamicWaypoint(90.4f, 295.0f, -5.9f, 3.9f, 0, true),
    DynamicWaypoint(83.0f, 288.8f, -5.4f, 3.8f, 0, true),
    DynamicWaypoint(74.6f, 294.8f, -5.5f, 2.4f, 0, true),
    DynamicWaypoint(81.2f, 303.7f, -5.4f, 0.9f, 0, true),
    DynamicWaypoint(88.9f, 305.3f, -5.9f, 0.3f, 0, true),
    DynamicWaypoint(97.7f, 306.9f, -6.6f, 0.0f, 0, true),
    DynamicWaypoint(95.5f, 301.2f, -6.7f, 4.0f, 0, true),
};

const DynamicWaypoint nr_seven_points[] = {
    DynamicWaypoint(104.7f, 294.0f, -6.7f, 5.3f, 0, true),
    DynamicWaypoint(113.5f, 288.3f, -6.5f, 5.7f, 0, true),
    DynamicWaypoint(120.1f, 289.0f, -5.4f, 0.2f, 0, true),
    DynamicWaypoint(110.2f, 301.9f, -5.5f, 2.2f, 0, true),
    DynamicWaypoint(102.7f, 308.5f, -6.6f, 2.8f, 0, true),
    DynamicWaypoint(101.2f, 301.5f, -6.7f, 5.0f, 0, true),
};

struct MANGOS_DLL_DECL mob_bloodwarder_falconerAI : public ScriptedAI
{
    int grp_nr;
    bool m_doneSetup;
    std::vector<std::pair<ObjectGuid, uint32>> m_summonGuids;
    uint32 m_nextScoutTimer;

    int32 m_grp_id;

    mob_bloodwarder_falconerAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_doneSetup = false;
        Reset();
    }

    void DoSetup()
    {
        m_grp_id = m_creature->GetMap()->GetCreatureGroupMgr().CreateNewGroup(
            "Bloodwarder Falconer Group", true);

        grp_nr = 1;
        if (m_creature->GetGUIDLow() == FALCONER_NR_TWO_GUID)
            grp_nr = 2;
        else if (m_creature->GetGUIDLow() == FALCONER_NR_THREE_GUID)
            grp_nr = 3;

        if (auto group =
                m_creature->GetMap()->GetCreatureGroupMgr().GetGroup(m_grp_id))
            group->AddMember(m_creature, false);

        // Spawn adds
        if (grp_nr == 1)
        {
            if (Creature* pSummon =
                    m_creature->SummonCreature(18155, -31.7f, 291.1f, -1.84f,
                        5.8f, TEMPSUMMON_CORPSE_TIMED_DESPAWN, 2 * 60 * 1000))
            {
                m_summonGuids.push_back(
                    std::pair<ObjectGuid, uint32>(pSummon->GetObjectGuid(), 0));
                if (auto group =
                        m_creature->GetMap()->GetCreatureGroupMgr().GetGroup(
                            m_grp_id))
                    group->AddMember(pSummon, false);
            }
            if (Creature* pSummon =
                    m_creature->SummonCreature(18155, -39.3f, 287.6f, -1.84f,
                        3.8f, TEMPSUMMON_CORPSE_TIMED_DESPAWN, 2 * 60 * 1000))
            {
                m_summonGuids.push_back(
                    std::pair<ObjectGuid, uint32>(pSummon->GetObjectGuid(), 1));
                if (auto group =
                        m_creature->GetMap()->GetCreatureGroupMgr().GetGroup(
                            m_grp_id))
                    group->AddMember(pSummon, false);
            }
            if (Creature* pSummon =
                    m_creature->SummonCreature(18155, -34.2f, 283.2f, -1.84f,
                        5.6f, TEMPSUMMON_CORPSE_TIMED_DESPAWN, 2 * 60 * 1000))
            {
                m_summonGuids.push_back(
                    std::pair<ObjectGuid, uint32>(pSummon->GetObjectGuid(), 2));
                if (auto group =
                        m_creature->GetMap()->GetCreatureGroupMgr().GetGroup(
                            m_grp_id))
                    group->AddMember(pSummon, false);
            }
        }
        else if (grp_nr == 2)
        {
            if (Creature* pSummon =
                    m_creature->SummonCreature(18155, 101.1f, 280.0f, -6.8f,
                        2.1f, TEMPSUMMON_CORPSE_TIMED_DESPAWN, 2 * 60 * 1000))
            {
                m_summonGuids.push_back(
                    std::pair<ObjectGuid, uint32>(pSummon->GetObjectGuid(), 0));
                if (auto group =
                        m_creature->GetMap()->GetCreatureGroupMgr().GetGroup(
                            m_grp_id))
                    group->AddMember(pSummon, false);
            }
            if (Creature* pSummon =
                    m_creature->SummonCreature(18155, 107.7f, 281.2f, -6.8f,
                        1.1f, TEMPSUMMON_CORPSE_TIMED_DESPAWN, 2 * 60 * 1000))
            {
                m_summonGuids.push_back(
                    std::pair<ObjectGuid, uint32>(pSummon->GetObjectGuid(), 1));
                if (auto group =
                        m_creature->GetMap()->GetCreatureGroupMgr().GetGroup(
                            m_grp_id))
                    group->AddMember(pSummon, false);
            }
        }
        else
        {
            if (Creature* pSummon =
                    m_creature->SummonCreature(18155, 95.5f, 301.2f, -6.7f,
                        4.0f, TEMPSUMMON_CORPSE_TIMED_DESPAWN, 2 * 60 * 1000))
            {
                m_summonGuids.push_back(
                    std::pair<ObjectGuid, uint32>(pSummon->GetObjectGuid(), 0));
                if (auto group =
                        m_creature->GetMap()->GetCreatureGroupMgr().GetGroup(
                            m_grp_id))
                    group->AddMember(pSummon, false);
            }
            if (Creature* pSummon =
                    m_creature->SummonCreature(18155, 101.2f, 301.5f, -6.7f,
                        5.0f, TEMPSUMMON_CORPSE_TIMED_DESPAWN, 2 * 60 * 1000))
            {
                m_summonGuids.push_back(
                    std::pair<ObjectGuid, uint32>(pSummon->GetObjectGuid(), 1));
                if (auto group =
                        m_creature->GetMap()->GetCreatureGroupMgr().GetGroup(
                            m_grp_id))
                    group->AddMember(pSummon, false);
            }
        }

        m_doneSetup = true;
    }

    uint32 m_multiShot;
    uint32 m_wingClip;
    uint32 m_callOfTheFalcon;
    uint32 m_falconUpdate;
    uint32 m_deterrence;

    void Reset() override
    {
        m_nextScoutTimer = 4000;
        m_multiShot = urand(8000, 14000);
        m_wingClip = urand(6000, 12000);
        m_callOfTheFalcon = 4000;
        m_falconUpdate = 750;
        m_deterrence = urand(4000, 7000);

        BirdFocusTarget(NULL);
    }

    void BirdFocusTarget(Unit* target)
    {
        for (auto p : m_summonGuids)
            if (Creature* c = m_creature->GetMap()->GetCreature(p.first))
                c->SetFocusTarget(target);
    }

    void JustRespawned() override
    {
        // Reconstruct our group
        if (auto group =
                m_creature->GetMap()->GetCreatureGroupMgr().GetGroup(m_grp_id))
        {
            // Clear all members; they will be readded in DoSetup()
            for (auto member : group->GetMembers())
            {
                // Despawn any of our members that still exist (except for
                // ourselves)
                if (member != m_creature)
                    member->ForcedDespawn();
            }
            // Recreate group
            m_creature->GetMap()->GetCreatureGroupMgr().DeleteGroup(m_grp_id);
            m_doneSetup = false;
        }
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
        {
            if (!m_doneSetup)
            {
                DoSetup();
                return;
            }

            // Skip all OOC logic if we have no pets
            if (m_summonGuids.size() == 0)
                return;

            // Time for a new scout?
            if (m_nextScoutTimer)
            {
                if (m_nextScoutTimer <= uiDiff)
                {
                    std::vector<std::pair<ObjectGuid, uint32>>::iterator itr =
                        m_summonGuids.begin() +
                        urand(0, m_summonGuids.size() - 1);
                    bool keep_pet = false;
                    if (Creature* pScout =
                            m_creature->GetMap()->GetCreature(itr->first))
                    {
                        if (pScout->isAlive())
                            keep_pet = true;
                        if (pScout->isAlive() && !pScout->isInCombat() &&
                            !m_creature->movement_gens.has(
                                movement::gen::waypoint))
                        {
                            std::vector<DynamicWaypoint> pos;
                            if (grp_nr == 1)
                            {
                                if (itr->second == 0)
                                {
                                    pos.assign(std::begin(nr_one_points),
                                        std::end(nr_one_points));
                                    m_creature->SetFacingTo(0.0f);
                                    m_creature->HandleEmote(
                                        EMOTE_ONESHOT_POINT);
                                }
                                else if (itr->second == 1)
                                {
                                    pos.assign(std::begin(nr_two_points),
                                        std::end(nr_two_points));
                                    m_creature->SetFacingTo(2.1f);
                                    m_creature->HandleEmote(
                                        EMOTE_ONESHOT_POINT);
                                }
                                else if (itr->second == 2)
                                {
                                    pos.assign(std::begin(nr_three_points),
                                        std::end(nr_three_points));
                                    m_creature->SetFacingTo(4.9f);
                                    m_creature->HandleEmote(
                                        EMOTE_ONESHOT_POINT);
                                }
                            }
                            else if (grp_nr == 2)
                            {
                                if (itr->second == 0)
                                {
                                    pos.assign(std::begin(nr_four_points),
                                        std::end(nr_four_points));
                                    m_creature->SetFacingTo(3.5f);
                                    m_creature->HandleEmote(
                                        EMOTE_ONESHOT_POINT);
                                }
                                else if (itr->second == 1)
                                {
                                    pos.assign(std::begin(nr_five_points),
                                        std::end(nr_five_points));
                                    m_creature->SetFacingTo(0.1f);
                                    m_creature->HandleEmote(
                                        EMOTE_ONESHOT_POINT);
                                }
                            }
                            else
                            {
                                if (itr->second == 0)
                                {
                                    pos.assign(std::begin(nr_six_points),
                                        std::end(nr_six_points));
                                    m_creature->SetFacingTo(3.6f);
                                    m_creature->HandleEmote(
                                        EMOTE_ONESHOT_POINT);
                                }
                                else if (itr->second == 1)
                                {
                                    pos.assign(std::begin(nr_seven_points),
                                        std::end(nr_seven_points));
                                    m_creature->SetFacingTo(5.9f);
                                    m_creature->HandleEmote(
                                        EMOTE_ONESHOT_POINT);
                                }
                            }
                            pScout->movement_gens.push(
                                new movement::DynamicWaypointMovementGenerator(
                                    pos, false));
                            m_creature->MonsterSay("Do as I say, fly!", 0);
                            m_nextScoutTimer = urand(10000, 18000);
                        }
                    }
                    if (!keep_pet)
                        m_summonGuids.erase(itr);
                }
                else
                    m_nextScoutTimer -= uiDiff;
            }

            return;
        }

        // In-Combat Logic
        if (m_deterrence < uiDiff)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_DETERRENCE) == CAST_OK)
                m_deterrence = urand(14000, 18000);
        }
        else
            m_deterrence -= uiDiff;

        if (m_multiShot <= uiDiff)
        {
            if (Unit* pTarget =
                    m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM,
                        0, SPELL_MULTI_SHOT, SELECT_FLAG_NOT_IN_MELEE_RANGE))
            {
                if (DoCastSpellIfCan(pTarget, SPELL_MULTI_SHOT) == CAST_OK)
                    m_multiShot = urand(8000, 16000);
            }
        }
        else
            m_multiShot -= uiDiff;

        if (m_wingClip <= uiDiff)
        {
            if (DoCastSpellIfCan(m_creature->getVictim(), SPELL_WING_CLIP) ==
                CAST_OK)
                m_wingClip = urand(6000, 18000);
        }
        else
            m_wingClip -= uiDiff;

        if (m_callOfTheFalcon <= uiDiff)
        {
            if (Unit* pTarget = m_creature->SelectAttackingTarget(
                    ATTACKING_TARGET_RANDOM, 0))
            {
                if (DoCastSpellIfCan(pTarget, SPELL_CALL_OF_THE_FALCON) ==
                    CAST_OK)
                {
                    std::string str("Kill ");
                    str += pTarget->GetName();
                    str += "!";
                    BirdFocusTarget(pTarget);
                    m_creature->MonsterYell(str.c_str(), 0);
                    m_callOfTheFalcon = urand(11000, 13000);
                    m_falconUpdate = 0;
                    return;
                }
            }
        }
        else
            m_callOfTheFalcon -= uiDiff;

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_mob_bloodwarder_falconer(Creature* pCreature)
{
    return new mob_bloodwarder_falconerAI(pCreature);
}

void AddSC_botanica_special_mobs()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "mob_bloodwarder_falconer";
    pNewScript->GetAI = &GetAI_mob_bloodwarder_falconer;
    pNewScript->RegisterSelf();
}
