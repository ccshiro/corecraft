/* Copyright (C) 2015 Corecraft <https://www.worldofcorecraft.com/>
 * This program is free software licensed under GPL version 2
 * Please see the included DOCS/LICENSE.TXT for more information */

#ifndef SCRIPTS__ZULGURUB_H
#define SCRIPTS__ZULGURUB_H

enum
{
    NUM_ENCOUNTERS = 8,

    TYPE_RAZORGORE = 0,
    TYPE_VAELASTRASZ = 1,
    TYPE_BROODLORD = 2,
    TYPE_FIREMAW = 3,
    TYPE_EBONROC = 4,
    TYPE_FLAMEGOR = 5,
    TYPE_CHROMAGGUS = 6,
    TYPE_NEFARIAN = 7,

    NUM_DATA = 2,
    DATA_DESTROYED_EGGS_COUNT = 8, // Razorgore
    DATA_KILLED_DRAKONIDS = 9,     // Nefarian

    DATA_TRIGGER_BREATH_ONE = 100, // Chromaggus toggles to cast breath #1
    DATA_TRIGGER_BREATH_TWO = 101, // Chromaggus toggles to cast breath #2
    DATA_TRIGGER_NEFARIAN_DRAKONIDS = 110,
    DATA_TRIGGER_NEFARIAN_CLASS_CALL = 111,

    RAZORGORE_EGGS_COUNT = 30,
    NEFARIAN_DRAKONID_COUNT = 40,

    GO_RAZORGORE_DOOR = 176964,
    GO_VAELASTRASZ_DOOR = 176965,
    GO_HALLS_OF_STRIFE_DOOR = 179364,
    GO_BROODLORD_DOOR = 179365,
    GO_CHROMAGGUS_GUARDING_DOOR = 179117,
    GO_NEFARIAN_DOOR = 176966,
    GO_BLACK_DRAGON_EGG = 177807,
    GO_ORB_OF_DOMINATION = 177808,
    GO_SUPPRESSION_DEVICE = 179784,

    NPC_RAZORGORE = 12435,
    NPC_GRETHOK_THE_CONTROLLER = 12557,
    NPC_BLACKWING_GUARSMAN = 14456,
    NPC_VAEL_RP_TRIGGER = 100186,
    NPC_CHROMAGGUS = 14020,
    NPC_NEFARIUS = 10162,
    NPC_NEFARIAN = 11583,

    SPELL_MIND_EXHAUSTION = 23958,
    SPELL_POSSESS = 19832,
};

class MANGOS_DLL_DECL instance_blackwing_lair : public ScriptedInstance
{
public:
    instance_blackwing_lair(Map* map);

    void Initialize() override;
    bool IsEncounterInProgress() const override;

    void SetData(uint32 type, uint32 data) override;
    uint32 GetData(uint32 type) override;

    void OnObjectCreate(GameObject* go) override;
    void OnCreatureCreate(Creature* creature) override;

    void Update(uint32 diff) override;

    const char* Save() override { return save_to_db_.c_str(); }
    void Load(const char* data) override;

private:
    uint32 encounters_[NUM_ENCOUNTERS];
    uint32 data_[NUM_DATA];
    std::string save_to_db_;
    uint32 chromaggus_colors_;
    uint32 nefarian_colors_;

    uint32 razorgore_add_timer_;
    uint32 wave_;
    bool spawned_vael_rp_trigger_;

    void class_call();
};

#endif
