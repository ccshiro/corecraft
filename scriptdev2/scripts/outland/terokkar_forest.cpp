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
SDName: Terokkar_Forest
SD%Complete: 80
SDComment: Quest support: 9889, 11096, 11093.
SDCategory: Terokkar Forest
EndScriptData */

/* ContentData
npc_slim
EndContentData */

#include "escort_ai.h"
#include "pet_ai.h"
#include "precompiled.h"

/*######
## npc_slim
######*/

enum
{
    FACTION_CONSORTIUM = 933
};

bool GossipHello_npc_slim(Player* pPlayer, Creature* pCreature)
{
    if (pCreature->isVendor() &&
        pPlayer->GetReputationRank(FACTION_CONSORTIUM) >= REP_FRIENDLY)
    {
        pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_VENDOR, GOSSIP_TEXT_BROWSE_GOODS,
            GOSSIP_SENDER_MAIN, GOSSIP_ACTION_TRADE);
        pPlayer->SEND_GOSSIP_MENU(9896, pCreature->GetObjectGuid());
    }
    else
        pPlayer->SEND_GOSSIP_MENU(9895, pCreature->GetObjectGuid());

    return true;
}

bool GossipSelect_npc_slim(
    Player* pPlayer, Creature* pCreature, uint32 /*uiSender*/, uint32 uiAction)
{
    if (uiAction == GOSSIP_ACTION_TRADE)
        pPlayer->SEND_VENDORLIST(pCreature->GetObjectGuid());

    return true;
}

void AddSC_terokkar_forest()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "npc_slim";
    pNewScript->pGossipHello = &GossipHello_npc_slim;
    pNewScript->pGossipSelect = &GossipSelect_npc_slim;
    pNewScript->RegisterSelf();
}
