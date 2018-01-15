/* Copyright (C) 2006 - 2012 ScriptDev2 <http://www.scriptdev2.com/>
 * Copyright (C) 2013 CoreCraft <https://www.worldofcorecraft.com/>
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
SDName: Blades_Edge_Mountains
SD%Complete: 100
SDComment: Quest support: 10821, 10594 (partially; half of it is in SmartAI)
SDCategory: Blade's Edge Mountains
EndScriptData */

/* ContentData
npc_oscillating_beamer
EndContentData */

#include "pet_ai.h"
#include "precompiled.h"

/*######
## npc_oscillating_beamer
######*/

enum
{
    NPC_OSCILLATING_TARGET = 21759,

    // This aura has been remade to indicate wheter someone is already firing
    // at this target
    SPELL_OSCILLATING_MARKER = 37418,
    SPELL_OSCILLATING_BEAM = 37697,
};

struct npc_oscillating_beamerAI : public ScriptedAI
{
    npc_oscillating_beamerAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        delay = 0;
    }

    uint32 delay;

    void UpdateAI(const uint32 diff) override
    {
        if (delay)
        {
            if (delay <= diff)
                delay = 0;
            else
            {
                delay -= diff;
                return;
            }
        }

        // Cast on 3 closest targets
        auto targets = GetCreatureListWithEntryInGrid(
            m_creature, NPC_OSCILLATING_TARGET, 80.0f);

        for (auto itr = targets.begin(); itr != targets.end();)
            if ((*itr)->GetDistance(m_creature) < 10.0f)
                itr = targets.erase(itr);
            else
                ++itr;

        if (targets.size() > 3)
        {
            std::sort(targets.begin(), targets.end(),
                [this](const Creature* a, const Creature* b)
                {
                    return m_creature->GetDistance(a) <
                           m_creature->GetDistance(b);
                });
            while (targets.size() != 3)
                targets.pop_back();
        }

        for (auto target : targets)
            m_creature->CastSpell(target, SPELL_OSCILLATING_BEAM, true);

        delay = 250;
    }
};

CreatureAI* GetAI_npc_oscillating_beamer(Creature* pCreature)
{
    return new npc_oscillating_beamerAI(pCreature);
}

/*######
## AddSC
######*/

void AddSC_blades_edge_mountains()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "npc_oscillating_beamer";
    pNewScript->GetAI = GetAI_npc_oscillating_beamer;
    pNewScript->RegisterSelf();
}
