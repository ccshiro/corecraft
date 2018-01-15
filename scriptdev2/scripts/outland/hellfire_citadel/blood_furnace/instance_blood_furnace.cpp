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
SDName: Instance_Blood_Furnace
SD%Complete: 100
SDComment:
SDCategory: Blood Furnace
EndScriptData */

#include "blood_furnace.h"

instance_blood_furnace::instance_blood_furnace(Map* pMap)
  : ScriptedInstance(pMap), m_uiBroggokEventTimer(90000),
    m_uiBroggokEventPhase(0), m_bHasFixedOrcsForBroggok(false),
    m_uiBroggokFixTimer(5000)
{
    Initialize();
}

void instance_blood_furnace::Initialize()
{
    memset(&m_auiEncounter, 0, sizeof(m_auiEncounter));
}

void instance_blood_furnace::OnCreatureCreate(Creature* pCreature)
{
    switch (pCreature->GetEntry())
    {
    case NPC_BROGGOK:
    case NPC_KELIDAN_THE_BREAKER:
        m_mNpcEntryGuidStore[pCreature->GetEntry()] =
            pCreature->GetObjectGuid();
        break;

    case NPC_NASCENT_FEL_ORC:
    case NPC_FEL_ORC_NEOPHYTE:
        m_luiNascentOrcGuids.push_back(pCreature->GetObjectGuid());
        break;
    }
}

void instance_blood_furnace::OnObjectCreate(GameObject* pGo)
{
    switch (pGo->GetEntry())
    {
    case GO_DOOR_MAKER_FRONT: // the maker front door
        break;
    case GO_DOOR_MAKER_REAR: // the maker rear door
        if (m_auiEncounter[TYPE_THE_MAKER_EVENT] == DONE)
            pGo->SetGoState(GO_STATE_ACTIVE);
        break;
    case GO_DOOR_BROGGOK_FRONT: // broggok front door
        break;
    case GO_DOOR_BROGGOK_REAR: // broggok rear door
        if (m_auiEncounter[TYPE_BROGGOK_EVENT] == DONE)
            pGo->SetGoState(GO_STATE_ACTIVE);
        break;
    case GO_DOOR_KELIDAN_EXIT: // kelidan exit door
        if (m_auiEncounter[TYPE_KELIDAN_EVENT] == DONE)
            pGo->SetGoState(GO_STATE_ACTIVE);
        break;
    case GO_DOOR_FINAL_EXIT: // final exit door
        if (m_auiEncounter[TYPE_KELIDAN_EVENT] == DONE)
            pGo->SetGoState(GO_STATE_ACTIVE);
        break;
    case GO_CELL_DOOR_LEVER:
        break;

    case GO_PRISON_CELL_BROGGOK_1:
        m_aBroggokEvent[0].m_cellGuid = pGo->GetObjectGuid();
        return;
    case GO_PRISON_CELL_BROGGOK_2:
        m_aBroggokEvent[1].m_cellGuid = pGo->GetObjectGuid();
        return;
    case GO_PRISON_CELL_BROGGOK_3:
        m_aBroggokEvent[2].m_cellGuid = pGo->GetObjectGuid();
        return;
    case GO_PRISON_CELL_BROGGOK_4:
        m_aBroggokEvent[3].m_cellGuid = pGo->GetObjectGuid();
        return;

    default:
        return;
    }
    m_mGoEntryGuidStore[pGo->GetEntry()] = pGo->GetObjectGuid();
}

void instance_blood_furnace::SetData(uint32 uiType, uint32 uiData)
{
    switch (uiType)
    {
    case TYPE_THE_MAKER_EVENT:
        if (uiData == IN_PROGRESS)
            DoUseDoorOrButton(GO_DOOR_MAKER_FRONT);
        if (uiData == FAIL)
            DoUseDoorOrButton(GO_DOOR_MAKER_FRONT);
        if (uiData == DONE)
        {
            DoUseDoorOrButton(GO_DOOR_MAKER_FRONT);
            DoUseDoorOrButton(GO_DOOR_MAKER_REAR);
        }
        m_auiEncounter[uiType] = uiData;
        break;
    case TYPE_BROGGOK_EVENT:
        if (m_auiEncounter[uiType] == uiData)
            return;
        m_auiEncounter[uiType] = uiData; // needs to be set before logic, to
                                         // avoid recursive calls and inifinite
                                         // loop

        // Combat door; the exit door is opened in event
        DoUseDoorOrButton(GO_DOOR_BROGGOK_FRONT);
        if (uiData == IN_PROGRESS)
        {
            if (m_uiBroggokEventPhase <= MAX_ORC_WAVES)
            {
                m_uiBroggokEventPhase = 0;
                DoSortBroggokOrcs();
                // open first cage
                DoNextBroggokEventPhase();
            }
        }
        else if (uiData == FAIL)
        {
            // On wipe we reset only the orcs; if the party wipes at the boss
            // itself then the orcs don't reset
            if (m_uiBroggokEventPhase <= MAX_ORC_WAVES)
            {
                for (auto& elem : m_aBroggokEvent)
                {
                    // Reset Orcs
                    if (!elem.m_bIsCellOpened)
                        continue;

                    elem.m_uiKilledOrcCount = 0;
                    for (const auto& _itr : elem.m_sSortedOrcGuids)
                    {
                        if (Creature* pOrc = instance->GetCreature(_itr))
                        {
                            if (!pOrc->isAlive())
                                pOrc->Respawn();
                            else
                                pOrc->AI()->EnterEvadeMode(); // Ensure they
                                                              // really evade

                            pOrc->SetFlag(
                                UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE |
                                                      UNIT_FLAG_NON_ATTACKABLE |
                                                      UNIT_FLAG_PASSIVE);
                            pOrc->SetAggroDistance(1.0f);
                        }
                    }

                    // Close Door
                    DoUseDoorOrButton(elem.m_cellGuid);
                    elem.m_bIsCellOpened = false;
                }

                // Reset Lever on Orc Phase Wipe
                if (GameObject* pLever =
                        GetSingleGameObjectFromStorage(GO_CELL_DOOR_LEVER))
                    pLever->ResetDoorOrButton();
            }
        }
        break;
    case TYPE_KELIDAN_EVENT:
        if (uiData == DONE)
        {
            DoUseDoorOrButton(GO_DOOR_KELIDAN_EXIT);
            DoUseDoorOrButton(GO_DOOR_FINAL_EXIT);
        }
        m_auiEncounter[uiType] = uiData;
        break;
    default:
        logging.error(
            "SD2: Instance Blood Furnace SetData with Type %u Data %u, but "
            "this is not implemented.",
            uiType, uiData);
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

void instance_blood_furnace::DoNextBroggokEventPhase()
{
    // Get Movement Position
    float dx, dy;
    GetMovementDistanceForIndex(m_uiBroggokEventPhase, dx, dy);

    // Open door to the final boss now and move boss to the center of the room
    if (m_uiBroggokEventPhase >= MAX_ORC_WAVES)
    {
        DoUseDoorOrButton(GO_DOOR_BROGGOK_REAR);

        if (Creature* pBroggok = GetSingleCreatureFromStorage(NPC_BROGGOK))
        {
            pBroggok->movement_gens.push(
                new movement::PointMovementGenerator(
                    0, dx, dy, pBroggok->GetZ(), false, true),
                movement::EVENT_ENTER_COMBAT);
            pBroggok->movement_gens.remove_all(movement::gen::idle);
            pBroggok->movement_gens.push(new movement::IdleMovementGenerator(
                456.75f, 99.551f, 9.630f, 1.591f));
            pBroggok->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE);
        }
    }
    else
    {
        // Open cage door
        if (!m_aBroggokEvent[m_uiBroggokEventPhase].m_bIsCellOpened)
            DoUseDoorOrButton(
                m_aBroggokEvent[m_uiBroggokEventPhase].m_cellGuid);

        m_aBroggokEvent[m_uiBroggokEventPhase].m_bIsCellOpened = true;

        for (const auto& elem :
            m_aBroggokEvent[m_uiBroggokEventPhase].m_sSortedOrcGuids)
        {
            if (Creature* pOrc = instance->GetCreature(elem))
            {
                // Remove unit flags from npcs
                pOrc->RemoveFlag(UNIT_FIELD_FLAGS,
                    UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE |
                        UNIT_FLAG_PASSIVE);

                pOrc->SetAggroDistance(0); // Reset aggro distance

                AttackNearestPlayer(pOrc);
                pOrc->SetInCombatWithZone();
            }
        }
    }

    // Prepare for further handling
    m_uiBroggokEventTimer = 90000;
    ++m_uiBroggokEventPhase;
}

void instance_blood_furnace::OnCreatureEvade(Creature* pCreature)
{
    if (m_auiEncounter[TYPE_BROGGOK_EVENT] == FAIL)
        return;

    if (pCreature->GetEntry() == NPC_BROGGOK)
        SetData(TYPE_BROGGOK_EVENT, FAIL);
    else if (pCreature->GetEntry() == NPC_NASCENT_FEL_ORC ||
             pCreature->GetEntry() == NPC_FEL_ORC_NEOPHYTE)
    {
        for (uint8 i = 0;
             i < std::min<uint32>(m_uiBroggokEventPhase, MAX_ORC_WAVES); ++i)
        {
            if (m_aBroggokEvent[i].m_sSortedOrcGuids.find(
                    pCreature->GetObjectGuid()) !=
                m_aBroggokEvent[i].m_sSortedOrcGuids.end())
                SetData(TYPE_BROGGOK_EVENT, FAIL);
        }
    }
}

void instance_blood_furnace::OnCreatureDeath(Creature* pCreature)
{
    if (m_auiEncounter[TYPE_BROGGOK_EVENT] != IN_PROGRESS)
        return;

    if (pCreature->GetEntry() == NPC_NASCENT_FEL_ORC ||
        pCreature->GetEntry() == NPC_FEL_ORC_NEOPHYTE)
    {
        uint8 uiClearedCells = 0;
        for (uint8 i = 0;
             i < std::min<uint32>(m_uiBroggokEventPhase, MAX_ORC_WAVES); ++i)
        {
            if (m_aBroggokEvent[i].m_sSortedOrcGuids.size() ==
                m_aBroggokEvent[i].m_uiKilledOrcCount)
            {
                ++uiClearedCells;
                continue;
            }

            // Increase kill counter, if we found a mob of this cell
            if (m_aBroggokEvent[i].m_sSortedOrcGuids.find(
                    pCreature->GetObjectGuid()) !=
                m_aBroggokEvent[i].m_sSortedOrcGuids.end())
                m_aBroggokEvent[i].m_uiKilledOrcCount++;

            if (m_aBroggokEvent[i].m_sSortedOrcGuids.size() ==
                m_aBroggokEvent[i].m_uiKilledOrcCount)
                ++uiClearedCells;
        }

        // Increase phase when all opened cells are cleared
        if (uiClearedCells == m_uiBroggokEventPhase)
            DoNextBroggokEventPhase();
    }
}

void instance_blood_furnace::Update(uint32 uiDiff)
{
    // Inital Broggok Event Sort
    if (!m_bHasFixedOrcsForBroggok)
    {
        if (m_uiBroggokFixTimer <= uiDiff)
        {
            DoSortBroggokOrcs();
            m_uiBroggokFixTimer = 0;
            m_bHasFixedOrcsForBroggok = true;
        }
        else
            m_uiBroggokFixTimer -= uiDiff;
    }

    // Broggok Event: For the last wave we don't check the timer; the boss is
    // released only when all mobs die, also the timer is only active on heroic
    if (m_auiEncounter[TYPE_BROGGOK_EVENT] == IN_PROGRESS &&
        m_uiBroggokEventPhase < MAX_ORC_WAVES &&
        !instance->IsRegularDifficulty())
    {
        if (m_uiBroggokEventTimer < uiDiff)
            DoNextBroggokEventPhase();
        else
            m_uiBroggokEventTimer -= uiDiff;
    }
}

uint32 instance_blood_furnace::GetData(uint32 uiType)
{
    if (uiType < MAX_ENCOUNTER)
        return m_auiEncounter[uiType];

    return 0;
}

void instance_blood_furnace::Load(const char* chrIn)
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
        if (elem == IN_PROGRESS || elem == FAIL)
            elem = NOT_STARTED;

    OUT_LOAD_INST_DATA_COMPLETE;
}

// Sort all nascent orcs in the instance in order to get only those near broggok
// doors
void instance_blood_furnace::DoSortBroggokOrcs()
{
    for (GUIDList::const_iterator itr = m_luiNascentOrcGuids.begin();
         itr != m_luiNascentOrcGuids.end(); ++itr)
    {
        if (Creature* pOrc = instance->GetCreature(*itr))
        {
            for (auto& elem : m_aBroggokEvent)
            {
                if (GameObject* pDoor =
                        instance->GetGameObject(elem.m_cellGuid))
                {
                    if (pOrc->IsInMap(pDoor) &&
                        pOrc->GetDistance(pDoor->GetX(), pDoor->GetY(),
                            pDoor->GetZ()) < 15.0f)
                    {
                        elem.m_sSortedOrcGuids.insert(pOrc->GetObjectGuid());
                        if (!pOrc->isAlive())
                            pOrc->Respawn();
                        pOrc->SetAggroDistance(1.0f);
                        pOrc->SetFlag(
                            UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE |
                                                  UNIT_FLAG_NOT_SELECTABLE);
                        break;
                    }
                }
            }
        }
    }
}

// Helper function to calculate the position to where the orcs should move
// For case of orc-indexes the difference, for Braggok his position
void instance_blood_furnace::GetMovementDistanceForIndex(
    uint32 uiIndex, float& dx, float& dy)
{
    GameObject* pDoor[2];

    if (uiIndex < MAX_ORC_WAVES)
    {
        // Use doors 0, 1 for index 0 or 1; and use doors 2, 3 for index 2 or 3
        pDoor[0] = instance->GetGameObject(
            m_aBroggokEvent[(uiIndex / 2) * 2].m_cellGuid);
        pDoor[1] = instance->GetGameObject(
            m_aBroggokEvent[(uiIndex / 2) * 2 + 1].m_cellGuid);
    }
    else
    {
        // Use doors 0 and 3 for Braggok case (which means the middle point is
        // the center of the room)
        pDoor[0] = instance->GetGameObject(m_aBroggokEvent[0].m_cellGuid);
        pDoor[1] = instance->GetGameObject(m_aBroggokEvent[3].m_cellGuid);
    }

    if (!pDoor[0] || !pDoor[1])
        return;

    if (uiIndex < MAX_ORC_WAVES)
    {
        dx = (pDoor[0]->GetX() + pDoor[1]->GetX()) / 2 -
             pDoor[uiIndex % 2]->GetX();
        dy = (pDoor[0]->GetY() + pDoor[1]->GetY()) / 2 -
             pDoor[uiIndex % 2]->GetY();
    }
    else
    {
        dx = (pDoor[0]->GetX() + pDoor[1]->GetX()) / 2;
        dy = (pDoor[0]->GetY() + pDoor[1]->GetY()) / 2;
    }
}

InstanceData* GetInstanceData_instance_blood_furnace(Map* pMap)
{
    return new instance_blood_furnace(pMap);
}

bool GOUse_go_prison_cell_lever(Player* /*pPlayer*/, GameObject* pGo)
{
    ScriptedInstance* pInstance = (ScriptedInstance*)pGo->GetInstanceData();

    if (!pInstance)
        return false;

    // Set broggok event in progress
    if (pInstance->GetData(TYPE_BROGGOK_EVENT) != DONE &&
        pInstance->GetData(TYPE_BROGGOK_EVENT) != IN_PROGRESS)
    {
        pInstance->SetData(TYPE_BROGGOK_EVENT, IN_PROGRESS);

        // Yell intro
        if (Creature* pBroggok =
                pInstance->GetSingleCreatureFromStorage(NPC_BROGGOK))
            DoScriptText(SAY_BROGGOK_INTRO, pBroggok);
    }

    return false;
}

void AddSC_instance_blood_furnace()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "instance_blood_furnace";
    pNewScript->GetInstanceData = &GetInstanceData_instance_blood_furnace;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "go_prison_cell_lever";
    pNewScript->pGOUse = &GOUse_go_prison_cell_lever;
    pNewScript->RegisterSelf();
}
