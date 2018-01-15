/* Copyright (C) 2006 - 2012 ScriptDev2 <http://www.scriptdev2.com/>
 * This program is free software licensed under GPL version 2
 * Please see the included DOCS/LICENSE.TXT for more information */

#ifndef DEF_MAGTHERIDONS_LAIR_H
#define DEF_MAGTHERIDONS_LAIR_H

#include "precompiled.h"

enum
{
    MAX_ENCOUNTER = 1,

    TYPE_MAGTHERIDON = 0,

    NPC_MAGTHERIDON = 17257,
    NPC_CHANNELER = 17256,
    NPC_HELLFIRE_RAID_TRIGGER = 17376,
    NPC_BLAZE = 100033,

    EMOTE_MAG_START = -1544014,
    EMOTE_MAG_1_MIN = -1544012,
    EMOTE_MAG_FREE = -1544015,
    SAY_MAG_FREED = -1544006,

    GO_MANTICRON_CUBE = 181713,
    GO_MAGTHERIDON_DOOR = 183847,
    GO_RAID_FX = 184653,
    GO_COLUMN_0 = 184638,
    GO_COLUMN_1 = 184639,
    GO_COLUMN_2 = 184635,
    GO_COLUMN_3 = 184634,
    GO_COLUMN_4 = 184636,
    GO_COLUMN_5 = 184637,
    GO_BLAZE = 181832, // Saved for cleanup purpose

    SPELL_OOC_SHADOW_CAGE = 30205,
    SPELL_SHADOW_CAGE = 30168,
    SPELL_SHADOW_GRASP = 30410,         // The player version
    SPELL_SHADOW_GRASP_TRIGGER = 30166, // The hellfire raid trigger version
    SPELL_MIND_EXHAUSTION = 44032,
};

class MANGOS_DLL_DECL instance_magtheridons_lair : public ScriptedInstance
{
public:
    instance_magtheridons_lair(Map* pMap);

    void Initialize() override;

    bool IsEncounterInProgress() const override;

    void OnCreatureCreate(Creature* pCreature) override;
    void OnObjectCreate(GameObject* pGo) override;
    void OnCreatureEnterCombat(Creature* pCreature) override;

    void SetData(uint32 uiType, uint32 uiData) override;
    uint32 GetData(uint32 uiType) override;

    void Update(uint32 uiDiff) override;

    void OnCubeClick(GameObject* go, Player* p);

    std::vector<std::pair<ObjectGuid, time_t>> m_blazes;

private:
    uint32 m_auiEncounter[MAX_ENCOUNTER];

    uint32 m_releaseTimer;
    uint32 m_releaseStage;

    std::vector<ObjectGuid> m_blazeGo;

    std::vector<ObjectGuid> m_cubes;
    std::vector<ObjectGuid> active_hellfire_triggers;

    void ToggleCubeInteractable(bool enable);
    void UpdateCubes(uint32 uiDiff);
};

#endif
