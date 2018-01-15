/*
 * Copyright (C) 2008-2012 TrinityCore <http://www.trinitycore.org/>
 * Copyright (C) 2012-2015 corecraft <https://www.worldofcorecraft.com/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "SmartAI.h"
#include "Chat.h"
#include "Group.h"
#include "movement/generators.h"
#include "movement/HomeMovementGenerator.h"
#include "movement/IdleMovementGenerator.h"
#include "movement/PointMovementGenerator.h"
#include "movement/TargetedMovementGenerator.h"
#include "ObjectGuid.h"
#include "ObjectMgr.h"
#include "ScriptMgr.h"
#include "SpellMgr.h"
#include "TemporarySummon.h"
#include "World.h"
#include "pet_behavior.h"
#include "pet_template.h"
#include "Database/DatabaseEnv.h"
#include "maps/map_grid.h"

#define SMART_ESCORT_ID_OFFSET 1000000

SmartAI::SmartAI(Creature* c) : CreatureAI(c), mBehavioralAI(c)
{
    // copy script to local (protection for table reload)

    mWayPoints = nullptr;
    mEscortState = SMART_ESCORT_NONE;
    mNextWPID = 0;
    mWPReached = false;
    mOOCWPReached = false;
    mWPPauseTimer = 0;
    mLastWP = nullptr;

    mCanRepeatPath = false;

    // spawn in run mode
    m_creature->SetWalk(false);
    mRun = false;
    mEscortRun = false;

    m_creature->GetPosition(mLastOOCPos.x, mLastOOCPos.y, mLastOOCPos.z);
    mLastOOCPos.o = m_creature->GetO();

    mCanAutoAttack = true;
    mCanCombatMove = true;

    mForcedPaused = false;
    mLastWPIDReached = 0;

    mEscortQuestID = 0;

    mEscortInvokerCheckTimer = 1000;
    mInvincibilityHpLevel = 0;
    mSparring = false;
    mBehavioralAIMovingUs = false;
    mCombatReactionsDisabled = false;
    mDisengageToggleAutoAttack = false;

    mHasEscorters = false;
    mEscortDespawn = -1;
    mPassive = false;
    UpdatePassive();

    mCreatureGroup = 0;

    memset(mSavedPos, 0, sizeof(float) * 4);
}

bool SmartAI::use_pet_behavior() const
{
    if (m_creature->behavior() && !m_creature->behavior()->paused() &&
        (m_creature->get_template()->ctemplate_flags &
            PET_CFLAGS_IGNORE_PET_BEHAVIOR) == 0)
        return true;
    return false;
}

void SmartAI::Reset()
{
    ResetInternal(SMART_RESET_TYPE_SCRIPT);
}

void SmartAI::ResetInternal(SmartResetType type)
{
    m_creature->SetFocusTarget(nullptr);
    GetScript()->OnReset(type);
    mBehavioralAI.OnReset();
    mBehavioralAIMovingUs = false;
    mSparring = false;
}

WayPoint* SmartAI::GetNextWayPoint()
{
    if (!mWayPoints || mWayPoints->empty())
        return nullptr;

    if (mNextWPID >= mWayPoints->size())
        return nullptr;

    mLastWP = &(*mWayPoints)[mNextWPID];
    if (mLastWP->id != mNextWPID)
        logging.error(
            "SmartAI::GetNextWayPoint: Got not expected waypoint id %u, "
            "expected %u",
            mLastWP->id, mNextWPID);

    ++mNextWPID;

    return mLastWP;
}

void SmartAI::StartPath(bool run, uint32 path, bool repeat, Unit* invoker)
{
    if (m_creature->isInCombat()) // no wp movement in combat
    {
        logging.error(
            "SmartAI::StartPath: Creature entry %u wanted to start waypoint "
            "movement while in combat, ignoring.",
            m_creature->GetEntry());
        return;
    }
    if (HasEscortState(SMART_ESCORT_ESCORTING) ||
        HasEscortState(SMART_ESCORT_PAUSED))
        StopPath();
    if (path)
        if (!LoadPath(path))
            return;
    if (!mWayPoints || mWayPoints->empty())
        return;

    // Prevent OOC movement from being active while escorting
    m_creature->movement_gens.push(new movement::StoppedMovementGenerator(), 0,
        movement::get_default_priority(movement::gen::random) + 1);

    AddEscortState(SMART_ESCORT_ESCORTING);
    mCanRepeatPath = repeat;

    if (invoker && invoker->GetTypeId() == TYPEID_PLAYER)
    {
        Player* p = (Player*)invoker;
        if (Group* group = p->GetGroup())
        {
            for (auto member : group->members(true))
            {
                if (!member->IsAtGroupRewardDistance(m_creature))
                    continue;
                mEscorters.insert(member->GetObjectGuid());
            }
        }

        mEscorters.insert(
            p->GetObjectGuid()); // std::set so re-attempted insert okay
        mHasEscorters = true;
    }
    else
        mHasEscorters = false;

    mEscortRun = run;

    if (WayPoint* wp = GetNextWayPoint())
    {
        m_creature->GetPosition(mLastOOCPos.x, mLastOOCPos.y, mLastOOCPos.z);
        mLastOOCPos.o = m_creature->GetO();
        m_creature->movement_gens.push(
            new movement::PointMovementGenerator(
                SMART_ESCORT_ID_OFFSET + wp->id, wp->x, wp->y, wp->z, false,
                mEscortRun),
            0, 30);
        GetScript()->ProcessEventsFor(SMART_EVENT_WAYPOINT_START, nullptr,
            wp->id, GetScript()->GetPathId());
        mWPReached = false;
        mOOCWPReached = false;
    }
}

bool SmartAI::LoadPath(uint32 entry)
{
    if (HasEscortState(SMART_ESCORT_ESCORTING))
        return false;
    mWayPoints = sSmartWaypointMgr::Instance()->GetPath(entry);
    if (!mWayPoints)
    {
        GetScript()->SetPathId(0);
        return false;
    }
    GetScript()->SetPathId(entry);
    return true;
}

void SmartAI::PausePath(uint32 delay, bool forced)
{
    if (!HasEscortState(SMART_ESCORT_ESCORTING | SMART_ESCORT_RETURNING |
                        SMART_ESCORT_PAUSED) ||
        !mLastWP)
        return;
    if (HasEscortState(SMART_ESCORT_PAUSED))
    {
        // Update the paused time if it's longer than our current
        if (delay > mWPPauseTimer)
            mWPPauseTimer = delay;
        return;
    }
    mForcedPaused = forced;
    m_creature->GetPosition(mLastOOCPos.x, mLastOOCPos.y, mLastOOCPos.z);
    mLastOOCPos.o = m_creature->GetO();
    AddEscortState(SMART_ESCORT_PAUSED);
    mWPPauseTimer = delay;
    if (forced)
        m_creature->movement_gens.remove_all(movement::gen::point);
    GetScript()->ProcessEventsFor(SMART_EVENT_WAYPOINT_PAUSED, nullptr,
        mLastWP->id, GetScript()->GetPathId());
}

void SmartAI::StopPath(uint32 DespawnTime, uint32 quest, bool fail)
{
    if (!HasEscortState(SMART_ESCORT_ESCORTING | SMART_ESCORT_PAUSED) ||
        !mLastWP)
        return;

    if (quest)
        mEscortQuestID = quest;
    if (DespawnTime)
        SetEscortDespawnTime(DespawnTime);

    m_creature->GetPosition(mLastOOCPos.x, mLastOOCPos.y, mLastOOCPos.z);
    mLastOOCPos.o = m_creature->GetO();
    m_creature->movement_gens.remove_all(movement::gen::point);
    GetScript()->ProcessEventsFor(SMART_EVENT_WAYPOINT_STOPPED, nullptr,
        mLastWP->id, GetScript()->GetPathId());
    EndPath(fail, true);
}

void SmartAI::EndPath(bool fail, bool force)
{
    if (!mLastWP)
        return;

    // Remove Escort states before processing event (to not affect any actions,
    // such as evade -- which behaves differentl during escort)
    RemoveEscortState(SMART_ESCORT_ESCORTING | SMART_ESCORT_RETURNING |
                      SMART_ESCORT_RETURNED | SMART_ESCORT_PAUSED);

    uint32 wp_id = mLastWP->id;
    uint32 path_id = GetScript()->GetPathId();

    mWayPoints = nullptr;
    mNextWPID = 0;
    mWPPauseTimer = 0;
    mLastWP = nullptr;

    if (mCanRepeatPath && !force)
        StartPath(mEscortRun, path_id, mCanRepeatPath);
    else
    {
        GetScript()->SetPathId(0);
        m_creature->movement_gens.remove_if([](auto* gen)
            {
                return gen->id() == movement::gen::stopped &&
                       gen->priority() ==
                           movement::get_default_priority(
                               movement::gen::random) +
                               1;
            });
    }

    if (mEscortQuestID)
    {
        // Note: Anyone still in mEscorters is within reward range
        for (const auto& elem : mEscorters)
            if (Player* plr = m_creature->GetMap()->GetPlayer(elem))
            {
                if (fail)
                    plr->FailQuest(mEscortQuestID);
                else
                    plr->AreaExploredOrEventHappens(mEscortQuestID);
            }
    }

    mEscorters.clear();
    mHasEscorters = false;

    if (mEscortDespawn >= 0)
    {
        m_creature->ForcedDespawn(mEscortDespawn);
        m_creature->movement_gens.push(new movement::StoppedMovementGenerator(),
            0, movement::get_default_priority(movement::gen::follow) + 1);
    }
    mEscortDespawn = -1;

    GetScript()->ProcessEventsFor(
        SMART_EVENT_WAYPOINT_ENDED, nullptr, wp_id, path_id, fail);
}

void SmartAI::ResumePath()
{
    if (mLastWP && !mWPReached)
        m_creature->movement_gens.push(
            new movement::PointMovementGenerator(
                SMART_ESCORT_ID_OFFSET + mLastWP->id, mLastWP->x, mLastWP->y,
                mLastWP->z, false, mEscortRun),
            0, 30);
}

void SmartAI::UpdatePath(const uint32 diff)
{
    if (!HasEscortState(SMART_ESCORT_ESCORTING))
        return;

    // Check nearby Invokers (fail quest of those that get too far away)
    if (mEscortInvokerCheckTimer <= diff)
    {
        mEscortInvokerCheckTimer = 1000;

        if (!IsEscortInvokerInRange())
        {
            StopPath(SMART_ESCORT_FAILED_DESPAWN_TIME, mEscortQuestID, true);
            return;
        }
    }
    else
        mEscortInvokerCheckTimer -= diff;

    // Handle pause
    if (HasEscortState(SMART_ESCORT_PAUSED))
    {
        if (mWPPauseTimer < diff)
        {
            if (!m_creature->isInCombat() &&
                !HasEscortState(SMART_ESCORT_RETURNING) &&
                (mWPReached || mOOCWPReached || mForcedPaused))
            {
                GetScript()->ProcessEventsFor(SMART_EVENT_WAYPOINT_RESUMED,
                    nullptr, mLastWP->id, GetScript()->GetPathId());
                RemoveEscortState(SMART_ESCORT_PAUSED);
                if (mForcedPaused) // if paused between 2 wps resend movement
                {
                    mWPReached = false;
                    mForcedPaused = false;
                    ResumePath();
                }
            }
            mWPPauseTimer = 0;
        }
        else
            mWPPauseTimer -= diff;
    }

    // Handle returning
    if (HasEscortState(SMART_ESCORT_RETURNING | SMART_ESCORT_RETURNED) &&
        mOOCWPReached)
    {
        RemoveEscortState(SMART_ESCORT_RETURNING);
        AddEscortState(SMART_ESCORT_RETURNED);

        bool grp_ooc_wp_reached = true;

        // If creature is grouped, we need to check that all members of the
        // group have reached the OOC WP before we continue
        if (auto grp = m_creature->GetGroup())
        {
            for (auto m : grp->GetMembers())
            {
                if (!m->isAlive())
                    continue;
                if (auto ai = dynamic_cast<SmartAI*>(m->AI()))
                {
                    if (!ai->HasEscortState(SMART_ESCORT_RETURNED))
                        grp_ooc_wp_reached = false;
                }
            }
        }

        if (grp_ooc_wp_reached)
        {
            // SMART_ESCORT_RETURNED is removed in AttackStart
            mOOCWPReached = false;
            if (!HasEscortState(SMART_ESCORT_PAUSED))
                ResumePath();
        }
    }

    if ((m_creature->isInCombat() && !mCombatReactionsDisabled) ||
        HasEscortState(SMART_ESCORT_PAUSED | SMART_ESCORT_RETURNING))
        return;

    // Handle next wp
    if (mWPReached)
    {
        mWPReached = false;
        if (mNextWPID == GetWPCount())
        {
            EndPath();
        }
        else if (WayPoint* wp = GetNextWayPoint())
        {
            m_creature->movement_gens.push(
                new movement::PointMovementGenerator(
                    SMART_ESCORT_ID_OFFSET + wp->id, wp->x, wp->y, wp->z, false,
                    mEscortRun),
                0, 30);
        }
    }
}

void SmartAI::UpdateAI(const uint32 diff)
{
    GetScript()->OnUpdate(diff);
    UpdatePath(diff);

    if (use_pet_behavior())
    {
        static_cast<Pet*>(m_creature)->behavior()->update(diff);
    }
    else
    {
        if (mCombatReactionsDisabled || !m_creature->SelectHostileTarget() ||
            !m_creature->getVictim())
            return;

        mBehavioralAI.Update(diff);

        if (mCanAutoAttack && !m_creature->hasUnitState(UNIT_STAT_CONTROLLED))
            DoMeleeAttackIfReady();
    }
}

void SmartAI::Notify(uint32 id, Unit* source)
{
    GetScript()->ProcessEventsFor(SMART_EVENT_AI_NOTIFICATION, source, id);
}

bool SmartAI::IsEscortInvokerInRange()
{
    if (!mHasEscorters)
        return true;

    for (auto itr = mEscorters.begin(); itr != mEscorters.end();)
    {
        auto curr = itr++;

        Player* plr = m_creature->GetMap()->GetPlayer(*curr);
        if (!plr ||
            !plr->IsWithinDist(m_creature, SMART_ESCORT_MAX_PLAYER_DIST))
        {
            if (plr && mEscortQuestID)
                plr->FailQuest(mEscortQuestID);
            mEscorters.erase(curr);
        }
    }

    return !mEscorters.empty();
}

void SmartAI::MovepointReached(uint32 id)
{
    if (mLastWPIDReached != id)
        GetScript()->ProcessEventsFor(
            SMART_EVENT_WAYPOINT_REACHED, nullptr, id);

    mLastWPIDReached = id;
    mWPReached = true;
}

void SmartAI::DropSparring(Unit* who)
{
    if (mSparring)
    {
        if (who && m_creature->getVictim() == who)
            return;

        auto v = m_creature->getVictim();
        m_creature->DeleteThreatList();
        m_creature->CombatStop(true);
        if (who && v && v->GetTypeId() == TYPEID_UNIT)
            if (auto ai =
                    dynamic_cast<SmartAI*>(static_cast<Creature*>(v)->AI()))
                ai->DropSparring(nullptr);
        mSparring = false;
    }
}

void SmartAI::Disengage(float radius, bool toggleAutoAttack)
{
    if (!m_creature->getVictim())
        return;

    mDisengageToggleAutoAttack = toggleAutoAttack;

    if (mDisengageToggleAutoAttack)
        SetAutoAttack(false);
    auto pos = m_creature->getVictim()->GetPoint(m_creature, radius);
    m_creature->movement_gens.push(
        new movement::PointMovementGenerator(
            DISENGAGE_POINT_ID, pos.x, pos.y, pos.z, true, true),
        movement::EVENT_LEAVE_COMBAT);
}

void SmartAI::MovementInform(movement::gen type, uint32 Data)
{
    if (type == movement::gen::point && Data == DISENGAGE_POINT_ID)
    {
        if (mDisengageToggleAutoAttack)
            SetAutoAttack(true);
        GetScript()->ProcessEventsFor(SMART_EVENT_DISENGAGE_CALLBACK);
        mDisengageToggleAutoAttack = false;
        return;
    }

    GetScript()->ProcessEventsFor(
        SMART_EVENT_MOVEMENTINFORM, nullptr, (int)type, Data);
    if (type != movement::gen::point ||
        !HasEscortState(SMART_ESCORT_ESCORTING) ||
        HasEscortState(SMART_ESCORT_PAUSED) || Data < SMART_ESCORT_ID_OFFSET)
        return;
    MovepointReached(Data - SMART_ESCORT_ID_OFFSET);
}

void SmartAI::RemoveAuras()
{
    // Remove non-passive auras that are not casted by m_creature
    m_creature->remove_auras_if([this](AuraHolder* holder)
        {
            return !holder->IsPassive() &&
                   holder->GetCasterGuid() != m_creature->GetObjectGuid();
        });
}

void SmartAI::EnterEvadeMode(bool by_group)
{
    // process creature evade actions
    m_creature->OnEvadeActions(by_group);

    // Totems can be implemented using smart ai; their evade needs to do much
    // less
    if (m_creature->IsTotem())
    {
        m_creature->CombatStop(false);
        return;
    }

    if (!m_creature->isAlive() || mCombatReactionsDisabled)
        return;

    if (!(m_creature->IsPet() && m_creature->GetOwner() &&
            m_creature->GetOwner()->GetTypeId() ==
                TYPEID_PLAYER)) // Play pets dont reset this stuff
    {
        m_creature->remove_auras_on_evade();
        m_creature->DeleteThreatList();
        m_creature->CombatStop(true);
        m_creature->ResetLootRecipients();
    }

    GetScript()->ProcessEventsFor(SMART_EVENT_EVADE); // must be after aura
                                                      // clear so we can cast
                                                      // spells from db

    // If we have no home movement generator (i.e., evade is not used after
    // combat but as part of a script), we push a home generator to our home
    // position.
    if (!m_creature->movement_gens.has(movement::gen::home))
    {
        float x, y, z, o;
        auto idle = dynamic_cast<movement::IdleMovementGenerator*>(
            m_creature->movement_gens.get(movement::gen::idle));
        // Use spawn coords if we have no idel pos
        if (!idle)
        {
            m_creature->GetRespawnCoord(x, y, z, &o);
        }
        // If we have an idle gen, we need to make a home gen to take us there
        // (or creature_addon will not be reapplied).
        else if (idle)
        {
            x = idle->x_;
            y = idle->y_;
            z = idle->z_;
            o = idle->o_;
        }
        m_creature->movement_gens.push(
            new movement::HomeMovementGenerator(x, y, z, o));
    }

    m_creature->movement_gens.on_event(movement::EVENT_LEAVE_COMBAT);

    ResetInternal(SMART_RESET_TYPE_EVADE);
}

void SmartAI::Leashed()
{
    GetScript()->ProcessEventsFor(SMART_EVENT_LEASH);
}

void SmartAI::MoveInLineOfSight(Unit* who)
{
    if (mPassive)
        return;

    if (!who)
        return;

    GetScript()->OnMoveInLineOfSight(who);

    if (use_pet_behavior())
        return;

    if (mCombatReactionsDisabled)
        return;

    if ((m_creature->GetCharmInfo() &&
            (m_creature->GetCharmInfo()->HasReactState(REACT_PASSIVE))) ||
        AssistPlayerInCombat(who))
        return;

    if (!CanAIAttack(who))
        return;

    if (!m_creature->CanStartAttacking(who))
        return;

    if (m_creature->IsWithinAggroDistance(who) &&
        m_creature->IsWithinWmoLOSInMap(who))
    {
        if (!m_creature->getVictim() || mSparring)
        {
            DropSparring(who);
            AttackStart(who);
        }
        else if (m_creature->GetMap()->IsDungeon())
        {
            who->SetInCombatWith(m_creature);
            m_creature->AddThreat(who, 0.0f);
        }
    }
}

bool SmartAI::IsVisible(Unit* pWho) const
{
    return m_creature->IsWithinDist(pWho,
               sWorld::Instance()->getConfig(CONFIG_FLOAT_SIGHT_MONSTER)) &&
           pWho->can_be_seen_by(m_creature, m_creature);
}

bool SmartAI::CanAIAttack(const Unit* /*who*/) const
{
    if (m_creature->GetCharmInfo() &&
        m_creature->GetCharmInfo()->GetReactState() == REACT_PASSIVE)
        return false;
    return true;
}

void SmartAI::AttackedBy(Unit* attacker)
{
    if (mCombatReactionsDisabled)
        return;

    if (use_pet_behavior())
    {
        static_cast<Pet*>(m_creature)->behavior()->attacked(attacker);
        return;
    }

    if (!m_creature->getVictim() || mSparring)
    {
        DropSparring(attacker);
        AttackStart(attacker);
    }
}

bool SmartAI::AssistPlayerInCombat(Unit* who)
{
    if (!who || !who->getVictim())
        return false;

    if (m_creature->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PASSIVE) ||
        m_creature->GetCreatureInfo()->flags_extra &
            CREATURE_FLAG_EXTRA_CIVILIAN)
        return false;

    // experimental (unknown) flag not present
    if (!(m_creature->GetCreatureInfo()->type_flags &
            CREATURE_TYPEFLAGS_CAN_ASSIST))
        return false;

    // not a player
    if (!who->getVictim()->GetCharmerOrOwnerPlayerOrPlayerItself())
        return false;

    // never attack friendly
    if (m_creature->IsFriendlyTo(who))
        return false;

    // too far away and no free sight?
    if (m_creature->IsWithinDistInMap(who, SMART_MAX_AID_DIST) &&
        m_creature->IsWithinWmoLOSInMap(who))
    {
        // already fighting someone?
        if (!m_creature->getVictim())
        {
            AttackStart(who);
            return true;
        }
        else
        {
            who->SetInCombatWith(m_creature);
            m_creature->AddThreat(who, 0.0f);
            return true;
        }
    }

    return false;
}

void SmartAI::GetAIInformation(ChatHandler& reader)
{
    std::string msg =
        "Phase: " + std::to_string(mScript.GetPhase()) + ", Passive: " +
        (mPassive ? "true" : "false") + ", Pacify: " +
        (IsPacified() ? "true" : "false") + ", Active: " +
        (m_creature->isActiveObject() ? "true" : "false") + ", Aggro dist: " +
        std::to_string(m_creature->GetAggroDistance(m_creature));
    if (mSparring)
        msg += ", Sparring: " +
               (m_creature->getVictim() ?
                       m_creature->getVictim()->GetObjectGuid().GetString() :
                       "invalid target");
    reader.PSendSysMessage("%s", msg.c_str());
    auto& events = mScript.GetEvents();
    if (events.size() > 0)
    {
        reader.PSendSysMessage("Events (%u total). Timed ones are:",
            static_cast<int>(events.size()));
        for (const SmartScriptHolder& e : events)
        {
            if (e.timer > 0)
                reader.PSendSysMessage(
                    "Id: %3u: Event: %3u Action: %3u. In %u seconds",
                    e.event_id, e.GetEventType(), e.GetActionType(),
                    e.timer / 1000);
        }
    }
    auto& tal = mScript.GetTimedActionList();
    if (tal.size() > 0)
    {
        reader.PSendSysMessage(
            "Next TAL action (%u total): ", static_cast<int>(tal.size()));
        for (const SmartScriptHolder& e : tal)
        {
            if (e.enableTimed)
                reader.PSendSysMessage(
                    "Id: %3u: Event: %3u Action: %3u. In %u seconds",
                    e.event_id, e.GetEventType(), e.GetActionType(),
                    e.timer / 1000);
        }
    }

    if (use_pet_behavior())
    {
        reader.SendSysMessage("Uses pet behavior, debug info:");
        reader.PSendSysMessage(
            "%s", static_cast<Pet*>(m_creature)->behavior()->debug().c_str());
    }

    reader.PSendSysMessage("BehavioralAI: %s", mBehavioralAI.debug().c_str());
}

void SmartAI::JustRespawned()
{
    mEscortDespawn = -1;
    mEscortState = SMART_ESCORT_NONE;
    ResetInternal(SMART_RESET_TYPE_RESPAWN);
    GetScript()->ProcessEventsFor(SMART_EVENT_RESPAWN);
}

void SmartAI::JustReachedHome()
{
    GetScript()->ProcessEventsFor(SMART_EVENT_REACHED_HOME);
    ResetInternal(SMART_RESET_TYPE_HOME);

    if (HasEscortState(SMART_ESCORT_RETURNING))
        mOOCWPReached = true;
}

void SmartAI::EnterCombat(Unit* enemy)
{
    GetScript()->ProcessEventsFor(SMART_EVENT_AGGRO, enemy);
    m_creature->GetPosition(mLastOOCPos.x, mLastOOCPos.y, mLastOOCPos.z);
    mLastOOCPos.o = m_creature->GetO();
}

void SmartAI::BeforeDeath(Unit* killer)
{
    GetScript()->ProcessEventsFor(SMART_EVENT_BEFORE_DEATH, killer);
}

void SmartAI::JustDied(Unit* killer)
{
    if (use_pet_behavior())
        static_cast<Pet*>(m_creature)->behavior()->died();

    GetScript()->ProcessEventsFor(SMART_EVENT_DEATH, killer);
    if (HasEscortState(SMART_ESCORT_ESCORTING) ||
        HasEscortState(SMART_ESCORT_PAUSED))
    {
        Player* first = nullptr;
        auto itr = mEscorters.begin();
        while (!first && itr != mEscorters.end())
            first = m_creature->GetMap()->GetPlayer(*(itr++));
        if (first && mEscortQuestID)
            first->FailGroupQuest(mEscortQuestID);

        mCanRepeatPath = false; // We cannot repeat if we die
        EndPath(true);
    }
}

void SmartAI::KilledUnit(Unit* victim)
{
    GetScript()->ProcessEventsFor(SMART_EVENT_KILL, victim);
}

void SmartAI::JustSummoned(Creature* creature)
{
    GetScript()->ProcessEventsFor(SMART_EVENT_SUMMONED_UNIT, creature);
}

void SmartAI::AttackStart(Unit* who)
{
    if (!who || mCombatReactionsDisabled || use_pet_behavior())
        return;

    // Don't process new targets while Possessed
    if (m_creature->hasUnitState(UNIT_STAT_CONTROLLED))
    {
        if (auto charmer = m_creature->GetCharmer())
            if (charmer != who)
                return;
    }

    if (mSparring) // Drop current threat
    {
        if (m_creature->getVictim() == who)
            return; // Our sparring target

        m_creature->DeleteThreatList();
        m_creature->CombatStop(true);
        mSparring = false;
    }

    if (who && m_creature->Attack(who, !IsPacified() && mCanAutoAttack))
    {
        RemoveEscortState(SMART_ESCORT_RETURNED);

        m_creature->AddThreat(who);
        m_creature->SetInCombatWith(who);
        who->SetInCombatWith(m_creature);
        if (who->GetTypeId() == TYPEID_UNIT)
            who->AddThreat(m_creature);

        m_creature->movement_gens.on_event(movement::EVENT_ENTER_COMBAT);

        if (mCanCombatMove)
        {
            if (mBehavioralAI.OnAttackStart())
                mBehavioralAIMovingUs =
                    true; // mBehavioralAI will take care of our movement
            else
            {
                if (!m_creature->movement_gens.has(movement::gen::chase))
                    m_creature->movement_gens.push(
                        new movement::ChaseMovementGenerator(),
                        movement::EVENT_LEAVE_COMBAT);
                mBehavioralAIMovingUs = false;
            }
        }
        else
        {
            m_creature->movement_gens.push(
                new movement::StoppedMovementGenerator(),
                movement::EVENT_LEAVE_COMBAT);
        }

        if (!m_creature->movement_gens.has(movement::gen::home))
            m_creature->movement_gens.push(
                new movement::HomeMovementGenerator());
    }
}

void SmartAI::SpellHit(Unit* unit, const SpellEntry* spellInfo)
{
    GetScript()->ProcessEventsFor(
        SMART_EVENT_SPELLHIT, unit, 0, 0, false, spellInfo);
}

void SmartAI::SpellHitTarget(Unit* target, const SpellEntry* spellInfo)
{
    GetScript()->ProcessEventsFor(
        SMART_EVENT_SPELLHIT_TARGET, target, 0, 0, false, spellInfo);
}

void SmartAI::DamageTaken(Unit* doneBy, uint32& damage)
{
    GetScript()->ProcessEventsFor(SMART_EVENT_DAMAGED, doneBy, damage);
    if (mInvincibilityHpLevel &&
        (damage >= m_creature->GetHealth() - mInvincibilityHpLevel))
    {
        damage = 0;
        m_creature->SetHealth(mInvincibilityHpLevel);
    }
}

void SmartAI::HealReceived(Unit* doneBy, uint32& addhealth)
{
    GetScript()->ProcessEventsFor(SMART_EVENT_RECEIVE_HEAL, doneBy, addhealth);
}

void SmartAI::ReceiveEmote(Player* player, uint32 textEmote)
{
    GetScript()->ProcessEventsFor(SMART_EVENT_RECEIVE_EMOTE, player, textEmote);
}

void SmartAI::OnSpawn()
{
    ResetInternal(SMART_RESET_TYPE_SPAWN);
    GetScript()->ProcessEventsFor(SMART_EVENT_SPAWN);
}

void SmartAI::SummonedBy(WorldObject* summoner)
{
    GetScript()->ProcessEventsFor(
        SMART_EVENT_JUST_SUMMONED, (summoner->GetTypeId() == TYPEID_UNIT ||
                                       summoner->GetTypeId() == TYPEID_PLAYER) ?
                                       (Unit*)summoner :
                                       nullptr);
}

void SmartAI::DamageDealt(
    Unit* doneTo, uint32& damage, DamageEffectType /*damagetype*/)
{
    GetScript()->ProcessEventsFor(SMART_EVENT_DAMAGED_TARGET, doneTo, damage);
}

void SmartAI::SummonedCreatureDespawn(Creature* unit)
{
    GetScript()->ProcessEventsFor(SMART_EVENT_SUMMON_DESPAWNED, unit);
}

void SmartAI::UpdateAIWhileCharmed(const uint32 /*diff*/)
{
}

void SmartAI::CorpseRemoved(uint32& respawnDelay)
{
    GetScript()->ProcessEventsFor(
        SMART_EVENT_CORPSE_REMOVED, nullptr, respawnDelay);
}

void SmartAI::InitializeAI()
{
    GetScript()->OnInitialize(m_creature);
    if (!m_creature->isDead())
        ResetInternal(SMART_RESET_TYPE_INIT_AI);
}

void SmartAI::OnCharmed(bool apply)
{
    if (apply)
        GetScript()->ProcessEventsFor(SMART_EVENT_CHARMED, nullptr, 0, 0);
    else
        GetScript()->ProcessEventsFor(SMART_EVENT_CHARM_EXPIRED, nullptr, 0, 0);
}

void SmartAI::DoAction(const int32 param)
{
    GetScript()->ProcessEventsFor(SMART_EVENT_ACTION_DONE, nullptr, param);
}

uint32 SmartAI::GetData(uint32 /*id*/)
{
    return 0;
}

void SmartAI::SetData(uint32 id, uint32 value)
{
    GetScript()->ProcessEventsFor(SMART_EVENT_DATA_SET, nullptr, id, value);
}

void SmartAI::SetGUID(uint64 /*guid*/, int32 /*id*/)
{
}

uint64 SmartAI::GetGUID(int32 /*id*/)
{
    return 0;
}

bool SmartAI::IgnoreTarget(Unit* target) const
{
    if (mBehavioralAI.TurnedOn())
        return mBehavioralAI.IgnoreTarget(target);
    return CreatureAI::IgnoreTarget(target);
}

void SmartAI::SetRun(bool run)
{
    mRun = run;
}

void SmartAI::SetFly(bool fly)
{
    m_creature->SetLevitate(fly);
    m_creature->SendHeartBeat();
}

void SmartAI::SetSwim(bool swim)
{
    if (swim)
        m_creature->m_movementInfo.AddMovementFlag(MOVEFLAG_SWIMMING);
    else
        m_creature->m_movementInfo.RemoveMovementFlag(MOVEFLAG_SWIMMING);
    m_creature->SendHeartBeat();
}

bool SmartAI::OnGossipHello(Player* player)
{
    bool processed =
        GetScript()->ProcessEventsFor(SMART_EVENT_GOSSIP_HELLO, player);
    if (processed)
        player->TalkedToCreature(
            m_creature->GetEntry(), m_creature->GetObjectGuid());
    return processed;
}

bool SmartAI::OnGossipSelect(Player* player, uint32 sender, uint32 id,
    uint32 menuId, const char* /*code*/)
{
    // If this select is from a menu, we use the menu id, if not we use the
    // sender id
    if (menuId)
        GetScript()->ProcessEventsFor(
            SMART_EVENT_GOSSIP_SELECT, player, menuId, id);
    else
        GetScript()->ProcessEventsFor(
            SMART_EVENT_GOSSIP_SELECT, player, sender, id);

    return false; // We use smart ai to react to gossip selections, not to
                  // override the core functionality
}

void SmartAI::OnQuestAccept(Player* player, Quest const* quest)
{
    GetScript()->ProcessEventsFor(
        SMART_EVENT_ACCEPTED_QUEST, player, quest->GetQuestId());
}

void SmartAI::OnQuestReward(Player* player, Quest const* quest)
{
    GetScript()->ProcessEventsFor(
        SMART_EVENT_REWARD_QUEST, player, quest->GetQuestId());
}

bool SmartAI::OnDummyEffect(
    Unit* caster, uint32 spellId, SpellEffectIndex effIndex)
{
    return GetScript()->ProcessEventsFor(
        SMART_EVENT_DUMMY_EFFECT, caster, spellId, (uint32)effIndex);
}

void SmartAI::SetCombatMove(bool on)
{
    if (mCanCombatMove == on)
        return;
    mCanCombatMove = on;
    if (!HasEscortState(SMART_ESCORT_ESCORTING) &&
        !HasEscortState(SMART_ESCORT_PAUSED))
    {
        if (on && m_creature->getVictim())
        {
            if (!m_creature->movement_gens.has(movement::gen::chase))
            {
                m_creature->movement_gens.push(
                    new movement::ChaseMovementGenerator(),
                    movement::EVENT_LEAVE_COMBAT);
                m_creature->CastStop();
            }
            m_creature->movement_gens.push(
                new movement::StoppedMovementGenerator(),
                movement::EVENT_LEAVE_COMBAT);
        }
        else
        {
            m_creature->movement_gens.remove_all(movement::gen::chase);
            m_creature->movement_gens.remove_all(movement::gen::stopped);
        }
    }
}

void SmartAI::SetScript9(SmartScriptHolder& e, uint32 entry, Unit* invoker)
{
    if (invoker)
        GetScript()->mLastInvoker = invoker->GetObjectGuid();
    GetScript()->SetScript9(e, entry);
}

void SmartAI::OnGameEvent(bool start, uint32 eventId)
{
    GetScript()->ProcessEventsFor(
        start ? SMART_EVENT_GAME_EVENT_START : SMART_EVENT_GAME_EVENT_END,
        nullptr, eventId);
}

void SmartAI::OnSpellClick(Unit* clicker)
{
    GetScript()->ProcessEventsFor(SMART_EVENT_ON_SPELLCLICK, clicker);
}

void SmartAI::ToggleBehavioralAI(bool state)
{
    mBehavioralAI.ToggleBehavior(state);
    if (!state)
        mBehavioralAIMovingUs = false;
    else
    {
        if (mBehavioralAI.OnAttackStart())
            mBehavioralAIMovingUs = true;
        else
            mBehavioralAIMovingUs = false;
    }
}

void SmartAI::ChangeBehavioralAI(uint32 behavior)
{
    mBehavioralAI.ChangeBehavior(behavior);
}

void SmartAI::UpdatePassive()
{
    for (const auto& e : GetScript()->GetEvents())
        if (e.event.type == SMART_EVENT_OOC_LOS ||
            e.event.type == SMART_EVENT_IC_LOS)
        {
            mPassive = false;
            return;
        }
    mPassive = m_creature->IsCivilian() || m_creature->IsNeutralToAll();
}

void SmartAI::ClearGroup()
{
    if (mCreatureGroup == 0)
        return;

    auto group = m_creature->GetGroup();

    if (group && group->GetId() == mCreatureGroup)
    {
        group->RemoveMember(m_creature, false);
        if (group->GetSize() == 0)
            m_creature->GetMap()->GetCreatureGroupMgr().DeleteGroup(
                group->GetId());
    }
}

void SmartGameObjectAI::UpdateAI(uint32 diff)
{
    GetScript()->OnUpdate(diff);
}

void SmartGameObjectAI::InitializeAI()
{
    GetScript()->OnInitialize(go);
    GetScript()->ProcessEventsFor(SMART_EVENT_SPAWN);
    Reset();
}

void SmartGameObjectAI::Reset()
{
    GetScript()->OnReset(SMART_RESET_TYPE_INIT_AI);
}

bool SmartGameObjectAI::OnGossipHello(Player* player)
{
    return GetScript()->ProcessEventsFor(
        SMART_EVENT_GOSSIP_HELLO, player, 0, 0, false, nullptr, go);
}

bool SmartGameObjectAI::OnGossipSelect(Player* player, uint32 sender, uint32 id,
    uint32 menuId, const char* /*code*/)
{
    // If this select is from a menu, we use the menu id, if not we use the
    // sender id
    if (menuId)
        GetScript()->ProcessEventsFor(
            SMART_EVENT_GOSSIP_SELECT, player, menuId, id, false, nullptr, go);
    else
        GetScript()->ProcessEventsFor(
            SMART_EVENT_GOSSIP_SELECT, player, sender, id, false, nullptr, go);

    return false; // We don't want to override the default core behavior
}

void SmartGameObjectAI::OnQuestAccept(Player* player, Quest const* quest)
{
    GetScript()->ProcessEventsFor(SMART_EVENT_ACCEPTED_QUEST, player,
        quest->GetQuestId(), 0, false, nullptr, go);
}

void SmartGameObjectAI::OnQuestReward(Player* player, Quest const* quest)
{
    GetScript()->ProcessEventsFor(SMART_EVENT_REWARD_QUEST, player,
        quest->GetQuestId(), 0, false, nullptr, go);
}

uint32 SmartGameObjectAI::GetDialogStatus(Player*)
{
    return 100;
}

void SmartGameObjectAI::SetData(uint32 id, uint32 value)
{
    GetScript()->ProcessEventsFor(SMART_EVENT_DATA_SET, nullptr, id, value);
}

void SmartGameObjectAI::SetScript9(
    SmartScriptHolder& e, uint32 entry, Unit* invoker)
{
    if (invoker)
        GetScript()->mLastInvoker = invoker->GetObjectGuid();
    GetScript()->SetScript9(e, entry);
}

void SmartGameObjectAI::OnGameEvent(bool start, uint32 eventId)
{
    GetScript()->ProcessEventsFor(
        start ? SMART_EVENT_GAME_EVENT_START : SMART_EVENT_GAME_EVENT_END,
        nullptr, eventId);
}

void SmartGameObjectAI::OnStateChanged(uint32 state, Unit* unit)
{
    GetScript()->ProcessEventsFor(SMART_EVENT_GO_STATE_CHANGED, unit, state);
}
