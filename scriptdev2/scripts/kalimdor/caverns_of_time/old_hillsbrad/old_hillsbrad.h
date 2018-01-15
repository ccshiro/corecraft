/* Copyright (C) 2006 - 2012 ScriptDev2 <http://www.scriptdev2.com/>
 * This program is free software licensed under GPL version 2
 * Please see the included DOCS/LICENSE.TXT for more information */

#ifndef DEF_OLD_HILLSBRAD_H
#define DEF_OLD_HILLSBRAD_H

#include "precompiled.h"

enum
{
    MAX_ENCOUNTER = 8,

    TYPE_BARREL_DIVERSION = 0,
    TYPE_THRALL_EVENT = 1,
    TYPE_THRALL_PART1 = 2,  // prison to keep
    TYPE_THRALL_PART2 = 3,  // keep to barn
    TYPE_THRALL_PART3 = 4,  // barn to inn
    TYPE_THRALL_PART4 = 5,  // inn to boss
    TYPE_THRALL_PART5 = 6,  // post-event is over if this is done, and Erozion
                            // can offer teleport out
    TYPE_THRALL_DEATHS = 7, // Number of times thrall has died

    NPC_THRALL = 17876,
    NPC_TARETHA = 18887,
    NPC_DRAKE = 17848,
    NPC_LODGE_QUEST_TRIGGER = 20155,
    NPC_EPOCH = 18096,

    NPC_DURNHOLDE_LOOKOUT = 22128,
    NPC_ARMORER = 18764,
    NPC_SKARLOC_MOUNT = 18798,
    NPC_IMAGE_OF_EROZION = 19438,

    QUEST_ENTRY_HILLSBRAD = 10282,
    QUEST_ENTRY_DIVERSION = 10283,
    QUEST_ENTRY_ESCAPE = 10284,
    QUEST_ENTRY_RETURN = 10285,

    GO_TH_PRISON_DOOR = 184393,
    GO_ROARING_FLAME = 182592,

    ITEM_ENTRY_INCENDIARY_BOMBS = 25853,

    SPELL_SHADOW_PRISON = 33071,

    WORLD_STATE_OH = 2436,
};

class MANGOS_DLL_DECL instance_old_hillsbrad : public ScriptedInstance
{
public:
    instance_old_hillsbrad(Map* pMap);
    ~instance_old_hillsbrad() {}

    void Initialize() override;

    void Update(const uint32 uiDiff) override;

    void OnCreatureCreate(Creature* pCreature) override;
    void OnObjectCreate(GameObject* pObject) override;
    void OnCreatureDeath(Creature* pCreature) override;
    void OnCreatureEnterCombat(Creature* pCreature) override;

    void SetData(uint32 uiType, uint32 uiData) override;
    uint32 GetData(uint32 uiType) override;

    void Load(const char* chrIn) override;
    const char* Save() override { return m_strInstData.c_str(); }

    void ResetThrallEvent();

    void UpdateLodgeQuestCredit();

    Creature* GetThrall() { return GetSingleCreatureFromStorage(NPC_THRALL); }
    Creature* GetTaretha() { return GetSingleCreatureFromStorage(NPC_TARETHA); }
    Creature* GetEpoch() { return GetSingleCreatureFromStorage(NPC_EPOCH); }

protected:
    uint32 m_auiEncounter[MAX_ENCOUNTER];
    std::string m_strInstData;

    uint32 m_uiBarrelCount;
    uint32 m_uiRoaringFlames;

    std::vector<ObjectGuid> m_roaringFlames;
    std::vector<ObjectGuid> m_lookouts;
};

#endif
