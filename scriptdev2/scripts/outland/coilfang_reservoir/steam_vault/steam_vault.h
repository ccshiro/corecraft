/* Copyright (C) 2006 - 2012 ScriptDev2 <http://www.scriptdev2.com/>
 * This program is free software licensed under GPL version 2
 * Please see the included DOCS/LICENSE.TXT for more information */

#ifndef DEF_STEAM_VAULT_H
#define DEF_STEAM_VAULT_H

enum
{
    MAX_ENCOUNTER = 3,

    TYPE_HYDROMANCER_THESPIA = 0,
    TYPE_MEKGINEER_STEAMRIGGER = 1,
    TYPE_WARLORD_KALITHRESH = 2,

    NPC_NAGA_DISTILLER = 17954,
    NPC_STEAMRIGGER = 17796,
    NPC_KALITHRESH = 17798,
    // NPC_THESPIA                   = 17797,

    GO_MAIN_CHAMBERS_DOOR = 183049,
    GO_ACCESS_PANEL_HYDRO = 184125,
    GO_ACCESS_PANEL_MEK = 184126,

    // DOOR GUARDS
    GUARD_ENTRY_ONE = 17722,
    GUARD_ENTRY_TWO = 17800,
    GUARD_ENTRY_THREE = 17800,
    GUARD_ENTRY_FOUR = 17803,
    GUARD_GUID_ONE = 1001416,
    GUARD_GUID_TWO = 1001414,
    GUARD_GUID_THREE = 1001415,
    GUARD_GUID_FOUR = 1001417,
};

class MANGOS_DLL_DECL instance_steam_vault : public ScriptedInstance
{
public:
    instance_steam_vault(Map* pMap);

    void Initialize() override;

    void OnCreatureCreate(Creature* pCreature) override;
    void OnObjectCreate(GameObject* pGo) override;

    void OnCreatureDeath(Creature* pCreature) override;

    void SetData(uint32 uiType, uint32 uiData) override;
    uint32 GetData(uint32 uiType) override;

    const char* Save() override { return m_strInstData.c_str(); }
    void Load(const char* chrIn) override;

private:
    uint32 m_auiEncounter[MAX_ENCOUNTER];
    std::string m_strInstData;
    bool m_doorOpen[2];

    GUIDList m_lNagaDistillerGuidList;
};

#endif
