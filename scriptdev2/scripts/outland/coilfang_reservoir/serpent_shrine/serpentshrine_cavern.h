/* Copyright (C) 2013 Corecraft <https://www.worldofcorecraft.com/>
 * This program is free software licensed under GPL version 2
 * Please see the included DOCS/LICENSE.TXT for more information */

#ifndef DEF_SERPENTSHRINE_CAVERN_H
#define DEF_SERPENTSHRINE_CAVERN_H

enum
{
    MAX_ENCOUNTER = 6,
    TYPE_HYDROSS = 0,
    TYPE_LURKER_BELOW = 1,
    TYPE_LEOTHERAS = 2,
    TYPE_KARATHRESS = 3,
    TYPE_MOROGRIM = 4,
    TYPE_LADY_VASHJ = 5,

    MAX_DATA = 4,
    TYPE_CONSOLES = 6,          // Saved to DB (Bitmask of toggled consoles)
    TYPE_SPELLBINDERS = 7,      // How many have been killed
    TYPE_KARATHRESS_GUARDS = 8, // How many have been killed
    TYPE_SPOREBAT_COUNT = 9,    // How many sporebats are currently alive

    TYPE_WATER_BOILING =
        100, // Not SAVED. Calculated on each GetData using m_vashjirGuards

    NPC_LURKER_BELOW = 21217,
    NPC_LEOTHERAS = 21215,
    NPC_SHADOW_LEOTHERAS = 21875, // 15% health break loose demon
    NPC_GREYHEART_SPELLBINDER = 21806,
    NPC_KARATHRESS = 21214,
    NPC_CARIDBIS = 21964,
    NPC_TIDALVESS = 21965,
    NPC_SHARKKIS = 21966,
    NPC_MOROGRIM = 21213,
    NPC_LADY_VASHJ = 21212,

    GO_HYDROSS_CONSOLE = 185114,
    GO_LURKER_CONSOLE = 185115,
    GO_LEOTHERAS_CONSOLE = 185116,
    GO_KARATHRESS_CONSOLE = 185117,
    GO_MOROGRIM_CONSOLE = 185118,
    GO_VASHJ_CONSOLE = 184568, // For the bridge
    GO_BRIDGE_PART_1 = 184203,
    GO_BRIDGE_PART_2 = 184204,
    GO_BRIDGE_PART_3 = 184205,

    NPC_TOXIC_SPOREBAT = 22140,
    NPC_VASHJIR_HONOR_GUARD = 21218,
    NPC_COILFANG_FRENZY = 21508,
    NPC_COILFANG_FRENZY_CORPSE = 21689,

    // Lady Vashj Specifics
    ITEM_TAINTED_CORE = 31088,
    NPC_SHIELD_GENERATOR = 100035,
    GO_GENERATOR_1 = 185051,
    GO_GENERATOR_2 = 185052,
    GO_GENERATOR_3 = 185053,
    GO_GENERATOR_4 = 185054,

    SAY_LEOTHERAS_AGGRO = -1548009,
    SPELL_LEOTHERAS_BANISH = 37546, // Added by creature_template_addon

    SAY_KARATHRESS_GUARD_DEATH_1 = -1548023,
    SAY_KARATHRESS_GUARD_DEATH_2 = -1548024,
    SAY_KARATHRESS_GUARD_DEATH_3 = -1548025,
    SPELL_POWER_OF_SHARKKIS = 38455,
    SPELL_POWER_OF_TIDALVESS = 38452,
    SPELL_POWER_OF_CARIDBIS = 38451,

    SPELL_SCALDING_WATER = 37284,
    WATER_UPDATE_INTERVAL = 1000,
};

class MANGOS_DLL_DECL instance_serpentshrine_cavern : public ScriptedInstance
{
public:
    instance_serpentshrine_cavern(Map* pMap);

    void Initialize() override;
    bool IsEncounterInProgress() const override;

    void Update(uint32 diff) override;

    void OnObjectCreate(GameObject* pGo) override;
    void OnCreatureCreate(Creature* pCreature) override;
    void OnCreatureDeath(Creature* pCreature) override;
    void OnPlayerLeave(Player* pPlayer) override;

    void SetData(uint32 uiType, uint32 uiData) override;
    uint32 GetData(uint32 uiType) override;

    const char* Save() override { return m_strInstData.c_str(); }
    void Load(const char* chrIn) override;

    void ResetLadyVashjEncounter();
    void ToggleShieldGenerators(bool interactable);

private:
    uint32 m_auiEncounter[MAX_ENCOUNTER];
    uint32 m_auiData[MAX_DATA];
    std::string m_strInstData;

    void SpawnDeadFishes();

    std::vector<ObjectGuid> m_vashjirGuards;
    uint32 m_waterUpdate;
    time_t m_fishNextSpawn;
};

#endif
