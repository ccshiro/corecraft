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

static std::vector<G3D::Vector3> build_circular_path(Creature* c);
static std::vector<G3D::Vector3> build_commander_path(Creature* c);

enum
{
    SPELL_FIREBALL = 22088,
    SPELL_FIREBALL_VOLLEY = 15285,
    SPELL_STUN_BOMB = 21188,

    NPC_AERIE_GRYPHON = 13161,
    NPC_WAR_RIDER = 13178,

    NPC_ID_SLIDORE_GRYPHON = 14946,
    NPC_ID_VIPORE_GRYPHON = 14948,
    NPC_ID_ICHMAN_GRYPHON = 14947,
    NPC_ID_GUSE_WAR_RIDER = 14943,
    NPC_ID_JEZTOR_WAR_RIDER = 14944,
    NPC_ID_MULVERICK_WAR_RIDER = 14945,
};

struct aerial_assault_riderAI : public ScriptedAI
{
    aerial_assault_riderAI(Creature* c) : ScriptedAI(c)
    {
        m_creature->SetVisibility(VISIBILITY_OFF);

        uint32 e = c->GetEntry();
        if (e == NPC_AERIE_GRYPHON || e == NPC_WAR_RIDER)
            path = build_circular_path(c);
        else
            path = build_commander_path(c);

        if (e == NPC_AERIE_GRYPHON || e == NPC_ID_SLIDORE_GRYPHON ||
            e == NPC_ID_VIPORE_GRYPHON || e == NPC_ID_ICHMAN_GRYPHON)
            team = ALLIANCE;
        else
            team = HORDE;

        moved = false;

        Reset();
    }

    // Don't actually fight
    void AttackStart(Unit*) override {}
    void EnterEvadeMode(bool by_group = false) override
    {
        m_creature->DeleteThreatList();
        m_creature->CombatStop(true);

        // process creature evade actions
        m_creature->OnEvadeActions(by_group);
    }

    Team team;
    bool moved;
    std::vector<G3D::Vector3> path;
    uint32 fireball;
    uint32 volley;
    uint32 bomb;
    bool reached_wp;
    uint32 point;

    void Reset() override
    {
        fireball = urand(6000, 8000);
        volley = urand(10000, 20000);
        bomb = urand(10000, 15000);
        reached_wp = false;
        point = 0;
        m_creature->movement_gens.remove_all(movement::gen::idle);
    }

    Player* get_target(float radius)
    {
        auto players =
            GetAllPlayersInObjectRangeCheckInCell(m_creature, radius);

        for (auto itr = players.begin(); itr != players.end();)
        {
            if ((*itr)->GetTeam() == team || !(*itr)->isTargetableForAttack())
                itr = players.erase(itr);
            else
                ++itr;
        }

        if (players.empty())
            return nullptr;
        auto itr = players.begin();
        std::advance(itr, urand(0, players.size() - 1));
        return *itr;
    }

    void MovementInform(movement::gen type, uint32 data) override
    {
        if (type != movement::gen::point)
            return;

        point = data + 1;
        if (point >= path.size())
            point = 0;

        reached_wp = true;
    }

    void UpdateAI(uint32 diff) override
    {
        // Select hostile target to make sure combat is properly updated in
        // regards to this mob
        m_creature->SelectHostileTarget();

        if (!moved && !path.empty())
        {
            m_creature->NearTeleportTo(
                path[0].x, path[0].y, path[0].z, m_creature->GetO());
            m_creature->SetVisibility(VISIBILITY_ON);
            m_creature->movement_gens.push(new movement::PointMovementGenerator(
                0, path[0].x, path[0].y, path[0].z, false, true));
            moved = true;
        }

        if (fireball >= diff)
            fireball -= diff;
        else
            fireball = 0;

        if (volley >= diff)
            volley -= diff;
        else
            volley = 0;

        if (bomb >= diff)
            bomb -= diff;
        else
            bomb = 0;

        if (reached_wp && (fireball < 1000 || volley < 1000 || bomb < 1000))
        {
            if (fireball <= diff)
            {
                if (Unit* target = get_target(80.0f))
                    DoCastSpellIfCan(target, SPELL_FIREBALL);

                fireball = urand(6000, 8000);
            }

            if (volley <= diff)
            {
                DoCastSpellIfCan(m_creature, SPELL_FIREBALL_VOLLEY);

                volley = urand(10000, 20000);
            }

            if (bomb <= diff)
            {
                if (Unit* target = get_target(80.0f))
                    m_creature->CastSpell(target->GetX(), target->GetY(),
                        target->GetZ(), SPELL_STUN_BOMB, false);

                bomb = urand(10000, 15000);
            }
        }
        else if (reached_wp && !m_creature->IsNonMeleeSpellCasted(false))
        {
            reached_wp = false;
            m_creature->movement_gens.push(
                new movement::PointMovementGenerator(point, path[point].x,
                    path[point].y, path[point].z, false, true));
        }
    }
};

CreatureAI* GetAI_npc_aerial_assault_rider(Creature* c)
{
    return new aerial_assault_riderAI(c);
}

static std::vector<G3D::Vector3> build_circular_path(Creature* c)
{
    std::vector<G3D::Vector3> path;

    // split the circle up in 16 parts
    for (int i = 0; i < 16; ++i)
    {
        float angle = (i + 1.0f) * (2 * M_PI_F) / 16;
        float x = 30 * cos(angle) + c->GetX();
        float y = 30 * sin(angle) + c->GetY();
        float z = c->GetZ() + 20.0f;
        path.emplace_back(x, y, z);
    }

    return path;
}

void alterac_valley::spawned_beacon(GameObject* go)
{
    pending_beacon n;

    n.go = go->GetObjectGuid();

    switch (go->GetEntry())
    {
    case GO_SLIDORES_BEACON:
    case GO_VIPORES_BEACON:
    case GO_ICHMANS_BEACON:
        n.creature_entry = NPC_AERIE_GRYPHON;
        break;
    case GO_GUSES_BEACON:
    case GO_JEZTORS_BEACON:
    case GO_MULVERICKS_BEACON:
        n.creature_entry = NPC_WAR_RIDER;
        break;
    }

    n.pos = G3D::Vector3(go->GetX(), go->GetY(), go->GetZ());

    n.timestamp = WorldTimer::time_no_syscall() + 60;

    pending_beacons_.push_back(n);
}

void alterac_valley::update_beacons()
{
    for (auto itr = pending_beacons_.begin(); itr != pending_beacons_.end();)
    {
        if (!itr->did_emote)
        {
            if (auto go = instance->GetGameObject(itr->go))
                go->MonsterTextEmote(
                    "An aerial assault is imminent. Deactivate the beacon to "
                    "stop it.",
                    nullptr);
            itr->did_emote = true;
        }

        if (WorldTimer::time_no_syscall() >= itr->timestamp)
        {
            if (auto go = instance->GetGameObject(itr->go))
            {
                go->SummonCreature(itr->creature_entry, itr->pos.x, itr->pos.y,
                    itr->pos.z, 0, TEMPSUMMON_CORPSE_TIMED_DESPAWN, 10000,
                    SUMMON_OPT_ACTIVE);
                go->Delete();
            }
            itr = pending_beacons_.erase(itr);
        }
        else
            ++itr;
    }
}

bool GOUse_av_beacon(Player* p, GameObject* go)
{
    bool ally_beacon = false;
    if (go->GetEntry() == GO_SLIDORES_BEACON ||
        go->GetEntry() == GO_VIPORES_BEACON ||
        go->GetEntry() == GO_ICHMANS_BEACON)
        ally_beacon = true;

    // Cannot deactivate your own faction's beacon
    if ((p->GetTeam() == ALLIANCE && ally_beacon) ||
        (p->GetTeam() == HORDE && !ally_beacon))
        return true;

    auto av = dynamic_cast<alterac_valley*>(p->GetMap()->GetInstanceData());
    if (!av)
        return true;
    auto& b = av->pending_beacons();
    using p_b = alterac_valley::pending_beacon;

    auto itr = std::find_if(b.begin(), b.end(), [go](const p_b& a)
        {
            return go->GetObjectGuid() == a.go;
        });
    if (itr == b.end())
        return true;

    std::stringstream ss;
    ss << p->GetName() << " deactivated the beacon. The aerial assault has "
                          "been put to rout."; // XXX: made-up
    go->MonsterTextEmote(ss.str().c_str(), nullptr);
    go->Delete();

    // remove pending beacon
    b.erase(itr);

    return true;
}

static const G3D::Vector3 ally_fly_pos(540, -50, 90);
static const G3D::Vector3 horde_fly_pos(-1330, -300, 140);

void alterac_valley::send_wing_commander(Creature* c)
{
    G3D::Vector3 fly_pos;
    G3D::Vector3 spawn_pos;
    uint32 mount_id;
    uint32 spawn_id;

    switch (c->GetEntry())
    {
    case NPC_ID_WC_SLIDORE:
        fly_pos = ally_fly_pos;
        spawn_pos = G3D::Vector3(-750, -330, 120);
        mount_id = 1148;
        spawn_id = 14946;
        break;
    case NPC_ID_WC_VIPORE:
        fly_pos = ally_fly_pos;
        spawn_pos = G3D::Vector3(-1200, -370, 110);
        mount_id = 1148;
        spawn_id = 14948;
        break;
    case NPC_ID_WC_ICHMAN:
        fly_pos = ally_fly_pos;
        spawn_pos = G3D::Vector3(-1320, -290, 160);
        mount_id = 1148;
        spawn_id = 14947;
        break;
    case NPC_ID_WC_GUSE:
        fly_pos = horde_fly_pos;
        spawn_pos = G3D::Vector3(250, -370, 100);
        mount_id = 11012;
        spawn_id = 14943;
        break;
    case NPC_ID_WC_JEZTOR:
        fly_pos = horde_fly_pos;
        spawn_pos = G3D::Vector3(620, -210, 110);
        mount_id = 11012;
        spawn_id = 14944;
        break;
    case NPC_ID_WC_MULVERICK:
        fly_pos = horde_fly_pos;
        spawn_pos = G3D::Vector3(670, -160, 130);
        mount_id = 11012;
        spawn_id = 14945;
        break;
    default:
        logging.error(
            "alterac_valley::send_wing_commander: tried to send someone that "
            "isn't a wing commander");
        return;
    }

    c->RemoveFlag(
        UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP | UNIT_NPC_FLAG_QUESTGIVER);
    c->SummonCreature(spawn_id, spawn_pos.x, spawn_pos.y, spawn_pos.z, 0,
        TEMPSUMMON_CORPSE_TIMED_DESPAWN, 10000, SUMMON_OPT_ACTIVE);
    c->m_movementInfo.SetMovementFlags(MOVEFLAG_FLYING);
    c->Mount(mount_id);
    c->movement_gens.push(new movement::PointMovementGenerator(
        0, fly_pos.x, fly_pos.y, fly_pos.z, false, true));
    c->ForcedDespawn(4000);
}

static const G3D::Vector3 slidore_patrol_path[] = {
    G3D::Vector3(-750.3, -329.7, 95.6), G3D::Vector3(-813.3, -348.8, 90.3),
    G3D::Vector3(-833.9, -388.1, 81.1), G3D::Vector3(-875.9, -385.9, 78.7),
    G3D::Vector3(-920.5, -390.8, 71.3), G3D::Vector3(-986.6, -393.9, 66.9),
    G3D::Vector3(-1024.1, -385.3, 68.8), G3D::Vector3(-1056.3, -368.6, 69.1),
    G3D::Vector3(-1074.0, -341.2, 68.7), G3D::Vector3(-1084.5, -300.5, 70.0),
    G3D::Vector3(-1088.6, -259.8, 70.6), G3D::Vector3(-1113.3, -284.6, 72.5),
    G3D::Vector3(-1138.6, -341.9, 66.8), G3D::Vector3(-1103.7, -340.0, 65.1),
    G3D::Vector3(-1073.9, -362.0, 69.5), G3D::Vector3(-1012.6, -354.4, 73.6),
    G3D::Vector3(-952.8, -335.8, 76.3), G3D::Vector3(-895.8, -324.2, 79.7),
    G3D::Vector3(-861.2, -307.7, 83.6), G3D::Vector3(-827.5, -303.0, 81.6),
    G3D::Vector3(-765.0, -308.2, 85.7), G3D::Vector3(-730.2, -336.3, 87.1)};

static const G3D::Vector3 vipore_patrol_path[] = {
    G3D::Vector3(-1199.9, -370.4, 86.7), G3D::Vector3(-1177.4, -344.4, 87.3),
    G3D::Vector3(-1177.5, -320.6, 87.7), G3D::Vector3(-1185.6, -291.4, 88.3),
    G3D::Vector3(-1204.9, -256.9, 89.0), G3D::Vector3(-1222.3, -287.2, 89.6),
    G3D::Vector3(-1237.4, -313.5, 90.1), G3D::Vector3(-1257.4, -328.0, 90.5),
    G3D::Vector3(-1265.3, -352.5, 91.0), G3D::Vector3(-1227.7, -344.5, 91.7),
    G3D::Vector3(-1202.7, -375.3, 92.4)};

static const G3D::Vector3 ichman_patrol_path[] = {
    G3D::Vector3(-1323.4, -292.9, 134.0), G3D::Vector3(-1340.8, -306.6, 133.2),
    G3D::Vector3(-1363.9, -318.7, 132.2), G3D::Vector3(-1392.0, -301.8, 131.0),
    G3D::Vector3(-1413.0, -289.2, 130.0), G3D::Vector3(-1415.4, -308.9, 129.3),
    G3D::Vector3(-1399.5, -320.8, 128.5), G3D::Vector3(-1364.5, -302.1, 127.1),
    G3D::Vector3(-1326.7, -276.8, 125.4), G3D::Vector3(-1306.3, -282.0, 124.6),
    G3D::Vector3(-1267.7, -301.2, 122.9), G3D::Vector3(-1266.0, -280.4, 122.2),
    G3D::Vector3(-1299.5, -290.8, 120.8)};

static const G3D::Vector3 guse_patrol_path[] = {
    G3D::Vector3(251.2, -372.9, 73.5), G3D::Vector3(280.5, -383.0, 37.2),
    G3D::Vector3(350.5, -386.4, 26.0), G3D::Vector3(393.9, -392.0, 21.3),
    G3D::Vector3(432.2, -378.1, 21.9), G3D::Vector3(462.4, -365.8, 20.7),
    G3D::Vector3(488.7, -346.5, 18.8), G3D::Vector3(519.1, -326.8, 18.8),
    G3D::Vector3(548.1, -330.2, 35.9), G3D::Vector3(570.2, -336.6, 51.8),
    G3D::Vector3(613.1, -332.5, 50.9), G3D::Vector3(628.3, -311.9, 49.3),
    G3D::Vector3(640.8, -288.3, 50.0), G3D::Vector3(670.8, -295.4, 51.7),
    G3D::Vector3(670.6, -322.1, 48.1), G3D::Vector3(671.1, -366.2, 42.7),
    G3D::Vector3(651.2, -371.9, 74.9), G3D::Vector3(610.8, -338.6, 69.1),
    G3D::Vector3(594.1, -335.1, 49.1), G3D::Vector3(553.5, -325.1, 25.9),
    G3D::Vector3(518.6, -319.4, 11.2), G3D::Vector3(477.9, -347.7, 18.2),
    G3D::Vector3(420.8, -376.6, 22.6), G3D::Vector3(371.7, -389.5, 21.3),
    G3D::Vector3(316.2, -382.6, 23.3), G3D::Vector3(252.0, -400.6, 57.3),
    G3D::Vector3(216.6, -388.2, 91.5)};

static const G3D::Vector3 jeztor_patrol_path[] = {
    G3D::Vector3(626.2, -211.6, 84.9), G3D::Vector3(631.8, -250.5, 64.3),
    G3D::Vector3(634.8, -278.9, 55.1), G3D::Vector3(632.7, -258.1, 56.6),
    G3D::Vector3(630.3, -232.5, 55.9), G3D::Vector3(625.1, -200.2, 55.3),
    G3D::Vector3(621.3, -173.7, 53.0), G3D::Vector3(625.3, -146.0, 53.4),
    G3D::Vector3(610.7, -116.1, 53.9), G3D::Vector3(583.9, -115.8, 55.4),
    G3D::Vector3(583.9, -140.2, 55.2), G3D::Vector3(607.7, -185.8, 54.3)};

static const G3D::Vector3 mulverick_patrol_path[] = {
    G3D::Vector3(626.2, -211.6, 84.9), G3D::Vector3(631.8, -250.5, 64.3),
    G3D::Vector3(634.8, -278.9, 55.1), G3D::Vector3(632.7, -258.1, 56.6),
    G3D::Vector3(630.3, -232.5, 55.9), G3D::Vector3(625.1, -200.2, 55.3),
    G3D::Vector3(621.3, -173.7, 53.0), G3D::Vector3(625.3, -146.0, 53.4),
    G3D::Vector3(610.7, -116.1, 53.9), G3D::Vector3(583.9, -115.8, 55.4),
    G3D::Vector3(583.9, -140.2, 55.2), G3D::Vector3(607.7, -185.8, 54.3),
    G3D::Vector3(657.0, -156.4, 85.3), G3D::Vector3(647.2, -117.0, 80.9),
    G3D::Vector3(646.3, -88.3, 77.3), G3D::Vector3(653.2, -66.1, 76.6),
    G3D::Vector3(655.8, -34.9, 75.5), G3D::Vector3(644.8, -11.7, 76.2),
    G3D::Vector3(620.6, -10.5, 72.9), G3D::Vector3(591.7, -11.3, 68.9),
    G3D::Vector3(564.4, -25.4, 63.0), G3D::Vector3(584.7, -51.9, 57.2),
    G3D::Vector3(601.4, -72.8, 60.1), G3D::Vector3(621.8, -105.2, 62.7),
    G3D::Vector3(662.5, -110.6, 71.6), G3D::Vector3(693.3, -119.0, 77.8),
    G3D::Vector3(698.6, -159.2, 79.7), G3D::Vector3(665.2, -162.0, 77.7)};

static std::vector<G3D::Vector3> build_commander_path(Creature* c)
{
    std::vector<G3D::Vector3> path;

    switch (c->GetEntry())
    {
    case NPC_ID_SLIDORE_GRYPHON:
        path.assign(
            std::begin(slidore_patrol_path), std::end(slidore_patrol_path));
        break;
    case NPC_ID_VIPORE_GRYPHON:
        path.assign(
            std::begin(vipore_patrol_path), std::end(vipore_patrol_path));
        break;
    case NPC_ID_ICHMAN_GRYPHON:
        path.assign(
            std::begin(ichman_patrol_path), std::end(ichman_patrol_path));
        break;
    case NPC_ID_GUSE_WAR_RIDER:
        path.assign(std::begin(guse_patrol_path), std::end(guse_patrol_path));
        break;
    case NPC_ID_JEZTOR_WAR_RIDER:
        path.assign(
            std::begin(jeztor_patrol_path), std::end(jeztor_patrol_path));
        break;
    case NPC_ID_MULVERICK_WAR_RIDER:
        path.assign(
            std::begin(mulverick_patrol_path), std::end(mulverick_patrol_path));
        break;
    default:
        logging.error(
            "build_commander_path: tried to build path for someone that isn't "
            "a wing commander");
        break;
    }

    return path;
}

void add_av_aerial_scripts()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "npc_aerial_assault_rider";
    pNewScript->GetAI = &GetAI_npc_aerial_assault_rider;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "go_av_beacon";
    pNewScript->pGOUse = &GOUse_av_beacon;
    pNewScript->RegisterSelf();
}
