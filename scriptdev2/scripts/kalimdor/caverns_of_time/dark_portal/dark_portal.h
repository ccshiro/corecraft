/* Copyright (C) 2006 - 2012 ScriptDev2 <http://www.scriptdev2.com/>
 * This program is free software licensed under GPL version 2
 * Please see the included DOCS/LICENSE.TXT for more information */

#ifndef DEF_DARKPORTAL_H
#define DEF_DARKPORTAL_H

#include "precompiled.h"

enum
{
    MAX_ENCOUNTER = 6,

    TYPE_RIFT_NUMBER = 0,
    TYPE_SHIELD_PCT = 1,
    TYPE_MEDIVH = 2,
    TYPE_CHRONO_LORD = 3,
    TYPE_TEMPORUS = 4,
    TYPE_AEONUS = 5,

    WORLD_STATE_EVENT_IN_PROGRESS = 2541, // 1/0
    WORLD_STATE_SHIELD_PCT = 2540,
    WORLD_STATE_RIFT_NR = 2784,

    QUEST_OPENING_PORTAL = 10297,
    QUEST_MASTER_TOUCH = 9836,

    // event controlers
    NPC_TIME_RIFT = 17838,
    NPC_MEDIVH = 15608,

    // main bosses
    NPC_CHRONO_LORD_DEJA = 17879,
    NPC_TEMPORUS = 17880,
    NPC_AEONUS = 17881,

    // boss replacements for heroic
    NPC_CHRONO_LORD = 21697,
    NPC_TIMEREAVER = 21698,

    // portal guardians
    NPC_RIFT_KEEPER_1 = 21104,
    NPC_RIFT_KEEPER_2 = 21148,
    NPC_RIFT_LORD_1 = 17839,
    NPC_RIFT_LORD_2 = 21140,

    // portal summons
    NPC_ASSASSIN_1 = 17835,
    NPC_ASSASSIN_2 = 21137,
    NPC_WHELP = 21818,
    NPC_CHRONOMANCER_1 = 17892,
    NPC_CHRONOMANCER_2 = 21136,
    NPC_EXECUTIONER_1 = 18994,
    NPC_EXECUTIONER_2 = 21138,
    NPC_VANQUISHER_1 = 18995,
    NPC_VANQUISHER_2 = 21139,

    // additional npcs
    NPC_COUNCIL_ENFORCER = 17023,
    NPC_TIME_KEEPER = 17918,
    NPC_SAAT = 20201,
    NPC_DARK_PORTAL_DUMMY = 18625,         // Unknown
    NPC_DARK_PORTAL_BLACK_CRYSTAL = 18553, // Crystal that spins around medivh
    NPC_DARK_PORTAL_BEAM =
        18555, // purple beams which travel from Medivh to the Dark Portal

    // event spells
    SPELL_RIFT_CHANNEL = 31387, // This is a link between the portal and its
                                // keeper. Visual effect only
    SPELL_CORRUPT =
        31326, // The npcs that attack medivh use this to damage his shield
    SPELL_CORRUPT_BOSS = 37853, // Aeonus use this spell to damage Medivh; it
                                // ticks with a 1 sec interval instead of 3
    SPELL_CORRUPT_TRIGGERED = 150006, // The server-side only spell we added
                                      // that triggers when corrupt does damage
    SPELL_MEDIVH_CHANNEL = 31556,     // Medivh channels this to open the portal
    SPELL_BLACK_CRYSTAL_STATE = 32563, // The crystals that spin around medivh
                                       // and later explode at 75%, 50% and 25%
    SPELL_CRYSTAL_ARCANE_EXPLOSION = 32614, // The explosion of energy that
                                            // occurs at aforementioned
                                            // percentages
    SPELL_RUNE_CIRCLE = 32570,              // Circle on medivh

    SPELL_BANISH_HELPER =
        31550, // used by the main bosses to banish the time keeprs
    SPELL_CORRUPT_AEONUS = 37853, // used by Aeonus to corrupt Medivh

    // cosmetic spells
    SPELL_PORTAL_CRYSTAL =
        32564, // summons 18553 - Dark Portal Crystal stalker (We dont use this)
    SPELL_BANISH_GREEN = 32567,

    // Dialog
    SAY_SAAT_WELCOME = -1269019,
    SAY_MEDIVH_INTRO = -1269021,
    SAY_MEDIVH_START_EVENT = -1269020,
    SAY_MEDIVH_WIN = -1269026,
    SAY_MEDIVH_FAIL = -1269025,
    SAY_MEDIVH_WEAK_75 = -1269022,
    SAY_MEDIVH_WEAK_50 = -1269023,
    SAY_MEDIVH_WEAK_25 = -1269024,
    SAY_ORCS_ENTER = -1269027,
    SAY_ORCS_ANSWER = -1269028,

    // Area triggers
    AREATRIGGER_SAAT = 4485,
    // AREATRIGGER_MEDIVH               = 4288,          // Doesn't contain a
    // box

    // Gameobjects
    GO_PINK_PORTAL = 185103,

    // Quests
    QUEST_OPENING_OF_THE_DARK_PORTAL = 10297,
    QUEST_MASTERS_TOUCH = 9836,
    ITEM_RESOTRED_APPRENTICE_KEY = 24489,

    MAX_ORC_WAVES = 5,
};

class MANGOS_DLL_DECL instance_dark_portal : public ScriptedInstance
{
public:
    instance_dark_portal(Map* pMap);

    void Initialize() override;
    void ResetEvent();

    void OnPlayerEnter(Player* pPlayer) override;
    void OnCreatureCreate(Creature* pCreature) override;

    void SetData(uint32 uiType, uint32 uiData) override;
    uint32 GetData(uint32 uiType) override;

    const char* Save() override { return m_strInstData.c_str(); }
    void Load(const char* chrIn) override;

    void HandleAreaTrigger(uint32 triggerId);

    void Update(uint32 uiDiff) override;

    // Outside reaction stuff
    void StartEvent();
    void SpawnNextPortal();
    bool IsBossWave();
    bool IsAttackMedivhCreature(uint32 entry) const;
    void OnNewPortal(ObjectGuid portal) { m_activePortals.push_back(portal); }
    void OnPortalClear(ObjectGuid portal,
        std::vector<ObjectGuid> remainingSummons, bool aeonusPortal);

private:
    void OnEventCompleted();
    void SpawnOrcs(Creature* spawner);
    void RetreatOrcs();
    void MoveOrc(Creature* orc, float x, float y, float z);

    uint32 m_auiEncounter[MAX_ENCOUNTER];
    std::string m_strInstData;

    uint32 m_eventStarted;

    std::vector<ObjectGuid> m_activePortals;
    std::vector<ObjectGuid> m_createFollows; // We cannot call follow on a newly
                                             // created creature, we must wait
                                             // until next update (or we'll
                                             // crash the server)
    std::vector<ObjectGuid> m_cleanupMobs;   // Mobs to cleanup (that are no
    // longer owned by a portal) when the
    // event resets
    std::vector<float*> m_availableSpawnPositions;
    time_t m_saatSpeakCooldown;
    uint32 m_uiNextPortalTimer;

    // Win RP Event
    uint32 m_uiPhase;
    uint32 m_uiOrcPhase;
    uint32 m_uiNextPhaseTimer;
    ObjectGuid m_orcSpokesPerson;
    std::vector<ObjectGuid> m_orcWaves[MAX_ORC_WAVES];
};

#endif
