/* Copyright (C) 2006 - 2012 ScriptDev2 <http://www.scriptdev2.com/>
 * This program is free software licensed under GPL version 2
 * Please see the included DOCS/LICENSE.TXT for more information */

#ifndef DEF_GRUULS_LAIR_H
#define DEF_GRUULS_LAIR_H

#include "precompiled.h"

enum
{
    MAX_ENCOUNTER = 2,
    TYPE_MAULGAR = 0,
    TYPE_GRUUL = 1,

    GO_PORT_GRONN_1 = 184468,
    GO_PORT_GRONN_2 = 184662,

    // Maulgar says handled by instance
    SAY_OGRE_DEATH_1 = -1565002,
    SAY_OGRE_DEATH_2 = -1565003,
    SAY_OGRE_DEATH_3 = -1565004,
    SAY_OGRE_DEATH_4 = -1565005,

    // NPC GUIDs
    NPC_MAULGAR = 18831,
    NPC_BLINDEYE = 18836,
    NPC_KIGGLER = 18835,
    NPC_KROSH = 18832,
    NPC_OLM = 18834,

    NPC_GRUUL = 19044
};

class MANGOS_DLL_DECL instance_gruuls_lair : public ScriptedInstance
{
public:
    instance_gruuls_lair(Map* pMap);

    void Initialize() override;
    bool IsEncounterInProgress() const override;

    void OnCreatureCreate(Creature* pCreature) override;
    void OnObjectCreate(GameObject* pGo) override;
    void OnCreatureDeath(Creature* pCreature) override;

    void SetData(uint32 uiType, uint32 uiData) override;
    uint32 GetData(uint32 uiType) override;

    const char* Save() override { return m_strSaveData.c_str(); }
    void Load(const char* chrIn) override;

private:
    uint32 m_auiEncounter[MAX_ENCOUNTER];
    std::string m_strSaveData;

    bool CheckAlive(uint32 entry);
};

#endif
