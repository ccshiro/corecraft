/* Copyright (C) 2013 Corecraft */

/* ScriptData
SDName: Instance - Mana Tombs
SD%Complete: 100
SDComment: Instance Data for Mana Tombs instance
SDCategory: Auchindoun, Mana Tombs
EndScriptData */

#include "escort_ai.h"
#include "mana_tombs.h"

instance_mana_tombs::instance_mana_tombs(Map* pMap) : ScriptedInstance(pMap)
{
    Initialize();
}
void instance_mana_tombs::Initialize()
{
    memset(&m_auiEncounter, 0, sizeof(m_auiEncounter));
}

void instance_mana_tombs::OnObjectCreate(GameObject* pGo)
{
    switch (pGo->GetEntry())
    {
    default:
        return;
    }

    m_mGoEntryGuidStore[pGo->GetEntry()] = pGo->GetObjectGuid();
}

void instance_mana_tombs::OnCreatureCreate(Creature* pCreature)
{
    switch (pCreature->GetEntry())
    {
    case NPC_SHAHEEN:
        break;
    default:
        return;
    }

    m_mNpcEntryGuidStore[pCreature->GetEntry()] = pCreature->GetObjectGuid();
}

void instance_mana_tombs::SetData(uint32 uiType, uint32 uiData)
{
    if (uiType >= MAX_ENCOUNTER)
        return;
    m_auiEncounter[uiType] = uiData;

    if (uiData == DONE)
    {
        OUT_SAVE_INST_DATA;

        std::ostringstream saveStream;
        saveStream << m_auiEncounter[0] << " " << m_auiEncounter[1] << " "
                   << m_auiEncounter[2] << " " << m_auiEncounter[3];

        m_strInstData = saveStream.str();

        SaveToDB();
        OUT_SAVE_INST_DATA_COMPLETE;
    }
}

uint32 instance_mana_tombs::GetData(uint32 uiType)
{
    if (uiType < MAX_ENCOUNTER)
        return m_auiEncounter[uiType];

    return 0;
}

void instance_mana_tombs::Load(const char* chrIn)
{
    if (!chrIn)
    {
        OUT_LOAD_INST_DATA_FAIL;
        return;
    }

    OUT_LOAD_INST_DATA(chrIn);

    std::istringstream loadStream(chrIn);
    loadStream >> m_auiEncounter[0] >> m_auiEncounter[1] >> m_auiEncounter[2] >>
        m_auiEncounter[3];

    for (auto& elem : m_auiEncounter)
    {
        if (elem == IN_PROGRESS)
            elem = NOT_STARTED;
    }

    OUT_LOAD_INST_DATA_COMPLETE;
}

InstanceData* GetInstanceData_instance_mana_tombs(Map* pMap)
{
    return new instance_mana_tombs(pMap);
}

/* Escort Event Data */
enum
{
    EMOTE_SHA_ARRIVE = -1570001,
    SAY_SHA_MADE_IT = -1570002,
    EMOTE_SHA_CONTROLLER = -1570003,

    SAY_SHA_GATHER = -1570004,
    SAY_SHA_FIRST_NETHER = -1570005,

    SAY_SHA_SECOND_NETHER = -1570006,
    SAY_SHA_SHOULD_DO_IT = -1570007,

    SAY_SHA_HRM_WHERE = -1570008,
    SAY_SHA_AH_THERE = -1570009,
    SAY_SHA_THIRD_NETHER = -1570010,

    SAY_SHA_NEXUS_TERROR = -1570011,

    SAY_SHA_REST = -1570012,
    SAY_SHA_READY = -1570013,
    SAY_SHA_LETS_GO = -1570014,

    YELL_XAR_SUMMON = -1570015,
    SAY_XAR_CONVO_P1 = -1570016,
    SAY_SHA_CONVO_P2 = -1570017,
    SAY_XAR_CONVO_P3 = -1570018,

    SAY_SHA_POST_BATTLE = -1570019,
    SAY_SHA_THANKS = -1570020,

    NPC_CONSORTIUM_LABORERS = 19672,
    NPC_ETHEREAL_THEURGIST = 18315,
    NPC_ETHEREAL_SPELLBINDER = 18312,
    NPC_NEXUS_TERROR = 19307,
    NPC_XIRAXIS = 19666,
};

#define LABORER_SIZE 7
float laborerPositions[LABORER_SIZE][4] = {
    {-387.2f, -57.9f, -1.0f, 2.8f}, {-386.9f, -76.3f, -1.0f, 4.3f},
    {-389.5f, -68.0f, -1.0f, 3.2f}, {-354.7f, -76.0f, -1.0f, 5.3f},
    {-357.1f, -64.2f, -1.0f, 1.1f}, {-373.0f, -71.1f, 0.7f, 4.9f},
    {-393.7f, -62.9f, -1.0f, 3.5f},
};

/*
 * Consortium Laborer
 */
struct npc_consortium_laborerAI : ScriptedAI
{
    npc_consortium_laborerAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        Reset();
    }
    void Reset() override { m_uiNewPointTimer = 8000; }

    uint32 m_uiNewPointTimer;
    float m_endOrientation;

    void MovementInform(movement::gen gen_type, uint32 /*uiData*/) override
    {
        if (gen_type == movement::gen::point)
        {
            m_uiNewPointTimer = urand(4000, 8000);
            m_creature->HandleEmoteState(EMOTE_STATE_WORK_MINING);
        }
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        if (m_uiNewPointTimer)
        {
            if (m_uiNewPointTimer <= uiDiff)
            {
                uint32 pos = urand(0, LABORER_SIZE - 1);
                m_creature->movement_gens.push(
                    new movement::PointMovementGenerator(0,
                        laborerPositions[pos][0], laborerPositions[pos][1],
                        laborerPositions[pos][2], true, true),
                    movement::EVENT_ENTER_COMBAT);
                m_endOrientation = laborerPositions[pos][3];
                m_uiNewPointTimer = 0;
            }
            else
                m_uiNewPointTimer -= uiDiff;
        }
    }
};

CreatureAI* GetAI_npc_consortium_laborer(Creature* pCreature)
{
    return new npc_consortium_laborerAI(pCreature);
}

/*
 * Xiraxis
 */
struct npc_xiraxisAI : npc_escortAI
{
    npc_xiraxisAI(Creature* pCreature) : npc_escortAI(pCreature)
    {
        m_pInstance = (instance_mana_tombs*)pCreature->GetInstanceData();
        Reset();
    }

    instance_mana_tombs* m_pInstance;

    void Reset() override {}

    void WaypointReached(uint32 uiPointId) override
    {
        switch (uiPointId)
        {
        case 1:
            DoScriptText(YELL_XAR_SUMMON, m_creature);
            break;
        case 6:
            DoScriptText(SAY_XAR_CONVO_P1, m_creature);
            break;
        case 7:
            if (m_pInstance)
                if (Creature* sha = m_pInstance->GetShaheen())
                    DoScriptText(SAY_SHA_CONVO_P2, sha);
            break;
        case 8:
            DoScriptText(SAY_XAR_CONVO_P3, m_creature);
            break;
        case 9:
            m_creature->RemoveFlag(
                UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_PASSIVE);
            if (m_pInstance)
                if (Creature* sha = m_pInstance->GetShaheen())
                    AttackStart(sha);
            break;
        }
    }

    void UpdateEscortAI(const uint32) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_npc_xiraxis(Creature* pCreature)
{
    return new npc_xiraxisAI(pCreature);
}

enum
{
    SPELL_VIRAANI_AURA = 33839,
    SPELL_ARCANE_BOLT = 31445,
    SPELL_ARCANE_BOOM = 33860,
};

/*
 * SHAHEEN
 */
struct npc_shaheenAI : npc_escortAI
{
    npc_shaheenAI(Creature* pCreature) : npc_escortAI(pCreature)
    {
        m_pInstance = (instance_mana_tombs*)pCreature->GetInstanceData();
        Reset();
    }

    instance_mana_tombs* m_pInstance;
    ObjectGuid m_escortingPlayer;
    ObjectGuid m_xiraxis;
    uint32 m_uiBoltTimer;
    uint32 m_uiBoomTimer;

    void Reset() override
    {
        m_uiBoltTimer = urand(2000, 4000);
        m_uiBoomTimer = 20000;
    }

    void OnTransporterUse(Player* pPlayer)
    {
        Start(false, pPlayer, NULL);
        m_escortingPlayer = pPlayer->GetObjectGuid();
        if (m_pInstance)
            m_pInstance->SetData(TYPE_SHAHEEN, IN_PROGRESS);
    }

    void JustDied(Unit* /*pKiller*/) override
    {
        if (m_pInstance)
            m_pInstance->SetData(TYPE_SHAHEEN, FAIL);
    }

    void JustSummoned(Creature* pCreature) override
    {
        switch (pCreature->GetEntry())
        {
        case NPC_ETHEREAL_THEURGIST:
        case NPC_ETHEREAL_SPELLBINDER:
            if (pCreature->AI())
                pCreature->AI()->AttackStart(m_creature);
            break;
        default:
            break;
        }
    }

    void WaypointReached(uint32 uiPointId) override
    {
        switch (uiPointId)
        {
        case 1:
            DoScriptText(EMOTE_SHA_ARRIVE, m_creature);
            break;
        case 2:
            if (Player* plr =
                    m_creature->GetMap()->GetPlayer(m_escortingPlayer))
                DoScriptText(SAY_SHA_MADE_IT, m_creature, plr);
            break;
        case 5:
            DoScriptText(EMOTE_SHA_CONTROLLER, m_creature);
            break;
        case 6:
            m_creature->SummonCreature(NPC_CONSORTIUM_LABORERS, -353.8f, -67.0f,
                -1.0f, 3.3f, TEMPSUMMON_TIMED_DESPAWN, 30 * 60 * 1000);
            m_creature->SummonCreature(NPC_CONSORTIUM_LABORERS, -353.4f, -68.3f,
                -1.0f, 3.3f, TEMPSUMMON_TIMED_DESPAWN, 30 * 60 * 1000);
            m_creature->SummonCreature(NPC_CONSORTIUM_LABORERS, -352.9f, -71.1f,
                -1.0f, 3.3f, TEMPSUMMON_TIMED_DESPAWN, 30 * 60 * 1000);
            m_creature->SummonCreature(NPC_CONSORTIUM_LABORERS, -352.6f, -72.8f,
                -1.0f, 3.3f, TEMPSUMMON_TIMED_DESPAWN, 30 * 60 * 1000);
            break;
        case 7:
            DoScriptText(SAY_SHA_GATHER, m_creature);
            // Pause escort until started again
            m_creature->SetFlag(UNIT_NPC_FLAGS,
                UNIT_NPC_FLAG_QUESTGIVER | UNIT_NPC_FLAG_GOSSIP);
            SetEscortPaused(true);
            m_creature->SetFacingTo(3.7f);
            break;
        case 11:
            DoScriptText(SAY_SHA_FIRST_NETHER, m_creature);
            break;
        case 22:
            DoScriptText(SAY_SHA_SECOND_NETHER, m_creature);
            m_creature->SummonCreature(NPC_ETHEREAL_SPELLBINDER, -376.0f,
                -132.2f, -1.0f, 4.7f, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT,
                120 * 1000);
            m_creature->SummonCreature(NPC_ETHEREAL_SPELLBINDER, -369.6f,
                -132.1f, -1.0f, 4.7f, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT,
                120 * 1000);
            m_creature->SummonCreature(NPC_ETHEREAL_THEURGIST, -369.7f, -197.1f,
                -1.0f, 1.6f, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT,
                120 * 1000);
            m_creature->SummonCreature(NPC_ETHEREAL_THEURGIST, -376.3f, -196.8f,
                -1.0f, 1.6f, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT,
                120 * 1000);
            break;
        case 23:
            DoScriptText(SAY_SHA_SHOULD_DO_IT, m_creature);
            break;
        case 31:
            DoScriptText(SAY_SHA_HRM_WHERE, m_creature);
            break;
        case 32:
            DoScriptText(SAY_SHA_AH_THERE, m_creature);
            break;
        case 38:
            DoScriptText(SAY_SHA_THIRD_NETHER, m_creature);
            m_creature->SummonCreature(NPC_ETHEREAL_SPELLBINDER, -283.6f,
                -169.0f, -2.4f, 0.6f, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT,
                120 * 1000);
            m_creature->SummonCreature(NPC_ETHEREAL_SPELLBINDER, -281.1f,
                -173.0f, -1.9f, 0.6f, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT,
                120 * 1000);
            m_creature->SummonCreature(NPC_ETHEREAL_THEURGIST, -238.4f, -169.8f,
                -1.0f, 2.6f, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT,
                120 * 1000);
            m_creature->SummonCreature(NPC_ETHEREAL_THEURGIST, -234.8f, -165.2f,
                -1.0f, 1.6f, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT,
                120 * 1000);
            break;
        case 42:
        {
            int32 grpId =
                m_creature->GetMap()->GetCreatureGroupMgr().CreateNewGroup(
                    "Nexus Terror", true);
            if (CreatureGroup* pGrp =
                    m_creature->GetMap()->GetCreatureGroupMgr().GetGroup(grpId))
            {
                if (Creature* sum = m_creature->SummonCreature(NPC_NEXUS_TERROR,
                        -34.0f, -223.8f, -0.2f, 3.0f,
                        TEMPSUMMON_CORPSE_TIMED_DESPAWN, 120 * 1000))
                    pGrp->AddMember(sum, false);
                if (Creature* sum = m_creature->SummonCreature(NPC_NEXUS_TERROR,
                        12.6f, -215.8f, -0.7f, 3.3f,
                        TEMPSUMMON_CORPSE_TIMED_DESPAWN, 120 * 1000))
                    pGrp->AddMember(sum, false);
            }
        }
        break;
        case 49:
            DoScriptText(SAY_SHA_NEXUS_TERROR, m_creature);
            break;
        case 52:
            m_creature->HandleEmoteState(EMOTE_STATE_WORK_MINING);
            break;
        case 53:
            m_creature->HandleEmoteState(EMOTE_STATE_NONE);
            m_creature->SetFacingTo(4.5f);
            DoScriptText(SAY_SHA_REST, m_creature);
            break;
        case 54:
            DoScriptText(SAY_SHA_READY, m_creature);
            break;
        case 55:
            DoScriptText(SAY_SHA_LETS_GO, m_creature);
            break;
        case 67:
            if (Creature* xeraxis = m_creature->SummonCreature(NPC_XIRAXIS,
                    -46.0f, -4.6f, -1.0f, 3.7f, TEMPSUMMON_CORPSE_TIMED_DESPAWN,
                    120 * 1000))
            {
                xeraxis->SetFlag(UNIT_FIELD_FLAGS,
                    UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_PASSIVE);
                if (xeraxis->AI())
                    ((npc_xiraxisAI*)xeraxis->AI())->Start(true);
                m_xiraxis = xeraxis->GetObjectGuid();
            }
            SetEscortPaused(true);
            break;
        case 68:
            DoScriptText(SAY_SHA_POST_BATTLE, m_creature);
            break;
        case 69:
            SetRun(true);
            DoScriptText(SAY_SHA_THANKS, m_creature);
            // Complete quest for everyone
            if (m_pInstance)
            {
                for (const auto& elem : m_creature->GetMap()->GetPlayers())
                {
                    Player* plr = elem.getSource();
                    if (plr->GetQuestStatus(QUEST_SOMEONE_ELSES_WORK) ==
                        QUEST_STATUS_INCOMPLETE)
                        plr->AreaExploredOrEventHappens(
                            QUEST_SOMEONE_ELSES_WORK);
                }

                m_pInstance->SetData(TYPE_SHAHEEN, DONE);
            }
            break;
        }
    }

    void UpdateEscortAI(const uint32 uiDiff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
        {
            if (m_xiraxis)
            {
                if (Creature* xiraxis =
                        m_creature->GetMap()->GetCreature(m_xiraxis))
                    if (!xiraxis->isAlive())
                    {
                        SetEscortPaused(false);
                        m_xiraxis = ObjectGuid();
                    }
            }
            return;
        }

        if (!m_creature->has_aura(SPELL_VIRAANI_AURA))
            m_creature->CastSpell(m_creature, SPELL_VIRAANI_AURA, false);

        if (m_uiBoltTimer <= uiDiff)
        {
            if (DoCastSpellIfCan(m_creature->getVictim(), SPELL_ARCANE_BOLT) ==
                CAST_OK)
                m_uiBoltTimer = urand(2000, 4000);
        }
        else
            m_uiBoltTimer -= uiDiff;

        if (m_uiBoomTimer <= uiDiff)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_ARCANE_BOOM) == CAST_OK)
                m_uiBoomTimer = 20000;
        }
        else
            m_uiBoomTimer -= uiDiff;

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_npc_shaheen(Creature* pCreature)
{
    return new npc_shaheenAI(pCreature);
}

bool GossipHello_shaheen(Player* pPlayer, Creature* pCreature)
{
    pPlayer->PrepareQuestMenu(pCreature->GetObjectGuid());

    pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, "We are ready. Let's go.",
        GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 10);
    pPlayer->SEND_GOSSIP_MENU(10015, pCreature->GetObjectGuid());

    return true;
}

bool GossipSelect_shaheen(Player* /*pPlayer*/, Creature* pCreature,
    uint32 /*uiSender*/, uint32 uiAction)
{
    if (uiAction == GOSSIP_ACTION_INFO_DEF + 10)
    {
        pCreature->RemoveFlag(
            UNIT_NPC_FLAGS, UNIT_NPC_FLAG_QUESTGIVER | UNIT_NPC_FLAG_GOSSIP);
        if (npc_shaheenAI* ai = dynamic_cast<npc_shaheenAI*>(pCreature->AI()))
            ai->SetEscortPaused(false);
    }

    return true;
}

// Ethereal Transporter
bool GOGossipHello_EtherealTransporer(Player* pPlayer, GameObject* pGo)
{
    pPlayer->PrepareQuestMenu(pGo->GetObjectGuid());

    // If event is not started, and we're dealing with a player who has
    // completed Safety but not the escort, we provide him with a gossip option
    if (InstanceData* id = pPlayer->GetInstanceData())
    {
        if (id->GetData(TYPE_SHAHEEN) == NOT_STARTED)
        {
            if (pPlayer->GetQuestRewardStatus(QUEST_SAFTEY_IS_JOB_ONE) &&
                !pPlayer->GetQuestRewardStatus(QUEST_SOMEONE_ELSES_WORK))
                pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT,
                    "<You close your eyes and press a button... again.>",
                    GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 10);
        }
    }

    pPlayer->SEND_GOSSIP_MENU(9899, pGo->GetObjectGuid());

    return true;
}

bool GOQuestRewarded_EtherealTransporer(
    Player* pPlayer, GameObject* /*pGo*/, const Quest* /*quest*/)
{
    if (InstanceData* id = pPlayer->GetInstanceData())
    {
        if (id->GetData(TYPE_SHAHEEN) == NOT_STARTED)
        {
            if (Creature* shaheen = pPlayer->SummonCreature(NPC_SHAHEEN,
                    -352.2f, -69.6f, -1.0f, 3.3f, TEMPSUMMON_MANUAL_DESPAWN, 0))
                if (shaheen->AI())
                {
                    if (npc_shaheenAI* AI =
                            dynamic_cast<npc_shaheenAI*>(shaheen->AI()))
                        AI->OnTransporterUse(pPlayer);
                }
        }
    }

    return true;
}

bool GOGossipSelect_EtherealTransporer(
    Player* pPlayer, GameObject* /*pGo*/, uint32 /*uiSender*/, uint32 uiAction)
{
    if (uiAction == GOSSIP_ACTION_INFO_DEF + 10)
    {
        if (InstanceData* id = pPlayer->GetInstanceData())
        {
            if (id->GetData(TYPE_SHAHEEN) == NOT_STARTED)
            {
                if (Creature* shaheen =
                        pPlayer->SummonCreature(NPC_SHAHEEN, -352.2f, -69.6f,
                            -1.0f, 3.3f, TEMPSUMMON_MANUAL_DESPAWN, 0))
                    if (shaheen->AI())
                    {
                        if (npc_shaheenAI* AI =
                                dynamic_cast<npc_shaheenAI*>(shaheen->AI()))
                            AI->OnTransporterUse(pPlayer);
                    }
            }
        }
    }

    pPlayer->CLOSE_GOSSIP_MENU();

    return true;
}

void AddSC_instance_mana_tombs()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "instance_mana_tombs";
    pNewScript->GetInstanceData = &GetInstanceData_instance_mana_tombs;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "go_ethereal_transporter";
    pNewScript->pGossipHelloGO = &GOGossipHello_EtherealTransporer;
    pNewScript->pGossipSelectGO = &GOGossipSelect_EtherealTransporer;
    pNewScript->pQuestRewardedGO = &GOQuestRewarded_EtherealTransporer;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "npc_shaheen";
    pNewScript->GetAI = &GetAI_npc_shaheen;
    pNewScript->pGossipHello = &GossipHello_shaheen;
    pNewScript->pGossipSelect = &GossipSelect_shaheen;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "npc_xiraxis";
    pNewScript->GetAI = &GetAI_npc_xiraxis;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "npc_consortium_laborer";
    pNewScript->GetAI = &GetAI_npc_consortium_laborer;
    pNewScript->RegisterSelf();
}
