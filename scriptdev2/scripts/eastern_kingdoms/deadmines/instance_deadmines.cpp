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
SDName: Instance_Deadmines
SD%Complete: 0
SDComment: Placeholder
SDCategory: Deadmines
EndScriptData */

#include "deadmines.h"
#include "precompiled.h"

instance_deadmines::instance_deadmines(Map* pMap) : ScriptedInstance(pMap)
{
    Initialize();
}

void instance_deadmines::Initialize()
{
    memset(&m_auiEncounter, 0, sizeof(m_auiEncounter));
}

void instance_deadmines::OnPlayerEnter(Player* pPlayer)
{
    // Respawn the Mysterious chest if one of the players who enter the instance
    // has the quest in his log
    if (pPlayer->GetQuestStatus(QUEST_FORTUNE_AWAITS) ==
            QUEST_STATUS_COMPLETE &&
        !pPlayer->GetQuestRewardStatus(QUEST_FORTUNE_AWAITS))
        DoRespawnGameObject(GO_MYSTERIOUS_CHEST, HOUR);
}

void instance_deadmines::OnCreatureCreate(Creature* pCreature)
{
    if (pCreature->GetEntry() == NPC_MR_SMITE)
        m_mNpcEntryGuidStore[NPC_MR_SMITE] = pCreature->GetObjectGuid();
}

void instance_deadmines::OnObjectCreate(GameObject* pGo)
{
    switch (pGo->GetEntry())
    {
    case GO_FACTORY_DOOR:
        if (m_auiEncounter[TYPE_RHAHKZOR] == DONE)
            pGo->SetGoState(GO_STATE_ACTIVE);

        break;
    case GO_MAST_ROOM_DOOR:
        if (m_auiEncounter[TYPE_SNEED] == DONE)
            pGo->SetGoState(GO_STATE_ACTIVE);

        break;
    case GO_FOUNDRY_DOOR:
        if (m_auiEncounter[TYPE_GILNID] == DONE)
            pGo->SetGoState(GO_STATE_ACTIVE);

        break;
    case GO_IRON_CLAD_DOOR:
        if (m_auiEncounter[TYPE_IRON_CLAD_DOOR] == DONE)
            pGo->SetGoState(GO_STATE_ACTIVE_ALTERNATIVE);

        break;
    case GO_DEFIAS_CANNON:
    case GO_MYSTERIOUS_CHEST:
        break;

    default:
        return;
    }

    m_mGoEntryGuidStore[pGo->GetEntry()] = pGo->GetObjectGuid();
}

void instance_deadmines::OnCreatureDeath(Creature* pCreature)
{
    switch (pCreature->GetEntry())
    {
    case NPC_RHAHKZOR:
        SetData(TYPE_RHAHKZOR, DONE);
        break;
    case NPC_SNEED:
        SetData(TYPE_SNEED, DONE);
        break;
    case NPC_GILNID:
        SetData(TYPE_GILNID, DONE);
        break;
    }
}

void instance_deadmines::SetData(uint32 uiType, uint32 uiData)
{
    switch (uiType)
    {
    case TYPE_RHAHKZOR:
    {
        if (uiData == DONE)
            DoUseDoorOrButton(GO_FACTORY_DOOR);

        m_auiEncounter[uiType] = uiData;
        break;
    }
    case TYPE_SNEED:
    {
        if (uiData == DONE)
            DoUseDoorOrButton(GO_MAST_ROOM_DOOR);

        m_auiEncounter[uiType] = uiData;
        break;
    }
    case TYPE_GILNID:
    {
        if (uiData == DONE)
            DoUseDoorOrButton(GO_FOUNDRY_DOOR);

        m_auiEncounter[uiType] = uiData;
        break;
    }
    case TYPE_IRON_CLAD_DOOR:
    {
        if (uiData == DONE)
            DoUseDoorOrButton(GO_IRON_CLAD_DOOR, 0, true);

        m_auiEncounter[uiType] = uiData;
        break;
    }
    }

    if (uiData == DONE)
    {
        OUT_SAVE_INST_DATA;

        std::ostringstream saveStream;
        saveStream << m_auiEncounter[0] << " " << m_auiEncounter[1] << " "
                   << m_auiEncounter[2] << " " << m_auiEncounter[3];

        m_strInstData = saveStream.str();

        SaveToDB();
        OUT_SAVE_INST_DATA_COMPLETE;
    }
}

uint32 instance_deadmines::GetData(uint32 uiType)
{
    if (uiType < MAX_ENCOUNTER)
        return m_auiEncounter[uiType];

    return 0;
}

void instance_deadmines::Load(const char* chrIn)
{
    if (!chrIn)
    {
        OUT_LOAD_INST_DATA_FAIL;
        return;
    }

    OUT_LOAD_INST_DATA(chrIn);

    std::istringstream loadStream(chrIn);
    loadStream >> m_auiEncounter[0] >> m_auiEncounter[1] >> m_auiEncounter[2] >>
        m_auiEncounter[3];

    for (auto& elem : m_auiEncounter)
    {
        if (elem == IN_PROGRESS)
            elem = NOT_STARTED;
    }

    OUT_LOAD_INST_DATA_COMPLETE;
}

InstanceData* GetInstanceData_instance_deadmines(Map* pMap)
{
    return new instance_deadmines(pMap);
}

void AddSC_instance_deadmines()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "instance_deadmines";
    pNewScript->GetInstanceData = &GetInstanceData_instance_deadmines;
    pNewScript->RegisterSelf();
}
