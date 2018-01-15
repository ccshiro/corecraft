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
SDName: Instance - Sethekk Halls
SD%Complete: 50
SDComment: Instance Data for Sethekk Halls instance
SDCategory: Auchindoun, Sethekk Halls
EndScriptData */

#include "precompiled.h"
#include "sethekk_halls.h"

instance_sethekk_halls::instance_sethekk_halls(Map* pMap)
  : ScriptedInstance(pMap)
{
    Initialize();
}
void instance_sethekk_halls::Initialize()
{
    memset(&m_auiEncounter, 0, sizeof(m_auiEncounter));
}

void instance_sethekk_halls::OnCreatureCreate(Creature* pCreature)
{
    switch (pCreature->GetEntry())
    {
    case NPC_LAKKA:
        if (m_auiEncounter[TYPE_SYTH] != DONE)
            pCreature->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
        break;
    default:
        return;
    }

    m_mNpcEntryGuidStore[pCreature->GetEntry()] = pCreature->GetObjectGuid();
}

void instance_sethekk_halls::OnObjectCreate(GameObject* pGo)
{
    switch (pGo->GetEntry())
    {
    case GO_IKISS_DOOR:
        if (m_auiEncounter[TYPE_IKISS] == DONE)
            pGo->SetGoState(GO_STATE_ACTIVE);
        break;
    case GO_IKISS_CHEST:
        if (m_auiEncounter[TYPE_IKISS] == DONE)
            pGo->RemoveFlag(
                GAMEOBJECT_FLAGS, GO_FLAG_NO_INTERACT | GO_FLAG_INTERACT_COND);
        else
            pGo->SetFlag(
                GAMEOBJECT_FLAGS, GO_FLAG_NO_INTERACT | GO_FLAG_INTERACT_COND);
        break;

    default:
        return;
    }

    m_mGoEntryGuidStore[pGo->GetEntry()] = pGo->GetObjectGuid();
}

void instance_sethekk_halls::SetData(uint32 uiType, uint32 uiData)
{
    switch (uiType)
    {
    case TYPE_SYTH:
        if (uiData == DONE)
        {
            if (Creature* pLakka = GetSingleCreatureFromStorage(NPC_LAKKA))
            {
                pLakka->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
                pLakka->MonsterYell(
                    "Well done! Hurry, though, we don't want to be caught!", 0);
            }
        }
        m_auiEncounter[uiType] = uiData;
        break;
    case TYPE_ANZU:
        m_auiEncounter[uiType] = uiData;
        break;
    case TYPE_IKISS:
        if (uiData == DONE)
        {
            DoUseDoorOrButton(GO_IKISS_DOOR, DAY);
            DoToggleGameObjectFlags(GO_IKISS_CHEST,
                GO_FLAG_NO_INTERACT | GO_FLAG_INTERACT_COND, false);
        }
        m_auiEncounter[uiType] = uiData;
        break;
    default:
        return;
    }

    if (uiData == DONE)
    {
        OUT_SAVE_INST_DATA;

        std::ostringstream saveStream;
        saveStream << m_auiEncounter[0] << " " << m_auiEncounter[1] << " "
                   << m_auiEncounter[2];

        m_strInstData = saveStream.str();

        SaveToDB();
        OUT_SAVE_INST_DATA_COMPLETE;
    }
}

uint32 instance_sethekk_halls::GetData(uint32 uiType)
{
    if (uiType < MAX_ENCOUNTER)
        return m_auiEncounter[uiType];

    return 0;
}

void instance_sethekk_halls::Load(const char* chrIn)
{
    if (!chrIn)
    {
        OUT_LOAD_INST_DATA_FAIL;
        return;
    }

    OUT_LOAD_INST_DATA(chrIn);

    std::istringstream loadStream(chrIn);
    loadStream >> m_auiEncounter[0] >> m_auiEncounter[1] >> m_auiEncounter[2];

    for (auto& elem : m_auiEncounter)
    {
        if (elem == IN_PROGRESS)
            elem = NOT_STARTED;
    }

    OUT_LOAD_INST_DATA_COMPLETE;
}

InstanceData* GetInstanceData_instance_sethekk_halls(Map* pMap)
{
    return new instance_sethekk_halls(pMap);
}

/* Lakka - Quest & Scripted Event */
#define LAKKA_RESCUE_OPTION "I'll have you out of there in just a moment."

bool GossipHello_npc_lakka(Player* pPlayer, Creature* pCreature)
{
    pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, LAKKA_RESCUE_OPTION,
        GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 1);
    pPlayer->SEND_GOSSIP_MENU(9636, pCreature->GetObjectGuid());

    return true;
}

bool GossipSelect_npc_lakka(
    Player* pPlayer, Creature* pCreature, uint32 /*uiSender*/, uint32 uiAction)
{
    if (uiAction == GOSSIP_ACTION_INFO_DEF + 1)
    {
        // Remove the possibility of further gossip
        pPlayer->CLOSE_GOSSIP_MENU();
        pCreature->RemoveFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP);

        std::vector<DynamicWaypoint> wps;
        wps.push_back(DynamicWaypoint(-142.4f, 162.9f, 0.009f, 0.36f));
        wps.push_back(DynamicWaypoint(-128.7f, 173.6f, 0.009f, 0.09f));
        wps.push_back(DynamicWaypoint(-97.3f, 174.0f, 0.009f, 6.26f));
        pCreature->movement_gens.push(
            new movement::DynamicWaypointMovementGenerator(wps, false));

        // Despawn and complete quest in 18 seconds (XXX: Any reason we can't
        // use ForceDespawn?)
        pCreature->queue_action(20000, [pCreature]()
            {
                // Despawn lakka
                pCreature->SetVisibility(VISIBILITY_OFF);
                pCreature->Kill(pCreature, false);
            });

        // Yell a thanks to our handsome saviors
        char buf[126] = {0};
        sprintf(buf,
            "Thank you for freeing me, %s! I'm going to make my way to "
            "Shattrath!",
            pPlayer->GetName()); // Name can be 16 chars at max so no worry of
                                 // overflowing
        pCreature->MonsterYell(buf, 0);

        // Open the chage we're inside of
        if (GameObject* pCage =
                GetClosestGameObjectWithEntry(pCreature, GO_LAKKA_CAGE, 25.0f))
            pCage->Use(pCreature);

        // Complete everyone's quest
        const Map::PlayerList& pl = pCreature->GetMap()->GetPlayers();
        for (const auto& elem : pl)
        {
            if (Player* plr = elem.getSource())
            {
                // Complete Brother Against Brother (Free Lakka part)
                if (plr->IsAtGroupRewardDistance(pCreature) && plr->isAlive() &&
                    !plr->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_GHOST))
                    plr->KilledMonsterCredit(
                        NPC_LAKKA, pCreature->GetObjectGuid());
            }
        }
    }

    return true;
}

void AddSC_instance_sethekk_halls()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "instance_sethekk_halls";
    pNewScript->GetInstanceData = &GetInstanceData_instance_sethekk_halls;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "npc_lakka";
    pNewScript->pGossipHello = &GossipHello_npc_lakka;
    pNewScript->pGossipSelect = &GossipSelect_npc_lakka;
    pNewScript->RegisterSelf();
}
