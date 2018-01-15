/*
Name: zangarmarsh
Done %: 100
Comment: Currently only used for Zangarmarh's world pvp.
*/

/* Contained Scripts:
ZM_field_scout
*/

#include "precompiled.h"

/*######
## Zangarmarsh_field_scout
######*/

#define GOSSIP_ITEM_TAKE_FLAG \
    "Give me the battle standard, I will take control of Twin Spire Ruins."
#define GOSSIP_ITEM_BUY "I have marks to redeem!"

bool field_scout_hello(Player* player, Creature* creature)
{
    if (player->GetTeam() == player->outdoor_pvp_team())
    {
        player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, GOSSIP_ITEM_TAKE_FLAG,
            GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 1);
        player->ADD_GOSSIP_ITEM(GOSSIP_ICON_VENDOR, GOSSIP_ITEM_BUY,
            GOSSIP_SENDER_MAIN, GOSSIP_ACTION_TRADE);

        player->SEND_GOSSIP_MENU(
            player->GetGossipTextId(creature), creature->GetObjectGuid());
    }
    else
        player->SEND_VENDORLIST(creature->GetObjectGuid());

    return true;
}

bool field_scout_select(
    Player* player, Creature* creature, uint32 /*sender*/, uint32 action)
{
    // Take Flag
    if (action == GOSSIP_ACTION_INFO_DEF + 1)
    {
        if (player->GetTeam() == player->outdoor_pvp_team())
        {
            // Add Flag Aura
            if (creature->GetEntry() == 18564)
                player->CastSpell(player, 32431, true); // Horde
            else
                player->CastSpell(player, 32430, true); // Alliance
        }
    }
    else if (action == GOSSIP_ACTION_TRADE)
    {
        player->SEND_VENDORLIST(creature->GetObjectGuid());
    }

    player->CLOSE_GOSSIP_MENU();

    return true;
}

void AddSC_zangarmarsh()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "ZM_field_scout";
    pNewScript->pGossipHello = &field_scout_hello;
    pNewScript->pGossipSelect = &field_scout_select;
    pNewScript->RegisterSelf();
}
