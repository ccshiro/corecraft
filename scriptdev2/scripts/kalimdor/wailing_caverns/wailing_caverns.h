/* Copyright (C) 2006 - 2012 ScriptDev2 <http://www.scriptdev2.com/>
 * Copyright (C) 2015 Corecraft <https://www.worldofcorecraft.com/>
 * This program is free software licensed under GPL version 2
 * Please see the included DOCS/LICENSE.TXT for more information */

#ifndef DEF_WAILING_CAVERNS_H
#define DEF_WAILING_CAVERNS_H

#include "precompiled.h"

enum
{
    MAX_ENCOUNTER = 6,

    TYPE_ANACONDRA = 0,
    TYPE_COBRAHN = 1,
    TYPE_PYTHAS = 2,
    TYPE_SERPENTIS = 3,
    TYPE_DISCIPLE = 4,
    TYPE_MUTANUS = 5,

    NPC_DISCIPLE = 3678,
    NPC_ANACONDRA = 3671,
    NPC_COBRAHN = 3669,
    NPC_PYTHAS = 3670,
    NPC_SERPENTIS = 3673,
    NPC_MUTANUS = 3654,
    NPC_DRUID_OF_THE_FANG = 3840,

    // Possible NPCs that can turn into Anacondra
    GUID_ANACONDRA0 = 26252,
    GUID_ANACONDRA1 = 27366,
    GUID_ANACONDRA2 = 18687,

    SAY_LORDS_DEAD = -1043000, // Zone yell when first 4 encounters are finished

    GO_MYSTERIOUS_CHEST = 180055, // used for quest 7944; spawns in the instance
                                  // only if one of the players has the quest

    QUEST_FORTUNE_AWAITS = 7944,
};

class MANGOS_DLL_DECL instance_wailing_caverns : public ScriptedInstance
{
public:
    instance_wailing_caverns(Map* pMap);
    ~instance_wailing_caverns() {}

    void Initialize() override;

    void OnPlayerEnter(Player* pPlayer) override;
    void OnCreatureCreate(Creature* pCreature) override;
    void OnCreatureDeath(Creature* creature) override;
    void OnObjectCreate(GameObject* pGo) override;

    void Update(uint32 diff) override;

    void SetData(uint32 uiType, uint32 uiData) override;
    uint32 GetData(uint32 uiType) override;

    const char* Save() override { return m_strInstData.c_str(); }
    void Load(const char* chrIn) override;

protected:
    uint32 m_auiEncounter[MAX_ENCOUNTER];
    std::string m_strInstData;

    std::vector<ObjectGuid> fangs;
    ObjectGuid anacondra_target;
};
#endif
