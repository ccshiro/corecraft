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

#include "alterac_valley.h"
#include "precompiled.h"

#define TOTAL_RIDERS 10
static float alliance_riders[TOTAL_RIDERS][4]{// front row, from east to west
    {615.6, -51.7, 42.0, 0.3}, {614.6, -49.1, 42.2, 0.3},
    {613.5, -46.4, 42.2, 0.3}, {612.6, -43.4, 42.2, 0.3},
    {611.0, -40.8, 42.0, 0.3},
    // back row, from east to west
    {611.2, -53.1, 41.6, 0.3}, {610.2, -50.5, 41.7, 0.3},
    {608.8, -47.8, 41.7, 0.3}, {608.2, -44.7, 41.7, 0.3},
    {606.4, -42.1, 41.0, 0.3}};

// commander's wp path out to his men
static DynamicWaypoint alliance_commander_walk[] = {
    DynamicWaypoint(605.5, -20.4, 43.2), DynamicWaypoint(615.4, -34.7, 42.4),
    DynamicWaypoint(618.4, -43.3, 42.3, 3.2)};

static float horde_riders[TOTAL_RIDERS][4]{
    // front row, from west to east
    {-1226.6, -620.0, 52.1, 2.4}, {-1228.7, -622.7, 52.4, 2.3},
    {-1231.0, -625.2, 52.6, 2.3}, {-1233.3, -627.8, 52.9, 2.3},
    {-1236.0, -629.5, 52.7, 2.3},
    // back row, from west to east
    {-1223.7, -623.3, 52.7, 2.3}, {-1226.0, -625.6, 53.2, 2.3},
    {-1228.4, -628.0, 53.7, 2.3}, {-1230.9, -630.3, 53.9, 2.3},
    {-1233.5, -632.6, 53.6, 2.3},
};

// commander's wp path out to his men
static DynamicWaypoint horde_commander_walk[] = {
    DynamicWaypoint(-1246.9, -628.1, 52.2),
    DynamicWaypoint(-1234.2, -619.8, 51.6),
    DynamicWaypoint(-1224.8, -609.2, 51.7, 4.6)};

void alterac_valley::do_cavalry(Team team, Creature* commander)
{
    if (team == ALLIANCE)
        ally_cavalry_cd = WorldTimer::time_no_syscall() + 30 * MINUTE;
    else
        horde_cavalry_cd = WorldTimer::time_no_syscall() + 30 * MINUTE;

    commander->RemoveFlag(
        UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP | UNIT_NPC_FLAG_QUESTGIVER);
    commander->SetActiveObjectState(true);

    int32 grp_id = instance->GetCreatureGroupMgr().CreateNewGroup(
        team == ALLIANCE ? "alliance cavalry" : "horde cavalry", true);
    auto grp = instance->GetCreatureGroupMgr().GetGroup(grp_id);
    if (!grp)
        return;

    float(*riders)[4] = team == ALLIANCE ? alliance_riders : horde_riders;
    uint32 rider_entry = team == ALLIANCE ? 13576 : 13440;
    cavalry_data* data =
        team == ALLIANCE ? &cavalry_data_[0] : &cavalry_data_[1];
    data->reset(this); // remove data from previous (if any) event

    for (int i = 0; i < TOTAL_RIDERS; ++i)
    {
        if (auto c = commander->SummonCreature(rider_entry, riders[i][0],
                riders[i][1], riders[i][2], riders[i][3],
                TEMPSUMMON_CORPSE_TIMED_DESPAWN, 10 * IN_MILLISECONDS,
                SUMMON_OPT_ACTIVE | SUMMON_OPT_NO_LOOT))
        {
            c->SetFlag(
                UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_PASSIVE);
            data->riders.push_back(c->GetObjectGuid());
            grp->AddMember(c, false);
        }
    }

    std::vector<DynamicWaypoint> wps(
        std::begin(
            team == ALLIANCE ? alliance_commander_walk : horde_commander_walk),
        std::end(
            team == ALLIANCE ? alliance_commander_walk : horde_commander_walk));

    commander->SetFlag(
        UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_PASSIVE);
    commander->movement_gens.remove_all(movement::gen::idle);
    commander->movement_gens.push(
        new movement::DynamicWaypointMovementGenerator(wps, false));
    data->commander = commander->GetObjectGuid();
    grp->AddMember(commander, false);

    // Yell
    if (team == ALLIANCE)
        commander->MonsterYell("Ram Riders, REGULATE!", 0);
    else
        commander->MonsterYell("Wolf riders, to Frost Dagger Pass!", 0);

    data->stage = 0;
    data->timer = 20000;
}

void alterac_valley::resume_cavalry(Team team)
{
    cavalry_data* data =
        team == ALLIANCE ? &cavalry_data_[0] : &cavalry_data_[1];
    data->stage = 6;
}

bool alterac_valley::cavalry_waiting(Team team) const
{
    const cavalry_data* data =
        team == ALLIANCE ? &cavalry_data_[0] : &cavalry_data_[1];
    return data->stage == 5;
}

void alterac_valley::update_cavalry(Team team)
{
    cavalry_data* data =
        team == ALLIANCE ? &cavalry_data_[0] : &cavalry_data_[1];
    Creature* commander = instance->GetCreature(data->commander);
    if (!commander)
        return; // XXX: abort event

    if (data->stage == 0 && data->timer == 0)
    {
        data->stage = 1;
        data->timer = 10000;

        if (team == ALLIANCE)
            commander->MonsterSay(
                "Lads and ladies. Today can be your greatest day, for today, "
                "you fight for the glory of Stormpike! "
                "Ride unto the field and grant your enemies no mercy! Strike "
                "the savages down and let their Warchief sort them out!",
                0);
        else
            commander->MonsterSay(
                "The time has come, brothers. We must not let the enemy "
                "advance through Frost Dagger Pass. "
                "Some of us will surely perish, but would any of you shy away "
                "from this duty? This honor? Onward! For the Horde!",
                0);
    }
    else if (data->stage == 1 && data->timer == 0)
    {
        data->stage = 2;
        data->timer = 5000;

        if (team == ALLIANCE)
            for_each_creature(data->riders, [](Creature* c)
                {
                    c->MonsterSay("For the King!", 0);
                });
        else
            for_each_creature(data->riders, [](Creature* c)
                {
                    c->MonsterSay("For the Warchief!", 0);
                });
    }
    else if (data->stage == 2 && data->timer == 0)
    {
        commander->AI()->Notify(1);

        uint32 i = 1;
        for_each_creature(data->riders, [&i](Creature* c)
            {
                c->AI()->Notify(i++);
            });

        data->stage = 3;
        data->timer = 10000;
    }
    else if (data->stage == 3 && data->timer == 0)
    {
        data->stage = 4;

        commander->RemoveFlag(
            UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_PASSIVE);
        for_each_creature(data->riders, [](Creature* c)
            {
                c->RemoveFlag(UNIT_FIELD_FLAGS,
                    UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_PASSIVE);
            });
    }
    else if ((data->stage == 4 || data->stage == 5) && !commander->isAlive())
    {
        data->stage = 100; // as soon as the riders have finished their path we
                           // need to start them again, as the commander is dead
    }
    else if (data->stage == 4 && commander->isAlive())
    {
        if (commander->movement_gens.top_id() == movement::gen::idle)
        {
            bool can_resume = true;
            for_each_creature(data->riders, [&can_resume](Creature* c)
                {
                    if (c->movement_gens.top_id() != movement::gen::idle)
                        can_resume = false;
                });

            if (can_resume)
            {
                commander->SetFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP);
                data->stage = 5;
            }
        }
    }
    else if (data->stage == 6)
    {
        data->stage = 7;

        if (commander->isAlive())
        {
            commander->AI()->Notify(2);
            commander->RemoveFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP);
        }
        for_each_creature(data->riders, [](Creature* c)
            {
                if (c->isAlive())
                    c->AI()->Notify(20);
            });
    }
    else if (data->stage == 100)
    {
        bool can_resume = true;
        for_each_creature(data->riders, [&can_resume](Creature* c)
            {
                if (c->movement_gens.top_id() != movement::gen::idle)
                    can_resume = false;
            });

        if (can_resume)
            data->stage = 6;
    }
}

void alterac_valley::cavalry_data::reset(alterac_valley* av)
{
    timer = 0;
    stage = 0;
    commander.Clear();
    riders.clear();
    if (auto grp = av->instance->GetCreatureGroupMgr().GetGroup(grp_id))
    {
        if (grp->IsTemporaryGroup())
            av->instance->GetCreatureGroupMgr().DeleteGroup(grp_id);
    }
    grp_id = 0;
}
