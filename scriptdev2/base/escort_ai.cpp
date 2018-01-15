/* Copyright (C) 2006 - 2012 ScriptDev2 <http://www.scriptdev2.com/>
 * This program is free software licensed under GPL version 2
 * Please see the included DOCS/LICENSE.TXT for more information */

/* ScriptData
SDName: EscortAI
SD%Complete: 100
SDComment:
SDCategory: Npc
EndScriptData */

#include "escort_ai.h"
#include "precompiled.h"
#include "../system/system.h"

const float MAX_PLAYER_DISTANCE = 66.0f;

enum
{
    POINT_LAST_POINT = 0xFFFFFF,
    POINT_HOME = 0xFFFFFE
};

npc_escortAI::npc_escortAI(Creature* pCreature)
  : ScriptedAI(pCreature), m_playerGuid(), m_uiWPWaitTimer(2500),
    m_uiPlayerCheckTimer(1000), m_uiEscortState(STATE_ESCORT_NONE),
    m_pQuestForEscort(nullptr), m_bIsRunning(false),
    m_bCanInstantRespawn(false), m_bCanReturnToStart(false)
{
    /* Empty */
}

void npc_escortAI::GetAIInformation(ChatHandler& reader)
{
    std::ostringstream oss;

    oss << "EscortAI ";
    if (m_playerGuid)
        oss << "started for " << m_playerGuid.GetString() << " ";
    if (m_pQuestForEscort)
        oss << "started with quest " << m_pQuestForEscort->GetQuestId();

    if (HasEscortState(STATE_ESCORT_ESCORTING))
    {
        oss << "\nEscortFlags: Escorting"
            << (HasEscortState(STATE_ESCORT_RETURNING) ? ", Returning" : "")
            << (HasEscortState(STATE_ESCORT_PAUSED) ? ", Paused" : "");

        if (CurrentWP != WaypointList.end())
            oss << "\nNext Waypoint Id = " << CurrentWP->uiId
                << " Position: " << CurrentWP->fX << " " << CurrentWP->fY << " "
                << CurrentWP->fZ;
    }

    reader.PSendSysMessage("%s", oss.str().c_str());
}

bool npc_escortAI::IsVisible(Unit* pWho) const
{
    if (!pWho)
        return false;

    return m_creature->IsWithinDist(pWho, VISIBLE_RANGE) &&
           pWho->can_be_seen_by(m_creature, m_creature);
}

void npc_escortAI::AttackStart(Unit* pWho)
{
    ScriptedAI::AttackStart(pWho);
}

void npc_escortAI::EnterCombat(Unit* pEnemy)
{
    if (pEnemy)
        Aggro(pEnemy);
}

void npc_escortAI::Aggro(Unit* /*pEnemy*/)
{
}

// see followerAI
bool npc_escortAI::AssistPlayerInCombat(Unit* pWho)
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

void npc_escortAI::MoveInLineOfSight(Unit* pWho)
{
    if (!m_creature->CanStartAttacking(pWho))
        return;

    if ((m_creature->GetCharmInfo() &&
            (m_creature->GetCharmInfo()->HasReactState(REACT_PASSIVE))) ||
        AssistPlayerInCombat(pWho))
        return;

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

void npc_escortAI::JustDied(Unit* /*pKiller*/)
{
    if (!HasEscortState(STATE_ESCORT_ESCORTING) || !m_playerGuid ||
        !m_pQuestForEscort)
        return;

    if (Player* pPlayer = GetPlayerForEscort())
    {
        if (Group* group = pPlayer->GetGroup())
        {
            for (auto member : group->members(true))
            {
                if (member->GetQuestStatus(m_pQuestForEscort->GetQuestId()) ==
                    QUEST_STATUS_INCOMPLETE)
                    member->FailQuest(m_pQuestForEscort->GetQuestId());
            }
        }
        else
        {
            if (pPlayer->GetQuestStatus(m_pQuestForEscort->GetQuestId()) ==
                QUEST_STATUS_INCOMPLETE)
                pPlayer->FailQuest(m_pQuestForEscort->GetQuestId());
        }
    }
}

void npc_escortAI::JustRespawned()
{
    m_uiEscortState = STATE_ESCORT_NONE;

    if (!IsCombatMovement())
        SetCombatMovement(true);

    // add a small delay before going to first waypoint, normal in near all
    // cases
    m_uiWPWaitTimer = 2500;

    if (m_creature->getFaction() != m_creature->GetCreatureInfo()->faction_A)
        m_creature->setFaction(m_creature->GetCreatureInfo()->faction_A);

    Reset();
}

void npc_escortAI::EnterEvadeMode(bool by_group)
{
    if (HasEscortState(STATE_ESCORT_ESCORTING))
        AddEscortState(STATE_ESCORT_RETURNING);

    ScriptedAI::EnterEvadeMode(by_group);
}

bool npc_escortAI::IsPlayerOrGroupInRange()
{
    if (Player* pPlayer = GetPlayerForEscort())
    {
        if (Group* group = pPlayer->GetGroup())
        {
            for (auto member : group->members(true))
            {
                if (m_creature->IsWithinDistInMap(member, MAX_PLAYER_DISTANCE))
                    return true;
            }
        }
        else
        {
            if (m_creature->IsWithinDistInMap(pPlayer, MAX_PLAYER_DISTANCE))
                return true;
        }
    }
    return false;
}

// Returns false, if npc is despawned
bool npc_escortAI::MoveToNextWaypoint()
{
    // Do nothing if escorting is paused
    if (HasEscortState(STATE_ESCORT_PAUSED))
        return true;

    // Final Waypoint reached (and final wait time waited)
    if (CurrentWP == WaypointList.end())
    {
        LOG_DEBUG(logging, "SD2: EscortAI reached end of waypoints");

        if (m_bCanReturnToStart)
        {
            float fRetX, fRetY, fRetZ;
            m_creature->GetRespawnCoord(fRetX, fRetY, fRetZ);

            m_creature->movement_gens.push(
                new movement::PointMovementGenerator(POINT_HOME, fRetX, fRetY,
                    fRetZ, false, !m_creature->IsWalking()),
                0, 30);

            m_uiWPWaitTimer = 0;

            LOG_DEBUG(logging,
                "SD2: EscortAI are returning home to spawn location: %u, %f, "
                "%f, %f",
                POINT_HOME, fRetX, fRetY, fRetZ);
            return true;
        }

        if (m_bCanInstantRespawn)
        {
            m_creature->SetDeathState(JUST_DIED);
            m_creature->Respawn();
        }
        else
            m_creature->ForcedDespawn();

        return false;
    }

    m_creature->movement_gens.push(
        new movement::PointMovementGenerator(CurrentWP->uiId, CurrentWP->fX,
            CurrentWP->fY, CurrentWP->fZ, false, !m_creature->IsWalking()),
        0, 30);
    LOG_DEBUG(logging, "SD2: EscortAI start waypoint %u (%f, %f, %f).",
        CurrentWP->uiId, CurrentWP->fX, CurrentWP->fY, CurrentWP->fZ);

    WaypointStart(CurrentWP->uiId);

    m_uiWPWaitTimer = 0;

    return true;
}

void npc_escortAI::UpdateAI(const uint32 uiDiff)
{
    if (HasEscortState(STATE_ESCORT_RETURNING) &&
        !m_creature->movement_gens.has(movement::gen::home))
        RemoveEscortState(STATE_ESCORT_RETURNING);

    // Waypoint Updating
    if (HasEscortState(STATE_ESCORT_ESCORTING) && !m_creature->getVictim() &&
        m_uiWPWaitTimer && !HasEscortState(STATE_ESCORT_RETURNING))
    {
        if (m_uiWPWaitTimer <= uiDiff)
        {
            if (!MoveToNextWaypoint())
                return;
        }
        else
            m_uiWPWaitTimer -= uiDiff;
    }

    // Check if player or any member of his group is within range
    if (HasEscortState(STATE_ESCORT_ESCORTING) && m_playerGuid &&
        !m_creature->getVictim() && !HasEscortState(STATE_ESCORT_RETURNING))
    {
        if (m_uiPlayerCheckTimer < uiDiff)
        {
            if (!HasEscortState(STATE_ESCORT_PAUSED) &&
                !IsPlayerOrGroupInRange())
            {
                LOG_DEBUG(logging,
                    "SD2: EscortAI failed because player/group was to far away "
                    "or not found");

                if (m_bCanInstantRespawn)
                {
                    m_creature->SetDeathState(JUST_DIED);
                    m_creature->Respawn();
                }
                else
                    m_creature->ForcedDespawn();

                return;
            }

            m_uiPlayerCheckTimer = 1000;
        }
        else
            m_uiPlayerCheckTimer -= uiDiff;
    }

    UpdateEscortAI(uiDiff);
}

void npc_escortAI::UpdateEscortAI(const uint32 /*uiDiff*/)
{
    // Check if we have a current target
    if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
        return;

    DoMeleeAttackIfReady();
}

void npc_escortAI::MovementInform(movement::gen type, uint32 uiPointId)
{
    if ((type != movement::gen::point && type != movement::gen::home) ||
        !HasEscortState(STATE_ESCORT_ESCORTING))
        return;

    // Combat start position reached, continue waypoint movement
    if (type == movement::gen::home)
    {
        LOG_DEBUG(logging,
            "SD2: EscortAI has returned to original position before combat");

        m_creature->SetWalk(!m_bIsRunning);
        RemoveEscortState(STATE_ESCORT_RETURNING);
    }
    else if (uiPointId == POINT_HOME)
    {
        LOG_DEBUG(logging,
            "SD2: EscortAI has returned to original home location and will "
            "continue from beginning of waypoint list.");

        CurrentWP = WaypointList.begin();
        m_uiWPWaitTimer = 0;
    }
    else
    {
        // Make sure that we are still on the right waypoint
        if (CurrentWP->uiId != uiPointId)
        {
            logging.error(
                "SD2: EscortAI for Npc %u reached waypoint out of order %u, "
                "expected %u.",
                m_creature->GetEntry(), uiPointId, CurrentWP->uiId);
            return;
        }

        LOG_DEBUG(
            logging, "SD2: EscortAI waypoint %u reached.", CurrentWP->uiId);

        // Call WP function
        WaypointReached(CurrentWP->uiId);

        m_uiWPWaitTimer = CurrentWP->uiWaitTime;

        ++CurrentWP;
    }

    if (!m_uiWPWaitTimer)
    {
        // Continue WP Movement if Can
        if (HasEscortState(STATE_ESCORT_ESCORTING) &&
            !HasEscortState(STATE_ESCORT_PAUSED | STATE_ESCORT_RETURNING) &&
            !m_creature->getVictim())
            MoveToNextWaypoint();
        else
            m_uiWPWaitTimer = 1;
    }
}

/*void npc_escortAI::AddWaypoint(uint32 id, float x, float y, float z, uint32
WaitTimeMs)
{
    Escort_Waypoint t(id, x, y, z, WaitTimeMs);

    WaypointList.push_back(t);
}*/

void npc_escortAI::FillPointMovementListForCreature()
{
    std::vector<ScriptPointMove> const& pPointsEntries =
        pSystemMgr.GetPointMoveList(m_creature->GetEntry());

    if (pPointsEntries.empty())
        return;

    std::vector<ScriptPointMove>::const_iterator itr;

    for (itr = pPointsEntries.begin(); itr != pPointsEntries.end(); ++itr)
    {
        Escort_Waypoint pPoint(
            itr->uiPointId, itr->fX, itr->fY, itr->fZ, itr->uiWaitTime);
        WaypointList.push_back(pPoint);
    }
}

void npc_escortAI::SetCurrentWaypoint(uint32 uiPointId)
{
    if (!(HasEscortState(STATE_ESCORT_PAUSED))) // Only when paused
        return;

    if (uiPointId == CurrentWP->uiId) // Already here
        return;

    bool bFoundWaypoint = false;
    for (std::list<Escort_Waypoint>::iterator itr = WaypointList.begin();
         itr != WaypointList.end(); ++itr)
    {
        if (itr->uiId == uiPointId)
        {
            CurrentWP = itr; // Set to found itr
            bFoundWaypoint = true;
            break;
        }
    }

    if (!bFoundWaypoint)
    {
        LOG_DEBUG(logging,
            "SD2: EscortAI current waypoint tried to set to id %u, but doesn't "
            "exist in WaypointList",
            uiPointId);
        return;
    }

    m_uiWPWaitTimer = 1;

    LOG_DEBUG(logging, "SD2: EscortAI current waypoint set to id %u",
        CurrentWP->uiId);
}

void npc_escortAI::SetRun(bool bRun)
{
    if (bRun)
    {
        if (!m_bIsRunning)
            m_creature->SetWalk(false);
        else
            LOG_DEBUG(logging,
                "SD2: EscortAI attempt to set run mode, but is already "
                "running.");
    }
    else
    {
        if (m_bIsRunning)
            m_creature->SetWalk(true);
        else
            LOG_DEBUG(logging,
                "SD2: EscortAI attempt to set walk mode, but is already "
                "walking.");
    }
    m_bIsRunning = bRun;
}

// TODO: get rid of this many variables passed in function.
void npc_escortAI::Start(bool bRun, const Player* pPlayer, const Quest* pQuest,
    bool bInstantRespawn, bool bCanLoopPath)
{
    if (m_creature->getVictim())
    {
        logging.error("SD2: EscortAI attempt to Start while in combat.");
        return;
    }

    if (HasEscortState(STATE_ESCORT_ESCORTING))
    {
        logging.error(
            "SD2: EscortAI attempt to Start while already escorting.");
        return;
    }

    if (!WaypointList.empty())
        WaypointList.clear();

    FillPointMovementListForCreature();

    if (WaypointList.empty())
    {
        logging.error(
            "SD2: EscortAI Start with 0 waypoints (possible missing entry in "
            "script_waypoint).");
        return;
    }

    // set variables
    m_bIsRunning = bRun;

    m_playerGuid = pPlayer ? pPlayer->GetObjectGuid() : ObjectGuid();
    m_pQuestForEscort = pQuest;

    m_bCanInstantRespawn = bInstantRespawn;
    m_bCanReturnToStart = bCanLoopPath;

    if (m_bCanReturnToStart && m_bCanInstantRespawn)
        LOG_DEBUG(logging,
            "SD2: EscortAI is set to return home after waypoint end and "
            "instant respawn at waypoint end. Creature will never despawn.");

    m_creature->movement_gens.remove_if([](auto*)
        {
            return true;
        });

    // disable npcflags
    m_creature->SetUInt32Value(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_NONE);

    LOG_DEBUG(logging, "SD2: EscortAI started with " SIZEFMTD
                       " waypoints. Run = %d, PlayerGuid = %s",
        WaypointList.size(), m_bIsRunning, m_playerGuid.GetString().c_str());

    CurrentWP = WaypointList.begin();

    // Set initial speed
    m_creature->SetWalk(!m_bIsRunning);

    AddEscortState(STATE_ESCORT_ESCORTING);

    JustStartedEscort();
}

void npc_escortAI::SetEscortPaused(bool bPaused)
{
    if (!HasEscortState(STATE_ESCORT_ESCORTING))
        return;

    if (bPaused)
        AddEscortState(STATE_ESCORT_PAUSED);
    else
        RemoveEscortState(STATE_ESCORT_PAUSED);
}
