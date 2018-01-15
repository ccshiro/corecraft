/* Copyright (C) 2012-2013 Corecraft
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* ScriptData
SDName: event_gauntlet_of_flame
SD%Complete:
SDComment:
SDCategory: Hellfire Citadel, Shattered Halls
EndScriptData */

/* ContentData

EndContentData */

#include "precompiled.h"
#include "shattered_halls.h"
#include <iterator>

enum
{
    NPC_ZEALOT_ENTRY = 17462,
    // Zealot guids
    ZEALOT_LEFT_ONE = 1001046,
    ZEALOT_LEFT_TWO = 1001047,
    ZEALOT_LEFT_THREE = 1001048,
    ZEALOT_LEFT_FOUR = 1001049,
    ZEALOT_RIGHT_ONE = 1001042,
    ZEALOT_RIGHT_TWO = 1001043,
    ZEALOT_RIGHT_THREE = 1001044,
    ZEALOT_RIGHT_FOUR = 1001045,

    ZEALOT_BG_ONE = 62933,
    ZEALOT_BG_TWO = 62928,
    ZEALOT_BG_THREE = 62930,
    ZEALOT_BG_FOUR = 62923,

    ZEALOT_START_ONE = 1001050,
    ZEALOT_START_TWO = 1001051,
    ZEALOT_START_THREE = 1001052,
    NPC_SCOUT_ENTRY = 17693,
    SCOUT_GUID = 1001053,

    NPC_ARCHER_ENTRY = 17427,
    ARCHER_LEFT_GUID = 62872,
    ARCHER_RIGHT_GUID = 62871,

    NPC_ENTRY_BLOOD_GUARD_N = 17461,
    NPC_ENTRY_BLOOD_GUARD_H = 20923,
    BLOOD_GUARD_N_GUID = 62921,
    BLOOD_GUARD_H_GUID = 1001055,

    SPELL_FLAME_ARROW = 30952,
    SPELL_FLAME_ARROW_CAST_TIME =
        2000 + 600,               // (cast time of spell + range weap delay)
    SPELL_FLAME_ARROW_SPEED = 40, // Travel speed of projectile
    SPELL_FIRE_PATCH_N = 23971,   // Stolen from Nazan & Vazruden fight, not
                                  // exactly correct but close enough
    SPELL_FIRE_PATCH_H = 30928,
    NPC_TARGET_DUMMY_ENTRY = 100005,

    TEXT_BLOOD_GUARD_RALLY = -1500001,
    TEXT_BLOOD_GUARD_READY = -1500002,
    TEXT_BLOOD_GUARD_AIM = -1500003,
    TEXT_BLOOD_GUARD_FIRE = -1500004,
    TEXT_SCOUT_INVADERS = -1500005
};

#define ZEALOT_ONE_PATH_LEN 8
const DynamicWaypoint zealotOnePath[ZEALOT_ONE_PATH_LEN] = {
    DynamicWaypoint(516.7f, 279.4f, 1.917f, 100, 0, true),
    DynamicWaypoint(518.0f, 295.6f, 1.926f, 100, 0, true),
    DynamicWaypoint(496.5f, 316.4f, 1.944f, 100, 0, true),
    DynamicWaypoint(447.8f, 315.1f, 1.922f, 100, 0, true),
    DynamicWaypoint(399.5f, 314.6f, 1.911f, 100, 0, true),
    DynamicWaypoint(354.9f, 315.0f, 1.918f, 100, 0, true),
    DynamicWaypoint(323.7f, 313.8f, 1.917f, 100, 0, true),
    DynamicWaypoint(286.2f, 314.3f, 1.882f, 100, 0, true),
};

#define ZEALOT_TWO_PATH_LEN 8
static DynamicWaypoint zealotTwoPath[ZEALOT_TWO_PATH_LEN] = {
    DynamicWaypoint(519.2f, 279.0f, 1.917f, 100, 0, true),
    DynamicWaypoint(519.5f, 294.9f, 1.926f, 100, 0, true),
    DynamicWaypoint(493.1f, 316.5f, 1.944f, 100, 0, true),
    DynamicWaypoint(447.6f, 317.4f, 1.920f, 100, 0, true),
    DynamicWaypoint(400.0f, 317.6f, 1.907f, 100, 0, true),
    DynamicWaypoint(355.2f, 317.9f, 1.918f, 100, 0, true),
    DynamicWaypoint(324.3f, 316.4f, 1.918f, 100, 0, true),
    DynamicWaypoint(286.7f, 318.1f, 1.881f, 100, 0, true),
};

struct MANGOS_DLL_DECL trigger_gauntlet_of_flame : public ScriptedAI
{
    trigger_gauntlet_of_flame(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_pInstance = (ScriptedInstance*)pCreature->GetInstanceData();
        m_bIsRegularMode = pCreature->GetMap()->IsRegularDifficulty();
        Reset();
    }

    ScriptedInstance* m_pInstance;
    bool m_bIsRegularMode;
    bool m_bIsEventStarted;
    bool m_bRegisteredBloodGuardInCombat;
    uint32 m_uiStartArchersTimer;
    uint32 m_uiArcherStartingPhase;
    uint32 m_uiSpawnZealotsTimer;
    uint32 m_uiArcherOneTimer;
    uint32 m_uiArcherTwoTimer;
    uint32 m_uiDelayedRespawnTimer;
    std::vector<ObjectGuid> m_delayedRespawns;
    std::vector<ObjectGuid> m_spawnedZealots;
    std::vector<std::pair<uint32, ObjectGuid>> m_pendingFlames;
    uint32 m_uiMinimumResetTimer;

    void Reset() override
    {
        SetCombatMovement(false);
        if (m_pInstance)
            m_pInstance->SetData(TYPE_GAUNTLET_OF_FLAME, NOT_STARTED);
        m_bIsEventStarted = false;
        m_bRegisteredBloodGuardInCombat = false;
        m_uiStartArchersTimer = 0;
        m_uiArcherStartingPhase = 0;
        m_uiSpawnZealotsTimer = 0;
        m_uiArcherOneTimer = 0;
        m_uiArcherTwoTimer = 0;
        m_uiDelayedRespawnTimer = 0;
        m_uiMinimumResetTimer = 60 * 1000;
        RespawnEvent();
        m_pendingFlames.clear();
    }

    void MoveInLineOfSight(Unit* pWho) override
    {
        if (pWho->GetTypeId() != TYPEID_PLAYER)
            return;
        if (((Player*)pWho)->isGameMaster())
            return;
        if (m_bIsEventStarted || !m_pInstance)
            return;
        if (m_pInstance->GetData(TYPE_GAUNTLET_OF_FLAME) == DONE)
            return;
        if (pWho->GetDistance(m_creature) > 50.0f)
            return;

        StartEvent();
        m_bIsEventStarted = true;
    }

    void DoZealotRespawn(Creature* zealot)
    {
        zealot->movement_gens.remove_if([](auto*)
            {
                return true;
            });
        zealot->StopMoving();
        m_delayedRespawns.push_back(zealot->GetObjectGuid());
    }

    void DoZealotWpMove(Creature* zealot)
    {
        zealot->movement_gens.push(new movement::WaypointMovementGenerator());
    }

    Creature* GetBloodGuard() const
    {
        return m_bIsRegularMode ?
                   m_creature->GetMap()->GetCreature(ObjectGuid(HIGHGUID_UNIT,
                       (uint32)NPC_ENTRY_BLOOD_GUARD_N,
                       (uint32)BLOOD_GUARD_N_GUID)) :
                   m_creature->GetMap()->GetCreature(ObjectGuid(HIGHGUID_UNIT,
                       (uint32)NPC_ENTRY_BLOOD_GUARD_H,
                       (uint32)BLOOD_GUARD_H_GUID));
    }

    bool DoArcherLogic(Creature* a)
    {
        if (!a->getVictim())
        {
            if (Creature* target = GetArcherTarget(a))
            {
                if (a->AI() &&
                    a->AI()->CanCastSpell(target, SPELL_FLAME_ARROW, false))
                {
                    a->CastSpell(target, SPELL_FLAME_ARROW, false);
                    float dist = a->GetDistance(target);
                    m_pendingFlames.push_back(std::pair<uint32, ObjectGuid>(
                        SPELL_FLAME_ARROW_CAST_TIME +
                            (uint32)floor(
                                dist / SPELL_FLAME_ARROW_SPEED * 1000.0f),
                        target->GetObjectGuid()));
                    return true;
                }
            }
        }
        return false;
    }

    Creature* GetArcherTarget(Creature* archer) const
    {
        // Select a random player within event boundaries
        std::list<Player*> players =
            m_pInstance->GetAllPlayersInMap(true, false);
        std::vector<Player*> viablePlayers;
        for (auto& player : players)
        {
            if (archer->IsWithinDistInMap(player, 230.0f))
                viablePlayers.push_back(player);
        }
        if (viablePlayers.empty())
            return NULL;

        // Select a flame within 10 yards of that player
        Player* plr = viablePlayers[urand(0, viablePlayers.size() - 1)];
        auto targets =
            GetCreatureListWithEntryInGrid(plr, NPC_TARGET_DUMMY_ENTRY, 10.0f);
        if (targets.empty())
            return NULL;

        // 25% chance to take the closest
        if (urand(1, 100) <= 25)
        {
            Creature* closest = NULL;
            float dist = 100.0f;
            for (auto& target : targets)
            {
                float d = plr->GetDistance(target);
                if (d < dist)
                {
                    dist = d;
                    closest = target;
                }
            }
            return closest;
        }

        auto found = targets.begin() + urand(0, targets.size() - 1);
        return *found;
    }

    void OnBloodGuardEnterCombat(Creature* bloodGuard);

    void StartEvent()
    {
        if (!m_pInstance ||
            m_pInstance->GetData(TYPE_GAUNTLET_OF_FLAME) == DONE)
            return;
        m_pInstance->SetData(TYPE_GAUNTLET_OF_FLAME, IN_PROGRESS);

        // Send start zealots to attack
        if (Creature* z1 =
                m_creature->GetMap()->GetCreature(ObjectGuid(HIGHGUID_UNIT,
                    (uint32)NPC_ZEALOT_ENTRY, (uint32)ZEALOT_START_ONE)))
            m_pInstance->AttackNearestPlayer(z1);
        if (Creature* z2 =
                m_creature->GetMap()->GetCreature(ObjectGuid(HIGHGUID_UNIT,
                    (uint32)NPC_ZEALOT_ENTRY, (uint32)ZEALOT_START_TWO)))
            m_pInstance->AttackNearestPlayer(z2);
        if (Creature* z3 =
                m_creature->GetMap()->GetCreature(ObjectGuid(HIGHGUID_UNIT,
                    (uint32)NPC_ZEALOT_ENTRY, (uint32)ZEALOT_START_THREE)))
            m_pInstance->AttackNearestPlayer(z3);
        // Send scout to warn the others
        if (Creature* s = m_creature->GetMap()->GetCreature(ObjectGuid(
                HIGHGUID_UNIT, (uint32)NPC_SCOUT_ENTRY, (uint32)SCOUT_GUID)))
        {
            s->movement_gens.remove_all(movement::gen::idle);
            s->movement_gens.push(new movement::PointMovementGenerator(
                1, 499.78f, 316.07f, 1.9436f, false, true));
            DoScriptText(TEXT_SCOUT_INVADERS, s);
        }
        m_uiStartArchersTimer = 21000;
    }

    void RespawnEvent();

    void StartArchers()
    {
        if (m_uiArcherStartingPhase == 0)
        {
            if (Creature* bg = GetBloodGuard())
                DoScriptText(TEXT_BLOOD_GUARD_RALLY, bg);
            m_uiStartArchersTimer = 4200;
        }
        else if (m_uiArcherStartingPhase == 1)
        {
            if (Creature* bg = GetBloodGuard())
                DoScriptText(TEXT_BLOOD_GUARD_READY, bg);
            m_uiStartArchersTimer = 1500;
        }
        else if (m_uiArcherStartingPhase == 2)
        {
            if (Creature* bg = GetBloodGuard())
                DoScriptText(TEXT_BLOOD_GUARD_AIM, bg);
            m_uiStartArchersTimer = 1500;
        }
        else if (m_uiArcherStartingPhase == 3)
        {
            if (Creature* bg = GetBloodGuard())
                DoScriptText(TEXT_BLOOD_GUARD_FIRE, bg);
            m_uiStartArchersTimer = 1500;
        }
        ++m_uiArcherStartingPhase;
        if (m_uiArcherStartingPhase == 4)
        {
            m_uiStartArchersTimer = 0;
            m_uiSpawnZealotsTimer = 4000;
            m_uiArcherOneTimer = 50; // Fire right away
            m_uiArcherTwoTimer = 50;
            MoveAllZealots();
            // Remove the scout
            if (Creature* scout =
                    m_creature->GetMap()->GetCreature(ObjectGuid(HIGHGUID_UNIT,
                        (uint32)NPC_SCOUT_ENTRY, (uint32)SCOUT_GUID)))
            {
                scout->SetVisibility(VISIBILITY_OFF);
                scout->DealDamage(scout, scout->GetHealth(), NULL,
                    DIRECT_DAMAGE, SPELL_SCHOOL_MASK_NORMAL, NULL, false,
                    false);
            }
        }
    }

    void MoveAllZealots()
    {
        if (Creature* z =
                m_creature->GetMap()->GetCreature(ObjectGuid(HIGHGUID_UNIT,
                    (uint32)NPC_ZEALOT_ENTRY, (uint32)ZEALOT_LEFT_ONE)))
            DoZealotWpMove(z);
        if (Creature* z =
                m_creature->GetMap()->GetCreature(ObjectGuid(HIGHGUID_UNIT,
                    (uint32)NPC_ZEALOT_ENTRY, (uint32)ZEALOT_LEFT_TWO)))
            DoZealotWpMove(z);
        if (Creature* z =
                m_creature->GetMap()->GetCreature(ObjectGuid(HIGHGUID_UNIT,
                    (uint32)NPC_ZEALOT_ENTRY, (uint32)ZEALOT_LEFT_THREE)))
            DoZealotWpMove(z);
        if (Creature* z =
                m_creature->GetMap()->GetCreature(ObjectGuid(HIGHGUID_UNIT,
                    (uint32)NPC_ZEALOT_ENTRY, (uint32)ZEALOT_LEFT_FOUR)))
            DoZealotWpMove(z);
        if (Creature* z =
                m_creature->GetMap()->GetCreature(ObjectGuid(HIGHGUID_UNIT,
                    (uint32)NPC_ZEALOT_ENTRY, (uint32)ZEALOT_RIGHT_ONE)))
            DoZealotWpMove(z);
        if (Creature* z =
                m_creature->GetMap()->GetCreature(ObjectGuid(HIGHGUID_UNIT,
                    (uint32)NPC_ZEALOT_ENTRY, (uint32)ZEALOT_RIGHT_TWO)))
            DoZealotWpMove(z);
        if (Creature* z =
                m_creature->GetMap()->GetCreature(ObjectGuid(HIGHGUID_UNIT,
                    (uint32)NPC_ZEALOT_ENTRY, (uint32)ZEALOT_RIGHT_THREE)))
            DoZealotWpMove(z);
        if (Creature* z =
                m_creature->GetMap()->GetCreature(ObjectGuid(HIGHGUID_UNIT,
                    (uint32)NPC_ZEALOT_ENTRY, (uint32)ZEALOT_RIGHT_FOUR)))
            DoZealotWpMove(z);
    }

    void UpdateAI(uint32 uiDiff) override
    {
        if (!m_pInstance)
            return;

        // Skip everything if event is already done
        if (m_pInstance->GetData(TYPE_GAUNTLET_OF_FLAME) == DONE)
            return;

        Creature* bloodGuard = GetBloodGuard();
        if (!bloodGuard || !bloodGuard->AI())
            return;
        if (bloodGuard->isInCombat() && bloodGuard->getVictim() &&
            !m_bRegisteredBloodGuardInCombat)
        {
            m_bRegisteredBloodGuardInCombat = true;
            OnBloodGuardEnterCombat(bloodGuard);
            if (!m_bIsEventStarted)
                m_bIsEventStarted = true;
        }

        if (m_uiDelayedRespawnTimer)
        {
            if (m_uiDelayedRespawnTimer <= uiDiff)
            {
                for (auto& elem : m_delayedRespawns)
                    if (Creature* z = m_creature->GetMap()->GetCreature(elem))
                    {
                        z->Respawn();
                        float x, y, _z, o;
                        z->GetRespawnCoord(x, y, _z, &o);
                        z->NearTeleportTo(x, y, _z, o);
                    }
                m_delayedRespawns.clear();
                m_uiDelayedRespawnTimer = 0;
            }
            else
                m_uiDelayedRespawnTimer -= uiDiff;
        }

        if (!m_bIsEventStarted)
            return;

        // Has event ended?
        if (!bloodGuard->isAlive())
        {
            m_bIsEventStarted = false;
            m_pInstance->SetData(TYPE_GAUNTLET_OF_FLAME, DONE);
            return;
        }

        if (m_uiMinimumResetTimer <= uiDiff)
        {
            // Check so we still have alive combatants in the event
            bool endEvent = true;
            std::list<Player*> players =
                m_pInstance->GetAllPlayersInMap(true, false);
            for (auto& player : players)
                if (bloodGuard->GetDistance(player) < 250)
                {
                    endEvent = false;
                    break;
                }
            if (endEvent)
            {
                Reset();
                return;
            }
            m_uiMinimumResetTimer = 1000; // No need to check so frequently
        }
        else
            m_uiMinimumResetTimer -= uiDiff;

        if (m_uiStartArchersTimer)
        {
            if (m_uiStartArchersTimer <= uiDiff)
            {
                StartArchers();
            }
            else
                m_uiStartArchersTimer -= uiDiff;
            return; // Skip rest until this phase is done
        }

        // Keep spawning zealots that back-up (but not if blood guard is in
        // combat)
        if (!bloodGuard->isInCombat())
        {
            if (m_uiSpawnZealotsTimer < uiDiff)
            {
                if (Creature* sum = m_creature->SummonCreature(NPC_ZEALOT_ENTRY,
                        zealotOnePath[0].X, zealotOnePath[0].Y,
                        zealotOnePath[0].Z, 2.26f,
                        TEMPSUMMON_CORPSE_TIMED_DESPAWN, 2500))
                {
                    std::vector<DynamicWaypoint> temp(zealotOnePath,
                        zealotOnePath + (ZEALOT_ONE_PATH_LEN - 1));
                    sum->movement_gens.push(
                        new movement::DynamicWaypointMovementGenerator(
                            temp, false));
                    sum->SetAggroDistance(50);
                    m_spawnedZealots.push_back(sum->GetObjectGuid());
                }
                if (Creature* sum = m_creature->SummonCreature(NPC_ZEALOT_ENTRY,
                        zealotTwoPath[0].X, zealotTwoPath[0].Y,
                        zealotTwoPath[0].Z, 2.26f,
                        TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 30000))
                {
                    std::vector<DynamicWaypoint> temp(zealotTwoPath,
                        zealotTwoPath + (ZEALOT_TWO_PATH_LEN - 1));
                    sum->movement_gens.push(
                        new movement::DynamicWaypointMovementGenerator(
                            temp, false));
                    sum->SetAggroDistance(50);
                    m_spawnedZealots.push_back(sum->GetObjectGuid());
                }
                m_uiSpawnZealotsTimer = 45000;
            }
            else
                m_uiSpawnZealotsTimer -= uiDiff;
        }

        if (m_uiArcherOneTimer)
        {
            if (m_uiArcherOneTimer < uiDiff)
            {
                if (Creature* a = m_creature->GetMap()->GetCreature(
                        ObjectGuid(HIGHGUID_UNIT, (uint32)NPC_ARCHER_ENTRY,
                            (uint32)ARCHER_LEFT_GUID)))
                {
                    if (a->isAlive())
                    {
                        if (DoArcherLogic(a))
                            m_uiArcherOneTimer = urand(8000, 12000);
                    }
                    else
                        m_uiArcherOneTimer = 0;
                }
            }
            else
                m_uiArcherOneTimer -= uiDiff;
        }

        if (m_uiArcherTwoTimer)
        {
            if (m_uiArcherTwoTimer < uiDiff)
            {
                if (Creature* a = m_creature->GetMap()->GetCreature(
                        ObjectGuid(HIGHGUID_UNIT, (uint32)NPC_ARCHER_ENTRY,
                            (uint32)ARCHER_RIGHT_GUID)))
                {
                    if (a->isAlive())
                    {
                        if (DoArcherLogic(a))
                            m_uiArcherTwoTimer = urand(8000, 12000);
                    }
                    else
                        m_uiArcherTwoTimer = 0;
                }
            }
            else
                m_uiArcherTwoTimer -= uiDiff;
        }

        for (std::vector<std::pair<uint32, ObjectGuid>>::iterator itr =
                 m_pendingFlames.begin();
             itr != m_pendingFlames.end();)
        {
            if (itr->first <= uiDiff)
            {
                Creature* target =
                    m_creature->GetMap()->GetCreature(itr->second);
                if (target)
                    target->CastSpell(target,
                        m_bIsRegularMode ? SPELL_FIRE_PATCH_N :
                                           SPELL_FIRE_PATCH_H,
                        true, NULL, NULL);
                itr = m_pendingFlames.erase(itr);
            }
            else
            {
                itr->first -= uiDiff;
                ++itr;
            }
        }
    }
};

void trigger_gauntlet_of_flame::RespawnEvent()
{
    // Respawn all mobs

    // Mobs at the start
    if (Creature* z = m_creature->GetMap()->GetCreature(ObjectGuid(
            HIGHGUID_UNIT, (uint32)NPC_ZEALOT_ENTRY, (uint32)ZEALOT_START_ONE)))
        z->Respawn();
    if (Creature* z = m_creature->GetMap()->GetCreature(ObjectGuid(
            HIGHGUID_UNIT, (uint32)NPC_ZEALOT_ENTRY, (uint32)ZEALOT_START_TWO)))
        z->Respawn();
    if (Creature* z =
            m_creature->GetMap()->GetCreature(ObjectGuid(HIGHGUID_UNIT,
                (uint32)NPC_ZEALOT_ENTRY, (uint32)ZEALOT_START_THREE)))
        z->Respawn();
    if (Creature* s = m_creature->GetMap()->GetCreature(ObjectGuid(
            HIGHGUID_UNIT, (uint32)NPC_SCOUT_ENTRY, (uint32)SCOUT_GUID)))
    {
        s->Respawn();
        s->SetVisibility(VISIBILITY_ON);
    }

    // Run-forward zealots
    if (Creature* z = m_creature->GetMap()->GetCreature(ObjectGuid(
            HIGHGUID_UNIT, (uint32)NPC_ZEALOT_ENTRY, (uint32)ZEALOT_LEFT_ONE)))
        DoZealotRespawn(z);
    if (Creature* z = m_creature->GetMap()->GetCreature(ObjectGuid(
            HIGHGUID_UNIT, (uint32)NPC_ZEALOT_ENTRY, (uint32)ZEALOT_LEFT_TWO)))
        DoZealotRespawn(z);
    if (Creature* z =
            m_creature->GetMap()->GetCreature(ObjectGuid(HIGHGUID_UNIT,
                (uint32)NPC_ZEALOT_ENTRY, (uint32)ZEALOT_LEFT_THREE)))
        DoZealotRespawn(z);
    if (Creature* z = m_creature->GetMap()->GetCreature(ObjectGuid(
            HIGHGUID_UNIT, (uint32)NPC_ZEALOT_ENTRY, (uint32)ZEALOT_LEFT_FOUR)))
        DoZealotRespawn(z);
    if (Creature* z = m_creature->GetMap()->GetCreature(ObjectGuid(
            HIGHGUID_UNIT, (uint32)NPC_ZEALOT_ENTRY, (uint32)ZEALOT_RIGHT_ONE)))
        DoZealotRespawn(z);
    if (Creature* z = m_creature->GetMap()->GetCreature(ObjectGuid(
            HIGHGUID_UNIT, (uint32)NPC_ZEALOT_ENTRY, (uint32)ZEALOT_RIGHT_TWO)))
        DoZealotRespawn(z);
    if (Creature* z =
            m_creature->GetMap()->GetCreature(ObjectGuid(HIGHGUID_UNIT,
                (uint32)NPC_ZEALOT_ENTRY, (uint32)ZEALOT_RIGHT_THREE)))
        DoZealotRespawn(z);
    if (Creature* z =
            m_creature->GetMap()->GetCreature(ObjectGuid(HIGHGUID_UNIT,
                (uint32)NPC_ZEALOT_ENTRY, (uint32)ZEALOT_RIGHT_FOUR)))
        DoZealotRespawn(z);
    m_uiDelayedRespawnTimer = 2500;

    // Archers
    if (Creature* a = m_creature->GetMap()->GetCreature(ObjectGuid(
            HIGHGUID_UNIT, (uint32)NPC_ARCHER_ENTRY, (uint32)ARCHER_LEFT_GUID)))
        a->Respawn();
    if (Creature* a =
            m_creature->GetMap()->GetCreature(ObjectGuid(HIGHGUID_UNIT,
                (uint32)NPC_ARCHER_ENTRY, (uint32)ARCHER_RIGHT_GUID)))
        a->Respawn();

    // The boss himself
    if (Creature* bg = GetBloodGuard())
        bg->Respawn();

    // Zealots in front of boss and archer
    if (Creature* z = m_creature->GetMap()->GetCreature(ObjectGuid(
            HIGHGUID_UNIT, (uint32)NPC_ZEALOT_ENTRY, (uint32)ZEALOT_BG_ONE)))
        z->Respawn();
    if (Creature* z = m_creature->GetMap()->GetCreature(ObjectGuid(
            HIGHGUID_UNIT, (uint32)NPC_ZEALOT_ENTRY, (uint32)ZEALOT_BG_TWO)))
        z->Respawn();
    if (Creature* z = m_creature->GetMap()->GetCreature(ObjectGuid(
            HIGHGUID_UNIT, (uint32)NPC_ZEALOT_ENTRY, (uint32)ZEALOT_BG_THREE)))
        z->Respawn();
    if (Creature* z = m_creature->GetMap()->GetCreature(ObjectGuid(
            HIGHGUID_UNIT, (uint32)NPC_ZEALOT_ENTRY, (uint32)ZEALOT_BG_FOUR)))
        z->Respawn();

    // Spawned zealots
    for (auto& elem : m_spawnedZealots)
        if (Creature* c = m_creature->GetMap()->GetCreature(elem))
            c->ForcedDespawn();
    m_spawnedZealots.clear();
}

void trigger_gauntlet_of_flame::OnBloodGuardEnterCombat(Creature* bloodGuard)
{
    // Make every combat unit part of the event that's still alive attack the
    // bloodguard's target (we shouldn't be able to skip mobs)
    if (Creature* z = m_creature->GetMap()->GetCreature(ObjectGuid(
            HIGHGUID_UNIT, (uint32)NPC_ZEALOT_ENTRY, (uint32)ZEALOT_LEFT_ONE)))
        if (z->isAlive() && z->AI())
            z->AI()->AttackStart(bloodGuard->getVictim());
    if (Creature* z = m_creature->GetMap()->GetCreature(ObjectGuid(
            HIGHGUID_UNIT, (uint32)NPC_ZEALOT_ENTRY, (uint32)ZEALOT_LEFT_TWO)))
        if (z->isAlive() && z->AI())
            z->AI()->AttackStart(bloodGuard->getVictim());
    if (Creature* z =
            m_creature->GetMap()->GetCreature(ObjectGuid(HIGHGUID_UNIT,
                (uint32)NPC_ZEALOT_ENTRY, (uint32)ZEALOT_LEFT_THREE)))
        if (z->isAlive() && z->AI())
            z->AI()->AttackStart(bloodGuard->getVictim());
    if (Creature* z = m_creature->GetMap()->GetCreature(ObjectGuid(
            HIGHGUID_UNIT, (uint32)NPC_ZEALOT_ENTRY, (uint32)ZEALOT_LEFT_FOUR)))
        if (z->isAlive() && z->AI())
            z->AI()->AttackStart(bloodGuard->getVictim());
    if (Creature* z = m_creature->GetMap()->GetCreature(ObjectGuid(
            HIGHGUID_UNIT, (uint32)NPC_ZEALOT_ENTRY, (uint32)ZEALOT_RIGHT_ONE)))
        if (z->isAlive() && z->AI())
            z->AI()->AttackStart(bloodGuard->getVictim());
    if (Creature* z = m_creature->GetMap()->GetCreature(ObjectGuid(
            HIGHGUID_UNIT, (uint32)NPC_ZEALOT_ENTRY, (uint32)ZEALOT_RIGHT_TWO)))
        if (z->isAlive() && z->AI())
            z->AI()->AttackStart(bloodGuard->getVictim());
    if (Creature* z =
            m_creature->GetMap()->GetCreature(ObjectGuid(HIGHGUID_UNIT,
                (uint32)NPC_ZEALOT_ENTRY, (uint32)ZEALOT_RIGHT_THREE)))
        if (z->isAlive() && z->AI())
            z->AI()->AttackStart(bloodGuard->getVictim());
    if (Creature* z =
            m_creature->GetMap()->GetCreature(ObjectGuid(HIGHGUID_UNIT,
                (uint32)NPC_ZEALOT_ENTRY, (uint32)ZEALOT_RIGHT_FOUR)))
        if (z->isAlive() && z->AI())
            z->AI()->AttackStart(bloodGuard->getVictim());
    // Spawned zealots
    for (auto& elem : m_spawnedZealots)
        if (Creature* c = m_creature->GetMap()->GetCreature(elem))
            if (c->isAlive() && c->AI())
                c->AI()->AttackStart(bloodGuard->getVictim());
    // Zealots at start
    if (Creature* z = m_creature->GetMap()->GetCreature(ObjectGuid(
            HIGHGUID_UNIT, (uint32)NPC_ZEALOT_ENTRY, (uint32)ZEALOT_START_ONE)))
        if (z->isAlive() && z->AI())
            z->AI()->AttackStart(bloodGuard->getVictim());
    if (Creature* z = m_creature->GetMap()->GetCreature(ObjectGuid(
            HIGHGUID_UNIT, (uint32)NPC_ZEALOT_ENTRY, (uint32)ZEALOT_START_TWO)))
        if (z->isAlive() && z->AI())
            z->AI()->AttackStart(bloodGuard->getVictim());
    if (Creature* z =
            m_creature->GetMap()->GetCreature(ObjectGuid(HIGHGUID_UNIT,
                (uint32)NPC_ZEALOT_ENTRY, (uint32)ZEALOT_START_THREE)))
        if (z->isAlive() && z->AI())
            z->AI()->AttackStart(bloodGuard->getVictim());

    // Zealots in front of boss and archer
    if (Creature* z = m_creature->GetMap()->GetCreature(ObjectGuid(
            HIGHGUID_UNIT, (uint32)NPC_ZEALOT_ENTRY, (uint32)ZEALOT_BG_ONE)))
        if (z->isAlive() && z->AI())
            z->AI()->AttackStart(bloodGuard->getVictim());
    if (Creature* z = m_creature->GetMap()->GetCreature(ObjectGuid(
            HIGHGUID_UNIT, (uint32)NPC_ZEALOT_ENTRY, (uint32)ZEALOT_BG_TWO)))
        if (z->isAlive() && z->AI())
            z->AI()->AttackStart(bloodGuard->getVictim());
    if (Creature* z = m_creature->GetMap()->GetCreature(ObjectGuid(
            HIGHGUID_UNIT, (uint32)NPC_ZEALOT_ENTRY, (uint32)ZEALOT_BG_THREE)))
        if (z->isAlive() && z->AI())
            z->AI()->AttackStart(bloodGuard->getVictim());
    if (Creature* z = m_creature->GetMap()->GetCreature(ObjectGuid(
            HIGHGUID_UNIT, (uint32)NPC_ZEALOT_ENTRY, (uint32)ZEALOT_BG_FOUR)))
        if (z->isAlive() && z->AI())
            z->AI()->AttackStart(bloodGuard->getVictim());
}

CreatureAI* GetAI_trigger_gauntlet_of_flame(Creature* pCreature)
{
    return new trigger_gauntlet_of_flame(pCreature);
}

struct MANGOS_DLL_DECL npc_gauntlet_flame_target : public ScriptedAI
{
    npc_gauntlet_flame_target(Creature* pCreature) : ScriptedAI(pCreature) {}

    void Reset() override { SetCombatMovement(false); }

    void MoveInLineOfSight(Unit* /*pWho*/) override {}
    void AttackStart(Unit* /*pWho*/) override {}
};

CreatureAI* GetAI_npc_gauntlet_flame_target(Creature* pCreature)
{
    return new npc_gauntlet_flame_target(pCreature);
}

void AddSC_event_gauntlet_of_flame()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "trigger_gauntlet_of_flame";
    pNewScript->GetAI = &GetAI_trigger_gauntlet_of_flame;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "npc_gauntlet_flame_target";
    pNewScript->GetAI = &GetAI_npc_gauntlet_flame_target;
    pNewScript->RegisterSelf();
}
