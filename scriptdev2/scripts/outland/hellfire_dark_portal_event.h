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

#ifndef SCRIPTS__DARK_PORTAL_H
#define SCRIPTS__DARK_PORTAL_H

#include "precompiled.h"

class outland_instance_data;

enum class fel_groups
{
    soldiers_shaadraz,
    soldiers_murketh,
    wrathmaster,
    pit_commander
};

enum
{
    NPC_ID_FEL_SOLDIER = 18944,
    NPC_ID_WRATH_MASTER = 19005,
    NPC_ID_PIT_COMMANDER = 18945,
};

class dark_portal_event
{
public:
    dark_portal_event(outland_instance_data* id);
    ~dark_portal_event();

    void update(uint32 diff);
    void spawn_group(uint32 id, fel_groups group, uint32 timer);

    void line_add(Creature* c);
    void line_remove(Creature* c);

    void commence_attack(Creature* c);

private:
    outland_instance_data* inst_data;
    ObjectGuid position[36];

    struct group_data
    {
        ~group_data();

        int32 grp_id = 0;
        dark_portal_event* dp_event = nullptr;
        outland_instance_data* inst_data = nullptr;

        int stage = 1;
        uint32 timer = 0;
        fel_groups group = fel_groups::soldiers_shaadraz;
        std::vector<ObjectGuid> members;

        Creature* spawn_creature(uint32 id, float x, float y, float z);
        void move_dynpoints(Creature* c, const DynamicWaypoint* begin,
            const DynamicWaypoint* end);
        void group_move(
            const DynamicWaypoint* begin, const DynamicWaypoint* end);
        bool all_idling();
        bool all_dead();

        void update(uint32 diff, bool paused);
        void do_stage(int s, bool paused);
        void do_soldiers_shaadraz(int s, bool delay_attack);
        void do_soldiers_murketh(int s, bool delay_attack);
        void do_wrathmaster(int s, bool delay_attack);
        void do_pit_commander(int s, bool delay_attack);
    };

    std::map<uint32, group_data> groups_;
    bool paused_;
    int pause_timer_;
};

#endif // SCRIPTS__DARK_PORTAL_H
