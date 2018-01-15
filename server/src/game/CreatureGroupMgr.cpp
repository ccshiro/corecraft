#include "CreatureGroupMgr.h"
#include "Creature.h"
#include "CreatureAI.h"
#include "logging.h"
#include "Map.h"
#include "ProgressBar.h"
#include "SharedDefines.h"
#include "TemporarySummon.h"
#include "Unit.h"
#include "World.h"
#include "Config/Config.h"
#include "Database/DatabaseEnv.h"

CreatureGroupMgr::sql_group_map CreatureGroupMgr::sqlGroups;
CreatureGroupMgr::sql_lowguid_map CreatureGroupMgr::sqlGroupMembers;

void CreatureGroupMgr::LoadAllGroupsFromDb()
{
    // Load member low guids
    std::unique_ptr<QueryResult> member_res(WorldDatabase.PQuery(
        "SELECT group_id, creature_guid FROM creature_group_members"));
    uint32 count = 0;
    std::map<uint32, uint32> group_sizes;
    if (member_res)
    {
        BarGoLink bar(member_res->GetRowCount());
        do
        {
            bar.step();
            Field* fields = member_res->Fetch();
            uint32 grpid = fields[0].GetUInt32();
            sqlGroupMembers[fields[1].GetUInt32()] = grpid;
            group_sizes[grpid] += 1;
            ++count;
        } while (member_res->NextRow());
    }
    logging.info("Loaded %u creature group members", count);

    std::unique_ptr<QueryResult> result(WorldDatabase.PQuery(
        "SELECT id, map, special_flags, leader_guid, movement_leader_guid, "
        "name FROM creature_groups"));
    count = 0;
    if (result)
    {
        BarGoLink bar(result->GetRowCount());
        do
        {
            bar.step();
            Field* fields = result->Fetch();
            creature_group_entry grpEntry;
            grpEntry.id = fields[0].GetInt32();
            grpEntry.map = fields[1].GetUInt32();
            grpEntry.special_flags = fields[2].GetUInt32();
            grpEntry.leader_guid = fields[3].GetUInt32();
            grpEntry.movement_leader_guid = fields[4].GetUInt32();
            grpEntry.group_name = fields[5].GetCppString();
            grpEntry.size = group_sizes[grpEntry.id];

            CreatureGroupMgr::sqlGroups.insert(
                std::pair<uint32, creature_group_entry>(
                    grpEntry.map, grpEntry));
            ++count;

        } while (result->NextRow());
    }
    logging.info("Loaded %u creature groups", count);

    // Load Group Movements too
    CreatureGroupMovement::LoadWaypointsFromDb();
}

CreatureGroupMgr::CreatureGroupMgr(Map* map)
  : m_owningMap(map), m_nextTemporaryId(-1), m_creatureGroupMovement(map),
    m_mgrUpdate(false)
{
    // Create all groups for map
    typedef std::pair<sql_group_map::iterator, sql_group_map::iterator>
        value_range;
    value_range range = CreatureGroupMgr::sqlGroups.equal_range(map->GetId());
    for (auto itr = range.first; itr != range.second; ++itr)
    {
        LOG_DEBUG(logging, "Map %i loading Creature Group with Id: %i",
            map->GetId(), itr->second.id);
        auto pGrp = new CreatureGroup(&itr->second, false);
        m_groupMap[itr->second.id] = pGrp;
        if (pGrp->HasFlag(CREATURE_GROUP_FLAG_GROUP_MOVEMENT))
            m_creatureGroupMovement.LoadCreatureGroupMovement(itr->second.id);
    }
}

CreatureGroupMgr::~CreatureGroupMgr()
{
    for (auto& elem : m_groupMap)
        delete elem.second;
}

void CreatureGroupMgr::ScheduleMovementUpdate(int32 grpId, uint32 delay)
{
    DelayNotify n;
    n.grpId = grpId;
    n.type = DELAY_NOTIFY_UPDATE;
    if (m_mgrUpdate)
        m_queuedDelay.push_back(std::pair<uint32, DelayNotify>(delay, n));
    else
        m_movementDelayNotifier.push_back(
            std::pair<uint32, DelayNotify>(delay, n));
}

void CreatureGroupMgr::UpdateGroupMgr(const uint32& diff)
{
    m_mgrUpdate = true;

    for (auto itr = m_movementDelayNotifier.begin();
         itr != m_movementDelayNotifier.end();)
    {
        if (itr->first <= diff)
        {
            if (CreatureGroup* pGroup = GetGroup(itr->second.grpId))
            {
                std::vector<Creature*> creatures;
                if (GetCreatureVectorForMovement(pGroup, creatures))
                {
                    if (itr->second.type == DELAY_NOTIFY_UPDATE)
                        m_creatureGroupMovement.UpdateMovement(
                            itr->second.grpId, creatures, true);
                    else if (itr->second.type == DELAY_NOTIFY_RESUME)
                        m_creatureGroupMovement.TryResumeMovement(
                            itr->second.grpId, creatures);
                }
                itr = m_movementDelayNotifier.erase(itr);
                continue;
            }
        }
        else
            itr->first -= diff;

        ++itr;
    }

    // Insert queued updates that happened during our manager update
    for (auto& p : m_queuedDelay)
        m_movementDelayNotifier.push_back(p);
    m_queuedDelay.clear();

    m_mgrUpdate = false;
}

void CreatureGroupMgr::OnAddToWorld(Creature* pCreature)
{
    if (sqlGroupMembers.find(pCreature->GetGUIDLow()) != sqlGroupMembers.end())
    {
        if (CreatureGroup* pGroup =
                m_groupMap[sqlGroupMembers[pCreature->GetGUIDLow()]])
        {
            pGroup->AddMember(pCreature, false);
            if (pGroup->HasFlag(CREATURE_GROUP_FLAG_GROUP_MOVEMENT))
            {
                m_creatureGroupMovement.AddCreature(pGroup->GetId(), pCreature);
                // Are we a full group and should start movement? (only do for
                // non-temporary groups)
                if (!pGroup->IsTemporaryGroup() &&
                    pGroup->GetSize() >= pGroup->GetMaxSize())
                    ProcessGroupEvent(
                        pGroup->GetId(), CREATURE_GROUP_EVENT_MOVEMENT_BEGIN);
            }
            pCreature->SetGroup(pGroup);
        }
    }
}

void CreatureGroupMgr::OnRemoveFromWorld(Creature* pCreature)
{
    if (sqlGroupMembers.find(pCreature->GetGUIDLow()) != sqlGroupMembers.end())
    {
        if (CreatureGroup* pGroup =
                m_groupMap[sqlGroupMembers[pCreature->GetGUIDLow()]])
        {
            pGroup->RemoveMember(pCreature, false);
            if (pGroup->HasFlag(CREATURE_GROUP_FLAG_GROUP_MOVEMENT))
                m_creatureGroupMovement.RemoveCreature(
                    pGroup->GetId(), pCreature);
            pCreature->SetGroup(nullptr);
        }
    }
}

CreatureGroup* CreatureGroupMgr::GetGroup(int32 id)
{
    auto itr = m_groupMap.find(id);
    if (itr == m_groupMap.end())
        return nullptr;
    return itr->second;
}

int32 CreatureGroupMgr::CreateNewGroup(const std::string& name, bool temporary)
{
    creature_group_entry entry;
    if (temporary)
        entry.id = m_nextTemporaryId--;
    entry.leader_guid = 0;
    entry.map = m_owningMap->GetId();
    entry.special_flags = 0;
    entry.group_name = name;

    // Insert into sql if the group is permanent
    if (!temporary)
    {
        std::string temp(entry.group_name);
        WorldDatabase.escape_string(temp);
        if (!WorldDatabase.DirectPExecute(
                "INSERT INTO creature_groups (map, name) VALUES(%i, \"%s\")",
                m_owningMap->GetId(), temp.c_str()))
            return 0;
        else
        {
            // We do not get the ID returned by the sql interface, instead we
            // use the
            // name and map to select it; we order by id in case there are
            // duplicate entries
            std::unique_ptr<QueryResult> result(WorldDatabase.PQuery(
                "SELECT id FROM creature_groups WHERE map=%i AND name=\"%s\" "
                "ORDER BY id DESC LIMIT 1",
                m_owningMap->GetId(), temp.c_str()));
            if (result)
            {
                Field* idField = result->Fetch();
                entry.id = idField[0].GetUInt32();
            }
            else
                return 0;
        }
    }

    // Until server is restarted this group is temporary as well,
    // difference is that this group is also inserted into SQL
    auto pGrp = new CreatureGroup(&entry, true);
    m_groupMap[entry.id] = pGrp;
    return entry.id;
}

bool CreatureGroupMgr::DeleteGroup(int32 id)
{
    // Check so group belogs to us
    auto itr = m_groupMap.find(id);
    if (itr == m_groupMap.end())
        return false;

    // Remove group id for all creatures in this instance of the map
    for (auto& elem : *itr->second)
        elem->SetGroup(nullptr);

    // Remove any lingering movement data for group
    GetMovementMgr().RemoveGroupData(id);

    // Delete the group (note: needs restart to take global effect)
    delete itr->second;
    m_groupMap.erase(itr);

    // For permanent groups we need to delete the SQL too
    if (id > 0)
    {
        WorldDatabase.PExecute(
            "DELETE FROM creature_group_members WHERE group_id=%i", id);
        WorldDatabase.PExecute("DELETE FROM creature_groups WHERE id=%i", id);
    }

    return true;
}

void CreatureGroupMgr::ProcessGroupEvent(int32 groupId,
    CREATURE_GROUP_EVENT groupEvent, Unit* pTarget /* = NULL */, uint32 timeout)
{
    if (groupId == 0)
        return;

    CreatureGroup* pGroup = GetGroup(groupId);
    if (!pGroup)
        return;

    switch (groupEvent)
    {
    // Cause all group members to aggro on aggro (don't invoke this event for
    // their aggroing, though)
    case CREATURE_GROUP_EVENT_AGGRO:
    {
        if (!pTarget)
            return;

        m_creatureGroupMovement.ClearPausedFlag(pGroup->GetId());

        std::vector<Creature*> creatures;

        bool friendly_check = pTarget->player_or_pet();
        for (auto& elem : *pGroup)
        {
            if (elem->IsControlledByPlayer() || !elem->isAlive())
                continue;
            creatures.push_back(elem);

            // Check so target is eligable for attacking, if target is a player
            // or
            // player pet we can always attack if not friendly,
            // but if it's a creature we need to be hostile towards that
            // creature
            // (given that mobs friendly, or netural,
            // to eachother can spar with one another)
            if (friendly_check)
            {
                if (elem->IsFriendlyTo(pTarget))
                    return;
            }
            else
            {
                if (!elem->IsHostileTo(pTarget))
                    return;
            }
        }

        for (auto c : creatures)
        {
            // Start attack if we're not in combat, with no victim, or attacking
            // a non-player / pet
            bool attack_start =
                !c->isInCombat() ||
                (!c->getVictim() ||
                    (c->getVictim()->GetTypeId() != TYPEID_PLAYER &&
                        !(c->getVictim()->GetCharmerOrOwner() &&
                            c->getVictim()->GetCharmerOrOwner()->GetTypeId() ==
                                TYPEID_PLAYER)));
            c->SetInCombatWith(pTarget);
            pTarget->SetInCombatWith(c);
            c->AddThreat(pTarget, 0, false, SPELL_SCHOOL_MASK_NONE, nullptr,
                true); // Mark as by group

            if (attack_start && c->AI())
                c->AI()->AttackStart(pTarget);

            // If creature is part of a movement group we must restore his walk
            // and run speed to the initial values
            if (pGroup->HasFlag(CREATURE_GROUP_FLAG_GROUP_MOVEMENT))
            {
                c->SetSpeedRate(
                    MOVE_WALK, c->GetCreatureInfo()->speed_walk, true);
                c->SetSpeedRate(
                    MOVE_RUN, c->GetCreatureInfo()->speed_run, true);
            }
        }
        break;
    }
    // Evade all members of group
    case CREATURE_GROUP_EVENT_EVADE:
    {
        if (!pTarget->isAlive())
            return;

        m_creatureGroupMovement.ClearPausedFlag(pGroup->GetId());

        // Check if we need to respawn any mobs due to our leader
        bool isRespawnForcingLeaderAlive = false;
        if (pGroup->HasFlag(CREATURE_GROUP_FLAG_LEADER_RESPAWN_ALL))
            if (Creature* pLeader = pGroup->GetLeader())
                if (pLeader->isAlive())
                    isRespawnForcingLeaderAlive = true;

        for (auto itr = pGroup->begin(); itr != pGroup->end(); ++itr)
        {
            auto pCreature = *itr;

            if (pCreature == pTarget)
                continue;

            if (pCreature->IsControlledByPlayer())
                continue;

            if (!pCreature->isAlive())
            {
                // Should we respawn this dead member?
                if (pGroup->HasFlag(
                        CREATURE_GROUP_FLAG_RESPAWN_ALL_ON_SURVIVOR) ||
                    isRespawnForcingLeaderAlive)
                {
                    // remove the corpse
                    pCreature->Respawn(true);
                    // make creature reappear as a "new" entity in a few
                    // seconds
                    pCreature->SetRespawnTime(3);
                }

                continue;
            }

            if (pCreature->AI())
                pCreature->AI()->EnterEvadeMode(true);
        }
        break;
    }
    case CREATURE_GROUP_EVENT_DEATH:
        if (!pTarget)
            return;

        // If leader dies and we should despawn on his death, we do so
        if (pGroup->HasFlag(CREATURE_GROUP_FLAG_LEADER_DESPAWN_ALL) &&
            pGroup->GetLeader() == pTarget)
        {
            for (auto& elem : *pGroup)
            {
                if (elem->isAlive())
                {
                    if (elem->IsTemporarySummon())
                        ((TemporarySummon*)elem)->UnSummon();
                    else
                        elem->ForcedDespawn();
                }
            }
        }
        break;
    case CREATURE_GROUP_EVENT_RESPAWN:
    {
        if (!pTarget || pTarget->GetTypeId() != TYPEID_UNIT)
            return;

        bool respawnAll =
            pGroup->HasFlag(CREATURE_GROUP_FLAG_RESPAWN_ALL_ON_SURVIVOR);
        if (!respawnAll && pGroup->GetLeader())
            if (Creature* leader = pGroup->GetLeader())
                if (leader->isAlive())
                    respawnAll = true;

        if (respawnAll)
        {
            for (auto& member : pGroup->GetMembers())
                if (!(member)->isAlive() &&
                    (member)->GetRespawnTimeEx() >
                        WorldTimer::time_no_syscall()) // Check so respawn
                                                       // hasn't already been
                                                       // invoked
                    (member)->Respawn();               // Respawn doesn't invoke
            // group_event_respawn; the actual
            // spawning does
        }

        // Restart movement if need be
        if (pGroup->HasFlag(CREATURE_GROUP_FLAG_GROUP_MOVEMENT))
        {
            std::vector<Creature*> creatures;
            if (GetCreatureVectorForMovement(pGroup, creatures))
                m_creatureGroupMovement.StartMovement(
                    pGroup->GetId(), creatures);
        }

        break;
    }
    case CREATURE_GROUP_EVENT_MOVEMENT_BEGIN:
        if (pGroup->HasFlag(CREATURE_GROUP_FLAG_GROUP_MOVEMENT))
        {
            std::vector<Creature*> creatures;
            if (GetCreatureVectorForMovement(pGroup, creatures))
                m_creatureGroupMovement.StartMovement(
                    pGroup->GetId(), creatures);
        }
        break;
    case CREATURE_GROUP_EVENT_MOVEMENT_UPDATE:
        if (!pTarget)
            return;
        if (pGroup->HasFlag(CREATURE_GROUP_FLAG_GROUP_MOVEMENT))
        {
            if (pTarget->GetTypeId() == TYPEID_UNIT)
            {
                std::vector<Creature*> creatures;
                if (GetCreatureVectorForMovement(pGroup, creatures))
                    m_creatureGroupMovement.UpdateMovement(
                        pGroup->GetId(), creatures);
            }
        }
        break;
    case CREATURE_GROUP_EVENT_MOVEMENT_PAUSE:
        if (pGroup->HasFlag(CREATURE_GROUP_FLAG_GROUP_MOVEMENT))
        {
            if (timeout)
            {
                DelayNotify n;
                n.grpId = pGroup->GetId();
                n.type = DELAY_NOTIFY_RESUME;
                m_movementDelayNotifier.push_back(
                    std::pair<uint32, DelayNotify>(timeout, n));
            }
            std::vector<Creature*> creatures;
            if (GetCreatureVectorForMovement(pGroup, creatures))
                m_creatureGroupMovement.PauseMovement(
                    pGroup->GetId(), creatures);
        }
        break;
    case CREATURE_GROUP_EVENT_MOVEMENT_RESUME:
        if (pGroup->HasFlag(CREATURE_GROUP_FLAG_GROUP_MOVEMENT))
        {
            std::vector<Creature*> creatures;
            if (GetCreatureVectorForMovement(pGroup, creatures))
                m_creatureGroupMovement.TryResumeMovement(
                    pGroup->GetId(), creatures);
        }
        break;

    default:
        break;
    }
}

bool CreatureGroupMgr::GetCreatureVectorForMovement(
    CreatureGroup* pGroup, std::vector<Creature*>& creatures)
{
    // Get all creatures still alive:
    creatures.reserve(pGroup->end() - pGroup->begin());
    for (auto& elem : *pGroup)
    {
        auto pCreature = elem;
        if (pCreature->isAlive())
        {
            // NOTE: Keep including NPC even if in combat, so that he has an
            // up to date catch-up-with group position for no combat groups
            if (pCreature->isInCombat() &&
                !pGroup->HasFlag(CREATURE_GROUP_FLAG_MOVEMENT_NO_COMBAT))
                return false;

            if (pCreature->movement_gens.top_id() == movement::gen::home)
                return false;

            if (pCreature->HasAuraType(SPELL_AURA_MOD_STUN) &&
                !pGroup->HasFlag(CREATURE_GROUP_FLAG_MOVEMENT_NO_COMBAT))
                return false;

            if (pCreature->movement_gens.has(movement::gen::gwp) &&
                !pCreature->isInCombat())
                return false;

            creatures.push_back(pCreature);
        }
    }
    return true;
}

void CreatureGroupMgr::ClearCurrentPauses(int32 grpId)
{
    for (auto itr = m_movementDelayNotifier.begin();
         itr != m_movementDelayNotifier.end();)
    {
        if (itr->second.grpId != grpId)
        {
            ++itr;
            continue;
        }

        if (itr->second.type == DELAY_NOTIFY_RESUME)
            itr = m_movementDelayNotifier.erase(itr);
        else
            ++itr;
    }
}

void CreatureGroupMgr::PauseMovementOfGroup(
    int32 grpId, uint32 delayInMilliseconds)
{
    CreatureGroup* grp = GetGroup(grpId);
    std::vector<Creature*> creatures;
    if (grp && GetCreatureVectorForMovement(grp, creatures))
    {
        m_creatureGroupMovement.PauseMovement(grpId, creatures, true);
        DelayNotify n;
        n.grpId = grpId;
        n.type = DELAY_NOTIFY_RESUME;
        m_movementDelayNotifier.push_back(
            std::pair<uint32, DelayNotify>(delayInMilliseconds, n));
    }
}

bool CreatureGroupMgr::GetRespawnPositionOfCreature(
    int32 grpId, Creature* pCreature, float& x, float& y, float& z, float& o)
{
    CreatureGroup* grp = GetGroup(grpId);
    if (!grp || !grp->HasFlag(CREATURE_GROUP_FLAG_GROUP_MOVEMENT))
        return false;

    std::vector<Creature*> creatures;
    if (!GetCreatureVectorForMovement(grp, creatures) || creatures.empty())
        return false;

    // Make sure we're not already in the vector (impossible if we're dead)
    if (std::find(creatures.begin(), creatures.end(), pCreature) !=
        creatures.end())
        return false;

    Creature* alive = creatures[0];

    // Get alive's and creature's formation offset in order to calculate the
    // imaginary wp as well as the resulting x, y
    FormationOffset aliveOffset, myOffset;
    if (!m_creatureGroupMovement.GetMemberOffset(
            grpId, alive->GetGUIDLow(), aliveOffset) ||
        !m_creatureGroupMovement.GetMemberOffset(
            grpId, pCreature->GetGUIDLow(), myOffset))
        return false;

    // Calculate the imaginary WP (In other words, the x,y of the position that
    // would be if a waypoint was placed here)
    float wpX, wpY, aliveO = alive->GetO();
    wpX = alive->GetX() +
          cos(aliveO - aliveOffset.angle + M_PI_F) * aliveOffset.dist;
    wpY = alive->GetY() +
          sin(aliveO - aliveOffset.angle + M_PI_F) * aliveOffset.dist;

    // Get my offset to the imaginary waypoint
    x = wpX + cos(aliveO - myOffset.angle) * myOffset.dist;
    y = wpY + sin(aliveO - myOffset.angle) * myOffset.dist;
    z = alive->GetZ();
    o = alive->GetO();

    // Schedule a movement refresh
    DelayNotify n;
    n.grpId = grpId;
    n.type = DELAY_NOTIFY_RESUME;
    m_movementDelayNotifier.push_back(std::pair<uint32, DelayNotify>(500, n));

    return true;
}
