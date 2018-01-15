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
SDName: Instance_Shadow_Labyrinth
SD%Complete: 100
SDComment:
SDCategory: Auchindoun, Shadow Labyrinth
EndScriptData */

#include "precompiled.h"
#include "shadow_labyrinth.h"

/* Shadow Labyrinth encounters:
1 - Ambassador Hellmaw event
2 - Blackheart the Inciter event
3 - Grandmaster Vorpil event
4 - Murmur event
*/

instance_shadow_labyrinth::instance_shadow_labyrinth(Map* pMap)
  : ScriptedInstance(pMap)
{
    Initialize();
}

void instance_shadow_labyrinth::Initialize()
{
    memset(&m_auiEncounter, 0, sizeof(m_auiEncounter));
}

void instance_shadow_labyrinth::OnObjectCreate(GameObject* pGo)
{
    switch (pGo->GetEntry())
    {
    case GO_REFECTORY_DOOR:
        if (GetData(TYPE_INCITER) == DONE)
            pGo->SetGoState(GO_STATE_ACTIVE);
        break;
    case GO_SCREAMING_HALL_DOOR:
        break;

    default:
        return;
    }

    m_mGoEntryGuidStore[pGo->GetEntry()] = pGo->GetObjectGuid();
}

void instance_shadow_labyrinth::OnCreatureCreate(Creature* pCreature)
{
    switch (pCreature->GetEntry())
    {
    case NPC_CABAL_RITUALIST_1:
    case NPC_CABAL_RITUALIST_2:
    case NPC_CABAL_RITUALIST_3:
    case NPC_CABAL_RITUALIST_4:
    {
        if (pCreature->isAlive())
        {
            SetData(TYPE_RITUALISTS, GetData(TYPE_RITUALISTS) + 1);
            if (auto hellmaw = GetSingleCreatureFromStorage(NPC_HELLMAW))
                if (!hellmaw->has_aura(SPELL_HELLMAW_BANISH))
                    pCreature->AddAuraThroughNewHolder(
                        SPELL_HELLMAW_BANISH, pCreature);
        }
        return;
    }
    case NPC_HELLMAW:
        if (GetData(TYPE_RITUALISTS) != 0)
            pCreature->AddAuraThroughNewHolder(SPELL_HELLMAW_BANISH, pCreature);
        else
            pCreature->remove_auras(SPELL_HELLMAW_BANISH);
        break;
    case NPC_MURMUR:
    case NPC_INCITER_1:
    case NPC_INCITER_2:
    case NPC_INCITER_3:
    case NPC_INCITER_4:
    case NPC_INCITER_5:
        break;
    default:
        return;
    }

    m_mNpcEntryGuidStore[pCreature->GetEntry()] = pCreature->GetObjectGuid();
}

void instance_shadow_labyrinth::OnCreatureDeath(Creature* pCreature)
{
    switch (pCreature->GetEntry())
    {
    case NPC_CABAL_RITUALIST_1:
    case NPC_CABAL_RITUALIST_2:
    case NPC_CABAL_RITUALIST_3:
    case NPC_CABAL_RITUALIST_4:
        if (GetData(TYPE_RITUALISTS) > 0)
            SetData(TYPE_RITUALISTS, GetData(TYPE_RITUALISTS) - 1);
        break;
    default:
        return;
    }
}

void instance_shadow_labyrinth::SetData(uint32 uiType, uint32 uiData)
{
    if (uiType >= MAX_ENCOUNTER)
        return;
    m_auiEncounter[uiType] = uiData;

    switch (uiType)
    {
    case TYPE_RITUALISTS:
        if (uiData == 0)
        {
            // Release hellmaw
            if (Creature* hellmaw = GetSingleCreatureFromStorage(NPC_HELLMAW))
            {
                hellmaw->remove_auras(SPELL_HELLMAW_BANISH);
                DoScriptText(SAY_HELLMAW_INTRO, hellmaw);
            }
        }
        return; // don't bother with saving this
    case TYPE_INCITER:
        if (uiData == DONE)
            if (GameObject* door =
                    this->GetSingleGameObjectFromStorage(GO_REFECTORY_DOOR))
                door->SetGoState(GO_STATE_ACTIVE);
        break;
    }

    if (uiData == DONE)
    {
        OUT_SAVE_INST_DATA;

        std::ostringstream saveStream;
        saveStream << m_auiEncounter[0] << " " << m_auiEncounter[1] << " "
                   << m_auiEncounter[2] << " " << m_auiEncounter[3] << " "
                   << m_auiEncounter[4];

        m_strInstData = saveStream.str();

        SaveToDB();
        OUT_SAVE_INST_DATA_COMPLETE;
    }
}

uint32 instance_shadow_labyrinth::GetData(uint32 uiType)
{
    if (uiType >= MAX_ENCOUNTER)
        return 0;

    return m_auiEncounter[uiType];
}

void instance_shadow_labyrinth::Load(const char* chrIn)
{
    if (!chrIn)
    {
        OUT_LOAD_INST_DATA_FAIL;
        return;
    }

    OUT_LOAD_INST_DATA(chrIn);

    std::istringstream loadStream(chrIn);
    loadStream >> m_auiEncounter[0] >> m_auiEncounter[1] >> m_auiEncounter[2] >>
        m_auiEncounter[3] >> m_auiEncounter[4];

    for (uint8 i = 0; i < MAX_ENCOUNTER; ++i)
    {
        if (m_auiEncounter[i] == IN_PROGRESS)
            m_auiEncounter[i] = NOT_STARTED;
        if (i == TYPE_RITUALISTS)
            m_auiEncounter[i] = 0;
    }

    OUT_LOAD_INST_DATA_COMPLETE;
}

InstanceData* GetInstanceData_instance_shadow_labyrinth(Map* pMap)
{
    return new instance_shadow_labyrinth(pMap);
}

enum
{
    SPELL_RP_MURMURS_WRATH = 33331,
    NPC_CABAL_SUMMONER = 18634
};

struct trigger_murmur_door : ScriptedAI
{
    trigger_murmur_door(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_pInstance = (instance_shadow_labyrinth*)pCreature->GetInstanceData();
        m_bOpenedDoor = false;
    }

    instance_shadow_labyrinth* m_pInstance;
    bool m_bOpenedDoor;

    void Reset() override {}

    void SummonedMovementInform(
        Creature* pSummoned, movement::gen type, uint32 uiData) override
    {
        if (type == movement::gen::point && uiData == 100)
        {
            if (Creature* murmur =
                    m_pInstance->GetSingleCreatureFromStorage(NPC_MURMUR))
            {
                pSummoned->RemoveFlag(
                    UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE);
                murmur->CastSpell(pSummoned, SPELL_RP_MURMURS_WRATH, true);
            }
        }
    }

    void MoveInLineOfSight(Unit* pWho) override
    {
        if (m_pInstance && !m_bOpenedDoor &&
            pWho->GetTypeId() == TYPEID_PLAYER &&
            !((Player*)pWho)->isGameMaster() &&
            pWho->GetDistance(m_creature) < 25.0f &&
            m_pInstance->GetData(TYPE_VORPIL) == DONE)
        {
            if (GameObject* door = m_pInstance->GetSingleGameObjectFromStorage(
                    GO_SCREAMING_HALL_DOOR))
                door->SetGoState(GO_STATE_ACTIVE);

            if (Creature* sum = m_creature->SummonCreature(NPC_CABAL_SUMMONER,
                    -156.9f, -333.6f, 17.1f, 1.5f, TEMPSUMMON_DEAD_DESPAWN, 0))
            {
                sum->SetFlag(UNIT_FIELD_FLAGS,
                    UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_PASSIVE);
                sum->movement_gens.push(
                    new movement::PointMovementGenerator(
                        100, -156.6f, -312.2f, 17.1f, false, true),
                    movement::EVENT_ENTER_COMBAT);
            }

            m_bOpenedDoor = true;
        }
    }
};

CreatureAI* GetAI_trigger_murmur_door(Creature* pCreature)
{
    return new trigger_murmur_door(pCreature);
}

bool GOUse_Arcane_Container(Player* plr, GameObject* go)
{
    if (Creature* c =
            go->SummonCreature(22890, go->GetX(), go->GetY(), go->GetZ(), 2.6f,
                TEMPSUMMON_CORPSE_TIMED_DESPAWN, 5 * MINUTE * IN_MILLISECONDS))
        if (c->AI() && !plr->isGameMaster())
            c->AI()->AttackStart(plr);
    go->Delete();
    return true;
}

void AddSC_instance_shadow_labyrinth()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "instance_shadow_labyrinth";
    pNewScript->GetInstanceData = &GetInstanceData_instance_shadow_labyrinth;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "trigger_murmur_door";
    pNewScript->GetAI = &GetAI_trigger_murmur_door;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "go_arcane_container_one";
    pNewScript->pGOUse = &GOUse_Arcane_Container;
    pNewScript->RegisterSelf();
}
