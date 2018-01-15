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
SDName: Shattrath_City
SD%Complete: 100
SDComment: Quest support: 10004. Flask vendors
SDCategory: Shattrath City
EndScriptData */

/* ContentData
npc_khadgars_servant
npc_shattrathflaskvendors
EndContentData */

#include "escort_ai.h"
#include "precompiled.h"

/*######
## npc_khadgars_servant
######*/

enum
{
    SAY_KHAD_START = -1000489,
    SAY_KHAD_SERV_0 = -1000234,

    SAY_KHAD_SERV_1 = -1000235,
    SAY_KHAD_SERV_2 = -1000236,
    SAY_KHAD_SERV_3 = -1000237,
    SAY_KHAD_SERV_4 = -1000238,

    SAY_KHAD_SERV_5 = -1000239,
    SAY_KHAD_SERV_6 = -1000240,
    SAY_KHAD_SERV_7 = -1000241,

    SAY_KHAD_SERV_8 = -1000242,
    SAY_KHAD_SERV_9 = -1000243,
    SAY_KHAD_SERV_10 = -1000244,
    SAY_KHAD_SERV_11 = -1000245,

    SAY_KHAD_SERV_12 = -1000246,
    SAY_KHAD_SERV_13 = -1000247,

    SAY_KHAD_SERV_14 = -1000248,
    SAY_KHAD_SERV_15 = -1000249,
    SAY_KHAD_SERV_16 = -1000250,
    SAY_KHAD_SERV_17 = -1000251,

    SAY_KHAD_SERV_18 = -1000252,
    SAY_KHAD_SERV_19 = -1000253,
    SAY_KHAD_SERV_20 = -1000254,
    SAY_KHAD_SERV_21 = -1000255,

    SAY_KHAD_INJURED = -1000490,
    SAY_KHAD_MIND_YOU = -1000491,
    SAY_KHAD_MIND_ALWAYS = -1000492,
    SAY_KHAD_ALDOR_GREET = -1000493,
    SAY_KHAD_SCRYER_GREET = -1000494,
    SAY_KHAD_HAGGARD = -1000495,

    NPC_KHADGAR = 18166,
    NPC_SHANIR = 18597,
    NPC_IZZARD = 18622,
    NPC_ADYRIA = 18596,
    NPC_ANCHORITE = 19142,
    NPC_ARCANIST = 18547,
    NPC_HAGGARD = 19684,

    QUEST_CITY_LIGHT = 10211
};

struct MANGOS_DLL_DECL npc_khadgars_servantAI : public npc_escortAI
{
    npc_khadgars_servantAI(Creature* pCreature) : npc_escortAI(pCreature)
    {
        if (pCreature->GetOwner() &&
            pCreature->GetOwner()->GetTypeId() == TYPEID_PLAYER)
            Start(false, (Player*)pCreature->GetOwner());
        else
            logging.error(
                "SD2: npc_khadgars_servant can not obtain owner or owner is "
                "not a player.");

        Reset();
    }

    uint32 m_uiPointId;
    uint32 m_uiTalkTimer;
    uint32 m_uiTalkCount;
    uint32 m_uiRandomTalkCooldown;

    void Reset() override
    {
        m_uiTalkTimer = 2500;
        m_uiTalkCount = 0;
        m_uiPointId = 0;
        m_uiRandomTalkCooldown = 0;
    }

    void MoveInLineOfSight(Unit* pWho) override
    {
        if (!m_uiRandomTalkCooldown && pWho->GetTypeId() == TYPEID_UNIT &&
            m_creature->IsWithinDistInMap(pWho, 10.0f))
        {
            switch (pWho->GetEntry())
            {
            case NPC_HAGGARD:
                if (Player* pPlayer = GetPlayerForEscort())
                    DoScriptText(SAY_KHAD_HAGGARD, pWho, pPlayer);
                m_uiRandomTalkCooldown = 7500;
                break;
            case NPC_ANCHORITE:
                if (Player* pPlayer = GetPlayerForEscort())
                    DoScriptText(SAY_KHAD_ALDOR_GREET, pWho, pPlayer);
                m_uiRandomTalkCooldown = 7500;
                break;
            case NPC_ARCANIST:
                if (Player* pPlayer = GetPlayerForEscort())
                    DoScriptText(SAY_KHAD_SCRYER_GREET, pWho, pPlayer);
                m_uiRandomTalkCooldown = 7500;
                break;
            }
        }
    }

    void WaypointStart(uint32 uiPointId) override
    {
        if (uiPointId == 2)
            DoScriptText(SAY_KHAD_SERV_0, m_creature);
    }

    void WaypointReached(uint32 uiPointId) override
    {
        m_uiPointId = uiPointId;

        switch (uiPointId)
        {
        case 0:
            if (Creature* pKhadgar =
                    GetClosestCreatureWithEntry(m_creature, NPC_KHADGAR, 10.0f))
                DoScriptText(SAY_KHAD_START, pKhadgar);
            break;
        case 5:
        case 24:
        case 50:
        case 63:
        case 74:
        case 75:
            SetEscortPaused(true);
            break;
        case 34:
            if (Creature* pIzzard =
                    GetClosestCreatureWithEntry(m_creature, NPC_IZZARD, 10.0f))
                DoScriptText(SAY_KHAD_MIND_YOU, pIzzard);
            break;
        case 35:
            if (Creature* pAdyria =
                    GetClosestCreatureWithEntry(m_creature, NPC_ADYRIA, 10.0f))
                DoScriptText(SAY_KHAD_MIND_ALWAYS, pAdyria);
            break;
        }
    }

    void UpdateEscortAI(const uint32 uiDiff) override
    {
        if (m_uiRandomTalkCooldown)
        {
            if (m_uiRandomTalkCooldown <= uiDiff)
                m_uiRandomTalkCooldown = 0;
            else
                m_uiRandomTalkCooldown -= uiDiff;
        }

        if (HasEscortState(STATE_ESCORT_PAUSED))
        {
            if (m_uiTalkTimer <= uiDiff)
            {
                ++m_uiTalkCount;
                m_uiTalkTimer = 7500;

                Player* pPlayer = GetPlayerForEscort();

                if (!pPlayer)
                    return;

                switch (m_uiPointId)
                {
                case 5: // to lower city
                {
                    switch (m_uiTalkCount)
                    {
                    case 1:
                        DoScriptText(SAY_KHAD_SERV_1, m_creature, pPlayer);
                        break;
                    case 2:
                        DoScriptText(SAY_KHAD_SERV_2, m_creature, pPlayer);
                        break;
                    case 3:
                        DoScriptText(SAY_KHAD_SERV_3, m_creature, pPlayer);
                        break;
                    case 4:
                        DoScriptText(SAY_KHAD_SERV_4, m_creature, pPlayer);
                        SetEscortPaused(false);
                        break;
                    }
                    break;
                }
                case 24: // in lower city
                {
                    switch (m_uiTalkCount)
                    {
                    case 5:
                        if (Creature* pShanir = GetClosestCreatureWithEntry(
                                m_creature, NPC_SHANIR, 15.0f))
                            DoScriptText(SAY_KHAD_INJURED, pShanir, pPlayer);

                        DoScriptText(SAY_KHAD_SERV_5, m_creature, pPlayer);
                        break;
                    case 6:
                        DoScriptText(SAY_KHAD_SERV_6, m_creature, pPlayer);
                        break;
                    case 7:
                        DoScriptText(SAY_KHAD_SERV_7, m_creature, pPlayer);
                        SetEscortPaused(false);
                        break;
                    }
                    break;
                }
                case 50: // outside
                {
                    switch (m_uiTalkCount)
                    {
                    case 8:
                        DoScriptText(SAY_KHAD_SERV_8, m_creature, pPlayer);
                        break;
                    case 9:
                        DoScriptText(SAY_KHAD_SERV_9, m_creature, pPlayer);
                        break;
                    case 10:
                        DoScriptText(SAY_KHAD_SERV_10, m_creature, pPlayer);
                        break;
                    case 11:
                        DoScriptText(SAY_KHAD_SERV_11, m_creature, pPlayer);
                        SetEscortPaused(false);
                        break;
                    }
                    break;
                }
                case 63: // scryer
                {
                    switch (m_uiTalkCount)
                    {
                    case 12:
                        DoScriptText(SAY_KHAD_SERV_12, m_creature, pPlayer);
                        break;
                    case 13:
                        DoScriptText(SAY_KHAD_SERV_13, m_creature, pPlayer);
                        SetEscortPaused(false);
                        break;
                    }
                    break;
                }
                case 74: // aldor
                {
                    switch (m_uiTalkCount)
                    {
                    case 14:
                        DoScriptText(SAY_KHAD_SERV_14, m_creature, pPlayer);
                        break;
                    case 15:
                        DoScriptText(SAY_KHAD_SERV_15, m_creature, pPlayer);
                        break;
                    case 16:
                        DoScriptText(SAY_KHAD_SERV_16, m_creature, pPlayer);
                        break;
                    case 17:
                        DoScriptText(SAY_KHAD_SERV_17, m_creature, pPlayer);
                        SetEscortPaused(false);
                        break;
                    }
                    break;
                }
                case 75: // a'dal
                {
                    switch (m_uiTalkCount)
                    {
                    case 18:
                        DoScriptText(SAY_KHAD_SERV_18, m_creature, pPlayer);
                        break;
                    case 19:
                        DoScriptText(SAY_KHAD_SERV_19, m_creature, pPlayer);
                        break;
                    case 20:
                        DoScriptText(SAY_KHAD_SERV_20, m_creature, pPlayer);
                        break;
                    case 21:
                        DoScriptText(SAY_KHAD_SERV_21, m_creature, pPlayer);
                        pPlayer->AreaExploredOrEventHappens(QUEST_CITY_LIGHT);
                        SetEscortPaused(false);
                        break;
                    }
                    break;
                }
                }
            }
            else
                m_uiTalkTimer -= uiDiff;
        }
    }
};

CreatureAI* GetAI_npc_khadgars_servant(Creature* pCreature)
{
    return new npc_khadgars_servantAI(pCreature);
}

/*
##################################################
Shattrath City Flask Vendors provides flasks to people exalted with 3 factions:
Haldor the Compulsive
Arcanist Xorith
Both sell special flasks for use in Outlands 25man raids only,
purchasable for one Mark of Illidari each
Purchase requires exalted reputation with Scryers/Aldor, Cenarion Expedition and
The Sha'tar
##################################################
*/

bool GossipHello_npc_shattrathflaskvendors(Player* pPlayer, Creature* pCreature)
{
    if (pCreature->GetEntry() == 23484)
    {
        // Aldor vendor
        if (pCreature->isVendor() &&
            (pPlayer->GetReputationRank(932) == REP_EXALTED) &&
            (pPlayer->GetReputationRank(935) == REP_EXALTED) &&
            (pPlayer->GetReputationRank(942) == REP_EXALTED))
        {
            pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_VENDOR,
                GOSSIP_TEXT_BROWSE_GOODS, GOSSIP_SENDER_MAIN,
                GOSSIP_ACTION_TRADE);
            pPlayer->SEND_GOSSIP_MENU(11085, pCreature->GetObjectGuid());
        }
        else
        {
            pPlayer->SEND_GOSSIP_MENU(11083, pCreature->GetObjectGuid());
        }
    }

    if (pCreature->GetEntry() == 23483)
    {
        // Scryers vendor
        if (pCreature->isVendor() &&
            (pPlayer->GetReputationRank(934) == REP_EXALTED) &&
            (pPlayer->GetReputationRank(935) == REP_EXALTED) &&
            (pPlayer->GetReputationRank(942) == REP_EXALTED))
        {
            pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_VENDOR,
                GOSSIP_TEXT_BROWSE_GOODS, GOSSIP_SENDER_MAIN,
                GOSSIP_ACTION_TRADE);
            pPlayer->SEND_GOSSIP_MENU(11085, pCreature->GetObjectGuid());
        }
        else
        {
            pPlayer->SEND_GOSSIP_MENU(11084, pCreature->GetObjectGuid());
        }
    }

    return true;
}

bool GossipSelect_npc_shattrathflaskvendors(
    Player* pPlayer, Creature* pCreature, uint32 /*uiSender*/, uint32 uiAction)
{
    if (uiAction == GOSSIP_ACTION_TRADE)
        pPlayer->SEND_VENDORLIST(pCreature->GetObjectGuid());

    return true;
}

void AddSC_shattrath_city()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "npc_khadgars_servant";
    pNewScript->GetAI = &GetAI_npc_khadgars_servant;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "npc_shattrathflaskvendors";
    pNewScript->pGossipHello = &GossipHello_npc_shattrathflaskvendors;
    pNewScript->pGossipSelect = &GossipSelect_npc_shattrathflaskvendors;
    pNewScript->RegisterSelf();
}
