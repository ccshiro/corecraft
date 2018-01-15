/* Copyright (C) 2006 - 2012 ScriptDev2 <http://www.scriptdev2.com/>
 * This program is free software licensed under GPL version 2
 * Please see the included DOCS/LICENSE.TXT for more information */

#ifndef DEF_SHATTERED_H
#define DEF_SHATTERED_H

enum
{
    MAX_ENCOUNTER = 5,

    TYPE_NETHEKURSE = 0,
    TYPE_OMROGG = 1,
    TYPE_GAUNTLET_OF_FLAME = 2,
    TYPE_BLADEFIST = 3, // Note: if players skip Omrogg and go straight to
                        // Karagth then Omrogg comes to aid Karagth
    TYPE_EXECUTION = 4,

    NPC_NETHEKURSE = 16807,
    NPC_NETHEKURSE_GUID = 57853,
    NPC_KARGATH_BLADEFIST = 16808,
    NPC_OMROGG = 16809,
    NPC_OMROGG_GUID = 57855,
    NPC_EXECUTIONER = 17301, // must be killed for the executioner event

    NPC_SOLDIER_ALLIANCE_1 = 17288, // quest giver for 9524
    NPC_SOLDIER_ALLIANCE_2 = 17289,
    NPC_SOLDIER_ALLIANCE_3 = 17292,
    NPC_OFFICER_ALLIANCE = 17290, // quest objective

    NPC_SOLDIER_HORDE_1 = 17294, // quest giver for 9525
    NPC_SOLDIER_HORDE_2 = 17295,
    NPC_SOLDIER_HORDE_3 = 17297,
    NPC_CAPTAIN_HORDE = 17296, // quest objective

    GO_NETHEKURSE_DOOR = 182540,
    GO_NETHEKURSE_ENTER_DOOR = 182539,

    SPELL_KARGATH_EXECUTIONER_1 = 39288, // 55 min - first prisoner - officer
    SPELL_KARGATH_EXECUTIONER_2 = 39289, // 10 min - second prisoner
    SPELL_KARGATH_EXECUTIONER_3 = 39290, // 15 min - last prisoner

    SAY_KARGATH_EXECUTE_ALLY = -1540049,
    SAY_KARGATH_EXECUTE_HORDE = -1540050,

    SPELL_EXECUTE_PRISONER = 30273,

    SAY_KARGATH_DWARF = -1540051,
    SAY_KARGATH_OFFICER = -1540052,
    SAY_KARGATH_PRIVATE = -1540053,
    SAY_KARGATH_TAUREN = -1540054,
    SAY_KARGATH_CAPTAIN = -1540055,
    SAY_KARGATH_SCOUT = -1540056,

    SAY_DWARF_REPLY = -1540057,
    SAY_OFFICER_REPLY = -1540058,
    SAY_PRIVATE_REPLY = -1540059,
    SAY_TAUREN_REPLY = -1540060,
    SAY_CAPTAIN_REPLY = -1540061,
    SAY_SCOUT_REPLY = -1540062,

    // AT_NETHEKURSE               = 4524,                     // Area trigger
    // used for the execution event
};

struct SpawnLocation
{
    uint32 m_uiAllianceEntry, m_uiHordeEntry;
    float m_fX, m_fY, m_fZ, m_fO;
};

const float afExecutionerLoc[4] = {151.443f, -84.439f, 1.938f, 6.283f};

const SpawnLocation aSoldiersLocs[] = {
    {0, NPC_SOLDIER_HORDE_1, 119.609f, 256.127f, -45.254f, 5.133f},
    {NPC_SOLDIER_ALLIANCE_1, 0, 131.106f, 254.520f, -45.236f, 3.951f},
    {NPC_SOLDIER_ALLIANCE_3, NPC_SOLDIER_HORDE_3, 151.040f, -91.558f, 1.936f,
     1.559f},
    {NPC_SOLDIER_ALLIANCE_2, NPC_SOLDIER_HORDE_2, 150.669f, -77.015f, 1.933f,
     4.705f},
    {NPC_OFFICER_ALLIANCE, NPC_CAPTAIN_HORDE, 138.241f, -84.198f, 1.907f,
     0.055f}};

class MANGOS_DLL_DECL instance_shattered_halls : public ScriptedInstance
{
public:
    instance_shattered_halls(Map* pMap);

    void Initialize() override;

    void OnPlayerEnter(Player* pPlayer) override;

    void OnObjectCreate(GameObject* pGo) override;
    void OnCreatureCreate(Creature* pCreature) override;

    void OnCreatureDeath(Creature* pCreature) override;
    void OnCreatureEvade(Creature* pCreature) override;
    void OnCreatureEnterCombat(Creature* pCreature) override;

    void SetData(uint32 uiType, uint32 uiData) override;
    uint32 GetData(uint32 uiType) override;

    const char* Save() override { return m_strInstData.c_str(); }
    void Load(const char* chrIn) override;

    void Update(uint32 uiDiff) override;

    void DoCastGroupDebuff(uint32 uiSpellId);

private:
    uint32 m_auiEncounter[MAX_ENCOUNTER];
    std::string m_strInstData;

    uint32 m_uiExecutionTimer;
    uint32 m_uiPhaseTimer;
    uint32 m_uiPhase;
    ObjectGuid m_killTarget;
    uint32 m_uiTeam;
    uint8 m_uiExecutionStage;
};

#endif
