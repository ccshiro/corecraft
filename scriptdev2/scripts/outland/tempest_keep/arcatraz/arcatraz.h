/* Copyright (C) 2006 - 2012 ScriptDev2 <http://www.scriptdev2.com/>
 * This program is free software licensed under GPL version 2
 * Please see the included DOCS/LICENSE.TXT for more information */

#ifndef DEF_ARCATRAZ_H
#define DEF_ARCATRAZ_H

#include "precompiled.h"

enum
{
    MAX_ENCOUNTER = 9,

    TYPE_ZEREKETH = 0,
    TYPE_DALLIAH = 1,
    TYPE_SOCCOTHRATES = 2,
    TYPE_HARBINGERSKYRISS = 3, // Handled with ACID (FAIL of 20905, 20906,
                               // 20908, 20909, 20910, 20911)
    TYPE_WARDEN_1 = 4, // Handled with ACID (20905 - Blazing Trickster, 20906 -
                       // Phase-Hunter)
    TYPE_WARDEN_2 = 5,
    TYPE_WARDEN_3 = 6, // Handled with ACID (20908 - Akkiris Lightning-Waker,
                       // 20909 - Sulfuron Magma-Thrower)
    TYPE_WARDEN_4 = 7, // Handled with ACID (20910 - Twilight Drakonaar, 20911 -
                       // Blackwing Drakonaar)
    TYPE_WARDEN_5 = 8,

    NPC_MELLICHAR = 20904, // Skyriss will kill this unit
    NPC_SOCCOTHRATES = 20886,
    NPC_DALLIAH = 20885,
    NPC_MILLHOUSE = 20977,

    GO_CORE_SECURITY_FIELD_ALPHA =
        184318, // Door opened when Wrath-Scryer Soccothrates dies
    GO_CORE_SECURITY_FIELD_BETA =
        184319,              // Door opened when Dalliah the Doomsayer dies
    GO_SEAL_SPHERE = 184802, // Shield 'protecting' mellichar
    GO_POD_ALPHA = 183961,   // Pod first boss wave
    GO_POD_BETA = 183963,    // Pod second boss wave
    GO_POD_DELTA = 183964,   // Pod third boss wave
    GO_POD_GAMMA = 183962,   // Pod fourth boss wave
    GO_POD_OMEGA = 183965,   // Pod fifth boss wave

    // Argue dialog (handled in soccothrate's script)
    SAY_SOC_ARGUE_1 = -1530029,
    SAY_DAH_ARGUE_1 = -1530014,
    SAY_SOC_ARGUE_2 = -1530030,
    SAY_DAH_ARGUE_2 = -1530015,
    SAY_SOC_ARGUE_3 = -1530031,
    SAY_DAH_ARGUE_3 = -1530016,
    SAY_SOC_ARGUE_4 = -1530032,

    SAY_DH_SOC_AGGRO = -1530017,
    SAY_DH_SOC_TAUNT_1 = -1530018,
    SAY_DH_SOC_TAUNT_2 = -1530019,
    SAY_DH_SOC_TAUNT_3 = -1530020,
    SAY_DH_SOC_DIE = -1530021,

    SAY_SOC_DH_AGGRO = -1530033,
    SAY_SOC_DH_TAUNT_1 = -1530034,
    SAY_SOC_DH_TAUNT_2 = -1530035,
    SAY_SOC_DH_TAUNT_3 = -1530036,
    SAY_SOC_DH_DIE = -1530037,

    QUEST_TRIAL_OF_NAARU = 10886,
};

class MANGOS_DLL_DECL instance_arcatraz : public ScriptedInstance
{
public:
    instance_arcatraz(Map* pMap);

    void Initialize() override;

    void OnObjectCreate(GameObject* pGo) override;
    void OnCreatureCreate(Creature* pCreature) override;

    void SetData(uint32 uiType, uint32 uiData) override;
    uint32 GetData(uint32 uiType) override;

    const char* Save() override { return m_strInstData.c_str(); }
    void Load(const char* chrIn) override;

    Creature* GetSoccothrates();
    Creature* GetDalliah();

    void Update(uint32 uiDiff) override;

private:
    uint32 m_auiEncounter[MAX_ENCOUNTER];
    std::string m_strInstData;

    uint32 m_uiAggroSocTimer;
    uint32 m_uiDieSocTimer;
    uint32 m_uiAggroDahTimer;
    uint32 m_uiDieDahTimer;
};

#endif
