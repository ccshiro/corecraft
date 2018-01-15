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
SDName: Netherstorm
SD%Complete: 100
SDComment: 10221
SDCategory: Netherstorm
EndScriptData */

/* ContentData
npc_dr_boom
EndContentData */

#include "escort_ai.h"
#include "pet_ai.h"
#include "precompiled.h"

/*######
## npc_dr_boom
######*/

enum
{
    SPELL_THROW_DYNAMITE = 35276,
    NPC_BOOM_BOT = 19692
};

struct MANGOS_DLL_DECL npc_dr_boomAI : public ScriptedAI
{
    npc_dr_boomAI(Creature* pCreature) : ScriptedAI(pCreature) { Reset(); }

    uint32 m_turnTimer;
    uint32 m_bombTimer;
    float m_facing;
    void Reset() override
    {
        m_facing = 3.3f;
        m_turnTimer = 500;
        m_bombTimer = urand(2000, 5000);
        SetCombatMovement(false);
    }

    void JustDied(Unit*) override
    {
        auto cs =
            GetCreatureListWithEntryInGrid(m_creature, NPC_BOOM_BOT, 60.0f);
        for (auto c : cs)
        {
            c->ForcedDespawn();
            c->SetRespawnTime(m_creature->GetRespawnDelay());
        }
    }

    void UpdateAI(const uint32 diff) override
    {
        m_creature->SetTargetGuid(ObjectGuid());
        m_creature->SetFacingTo(m_facing);

        if (m_turnTimer <= diff)
        {
            static const float directions[6] = {
                3.5f, 3.1f, 2.8f, 2.6f, 2.3f, 2.0f};
            m_facing = directions[urand(0, 5)];
            m_turnTimer = 500;
        }
        else
            m_turnTimer -= diff;

        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        if (m_bombTimer <= diff)
        {
            if (Unit* tar = m_creature->SelectAttackingTarget(
                    ATTACKING_TARGET_RANDOM, 0, SPELL_THROW_DYNAMITE))
                m_creature->CastSpell(tar->GetX(), tar->GetY(), tar->GetZ(),
                    SPELL_THROW_DYNAMITE, false);
            m_bombTimer = urand(2000, 5000);
        }
        else
            m_bombTimer -= diff;
    }
};

CreatureAI* GetAI_npc_dr_boom(Creature* pCreature)
{
    return new npc_dr_boomAI(pCreature);
}

void AddSC_netherstorm()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "npc_dr_boom";
    pNewScript->GetAI = &GetAI_npc_dr_boom;
    pNewScript->RegisterSelf();
}
