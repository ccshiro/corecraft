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

enum
{
    NPC_TEXT_ALLY_DEFAULT =
        6174, // "My faith guides my actions, $r. I know that Ivus will come."
    NPC_TEXT_HORDE_DEFAULT = 6093, // "The blood of our enemies shall be spilt
                                   // upon the battlefield. The great elementals
                                   // shall answer our call and crush all that
                                   // stand in our way."
    NPC_TEXT_ALLY_READY = 6111, // "We have gathered the storm crystals needed
                                // to call upon the Forest Lord. To complete the
                                // ritual we will need escorting and assistance.
                                // Let us know when you have gathered a squad of
                                // able soldiers." (XXX: made-up)
    NPC_TEXT_HORDE_READY =
        6112, // "The sacrificial blood needed to appease Lokholar himself has
              // been accumulated. To complete the ritual we will need a group
              // of soldiers guarding and assisting us." (XXX: made-up)
};

bool GossipHello_npc_av_druid(Player* p, Creature* c)
{
    auto av = dynamic_cast<alterac_valley*>(c->GetMap()->GetInstanceData());
    if (!av)
        return true;

    if (c->isQuestGiver())
        p->PrepareQuestMenu(c->GetObjectGuid());

    if (av->GetData(p->GetTeam() == ALLIANCE ? STORM_CRYSTAL_COUNT :
                                               SOLDIER_BLOOD_COUNT) >= 200 &&
        av->boss_data(p->GetTeam()).boss.IsEmpty())
    {
        if (p->GetTeam() == ALLIANCE)
            p->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT,
                "We will protect, and assist you, Arch Druid Renferal. Deploy!",
                GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 10);
        else
            p->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT,
                "We will protect, and assist you, Primalist Thurloga. Deploy!",
                GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 10);

        p->SEND_GOSSIP_MENU(p->GetTeam() == ALLIANCE ? NPC_TEXT_ALLY_READY :
                                                       NPC_TEXT_HORDE_READY,
            c->GetObjectGuid());
    }
    else
    {
        p->SEND_GOSSIP_MENU(p->GetTeam() == ALLIANCE ? NPC_TEXT_ALLY_DEFAULT :
                                                       NPC_TEXT_HORDE_DEFAULT,
            c->GetObjectGuid());
    }

    return true;
}

namespace druids
{
static const ObjectGuid arch_druid(HIGHGUID_UNIT, 13442, 53640);
static const ObjectGuid one(HIGHGUID_UNIT, 13443, 53644);
static const ObjectGuid two(HIGHGUID_UNIT, 13443, 53643);
static const ObjectGuid three(HIGHGUID_UNIT, 13443, 53642);
}

namespace shamans
{
static const ObjectGuid primalist(HIGHGUID_UNIT, 13236, 53159);
static const ObjectGuid one(HIGHGUID_UNIT, 13284, 1003282);
static const ObjectGuid two(HIGHGUID_UNIT, 13284, 1003283);
static const ObjectGuid three(HIGHGUID_UNIT, 13284, 1003284);
}

bool GossipSelect_npc_av_druid(Player* p, Creature* c, uint32, uint32 action)
{
    auto av = dynamic_cast<alterac_valley*>(c->GetMap()->GetInstanceData());
    if (!av)
        return true;

    if (action != GOSSIP_ACTION_INFO_DEF + 10)
        return true;

    // cannot have ivus/lokholar spawned already
    if (!av->boss_data(p->GetTeam()).boss.IsEmpty())
        return true;

    uint32 data =
        p->GetTeam() == ALLIANCE ? STORM_CRYSTAL_COUNT : SOLDIER_BLOOD_COUNT;
    uint32 count = av->GetData(data);

    if (count < 200 || !c->HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP))
        return true;

    int32 grp_id = c->GetMap()->GetCreatureGroupMgr().CreateNewGroup(
        p->GetTeam() == ALLIANCE ? "Arch Druid" : "Primalist", true);
    CreatureGroup* grp = c->GetMap()->GetCreatureGroupMgr().GetGroup(grp_id);
    if (!grp)
        return true;

    av->SetData(data, count - 200);

    auto notify_add_grp = [grp](Creature* c)
    {
        if (c->AI())
            c->AI()->Notify(1);
        grp->AddMember(c, false);
        c->SetActiveObjectState(true);
        c->RemoveFlag(
            UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP | UNIT_NPC_FLAG_QUESTGIVER);
    };

    notify_add_grp(c);
    if (auto d = c->GetMap()->GetCreature(
            p->GetTeam() == ALLIANCE ? druids::one : shamans::one))
        notify_add_grp(d);
    if (auto d = c->GetMap()->GetCreature(
            p->GetTeam() == ALLIANCE ? druids::two : shamans::two))
        notify_add_grp(d);
    if (auto d = c->GetMap()->GetCreature(
            p->GetTeam() == ALLIANCE ? druids::three : shamans::three))
        notify_add_grp(d);

    av->boss_data(p->GetTeam()).stage = 1;
    av->boss_data(p->GetTeam()).grp_id = grp_id;

    return true;
}

std::vector<Creature*> alterac_valley::boss_creatures(Team t)
{
    std::vector<Creature*> vec;

    auto lookup_func = [this](ObjectGuid g) -> Creature*
    {
        auto c = instance->GetCreature(g);
        if (c && c->isAlive())
            return c;
        return nullptr;
    };

    if (t == ALLIANCE)
    {
        auto ad = lookup_func(druids::arch_druid);
        auto d1 = lookup_func(druids::one);
        auto d2 = lookup_func(druids::two);
        auto d3 = lookup_func(druids::three);
        vec.push_back(ad);
        vec.push_back(d1);
        vec.push_back(d2);
        vec.push_back(d3);
    }
    else
    {
        auto pr = lookup_func(shamans::primalist);
        auto s1 = lookup_func(shamans::one);
        auto s2 = lookup_func(shamans::two);
        auto s3 = lookup_func(shamans::three);
        vec.push_back(pr);
        vec.push_back(s1);
        vec.push_back(s2);
        vec.push_back(s3);
    }

    return vec;
}

static const DynamicWaypoint ally_to_fos_path[] = {
    DynamicWaypoint(629.5, -101.7, 40.3, 4.6, 0, true),
    DynamicWaypoint(624.1, -116.7, 36.4, 4.4, 0, true),
    DynamicWaypoint(619.8, -131.8, 33.6, 4.4, 0, true),
    DynamicWaypoint(617.8, -146.7, 33.4, 4.6, 0, true),
    DynamicWaypoint(619.2, -153.9, 33.7, 4.9, 0, true),
    DynamicWaypoint(621.5, -168.0, 36.4, 4.9, 0, true),
    DynamicWaypoint(624.5, -188.1, 38.6, 4.9, 0, true),
    DynamicWaypoint(627.6, -208.2, 39.0, 4.9, 0, true),
    DynamicWaypoint(630.3, -226.3, 38.1, 4.9, 0, true),
    DynamicWaypoint(633.1, -244.6, 35.7, 4.9, 0, true),
    DynamicWaypoint(636.1, -264.0, 31.6, 4.9, 0, true),
    DynamicWaypoint(638.3, -278.6, 30.2, 4.9, 0, true),
    DynamicWaypoint(636.1, -289.6, 30.1, 4.5, 0, true),
    DynamicWaypoint(633.6, -306.2, 30.1, 4.6, 0, true),
    DynamicWaypoint(625.0, -320.2, 30.1, 4.1, 0, true),
    DynamicWaypoint(612.4, -333.4, 30.3, 3.9, 0, true),
    DynamicWaypoint(599.3, -337.1, 30.3, 3.4, 0, true),
    DynamicWaypoint(582.5, -333.6, 29.6, 2.9, 0, true),
    DynamicWaypoint(570.6, -328.4, 23.5, 2.7, 0, true),
    DynamicWaypoint(558.7, -324.4, 16.4, 2.8, 0, true),
    DynamicWaypoint(542.9, -321.1, 6.4, 3.0, 0, true),
    DynamicWaypoint(523.7, -321.7, -0.3, 3.2, 0, true),
    DynamicWaypoint(505.9, -330.9, -1.1, 3.6, 0, true),
    DynamicWaypoint(494.4, -341.6, -1.2, 3.9, 0, true),
    DynamicWaypoint(474.0, -361.4, -1.2, 3.9, 0, true),
    DynamicWaypoint(457.7, -372.9, -1.2, 3.7, 0, true),
    DynamicWaypoint(436.6, -378.0, -1.2, 3.4, 0, true),
    DynamicWaypoint(420.8, -380.2, -1.2, 3.4, 0, true),
    DynamicWaypoint(406.8, -387.4, -1.2, 3.6, 0, true),
    DynamicWaypoint(384.6, -392.3, -1.1, 3.3, 0, true),
    DynamicWaypoint(361.2, -391.0, -0.3, 3.1, 0, true),
    DynamicWaypoint(337.6, -385.4, -0.5, 2.9, 0, true),
    DynamicWaypoint(314.2, -381.1, -0.8, 3.0, 0, true),
    DynamicWaypoint(301.1, -382.1, 1.1, 3.3, 0, true),
    DynamicWaypoint(289.3, -384.2, 4.6, 3.3, 0, true),
    DynamicWaypoint(278.9, -389.8, 9.3, 3.6, 0, true),
    DynamicWaypoint(271.1, -395.2, 15.1, 3.8, 0, true),
    DynamicWaypoint(264.5, -402.5, 22.2, 3.8, 0, true),
    DynamicWaypoint(256.5, -409.9, 29.4, 3.9, 0, true),
    DynamicWaypoint(244.3, -416.7, 35.2, 3.7, 0, true),
    DynamicWaypoint(225.2, -419.4, 39.4, 3.1, 0, true),
    DynamicWaypoint(208.1, -412.3, 41.9, 2.7, 0, true),
    DynamicWaypoint(184.6, -406.6, 42.7, 2.9, 0, true),
    DynamicWaypoint(164.2, -401.7, 42.6, 2.9, 0, true),
    DynamicWaypoint(137.1, -386.9, 42.3, 2.6, 0, true),
    DynamicWaypoint(117.3, -379.8, 43.7, 2.8, 0, true),
    DynamicWaypoint(102.1, -387.0, 45.1, 3.6, 0, true),
    DynamicWaypoint(83.4, -391.5, 45.0, 3.4, 0, true),
    DynamicWaypoint(60.7, -390.3, 45.2, 3.1, 0, true),
    DynamicWaypoint(38.2, -390.3, 45.6, 3.1, 0, true),
    DynamicWaypoint(17.9, -396.8, 45.6, 3.5, 0, true),
    DynamicWaypoint(2.2, -410.8, 45.3, 3.8, 0, true),
    DynamicWaypoint(-15.7, -417.9, 43.7, 3.5, 0, true),
    DynamicWaypoint(-34.0, -410.1, 31.9, 2.8, 0, true),
    DynamicWaypoint(-45.5, -394.6, 20.0, 2.2, 0, true),
    DynamicWaypoint(-54.1, -375.8, 13.3, 2.0, 0, true),
    DynamicWaypoint(-71.6, -376.1, 14.1, 3.2, 0, true),
    DynamicWaypoint(-97.3, -388.6, 14.4, 3.5, 0, true),
    DynamicWaypoint(-119.2, -378.9, 12.5, 2.8, 0, true),
    DynamicWaypoint(-147.5, -362.8, 9.8, 2.6, 0, true),
    DynamicWaypoint(-158.9, -348.4, 10.8, 3.2, 0, true),
    DynamicWaypoint(-172.7, -351.1, 9.6, 3.3, 0, true),
    DynamicWaypoint(-180.1, -361.8, 7.1, 4.1, 0, true),
    DynamicWaypoint(-184.1, -356.9, 6.7, 2.3, 0, true),
    DynamicWaypoint(
        -192.4, -349.4, 6.7, 2.4, RESPAWN_ONE_WEEK* IN_MILLISECONDS, true)};

static const DynamicWaypoint horde_to_fos_path[] = {
    DynamicWaypoint(-1248.0, -342.4, 59.2, 4.8, 0, true),
    DynamicWaypoint(-1246.8, -353.6, 59.5, 4.8, 0, true),
    DynamicWaypoint(-1241.1, -365.2, 59.6, 5.2, 0, true),
    DynamicWaypoint(-1227.6, -368.9, 58.1, 6.0, 0, true),
    DynamicWaypoint(-1213.4, -366.9, 56.3, 0.1, 0, true),
    DynamicWaypoint(-1195.5, -366.5, 53.1, 0.0, 0, true),
    DynamicWaypoint(-1182.0, -360.8, 52.4, 0.4, 0, true),
    DynamicWaypoint(-1165.8, -354.2, 51.7, 0.4, 0, true),
    DynamicWaypoint(-1148.8, -351.8, 51.1, 0.1, 0, true),
    DynamicWaypoint(-1136.6, -350.3, 51.3, 0.1, 0, true),
    DynamicWaypoint(-1119.8, -359.6, 51.5, 5.8, 0, true),
    DynamicWaypoint(-1103.8, -365.9, 51.5, 5.9, 0, true),
    DynamicWaypoint(-1088.4, -366.5, 51.4, 6.3, 0, true),
    DynamicWaypoint(-1070.3, -364.0, 51.4, 0.1, 0, true),
    DynamicWaypoint(-1056.9, -370.1, 51.4, 5.8, 0, true),
    DynamicWaypoint(-1047.9, -383.0, 51.1, 5.3, 0, true),
    DynamicWaypoint(-1034.0, -391.1, 50.7, 5.8, 0, true),
    DynamicWaypoint(-1015.2, -397.6, 50.8, 6.0, 0, true),
    DynamicWaypoint(-996.4, -399.6, 50.2, 6.2, 0, true),
    DynamicWaypoint(-975.1, -399.3, 49.3, 0.1, 0, true),
    DynamicWaypoint(-953.3, -394.0, 48.7, 0.2, 0, true),
    DynamicWaypoint(-939.1, -390.4, 48.8, 0.2, 0, true),
    DynamicWaypoint(-922.1, -386.2, 49.5, 0.2, 0, true),
    DynamicWaypoint(-904.1, -381.7, 48.8, 0.2, 0, true),
    DynamicWaypoint(-892.9, -381.9, 48.6, 6.3, 0, true),
    DynamicWaypoint(-877.4, -388.5, 48.6, 5.9, 0, true),
    DynamicWaypoint(-863.4, -391.0, 48.9, 6.2, 0, true),
    DynamicWaypoint(-846.7, -392.7, 50.7, 6.2, 0, true),
    DynamicWaypoint(-837.2, -404.4, 51.6, 5.4, 0, true),
    DynamicWaypoint(-830.4, -420.1, 52.6, 5.1, 0, true),
    DynamicWaypoint(-824.6, -434.7, 53.3, 5.1, 0, true),
    DynamicWaypoint(-817.8, -444.1, 54.6, 5.3, 0, true),
    DynamicWaypoint(-807.3, -448.9, 55.1, 6.0, 0, true),
    DynamicWaypoint(-798.2, -445.9, 56.2, 0.3, 0, true),
    DynamicWaypoint(-789.1, -441.1, 58.1, 0.6, 0, true),
    DynamicWaypoint(-778.4, -433.2, 61.3, 0.6, 0, true),
    DynamicWaypoint(-770.2, -431.0, 63.2, 0.3, 0, true),
    DynamicWaypoint(-757.3, -430.3, 65.3, 0.1, 0, true),
    DynamicWaypoint(-744.6, -425.3, 66.8, 0.4, 0, true),
    DynamicWaypoint(-731.5, -417.6, 67.6, 0.6, 0, true),
    DynamicWaypoint(-718.5, -405.9, 67.6, 0.8, 0, true),
    DynamicWaypoint(-715.2, -392.7, 67.6, 1.5, 0, true),
    DynamicWaypoint(-713.7, -378.4, 67.3, 1.5, 0, true),
    DynamicWaypoint(-709.3, -363.3, 66.7, 1.3, 0, true),
    DynamicWaypoint(-698.2, -366.5, 66.1, 6.0, 0, true),
    DynamicWaypoint(-685.8, -372.3, 65.7, 5.8, 0, true),
    DynamicWaypoint(-665.2, -382.0, 64.3, 5.8, 0, true),
    DynamicWaypoint(-646.2, -390.9, 60.6, 5.8, 0, true),
    DynamicWaypoint(-629.7, -395.7, 59.1, 6.0, 0, true),
    DynamicWaypoint(-621.9, -385.4, 58.2, 0.9, 0, true),
    DynamicWaypoint(-621.7, -368.5, 57.0, 1.6, 0, true),
    DynamicWaypoint(-620.6, -352.4, 55.7, 1.5, 0, true),
    DynamicWaypoint(-617.7, -345.3, 55.1, 1.2, 0, true),
    DynamicWaypoint(-609.2, -338.6, 53.8, 0.7, 0, true),
    DynamicWaypoint(-600.1, -327.4, 50.8, 0.9, 0, true),
    DynamicWaypoint(-589.7, -319.1, 48.7, 0.7, 0, true),
    DynamicWaypoint(-575.1, -317.5, 44.9, 0.1, 0, true),
    DynamicWaypoint(-560.7, -327.0, 40.0, 5.7, 0, true),
    DynamicWaypoint(-544.9, -337.4, 37.7, 5.7, 0, true),
    DynamicWaypoint(-533.1, -342.7, 35.7, 5.9, 0, true),
    DynamicWaypoint(-523.0, -341.1, 34.6, 0.2, 0, true),
    DynamicWaypoint(-516.2, -331.7, 33.7, 0.9, 0, true),
    DynamicWaypoint(-505.5, -316.2, 32.1, 1.0, 0, true),
    DynamicWaypoint(-496.3, -296.5, 30.9, 1.1, 0, true),
    DynamicWaypoint(-489.9, -284.8, 28.6, 0.9, 0, true),
    DynamicWaypoint(-477.6, -279.0, 25.6, 0.4, 0, true),
    DynamicWaypoint(-456.3, -277.9, 22.1, 0.1, 0, true),
    DynamicWaypoint(-440.6, -272.7, 20.4, 0.3, 0, true),
    DynamicWaypoint(-437.4, -260.2, 20.8, 1.3, 0, true),
    DynamicWaypoint(-433.5, -244.9, 22.9, 1.3, 0, true),
    DynamicWaypoint(-424.9, -229.7, 22.7, 1.1, 0, true),
    DynamicWaypoint(-419.1, -219.3, 24.3, 1.1, 0, true),
    DynamicWaypoint(-425.0, -203.6, 26.2, 1.8, 0, true),
    DynamicWaypoint(-423.7, -189.7, 25.9, 1.5, 0, true),
    DynamicWaypoint(-418.7, -168.2, 23.8, 1.3, 0, true),
    DynamicWaypoint(-419.1, -146.8, 24.1, 1.7, 0, true),
    DynamicWaypoint(-417.5, -126.3, 24.0, 1.5, 0, true),
    DynamicWaypoint(-404.9, -125.3, 25.5, 0.1, 0, true),
    DynamicWaypoint(-392.2, -127.8, 26.1, 6.1, 0, true),
    DynamicWaypoint(-377.1, -122.2, 26.0, 0.4, 0, true),
    DynamicWaypoint(-368.7, -122.0, 26.4, 6.3, 0, true),
    DynamicWaypoint(
        -359.3, -130.2, 26.4, 5.6, RESPAWN_ONE_WEEK* IN_MILLISECONDS, true)};

void alterac_valley::update_ivus_lok(ivus_lok_data& d, Team t)
{
    if (d.stage == 0)
        return;

    std::vector<Creature*> vec = boss_creatures(t);
    if (d.stage < 5 && (vec[0] == nullptr || vec[0]->isDead()))
    {
        d.reset(this, t);
        return;
    }

    if (d.stage == 1)
    {
        // Wait for mobs to start escort moving (uses idle motion type)
        for (auto c : vec)
        {
            if (c && c->movement_gens.top_id() == movement::gen::point)
            {
                d.stage = 2;
                break;
            }
        }
    }
    else if (d.stage == 2)
    {
        CreatureGroup* grp = instance->GetCreatureGroupMgr().GetGroup(d.grp_id);
        if (!grp)
            return;

        // Wait for mobs to finish WP moving
        bool all_idle = true;
        for (auto c : vec)
        {
            if (c && c->movement_gens.has(movement::gen::point))
                all_idle = false;
        }

        if (all_idle)
        {
            d.stage = 3;

            grp->AddFlag(CREATURE_GROUP_FLAG_GROUP_MOVEMENT);

            // Add waypoints
            auto begin = t == ALLIANCE ? std::begin(ally_to_fos_path) :
                                         std::begin(horde_to_fos_path);
            auto end = t == ALLIANCE ? std::end(ally_to_fos_path) :
                                       std::end(horde_to_fos_path);
            for (auto itr = begin; itr != end; ++itr)
                vec[0]
                    ->GetMap()
                    ->GetCreatureGroupMgr()
                    .GetMovementMgr()
                    .AddWaypoint(grp->GetId(), *itr);

            // Notify AIs (so they can mount) and update group formation
            for (auto c : grp->GetMembers())
            {
                c->AI()->Notify(2);
                c->GetMap()
                    ->GetCreatureGroupMgr()
                    .GetMovementMgr()
                    .SetNewFormation(grp->GetId(), c);
            }

            // Start movement
            vec[0]
                ->GetMap()
                ->GetCreatureGroupMgr()
                .GetMovementMgr()
                .StartMovement(grp->GetId(), grp->GetMembers());
        }
    }
    else if (d.stage == 3)
    {
        bool all_idle = true;
        for (auto c : vec)
        {
            if (c && c->movement_gens.has(movement::gen::gwp))
                all_idle = false;
        }

        // Have we reached field of strife?
        if (all_idle)
        {
            if (t == ALLIANCE)
                vec[0]->MonsterSay(
                    "Soldiers of Stormpike, aid and protect us! The Forest "
                    "Lord has granted us his protection. The portal must now "
                    "be opened!",
                    0); // XXX: quote from wowwiki, might be wrong
            else
                vec[0]->MonsterSay(
                    "Watch as the blood seeps into the snow, children. The "
                    "elemental will be bound to us!",
                    0);

            // Cast conjure spell for all parties (make unattackable, and also
            // passive while casting this spell, XXX: probably not correct)
            for (auto c : vec)
                if (c)
                {
                    c->Unmount();
                    c->CastSpell(c, t == ALLIANCE ? 21646 : 21267, false);
                    c->SetFlag(UNIT_FIELD_FLAGS,
                        UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_PASSIVE);
                }

            d.stage = 4;
            d.timer = 10000;
        }
    }
    else if (d.stage == 4 && d.timer == 0)
    {
        d.stage = 5;

        for (auto c : vec)
            if (c)
            {
                c->RemoveFlag(UNIT_FIELD_FLAGS,
                    UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_PASSIVE);
                c->movement_gens.push(new movement::IdleMovementGenerator());
            }

        if (t == ALLIANCE)
            vec[0]->MonsterSay("Channel your energies into the Circle!",
                0); // XXX: made-up (derived from the horde one below, which is
                    // confirmed)
        else
            vec[0]->MonsterSay("Channel your energies into the Altar!", 0);

        // Spawn altar/circle
        if (t == ALLIANCE)
            vec[0]->SummonGameObject(
                178670, -200, -345, 6.75, 0, 0, 0, 0, 0, 0);
        else
            vec[0]->SummonGameObject(
                178465, -359, -134.24, 26.46, 0, 0, 0, 0, 0, 0);
    }
    else if (d.stage == 5)
    {
        // Wait for Ivus or Lokholar to spawn
        if (d.boss.IsEmpty())
            return;

        Creature* boss = instance->GetCreature(d.boss);
        if (!boss)
            return;

        // Reward honor for summoning Ivus or Lokholar
        if (auto bgmap = dynamic_cast<BattleGroundMap*>(instance))
            if (auto bg = dynamic_cast<BattleGroundAV*>(bgmap->GetBG()))
                bg->RewardHonorToTeam(bg->GetBonusHonorFromKill(3), t);

        // Despawn the summoning mobs
        if (t == ALLIANCE)
        {
            if (Creature* c = instance->GetCreature(druids::arch_druid))
                c->ForcedDespawn();
            if (Creature* c = instance->GetCreature(druids::one))
                c->ForcedDespawn();
            if (Creature* c = instance->GetCreature(druids::two))
                c->ForcedDespawn();
            if (Creature* c = instance->GetCreature(druids::three))
                c->ForcedDespawn();
        }
        else
        {
            if (Creature* c = instance->GetCreature(shamans::primalist))
                c->ForcedDespawn();
            if (Creature* c = instance->GetCreature(shamans::one))
                c->ForcedDespawn();
            if (Creature* c = instance->GetCreature(shamans::two))
                c->ForcedDespawn();
            if (Creature* c = instance->GetCreature(shamans::three))
                c->ForcedDespawn();
        }

        if (t == ALLIANCE)
            boss->MonsterYell(
                "Wicked, wicked, mortals! The forest weeps. The elements "
                "recoil at the destruction. Ivus must purge you from this "
                "world!",
                0);
        else
            boss->MonsterYell(
                "I drink in your suffering, mortal. Let your essence congeal "
                "with Lokholar!",
                0);

        // Start the field of strife path
        boss->AI()->Notify(1);

        // Move to next stage in 5 minutes
        d.timer = 1 * MINUTE * IN_MILLISECONDS;
        d.stage = 6;
    }
    else if (d.stage == 6 && d.timer == 0)
    {
        Creature* boss = instance->GetCreature(d.boss);
        if (!boss || !boss->isAlive())
        {
            d.reset(this, t);
            return;
        }

        d.stage = 7;

        // Start attack on base
        boss->AI()->Notify(2);
    }
    else if (d.stage == 7 && d.timer == 0)
    {
        d.timer = 1000;

        Creature* boss = instance->GetCreature(d.boss);
        if (!boss || !boss->isAlive())
        {
            d.reset(this, t);
            return;
        }
    }
}

void alterac_valley::ivus_lok_data::reset(alterac_valley* av, Team t)
{
    if (t == ALLIANCE)
    {
        // If the summoning mobs haven't respawned already, despawn them
        Creature* arch_druid = av->instance->GetCreature(druids::arch_druid);
        if (arch_druid &&
            !arch_druid->HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP))
        {
            arch_druid->ForcedDespawn();
            if (Creature* c = av->instance->GetCreature(druids::one))
                c->ForcedDespawn();
            if (Creature* c = av->instance->GetCreature(druids::two))
                c->ForcedDespawn();
            if (Creature* c = av->instance->GetCreature(druids::three))
                c->ForcedDespawn();
        }
    }
    else
    {
        // If the summoning mobs haven't respawned already, despawn them
        Creature* primalist = av->instance->GetCreature(shamans::primalist);
        if (primalist &&
            !primalist->HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP))
        {
            primalist->ForcedDespawn();
            if (Creature* c = av->instance->GetCreature(shamans::one))
                c->ForcedDespawn();
            if (Creature* c = av->instance->GetCreature(shamans::two))
                c->ForcedDespawn();
            if (Creature* c = av->instance->GetCreature(shamans::three))
                c->ForcedDespawn();
        }
    }

    if (Creature* b = av->instance->GetCreature(boss))
        b->ForcedDespawn();

    av->instance->GetCreatureGroupMgr().DeleteGroup(grp_id);

    boss.Clear();
    grp_id = 0;
    stage = 0;
    timer = 0;
}

void add_av_boss_summon_scripts()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "npc_av_druid";
    pNewScript->pGossipHello = &GossipHello_npc_av_druid;
    pNewScript->pGossipSelect = &GossipSelect_npc_av_druid;
    pNewScript->RegisterSelf();
}
