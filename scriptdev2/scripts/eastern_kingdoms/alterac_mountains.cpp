/* Copyright (C) 2006 - 2012 ScriptDev2 <http://www.scriptdev2.com/>
 * Copyright (C) 2014 Corecraft <https://www.worldofcorecraft.com>
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
SDName: Alterac_Mountains
SD%Complete: 0
SDComment: Placeholder
SDCategory: Alterac Mountains
EndScriptData */

/* ContentData
EndContentData */

#include "precompiled.h"

enum
{
    SPELL_CLEAVE = 19642,
};

struct npc_warmaster_laggrondAI : public CreatureAI
{
    npc_warmaster_laggrondAI(Creature* c) : CreatureAI(c) { Reset(); }

    uint32 cleave;

    void Reset() override { cleave = urand(10000, 20000); }

    void UpdateAI(uint32 diff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        if (cleave <= diff)
        {
            if (DoCastSpellIfCan(m_creature->getVictim(), SPELL_CLEAVE) ==
                CAST_OK)
                cleave = urand(10000, 20000);
        }
        else
            cleave -= diff;

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_npc_warmaster_laggrond(Creature* c)
{
    return new npc_warmaster_laggrondAI(c);
}

enum
{
    SPELL_SHIELD_BLOCK = 12169,
    SPELL_REVENGE = 19130,
};

struct npc_lieutenant_haggerdinAI : public CreatureAI
{
    npc_lieutenant_haggerdinAI(Creature* c) : CreatureAI(c) { Reset(); }

    uint32 shield_block;
    uint32 revenge;

    void Reset() override
    {
        shield_block = urand(10000, 20000);
        revenge = urand(4000, 8000);
    }

    void UpdateAI(uint32 diff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        if (shield_block <= diff)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_SHIELD_BLOCK) == CAST_OK)
                shield_block = urand(10000, 20000);
        }
        else
            shield_block -= diff;

        if (revenge <= diff)
        {
            if (DoCastSpellIfCan(m_creature->getVictim(), SPELL_REVENGE) ==
                CAST_OK)
                revenge = urand(4000, 8000);
        }
        else
            revenge -= diff;

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_npc_lieutenant_haggerdin(Creature* c)
{
    return new npc_lieutenant_haggerdinAI(c);
}

// returns: highest trinket id (or 0) that the player is eligible for
static uint32 av_trinket_id(Player* p)
{
    static const uint32 trinkets[2][6] = {// alliance trinkets
        {17691, 17900, 17901, 17902, 17903, 17904},
        // horde trinkets
        {17690, 17905, 17906, 17907, 17908, 17909}};
    static const uint32 quests[2][6] = {// alliance quests
        {7162, 7168, 7169, 7170, 7171, 7172},
        // horde quests
        {7161, 7163, 7164, 7165, 7166, 7167}};

    uint32 trinket = 0;

    for (int i = 0; i < 6; ++i)
    {
        uint32 quest_id = quests[p->GetTeam() == ALLIANCE ? 0 : 1][i];

        if (!p->GetQuestRewardStatus(quest_id))
            break;

        trinket = trinkets[p->GetTeam() == ALLIANCE ? 0 : 1][i];
    }

    return trinket;
}

bool GossipHello_av_trinket_refund(Player* p, Creature* c)
{
    if (c->isQuestGiver())
        p->PrepareQuestMenu(c->GetObjectGuid());

    uint32 trinket_id = av_trinket_id(p);
    if (trinket_id != 0 && !p->HasItemCount(trinket_id, 1, true))
    {
        std::stringstream ss;
        ss << (p->GetTeam() == ALLIANCE ? "Lieutenant" : "Warmaster")
           << ", I have lost my insignia. "
           << "Could you please supply me with a replacement?";
        p->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, ss.str(), GOSSIP_SENDER_MAIN,
            GOSSIP_ACTION_INFO_DEF + 10);
    }

    p->SEND_GOSSIP_MENU(p->GetGossipTextId(c), c->GetObjectGuid());
    return true;
}

bool GossipSelect_av_trinket_refund(Player* p, Creature*, uint32, uint32 action)
{
    if (action == GOSSIP_ACTION_INFO_DEF + 10)
    {
        uint32 trinket_id = av_trinket_id(p);
        if (trinket_id != 0 && !p->HasItemCount(trinket_id, 1, true))
            p->add_item(trinket_id, 1);
    }

    p->CLOSE_GOSSIP_MENU();
    return true;
}

void AddSC_alterac_mountains()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "npc_warmaster_laggrond";
    pNewScript->GetAI = &GetAI_npc_warmaster_laggrond;
    pNewScript->pGossipHello = &GossipHello_av_trinket_refund;
    pNewScript->pGossipSelect = &GossipSelect_av_trinket_refund;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "npc_lieutenant_haggerdin";
    pNewScript->GetAI = &GetAI_npc_lieutenant_haggerdin;
    pNewScript->pGossipHello = &GossipHello_av_trinket_refund;
    pNewScript->pGossipSelect = &GossipSelect_av_trinket_refund;
    pNewScript->RegisterSelf();
}
