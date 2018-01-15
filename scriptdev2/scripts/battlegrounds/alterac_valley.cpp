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

enum
{
    NPC_TEXT_UPGRADE_ZERO = 6073,  // "Our units are not upgraded, and I don't
                                   // have enough supplies to upgrade them."
    NPC_TEXT_UPGRADE_ONE = 6217,   // "Our units are upgraded to Seasoned, but I
                                   // don't have enough supplies to upgrade them
                                   // to Veteran."
    NPC_TEXT_UPGRADE_TWO = 6218,   // "Our units are upgraded to Veteran, but I
                                   // don't have enough supplies to upgrade them
                                   // to Champion."
    NPC_TEXT_UPGRADE_THREE = 6222, // "Our units are upgraded to Champions, but
                                   // I can still collect more supplies."
    // Upgrade ready texts
    NPC_TEXT_UPGRADE_RDY_ZERO =
        6788, // "I have enough supplies to upgrade our troops to seasoned."
    NPC_TEXT_UPGRADE_RDY_ONE =
        6789, // "I have enough supplies to upgrade our troops to veterans."
    NPC_TEXT_UPGRADE_RDY_TWO =
        6790, // "I have enough supplies to upgrade our troops to champions."

    SCRAP_UPDATE_COUNT = 500,    // Amount of scraps needed for each upgrade
    NPC_TEXT_SCRAPS_ZERO = 6784, // "I barely have any supplies for upgrades."
    NPC_TEXT_SCRAPS_ONE =
        6735, // "I need many more supplies in order to upgrade our units."
    NPC_TEXT_SCRAPS_TWO_ZERO = 6776, // "I have about half the supplies needed
                                     // to upgrade to seasoned units."
    NPC_TEXT_SCRAPS_TWO_ONE = 6781, // "I have about half the supplies needed to
                                    // upgrade to veteran units."
    NPC_TEXT_SCRAPS_TWO_TWO = 6775, // "I have about half the supplies needed to
                                    // upgrade to champion units."
    NPC_TEXT_SCRAPS_THREE =
        6736, // "I almost have enough supplies to upgrade our troops."

    FROSTWOLF_FACTION_ID = 729,
    STORMPIKE_FACTION_ID = 730
};

bool GossipHello_armor_scraps(Player* p, Creature* c)
{
    if (c->isQuestGiver())
        p->PrepareQuestMenu(c->GetObjectGuid());

    if (auto av = dynamic_cast<alterac_valley*>(c->GetInstanceData()))
    {
        uint32 upgrade = av->GetData(p->GetTeam() == ALLIANCE ?
                                         ALLIANCE_SOLDIERS_UPGRADE_LEVEL :
                                         HORDE_SOLDIERS_UPGRADE_LEVEL);
        uint32 scraps =
            av->GetData(p->GetTeam() == ALLIANCE ? ALLIANCE_ARMOR_SCRAPS :
                                                   HORDE_ARMOR_SCRAPS);
        bool upgrade_rdy = (scraps >= (upgrade + 1) * 500);

        uint32 text_id;
        if (upgrade == 0)
            text_id = !upgrade_rdy ? NPC_TEXT_UPGRADE_ZERO :
                                     NPC_TEXT_UPGRADE_RDY_ZERO;
        else if (upgrade == 1)
            text_id =
                !upgrade_rdy ? NPC_TEXT_UPGRADE_ONE : NPC_TEXT_UPGRADE_RDY_ONE;
        else if (upgrade == 2)
            text_id =
                !upgrade_rdy ? NPC_TEXT_UPGRADE_TWO : NPC_TEXT_UPGRADE_RDY_TWO;
        else
            text_id = NPC_TEXT_UPGRADE_THREE;

        if (upgrade < 3)
        {
            if (!upgrade_rdy)
                p->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT,
                    "How many more supplies are needed for the next upgrade?",
                    GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 10);
            else
            {
                // Requires honored to upgrade
                if (p->GetReputationRank(p->GetTeam() == ALLIANCE ?
                                             STORMPIKE_FACTION_ID :
                                             FROSTWOLF_FACTION_ID) >=
                    REP_HONORED)
                    p->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, "Upgrade our troops.",
                        GOSSIP_SENDER_MAIN,
                        GOSSIP_ACTION_INFO_DEF + 20); // XXX: This say has not
                                                      // been confirmed and is
                                                      // made up
            }
        }

        p->SEND_GOSSIP_MENU(text_id, c->GetObjectGuid());
    }

    return true;
}

bool GossipSelect_armor_scraps(Player* p, Creature* c, uint32, uint32 action)
{
    auto av = dynamic_cast<alterac_valley*>(c->GetInstanceData());
    if (!av)
        return true;

    uint32 upgrade =
        av->GetData(p->GetTeam() == ALLIANCE ? ALLIANCE_SOLDIERS_UPGRADE_LEVEL :
                                               HORDE_SOLDIERS_UPGRADE_LEVEL);
    uint32 scraps = av->GetData(
        p->GetTeam() == ALLIANCE ? ALLIANCE_ARMOR_SCRAPS : HORDE_ARMOR_SCRAPS);
    bool upgrade_rdy = (scraps >= (upgrade + 1) * 500);

    if (action == GOSSIP_ACTION_INFO_DEF + 10)
    {
        scraps %= SCRAP_UPDATE_COUNT;

        uint32 text_id;
        if (scraps < 100)
            text_id = NPC_TEXT_SCRAPS_ZERO;
        else if (scraps < 250)
            text_id = NPC_TEXT_SCRAPS_ONE;
        else if (scraps < 400)
        {
            if (upgrade == 0)
                text_id = NPC_TEXT_SCRAPS_TWO_ZERO;
            else if (upgrade == 1)
                text_id = NPC_TEXT_SCRAPS_TWO_ONE;
            else
                text_id = NPC_TEXT_SCRAPS_TWO_TWO;
        }
        else
            text_id = NPC_TEXT_SCRAPS_THREE;

        p->SEND_GOSSIP_MENU(text_id, c->GetObjectGuid());
    }
    else if (action == GOSSIP_ACTION_INFO_DEF + 20)
    {
        if (upgrade < 3 && upgrade_rdy)
        {
            if (BattleGroundAV* bg_av =
                    dynamic_cast<BattleGroundAV*>(p->GetBattleGround()))
                bg_av->DoArmorScrapsUpgrade(p->GetTeam() == ALLIANCE ?
                                                BG_AV_TEAM_ALLIANCE :
                                                BG_AV_TEAM_HORDE);
            av->soldiers_update_callback(p->GetTeam());
        }
        p->CLOSE_GOSSIP_MENU();
    }

    return true;
}

enum
{
    NPC_TEXT_QUARTER_ALLY_MAIN =
        6255, // "They say an army travels on its stomach, and that's the truth.
              // For any offensive to succeed, your troops have to be well-fed
              // and well supplied."
    NPC_TEXT_QUARTER_HORDE_MAIN =
        6257, // "Some say that strength and bravery are most needed for an
              // army's success. And it's true! But an army also needs supplies
              // to keep itself in good shape, and it's my job to make sure they
              // have what they need."

    // Progress
    NPC_TEXT_QUARTER_ZERO = 6721, // "We have barely any supplies. If we want to
                                  // send a ground assault into the Field of
                                  // Strife, then we'll need a lot more!"
    NPC_TEXT_QUARTER_ONE = 6720,  // "We have some supplies, but not many. We'll
                                  // need a lot more in order to send a ground
                                  // assault into the Field of Strife."
    NPC_TEXT_QUARTER_TWO = 6718,  // "We have a lot of supplies, but we still
                                  // need a lot more in order to send a ground
                                  // assault into the Field of Strife."
    NPC_TEXT_QUARTER_THREE = 6722, // "We almost have enough supplies to launch
                                   // a ground assault. Just a few more, now!"
    NPC_TEXT_QUARTER_FOUR_ALLY =
        6730, // "$N, we have enough supplies to launch ground assaults. If your
              // standing with the Stormpike Guard is high enough, then speak
              // with Corporal Noreg Stormpike."
    NPC_TEXT_QUARTER_FOUR_HORDE =
        6719, // "$N, we have enough supplies to launch ground assaults into the
              // Field of Strife. If you want to send the orders, and your rank
              // is high enough, then speak with Sergeant Yazra Bloodsnarl in
              // Frostwolf Village."
};

// ground assault explained: http://eu.battle.net/wow/en/forum/topic/3313137052

bool GossipHello_quartermaster(Player* p, Creature* c)
{
    if (c->isQuestGiver())
        p->PrepareQuestMenu(c->GetObjectGuid());

    p->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT,
        "How many more supplies are needed to send ground assaults?",
        GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 10);
    p->ADD_GOSSIP_ITEM(GOSSIP_ICON_VENDOR, "I want to browse your goods.",
        GOSSIP_SENDER_MAIN, GOSSIP_ACTION_TRADE);

    p->SEND_GOSSIP_MENU(p->GetTeam() == ALLIANCE ? NPC_TEXT_QUARTER_ALLY_MAIN :
                                                   NPC_TEXT_QUARTER_HORDE_MAIN,
        c->GetObjectGuid());

    return true;
}

bool GossipSelect_quartermaster(Player* p, Creature* c, uint32, uint32 action)
{
    auto av = dynamic_cast<alterac_valley*>(c->GetMap()->GetInstanceData());
    if (!av)
        return true;

    if (action == GOSSIP_ACTION_TRADE)
        p->SEND_VENDORLIST(c->GetObjectGuid());
    else if (action == GOSSIP_ACTION_INFO_DEF + 10)
    {
        uint32 supplies = p->GetTeam() == ALLIANCE ?
                              av->GetData(ALLIANCE_TOTAL_SUPPLIES) :
                              av->GetData(HORDE_TOTAL_SUPPLIES);
        uint32 text_id;
        if (supplies < 70)
            text_id = NPC_TEXT_QUARTER_ZERO;
        else if (supplies < 140)
            text_id = NPC_TEXT_QUARTER_ONE;
        else if (supplies < 210)
            text_id = NPC_TEXT_QUARTER_TWO;
        else if (supplies < 280)
            text_id = NPC_TEXT_QUARTER_THREE;
        else
            text_id = p->GetTeam() == ALLIANCE ? NPC_TEXT_QUARTER_FOUR_ALLY :
                                                 NPC_TEXT_QUARTER_FOUR_HORDE;

        p->SEND_GOSSIP_MENU(text_id, c->GetObjectGuid());
    }

    return true;
}

enum
{
    SPELL_YAZRA_REND = 11977,    // 15-25 sec
    SPELL_NOREG_BLOCK = 12169,   // 15-25 sec
    SPELL_NOREG_REVENGE = 19130, // 10-15 sec

    NPC_TEXT_YAZRA_DEFAULT =
        6285, // "$N!  Our supplies are dwindling and until we get more we can't
              // launch large ground assaults into the Field of Strife!$B$BSpeak
              // with the Frostwolf Quartermaster.  He'll tell you what you can
              // do to resupply our assault forces." (XXX: made-up, derived from
              // noreg)
    NPC_TEXT_YAZRA_VETERAN =
        6283, // "$N, it's good to see veterans like you in the field!  I'm
              // sorry to report that our supplies are currently too low to
              // launch any large ground assaults against the Alliance.$B$BSpeak
              // with our quartermaster; he has supply missions available."
              // (XXX: made-up, derived from noreg)
    NPC_TEXT_NOREG_DEFAULT =
        6284, // "$N!  Our supplies are dwindling and until we get more we can't
              // launch large ground assaults into the Field of Strife!$B$BSpeak
              // with the Stormpike Quartermaster.  He'll tell you what you can
              // do to resupply our assault forces."
    NPC_TEXT_NOREG_VETERAN =
        6288, // "$N, it's good to see veterans like you in the field!  I'm
              // sorry to report that our supplies are currently too low to
              // launch any large ground assaults against the Horde.$B$BSpeak
              // with our quartermaster; he has supply missions available."
    NPC_TEXT_GROUND_READY_LOW = 6280, // "We are ready to launch a ground
                                      // assault, but I need someone of higher
                                      // standing to whom I can entrust these
                                      // orders." (XXX: Made-up)
    NPC_TEXT_GROUND_READY_ALLY =
        6276, // "Ah, $N! You're just the person I was hoping to see! We have
              // enough supplies to send a ground assault against the Horde!
              // Field Marshal Teravaine is waiting in the Field of Strife for
              // orders...$B$BYou're a brave veteran who's proven himself time
              // and again. Do you want to deliver Teravaine the orders?"
    NPC_TEXT_GROUND_READY_HORDE =
        6278, // "Ah, $N! You're just the person I was hoping to see! We have
              // enough supplies to send a ground assault against the Alliance!
              // Warmaster Garrick is waiting in the Field of Strife for
              // orders...$B$BYou're a brave veteran who's proven himself time
              // and again. Do you want to deliver Garrick the orders?" (XXX:
              // derived from 6276)
    NPC_TEXT_HAS_ORDERS_ALLY =
        6134, // "$N, what are you waiting for?  Find Field Marshal Teravaine in
              // the Field of Strife and give him the assault orders.  Once he
              // gets the orders, he and his troops will charge into the Horde
              // and take no prisoners!"
    NPC_TEXT_HAS_ORDERS_HORDE =
        6135, // "$N, what are you waiting for?  Find Field Marshal Teravaine in
              // the Field of Strife and give him the assault orders.  Once he
              // gets the orders, he and his troops will charge into the Horde
              // and take no prisoners!" (XXX: made-up, derived from 6134)
};

struct ground_assault_launcherAI : public ScriptedAI
{
    ground_assault_launcherAI(Creature* c) : ScriptedAI(c)
    {
        Reset();
        noreg = c->GetEntry() == 13447;
    }

    bool noreg;
    uint32 long_spell;
    uint32 short_spell;

    void Reset() override
    {
        long_spell = urand(15000, 25000);
        short_spell = urand(10000, 15000);
    }

    void UpdateAI(uint32 diff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        if (long_spell <= diff)
        {
            if (DoCastSpellIfCan(noreg ? m_creature : m_creature->getVictim(),
                    noreg ? SPELL_NOREG_BLOCK : SPELL_YAZRA_REND) == CAST_OK)
                long_spell = urand(15000, 25000);
        }
        else
            long_spell -= diff;

        if (short_spell <= diff && noreg)
        {
            if (DoCastSpellIfCan(
                    m_creature->getVictim(), SPELL_NOREG_REVENGE) == CAST_OK)
                short_spell = urand(15000, 25000);
        }
        else if (noreg)
            short_spell -= diff;

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_ground_assault_launcher(Creature* c)
{
    return new ground_assault_launcherAI(c);
}

bool GossipHello_ground_assault_launcher(Player* p, Creature* c)
{
    auto av = dynamic_cast<alterac_valley*>(p->GetMap()->GetInstanceData());
    if (!av)
        return true;

    bool orders_available;
    if (p->GetTeam() == ALLIANCE)
        orders_available = av->GetData(ALLIANCE_TOTAL_SUPPLIES) >= 280;
    else
        orders_available = av->GetData(HORDE_TOTAL_SUPPLIES) >= 280;

    uint32 text_id;
    uint32 orders_item = p->GetTeam() == ALLIANCE ? 17353 : 17442;

    if (p->HasItemCount(orders_item, 1))
    {
        text_id = p->GetTeam() == ALLIANCE ? NPC_TEXT_HAS_ORDERS_ALLY :
                                             NPC_TEXT_HAS_ORDERS_HORDE;
    }
    else if (orders_available)
    {
        // Requires honored to launch
        if (p->GetReputationRank(p->GetTeam() == ALLIANCE ?
                                     STORMPIKE_FACTION_ID :
                                     FROSTWOLF_FACTION_ID) < REP_HONORED)
        {
            text_id = NPC_TEXT_GROUND_READY_LOW;
        }
        else
        {
            if (p->GetTeam() == ALLIANCE)
                text_id = NPC_TEXT_GROUND_READY_ALLY;
            else
                text_id = NPC_TEXT_GROUND_READY_HORDE;
            p->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT,
                "Yes, I will deliver the orders.", GOSSIP_SENDER_MAIN,
                GOSSIP_ACTION_INFO_DEF + 10);
        }
    }
    else
    {
        // Different greeting if you're honored or not
        if (p->GetReputationRank(p->GetTeam() == ALLIANCE ?
                                     STORMPIKE_FACTION_ID :
                                     FROSTWOLF_FACTION_ID) < REP_HONORED)
            text_id = p->GetTeam() == ALLIANCE ? NPC_TEXT_NOREG_DEFAULT :
                                                 NPC_TEXT_YAZRA_DEFAULT;
        else
            text_id = p->GetTeam() == ALLIANCE ? NPC_TEXT_NOREG_VETERAN :
                                                 NPC_TEXT_YAZRA_VETERAN;
        if (p->GetTeam() == ALLIANCE)
            p->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT,
                "Where is the Stormpike Quartermaster?", GOSSIP_SENDER_MAIN,
                GOSSIP_ACTION_INFO_DEF + 20);
        else
            p->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT,
                "Where is the Frostwolf Quartermaster?", GOSSIP_SENDER_MAIN,
                GOSSIP_ACTION_INFO_DEF + 20);
    }

    p->SEND_GOSSIP_MENU(text_id, c->GetObjectGuid());

    return true;
}

bool GossipSelect_ground_assault_launcher(
    Player* p, Creature* c, uint32, uint32 action)
{
    auto av = dynamic_cast<alterac_valley*>(c->GetMap()->GetInstanceData());
    if (!av)
        return true;

    if (action == GOSSIP_ACTION_INFO_DEF + 20)
    {
        if (p->GetTeam() == ALLIANCE)
            c->MonsterSay(
                "The Stormpike Quartermaster keeps his supplies under an "
                "awning, just west of here.",
                0);
        else
            c->MonsterSay(
                "The Frostwolf Quartermaster is southwest of this bunker. "
                "Enter the bunker and then exit on the right, you'll see the "
                "quartermaster a short way down the path.",
                0);
        p->CLOSE_GOSSIP_MENU();
        return true;
    }

    if (action != GOSSIP_ACTION_INFO_DEF + 10)
        return true;

    uint32 orders_item = p->GetTeam() == ALLIANCE ? 17353 : 17442;

    // Cannot be used when already possessing an assault order
    if (p->HasItemCount(orders_item, 1))
        return true;

    // Option only usable by honored people
    if (p->GetReputationRank(p->GetTeam() == ALLIANCE ?
                                 STORMPIKE_FACTION_ID :
                                 FROSTWOLF_FACTION_ID) < REP_HONORED)
        return true;

    bool assault_rdy;
    if (p->GetTeam() == ALLIANCE)
        assault_rdy = av->GetData(ALLIANCE_TOTAL_SUPPLIES) >= 280 &&
                      av->ally_ground_cd < WorldTimer::time_no_syscall();
    else
        assault_rdy = av->GetData(HORDE_TOTAL_SUPPLIES) >= 280 &&
                      av->horde_ground_cd < WorldTimer::time_no_syscall();

    // If the assault is ready we also need to spawn Teravaine or Garrick
    if (assault_rdy)
    {
        if (p->GetTeam() == ALLIANCE)
            c->SummonCreature(13446, -263.425, -416.792, 17.4983, 5.52903,
                TEMPSUMMON_CORPSE_TIMED_DESPAWN, 10 * IN_MILLISECONDS,
                SUMMON_OPT_NO_LOOT);
        else
            c->SummonCreature(13449, -268.716, -145.438, 13.1008, 2.76699,
                TEMPSUMMON_CORPSE_TIMED_DESPAWN, 10 * IN_MILLISECONDS,
                SUMMON_OPT_NO_LOOT);

        // Disable spawning of new Teravaine/Garrick until they are dead (upon
        // when a cooldown starts)
        if (p->GetTeam() == ALLIANCE)
            av->ally_ground_cd = std::numeric_limits<time_t>::max();
        else
            av->horde_ground_cd = std::numeric_limits<time_t>::max();
    }

    // Give out the orders item
    p->add_item(orders_item, 1);

    p->CLOSE_GOSSIP_MENU();
    return true;
}

struct npc_empty_stables_petAI : public ScriptedAI
{
    npc_empty_stables_petAI(Creature* c) : ScriptedAI(c) {}
    void Reset() override {}

    ObjectGuid owner;

    void SummonedBy(WorldObject* object) override
    {
        if (object->GetTypeId() != TYPEID_PLAYER)
            return;

        owner = object->GetObjectGuid();
        m_creature->movement_gens.push(new movement::FollowMovementGenerator(
            static_cast<Player*>(object)));
    }

    void UpdateAI(uint32 /*diff*/) override
    {
        bool unsummon = true;

        if (owner)
        {
            WorldObject* owner_obj =
                m_creature->GetMap()->GetWorldObject(owner);
            if (owner_obj && owner_obj->GetTypeId() == TYPEID_PLAYER &&
                static_cast<Player*>(owner_obj)->isAlive() &&
                m_creature->IsWithinDistInMap(owner_obj, 60.0f))
                unsummon = false;
        }

        if (unsummon)
            m_creature->ForcedDespawn();
    }
};

CreatureAI* GetAI_empty_stables_pet(Creature* c)
{
    return new npc_empty_stables_petAI(c);
}

enum
{
    NPC_TEXT_STABLES_DEFAULT_A = 6316, // "The indigenous rams of Alterac are
                                       // vital to the functionality of our
                                       // cavalry!"
    NPC_TEXT_STABLES_DEFAULT_H = 6318, // "The indigenous wolves of Alterac are
                                       // vital to the functionality of our
                                       // cavalry!" (XXX: Made up, needs
                                       // checking on retail)

    NPC_TEXT_STABLES_TURNIN = 6317, // "Just a few more and our stables will be
                                    // overflowing with mounts. Release it into
                                    // my custody, soldier. I'll take it from
                                    // here."
};

static Creature* get_players_stables_turnin(Player* p)
{
    auto cl = GetCreatureListWithEntryInGrid(
        p, p->GetTeam() == ALLIANCE ? 10994 : 10995, 40);

    for (auto c : cl)
        if (auto ai = dynamic_cast<npc_empty_stables_petAI*>(c->AI()))
            if (ai->owner == p->GetObjectGuid())
                return c;

    return nullptr;
}

bool GossipHello_av_stables(Player* p, Creature* c)
{
    bool has_turnin = get_players_stables_turnin(p) !=
                      nullptr; // has a ram or wolf for turnin

    if (c->isQuestGiver())
        p->PrepareQuestMenu(c->GetObjectGuid());

    uint32 text_id;
    if (has_turnin)
    {
        text_id = NPC_TEXT_STABLES_TURNIN;
        p->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT,
            "With pleasure. These things stink!", GOSSIP_SENDER_MAIN,
            GOSSIP_ACTION_INFO_DEF + 10);
    }
    else
    {
        text_id = p->GetTeam() == ALLIANCE ? NPC_TEXT_STABLES_DEFAULT_A :
                                             NPC_TEXT_STABLES_DEFAULT_H;
    }

    if (ShowStable(c, p))
        p->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, "I'd like to stable my pet here.",
            GOSSIP_SENDER_SEC_STABLEMASTER, GOSSIP_ACTION_STABLE);

    p->SEND_GOSSIP_MENU(text_id, c->GetObjectGuid());

    return true;
}

bool GossipSelect_av_stables(Player* p, Creature* c, uint32, uint32 action)
{
    if (action == GOSSIP_ACTION_STABLE && ShowStable(c, p))
    {
        p->SEND_STABLELIST(c->GetObjectGuid());
        return true;
    }

    if (action != GOSSIP_ACTION_INFO_DEF + 10)
        return true;

    Creature* pet = get_players_stables_turnin(p);
    if (!pet)
        return true;

    pet->ForcedDespawn();
    p->CompleteQuest(p->GetTeam() == ALLIANCE ? 7027 : 7001);

    p->CLOSE_GOSSIP_MENU();

    return true;
}

enum
{
    NPC_TEXT_RIDER_ALLY = 6313,        // "Death to Frostwolf!"
    NPC_TEXT_RIDER_HORDE = 6314,       // "Death to Stormpike!"
    NPC_TEXT_RIDER_NEED_MOUNTS = 6800, // "We have the hides we need, but we
                                       // still need more mounts. Speak to our
                                       // stable master, and she'll tell you how
                                       // you can help." (XXX: made-up)
    NPC_TEXT_RIDER_READY = 6801,       // "We ride on your command!"
    NPC_TEXT_RIDER_LOW_REP = 6802,  // "We are ready to ride, but someone with
                                    // higher standing needs to issue the order,
                                    // or the men will never follow!" (XXX:
                                    // made-up)
    NPC_TEXT_RIDER_COOLDOWN = 6803, // "The cavalry has not been reassembled
                                    // since our last assault, we need more time
                                    // to prepare!" (XXX: made-up)
};

bool GossipHello_rider_commander(Player* p, Creature* c)
{
    if (c->isInCombat())
        return true; // XXX: Not sure this could happen, but if it could it'd be
                     // bad

    auto av = dynamic_cast<alterac_valley*>(c->GetInstanceData());
    if (!av)
        return true;

    // For gossiping mid-event
    if (av->cavalry_waiting(p->GetTeam()))
    {
        if (p->GetReputationRank(p->GetTeam() == ALLIANCE ?
                                     STORMPIKE_FACTION_ID :
                                     FROSTWOLF_FACTION_ID) < REP_HONORED)
        {
            p->SEND_GOSSIP_MENU(NPC_TEXT_RIDER_LOW_REP, c->GetObjectGuid());
            return true;
        }

        p->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, "Press onwards!",
            GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 20);
        p->SEND_GOSSIP_MENU(NPC_TEXT_RIDER_READY, c->GetObjectGuid());
        return true;
    }

    bool hides =
        av->GetData(p->GetTeam() == ALLIANCE ? ALLIANCE_COLLECTED_WOLF_HIDES :
                                               HORDE_COLLECTED_RAM_HIDES) >= 25;

    if (!hides)
    {
        if (c->isQuestGiver())
            p->PrepareQuestMenu(c->GetObjectGuid());
        p->SEND_GOSSIP_MENU(p->GetTeam() == ALLIANCE ? NPC_TEXT_RIDER_ALLY :
                                                       NPC_TEXT_RIDER_HORDE,
            c->GetObjectGuid());
        return true;
    }

    bool mounts =
        av->GetData(p->GetTeam() == ALLIANCE ? ALLIANCE_TAMED_ALTERAC_RAMS :
                                               HORDE_TAMED_FROSTWOLVES) >= 25;

    uint32 text_id;
    if (mounts)
    {
        // Requires honored to upgrade
        if (p->GetReputationRank(p->GetTeam() == ALLIANCE ?
                                     STORMPIKE_FACTION_ID :
                                     FROSTWOLF_FACTION_ID) >= REP_HONORED)
        {
            time_t cd = p->GetTeam() == ALLIANCE ? av->ally_cavalry_cd :
                                                   av->horde_cavalry_cd;
            if (WorldTimer::time_no_syscall() < cd)
            {
                text_id = NPC_TEXT_RIDER_COOLDOWN;
            }
            else
            {
                text_id = NPC_TEXT_RIDER_READY;
                p->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT,
                    "YAW! Er, to the front lines with you!", GOSSIP_SENDER_MAIN,
                    GOSSIP_ACTION_INFO_DEF + 10);
            }
        }
        else
        {
            text_id = NPC_TEXT_RIDER_LOW_REP;
        }
    }
    else
    {
        text_id = NPC_TEXT_RIDER_NEED_MOUNTS;
    }

    p->SEND_GOSSIP_MENU(text_id, c->GetObjectGuid());

    return true;
}

bool GossipSelect_rider_commander(Player* p, Creature* c, uint32, uint32 action)
{
    auto av = dynamic_cast<alterac_valley*>(c->GetInstanceData());
    if (!av)
        return true;

    // For gossiping mid-event
    if (av->cavalry_waiting(p->GetTeam()) &&
        action == GOSSIP_ACTION_INFO_DEF + 20)
    {
        av->resume_cavalry(p->GetTeam());
        p->CLOSE_GOSSIP_MENU();
        return true;
    }

    if (action != GOSSIP_ACTION_INFO_DEF + 10)
        return true;

    uint32 hides_data = p->GetTeam() == ALLIANCE ?
                            ALLIANCE_COLLECTED_WOLF_HIDES :
                            HORDE_COLLECTED_RAM_HIDES;
    uint32 tamed_data = p->GetTeam() == ALLIANCE ? ALLIANCE_TAMED_ALTERAC_RAMS :
                                                   HORDE_TAMED_FROSTWOLVES;

    if (av->GetData(hides_data) < 25 || av->GetData(tamed_data) < 25)
        return true;

    p->CLOSE_GOSSIP_MENU();

    av->SetData(hides_data, av->GetData(hides_data) - 25);
    av->SetData(tamed_data, av->GetData(tamed_data) - 25);

    av->do_cavalry(p->GetTeam(), c);

    return true;
}

enum
{
    NPC_TEXT_SLIDORE_NEED_RESCUE =
        6180, // "I heard their mocking laughter as my gryphon crashed and
              // burned, $c.$B$B"Slidore, you stink!" They shouted.$B$BNobody
              // talks to Slidore that way! Help me get out of here so that I
              // can show them who the real stinker is!"
    NPC_TEXT_VIPORE_NEED_RESCUE =
        6179, // "I can't believe it... I was doing a routine recon mission over
              // the central DMZ when all hell broke loose. I saw the Horde Wing
              // Commander, Mulverick, take out both Ichman and Slidore! Anger
              // took hold of me and I broke from my wing man and drove that
              // bastard Mulverick down. Unfortunately, he clipped my gryphon
              // before he crashed, forcing me down as well. I ended up captured
              // by these savages.$B$BI have to get back to base! Help!"
    NPC_TEXT_ICHMAN_NEED_RESCUE =
        6178, // "I was shot down by that reckless fool, Mulverick. I fear that
              // if I do not make it back to base, all will be lost! Mulverick
              // and his squad of War Riders must be stopped!"
    NPC_TEXT_GUSE_NEED_RESCUE = 6095, // "Good work, soldier. I need to get back
                                      // to base camp! Will you cover me?"
    NPC_TEXT_JEZTOR_NEED_RESCUE =
        6096, // "About damn time someone came. What took you so long? Ah,
              // nevermind. Cover me, soldier. I gotta make it back to base!"
    NPC_TEXT_MULVERICK_NEED_RESCUE = 6101, // "I need a few good wing men to get
                                           // me out of here safely and back to
                                           // base."

    NPC_TEXT_ASSAULT_NOT_READY = 6102, // "The aerial assault is not ready to be
                                       // launched yet." (XXX: made-up)
    NPC_TEXT_ASSAULT_READY = 6103,     // "The aerial assault is ready to be
    // deployed. I'm just waiting on orders from
    // a high-ranking Veteran." (XXX: made-up)
};

static std::pair<uint32, uint32> aerial_data(Creature* c)
{
    switch (c->GetEntry())
    {
    case NPC_ID_WC_SLIDORE:
        return std::pair<uint32, uint32>(ALLIANCE_SLIDORE_TURNINS, 90);
    case NPC_ID_WC_VIPORE:
        return std::pair<uint32, uint32>(ALLIANCE_VIPORE_TURNINS, 60);
    case NPC_ID_WC_ICHMAN:
        return std::pair<uint32, uint32>(ALLIANCE_ICHMAN_TURNINS, 30);
    case NPC_ID_WC_GUSE:
        return std::pair<uint32, uint32>(HORDE_GUSE_TURNINS, 90);
    case NPC_ID_WC_JEZTOR:
        return std::pair<uint32, uint32>(HORDE_JEZTOR_TURNINS, 60);
    case NPC_ID_WC_MULVERICK:
        return std::pair<uint32, uint32>(HORDE_MULVERICK_TURNINS, 30);
    }
    return std::make_pair(0, 0);
}

static bool aerial_ready(Creature* c)
{
    auto av = dynamic_cast<alterac_valley*>(c->GetMap()->GetInstanceData());
    if (!av)
        return false;

    auto p = aerial_data(c);
    return av->GetData(p.first) >= p.second;
}

bool GossipHello_wing_commander(Player* p, Creature* c)
{
    if (c->isQuestGiver())
    {
        p->PrepareQuestMenu(c->GetObjectGuid());

        if (aerial_ready(c))
        {
            if (p->GetReputationRank(p->GetTeam() == ALLIANCE ?
                                         STORMPIKE_FACTION_ID :
                                         FROSTWOLF_FACTION_ID) >= REP_HONORED)
            {
                // XXX: says are made-up
                p->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT,
                    "Give me a beacon. I'll light it up and your boys will "
                    "know where to strike.",
                    GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 20);
                p->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT,
                    "I want you to enter the fray yourself. Take your rider, "
                    "and patrol the area you were held captive.",
                    GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 30);
            }
            p->SEND_GOSSIP_MENU(NPC_TEXT_ASSAULT_READY, c->GetObjectGuid());
        }
        else
        {
            p->SEND_GOSSIP_MENU(NPC_TEXT_ASSAULT_NOT_READY, c->GetObjectGuid());
        }

        return true;
    }

    switch (c->GetEntry())
    {
    case NPC_ID_WC_SLIDORE:
        p->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT,
            "I got your back, Slidore, but to be honest, you do stink. Take a "
            "shower, man.",
            GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 10);
        p->SEND_GOSSIP_MENU(NPC_TEXT_SLIDORE_NEED_RESCUE, c->GetObjectGuid());
        break;
    case NPC_ID_WC_VIPORE:
        p->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT,
            "I got you covered, Vipore. Move out!", GOSSIP_SENDER_MAIN,
            GOSSIP_ACTION_INFO_DEF + 10);
        p->SEND_GOSSIP_MENU(NPC_TEXT_VIPORE_NEED_RESCUE, c->GetObjectGuid());
        break;
    case NPC_ID_WC_ICHMAN:
        p->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT,
            "I got your back, Ichman. GO GO GO!", GOSSIP_SENDER_MAIN,
            GOSSIP_ACTION_INFO_DEF + 10);
        p->SEND_GOSSIP_MENU(NPC_TEXT_ICHMAN_NEED_RESCUE, c->GetObjectGuid());
        break;
    case NPC_ID_WC_GUSE:
        p->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, "I got your back, Guse.",
            GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 10);
        p->SEND_GOSSIP_MENU(NPC_TEXT_GUSE_NEED_RESCUE, c->GetObjectGuid());
        break;
    case NPC_ID_WC_JEZTOR:
        p->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, "You can count on me, Jeztor.",
            GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 10);
        p->SEND_GOSSIP_MENU(NPC_TEXT_JEZTOR_NEED_RESCUE, c->GetObjectGuid());
        break;
    case NPC_ID_WC_MULVERICK:
        p->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, "I will be your wingman!",
            GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 10);
        p->SEND_GOSSIP_MENU(NPC_TEXT_MULVERICK_NEED_RESCUE, c->GetObjectGuid());
        break;
    }
    return true;
}

bool GossipSelect_wing_commander(Player* p, Creature* c, uint32, uint32 action)
{
    auto av = dynamic_cast<alterac_valley*>(c->GetMap()->GetInstanceData());
    if (!av)
        return true;

    bool pay = false;

    if (action == GOSSIP_ACTION_INFO_DEF + 10)
    {
        c->SetStandState(UNIT_STAND_STATE_STAND);
        c->SetActiveObjectState(true);
        c->RemoveFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP);
        c->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE);
        c->AI()->Notify(1);
    }
    else if (action == GOSSIP_ACTION_INFO_DEF + 20 && aerial_ready(c))
    {
        pay = true;
        uint32 item;
        switch (c->GetEntry())
        {
        case NPC_ID_WC_SLIDORE:
            item = 17507;
            break;
        case NPC_ID_WC_VIPORE:
            item = 17506;
            break;
        case NPC_ID_WC_ICHMAN:
            item = 17505;
            break;
        case NPC_ID_WC_GUSE:
            item = 17324;
            break;
        case NPC_ID_WC_JEZTOR:
            item = 17325;
            break;
        case NPC_ID_WC_MULVERICK:
            item = 17323;
            break;
        default:
            return true;
        }
        if (!p->add_item(item, 1))
        {
            p->CLOSE_GOSSIP_MENU();
            return true;
        }
    }
    else if (action == GOSSIP_ACTION_INFO_DEF + 30 && aerial_ready(c))
    {
        av->send_wing_commander(c);
        pay = true;
    }

    if (pay)
    {
        auto p = aerial_data(c);
        av->SetData(p.first, av->GetData(p.first) - p.second);
    }

    p->CLOSE_GOSSIP_MENU();

    return true;
}

enum
{
    NPC_ID_STORMPIKE_BOWMAN = 13358,
    NPC_ID_FROSTWOLF_BOWMAN = 13359,

    SPELL_BOWMAN_SHOOT = 22121,
};

struct npc_av_bowmanAI : public ScriptedAI
{
    npc_av_bowmanAI(Creature* c) : ScriptedAI(c)
    {
        team = c->GetEntry() == NPC_ID_STORMPIKE_BOWMAN ? ALLIANCE : HORDE;

        Reset();
    }

    Team team;
    uint32 target_timer;
    uint32 shoot_timer;

    void Reset() override
    {
        m_creature->SetFocusTarget(nullptr);
        SetCombatMovement(false);

        target_timer = 1000;
        shoot_timer = urand(2000, 4000);
    }

    Unit* get_target()
    {
        auto players = GetAllPlayersInObjectRangeCheckInCell(m_creature, 80.0f);

        std::vector<Player*> possible;
        for (auto p : players)
        {
            if (p->GetTeam() != team && p->isTargetableForAttack() &&
                IsVisible(p) && !m_creature->IsInEvadeMode() &&
                m_creature->IsWithinWmoLOSInMap(p))
                possible.push_back(p);
        }

        if (possible.empty())
            return nullptr;

        return possible[urand(0, possible.size() - 1)];
    }

    void UpdateAI(uint32 diff) override
    {
        m_creature->SelectHostileTarget();

        if (target_timer <= diff)
            target_timer = 0;
        else
            target_timer -= diff;

        if (m_creature->IsInEvadeMode())
            return;

        // update focus target
        if (target_timer == 0)
        {
            bool found = false;

            if (Unit* t = get_target())
            {
                AttackStart(t);
                m_creature->SetFocusTarget(t);
                found = true;
            }

            target_timer = found ? 30000 : 1000;
            return;
        }

        if (!m_creature->getVictim())
            return;

        if (!m_creature->IsWithinDistInMap(m_creature->getVictim(), 100.0f))
        {
            EnterEvadeMode();
            return;
        }

        if (shoot_timer <= diff)
        {
            if (!m_creature->CanReachWithMeleeAttack(m_creature->getVictim()) &&
                DoCastSpellIfCan(m_creature->getVictim(), SPELL_BOWMAN_SHOOT) ==
                    CAST_OK)
                shoot_timer = urand(2000, 4000);
        }
        else
            shoot_timer -= diff;

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_npc_av_bowman(Creature* c)
{
    return new npc_av_bowmanAI(c);
}

void AddSC_alterac_valley()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "npc_armor_scraps";
    pNewScript->pGossipHello = &GossipHello_armor_scraps;
    pNewScript->pGossipSelect = &GossipSelect_armor_scraps;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "npc_quartermaster";
    pNewScript->pGossipHello = &GossipHello_quartermaster;
    pNewScript->pGossipSelect = &GossipSelect_quartermaster;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "npc_ground_assault_launcher";
    pNewScript->GetAI = &GetAI_ground_assault_launcher;
    pNewScript->pGossipHello = &GossipHello_ground_assault_launcher;
    pNewScript->pGossipSelect = &GossipSelect_ground_assault_launcher;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "npc_empty_stables_pet";
    pNewScript->GetAI = &GetAI_empty_stables_pet;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "npc_av_stables";
    pNewScript->pGossipHello = &GossipHello_av_stables;
    pNewScript->pGossipSelect = &GossipSelect_av_stables;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "npc_rider_commander";
    pNewScript->pGossipHello = &GossipHello_rider_commander;
    pNewScript->pGossipSelect = &GossipSelect_rider_commander;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "npc_wing_commander";
    pNewScript->pGossipHello = &GossipHello_wing_commander;
    pNewScript->pGossipSelect = &GossipSelect_wing_commander;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "npc_av_bowman";
    pNewScript->GetAI = &GetAI_npc_av_bowman;
    pNewScript->RegisterSelf();

    extern void add_av_aerial_scripts();
    add_av_aerial_scripts();

    extern void add_av_boss_summon_scripts();
    add_av_boss_summon_scripts();
}
