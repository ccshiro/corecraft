/* Copyright (C) 2006 - 2012 ScriptDev2 <http://www.scriptdev2.com/>
 * This program is free software licensed under GPL version 2
 * Please see the included DOCS/LICENSE.TXT for more information */

#ifndef DEF_SHADOW_LABYRINTH_H
#define DEF_SHADOW_LABYRINTH_H

enum
{
    MAX_ENCOUNTER = 5,

    TYPE_RITUALISTS = 0,
    TYPE_HELLMAW = 1,
    TYPE_INCITER = 2,
    TYPE_VORPIL = 3,
    TYPE_MURMUR = 4,

    NPC_MURMUR = 18708,
    NPC_HELLMAW = 18731,
    NPC_VORPIL = 18732,
    NPC_FEL_OVERSEER = 18796,
    NPC_CABAL_RITUALIST_1 = 18794,
    NPC_CABAL_RITUALIST_2 = 100016,
    NPC_CABAL_RITUALIST_3 = 100018,
    NPC_CABAL_RITUALIST_4 = 100020,

    GO_REFECTORY_DOOR = 183296, // door opened when blackheart the inciter dies
    GO_SCREAMING_HALL_DOOR = 183295,

    SAY_HELLMAW_INTRO = -1555000,
    SPELL_HELLMAW_BANISH = 30231,

    // Incite Chaos Casters
    NPC_INCITER_1 = 19300,
    NPC_INCITER_2 = 19301,
    NPC_INCITER_3 = 19302,
    NPC_INCITER_4 = 19303,
    NPC_INCITER_5 = 19304,
};

class MANGOS_DLL_DECL instance_shadow_labyrinth : public ScriptedInstance
{
public:
    instance_shadow_labyrinth(Map* pMap);

    void Initialize() override;

    void OnObjectCreate(GameObject* pGo) override;
    void OnCreatureCreate(Creature* pCreature) override;
    void OnCreatureDeath(Creature* pCreature) override;

    void SetData(uint32 uiType, uint32 uiData) override;
    uint32 GetData(uint32 uiType) override;

    const char* Save() override { return m_strInstData.c_str(); }
    void Load(const char* chrIn) override;

private:
    uint32 m_auiEncounter[MAX_ENCOUNTER];
    std::string m_strInstData;
};

#endif
