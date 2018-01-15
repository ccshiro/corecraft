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
SDName: Karazhan
SD%Complete: 100
SDComment: Support for Barnes (Opera controller), Berthold (Doorman) and
Spectral Stable Hand. Gossip for Archmage Alturus and Archmage Leryda
implemented too.
SDCategory: Karazhan
EndScriptData */

/* ContentData
npc_barnes
npc_berthold
npc_spectral_stable_hand
EndContentData */

#include "karazhan.h"
#include "escort_ai.h"

/*######
# npc_barnesAI
######*/

#define GOSSIP_READY "I'm not an actor."

#define SAY_START_BARNES -1532115
#define SAY_OZ_INTRO1 \
    "Finally, everything is in place. Are you ready for your big stage debut?"
#define OZ_GOSSIP1 "I'm not an actor."
#define SAY_OZ_INTRO2 "Don't worry, you'll be fine. You look like a natural!"
#define OZ_GOSSIP2 "Ok, I'll give it a try, then."

#define SAY_RAJ_INTRO1                                                      \
    "The romantic plays are really tough, but you'll do better this time. " \
    "You have TALENT. Ready?"
#define RAJ_GOSSIP1 "I've never been more ready."

#define OZ_GM_GOSSIP1 "[GM] Change event to EVENT_OZ"
#define OZ_GM_GOSSIP2 "[GM] Change event to EVENT_HOOD"
#define OZ_GM_GOSSIP3 "[GM] Change event to EVENT_RAJ"
#define OZ_GM_GOSSIP4 "[GM] Start event without RP"
#define OZ_GM_GOSSIP5 "[GM] Toggle door"

#define MOROES_STILL_ALIVE_GOSSIP 15010
#define RAJ_WIPED_GOSSIP 15011
#define RAJ_WIPED_ANSWER "I've never been more ready."
#define HOOD_WIPED_GOSSIP 15012
#define HOOD_WIPED_ANSWER "The wolf's going down."
#define OZ_WIPED_GOSSIP 15013
#define OZ_WIPED_ANSWER "I'll nail it!"

#define BARNES_GOSSIP_EVENT_OVER 15014

struct Dialogue
{
    int32 iTextId;
    uint32 uiTimer;
};

static Dialogue aOzDialogue[4] = {
    {-1532103, 6000}, {-1532104, 18000}, {-1532105, 9000}, {-1532106, 15000}};

static Dialogue aHoodDialogue[4] = {
    {-1532107, 6000}, {-1532108, 10000}, {-1532109, 14000}, {-1532110, 15000}};

static Dialogue aRAJDialogue[4] = {
    {-1532111, 5000}, {-1532112, 7000}, {-1532113, 14000}, {-1532114, 14000}};

struct Spawn
{
    uint32 uiEntry;
    float fPosX;
};

// Entries and spawn locations for creatures in Oz event
#define OZ_SPAWN_SIZE 4
Spawn aSpawns_OZ[OZ_SPAWN_SIZE] = {
    {17535, -10896.0f}, // Dorothee
    {17546, -10891.0f}, // Roar
    {17547, -10884.0f}, // Tinhead
    {17543, -10902.0f}, // Strawman
};
Spawn Spawn_HOOD = {17603, -10892.0f}; // Grandmother
Spawn Spawn_RAJ = {17534, -10893.0f};  // Julianne

enum
{
    NPC_SPOTLIGHT = 19525,

    SPELL_SPOTLIGHT = 25824,
    SPELL_TUXEDO = 32616,

    GO_HOOD_ONE = 24382,
    GO_HOOD_TWO = 24383,
    GO_HOOD_THREE = 24384,
    GO_HOOD_FOUR = 24381,
    GO_BACKDROP = 24346,
};

const float SPAWN_Z = 90.5f;
const float SPAWN_Y = -1758.0f;
const float SPAWN_O = 4.738f;

struct MANGOS_DLL_DECL npc_barnesAI : public npc_escortAI
{
    npc_barnesAI(Creature* pCreature) : npc_escortAI(pCreature)
    {
        m_pInstance = (instance_karazhan*)pCreature->GetInstanceData();
        Reset();
    }

    void CleanOut(std::vector<ObjectGuid> golist)
    {
        for (auto& elem : golist)
        {
            if (GameObject* pGo = m_creature->GetMap()->GetGameObject(elem))
                pGo->SetRespawnTime(7 * 24 * 60 * 60);
        }
    }

    void CleanOut(uint32 go_entry)
    {
        if (!m_pInstance)
            return;
        if (go_entry == GO_OZ_BACKDROP)
        {
            if (GameObject* pGo = m_creature->GetMap()->GetGameObject(
                    m_pInstance->m_OZBackdrop))
                pGo->SetRespawnTime(7 * 24 * 60 * 60);
        }
        else if (go_entry == GO_HOOD_BACKDROP)
        {
            if (GameObject* pGo = m_creature->GetMap()->GetGameObject(
                    m_pInstance->m_HOODBackdrop))
                pGo->SetRespawnTime(7 * 24 * 60 * 60);
        }
        else if (go_entry == GO_RAJ_BACKDROP)
        {
            if (GameObject* pGo = m_creature->GetMap()->GetGameObject(
                    m_pInstance->m_RAJBackdrop))
                pGo->SetRespawnTime(7 * 24 * 60 * 60);
        }
    }

    void SpawnScene(std::vector<ObjectGuid> golist)
    {
        for (auto& elem : golist)
        {
            if (GameObject* pGo = m_creature->GetMap()->GetGameObject(elem))
                pGo->Respawn();
        }
    }

    void SpawnScene(uint32 go_entry)
    {
        if (go_entry == GO_OZ_BACKDROP)
        {
            if (GameObject* pGo = m_creature->GetMap()->GetGameObject(
                    m_pInstance->m_OZBackdrop))
                pGo->Respawn();
        }
        else if (go_entry == GO_HOOD_BACKDROP)
        {
            if (GameObject* pGo = m_creature->GetMap()->GetGameObject(
                    m_pInstance->m_HOODBackdrop))
                pGo->Respawn();
        }
        else if (go_entry == GO_RAJ_BACKDROP)
        {
            if (GameObject* pGo = m_creature->GetMap()->GetGameObject(
                    m_pInstance->m_RAJBackdrop))
                pGo->Respawn();
        }
    }

    void RemoveAllSceneObjects()
    {
        CleanOut(m_pInstance->m_OperaBalcony);
        CleanOut(m_pInstance->m_OperaHay);
        CleanOut(m_pInstance->m_OperaHouses);
        CleanOut(m_pInstance->m_OperaMoon);
        CleanOut(m_pInstance->m_OperaTrees);
        CleanOut(GO_RAJ_BACKDROP);
        CleanOut(GO_HOOD_BACKDROP);
        CleanOut(GO_OZ_BACKDROP);
    }

    instance_karazhan* m_pInstance;

    ObjectGuid m_spotlightGuid;

    std::map<uint64 /* GUID */, GameObject*> m_OperaSceneObjects;

    uint32 m_uiTalkCount;
    uint32 m_uiTalkTimer;
    uint32 m_uiWipeTimer;

    void Reset() override
    {
        m_spotlightGuid.Clear();

        m_uiTalkCount = 0;
        m_uiTalkTimer = 2000;
        m_uiWipeTimer = 5000;
    }

    void StartEvent()
    {
        if (!m_pInstance)
            return;

        m_pInstance->SetData(TYPE_OPERA, IN_PROGRESS);

        // Set remaining mobs to 4
        if (m_pInstance->GetData(DATA_OPERA_EVENT) == EVENT_OZ)
            m_pInstance->SetData(DATA_OPERA_OZ_COUNT, OZ_SPAWN_SIZE);

        DoScriptText(SAY_START_BARNES, m_creature);

        RemoveAllSceneObjects();
        switch (m_pInstance->GetData(DATA_OPERA_EVENT))
        {
        case EVENT_OZ:
            SpawnScene(GO_OZ_BACKDROP);
            SpawnScene(m_pInstance->m_OperaHay);
            break;
        case EVENT_HOOD:
            SpawnScene(GO_HOOD_BACKDROP);
            SpawnScene(m_pInstance->m_OperaTrees);
            SpawnScene(m_pInstance->m_OperaHouses);
            break;
        case EVENT_RAJ:
            SpawnScene(GO_RAJ_BACKDROP);
            SpawnScene(m_pInstance->m_OperaMoon);
            SpawnScene(m_pInstance->m_OperaBalcony);
            break;
        default:
            logging.error("SD2: Barnes Opera Event - Wrong EventId set: %d",
                m_pInstance->GetData(DATA_OPERA_EVENT));
            return;
        }

        Start(false, NULL, NULL, true);
    }

    void WaypointReached(uint32 uiPointId) override
    {
        if (!m_pInstance)
            return;

        switch (uiPointId)
        {
        case 0:
            m_creature->CastSpell(m_creature, SPELL_TUXEDO, false);
            m_pInstance->DoUseDoorOrButton(GO_STAGE_DOOR_LEFT);
            break;
        case 4:
            m_uiTalkCount = 0;
            SetEscortPaused(true);

            // Phase the crowd
            m_creature->SetFacingTo(4.65);

            if (Creature* pSpotlight = m_creature->SummonCreature(NPC_SPOTLIGHT,
                    m_creature->GetX(), m_creature->GetY(), m_creature->GetZ(),
                    0.0f, TEMPSUMMON_TIMED_OR_DEAD_DESPAWN, 60000))
            {
                pSpotlight->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
                pSpotlight->CastSpell(pSpotlight, SPELL_SPOTLIGHT, false);
                m_spotlightGuid = pSpotlight->GetObjectGuid();
            }
            break;
        case 8:
        {
            GameObject* pGo =
                m_pInstance->GetSingleGameObjectFromStorage(GO_STAGE_DOOR_LEFT);
            if (pGo)
                pGo->SetGoState(GO_STATE_READY);
        }
        break;
        case 9:
        {
            PrepareEncounter();
            m_pInstance->DoUseDoorOrButton(GO_STAGE_CURTAIN);

            m_creature->SetOrientation(1.4);
        }
        break;
        }
    }

    void Talk(uint32 uiCount)
    {
        if (!m_pInstance)
            return;

        int32 iTextId = 0;

        switch (m_pInstance->GetData(DATA_OPERA_EVENT))
        {
        case EVENT_OZ:
            if (aOzDialogue[uiCount].iTextId)
                iTextId = aOzDialogue[uiCount].iTextId;
            if (aOzDialogue[uiCount].uiTimer)
                m_uiTalkTimer = aOzDialogue[uiCount].uiTimer;
            break;

        case EVENT_HOOD:
            if (aHoodDialogue[uiCount].iTextId)
                iTextId = aHoodDialogue[uiCount].iTextId;
            if (aHoodDialogue[uiCount].uiTimer)
                m_uiTalkTimer = aHoodDialogue[uiCount].uiTimer;
            break;

        case EVENT_RAJ:
            if (aRAJDialogue[uiCount].iTextId)
                iTextId = aRAJDialogue[uiCount].iTextId;
            if (aRAJDialogue[uiCount].uiTimer)
                m_uiTalkTimer = aRAJDialogue[uiCount].uiTimer;
            break;
        }

        if (iTextId)
            DoScriptText(iTextId, m_creature);
    }

    void PrepareEncounter()
    {
        if (!m_pInstance)
            return;

        LOG_DEBUG(logging,
            "SD2: Barnes Opera Event - Introduction complete - preparing "
            "encounter %d",
            m_pInstance->GetData(DATA_OPERA_EVENT));

        switch (m_pInstance->GetData(DATA_OPERA_EVENT))
        {
        case EVENT_OZ:
            for (auto& elem : aSpawns_OZ)
            {
                m_creature->SummonCreature(elem.uiEntry, elem.fPosX, SPAWN_Y,
                    SPAWN_Z, SPAWN_O, TEMPSUMMON_CORPSE_TIMED_DESPAWN, 10000);
            }
            break;
        case EVENT_HOOD:
            m_creature->SummonCreature(Spawn_HOOD.uiEntry, Spawn_HOOD.fPosX,
                SPAWN_Y, SPAWN_Z, SPAWN_O, TEMPSUMMON_CORPSE_TIMED_DESPAWN,
                sWorld::Instance()->getConfig(
                    CONFIG_UINT32_CORPSE_DECAY_WORLDBOSS) *
                    1000);
            break;
        case EVENT_RAJ:
            m_creature->SummonCreature(Spawn_RAJ.uiEntry, Spawn_RAJ.fPosX,
                SPAWN_Y, SPAWN_Z, SPAWN_O, TEMPSUMMON_CORPSE_TIMED_DESPAWN,
                sWorld::Instance()->getConfig(
                    CONFIG_UINT32_CORPSE_DECAY_WORLDBOSS) *
                    1000);
            break;
        default:
            logging.error("SD2: Barnes Opera Event - Wrong EventId set: %d",
                m_pInstance->GetData(DATA_OPERA_EVENT));
            break;
        }
    }

    void UpdateEscortAI(const uint32 uiDiff) override
    {
        if (HasEscortState(STATE_ESCORT_PAUSED))
        {
            if (m_uiTalkTimer < uiDiff)
            {
                if (m_uiTalkCount > 3)
                {
                    if (Creature* pSpotlight =
                            m_creature->GetMap()->GetCreature(m_spotlightGuid))
                        pSpotlight->ForcedDespawn();

                    SetEscortPaused(false);
                    return;
                }

                Talk(m_uiTalkCount++);
            }
            else
                m_uiTalkTimer -= uiDiff;
        }
    }
};

CreatureAI* GetAI_npc_barnesAI(Creature* pCreature)
{
    return new npc_barnesAI(pCreature);
}

bool GossipHello_npc_barnes(Player* pPlayer, Creature* pCreature)
{
    if (instance_karazhan* pInstance =
            (instance_karazhan*)pCreature->GetInstanceData())
    {
        if (pInstance->GetData(TYPE_OPERA) == IN_PROGRESS)
            return true;

        // Check for death of Moroes and if opera event is not done already
        if (pInstance->GetData(TYPE_MOROES) == DONE &&
            pInstance->GetData(TYPE_OPERA) != DONE)
        {
            if (pPlayer->isGameMaster()) // for GMs we add the possibility to
                                         // change the event
            {
                pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_DOT, OZ_GM_GOSSIP1,
                    GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 3);
                pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_DOT, OZ_GM_GOSSIP2,
                    GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 4);
                pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_DOT, OZ_GM_GOSSIP3,
                    GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 5);
                pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_DOT, OZ_GM_GOSSIP4,
                    GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 6);
                pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_DOT, OZ_GM_GOSSIP5,
                    GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 7);
            }

            if (!pInstance->m_bHasWipedOnOpera)
            {
                pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, OZ_GOSSIP1,
                    GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 1);
                pPlayer->SEND_GOSSIP_MENU(8970, pCreature->GetObjectGuid());
            }
            else
            {
                switch (pInstance->GetData(DATA_OPERA_EVENT))
                {
                case EVENT_OZ:
                    pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, OZ_WIPED_ANSWER,
                        GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 2);
                    pPlayer->SEND_GOSSIP_MENU(
                        OZ_WIPED_GOSSIP, pCreature->GetObjectGuid());
                    break;

                case EVENT_HOOD:
                    pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT,
                        HOOD_WIPED_ANSWER, GOSSIP_SENDER_MAIN,
                        GOSSIP_ACTION_INFO_DEF + 2);
                    pPlayer->SEND_GOSSIP_MENU(
                        HOOD_WIPED_GOSSIP, pCreature->GetObjectGuid());
                    break;

                case EVENT_RAJ:
                    pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, RAJ_WIPED_ANSWER,
                        GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 2);
                    pPlayer->SEND_GOSSIP_MENU(
                        RAJ_WIPED_GOSSIP, pCreature->GetObjectGuid());
                    break;
                }
            }
            return true;
        }
        else if (pInstance->GetData(TYPE_MOROES) != DONE)
        {
            pPlayer->SEND_GOSSIP_MENU(
                MOROES_STILL_ALIVE_GOSSIP, pCreature->GetObjectGuid());
            return true;
        }
    }

    // Moroes and opera both done:
    pCreature->HandleEmoteCommand(TEXTEMOTE_CLAP); // /clap
    pPlayer->SEND_GOSSIP_MENU(
        BARNES_GOSSIP_EVENT_OVER, pCreature->GetObjectGuid());
    return true;
}

bool GossipSelect_npc_barnes(
    Player* pPlayer, Creature* pCreature, uint32 /*uiSender*/, uint32 uiAction)
{
    npc_barnesAI* pBarnesAI = dynamic_cast<npc_barnesAI*>(pCreature->AI());
    instance_karazhan* pInstance =
        (instance_karazhan*)pCreature->GetInstanceData();

    if (pInstance)
        if (pInstance->GetData(TYPE_MOROES) != DONE ||
            pInstance->GetData(TYPE_OPERA) == IN_PROGRESS ||
            pInstance->GetData(TYPE_OPERA) == DONE)
            return true;

    switch (uiAction)
    {
    case GOSSIP_ACTION_INFO_DEF + 1:
        pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, OZ_GOSSIP2,
            GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 2);
        pPlayer->SEND_GOSSIP_MENU(8971, pCreature->GetObjectGuid());
        break;
    case GOSSIP_ACTION_INFO_DEF + 2:
        pPlayer->CLOSE_GOSSIP_MENU();
        if (pBarnesAI)
            pBarnesAI->StartEvent();
        break;
    case GOSSIP_ACTION_INFO_DEF + 3:
        pPlayer->CLOSE_GOSSIP_MENU();
        if (pBarnesAI && pPlayer->isGameMaster() && pInstance)
            pInstance->SetData(DATA_OPERA_EVENT, EVENT_OZ);
        logging.info("SD2: %s manually set Opera event to EVENT_OZ",
            pPlayer->GetGuidStr().c_str());
        break;
    case GOSSIP_ACTION_INFO_DEF + 4:
        pPlayer->CLOSE_GOSSIP_MENU();
        if (pBarnesAI && pPlayer->isGameMaster() && pInstance)
            pInstance->SetData(DATA_OPERA_EVENT, EVENT_HOOD);
        ;
        logging.info("SD2: %s manually set Opera event to EVENT_HOOD",
            pPlayer->GetGuidStr().c_str());
        break;
    case GOSSIP_ACTION_INFO_DEF + 5:
        pPlayer->CLOSE_GOSSIP_MENU();
        if (pBarnesAI && pPlayer->isGameMaster() && pInstance)
            pInstance->SetData(DATA_OPERA_EVENT, EVENT_RAJ);
        ;
        logging.info("SD2: %s manually set Opera event to EVENT_RAJ",
            pPlayer->GetGuidStr().c_str());
        break;
    case GOSSIP_ACTION_INFO_DEF + 6:
        pPlayer->CLOSE_GOSSIP_MENU();
        if (pBarnesAI && pPlayer->isGameMaster())
        {
            if (pInstance)
            {
                pInstance->SetData(TYPE_OPERA, IN_PROGRESS);
                pInstance->SetData(DATA_OPERA_OZ_COUNT, OZ_SPAWN_SIZE);
            }
            pBarnesAI->PrepareEncounter();
            if (pBarnesAI->m_pInstance)
                pBarnesAI->m_pInstance->DoUseDoorOrButton(GO_STAGE_CURTAIN);
        }
        break;
    case GOSSIP_ACTION_INFO_DEF + 7:
        pPlayer->CLOSE_GOSSIP_MENU();
        if (pBarnesAI && pPlayer->isGameMaster() && pBarnesAI->m_pInstance)
            pBarnesAI->m_pInstance->DoUseDoorOrButton(GO_STAGE_DOOR_LEFT);
        break;
    }

    return true;
}

/*###
# npc_berthold
####*/

enum
{
    SPELL_GUARDIAN_LIB_TELEPORT = 39567,

    TEXT_INTRO_ID = 8401,
    TEXT_ARAN_ID = 10741,
    TEXT_THIS_PLACE_ID = 30000,
    TEXT_WHERE_MEDIVH_ID = 30001,
    TEXT_NAVIGATE_TOWER_ID = 30002,
};

#define GOSSIP_THIS_PLACE "What is this place?"
#define GOSSIP_WHERE_MEDIVH "Where is Medivh?"
#define GOSSIP_NAVIGATE_TOWER "How do you navigate the tower?"
#define GOSSIP_ITEM_TELEPORT "Please transport me to the Guardian's Library."

bool GossipHello_npc_berthold(Player* pPlayer, Creature* pCreature)
{
    if (ScriptedInstance* pInstance =
            (ScriptedInstance*)pCreature->GetInstanceData())
    {
        pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, GOSSIP_THIS_PLACE,
            GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 10);
        pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, GOSSIP_WHERE_MEDIVH,
            GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 11);
        pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, GOSSIP_NAVIGATE_TOWER,
            GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 12);
        if (pInstance->GetData(TYPE_ARAN) == DONE)
        {
            pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, GOSSIP_ITEM_TELEPORT,
                GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 20);
            pPlayer->SEND_GOSSIP_MENU(TEXT_ARAN_ID, pCreature->GetObjectGuid());
        }
        else
            pPlayer->SEND_GOSSIP_MENU(
                TEXT_INTRO_ID, pCreature->GetObjectGuid());
    }

    return true;
}

bool GossipSelect_npc_berthold(
    Player* pPlayer, Creature* pCreature, uint32 /*uiSender*/, uint32 uiAction)
{
    switch (uiAction)
    {
    case GOSSIP_ACTION_INFO_DEF + 10:
        pPlayer->SEND_GOSSIP_MENU(
            TEXT_THIS_PLACE_ID, pCreature->GetObjectGuid());
        break;
    case GOSSIP_ACTION_INFO_DEF + 11:
        pPlayer->SEND_GOSSIP_MENU(
            TEXT_WHERE_MEDIVH_ID, pCreature->GetObjectGuid());
        break;
    case GOSSIP_ACTION_INFO_DEF + 12:
        pPlayer->SEND_GOSSIP_MENU(
            TEXT_NAVIGATE_TOWER_ID, pCreature->GetObjectGuid());
        break;
    case GOSSIP_ACTION_INFO_DEF + 20:
        if (ScriptedInstance* pInstance =
                (ScriptedInstance*)pCreature->GetInstanceData())
            if (pInstance->GetData(TYPE_ARAN) == DONE)
                pPlayer->CastSpell(pPlayer, SPELL_GUARDIAN_LIB_TELEPORT, true);
        pPlayer->CLOSE_GOSSIP_MENU();
        break;
    }

    return true;
}

/*###
# npc_spectral_stable_hand
####*/

#define SPECTRAL_SAY_1 "<sigh> Seems like I've been at this forever..."
#define SPECTRAL_SAY_2 "Another day, another stable to muck out."
#define SPECTRAL_SAY_3 "I grow tired of this routine."

enum
{
    SPELL_WHIP_RAGE = 29340,
    SPELL_KNOCKDOWN = 18812,
    SPELL_PIERCE_ARMOR = 6016,
    SPELL_HEALING_TOUCH = 29339,

    NPC_SPECTRAL_STALLION = 15548,
    NPC_SPECTRAL_CHARGER = 15547,

    ITEM_SHOVEL = 6205,
};

struct MANGOS_DLL_DECL npc_spectral_stable_handAI : public ScriptedAI
{
    npc_spectral_stable_handAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        Reset();

        if (urand(0, 1))
        {
            m_bForkMob = false;
            // Change weapon to shovel:
            SetEquipmentSlots(false, ITEM_SHOVEL);
        }
        else
        {
            m_bForkMob = true;
            // Weapon is fork by default, no need to change
        }
    }

    // Can either be a shovel mob or a fork mob
    bool m_bForkMob;

    uint32 m_rpSayTimer;
    uint32 m_whipTimer;
    // Shovel:
    uint32 m_knockTimer;
    // Fork:
    uint32 m_healTimer;
    uint32 m_pierceTimer;

    void Reset() override
    {
        m_rpSayTimer = urand(10000, 20000);
        m_whipTimer = 15000;
        m_healTimer = 35000;
        m_pierceTimer = urand(20000, 25000);
        m_knockTimer = urand(5000, 10000);
    }

    void GetNearbyHorses(std::vector<Creature*>& out, float range)
    {
        auto horse1 = GetCreatureListWithEntryInGrid(
            m_creature, NPC_SPECTRAL_STALLION, range);
        auto horse2 = GetCreatureListWithEntryInGrid(
            m_creature, NPC_SPECTRAL_CHARGER, range);
        out.reserve(horse1.size() + horse2.size());
        out.insert(out.end(), horse1.begin(), horse1.end());
        out.insert(out.end(), horse2.begin(), horse2.end());
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
        {
            if (m_rpSayTimer <= uiDiff)
            {
                // 20% chance to do a random say
                if (!urand(0, 5))
                {
                    switch (urand(1, 3))
                    {
                    case 1:
                        m_creature->MonsterSay(SPECTRAL_SAY_1, LANG_UNIVERSAL);
                        break;
                    case 2:
                        m_creature->MonsterSay(SPECTRAL_SAY_2, LANG_UNIVERSAL);
                        break;
                    case 3:
                        m_creature->MonsterSay(SPECTRAL_SAY_3, LANG_UNIVERSAL);
                        break;
                    }
                }
                m_rpSayTimer = urand(10000, 20000);
            }
            else
                m_rpSayTimer -= uiDiff;
            return;
        }

        // Both
        if (m_whipTimer <= uiDiff)
        {
            std::vector<Creature*> horses;
            GetNearbyHorses(horses, 30.0f);
            Creature* tar = NULL;
            for (auto& horse : horses)
            {
                if (!(horse)->has_aura(SPELL_WHIP_RAGE) &&
                    (horse)->IsWithinWmoLOSInMap(m_creature) &&
                    (horse)->isAlive())
                {
                    tar = horse;
                    break;
                }
            }
            if (tar)
                if (DoCastSpellIfCan(tar, SPELL_WHIP_RAGE) == CAST_OK)
                    m_whipTimer = 30000;
        }
        else
            m_whipTimer -= uiDiff;

        if (m_bForkMob)
        {
            // Fork Mob
            if (m_healTimer <= uiDiff)
            {
                std::vector<Creature*> horses;
                GetNearbyHorses(horses, 40.0f);
                Creature* tar = NULL;
                float lastHp = 100.0f;
                for (auto& horse : horses)
                {
                    float hp = (horse)->GetHealthPercent();
                    if (hp < lastHp &&
                        (horse)->IsWithinWmoLOSInMap(m_creature) &&
                        (horse)->isAlive())
                    {
                        tar = horse;
                        lastHp = hp;
                    }
                }
                if (tar)
                    if (DoCastSpellIfCan(tar, SPELL_HEALING_TOUCH) == CAST_OK)
                        m_healTimer = 70000;
            }
            else
                m_healTimer -= uiDiff;

            if (m_pierceTimer <= uiDiff)
            {
                if (DoCastSpellIfCan(
                        m_creature->getVictim(), SPELL_PIERCE_ARMOR) == CAST_OK)
                    m_pierceTimer = urand(40000, 50000);
            }
            else
                m_pierceTimer -= uiDiff;
        }
        else
        {
            // Shovel Mob
            if (m_knockTimer <= uiDiff)
            {
                if (DoCastSpellIfCan(
                        m_creature->getVictim(), SPELL_KNOCKDOWN) == CAST_OK)
                    m_knockTimer = urand(10000, 20000);
            }
            else
                m_knockTimer -= uiDiff;
        }

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_npc_spectral_stable_hand(Creature* pCreature)
{
    return new npc_spectral_stable_handAI(pCreature);
}

#define MAX_RINGS 16
const uint32 VioletSignetRings[MAX_RINGS] = {
    // Caster dps
    29284, 29285, 29286, 29287,
    // Tank
    29276, 29277, 29278, 29279,
    // Melee/Ranged dps
    29280, 29281, 29282, 29283,
    // Healer
    29288, 29289, 29291, 29290};
const uint32 VioletSignetQuests[MAX_RINGS] = {
    // Caster dps
    10729, 10733, 10738, 10725,
    // Tank
    10732, 10736, 10741, 10728,
    // Melee/Ranged dps
    10731, 10735, 10740, 10727,
    // Healer
    10730, 10734, 10739, 10726};

bool HasVioletRings(Player* plr)
{
    for (const uint32* i = VioletSignetRings;
         i != VioletSignetRings + MAX_RINGS; ++i)
        if (plr->HasItemCount(*i, 1, true))
            return true;
    return false;
}

bool GossipHello_ArchmageLeryda(Player* p, Creature* c)
{
    if (c->isQuestGiver())
        p->PrepareQuestMenu(c->GetObjectGuid());

    bool hasDoneRingQuest = false;
    for (auto& VioletSignetQuest : VioletSignetQuests)
        if (p->GetQuestRewardStatus(VioletSignetQuest))
        {
            hasDoneRingQuest = true;
            break;
        }
    if (hasDoneRingQuest)
    {
        std::set<uint32> items(
            VioletSignetRings, VioletSignetRings + MAX_RINGS);
        if (!HasVioletRings(p))
            p->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT,
                "I've lost my Violet Signet and seek a replacement.",
                GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 10);
    }

    p->SEND_GOSSIP_MENU(p->GetGossipTextId(c), c->GetObjectGuid());
    return true;
}

bool GossipSelect_ArchmageLeryda(
    Player* p, Creature* /*c*/, uint32 /*sender*/, uint32 a)
{
    if (a == GOSSIP_ACTION_INFO_DEF + 10)
    {
        std::set<uint32> items(
            VioletSignetRings, VioletSignetRings + MAX_RINGS);
        if (!HasVioletRings(p))
        {
            // Go from end to beginning (That way we prioritize higher
            // reputations first)
            for (int i = MAX_RINGS - 1; i >= 0; --i)
                if (p->GetQuestRewardStatus(VioletSignetQuests[i]))
                {
                    p->add_item(VioletSignetRings[i], 1);
                    break;
                }
        }
    }

    p->CLOSE_GOSSIP_MENU();
    return true;
}

bool GossipHello_ArchmageAlturus(Player* p, Creature* c)
{
    if (c->isQuestGiver())
        p->PrepareQuestMenu(c->GetObjectGuid());

    if (p->GetQuestRewardStatus(9837) && !p->HasItemCount(24490, 1, true))
        p->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, "I seem to have misplaced my key.",
            GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 10);

    if (p->GetQuestRewardStatus(9637) && !p->HasItemCount(24140, 1, true))
        p->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, "I have lost my Blackened Urn.",
            GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 20);

    p->SEND_GOSSIP_MENU(p->GetGossipTextId(c), c->GetObjectGuid());
    return true;
}

bool GossipSelect_ArchmageAlturus(
    Player* p, Creature* /*c*/, uint32 /*sender*/, uint32 a)
{
    if (a == GOSSIP_ACTION_INFO_DEF + 10)
    {
        if (p->GetQuestRewardStatus(9837) && !p->HasItemCount(24490, 1, true))
            p->add_item(24490, 1);
    }
    else if (a == GOSSIP_ACTION_INFO_DEF + 20)
    {
        if (p->GetQuestRewardStatus(9637) && !p->HasItemCount(24140, 1, true))
            p->add_item(24140, 1);
    }

    p->CLOSE_GOSSIP_MENU();
    return true;
}

void AddSC_karazhan()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "npc_barnes";
    pNewScript->GetAI = &GetAI_npc_barnesAI;
    pNewScript->pGossipHello = &GossipHello_npc_barnes;
    pNewScript->pGossipSelect = &GossipSelect_npc_barnes;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "npc_berthold";
    pNewScript->pGossipHello = &GossipHello_npc_berthold;
    pNewScript->pGossipSelect = &GossipSelect_npc_berthold;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "npc_spectral_stable_hand";
    pNewScript->GetAI = &GetAI_npc_spectral_stable_hand;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "npc_archmage_leryda";
    pNewScript->pGossipHello = &GossipHello_ArchmageLeryda;
    pNewScript->pGossipSelect = &GossipSelect_ArchmageLeryda;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "npc_archmage_alturus";
    pNewScript->pGossipHello = &GossipHello_ArchmageAlturus;
    pNewScript->pGossipSelect = &GossipSelect_ArchmageAlturus;
    pNewScript->RegisterSelf();
}
