/* Copyright (C) 2006 - 2012 ScriptDev2 <http://www.scriptdev2.com/>
 * This program is free software licensed under GPL version 2
 * Please see the included DOCS/LICENSE.TXT for more information */

/* ScriptData
SDName: FollowerAI
SD%Complete: 60
SDComment: This AI is under development
SDCategory: Npc
EndScriptData */

#include "follower_ai.h"
#include "precompiled.h"

const float MAX_PLAYER_DISTANCE = 100.0f;

enum
{
    POINT_COMBAT_START = 0xFFFFFF
};

FollowerAI::FollowerAI(Creature* pCreature)
  : ScriptedAI(pCreature), m_leaderGuid(), m_uiUpdateFollowTimer(2500),
    m_uiFollowState(STATE_FOLLOW_NONE), m_pQuestForFollow(nullptr)
{
}

// This part provides assistance to a player that are attacked by pWho, even if
// out of normal aggro range
// It will cause m_creature to attack pWho that are attacking _any_ player
// (which has been confirmed may happen also on offi)
bool FollowerAI::AssistPlayerInCombat(Unit* pWho)
{
    if (!pWho->getVictim())
        return false;

    // experimental (unknown) flag not present
    if (!(m_creature->GetCreatureInfo()->type_flags &
            CREATURE_TYPEFLAGS_CAN_ASSIST))
        return false;

    // unit state prevents (similar check is done in CanInitiateAttack which
    // also include checking unit_flags. We skip those here)
    if (m_creature->hasUnitState(UNIT_STAT_STUNNED | UNIT_STAT_DIED))
        return false;

    // victim of pWho is not a player
    if (!pWho->getVictim()->GetCharmerOrOwnerPlayerOrPlayerItself())
        return false;

    // never attack friendly
    if (m_creature->IsFriendlyTo(pWho))
        return false;

    // too far away and no free sight?
    if (m_creature->IsWithinDistInMap(pWho, MAX_PLAYER_DISTANCE) &&
        m_creature->IsWithinWmoLOSInMap(pWho))
    {
        // already fighting someone?
        if (!m_creature->getVictim())
        {
            AttackStart(pWho);
            return true;
        }
        else
        {
            pWho->SetInCombatWith(m_creature);
            m_creature->AddThreat(pWho);
            return true;
        }
    }

    return false;
}

void FollowerAI::MoveInLineOfSight(Unit* pWho)
{
    if (pWho->isTargetableForAttack() &&
        pWho->isInAccessablePlaceFor(m_creature))
    {
        // AssistPlayerInCombat can start attack, so return if true
        if (HasFollowState(STATE_FOLLOW_INPROGRESS) &&
            AssistPlayerInCombat(pWho))
            return;

        if (!m_creature->CanInitiateAttack())
            return;

        if (!m_creature->CanFly() &&
            m_creature->GetDistanceZ(pWho) > CREATURE_Z_ATTACK_RANGE)
            return;

        if (m_creature->IsHostileTo(pWho))
        {
            if (m_creature->IsWithinAggroDistance(pWho) &&
                m_creature->IsWithinWmoLOSInMap(pWho))
            {
                if (!m_creature->getVictim())
                {
                    AttackStart(pWho);
                }
                else if (m_creature->GetMap()->IsDungeon())
                {
                    pWho->SetInCombatWith(m_creature);
                    m_creature->AddThreat(pWho);
                }
            }
        }
    }
}

void FollowerAI::JustDied(Unit* /*pKiller*/)
{
    if (!HasFollowState(STATE_FOLLOW_INPROGRESS) || !m_leaderGuid ||
        !m_pQuestForFollow)
        return;

    // TODO: need a better check for quests with time limit.
    if (Player* pPlayer = GetLeaderForFollower())
    {
        if (Group* group = pPlayer->GetGroup())
        {
            for (auto member : group->members(true))
            {
                if (member->GetQuestStatus(m_pQuestForFollow->GetQuestId()) ==
                    QUEST_STATUS_INCOMPLETE)
                    member->FailQuest(m_pQuestForFollow->GetQuestId());
            }
        }
        else
        {
            if (pPlayer->GetQuestStatus(m_pQuestForFollow->GetQuestId()) ==
                QUEST_STATUS_INCOMPLETE)
                pPlayer->FailQuest(m_pQuestForFollow->GetQuestId());
        }
    }
}

void FollowerAI::JustRespawned()
{
    m_uiFollowState = STATE_FOLLOW_NONE;

    if (!IsCombatMovement())
        SetCombatMovement(true);

    if (m_creature->getFaction() != m_creature->GetCreatureInfo()->faction_A)
        m_creature->setFaction(m_creature->GetCreatureInfo()->faction_A);

    Reset();
}

void FollowerAI::UpdateAI(const uint32 uiDiff)
{
    if (HasFollowState(STATE_FOLLOW_INPROGRESS) && !m_creature->getVictim())
    {
        if (m_uiUpdateFollowTimer < uiDiff)
        {
            if (HasFollowState(STATE_FOLLOW_COMPLETE) &&
                !HasFollowState(STATE_FOLLOW_POSTEVENT))
            {
                LOG_DEBUG(
                    logging, "SD2: FollowerAI is set completed, despawns.");
                m_creature->ForcedDespawn();
                return;
            }

            bool bIsMaxRangeExceeded = true;

            if (Player* pPlayer = GetLeaderForFollower())
            {
                if (Group* group = pPlayer->GetGroup())
                {
                    for (auto member : group->members(true))
                    {
                        if (m_creature->IsWithinDistInMap(
                                member, MAX_PLAYER_DISTANCE))
                        {
                            bIsMaxRangeExceeded = false;
                            break;
                        }
                    }
                }
                else
                {
                    if (m_creature->IsWithinDistInMap(
                            pPlayer, MAX_PLAYER_DISTANCE))
                        bIsMaxRangeExceeded = false;
                }
            }

            if (bIsMaxRangeExceeded)
            {
                LOG_DEBUG(logging,
                    "SD2: FollowerAI failed because player/group was to far "
                    "away or not found");
                m_creature->ForcedDespawn();
                return;
            }

            m_uiUpdateFollowTimer = 1000;
        }
        else
            m_uiUpdateFollowTimer -= uiDiff;
    }

    UpdateFollowerAI(uiDiff);
}

void FollowerAI::UpdateFollowerAI(const uint32 /*uiDiff*/)
{
    if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
        return;

    DoMeleeAttackIfReady();
}

void FollowerAI::StartFollow(
    Player* pLeader, uint32 uiFactionForFollower, const Quest* pQuest)
{
    if (m_creature->getVictim())
    {
        LOG_DEBUG(
            logging, "SD2: FollowerAI attempt to StartFollow while in combat.");
        return;
    }

    if (HasFollowState(STATE_FOLLOW_INPROGRESS))
    {
        logging.error(
            "SD2: FollowerAI attempt to StartFollow while already following.");
        return;
    }

    // set variables
    m_leaderGuid = pLeader->GetObjectGuid();

    if (uiFactionForFollower)
        m_creature->setFaction(uiFactionForFollower);

    m_pQuestForFollow = pQuest;

    m_creature->SetUInt32Value(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_NONE);

    AddFollowState(STATE_FOLLOW_INPROGRESS);

    m_creature->movement_gens.push(
        new movement::FollowMovementGenerator(pLeader));

    LOG_DEBUG(logging, "SD2: FollowerAI start follow %s (Guid %s)",
        pLeader->GetName(), m_leaderGuid.GetString().c_str());
}

Player* FollowerAI::GetLeaderForFollower()
{
    if (Player* pLeader = m_creature->GetMap()->GetPlayer(m_leaderGuid))
    {
        if (pLeader->isAlive())
            return pLeader;
        else
        {
            if (Group* group = pLeader->GetGroup())
            {
                for (auto member : group->members(true))
                {
                    if (member->isAlive() &&
                        m_creature->IsWithinDistInMap(
                            member, MAX_PLAYER_DISTANCE))
                    {
                        LOG_DEBUG(logging,
                            "SD2: FollowerAI GetLeader changed and returned "
                            "new leader.");
                        m_leaderGuid = member->GetObjectGuid();
                        return member;
                    }
                }
            }
        }
    }

    LOG_DEBUG(
        logging, "SD2: FollowerAI GetLeader can not find suitable leader.");
    return NULL;
}

void FollowerAI::SetFollowComplete(bool bWithEndEvent)
{
    m_creature->movement_gens.remove_all(movement::gen::follow);

    if (bWithEndEvent)
        AddFollowState(STATE_FOLLOW_POSTEVENT);
    else
    {
        if (HasFollowState(STATE_FOLLOW_POSTEVENT))
            RemoveFollowState(STATE_FOLLOW_POSTEVENT);
    }

    AddFollowState(STATE_FOLLOW_COMPLETE);
}

void FollowerAI::SetFollowPaused(bool bPaused)
{
    if (!HasFollowState(STATE_FOLLOW_INPROGRESS) ||
        HasFollowState(STATE_FOLLOW_COMPLETE))
        return;

    if (bPaused)
    {
        AddFollowState(STATE_FOLLOW_PAUSED);

        m_creature->movement_gens.remove_all(movement::gen::follow);
    }
    else
    {
        RemoveFollowState(STATE_FOLLOW_PAUSED);

        if (Player* pLeader = GetLeaderForFollower())
        {
            m_creature->movement_gens.push(
                new movement::FollowMovementGenerator(pLeader));
        }
    }
}
