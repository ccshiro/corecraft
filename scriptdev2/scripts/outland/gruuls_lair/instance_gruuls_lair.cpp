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
SDName: instance_gruuls_lair
SD%Complete: 100
SDComment:
SDCategory: Gruul's Lair
EndScriptData */

#include "gruuls_lair.h"

/* Gruuls Lair encounters:
1 - High King Maulgar event
2 - Gruul event
*/

instance_gruuls_lair::instance_gruuls_lair(Map* pMap) : ScriptedInstance(pMap)
{
    Initialize();
}

void instance_gruuls_lair::Initialize()
{
    memset(&m_auiEncounter, 0, sizeof(m_auiEncounter));
}

bool instance_gruuls_lair::IsEncounterInProgress() const
{
    for (auto& elem : m_auiEncounter)
        if (elem == IN_PROGRESS)
            return true;

    return false;
}

void instance_gruuls_lair::OnCreatureCreate(Creature* pCreature)
{
    switch (pCreature->GetEntry())
    {
    case NPC_MAULGAR:
    case NPC_KROSH:
    case NPC_OLM:
    case NPC_KIGGLER:
    case NPC_BLINDEYE:
    case NPC_GRUUL:
        m_mNpcEntryGuidStore[pCreature->GetEntry()] =
            pCreature->GetObjectGuid();
        break;
    }
}

bool instance_gruuls_lair::CheckAlive(uint32 entry)
{
    if (Creature* c = this->GetSingleCreatureFromStorage(entry))
        return c->isAlive();
    return false;
}

void instance_gruuls_lair::OnCreatureDeath(Creature* pCreature)
{
    switch (pCreature->GetEntry())
    {
    case NPC_KROSH:
    case NPC_OLM:
    case NPC_KIGGLER:
    case NPC_BLINDEYE:
        uint32 alive = 4;
        alive += !CheckAlive(NPC_KROSH) ? -1 : 0;
        alive += !CheckAlive(NPC_OLM) ? -1 : 0;
        alive += !CheckAlive(NPC_KIGGLER) ? -1 : 0;
        alive += !CheckAlive(NPC_BLINDEYE) ? -1 : 0;
        Creature* maulgar = GetSingleCreatureFromStorage(NPC_MAULGAR);
        if (maulgar && maulgar->isAlive())
        {
            switch (alive)
            {
            case 3:
                DoScriptText(SAY_OGRE_DEATH_1, maulgar);
                break;
            case 2:
                DoScriptText(SAY_OGRE_DEATH_2, maulgar);
                break;
            case 1:
                DoScriptText(SAY_OGRE_DEATH_3, maulgar);
                break;
            case 0:
                DoScriptText(SAY_OGRE_DEATH_4, maulgar);
                break;
            }
        }
        break;
    }
}

void instance_gruuls_lair::OnObjectCreate(GameObject* pGo)
{
    switch (pGo->GetEntry())
    {
    case GO_PORT_GRONN_1:
        if (GetData(TYPE_MAULGAR) == DONE)
            pGo->SetGoState(GO_STATE_ACTIVE);
        break;
    case GO_PORT_GRONN_2:
        break;

    default:
        return;
    }
    m_mGoEntryGuidStore[pGo->GetEntry()] = pGo->GetObjectGuid();
}

void instance_gruuls_lair::SetData(uint32 uiType, uint32 uiData)
{
    switch (uiType)
    {
    case TYPE_MAULGAR:
        if (uiData == DONE)
            DoUseDoorOrButton(GO_PORT_GRONN_1);
        m_auiEncounter[uiType] = uiData;
        break;
    case TYPE_GRUUL:
        DoUseDoorOrButton(GO_PORT_GRONN_2);
        m_auiEncounter[uiType] = uiData;
        break;
    }

    if (uiData == DONE)
    {
        OUT_SAVE_INST_DATA;

        std::ostringstream saveStream;
        saveStream << m_auiEncounter[0] << " " << m_auiEncounter[1];

        m_strSaveData = saveStream.str();

        SaveToDB();
        OUT_SAVE_INST_DATA_COMPLETE;
    }
}

uint32 instance_gruuls_lair::GetData(uint32 uiType)
{
    switch (uiType)
    {
    case TYPE_MAULGAR:
        return m_auiEncounter[uiType];
    case TYPE_GRUUL:
        return m_auiEncounter[uiType];

    default:
        return 0;
    }
}

void instance_gruuls_lair::Load(const char* chrIn)
{
    if (!chrIn)
    {
        OUT_LOAD_INST_DATA_FAIL;
        return;
    }

    OUT_LOAD_INST_DATA(chrIn);

    std::istringstream loadStream(chrIn);

    loadStream >> m_auiEncounter[0] >> m_auiEncounter[1];

    for (auto& elem : m_auiEncounter)
        if (elem == IN_PROGRESS)
            elem = NOT_STARTED;

    OUT_LOAD_INST_DATA_COMPLETE;
}

InstanceData* GetInstanceData_instance_gruuls_lair(Map* pMap)
{
    return new instance_gruuls_lair(pMap);
}

void AddSC_instance_gruuls_lair()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "instance_gruuls_lair";
    pNewScript->GetInstanceData = &GetInstanceData_instance_gruuls_lair;
    pNewScript->RegisterSelf();
}
