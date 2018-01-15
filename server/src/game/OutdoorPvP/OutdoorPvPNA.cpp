/*
 * This file is part of the CMaNGOS Project. See AUTHORS file for Copyright
 *information
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include "OutdoorPvPNA.h"
#include "Creature.h"
#include "GameObject.h"
#include "Object.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "World.h"
#include "WorldPacket.h"

OutdoorPvPNA::OutdoorPvPNA()
  : OutdoorPvP(), m_zoneOwner(TEAM_NONE), m_zoneWorldState(0),
    m_zoneMapState(WORLD_STATE_NA_HALAA_NEUTRAL), m_guardsLeft(0),
    m_isUnderSiege(false)
{
    // initially set graveyard owner to neither faction
    sObjectMgr::Instance()->SetGraveYardLinkTeam(
        GRAVEYARD_ID_HALAA, GRAVEYARD_ZONE_ID_HALAA, TEAM_NONE);
}

void OutdoorPvPNA::FillInitialWorldStates(WorldPacket& data, uint32& count)
{
    if (m_zoneOwner != TEAM_NONE)
    {
        FillInitialWorldState(data, count, m_zoneWorldState, WORLD_STATE_ADD);

        // map states
        for (auto& elem : m_roostWorldState)
            FillInitialWorldState(data, count, elem, WORLD_STATE_ADD);
    }

    FillInitialWorldState(data, count, m_zoneMapState, WORLD_STATE_ADD);
    FillInitialWorldState(
        data, count, WORLD_STATE_NA_GUARDS_MAX, MAX_NA_GUARDS);
    FillInitialWorldState(
        data, count, WORLD_STATE_NA_GUARDS_LEFT, m_guardsLeft);
}

void OutdoorPvPNA::SendRemoveWorldStates(Player* player)
{
    player->SendUpdateWorldState(m_zoneWorldState, WORLD_STATE_REMOVE);
    player->SendUpdateWorldState(m_zoneMapState, WORLD_STATE_REMOVE);

    for (auto& elem : m_roostWorldState)
        player->SendUpdateWorldState(elem, WORLD_STATE_REMOVE);
}

void OutdoorPvPNA::HandlePlayerEnterZone(Player* player, bool isMainZone)
{
    OutdoorPvP::HandlePlayerEnterZone(player, isMainZone);

    // remove the buff from the player first because there are some issues at
    // relog
    player->remove_auras(SPELL_STRENGTH_HALAANI);

    // buff the player if same team is controlling the zone
    if (player->GetTeam() == m_zoneOwner)
        player->CastSpell(player, SPELL_STRENGTH_HALAANI, true);
}

void OutdoorPvPNA::HandlePlayerLeaveZone(Player* player, bool isMainZone)
{
    // remove the buff from the player
    player->remove_auras(SPELL_STRENGTH_HALAANI);

    OutdoorPvP::HandlePlayerLeaveZone(player, isMainZone);
}

void OutdoorPvPNA::HandleObjectiveComplete(
    uint32 eventId, std::list<Player*> players, Team team)
{
    if (eventId == EVENT_HALAA_BANNER_WIN_ALLIANCE ||
        eventId == EVENT_HALAA_BANNER_WIN_HORDE)
    {
        for (auto& player : players)
        {
            if ((player) && (player)->GetTeam() == team)
                (player)->KilledMonsterCredit(NPC_HALAA_COMBATANT);
        }
    }
}

// Cast player spell on opponent kill
void OutdoorPvPNA::HandlePlayerKillInsideArea(Player* player)
{
    if (player->GetAreaId() == 3628)
        player->CastSpell(player,
            player->GetTeam() == ALLIANCE ? SPELL_NAGRAND_TOKEN_ALLIANCE :
                                            SPELL_NAGRAND_TOKEN_HORDE,
            true);
}

void OutdoorPvPNA::HandleCreatureCreate(Creature* creature)
{
    uint32 entry = creature->GetGUIDLow();

    if (entry >= 1002755 && entry <= 1002794)
    {
        // Horde Guards
        if (entry <= 1002769)
        {
            Horde_CreatureGuids.push_back(creature->GetObjectGuid());
        }
        // Alliance Guards
        else if (entry <= 1002784)
        {
            Alliance_CreatureGuids.push_back(creature->GetObjectGuid());
        }
        // Horde Vendors
        else if (entry <= 1002789)
        {
            Horde_CreatureGuids.push_back(creature->GetObjectGuid());
        }
        // Alliance Vendors
        else
        {
            Alliance_CreatureGuids.push_back(creature->GetObjectGuid());
        }
        // Default dead/no corpse
        creature->SetDeathState(JUST_DIED);
        creature->RemoveCorpse();
        creature->SetVisibility(VISIBILITY_REMOVE_CORPSE);
        creature->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PASSIVE);
    }

    // Bomb spell Target npc
    if (entry == 18225)
    {
        creature->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE);
        creature->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PASSIVE);
        creature->SetVisibility(VISIBILITY_REMOVE_CORPSE);
    }
}

void OutdoorPvPNA::HandleCreatureRespawn(Creature* creature)
{
    switch (creature->GetEntry())
    {
    case NPC_HORDE_HALAANI_GUARD:
    case NPC_ALLIANCE_HANAANI_GUARD:
        // prevent updating guard counter on owner take over
        if (m_guardsLeft == MAX_NA_GUARDS)
            return;

        ++m_guardsLeft;
        SendUpdateWorldState(WORLD_STATE_NA_GUARDS_LEFT, m_guardsLeft);
        break;
    }
}

void OutdoorPvPNA::HandleCreatureDeath(Creature* creature)
{
    if (creature->GetEntry() != NPC_HORDE_HALAANI_GUARD &&
        creature->GetEntry() != NPC_ALLIANCE_HANAANI_GUARD)
        return;

    creature->SetRespawnTime(60 * MINUTE);
    creature->RemoveCorpse();

    // decrease the counter
    --m_guardsLeft;
    SendUpdateWorldState(WORLD_STATE_NA_GUARDS_LEFT, m_guardsLeft);

    if (m_guardsLeft == 0)
    {
        // set the zone under siege and increase the respawn timer
        m_isUnderSiege = true;

        Map* map = creature->GetMap();
        if (!map)
            return;

        for (auto guid : Alliance_CreatureGuids)
        {
            Creature* obj = map->GetCreature(guid);
            if (!obj)
                continue;

            obj->SetRespawnTime(HOUR);
        }

        for (auto guid : Horde_CreatureGuids)
        {
            Creature* obj = map->GetCreature(guid);
            if (!obj)
                continue;

            obj->SetRespawnTime(HOUR);
        }

        // make capturable
        UnlockHalaa(creature);

        // update world state
        SendUpdateWorldState(m_zoneMapState, WORLD_STATE_REMOVE);
        m_zoneMapState = m_zoneOwner == ALLIANCE ?
                             WORLD_STATE_NA_HALAA_NEUTRAL_A :
                             WORLD_STATE_NA_HALAA_NEUTRAL_H;
        SendUpdateWorldState(m_zoneMapState, WORLD_STATE_ADD);

        sWorld::Instance()->SendDefenseMessage(
            ZONE_ID_NAGRAND, LANG_OPVP_NA_DEFENSELESS);
    }
}

void OutdoorPvPNA::HandleGameObjectCreate(GameObject* go)
{
    OutdoorPvP::HandleGameObjectCreate(go);

    switch (go->GetEntry())
    {
    case GO_HALAA_BANNER:
        m_capturePoint = go->GetObjectGuid();
        go->SetGoArtKit(GetBannerArtKit(m_zoneOwner));
        break;

    case GO_WYVERN_ROOST_ALLIANCE_SOUTH:
        m_roostsAlliance[0] = go->GetObjectGuid();
        break;
    case GO_WYVERN_ROOST_ALLIANCE_NORTH:
        m_roostsAlliance[1] = go->GetObjectGuid();
        break;
    case GO_WYVERN_ROOST_ALLIANCE_EAST:
        m_roostsAlliance[2] = go->GetObjectGuid();
        break;
    case GO_WYVERN_ROOST_ALLIANCE_WEST:
        m_roostsAlliance[3] = go->GetObjectGuid();
        break;

    case GO_BOMB_WAGON_HORDE_SOUTH:
        m_wagonsHorde[0] = go->GetObjectGuid();
        break;
    case GO_BOMB_WAGON_HORDE_NORTH:
        m_wagonsHorde[1] = go->GetObjectGuid();
        break;
    case GO_BOMB_WAGON_HORDE_EAST:
        m_wagonsHorde[2] = go->GetObjectGuid();
        break;
    case GO_BOMB_WAGON_HORDE_WEST:
        m_wagonsHorde[3] = go->GetObjectGuid();
        break;

    case GO_DESTROYED_ROOST_ALLIANCE_SOUTH:
        m_roostsBrokenAlliance[0] = go->GetObjectGuid();
        break;
    case GO_DESTROYED_ROOST_ALLIANCE_NORTH:
        m_roostsBrokenAlliance[1] = go->GetObjectGuid();
        break;
    case GO_DESTROYED_ROOST_ALLIANCE_EAST:
        m_roostsBrokenAlliance[2] = go->GetObjectGuid();
        break;
    case GO_DESTROYED_ROOST_ALLIANCE_WEST:
        m_roostsBrokenAlliance[3] = go->GetObjectGuid();
        break;

    case GO_WYVERN_ROOST_HORDE_SOUTH:
        m_roostsHorde[0] = go->GetObjectGuid();
        break;
    case GO_WYVERN_ROOST_HORDE_NORTH:
        m_roostsHorde[1] = go->GetObjectGuid();
        break;
    case GO_WYVERN_ROOST_HORDE_EAST:
        m_roostsHorde[2] = go->GetObjectGuid();
        break;
    case GO_WYVERN_ROOST_HORDE_WEST:
        m_roostsHorde[3] = go->GetObjectGuid();
        break;

    case GO_BOMB_WAGON_ALLIANCE_SOUTH:
        m_wagonsAlliance[0] = go->GetObjectGuid();
        break;
    case GO_BOMB_WAGON_ALLIANCE_NORTH:
        m_wagonsAlliance[1] = go->GetObjectGuid();
        break;
    case GO_BOMB_WAGON_ALLIANCE_EAST:
        m_wagonsAlliance[2] = go->GetObjectGuid();
        break;
    case GO_BOMB_WAGON_ALLIANCE_WEST:
        m_wagonsAlliance[3] = go->GetObjectGuid();
        break;

    case GO_DESTROYED_ROOST_HORDE_SOUTH:
        m_roostsBrokenHorde[0] = go->GetObjectGuid();
        break;
    case GO_DESTROYED_ROOST_HORDE_NORTH:
        m_roostsBrokenHorde[1] = go->GetObjectGuid();
        break;
    case GO_DESTROYED_ROOST_HORDE_EAST:
        m_roostsBrokenHorde[2] = go->GetObjectGuid();
        break;
    case GO_DESTROYED_ROOST_HORDE_WEST:
        m_roostsBrokenHorde[3] = go->GetObjectGuid();
        break;
    }
}

void OutdoorPvPNA::UpdateWorldState(uint32 value)
{
    SendUpdateWorldState(m_zoneWorldState, value);
    SendUpdateWorldState(m_zoneMapState, value);

    UpdateWyvernsWorldState(value);
}

void OutdoorPvPNA::UpdateWyvernsWorldState(uint32 value)
{
    for (auto& elem : m_roostWorldState)
        SendUpdateWorldState(elem, value);
}

// process the capture events
bool OutdoorPvPNA::HandleEvent(uint32 eventId, GameObject* go)
{
    // If we are not using the Halaa banner return
    if (go->GetEntry() != GO_HALAA_BANNER)
        return false;

    bool eventHandled = true;

    switch (eventId)
    {
    case EVENT_HALAA_BANNER_WIN_ALLIANCE:
        ProcessCaptureEvent(go, ALLIANCE);
        eventHandled = false;
        break;
    case EVENT_HALAA_BANNER_WIN_HORDE:
        ProcessCaptureEvent(go, HORDE);
        eventHandled = false;
        break;
    case EVENT_HALAA_BANNER_PROGRESS_ALLIANCE:
        SetBannerVisual(go, CAPTURE_ARTKIT_ALLIANCE, CAPTURE_ANIM_ALLIANCE);
        sWorld::Instance()->SendDefenseMessage(
            ZONE_ID_NAGRAND, LANG_OPVP_NA_PROGRESS_A);
        break;
    case EVENT_HALAA_BANNER_PROGRESS_HORDE:
        SetBannerVisual(go, CAPTURE_ARTKIT_HORDE, CAPTURE_ANIM_HORDE);
        sWorld::Instance()->SendDefenseMessage(
            ZONE_ID_NAGRAND, LANG_OPVP_NA_PROGRESS_H);
        break;
    }

    // there are some events which required further DB script
    return eventHandled;
}

void OutdoorPvPNA::ProcessCaptureEvent(GameObject* go, Team team)
{
    BuffTeam(m_zoneOwner, SPELL_STRENGTH_HALAANI, true);
    // Delete Old spawns before creating new
    DeleteSpawnCreatures();
    // update capture point owner
    m_zoneOwner = team;

    SpawnNPCsForTeam(team, go);
    LockHalaa(go);
    m_guardsLeft = MAX_NA_GUARDS;

    m_isUnderSiege = false;

    UpdateWorldState(WORLD_STATE_REMOVE);

    sObjectMgr::Instance()->SetGraveYardLinkTeam(
        GRAVEYARD_ID_HALAA, GRAVEYARD_ZONE_ID_HALAA, m_zoneOwner);

    if (m_zoneOwner == ALLIANCE)
    {
        m_zoneWorldState = WORLD_STATE_NA_GUARDS_ALLIANCE;
        m_zoneMapState = WORLD_STATE_NA_HALAA_ALLIANCE;
    }
    else
    {
        m_zoneWorldState = WORLD_STATE_NA_GUARDS_HORDE;
        m_zoneMapState = WORLD_STATE_NA_HALAA_HORDE;
    }

    HandleFactionObjects(go);
    UpdateWorldState(WORLD_STATE_ADD);

    SendUpdateWorldState(WORLD_STATE_NA_GUARDS_LEFT, m_guardsLeft);

    BuffTeam(m_zoneOwner, SPELL_STRENGTH_HALAANI);
    sWorld::Instance()->SendDefenseMessage(
        ZONE_ID_NAGRAND, m_zoneOwner == ALLIANCE ? LANG_OPVP_NA_CAPTURE_A :
                                                   LANG_OPVP_NA_CAPTURE_H);
}

// Handle the gameobjects spawn/despawn depending on the controller faction
void OutdoorPvPNA::HandleFactionObjects(const WorldObject* objRef)
{
    if (m_zoneOwner == ALLIANCE)
    {
        for (uint8 i = 0; i < MAX_NA_ROOSTS; ++i)
        {
            RespawnGO(objRef, m_wagonsHorde[i], false);
            RespawnGO(objRef, m_roostsBrokenAlliance[i], false);
            RespawnGO(objRef, m_roostsAlliance[i], false);
            RespawnGO(objRef, m_wagonsAlliance[i], false);
            RespawnGO(objRef, m_roostsBrokenHorde[i], true);

            m_roostWorldState[i] = nagrandRoostStatesHordeNeutral[i];
        }
    }
    else
    {
        for (uint8 i = 0; i < MAX_NA_ROOSTS; ++i)
        {
            RespawnGO(objRef, m_wagonsAlliance[i], false);
            RespawnGO(objRef, m_roostsBrokenHorde[i], false);
            RespawnGO(objRef, m_roostsHorde[i], false);
            RespawnGO(objRef, m_wagonsHorde[i], false);
            RespawnGO(objRef, m_roostsBrokenAlliance[i], true);

            m_roostWorldState[i] = nagrandRoostStatesAllianceNeutral[i];
        }
    }
}

void OutdoorPvPNA::SpawnNPCsForTeam(Team team, GameObject* go)
{
    m_guardsLeft = 15;
    if (team == ALLIANCE)
    {
        Map* map = go->GetMap();
        if (!map)
            return;
        for (auto guid : Alliance_CreatureGuids)
        {
            Creature* obj = map->GetCreature(guid);
            if (!obj)
                continue;
            obj->SetDeadByDefault(false);
            obj->SetVisibility(VISIBILITY_ON);
            obj->SetDeathState(JUST_ALIVED);
            obj->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PASSIVE);
            obj->Respawn();
        }

        for (auto guid : Horde_CreatureGuids)
        {
            Creature* obj = map->GetCreature(guid);
            if (!obj)
                continue;
            obj->SetDeadByDefault(true);
            obj->SetVisibility(VISIBILITY_REMOVE_CORPSE);
            obj->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PASSIVE);
            obj->SetRespawnTime(std::numeric_limits<uint32>::max());
            obj->SetDeathState(JUST_DIED);
            obj->RemoveCorpse();
        }
    }
    else if (team == HORDE)
    {
        Map* map = go->GetMap();
        if (!map)
            return;
        for (auto guid : Horde_CreatureGuids)
        {
            Creature* obj = map->GetCreature(guid);
            if (!obj)
                continue;
            obj->SetDeadByDefault(false);
            obj->SetVisibility(VISIBILITY_ON);
            obj->SetDeathState(JUST_ALIVED);
            obj->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PASSIVE);
            obj->Respawn();
        }

        for (auto guid : Alliance_CreatureGuids)
        {
            Creature* obj = map->GetCreature(guid);
            if (!obj)
                continue;
            obj->SetDeadByDefault(true);
            obj->SetVisibility(VISIBILITY_REMOVE_CORPSE);
            obj->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PASSIVE);
            obj->SetDeathState(JUST_DIED);
            obj->SetRespawnTime(std::numeric_limits<uint32>::max());
            obj->RemoveCorpse();
        }
    }
}

bool OutdoorPvPNA::HandleGameObjectUse(Player* player, GameObject* go)
{
    if (player->GetTeam() == ALLIANCE)
    {
        for (uint8 i = 0; i < MAX_NA_ROOSTS; ++i)
        {
            if (go->GetEntry() == nagrandWagonsAlliance[i])
            {
                // update roost states
                UpdateWyvernsWorldState(WORLD_STATE_REMOVE);
                m_roostWorldState[i] = nagrandRoostStatesHordeNeutral[i];
                UpdateWyvernsWorldState(WORLD_STATE_ADD);

                // spawn the broken roost and despawn the other one
                RespawnGO(go, m_wagonsAlliance[i], false);
                RespawnGO(go, m_roostsHorde[i], false);
                RespawnGO(go, m_roostsBrokenHorde[i], true);

                // no need to iterate the other roosts
                return false;
            }
            else if (go->GetEntry() == nagrandRoostsBrokenAlliance[i])
            {
                // update roost states
                UpdateWyvernsWorldState(WORLD_STATE_REMOVE);
                m_roostWorldState[i] = nagrandRoostStatesAlliance[i];
                UpdateWyvernsWorldState(WORLD_STATE_ADD);

                // spawn the repaired one along with the explosive wagon
                RespawnGO(go, m_wagonsHorde[i], true);
                RespawnGO(go, m_roostsAlliance[i], true);
                RespawnGO(go, m_roostsBrokenAlliance[i], false);

                return false;
            }
            else if (go->GetEntry() == nagrandRoostsAlliance[i])
            {
                // Activate Flight path
                std::vector<uint32> nodes;
                nodes.resize(2);
                nodes[0] = FlightPathStartNodes[i];
                nodes[1] = FlightPathEndNodes[i];
                player->ActivateTaxiPathTo(nodes);

                // Add 10 bombs to the player
                inventory::transaction trans;
                trans.add(24538, 10);
                player->storage().finalize(trans);

                // mark player as pvp
                player->UpdatePvP(true, true);
                player->SetFlag(PLAYER_FLAGS, PLAYER_FLAGS_IN_PVP);

                // prevent despawning after go use
                go->SetRespawnTime(0);

                // no need to iterate the other roosts
                return false;
            }
        }
    }
    else if (player->GetTeam() == HORDE)
    {
        for (uint8 i = 0; i < MAX_NA_ROOSTS; ++i)
        {
            if (go->GetEntry() == nagrandWagonsHorde[i])
            {
                // update roost states
                UpdateWyvernsWorldState(WORLD_STATE_REMOVE);
                m_roostWorldState[i] = nagrandRoostStatesAllianceNeutral[i];
                UpdateWyvernsWorldState(WORLD_STATE_ADD);

                // spawn the broken roost and despawn the other one
                RespawnGO(go, m_roostsAlliance[i], false);
                RespawnGO(go, m_wagonsHorde[i], false);
                RespawnGO(go, m_roostsBrokenAlliance[i], true);

                // no need to iterate the other roosts
                return false;
            }
            else if (go->GetEntry() == nagrandRoostsBrokenHorde[i])
            {
                // update roost states
                UpdateWyvernsWorldState(WORLD_STATE_REMOVE);
                m_roostWorldState[i] = nagrandRoostStatesHorde[i];
                UpdateWyvernsWorldState(WORLD_STATE_ADD);

                // spawn the repaired one along with the explosive wagon - the
                // broken one despawns by self
                RespawnGO(go, m_wagonsAlliance[i], true);
                RespawnGO(go, m_roostsHorde[i], true);
                RespawnGO(go, m_roostsBrokenHorde[i], false);
                // no need to iterate the other roosts
                return false;
            }
            else if (go->GetEntry() == nagrandRoostsHorde[i])
            {
                // Activate Flight path
                std::vector<uint32> nodes;
                nodes.resize(2);
                nodes[0] = FlightPathStartNodes[i];
                nodes[1] = FlightPathEndNodes[i];
                player->ActivateTaxiPathTo(nodes);

                // Add 10 bombs to the player
                inventory::transaction trans;
                trans.add(24538, 10);
                player->storage().finalize(trans);

                // mark player as pvp
                player->UpdatePvP(true, true);
                player->SetFlag(PLAYER_FLAGS, PLAYER_FLAGS_IN_PVP);

                // prevent despawning after go use
                go->SetRespawnTime(0);

                // no need to iterate the other roosts
                return false;
            }
        }
    }

    return false;
}

// Lock Halaa when captured
void OutdoorPvPNA::LockHalaa(const WorldObject* objRef)
{
    if (GameObject* go = objRef->GetMap()->GetGameObject(m_capturePoint))
        go->SetLootState(GO_JUST_DEACTIVATED);
    else
        // if grid is unloaded, changing the saved slider value is enough
        sOutdoorPvPMgr::Instance()->SetCapturePointSlider(GO_HALAA_BANNER,
            m_zoneOwner == ALLIANCE ? -CAPTURE_SLIDER_ALLIANCE :
                                      -CAPTURE_SLIDER_HORDE);
}

// Unlock Halaa when all the soldiers are killed
void OutdoorPvPNA::UnlockHalaa(const WorldObject* objRef)
{
    if (GameObject* go = objRef->GetMap()->GetGameObject(m_capturePoint))
        go->SetCapturePointSlider(m_zoneOwner == ALLIANCE ?
                                      CAPTURE_SLIDER_ALLIANCE :
                                      CAPTURE_SLIDER_HORDE);
    else
        // if grid is unloaded, resetting the saved slider value is enough
        sOutdoorPvPMgr::Instance()->SetCapturePointSlider(
            GO_HALAA_BANNER, m_zoneOwner == ALLIANCE ? CAPTURE_SLIDER_ALLIANCE :
                                                       CAPTURE_SLIDER_HORDE);
}
