/* Copyright (C) 2015 Corecraft <https://www.worldofcorecraft.com/>
 * This program is free software licensed under GPL version 2
 * Please see the included DOCS/LICENSE.TXT for more information */

#ifndef SCRIPTS__ZULGURUB_H
#define SCRIPTS__ZULGURUB_H

enum
{
    NUM_ENCOUNTERS = 10,

    TYPE_JEKLIK = 0,
    TYPE_VENOXIS = 1,
    TYPE_MARLI = 2,
    TYPE_THEKAL = 3,
    TYPE_ARLOKK = 4,
    TYPE_HAKKAR = 5,
    TYPE_MANDOKIR = 6,
    TYPE_JINDO = 7,
    TYPE_GAHZRANKA = 8,
    TYPE_EDGE_OF_MADNESS = 9,

    NUM_DATA = 2,
    TYPE_ARLOKK_PANTHERS_COUNT = 10,
    TYPE_ARLOKK_TP_PLACE = 11,

    GO_GONG_OF_BETHEKK = 180526, // Summons Arlokk
    GO_FORCEFIELD = 180497,      // Closed when Arlokk is in progress
    NPC_ARLOKK = 14515,
    NPC_ZULIAN_PROWLER = 15101,
    NPC_HAKKAR = 14834,
    NPC_JINDO = 11380,
    NPC_CHAINED_SPIRIT = 15117,
};

class MANGOS_DLL_DECL instance_zulgurub : public ScriptedInstance
{
public:
    instance_zulgurub(Map* map);

    void Initialize() override;
    bool IsEncounterInProgress() const override;

    void SetData(uint32 type, uint32 data) override;
    uint32 GetData(uint32 type) override;

    void OnObjectCreate(GameObject* go) override;
    void OnCreatureCreate(Creature* creature) override;
    void OnCreatureDeath(Creature* creature) override;
    void OnCreatureEvade(Creature* creature) override;
    void OnPlayerDeath(Player* player) override;

    void Update(uint32 diff) override;

    const char* Save() override { return save_to_db_.c_str(); }
    void Load(const char* data) override;

private:
    uint32 encounters_[NUM_ENCOUNTERS];
    uint32 data_[NUM_DATA];
    std::string save_to_db_;
    std::vector<ObjectGuid> chained_spirits_;
    bool update_hakkar_;

    void update_hakkar_power(Creature* hakkar);
};

#endif
