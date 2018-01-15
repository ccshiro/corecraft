/* Copyright (C) 2006 - 2012 ScriptDev2 <http://www.scriptdev2.com/>
 * This program is free software licensed under GPL version 2
 * Please see the included DOCS/LICENSE.TXT for more information */

#ifndef DEF_SETHEKK_HALLS_H
#define DEF_SETHEKK_HALLS_H

enum
{
    MAX_ENCOUNTER = 3,

    TYPE_SYTH = 0,
    TYPE_ANZU = 1,
    TYPE_IKISS = 2,

    GO_IKISS_DOOR = 183398,
    GO_IKISS_CHEST = 187372,
    GO_LAKKA_CAGE = 183051,

    NPC_LAKKA = 18956,
};

class MANGOS_DLL_DECL instance_sethekk_halls : public ScriptedInstance
{
public:
    instance_sethekk_halls(Map* pMap);
    ~instance_sethekk_halls() {}

    void Initialize() override;
    void OnObjectCreate(GameObject* pGo) override;
    void OnCreatureCreate(Creature* pCreature) override;

    void SetData(uint32 uiType, uint32 uiData) override;
    uint32 GetData(uint32 uiType) override;

    const char* Save() override { return m_strInstData.c_str(); }
    void Load(const char* chrIn) override;

private:
    uint32 m_auiEncounter[MAX_ENCOUNTER];
    std::string m_strInstData;
};

#endif
