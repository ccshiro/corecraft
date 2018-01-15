/* Copyright (C) 2006 - 2012 ScriptDev2 <http://www.scriptdev2.com/>
 * This program is free software licensed under GPL version 2
 * Please see the included DOCS/LICENSE.TXT for more information */

#ifndef DEF_KARAZHAN_H
#define DEF_KARAZHAN_H

#include "precompiled.h"

enum
{
    MAX_ENCOUNTER = 12,

    TYPE_SERVANTS = 0, // Not an actual encounter in retail -- but we certainly
                       // don't want it to be farmable
    TYPE_ATTUMEN = 1,
    TYPE_MOROES = 2,
    TYPE_MAIDEN = 3,
    TYPE_OPERA = 4,
    TYPE_CURATOR = 5,
    TYPE_TERESTIAN = 6,
    TYPE_ARAN = 7,
    TYPE_NETHERSPITE = 8,
    TYPE_CHESS = 9,
    TYPE_MALCHEZAAR = 10,
    TYPE_NIGHTBANE = 11,

    // Encounter mobs (These are slain to mark the end of an encounter; which
    // stops trash spawning, etc)
    // For any boss that isn't an event the boss itself is the encounter mob and
    // no extra handling is needed
    NPC_ENCOUNTER_SERVANTS = 100030,
    NPC_ENCOUNTER_OPERA = 100031,
    NPC_ENCOUNTER_CHESS = 100032,

    MAX_DATA_ENTRIES = 7,
    DATA_OPERA_EVENT = 12, // This entry is saved to the db
    DATA_OPERA_OZ_COUNT = 13,
    DATA_CHESS_PLAYING_TEAM = 14,
    DATA_CHESS_DEAD_ALLIANCE_PIECES = 15,
    DATA_CHESS_DEAD_HORDE_PIECES = 16,
    DATA_UNUSED = 17,         // NOTE: Use this if more data entries needed
    DATA_SERVANTS_COUNT = 18, // When 0 no servant quarter mobs remain (WARNING:
                              // Do NOT change index as it's used by SmartAI)

    DEAD_PIECE_INCREASE = 10001,

    // Normal Mobs:
    NPC_COLDMIST_STALKER = 16170,
    NPC_COLDMIST_WIDOW = 16171,
    NPC_SHADOWBAT = 16173,
    NPC_GREATER_SHADOWBAT = 16174,
    NPC_VAMPIRIC_SHADOWBAT = 16175,
    NPC_SHADOWBEAST = 16176,
    NPC_DREADBEAST = 16177,
    NPC_PHASE_HOUND = 16178,

    // Encounter mobs
    NPC_HYAKISS = 16179,   // Scripted in SmartAI
    NPC_SHADIKITH = 16180, // Scripted in SmartAI
    NPC_ROKAD = 16181,     // Scripted in SmartAI
    NPC_MIDNIGHT = 16151,
    NPC_ATTUMEN_THE_HUNTSMAN_FAKE = 15550,
    NPC_ATTUMEN_THE_HUNTSMAN = 16152,
    NPC_MOROES = 15687,
    NPC_BARNES = 16812,
    NPC_JULIANNE = 17534,
    NPC_ROMULO = 17533,
    NPC_DOROTHEE = 17535,
    NPC_TITO = 17548,
    NPC_ROAR = 17546,
    NPC_STRAWMAN = 17543,
    NPC_TINHEAD = 17547,
    NPC_CRONE = 18168,
    NPC_MEDIVH = 16816,
    NPC_NIGHTBANE = 17225,
    NPC_NIGHTBANE_HELPER_TARGET = 17260,
    NPC_INFERNAL_RELAY = 17645,

    SAY_DOROTHEE_TITO_DEATH = -1532027,

    GO_STAGE_CURTAIN = 183932,
    GO_STAGE_DOOR_LEFT = 184278,
    GO_STAGE_DOOR_RIGHT = 184279,
    GO_PRIVATE_LIBRARY_DOOR = 184517,
    GO_SIDE_ENTRANCE_DOOR = 184275,
    GO_DUST_COVERED_CHEST = 185119,

    GO_SERVANTS_ACCESS_DOOR = 184281,

    GO_MASTERS_TERRACE_DOOR_ONE = 184274,
    GO_MASTERS_TERRACE_DOOR_TWO = 184280,

    // Uninteractable doors that open/close depending on event:
    GO_MASSIVE_DOOR = 185521,             // Netherspite's door
    GO_NETHERSPACE_DOOR = 185134,         // Malchezaar's door
    GO_GAMESMANS_HALL_EXIT_DOOR = 184277, // Opens when chess is completed

    // Chess door:
    GO_GAMESMANS_HALL_DOOR = 184276, // Always unlocked except for during event

    // Opera event stage decoration
    GO_OZ_BACKDROP = 183442,
    GO_OZ_HAY = 183496,
    GO_HOOD_BACKDROP = 183491,
    GO_HOOD_TREE = 183492,
    GO_HOOD_HOUSE = 183493,
    GO_RAJ_BACKDROP = 183443,
    GO_RAJ_MOON = 183494,
    GO_RAJ_BALCONY = 183495,

    SPELL_GAME_IN_SESSION = 39331,

    KARAZHAN_MAP_ID = 532,
};

enum OperaEvents
{
    // Do NOT alter this order
    EVENT_OZ = 1,
    EVENT_HOOD = 2,
    EVENT_RAJ = 3
};

class MANGOS_DLL_DECL instance_karazhan : public ScriptedInstance
{
public:
    instance_karazhan(Map* pMap);
    ~instance_karazhan() {}

    void Initialize() override;
    bool IsEncounterInProgress() const override;

    void Update(uint32 diff) override;

    void OnPlayerEnter(Player* player) override;
    void OnCreatureCreate(Creature* pCreature) override;
    void OnObjectCreate(GameObject* pGo) override;
    void OnCreatureEvade(Creature* pCreature) override;
    void OnCreatureDeath(Creature* pCreature) override;

    void SetData(uint32 uiType, uint32 uiData) override;
    uint32 GetData(uint32 uiType) override;

    void SetData64(uint32 uiType, uint64 uiData) override;
    uint64 GetData64(uint32 uiType) override;

    void Load(const char* chrIn) override;
    const char* Save() override { return m_strInstData.c_str(); }

    std::vector<ObjectGuid> m_OperaTrees;
    std::vector<ObjectGuid> m_OperaHouses;
    std::vector<ObjectGuid> m_OperaMoon;
    std::vector<ObjectGuid> m_OperaBalcony;
    std::vector<ObjectGuid> m_OperaHay;
    ObjectGuid m_RAJBackdrop;
    ObjectGuid m_OZBackdrop;
    ObjectGuid m_HOODBackdrop;

    void ResetBoard();

    bool m_bHasWipedOnOpera;

    std::vector<ObjectGuid> m_EmptySpots;
    std::vector<ObjectGuid> m_cleanupMiniatures;

    int32 m_uiOzGrpId;

private:
    uint32 m_auiEncounter[MAX_ENCOUNTER];
    std::string m_strInstData;
    uint32 m_auiData[MAX_DATA_ENTRIES];

    std::map<uint32 /* = GUIDLow */, uint64 /* = GUID */> m_chessControllers;
    Team m_playingTeam;

    uint32 m_uiDeadAlliancePieces;
    uint32 m_uiDeadHordePieces;
    bool m_forceNewGrp;
};

#endif
