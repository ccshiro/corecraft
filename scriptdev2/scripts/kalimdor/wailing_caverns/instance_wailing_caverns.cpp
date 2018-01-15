/* Copyright (C) 2006 - 2012 ScriptDev2 <http://www.scriptdev2.com/>
 * Copyright (C) 2015 Corecraft <https://www.worldofcorecraft.com/>
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
SDName: Instance_Wailing_Caverns
SD%Complete: 100
SDComment:
SDCategory: Wailing Caverns
EndScriptData */

#include "precompiled.h"
#include "wailing_caverns.h"

instance_wailing_caverns::instance_wailing_caverns(Map* pMap)
  : ScriptedInstance(pMap)
{
    Initialize();
}

void instance_wailing_caverns::Initialize()
{
    memset(&m_auiEncounter, 0, sizeof(m_auiEncounter));
}

void instance_wailing_caverns::OnPlayerEnter(Player* pPlayer)
{
    // Respawn the Mysterious chest if one of the players who enter the instance
    // has the quest in his log
    if (pPlayer->GetQuestStatus(QUEST_FORTUNE_AWAITS) ==
            QUEST_STATUS_COMPLETE &&
        !pPlayer->GetQuestRewardStatus(QUEST_FORTUNE_AWAITS))
        DoRespawnGameObject(GO_MYSTERIOUS_CHEST, HOUR);
}

void instance_wailing_caverns::OnCreatureCreate(Creature* pCreature)
{
    switch (pCreature->GetEntry())
    {
    case NPC_DISCIPLE:
        m_mNpcEntryGuidStore[pCreature->GetEntry()] =
            pCreature->GetObjectGuid();
        break;
    case NPC_DRUID_OF_THE_FANG:
    {
        auto low = pCreature->GetGUIDLow();
        if (low == GUID_ANACONDRA0 || low == GUID_ANACONDRA1 ||
            low == GUID_ANACONDRA2)
        {
            fangs.push_back(pCreature->GetObjectGuid());
            if (fangs.size() == 3)
            {
                int index = urand(0, 2);
                anacondra_target = fangs[index];
                fangs.clear();
                if (auto npc = instance->GetAnyTypeCreature(fangs[index]))
                    npc->UpdateEntry(NPC_ANACONDRA);
            }
        }
        break;
    }
    }
}

void instance_wailing_caverns::OnCreatureDeath(Creature* creature)
{
    bool check = false;

    switch (creature->GetEntry())
    {
    case NPC_ANACONDRA:
        check = true;
        SetData(TYPE_ANACONDRA, DONE);
        break;
    case NPC_COBRAHN:
        check = true;
        SetData(TYPE_COBRAHN, DONE);
        break;
    case NPC_PYTHAS:
        check = true;
        SetData(TYPE_PYTHAS, DONE);
        break;
    case NPC_SERPENTIS:
        check = true;
        SetData(TYPE_SERPENTIS, DONE);
        break;
    case NPC_MUTANUS:
        SetData(TYPE_MUTANUS, DONE);
        break;
    }

    if (check && GetData(TYPE_ANACONDRA) == DONE &&
        GetData(TYPE_COBRAHN) == DONE && GetData(TYPE_PYTHAS) == DONE &&
        GetData(TYPE_SERPENTIS) == DONE)
    {
        if (auto disciple = GetSingleCreatureFromStorage(NPC_DISCIPLE))
            DoScriptText(SAY_LORDS_DEAD, disciple, nullptr);
    }
}

void instance_wailing_caverns::OnObjectCreate(GameObject* pGo)
{
    if (pGo->GetEntry() == GO_MYSTERIOUS_CHEST)
        m_mGoEntryGuidStore[GO_MYSTERIOUS_CHEST] = pGo->GetObjectGuid();
}

void instance_wailing_caverns::Update(uint32)
{
    if (anacondra_target)
        if (auto npc = instance->GetAnyTypeCreature(anacondra_target))
        {
            npc->SummonCreature(NPC_ANACONDRA, npc->GetX(), npc->GetY(),
                npc->GetZ(), npc->GetO(), TEMPSUMMON_CORPSE_TIMED_DESPAWN,
                300000);
            npc->ForcedDespawn();
            npc->SetRespawnTime(7 * DAY);
            anacondra_target.Clear();
        }
}

void instance_wailing_caverns::SetData(uint32 uiType, uint32 uiData)
{
    switch (uiType)
    {
    case TYPE_ANACONDRA:
        m_auiEncounter[0] = uiData;
        break;
    case TYPE_COBRAHN:
        m_auiEncounter[1] = uiData;
        break;
    case TYPE_PYTHAS:
        m_auiEncounter[2] = uiData;
        break;
    case TYPE_SERPENTIS:
        m_auiEncounter[3] = uiData;
        break;
    case TYPE_DISCIPLE:
        m_auiEncounter[4] = uiData;
        break;
    case TYPE_MUTANUS:
        m_auiEncounter[5] = uiData;
        break;
    }

    if (uiData == DONE)
    {
        OUT_SAVE_INST_DATA;

        std::ostringstream saveStream;

        saveStream << m_auiEncounter[0] << " " << m_auiEncounter[1] << " "
                   << m_auiEncounter[2] << " " << m_auiEncounter[3] << " "
                   << m_auiEncounter[4] << " " << m_auiEncounter[5];

        m_strInstData = saveStream.str();
        SaveToDB();
        OUT_SAVE_INST_DATA_COMPLETE;
    }
}

void instance_wailing_caverns::Load(const char* chrIn)
{
    if (!chrIn)
    {
        OUT_LOAD_INST_DATA_FAIL;
        return;
    }

    OUT_LOAD_INST_DATA(chrIn);

    std::istringstream loadStream(chrIn);
    loadStream >> m_auiEncounter[0] >> m_auiEncounter[1] >> m_auiEncounter[2] >>
        m_auiEncounter[3] >> m_auiEncounter[4] >> m_auiEncounter[5];

    for (auto& elem : m_auiEncounter)
    {
        if (elem == IN_PROGRESS)
            elem = NOT_STARTED;
    }

    OUT_LOAD_INST_DATA_COMPLETE;
}

uint32 instance_wailing_caverns::GetData(uint32 uiType)
{
    switch (uiType)
    {
    case TYPE_ANACONDRA:
        return m_auiEncounter[0];
        break;
    case TYPE_COBRAHN:
        return m_auiEncounter[1];
        break;
    case TYPE_PYTHAS:
        return m_auiEncounter[2];
        break;
    case TYPE_SERPENTIS:
        return m_auiEncounter[3];
        break;
    case TYPE_DISCIPLE:
        return m_auiEncounter[4];
        break;
    case TYPE_MUTANUS:
        return m_auiEncounter[5];
        break;
    }
    return 0;
}

InstanceData* GetInstanceData_instance_wailing_caverns(Map* pMap)
{
    return new instance_wailing_caverns(pMap);
}

void AddSC_instance_wailing_caverns()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "instance_wailing_caverns";
    pNewScript->GetInstanceData = &GetInstanceData_instance_wailing_caverns;
    pNewScript->RegisterSelf();
}
