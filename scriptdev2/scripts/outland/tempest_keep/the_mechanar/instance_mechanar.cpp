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
SDName: Instance_Mechanar
SD%Complete: 20
SDComment:
SDCategory: Mechanar
EndScriptData */

#include "mechanar.h"

instance_mechanar::instance_mechanar(Map* pMap) : ScriptedInstance(pMap)
{
    Initialize();
}

void instance_mechanar::Initialize()
{
    memset(&m_auiEncounter, 0, sizeof(m_auiEncounter));
}

void instance_mechanar::SetData(uint32 uiType, uint32 uiData)
{
    switch (uiType)
    {
    case TYPE_SEPETHREA:
        m_auiEncounter[0] = uiData;
        break;

    case TYPE_GATEWATCHER_IRONHAND:
        m_auiEncounter[1] = uiData;
        break;

    case TYPE_GATEWATCHER_GYROKILL:
        m_auiEncounter[2] = uiData;
        break;

    case TYPE_PATHALEON_THE_CALCULATOR:
        m_auiEncounter[3] = uiData;
        break;
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

void instance_mechanar::Load(const char* chrIn)
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

uint32 instance_mechanar::GetData(uint32 uiType)
{
    switch (uiType)
    {
    case TYPE_SEPETHREA:
        return m_auiEncounter[0];
        break;

    case TYPE_GATEWATCHER_IRONHAND:
        return m_auiEncounter[1];
        break;

    case TYPE_GATEWATCHER_GYROKILL:
        return m_auiEncounter[2];
        break;

    case TYPE_PATHALEON_THE_CALCULATOR:
        return m_auiEncounter[3];
        break;
    }

    return 0;
}

void instance_mechanar::OnObjectCreate(GameObject* pGo)
{
    switch (pGo->GetEntry())
    {
    case GO_MOARG_1_DOOR:
        if (GetData(TYPE_GATEWATCHER_IRONHAND) == DONE)
            pGo->SetGoState(GO_STATE_ACTIVE);
        break;

    case GO_MOARG_2_DOOR:
        if (GetData(TYPE_GATEWATCHER_GYROKILL) == DONE)
            pGo->SetGoState(GO_STATE_ACTIVE);
        break;

    case GO_NETHERMANCER_ENCOUNTER_DOOR:
        break;

    default:
        return;
    }
    m_mGoEntryGuidStore[pGo->GetEntry()] = pGo->GetObjectGuid();
}

InstanceData* GetInstanceData_instance_mechanar(Map* pMap)
{
    return new instance_mechanar(pMap);
}

void AddSC_instance_mechanar()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "instance_mechanar";
    pNewScript->GetInstanceData = &GetInstanceData_instance_mechanar;
    pNewScript->RegisterSelf();
}
