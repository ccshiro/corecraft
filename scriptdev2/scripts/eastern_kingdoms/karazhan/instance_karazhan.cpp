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
SDName: Instance_Karazhan
SD%Complete: 100
SDComment:
SDCategory: Karazhan
EndScriptData */

#include "karazhan.h"
#include "precompiled.h"

/*
0  - Attumen + Midnight (optional)
1  - Moroes
2  - Maiden of Virtue (optional)
3  - Opera Event
4  - Curator
5  - Terestian Illhoof (optional)
6  - Shade of Aran (optional)
7  - Netherspite (optional)
8  - Chess Event
9  - Prince Malchezzar
10 - Nightbane
*/

instance_karazhan::instance_karazhan(Map* pMap)
  : ScriptedInstance(pMap), m_bHasWipedOnOpera(false), m_uiOzGrpId(0),
    m_playingTeam(TEAM_NONE), m_uiDeadAlliancePieces(0), m_uiDeadHordePieces(0),
    m_forceNewGrp(true)
{
    Initialize();
}

void instance_karazhan::Initialize()
{
    memset(&m_auiEncounter, 0, sizeof(m_auiEncounter));
    memset(&m_auiData, 0, sizeof(m_auiData));
}

bool instance_karazhan::IsEncounterInProgress() const
{
    for (auto& elem : m_auiEncounter)
        if (elem == IN_PROGRESS)
            return true;

    return false;
}

void instance_karazhan::Update(uint32 /*diff*/)
{
    if (GetData(TYPE_CHESS) == IN_PROGRESS)
    {
        for (auto& ref : instance->GetPlayers())
            if (auto player = ref.getSource())
                if (!player->has_aura(
                        SPELL_GAME_IN_SESSION, SPELL_AURA_MOD_PACIFY_SILENCE))
                    player->CastSpell(player, SPELL_GAME_IN_SESSION, true);
    }
}

void instance_karazhan::OnPlayerEnter(Player* player)
{
    if (GetData(TYPE_CHESS) != IN_PROGRESS)
        player->remove_auras(SPELL_GAME_IN_SESSION);
}

void instance_karazhan::OnCreatureCreate(Creature* pCreature)
{
    switch (pCreature->GetEntry())
    {
    case NPC_DOROTHEE:
    case NPC_TINHEAD:
    case NPC_STRAWMAN:
    case NPC_ROAR:
        if (m_forceNewGrp)
        {
            if (m_uiOzGrpId)
                instance->GetCreatureGroupMgr().DeleteGroup(m_uiOzGrpId);
            m_uiOzGrpId = instance->GetCreatureGroupMgr().CreateNewGroup(
                "Oz Group", true);
            m_forceNewGrp = false;
        }
    // No Break
    case NPC_MIDNIGHT:
    case NPC_ATTUMEN_THE_HUNTSMAN_FAKE:
    case NPC_ATTUMEN_THE_HUNTSMAN:
    case NPC_MOROES:
    case NPC_BARNES:
    case NPC_JULIANNE:
    case NPC_ROMULO:
    case NPC_MEDIVH:
    case NPC_NIGHTBANE:
    case NPC_NIGHTBANE_HELPER_TARGET:
    case NPC_INFERNAL_RELAY:
    case NPC_ENCOUNTER_SERVANTS:
    case NPC_ENCOUNTER_OPERA:
    case NPC_ENCOUNTER_CHESS:
        m_mNpcEntryGuidStore[pCreature->GetEntry()] =
            pCreature->GetObjectGuid();
        break;
    case 80001:
    {
        // Skip Medivh's spawns
        if (pCreature->GetOwner())
        {
            if (pCreature->GetOwner()->GetEntry() == NPC_MEDIVH)
                return;
        }

        m_EmptySpots.push_back(pCreature->GetObjectGuid());
    }
    }
}

void instance_karazhan::OnCreatureEvade(Creature* pCreature)
{
    switch (pCreature->GetEntry())
    {
    case NPC_DOROTHEE:
    case NPC_TINHEAD:
    case NPC_STRAWMAN:
    case NPC_ROAR:
        m_forceNewGrp = true;
        break;
    }
}

void instance_karazhan::OnCreatureDeath(Creature* pCreature)
{
    switch (pCreature->GetEntry())
    {
    case NPC_TITO:
        if (Creature* dorothee = GetSingleCreatureFromStorage(NPC_DOROTHEE))
            DoScriptText(SAY_DOROTHEE_TITO_DEATH, dorothee);
        break;
    }
}

void instance_karazhan::OnObjectCreate(GameObject* pGo)
{
    switch (pGo->GetEntry())
    {
    case GO_STAGE_CURTAIN:
        break;
    case GO_STAGE_DOOR_LEFT:
        if (GetData(TYPE_OPERA) == DONE)
            pGo->SetGoState(GO_STATE_ACTIVE);
        break;
    case GO_STAGE_DOOR_RIGHT:
        if (GetData(TYPE_OPERA) == DONE)
            pGo->SetGoState(GO_STATE_ACTIVE);
        break;
    case GO_PRIVATE_LIBRARY_DOOR:
        break;
    case GO_MASSIVE_DOOR:
        break;
    case GO_NETHERSPACE_DOOR:
        break;
    case GO_SIDE_ENTRANCE_DOOR:
        if (GetData(TYPE_OPERA) == DONE)
            pGo->RemoveFlag(GAMEOBJECT_FLAGS, GO_FLAG_LOCKED);
        break;
    case GO_SERVANTS_ACCESS_DOOR:
        if (GetData(TYPE_OPERA) == DONE)
            pGo->RemoveFlag(GAMEOBJECT_FLAGS, GO_FLAG_LOCKED);
        break;

    case GO_DUST_COVERED_CHEST:
        break;

    case GO_MASTERS_TERRACE_DOOR_ONE:
        break;
    case GO_MASTERS_TERRACE_DOOR_TWO:
        break;

    case GO_GAMESMANS_HALL_EXIT_DOOR:
        if (GetData(TYPE_CHESS) == DONE)
            pGo->SetGoState(GO_STATE_ACTIVE);
        break;
    case GO_GAMESMANS_HALL_DOOR:
        break;

    case GO_OZ_BACKDROP:
        m_OZBackdrop = pGo->GetObjectGuid();
        break;
    case GO_OZ_HAY:
        m_OperaHay.push_back(pGo->GetObjectGuid());
        break;
    case GO_HOOD_BACKDROP:
        m_HOODBackdrop = pGo->GetObjectGuid();
        break;
    case GO_HOOD_TREE:
        m_OperaTrees.push_back(pGo->GetObjectGuid());
        break;
    case GO_HOOD_HOUSE:
        m_OperaHouses.push_back(pGo->GetObjectGuid());
        break;
    case GO_RAJ_BACKDROP:
        m_RAJBackdrop = pGo->GetObjectGuid();
        break;
    case GO_RAJ_MOON:
        m_OperaMoon.push_back(pGo->GetObjectGuid());
        break;
    case GO_RAJ_BALCONY:
        m_OperaBalcony.push_back(pGo->GetObjectGuid());
        break;

    default:
        return;
    }
    m_mGoEntryGuidStore[pGo->GetEntry()] = pGo->GetObjectGuid();
}

void instance_karazhan::SetData(uint32 uiType, uint32 uiData)
{
    switch (uiType)
    {
    case TYPE_SERVANTS:
        m_auiEncounter[uiType] = uiData;
        if (uiData == IN_PROGRESS)
        {
            if (Creature* encounter =
                    GetSingleCreatureFromStorage(NPC_ENCOUNTER_SERVANTS))
            {
                switch (urand(0, 2))
                {
                case 0:
                    encounter->SummonCreature(NPC_HYAKISS, -10994.8f, -2012.1f,
                        46.0f, 1.1f, TEMPSUMMON_CORPSE_TIMED_DESPAWN,
                        sWorld::Instance()->getConfig(
                            CONFIG_UINT32_CORPSE_DECAY_WORLDBOSS) *
                            1000);
                    break;
                case 1:
                    encounter->SummonCreature(NPC_SHADIKITH, -10933.2f,
                        -2045.8f, 49.5f, 1.4f, TEMPSUMMON_CORPSE_TIMED_DESPAWN,
                        sWorld::Instance()->getConfig(
                            CONFIG_UINT32_CORPSE_DECAY_WORLDBOSS) *
                            1000);
                    break;
                case 2:
                    encounter->SummonCreature(NPC_ROKAD, -10932.7f, -2039.6f,
                        49.5f, 1.4f, TEMPSUMMON_CORPSE_TIMED_DESPAWN,
                        sWorld::Instance()->getConfig(
                            CONFIG_UINT32_CORPSE_DECAY_WORLDBOSS) *
                            1000);
                    break;
                }
            }
        }
        else if (uiData == DONE)
        {
            // Kill encounter mob
            if (Creature* encounter =
                    GetSingleCreatureFromStorage(NPC_ENCOUNTER_SERVANTS))
                encounter->Kill(encounter);
        }
        break;
    case TYPE_ATTUMEN:
        m_auiEncounter[uiType] = uiData;
        if (uiData == FAIL)
        {
            if (Creature* midnight = GetSingleCreatureFromStorage(NPC_MIDNIGHT))
            {
                midnight->remove_auras();
                midnight->Respawn();
            }
            if (Creature* attumen =
                    GetSingleCreatureFromStorage(NPC_ATTUMEN_THE_HUNTSMAN_FAKE))
                attumen->ForcedDespawn();
            if (Creature* attumen =
                    GetSingleCreatureFromStorage(NPC_ATTUMEN_THE_HUNTSMAN))
                attumen->ForcedDespawn();
        }
        else if (uiData == DONE)
        {
            if (Creature* midnight = GetSingleCreatureFromStorage(NPC_MIDNIGHT))
                midnight->Kill(
                    midnight); // We kill midnight to stop the spawning of trash
        }
        break;
    case TYPE_MOROES:
        m_auiEncounter[uiType] = uiData;
        // Choose a random opera event when moroes dies
        if (uiData == DONE && !GetData(DATA_OPERA_EVENT))
            SetData(DATA_OPERA_EVENT, urand(EVENT_OZ, EVENT_RAJ));
        break;
    case TYPE_MAIDEN:
        m_auiEncounter[uiType] = uiData;
        break;
    case TYPE_OPERA:
        m_auiEncounter[uiType] = uiData;
        if (uiData == DONE)
        {
            DoUseDoorOrButton(GO_STAGE_DOOR_LEFT);
            DoUseDoorOrButton(GO_STAGE_DOOR_RIGHT);
            // Kill encounter mob
            if (Creature* encounter =
                    GetSingleCreatureFromStorage(NPC_ENCOUNTER_OPERA))
                encounter->Kill(encounter);
        }
        else if (uiData == FAIL)
        {
            m_bHasWipedOnOpera = true;

            if (GameObject* pLeftDoor =
                    GetSingleGameObjectFromStorage(GO_STAGE_DOOR_LEFT))
                pLeftDoor->SetGoState(GO_STATE_READY);
            if (GameObject* pRightDoor =
                    GetSingleGameObjectFromStorage(GO_STAGE_DOOR_RIGHT))
                pRightDoor->SetGoState(GO_STATE_READY);
            if (GameObject* pCurtain =
                    GetSingleGameObjectFromStorage(GO_STAGE_CURTAIN))
                pCurtain->SetGoState(GO_STATE_READY);
        }
        break;
    case TYPE_CURATOR:
        m_auiEncounter[uiType] = uiData;
        break;
    case TYPE_TERESTIAN:
        m_auiEncounter[uiType] = uiData;
        break;
    case TYPE_ARAN:
        if (uiData == IN_PROGRESS)
        {
            if (GameObject* pDoor =
                    GetSingleGameObjectFromStorage(GO_PRIVATE_LIBRARY_DOOR))
                pDoor->SetFlag(GAMEOBJECT_FLAGS, GO_FLAG_NO_INTERACT);
        }
        else
        {
            if (GameObject* pDoor =
                    GetSingleGameObjectFromStorage(GO_PRIVATE_LIBRARY_DOOR))
                pDoor->RemoveFlag(GAMEOBJECT_FLAGS, GO_FLAG_NO_INTERACT);
        }

        m_auiEncounter[uiType] = uiData;
        break;
    case TYPE_NETHERSPITE:
        if (uiData == IN_PROGRESS)
        {
            if (GameObject* pGo =
                    GetSingleGameObjectFromStorage(GO_MASSIVE_DOOR))
            {
                pGo->SetGoState(GO_STATE_READY);
            }
        }
        else
        {
            if (GameObject* pGo =
                    GetSingleGameObjectFromStorage(GO_MASSIVE_DOOR))
            {
                pGo->SetGoState(GO_STATE_ACTIVE);
            }
        }

        m_auiEncounter[uiType] = uiData;

        break;
    case TYPE_CHESS:
        if (uiData == DONE)
        {
            DoRespawnGameObject(GO_DUST_COVERED_CHEST, DAY);
            if (GameObject* pGo =
                    GetSingleGameObjectFromStorage(GO_GAMESMANS_HALL_EXIT_DOOR))
            {
                pGo->SetGoState(GO_STATE_ACTIVE);
            }
            // Kill encounter mob
            if (Creature* encounter =
                    GetSingleCreatureFromStorage(NPC_ENCOUNTER_CHESS))
                encounter->Kill(encounter);
        }
        else if (uiData == IN_PROGRESS)
        {
            if (GameObject* pGo =
                    GetSingleGameObjectFromStorage(GO_GAMESMANS_HALL_DOOR))
            {
                pGo->SetGoState(GO_STATE_READY);
                pGo->SetFlag(GAMEOBJECT_FLAGS, GO_FLAG_INTERACT_COND);
            }
        }

        if (uiData != IN_PROGRESS)
        {
            if (GameObject* pGo =
                    GetSingleGameObjectFromStorage(GO_GAMESMANS_HALL_DOOR))
            {
                pGo->SetGoState(GO_STATE_READY);
                pGo->RemoveFlag(GAMEOBJECT_FLAGS, GO_FLAG_INTERACT_COND);
            }

            for (auto& ref : instance->GetPlayers())
                if (auto player = ref.getSource())
                    player->remove_auras(SPELL_GAME_IN_SESSION);
        }

        m_auiEncounter[uiType] = uiData;
        break;
    case TYPE_MALCHEZAAR:
        if (uiData == IN_PROGRESS)
        {
            if (GameObject* pGo =
                    GetSingleGameObjectFromStorage(GO_NETHERSPACE_DOOR))
                pGo->SetGoState(GO_STATE_READY);
        }
        else
        {
            if (GameObject* pGo =
                    GetSingleGameObjectFromStorage(GO_NETHERSPACE_DOOR))
                pGo->SetGoState(GO_STATE_ACTIVE);
        }

        m_auiEncounter[uiType] = uiData;
        break;
    case TYPE_NIGHTBANE:
        if (uiData == IN_PROGRESS)
        {
            if (GameObject* pGo =
                    GetSingleGameObjectFromStorage(GO_MASTERS_TERRACE_DOOR_ONE))
                pGo->SetGoState(GO_STATE_READY);
            if (GameObject* pGo =
                    GetSingleGameObjectFromStorage(GO_MASTERS_TERRACE_DOOR_TWO))
                pGo->SetGoState(GO_STATE_READY);
        }
        else
        {
            if (GameObject* pGo =
                    GetSingleGameObjectFromStorage(GO_MASTERS_TERRACE_DOOR_ONE))
                pGo->SetGoState(GO_STATE_ACTIVE);
            if (GameObject* pGo =
                    GetSingleGameObjectFromStorage(GO_MASTERS_TERRACE_DOOR_TWO))
                pGo->SetGoState(GO_STATE_ACTIVE);
        }
        m_auiEncounter[uiType] = uiData;
        break;

    case DATA_OPERA_EVENT:
        m_auiData[uiType - MAX_ENCOUNTER] = uiData;
        break;

    case DATA_OPERA_OZ_COUNT:
        if (uiData == 0)
            if (Creature* barnes = GetSingleCreatureFromStorage(NPC_BARNES))
                barnes->SummonCreature(NPC_CRONE, -10893.8f, -1758.7f, 90.5f,
                    4.6f, TEMPSUMMON_CORPSE_TIMED_DESPAWN,
                    sWorld::Instance()->getConfig(
                        CONFIG_UINT32_CORPSE_DECAY_WORLDBOSS) *
                        1000);
        m_auiData[uiType - MAX_ENCOUNTER] = uiData;
        break;

    case DATA_CHESS_PLAYING_TEAM:
        m_auiData[uiType - MAX_ENCOUNTER] = uiData;
        break;

    case DATA_CHESS_DEAD_ALLIANCE_PIECES:
        if (uiData == DEAD_PIECE_INCREASE)
            m_auiData[uiType - MAX_ENCOUNTER] += 1;
        else
            m_auiData[uiType - MAX_ENCOUNTER] = uiData;
        break;

    case DATA_CHESS_DEAD_HORDE_PIECES:
        if (uiData == DEAD_PIECE_INCREASE)
            m_auiData[uiType - MAX_ENCOUNTER] += 1;
        else
            m_auiData[uiType - MAX_ENCOUNTER] = uiData;
        break;

    case DATA_SERVANTS_COUNT:
        if (uiData == SPECIAL) // On spawn
            m_auiData[uiType - MAX_ENCOUNTER] += 1;
        else if (uiData == DONE) // On kill
            m_auiData[uiType - MAX_ENCOUNTER] -= 1;
        if (m_auiData[uiType - MAX_ENCOUNTER] == 0 &&
            GetData(TYPE_SERVANTS) != DONE)
            SetData(TYPE_SERVANTS, IN_PROGRESS); // Spawn a boss
        break;
    }

    if ((uiData == DONE && uiType < MAX_ENCOUNTER) ||
        uiType == DATA_OPERA_EVENT) // Some data should be DB-saved as well
    {
        OUT_SAVE_INST_DATA;

        std::ostringstream saveStream;
        saveStream << m_auiEncounter[0] << " " << m_auiEncounter[1] << " "
                   << m_auiEncounter[2] << " " << m_auiEncounter[3] << " "
                   << m_auiEncounter[4] << " " << m_auiEncounter[5] << " "
                   << m_auiEncounter[6] << " " << m_auiEncounter[7] << " "
                   << m_auiEncounter[8] << " " << m_auiEncounter[9] << " "
                   << m_auiEncounter[10] << " " << m_auiEncounter[11] << " "
                   << m_auiData[0]; // Encounter data

        m_strInstData = saveStream.str();

        SaveToDB();
        OUT_SAVE_INST_DATA_COMPLETE;
    }
}

uint32 instance_karazhan::GetData(uint32 uiType)
{
    switch (uiType)
    {
    case TYPE_SERVANTS:
        return m_auiEncounter[uiType];
    case TYPE_ATTUMEN:
        return m_auiEncounter[uiType];
    case TYPE_MOROES:
        return m_auiEncounter[uiType];
    case TYPE_MAIDEN:
        return m_auiEncounter[uiType];
    case TYPE_OPERA:
        return m_auiEncounter[uiType];
    case TYPE_CURATOR:
        return m_auiEncounter[uiType];
    case TYPE_TERESTIAN:
        return m_auiEncounter[uiType];
    case TYPE_ARAN:
        return m_auiEncounter[uiType];
    case TYPE_NETHERSPITE:
        return m_auiEncounter[uiType];
    case TYPE_CHESS:
        return m_auiEncounter[uiType];
    case TYPE_MALCHEZAAR:
        return m_auiEncounter[uiType];
    case TYPE_NIGHTBANE:
        return m_auiEncounter[uiType];

    case DATA_OPERA_EVENT:
        return m_auiData[uiType - MAX_ENCOUNTER];
    case DATA_OPERA_OZ_COUNT:
        return m_auiData[uiType - MAX_ENCOUNTER];
    case DATA_CHESS_PLAYING_TEAM:
        return m_auiData[uiType - MAX_ENCOUNTER];
    case DATA_CHESS_DEAD_ALLIANCE_PIECES:
        return m_auiData[uiType - MAX_ENCOUNTER];
    case DATA_CHESS_DEAD_HORDE_PIECES:
        return m_auiData[uiType - MAX_ENCOUNTER];
    case DATA_SERVANTS_COUNT:
        return m_auiData[uiType - MAX_ENCOUNTER];

    default:
        return 0;
    }
}

void instance_karazhan::SetData64(uint32 uiType, uint64 uiData)
{
    uint32 e = uiType;
    if (e == 21684 || e == 21683 || e == 21682 || e == 21664 || e == 21160 ||
        e == 17211 || e == 21752 || e == 21750 || e == 21747 || e == 21748 ||
        e == 21726 || e == 17469)
    {
        std::map<uint32, uint64>::iterator itr =
            m_chessControllers.find(uiType);
        if (itr == m_chessControllers.end())
            m_chessControllers.insert(
                std::pair<uint32, uint64>(uiType, uiData));
        else
            itr->second = uiData;
    }
}

uint64 instance_karazhan::GetData64(uint32 uiType)
{
    uint32 e = uiType;
    if (e == 21684 || e == 21683 || e == 21682 || e == 21664 || e == 21160 ||
        e == 17211 || e == 21752 || e == 21750 || e == 21747 || e == 21748 ||
        e == 21726 || e == 17469)
    {
        std::map<uint32, uint64>::iterator itr =
            m_chessControllers.find(uiType);
        if (itr != m_chessControllers.end())
            return itr->second;
    }

    return 0;
}

void instance_karazhan::Load(const char* chrIn)
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
        m_auiEncounter[6] >> m_auiEncounter[7] >> m_auiEncounter[8] >>
        m_auiEncounter[9] >> m_auiEncounter[10] >> m_auiEncounter[11] >>
        m_auiData[0]; // Encounter Data

    for (auto& elem : m_auiEncounter)
        if (elem == IN_PROGRESS) // Do not load an encounter as "In
                                 // Progress" - reset it instead.
            elem = NOT_STARTED;

    OUT_LOAD_INST_DATA_COMPLETE;
}

InstanceData* GetInstanceData_instance_karazhan(Map* pMap)
{
    return new instance_karazhan(pMap);
}

void AddSC_instance_karazhan()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "instance_karazhan";
    pNewScript->GetInstanceData = &GetInstanceData_instance_karazhan;
    pNewScript->RegisterSelf();
}
