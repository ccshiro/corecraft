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
SDName: instance_serpentshrine_caverns
SD%Complete: 100
SDComment:
SDCategory: Coilfang Resevoir, Serpent Shrine Cavern
EndScriptData */

#include "precompiled.h"
#include "serpentshrine_cavern.h"

instance_serpentshrine_cavern::instance_serpentshrine_cavern(Map* pMap)
  : ScriptedInstance(pMap), m_waterUpdate(WATER_UPDATE_INTERVAL),
    m_fishNextSpawn(0)
{
    Initialize();
}

void instance_serpentshrine_cavern::Initialize()
{
    memset(&m_auiEncounter, 0, sizeof(m_auiEncounter));
    memset(&m_auiData, 0, sizeof(m_auiData));
}

bool instance_serpentshrine_cavern::IsEncounterInProgress() const
{
    for (auto& elem : m_auiEncounter)
        if (elem == IN_PROGRESS)
            return true;

    return false;
}

void instance_serpentshrine_cavern::Update(uint32 diff)
{
    if (GetData(TYPE_LURKER_BELOW) == DONE)
        return;

    if (m_waterUpdate <= diff)
    {
        bool boil = (bool)GetData(TYPE_WATER_BOILING);
        if (!boil && time(NULL) < m_fishNextSpawn)
        {
            m_waterUpdate = WATER_UPDATE_INTERVAL;
            return;
        }

        bool summonedFish = false;
        for (const auto& elem : instance->GetPlayers())
        {
            Player* p = elem.getSource();
            if (!p || !p->isAlive() || p->isGameMaster())
                continue;
            float x, y, z;
            p->GetPosition(x, y, z);
            if (instance->GetTerrain()->IsInWater(x, y, z) &&
                p->m_movementInfo.HasMovementFlag(MOVEFLAG_SWIMMING))
            {
                if (boil && !p->has_aura(SPELL_SCALDING_WATER))
                    p->CastSpell(p, SPELL_SCALDING_WATER, true);
                else if (!boil)
                {
                    p->SummonCreature(NPC_COILFANG_FRENZY, x, y, z, p->GetO(),
                        TEMPSUMMON_TIMED_DEATH, 20000);
                    summonedFish = true;
                }
            }
            else
            {
                if (boil && p->HasAuraType(SPELL_AURA_PERIODIC_DAMAGE))
                    p->remove_auras(SPELL_SCALDING_WATER);
            }
        }

        if (summonedFish)
            m_fishNextSpawn = time(NULL) + urand(2, 5);
        m_waterUpdate = WATER_UPDATE_INTERVAL;
    }
    else
        m_waterUpdate -= diff;
}

void instance_serpentshrine_cavern::OnObjectCreate(GameObject* pGo)
{
    switch (pGo->GetEntry())
    {
    case GO_GENERATOR_1:
    case GO_GENERATOR_2:
    case GO_GENERATOR_3:
    case GO_GENERATOR_4:
        break;
    case GO_HYDROSS_CONSOLE:
        if (GetData(TYPE_CONSOLES) & 0x1)
            pGo->SetGoState(GO_STATE_ACTIVE);
        else if (GetData(TYPE_HYDROSS) == DONE)
            pGo->RemoveFlag(GAMEOBJECT_FLAGS, GO_FLAG_NO_INTERACT);
        break;
    case GO_LURKER_CONSOLE:
        if (GetData(TYPE_CONSOLES) & 0x2)
            pGo->SetGoState(GO_STATE_ACTIVE);
        else if (GetData(TYPE_LURKER_BELOW) == DONE)
            pGo->RemoveFlag(GAMEOBJECT_FLAGS, GO_FLAG_NO_INTERACT);
        break;
    case GO_LEOTHERAS_CONSOLE:
        if (GetData(TYPE_CONSOLES) & 0x4)
            pGo->SetGoState(GO_STATE_ACTIVE);
        else if (GetData(TYPE_LEOTHERAS) == DONE)
            pGo->RemoveFlag(GAMEOBJECT_FLAGS, GO_FLAG_NO_INTERACT);
        break;
    case GO_KARATHRESS_CONSOLE:
        if (GetData(TYPE_CONSOLES) & 0x8)
            pGo->SetGoState(GO_STATE_ACTIVE);
        else if (GetData(TYPE_KARATHRESS) == DONE)
            pGo->RemoveFlag(GAMEOBJECT_FLAGS, GO_FLAG_NO_INTERACT);
        break;
    case GO_MOROGRIM_CONSOLE:
        if (GetData(TYPE_CONSOLES) & 0x10)
            pGo->SetGoState(GO_STATE_ACTIVE);
        else if (GetData(TYPE_MOROGRIM) == DONE)
            pGo->RemoveFlag(GAMEOBJECT_FLAGS, GO_FLAG_NO_INTERACT);
        break;
    case GO_VASHJ_CONSOLE:
        if ((GetData(TYPE_CONSOLES) & (0x1 | 0x2 | 0x4 | 0x8 | 0x10)) ==
                (0x1 | 0x2 | 0x4 | 0x8 | 0x10) &&
            !(GetData(TYPE_CONSOLES) & 0x20))
            pGo->RemoveFlag(GAMEOBJECT_FLAGS, GO_FLAG_NO_INTERACT);
    // No break
    case GO_BRIDGE_PART_1:
    case GO_BRIDGE_PART_2:
    case GO_BRIDGE_PART_3:
        if (GetData(TYPE_CONSOLES) & 0x20)
            pGo->SetGoState(GO_STATE_ACTIVE);
        break;
    default:
        return;
    }
    m_mGoEntryGuidStore[pGo->GetEntry()] = pGo->GetObjectGuid();
}

void instance_serpentshrine_cavern::OnCreatureCreate(Creature* pCreature)
{
    switch (pCreature->GetEntry())
    {
    case NPC_LEOTHERAS:
    case NPC_SHADOW_LEOTHERAS:
    case NPC_KARATHRESS:
    case NPC_TIDALVESS:
    case NPC_SHARKKIS:
    case NPC_CARIDBIS:
    case NPC_MOROGRIM:
    case NPC_LADY_VASHJ:
        break;
    case NPC_TOXIC_SPOREBAT:
        SetData(TYPE_SPOREBAT_COUNT, GetData(TYPE_SPOREBAT_COUNT) + 1);
        return;
    case NPC_VASHJIR_HONOR_GUARD:
        m_vashjirGuards.push_back(pCreature->GetObjectGuid());
        return;
    default:
        return;
    }
    m_mNpcEntryGuidStore[pCreature->GetEntry()] = pCreature->GetObjectGuid();
}

void instance_serpentshrine_cavern::OnCreatureDeath(Creature* pCreature)
{
    switch (pCreature->GetEntry())
    {
    case NPC_GREYHEART_SPELLBINDER:
        SetData(TYPE_SPELLBINDERS, GetData(TYPE_SPELLBINDERS) + 1);
        break;
    case NPC_TIDALVESS:
        pCreature->CastSpell(pCreature, SPELL_POWER_OF_TIDALVESS, true);
        SetData(TYPE_KARATHRESS_GUARDS, GetData(TYPE_KARATHRESS_GUARDS) + 1);
        break;
    case NPC_SHARKKIS:
        pCreature->CastSpell(pCreature, SPELL_POWER_OF_SHARKKIS, true);
        SetData(TYPE_KARATHRESS_GUARDS, GetData(TYPE_KARATHRESS_GUARDS) + 1);
        break;
    case NPC_CARIDBIS:
        pCreature->CastSpell(pCreature, SPELL_POWER_OF_CARIDBIS, true);
        SetData(TYPE_KARATHRESS_GUARDS, GetData(TYPE_KARATHRESS_GUARDS) + 1);
        break;
    case NPC_TOXIC_SPOREBAT:
        if (GetData(TYPE_SPOREBAT_COUNT) > 0)
            SetData(TYPE_SPOREBAT_COUNT, GetData(TYPE_SPOREBAT_COUNT) - 1);
        break;
    case NPC_VASHJIR_HONOR_GUARD:
        if (GetData(TYPE_LURKER_BELOW) != DONE && GetData(TYPE_WATER_BOILING))
            SpawnDeadFishes();
        break;
    default:
        return;
    }
}

void instance_serpentshrine_cavern::OnPlayerLeave(Player* pPlayer)
{
    pPlayer->remove_auras(SPELL_SCALDING_WATER);
}

void instance_serpentshrine_cavern::SetData(uint32 uiType, uint32 uiData)
{
    switch (uiType)
    {
    case TYPE_HYDROSS:
        if (uiData == DONE)
            if (GameObject* console =
                    GetSingleGameObjectFromStorage(GO_HYDROSS_CONSOLE))
                console->RemoveFlag(GAMEOBJECT_FLAGS, GO_FLAG_NO_INTERACT);
        m_auiEncounter[uiType] = uiData;
        break;
    case TYPE_LURKER_BELOW:
        if (uiData == DONE)
        {
            if (GameObject* console =
                    GetSingleGameObjectFromStorage(GO_LURKER_CONSOLE))
                console->RemoveFlag(GAMEOBJECT_FLAGS, GO_FLAG_NO_INTERACT);
            // Make sure to remove scalding water from everyone
            for (const auto& elem : instance->GetPlayers())
            {
                Player* p = elem.getSource();
                if (p && p->isAlive())
                    p->remove_auras(SPELL_SCALDING_WATER);
            }
        }
        m_auiEncounter[uiType] = uiData;
        break;
    case TYPE_LEOTHERAS:
        if (uiData == FAIL)
            SetData(TYPE_SPELLBINDERS, 0);
        else if (uiData == DONE)
            if (GameObject* console =
                    GetSingleGameObjectFromStorage(GO_LEOTHERAS_CONSOLE))
                console->RemoveFlag(GAMEOBJECT_FLAGS, GO_FLAG_NO_INTERACT);
        m_auiEncounter[uiType] = uiData;
        break;
    case TYPE_KARATHRESS:
        if (uiData == FAIL)
            SetData(TYPE_KARATHRESS_GUARDS, 0);
        else if (uiData == DONE)
            if (GameObject* console =
                    GetSingleGameObjectFromStorage(GO_KARATHRESS_CONSOLE))
                console->RemoveFlag(GAMEOBJECT_FLAGS, GO_FLAG_NO_INTERACT);
        m_auiEncounter[uiType] = uiData;
        break;
    case TYPE_MOROGRIM:
        if (uiData == DONE)
            if (GameObject* console =
                    GetSingleGameObjectFromStorage(GO_MOROGRIM_CONSOLE))
                console->RemoveFlag(GAMEOBJECT_FLAGS, GO_FLAG_NO_INTERACT);
        m_auiEncounter[uiType] = uiData;
        break;
    case TYPE_LADY_VASHJ:
        ResetLadyVashjEncounter();
        m_auiEncounter[uiType] = uiData;
        break;
    /* DATA */
    case TYPE_CONSOLES:
        if ((uiData & (0x1 | 0x2 | 0x4 | 0x8 | 0x10)) ==
                (0x1 | 0x2 | 0x4 | 0x8 | 0x10) &&
            !(uiData & 0x20))
        {
            // Make the bridge console interactable
            if (GameObject* console =
                    GetSingleGameObjectFromStorage(GO_VASHJ_CONSOLE))
                console->RemoveFlag(GAMEOBJECT_FLAGS, GO_FLAG_NO_INTERACT);
        }
        else if (uiData & 0x20)
        {
            // Open the bridge
            DoUseDoorOrButton(GO_BRIDGE_PART_1);
            DoUseDoorOrButton(GO_BRIDGE_PART_2);
            DoUseDoorOrButton(GO_BRIDGE_PART_3);
        }
        m_auiData[uiType - MAX_ENCOUNTER] = uiData;
        break;
    case TYPE_SPELLBINDERS:
        m_auiData[uiType - MAX_ENCOUNTER] = uiData;
        if (uiData >= 3)
            if (Creature* c = GetSingleCreatureFromStorage(NPC_LEOTHERAS))
            {
                c->remove_auras(SPELL_LEOTHERAS_BANISH);
                DoScriptText(SAY_LEOTHERAS_AGGRO, c);
                c->SetStandState(UNIT_STAND_STATE_STAND);
                c->AI()->Pacify(false);
                if (auto ai = dynamic_cast<ScriptedAI*>(c->AI()))
                    ai->DoResetThreat();
                c->movement_gens.remove_all(movement::gen::stopped);
            }
        break;
    case TYPE_KARATHRESS_GUARDS:
        m_auiData[uiType - MAX_ENCOUNTER] = uiData;
        if (Creature* c = GetSingleCreatureFromStorage(NPC_KARATHRESS))
        {
            if (uiData == 1)
                DoScriptText(SAY_KARATHRESS_GUARD_DEATH_1, c);
            else if (uiData == 2)
                DoScriptText(SAY_KARATHRESS_GUARD_DEATH_2, c);
            else if (uiData == 3)
                DoScriptText(SAY_KARATHRESS_GUARD_DEATH_2, c);
        }
        break;
    case TYPE_SPOREBAT_COUNT:
        m_auiData[uiType - MAX_ENCOUNTER] = uiData;
        break;
    case TYPE_WATER_BOILING:
        return; // Not saved
    default:
        return;
    }

    if (uiType >= MAX_ENCOUNTER && uiType != TYPE_CONSOLES)
        return; // It's a non-saved data entry

    if (uiData == DONE || uiType == TYPE_CONSOLES)
    {
        OUT_SAVE_INST_DATA;

        std::ostringstream saveStream;
        saveStream << m_auiEncounter[0] << " " << m_auiEncounter[1] << " "
                   << m_auiEncounter[2] << " " << m_auiEncounter[3] << " "
                   << m_auiEncounter[4] << " " << m_auiEncounter[5] << " "
                   << m_auiData[0];

        m_strInstData = saveStream.str();

        SaveToDB();
        OUT_SAVE_INST_DATA_COMPLETE;
    }
}

void instance_serpentshrine_cavern::ResetLadyVashjEncounter()
{
    // Make Shield Generators non-interactable
    ToggleShieldGenerators(false);
    // Remove Tainted Cores from people
    for (const auto& elem : instance->GetPlayers())
        if (Player* plr = elem.getSource())
            plr->destroy_item(ITEM_TAINTED_CORE);
    SetData(TYPE_SPOREBAT_COUNT, 0);
}

void instance_serpentshrine_cavern::ToggleShieldGenerators(bool interactable)
{
    if (GameObject* go = GetSingleGameObjectFromStorage(GO_GENERATOR_1))
        interactable ? go->RemoveFlag(GAMEOBJECT_FLAGS, GO_FLAG_NO_INTERACT) :
                       go->SetFlag(GAMEOBJECT_FLAGS, GO_FLAG_NO_INTERACT);
    if (GameObject* go = GetSingleGameObjectFromStorage(GO_GENERATOR_2))
        interactable ? go->RemoveFlag(GAMEOBJECT_FLAGS, GO_FLAG_NO_INTERACT) :
                       go->SetFlag(GAMEOBJECT_FLAGS, GO_FLAG_NO_INTERACT);
    if (GameObject* go = GetSingleGameObjectFromStorage(GO_GENERATOR_3))
        interactable ? go->RemoveFlag(GAMEOBJECT_FLAGS, GO_FLAG_NO_INTERACT) :
                       go->SetFlag(GAMEOBJECT_FLAGS, GO_FLAG_NO_INTERACT);
    if (GameObject* go = GetSingleGameObjectFromStorage(GO_GENERATOR_4))
        interactable ? go->RemoveFlag(GAMEOBJECT_FLAGS, GO_FLAG_NO_INTERACT) :
                       go->SetFlag(GAMEOBJECT_FLAGS, GO_FLAG_NO_INTERACT);
}

void instance_serpentshrine_cavern::Load(const char* chrIn)
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
        m_auiData[0];

    for (auto& elem : m_auiEncounter)
    {
        if (elem == IN_PROGRESS)
            elem = NOT_STARTED;
    }

    OUT_LOAD_INST_DATA_COMPLETE;
}

uint32 instance_serpentshrine_cavern::GetData(uint32 uiType)
{
    switch (uiType)
    {
    case TYPE_HYDROSS:
        return m_auiEncounter[uiType];
    case TYPE_LURKER_BELOW:
        return m_auiEncounter[uiType];
    case TYPE_LEOTHERAS:
        return m_auiEncounter[uiType];
    case TYPE_KARATHRESS:
        return m_auiEncounter[uiType];
    case TYPE_MOROGRIM:
        return m_auiEncounter[uiType];
    case TYPE_LADY_VASHJ:
        return m_auiEncounter[uiType];

    // Data
    case TYPE_CONSOLES:
        return m_auiData[uiType - MAX_ENCOUNTER];
    case TYPE_SPELLBINDERS:
        return m_auiData[uiType - MAX_ENCOUNTER];
    case TYPE_KARATHRESS_GUARDS:
        return m_auiData[uiType - MAX_ENCOUNTER];
    case TYPE_SPOREBAT_COUNT:
        return m_auiData[uiType - MAX_ENCOUNTER];

    // Not saved
    case TYPE_WATER_BOILING:
    {
        bool boiling = true;
        for (auto& elem : m_vashjirGuards)
        {
            if (Creature* c = instance->GetCreature(elem))
                if (!c->isAlive())
                    continue;
            boiling = false;
            break;
        }
        return boiling;
    }

    default:
        break;
    }

    return 0;
}

#define MAX_FISH 32
float fishSpawns[MAX_FISH][3] = {{-216.2449951f, -374.8999939f, 40.0f},
    {-216.2449951f, -374.8999939f, 40.0f},
    {-216.2449951f, -374.8999939f, 40.0f},
    {-216.2449951f, -374.8999939f, 40.0f},
    {63.08993912f, -252.824646f, -20.78470612f},
    {64.64645386f, -310.5985718f, -20.78470612f},
    {109.9096527f, -352.6861267f, -20.78470612f},
    {85.88866425f, -401.9228516f, -20.78470612f},
    {101.7817001f, -454.1466064f, -20.78470612f},
    {35.77406693f, -462.8544312f, -20.78470612f},
    {-29.09943199f, -484.7655945f, -20.77805901f},
    {-17.85966492f, -548.0906982f, -20.77805901f},
    {18.05982399f, -610.088562f, -20.77805901f},
    {144.2252502f, -604.2471313f, -20.77805901f},
    {191.2280273f, -518.2661133f, -20.77805901f},
    {152.8161926f, -438.084198f, -20.77806854f},
    {-41.06258392f, -407.7570496f, -20.77709198f},
    {-109.0447922f, -429.3956604f, -20.77709198f},
    {-181.1824036f, -309.01474f, -20.77709198f},
    {-245.7097473f, -261.0198669f, -20.77709198f},
    {-56.96102524f, -425.0892334f, -20.77709198f},
    {78.62160492f, -406.0816956f, -20.77809715f},
    {-3.22665143f, -378.1266174f, -20.77814484f},
    {-98.65718079f, -287.9035034f, -20.77803802f},
    {-31.45067024f, -204.4026642f, -20.77803802f},
    {-99.22206879f, -165.2540131f, -20.77802277f},
    {-194.8564148f, -150.2410736f, -20.77802277f},
    {-229.1348419f, -190.4382172f, -20.77802277f},
    {82.22849274f, -511.9176636f, -20.77802277f},
    {182.2297058f, -391.3478394f, -20.77802277f},
    {200.8041229f, -292.5113525f, -20.77802277f},
    {206.4415588f, -216.7283478f, -20.77976799f}};

void instance_serpentshrine_cavern::SpawnDeadFishes()
{
    // Need someone to spawn the fishes from
    Player* p = GetPlayerInMap();
    if (!p)
        return;

    // Get respawn time of Vashj'ir Guard with highest respawn time
    time_t respTime = time(NULL) + 10000;
    for (auto& elem : m_vashjirGuards)
        if (Creature* c = instance->GetCreature(elem))
        {
            time_t resp = c->GetRespawnTime();
            if (resp < respTime)
                respTime = resp;
        }

    for (auto& fishSpawn : fishSpawns)
        p->SummonCreature(NPC_COILFANG_FRENZY_CORPSE, fishSpawn[0],
            fishSpawn[1], fishSpawn[2], frand(0.0f, 2 * M_PI_F),
            TEMPSUMMON_TIMED_DESPAWN, (respTime - time(NULL)) * 1000);
}

InstanceData* GetInstanceData_instance_serpentshrine_cavern(Map* pMap)
{
    return new instance_serpentshrine_cavern(pMap);
}

bool GOUse_serpentshrine_console(Player* /*p*/, GameObject* go)
{
    if (go->GetInstanceData())
    {
        if (ScriptedInstance* inst =
                dynamic_cast<ScriptedInstance*>(go->GetInstanceData()))
        {
            switch (go->GetEntry())
            {
            case GO_HYDROSS_CONSOLE:
                inst->SetData(
                    TYPE_CONSOLES, inst->GetData(TYPE_CONSOLES) | 0x1);
                break;
            case GO_LURKER_CONSOLE:
                inst->SetData(
                    TYPE_CONSOLES, inst->GetData(TYPE_CONSOLES) | 0x2);
                break;
            case GO_LEOTHERAS_CONSOLE:
                inst->SetData(
                    TYPE_CONSOLES, inst->GetData(TYPE_CONSOLES) | 0x4);
                break;
            case GO_KARATHRESS_CONSOLE:
                inst->SetData(
                    TYPE_CONSOLES, inst->GetData(TYPE_CONSOLES) | 0x8);
                break;
            case GO_MOROGRIM_CONSOLE:
                inst->SetData(
                    TYPE_CONSOLES, inst->GetData(TYPE_CONSOLES) | 0x10);
                break;
            case GO_VASHJ_CONSOLE:
                inst->SetData(
                    TYPE_CONSOLES, inst->GetData(TYPE_CONSOLES) | 0x20);
                break;
            }
        }
    }
    return true;
}

enum
{
    GO_COILFANG_WATERFALL = 184212
};

bool GOUse_waterfall_console(Player* p, GameObject* /*go*/)
{
    if (GameObject* pGo =
            GetClosestGameObjectWithEntry(p, GO_COILFANG_WATERFALL, 60.0f))
    {
        if (pGo->getLootState() == GO_READY)
            pGo->UseDoorOrButton();
    }
    return true;
}

void AddSC_instance_serpentshrine_cavern()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "instance_serpentshrine_cavern";
    pNewScript->GetInstanceData =
        &GetInstanceData_instance_serpentshrine_cavern;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "go_serpentshrine_console";
    pNewScript->pGOUse = &GOUse_serpentshrine_console;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "go_waterfall_console"; // Console outside. Waterfall
                                               // itself handled in
                                               // areatrigger_scripts.cpp
    pNewScript->pGOUse = &GOUse_waterfall_console;
    pNewScript->RegisterSelf();
}
