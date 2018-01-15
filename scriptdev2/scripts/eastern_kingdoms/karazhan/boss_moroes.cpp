/* Copyright (C) 2012 CoreCraft
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
SDName: Boss_Moroes
SD%Complete: 100
SDComment:
SDCategory: Karazhan
EndScriptData */

#include "karazhan.h"
#include "precompiled.h"

enum
{
    SAY_AGGRO = -1532011,
    SAY_GARROTE_1 = -1532012,
    SAY_GARROTE_2 = -1532013,
    SAY_KILL_1 = -1532014,
    SAY_KILL_2 = -1532015,
    SAY_KILL_3 = -1532016,
    SAY_DEATH = -1532017,
    EMOTE_DEADLY_THROW = -1532116,

    SPELL_VANISH = 29448,
    SPELL_GARROTE = 37066,
    SPELL_BLIND = 34694,
    SPELL_GOUGE = 29425,
    SPELL_FRENZY = 37023,
    SPELL_DEADLY_THROW = 37074
};

static const float GuestLocations[][4] = {
    {-10991.0f, -1884.33f, 81.73f, 0.614315f},
    {-10989.4f, -1885.88f, 81.73f, 0.904913f},
    {-10978.1f, -1887.07f, 81.73f, 2.035550f},
    {-10975.9f, -1885.81f, 81.73f, 2.253890f}};

static const uint32 MoroesAddsIds[] = {
    17007, 19872, 19873, 19874, 19875, 19876};

#define GUEST_WHINE_SIZE 4
static const int32 GuestWhine[GUEST_WHINE_SIZE] = {
    -1532117, -1532118, -1532119, -1532120};

#define MOROES_ANSWER_SIZE 5
static const int32 MoroesAnswers[MOROES_ANSWER_SIZE] = {
    -1532121, -1532122, -1532123, -1532124, -1532125};

struct MANGOS_DLL_DECL boss_moroesAI : public ScriptedAI
{
    boss_moroesAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_grpId = 0;
        m_instance = pCreature->GetMapId() == KARAZHAN_MAP_ID ?
                         (instance_karazhan*)pCreature->GetInstanceData() :
                         NULL;
        Reset();
    }

    void SpawnAdds()
    {
        // Add adds to creature group and summon them:
        m_grpId = m_creature->GetMap()->GetCreatureGroupMgr().CreateNewGroup(
            "Moroes' Group", true);
        if (CreatureGroup* grp =
                m_creature->GetMap()->GetCreatureGroupMgr().GetGroup(m_grpId))
        {
            grp->AddMember(m_creature, false);
            grp->SetLeader(m_creature, false);
            grp->AddFlag(CREATURE_GROUP_FLAG_LEADER_RESPAWN_ALL);

            std::vector<uint32> ids(MoroesAddsIds, MoroesAddsIds + 6);
            while (ids.size() > 4)
                ids.erase(ids.begin() + urand(0, ids.size() - 1));
            for (uint32 i = 0; i < 4; ++i)
                m_creature->SummonCreature(ids[i], GuestLocations[i][0],
                    GuestLocations[i][1], GuestLocations[i][2],
                    GuestLocations[i][3], TEMPSUMMON_CORPSE_TIMED_DESPAWN,
                    30 * MINUTE * IN_MILLISECONDS);
        }
    }

    void JustSummoned(Creature* creature) override
    {
        if (CreatureGroup* grp =
                m_creature->GetMap()->GetCreatureGroupMgr().GetGroup(m_grpId))
            grp->AddMember(creature, false);
    }

    int32 m_grpId;
    instance_karazhan* m_instance;
    uint32 m_vanishTimer;
    uint32 m_garroteTimer; // Only used in conjunction with vanish
    uint32 m_blindTimer;
    uint32 m_gougeTimer;
    uint32 m_deadlyThrowTimer;
    bool m_inVanish;
    bool m_enrage;
    uint32 m_rpTimer;
    uint32 m_rpPhase;
    ObjectGuid m_whinedGuid;

    void Reset() override
    {
        m_vanishTimer = urand(35000, 40000);
        m_blindTimer = urand(30000, 35000);
        m_gougeTimer = urand(25000, 30000);
        m_garroteTimer = 0; // Set in conjunction with blind
        m_deadlyThrowTimer = 10 * MINUTE * IN_MILLISECONDS; // Hard enrage
        m_enrage = false;
        m_inVanish = false;
        m_rpTimer = urand(10000, 15000);
        m_rpPhase = 1;
        m_whinedGuid = ObjectGuid();

        m_creature->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE);
    }

    void Aggro(Unit* /*pWho*/) override
    {
        if (m_instance)
            m_instance->SetData(TYPE_MOROES, IN_PROGRESS);
        DoScriptText(SAY_AGGRO, m_creature);
    }

    void JustReachedHome() override
    {
        if (m_instance)
            m_instance->SetData(TYPE_MOROES, FAIL);
    }

    void KilledUnit(Unit* victim) override
    {
        DoKillSay(m_creature, victim, SAY_KILL_1, SAY_KILL_2, SAY_KILL_3);
    }

    void JustDied(Unit* /*pVictim*/) override
    {
        if (!m_instance)
            return;

        DoScriptText(SAY_DEATH, m_creature);
        m_instance->SetData(TYPE_MOROES, DONE);

        // Remove Garrote when Moroes dies (only do it on death, not evade)
        for (const auto& elem : m_creature->GetMap()->GetPlayers())
        {
            if (elem.getSource() && elem.getSource()->isAlive())
                elem.getSource()->remove_auras(SPELL_GARROTE);
        }
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_grpId)
            SpawnAdds();

        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
        {
            if (m_rpTimer <= uiDiff)
            {
                switch (m_rpPhase)
                {
                case 1:
                    if (CreatureGroup* grp = m_creature->GetMap()
                                                 ->GetCreatureGroupMgr()
                                                 .GetGroup(m_grpId))
                    {
                        auto cv = grp->GetMembers();
                        if (!cv.empty()) // Guests always alive out of combat
                        {
                            auto itr = find(cv.begin(), cv.end(), m_creature);
                            if (itr != cv.end())
                                cv.erase(itr);
                            Creature* c = cv[urand(0, cv.size() - 1)];
                            m_whinedGuid = c->GetObjectGuid();
                            DoScriptText(
                                GuestWhine[urand(0, GUEST_WHINE_SIZE - 1)], c);
                        }
                        m_rpTimer = 4000;
                        m_rpPhase = 2;
                    }
                    break;
                case 2:
                    if (Creature* whined =
                            m_creature->GetMap()->GetCreature(m_whinedGuid))
                    {
                        m_creature->SetFacingToObject(whined);
                        m_creature->movement_gens.push(
                            new movement::StoppedMovementGenerator(6000),
                            movement::EVENT_ENTER_COMBAT);
                        DoScriptText(
                            MoroesAnswers[urand(0, MOROES_ANSWER_SIZE - 1)],
                            m_creature);
                        m_rpTimer = 6000 + urand(10000, 15000);
                        m_rpPhase = 1;
                    }
                    break;
                }
            }
            else
                m_rpTimer -= uiDiff;
            return;
        }

        if (m_inVanish)
        {
            // Keep updating other combat timers in vanish:
            if (m_vanishTimer > uiDiff)
                m_vanishTimer -= uiDiff;
            if (m_blindTimer > uiDiff)
                m_blindTimer -= uiDiff;
            if (m_gougeTimer > uiDiff)
                m_gougeTimer -= uiDiff;
            if (m_deadlyThrowTimer > uiDiff)
                m_deadlyThrowTimer -= uiDiff;

            // Garrote logic:
            if (m_garroteTimer <= uiDiff)
            {
                std::vector<Unit*> ungarrotedTargets;
                ungarrotedTargets.reserve(10);
                // Pick a target that doesn't already have garrote
                for (const auto& elem :
                    m_creature->getThreatManager().getThreatList())
                {
                    if (Unit* u = m_creature->GetMap()->GetUnit(
                            (elem)->getUnitGuid()))
                        if (u->GetTypeId() == TYPEID_PLAYER &&
                            !u->has_aura(SPELL_GARROTE))
                            ungarrotedTargets.push_back(u);
                }
                Unit* target = !ungarrotedTargets.empty() ?
                                   ungarrotedTargets[urand(
                                       0, ungarrotedTargets.size() - 1)] :
                                   m_creature->SelectAttackingTarget(
                                       ATTACKING_TARGET_RANDOM, 0);
                if (target)
                {
                    auto pos = target->GetPoint(M_PI_F,
                        target->GetObjectBoundingRadius() +
                            m_creature->GetObjectBoundingRadius(),
                        true);
                    m_creature->NearTeleportTo(
                        pos.x, pos.y, pos.z, m_creature->GetAngle(target));

                    DoCast(target, SPELL_GARROTE, false);
                    DoScriptText(urand(0, 1) ? SAY_GARROTE_1 : SAY_GARROTE_2,
                        m_creature);
                    m_creature->remove_auras(SPELL_VANISH);
                    m_inVanish = false;
                }
                else
                    EnterEvadeMode();
            }
            else
                m_garroteTimer -= uiDiff;

            return;
        }

        if (m_deadlyThrowTimer <= uiDiff)
        {
            if (DoCastSpellIfCan(m_creature->getVictim(), SPELL_DEADLY_THROW) ==
                CAST_OK)
            {
                DoScriptText(EMOTE_DEADLY_THROW, m_creature);
                m_deadlyThrowTimer = 5000;
            }
        }
        else
            m_deadlyThrowTimer -= uiDiff;

        if (!m_enrage && m_creature->GetHealthPercent() < 31.0f)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_FRENZY) == CAST_OK)
                m_enrage = true;
        }

        if (m_vanishTimer <= uiDiff)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_VANISH) == CAST_OK)
            {
                m_inVanish = true;
                m_garroteTimer = urand(3000, 8000);
                m_vanishTimer = urand(35000, 40000);
            }
        }
        else
            m_vanishTimer -= uiDiff;

        if (m_gougeTimer <= uiDiff)
        {
            if (DoCastSpellIfCan(m_creature->getVictim(), SPELL_GOUGE) ==
                CAST_OK)
                m_gougeTimer = urand(25000, 30000);
        }
        else
            m_gougeTimer -= uiDiff;

        if (m_blindTimer <= uiDiff)
        {
            if (Unit* pTarget = m_creature->SelectAttackingTarget(
                    ATTACKING_TARGET_RANDOM, 1, SPELL_BLIND))
                if (DoCastSpellIfCan(pTarget, SPELL_BLIND) == CAST_OK)
                    m_blindTimer = urand(30000, 35000);
        }
        else
            m_blindTimer -= uiDiff;

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_boss_moroes(Creature* pCreature)
{
    return new boss_moroesAI(pCreature);
}

void AddSC_boss_moroes()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "boss_moroes";
    pNewScript->GetAI = &GetAI_boss_moroes;
    pNewScript->RegisterSelf();
}
