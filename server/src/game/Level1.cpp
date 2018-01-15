/*
 * Copyright (C) 2005-2012 MaNGOS <http://getmangos.com/>
 *
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

#include "Chat.h"
#include "Common.h"
#include "DBCStores.h"
#include "Language.h"
#include "logging.h"
#include "Mail.h"
#include "MapManager.h"
#include "MapPersistentStateMgr.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Opcodes.h"
#include "Player.h"
#include "Transport.h"
#include "Util.h"
#include "VMapFactory.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "Database/DatabaseEnv.h"
#include "G3D/Ray.h"
#include "framework/grid/grid_helpers.h"
#include "maps/map_grid.h"

//-----------------------Npc Commands-----------------------
bool ChatHandler::HandleNpcSayCommand(char* args)
{
    if (!*args)
        return false;

    Creature* pCreature = getSelectedCreature();
    if (!pCreature)
    {
        SendSysMessage(LANG_SELECT_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    pCreature->MonsterSay(args, LANG_UNIVERSAL);

    return true;
}

bool ChatHandler::HandleNpcYellCommand(char* args)
{
    if (!*args)
        return false;

    Creature* pCreature = getSelectedCreature();
    if (!pCreature)
    {
        SendSysMessage(LANG_SELECT_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    pCreature->MonsterYell(args, LANG_UNIVERSAL);

    return true;
}

// show text emote by creature in chat
bool ChatHandler::HandleNpcTextEmoteCommand(char* args)
{
    if (!*args)
        return false;

    Creature* pCreature = getSelectedCreature();

    if (!pCreature)
    {
        SendSysMessage(LANG_SELECT_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    pCreature->MonsterTextEmote(args, nullptr);

    return true;
}

// make npc whisper to player
bool ChatHandler::HandleNpcWhisperCommand(char* args)
{
    Player* target;
    if (!ExtractPlayerTarget(&args, &target))
        return false;

    ObjectGuid guid = m_session->GetPlayer()->GetSelectionGuid();
    if (!guid)
        return false;

    Creature* pCreature = m_session->GetPlayer()->GetMap()->GetCreature(guid);

    if (!pCreature || !target || !*args)
        return false;

    // check online security
    if (HasLowerSecurity(target))
        return false;

    pCreature->MonsterWhisper(args, target);

    return true;
}
//----------------------------------------------------------

// global announce
bool ChatHandler::HandleAnnounceCommand(char* args)
{
    if (!*args)
        return false;

    sWorld::Instance()->SendWorldText(LANG_SYSTEMMESSAGE, args);
    return true;
}

// notification player at the screen
bool ChatHandler::HandleNotifyCommand(char* args)
{
    if (!*args)
        return false;

    std::string str = GetMangosString(LANG_GLOBAL_NOTIFY);
    str += args;

    WorldPacket data(SMSG_NOTIFICATION, (str.size() + 1));
    data << str;
    sWorld::Instance()->SendGlobalMessage(&data);

    return true;
}

// Enable\Dissable GM Mode
bool ChatHandler::HandleGMCommand(char* args)
{
    if (!*args)
    {
        if (m_session->GetPlayer()->isGameMaster())
            m_session->SendNotification(LANG_GM_ON);
        else
            m_session->SendNotification(LANG_GM_OFF);
        return true;
    }

    bool value;
    char* str = ExtractLiteralArg(&args);
    if (str != nullptr && strcmp(str, "toggle") == 0)
    {
        value = !m_session->GetPlayer()->isGameMaster();
    }
    else if (str != nullptr && strcmp(str, "on") == 0)
    {
        value = true;
    }
    else if (str != nullptr && strcmp(str, "off") == 0)
    {
        value = false;
    }
    else
    {
        SendSysMessage(LANG_USE_BOL);
        SetSentErrorMessage(true);
        return false;
    }

    if (value)
    {
        m_session->GetPlayer()->SetGameMaster(true);
        m_session->SendNotification(LANG_GM_ON);
    }
    else
    {
        m_session->GetPlayer()->SetGameMaster(false);
        m_session->SendNotification(LANG_GM_OFF);
    }

    return true;
}

// Enables or disables hiding of the staff badge
bool ChatHandler::HandleGMChatCommand(char* args)
{
    if (!*args)
    {
        if (m_session->GetPlayer()->isGMChat())
            m_session->SendNotification(LANG_GM_CHAT_ON);
        else
            m_session->SendNotification(LANG_GM_CHAT_OFF);
        return true;
    }

    bool value;
    if (!ExtractOnOff(&args, value))
    {
        SendSysMessage(LANG_USE_BOL);
        SetSentErrorMessage(true);
        return false;
    }

    if (value)
    {
        m_session->GetPlayer()->SetGMChat(true);
        m_session->SendNotification(LANG_GM_CHAT_ON);
    }
    else
    {
        m_session->GetPlayer()->SetGMChat(false);
        m_session->SendNotification(LANG_GM_CHAT_OFF);
    }

    return true;
}

// Enable\Dissable Invisible mode
bool ChatHandler::HandleGMVisibleCommand(char* args)
{
    if (!*args)
    {
        PSendSysMessage(LANG_YOU_ARE, m_session->GetPlayer()->isGMVisible() ?
                                          GetMangosString(LANG_VISIBLE) :
                                          GetMangosString(LANG_INVISIBLE));
        return true;
    }

    bool value;
    if (!ExtractOnOff(&args, value))
    {
        SendSysMessage(LANG_USE_BOL);
        SetSentErrorMessage(true);
        return false;
    }

    if (value)
    {
        m_session->GetPlayer()->SetGMVisible(true);
        m_session->SendNotification(LANG_INVISIBLE_VISIBLE);
    }
    else
    {
        m_session->SendNotification(LANG_INVISIBLE_INVISIBLE);
        m_session->GetPlayer()->SetGMVisible(false);
    }

    return true;
}

bool ChatHandler::HandleGPSCommand(char* args)
{
    WorldObject* obj = getSelectedUnit();

    if (!obj)
    {
        SendSysMessage(LANG_SELECT_CHAR_OR_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    bool verbose = false;
    const char* arg = ExtractQuotedOrLiteralArg(&args);
    if (arg && (strcmp("-v", arg) == 0 || strcmp("--verbose", arg) == 0 ||
                   strcmp("-verbose", arg) == 0))
        verbose = true;

    uint32 zone_id, area_id;
    obj->GetZoneAndAreaId(zone_id, area_id);

    MapEntry const* mapEntry = sMapStore.LookupEntry(obj->GetMapId());
    AreaTableEntry const* zoneEntry = GetAreaEntryByAreaID(zone_id);
    AreaTableEntry const* areaEntry = GetAreaEntryByAreaID(area_id);

    float zone_x = obj->GetX();
    float zone_y = obj->GetY();

    if (!Map2ZoneCoordinates(zone_x, zone_y, zone_id))
    {
        zone_x = 0;
        zone_y = 0;
    }

    TerrainInfo const* map = obj->GetTerrain();
    auto p = maps::world_coords_to_data_cell(obj->GetX(), obj->GetY());

    int gx = p.first;
    int gy = p.second;

    bool have_map = GridMap::ExistMap(obj->GetMapId(), gx, gy);
    bool have_vmap = GridMap::ExistVMap(obj->GetMapId(), gx, gy);
    auto vmgr = VMAP::VMapFactory::createOrGetVMapManager();

    bool outdoor = false;
    float grid_height = INVALID_HEIGHT_VALUE;
    std::vector<G3D::Vector3> vmap_heights;

    if (auto grid =
            const_cast<TerrainInfo*>(map)->GetGrid(obj->GetX(), obj->GetY()))
        grid_height = grid->getHeight(obj->GetX(), obj->GetY());

    if (have_vmap)
    {
        outdoor = map->IsOutdoors(obj->GetX(), obj->GetY(), obj->GetZ());

        vmap_heights = vmgr->getAllIntersections(obj->GetMapId(), true,
            G3D::Vector3(obj->GetX(), obj->GetY(), MAX_HEIGHT),
            G3D::Vector3(0, 0, -1));
    }

    auto cell = framework::grid::coords_to_cell_pair(
        MAP_CELL_MID, MAP_CELL_SIZE, obj->GetX(), obj->GetY());

    PSendSysMessage("Map: %u:%u (%s) Zone: %u (%s) Area: %u (%s)",
        obj->GetMapId(), obj->GetInstanceId(),
        (mapEntry ? mapEntry->name[GetSessionDbcLocale()] : "<unknown>"),
        zone_id,
        (zoneEntry ? zoneEntry->area_name[GetSessionDbcLocale()] : "<unknown>"),
        area_id, (areaEntry ? areaEntry->area_name[GetSessionDbcLocale()] :
                              "<unknown>"));

    PSendSysMessage("X: %.3f, Y: %.3f, Z: %.3f, O: %.3f, Cell: (%d, %d)",
        obj->GetX(), obj->GetY(), obj->GetZ(), obj->GetO(), cell.first,
        cell.second);

    if (verbose)
        PSendSysMessage("Map Data: %s VMap Data: %s Data cell: (%d, %d)",
            (have_map ? "available" : "not available"),
            (have_vmap ? "available" : "not available"), gx, gy);

    if (verbose)
    {
        std::string vm_height = "";
        if (vmap_heights.size() > 0)
        {
            vm_height = "VMap Heights: ";
            for (auto pos : vmap_heights)
                vm_height += std::to_string(pos.z) + " ";
        }
        PSendSysMessage("Target is %s. Grid Height: %f %s",
            (outdoor ? "Outdoor" : "Indoor"), grid_height, vm_height.c_str());
    }

    GridMapLiquidData liquid_status;
    bool vmap_check = vmgr->HasLiquidData(mapEntry->MapID, obj->GetAreaId());
    GridMapLiquidStatus res = map->getLiquidStatus(obj->GetX(), obj->GetY(),
        obj->GetZ(), MAP_ALL_LIQUIDS, vmap_check, &liquid_status);
    if (res && verbose)
    {
        PSendSysMessage(LANG_LIQUID_STATUS, liquid_status.level,
            liquid_status.depth_level, liquid_status.entry,
            liquid_status.type_flags, res, vmap_check ? "yes" : "no");
    }

    Transport* trans = obj->GetTransport();
    if (trans && verbose)
    {
        G3D::Vector3 pos;
        obj->m_movementInfo.transport.pos.Get(pos.x, pos.y, pos.z);
        float height = trans->GetHeight(pos);
        if (height > INVALID_HEIGHT)
            PSendSysMessage(
                "On transport \"%s\" (x: %f, y: %f, z: %f). Transport floor: "
                "%f.",
                trans->GetName(), pos.x, pos.y, pos.z, height);
        else
            PSendSysMessage(
                "On transport \"%s\" (x: %f, y: %f, z: %f). Unable to find "
                "transport floor!",
                trans->GetName(), pos.x, pos.y, pos.z);
    }

    return true;
}

// Edit Player HP
bool ChatHandler::HandleModifyHPCommand(char* args)
{
    if (!*args)
        return false;

    int32 hp = atoi(args);
    int32 hpm = atoi(args);

    if (hp <= 0 || hpm <= 0 || hpm < hp)
    {
        SendSysMessage(LANG_BAD_VALUE);
        SetSentErrorMessage(true);
        return false;
    }

    Player* chr = getSelectedPlayer();
    if (chr == nullptr)
    {
        SendSysMessage(LANG_NO_CHAR_SELECTED);
        SetSentErrorMessage(true);
        return false;
    }

    // check online security
    if (HasLowerSecurity(chr))
        return false;

    PSendSysMessage(LANG_YOU_CHANGE_HP, GetNameLink(chr).c_str(), hp, hpm);
    if (needReportToTarget(chr))
        ChatHandler(chr).PSendSysMessage(
            LANG_YOURS_HP_CHANGED, GetNameLink().c_str(), hp, hpm);

    chr->SetMaxHealth(hpm);
    chr->SetHealth(hp);

    return true;
}

// Edit Player Mana
bool ChatHandler::HandleModifyManaCommand(char* args)
{
    if (!*args)
        return false;

    int32 mana = atoi(args);
    int32 manam = atoi(args);

    if (mana <= 0 || manam <= 0 || manam < mana)
    {
        SendSysMessage(LANG_BAD_VALUE);
        SetSentErrorMessage(true);
        return false;
    }

    Player* chr = getSelectedPlayer();
    if (chr == nullptr)
    {
        SendSysMessage(LANG_NO_CHAR_SELECTED);
        SetSentErrorMessage(true);
        return false;
    }

    // check online security
    if (HasLowerSecurity(chr))
        return false;

    PSendSysMessage(
        LANG_YOU_CHANGE_MANA, GetNameLink(chr).c_str(), mana, manam);
    if (needReportToTarget(chr))
        ChatHandler(chr).PSendSysMessage(
            LANG_YOURS_MANA_CHANGED, GetNameLink().c_str(), mana, manam);

    chr->SetMaxPower(POWER_MANA, manam);
    chr->SetPower(POWER_MANA, mana);

    return true;
}

// Edit Player Energy
bool ChatHandler::HandleModifyEnergyCommand(char* args)
{
    if (!*args)
        return false;

    int32 energy = atoi(args) * 10;
    int32 energym = atoi(args) * 10;

    if (energy <= 0 || energym <= 0 || energym < energy)
    {
        SendSysMessage(LANG_BAD_VALUE);
        SetSentErrorMessage(true);
        return false;
    }

    Player* chr = getSelectedPlayer();
    if (!chr)
    {
        SendSysMessage(LANG_NO_CHAR_SELECTED);
        SetSentErrorMessage(true);
        return false;
    }

    // check online security
    if (HasLowerSecurity(chr))
        return false;

    PSendSysMessage(LANG_YOU_CHANGE_ENERGY, GetNameLink(chr).c_str(),
        energy / 10, energym / 10);
    if (needReportToTarget(chr))
        ChatHandler(chr).PSendSysMessage(LANG_YOURS_ENERGY_CHANGED,
            GetNameLink().c_str(), energy / 10, energym / 10);

    chr->SetMaxPower(POWER_ENERGY, energym);
    chr->SetPower(POWER_ENERGY, energy);

    LOG_DEBUG(logging, GetMangosString(LANG_CURRENT_ENERGY),
        chr->GetMaxPower(POWER_ENERGY));

    return true;
}

// Edit Player Rage
bool ChatHandler::HandleModifyRageCommand(char* args)
{
    if (!*args)
        return false;

    int32 rage = atoi(args) * 10;
    int32 ragem = atoi(args) * 10;

    if (rage <= 0 || ragem <= 0 || ragem < rage)
    {
        SendSysMessage(LANG_BAD_VALUE);
        SetSentErrorMessage(true);
        return false;
    }

    Player* chr = getSelectedPlayer();
    if (chr == nullptr)
    {
        SendSysMessage(LANG_NO_CHAR_SELECTED);
        SetSentErrorMessage(true);
        return false;
    }

    // check online security
    if (HasLowerSecurity(chr))
        return false;

    PSendSysMessage(
        LANG_YOU_CHANGE_RAGE, GetNameLink(chr).c_str(), rage / 10, ragem / 10);
    if (needReportToTarget(chr))
        ChatHandler(chr).PSendSysMessage(LANG_YOURS_RAGE_CHANGED,
            GetNameLink().c_str(), rage / 10, ragem / 10);

    chr->SetMaxPower(POWER_RAGE, ragem);
    chr->SetPower(POWER_RAGE, rage);

    return true;
}

// Edit Player Faction
bool ChatHandler::HandleModifyFactionCommand(char* args)
{
    Creature* chr = getSelectedCreature();
    if (!chr)
    {
        SendSysMessage(LANG_SELECT_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    if (!*args)
    {
        if (chr)
        {
            uint32 factionid = chr->getFaction();
            uint32 flag = chr->GetUInt32Value(UNIT_FIELD_FLAGS);
            uint32 npcflag = chr->GetUInt32Value(UNIT_NPC_FLAGS);
            uint32 dyflag = chr->GetUInt32Value(UNIT_DYNAMIC_FLAGS);
            PSendSysMessage(LANG_CURRENT_FACTION, chr->GetGUIDLow(), factionid,
                flag, npcflag, dyflag);
        }
        return true;
    }

    if (!chr)
    {
        SendSysMessage(LANG_NO_CHAR_SELECTED);
        SetSentErrorMessage(true);
        return false;
    }

    uint32 factionid;
    if (!ExtractUint32KeyFromLink(&args, "Hfaction", factionid))
        return false;

    if (!sFactionTemplateStore.LookupEntry(factionid))
    {
        PSendSysMessage(LANG_WRONG_FACTION, factionid);
        SetSentErrorMessage(true);
        return false;
    }

    uint32 flag;
    if (!ExtractOptUInt32(&args, flag, chr->GetUInt32Value(UNIT_FIELD_FLAGS)))
        return false;

    uint32 npcflag;
    if (!ExtractOptUInt32(&args, npcflag, chr->GetUInt32Value(UNIT_NPC_FLAGS)))
        return false;

    uint32 dyflag;
    if (!ExtractOptUInt32(
            &args, dyflag, chr->GetUInt32Value(UNIT_DYNAMIC_FLAGS)))
        return false;

    PSendSysMessage(LANG_YOU_CHANGE_FACTION, chr->GetGUIDLow(), factionid, flag,
        npcflag, dyflag);

    chr->setFaction(factionid);
    chr->SetUInt32Value(UNIT_FIELD_FLAGS, flag);
    chr->SetUInt32Value(UNIT_NPC_FLAGS, npcflag);
    chr->SetUInt32Value(UNIT_DYNAMIC_FLAGS, dyflag);

    return true;
}

// Edit Player TP
bool ChatHandler::HandleModifyTalentCommand(char* args)
{
    if (!*args)
        return false;

    int tp = atoi(args);
    if (tp < 0)
        return false;

    Player* target = getSelectedPlayer();
    if (!target)
    {
        SendSysMessage(LANG_NO_CHAR_SELECTED);
        SetSentErrorMessage(true);
        return false;
    }

    // check online security
    if (HasLowerSecurity(target))
        return false;

    target->SetFreeTalentPoints(tp);
    return true;
}

// Enable On\OFF all taxi paths
bool ChatHandler::HandleTaxiCheatCommand(char* args)
{
    bool value;
    if (!ExtractOnOff(&args, value))
    {
        SendSysMessage(LANG_USE_BOL);
        SetSentErrorMessage(true);
        return false;
    }

    Player* chr = getSelectedPlayer();
    if (!chr)
        chr = m_session->GetPlayer();
    // check online security
    else if (HasLowerSecurity(chr))
        return false;

    if (value)
    {
        chr->SetTaxiCheater(true);
        PSendSysMessage(LANG_YOU_GIVE_TAXIS, GetNameLink(chr).c_str());
        if (needReportToTarget(chr))
            ChatHandler(chr).PSendSysMessage(
                LANG_YOURS_TAXIS_ADDED, GetNameLink().c_str());
    }
    else
    {
        chr->SetTaxiCheater(false);
        PSendSysMessage(LANG_YOU_REMOVE_TAXIS, GetNameLink(chr).c_str());
        if (needReportToTarget(chr))
            ChatHandler(chr).PSendSysMessage(
                LANG_YOURS_TAXIS_REMOVED, GetNameLink().c_str());
    }

    return true;
}

// Edit Player Aspeed
bool ChatHandler::HandleModifyASpeedCommand(char* args)
{
    if (!*args)
        return false;

    float modSpeed = (float)atof(args);

    if (modSpeed > 100 || modSpeed < 0.1)
    {
        SendSysMessage(LANG_BAD_VALUE);
        SetSentErrorMessage(true);
        return false;
    }

    Player* chr = getSelectedPlayer();
    if (chr == nullptr)
    {
        SendSysMessage(LANG_NO_CHAR_SELECTED);
        SetSentErrorMessage(true);
        return false;
    }

    // check online security
    if (HasLowerSecurity(chr))
        return false;

    std::string chrNameLink = GetNameLink(chr);

    if (chr->IsTaxiFlying())
    {
        PSendSysMessage(LANG_CHAR_IN_FLIGHT, chrNameLink.c_str());
        SetSentErrorMessage(true);
        return false;
    }

    PSendSysMessage(LANG_YOU_CHANGE_ASPEED, modSpeed, chrNameLink.c_str());
    if (needReportToTarget(chr))
        ChatHandler(chr).PSendSysMessage(
            LANG_YOURS_ASPEED_CHANGED, GetNameLink().c_str(), modSpeed);

    chr->UpdateSpeed(MOVE_WALK, true, modSpeed);
    chr->UpdateSpeed(MOVE_RUN, true, modSpeed);
    chr->UpdateSpeed(MOVE_SWIM, true, modSpeed);
    // chr->UpdateSpeed(MOVE_TURN,   true, modSpeed);
    chr->UpdateSpeed(MOVE_FLIGHT, true, modSpeed);
    return true;
}

// Edit Player Speed
bool ChatHandler::HandleModifySpeedCommand(char* args)
{
    if (!*args)
        return false;

    float modSpeed = (float)atof(args);

    if (modSpeed > 10 || modSpeed < 0.1)
    {
        SendSysMessage(LANG_BAD_VALUE);
        SetSentErrorMessage(true);
        return false;
    }

    Player* chr = getSelectedPlayer();
    if (chr == nullptr)
    {
        SendSysMessage(LANG_NO_CHAR_SELECTED);
        SetSentErrorMessage(true);
        return false;
    }

    // check online security
    if (HasLowerSecurity(chr))
        return false;

    std::string chrNameLink = GetNameLink(chr);

    if (chr->IsTaxiFlying())
    {
        PSendSysMessage(LANG_CHAR_IN_FLIGHT, chrNameLink.c_str());
        SetSentErrorMessage(true);
        return false;
    }

    PSendSysMessage(LANG_YOU_CHANGE_SPEED, modSpeed, chrNameLink.c_str());
    if (needReportToTarget(chr))
        ChatHandler(chr).PSendSysMessage(
            LANG_YOURS_SPEED_CHANGED, GetNameLink().c_str(), modSpeed);

    chr->UpdateSpeed(MOVE_RUN, true, modSpeed);

    return true;
}

// Edit Player Swim Speed
bool ChatHandler::HandleModifySwimCommand(char* args)
{
    if (!*args)
        return false;

    float modSpeed = (float)atof(args);

    if (modSpeed > 10.0f || modSpeed < 0.01f)
    {
        SendSysMessage(LANG_BAD_VALUE);
        SetSentErrorMessage(true);
        return false;
    }

    Player* chr = getSelectedPlayer();
    if (chr == nullptr)
    {
        SendSysMessage(LANG_NO_CHAR_SELECTED);
        SetSentErrorMessage(true);
        return false;
    }

    // check online security
    if (HasLowerSecurity(chr))
        return false;

    std::string chrNameLink = GetNameLink(chr);

    if (chr->IsTaxiFlying())
    {
        PSendSysMessage(LANG_CHAR_IN_FLIGHT, chrNameLink.c_str());
        SetSentErrorMessage(true);
        return false;
    }

    PSendSysMessage(LANG_YOU_CHANGE_SWIM_SPEED, modSpeed, chrNameLink.c_str());
    if (needReportToTarget(chr))
        ChatHandler(chr).PSendSysMessage(
            LANG_YOURS_SWIM_SPEED_CHANGED, GetNameLink().c_str(), modSpeed);

    chr->UpdateSpeed(MOVE_SWIM, true, modSpeed);

    return true;
}

// Edit Player Walk Speed
bool ChatHandler::HandleModifyBWalkCommand(char* args)
{
    if (!*args)
        return false;

    float modSpeed = (float)atof(args);

    if (modSpeed > 10.0f || modSpeed < 0.1f)
    {
        SendSysMessage(LANG_BAD_VALUE);
        SetSentErrorMessage(true);
        return false;
    }

    Player* chr = getSelectedPlayer();
    if (chr == nullptr)
    {
        SendSysMessage(LANG_NO_CHAR_SELECTED);
        SetSentErrorMessage(true);
        return false;
    }

    // check online security
    if (HasLowerSecurity(chr))
        return false;

    std::string chrNameLink = GetNameLink(chr);

    if (chr->IsTaxiFlying())
    {
        PSendSysMessage(LANG_CHAR_IN_FLIGHT, chrNameLink.c_str());
        SetSentErrorMessage(true);
        return false;
    }

    PSendSysMessage(LANG_YOU_CHANGE_BACK_SPEED, modSpeed, chrNameLink.c_str());
    if (needReportToTarget(chr))
        ChatHandler(chr).PSendSysMessage(
            LANG_YOURS_BACK_SPEED_CHANGED, GetNameLink().c_str(), modSpeed);

    chr->UpdateSpeed(MOVE_RUN_BACK, true, modSpeed);

    return true;
}

// Edit Player Fly
bool ChatHandler::HandleModifyFlyCommand(char* args)
{
    if (!*args)
        return false;

    float modSpeed = (float)atof(args);

    if (modSpeed > 10.0f || modSpeed < 0.1f)
    {
        SendSysMessage(LANG_BAD_VALUE);
        SetSentErrorMessage(true);
        return false;
    }

    Player* chr = getSelectedPlayer();
    if (chr == nullptr)
    {
        SendSysMessage(LANG_NO_CHAR_SELECTED);
        SetSentErrorMessage(true);
        return false;
    }

    // check online security
    if (HasLowerSecurity(chr))
        return false;

    PSendSysMessage(
        LANG_YOU_CHANGE_FLY_SPEED, modSpeed, GetNameLink(chr).c_str());
    if (needReportToTarget(chr))
        ChatHandler(chr).PSendSysMessage(
            LANG_YOURS_FLY_SPEED_CHANGED, GetNameLink().c_str(), modSpeed);

    chr->UpdateSpeed(MOVE_FLIGHT, true, modSpeed);

    return true;
}

// Edit Player Scale
bool ChatHandler::HandleModifyScaleCommand(char* args)
{
    if (!*args)
        return false;

    float Scale = (float)atof(args);
    if (Scale > 10.0f || Scale <= 0.0f)
    {
        SendSysMessage(LANG_BAD_VALUE);
        SetSentErrorMessage(true);
        return false;
    }

    Unit* target = getSelectedUnit();
    if (target == nullptr)
    {
        SendSysMessage(LANG_SELECT_CHAR_OR_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    if (target->GetTypeId() == TYPEID_PLAYER)
    {
        // check online security
        if (HasLowerSecurity((Player*)target))
            return false;

        PSendSysMessage(
            LANG_YOU_CHANGE_SIZE, Scale, GetNameLink((Player*)target).c_str());
        if (needReportToTarget((Player*)target))
            ChatHandler((Player*)target)
                .PSendSysMessage(
                    LANG_YOURS_SIZE_CHANGED, GetNameLink().c_str(), Scale);
    }

    target->SetObjectScale(Scale);
    target->UpdateModelData();

    return true;
}

// Enable Player mount
bool ChatHandler::HandleModifyMountCommand(char* args)
{
    if (!*args)
        return false;

    uint16 mId = 1147;
    float speed = (float)15;
    uint32 num = atoi(args);
    switch (num)
    {
    case 1:
        mId = 14340;
        break;
    case 2:
        mId = 4806;
        break;
    case 3:
        mId = 6471;
        break;
    case 4:
        mId = 12345;
        break;
    case 5:
        mId = 6472;
        break;
    case 6:
        mId = 6473;
        break;
    case 7:
        mId = 10670;
        break;
    case 8:
        mId = 10719;
        break;
    case 9:
        mId = 10671;
        break;
    case 10:
        mId = 10672;
        break;
    case 11:
        mId = 10720;
        break;
    case 12:
        mId = 14349;
        break;
    case 13:
        mId = 11641;
        break;
    case 14:
        mId = 12244;
        break;
    case 15:
        mId = 12242;
        break;
    case 16:
        mId = 14578;
        break;
    case 17:
        mId = 14579;
        break;
    case 18:
        mId = 14349;
        break;
    case 19:
        mId = 12245;
        break;
    case 20:
        mId = 14335;
        break;
    case 21:
        mId = 207;
        break;
    case 22:
        mId = 2328;
        break;
    case 23:
        mId = 2327;
        break;
    case 24:
        mId = 2326;
        break;
    case 25:
        mId = 14573;
        break;
    case 26:
        mId = 14574;
        break;
    case 27:
        mId = 14575;
        break;
    case 28:
        mId = 604;
        break;
    case 29:
        mId = 1166;
        break;
    case 30:
        mId = 2402;
        break;
    case 31:
        mId = 2410;
        break;
    case 32:
        mId = 2409;
        break;
    case 33:
        mId = 2408;
        break;
    case 34:
        mId = 2405;
        break;
    case 35:
        mId = 14337;
        break;
    case 36:
        mId = 6569;
        break;
    case 37:
        mId = 10661;
        break;
    case 38:
        mId = 10666;
        break;
    case 39:
        mId = 9473;
        break;
    case 40:
        mId = 9476;
        break;
    case 41:
        mId = 9474;
        break;
    case 42:
        mId = 14374;
        break;
    case 43:
        mId = 14376;
        break;
    case 44:
        mId = 14377;
        break;
    case 45:
        mId = 2404;
        break;
    case 46:
        mId = 2784;
        break;
    case 47:
        mId = 2787;
        break;
    case 48:
        mId = 2785;
        break;
    case 49:
        mId = 2736;
        break;
    case 50:
        mId = 2786;
        break;
    case 51:
        mId = 14347;
        break;
    case 52:
        mId = 14346;
        break;
    case 53:
        mId = 14576;
        break;
    case 54:
        mId = 9695;
        break;
    case 55:
        mId = 9991;
        break;
    case 56:
        mId = 6448;
        break;
    case 57:
        mId = 6444;
        break;
    case 58:
        mId = 6080;
        break;
    case 59:
        mId = 6447;
        break;
    case 60:
        mId = 4805;
        break;
    case 61:
        mId = 9714;
        break;
    case 62:
        mId = 6448;
        break;
    case 63:
        mId = 6442;
        break;
    case 64:
        mId = 14632;
        break;
    case 65:
        mId = 14332;
        break;
    case 66:
        mId = 14331;
        break;
    case 67:
        mId = 8469;
        break;
    case 68:
        mId = 2830;
        break;
    case 69:
        mId = 2346;
        break;
    default:
        SendSysMessage(LANG_NO_MOUNT);
        SetSentErrorMessage(true);
        return false;
    }

    Player* chr = getSelectedPlayer();
    if (!chr)
    {
        SendSysMessage(LANG_NO_CHAR_SELECTED);
        SetSentErrorMessage(true);
        return false;
    }

    // check online security
    if (HasLowerSecurity(chr))
        return false;

    PSendSysMessage(LANG_YOU_GIVE_MOUNT, GetNameLink(chr).c_str());
    if (needReportToTarget(chr))
        ChatHandler(chr).PSendSysMessage(
            LANG_MOUNT_GIVED, GetNameLink().c_str());

    chr->SetUInt32Value(UNIT_FIELD_FLAGS, UNIT_FLAG_PVP);
    chr->Mount(mId);

    WorldPacket data(SMSG_FORCE_RUN_SPEED_CHANGE, (8 + 4 + 1 + 4));
    data << chr->GetPackGUID();
    data << (uint32)0;
    data << (uint8)0; // new 2.1.0
    data << float(speed);
    chr->SendMessageToSet(&data, true);

    data.initialize(SMSG_FORCE_SWIM_SPEED_CHANGE, (8 + 4 + 4));
    data << chr->GetPackGUID();
    data << (uint32)0;
    data << float(speed);
    chr->SendMessageToSet(&data, true);

    return true;
}

// Edit Player money
bool ChatHandler::HandleModifyMoneyCommand(char* args)
{
    if (!*args)
        return false;

    Player* chr = getSelectedPlayer();
    if (chr == nullptr)
    {
        SendSysMessage(LANG_NO_CHAR_SELECTED);
        SetSentErrorMessage(true);
        return false;
    }

    // check online security
    if (HasLowerSecurity(chr))
        return false;

    int32 copper;
    if (!ExtractCopper(&args, copper, true))
        return true; // ExtractCopper sends usage error

    uint32 chr_money = chr->storage().money().get();

    if (copper > 0)
    {
        inventory::transaction trans;
        trans.add(copper);
        if (chr->storage().finalize(trans))
        {
            PSendSysMessage(
                LANG_YOU_GIVE_MONEY, copper, GetNameLink(chr).c_str());
            if (needReportToTarget(chr))
                ChatHandler(chr).PSendSysMessage(
                    LANG_YOURS_MONEY_GIVEN, GetNameLink().c_str(), copper);
        }
    }
    else if (copper < 0)
    {
        copper = std::abs(copper);
        if (chr_money < static_cast<uint32>(copper))
            copper = chr_money;

        inventory::transaction trans;
        trans.remove(copper);
        if (chr->storage().finalize(trans))
        {
            if (static_cast<uint32>(copper) == chr_money)
            {
                PSendSysMessage(
                    LANG_YOU_TAKE_ALL_MONEY, GetNameLink(chr).c_str());
                if (needReportToTarget(chr))
                    ChatHandler(chr).PSendSysMessage(
                        LANG_YOURS_ALL_MONEY_GONE, GetNameLink().c_str());
            }
            else
            {
                PSendSysMessage(
                    LANG_YOU_TAKE_MONEY, copper, GetNameLink(chr).c_str());
                if (needReportToTarget(chr))
                    ChatHandler(chr).PSendSysMessage(
                        LANG_YOURS_MONEY_TAKEN, GetNameLink().c_str(), copper);
            }
        }
    }

    return true;
}

bool ChatHandler::HandleModifyHonorCommand(char* args)
{
    if (!*args)
        return false;

    Player* target = getSelectedPlayer();
    if (!target)
    {
        SendSysMessage(LANG_PLAYER_NOT_FOUND);
        SetSentErrorMessage(true);
        return false;
    }

    // check online security
    if (HasLowerSecurity(target))
        return false;

    int32 amount = (int32)atoi(args);

    target->ModifyHonorPoints(amount);

    PSendSysMessage(LANG_COMMAND_MODIFY_HONOR, GetNameLink(target).c_str(),
        target->GetHonorPoints());

    return true;
}

bool ChatHandler::HandleLookupAreaCommand(char* args)
{
    if (!*args)
        return false;

    std::string namepart = args;
    std::wstring wnamepart;

    if (!Utf8toWStr(namepart, wnamepart))
        return false;

    uint32 counter = 0; // Counter for figure out that we found smth.

    // converting string that we try to find to lower case
    wstrToLower(wnamepart);

    // Search in AreaTable.dbc
    for (uint32 areaflag = 0; areaflag < sAreaStore.GetNumRows(); ++areaflag)
    {
        AreaTableEntry const* areaEntry = sAreaStore.LookupEntry(areaflag);
        if (areaEntry)
        {
            int loc = GetSessionDbcLocale();
            std::string name = areaEntry->area_name[loc];
            if (name.empty())
                continue;

            if (!Utf8FitTo(name, wnamepart))
            {
                loc = 0;
                for (; loc < MAX_LOCALE; ++loc)
                {
                    if (loc == GetSessionDbcLocale())
                        continue;

                    name = areaEntry->area_name[loc];
                    if (name.empty())
                        continue;

                    if (Utf8FitTo(name, wnamepart))
                        break;
                }
            }

            if (loc < MAX_LOCALE)
            {
                // send area in "id - [name]" format
                std::ostringstream ss;
                if (m_session)
                    ss << areaEntry->ID
                       << " - |cffffffff|Harea:" << areaEntry->ID << "|h["
                       << name << " " << localeNames[loc] << "]|h|r";
                else
                    ss << areaEntry->ID << " - " << name << " "
                       << localeNames[loc];

                SendSysMessage(ss.str().c_str());

                ++counter;
            }
        }
    }

    if (counter == 0) // if counter == 0 then we found nth
        SendSysMessage(LANG_COMMAND_NOAREAFOUND);

    return true;
}

bool ChatHandler::HandleLookupTplocCommand(char* args)
{
    if (!*args)
    {
        SendSysMessage(LANG_COMMAND_TELE_PARAMETER);
        SetSentErrorMessage(true);
        return false;
    }

    std::string namepart = args;
    std::wstring wnamepart;

    if (!Utf8toWStr(namepart, wnamepart))
        return false;

    // converting string that we try to find to lower case
    wstrToLower(wnamepart);

    std::ostringstream reply;

    GameTeleMap const& teleMap = sObjectMgr::Instance()->GetGameTeleMap();
    for (const auto& elem : teleMap)
    {
        GameTele const* tele = &elem.second;

        if (tele->wnameLow.find(wnamepart) == std::wstring::npos)
            continue;

        if (m_session)
            reply << "  |cffffffff|Htele:" << elem.first << "|h[" << tele->name
                  << "]|h|r\n";
        else
            reply << "  " << elem.first << " " << tele->name << "\n";
    }

    if (reply.str().empty())
        SendSysMessage(LANG_COMMAND_TELE_NOLOCATION);
    else
        PSendSysMessage(LANG_COMMAND_TELE_LOCATION, reply.str().c_str());

    return true;
}

// Save all players in the world
bool ChatHandler::HandleSaveAllCommand(char* /*args*/)
{
    sObjectAccessor::Instance()->SaveAllPlayers();
    SendSysMessage(LANG_PLAYERS_SAVED);
    return true;
}

// Send mail by command
bool ChatHandler::HandleSendMailCommand(char* args)
{
    // format: name "subject text" "mail text"
    Player* target;
    ObjectGuid target_guid;
    std::string target_name;
    if (!ExtractPlayerTarget(&args, &target, &target_guid, &target_name))
        return false;

    MailDraft draft;

    // fill draft
    if (!HandleSendMailHelper(draft, args))
        return false;

    // from console show nonexistent sender
    MailSender sender(MAIL_NORMAL,
        m_session ? m_session->GetPlayer()->GetObjectGuid().GetCounter() : 0,
        MAIL_STATIONERY_GM);

    draft.SendMailTo(MailReceiver(target, target_guid), sender);

    std::string nameLink = playerLink(target_name);
    PSendSysMessage(LANG_MAIL_SENT, nameLink.c_str());
    return true;
}

bool ChatHandler::HandleModifyDrunkCommand(char* args)
{
    if (!*args)
        return false;

    uint32 drunklevel = (uint32)atoi(args);
    if (drunklevel > 100)
        drunklevel = 100;

    uint16 drunkMod = drunklevel * 0xFFFF / 100;

    m_session->GetPlayer()->SetDrunkValue(drunkMod);

    return true;
}

bool ChatHandler::HandleSetViewCommand(char* /*args*/)
{
    if (Unit* unit = getSelectedUnit())
        m_session->GetPlayer()->GetCamera().SetView(unit);
    else
    {
        PSendSysMessage(LANG_SELECT_CHAR_OR_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    return true;
}

bool ChatHandler::HandleSaveCommand(char* /*args*/)
{
    Player* player = m_session->GetPlayer();

    // save GM account without delay and output message (testing, etc)
    if (GetAccessLevel() > SEC_PLAYER)
    {
        player->SaveToDB();
        SendSysMessage(LANG_PLAYER_SAVED);
        return true;
    }

    // save or plan save after 20 sec (logout delay) if current next save time
    // more this value and _not_ output any messages to prevent cheat planning
    uint32 save_interval =
        sWorld::Instance()->getConfig(CONFIG_UINT32_INTERVAL_SAVE);
    if (save_interval == 0 ||
        (save_interval > 20 * IN_MILLISECONDS &&
            player->GetSaveTimer() <= save_interval - 20 * IN_MILLISECONDS))
        player->SaveToDB();

    return true;
}
