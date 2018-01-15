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

#ifndef MANGOSSCRIPT__ALTERAC_VALLEY_H
#define MANGOSSCRIPT__ALTERAC_VALLEY_H

#include "precompiled.h"

enum
{
    GROUND_ASSAULT_DELAY = 5 * 60,

    NPC_TERAVAINE = 13446,
    NPC_GARRICK = 13449,

    GO_SLIDORES_BEACON = 178725,
    GO_VIPORES_BEACON = 178724,
    GO_ICHMANS_BEACON = 178726,
    GO_GUSES_BEACON = 178545,
    GO_JEZTORS_BEACON = 178547,
    GO_MULVERICKS_BEACON = 178549,

    // Wing Commanders
    // Alliance
    NPC_ID_WC_SLIDORE = 13438,
    NPC_ID_WC_VIPORE = 13439,
    NPC_ID_WC_ICHMAN = 13437,
    // Horde
    NPC_ID_WC_GUSE = 13179,
    NPC_ID_WC_JEZTOR = 13180,
    NPC_ID_WC_MULVERICK = 13181,

    NPC_ID_IVUS = 13419,
    NPC_ID_LOKHOLAR = 13256,

    NPC_ID_FROSTWOLF_EXPLOSIVES_EXPERT = 13597,
    NPC_ID_STROMPIKE_EXPLOSIVES_EXPERT = 13598,
};

class MANGOS_DLL_DECL alterac_valley : public ScriptedInstance
{
public: // public member structures
    struct pending_beacon
    {
        bool did_emote = false;
        ObjectGuid go;
        uint32 creature_entry;
        G3D::Vector3 pos;
        time_t timestamp; // timestamp when creature should spawn
    };

public: // start of class
    alterac_valley(Map* map);

    void SetData(uint32 index, uint32 data) override;
    uint32 GetData(uint32 index) override;

    void Update(uint32 diff) override;

    void OnCreatureCreate(Creature* c) override;
    void OnCreatureDeath(Creature* c) override;
    void OnEvent(const std::string& event_id) override;
    void OnObjectCreate(GameObject* go) override;
    void OnCreatureRespawn(Creature* c) override;

    time_t ally_ground_cd = 0;  // timestamp of when ground assault goes off
                                // cooldown for the alliance side
    time_t horde_ground_cd = 0; // timestamp of when ground assault goes off
                                // cooldown for the horde side
    time_t ally_cavalry_cd =
        0; // timestamp of when cavalry goes off cooldown for the alliance side
    time_t horde_cavalry_cd =
        0; // timestamp of when cavalry goes off cooldown for the horde side

    void do_ground_assault(Team team, Creature* commander);

    void do_cavalry(Team team, Creature* commander);
    void resume_cavalry(Team team);
    // true if waiting for gossip to re-enable event
    bool cavalry_waiting(Team team) const;

    std::vector<pending_beacon>& pending_beacons() { return pending_beacons_; }

    void send_wing_commander(Creature* c);

    // Callback for when soldier level is upgraded
    void soldiers_update_callback(Team team);

private:
    uint32 data_[MAX_AV_INST_DATA];
    std::vector<ObjectGuid>
        alliance_soldiers; // Saved for when we need to process armor upgrades
    std::vector<ObjectGuid>
        horde_soldiers; // Saved for when we need to process armor upgrades

    void for_each_creature(
        const std::vector<ObjectGuid>& v, std::function<void(Creature*)> f);

    struct ground_data
    {
        uint32 timer = 0;
        uint32 stage = 0;
        ObjectGuid commander;
        std::vector<ObjectGuid> grp_one;
        std::vector<ObjectGuid> grp_two;
        int32 grp_id[2] = {0};
        void reset(alterac_valley* av);
    };
    ground_data ground_data_[2];

    void update_ground(Team team);

    struct cavalry_data
    {
        uint32 timer = 0;
        uint32 stage = 0;
        ObjectGuid commander;
        std::vector<ObjectGuid> riders;
        int32 grp_id = 0;
        void reset(alterac_valley* av);
    };
    cavalry_data cavalry_data_[2];

    void update_cavalry(Team team);

    void spawned_beacon(GameObject* go);
    std::vector<pending_beacon> pending_beacons_;
    void update_beacons();

    // order: commander, druid, druid, druid (can be nullptrs), guaranteed 4
    // elements
    std::vector<Creature*> boss_creatures(Team t);

    struct ivus_lok_data
    {
        uint32 stage = 0;
        uint32 timer = 0;
        int32 grp_id = 0;
        ObjectGuid boss;
        void reset(alterac_valley* av, Team t);
    };
    ivus_lok_data ivus_lok_data_[2];
    void update_ivus_lok(ivus_lok_data& d, Team t);

public:
    ivus_lok_data& boss_data(Team t)
    {
        return t == ALLIANCE ? ivus_lok_data_[0] : ivus_lok_data_[1];
    }
};

#endif // MANGOSSCRIPT__ALTERAC_VALLEY_H
