/* Copyright (C) 2015 Corecraft <https://www.worldofcorecraft.com/>
 * This program is free software licensed under GPL version 2
 * Please see the included DOCS/LICENSE.TXT for more information */

#ifndef SCRIPTS__AQ40_H
#define SCRIPTS__AQ40_H

enum
{
    NUM_ENCOUNTERS = 8,

    TYPE_SKERAM = 0,
    TYPE_BUG_TRIO = 1,
    TYPE_SARTURA = 2,
    TYPE_FANKRISS = 3,
    TYPE_VISCIDUS = 4,
    TYPE_HUHURAN = 5,
    TYPE_TWIN_EMPERORS = 6,
    TYPE_OURO = 7,
    TYPE_CTHUN = 8,

    NUM_DATA = 1,

    DATA_TRIGGER_SKERAM_SPLIT = 100,
    DATA_TRIGGER_VISCIDUS_SPAWN_GLOBS = 101,
    DATA_TRIGGER_TWIN_SHARE_INFO = 102,
    DATA_TRIGGER_TWIN_TELEPORT = 103,

    GO_SKERAM_DOOR = 180636,
    GO_TWINS_ENTRY_DOOR = 180634,
    GO_TWINS_EXIT_DOOR = 180635,

    NPC_SKERAM = 15263,
    NPC_SARTURA = 15516,
    NPC_VISCIDUS = 15299,
    NPC_MASTERS_EYE = 15963,
    NPC_EMPEROR_VEKNILASH = 15275,
    NPC_EMPEROR_VEKLOR = 15276,
    NPC_OURO = 15517,
    NPC_OURO_SPAWNER = 15957,
    NPC_CTHUN = 15727,
};

class MANGOS_DLL_DECL instance_aq40 : public ScriptedInstance
{
public:
    instance_aq40(Map* map);

    void Initialize() override;
    bool IsEncounterInProgress() const override;

    void SetData(uint32 type, uint32 data) override;
    uint32 GetData(uint32 type) override;

    void OnObjectCreate(GameObject* go) override;
    void OnCreatureCreate(Creature* creature) override;

    void Update(uint32 diff) override;

    const char* Save() override { return save_to_db_.c_str(); }
    void Load(const char* data) override;

    void handle_area_trigger(Player* player, const AreaTriggerEntry* at);

private:
    uint32 encounters_[NUM_ENCOUNTERS];
    uint32 data_[NUM_DATA];
    std::string save_to_db_;
    uint32 eject_cooldown_;
    bool spawned_eye_;

    void skeram_split();
    void viscidus_globs();
    void twins_share_info();
    void twins_teleport();
};

#endif
