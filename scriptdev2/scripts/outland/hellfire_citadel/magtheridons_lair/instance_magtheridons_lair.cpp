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
SDName: Instance_Magtheridons_Lair
SD%Complete: 100
SDComment:
SDCategory: Hellfire Citadel, Magtheridon's lair
EndScriptData */

#include "magtheridons_lair.h"
#include "Spell.h"

instance_magtheridons_lair::instance_magtheridons_lair(Map* pMap)
  : ScriptedInstance(pMap), m_releaseTimer(0), m_releaseStage(0)
{
    Initialize();
}

void instance_magtheridons_lair::Initialize()
{
    memset(&m_auiEncounter, 0, sizeof(m_auiEncounter));

    m_releaseTimer = 0;
}

bool instance_magtheridons_lair::IsEncounterInProgress() const
{
    for (auto& elem : m_auiEncounter)
        if (elem == IN_PROGRESS)
            return true;

    return false;
}

void instance_magtheridons_lair::OnCreatureCreate(Creature* pCreature)
{
    switch (pCreature->GetEntry())
    {
    case NPC_MAGTHERIDON:
        m_mNpcEntryGuidStore[NPC_MAGTHERIDON] = pCreature->GetObjectGuid();
        break;
    case NPC_CHANNELER:
        break;
    case NPC_BLAZE:
        m_blazes.push_back(
            std::pair<ObjectGuid, time_t>(pCreature->GetObjectGuid(), 0));
        break;
    }
}

void instance_magtheridons_lair::OnObjectCreate(GameObject* pGo)
{
    switch (pGo->GetEntry())
    {
    case GO_RAID_FX:
    case GO_MAGTHERIDON_DOOR:
    case GO_COLUMN_0:
    case GO_COLUMN_1:
    case GO_COLUMN_2:
    case GO_COLUMN_3:
    case GO_COLUMN_4:
    case GO_COLUMN_5:
        break;
    case GO_MANTICRON_CUBE:
        m_cubes.push_back(pGo->GetObjectGuid());
        return;
    case GO_BLAZE:
        m_blazeGo.push_back(pGo->GetObjectGuid());
        return;
    default:
        return;
    }
    m_mGoEntryGuidStore[pGo->GetEntry()] = pGo->GetObjectGuid();
}

void instance_magtheridons_lair::OnCreatureEnterCombat(Creature* pCreature)
{
    if (pCreature->GetEntry() == NPC_CHANNELER &&
        GetData(TYPE_MAGTHERIDON) != IN_PROGRESS &&
        GetData(TYPE_MAGTHERIDON) != DONE)
        SetData(TYPE_MAGTHERIDON, IN_PROGRESS);
}

void instance_magtheridons_lair::Update(uint32 uiDiff)
{
    if (m_releaseTimer)
    {
        if (m_releaseTimer <= uiDiff)
        {
            if (m_releaseStage == 1)
            {
                // Release
                if (Creature* mag =
                        GetSingleCreatureFromStorage(NPC_MAGTHERIDON))
                {
                    DoScriptText(EMOTE_MAG_FREE, mag);
                    DoScriptText(SAY_MAG_FREED, mag);
                    mag->HandleEmote(EMOTE_ONESHOT_BATTLEROAR);
                    mag->RemoveFlag(UNIT_FIELD_FLAGS,
                        UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_PASSIVE |
                            UNIT_FLAG_NOT_SELECTABLE);
                    m_releaseTimer = 2500;
                    m_releaseStage = 2;
                }
            }
            else if (m_releaseStage == 2)
            {
                if (Creature* mag =
                        GetSingleCreatureFromStorage(NPC_MAGTHERIDON))
                {
                    mag->remove_auras(SPELL_OOC_SHADOW_CAGE);
                    mag->movement_gens.remove_all(movement::gen::stopped);
                    mag->AI()->Pacify(false);
                    if (auto ai = dynamic_cast<ScriptedAI*>(mag->AI()))
                        ai->DoResetThreat();
                }
                ToggleCubeInteractable(true);
                m_releaseTimer = 0;
            }
            else
            {
                if (Creature* mag =
                        GetSingleCreatureFromStorage(NPC_MAGTHERIDON))
                    DoScriptText(EMOTE_MAG_1_MIN, mag);
                m_releaseTimer = 1 * MINUTE * IN_MILLISECONDS;
                m_releaseStage = 1;
            }
        }
        else
            m_releaseTimer -= uiDiff;
    }

    UpdateCubes(uiDiff);
}

void instance_magtheridons_lair::SetData(uint32 uiType, uint32 uiData)
{
    switch (uiType)
    {
    case TYPE_MAGTHERIDON:
        // Cubes always disabled when event changes
        ToggleCubeInteractable(false);

        if (uiData == IN_PROGRESS)
        {
            if (Creature* mag = GetSingleCreatureFromStorage(NPC_MAGTHERIDON))
                DoScriptText(EMOTE_MAG_START, mag);
            m_releaseTimer = 1 * MINUTE * IN_MILLISECONDS;
            m_releaseStage = 0;
            DoUseDoorOrButton(GO_MAGTHERIDON_DOOR);
        }
        else
        {
            DoUseDoorOrButton(GO_MAGTHERIDON_DOOR);
            m_releaseTimer = 0;

            // Cleanup blazes
            for (auto& elem : m_blazeGo)
                if (GameObject* go = instance->GetGameObject(elem))
                    go->Delete();
            m_blazeGo.clear();
            for (auto& elem : m_blazes)
            {
                if (elem.second > WorldTimer::time_no_syscall())
                    if (Creature* c = instance->GetCreature(elem.first))
                    {
                        c->SetVisibility(VISIBILITY_ON);
                        c->Kill(c);
                        c->Respawn();
                    }
                elem.second = 0;
            }

            if (uiData != DONE)
            {
                // Reset game objects
                if (GameObject* go = GetSingleGameObjectFromStorage(GO_RAID_FX))
                    go->SetGoState(GO_STATE_READY);
                if (GameObject* go =
                        GetSingleGameObjectFromStorage(GO_COLUMN_0))
                    go->SetGoState(GO_STATE_READY);
                if (GameObject* go =
                        GetSingleGameObjectFromStorage(GO_COLUMN_1))
                    go->SetGoState(GO_STATE_READY);
                if (GameObject* go =
                        GetSingleGameObjectFromStorage(GO_COLUMN_2))
                    go->SetGoState(GO_STATE_READY);
                if (GameObject* go =
                        GetSingleGameObjectFromStorage(GO_COLUMN_3))
                    go->SetGoState(GO_STATE_READY);
                if (GameObject* go =
                        GetSingleGameObjectFromStorage(GO_COLUMN_4))
                    go->SetGoState(GO_STATE_READY);
                if (GameObject* go =
                        GetSingleGameObjectFromStorage(GO_COLUMN_5))
                    go->SetGoState(GO_STATE_READY);
            }
        }
        break;
    default:
        return;
    }

    m_auiEncounter[uiType] = uiData;
}

uint32 instance_magtheridons_lair::GetData(uint32 uiType)
{
    if (uiType == TYPE_MAGTHERIDON)
        return m_auiEncounter[uiType];

    return 0;
}

/* Cube Functionality */
void instance_magtheridons_lair::ToggleCubeInteractable(bool enable)
{
    for (auto& elem : m_cubes)
        if (GameObject* go = instance->GetGameObject(elem))
        {
            if (enable)
                go->RemoveFlag(GAMEOBJECT_FLAGS, GO_FLAG_NO_INTERACT);
            else
                go->SetFlag(GAMEOBJECT_FLAGS, GO_FLAG_NO_INTERACT);
        }
}

void instance_magtheridons_lair::OnCubeClick(GameObject* go, Player* p)
{
    if (p->has_aura(SPELL_MIND_EXHAUSTION))
        return;

    if (go->HasFlag(GAMEOBJECT_FLAGS, GO_FLAG_NO_INTERACT))
        return;

    Creature* trigger =
        GetClosestCreatureWithEntry(go, NPC_HELLFIRE_RAID_TRIGGER, 25.0f);
    if (!trigger)
        return;

    if (std::find(active_hellfire_triggers.begin(),
            active_hellfire_triggers.end(),
            trigger->GetObjectGuid()) != active_hellfire_triggers.end())
        return;

    active_hellfire_triggers.push_back(trigger->GetObjectGuid());

    p->CastSpell(p, SPELL_SHADOW_GRASP, true);

    // It should take a bit to start the channel
    trigger->queue_action(1000, [this, trigger]()
        {
            if (std::find(active_hellfire_triggers.begin(),
                    active_hellfire_triggers.end(),
                    trigger->GetObjectGuid()) == active_hellfire_triggers.end())
                return;
            trigger->CastSpell(trigger, SPELL_SHADOW_GRASP_TRIGGER,
                false); // Spell Script Target
        });
}

void instance_magtheridons_lair::UpdateCubes(uint32 /*uiDiff*/)
{
    Creature* mag = GetSingleCreatureFromStorage(NPC_MAGTHERIDON);
    if (!mag)
        return;

    for (auto itr = active_hellfire_triggers.begin();
         itr != active_hellfire_triggers.end();)
    {
        Creature* trigger = instance->GetCreature(*itr);
        if (trigger)
        {
            bool found = false;
            for (auto& player :
                GetAllPlayersInObjectRangeCheckInCell(trigger, 15.0f))
            {
                if (player->has_aura(SPELL_SHADOW_GRASP))
                {
                    found = true;
                    break;
                }
            }
            if (found)
            {
                ++itr;
                continue;
            }
        }

        trigger->InterruptNonMeleeSpells(false);
        itr = active_hellfire_triggers.erase(itr);
    }

    if (mag->GetAuraCount(SPELL_SHADOW_GRASP_TRIGGER) == 5)
    {
        if (!mag->has_aura(SPELL_SHADOW_CAGE))
        {
            mag->InterruptNonMeleeSpells(false);
            mag->CastSpell(mag, SPELL_SHADOW_CAGE, true);
        }
    }
    else
    {
        // Remove Shadow Cage if present
        mag->remove_auras(SPELL_SHADOW_CAGE);
    }
}

InstanceData* GetInstanceData_instance_magtheridons_lair(Map* pMap)
{
    return new instance_magtheridons_lair(pMap);
}

void AddSC_instance_magtheridons_lair()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "instance_magtheridons_lair";
    pNewScript->GetInstanceData = &GetInstanceData_instance_magtheridons_lair;
    pNewScript->RegisterSelf();
}
