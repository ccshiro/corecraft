/* Copyright (C) 2006 - 2012 ScriptDev2 <http://www.scriptdev2.com/>
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
SDName: Instance_Dark_Portal
SD%Complete: 100
SDComment:
SDCategory: Caverns of Time, The Dark Portal
EndScriptData */

#include "dark_portal.h"
#include "precompiled.h"

#define PORTAL_SIZE 4
static float portalSpawns[PORTAL_SIZE][4] = {
    {-2013.8f, 7017.6f, 22.3f, 2.4f}, {-1879.3f, 7102.8f, 23.0f, 2.7f},
    {-1930.6f, 7184.9f, 23.0f, 4.0f}, {-1960.7f, 7030.8f, 21.9f, 1.9f},
};

instance_dark_portal::instance_dark_portal(Map* pMap) : ScriptedInstance(pMap)
{
    Initialize();
}

void instance_dark_portal::Initialize()
{
    memset(&m_auiEncounter, 0, sizeof(m_auiEncounter));

    m_saatSpeakCooldown = 0;
    ResetEvent();
}

void instance_dark_portal::ResetEvent()
{
    if (GetData(TYPE_MEDIVH) == DONE)
        return;

    m_uiNextPortalTimer = 0;

    m_availableSpawnPositions =
        std::vector<float*>(portalSpawns, portalSpawns + PORTAL_SIZE);

    for (auto& elem : m_activePortals)
    {
        if (Creature* npc = instance->GetCreature(elem))
            npc->Kill(npc, false);
    }
    m_activePortals.clear();

    m_eventStarted = 0;
    SetData(TYPE_SHIELD_PCT, 100);
    SetData(TYPE_RIFT_NUMBER, 0);
    SetData(TYPE_MEDIVH, NOT_STARTED);

    DoUpdateWorldState(WORLD_STATE_EVENT_IN_PROGRESS, m_eventStarted);
    DoUpdateWorldState(WORLD_STATE_SHIELD_PCT, GetData(TYPE_SHIELD_PCT));
    DoUpdateWorldState(WORLD_STATE_RIFT_NR, GetData(TYPE_RIFT_NUMBER));

    m_createFollows.clear();

    m_uiPhase = 0;
    m_uiNextPhaseTimer = 0;

    for (auto& elem : m_cleanupMobs)
        if (Creature* c = instance->GetCreature(elem))
            c->ForcedDespawn();
    m_cleanupMobs.clear();
}

void instance_dark_portal::OnPlayerEnter(Player* pPlayer)
{
    // Send player the world state
    pPlayer->SendUpdateWorldState(
        WORLD_STATE_EVENT_IN_PROGRESS, m_eventStarted);
    pPlayer->SendUpdateWorldState(
        WORLD_STATE_SHIELD_PCT, GetData(TYPE_SHIELD_PCT));
    pPlayer->SendUpdateWorldState(
        WORLD_STATE_RIFT_NR, GetData(TYPE_RIFT_NUMBER));
}

void instance_dark_portal::HandleAreaTrigger(uint32 triggerId)
{
    // Other area triggers in this map:
    // 1626: Not part of actual map, random location, very off
    // 4288: Medivh (box values not added, radius is 20 yards, we solve it
    // ourselves in medivh's script)
    // 4322: Portal exit
    // 4485: Saat

    if (triggerId == AREATRIGGER_SAAT)
    {
        if (time(NULL) > m_saatSpeakCooldown)
        {
            if (Creature* saat = GetSingleCreatureFromStorage(NPC_SAAT))
            {
                DoScriptText(SAY_SAAT_WELCOME, saat);
                m_saatSpeakCooldown = time(NULL) + 300;
            }
        }
    }
}

void instance_dark_portal::StartEvent()
{
    if (GetData(TYPE_MEDIVH) == IN_PROGRESS || GetData(TYPE_MEDIVH) == DONE)
        return;

    Creature* medivh = GetSingleCreatureFromStorage(NPC_MEDIVH);
    if (!medivh)
        return;

    DoScriptText(SAY_MEDIVH_START_EVENT, medivh);

    m_eventStarted = 1;
    SetData(TYPE_SHIELD_PCT, 100);
    SetData(TYPE_RIFT_NUMBER, 0);
    DoUpdateWorldState(WORLD_STATE_EVENT_IN_PROGRESS, m_eventStarted);
    DoUpdateWorldState(WORLD_STATE_SHIELD_PCT, GetData(TYPE_SHIELD_PCT));
    DoUpdateWorldState(WORLD_STATE_RIFT_NR, GetData(TYPE_RIFT_NUMBER));

    SetData(TYPE_MEDIVH, IN_PROGRESS);
    m_uiNextPortalTimer = 15000;

    m_uiPhase = 0;
    m_uiNextPhaseTimer = 0;

    // Spinning crystals are summoned by medivh
}

void instance_dark_portal::OnCreatureCreate(Creature* pCreature)
{
    if (IsAttackMedivhCreature(pCreature->GetEntry()))
        m_createFollows.push_back(pCreature->GetObjectGuid());

    switch (pCreature->GetEntry())
    {
    case NPC_MEDIVH:
    case NPC_SAAT:
    case NPC_DARK_PORTAL_DUMMY:
        m_mNpcEntryGuidStore[pCreature->GetEntry()] =
            pCreature->GetObjectGuid();
        break;

    case NPC_COUNCIL_ENFORCER:
    {
        if (m_uiOrcPhase < MAX_ORC_WAVES)
            m_orcWaves[m_uiOrcPhase].push_back(pCreature->GetObjectGuid());
        break;
    }
    }
}

void instance_dark_portal::SetData(uint32 uiType, uint32 uiData)
{
    if (uiType < MAX_ENCOUNTER)
        m_auiEncounter[uiType] = uiData;

    if (uiData == DONE)
    {
        OUT_SAVE_INST_DATA;

        std::ostringstream saveStream;
        saveStream << m_auiEncounter[0] << " " << m_auiEncounter[1] << " "
                   << m_auiEncounter[2] << " " << m_auiEncounter[3] << " "
                   << m_auiEncounter[4] << " " << m_auiEncounter[5];

        m_strInstData = saveStream.str();

        SaveToDB();
        OUT_SAVE_INST_DATA_COMPLETE;
    }
}

uint32 instance_dark_portal::GetData(uint32 uiType)
{
    if (uiType < MAX_ENCOUNTER)
        return m_auiEncounter[uiType];

    return 0;
}

void instance_dark_portal::Load(const char* chrIn)
{
    if (!chrIn)
    {
        OUT_LOAD_INST_DATA_FAIL;
        return;
    }

    OUT_LOAD_INST_DATA(chrIn);

    std::istringstream loadStream(chrIn);
    loadStream >> m_auiEncounter[0] >> m_auiEncounter[1] >> m_auiEncounter[2] >>
        m_auiEncounter[3] >> m_auiEncounter[4] >> m_auiEncounter[5];

    for (auto& elem : m_auiEncounter)
    {
        if (elem == IN_PROGRESS)
            elem = NOT_STARTED;
    }

    OUT_LOAD_INST_DATA_COMPLETE;
}

bool instance_dark_portal::IsAttackMedivhCreature(uint32 entry) const
{
    switch (entry)
    {
    case NPC_AEONUS:
    case NPC_ASSASSIN_1:
    case NPC_ASSASSIN_2:
    case NPC_WHELP:
    case NPC_CHRONOMANCER_1:
    case NPC_CHRONOMANCER_2:
    case NPC_EXECUTIONER_1:
    case NPC_EXECUTIONER_2:
    case NPC_VANQUISHER_1:
    case NPC_VANQUISHER_2:
        return true;
    default:
        break;
    }

    return false;
}

void instance_dark_portal::OnEventCompleted()
{
    SetData(TYPE_MEDIVH, DONE);
    m_uiPhase = 0;
    m_uiNextPhaseTimer = 4000;
}

void instance_dark_portal::OnPortalClear(ObjectGuid portal,
    std::vector<ObjectGuid> remainingSummons, bool aeonusPortal)
{
    Creature* rift = instance->GetCreature(portal);
    if (!rift)
        return;

    if (GetData(TYPE_RIFT_NUMBER) == 18 && aeonusPortal)
    {
        OnEventCompleted();
        return;
    }

    // Get position that corresponds to this portal
    uint32 i = 0;
    for (; i < PORTAL_SIZE; ++i)
        if (rift->GetDistance2d(portalSpawns[i][0], portalSpawns[i][1]) < 5.0f)
            break;

    // Remove portal from the active list
    std::vector<ObjectGuid>::iterator find =
        std::find(m_activePortals.begin(), m_activePortals.end(), portal);
    if (find != m_activePortals.end())
        m_activePortals.erase(find);

    // Send a new wave if no other portal remains
    if (m_activePortals.empty())
    {
        if (!IsBossWave())
            SpawnNextPortal();
        else
        {
            // If it's a boss we just reduce the portal timer, we don't send a
            // new one right away
            if (m_uiNextPortalTimer > 120 * 1000)
                m_uiNextPortalTimer = 120 * 1000;
        }
    }

    // Add the rift position back in to available positions (do this after we
    // spawn a new rift, to not get the same location again)
    m_availableSpawnPositions.push_back(portalSpawns[i]);

    m_cleanupMobs.insert(
        m_cleanupMobs.end(), remainingSummons.begin(), remainingSummons.end());
}

bool instance_dark_portal::IsBossWave()
{
    return GetData(TYPE_RIFT_NUMBER) == 6 || GetData(TYPE_RIFT_NUMBER) == 12 ||
           GetData(TYPE_RIFT_NUMBER) == 18;
}

void instance_dark_portal::SpawnNextPortal()
{
    // We cannot advance event if all locations taken
    if (m_availableSpawnPositions.empty())
        return;

    // Spawn and remove location (it will get added back when this portal is
    // cleared out)
    if (Creature* medivh = GetSingleCreatureFromStorage(NPC_MEDIVH))
    {
        uint32 location = urand(0, m_availableSpawnPositions.size() - 1);
        Creature* portal = medivh->SummonCreature(NPC_TIME_RIFT,
            m_availableSpawnPositions[location][0],
            m_availableSpawnPositions[location][1],
            m_availableSpawnPositions[location][2],
            m_availableSpawnPositions[location][3], TEMPSUMMON_MANUAL_DESPAWN,
            0);
        if (!portal)
            return;
        m_availableSpawnPositions.erase(
            m_availableSpawnPositions.begin() + location);
        // Portal registers itself, in other words we do not need to insert it
        // into active portals
    }

    // Update rift count
    SetData(TYPE_RIFT_NUMBER, GetData(TYPE_RIFT_NUMBER) + 1);
    DoUpdateWorldState(WORLD_STATE_RIFT_NR, GetData(TYPE_RIFT_NUMBER));

    // Portal itself will spawn the mobs, we only set the time-out timer to the
    // next portal
    if (GetData(TYPE_RIFT_NUMBER) == 18)
        m_uiNextPortalTimer = 0;
    else if (IsBossWave())
        m_uiNextPortalTimer =
            GetData(TYPE_RIFT_NUMBER) == 6 ? 240 * 1000 : 220 * 1000;
    else
    {
        // Time between portals get reduced with each boss encounter
        if (GetData(TYPE_RIFT_NUMBER) <= 6)
            m_uiNextPortalTimer = 120 * 1000;
        else if (GetData(TYPE_RIFT_NUMBER) <= 12)
            m_uiNextPortalTimer = 100 * 1000;
        else
            m_uiNextPortalTimer = 80 * 1000;
    }
}

void instance_dark_portal::Update(uint32 uiDiff)
{
    if (m_uiNextPortalTimer)
    {
        if (m_uiNextPortalTimer <= uiDiff)
        {
            m_uiNextPortalTimer = 0; // SpawnNextPortal() sets timer
            SpawnNextPortal();
        }
        else
            m_uiNextPortalTimer -= uiDiff;
    }

    for (auto& guid : m_createFollows)
    {
        Creature* medivh = GetSingleCreatureFromStorage(NPC_MEDIVH);
        Creature* follower = instance->GetCreature(guid);
        if (follower && follower->isAlive() && medivh && medivh->isAlive())
            follower->movement_gens.push(
                new movement::PointMovementGenerator(0, medivh->GetX(),
                    medivh->GetY(), medivh->GetZ(), true, false),
                0, 30);
    }
    m_createFollows.clear();

    if (m_uiNextPhaseTimer)
    {
        if (m_uiNextPhaseTimer <= uiDiff)
        {
            Creature* medivh = GetSingleCreatureFromStorage(NPC_MEDIVH);
            if (!medivh)
            {
                m_uiNextPhaseTimer = 0;
                return;
            }

            switch (m_uiPhase)
            {
            case 0:
                DoScriptText(SAY_MEDIVH_WIN, medivh);
                medivh->InterruptNonMeleeSpells(false);
                medivh->remove_auras();
                medivh->SummonGameObject(GO_PINK_PORTAL, -2084.9f, 7125.2f,
                    29.0f, 6.17f, 0, 0, 0, 0, 0);
                if (medivh->AI())
                    medivh->AI()->Reset(); // Reset removes the dark crystals
                m_uiNextPhaseTimer = 4000;
                m_uiOrcPhase = 0;

                // Complete quest for everyone
                for (const auto& elem : instance->GetPlayers())
                {
                    Player* plr = elem.getSource();
                    if (plr->GetQuestStatus(QUEST_OPENING_OF_THE_DARK_PORTAL) ==
                        QUEST_STATUS_INCOMPLETE)
                        plr->AreaExploredOrEventHappens(
                            QUEST_OPENING_OF_THE_DARK_PORTAL);
                    if (plr->GetQuestStatus(QUEST_MASTERS_TOUCH) ==
                        QUEST_STATUS_INCOMPLETE)
                        plr->AreaExploredOrEventHappens(QUEST_MASTERS_TOUCH);
                }
                break;
            case 1:
                SpawnOrcs(medivh);
                m_uiNextPhaseTimer = 4000;
                // Keep repeating phase for each orc wave
                --m_uiPhase;
                ++m_uiOrcPhase;
                if (m_uiOrcPhase == 5)
                {
                    ++m_uiPhase;
                    m_uiNextPhaseTimer = 10000;
                }
                break;
            case 2:
                DoScriptText(SAY_ORCS_ENTER, medivh);
                m_uiNextPhaseTimer = 7000;
                break;
            case 3:
                if (Creature* spokesPerson =
                        instance->GetCreature(m_orcSpokesPerson))
                {
                    spokesPerson->movement_gens.remove_all(movement::gen::idle);
                    spokesPerson->SetFacingTo(3.0f);
                    DoScriptText(SAY_ORCS_ANSWER, spokesPerson);
                }
                m_uiNextPhaseTimer = 7500;
                m_uiOrcPhase = 0;
                break;
            case 4:
                RetreatOrcs();
                m_uiNextPhaseTimer = 2000;
                // Keep repeating phase for each orc wave
                --m_uiPhase;
                ++m_uiOrcPhase;
                if (m_uiOrcPhase == 5)
                {
                    ++m_uiPhase;
                    m_uiNextPhaseTimer = 4000;
                }
                break;
            case 5:
                // Enable gossiping with medivh, to complete Master's Touch
                medivh->SetFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_QUESTGIVER);
                break;
            default:
                m_uiNextPhaseTimer = 0;
                break;
            }
            ++m_uiPhase;
        }
        else
            m_uiNextPhaseTimer -= uiDiff;
    }
}

void instance_dark_portal::MoveOrc(Creature* orc, float x, float y, float z)
{
    // Idle & stopped generator during event
    orc->movement_gens.push(
        new movement::IdleMovementGenerator(x, y, z, 6.2f), 0, 30);
    orc->movement_gens.push(new movement::StoppedMovementGenerator(), 0, 29);
    // Leaving event
    orc->movement_gens.push(
        new movement::PointMovementGenerator(
            100, orc->GetX(), orc->GetY(), orc->GetZ(), true, false),
        0, 20);
    // Going to event
    orc->movement_gens.push(
        new movement::PointMovementGenerator(0, x, y, z, true, false), 0, 40);
}

void instance_dark_portal::SpawnOrcs(Creature* s)
{
    switch (m_uiOrcPhase)
    {
    case 0:
        if (Creature* o = s->SummonCreature(NPC_COUNCIL_ENFORCER, -2089.0f,
                7125.8f, 34.6f, 6.3f, TEMPSUMMON_MANUAL_DESPAWN, 0))
        {
            MoveOrc(o, -2040.2f, 7122.7f, 23.0f);
            m_orcSpokesPerson = o->GetObjectGuid();
        }
        break;
    case 1:
        if (Creature* o = s->SummonCreature(NPC_COUNCIL_ENFORCER, -2091.3f,
                7118.9f, 34.6f, 6.3f, TEMPSUMMON_MANUAL_DESPAWN, 0))
            MoveOrc(o, -2046.7f, 7113.3f, 24.2f);
        if (Creature* o = s->SummonCreature(NPC_COUNCIL_ENFORCER, -2090.5f,
                7122.3f, 34.6f, 6.3f, TEMPSUMMON_MANUAL_DESPAWN, 0))
            MoveOrc(o, -2046.4f, 7117.0f, 24.2f);
        if (Creature* o = s->SummonCreature(NPC_COUNCIL_ENFORCER, -2090.5f,
                7125.8f, 34.6f, 6.3f, TEMPSUMMON_MANUAL_DESPAWN, 0))
            MoveOrc(o, -2046.2f, 7120.9f, 24.0f);
        if (Creature* o = s->SummonCreature(NPC_COUNCIL_ENFORCER, -2089.7f,
                7129.0f, 34.6f, 6.3f, TEMPSUMMON_MANUAL_DESPAWN, 0))
            MoveOrc(o, -2046.0f, 7124.6f, 24.4);
        if (Creature* o = s->SummonCreature(NPC_COUNCIL_ENFORCER, -2089.7f,
                7132.0f, 34.6f, 6.3f, TEMPSUMMON_MANUAL_DESPAWN, 0))
            MoveOrc(o, -2046.2f, 7128.8f, 24.9f);
        break;
    case 2:
        if (Creature* o = s->SummonCreature(NPC_COUNCIL_ENFORCER, -2091.3f,
                7118.9f, 34.6f, 6.3f, TEMPSUMMON_MANUAL_DESPAWN, 0))
            MoveOrc(o, -2053.9f, 7113.8f, 26.9f);
        if (Creature* o = s->SummonCreature(NPC_COUNCIL_ENFORCER, -2090.5f,
                7122.3f, 34.6f, 6.3f, TEMPSUMMON_MANUAL_DESPAWN, 0))
            MoveOrc(o, -2053.5f, 7117.7f, 26.8f);
        if (Creature* o = s->SummonCreature(NPC_COUNCIL_ENFORCER, -2090.5f,
                7125.8f, 34.6f, 6.3f, TEMPSUMMON_MANUAL_DESPAWN, 0))
            MoveOrc(o, -2053.2f, 7121.1f, 26.9f);
        if (Creature* o = s->SummonCreature(NPC_COUNCIL_ENFORCER, -2089.7f,
                7129.0f, 34.6f, 6.3f, TEMPSUMMON_MANUAL_DESPAWN, 0))
            MoveOrc(o, -2052.9f, 7125.3f, 27.2f);
        if (Creature* o = s->SummonCreature(NPC_COUNCIL_ENFORCER, -2089.7f,
                7132.0f, 34.6f, 6.3f, TEMPSUMMON_MANUAL_DESPAWN, 0))
            MoveOrc(o, -2052.6f, 7128.7f, 27.7f);
        break;
    case 3:
        if (Creature* o = s->SummonCreature(NPC_COUNCIL_ENFORCER, -2091.3f,
                7118.9f, 34.6f, 6.3f, TEMPSUMMON_MANUAL_DESPAWN, 0))
            MoveOrc(o, -2060.9f, 7114.4f, 29.2f);
        if (Creature* o = s->SummonCreature(NPC_COUNCIL_ENFORCER, -2090.5f,
                7122.3f, 34.6f, 6.3f, TEMPSUMMON_MANUAL_DESPAWN, 0))
            MoveOrc(o, -2060.7f, 7118.1f, 29.2f);
        if (Creature* o = s->SummonCreature(NPC_COUNCIL_ENFORCER, -2090.5f,
                7125.8f, 34.6f, 6.3f, TEMPSUMMON_MANUAL_DESPAWN, 0))
            MoveOrc(o, -2060.4f, 7121.6f, 29.3f);
        if (Creature* o = s->SummonCreature(NPC_COUNCIL_ENFORCER, -2089.7f,
                7129.0f, 34.6f, 6.3f, TEMPSUMMON_MANUAL_DESPAWN, 0))
            MoveOrc(o, -2059.8f, 7125.7f, 29.5f);
        if (Creature* o = s->SummonCreature(NPC_COUNCIL_ENFORCER, -2089.7f,
                7132.0f, 34.6f, 6.3f, TEMPSUMMON_MANUAL_DESPAWN, 0))
            MoveOrc(o, -2059.5f, 7128.5f, 29.9f);
        break;
    case 4:
        if (Creature* o = s->SummonCreature(NPC_COUNCIL_ENFORCER, -2091.3f,
                7118.9f, 34.6f, 6.3f, TEMPSUMMON_MANUAL_DESPAWN, 0))
            MoveOrc(o, -2066.7f, 7114.9f, 30.4f);
        if (Creature* o = s->SummonCreature(NPC_COUNCIL_ENFORCER, -2090.5f,
                7122.3f, 34.6f, 6.3f, TEMPSUMMON_MANUAL_DESPAWN, 0))
            MoveOrc(o, -2066.2f, 7118.7f, 30.4f);
        if (Creature* o = s->SummonCreature(NPC_COUNCIL_ENFORCER, -2090.5f,
                7125.8f, 34.6f, 6.3f, TEMPSUMMON_MANUAL_DESPAWN, 0))
            MoveOrc(o, -2065.6f, 7122.9f, 30.3f);
        if (Creature* o = s->SummonCreature(NPC_COUNCIL_ENFORCER, -2089.7f,
                7129.0f, 34.6f, 6.3f, TEMPSUMMON_MANUAL_DESPAWN, 0))
            MoveOrc(o, -2065.1f, 7126.3f, 30.4f);
        if (Creature* o = s->SummonCreature(NPC_COUNCIL_ENFORCER, -2089.7f,
                7132.0f, 34.6f, 6.3f, TEMPSUMMON_MANUAL_DESPAWN, 0))
            MoveOrc(o, -2064.7f, 7129.4f, 30.3f);
        break;
    default:
        break;
    }
}

void instance_dark_portal::RetreatOrcs()
{
    if (m_uiOrcPhase >= MAX_ORC_WAVES)
        return;

    // Extract waves in reverse order
    for (auto& elem : m_orcWaves[(MAX_ORC_WAVES - 1) - m_uiOrcPhase])
        if (Creature* orc = instance->GetCreature(elem))
        {
            orc->movement_gens.remove_all(movement::gen::stopped);
            orc->movement_gens.remove_all(movement::gen::idle);
        }
}

InstanceData* GetInstanceData_instance_dark_portal(Map* pMap)
{
    return new instance_dark_portal(pMap);
}

bool AreaTrigger_dark_portal(Player* pPlayer, AreaTriggerEntry const* pAt)
{
    if (pPlayer->isGameMaster())
        return false;

    if (InstanceData* id = pPlayer->GetMap()->GetInstanceData())
    {
        if (instance_dark_portal* darkPortalInstance =
                dynamic_cast<instance_dark_portal*>(id))
            darkPortalInstance->HandleAreaTrigger(pAt->id);
    }

    return false;
}

void AddSC_instance_dark_portal()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "instance_dark_portal";
    pNewScript->GetInstanceData = &GetInstanceData_instance_dark_portal;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "at_dark_portal";
    pNewScript->pAreaTrigger = &AreaTrigger_dark_portal;
    pNewScript->RegisterSelf();
}
