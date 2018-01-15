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
SDName: Instance_Old_Hillsbrad
SD%Complete: 100
SDComment:
SDCategory: Caverns of Time, Old Hillsbrad Foothills
EndScriptData */

#include "old_hillsbrad.h"
#include "precompiled.h"

instance_old_hillsbrad::instance_old_hillsbrad(Map* pMap)
  : ScriptedInstance(pMap), m_uiBarrelCount(0), m_uiRoaringFlames(0)
{
    Initialize();
}

void instance_old_hillsbrad::Initialize()
{
    memset(&m_auiEncounter, 0, sizeof(m_auiEncounter));
}

void instance_old_hillsbrad::OnCreatureCreate(Creature* pCreature)
{
    switch (pCreature->GetEntry())
    {
    case NPC_THRALL:
        // Move thrall to his check point by respawning him soon
        pCreature->ForcedDespawn();
    case NPC_TARETHA:
    case NPC_EPOCH:
    case NPC_ARMORER:
    case NPC_SKARLOC_MOUNT:
        m_mNpcEntryGuidStore[pCreature->GetEntry()] =
            pCreature->GetObjectGuid();
        break;

    case NPC_DURNHOLDE_LOOKOUT:
        m_lookouts.push_back(pCreature->GetObjectGuid());
        break;
    }
}

void instance_old_hillsbrad::OnCreatureDeath(Creature* pCreature)
{
    if (pCreature->GetEntry() == NPC_EPOCH)
    {
        // notify thrall so he can continue
        if (Creature* pThrall = GetSingleCreatureFromStorage(NPC_THRALL))
            pThrall->AI()->KilledUnit(pCreature);
    }
}

void instance_old_hillsbrad::OnObjectCreate(GameObject* pObject)
{
    switch (pObject->GetEntry())
    {
    case GO_TH_PRISON_DOOR:
        m_mGoEntryGuidStore[pObject->GetEntry()] = pObject->GetObjectGuid();
        break;

    case GO_ROARING_FLAME:
        m_roaringFlames.push_back(pObject->GetObjectGuid());
        break;
    }
}

void instance_old_hillsbrad::OnCreatureEnterCombat(Creature* pCreature)
{
    // Aggro all nearby temporary summons as well
    if (pCreature->IsTemporarySummon() && pCreature->getVictim())
    {
        auto creatures = GetFriendlyCreatureListInGrid(pCreature, 20.0f);
        for (auto& creature : creatures)
        {
            if ((creature)->IsTemporarySummon() && (creature)->AI() &&
                (creature)->CanAssist(pCreature, pCreature->getVictim(), false))
                (creature)->AI()->AttackStart(pCreature->getVictim());
        }
    }
}

void instance_old_hillsbrad::ResetThrallEvent()
{
    if (Creature* armorer = GetSingleCreatureFromStorage(NPC_ARMORER))
        if (!armorer->isAlive())
            armorer->Respawn();

    if (GameObject* prisonDoor =
            GetSingleGameObjectFromStorage(GO_TH_PRISON_DOOR))
        prisonDoor->ResetDoorOrButton();

    if (Creature* taretha = GetTaretha())
    {
        taretha->remove_auras();
        taretha->SetStandState(UNIT_STAND_STATE_STAND);
        // Decide taretha's gossip status
        if (GetData(TYPE_THRALL_PART3) == DONE)
        {
            taretha->SetFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP);
            taretha->CastSpell(taretha, SPELL_SHADOW_PRISON, true);
        }
        else
            taretha->RemoveFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP);
    }
}

void instance_old_hillsbrad::SetData(uint32 uiType, uint32 uiData)
{
    switch (uiType)
    {
    case TYPE_BARREL_DIVERSION:
    {
        if (GetData(TYPE_BARREL_DIVERSION) == DONE)
            return;

        if (uiData == SPECIAL)
        {
            LOG_DEBUG(logging,
                "SD2: Instance Old Hillsbrad: go_barrel_old_hillsbrad count %u",
                m_uiBarrelCount);

            ++m_uiBarrelCount;
            DoUpdateWorldState(WORLD_STATE_OH, m_uiBarrelCount);
            m_auiEncounter[TYPE_BARREL_DIVERSION] = IN_PROGRESS;

            // DONE:
            if (m_uiBarrelCount >= 5)
            {
                m_uiRoaringFlames = 5000;
                m_auiEncounter[TYPE_BARREL_DIVERSION] = DONE;

                // Remove all lookouts & their groups on heroic difficulty
                if (!instance->IsRegularDifficulty())
                {
                    for (auto& elem : m_lookouts)
                    {
                        if (Creature* look = instance->GetCreature(elem))
                        {
                            // Remove all members of the group; and make them
                            // not respawn
                            if (CreatureGroup* grp = look->GetGroup())
                            {
                                for (auto& member : grp->GetMembers())
                                {
                                    (member)->SetRespawnDelay(
                                        48 *
                                        HOUR); // Longer than instance reset
                                    if ((member)->isAlive())
                                    {
                                        (member)->SetVisibility(VISIBILITY_OFF);
                                        (member)->Kill(member, false);
                                    }
                                    else
                                    {
                                        (member)->SetRespawnTime(48 * HOUR);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        break;
    }
    case TYPE_THRALL_EVENT:
    {
        // nothing to do if already done and thrall respawn
        if (m_auiEncounter[TYPE_THRALL_EVENT] == DONE)
            return;

        if (uiData == FAIL)
        {
            // You only get 3 tries pre 2.2 (upped to 20 in 2.2):
            if (GetData(TYPE_THRALL_DEATHS) <= 3)
            {
                ResetThrallEvent();
                m_auiEncounter[TYPE_THRALL_EVENT] = 0;

                if (m_auiEncounter[TYPE_THRALL_PART1] != DONE)
                    m_auiEncounter[TYPE_THRALL_PART1] = 0;
                else
                    m_auiEncounter[TYPE_THRALL_EVENT] =
                        IN_PROGRESS; // Thrall is in progress if we've gotten
                                     // past the basement -> skarloc route

                if (m_auiEncounter[TYPE_THRALL_PART2] != DONE)
                    m_auiEncounter[TYPE_THRALL_PART2] = 0;
                if (m_auiEncounter[TYPE_THRALL_PART3] != DONE)
                    m_auiEncounter[TYPE_THRALL_PART3] = 0;
                if (m_auiEncounter[TYPE_THRALL_PART4] != DONE)
                    m_auiEncounter[TYPE_THRALL_PART4] = 0;
                if (m_auiEncounter[TYPE_THRALL_PART5] != DONE)
                    m_auiEncounter[TYPE_THRALL_PART5] = 0;
            }
            else
                m_auiEncounter[TYPE_THRALL_EVENT] = FAIL;
        }
        else
            m_auiEncounter[TYPE_THRALL_EVENT] = uiData;

        LOG_DEBUG(logging,
            "SD2: Instance Old Hillsbrad: Thrall escort event adjusted to data "
            "%u.",
            uiData);
        break;
    }
    case TYPE_THRALL_PART1:
        m_auiEncounter[TYPE_THRALL_PART1] = uiData;
        if (uiData == DONE)
        {
            if (Creature* thrall = GetSingleCreatureFromStorage(NPC_THRALL))
                thrall->SetFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP);
        }
        LOG_DEBUG(logging,
            "SD2: Instance Old Hillsbrad: Thrall event part I adjusted to data "
            "%u.",
            uiData);
        break;
    case TYPE_THRALL_PART2:
        m_auiEncounter[TYPE_THRALL_PART2] = uiData;
        LOG_DEBUG(logging,
            "SD2: Instance Old Hillsbrad: Thrall event part II adjusted to "
            "data %u.",
            uiData);
        break;
    case TYPE_THRALL_PART3:
        m_auiEncounter[TYPE_THRALL_PART3] = uiData;
        LOG_DEBUG(logging,
            "SD2: Instance Old Hillsbrad: Thrall event part III adjusted to "
            "data %u.",
            uiData);
        break;
    case TYPE_THRALL_PART4:
        m_auiEncounter[TYPE_THRALL_PART4] = uiData;
        LOG_DEBUG(logging,
            "SD2: Instance Old Hillsbrad: Thrall event part IV adjusted to "
            "data %u.",
            uiData);
        break;
    case TYPE_THRALL_PART5:
        m_auiEncounter[TYPE_THRALL_PART5] = uiData;
        break;
    case TYPE_THRALL_DEATHS:
        m_auiEncounter[TYPE_THRALL_DEATHS] = uiData;
        break;
    }

    if (uiData == DONE || uiType == TYPE_THRALL_DEATHS)
    {
        OUT_SAVE_INST_DATA;

        std::ostringstream saveStream;
        saveStream << m_auiEncounter[0] << " " << m_auiEncounter[1] << " "
                   << m_auiEncounter[2] << " " << m_auiEncounter[3] << " "
                   << m_auiEncounter[4] << " " << m_auiEncounter[5] << " "
                   << m_auiEncounter[6] << " " << m_auiEncounter[7];

        m_strInstData = saveStream.str();

        SaveToDB();
        OUT_SAVE_INST_DATA_COMPLETE;
    }
}

void instance_old_hillsbrad::Load(const char* chrIn)
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
        m_auiEncounter[6] >> m_auiEncounter[7];

    for (auto& elem : m_auiEncounter)
        if (elem == IN_PROGRESS || elem == FAIL)
            elem = NOT_STARTED;

    OUT_LOAD_INST_DATA_COMPLETE;
}

void instance_old_hillsbrad::Update(const uint32 uiDiff)
{
    if (m_uiRoaringFlames)
    {
        if (m_uiRoaringFlames <= uiDiff)
        {
            UpdateLodgeQuestCredit();
            DoUpdateWorldState(WORLD_STATE_OH, 0);

            if (Creature* pThrall = GetThrall())
                pThrall->SummonCreature(NPC_DRAKE, 2132.9f, 73.1f, 64.7f, 3.7f,
                    TEMPSUMMON_CORPSE_TIMED_DESPAWN, 30 * 60 * 1000);

            for (auto& elem : m_roaringFlames)
            {
                if (GameObject* pGo = instance->GetGameObject(elem))
                {
                    pGo->SetRespawnTime(HOUR);
                    pGo->Refresh();
                }
            }
            m_uiRoaringFlames = 0;
        }
        else
            m_uiRoaringFlames -= uiDiff;
    }
}

uint32 instance_old_hillsbrad::GetData(uint32 uiData)
{
    switch (uiData)
    {
    case TYPE_BARREL_DIVERSION:
        return m_auiEncounter[TYPE_BARREL_DIVERSION];
    case TYPE_THRALL_EVENT:
        return m_auiEncounter[TYPE_THRALL_EVENT];
    case TYPE_THRALL_PART1:
        return m_auiEncounter[TYPE_THRALL_PART1];
    case TYPE_THRALL_PART2:
        return m_auiEncounter[TYPE_THRALL_PART2];
    case TYPE_THRALL_PART3:
        return m_auiEncounter[TYPE_THRALL_PART3];
    case TYPE_THRALL_PART4:
        return m_auiEncounter[TYPE_THRALL_PART4];
    case TYPE_THRALL_PART5:
        return m_auiEncounter[TYPE_THRALL_PART5];
    case TYPE_THRALL_DEATHS:
        return m_auiEncounter[TYPE_THRALL_DEATHS];
    default:
        return 0;
    }
}

void instance_old_hillsbrad::UpdateLodgeQuestCredit()
{
    Map::PlayerList const& players = instance->GetPlayers();

    if (!players.isEmpty())
    {
        for (const auto& player : players)
        {
            if (Player* pPlayer = player.getSource())
                pPlayer->KilledMonsterCredit(NPC_LODGE_QUEST_TRIGGER);
        }
    }
}

InstanceData* GetInstanceData_instance_old_hillsbrad(Map* pMap)
{
    return new instance_old_hillsbrad(pMap);
}

void AddSC_instance_old_hillsbrad()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "instance_old_hillsbrad";
    pNewScript->GetInstanceData = &GetInstanceData_instance_old_hillsbrad;
    pNewScript->RegisterSelf();
}
