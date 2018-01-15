#include "CreatureGroupMovement.h"
#include "Creature.h"
#include "CreatureAI.h"
#include "CreatureGroup.h"
#include "CreatureGroupMgr.h"
#include "logging.h"
#include "Map.h"
#include "movement/generators.h"
#include "movement/IdleMovementGenerator.h"
#include "movement/PointMovementGenerator.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "ProgressBar.h"
#include "SharedDefines.h"
#include "TemporarySummon.h"
#include "Unit.h"
#include "World.h"
#include "Config/Config.h"
#include "Database/DatabaseEnv.h"
#include <algorithm>
#include <cmath>

#define VISUAL_GROUP_WAYPOINT 2
#define GWP_PAUSED_PRIO 13

CreatureGroupMovement::sql_group_waypoint_map
    CreatureGroupMovement::sqlGroupWaypointMap;

CreatureGroupMovement::~CreatureGroupMovement()
{
    for (auto& elem : m_movementDataMap)
        delete elem.second;
}

void CreatureGroupMovement::LoadWaypointsFromDb()
{
    logging.info("Loading Creature Group Waypoints...");
    uint32 loadedTotal = 0;
    // Make SQL sort them from point 0 to size-1 for us
    std::unique_ptr<QueryResult> result(WorldDatabase.PQuery(
        "SELECT group_id, point, position_x, position_y, position_z, "
        "orientation, "
        "delay, run, mmap FROM creature_group_waypoints ORDER BY group_id ASC, "
        "point ASC"));
    if (result)
    {
        BarGoLink bar(result->GetRowCount());
        do
        {
            ++loadedTotal;

            bar.step();
            Field* fields = result->Fetch();
            GroupWaypointSqlData data;
            data.group_id = fields[0].GetInt32();
            data.point = fields[1].GetInt32();
            data.position_x = fields[2].GetFloat();
            data.position_y = fields[3].GetFloat();
            data.position_z = fields[4].GetFloat();
            data.orientation = fields[5].GetFloat();
            data.delay = fields[6].GetUInt32();
            data.run = fields[7].GetBool();
            data.mmap = fields[8].GetBool();

            CreatureGroupMovement::sqlGroupWaypointMap[data.group_id].push_back(
                data);

        } while (result->NextRow());
    }
    logging.info("Loaded %u Creature Group Waypoints\n", loadedTotal);
}

void CreatureGroupMovement::LoadCreatureGroupMovement(int32 grpId)
{
    auto find = CreatureGroupMovement::sqlGroupWaypointMap.find(grpId);
    if (find != CreatureGroupMovement::sqlGroupWaypointMap.end())
    {
        std::vector<GroupWaypoint> temp;
        for (auto& elem : find->second)
        {
            GroupWaypointSqlData d = elem;
            GroupWaypoint p;
            p.point = d.point;
            p.x = d.position_x;
            p.y = d.position_y;
            p.z = d.position_z;
            p.o = d.orientation;
            p.delay = d.delay;
            p.run = d.run;
            p.mmap = d.mmap;
            temp.push_back(p);
        }
        m_waypointMap.insert(
            std::pair<int32, std::vector<GroupWaypoint>>(find->first, temp));
    }
}

void CreatureGroupMovement::AddCreature(int32 grpId, Creature* creature)
{
    FormationOffset fo;
    fo.lowGuid = creature->GetGUIDLow();
    InitializeFormationOffset(grpId, fo, creature);
    m_formationMap[grpId].push_back(fo);
}

void CreatureGroupMovement::SetNewFormation(int32 grpId, Creature* creature)
{
    RemoveCreature(grpId, creature);
    AddCreature(grpId, creature);
}

void CreatureGroupMovement::ResetAllFormations(int32 grpId)
{
    auto find = m_formationMap.find(grpId);
    if (find == m_formationMap.end())
        return;

    // Not the coolest of designs to do it this way:
    if (CreatureGroup* grp = m_owningMap->GetCreatureGroupMgr().GetGroup(grpId))
    {
        for (auto& elem : *grp)
        {
            // Teleport creature to his spawn position before resetting
            // formation
            float x, y, z, o;
            elem->GetRespawnCoord(x, y, z, &o);
            elem->NearTeleportTo(x, y, z, o);
            SetNewFormation(grpId, elem);
        }
    }
}

void CreatureGroupMovement::RemoveCreature(int32 grpId, Creature* creature)
{
    auto find = m_formationMap.find(grpId);
    if (find == m_formationMap.end())
        return;

    for (auto itr = find->second.begin(); itr != find->second.end(); ++itr)
    {
        if (itr->lowGuid == creature->GetGUIDLow())
        {
            find->second.erase(itr);
            break;
        }
    }
}

bool CreatureGroupMovement::GetMemberOffset(
    int32 grpId, uint32 lowGuid, FormationOffset& offset)
{
    // Get FormationOffset of member
    auto find = m_formationMap.find(grpId);
    if (find == m_formationMap.end())
        return false;
    for (auto& elem : find->second)
    {
        if (elem.lowGuid == lowGuid)
        {
            offset = elem;
            return true;
        }
    }
    return false;
}

void CreatureGroupMovement::InternalDoMovement(
    int32 grpId, std::vector<Creature*> creatures)
{
    if (creatures.empty())
        return;

    // Get movement data if applicable
    GroupMovementData* movementData;
    auto find = m_movementDataMap.find(grpId);
    if (find == m_movementDataMap.end())
        return;
    movementData = find->second;

    // Find the leader
    Creature* leader = nullptr;
    if (movementData->leaderGuid != 0)
        for (std::vector<Creature*>::size_type i = 0; i < creatures.size(); ++i)
            if (creatures[i]->GetGUIDLow() == movementData->leaderGuid)
            {
                leader = creatures[i];
                creatures.erase(creatures.begin() + i);
                break;
            }
    // Pick new leader if he went and died or is in combat
    if (!leader || leader->isInCombat())
    {
        leader = get_leader(grpId, creatures);
        if (leader)
            movementData->leaderGuid = leader->GetGUIDLow();
        if (!leader)
            return;
    }

    // Get waypoint we're headed to
    auto findWp = m_waypointMap.find(grpId);
    if (findWp == m_waypointMap.end())
        return;
    if (movementData->nextPoint >= findWp->second.size())
        movementData->nextPoint = 0;

    FormationOffset offset;
    if (!GetMemberOffset(grpId, leader->GetGUIDLow(), offset))
        return;
    // Get waypoint for leader
    GroupWaypoint wp =
        GetAdjustedWaypoint(grpId, movementData->nextPoint, offset, leader);
    // If waypoint is really close, pick next one
    if (leader->IsWithinDist3d(wp.x, wp.y, wp.z, 2.0f))
    {
        movementData->nextPoint += 1;
        if (movementData->nextPoint >= findWp->second.size())
            movementData->nextPoint = 0;
        wp =
            GetAdjustedWaypoint(grpId, movementData->nextPoint, offset, leader);
    }

    // Calculate distance and time it will take to reach for him
    float dist = leader->GetDistance(wp.x, wp.y, wp.z) +
                 leader->GetObjectBoundingRadius();
    float speed;
    if (wp.run)
        speed = leader->GetSpeedRate(MOVE_RUN);
    else
        speed = leader->GetSpeedRate(MOVE_WALK);

    if (speed <= 0.0f)
        speed = 1.0f;

    float time = dist / speed;

    if (time <= 1.0f)
        time = 1.0f;

    // Make everyone else walk towards their adjusted waypoint in a speed that
    // gives the same travel time as the leader
    for (auto creature : creatures)
    {
        // Get this member's offset
        FormationOffset memberOffset;
        if (!GetMemberOffset(grpId, creature->GetGUIDLow(), memberOffset))
            continue;

        // Get our adjusted waypoint, as well as our speed and start walking or
        // running
        GroupWaypoint myWp = GetAdjustedWaypoint(
            grpId, movementData->nextPoint, memberOffset, creature);
        float d = creature->GetDistance(myWp.x, myWp.y, myWp.z) +
                  creature->GetObjectBoundingRadius();
        float s = d / time;

        if (s <= 0.0f)
            s = wp.run ? 1.15f : 0.85f;

        if (!creature->isInCombat())
        {
            if (wp.run)
                creature->SetSpeedRate(MOVE_RUN, s, true);
            else
                creature->SetSpeedRate(MOVE_WALK, s, true);
        }

        // Pop stopped/gwp/point movement
        creature->movement_gens.remove_if([](const movement::Generator* gen)
            {
                return gen->id() == movement::gen::gwp ||
                       gen->id() == movement::gen::point ||
                       gen->id() == movement::gen::stopped;
            });

        // Remove idle movement generator if any
        // TODO: GWP should have a permanent movement generator active!
        creature->movement_gens.remove_all(movement::gen::idle);

        // Begin movement
        creature->movement_gens.push(
            new movement::GwpMovementGenerator(GWP_POINT_ID, myWp.x, myWp.y,
                myWp.z, creature->isInCombat() ? true : myWp.mmap,
                creature->isInCombat() ? true : wp.run));
    }

    // Set next point
    movementData->currentPoint = movementData->nextPoint;
    movementData->nextPoint += 1;

    // Pop stopped/point movement
    leader->movement_gens.remove_if([](const movement::Generator* gen)
        {
            return gen->id() == movement::gen::gwp ||
                   gen->id() == movement::gen::point ||
                   gen->id() == movement::gen::stopped;
        });

    // Remove idle movement generator if any
    // TODO: GWP should have a permanent movement generator active!
    leader->movement_gens.remove_all(movement::gen::idle);

    // Start moving leader
    leader->movement_gens.push(new movement::GwpMovementGenerator(
        GWP_POINT_ID, wp.x, wp.y, wp.z, wp.mmap, wp.run));
}

void CreatureGroupMovement::StartMovement(
    int32 grpId, const std::vector<Creature*>& creatures)
{
    if (creatures.empty())
        return;

    auto find = m_waypointMap.find(grpId);
    if (find == m_waypointMap.end() || find->second.empty())
        return;

    // Decide on a leader
    Creature* leader = get_leader(grpId, creatures);
    if (!leader)
        leader = *creatures.begin();

    // Select nearest waypoint as our next point
    uint32 point = closest_point(leader, find->second);

    // Update movement data (or create new if none exists)
    GroupMovementData* movementData = make_or_get_move_data(grpId);

    movementData->paused = false;
    movementData->currentPoint =
        (point != 0) ? point - 1 : find->second.size() - 1;
    movementData->leaderGuid = leader->GetGUIDLow();
    movementData->nextPoint = point;

    // Start movement
    InternalDoMovement(grpId, creatures);
}

void CreatureGroupMovement::RestartMovement(
    int32 grpId, const std::vector<Creature*>& creatures)
{
    if (creatures.empty())
        return;

    auto find = m_waypointMap.find(grpId);
    if (find == m_waypointMap.end() || find->second.empty())
        return;

    // Decide on a leader
    Creature* leader = get_leader(grpId, creatures);
    if (!leader)
        leader = *creatures.begin();

    // Update movement data (or create new if none exists)
    GroupMovementData* movementData = make_or_get_move_data(grpId);

    movementData->leaderGuid = leader->GetGUIDLow();

    // Don't resume if movement was delayed or paused
    if (movementData->delayed || movementData->paused)
        return;

    // Start movement (we will continue with the point we were moving to before
    // the evade)
    InternalDoMovement(grpId, creatures);
}

Creature* CreatureGroupMovement::get_leader(
    int32 grpId, const std::vector<Creature*>& creatures)
{
    Creature* leader = *creatures.begin();
    // If we have a prefered leader, use him instead (if he's available)
    if (CreatureGroup* grp = (*creatures.begin())
                                 ->GetMap()
                                 ->GetCreatureGroupMgr()
                                 .GetGroup(grpId))
        if (creature_group_entry* entry = grp->GetGroupEntry())
            if (entry->movement_leader_guid)
                for (const auto& creature : creatures)
                    if (!creature->isInCombat() &&
                        creature->GetGUIDLow() == entry->movement_leader_guid)
                    {
                        leader = creature;
                        break;
                    }
    return leader;
}

uint32 CreatureGroupMovement::closest_point(
    Creature* leader, const std::vector<GroupWaypoint>& wps)
{
    float maxDist = 100 * 1000;
    uint32 point = 0;
    for (std::vector<GroupWaypoint>::size_type i = 0; i < wps.size(); ++i)
    {
        float dist = leader->GetDistance(wps[i].x, wps[i].y, wps[i].z);
        if (dist < maxDist)
        {
            point = i;
            maxDist = dist;
        }
    }
    return point;
}

GroupMovementData* CreatureGroupMovement::make_or_get_move_data(int32 grpId)
{
    GroupMovementData* movementData;
    auto findData = m_movementDataMap.find(grpId);
    if (findData == m_movementDataMap.end())
    {
        movementData = new GroupMovementData;
        m_movementDataMap.insert(
            std::pair<int32, GroupMovementData*>(grpId, movementData));
    }
    else
        movementData = findData->second;
    return movementData;
}

void CreatureGroupMovement::UpdateMovement(
    int32 grpId, const std::vector<Creature*>& creatures, bool notfiyEvent)
{
    if (creatures.empty())
        return;

    // Don't update if we're paused
    auto findData = m_movementDataMap.find(grpId);
    if (findData != m_movementDataMap.end() && findData->second->paused)
        return;

    // clear delayed state
    findData->second->delayed = false;

    auto findWp = m_waypointMap.find(grpId);
    if (findWp == m_waypointMap.end() || findWp->second.empty())
        return;

    if (!notfiyEvent)
    {
        if (findData != m_movementDataMap.end())
        {
            uint32 point = findData->second->currentPoint;

            // Movement inform all the creatures
            for (auto c : creatures)
            {
                if (c->AI())
                    c->AI()->MovementInform(movement::gen::gwp, point + 1);

                if (c->IsTemporarySummon())
                {
                    TemporarySummon* summon = (TemporarySummon*)c;
                    if (summon->GetSummonerGuid().IsCreature())
                        if (Creature* summoner = c->GetMap()->GetCreature(
                                summon->GetSummonerGuid()))
                            if (summoner->AI())
                                summoner->AI()->SummonedMovementInform(
                                    summon, movement::gen::gwp, point + 1);
                }
            }

            // Check so script didn't remove our movement flag
            if (CreatureGroup* group =
                    m_owningMap->GetCreatureGroupMgr().GetGroup(grpId))
                if (!group->HasFlag(CREATURE_GROUP_FLAG_GROUP_MOVEMENT))
                    return;

            // Check if the walked waypoint had delay
            if (findWp->second.size() > point)
                if (findWp->second[point].delay > 0)
                {
                    // Schedule a re-update in "delay" amount of milliseconds
                    findData->second->delayed = true;
                    m_owningMap->GetCreatureGroupMgr().ScheduleMovementUpdate(
                        grpId, findWp->second[point].delay);
                    return;
                }
        }
    }

    // Start movement
    InternalDoMovement(grpId, creatures);
}

GroupWaypoint CreatureGroupMovement::GetAdjustedWaypoint(
    int32 grpId, uint32 point, FormationOffset& offset, Creature* ref)
{
    GroupWaypoint wp;
    wp.point = point;

    auto find = m_waypointMap.find(grpId);
    if (find == m_waypointMap.end())
        return wp;
    if (point >= find->second.size())
        return wp;

    wp.delay = find->second[point].delay;
    wp.x = find->second[point].x;
    wp.y = find->second[point].y;
    wp.z = find->second[point].z;
    wp.o = find->second[point].o;
    wp.run = find->second[point].run;
    wp.mmap = find->second[point].mmap;

    if (wp.mmap)
    {
        // Calculate using angle of waypoint and our angle combined
        auto pos = ref->GetPointXYZ(
            G3D::Vector3(wp.x, wp.y, wp.z), wp.o - offset.angle, offset.dist);
        wp.x = pos.x;
        wp.y = pos.y;
        wp.z = pos.z;
    }
    else
    {
        // Calculate using angle of waypoint and our angle combined
        wp.x = find->second[point].x + cos(wp.o - offset.angle) * offset.dist;
        wp.y = find->second[point].y + sin(wp.o - offset.angle) * offset.dist;
    }

    return wp;
}

void CreatureGroupMovement::AddWaypoint(
    int32 grpId, const DynamicWaypoint& wp, bool mmap /*= false*/)
{
    auto find = m_waypointMap.find(grpId);
    if (find == m_waypointMap.end())
    {
        // If the group didn't have waypoint before we need to add the group and
        // update the find iterator
        std::vector<GroupWaypoint> temp;
        m_waypointMap.insert(
            std::pair<int32, std::vector<GroupWaypoint>>(grpId, temp));
        find = m_waypointMap.find(grpId);
    }

    // Update formation if this will be the first waypoint
    bool setFormation = false;
    if (find->second.empty())
        setFormation = true;

    // Add the waypoint
    GroupWaypoint grpWp;
    grpWp.x = wp.X;
    grpWp.y = wp.Y;
    grpWp.z = wp.Z;
    grpWp.o = wp.O;
    grpWp.delay = wp.delay;
    grpWp.run = wp.run;
    grpWp.mmap = mmap;
    grpWp.point = find->second.size();
    find->second.push_back(grpWp);
    // If non-temporary group id, insert into SQL:
    if (grpId > 0)
    {
        WorldDatabase.PExecute(
            "INSERT INTO creature_group_waypoints (group_id, point, "
            "position_x, position_y, position_z, orientation, delay) "
            "VALUES(%i, %i, %f, %f, %f, %f, %i)",
            grpId, grpWp.point, grpWp.x, grpWp.y, grpWp.z, grpWp.o,
            grpWp.delay);
    }

    // Update formation if first wp just got added
    if (setFormation)
        ResetAllFormations(grpId);
}

void CreatureGroupMovement::DeleteWaypoint(int32 grpId, uint32 point)
{
    auto find = m_waypointMap.find(grpId);
    if (find != m_waypointMap.end())
    {
        if (point >= find->second.size())
            return;

        // Erase point from memory and SQL (if not temporary)
        find->second.erase(find->second.begin() + point);
        if (grpId > 0)
            WorldDatabase.PExecute(
                "DELETE FROM creature_group_waypoints WHERE group_id=%i AND "
                "point = %i",
                grpId, point);

        if (point >= find->second.size())
            return; // No points come after

        // Update all points that come after this point
        for (std::vector<GroupWaypoint>::size_type i = point;
             i < find->second.size(); ++i)
            find->second[i].point -= 1;

        // Also update all edit points, if any GM edit points exist
        if (m_displayedWps.size() > 0)
            for (auto& elem : m_displayedWps)
                if (elem.second.grpId == grpId && elem.second.point >= point)
                    elem.second.point -= 1;

        // Commit to SQL if group is not temporary
        if (grpId > 0)
            WorldDatabase.PExecute(
                "UPDATE creature_group_waypoints SET point = point - 1 WHERE "
                "group_id=%i AND point > %i",
                grpId, point);
    }
}

void CreatureGroupMovement::MoveWaypoint(
    int32 grpId, uint32 point, float x, float y, float z, float o)
{
    auto find = m_waypointMap.find(grpId);
    if (find != m_waypointMap.end())
    {
        if (point >= find->second.size())
            return;

        find->second[point].x = x;
        find->second[point].y = y;
        find->second[point].z = z;
        find->second[point].o = o;

        if (grpId > 0)
            WorldDatabase.PExecute(
                "UPDATE creature_group_waypoints SET position_x=%f, "
                "position_y=%f, position_z=%f, orientation=%f WHERE "
                "group_id=%i AND point=%i",
                x, y, z, o, grpId, point);
    }
}

void CreatureGroupMovement::GetWaypoints(
    int32 grpId, std::vector<GroupWaypoint>& waypoints)
{
    auto find = m_waypointMap.find(grpId);
    if (find != m_waypointMap.end())
    {
        waypoints.reserve(find->second.size());
        waypoints.insert(
            waypoints.begin(), find->second.begin(), find->second.end());
    }
}

void CreatureGroupMovement::InitializeFormationOffset(
    int32 grpId, FormationOffset& offset, Creature* creature)
{
    // Calculate dist and angle if we have a first group wp, if not it means
    // we're calling
    // without having any waypoints, and once we add some ResetAllFormations()
    // will recalculate this
    auto itr = m_waypointMap.find(grpId);
    if (itr != m_waypointMap.end() && !itr->second.empty())
    {
        // Get values for waypoint
        float x = itr->second[0].x;
        float y = itr->second[0].y;
        float o = itr->second[0].o;

        // Get delta x (note: order is important due to angle calculation)
        float dx = creature->GetX() - x;
        float dy = creature->GetY() - y;

        // Get angle between point and creature
        float angle = atan2(dy, dx);
        if (angle < 0)
            angle += 2 * M_PI_F;
        // Remove point's orientation from angle
        offset.angle = o - angle;
        if (offset.angle < 0)
            offset.angle += 2 * M_PI_F;

        // Get distance
        offset.dist = sqrt((dx * dx) + (dy * dy));
        if (offset.dist < 0)
            offset.dist = 0;
    }
}

bool CreatureGroupMovement::IsLeader(int32 grpId, Creature* creature)
{
    auto find = m_movementDataMap.find(grpId);
    if (find == m_movementDataMap.end())
        return false;
    return (find->second->leaderGuid == creature->GetGUIDLow());
}

/* GM commands (for editing) */

bool CreatureGroupMovement::DisplayAllWps(int32 grpId, Player* summoner)
{
    auto itr = m_waypointMap.find(grpId);
    if (itr == m_waypointMap.end() || itr->second.empty())
        return false;

    // Get a list of already spawned points
    std::vector<uint32> spawnedPoints;
    spawnedPoints.reserve(itr->second.size()); // Max possible size
    for (auto& elem : m_displayedWps)
        if (elem.second.grpId)
            spawnedPoints.push_back(elem.second.point);

    for (std::vector<GroupWaypoint>::size_type i = 0; i < itr->second.size();
         ++i)
    {
        // Skip if point is already spawned
        if (std::find(spawnedPoints.begin(), spawnedPoints.end(), i) !=
            spawnedPoints.end())
            continue;

        Creature* creature = summoner->SummonCreature(VISUAL_GROUP_WAYPOINT,
            itr->second[i].x, itr->second[i].y, itr->second[i].z,
            itr->second[i].o, TEMPSUMMON_MANUAL_DESPAWN, 0);
        if (!creature)
            return false;
        creature->SetVisibility(VISIBILITY_OFF);
        DisplayedWaypoint dWp;
        dWp.grpId = grpId;
        dWp.point = i;
        dWp.wpGuid = creature->GetObjectGuid();
        m_displayedWps.insert(
            std::pair<uint32, DisplayedWaypoint>(creature->GetGUIDLow(), dWp));
    }
    return true;
}

bool CreatureGroupMovement::HideAllWps(int32 grpId)
{
    auto itr = m_waypointMap.find(grpId);
    if (itr == m_waypointMap.end() || itr->second.empty())
        return false;

    for (auto itr = m_displayedWps.begin(); itr != m_displayedWps.end();)
    {
        if (itr->second.grpId == grpId)
        {
            if (Creature* creature =
                    m_owningMap->GetCreature(itr->second.wpGuid))
                creature->AddObjectToRemoveList();
            itr = m_displayedWps.erase(itr);
            continue;
        }
        ++itr;
    }
    return true;
}

bool CreatureGroupMovement::RemoveWp(Creature* waypoint)
{
    auto itr = m_displayedWps.find(waypoint->GetGUIDLow());
    if (itr == m_displayedWps.end())
        return false;
    DeleteWaypoint(itr->second.grpId, itr->second.point);
    waypoint->AddObjectToRemoveList();
    return true;
}

bool CreatureGroupMovement::MoveWp(Creature* waypoint)
{
    auto itr = m_displayedWps.find(waypoint->GetGUIDLow());
    if (itr == m_displayedWps.end())
        return false;
    MoveWaypoint(itr->second.grpId, itr->second.point, waypoint->GetX(),
        waypoint->GetY(), waypoint->GetZ(), waypoint->GetO());
    return true;
}

bool CreatureGroupMovement::SetRun(Creature* waypoint, bool run)
{
    auto itr = m_displayedWps.find(waypoint->GetGUIDLow());
    if (itr == m_displayedWps.end())
        return false;
    auto wpItr = m_waypointMap.find(itr->second.grpId);
    if (wpItr == m_waypointMap.end() ||
        wpItr->second.size() <= itr->second.point)
        return false;
    wpItr->second[itr->second.point].run = run;
    if (itr->second.grpId > 0)
        WorldDatabase.PExecute(
            "UPDATE creature_group_waypoints SET run=%i WHERE group_id=%i AND "
            "point=%i",
            run, itr->second.grpId, itr->second.point);
    return true;
}

bool CreatureGroupMovement::SetDelay(Creature* waypoint, uint32 delay)
{
    auto itr = m_displayedWps.find(waypoint->GetGUIDLow());
    if (itr == m_displayedWps.end())
        return false;
    auto wpItr = m_waypointMap.find(itr->second.grpId);
    if (wpItr == m_waypointMap.end() ||
        wpItr->second.size() <= itr->second.point)
        return false;
    wpItr->second[itr->second.point].delay = delay;
    if (itr->second.grpId > 0)
        WorldDatabase.PExecute(
            "UPDATE creature_group_waypoints SET delay=%i WHERE group_id=%i "
            "AND point=%i",
            delay, itr->second.grpId, itr->second.point);
    return true;
}

void CreatureGroupMovement::PauseMovement(int32 grpId,
    const std::vector<Creature*>& creatures, bool /*force*/ /* = false */)
{
    // Ignore if we have no movement data
    auto findData = m_movementDataMap.find(grpId);
    if (findData == m_movementDataMap.end())
        return;

    // Make sure all members are OOC
    for (auto& creature : creatures)
        if (creature->isInCombat())
            return;

    findData->second->paused = true;

    // Push stopped gen
    for (auto& creature : creatures)
        if (creature->movement_gens.top_id() != movement::gen::distract &&
            !creature->movement_gens.has(movement::gen::stopped))
            creature->movement_gens.push(
                new movement::StoppedMovementGenerator(),
                movement::EVENT_ENTER_COMBAT, GWP_PAUSED_PRIO);
}

void CreatureGroupMovement::TryResumeMovement(
    int32 grpId, const std::vector<Creature*>& creatures)
{
    // Ignore if we have no movement data
    auto findData = m_movementDataMap.find(grpId);
    if (findData == m_movementDataMap.end())
        return;

    // Do not resume movement if someone is still stunned
    for (auto& creature : creatures)
        if ((creature)->HasAuraType(SPELL_AURA_MOD_STUN))
            return;

    findData->second->paused = false;

    // Make next point current
    findData->second->nextPoint = findData->second->currentPoint;

    // Start movement
    InternalDoMovement(grpId, creatures);
}

void CreatureGroupMovement::ClearPausedFlag(int32 grpId)
{
    auto fd = m_movementDataMap.find(grpId);
    if (fd != m_movementDataMap.end())
        fd->second->paused = false;
}

uint32 CreatureGroupMovement::GetNumberOfWaypoints(int32 grpId)
{
    auto itr = m_waypointMap.find(grpId);
    if (itr == m_waypointMap.end() || itr->second.empty())
        return 0;
    return itr->second.size();
}

void CreatureGroupMovement::RemoveGroupData(int32 grpId)
{
    m_waypointMap.erase(grpId);
    m_formationMap.erase(grpId);

    auto itr = m_movementDataMap.find(grpId);
    if (itr != m_movementDataMap.end())
    {
        delete itr->second;
        m_movementDataMap.erase(itr);
    }
}
