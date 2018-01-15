/* Copyright (C) 2014 Corecraft <https://www.worldofcorecraft.com>
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

#include "hellfire_dark_portal_event.h"
#include "outland.h"
#include "precompiled.h"

static const auto pit_guid =
    ObjectGuid(HIGHGUID_UNIT, NPC_ID_PIT_COMMANDER, 1007120);
static const auto trigger_guid = ObjectGuid(HIGHGUID_UNIT, 21252, 98026);

dark_portal_event::dark_portal_event(outland_instance_data* o)
  : inst_data(o), paused_(false), pause_timer_(2000)
{
    spawn_group(0, fel_groups::soldiers_shaadraz, 10000);
    spawn_group(1, fel_groups::soldiers_murketh, 10000);

    spawn_group(2, fel_groups::wrathmaster, 30000);
    spawn_group(3, fel_groups::wrathmaster, 50000);

    spawn_group(4, fel_groups::pit_commander, 40000);

    spawn_group(5, fel_groups::soldiers_shaadraz, 90000);
    spawn_group(6, fel_groups::soldiers_murketh, 110000);

    spawn_group(7, fel_groups::wrathmaster, 140000);
    spawn_group(8, fel_groups::wrathmaster, 140000);
}

dark_portal_event::~dark_portal_event()
{
    // Don't remove group; group system might already be deleted
    for (auto& pair : groups_)
        pair.second.inst_data = nullptr;
}

void dark_portal_event::update(uint32 diff)
{
    std::vector<std::pair<uint32, fel_groups>> to_spawn;

    // Only update event when players are nearby
    pause_timer_ -= diff;
    if (pause_timer_ <= 0)
    {
        pause_timer_ = 2000;
        if (auto c = inst_data->instance->GetCreature(trigger_guid))
        {
            if (GetClosestPlayer(c, 200.0f) != nullptr)
                paused_ = false;
            else
                paused_ = true;
        }
    }

    auto b = groups_.begin();
    auto e = groups_.end();
    for (auto& g : groups_)
    {
        assert(b == groups_.begin());
        assert(e == groups_.end());
        if (g.second.all_dead())
            to_spawn.push_back(std::make_pair(g.first, g.second.group));
        else
            g.second.update(diff, paused_);
    }

    for (auto p : to_spawn)
        spawn_group(p.first, p.second,
            p.second == fel_groups::pit_commander ? 60000 : 10000);
}

void dark_portal_event::spawn_group(uint32 id, fel_groups group, uint32 timer)
{
    groups_.erase(id);
    groups_[id].inst_data = inst_data;
    groups_[id].dp_event = this;
    groups_[id].timer = timer;
    groups_[id].group = group;
    groups_[id].grp_id =
        inst_data->instance->GetCreatureGroupMgr().CreateNewGroup(
            std::string("dark portal #") + std::to_string(id), true);
}

void dark_portal_event::line_add(Creature* c)
{
    // four lines of zig-zag patterns, like this:
    // 0     4
    //    1     5
    // 2     6
    //    3     7

    static const G3D::Vector3 front_start(-220, 1150, 41.6);
    static const float x_delta = -7.5; // Decrease between points
    static const float y_delta = 6;    // Increase between rows

    int i;
    for (i = 0; i < 36; ++i)
    {
        if (position[i].IsEmpty())
            break;
        auto c = inst_data->instance->GetCreature(position[i]);
        if (!c || c->isDead())
        {
            position[i].Clear();
            break;
        }
    }

    G3D::Vector3 pos(front_start.x, front_start.y, front_start.z);
    int line = i % 4;
    pos.x += (i / 4) * x_delta;
    if (line == 1 || line == 3)
        pos.x += x_delta / 2.0f;
    pos.y += y_delta * line;

    position[i] = c->GetObjectGuid();

    std::vector<DynamicWaypoint> wp{{pos.x, pos.y, pos.z, 4.7}};
    c->movement_gens.push(
        new movement::DynamicWaypointMovementGenerator(wp, false));
    c->movement_gens.push(
        new movement::IdleMovementGenerator(pos.x, pos.y, pos.z, 4.7));
}

void dark_portal_event::line_remove(Creature* c)
{
    auto itr =
        std::find(std::begin(position), std::end(position), c->GetObjectGuid());
    if (itr != std::end(position))
        itr->Clear();
}

void dark_portal_event::commence_attack(Creature* c)
{
    if (c->AI())
        c->AI()->Notify(1);
}

dark_portal_event::group_data::~group_data()
{
    if (inst_data)
        inst_data->instance->GetCreatureGroupMgr().DeleteGroup(grp_id);
}

void dark_portal_event::group_data::update(uint32 diff, bool paused)
{
    if (stage == -1)
        return;

    if (timer <= diff)
    {
        timer = 0;       // can be modified to a non-zero timer in do_stage
        int s = stage++; // can be modified to another stage in do_stage
        do_stage(s, paused);
    }
    else
        timer -= diff;
}

Creature* dark_portal_event::group_data::spawn_creature(
    uint32 id, float x, float y, float z)
{
    Creature* c = nullptr;

    // Pit commander is static since he needs special visbility
    if (id == NPC_ID_PIT_COMMANDER)
    {
        c = inst_data->instance->GetCreature(pit_guid);
        if (c && c->isDead())
            c->Respawn(true);
    }
    else
    {
        c = inst_data->instance->SummonCreature(id, x, y, z, 0,
            TEMPSUMMON_CORPSE_TIMED_DESPAWN, 8000, SUMMON_OPT_ACTIVE);
    }

    if (c)
    {
        c->movement_gens.remove_all(movement::gen::idle);
        members.push_back(c->GetObjectGuid());
    }

    return c;
}

void dark_portal_event::group_data::move_dynpoints(
    Creature* c, const DynamicWaypoint* begin, const DynamicWaypoint* end)
{
    std::vector<DynamicWaypoint> wps(begin, end);
    c->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_PASSIVE);
    c->movement_gens.push(
        new movement::DynamicWaypointMovementGenerator(wps, false));
}

void dark_portal_event::group_data::group_move(
    const DynamicWaypoint* begin, const DynamicWaypoint* end)
{
    auto grp = inst_data->instance->GetCreatureGroupMgr().GetGroup(grp_id);
    if (!grp)
        return;

    grp->AddFlag(CREATURE_GROUP_FLAG_GROUP_MOVEMENT);

    std::vector<Creature*> cv;
    for (auto guid : members)
        if (auto c = inst_data->instance->GetCreature(guid))
        {
            c->RemoveFlag(
                UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_PASSIVE);
            cv.push_back(c);
            grp->AddMember(c, false);
        }

    // Add waypoints
    for (auto itr = begin; itr != end; ++itr)
        inst_data->instance->GetCreatureGroupMgr().GetMovementMgr().AddWaypoint(
            grp_id, *itr, true);

    // Update formations
    for (auto c : cv)
        inst_data->instance->GetCreatureGroupMgr()
            .GetMovementMgr()
            .SetNewFormation(grp_id, c);

    // Begin movement
    inst_data->instance->GetCreatureGroupMgr().GetMovementMgr().StartMovement(
        grp_id, cv);
}

bool dark_portal_event::group_data::all_idling()
{
    std::size_t waiting = 0;
    for (auto guid : members)
        if (auto c = inst_data->instance->GetCreature(guid))
            if (c->movement_gens.empty())
                ++waiting;
    if (members.size() == waiting)
        return true;
    return false;
}

bool dark_portal_event::group_data::all_dead()
{
    if (members.empty())
        return false;

    uint32 dead = 0;
    for (auto guid : members)
    {
        auto c = inst_data->instance->GetCreature(guid);
        if (!c || !c->isAlive())
            ++dead;
    }
    if (members.size() == dead)
        return true;
    return false;
}

void dark_portal_event::group_data::do_stage(int s, bool paused)
{
    switch (group)
    {
    case fel_groups::soldiers_shaadraz:
        do_soldiers_shaadraz(s, paused);
        break;
    case fel_groups::soldiers_murketh:
        do_soldiers_murketh(s, paused);
        break;
    case fel_groups::wrathmaster:
        do_wrathmaster(s, paused);
        break;
    case fel_groups::pit_commander:
        do_pit_commander(s, paused);
        break;
    }
}

static const DynamicWaypoint shaadraz_one[] = {
    DynamicWaypoint(-288.1, 1527.3, 33.0, 0.1, 0, false),
    DynamicWaypoint(-273.7, 1529.2, 31.5, 0.2, 0, false),
    DynamicWaypoint(-260.0, 1532.8, 30.6, 0.3, 0, false),
    DynamicWaypoint(-249.6, 1536.2, 28.8, 0.3, 0, false),
    DynamicWaypoint(-242.9, 1537.9, 25.4, 5.9, 0, false),
    DynamicWaypoint(-214.2, 1526.5, 23.5, 4.5, 60000, false)};

static const DynamicWaypoint shaadraz_two[] = {
    DynamicWaypoint(-288.1, 1527.3, 33.0, 0.1, 0, false),
    DynamicWaypoint(-273.7, 1529.2, 31.5, 0.2, 0, false),
    DynamicWaypoint(-260.0, 1532.8, 30.6, 0.3, 0, false),
    DynamicWaypoint(-249.6, 1536.2, 28.8, 0.3, 0, false),
    DynamicWaypoint(-242.9, 1537.9, 25.4, 5.9, 0, false),
    DynamicWaypoint(-222.7, 1528.9, 23.9, 4.5, 60000, false)};

static const DynamicWaypoint shaadraz_three[] = {
    DynamicWaypoint(-288.1, 1527.3, 33.0, 0.1, 0, false),
    DynamicWaypoint(-273.7, 1529.2, 31.5, 0.2, 0, false),
    DynamicWaypoint(-260.0, 1532.8, 30.6, 0.3, 0, false),
    DynamicWaypoint(-249.6, 1536.2, 28.8, 0.3, 0, false),
    DynamicWaypoint(-242.9, 1537.9, 25.4, 5.9, 0, false),
    DynamicWaypoint(-230.9, 1531.0, 24.2, 4.5, 60000, false)};

static const DynamicWaypoint shaadraz_four[] = {
    DynamicWaypoint(-288.1, 1527.3, 33.0, 0.1, 0, false),
    DynamicWaypoint(-273.7, 1529.2, 31.5, 0.2, 0, false),
    DynamicWaypoint(-260.0, 1532.8, 30.6, 0.3, 0, false),
    DynamicWaypoint(-249.6, 1536.2, 28.8, 0.3, 0, false),
    DynamicWaypoint(-242.9, 1537.9, 25.4, 5.9, 0, false),
    DynamicWaypoint(-238.2, 1532.8, 24.4, 4.5, 60000, false)};

static const DynamicWaypoint shaadraz_gwp[] = {
    DynamicWaypoint(-227.1, 1529.9, 24.0, 4.5, 0, false),
    DynamicWaypoint(-227.7, 1519.4, 22.8, 4.6, 0, false),
    DynamicWaypoint(-229.8, 1504.2, 21.0, 4.6, 0, false),
    DynamicWaypoint(-232.9, 1490.6, 19.2, 4.5, 0, false),
    DynamicWaypoint(-237.3, 1474.0, 16.9, 4.5, 0, false),
    DynamicWaypoint(-241.0, 1456.9, 15.0, 4.5, 0, false),
    DynamicWaypoint(-245.8, 1442.1, 14.4, 4.6, 0, false),
    DynamicWaypoint(-249.8, 1426.3, 13.6, 4.6, 0, false),
    DynamicWaypoint(-252.5, 1408.5, 12.2, 4.6, 0, false),
    DynamicWaypoint(-252.3, 1386.5, 11.1, 4.8, 0, false),
    DynamicWaypoint(-250.9, 1365.3, 10.9, 4.8, 0, false),
    DynamicWaypoint(-248.3, 1341.8, 12.9, 4.8, 0, false),
    DynamicWaypoint(-247.5, 1320.7, 16.1, 4.8, 0, false),
    DynamicWaypoint(-243.6, 1299.7, 19.6, 4.9, 0, false),
    DynamicWaypoint(-240.7, 1277.0, 23.6, 4.8, 0, false),
    DynamicWaypoint(-240.3, 1255.7, 27.3, 4.7, 0, false),
    DynamicWaypoint(-240.4, 1236.8, 30.5, 4.7, 0, false),
    DynamicWaypoint(-241.5, 1219.8, 34.0, 4.7, 0, false),
    DynamicWaypoint(-244.8, 1208.0, 38.8, 4.7, 0, false),
    DynamicWaypoint(-245.6, 1194.5, 42.9, 4.7, 0, false),
    DynamicWaypoint(-245.4, 1184.8, 42.3, 4.7, 60000, false)};

void dark_portal_event::group_data::do_soldiers_shaadraz(
    int s, bool delay_attack)
{
    switch (s)
    {
    case 1:
        if (auto c = spawn_creature(NPC_ID_FEL_SOLDIER, -300, 1525, 37.41))
            move_dynpoints(c, std::begin(shaadraz_one), std::end(shaadraz_one));
        timer = 3000;
        break;
    case 2:
        if (auto c = spawn_creature(NPC_ID_FEL_SOLDIER, -300, 1525, 37.41))
            move_dynpoints(c, std::begin(shaadraz_two), std::end(shaadraz_two));
        timer = 3000;
        break;
    case 3:
        if (auto c = spawn_creature(NPC_ID_FEL_SOLDIER, -300, 1525, 37.41))
            move_dynpoints(
                c, std::begin(shaadraz_three), std::end(shaadraz_three));
        timer = 3000;
        break;
    case 4:
        if (auto c = spawn_creature(NPC_ID_FEL_SOLDIER, -300, 1525, 37.41))
            move_dynpoints(
                c, std::begin(shaadraz_four), std::end(shaadraz_four));
        timer = 10000;
        break;
    case 5:
        if (all_idling())
            group_move(std::begin(shaadraz_gwp), std::end(shaadraz_gwp));
        else
        {
            timer = 1000;
            stage = 5;
        }
        break;
    case 6:
        if (!all_idling())
        {
            stage = 6;
            timer = 1000;
        }
        break;
    case 7:
    {
        auto grp = inst_data->instance->GetCreatureGroupMgr().GetGroup(grp_id);
        if (grp)
            grp->RemoveFlag(CREATURE_GROUP_FLAG_GROUP_MOVEMENT);

        for (auto guid : members)
        {
            if (auto c = inst_data->instance->GetCreature(guid))
            {
                dp_event->line_add(c);

                // Remove creature from group
                if (grp)
                    grp->RemoveMember(c, false);
                c->SetActiveObjectState(false);
            }
        }

        timer = urand(30000, 120000);
        break;
    }
    case 8:
    {
        if (delay_attack)
        {
            stage = 8;
            timer = urand(10000, 240000);
            return;
        }
        for (auto guid : members)
        {
            if (auto c = inst_data->instance->GetCreature(guid))
            {
                c->SetActiveObjectState(true);
                dp_event->line_remove(c);
                dp_event->commence_attack(c);
            }
        }
        stage = -1;
        break;
    }
    }
}

static const DynamicWaypoint murketh_one[] = {
    DynamicWaypoint(-159.8, 1513.1, 28.9, 3.1, 0, false),
    DynamicWaypoint(-180.8, 1514.0, 27.3, 3.1, 0, false),
    DynamicWaypoint(-194.8, 1514.6, 27.2, 3.3, 0, false),
    DynamicWaypoint(-205.1, 1512.7, 23.1, 2.9, 0, false),
    DynamicWaypoint(-238.9, 1521.4, 23.2, 4.4, 0, false)};

static const DynamicWaypoint murketh_two[] = {
    DynamicWaypoint(-159.8, 1513.1, 28.9, 3.1, 0, false),
    DynamicWaypoint(-180.8, 1514.0, 27.3, 3.1, 0, false),
    DynamicWaypoint(-194.8, 1514.6, 27.2, 3.3, 0, false),
    DynamicWaypoint(-205.1, 1512.7, 23.1, 2.9, 0, false),
    DynamicWaypoint(-231.5, 1520.4, 23.0, 4.4, 0, false)};

static const DynamicWaypoint murketh_three[] = {
    DynamicWaypoint(-159.8, 1513.1, 28.9, 3.1, 0, false),
    DynamicWaypoint(-180.8, 1514.0, 27.3, 3.1, 0, false),
    DynamicWaypoint(-194.8, 1514.6, 27.2, 3.3, 0, false),
    DynamicWaypoint(-205.1, 1512.7, 23.1, 2.9, 0, false),
    DynamicWaypoint(-223.2, 1519.0, 22.7, 4.4, 0, false)};

static const DynamicWaypoint murketh_four[] = {
    DynamicWaypoint(-159.8, 1513.1, 28.9, 3.1, 0, false),
    DynamicWaypoint(-180.8, 1514.0, 27.3, 3.1, 0, false),
    DynamicWaypoint(-194.8, 1514.6, 27.2, 3.3, 0, false),
    DynamicWaypoint(-205.1, 1512.7, 23.1, 2.9, 0, false),
    DynamicWaypoint(-215.2, 1517.6, 22.4, 4.4, 0, false)};

static const DynamicWaypoint murketh_gwp[] = {
    DynamicWaypoint(-227.7, 1519.4, 22.8, 4.6, 0, false),
    DynamicWaypoint(-229.8, 1504.2, 21.0, 4.6, 0, false),
    DynamicWaypoint(-232.9, 1490.6, 19.2, 4.5, 0, false),
    DynamicWaypoint(-237.3, 1474.0, 16.9, 4.5, 0, false),
    DynamicWaypoint(-241.0, 1456.9, 15.0, 4.5, 0, false),
    DynamicWaypoint(-245.8, 1442.1, 14.4, 4.6, 0, false),
    DynamicWaypoint(-249.8, 1426.3, 13.6, 4.6, 0, false),
    DynamicWaypoint(-252.5, 1408.5, 12.2, 4.6, 0, false),
    DynamicWaypoint(-252.3, 1386.5, 11.1, 4.8, 0, false),
    DynamicWaypoint(-250.9, 1365.3, 10.9, 4.8, 0, false),
    DynamicWaypoint(-248.3, 1341.8, 12.9, 4.8, 0, false),
    DynamicWaypoint(-247.5, 1320.7, 16.1, 4.8, 0, false),
    DynamicWaypoint(-243.6, 1299.7, 19.6, 4.9, 0, false),
    DynamicWaypoint(-240.7, 1277.0, 23.6, 4.8, 0, false),
    DynamicWaypoint(-240.3, 1255.7, 27.3, 4.7, 0, false),
    DynamicWaypoint(-240.4, 1236.8, 30.5, 4.7, 0, false),
    DynamicWaypoint(-241.5, 1219.8, 34.0, 4.7, 0, false),
    DynamicWaypoint(-244.8, 1208.0, 38.8, 4.7, 0, false),
    DynamicWaypoint(-245.6, 1194.5, 42.9, 4.7, 0, false),
    DynamicWaypoint(-245.4, 1184.8, 42.3, 4.7, 60000, false)};

void dark_portal_event::group_data::do_soldiers_murketh(
    int s, bool delay_attack)
{
    switch (s)
    {
    case 1:
        if (auto c = spawn_creature(NPC_ID_FEL_SOLDIER, -150, 1510, 33.1))
            move_dynpoints(c, std::begin(murketh_one), std::end(murketh_one));
        timer = 3000;
        break;
    case 2:
        if (auto c = spawn_creature(NPC_ID_FEL_SOLDIER, -150, 1510, 33.1))
            move_dynpoints(c, std::begin(murketh_two), std::end(murketh_two));
        timer = 3000;
        break;
    case 3:
        if (auto c = spawn_creature(NPC_ID_FEL_SOLDIER, -150, 1510, 33.1))
            move_dynpoints(
                c, std::begin(murketh_three), std::end(murketh_three));
        timer = 3000;
        break;
    case 4:
        if (auto c = spawn_creature(NPC_ID_FEL_SOLDIER, -150, 1510, 33.1))
            move_dynpoints(c, std::begin(murketh_four), std::end(murketh_four));
        timer = 10000;
        break;
    case 5:
        if (all_idling())
        {
            group_move(std::begin(murketh_gwp), std::end(murketh_gwp));
            timer = 1000;
        }
        else
        {
            timer = 1000;
            stage = 5;
        }
        break;
    case 6:
        if (!all_idling())
        {
            stage = 6;
            timer = 1000;
        }
        break;
    case 7:
    {
        auto grp = inst_data->instance->GetCreatureGroupMgr().GetGroup(grp_id);
        if (grp)
            grp->RemoveFlag(CREATURE_GROUP_FLAG_GROUP_MOVEMENT);

        for (auto guid : members)
        {
            if (auto c = inst_data->instance->GetCreature(guid))
            {
                dp_event->line_add(c);

                // Remove creature from group
                if (grp)
                    grp->RemoveMember(c, false);
                c->SetActiveObjectState(false);
            }
        }

        timer = urand(30000, 120000);
        break;
    }
    case 8:
    {
        if (delay_attack)
        {
            stage = 8;
            timer = urand(10000, 240000);
            return;
        }
        for (auto guid : members)
        {
            if (auto c = inst_data->instance->GetCreature(guid))
            {
                c->SetActiveObjectState(true);
                dp_event->line_remove(c);
                dp_event->commence_attack(c);
            }
        }
        stage = -1;
        break;
    }
    }
}

static const DynamicWaypoint wrathmaster_gwp[] = {
    DynamicWaypoint(-200.1, 1689.0, 50.7, 4.7, 0, false),
    DynamicWaypoint(-199.2, 1672.2, 46.8, 4.7, 0, false),
    DynamicWaypoint(-199.3, 1655.0, 43.1, 4.7, 0, false),
    DynamicWaypoint(-201.0, 1633.4, 38.9, 4.6, 0, false),
    DynamicWaypoint(-204.5, 1610.8, 34.6, 4.6, 0, false),
    DynamicWaypoint(-209.3, 1588.3, 30.7, 4.5, 0, false),
    DynamicWaypoint(-216.4, 1562.3, 27.4, 4.4, 0, false),
    DynamicWaypoint(-222.8, 1537.8, 24.8, 4.5, 0, false),
    DynamicWaypoint(-227.8, 1514.9, 22.2, 4.5, 0, false),
    DynamicWaypoint(-233.1, 1491.0, 19.3, 4.5, 0, false),
    DynamicWaypoint(-238.9, 1464.3, 15.7, 4.5, 0, false),
    DynamicWaypoint(-244.9, 1439.1, 14.2, 4.5, 0, false),
    DynamicWaypoint(-249.8, 1419.2, 13.1, 4.5, 0, false),
    DynamicWaypoint(-252.9, 1396.5, 11.6, 4.7, 0, false),
    DynamicWaypoint(-252.1, 1375.9, 10.8, 4.8, 0, false),
    DynamicWaypoint(-251.3, 1354.5, 11.5, 4.8, 0, false),
    DynamicWaypoint(-248.3, 1331.7, 14.3, 4.9, 0, false),
    DynamicWaypoint(-244.0, 1305.6, 18.7, 4.9, 0, false),
    DynamicWaypoint(-240.7, 1276.8, 23.6, 4.8, 0, false),
    DynamicWaypoint(-239.8, 1247.8, 28.6, 4.7, 0, false),
    DynamicWaypoint(-240.5, 1222.6, 33.3, 4.7, 0, false),
    DynamicWaypoint(-242.2, 1207.6, 38.6, 4.6, 0, false),
    DynamicWaypoint(-244.2, 1192.9, 42.9, 4.7, 0, false),
    DynamicWaypoint(-245.8, 1183.9, 42.2, 4.7, 60000, false)};

void dark_portal_event::group_data::do_wrathmaster(int s, bool delay_attack)
{
    switch (s)
    {
    case 1:
        spawn_creature(NPC_ID_WRATH_MASTER, -200.5, 1683.4, 49.4);
        spawn_creature(NPC_ID_FEL_SOLDIER, -204.3, 1688.6, 50.9);
        spawn_creature(NPC_ID_FEL_SOLDIER, -206.0, 1692.5, 51.9);
        spawn_creature(NPC_ID_FEL_SOLDIER, -196.7, 1688.1, 50.3);
        spawn_creature(NPC_ID_FEL_SOLDIER, -194.5, 1691.3, 50.9);
        break;
    case 2:
        group_move(std::begin(wrathmaster_gwp), std::end(wrathmaster_gwp));
        timer = 1000;
        break;
    case 3:
        if (!all_idling())
        {
            timer = 1000;
            stage = 3;
        }
        break;
    case 4:
    {
        auto grp = inst_data->instance->GetCreatureGroupMgr().GetGroup(grp_id);
        if (grp)
            grp->RemoveFlag(CREATURE_GROUP_FLAG_GROUP_MOVEMENT);

        for (auto guid : members)
        {
            if (auto c = inst_data->instance->GetCreature(guid))
            {
                dp_event->line_add(c);

                // Remove creature from group
                if (grp)
                    grp->RemoveMember(c, false);
                c->SetActiveObjectState(false);
            }
        }

        timer = urand(30000, 120000);
        break;
    }
    case 5:
    {
        if (delay_attack)
        {
            stage = 8;
            timer = urand(30000, 120000);
            return;
        }
        for (auto guid : members)
        {
            if (auto c = inst_data->instance->GetCreature(guid))
            {
                c->SetActiveObjectState(true);
                dp_event->line_remove(c);
                dp_event->commence_attack(c);
            }
        }
        stage = -1;
        break;
    }
    }
}

void dark_portal_event::group_data::do_pit_commander(int s, bool delay_attack)
{
    switch (s)
    {
    case 1:
    {
        // Pit commander uses SmartAI escort movement to allow being aggrod and
        // continue path
        if (auto c = spawn_creature(NPC_ID_PIT_COMMANDER, -200, 1700, 53.5))
        {
            c->queue_action(1000, [c]()
                {
                    if (c->AI())
                        c->AI()->Notify(1);
                });
        }
        stage = 2;
        timer = 1000;
        break;
    }
    case 2:
    {
        // Wait until pit commander reaches dark portal
        if (auto c = inst_data->instance->GetCreature(pit_guid))
        {
            if (c->movement_gens.top_id() != movement::gen::idle)
            {
                stage = 2;
                timer = 1000;
            }
        }
        break;
    }
    case 3:
    {
        // Pit commander is active, deactivate him if event is paused
        if (delay_attack)
        {
            if (auto c = inst_data->instance->GetCreature(pit_guid))
            {
                c->SetActiveObjectState(false);
                stage = 4;
                timer = 1000;
            }
        }
        break;
    }
    case 4:
    {
        // Pit commander is inactive, activate him if event is not paused
        if (!delay_attack)
        {
            if (auto c = inst_data->instance->GetCreature(pit_guid))
            {
                c->SetActiveObjectState(true);
                stage = 3;
                timer = 1000;
            }
        }
        break;
    }
    }
}
