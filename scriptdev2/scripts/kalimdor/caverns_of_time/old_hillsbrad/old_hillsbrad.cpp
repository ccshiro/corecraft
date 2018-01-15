/* Copyright (C) 2006 - 2012 ScriptDev2 <http://www.scriptdev2.com/>
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

/* ScriptData
SDName: Old_Hillsbrad
SD%Complete: 100
SDComment: Quest support: 10283, 10284.
SDCategory: Caverns of Time, Old Hillsbrad Foothills
EndScriptData */

/* ContentData
npc_erozion
npc_thrall_old_hillsbrad
npc_taretha
EndContentData */

#include "old_hillsbrad.h"
#include "escort_ai.h"

struct MANGOS_DLL_DECL npc_tarethaAI : public npc_escortAI
{
    npc_tarethaAI(Creature* pCreature);

    instance_old_hillsbrad* m_pInstance;
    ObjectGuid m_erozionGuid;
    uint32 m_uiErozionEventTimer;
    uint32 m_uiErozionPhase;

    void Reset() override {}
    void JustSummoned(Creature* pSummoned) override;
    void WaypointReached(uint32 uiPoint) override;
    void UpdateEscortAI(const uint32 uiDiff) override;
};

/*######
## npc_erozion
######*/

enum
{
    GOSSIP_ITEM_NEED_BOMBS = -3560001,
    GOSSIP_ITEM_TELEPORT = -3560009,

    TEXT_ID_DEFAULT = 9778,
    TEXT_ID_GOT_ITEM = 9515,
    TEXT_ID_END = 9611,
};

bool GossipHello_npc_erozion(Player* pPlayer, Creature* pCreature)
{
    if (pCreature->isQuestGiver())
        pPlayer->PrepareQuestMenu(pCreature->GetObjectGuid());

    if (ScriptedInstance* pInstance =
            (ScriptedInstance*)pCreature->GetInstanceData())
    {
        if (pInstance->GetData(TYPE_THRALL_PART5) == DONE)
        {
            // Offer teleport out of here
            pPlayer->ADD_GOSSIP_ITEM_ID(GOSSIP_ICON_CHAT, GOSSIP_ITEM_TELEPORT,
                GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 20);
            pPlayer->SEND_GOSSIP_MENU(TEXT_ID_END, pCreature->GetObjectGuid());
        }
        else
        {
            if (!pPlayer->HasItemCount(ITEM_ENTRY_INCENDIARY_BOMBS, 1))
                pPlayer->ADD_GOSSIP_ITEM_ID(GOSSIP_ICON_CHAT,
                    GOSSIP_ITEM_NEED_BOMBS, GOSSIP_SENDER_MAIN,
                    GOSSIP_ACTION_INFO_DEF + 10);

            pPlayer->SEND_GOSSIP_MENU(
                TEXT_ID_DEFAULT, pCreature->GetObjectGuid());
        }
    }

    return true;
}

bool GossipSelect_npc_erozion(
    Player* pPlayer, Creature* pCreature, uint32 /*uiSender*/, uint32 uiAction)
{
    if (uiAction == GOSSIP_ACTION_INFO_DEF + 10)
    {
        pPlayer->add_item(ITEM_ENTRY_INCENDIARY_BOMBS, 1);
        pPlayer->SEND_GOSSIP_MENU(TEXT_ID_GOT_ITEM, pCreature->GetObjectGuid());
    }

    if (uiAction == GOSSIP_ACTION_INFO_DEF + 20)
    {
        pPlayer->TeleportTo(1, -8379.3f, -4261.2f, -207.0f, 0.7f);
    }

    return true;
}

/*######
## npc_thrall_old_hillsbrad
######*/

enum
{
    SUMMONED_DESPAWN_TIME = 30 * 60 * 1000,

    // Thrall texts
    SAY_TH_START_EVENT_PART1 = -1560023,
    SAY_TH_ARMORY = -1560024,
    SAY_TH_ARMORY_GO = -1560061,

    // Skarloc RP
    SAY_TH_SKARLOC_MEET = -1560025,
    SAY_SKARLOC_MEET = -1560000,
    SAY_TH_SKARLOC_TAUNT = -1560026,

    SAY_TH_START_EVENT_PART2 = -1560027,
    SAY_TH_MOUNTS_UP = -1560028,
    EMOTE_TH_SCARE_MOUNT = -1560062,

    SAY_LOOK_BARNS_ENTER = -1560063,
    SAY_PROT_BARNS_RESPONSE = -1560064,
    EMOTE_TH_BARNS_HORSE = -1560065,
    SAY_TH_BARNS_RILE = -1560066,
    SAY_TH_BARNS_EXIT = -1560067,

    SAY_TH_CHURCH_START = -1560068,
    SAY_LOOK_CHURCH_AMBUSH = -1560069,
    // SAY_TH_CHURCH_END               = -1560029, Unused

    SAY_LOOK_INN_AMBUSH = -1560070,
    SAY_TH_MEET_TARETHA = -1560030,

    SAY_EPOCH_ENTER1 = -1560013,
    SAY_EPOCH_ENTER2 = -1560014,
    SAY_EPOCH_ENTER3 = -1560015,

    SAY_TH_EPOCH_WONDER = -1560031,
    SAY_TH_EPOCH_KILL_TARETHA = -1560032,

    SAY_TH_RANDOM_LOW_HP1 = -1560034,
    SAY_TH_RANDOM_LOW_HP2 = -1560035,

    SAY_TH_RANDOM_DIE1 = -1560036,
    SAY_TH_RANDOM_DIE2 = -1560037,

    SAY_TH_RANDOM_AGGRO1 = -1560038,
    SAY_TH_RANDOM_AGGRO2 = -1560039,
    SAY_TH_RANDOM_AGGRO3 = -1560040,
    SAY_TH_RANDOM_AGGRO4 = -1560041,

    SAY_TH_RANDOM_KILL1 = -1560042,
    SAY_TH_RANDOM_KILL2 = -1560043,
    SAY_TH_RANDOM_KILL3 = -1560044,

    SAY_TH_KILL_ARMORER = -1560050,

    SAY_TH_LEAVE_COMBAT1 = -1560045,
    SAY_TH_LEAVE_COMBAT2 = -1560046,
    SAY_TH_LEAVE_COMBAT3 = -1560047,

    // Taretha texts
    SAY_TA_ESCAPED = -1560049,

    // end event texts
    SAY_TA_FREE = -1560048,
    SAY_TR_GLAD_SAFE = -1560054,
    SAY_TA_NEVER_MET = -1560055,
    SAY_TR_THEN_WHO = -1560056,
    SAY_PRE_WIPE = -1560057,
    SAY_WIPE_MEMORY = -1560051,
    SAY_AFTER_WIPE = -1560058,
    SAY_ABOUT_TARETHA = -1560052,
    SAY_TH_EVENT_COMPLETE = -1560033,
    SAY_TA_FAREWELL = -1560053,

    // Other mobs' says
    SAY_ARM_SPOT_THRALL = -1560060,

    // Misc for Thrall
    SPELL_STRIKE = 14516,
    SPELL_SHIELD_BLOCK = 12169,
    SPELL_SUMMON_EROZION_IMAGE = 33954, // if thrall dies during escort?
    SPELL_MEMORY_WIPE = 33336,

    EQUIP_ID_WEAPON = 927,
    EQUIP_ID_SHIELD = 15621,
    MODEL_THRALL_UNEQUIPPED = 17292,
    MODEL_THRALL_EQUIPPED = 18165,

    // Spells
    SPELL_PUNCH_ARMORER = 40450,
    SPELL_SHADOW_SPIKE = 33125,

    // misc creature entries
    NPC_SKARLOC = 17862,

    NPC_RIFLE = 17820,
    NPC_WARDEN = 17833,
    NPC_VETERAN = 17860,
    NPC_SENTRY = 17819,
    NPC_MAGE = 18934,

    NPC_BARN_GUARDSMAN = 18092,
    NPC_BARN_PROTECTOR = 18093,
    NPC_BARN_LOOKOUT = 18094,

    NPC_CHURCH_GUARDSMAN = 18092,
    NPC_CHURCH_PROTECTOR = 18093,
    NPC_CHURCH_LOOKOUT = 18094,

    NPC_INN_GUARDSMAN = 18092,
    NPC_INN_PROTECTOR = 18093,
    NPC_INN_LOOKOUT = 18094,

    MODEL_SKARLOC_MOUNT = 18223,
    NPC_EROZION = 18723,
    NPC_THRALL_QUEST_TRIGGER = 20156,

    // gossip
    TEXT_ID_START = 9568,
    GOSSIP_ITEM_START = -3560007,
    TEXT_ID_SKARLOC1 = 9614, // I'm glad Taretha is alive. We now must find a
                             // way to free her...
    GOSSIP_ITEM_SKARLOC1 = -3560002, // "Taretha cannot see you, Thrall."
    TEXT_ID_SKARLOC2 = 9579, // What do you mean by this? Is Taretha in danger?
    GOSSIP_ITEM_SKARLOC2 =
        -3560003, // "The situation is rather complicated, Thrall. It would be
                  // best for you to head into the mountains now, before more of
                  // Blackmoore's men show up. We'll make sure Taretha is safe."
    TEXT_ID_SKARLOC3 = 9580,
    GOSSIP_ITEM_SKARLOC3 = -3560008,

    TEXT_ID_TARREN = 9597,         // tarren mill is beyond these trees
    GOSSIP_ITEM_TARREN = -3560004, // "We're ready, Thrall."

    TEXT_ID_INN = 9600, // Thrall's gossip in the inn

    TEXT_ID_COMPLETE = 9578, // Thank you friends, I owe my freedom to you.
                             // Where is Taretha? I hoped to see her
};

enum ThrallRPEvent
{
    TH_RP_EVENT_SKARLOC = 1,
};

const float SPEED_RUN = 1.0f;
const float SPEED_MOUNT = 1.6f;

struct MANGOS_DLL_DECL npc_thrall_old_hillsbradAI : public npc_escortAI
{
    npc_thrall_old_hillsbradAI(Creature* pCreature) : npc_escortAI(pCreature)
    {
        m_pInstance = (instance_old_hillsbrad*)pCreature->GetInstanceData();
        m_bHadMount = false;
        m_uiResumeEscortTimer = 0;
        m_skarlocGrpId = 0;
        m_uiRPEvent = 0;
        m_uiRPPhase = 0;
        m_uiNextRPPhaseTimer = 0;
        pCreature->SetActiveObjectState(true); // required for proper relocation
        m_bHasIncreasedDeaths = false;
        Reset();
    }

    instance_old_hillsbrad* m_pInstance;

    std::vector<ObjectGuid> m_spawnedStuff;
    bool m_bIsLowHp;
    bool m_bHadMount;
    bool m_bHasIncreasedDeaths;
    uint32 m_uiResumeEscortTimer;
    int32 m_skarlocGrpId;
    uint32 m_uiRPEvent;
    uint32 m_uiRPPhase;
    uint32 m_uiNextRPPhaseTimer;
    uint32 m_uiStrikeTimer;
    uint32 m_uiBlockTimer;

    std::vector<ObjectGuid> m_barnMobs;

    void Reset() override
    {
        m_bIsLowHp = false;
        m_uiStrikeTimer = urand(4000, 8000);
        m_uiBlockTimer = urand(5000, 10000);

        if (m_bHadMount)
            DoMount();

        if (!HasEscortState(STATE_ESCORT_ESCORTING))
        {
            DoUnmount();
            m_bHadMount = false;
            SetEquipmentSlots(true);
            m_creature->SetDisplayId(MODEL_THRALL_UNEQUIPPED);
        }
    }

    void DamageTaken(Unit* /*pDealer*/, uint32& /*uiDamage*/) override
    {
        if (!m_uiBlockTimer)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_SHIELD_BLOCK) == CAST_OK)
                m_uiBlockTimer = urand(10000, 20000);
        }
    }

    void EnterEvadeMode(bool by_group = false) override
    {
        if (HasEscortState(STATE_ESCORT_ESCORTING))
        {
            switch (urand(0, 2))
            {
            case 0:
                DoScriptText(SAY_TH_LEAVE_COMBAT1, m_creature);
                break;
            case 1:
                DoScriptText(SAY_TH_LEAVE_COMBAT2, m_creature);
                break;
            case 2:
                DoScriptText(SAY_TH_LEAVE_COMBAT3, m_creature);
                break;
            }
        }

        npc_escortAI::EnterEvadeMode(by_group);
    }

    void MovementInform(movement::gen gen_type, uint32 uiData) override
    {
        if (!m_pInstance)
            return;

        if (gen_type == movement::gen::point)
        {
            switch (uiData)
            {
            // Custom, non-wp points
            case 10000: // Punch Armorer
                if (Creature* armorer =
                        m_pInstance->GetSingleCreatureFromStorage(NPC_ARMORER))
                {
                    DoCastSpellIfCan(armorer, SPELL_PUNCH_ARMORER);
                    armorer->queue_action_ticks(2, [armorer]()
                        {
                            armorer->Kill(armorer);
                        });
                }
                DoScriptText(SAY_TH_KILL_ARMORER, m_creature);
                m_uiResumeEscortTimer = 3000;
                break;
            default:
                npc_escortAI::MovementInform(gen_type, uiData);
                break;
            }
        }
    }

    void WaypointReached(uint32 uiPoint) override
    {
        if (!m_pInstance)
            return;

        switch (uiPoint)
        {
        case 0:
        case 1:
            SetRun(true);
            break;
        case 8:
            SetRun(false);
            if (Creature* armorer =
                    m_pInstance->GetSingleCreatureFromStorage(NPC_ARMORER))
            {
                armorer->movement_gens.push(
                    new movement::StoppedMovementGenerator());
                armorer->HandleEmote(EMOTE_ONESHOT_NONE);
                armorer->SetFacingToObject(m_creature);
                DoScriptText(SAY_ARM_SPOT_THRALL, armorer);
                SetEscortPaused(true);
                auto pos = armorer->GetPoint(
                    armorer->GetAngle(m_creature) - armorer->GetO(), 3.0f,
                    true);
                m_creature->movement_gens.push(
                    new movement::PointMovementGenerator(
                        10000, pos.x, pos.y, pos.z, true, false));
            }
            break;
        case 9:
            DoScriptText(SAY_TH_ARMORY, m_creature);
            SetEquipmentSlots(
                false, EQUIP_ID_WEAPON, EQUIP_ID_SHIELD, EQUIP_NO_CHANGE);
            break;
        case 10:
            m_creature->SetDisplayId(MODEL_THRALL_EQUIPPED);
            break;
        case 11:
            SetRun(true);
            DoScriptText(SAY_TH_ARMORY_GO, m_creature);
            break;
        case 15:
            m_creature->SummonCreature(NPC_VETERAN, 2202.6f, 135.3f, 87.9f,
                5.8f, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT,
                SUMMONED_DESPAWN_TIME);
            m_creature->SummonCreature(NPC_SENTRY, 2200.5f, 130.9f, 87.9f, 5.8f,
                TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, SUMMONED_DESPAWN_TIME);
            m_creature->SummonCreature(NPC_SENTRY, 2198.5f, 132.0f, 87.9f, 5.8f,
                TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, SUMMONED_DESPAWN_TIME);
            m_creature->SummonCreature(NPC_MAGE, 2200.8f, 136.1f, 87.9f, 5.8f,
                TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, SUMMONED_DESPAWN_TIME);
            break;
        case 18:
            m_creature->SummonCreature(NPC_VETERAN, 2145.9f, 120.4f, 76.0f,
                0.4f, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT,
                SUMMONED_DESPAWN_TIME);
            m_creature->SummonCreature(NPC_SENTRY, 2144.7f, 122.9f, 76.0f, 0.4f,
                TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, SUMMONED_DESPAWN_TIME);
            m_creature->SummonCreature(NPC_SENTRY, 2143.7f, 124.8f, 76.0f, 0.4f,
                TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, SUMMONED_DESPAWN_TIME);
            m_creature->SummonCreature(NPC_MAGE, 2142.6f, 127.4f, 76.0f, 0.4f,
                TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, SUMMONED_DESPAWN_TIME);
            break;
        case 21:
            m_creature->SummonCreature(NPC_WARDEN, 2138.7f, 168.6f, 66.2f, 2.5f,
                TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, SUMMONED_DESPAWN_TIME);
            m_creature->SummonCreature(NPC_MAGE, 2139.9f, 169.0f, 66.2f, 2.5f,
                TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, SUMMONED_DESPAWN_TIME);
            m_creature->SummonCreature(NPC_VETERAN, 2142.0f, 172.1f, 66.2f,
                2.5f, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT,
                SUMMONED_DESPAWN_TIME);
            m_creature->SummonCreature(NPC_VETERAN, 2141.9f, 174.0f, 66.2f,
                2.5f, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT,
                SUMMONED_DESPAWN_TIME);
            break;
        case 23:
            m_creature->SummonCreature(NPC_VETERAN, 2108.8f, 189.8f, 66.2f,
                2.5f, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT,
                SUMMONED_DESPAWN_TIME);
            m_creature->SummonCreature(NPC_MAGE, 2108.3f, 191.9f, 66.2f, 2.5f,
                TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, SUMMONED_DESPAWN_TIME);
            m_creature->SummonCreature(NPC_MAGE, 2110.4f, 194.7f, 66.2f, 2.5f,
                TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, SUMMONED_DESPAWN_TIME);
            m_creature->SummonCreature(NPC_VETERAN, 2112.8f, 195.7f, 66.2f,
                2.5f, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT,
                SUMMONED_DESPAWN_TIME);
            break;
        case 30:
            // We create a creature group, and Spawn boss and his adds.
            m_skarlocGrpId =
                m_creature->GetMap()->GetCreatureGroupMgr().CreateNewGroup(
                    "Skarloc", true);
            if (CreatureGroup* pGrp =
                    m_creature->GetMap()->GetCreatureGroupMgr().GetGroup(
                        m_skarlocGrpId))
            {
                pGrp->AddFlag(CREATURE_GROUP_FLAG_GROUP_MOVEMENT);
                pGrp->AddFlag(CREATURE_GROUP_FLAG_LEADER_RESPAWN_ALL);

                std::vector<Creature*> creatures;
                if (Creature* s =
                        m_creature->SummonCreature(NPC_SKARLOC, 2001.6f, 285.0f,
                            66.2f, 5.7f, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT,
                            SUMMONED_DESPAWN_TIME))
                {
                    pGrp->AddMember(s, false);
                    m_creature->GetMap()
                        ->GetCreatureGroupMgr()
                        .GetMovementMgr()
                        .SetNewFormation(m_skarlocGrpId, s);
                    pGrp->SetLeader(s, false);
                    s->movement_gens.remove_all(movement::gen::idle);
                    s->Mount(MODEL_SKARLOC_MOUNT);
                    creatures.push_back(s);
                    s->SetFlag(UNIT_FIELD_FLAGS,
                        UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_PASSIVE);
                }
                if (Creature* v =
                        m_creature->SummonCreature(NPC_VETERAN, 1999.8f, 282.4f,
                            66.2f, 5.7f, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT,
                            SUMMONED_DESPAWN_TIME))
                {
                    pGrp->AddMember(v, false);
                    m_creature->GetMap()
                        ->GetCreatureGroupMgr()
                        .GetMovementMgr()
                        .SetNewFormation(m_skarlocGrpId, v);
                    creatures.push_back(v);
                    v->movement_gens.remove_all(movement::gen::idle);
                    v->SetFlag(UNIT_FIELD_FLAGS,
                        UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_PASSIVE);
                }
                if (Creature* w =
                        m_creature->SummonCreature(NPC_WARDEN, 2004.0f, 287.3f,
                            66.2f, 5.7f, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT,
                            SUMMONED_DESPAWN_TIME))
                {
                    pGrp->AddMember(w, false);
                    m_creature->GetMap()
                        ->GetCreatureGroupMgr()
                        .GetMovementMgr()
                        .SetNewFormation(m_skarlocGrpId, w);
                    creatures.push_back(w);
                    w->movement_gens.remove_all(movement::gen::idle);
                    w->SetFlag(UNIT_FIELD_FLAGS,
                        UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_PASSIVE);
                }

                // Add waypoints
                m_creature->GetMap()
                    ->GetCreatureGroupMgr()
                    .GetMovementMgr()
                    .AddWaypoint(
                        m_skarlocGrpId, DynamicWaypoint(2001.6f, 285.0f, 66.2f,
                                            100.0f, 0, true));
                m_creature->GetMap()
                    ->GetCreatureGroupMgr()
                    .GetMovementMgr()
                    .AddWaypoint(
                        m_skarlocGrpId, DynamicWaypoint(2022.7f, 275.8f, 64.5f,
                                            100.0f, 0, true));
                m_creature->GetMap()
                    ->GetCreatureGroupMgr()
                    .GetMovementMgr()
                    .AddWaypoint(
                        m_skarlocGrpId, DynamicWaypoint(2050.1f, 251.7f, 62.8f,
                                            100.0f, 0, true));
                m_creature->GetMap()
                    ->GetCreatureGroupMgr()
                    .GetMovementMgr()
                    .AddWaypoint(
                        m_skarlocGrpId, DynamicWaypoint(2058.5, 237.0f, 63.8f));

                m_creature->GetMap()
                    ->GetCreatureGroupMgr()
                    .GetMovementMgr()
                    .StartMovement(m_skarlocGrpId, creatures);
            }
            SetEscortPaused(true);
            break;
        case 31: // Respawn point
            m_creature->SetFacingTo(0.5f);
            break;
        case 32:
            m_creature->SetOrientation(5.3f); // We cannot use facing, the
                                              // launching of a move gen will
                                              // bug here
            DoMount();
            if (Creature* horse = m_pInstance->GetSingleCreatureFromStorage(
                    NPC_SKARLOC_MOUNT))
                horse->ForcedDespawn();
            break;
        case 33:
            m_creature->HandleEmote(EMOTE_ONESHOT_EXCLAMATION);
            DoScriptText(SAY_TH_MOUNTS_UP, m_creature);
            SetRun(true);
            m_creature->SetWalk(false); // For some reason after respawn
                                        // m_bIsRunning is set to true at some
                                        // points in here, despite the mob
                                        // walking...
            break;
        case 62:
            DoUnmount();
            m_creature->SetFacingTo(6.0f);
            m_creature->SummonCreature(NPC_SKARLOC_MOUNT, 2487.9f, 625.2f,
                58.1f, 1.2f, TEMPSUMMON_MANUAL_DESPAWN, 0);
            m_bHadMount = false;
            SetRun(false);
            break;
        case 63:
            DoScriptText(EMOTE_TH_SCARE_MOUNT, m_creature);
            if (Creature* horse = m_pInstance->GetSingleCreatureFromStorage(
                    NPC_SKARLOC_MOUNT))
            {
                std::vector<DynamicWaypoint> wps;
                wps.push_back(
                    DynamicWaypoint(2489.3f, 607.6f, 55.9f, 100.0f, 0, true));
                wps.push_back(
                    DynamicWaypoint(2500.0f, 589.4f, 55.5f, 100.0f, 0, true));
                wps.push_back(
                    DynamicWaypoint(2496.3f, 571.2f, 54.6f, 100.0f, 0, true));
                wps.push_back(
                    DynamicWaypoint(2498.5f, 566.9f, 51.8f, 100.0f, 0, true));
                wps.push_back(DynamicWaypoint(
                    2503.0f, 558.5f, 45.8f, 100.0f, 20000, true));
                horse->movement_gens.remove_all(movement::gen::idle);
                horse->movement_gens.push(
                    new movement::DynamicWaypointMovementGenerator(wps, false));
                horse->ForcedDespawn(14000);
            }
            SetEscortPaused(true);
            m_creature->SetFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP);
            m_pInstance->SetData(TYPE_THRALL_PART2, DONE);
            SetRun(true);
            break;
        case 64: // Respawn point
            SetRun(true);
            break;
        case 65:
            SetRun(true);
            break;
        case 67:
            SetRun(false);
            break;
        case 70:
            m_barnMobs.clear();
            if (Creature* c = m_creature->SummonCreature(NPC_BARN_PROTECTOR,
                    2499.6f, 696.3f, 55.50f, 3.9f,
                    TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT,
                    SUMMONED_DESPAWN_TIME))
            {
                m_barnMobs.push_back(c->GetObjectGuid());
                c->SetFlag(UNIT_FIELD_FLAGS,
                    UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_PASSIVE);
            }
            if (Creature* c = m_creature->SummonCreature(NPC_BARN_GUARDSMAN,
                    2501.7f, 698.0f, 55.50f, 3.9f,
                    TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT,
                    SUMMONED_DESPAWN_TIME))
            {
                m_barnMobs.push_back(c->GetObjectGuid());
                c->SetFlag(UNIT_FIELD_FLAGS,
                    UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_PASSIVE);
            }
            if (Creature* c = m_creature->SummonCreature(NPC_BARN_LOOKOUT,
                    2497.9f, 697.9f, 55.50f, 3.9f,
                    TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT,
                    SUMMONED_DESPAWN_TIME))
            {
                DoScriptText(SAY_LOOK_BARNS_ENTER, c);
                m_barnMobs.push_back(c->GetObjectGuid());
                c->SetFlag(UNIT_FIELD_FLAGS,
                    UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_PASSIVE);
            }
            if (Creature* c = m_creature->SummonCreature(NPC_BARN_LOOKOUT,
                    2499.9f, 699.8f, 55.50f, 3.9f,
                    TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT,
                    SUMMONED_DESPAWN_TIME))
            {
                m_barnMobs.push_back(c->GetObjectGuid());
                c->SetFlag(UNIT_FIELD_FLAGS,
                    UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_PASSIVE);
            }
            break;
        case 71:
            if (m_barnMobs.size() > 0)
                if (Creature* prot =
                        m_creature->GetMap()->GetCreature(m_barnMobs[0]))
                    DoScriptText(SAY_PROT_BARNS_RESPONSE, prot);
            break;
        case 72:
            DoScriptText(EMOTE_TH_BARNS_HORSE, m_creature);
            break;
        case 73:
            if (m_barnMobs.size() > 2)
                if (Creature* look =
                        m_creature->GetMap()->GetCreature(m_barnMobs[2]))
                    DoScriptText(SAY_TH_BARNS_RILE, look);
            for (auto& elem : m_barnMobs)
                if (Creature* m = m_creature->GetMap()->GetCreature(elem))
                {
                    m->movement_gens.push(new movement::FollowMovementGenerator(
                        m_creature, 0, 0));
                    m->RemoveFlag(UNIT_FIELD_FLAGS,
                        UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_PASSIVE);
                }
            m_barnMobs.clear();
            break;
        case 80:
            DoScriptText(SAY_TH_BARNS_EXIT, m_creature);
            SetRun(true);
            break;
        case 86:
            SetRun(false);
            break;
        case 88:
            DoScriptText(SAY_TH_CHURCH_START, m_creature);
            break;
        case 89:
            m_creature->SummonCreature(NPC_CHURCH_PROTECTOR, 2628.0f, 663.4f,
                54.3f, 4.4f, TEMPSUMMON_TIMED_OR_DEAD_DESPAWN,
                SUMMONED_DESPAWN_TIME);
            if (Creature* look = m_creature->SummonCreature(NPC_CHURCH_LOOKOUT,
                    2629.9f, 663.2f, 54.3f, 4.4f,
                    TEMPSUMMON_TIMED_OR_DEAD_DESPAWN, SUMMONED_DESPAWN_TIME))
                DoScriptText(SAY_LOOK_CHURCH_AMBUSH, look);
            m_creature->SummonCreature(NPC_CHURCH_GUARDSMAN, 2631.9f, 662.5f,
                54.3f, 4.4f, TEMPSUMMON_TIMED_OR_DEAD_DESPAWN,
                SUMMONED_DESPAWN_TIME);
            m_creature->SummonCreature(NPC_CHURCH_LOOKOUT, 2633.3f, 662.0f,
                54.3f, 4.4f, TEMPSUMMON_TIMED_OR_DEAD_DESPAWN,
                SUMMONED_DESPAWN_TIME);
            SetRun(true);
            break;
        case 93:
            if (Creature* pTaretha = m_pInstance->GetTaretha())
                pTaretha->CastSpell(pTaretha, SPELL_SHADOW_PRISON, true);
            SetRun(false);
            break;
        case 98:
            m_creature->SummonCreature(NPC_INN_PROTECTOR, 2654.5f, 665.7f,
                61.9f, 1.9f, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT,
                SUMMONED_DESPAWN_TIME);
            m_creature->SummonCreature(NPC_INN_GUARDSMAN, 2648.8f, 665.0f,
                61.9f, 1.2f, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT,
                SUMMONED_DESPAWN_TIME);
            if (Creature* look =
                    m_creature->SummonCreature(NPC_INN_LOOKOUT, 2654.3f, 657.9f,
                        61.9f, 1.7f, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT,
                        SUMMONED_DESPAWN_TIME))
                DoScriptText(SAY_LOOK_INN_AMBUSH, look);
            m_creature->SummonCreature(NPC_INN_LOOKOUT, 2648.1f, 659.1f, 61.9f,
                1.3f, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT,
                SUMMONED_DESPAWN_TIME);
            break;
        case 101:
            if (Creature* pTaretha = m_pInstance->GetTaretha())
                DoScriptText(SAY_TA_ESCAPED, pTaretha, m_creature);
            break;
        case 102:
            DoScriptText(SAY_TH_MEET_TARETHA, m_creature);
            m_pInstance->SetData(TYPE_THRALL_PART3, DONE);
            m_creature->SetFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP);
            if (Creature* pTaretha = m_pInstance->GetTaretha())
                pTaretha->SetFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP);
            SetEscortPaused(true);
            break;
        case 103: // Respawn point
            if (Creature* pTaretha = m_pInstance->GetTaretha())
                pTaretha->RemoveFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP);
            DoScriptText(SAY_TH_EPOCH_WONDER, m_creature);
            break;
        case 104:
            if (Creature* pTaretha = m_pInstance->GetTaretha())
            {
                pTaretha->CastSpell(pTaretha, SPELL_SHADOW_SPIKE, false);
                pTaretha->SetStandState(UNIT_STAND_STATE_DEAD);
            }
            if (Creature* pEpoch = m_pInstance->GetEpoch())
                DoScriptText(SAY_EPOCH_ENTER2, pEpoch);
            break;
        case 105:
            DoScriptText(SAY_TH_EPOCH_KILL_TARETHA, m_creature);
            SetRun(true);
            break;
        case 112:
            if (Creature* pEpoch = m_pInstance->GetEpoch())
                DoScriptText(SAY_EPOCH_ENTER3, pEpoch);
            m_creature->SetFacingTo(1.5f);
            m_creature->movement_gens.push(
                new movement::IdleMovementGenerator(m_creature->GetX(),
                    m_creature->GetY(), m_creature->GetZ(), 1.5f));
            break;
        case 113:
            SetEscortPaused(true);

            if (Creature* pEpoch = m_pInstance->GetEpoch())
            {
                // Let Epoch know that it's time to fight by triggering a
                // movement inform; pretty shitty way but inter-AI communication
                // doesn't really exist
                pEpoch->movement_gens.push(
                    new movement::PointMovementGenerator(10000, pEpoch->GetX(),
                        pEpoch->GetY(), pEpoch->GetZ(), false, false));
            }

            // Instance Script will start our path again when Epoch is dead
            break;
        case 114:
        {
            m_creature->movement_gens.remove_all(movement::gen::idle);

            // trigger taretha to run down outside
            if (Creature* pTaretha = m_pInstance->GetTaretha())
            {
                if (npc_tarethaAI* pTarethaAI =
                        dynamic_cast<npc_tarethaAI*>(pTaretha->AI()))
                    pTarethaAI->Start(true, GetPlayerForEscort());
                pTaretha->remove_auras(SPELL_SHADOW_PRISON);
                pTaretha->SetStandState(UNIT_STAND_STATE_STAND);
            }

            // kill credit creature for quest
            Map::PlayerList const& lPlayerList =
                m_pInstance->instance->GetPlayers();

            if (!lPlayerList.isEmpty())
            {
                for (const auto& elem : lPlayerList)
                {
                    if (Player* pPlayer = elem.getSource())
                        pPlayer->KilledMonsterCredit(NPC_THRALL_QUEST_TRIGGER,
                            m_creature->GetObjectGuid());
                }
            }

            // a lot will happen here, thrall and taretha talk, erozion appear
            // at spot to explain
            // handled by taretha script
            SetEscortPaused(true);
            break;
        }
        case 116:
            m_creature->SetActiveObjectState(false);
            break;
        }
    }

    void WaypointStart(uint32 /*uiPointId*/) override
    {
        if (!m_pInstance)
            return;
    }

    void StartWP()
    {
        m_creature->RemoveFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP);
        SetEscortPaused(false);
    }

    void DoMount()
    {
        m_creature->Mount(MODEL_SKARLOC_MOUNT);
        m_creature->SetSpeedRate(MOVE_RUN, SPEED_MOUNT);
    }

    void DoUnmount()
    {
        m_creature->Unmount();
        m_creature->SetSpeedRate(MOVE_RUN, SPEED_RUN);
    }

    void Aggro(Unit* /*pWho*/) override
    {
        switch (urand(0, 3))
        {
        case 0:
            DoScriptText(SAY_TH_RANDOM_AGGRO1, m_creature);
            break;
        case 1:
            DoScriptText(SAY_TH_RANDOM_AGGRO2, m_creature);
            break;
        case 2:
            DoScriptText(SAY_TH_RANDOM_AGGRO3, m_creature);
            break;
        case 3:
            DoScriptText(SAY_TH_RANDOM_AGGRO4, m_creature);
            break;
        }

        if (m_creature->IsMounted())
        {
            DoUnmount();
            m_bHadMount = true;
        }
    }

    void JustSummoned(Creature* pSummoned) override
    {
        m_spawnedStuff.push_back(pSummoned->GetObjectGuid());

        switch (pSummoned->GetEntry())
        {
        case NPC_EPOCH:
            DoScriptText(SAY_EPOCH_ENTER1, pSummoned);
            pSummoned->SetFlag(
                UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_PASSIVE);
            break;
        default:
            break;
        }
    }

    void KilledUnit(Unit* pVictim) override
    {
        if (pVictim->GetEntry() == NPC_ARMORER)
            return;

        switch (urand(0, 2))
        {
        case 0:
            DoScriptText(SAY_TH_RANDOM_KILL1, m_creature);
            break;
        case 1:
            DoScriptText(SAY_TH_RANDOM_KILL2, m_creature);
            break;
        case 2:
            DoScriptText(SAY_TH_RANDOM_KILL3, m_creature);
            break;
        }

        // Death called from instance script (or if he has the killing blow of
        // course)
        // Thrall should normally always be the one killing, but no support for
        // this yet.
        if (pVictim->GetEntry() == NPC_EPOCH)
            SetEscortPaused(false);
    }

    void JustDied(Unit* /*pKiller*/) override
    {
        DoScriptText(
            urand(0, 1) ? SAY_TH_RANDOM_DIE1 : SAY_TH_RANDOM_DIE2, m_creature);

        // Despawn everything we've spawned
        for (auto& elem : m_spawnedStuff)
            if (Creature* c = m_creature->GetMap()->GetCreature(elem))
                c->ForcedDespawn();
        m_spawnedStuff.clear();

        m_pInstance->SetData(
            TYPE_THRALL_DEATHS, m_pInstance->GetData(TYPE_THRALL_DEATHS) + 1);
        m_bHasIncreasedDeaths = true; // Set to false in just respawned

        // You only get 3 tries pre 2.2 (upped to 20 in 2.2):
        if (m_pInstance->GetData(TYPE_THRALL_DEATHS) > 3)
        {
            m_creature->SetRespawnDelay(48 * HOUR);
            m_creature->SetRespawnTime(48 * HOUR);
            m_pInstance->SetData(TYPE_THRALL_EVENT, FAIL);
        }
        else
        {
            m_creature->SummonCreature(NPC_IMAGE_OF_EROZION, m_creature->GetX(),
                m_creature->GetY(), m_creature->GetZ(), m_creature->GetO(),
                TEMPSUMMON_MANUAL_DESPAWN, 0);
        }
    }

    void CorpseRemoved(uint32& uiRespawnDelay) override
    {
        // if we're done, just set some high so he never really respawn
        if (m_pInstance && (m_pInstance->GetData(TYPE_THRALL_EVENT) == DONE ||
                               m_pInstance->GetData(TYPE_THRALL_DEATHS) > 3))
            uiRespawnDelay = 48 * HOUR;
    }

    void JustRespawned() override
    {
        if (!m_pInstance)
            return;

        // If escort respawns us we don't get our death count increased, so we
        // need to do that here
        if (!m_bHasIncreasedDeaths)
            m_pInstance->SetData(TYPE_THRALL_DEATHS,
                m_pInstance->GetData(TYPE_THRALL_DEATHS) + 1);
        m_bHasIncreasedDeaths = false;

        if (m_pInstance->GetData(TYPE_THRALL_DEATHS) > 3 ||
            m_pInstance->GetData(TYPE_THRALL_EVENT) == DONE)
        {
            m_creature->ForcedDespawn();
            return;
        }

        Reset();

        m_creature->movement_gens.remove_all(movement::gen::idle);

        m_uiResumeEscortTimer = 0;
        m_skarlocGrpId = 0;

        if (m_pInstance->GetData(TYPE_THRALL_EVENT) == IN_PROGRESS)
        {
            SetEscortPaused(true);

            m_bHadMount = false;
            DoUnmount();

            m_creature->RemoveFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_QUESTGIVER);

            // check current states before fail and set specific for the part
            if (m_pInstance->GetData(TYPE_THRALL_PART1) != DONE)
            {
                SetCurrentWaypoint(0); // basement

                SetEquipmentSlots(true);
                m_creature->SetDisplayId(MODEL_THRALL_UNEQUIPPED);

                m_creature->SetFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP);

                m_creature->NearTeleportTo(2231.9f, 120.0f, 82.3f, 4.2f);
            }
            else if (m_pInstance->GetData(TYPE_THRALL_PART2) != DONE)
            {
                SetCurrentWaypoint(31); // post-skarloc
                m_creature->SetFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP);
                m_creature->NearTeleportTo(2049.1f, 250.8f, 62.9f, 2.4f);
            }
            else if (m_pInstance->GetData(TYPE_THRALL_PART3) != DONE)
            {
                SetCurrentWaypoint(64); // barn
                m_creature->SetFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP);
                m_creature->NearTeleportTo(2485.6f, 626.1f, 58.0f, 1.4f);
            }
            else if (m_pInstance->GetData(TYPE_THRALL_PART4) != DONE)
            {
                SetCurrentWaypoint(103); // inn
                m_creature->SetFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP);
                m_creature->NearTeleportTo(2660.6f, 659.2f, 61.9f, 5.7f);
            }

            m_pInstance->SetData(TYPE_THRALL_EVENT, FAIL);
        }
    }

    void SummonedMovementInform(
        Creature* pSummoned, movement::gen type, uint32 uiData) override
    {
        if (pSummoned->GetEntry() == NPC_SKARLOC && type == movement::gen::gwp)
        {
            if (uiData == 2)
            {
                DoScriptText(SAY_TH_SKARLOC_MEET, m_creature);
                m_creature->HandleEmote(EMOTE_ONESHOT_POINT);
            }
            else if (uiData == 3)
            {
                pSummoned->Unmount();
                // Make thrall summoned so it gets added to his big remove list
                m_creature->SummonCreature(NPC_SKARLOC_MOUNT, pSummoned->GetX(),
                    pSummoned->GetY(), pSummoned->GetZ(), pSummoned->GetO(),
                    TEMPSUMMON_MANUAL_DESPAWN, 0);
            }
            else if (uiData == 4)
            {
                if (CreatureGroup* pGrp =
                        m_creature->GetMap()->GetCreatureGroupMgr().GetGroup(
                            m_skarlocGrpId))
                    pGrp->RemoveFlag(CREATURE_GROUP_FLAG_GROUP_MOVEMENT);
                DoScriptText(SAY_SKARLOC_MEET, pSummoned);

                // Attack
                m_uiRPEvent = TH_RP_EVENT_SKARLOC;
                m_uiRPPhase = 1;
                m_uiNextRPPhaseTimer = 11000;
            }
        }
    }

    void UpdateEscortAI(const uint32 uiDiff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
        {
            if (m_uiResumeEscortTimer)
            {
                if (m_uiResumeEscortTimer <= uiDiff)
                {
                    SetEscortPaused(false);
                    m_uiResumeEscortTimer = 0;
                }
                else
                    m_uiResumeEscortTimer -= uiDiff;
            }

            if (m_uiNextRPPhaseTimer)
            {
                if (m_uiNextRPPhaseTimer <= uiDiff)
                {
                    m_uiNextRPPhaseTimer = 0;

                    switch (m_uiRPEvent)
                    {
                    case TH_RP_EVENT_SKARLOC:
                    {
                        // Remove passive flags & attack Thrall
                        CreatureGroup* pGrp = m_creature->GetMap()
                                                  ->GetCreatureGroupMgr()
                                                  .GetGroup(m_skarlocGrpId);
                        if (pGrp)
                        {
                            for (auto& creature : pGrp->GetMembers())
                            {
                                (creature)->RemoveFlag(
                                    UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE |
                                                          UNIT_FLAG_PASSIVE);
                                if ((creature)->AI())
                                    (creature)->AI()->AttackStart(m_creature);
                            }
                        }
                        // Destroy the creature group
                        m_creature->GetMap()->GetCreatureGroupMgr().DeleteGroup(
                            m_skarlocGrpId);
                    }
                    break;
                    }
                }
                else
                    m_uiNextRPPhaseTimer -= uiDiff;
            }

            return;
        }

        // Only use abilities after we get our gear equipped
        if (m_creature->GetDisplayId() == MODEL_THRALL_EQUIPPED)
        {
            if (m_uiStrikeTimer <= uiDiff)
            {
                if (DoCastSpellIfCan(m_creature->getVictim(), SPELL_STRIKE) ==
                    CAST_OK)
                    m_uiStrikeTimer = urand(8000, 12000);
            }
            else
                m_uiStrikeTimer -= uiDiff;

            if (m_uiBlockTimer)
            {
                if (m_uiBlockTimer <= uiDiff)
                {
                    // Spell done in DamageTaken
                    m_uiBlockTimer = 0;
                }
                else
                    m_uiBlockTimer -= uiDiff;
            }
        }

        if (!m_bIsLowHp && m_creature->GetHealthPercent() < 20.0f)
        {
            DoScriptText(
                urand(0, 1) ? SAY_TH_RANDOM_LOW_HP1 : SAY_TH_RANDOM_LOW_HP2,
                m_creature);
            m_bIsLowHp = true;
        }

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_npc_thrall_old_hillsbrad(Creature* pCreature)
{
    return new npc_thrall_old_hillsbradAI(pCreature);
}

bool GossipHello_npc_thrall_old_hillsbrad(Player* pPlayer, Creature* pCreature)
{
    if (pCreature->isQuestGiver())
    {
        pPlayer->PrepareQuestMenu(pCreature->GetObjectGuid());
        pPlayer->SendPreparedQuest(pCreature->GetObjectGuid());
    }

    if (instance_old_hillsbrad* pInstance =
            (instance_old_hillsbrad*)pCreature->GetInstanceData())
    {
        if (pPlayer->isGameMaster())
        {
            pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT,
                "[GM] Move to part 1 (basement)", GOSSIP_SENDER_MAIN,
                GOSSIP_ACTION_INFO_DEF + 100);
            pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT,
                "[GM] Move to part 2 (Skarloc)", GOSSIP_SENDER_MAIN,
                GOSSIP_ACTION_INFO_DEF + 110);
            pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT,
                "[GM] Move to part 3 (barn)", GOSSIP_SENDER_MAIN,
                GOSSIP_ACTION_INFO_DEF + 120);
            pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT,
                "[GM] Move to part 4 (inn)", GOSSIP_SENDER_MAIN,
                GOSSIP_ACTION_INFO_DEF + 130);
        }

        if (!pInstance->GetData(TYPE_THRALL_EVENT))
        {
            if (pInstance->GetData(TYPE_BARREL_DIVERSION) == DONE)
                pPlayer->ADD_GOSSIP_ITEM_ID(GOSSIP_ICON_CHAT, GOSSIP_ITEM_START,
                    GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 10);

            if (pPlayer->isGameMaster())
                pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, "[GM] Start Event",
                    GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 10);

            // Send the menu even if we cannot start (just do not include the
            // option)
            pPlayer->SEND_GOSSIP_MENU(
                TEXT_ID_START, pCreature->GetObjectGuid());
        }

        if (pInstance->GetData(TYPE_THRALL_PART1) == DONE &&
            !pInstance->GetData(TYPE_THRALL_PART2))
        {
            pPlayer->ADD_GOSSIP_ITEM_ID(GOSSIP_ICON_CHAT, GOSSIP_ITEM_SKARLOC1,
                GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 20);
            pPlayer->SEND_GOSSIP_MENU(
                TEXT_ID_SKARLOC1, pCreature->GetObjectGuid());
        }

        if (pInstance->GetData(TYPE_THRALL_PART2) == DONE &&
            !pInstance->GetData(TYPE_THRALL_PART3))
        {
            pPlayer->ADD_GOSSIP_ITEM_ID(GOSSIP_ICON_CHAT, GOSSIP_ITEM_TARREN,
                GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 30);
            pPlayer->SEND_GOSSIP_MENU(
                TEXT_ID_TARREN, pCreature->GetObjectGuid());
        }

        if (pInstance->GetData(TYPE_THRALL_PART3) == DONE &&
            !pInstance->GetData(TYPE_THRALL_PART4))
        {
            pPlayer->SEND_GOSSIP_MENU(TEXT_ID_INN, pCreature->GetObjectGuid());
        }
    }
    return true;
}

bool GossipSelect_npc_thrall_old_hillsbrad(
    Player* pPlayer, Creature* pCreature, uint32 /*uiSender*/, uint32 uiAction)
{
    instance_old_hillsbrad* pInstance =
        (instance_old_hillsbrad*)pCreature->GetInstanceData();

    if (!pCreature->HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP))
        return true;

    switch (uiAction)
    {
    case GOSSIP_ACTION_INFO_DEF + 10:
    {
        // Make sure we're actually eligible to start the event, to avoid
        // cheating
        if (!((pInstance->GetData(TYPE_BARREL_DIVERSION) == DONE &&
                  !pInstance->GetData(TYPE_THRALL_EVENT)) ||
                pPlayer->isGameMaster()))
            return false;

        pPlayer->CLOSE_GOSSIP_MENU();

        pInstance->SetData(TYPE_THRALL_EVENT, IN_PROGRESS);
        pInstance->SetData(TYPE_THRALL_PART1, IN_PROGRESS);
        // Armorer might still be dead after a server restart
        if (Creature* armorer =
                pInstance->GetSingleCreatureFromStorage(NPC_ARMORER))
            if (!armorer->isAlive())
                armorer->Respawn();

        pInstance->DoUseDoorOrButton(GO_TH_PRISON_DOOR);

        DoScriptText(SAY_TH_START_EVENT_PART1, pCreature);

        pCreature->RemoveFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP);
        pCreature->SetWalk(false);

        if (npc_thrall_old_hillsbradAI* pThrallAI =
                dynamic_cast<npc_thrall_old_hillsbradAI*>(pCreature->AI()))
        {
            pThrallAI->SetEscortPaused(false);
            pThrallAI->Start(true, pPlayer, NULL, true);
        }

        break;
    }
    case GOSSIP_ACTION_INFO_DEF + 20:
    {
        pPlayer->ADD_GOSSIP_ITEM_ID(GOSSIP_ICON_CHAT, GOSSIP_ITEM_SKARLOC2,
            GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 21);
        pPlayer->SEND_GOSSIP_MENU(TEXT_ID_SKARLOC2, pCreature->GetObjectGuid());
        break;
    }
    case GOSSIP_ACTION_INFO_DEF + 21:
    {
        pPlayer->ADD_GOSSIP_ITEM_ID(GOSSIP_ICON_CHAT, GOSSIP_ITEM_SKARLOC3,
            GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 22);
        pPlayer->SEND_GOSSIP_MENU(TEXT_ID_SKARLOC3, pCreature->GetObjectGuid());
        break;
    }
    case GOSSIP_ACTION_INFO_DEF + 22:
    {
        pInstance->SetData(TYPE_THRALL_PART2, IN_PROGRESS);
        DoScriptText(SAY_TH_START_EVENT_PART2, pCreature);

        pCreature->RemoveFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP);
        if (npc_thrall_old_hillsbradAI* pThrallAI =
                dynamic_cast<npc_thrall_old_hillsbradAI*>(pCreature->AI()))
            pThrallAI->StartWP();
        break;
    }
    case GOSSIP_ACTION_INFO_DEF + 30:
    {
        pPlayer->CLOSE_GOSSIP_MENU();

        pInstance->SetData(TYPE_THRALL_PART3, IN_PROGRESS);

        pCreature->RemoveFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP);
        if (npc_thrall_old_hillsbradAI* pThrallAI =
                dynamic_cast<npc_thrall_old_hillsbradAI*>(pCreature->AI()))
            pThrallAI->StartWP();

        break;
    }
    case GOSSIP_ACTION_INFO_DEF + 100:
    case GOSSIP_ACTION_INFO_DEF + 110:
    case GOSSIP_ACTION_INFO_DEF + 120:
    case GOSSIP_ACTION_INFO_DEF + 130:
    {
        pPlayer->CLOSE_GOSSIP_MENU();
        if (pInstance->GetData(TYPE_THRALL_EVENT) != IN_PROGRESS)
            pInstance->SetData(TYPE_THRALL_EVENT, IN_PROGRESS);
        int j = (uiAction - GOSSIP_ACTION_INFO_DEF - 100) / 10;
        for (int i = 0; i < 5; ++i)
        {
            int state = NOT_STARTED;
            if (j == i)
                state = IN_PROGRESS;
            else if (j > i)
                state = DONE;
            pInstance->SetData(TYPE_THRALL_PART1 + i, state);
        }
        if (auto ai =
                dynamic_cast<npc_thrall_old_hillsbradAI*>(pCreature->AI()))
        {
            if (!ai->HasEscortState(STATE_ESCORT_ESCORTING))
            {
                ai->Start(true, pPlayer, NULL, true);
                ai->SetEscortPaused(true);
            }
        }
        pCreature->ForcedDespawn();
        break;
    }
    }
    return true;
}

/*######
## npc_taretha
######*/

enum
{
    TEXT_ID_EPOCH1 =
        9610, // Thank you for helping Thrall escape, friends. Now I only hope
    GOSSIP_ITEM_EPOCH1 = -3560005, // "Strange wizard?"
    TEXT_ID_EPOCH2 = 9613,         // Yes, friends. This man was no wizard of
    GOSSIP_ITEM_EPOCH2 = -3560006, // "We'll get you out. Taretha. Don't worry.
                                   // I doubt the wizard would wander too far
                                   // away."
};

npc_tarethaAI::npc_tarethaAI(Creature* pCreature) : npc_escortAI(pCreature)
{
    m_pInstance = (instance_old_hillsbrad*)pCreature->GetInstanceData();
    m_uiErozionEventTimer = 5000;
    m_uiErozionPhase = 0;
    m_creature->RemoveFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP);
    Reset();
}

void npc_tarethaAI::JustSummoned(Creature* pSummoned)
{
    if (pSummoned->GetEntry() == NPC_EROZION)
        m_erozionGuid = pSummoned->GetObjectGuid();
}

void npc_tarethaAI::WaypointReached(uint32 uiPoint)
{
    switch (uiPoint)
    {
    case 6:
        DoScriptText(SAY_TA_FREE, m_creature);
        break;
    case 7:
        m_creature->HandleEmote(EMOTE_ONESHOT_CHEER);
        SetEscortPaused(true);
        SetRun(false);
        break;
    }
}

void npc_tarethaAI::UpdateEscortAI(const uint32 uiDiff)
{
    if (!HasEscortState(STATE_ESCORT_PAUSED))
        return;

    if (m_uiErozionEventTimer < uiDiff)
    {
        ++m_uiErozionPhase;
        m_uiErozionEventTimer = 5000;

        switch (m_uiErozionPhase)
        {
        case 1:
            if (Creature* pThrall = m_pInstance->GetThrall())
            {
                pThrall->SetFacingToObject(m_creature);
                DoScriptText(SAY_TR_GLAD_SAFE, pThrall);
            }
            break;
        case 2:
            DoScriptText(SAY_TA_NEVER_MET, m_creature);
            break;
        case 3:
            if (Creature* pThrall = m_pInstance->GetThrall())
                DoScriptText(SAY_TR_THEN_WHO, pThrall);
            m_creature->SummonCreature(NPC_EROZION, 2646.47f, 680.416f, 55.38f,
                4.16f, TEMPSUMMON_TIMED_DESPAWN, SUMMONED_DESPAWN_TIME);
            break;
        case 4:
            if (Creature* pErozion =
                    m_creature->GetMap()->GetCreature(m_erozionGuid))
                DoScriptText(SAY_PRE_WIPE, pErozion);
            break;
        case 5:
            m_uiErozionEventTimer = 1;
            // if (Creature* pErozion =
            // m_creature->GetMap()->GetCreature(m_erozionGuid))
            // pErozion->AI()->DoCastSpellIfCan();
            break;
        case 6:
            if (Creature* pErozion =
                    m_creature->GetMap()->GetCreature(m_erozionGuid))
            {
                pErozion->CastSpell(m_creature, SPELL_MEMORY_WIPE, false);
                if (Creature* thrall = m_pInstance->GetThrall())
                    pErozion->CastSpell(thrall, SPELL_MEMORY_WIPE, true);
                DoScriptText(SAY_WIPE_MEMORY, pErozion);
                m_uiErozionEventTimer = 10000;
            }
            break;
        case 7:
            if (Creature* pErozion =
                    m_creature->GetMap()->GetCreature(m_erozionGuid))
                DoScriptText(SAY_ABOUT_TARETHA, pErozion);
            break;
            // case 8:
            m_uiErozionEventTimer = 1; // This phase didn't appear in retail
                                       /*if (Creature* pErozion =
                                          m_creature->GetMap()->GetCreature(m_erozionGuid))
                                           DoScriptText(SAY_AFTER_WIPE, pErozion);*/
            break;
        case 9:
            if (Creature* pThrall = m_pInstance->GetThrall())
                DoScriptText(SAY_TH_EVENT_COMPLETE, pThrall);
            break;
        case 10:
            DoScriptText(SAY_TA_FAREWELL, m_creature);
            SetEscortPaused(false);

            if (Creature* pThrall = m_pInstance->GetThrall())
            {
                if (npc_thrall_old_hillsbradAI* pThrallAI =
                        dynamic_cast<npc_thrall_old_hillsbradAI*>(
                            pThrall->AI()))
                    pThrallAI->SetEscortPaused(false);
                pThrall->SetFlag(UNIT_FIELD_FLAGS,
                    UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_PASSIVE);
            }

            m_creature->SetFlag(
                UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_PASSIVE);

            m_pInstance->SetData(TYPE_THRALL_PART5, DONE);

            break;
        }
    }
    else
        m_uiErozionEventTimer -= uiDiff;
}

CreatureAI* GetAI_npc_taretha(Creature* pCreature)
{
    return new npc_tarethaAI(pCreature);
}

bool GossipHello_npc_taretha(Player* pPlayer, Creature* pCreature)
{
    instance_old_hillsbrad* pInstance =
        (instance_old_hillsbrad*)pCreature->GetInstanceData();

    if (pInstance && pInstance->GetData(TYPE_THRALL_PART3) == DONE &&
        pInstance->GetData(TYPE_THRALL_PART4) == NOT_STARTED)
    {
        pPlayer->ADD_GOSSIP_ITEM_ID(GOSSIP_ICON_CHAT, GOSSIP_ITEM_EPOCH1,
            GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 1);
        pPlayer->SEND_GOSSIP_MENU(TEXT_ID_EPOCH1, pCreature->GetObjectGuid());
    }

    return true;
}

bool GossipSelect_npc_taretha(
    Player* pPlayer, Creature* pCreature, uint32 /*uiSender*/, uint32 uiAction)
{
    instance_old_hillsbrad* pInstance =
        (instance_old_hillsbrad*)pCreature->GetInstanceData();

    if (!pCreature->HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP))
        return true;

    if (uiAction == GOSSIP_ACTION_INFO_DEF + 1)
    {
        pPlayer->ADD_GOSSIP_ITEM_ID(GOSSIP_ICON_CHAT, GOSSIP_ITEM_EPOCH2,
            GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 2);
        pPlayer->SEND_GOSSIP_MENU(TEXT_ID_EPOCH2, pCreature->GetObjectGuid());
    }
    else if (uiAction == GOSSIP_ACTION_INFO_DEF + 2)
    {
        pPlayer->CLOSE_GOSSIP_MENU();

        if (pInstance && pInstance->GetData(TYPE_THRALL_EVENT) == IN_PROGRESS &&
            pInstance && pInstance->GetData(TYPE_THRALL_PART3) == DONE &&
            pInstance->GetData(TYPE_THRALL_PART4) == NOT_STARTED)
        {
            if (Creature* pThrall = pInstance->GetThrall())
            {
                pInstance->SetData(TYPE_THRALL_PART4, IN_PROGRESS);
                pThrall->SummonCreature(NPC_EPOCH, 2639.13f, 698.55f, 70.0f,
                    4.59f, TEMPSUMMON_TIMED_OR_DEAD_DESPAWN,
                    SUMMONED_DESPAWN_TIME);

                if (npc_thrall_old_hillsbradAI* pThrallAI =
                        dynamic_cast<npc_thrall_old_hillsbradAI*>(
                            pThrall->AI()))
                    pThrallAI->StartWP();
            }
        }
    }

    return true;
}

enum
{
    SAY_EROZION_WARNING_1 = -1560071,
    SAY_EROZION_WARNING_2 = -1560072,
};

/*#####
## npc_image_of_erozion
######*/
struct MANGOS_DLL_DECL npc_image_of_erozionAI : public ScriptedAI
{
    npc_image_of_erozionAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_uiSpeakTimer = 2000;
        m_pInstance = (instance_old_hillsbrad*)pCreature->GetInstanceData();
        Reset();
    }

    uint32 m_uiSpeakTimer;
    instance_old_hillsbrad* m_pInstance;

    void Reset() override {}

    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_pInstance)
            return;

        if (m_uiSpeakTimer)
        {
            if (m_uiSpeakTimer <= uiDiff)
            {
                if (m_pInstance->GetData(TYPE_THRALL_DEATHS) <= 3)
                {
                    if (Player* nearest = m_creature->FindNearestPlayer(40.0f))
                        m_creature->SetFacingToObject(nearest);

                    if (m_pInstance->GetData(TYPE_THRALL_DEATHS) < 3)
                        DoScriptText(SAY_EROZION_WARNING_1, m_creature);
                    else
                        DoScriptText(SAY_EROZION_WARNING_2, m_creature);

                    if (Creature* thrall = m_pInstance->GetThrall())
                        thrall->Respawn();

                    m_creature->ForcedDespawn(10000);
                    m_uiSpeakTimer = 0;
                }
            }
            else
                m_uiSpeakTimer -= uiDiff;
        }
    }
};

CreatureAI* GetAI_npc_image_of_erozion(Creature* pCreature)
{
    return new npc_image_of_erozionAI(pCreature);
}

/*######
## npc_brazen
######*/
enum
{
    TEXT_BRAZEN_ID = 9779,
    GOSSIP_ITEM_BRAZEN = -3560010,

    TAXI_PATH_ID = 534,
};

bool GossipHello_npc_brazen(Player* pPlayer, Creature* pCreature)
{
    pPlayer->ADD_GOSSIP_ITEM_ID(GOSSIP_ICON_CHAT, GOSSIP_ITEM_BRAZEN,
        GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 1);
    pPlayer->SEND_GOSSIP_MENU(TEXT_BRAZEN_ID, pCreature->GetObjectGuid());

    return true;
}

bool GossipSelect_npc_brazen(Player* pPlayer, Creature* /*pCreature*/,
    uint32 /*uiSender*/, uint32 uiAction)
{
    if (uiAction == GOSSIP_ACTION_INFO_DEF + 1)
    {
        pPlayer->CLOSE_GOSSIP_MENU();
        pPlayer->ActivateTaxiPathTo(TAXI_PATH_ID);
    }

    return true;
}

/*######
## AddSC
######*/

void AddSC_old_hillsbrad()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "npc_erozion";
    pNewScript->pGossipHello = &GossipHello_npc_erozion;
    pNewScript->pGossipSelect = &GossipSelect_npc_erozion;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "npc_thrall_old_hillsbrad";
    pNewScript->pGossipHello = &GossipHello_npc_thrall_old_hillsbrad;
    pNewScript->pGossipSelect = &GossipSelect_npc_thrall_old_hillsbrad;
    pNewScript->GetAI = &GetAI_npc_thrall_old_hillsbrad;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "npc_taretha";
    pNewScript->pGossipHello = &GossipHello_npc_taretha;
    pNewScript->pGossipSelect = &GossipSelect_npc_taretha;
    pNewScript->GetAI = &GetAI_npc_taretha;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "npc_image_of_erozion";
    pNewScript->GetAI = &GetAI_npc_image_of_erozion;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "npc_brazen";
    pNewScript->pGossipHello = &GossipHello_npc_brazen;
    pNewScript->pGossipSelect = &GossipSelect_npc_brazen;
    pNewScript->RegisterSelf();
}
