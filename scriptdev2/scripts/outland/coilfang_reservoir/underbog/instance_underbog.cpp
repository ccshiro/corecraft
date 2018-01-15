/* Copyright (C) 2012 Corecraft
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
SDName: Instance_Hellfire_Ramparts
SD%Complete: 100
SDComment:
SDCategory: Underbog
EndScriptData */

#include "precompiled.h"
#include "underbog.h"

instance_underbog::instance_underbog(Map* pMap) : ScriptedInstance(pMap)
{
    Initialize();
}

void instance_underbog::Initialize()
{
}

void instance_underbog::OnCreatureCreate(Creature* pCreature)
{
    switch (pCreature->GetEntry())
    {
    case NPC_CLAW:
    case NPC_WINDCALLER_CLAW:
    case NPC_SWAMPLORD_MUSELEK:
        m_mNpcEntryGuidStore[pCreature->GetEntry()] =
            pCreature->GetObjectGuid();
        break;

    default:
        break;
    }
}

void instance_underbog::OnObjectCreate(GameObject* /*pGo*/)
{
}

InstanceData* GetInstanceData_instance_underbog(Map* pMap)
{
    return new instance_underbog(pMap);
}

void AddSC_instance_underbog()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "instance_underbog";
    pNewScript->GetInstanceData = &GetInstanceData_instance_underbog;
    pNewScript->RegisterSelf();
}
