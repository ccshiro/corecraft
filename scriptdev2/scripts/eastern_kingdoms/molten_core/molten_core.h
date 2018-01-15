/* Copyright (C) 2015 Corecraft <https://www.worldofcorecraft.com/>
 * This program is free software licensed under GPL version 2
 * Please see the included DOCS/LICENSE.TXT for more information */

#ifndef SCRIPTS__MOLTEN_CORE_H
#define SCRIPTS__MOLTEN_CORE_H

enum
{
    NUM_ENCOUNTERS = 10,

    TYPE_LUCIFRON = 0,
    TYPE_MAGMADAR = 1,
    TYPE_GEHENNAS = 2,
    TYPE_GARR = 3,
    TYPE_SHAZZRAH = 4,
    TYPE_BARON_GEDDON = 5,
    TYPE_GOLEMAGG = 6,
    TYPE_SULFURON = 7,
    TYPE_MAJORDOMO = 8,
    TYPE_RAGNAROS = 9,

    // When the rune is doused the associated encounter is set to status SPECIAL
    GO_RUNE_OF_KRESS = 176956, // Magmadar
    GO_RUNE_OF_MOHN = 176957,  // Gehennas
    GO_RUNE_OF_BLAZ = 176955,  // Garr
    GO_RUNE_OF_MAZJ = 176953,  // Shazzrah
    GO_RUNE_OF_ZETH = 176952,  // Baron Geddon
    GO_RUNE_OF_THERI = 176954, // Golemagg
    GO_RUNE_OF_KORO = 176951,  // Sulfuron Harbringer

    NPC_MAJORDOMO = 12018,

    MAJORDOMO_SPECIAL2 = 5, // This is the value the Majordomo encounter has
                            // when spawned at ragnaros
};

class MANGOS_DLL_DECL instance_molten_core : public ScriptedInstance
{
public:
    instance_molten_core(Map* map);

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
    std::string save_to_db_;
    bool update_runes_;

    void update_majordomo_runes();
};

#endif
