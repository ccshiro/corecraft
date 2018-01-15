/* Copyright (C) 2013 Corecraft <https://www.worldofcorecraft.com/>
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
SDName: instance_the_eye
SD%Complete: 100
SDComment:
SDCategory: Tempest Keep, The Eye
EndScriptData */

#include "precompiled.h"
#include "the_eye.h"

instance_the_eye::instance_the_eye(Map* pMap)
  : ScriptedInstance(pMap), m_kaelPhase(0), m_releaseEntry(0),
    m_releaseTimer(0), m_beginPhase(0), m_timeout(0), m_killedAdvisorsCount(0)
{
    Initialize();
}

void instance_the_eye::Initialize()
{
    memset(&m_auiEncounter, 0, sizeof(m_auiEncounter));
}

bool instance_the_eye::IsEncounterInProgress() const
{
    for (auto& elem : m_auiEncounter)
    {
        if (elem == IN_PROGRESS)
            return true;
    }

    return false;
}

void instance_the_eye::OnCreatureCreate(Creature* pCreature)
{
    switch (pCreature->GetEntry())
    {
    case NPC_SOLARIAN:
    case NPC_KAELTHAS:
    case NPC_THALADRED:
    case NPC_SANGUINAR:
    case NPC_CAPERNIAN:
    case NPC_TELONICUS:
        break;
    case NPC_PHOENIX:
    case NPC_PHOENIX_EGG:
        m_cleanups.push_back(pCreature->GetObjectGuid());
        return;
    default:
        return;
    }

    m_mNpcEntryGuidStore[pCreature->GetEntry()] = pCreature->GetObjectGuid();
}

void instance_the_eye::OnObjectCreate(GameObject* pGo)
{
    switch (pGo->GetEntry())
    {
    case GO_STATUE_LEFT:
    case GO_STATUE_RIGHT:
    case GO_TEMPEST_BRIDGE_WINDOW:
        break;
    case GO_ALAR_DOOR_1:
    case GO_ALAR_DOOR_2:
        if (GetData(TYPE_ALAR) == DONE)
            pGo->SetGoState(GO_STATE_ACTIVE);
        break;
    case GO_VOID_DOOR_1:
    case GO_VOID_DOOR_2:
        if (GetData(TYPE_VOID_REAVER) == DONE)
            pGo->SetGoState(GO_STATE_ACTIVE);
        break;
    case GO_SOLARIAN_DOOR_1:
    case GO_SOLARIAN_DOOR_2:
        if (GetData(TYPE_ALAR) == DONE)
            pGo->SetGoState(GO_STATE_ACTIVE);
        break;
    default:
        return;
    }

    m_mGoEntryGuidStore[pGo->GetEntry()] = pGo->GetObjectGuid();
}

void instance_the_eye::OnCreatureEvade(Creature* pCreature)
{
    switch (pCreature->GetEntry())
    {
    case NPC_STAFF_OF_DISINTEGRATION:
    case NPC_COSMIC_INFUSER:
    case NPC_WARP_SPLICER:
    case NPC_DEVASTATION:
    case NPC_PHASESHIFT_BULWARK:
    case NPC_INFINITY_BLADES:
    case NPC_NETHERSTRAND_LONGBOW:
        SetData(TYPE_KAELTHAS, FAIL);
        break;
    default:
        return;
    }
}

void instance_the_eye::OnCreatureDeath(Creature* pCreature)
{
    switch (pCreature->GetEntry())
    {
    case NPC_THALADRED:
    case NPC_SANGUINAR:
    case NPC_CAPERNIAN:
    case NPC_TELONICUS:
        ++m_killedAdvisorsCount;
        if (m_killedAdvisorsCount >= 4 &&
            GetData(DATA_KAELTHAS_PHASE) == KAEL_PHASE_ALL_ADVISORS)
            SetData(DATA_KAELTHAS_PHASE, KAEL_PHASE_PRE_RELEASE);
        break;

    case NPC_STAFF_OF_DISINTEGRATION:
    case NPC_COSMIC_INFUSER:
    case NPC_WARP_SPLICER:
    case NPC_DEVASTATION:
    case NPC_PHASESHIFT_BULWARK:
    case NPC_INFINITY_BLADES:
    case NPC_NETHERSTRAND_LONGBOW:
        pCreature->ForcedDespawn(60 * IN_MILLISECONDS);
        break;

    default:
        return;
    }
}

void instance_the_eye::OnPlayerLeave(Player* player)
{
    // Logging offline/teleporting out with Wrath of the Astromancer DoT
    // protection
    if (player->has_aura(33045))
    {
        // Solarian WotA Leave Protection NPC (spell cast by SmartAI)
        if (auto solarian = GetSingleCreatureFromStorage(NPC_SOLARIAN))
        {
            solarian->SummonCreature(100179, player->GetX(), player->GetY(),
                player->GetZ(), 0, TEMPSUMMON_TIMED_DESPAWN, 20000,
                SUMMON_OPT_NOT_COMBAT_SUMMON);
        }
    }
}

void instance_the_eye::SetData(uint32 uiType, uint32 uiData)
{
    switch (uiType)
    {
    case TYPE_VOID_REAVER:
        if (uiData == DONE)
        {
            if (GameObject* go = GetSingleGameObjectFromStorage(GO_VOID_DOOR_1))
                go->SetGoState(GO_STATE_ACTIVE);
            if (GameObject* go = GetSingleGameObjectFromStorage(GO_VOID_DOOR_2))
                go->SetGoState(GO_STATE_ACTIVE);
        }
        m_auiEncounter[uiType] = uiData;
        break;
    case TYPE_SOLARIAN:
        if (uiData == DONE)
        {
            if (GameObject* go =
                    GetSingleGameObjectFromStorage(GO_SOLARIAN_DOOR_1))
                go->SetGoState(GO_STATE_ACTIVE);
            if (GameObject* go =
                    GetSingleGameObjectFromStorage(GO_SOLARIAN_DOOR_2))
                go->SetGoState(GO_STATE_ACTIVE);
        }
        m_auiEncounter[uiType] = uiData;
        break;
    case TYPE_ALAR:
        if (uiData == DONE)
        {
            if (GameObject* go = GetSingleGameObjectFromStorage(GO_ALAR_DOOR_1))
                go->SetGoState(GO_STATE_ACTIVE);
            if (GameObject* go = GetSingleGameObjectFromStorage(GO_ALAR_DOOR_2))
                go->SetGoState(GO_STATE_ACTIVE);
        }
        m_auiEncounter[uiType] = uiData;
        break;
    case TYPE_KAELTHAS:
        // Don't reprocess fail if the encounter is already failed
        if (uiData == FAIL && m_auiEncounter[uiType] == FAIL)
            return;

        // Close/Open doors
        if (GameObject* go = GetSingleGameObjectFromStorage(GO_SOLARIAN_DOOR_1))
            go->SetGoState(
                uiData == IN_PROGRESS ? GO_STATE_READY : GO_STATE_ACTIVE);
        if (GameObject* go = GetSingleGameObjectFromStorage(GO_SOLARIAN_DOOR_2))
            go->SetGoState(
                uiData == IN_PROGRESS ? GO_STATE_READY : GO_STATE_ACTIVE);

        // Reset encounter on fail
        if (uiData == FAIL)
        {
            // Respawn / Evade all mobs
            if (Creature* c = GetSingleCreatureFromStorage(NPC_SANGUINAR))
            {
                c->Respawn();
                c->SetFlag(UNIT_FIELD_FLAGS,
                    UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_PASSIVE);
            }
            if (Creature* c = GetSingleCreatureFromStorage(NPC_CAPERNIAN))
            {
                c->Respawn();
                c->SetFlag(UNIT_FIELD_FLAGS,
                    UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_PASSIVE);
            }
            if (Creature* c = GetSingleCreatureFromStorage(NPC_TELONICUS))
            {
                c->Respawn();
                c->SetFlag(UNIT_FIELD_FLAGS,
                    UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_PASSIVE);
            }
            if (Creature* c = GetSingleCreatureFromStorage(NPC_THALADRED))
            {
                c->Respawn();
                c->SetFlag(UNIT_FIELD_FLAGS,
                    UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_PASSIVE);
            }
            if (Creature* kael = GetSingleCreatureFromStorage(NPC_KAELTHAS))
            {
                kael->AI()->EnterEvadeMode();
                kael->SetFlag(
                    UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE); // Not passive
            }

            // Destroy weapons
            static const int max_weap = 9;
            static const uint32 weapons[max_weap] = {
                30311, 30312, 30313, 30314, 30316, 30317, 30318, 30319, 30320};
            for (const auto& elem : instance->GetPlayers())
            {
                if (Player* p = elem.getSource())
                    for (auto& weapon : weapons)
                        p->destroy_item(weapon);
            }

            // Reset game objects
            if (GameObject* go = GetSingleGameObjectFromStorage(GO_STATUE_LEFT))
                go->SetGoState(GO_STATE_READY);
            if (GameObject* go =
                    GetSingleGameObjectFromStorage(GO_STATUE_RIGHT))
                go->SetGoState(GO_STATE_READY);
            if (GameObject* go =
                    GetSingleGameObjectFromStorage(GO_TEMPEST_BRIDGE_WINDOW))
                go->SetGoState(GO_STATE_READY);

            // Cleanup summons
            for (auto& elem : m_cleanups)
                if (Creature* c = instance->GetCreature(elem))
                    c->ForcedDespawn();
            m_cleanups.clear();

            SetData(DATA_KAELTHAS_PHASE, 0);
        }
        else if (uiData == IN_PROGRESS)
            SetData(DATA_KAELTHAS_PHASE, KAEL_PHASE_INTRO_RP);

        m_releaseTimer = 0;
        m_releaseEntry = 0;
        m_beginPhase = 0;
        m_timeout = 0;
        m_killedAdvisorsCount = 0;

        m_auiEncounter[uiType] = uiData;
        break;

    case DATA_KAELTHAS_PHASE:
        if (uiData == KAEL_PHASE_ADVISOR)
        {
            if (Creature* thaladred =
                    GetSingleCreatureFromStorage(NPC_THALADRED))
            {
                thaladred->RemoveFlag(UNIT_FIELD_FLAGS,
                    UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_PASSIVE);
                thaladred->SetInCombatWithZone();
                // Fail event if no players are nearby when it's time to release
                // thaladred
                if (!thaladred->getVictim())
                    SetData(TYPE_KAELTHAS, FAIL);
            }
        }
        else if (uiData == KAEL_PHASE_WEAPONS)
        {
            if (Creature* kael = GetSingleCreatureFromStorage(NPC_KAELTHAS))
            {
                DoScriptText(SAY_KAEL_SUMMON_WEAPONS, kael);
                kael->CastSpell(kael, SPELL_SUMMON_WEAPONS, false);
            }
            m_timeout = 100 * IN_MILLISECONDS;
        }
        else if (uiData == KAEL_PHASE_ALL_ADVISORS)
        {
            if (Creature* kael = GetSingleCreatureFromStorage(NPC_KAELTHAS))
                DoScriptText(SAY_KAEL_RES_ADVISORS, kael);
            m_beginPhase = 12000;
        }
        else if (uiData == KAEL_PHASE_PRE_RELEASE)
        {
            if (Creature* kael = GetSingleCreatureFromStorage(NPC_KAELTHAS))
                DoScriptText(SAY_KAEL_RELEASE, kael);
            m_beginPhase = 5000;
            m_timeout = 0;
        }
        m_kaelPhase = uiData;
        return; // Don't continue on for data

    default:
        return;
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

uint32 instance_the_eye::GetData(uint32 uiType)
{
    switch (uiType)
    {
    case TYPE_VOID_REAVER:
        return m_auiEncounter[uiType];
    case TYPE_SOLARIAN:
        return m_auiEncounter[uiType];
    case TYPE_ALAR:
        return m_auiEncounter[uiType];
    case TYPE_KAELTHAS:
        return m_auiEncounter[uiType];
    case DATA_KAELTHAS_PHASE:
        return m_kaelPhase;
    default:
        break;
    }

    return 0;
}

void instance_the_eye::Load(const char* chrIn)
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

void instance_the_eye::OnAdvisorDeath(Creature* pCreature)
{
    if (GetData(DATA_KAELTHAS_PHASE) == KAEL_PHASE_ADVISOR)
    {
        Creature* kael = GetSingleCreatureFromStorage(NPC_KAELTHAS);
        if (!kael)
            return;

        m_releaseEntry = 0;
        switch (pCreature->GetEntry())
        {
        case NPC_THALADRED:
            m_releaseEntry = NPC_SANGUINAR;
            DoScriptText(SAY_KEAL_SANGUINAR_RELEASE, kael);
            m_releaseTimer = 12500;
            break;
        case NPC_SANGUINAR:
            m_releaseEntry = NPC_CAPERNIAN;
            DoScriptText(SAY_KAEL_CAPERNIAN_RELEASE, kael);
            m_releaseTimer = 7000;
            break;
        case NPC_CAPERNIAN:
            m_releaseEntry = NPC_TELONICUS;
            DoScriptText(SAY_KAEL_TELONICUS_RELEASE, kael);
            m_releaseTimer = 8500;
            break;
        case NPC_TELONICUS:
            SetData(DATA_KAELTHAS_PHASE, KAEL_PHASE_WEAPONS);
            return; // Don't continue
        default:
            return;
        }
    }
}

void instance_the_eye::Update(uint32 diff)
{
    if (m_releaseEntry)
    {
        if (m_releaseTimer <= diff)
        {
            if (Creature* c = GetSingleCreatureFromStorage(m_releaseEntry))
            {
                c->RemoveFlag(UNIT_FIELD_FLAGS,
                    UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_PASSIVE);
                c->SetInCombatWithZone();
            }
            m_releaseTimer = 0;
            m_releaseEntry = 0;
        }
        else
            m_releaseTimer -= diff;
    }

    if (m_beginPhase)
    {
        if (m_beginPhase <= diff)
        {
            if (GetData(DATA_KAELTHAS_PHASE) == KAEL_PHASE_ALL_ADVISORS)
            {
                if (Creature* kael = GetSingleCreatureFromStorage(NPC_KAELTHAS))
                    kael->CastSpell(kael, SPELL_RES_ADVISORS, false);
                m_timeout = 175 * IN_MILLISECONDS;
            }
            else if (GetData(DATA_KAELTHAS_PHASE) == KAEL_PHASE_PRE_RELEASE)
            {
                SetData(DATA_KAELTHAS_PHASE, KAEL_PHASE_KAELTHAS_P1);
            }

            m_beginPhase = 0;
        }
        else
            m_beginPhase -= diff;
    }

    if (m_timeout)
    {
        if (m_timeout <= diff)
        {
            SetData(DATA_KAELTHAS_PHASE, GetData(DATA_KAELTHAS_PHASE) + 1);
            m_timeout = 0;
        }
        else
            m_timeout -= diff;
    }
}

InstanceData* GetInstanceData_instance_the_eye(Map* pMap)
{
    return new instance_the_eye(pMap);
}

void AddSC_instance_the_eye()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "instance_the_eye";
    pNewScript->GetInstanceData = &GetInstanceData_instance_the_eye;
    pNewScript->RegisterSelf();
}
