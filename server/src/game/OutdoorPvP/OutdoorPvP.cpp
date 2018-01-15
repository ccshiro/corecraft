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

#include "OutdoorPvP.h"
#include "GameObject.h"
#include "Language.h"
#include "MapManager.h"
#include "Object.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "World.h"

/**
   Function that adds a player to the players of the affected outdoor pvp zones

   @param   player to add
   @param   whether zone is main outdoor pvp zone or a affected zone
 */
void OutdoorPvP::HandlePlayerEnterZone(Player* player, bool isMainZone)
{
    m_zonePlayers[player->GetObjectGuid()] = isMainZone;
}

bool OutdoorPvP::AddCreature(uint32 entry, Team teamval, uint32 guidLow)
{
    // Like Summoning creatures
    CreatureInfo const* cinfo =
        sObjectMgr::Instance()->GetCreatureTemplate(entry);
    if (!cinfo)
        return false;

    CreatureData const* data = sObjectMgr::Instance()->GetCreatureData(guidLow);
    if (!data)
    {
        logging.error(
            "Creature (GUID: %u) not found in table `creature`, can't load. "
            "OutdoorPvp::AddCreature()",
            guidLow);
        return false;
    }

    Map* pMap = sMapMgr::Instance()->FindMap(data->mapid);
    if (!pMap)
        return false;

    auto creature = new Creature(CREATURE_SUBTYPE_GENERIC);

    CreatureCreatePos pos(
        pMap, data->posX, data->posY, data->posZ, data->orientation);
    if (!creature->Create(guidLow, pos, cinfo, teamval, data))
    {
        delete creature;
        return false;
    }

    if (!pMap->insert(creature))
    {
        delete creature;
        return false;
    }

    // Home Position
    creature->set_default_movement_gen(movement::gen(data->movementType));
    creature->AIM_Initialize();
    creature->SetRespawnDelay(data->spawntimesecs);
    m_Creatures.push_back(creature);
    return true;
}

void OutdoorPvP::DeleteSpawnCreatures()
{
    auto copy = m_Creatures;
    m_Creatures.clear();
    for (Creature* elem : copy)
    {
        if (elem->IsInWorld())
            elem->GetMap()->erase(elem, true);
        else
            elem->queue_action(0, [elem]
                {
                    elem->GetMap()->erase(elem, true);
                });
    }
}

/**
   Function that removes a player from the players of the affected outdoor pvp
   zones

   @param   player to remove
   @param   whether zone is main outdoor pvp zone or a affected zone
 */
void OutdoorPvP::HandlePlayerLeaveZone(Player* player, bool isMainZone)
{
    if (m_zonePlayers.erase(player->GetObjectGuid()))
    {
        // remove the world state information from the player
        if (isMainZone && !player->GetSession()->PlayerLogout())
            SendRemoveWorldStates(player);

        LOG_DEBUG(
            logging, "Player %s left an Outdoor PvP zone", player->GetName());
    }
}

/**
   Function that updates the world state for all the players of the outdoor pvp
   zone

   @param   world state to update
   @param   new world state value
 */
void OutdoorPvP::SendUpdateWorldState(uint32 field, uint32 value)
{
    for (GuidZoneMap::const_iterator itr = m_zonePlayers.begin();
         itr != m_zonePlayers.end(); ++itr)
    {
        // only send world state update to main zone
        if (!itr->second)
            continue;

        if (Player* player = sObjectMgr::Instance()->GetPlayer(itr->first))
            player->SendUpdateWorldState(field, value);
    }
}

void OutdoorPvP::HandleGameObjectCreate(GameObject* go)
{
    // set initial data and activate capture points
    if (go->GetGOInfo()->type == GAMEOBJECT_TYPE_CAPTURE_POINT)
        go->SetCapturePointSlider(
            sOutdoorPvPMgr::Instance()->GetCapturePointSliderValue(
                go->GetEntry(), CAPTURE_SLIDER_MIDDLE));
}

void OutdoorPvP::HandleGameObjectRemove(GameObject* go)
{
    // save capture point slider value (negative value if locked)
    if (go->GetGOInfo()->type == GAMEOBJECT_TYPE_CAPTURE_POINT)
        sOutdoorPvPMgr::Instance()->SetCapturePointSlider(go->GetEntry(),
            go->getLootState() == GO_ACTIVATED ? go->GetCapturePointSlider() :
                                                 -go->GetCapturePointSlider());
}

/**
   Function that handles player kills in the main outdoor pvp zones

   @param   player who killed another player
   @param   victim who was killed
 */
void OutdoorPvP::HandlePlayerKill(Player* killer, Player* victim)
{
    if (victim->HasAuraType(SPELL_AURA_NO_PVP_CREDIT))
        return;

    if (Group* group = killer->GetGroup())
    {
        for (auto member : group->members(true))
        {
            // skip if too far away
            if (!member->IsAtGroupRewardDistance(victim))
                continue;

            // creature kills must be notified, even if not inside objective /
            // not outdoor pvp active
            // player kills only count if active and inside objective
            if (member->CanUseCapturePoint())
                HandlePlayerKillInsideArea(member);
        }
    }
    else
    {
        // creature kills must be notified, even if not inside objective / not
        // outdoor pvp active
        if (killer && killer->CanUseCapturePoint())
            HandlePlayerKillInsideArea(killer);
    }
}

// apply a team buff for the main and affected zones
void OutdoorPvP::BuffTeam(Team team, uint32 spellId, bool remove /*= false*/)
{
    for (GuidZoneMap::const_iterator itr = m_zonePlayers.begin();
         itr != m_zonePlayers.end(); ++itr)
    {
        Player* player = sObjectMgr::Instance()->GetPlayer(itr->first);
        if (player && player->GetTeam() == team)
        {
            if (remove)
                player->remove_auras(spellId);
            else
                player->CastSpell(player, spellId, true);
        }
    }
}

uint32 OutdoorPvP::GetBannerArtKit(Team team,
    uint32 artKitAlliance /*= CAPTURE_ARTKIT_ALLIANCE*/,
    uint32 artKitHorde /*= CAPTURE_ARTKIT_HORDE*/,
    uint32 artKitNeutral /*= CAPTURE_ARTKIT_NEUTRAL*/)
{
    switch (team)
    {
    case ALLIANCE:
        return artKitAlliance;
    case HORDE:
        return artKitHorde;
    default:
        return artKitNeutral;
    }
}

void OutdoorPvP::SetBannerVisual(
    const WorldObject* objRef, ObjectGuid goGuid, uint32 artKit, uint32 animId)
{
    if (GameObject* go = objRef->GetMap()->GetGameObject(goGuid))
        SetBannerVisual(go, artKit, animId);
}

void OutdoorPvP::SetBannerVisual(GameObject* go, uint32 artKit, uint32 animId)
{
    go->SendGameObjectCustomAnim(go->GetObjectGuid(), animId);
    go->SetGoArtKit(artKit);
    go->Refresh();
}

void OutdoorPvP::RespawnGO(
    const WorldObject* objRef, ObjectGuid goGuid, bool respawn)
{
    if (GameObject* go = objRef->GetMap()->GetGameObject(goGuid))
    {
        if (respawn)
        {
            go->SetRespawnTime(0);
            go->Refresh();
        }
        else if (go->isSpawned())
        {
            go->SetRespawnTime(7 * DAY);
            go->SetLootState(GO_JUST_DEACTIVATED);
        }
    }
}
