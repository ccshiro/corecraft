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
SDName: Instance_Arcatraz
SD%Complete: 100
SDComment:
SDCategory: Tempest Keep, The Arcatraz
EndScriptData */

#include "arcatraz.h"
#include "precompiled.h"

/* Arcatraz encounters:
1 - Zereketh the Unbound event
2 - Dalliah the Doomsayer event
3 - Wrath-Scryer Soccothrates event
4 - Harbinger Skyriss event, 5 sub-events
*/

instance_arcatraz::instance_arcatraz(Map* pMap) : ScriptedInstance(pMap)
{
    Initialize();
}

void instance_arcatraz::Initialize()
{
    memset(&m_auiEncounter, 0, sizeof(m_auiEncounter));

    m_uiAggroSocTimer = 0;
    m_uiDieSocTimer = 0;
    m_uiAggroDahTimer = 0;
    m_uiDieDahTimer = 0;
}

void instance_arcatraz::OnObjectCreate(GameObject* pGo)
{
    switch (pGo->GetEntry())
    {
    case GO_CORE_SECURITY_FIELD_ALPHA:
        if (GetData(TYPE_SOCCOTHRATES) == DONE)
            pGo->SetGoState(GO_STATE_ACTIVE);
        break;
    case GO_CORE_SECURITY_FIELD_BETA:
        if (GetData(TYPE_DALLIAH) == DONE)
            pGo->SetGoState(GO_STATE_ACTIVE);
        break;
    case GO_SEAL_SPHERE:
    case GO_POD_ALPHA:
    case GO_POD_BETA:
    case GO_POD_DELTA:
    case GO_POD_GAMMA:
    case GO_POD_OMEGA:
        break;
    default:
        return;
    }

    m_mGoEntryGuidStore[pGo->GetEntry()] = pGo->GetObjectGuid();
}

void instance_arcatraz::OnCreatureCreate(Creature* pCreature)
{
    switch (pCreature->GetEntry())
    {
    case NPC_MELLICHAR:
    case NPC_SOCCOTHRATES:
    case NPC_DALLIAH:
    case NPC_MILLHOUSE:
        break;
    default:
        return;
    }

    m_mNpcEntryGuidStore[pCreature->GetEntry()] = pCreature->GetObjectGuid();
}

void instance_arcatraz::SetData(uint32 uiType, uint32 uiData)
{
    switch (uiType)
    {
    case TYPE_ZEREKETH:
        m_auiEncounter[TYPE_ZEREKETH] = uiData;
        break;

    case TYPE_DALLIAH:
        if (uiData == DONE)
        {
            DoUseDoorOrButton(GO_CORE_SECURITY_FIELD_BETA);
            m_uiDieDahTimer = 5000;
        }
        else if (uiData == IN_PROGRESS)
            m_uiAggroDahTimer = 5000;
        m_auiEncounter[TYPE_DALLIAH] = uiData;
        break;

    case TYPE_SOCCOTHRATES:
        if (uiData == DONE)
        {
            DoUseDoorOrButton(GO_CORE_SECURITY_FIELD_ALPHA);
            m_uiDieSocTimer = 5000;
        }
        else if (uiData == IN_PROGRESS)
            m_uiAggroSocTimer = 5000;
        m_auiEncounter[TYPE_SOCCOTHRATES] = uiData;
        break;

    case TYPE_HARBINGERSKYRISS:
        if (uiData == NOT_STARTED || uiData == FAIL)
        {
            SetData(TYPE_WARDEN_1, NOT_STARTED);
            SetData(TYPE_WARDEN_2, NOT_STARTED);
            SetData(TYPE_WARDEN_3, NOT_STARTED);
            SetData(TYPE_WARDEN_4, NOT_STARTED);
            SetData(TYPE_WARDEN_5, NOT_STARTED);
        }
        if (uiData == FAIL)
        {
            // Respawn Mellichar if he's dead (he toggles his shield off
            // himself)
            if (Creature* mellichar =
                    GetSingleCreatureFromStorage(NPC_MELLICHAR))
            {
                if (!mellichar->isAlive())
                    mellichar->Respawn();
            }
            // Reset all pods
            if (GameObject* pod = GetSingleGameObjectFromStorage(GO_POD_ALPHA))
                pod->SetGoState(GO_STATE_READY);
            if (GameObject* pod = GetSingleGameObjectFromStorage(GO_POD_BETA))
                pod->SetGoState(GO_STATE_READY);
            if (GameObject* pod = GetSingleGameObjectFromStorage(GO_POD_DELTA))
                pod->SetGoState(GO_STATE_READY);
            if (GameObject* pod = GetSingleGameObjectFromStorage(GO_POD_OMEGA))
                pod->SetGoState(GO_STATE_READY);
            if (GameObject* pod = GetSingleGameObjectFromStorage(GO_POD_GAMMA))
                pod->SetGoState(GO_STATE_READY);
        }
        if (uiData == DONE)
        {
            Creature* millhouse = GetSingleCreatureFromStorage(NPC_MILLHOUSE);
            if (millhouse && millhouse->isAlive())
            {
                // Complete quest
                for (const auto& elem : instance->GetPlayers())
                    if (elem.getSource()->GetQuestStatus(
                            QUEST_TRIAL_OF_NAARU) == QUEST_STATUS_INCOMPLETE)
                        elem.getSource()->AreaExploredOrEventHappens(
                            QUEST_TRIAL_OF_NAARU);
            }
            else
            {
                // Fail quest
                for (const auto& elem : instance->GetPlayers())
                    if (elem.getSource()->GetQuestStatus(
                            QUEST_TRIAL_OF_NAARU) == QUEST_STATUS_INCOMPLETE)
                        elem.getSource()->FailQuest(QUEST_TRIAL_OF_NAARU);
            }
        }
        m_auiEncounter[TYPE_HARBINGERSKYRISS] = uiData;
        break;

    case TYPE_WARDEN_1:
        if (uiData == IN_PROGRESS)
            DoUseDoorOrButton(GO_POD_ALPHA);
        m_auiEncounter[TYPE_WARDEN_1] = uiData;
        break;

    case TYPE_WARDEN_2:
        if (uiData == IN_PROGRESS)
            DoUseDoorOrButton(GO_POD_BETA);
        m_auiEncounter[TYPE_WARDEN_2] = uiData;
        break;

    case TYPE_WARDEN_3:
        if (uiData == IN_PROGRESS)
            DoUseDoorOrButton(GO_POD_DELTA);
        m_auiEncounter[TYPE_WARDEN_3] = uiData;
        break;

    case TYPE_WARDEN_4:
        if (uiData == IN_PROGRESS)
            DoUseDoorOrButton(GO_POD_GAMMA);
        m_auiEncounter[TYPE_WARDEN_4] = uiData;
        break;

    case TYPE_WARDEN_5:
        if (uiData == IN_PROGRESS)
            DoUseDoorOrButton(GO_POD_OMEGA);
        m_auiEncounter[TYPE_WARDEN_5] = uiData;
        break;
    default:
        return;
    }

    if (uiData == DONE)
    {
        OUT_SAVE_INST_DATA;

        std::ostringstream saveStream;
        saveStream << m_auiEncounter[0] << " " << m_auiEncounter[1] << " "
                   << m_auiEncounter[2] << " " << m_auiEncounter[3] << " "
                   << m_auiEncounter[4] << " " << m_auiEncounter[5] << " "
                   << m_auiEncounter[6] << " " << m_auiEncounter[7] << " "
                   << m_auiEncounter[8];

        m_strInstData = saveStream.str();

        SaveToDB();
        OUT_SAVE_INST_DATA_COMPLETE;
    }
}

void instance_arcatraz::Load(const char* chrIn)
{
    if (!chrIn)
    {
        OUT_LOAD_INST_DATA_FAIL;
        return;
    }

    OUT_LOAD_INST_DATA(chrIn);

    std::istringstream loadStream(chrIn);
    loadStream >> m_auiEncounter[0] >> m_auiEncounter[1] >> m_auiEncounter[2] >>
        m_auiEncounter[3] >> m_auiEncounter[4] >> m_auiEncounter[5] >>
        m_auiEncounter[6] >> m_auiEncounter[7] >> m_auiEncounter[8];

    for (auto& elem : m_auiEncounter)
    {
        if (elem == IN_PROGRESS)
            elem = NOT_STARTED;
    }

    OUT_LOAD_INST_DATA_COMPLETE;
}

uint32 instance_arcatraz::GetData(uint32 uiType)
{
    if (uiType < MAX_ENCOUNTER)
        return m_auiEncounter[uiType];
    return 0;
}

void instance_arcatraz::Update(uint32 uiDiff)
{
    if (m_uiAggroSocTimer)
    {
        if (m_uiAggroSocTimer <= uiDiff)
        {
            if (Creature* dah = GetDalliah())
                DoScriptText(SAY_DH_SOC_AGGRO, dah);
            m_uiAggroSocTimer = 0;
        }
        else
            m_uiAggroSocTimer -= uiDiff;
    }

    if (m_uiDieSocTimer)
    {
        if (m_uiDieSocTimer <= uiDiff)
        {
            if (Creature* dah = GetDalliah())
                DoScriptText(SAY_DH_SOC_DIE, dah);
            m_uiDieSocTimer = 0;
        }
        else
            m_uiDieSocTimer -= uiDiff;
    }

    if (m_uiAggroDahTimer)
    {
        if (m_uiAggroDahTimer <= uiDiff)
        {
            if (Creature* soc = GetSoccothrates())
                DoScriptText(SAY_SOC_DH_AGGRO, soc);
            m_uiAggroDahTimer = 0;
        }
        else
            m_uiAggroDahTimer -= uiDiff;
    }

    if (m_uiDieDahTimer)
    {
        if (m_uiDieDahTimer <= uiDiff)
        {
            if (Creature* soc = GetSoccothrates())
                DoScriptText(SAY_SOC_DH_DIE, soc);
            m_uiDieDahTimer = 0;
        }
        else
            m_uiDieDahTimer -= uiDiff;
    }
}

Creature* instance_arcatraz::GetSoccothrates()
{
    Creature* soc = GetSingleCreatureFromStorage(NPC_SOCCOTHRATES);
    return soc && soc->isAlive() ? soc : NULL;
}

Creature* instance_arcatraz::GetDalliah()
{
    Creature* dah = GetSingleCreatureFromStorage(NPC_DALLIAH);
    return dah && dah->isAlive() ? dah : NULL;
}

InstanceData* GetInstanceData_instance_arcatraz(Map* pMap)
{
    return new instance_arcatraz(pMap);
}

void AddSC_instance_arcatraz()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "instance_arcatraz";
    pNewScript->GetInstanceData = &GetInstanceData_instance_arcatraz;
    pNewScript->RegisterSelf();
}
