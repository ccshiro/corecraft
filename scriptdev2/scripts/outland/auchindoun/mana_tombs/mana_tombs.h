/* Copyright (C) 2013 Corecraft */

#ifndef DEF_MANA_TOMBS_H
#define DEF_MANA_TOMBS_H

#include "precompiled.h"

enum
{
    MAX_ENCOUNTER = 4,

    TYPE_PANDEMONIUS = 0,
    TYPE_TAVAROK = 1,
    TYPE_SHAFFAR = 2,
    TYPE_SHAHEEN = 3, // Escort

    QUEST_SAFTEY_IS_JOB_ONE = 10216,
    QUEST_SOMEONE_ELSES_WORK = 10218,
    NPC_SHAHEEN = 19671,
};

class MANGOS_DLL_DECL instance_mana_tombs : public ScriptedInstance
{
public:
    instance_mana_tombs(Map* pMap);
    ~instance_mana_tombs() {}

    void Initialize() override;
    void OnObjectCreate(GameObject* pGo) override;
    void OnCreatureCreate(Creature* pCreature) override;

    void SetData(uint32 uiType, uint32 uiData) override;
    uint32 GetData(uint32 uiType) override;

    const char* Save() override { return m_strInstData.c_str(); }
    void Load(const char* chrIn) override;

    Creature* GetShaheen() { return GetSingleCreatureFromStorage(NPC_SHAHEEN); }

private:
    uint32 m_auiEncounter[MAX_ENCOUNTER];
    std::string m_strInstData;
};

#endif
