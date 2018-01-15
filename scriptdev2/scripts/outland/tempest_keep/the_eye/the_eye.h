/* Copyright (C) 2013 Corecraft <https://www.worldofcorecraft.com/>
 * This program is free software licensed under GPL version 2
 * Please see the included DOCS/LICENSE.TXT for more information */

#ifndef DEF_TEMPEST_KEEP_H
#define DEF_TEMPEST_KEEP_H

enum
{
    MAX_ENCOUNTER = 4,

    TYPE_VOID_REAVER = 0,
    TYPE_SOLARIAN = 1,
    TYPE_ALAR = 2,
    TYPE_KAELTHAS = 3,
    // Not saved in DB:
    DATA_KAELTHAS_PHASE = 4,

    NPC_SOLARIAN = 18805,

    GO_STATUE_LEFT = 184597,
    GO_STATUE_RIGHT = 184596,
    GO_TEMPEST_BRIDGE_WINDOW = 184069,

    GO_ALAR_DOOR_1 = 184327,
    GO_ALAR_DOOR_2 = 184329,
    GO_VOID_DOOR_1 = 184326,
    GO_VOID_DOOR_2 = 184328,
    GO_SOLARIAN_DOOR_1 = 184324,
    GO_SOLARIAN_DOOR_2 = 184325,

    NPC_PHOENIX = 21362,
    NPC_PHOENIX_EGG = 21364,

    // Kael'thas event stuff
    KAEL_PHASE_INTRO_RP = 1,
    KAEL_PHASE_ADVISOR,      // Phase one
    KAEL_PHASE_WEAPONS,      // Phase two
    KAEL_PHASE_ALL_ADVISORS, // Phase three
    KAEL_PHASE_PRE_RELEASE,
    KAEL_PHASE_KAELTHAS_P1, // Phase four
    KAEL_PHASE_KAELTHAS_P1_TO_P2,
    KAEL_PHASE_KAELTHAS_P2, // Phase five

    NPC_KAELTHAS = 19622,
    NPC_THALADRED = 20064,
    NPC_SANGUINAR = 20060,
    NPC_CAPERNIAN = 20062,
    NPC_TELONICUS = 20063,

    SAY_KAEL_THALADRED_RELEASE = -1550019,
    SAY_KEAL_SANGUINAR_RELEASE = -1550020,
    SAY_KAEL_CAPERNIAN_RELEASE = -1550017,
    SAY_KAEL_TELONICUS_RELEASE = -1550018,

    SAY_KAEL_SUMMON_WEAPONS = -1550021,
    SPELL_SUMMON_WEAPONS = 36976,

    NPC_STAFF_OF_DISINTEGRATION = 21274,
    NPC_COSMIC_INFUSER = 21270,
    NPC_WARP_SPLICER = 21272,
    NPC_DEVASTATION = 21269,
    NPC_PHASESHIFT_BULWARK = 21273,
    NPC_INFINITY_BLADES = 21271,
    NPC_NETHERSTRAND_LONGBOW = 21268,

    SAY_KAEL_RES_ADVISORS = -1550022,
    SPELL_RES_ADVISORS = 36450,

    SAY_KAEL_RELEASE = -1550023,
};

class MANGOS_DLL_DECL instance_the_eye : public ScriptedInstance
{
public:
    instance_the_eye(Map* pMap);

    void Initialize() override;
    bool IsEncounterInProgress() const override;

    void OnCreatureCreate(Creature* pCreature) override;
    void OnObjectCreate(GameObject* pGo) override;
    void OnCreatureEvade(Creature* pCreature) override;
    void OnCreatureDeath(Creature* pCreature) override;

    void OnPlayerLeave(Player* player) override;

    void SetData(uint32 uiType, uint32 uiData) override;
    uint32 GetData(uint32 uiType) override;

    const char* Save() override { return m_strInstData.c_str(); }
    void Load(const char* chrIn) override;

    void Update(uint32 diff) override;

    void OnAdvisorDeath(Creature* pCreature);

private:
    uint32 m_auiEncounter[MAX_ENCOUNTER];
    std::string m_strInstData;
    uint32 m_kaelPhase;
    uint32 m_releaseEntry;
    uint32 m_releaseTimer;
    uint32 m_beginPhase;
    uint32 m_timeout;
    uint32 m_killedAdvisorsCount;
    std::vector<ObjectGuid> m_cleanups;
};

#endif
