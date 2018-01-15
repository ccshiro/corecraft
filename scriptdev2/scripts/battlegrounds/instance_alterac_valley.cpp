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

alterac_valley::alterac_valley(Map* map) : ScriptedInstance(map), data_{0}
{
}

void alterac_valley::SetData(uint32 index, uint32 data)
{
    if (index >= MAX_AV_INST_DATA)
    {
        logging.error(
            "alterac_valley::SetData(): tried to set data with index %u (max "
            "index = %d)",
            index, MAX_AV_INST_DATA);
        return;
    }

    data_[index] = data;
}

uint32 alterac_valley::GetData(uint32 index)
{
    if (index >= MAX_AV_INST_DATA)
    {
        logging.error(
            "alterac_valley::GetData(): tried to get data with index %u (max "
            "index = %d)",
            index, MAX_AV_INST_DATA);
        return 0;
    }

    return data_[index];
}

void alterac_valley::Update(uint32 diff)
{
    if (!ground_data_[0].commander.IsEmpty())
    {
        if (ground_data_[0].timer)
        {
            if (ground_data_[0].timer <= diff)
                ground_data_[0].timer = 0;
            else
                ground_data_[0].timer -= diff;
        }

        update_ground(ALLIANCE);
    }

    if (!ground_data_[1].commander.IsEmpty())
    {
        if (ground_data_[1].timer)
        {
            if (ground_data_[1].timer <= diff)
                ground_data_[1].timer = 0;
            else
                ground_data_[1].timer -= diff;
        }

        update_ground(HORDE);
    }

    if (!cavalry_data_[0].commander.IsEmpty())
    {
        if (cavalry_data_[0].timer)
        {
            if (cavalry_data_[0].timer <= diff)
                cavalry_data_[0].timer = 0;
            else
                cavalry_data_[0].timer -= diff;
        }

        update_cavalry(ALLIANCE);
    }

    if (!cavalry_data_[1].commander.IsEmpty())
    {
        if (cavalry_data_[1].timer)
        {
            if (cavalry_data_[1].timer <= diff)
                cavalry_data_[1].timer = 0;
            else
                cavalry_data_[1].timer -= diff;
        }

        update_cavalry(HORDE);
    }

    update_beacons();

    if (ivus_lok_data_[0].timer <= diff)
        ivus_lok_data_[0].timer = 0;
    else
        ivus_lok_data_[0].timer -= diff;

    if (ivus_lok_data_[1].timer <= diff)
        ivus_lok_data_[1].timer = 0;
    else
        ivus_lok_data_[1].timer -= diff;

    update_ivus_lok(ivus_lok_data_[0], ALLIANCE);
    update_ivus_lok(ivus_lok_data_[1], HORDE);

    if (GetData(ALLY_WING_COMMANDER_BONUS_HONOR) == 1)
    {
        if (auto bgmap = dynamic_cast<BattleGroundMap*>(instance))
            if (auto bg = dynamic_cast<BattleGroundAV*>(bgmap->GetBG()))
                bg->RewardHonorToTeam(bg->GetBonusHonorFromKill(2), ALLIANCE);

        SetData(ALLY_WING_COMMANDER_BONUS_HONOR, 0);
    }

    if (GetData(HORDE_WING_COMMANDER_BONUS_HONOR) == 1)
    {
        if (auto bgmap = dynamic_cast<BattleGroundMap*>(instance))
            if (auto bg = dynamic_cast<BattleGroundAV*>(bgmap->GetBG()))
                bg->RewardHonorToTeam(bg->GetBonusHonorFromKill(2), HORDE);

        SetData(HORDE_WING_COMMANDER_BONUS_HONOR, 0);
    }
}

void alterac_valley::OnCreatureDeath(Creature* c)
{
    switch (c->GetEntry())
    {
    case NPC_GARRICK:
        m_mNpcEntryGuidStore[NPC_GARRICK] = ObjectGuid();
        horde_ground_cd = 15 * MINUTE;
        break;
    case NPC_TERAVAINE:
        m_mNpcEntryGuidStore[NPC_TERAVAINE] = ObjectGuid();
        ally_ground_cd = 15 * MINUTE;
        break;
    case NPC_ID_FROSTWOLF_EXPLOSIVES_EXPERT:
        if (auto bgmap = dynamic_cast<BattleGroundMap*>(instance))
            if (auto bg_av = dynamic_cast<BattleGroundAV*>(bgmap->GetBG()))
                bg_av->SpawnEvent(BG_AV_LAND_MINES_H, 0, false);
        break;
    case NPC_ID_STROMPIKE_EXPLOSIVES_EXPERT:
        if (auto bgmap = dynamic_cast<BattleGroundMap*>(instance))
            if (auto bg_av = dynamic_cast<BattleGroundAV*>(bgmap->GetBG()))
                bg_av->SpawnEvent(BG_AV_LAND_MINES_A, 0, false);
        break;
    }
}

void alterac_valley::OnEvent(const std::string& event_id)
{
    if (event_id.compare("horde ground") == 0)
    {
        if (auto c = GetSingleCreatureFromStorage(NPC_GARRICK))
            do_ground_assault(HORDE, c);
    }
    else if (event_id.compare("alliance ground") == 0)
    {
        if (auto c = GetSingleCreatureFromStorage(NPC_TERAVAINE))
            do_ground_assault(ALLIANCE, c);
    }
}

void alterac_valley::OnObjectCreate(GameObject* go)
{
    switch (go->GetEntry())
    {
    case GO_SLIDORES_BEACON:
    case GO_VIPORES_BEACON:
    case GO_ICHMANS_BEACON:
    case GO_GUSES_BEACON:
    case GO_JEZTORS_BEACON:
    case GO_MULVERICKS_BEACON:
        spawned_beacon(go);
    }
}

void alterac_valley::for_each_creature(
    const std::vector<ObjectGuid>& v, std::function<void(Creature*)> f)
{
    for (auto guid : v)
        if (auto c = instance->GetCreature(guid))
            if (c->isAlive())
                f(c);
}

/* creature upgrades (excludes guards. which are handled in core's PopulateNode
 * function) */
struct upgrade_levels
{
    upgrade_levels(uint32 a, uint32 b, uint32 c, uint32 d)
    {
        entries[0] = a;
        entries[1] = b;
        entries[2] = c;
        entries[3] = d;
    }

    uint32 entries[4];

    bool operator==(uint32 e) const { return entries[0] == e; }
    uint32 get_entry(uint32 level) const
    {
        if (level > 3)
            return entries[3];
        return entries[level];
    }
};

static const upgrade_levels alliance_upgrade_mobs[] = {
    // Stormpike Guardsman
    upgrade_levels{12127, 13324, 13333, 13424},
    // Stormpike Mountaineer
    upgrade_levels{12047, 13325, 13335, 13426},
    // Alliance Sentinel
    upgrade_levels{12048, 13327, 13336, 13427},
    // Irondeep Guard
    upgrade_levels{13080, 13552, 13553, 13554},
    // Irondeep Surveyor
    upgrade_levels{13098, 13555, 13556, 13557},
    // Coldmine Invader
    upgrade_levels{13087, 13549, 13550, 13551},
    // Coldmine Explorer
    upgrade_levels{13096, 13546, 13547, 13548},
};

static const upgrade_levels horde_upgrade_mobs[] = {
    // Frostwolf Warrior
    upgrade_levels{12052, 13330, 13337, 13428},
    // Frostwolf Legionnaire
    upgrade_levels{12051, 13329, 13334, 13425},
    // Irondeep Explorer
    upgrade_levels{13099, 13540, 13541, 13542},
    // Irondeep Raider
    upgrade_levels{13081, 13543, 13544, 13545},
    // Coldmine Guard
    upgrade_levels{13089, 13534, 13535, 13536},
    // Coldmine Surveyor
    upgrade_levels{13097, 13537, 13538, 13539},
};

void alterac_valley::OnCreatureCreate(Creature* c)
{
    if (c->GetEntry() == NPC_GARRICK)
        m_mNpcEntryGuidStore[NPC_GARRICK] = c->GetObjectGuid();
    else if (c->GetEntry() == NPC_TERAVAINE)
        m_mNpcEntryGuidStore[NPC_TERAVAINE] = c->GetObjectGuid();
    else if (c->GetEntry() == NPC_ID_IVUS)
        ivus_lok_data_[0].boss = c->GetObjectGuid();
    else if (c->GetEntry() == NPC_ID_LOKHOLAR)
        ivus_lok_data_[1].boss = c->GetObjectGuid();

    auto a_itr = std::find(std::begin(alliance_upgrade_mobs),
        std::end(alliance_upgrade_mobs), c->GetEntry());
    if (a_itr != std::end(alliance_upgrade_mobs))
    {
        alliance_soldiers.push_back(c->GetObjectGuid());

        uint32 level = GetData(ALLIANCE_SOLDIERS_UPGRADE_LEVEL);
        if (level > 0)
            c->UpdateEntry(a_itr->get_entry(level));
    }

    auto h_itr = std::find(std::begin(horde_upgrade_mobs),
        std::end(horde_upgrade_mobs), c->GetEntry());
    if (h_itr != std::end(horde_upgrade_mobs))
    {
        horde_soldiers.push_back(c->GetObjectGuid());

        uint32 level = GetData(HORDE_SOLDIERS_UPGRADE_LEVEL);
        if (level > 0)
            c->UpdateEntry(h_itr->get_entry(level));
    }
}

void alterac_valley::OnCreatureRespawn(Creature* c)
{
    // Select the correct NPC upgrade level when an ALLIANCE mob respawns
    auto a_itr = std::find(std::begin(alliance_upgrade_mobs),
        std::end(alliance_upgrade_mobs), c->GetEntry());
    if (a_itr != std::end(alliance_upgrade_mobs))
    {
        uint32 level = GetData(ALLIANCE_SOLDIERS_UPGRADE_LEVEL);
        if (level > 0)
            c->UpdateEntry(a_itr->get_entry(level));
    }

    // Select the correct NPC upgrade level when a HORDE mob respawns
    auto h_itr = std::find(std::begin(horde_upgrade_mobs),
        std::end(horde_upgrade_mobs), c->GetEntry());
    if (h_itr != std::end(horde_upgrade_mobs))
    {
        uint32 level = GetData(HORDE_SOLDIERS_UPGRADE_LEVEL);
        if (level > 0)
            c->UpdateEntry(h_itr->get_entry(level));
    }

    switch (c->GetEntry())
    {
    case NPC_ID_FROSTWOLF_EXPLOSIVES_EXPERT:
        if (auto bgmap = dynamic_cast<BattleGroundMap*>(instance))
            if (auto bg_av = dynamic_cast<BattleGroundAV*>(bgmap->GetBG()))
                bg_av->SpawnEvent(BG_AV_LAND_MINES_H, 0, true);
        break;
    case NPC_ID_STROMPIKE_EXPLOSIVES_EXPERT:
        if (auto bgmap = dynamic_cast<BattleGroundMap*>(instance))
            if (auto bg_av = dynamic_cast<BattleGroundAV*>(bgmap->GetBG()))
                bg_av->SpawnEvent(BG_AV_LAND_MINES_A, 0, true);
        break;
    }
}

void alterac_valley::soldiers_update_callback(Team team)
{
    std::vector<ObjectGuid>& vec =
        team == ALLIANCE ? alliance_soldiers : horde_soldiers;

    for (auto guid : vec)
    {
        if (Creature* c = instance->GetCreature(guid))
            if (c->isAlive())
            {
                c->SetDeathState(CORPSE);
                c->Respawn();
            }
    }
}

InstanceData* GetInstanceData_instance_alterac_valley(Map* map)
{
    return new alterac_valley(map);
}

void AddSC_instance_alterac_valley()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "instance_alterac_valley";
    pNewScript->GetInstanceData = &GetInstanceData_instance_alterac_valley;
    pNewScript->RegisterSelf();
}
