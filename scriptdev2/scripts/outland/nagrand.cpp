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
SDName: Nagrand
SD%Complete: 90
SDComment: Quest support: 9991, 10085, 10646. TextId's unknown for
altruis_the_sufferer (npc_text)
SDCategory: Nagrand
EndScriptData */

/* ContentData
npc_altruis_the_sufferer
npc_creditmarker_visit_with_ancestors
EndContentData */

#include "escort_ai.h"
#include "precompiled.h"

/*######
## npc_altruis_the_sufferer
######*/

enum
{
    QUEST_SURVEY = 9991,
    QUEST_PUPIL = 10646,

    TAXI_PATH_ID = 532
};

bool GossipHello_npc_altruis_the_sufferer(Player* pPlayer, Creature* pCreature)
{
    if (pCreature->isQuestGiver())
        pPlayer->PrepareQuestMenu(pCreature->GetObjectGuid());

    // gossip before obtaining Survey the Land
    if (pPlayer->GetQuestStatus(QUEST_SURVEY) == QUEST_STATUS_NONE)
        pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT,
            "I see twisted steel and smell sundered earth.", GOSSIP_SENDER_MAIN,
            GOSSIP_ACTION_INFO_DEF + 10);

    // gossip when Survey the Land is incomplete (technically, after the flight)
    if (pPlayer->GetQuestStatus(QUEST_SURVEY) == QUEST_STATUS_INCOMPLETE)
        pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, "Well...?",
            GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 20);

    // wowwiki.com/Varedis
    if (pPlayer->GetQuestStatus(QUEST_PUPIL) == QUEST_STATUS_INCOMPLETE)
        pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT,
            "[PH] Story about Illidan's Pupil", GOSSIP_SENDER_MAIN,
            GOSSIP_ACTION_INFO_DEF + 30);

    pPlayer->SEND_GOSSIP_MENU(9419, pCreature->GetObjectGuid());
    return true;
}

bool GossipSelect_npc_altruis_the_sufferer(
    Player* pPlayer, Creature* pCreature, uint32 /*uiSender*/, uint32 uiAction)
{
    switch (uiAction)
    {
    case GOSSIP_ACTION_INFO_DEF + 10:
        pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, "Legion?",
            GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 11);
        pPlayer->SEND_GOSSIP_MENU(9420, pCreature->GetObjectGuid());
        break;
    case GOSSIP_ACTION_INFO_DEF + 11:
        pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, "And now?",
            GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 12);
        pPlayer->SEND_GOSSIP_MENU(9421, pCreature->GetObjectGuid());
        break;
    case GOSSIP_ACTION_INFO_DEF + 12:
        pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, "How do you see them now?",
            GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 13);
        pPlayer->SEND_GOSSIP_MENU(9422, pCreature->GetObjectGuid());
        break;
    case GOSSIP_ACTION_INFO_DEF + 13:
        pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, "Forge camps?",
            GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 14);
        pPlayer->SEND_GOSSIP_MENU(9423, pCreature->GetObjectGuid());
        break;
    case GOSSIP_ACTION_INFO_DEF + 14:
        pPlayer->SEND_GOSSIP_MENU(9424, pCreature->GetObjectGuid());
        break;

    case GOSSIP_ACTION_INFO_DEF + 20:
        pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, "Ok.", GOSSIP_SENDER_MAIN,
            GOSSIP_ACTION_INFO_DEF + 21);
        pPlayer->SEND_GOSSIP_MENU(9427, pCreature->GetObjectGuid());
        break;
    case GOSSIP_ACTION_INFO_DEF + 21:
        pPlayer->CLOSE_GOSSIP_MENU();
        pPlayer->AreaExploredOrEventHappens(QUEST_SURVEY);
        break;

    case GOSSIP_ACTION_INFO_DEF + 30:
        pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, "[PH] Story done",
            GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 31);
        pPlayer->SEND_GOSSIP_MENU(384, pCreature->GetObjectGuid());
        break;
    case GOSSIP_ACTION_INFO_DEF + 31:
        pPlayer->CLOSE_GOSSIP_MENU();
        pPlayer->AreaExploredOrEventHappens(QUEST_PUPIL);
        break;
    }
    return true;
}

bool QuestAccept_npc_altruis_the_sufferer(
    Player* pPlayer, Creature* /*pCreature*/, const Quest* /*pQuest*/)
{
    if (!pPlayer->GetQuestRewardStatus(QUEST_SURVEY)) // Survey the Land
    {
        pPlayer->CLOSE_GOSSIP_MENU();
        pPlayer->ActivateTaxiPathTo(TAXI_PATH_ID);
    }
    return true;
}

/*######
## npc_creditmarker_visit_with_ancestors (Quest 10085)
######*/

enum
{
    QUEST_VISIT_WITH_ANCESTORS = 10085
};

struct MANGOS_DLL_DECL npc_creditmarker_visit_with_ancestorsAI
    : public ScriptedAI
{
    npc_creditmarker_visit_with_ancestorsAI(Creature* pCreature)
      : ScriptedAI(pCreature)
    {
        Reset();
    }

    void Reset() override { delay = 0; }
    uint32 delay;

    void MoveInLineOfSight(Unit* pWho) override
    {
        if (delay)
            return;

        if (pWho->GetTypeId() == TYPEID_PLAYER &&
            !((Player*)pWho)->isGameMaster() &&
            m_creature->IsWithinDistInMap(pWho, 30.0f))
        {
            if (((Player*)pWho)->GetQuestStatus(QUEST_VISIT_WITH_ANCESTORS) ==
                QUEST_STATUS_INCOMPLETE)
            {
                uint32 creditMarkerId = m_creature->GetEntry();
                if ((creditMarkerId >= 18840) && (creditMarkerId <= 18843))
                {
                    // 18840: Sunspring, 18841: Laughing, 18842: Garadar, 18843:
                    // Bleeding
                    if (!((Player*)pWho)
                             ->GetReqKillOrCastCurrentCount(
                                 QUEST_VISIT_WITH_ANCESTORS, creditMarkerId))
                    {
                        float x, y, z, o = m_creature->GetAngle(pWho);
                        m_creature->GetPosition(x, y, z);
                        if (Creature* c = m_creature->SummonCreature(18904, x,
                                y, z, o, TEMPSUMMON_TIMED_DESPAWN, 10000))
                        {
                            switch (urand(0, 4))
                            {
                            case 0:
                                c->MonsterWhisper(
                                    "They lack control... Oshu'gun calls to "
                                    "them...",
                                    pWho);
                                break;
                            case 1:
                                c->MonsterWhisper(
                                    "Turn back, mortal... This is not your "
                                    "battle.",
                                    pWho);
                                break;
                            case 2:
                                c->MonsterWhisper(
                                    "You cannot stop them...", pWho);
                                break;
                            case 3:
                                c->MonsterWhisper(
                                    "We are infinite... Eternal...", pWho);
                                break;
                            case 4:
                                c->MonsterWhisper(
                                    "It is a beacon. A remnant of a forgotten "
                                    "era.",
                                    pWho);
                                break;
                            }
                        }
                        ((Player*)pWho)
                            ->KilledMonsterCredit(
                                creditMarkerId, m_creature->GetObjectGuid());
                        delay = 10000;
                    }
                }
            }
        }
    }

    void UpdateAI(const uint32 diff) override
    {
        if (delay)
        {
            if (delay <= diff)
                delay = 0;
            else
                delay -= diff;
        }
    }
};

CreatureAI* GetAI_npc_creditmarker_visit_with_ancestors(Creature* pCreature)
{
    return new npc_creditmarker_visit_with_ancestorsAI(pCreature);
}

/*######
## AddSC
######*/

void AddSC_nagrand()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "npc_altruis_the_sufferer";
    pNewScript->pGossipHello = &GossipHello_npc_altruis_the_sufferer;
    pNewScript->pGossipSelect = &GossipSelect_npc_altruis_the_sufferer;
    pNewScript->pQuestAcceptNPC = &QuestAccept_npc_altruis_the_sufferer;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "npc_creditmarker_visit_with_ancestors";
    pNewScript->GetAI = &GetAI_npc_creditmarker_visit_with_ancestors;
    pNewScript->RegisterSelf();
}
