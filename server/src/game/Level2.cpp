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

#include "AccountMgr.h"
#include "Chat.h"
#include "Common.h"
#include "CreatureAI.h"
#include "CreatureGroupMgr.h"
#include "DBCStores.h"
#include "Formulas.h"
#include "GameEventMgr.h"
#include "GameObject.h"
#include "GameObjectModel.h"
#include "Item.h"
#include "Language.h"
#include "MapManager.h"
#include "MapPersistentStateMgr.h"
#include "ObjectAccessor.h"
#include "ObjectGuid.h"
#include "ObjectMgr.h"
#include "Opcodes.h"
#include "Pet.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SpellMgr.h"
#include "TemporarySummon.h"
#include "Totem.h"
#include "Transport.h"
#include "Util.h"
#include "World.h"
#include "Database/DatabaseEnv.h"
#include "MoveMap.h"    // for mmap manager
#include "PathFinder.h" // for mmap commands
#include "movement/TargetedMovementGenerator.h"
#include "movement/WaypointManager.h"
#include <boost/algorithm/string/find.hpp>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <map>
#include <typeinfo>

static uint32 ReputationRankStrIndex[MAX_REPUTATION_RANK] = {LANG_REP_HATED,
    LANG_REP_HOSTILE, LANG_REP_UNFRIENDLY, LANG_REP_NEUTRAL, LANG_REP_FRIENDLY,
    LANG_REP_HONORED, LANG_REP_REVERED, LANG_REP_EXALTED};

// mute player for some times
bool ChatHandler::HandleMuteCommand(char* args)
{
    char* nameStr = ExtractOptNotLastArg(&args);

    Player* target;
    ObjectGuid target_guid;
    std::string target_name;
    if (!ExtractPlayerTarget(&nameStr, &target, &target_guid, &target_name))
        return false;

    uint32 notspeaktime;
    if (!ExtractUInt32(&args, notspeaktime))
        return false;

    uint32 account_id =
        target ? target->GetSession()->GetAccountId() :
                 sObjectMgr::Instance()->GetPlayerAccountIdByGUID(target_guid);

    // find only player from same account if any
    if (!target)
    {
        if (WorldSession* session = sWorld::Instance()->FindSession(account_id))
            target = session->GetPlayer();
    }

    // must have strong lesser security level
    if (HasLowerSecurity(target, target_guid, true))
        return false;

    time_t mutetime = WorldTimer::time_no_syscall() + notspeaktime * 60;

    if (target)
        target->GetSession()->m_muteTime = mutetime;

    LoginDatabase.PExecute("UPDATE account SET mutetime = " UI64FMTD
                           " WHERE id = '%u'",
        uint64(mutetime), account_id);

    if (target)
        ChatHandler(target).PSendSysMessage(
            LANG_YOUR_CHAT_DISABLED, notspeaktime);

    std::string nameLink = playerLink(target_name);

    PSendSysMessage(LANG_YOU_DISABLE_CHAT, nameLink.c_str(), notspeaktime);
    return true;
}

// unmute player
bool ChatHandler::HandleUnmuteCommand(char* args)
{
    Player* target;
    ObjectGuid target_guid;
    std::string target_name;
    if (!ExtractPlayerTarget(&args, &target, &target_guid, &target_name))
        return false;

    uint32 account_id =
        target ? target->GetSession()->GetAccountId() :
                 sObjectMgr::Instance()->GetPlayerAccountIdByGUID(target_guid);

    // find only player from same account if any
    if (!target)
    {
        if (WorldSession* session = sWorld::Instance()->FindSession(account_id))
            target = session->GetPlayer();
    }

    // must have strong lesser security level
    if (HasLowerSecurity(target, target_guid, true))
        return false;

    if (target)
    {
        if (target->CanSpeak())
        {
            SendSysMessage(LANG_CHAT_ALREADY_ENABLED);
            SetSentErrorMessage(true);
            return false;
        }

        target->GetSession()->m_muteTime = 0;
    }

    LoginDatabase.PExecute(
        "UPDATE account SET mutetime = '0' WHERE id = '%u'", account_id);

    if (target)
        ChatHandler(target).PSendSysMessage(LANG_YOUR_CHAT_ENABLED);

    std::string nameLink = playerLink(target_name);

    PSendSysMessage(LANG_YOU_ENABLE_CHAT, nameLink.c_str());
    return true;
}

void ChatHandler::ShowTriggerTargetListHelper(
    uint32 id, AreaTrigger const* at, bool subpart /*= false*/)
{
    if (m_session)
    {
        char dist_buf[50];
        if (!subpart)
        {
            float dist = m_session->GetPlayer()->GetDistance2d(
                at->target_X, at->target_Y);
            snprintf(dist_buf, 50, GetMangosString(LANG_TRIGGER_DIST), dist);
        }
        else
            dist_buf[0] = '\0';

        PSendSysMessage(LANG_TRIGGER_TARGET_LIST_CHAT, subpart ? " -> " : "",
            id, id, at->target_mapId, at->target_X, at->target_Y, at->target_Z,
            dist_buf);
    }
    else
        PSendSysMessage(LANG_TRIGGER_TARGET_LIST_CONSOLE, subpart ? " -> " : "",
            id, at->target_mapId, at->target_X, at->target_Y, at->target_Z);
}

void ChatHandler::ShowTriggerListHelper(AreaTriggerEntry const* atEntry)
{
    char const* tavern =
        sObjectMgr::Instance()->IsTavernAreaTrigger(atEntry->id) ?
            GetMangosString(LANG_TRIGGER_TAVERN) :
            "";
    char const* quest =
        sObjectMgr::Instance()->GetQuestForAreaTrigger(atEntry->id) ?
            GetMangosString(LANG_TRIGGER_QUEST) :
            "";

    if (m_session)
    {
        float dist =
            m_session->GetPlayer()->GetDistance2d(atEntry->x, atEntry->y);
        char dist_buf[50];
        snprintf(dist_buf, 50, GetMangosString(LANG_TRIGGER_DIST), dist);

        PSendSysMessage(LANG_TRIGGER_LIST_CHAT, atEntry->id, atEntry->id,
            atEntry->mapid, atEntry->x, atEntry->y, atEntry->z, dist_buf,
            tavern, quest);
    }
    else
        PSendSysMessage(LANG_TRIGGER_LIST_CONSOLE, atEntry->id, atEntry->mapid,
            atEntry->x, atEntry->y, atEntry->z, tavern, quest);

    if (AreaTrigger const* at =
            sObjectMgr::Instance()->GetAreaTrigger(atEntry->id))
        ShowTriggerTargetListHelper(atEntry->id, at, true);
}

bool ChatHandler::HandleTriggerCommand(char* args)
{
    AreaTriggerEntry const* atEntry = nullptr;

    Player* pl = m_session ? m_session->GetPlayer() : nullptr;

    // select by args
    if (*args)
    {
        uint32 atId;
        if (!ExtractUint32KeyFromLink(&args, "Hareatrigger", atId))
            return false;

        if (!atId)
            return false;

        atEntry = sAreaTriggerStore.LookupEntry(atId);

        if (!atEntry)
        {
            PSendSysMessage(LANG_COMMAND_GOAREATRNOTFOUND, atId);
            SetSentErrorMessage(true);
            return false;
        }
    }
    // find nearest
    else
    {
        if (!m_session)
            return false;

        float dist2 = 100 * 100;

        Player* pl = m_session->GetPlayer();

        // Search triggers
        for (uint32 id = 0; id < sAreaTriggerStore.GetNumRows(); ++id)
        {
            AreaTriggerEntry const* atTestEntry =
                sAreaTriggerStore.LookupEntry(id);
            if (!atTestEntry)
                continue;

            if (atTestEntry->mapid != m_session->GetPlayer()->GetMapId())
                continue;

            float dx = atTestEntry->x - pl->GetX();
            float dy = atTestEntry->y - pl->GetY();

            float test_dist2 = dx * dx + dy * dy;

            if (test_dist2 >= dist2)
                continue;

            dist2 = test_dist2;
            atEntry = atTestEntry;
        }

        if (!atEntry)
        {
            SendSysMessage(LANG_COMMAND_NOTRIGGERFOUND);
            SetSentErrorMessage(true);
            return false;
        }
    }

    ShowTriggerListHelper(atEntry);

    int loc_idx = GetSessionDbLocaleIndex();

    AreaTrigger const* at = sObjectMgr::Instance()->GetAreaTrigger(atEntry->id);
    if (at)
        PSendSysMessage(LANG_TRIGGER_REQ_LEVEL, at->requiredLevel);

    if (uint32 quest_id =
            sObjectMgr::Instance()->GetQuestForAreaTrigger(atEntry->id))
    {
        SendSysMessage(LANG_TRIGGER_EXPLORE_QUEST);
        ShowQuestListHelper(quest_id, loc_idx, pl);
    }

    if (at)
    {
        if (at->requiredQuest)
        {
            SendSysMessage(LANG_TRIGGER_REQ_QUEST);
            ShowQuestListHelper(at->requiredQuest, loc_idx, pl);
        }
    }

    return true;
}

bool ChatHandler::HandleTriggerActiveCommand(char* /*args*/)
{
    uint32 counter = 0; // Counter for figure out that we found smth.

    Player* pl = m_session->GetPlayer();

    // Search in AreaTable.dbc
    for (uint32 id = 0; id < sAreaTriggerStore.GetNumRows(); ++id)
    {
        AreaTriggerEntry const* atEntry = sAreaTriggerStore.LookupEntry(id);
        if (!atEntry)
            continue;

        if (!IsPointInAreaTriggerZone(
                atEntry, pl->GetMapId(), pl->GetX(), pl->GetY(), pl->GetZ()))
            continue;

        ShowTriggerListHelper(atEntry);

        ++counter;
    }

    if (counter == 0) // if counter == 0 then we found nth
        SendSysMessage(LANG_COMMAND_NOTRIGGERFOUND);

    return true;
}

bool ChatHandler::HandleTriggerNearCommand(char* args)
{
    float distance = (!*args) ? 10.0f : (float)atof(args);
    float dist2 = distance * distance;
    uint32 counter = 0; // Counter for figure out that we found smth.

    Player* pl = m_session->GetPlayer();

    // Search triggers
    for (uint32 id = 0; id < sAreaTriggerStore.GetNumRows(); ++id)
    {
        AreaTriggerEntry const* atEntry = sAreaTriggerStore.LookupEntry(id);
        if (!atEntry)
            continue;

        if (atEntry->mapid != m_session->GetPlayer()->GetMapId())
            continue;

        float dx = atEntry->x - pl->GetX();
        float dy = atEntry->y - pl->GetY();

        if (dx * dx + dy * dy > dist2)
            continue;

        ShowTriggerListHelper(atEntry);

        ++counter;
    }

    // Search trigger targets
    for (uint32 id = 0; id < sAreaTriggerStore.GetNumRows(); ++id)
    {
        AreaTriggerEntry const* atEntry = sAreaTriggerStore.LookupEntry(id);
        if (!atEntry)
            continue;

        AreaTrigger const* at =
            sObjectMgr::Instance()->GetAreaTrigger(atEntry->id);
        if (!at)
            continue;

        if (at->target_mapId != m_session->GetPlayer()->GetMapId())
            continue;

        float dx = at->target_X - pl->GetX();
        float dy = at->target_Y - pl->GetY();

        if (dx * dx + dy * dy > dist2)
            continue;

        ShowTriggerTargetListHelper(atEntry->id, at);

        ++counter;
    }

    if (counter == 0) // if counter == 0 then we found nth
        SendSysMessage(LANG_COMMAND_NOTRIGGERFOUND);

    return true;
}

bool ChatHandler::HandleGameObjectTargetCommand(char* args)
{
    Player* pl = m_session->GetPlayer();
    QueryResult* result;
    GameEventMgr::ActiveEvents const& activeEventsList =
        sGameEventMgr::Instance()->GetActiveEventList();
    if (*args)
    {
        // number or [name] Shift-click form
        // |color|Hgameobject_entry:go_id|h[name]|h|r
        char* cId = ExtractKeyFromLink(&args, "Hgameobject_entry");
        if (!cId)
            return false;

        uint32 id;
        if (ExtractUInt32(&cId, id))
        {
            result = WorldDatabase.PQuery(
                "SELECT guid, id, position_x, position_y, position_z, "
                "orientation, map, (POW(position_x - '%f', 2) + POW(position_y "
                "- '%f', 2) + POW(position_z - '%f', 2)) AS order_ FROM "
                "gameobject WHERE map = '%i' AND id = '%u' ORDER BY order_ ASC "
                "LIMIT 1",
                pl->GetX(), pl->GetY(), pl->GetZ(), pl->GetMapId(), id);
        }
        else
        {
            std::string name = cId;
            WorldDatabase.escape_string(name);
            result = WorldDatabase.PQuery(
                "SELECT guid, id, position_x, position_y, position_z, "
                "orientation, map, (POW(position_x - %f, 2) + POW(position_y - "
                "%f, 2) + POW(position_z - %f, 2)) AS order_ "
                "FROM gameobject,gameobject_template WHERE "
                "gameobject_template.entry = gameobject.id AND map = %i AND "
                "name " _LIKE_ " " _CONCAT3_(
                    "'%%'", "'%s'", "'%%'") " ORDER BY order_ ASC LIMIT 1",
                pl->GetX(), pl->GetY(), pl->GetZ(), pl->GetMapId(),
                name.c_str());
        }
    }
    else
    {
        std::ostringstream eventFilter;
        eventFilter << " AND (event IS NULL ";
        bool initString = true;

        for (const auto& elem : activeEventsList)
        {
            if (initString)
            {
                eventFilter << "OR event IN (" << elem;
                initString = false;
            }
            else
                eventFilter << "," << elem;
        }

        if (!initString)
            eventFilter << "))";
        else
            eventFilter << ")";

        result = WorldDatabase.PQuery(
            "SELECT gameobject.guid, id, position_x, position_y, position_z, "
            "orientation, map, "
            "(POW(position_x - %f, 2) + POW(position_y - %f, 2) + "
            "POW(position_z - %f, 2)) AS order_ FROM gameobject "
            "LEFT OUTER JOIN game_event_gameobject on "
            "gameobject.guid=game_event_gameobject.guid WHERE map = '%i' %s "
            "ORDER BY order_ ASC LIMIT 10",
            m_session->GetPlayer()->GetX(), m_session->GetPlayer()->GetY(),
            m_session->GetPlayer()->GetZ(), m_session->GetPlayer()->GetMapId(),
            eventFilter.str().c_str());
    }

    if (!result)
    {
        SendSysMessage(LANG_COMMAND_TARGETOBJNOTFOUND);
        return true;
    }

    bool found = false;
    float x, y, z, o;
    uint32 lowguid, id, pool_id;
    uint16 mapid;

    do
    {
        Field* fields = result->Fetch();
        lowguid = fields[0].GetUInt32();
        id = fields[1].GetUInt32();
        x = fields[2].GetFloat();
        y = fields[3].GetFloat();
        z = fields[4].GetFloat();
        o = fields[5].GetFloat();
        mapid = fields[6].GetUInt16();
        pool_id = sPoolMgr::Instance()->IsPartOfAPool<GameObject>(lowguid);
        if (!pool_id || (pl->GetMap()->GetPersistentState() &&
                            pl->GetMap()
                                ->GetPersistentState()
                                ->IsSpawnedPoolObject<GameObject>(lowguid)))
            found = true;
    } while (result->NextRow() && (!found));

    delete result;

    if (!found)
    {
        PSendSysMessage(LANG_GAMEOBJECT_NOT_EXIST, id);
        return false;
    }

    GameObjectInfo const* goI = ObjectMgr::GetGameObjectInfo(id);

    if (!goI)
    {
        PSendSysMessage(LANG_GAMEOBJECT_NOT_EXIST, id);
        return false;
    }

    GameObject* target = m_session->GetPlayer()->GetMap()->GetGameObject(
        ObjectGuid(HIGHGUID_GAMEOBJECT, id, lowguid));

    PSendSysMessage(LANG_GAMEOBJECT_DETAIL, lowguid, goI->name, lowguid, id, x,
        y, z, mapid, o);

    if (target)
    {
        time_t curRespawnDelay =
            target->GetRespawnTimeEx() - WorldTimer::time_no_syscall();
        if (curRespawnDelay < 0)
            curRespawnDelay = 0;

        std::string curRespawnDelayStr =
            secsToTimeString(curRespawnDelay, true);
        std::string defRespawnDelayStr =
            secsToTimeString(target->GetRespawnDelay(), true);

        PSendSysMessage(LANG_COMMAND_RAWPAWNTIMES, defRespawnDelayStr.c_str(),
            curRespawnDelayStr.c_str());

        ShowNpcOrGoSpawnInformation<GameObject>(target->GetGUIDLow());
    }
    return true;
}

// delete object by selection or guid
bool ChatHandler::HandleGameObjectDeleteCommand(char* args)
{
    // number or [name] Shift-click form |color|Hgameobject:go_guid|h[name]|h|r
    uint32 lowguid;
    if (!ExtractUint32KeyFromLink(&args, "Hgameobject", lowguid))
        return false;

    if (!lowguid)
        return false;

    GameObject* obj = nullptr;

    // by DB guid
    if (GameObjectData const* go_data =
            sObjectMgr::Instance()->GetGOData(lowguid))
        obj = GetGameObjectWithGuid(lowguid, go_data->id);

    if (!obj)
    {
        PSendSysMessage(LANG_COMMAND_OBJNOTFOUND, lowguid);
        SetSentErrorMessage(true);
        return false;
    }

    if (ObjectGuid ownerGuid = obj->GetOwnerGuid())
    {
        Unit* owner =
            ObjectAccessor::GetUnit(*m_session->GetPlayer(), ownerGuid);
        if (!owner || !ownerGuid.IsPlayer())
        {
            PSendSysMessage(LANG_COMMAND_DELOBJREFERCREATURE, obj->GetGUIDLow(),
                ownerGuid.GetString().c_str());
            SetSentErrorMessage(true);
            return false;
        }

        owner->RemoveGameObject(obj, false);
    }

    obj->SetRespawnTime(0); // not save respawn time
    obj->Delete();
    obj->DeleteFromDB();

    PSendSysMessage(LANG_COMMAND_DELOBJMESSAGE, obj->GetGUIDLow());

    return true;
}

// turn selected object
bool ChatHandler::HandleGameObjectTurnCommand(char* args)
{
    // number or [name] Shift-click form |color|Hgameobject:go_id|h[name]|h|r
    uint32 lowguid;
    if (!ExtractUint32KeyFromLink(&args, "Hgameobject", lowguid))
        return false;

    if (!lowguid)
        return false;

    GameObject* obj = nullptr;

    // by DB guid
    if (GameObjectData const* go_data =
            sObjectMgr::Instance()->GetGOData(lowguid))
        obj = GetGameObjectWithGuid(lowguid, go_data->id);

    if (!obj)
    {
        PSendSysMessage(LANG_COMMAND_OBJNOTFOUND, lowguid);
        SetSentErrorMessage(true);
        return false;
    }

    float o;
    if (!ExtractOptFloat(&args, o, m_session->GetPlayer()->GetO()))
        return false;

    Map* map = obj->GetMap();
    map->erase(obj, false);

    obj->Relocate(obj->GetX(), obj->GetY(), obj->GetZ());
    obj->SetOrientation(o);
    obj->UpdateRotationFields();

    if (!map->insert(obj))
    {
        SendSysMessage("Map insert failed");
        delete obj;
        return true;
    }

    obj->SaveToDB();
    obj->Refresh();

    PSendSysMessage(LANG_COMMAND_TURNOBJMESSAGE, obj->GetGUIDLow(),
        obj->GetGOInfo()->name, obj->GetGUIDLow());

    return true;
}

// move selected object
bool ChatHandler::HandleGameObjectMoveCommand(char* args)
{
    // number or [name] Shift-click form |color|Hgameobject:go_guid|h[name]|h|r
    uint32 lowguid;
    if (!ExtractUint32KeyFromLink(&args, "Hgameobject", lowguid))
        return false;

    if (!lowguid)
        return false;

    GameObject* obj = nullptr;

    // by DB guid
    if (GameObjectData const* go_data =
            sObjectMgr::Instance()->GetGOData(lowguid))
        obj = GetGameObjectWithGuid(lowguid, go_data->id);

    if (!obj)
    {
        PSendSysMessage(LANG_COMMAND_OBJNOTFOUND, lowguid);
        SetSentErrorMessage(true);
        return false;
    }

    if (!*args)
    {
        Player* chr = m_session->GetPlayer();

        Map* map = obj->GetMap();
        map->erase(obj, false);

        obj->Relocate(chr->GetX(), chr->GetY(), chr->GetZ());
        obj->SetFloatValue(GAMEOBJECT_POS_X, chr->GetX());
        obj->SetFloatValue(GAMEOBJECT_POS_Y, chr->GetY());
        obj->SetFloatValue(GAMEOBJECT_POS_Z, chr->GetZ());

        if (!map->insert(obj))
        {
            SendSysMessage("Map insert failed");
            delete obj;
            return true;
        }
    }
    else
    {
        float x;
        if (!ExtractFloat(&args, x))
            return false;

        float y;
        if (!ExtractFloat(&args, y))
            return false;

        float z;
        if (!ExtractFloat(&args, z))
            return false;

        if (!maps::verify_coords(x, y))
        {
            PSendSysMessage(LANG_INVALID_TARGET_COORD, x, y, obj->GetMapId());
            SetSentErrorMessage(true);
            return false;
        }

        Map* map = obj->GetMap();
        map->erase(obj, false);

        obj->Relocate(x, y, z);
        obj->SetFloatValue(GAMEOBJECT_POS_X, x);
        obj->SetFloatValue(GAMEOBJECT_POS_Y, y);
        obj->SetFloatValue(GAMEOBJECT_POS_Z, z);

        if (!map->insert(obj))
        {
            SendSysMessage("Failed to insert into map");
            delete obj;
            return true;
        }
    }

    obj->SaveToDB();
    obj->Refresh();

    PSendSysMessage(LANG_COMMAND_MOVEOBJMESSAGE, obj->GetGUIDLow(),
        obj->GetGOInfo()->name, obj->GetGUIDLow());

    return true;
}

// spawn go
bool ChatHandler::HandleGameObjectAddCommand(char* args)
{
    // number or [name] Shift-click form
    // |color|Hgameobject_entry:go_id|h[name]|h|r
    uint32 id;
    if (!ExtractUint32KeyFromLink(&args, "Hgameobject_entry", id))
        return false;

    if (!id)
        return false;

    int32 spawntimeSecs;
    if (!ExtractOptInt32(&args, spawntimeSecs, 0))
        return false;

    const GameObjectInfo* gInfo = ObjectMgr::GetGameObjectInfo(id);

    if (!gInfo)
    {
        PSendSysMessage(LANG_GAMEOBJECT_NOT_EXIST, id);
        SetSentErrorMessage(true);
        return false;
    }

    if (gInfo->displayId &&
        !sGameObjectDisplayInfoStore.LookupEntry(gInfo->displayId))
    {
        // report to DB errors log as in loading case
        logging.error(
            "Gameobject (Entry %u GoType: %u) have invalid displayId (%u), not "
            "spawned.",
            id, gInfo->type, gInfo->displayId);
        PSendSysMessage(LANG_GAMEOBJECT_HAVE_INVALID_DATA, id);
        SetSentErrorMessage(true);
        return false;
    }

    Player* chr = m_session->GetPlayer();
    float x = float(chr->GetX());
    float y = float(chr->GetY());
    float z = float(chr->GetZ());
    float o = float(chr->GetO());
    Map* map = chr->GetMap();

    auto pGameObj = new GameObject;

    // used guids from specially reserved range (can be 0 if no free values)
    uint32 db_lowGUID =
        sObjectMgr::Instance()->GenerateStaticGameObjectLowGuid();
    if (!db_lowGUID)
    {
        SendSysMessage(LANG_NO_FREE_STATIC_GUID_FOR_SPAWN);
        SetSentErrorMessage(true);
        return false;
    }

    if (!pGameObj->Create(db_lowGUID, gInfo->id, map, x, y, z, o))
    {
        delete pGameObj;
        return false;
    }

    if (spawntimeSecs)
        pGameObj->SetRespawnTime(spawntimeSecs);

    // fill the gameobject data and save to the db
    pGameObj->SaveToDB(map->GetId(), (1 << map->GetSpawnMode()));

    // this will generate a new guid if the object is in an instance
    if (!pGameObj->LoadFromDB(db_lowGUID, map))
    {
        delete pGameObj;
        return false;
    }

    LOG_DEBUG(logging, GetMangosString(LANG_GAMEOBJECT_CURRENT), gInfo->name,
        db_lowGUID, x, y, z, o);

    if (!map->insert(pGameObj))
    {
        SendSysMessage("Failed to insert into map");
        delete pGameObj;
        return true;
    }

    sObjectMgr::Instance()->add_static_game_object(
        sObjectMgr::Instance()->GetGOData(db_lowGUID));

    PSendSysMessage(LANG_GAMEOBJECT_ADD, id, gInfo->name, db_lowGUID, x, y, z);
    return true;
}

bool ChatHandler::HandleGameObjectNearCommand(char* args)
{
    float distance;
    if (!ExtractOptFloat(&args, distance, 10.0f))
        return false;

    uint32 count = 0;

    Player* pl = m_session->GetPlayer();
    std::unique_ptr<QueryResult> result(WorldDatabase.PQuery(
        "SELECT guid, id, position_x, position_y, position_z, map, "
        "(POW(position_x - '%f', 2) + POW(position_y - '%f', 2) + "
        "POW(position_z - '%f', 2)) AS order_ "
        "FROM gameobject WHERE map='%u' AND (POW(position_x - '%f', 2) + "
        "POW(position_y - '%f', 2) + POW(position_z - '%f', 2)) <= '%f' ORDER "
        "BY order_",
        pl->GetX(), pl->GetY(), pl->GetZ(), pl->GetMapId(), pl->GetX(),
        pl->GetY(), pl->GetZ(), distance * distance));

    if (result)
    {
        do
        {
            Field* fields = result->Fetch();
            uint32 guid = fields[0].GetUInt32();
            uint32 entry = fields[1].GetUInt32();
            float x = fields[2].GetFloat();
            float y = fields[3].GetFloat();
            float z = fields[4].GetFloat();
            int mapid = fields[5].GetUInt16();

            GameObjectInfo const* gInfo = ObjectMgr::GetGameObjectInfo(entry);

            if (!gInfo)
                continue;

            PSendSysMessage(LANG_GO_MIXED_LIST_CHAT, guid,
                PrepareStringNpcOrGoSpawnInformation<GameObject>(guid).c_str(),
                entry, guid, gInfo->name, x, y, z, mapid);

            ++count;
        } while (result->NextRow());
    }

    PSendSysMessage(LANG_COMMAND_NEAROBJMESSAGE, distance, count);
    return true;
}

bool ChatHandler::HandleGUIDCommand(char* /*args*/)
{
    ObjectGuid guid = m_session->GetPlayer()->GetSelectionGuid();

    if (!guid)
    {
        SendSysMessage(LANG_NO_SELECTION);
        SetSentErrorMessage(true);
        return false;
    }

    PSendSysMessage(LANG_OBJECT_GUID, guid.GetString().c_str());
    return true;
}

void ChatHandler::ShowFactionListHelper(FactionEntry const* factionEntry,
    LocaleConstant loc, FactionState const* repState /*= NULL*/,
    Player* target /*= NULL */)
{
    std::string name = factionEntry->name[loc];
    // send faction in "id - [faction] rank reputation [visible] [at war] [own
    // team] [unknown] [invisible] [inactive]" format
    // or              "id - [faction] [no reputation]" format
    std::ostringstream ss;
    if (m_session)
        ss << factionEntry->ID << " - |cffffffff|Hfaction:" << factionEntry->ID
           << "|h[" << name << " " << localeNames[loc] << "]|h|r";
    else
        ss << factionEntry->ID << " - " << name << " " << localeNames[loc];

    if (repState) // and then target!=NULL also
    {
        ReputationRank rank = target->GetReputationMgr().GetRank(factionEntry);
        std::string rankName = GetMangosString(ReputationRankStrIndex[rank]);

        ss << " " << rankName << "|h|r ("
           << target->GetReputationMgr().GetReputation(factionEntry) << ")";

        if (repState->Flags & FACTION_FLAG_VISIBLE)
            ss << GetMangosString(LANG_FACTION_VISIBLE);
        if (repState->Flags & FACTION_FLAG_AT_WAR)
            ss << GetMangosString(LANG_FACTION_ATWAR);
        if (repState->Flags & FACTION_FLAG_PEACE_FORCED)
            ss << GetMangosString(LANG_FACTION_PEACE_FORCED);
        if (repState->Flags & FACTION_FLAG_HIDDEN)
            ss << GetMangosString(LANG_FACTION_HIDDEN);
        if (repState->Flags & FACTION_FLAG_INVISIBLE_FORCED)
            ss << GetMangosString(LANG_FACTION_INVISIBLE_FORCED);
        if (repState->Flags & FACTION_FLAG_INACTIVE)
            ss << GetMangosString(LANG_FACTION_INACTIVE);
    }
    else if (target)
        ss << GetMangosString(LANG_FACTION_NOREPUTATION);

    SendSysMessage(ss.str().c_str());
}

bool ChatHandler::HandleLookupFactionCommand(char* args)
{
    if (!*args)
        return false;

    // Can be NULL at console call
    Player* target = getSelectedPlayer();

    std::string namepart = args;
    std::wstring wnamepart;

    if (!Utf8toWStr(namepart, wnamepart))
        return false;

    // converting string that we try to find to lower case
    wstrToLower(wnamepart);

    uint32 counter = 0; // Counter for figure out that we found smth.

    for (uint32 id = 0; id < sFactionStore.GetNumRows(); ++id)
    {
        FactionEntry const* factionEntry = sFactionStore.LookupEntry(id);
        if (factionEntry)
        {
            int loc = GetSessionDbcLocale();
            std::string name = factionEntry->name[loc];
            if (name.empty())
                continue;

            if (!Utf8FitTo(name, wnamepart))
            {
                loc = 0;
                for (; loc < MAX_LOCALE; ++loc)
                {
                    if (loc == GetSessionDbcLocale())
                        continue;

                    name = factionEntry->name[loc];
                    if (name.empty())
                        continue;

                    if (Utf8FitTo(name, wnamepart))
                        break;
                }
            }

            if (loc < MAX_LOCALE)
            {
                FactionState const* repState =
                    target ? target->GetReputationMgr().GetState(factionEntry) :
                             nullptr;
                ShowFactionListHelper(
                    factionEntry, LocaleConstant(loc), repState, target);
                counter++;
            }
        }
    }

    if (counter == 0) // if counter == 0 then we found nth
        SendSysMessage(LANG_COMMAND_FACTION_NOTFOUND);
    return true;
}

bool ChatHandler::HandleModifyRepCommand(char* args)
{
    if (!*args)
        return false;

    Player* target = nullptr;
    target = getSelectedPlayer();

    if (!target)
    {
        SendSysMessage(LANG_PLAYER_NOT_FOUND);
        SetSentErrorMessage(true);
        return false;
    }

    // check online security
    if (HasLowerSecurity(target))
        return false;

    uint32 factionId;
    if (!ExtractUint32KeyFromLink(&args, "Hfaction", factionId))
        return false;

    if (!factionId)
        return false;

    int32 amount = 0;
    if (!ExtractInt32(&args, amount))
    {
        char* rankTxt = ExtractLiteralArg(&args);
        if (!rankTxt)
            return false;

        std::string rankStr = rankTxt;
        std::wstring wrankStr;
        if (!Utf8toWStr(rankStr, wrankStr))
            return false;
        wstrToLower(wrankStr);

        int r = 0;
        amount = -42000;
        for (; r < MAX_REPUTATION_RANK; ++r)
        {
            std::string rank = GetMangosString(ReputationRankStrIndex[r]);
            if (rank.empty())
                continue;

            std::wstring wrank;
            if (!Utf8toWStr(rank, wrank))
                continue;

            wstrToLower(wrank);

            if (wrank.substr(0, wrankStr.size()) == wrankStr)
            {
                int32 delta;
                if (!ExtractOptInt32(&args, delta, 0) || (delta < 0) ||
                    (delta > ReputationMgr::PointsInRank[r] - 1))
                {
                    PSendSysMessage(LANG_COMMAND_FACTION_DELTA,
                        (ReputationMgr::PointsInRank[r] - 1));
                    SetSentErrorMessage(true);
                    return false;
                }
                amount += delta;
                break;
            }
            amount += ReputationMgr::PointsInRank[r];
        }
        if (r >= MAX_REPUTATION_RANK)
        {
            PSendSysMessage(LANG_COMMAND_FACTION_INVPARAM, rankTxt);
            SetSentErrorMessage(true);
            return false;
        }
    }

    FactionEntry const* factionEntry = sFactionStore.LookupEntry(factionId);

    if (!factionEntry)
    {
        PSendSysMessage(LANG_COMMAND_FACTION_UNKNOWN, factionId);
        SetSentErrorMessage(true);
        return false;
    }

    if (factionEntry->reputationListID < 0)
    {
        PSendSysMessage(LANG_COMMAND_FACTION_NOREP_ERROR,
            factionEntry->name[GetSessionDbcLocale()], factionId);
        SetSentErrorMessage(true);
        return false;
    }

    target->GetReputationMgr().SetReputation(factionEntry, amount);
    PSendSysMessage(LANG_COMMAND_MODIFY_REP,
        factionEntry->name[GetSessionDbcLocale()], factionId,
        GetNameLink(target).c_str(),
        target->GetReputationMgr().GetReputation(factionEntry));
    return true;
}

//-----------------------Npc Commands-----------------------
// add spawn of creature
bool ChatHandler::HandleNpcAddCommand(char* args)
{
    if (!*args)
        return false;

    uint32 id;
    if (!ExtractUint32KeyFromLink(&args, "Hcreature_entry", id))
        return false;

    CreatureInfo const* cinfo = ObjectMgr::GetCreatureTemplate(id);
    if (!cinfo)
    {
        PSendSysMessage(LANG_COMMAND_INVALIDCREATUREID, id);
        SetSentErrorMessage(true);
        return false;
    }

    Player* chr = m_session->GetPlayer();
    CreatureCreatePos pos(chr, chr->GetO());
    Map* map = chr->GetMap();

    auto pCreature = new Creature;

    // used guids from specially reserved range (can be 0 if no free values)
    uint32 lowguid = sObjectMgr::Instance()->GenerateStaticCreatureLowGuid();
    if (!lowguid)
    {
        SendSysMessage(LANG_NO_FREE_STATIC_GUID_FOR_SPAWN);
        SetSentErrorMessage(true);
        return false;
    }

    if (!pCreature->Create(lowguid, pos, cinfo))
    {
        delete pCreature;
        return false;
    }

    pCreature->SaveToDB(map->GetId(), (1 << map->GetSpawnMode()));

    uint32 db_guid = pCreature->GetGUIDLow();

    // To call _LoadGoods(); _LoadQuests(); CreateTrainerSpells();
    pCreature->LoadFromDB(db_guid, map);

    if (!map->insert(pCreature))
    {
        SendSysMessage("Failed to insert into map");
        delete pCreature;
        return true;
    }
    sObjectMgr::Instance()->add_static_creature(
        sObjectMgr::Instance()->GetCreatureData(db_guid));
    return true;
}

// add item in vendorlist
bool ChatHandler::HandleNpcAddVendorItemCommand(char* args)
{
    uint32 itemId;
    if (!ExtractUint32KeyFromLink(&args, "Hitem", itemId))
    {
        SendSysMessage(LANG_COMMAND_NEEDITEMSEND);
        SetSentErrorMessage(true);
        return false;
    }

    uint32 maxcount;
    if (!ExtractOptUInt32(&args, maxcount, 0))
        return false;

    uint32 incrtime;
    if (!ExtractOptUInt32(&args, incrtime, 0))
        return false;

    uint32 extendedcost;
    if (!ExtractOptUInt32(&args, extendedcost, 0))
        return false;

    uint32 weight;
    if (!ExtractOptUInt32(&args, weight, 0))
        return false;

    Creature* vendor = getSelectedCreature();

    uint32 vendor_entry = vendor ? vendor->GetEntry() : 0;

    if (!sObjectMgr::Instance()->IsVendorItemValid(false, "npc_vendor",
            vendor_entry, itemId, maxcount, incrtime, extendedcost,
            m_session->GetPlayer()))
    {
        SetSentErrorMessage(true);
        return false;
    }

    sObjectMgr::Instance()->AddVendorItem(
        vendor_entry, itemId, maxcount, incrtime, extendedcost, weight);

    ItemPrototype const* pProto = ObjectMgr::GetItemPrototype(itemId);

    PSendSysMessage(LANG_ITEM_ADDED_TO_LIST, itemId, pProto->Name1, maxcount,
        incrtime, extendedcost);
    return true;
}

// del item from vendor list
bool ChatHandler::HandleNpcDelVendorItemCommand(char* args)
{
    if (!*args)
        return false;

    Creature* vendor = getSelectedCreature();
    if (!vendor || !vendor->isVendor())
    {
        SendSysMessage(LANG_COMMAND_VENDORSELECTION);
        SetSentErrorMessage(true);
        return false;
    }

    uint32 itemId;
    if (!ExtractUint32KeyFromLink(&args, "Hitem", itemId))
    {
        SendSysMessage(LANG_COMMAND_NEEDITEMSEND);
        SetSentErrorMessage(true);
        return false;
    }

    if (!sObjectMgr::Instance()->RemoveVendorItem(vendor->GetEntry(), itemId))
    {
        PSendSysMessage(LANG_ITEM_NOT_IN_LIST, itemId);
        SetSentErrorMessage(true);
        return false;
    }

    ItemPrototype const* pProto = ObjectMgr::GetItemPrototype(itemId);

    PSendSysMessage(LANG_ITEM_DELETED_FROM_LIST, itemId, pProto->Name1);
    return true;
}

// show info about AI
bool ChatHandler::HandleNpcAIInfoCommand(char* /*args*/)
{
    Creature* pTarget = getSelectedCreature();

    if (!pTarget)
    {
        SendSysMessage(LANG_SELECT_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    PSendSysMessage(LANG_NPC_AI_HEADER, pTarget->GetEntry());

    std::string strScript = pTarget->GetScriptName();
    std::string strAI = pTarget->GetAIName();
    char const* cstrAIClass =
        pTarget->AI() ? typeid(*pTarget->AI()).name() : " - ";

    PSendSysMessage(LANG_NPC_AI_NAMES, strAI.empty() ? " - " : strAI.c_str(),
        cstrAIClass ? cstrAIClass : " - ",
        strScript.empty() ? " - " : strScript.c_str());

    if (pTarget->AI())
        pTarget->AI()->GetAIInformation(*this);

    return true;
}

// add move for creature
bool ChatHandler::HandleNpcAddMoveCommand(char* args)
{
    uint32 lowguid;
    if (!ExtractUint32KeyFromLink(&args, "Hcreature", lowguid))
        return false;

    uint32 wait;
    if (!ExtractOptUInt32(&args, wait, 0))
        return false;

    CreatureData const* data = sObjectMgr::Instance()->GetCreatureData(lowguid);
    if (!data)
    {
        PSendSysMessage(LANG_COMMAND_CREATGUIDNOTFOUND, lowguid);
        SetSentErrorMessage(true);
        return false;
    }

    Player* player = m_session->GetPlayer();

    if (player->GetMapId() != data->mapid)
    {
        PSendSysMessage(LANG_COMMAND_CREATUREATSAMEMAP, lowguid);
        SetSentErrorMessage(true);
        return false;
    }

    Creature* pCreature =
        player->GetMap()->GetCreature(data->GetObjectGuid(lowguid));

    sWaypointMgr::Instance()->AddLastNode(lowguid, player->GetX(),
        player->GetY(), player->GetZ(), player->GetO(), wait, 0);

    // update movement type
    WorldDatabase.PExecuteLog(
        "UPDATE creature SET MovementType=%u WHERE guid=%u",
        (int)movement::gen::waypoint, lowguid);
    if (pCreature)
    {
        pCreature->set_default_movement_gen(movement::gen::waypoint);
        pCreature->movement_gens.reset();
        if (pCreature->isAlive()) // dead creature will reset movement generator
                                  // at respawn
        {
            pCreature->SetDeathState(JUST_DIED);
            pCreature->Respawn();
        }
        pCreature->SaveToDB();
    }

    SendSysMessage(LANG_WAYPOINT_ADDED);

    return true;
}

// change level of creature or pet
bool ChatHandler::HandleNpcChangeLevelCommand(char* args)
{
    if (!*args)
        return false;

    uint8 lvl = (uint8)atoi(args);
    if (lvl < 1 ||
        lvl > sWorld::Instance()->getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL) + 3)
    {
        SendSysMessage(LANG_BAD_VALUE);
        SetSentErrorMessage(true);
        return false;
    }

    Creature* pCreature = getSelectedCreature();
    if (!pCreature)
    {
        SendSysMessage(LANG_SELECT_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    if (pCreature->IsPet())
        ((Pet*)pCreature)->GivePetLevel(lvl);
    else
    {
        pCreature->SetMaxHealth(100 + 30 * lvl);
        pCreature->SetHealth(100 + 30 * lvl);
        pCreature->SetLevel(lvl);

        if (pCreature->HasStaticDBSpawnData())
            pCreature->SaveToDB();
    }

    return true;
}

// set npcflag of creature
bool ChatHandler::HandleNpcFlagCommand(char* args)
{
    if (!*args)
        return false;

    uint32 npcFlags = (uint32)atoi(args);

    Creature* pCreature = getSelectedCreature();

    if (!pCreature)
    {
        SendSysMessage(LANG_SELECT_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    pCreature->SetUInt32Value(UNIT_NPC_FLAGS, npcFlags);

    WorldDatabase.PExecuteLog(
        "UPDATE creature_template SET npcflag = '%u' WHERE entry = '%u'",
        npcFlags, pCreature->GetEntry());

    SendSysMessage(LANG_VALUE_SAVED_REJOIN);

    return true;
}

bool ChatHandler::HandleNpcBossLinkCommand(char* args)
{
    if (!m_session->GetPlayer()->GetTargetGuid() ||
        !m_session->GetPlayer()->GetTargetGuid().IsCreature())
    {
        SendSysMessage("Must select a creature to use this command.");
        return false;
    }

    Creature* tar = m_session->GetPlayer()->GetMap()->GetCreature(
        m_session->GetPlayer()->GetTargetGuid());
    if (!tar)
        return false;

    // entry
    uint32 entry, guid;
    if (ExtractUInt32(&args, entry))
    {
        if (entry == 0)
        {
            // Clear
            WorldDatabase.PQuery(
                "UPDATE creature SET boss_link_entry=0, boss_link_guid=0 WHERE "
                "guid=%i",
                tar->GetGUIDLow());
            PSendSysMessage(
                "%s: linkage has been cleared. (Req. restart)", tar->GetName());
        }
        else
        {
            if (ExtractUInt32(&args, guid))
            {
                // Update
                WorldDatabase.PQuery(
                    "UPDATE creature SET boss_link_entry=%i, boss_link_guid=%i "
                    "WHERE guid=%i",
                    entry, guid, tar->GetGUIDLow());
                if (Creature* boss = tar->GetMap()->GetCreature(
                        ObjectGuid(HIGHGUID_UNIT, entry, guid)))
                    PSendSysMessage("%s is now linked to %s. (Req. restart)",
                        tar->GetName(), boss->GetName());
            }
            else
                return false;
        }
    }
    else
    {
        if (tar->GetBossLink())
        {
            if (Creature* boss = tar->GetMap()->GetCreature(tar->GetBossLink()))
                PSendSysMessage(
                    "%s is linked to %s.", tar->GetName(), boss->GetName());
        }
        else
            PSendSysMessage("%s is not linked to anything.", tar->GetName());
    }

    return true;
}

bool ChatHandler::HandleNpcDeleteCommand(char* args)
{
    Creature* unit = nullptr;

    if (*args)
    {
        // number or [name] Shift-click form
        // |color|Hcreature:creature_guid|h[name]|h|r
        uint32 lowguid;
        if (!ExtractUint32KeyFromLink(&args, "Hcreature", lowguid))
            return false;

        if (!lowguid)
            return false;

        if (CreatureData const* data =
                sObjectMgr::Instance()->GetCreatureData(lowguid))
            unit = m_session->GetPlayer()->GetMap()->GetCreature(
                data->GetObjectGuid(lowguid));
    }
    else
        unit = getSelectedCreature();

    if (!unit)
    {
        SendSysMessage(LANG_SELECT_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    switch (unit->GetSubtype())
    {
    case CREATURE_SUBTYPE_GENERIC:
    {
        unit->CombatStop();
        if (CreatureData const* data =
                sObjectMgr::Instance()->GetCreatureData(unit->GetGUIDLow()))
        {
            Creature::AddToRemoveListInMaps(unit->GetGUIDLow(), data);
            Creature::DeleteFromDB(unit->GetGUIDLow(), data);
        }
        else
            unit->AddObjectToRemoveList();
        break;
    }
    case CREATURE_SUBTYPE_PET:
        ((Pet*)unit)->Unsummon(PET_SAVE_AS_CURRENT);
        break;
    case CREATURE_SUBTYPE_TOTEM:
        ((Totem*)unit)->UnSummon();
        break;
    case CREATURE_SUBTYPE_TEMPORARY_SUMMON:
        ((TemporarySummon*)unit)->UnSummon();
        break;
    default:
        return false;
    }

    SendSysMessage(LANG_COMMAND_DELCREATMESSAGE);

    return true;
}

// move selected creature
bool ChatHandler::HandleNpcMoveCommand(char* args)
{
    uint32 lowguid = 0;

    Creature* pCreature = getSelectedCreature();

    if (!pCreature)
    {
        // number or [name] Shift-click form
        // |color|Hcreature:creature_guid|h[name]|h|r
        if (!ExtractUint32KeyFromLink(&args, "Hcreature", lowguid))
            return false;

        CreatureData const* data =
            sObjectMgr::Instance()->GetCreatureData(lowguid);
        if (!data)
        {
            PSendSysMessage(LANG_COMMAND_CREATGUIDNOTFOUND, lowguid);
            SetSentErrorMessage(true);
            return false;
        }

        Player* player = m_session->GetPlayer();

        if (player->GetMapId() != data->mapid)
        {
            PSendSysMessage(LANG_COMMAND_CREATUREATSAMEMAP, lowguid);
            SetSentErrorMessage(true);
            return false;
        }

        pCreature = player->GetMap()->GetCreature(data->GetObjectGuid(lowguid));
    }
    else
        lowguid = pCreature->GetGUIDLow();

    float x = m_session->GetPlayer()->GetX();
    float y = m_session->GetPlayer()->GetY();
    float z = m_session->GetPlayer()->GetZ();
    float o = m_session->GetPlayer()->GetO();

    if (pCreature)
    {
        if (CreatureData const* data = sObjectMgr::Instance()->GetCreatureData(
                pCreature->GetGUIDLow()))
        {
            const_cast<CreatureData*>(data)->posX = x;
            const_cast<CreatureData*>(data)->posY = y;
            const_cast<CreatureData*>(data)->posZ = z;
            const_cast<CreatureData*>(data)->orientation = o;
        }
        pCreature->GetMap()->relocate(pCreature, x, y, z, o);
        pCreature->movement_gens.reset();
        if (pCreature->isAlive()) // dead creature will reset movement generator
                                  // at respawn
        {
            pCreature->SetDeathState(JUST_DIED);
            pCreature->Respawn();
        }
    }

    WorldDatabase.PExecuteLog(
        "UPDATE creature SET position_x = '%f', position_y = '%f', position_z "
        "= '%f', orientation = '%f' WHERE guid = '%u'",
        x, y, z, o, lowguid);
    PSendSysMessage(LANG_COMMAND_CREATUREMOVED);
    return true;
}

/**HandleNpcSetMoveTypeCommand
 * Set the movement type for an NPC.<br/>
 * <br/>
 * Valid movement types are:
 * <ul>
 * <li> stay - NPC wont move </li>
 * <li> random - NPC will move randomly according to the spawndist </li>
 * <li> way - NPC will move with given waypoints set </li>
 * </ul>
 * additional parameter: NODEL - so no waypoints are deleted, if you
 *                       change the movement type
 */
bool ChatHandler::HandleNpcSetMoveTypeCommand(char* args)
{
    // 3 arguments:
    // GUID (optional - you can also select the creature)
    // stay|random|way|random2 (determines the kind of movement)
    // NODEL (optional - tells the system NOT to delete any waypoints)
    //        this is very handy if you want to do waypoints, that are
    //        later switched on/off according to special events (like escort
    //        quests, etc)

    uint32 lowguid;
    Creature* pCreature;
    if (!ExtractUInt32(&args,
            lowguid)) // case .setmovetype $move_type (with selected creature)
    {
        pCreature = getSelectedCreature();
        if (!pCreature || !pCreature->HasStaticDBSpawnData())
            return false;
        lowguid = pCreature->GetGUIDLow();
    }
    else // case .setmovetype #creature_guid $move_type (with guid)
    {
        CreatureData const* data =
            sObjectMgr::Instance()->GetCreatureData(lowguid);
        if (!data)
        {
            PSendSysMessage(LANG_COMMAND_CREATGUIDNOTFOUND, lowguid);
            SetSentErrorMessage(true);
            return false;
        }

        Player* player = m_session->GetPlayer();

        if (player->GetMapId() != data->mapid)
        {
            PSendSysMessage(LANG_COMMAND_CREATUREATSAMEMAP, lowguid);
            SetSentErrorMessage(true);
            return false;
        }

        pCreature = player->GetMap()->GetCreature(data->GetObjectGuid(lowguid));
    }

    movement::gen move_type;
    char* type_str = ExtractLiteralArg(&args);
    if (!type_str)
        return false;

    if (strncmp(type_str, "stay", strlen(type_str)) == 0)
        move_type = movement::gen::idle;
    else if (strncmp(type_str, "random", strlen(type_str)) == 0)
        move_type = movement::gen::random;
    else if (strncmp(type_str, "way", strlen(type_str)) == 0)
        move_type = movement::gen::random;
    else if (strncmp(type_str, "random2", strlen(type_str)) == 0)
        move_type = movement::gen::random_waterair;
    else
        return false;

    bool doNotDelete = ExtractLiteralArg(&args, "NODEL") != nullptr;
    if (!doNotDelete && *args) // need fail if false in result wrong literal
        return false;

    // now lowguid is low guid really existing creature
    // and pCreature point (maybe) to this creature or NULL

    // update movement type
    if (!doNotDelete)
        sWaypointMgr::Instance()->DeletePath(lowguid);

    if (pCreature)
    {
        pCreature->set_default_movement_gen(move_type);
        pCreature->movement_gens.reset();
        if (pCreature->isAlive()) // dead creature will reset movement generator
                                  // at respawn
        {
            pCreature->SetDeathState(JUST_DIED);
            pCreature->Respawn();
        }
        pCreature->SaveToDB();
    }

    if (doNotDelete)
        PSendSysMessage(LANG_MOVE_TYPE_SET_NODEL, type_str);
    else
        PSendSysMessage(LANG_MOVE_TYPE_SET, type_str);

    return true;
}

// set model of creature
bool ChatHandler::HandleNpcSetModelCommand(char* args)
{
    if (!*args)
        return false;

    uint32 displayId = (uint32)atoi(args);

    Creature* pCreature = getSelectedCreature();

    if (!pCreature || pCreature->IsPet())
    {
        SendSysMessage(LANG_SELECT_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    pCreature->SetDisplayId(displayId);
    pCreature->SetNativeDisplayId(displayId);

    if (pCreature->HasStaticDBSpawnData())
        pCreature->SaveToDB();

    return true;
}
// set faction of creature
bool ChatHandler::HandleNpcFactionIdCommand(char* args)
{
    if (!*args)
        return false;

    uint32 factionId = (uint32)atoi(args);

    if (!sFactionTemplateStore.LookupEntry(factionId))
    {
        PSendSysMessage(LANG_WRONG_FACTION, factionId);
        SetSentErrorMessage(true);
        return false;
    }

    Creature* pCreature = getSelectedCreature();

    if (!pCreature)
    {
        SendSysMessage(LANG_SELECT_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    pCreature->setFaction(factionId);

    // faction is set in creature_template - not inside creature

    // update in memory
    if (CreatureInfo const* cinfo = pCreature->GetCreatureInfo())
    {
        const_cast<CreatureInfo*>(cinfo)->faction_A = factionId;
        const_cast<CreatureInfo*>(cinfo)->faction_H = factionId;
    }

    // and DB
    WorldDatabase.PExecuteLog(
        "UPDATE creature_template SET faction_A = '%u', faction_H = '%u' WHERE "
        "entry = '%u'",
        factionId, factionId, pCreature->GetEntry());

    return true;
}
// set spawn dist of creature
bool ChatHandler::HandleNpcSpawnDistCommand(char* args)
{
    if (!*args)
        return false;

    float option = (float)atof(args);
    if (option < 0.0f)
    {
        SendSysMessage(LANG_BAD_VALUE);
        return false;
    }

    Creature* pCreature = getSelectedCreature();
    uint32 u_guidlow = 0;

    if (pCreature)
        u_guidlow = pCreature->GetGUIDLow();
    else
        return false;

    movement::gen mtype = movement::gen::idle;
    if (option > 0.0f)
        mtype = pCreature->get_default_movement_gen() ==
                        movement::gen::random_waterair ?
                    movement::gen::random_waterair :
                    movement::gen::random;

    pCreature->SetRespawnRadius((float)option);
    pCreature->set_default_movement_gen(mtype);
    pCreature->movement_gens.reset();
    if (pCreature->isAlive()) // dead creature will reset movement generator at
                              // respawn
    {
        pCreature->SetDeathState(JUST_DIED);
        pCreature->Respawn();
    }

    WorldDatabase.PExecuteLog(
        "UPDATE creature SET spawndist=%f, MovementType=%i WHERE guid=%u",
        option, mtype, u_guidlow);
    PSendSysMessage(LANG_COMMAND_SPAWNDIST, option);
    return true;
}
// spawn time handling
bool ChatHandler::HandleNpcSpawnTimeCommand(char* args)
{
    uint32 stime;
    if (!ExtractUInt32(&args, stime))
        return false;

    Creature* pCreature = getSelectedCreature();
    if (!pCreature)
    {
        PSendSysMessage(LANG_SELECT_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    uint32 u_guidlow = pCreature->GetGUIDLow();

    WorldDatabase.PExecuteLog(
        "UPDATE creature SET spawntimesecs=%i WHERE guid=%u", stime, u_guidlow);
    pCreature->SetRespawnDelay(stime);
    PSendSysMessage(LANG_COMMAND_SPAWNTIME, stime);

    return true;
}
// npc follow handling
bool ChatHandler::HandleNpcFollowCommand(char* /*args*/)
{
    Player* player = m_session->GetPlayer();
    Creature* creature = getSelectedCreature();

    if (!creature)
    {
        PSendSysMessage(LANG_SELECT_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    // Follow player - Using pet's default dist and angle
    creature->movement_gens.push(new movement::FollowMovementGenerator(
        player, PET_FOLLOW_DIST, PET_FOLLOW_ANGLE));

    PSendSysMessage(LANG_CREATURE_FOLLOW_YOU_NOW, creature->GetName());
    return true;
}
// npc unfollow handling
bool ChatHandler::HandleNpcUnFollowCommand(char* /*args*/)
{
    Creature* creature = getSelectedCreature();

    if (!creature)
    {
        PSendSysMessage(LANG_SELECT_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    creature->movement_gens.remove_all(movement::gen::follow);

    PSendSysMessage(LANG_CREATURE_NOT_FOLLOW_YOU_NOW, creature->GetName());
    return true;
}
// npc tame handling
bool ChatHandler::HandleNpcTameCommand(char* /*args*/)
{
    Creature* creatureTarget = getSelectedCreature();

    if (!creatureTarget || creatureTarget->IsPet())
    {
        PSendSysMessage(LANG_SELECT_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    Player* player = m_session->GetPlayer();

    if (player->GetPetGuid())
    {
        SendSysMessage(LANG_YOU_ALREADY_HAVE_PET);
        SetSentErrorMessage(true);
        return false;
    }

    player->CastSpell(
        creatureTarget, 13481, true); // Tame Beast, triggered effect
    return true;
}

// npc deathstate handling
bool ChatHandler::HandleNpcSetDeathStateCommand(char* args)
{
    bool value;
    if (!ExtractOnOff(&args, value))
    {
        SendSysMessage(LANG_USE_BOL);
        SetSentErrorMessage(true);
        return false;
    }

    Creature* pCreature = getSelectedCreature();
    if (!pCreature || !pCreature->HasStaticDBSpawnData())
    {
        SendSysMessage(LANG_SELECT_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    if (value)
        pCreature->SetDeadByDefault(true);
    else
        pCreature->SetDeadByDefault(false);

    pCreature->SaveToDB();
    pCreature->Respawn();

    return true;
}

// TODO: NpcCommands that need to be fixed :

bool ChatHandler::HandleNpcNameCommand(char* /*args*/)
{
    /* Temp. disabled
    if (!*args)
        return false;

    if (strlen((char*)args)>75)
    {
        PSendSysMessage(LANG_TOO_LONG_NAME, strlen((char*)args)-75);
        return true;
    }

    for (uint8 i = 0; i < strlen(args); ++i)
    {
        if (!isalpha(args[i]) && args[i]!=' ')
        {
            SendSysMessage(LANG_CHARS_ONLY);
            return false;
        }
    }

    ObjectGuid guid = m_session->GetPlayer()->GetSelectionGuid();
    if (guid.IsEmpty())
    {
        SendSysMessage(LANG_NO_SELECTION);
        return true;
    }

    Creature* pCreature = ObjectAccessor::GetCreature(*m_session->GetPlayer(),
    guid);

    if (!pCreature)
    {
        SendSysMessage(LANG_SELECT_CREATURE);
        return true;
    }

    pCreature->SetName(args);
    uint32 idname =
    sObjectMgr::Instance()->AddCreatureTemplate(pCreature->GetName());
    pCreature->SetUInt32Value(OBJECT_FIELD_ENTRY, idname);

    pCreature->SaveToDB();
    */

    return true;
}

bool ChatHandler::HandleNpcSubNameCommand(char* /*args*/)
{
    /* Temp. disabled

    if (!*args)
        args = "";

    if (strlen((char*)args)>75)
    {

        PSendSysMessage(LANG_TOO_LONG_SUBNAME, strlen((char*)args)-75);
        return true;
    }

    for (uint8 i = 0; i < strlen(args); i++)
    {
        if (!isalpha(args[i]) && args[i]!=' ')
        {
            SendSysMessage(LANG_CHARS_ONLY);
            return false;
        }
    }

    ObjectGuid guid = m_session->GetPlayer()->GetSelectionGuid();
    if (guid.IsEmpty())
    {
        SendSysMessage(LANG_NO_SELECTION);
        return true;
    }

    Creature* pCreature = ObjectAccessor::GetCreature(*m_session->GetPlayer(),
    guid);

    if (!pCreature)
    {
        SendSysMessage(LANG_SELECT_CREATURE);
        return true;
    }

    uint32 idname =
    sObjectMgr::Instance()->AddCreatureSubName(pCreature->GetName(),args,pCreature->GetUInt32Value(UNIT_FIELD_DISPLAYID));
    pCreature->SetUInt32Value(OBJECT_FIELD_ENTRY, idname);

    pCreature->SaveToDB();
    */
    return true;
}

// demorph player or unit
bool ChatHandler::HandleDeMorphCommand(char* /*args*/)
{
    Unit* target = getSelectedUnit();
    if (!target)
        target = m_session->GetPlayer();

    // check online security
    else if (target->GetTypeId() == TYPEID_PLAYER &&
             HasLowerSecurity((Player*)target))
        return false;

    target->DeMorph();

    return true;
}

// morph creature or player
bool ChatHandler::HandleModifyMorphCommand(char* args)
{
    if (!*args)
        return false;

    uint32 display_id = (uint32)atoi(args);

    CreatureDisplayInfoEntry const* displayEntry =
        sCreatureDisplayInfoStore.LookupEntry(display_id);
    if (!displayEntry)
    {
        SendSysMessage(LANG_BAD_VALUE);
        SetSentErrorMessage(true);
        return false;
    }

    Unit* target = getSelectedUnit();
    if (!target)
        target = m_session->GetPlayer();

    // check online security
    else if (target->GetTypeId() == TYPEID_PLAYER &&
             HasLowerSecurity((Player*)target))
        return false;

    target->SetDisplayId(display_id);

    return true;
}

// kick player
bool ChatHandler::HandleKickPlayerCommand(char* args)
{
    Player* target;
    if (!ExtractPlayerTarget(&args, &target))
        return false;

    if (m_session && target == m_session->GetPlayer())
    {
        SendSysMessage(LANG_COMMAND_KICKSELF);
        SetSentErrorMessage(true);
        return false;
    }

    // check online security
    if (HasLowerSecurity(target))
        return false;

    // send before target pointer invalidate
    PSendSysMessage(LANG_COMMAND_KICKMESSAGE, GetNameLink(target).c_str());
    target->GetSession()->KickPlayer();
    return true;
}

// show info of player
bool ChatHandler::HandlePInfoCommand(char* args)
{
    Player* target;
    ObjectGuid target_guid;
    std::string target_name;
    if (!ExtractPlayerTarget(&args, &target, &target_guid, &target_name))
        return false;

    uint32 accId = 0;
    uint32 money = 0;
    uint32 total_player_time = 0;
    uint32 level = 0;
    uint32 latency = 0;

    // get additional information from Player object
    if (target)
    {
        // check online security
        if (HasLowerSecurity(target))
            return false;

        accId = target->GetSession()->GetAccountId();
        money = target->storage().money().get();
        total_player_time = target->GetTotalPlayedTime();
        level = target->getLevel();
        latency = target->GetSession()->GetLatency();
    }
    // get additional information from DB
    else
    {
        // check offline security
        if (HasLowerSecurity(nullptr, target_guid))
            return false;

        //                                                     0          1
        //                                                     2      3
        std::unique_ptr<QueryResult> result(CharacterDatabase.PQuery(
            "SELECT totaltime, level, money, account FROM characters WHERE "
            "guid = '%u'",
            target_guid.GetCounter()));
        if (!result)
            return false;

        Field* fields = result->Fetch();
        total_player_time = fields[0].GetUInt32();
        level = fields[1].GetUInt32();
        money = fields[2].GetUInt32();
        accId = fields[3].GetUInt32();
    }

    std::string username = GetMangosString(LANG_ERROR);
    std::string last_ip = GetMangosString(LANG_ERROR);
    AccountTypes security = SEC_PLAYER;
    std::string last_login = GetMangosString(LANG_ERROR);

    std::unique_ptr<QueryResult> result(LoginDatabase.PQuery(
        "SELECT username,gmlevel,last_ip,last_login FROM account WHERE id = "
        "'%u'",
        accId));
    if (result)
    {
        Field* fields = result->Fetch();
        username = fields[0].GetCppString();
        security = (AccountTypes)fields[1].GetUInt32();

        if (GetAccessLevel() >= security)
        {
            last_ip = fields[2].GetCppString();
            last_login = fields[3].GetCppString();
        }
        else
        {
            last_ip = "-";
            last_login = "-";
        }
    }

    std::string nameLink = playerLink(target_name);

    PSendSysMessage(LANG_PINFO_ACCOUNT,
        (target ? "" : GetMangosString(LANG_OFFLINE)), nameLink.c_str(),
        target_guid.GetCounter(), username.c_str(), accId, security,
        last_ip.c_str(), last_login.c_str(), latency);

    std::string timeStr = secsToTimeString(total_player_time, true, true);
    uint32 gold = money / GOLD;
    uint32 silv = (money % GOLD) / SILVER;
    uint32 copp = (money % GOLD) % SILVER;
    PSendSysMessage(LANG_PINFO_LEVEL, timeStr.c_str(), level, gold, silv, copp);

    return true;
}

/**
 * Add a waypoint to a creature.
 *
 * The user can either select an npc or provide its GUID.
 *
 * The user can even select a visual waypoint - then the new waypoint
 * is placed *after* the selected one - this makes insertion of new
 * waypoints possible.
 *
 * eg:
 * .wp add 12345
 * -> adds a waypoint to the npc with the GUID 12345
 *
 * .wp add
 * -> adds a waypoint to the currently selected creature
 *
 *
 * @param args if the user did not provide a GUID, it is NULL
 *
 * @return true - command did succeed, false - something went wrong
 */
bool ChatHandler::HandleWpAddCommand(char* args)
{
    LOG_DEBUG(logging, "DEBUG: HandleWpAddCommand");

    // optional
    char* guid_str = nullptr;

    if (*args)
    {
        guid_str = strtok(args, " ");
    }

    uint32 lowguid = 0;
    uint32 point = 0;
    Creature* target = getSelectedCreature();
    // Did player provide a GUID?
    if (!guid_str)
    {
        LOG_DEBUG(logging, "DEBUG: HandleWpAddCommand - No GUID provided");

        // No GUID provided
        // -> Player must have selected a creature

        if (!target || !target->HasStaticDBSpawnData())
        {
            SendSysMessage(LANG_SELECT_CREATURE);
            SetSentErrorMessage(true);
            return false;
        }

        if (target->GetEntry() == VISUAL_WAYPOINT)
        {
            LOG_DEBUG(logging,
                "DEBUG: HandleWpAddCommand - target->GetEntry() == "
                "VISUAL_WAYPOINT (1) ");

            QueryResult* result = WorldDatabase.PQuery(
                "SELECT id, point FROM creature_movement WHERE wpguid = %u",
                target->GetGUIDLow());
            if (!result)
            {
                PSendSysMessage(
                    LANG_WAYPOINT_NOTFOUNDSEARCH, target->GetGUIDLow());
                // User selected a visual spawnpoint -> get the NPC
                // Select NPC GUID
                // Since we compare float values, we have to deal with
                // some difficulties.
                // Here we search for all waypoints that only differ in one from
                // 1 thousand
                // (0.001) - There is no other way to compare C++ floats with
                // mySQL floats
                // See also:
                // http://dev.mysql.com/doc/refman/5.0/en/problems-with-float.html
                const char* maxDIFF = "0.01";
                result = WorldDatabase.PQuery(
                    "SELECT id, point FROM creature_movement WHERE "
                    "(abs(position_x - %f) <= %s ) and (abs(position_y - %f) "
                    "<= %s ) and (abs(position_z - %f) <= %s )",
                    target->GetX(), maxDIFF, target->GetY(), maxDIFF,
                    target->GetZ(), maxDIFF);
                if (!result)
                {
                    PSendSysMessage(
                        LANG_WAYPOINT_NOTFOUNDDBPROBLEM, target->GetGUIDLow());
                    SetSentErrorMessage(true);
                    return false;
                }
            }
            do
            {
                Field* fields = result->Fetch();
                lowguid = fields[0].GetUInt32();
                point = fields[1].GetUInt32();
            } while (result->NextRow());
            delete result;

            CreatureData const* data =
                sObjectMgr::Instance()->GetCreatureData(lowguid);
            if (!data)
            {
                PSendSysMessage(LANG_WAYPOINT_CREATNOTFOUND, lowguid);
                SetSentErrorMessage(true);
                return false;
            }

            target = m_session->GetPlayer()->GetMap()->GetCreature(
                data->GetObjectGuid(lowguid));
            if (!target)
            {
                PSendSysMessage(LANG_WAYPOINT_NOTFOUNDDBPROBLEM, lowguid);
                SetSentErrorMessage(true);
                return false;
            }
        }
        else
        {
            lowguid = target->GetGUIDLow();
        }
    }
    else
    {
        LOG_DEBUG(logging, "DEBUG: HandleWpAddCommand - GUID provided");

        // GUID provided
        // Warn if player also selected a creature
        // -> Creature selection is ignored <-
        if (target)
        {
            SendSysMessage(LANG_WAYPOINT_CREATSELECTED);
        }
        lowguid = atoi((char*)guid_str);

        CreatureData const* data =
            sObjectMgr::Instance()->GetCreatureData(lowguid);
        if (!data)
        {
            PSendSysMessage(LANG_WAYPOINT_CREATNOTFOUND, lowguid);
            SetSentErrorMessage(true);
            return false;
        }

        target = m_session->GetPlayer()->GetMap()->GetCreature(
            data->GetObjectGuid(lowguid));
        if (!target || target->IsPet())
        {
            PSendSysMessage(LANG_WAYPOINT_CREATNOTFOUND, lowguid);
            SetSentErrorMessage(true);
            return false;
        }
    }
    // lowguid -> GUID of the NPC
    // point   -> number of the waypoint (if not 0)
    LOG_DEBUG(logging, "DEBUG: HandleWpAddCommand - danach");

    LOG_DEBUG(logging, "DEBUG: HandleWpAddCommand - point == 0");

    Player* player = m_session->GetPlayer();
    sWaypointMgr::Instance()->AddLastNode(lowguid, player->GetX(),
        player->GetY(), player->GetZ(), player->GetO(), 0, 0);

    // update movement type
    if (target)
    {
        target->set_default_movement_gen(movement::gen::waypoint);
        target->movement_gens.reset();
        if (target->isAlive()) // dead creature will reset movement generator at
                               // respawn
        {
            target->SetDeathState(JUST_DIED);
            target->Respawn();
        }
        target->SaveToDB();
    }
    else
        WorldDatabase.PExecuteLog(
            "UPDATE creature SET MovementType=%u WHERE guid=%u",
            movement::gen::waypoint, lowguid);

    PSendSysMessage(LANG_WAYPOINT_ADDED, point, lowguid);

    return true;
} // HandleWpAddCommand

/**
 * .wp modify emote | spell | text | del | move | add | run
 *
 * add -> add a WP after the selected visual waypoint
 *        User must select a visual waypoint and then issue ".wp modify add"
 *
 * emote <emoteID>
 *   User has selected a visual waypoint before.
 *   <emoteID> is added to this waypoint. Everytime the
 *   NPC comes to this waypoint, the emote is called.
 *
 * emote <GUID> <WPNUM> <emoteID>
 *   User has not selected visual waypoint before.
 *   For the waypoint <WPNUM> for the NPC with <GUID>
 *   an emote <emoteID> is added.
 *   Everytime the NPC comes to this waypoint, the emote is called.
 *
 * run <1/0>
 *   1 means we'll run at this point, 0 means we'll walk
 *
 *
 * info <GUID> <WPNUM> -> User did not select a visual waypoint and
 */
bool ChatHandler::HandleWpModifyCommand(char* args)
{
    LOG_DEBUG(logging, "DEBUG: HandleWpModifyCommand");

    if (!*args)
        return false;

    CreatureInfo const* waypointInfo =
        ObjectMgr::GetCreatureTemplate(VISUAL_WAYPOINT);
    if (!waypointInfo || waypointInfo->GetHighGuid() != HIGHGUID_UNIT)
        return false; // must exist as normal creature in mangos.sql
                      // 'creature_template'

    // first arg: add del text emote spell waittime move run
    char* show_str = strtok(args, " ");
    if (!show_str)
    {
        return false;
    }

    std::string show = show_str;
    // Check
    // Remember: "show" must also be the name of a column!
    if ((show != "emote") && (show != "spell") && (show != "textid1") &&
        (show != "textid2") && (show != "textid3") && (show != "textid4") &&
        (show != "textid5") && (show != "waittime") && (show != "del") &&
        (show != "move") && (show != "add") && (show != "model1") &&
        (show != "model2") && (show != "orientation") && show != "run")
    {
        return false;
    }

    // Next arg is: <GUID> <WPNUM> <ARGUMENT>

    // Did user provide a GUID
    // or did the user select a creature?
    // -> variable lowguid is filled with the GUID of the NPC
    uint32 lowguid = 0;
    uint32 point = 0;
    uint32 wpGuid = 0;
    Creature* target = getSelectedCreature();

    if (target)
    {
        LOG_DEBUG(
            logging, "DEBUG: HandleWpModifyCommand - User did select an NPC");

        // Did the user select a visual spawnpoint?
        if (target->GetEntry() != VISUAL_WAYPOINT)
        {
            PSendSysMessage(LANG_WAYPOINT_VP_SELECT);
            SetSentErrorMessage(true);
            return false;
        }

        wpGuid = target->GetGUIDLow();

        // The visual waypoint
        QueryResult* result = WorldDatabase.PQuery(
            "SELECT id, point FROM creature_movement WHERE wpguid = %u LIMIT 1",
            target->GetGUIDLow());
        if (!result)
        {
            PSendSysMessage(LANG_WAYPOINT_NOTFOUNDDBPROBLEM, wpGuid);
            SetSentErrorMessage(true);
            return false;
        }
        LOG_DEBUG(
            logging, "DEBUG: HandleWpModifyCommand - After getting wpGuid");

        Field* fields = result->Fetch();
        lowguid = fields[0].GetUInt32();
        point = fields[1].GetUInt32();

        // Cleanup memory
        LOG_DEBUG(logging, "DEBUG: HandleWpModifyCommand - Cleanup memory");
        delete result;
    }
    else
    {
        // User did provide <GUID> <WPNUM>

        char* guid_str = strtok((char*)nullptr, " ");
        if (!guid_str)
        {
            SendSysMessage(LANG_WAYPOINT_NOGUID);
            return false;
        }
        lowguid = atoi((char*)guid_str);

        CreatureData const* data =
            sObjectMgr::Instance()->GetCreatureData(lowguid);
        if (!data)
        {
            PSendSysMessage(LANG_WAYPOINT_CREATNOTFOUND, lowguid);
            SetSentErrorMessage(true);
            return false;
        }

        PSendSysMessage("DEBUG: GUID provided: %d", lowguid);

        char* point_str = strtok((char*)nullptr, " ");
        if (!point_str)
        {
            SendSysMessage(LANG_WAYPOINT_NOWAYPOINTGIVEN);
            return false;
        }
        point = atoi((char*)point_str);

        PSendSysMessage("DEBUG: wpNumber provided: %d", point);

        // Now we need the GUID of the visual waypoint
        // -> "del", "move", "add" command

        std::unique_ptr<QueryResult> result(WorldDatabase.PQuery(
            "SELECT wpguid FROM creature_movement WHERE id = '%u' AND point = "
            "'%u' LIMIT 1",
            lowguid, point));
        if (!result)
        {
            PSendSysMessage(LANG_WAYPOINT_NOTFOUNDSEARCH, lowguid, point);
            SetSentErrorMessage(true);
            return false;
        }

        Field* fields = result->Fetch();
        wpGuid = fields[0].GetUInt32();
    }

    char* arg_str = nullptr;
    // Check for argument
    if ((show.find("text") == std::string::npos) && (show != "del") &&
        (show != "move") && (show != "add"))
    {
        // Text is enclosed in "<>", all other arguments not
        if (show.find("text") != std::string::npos)
            arg_str = strtok((char*)nullptr, "<>");
        else
            arg_str = strtok((char*)nullptr, " ");

        if (!arg_str)
        {
            PSendSysMessage(LANG_WAYPOINT_ARGUMENTREQ, show_str);
            return false;
        }
    }

    LOG_DEBUG(logging,
        "DEBUG: HandleWpModifyCommand - Parameters parsed - now execute the "
        "command");

    // wpGuid  -> GUID of the waypoint creature
    // lowguid -> GUID of the NPC
    // point   -> waypoint number

    // Special functions:
    // add - move - del -> no args commands
    // Add a waypoint after the selected visual
    if (show == "add" && target)
    {
        PSendSysMessage("DEBUG: wp modify add, GUID: %u", lowguid);

        // Get the creature for which we read the waypoint
        CreatureData const* data =
            sObjectMgr::Instance()->GetCreatureData(lowguid);
        if (!data)
        {
            PSendSysMessage(LANG_WAYPOINT_CREATNOTFOUND, lowguid);
            SetSentErrorMessage(true);
            return false;
        }

        Creature* npcCreature = m_session->GetPlayer()->GetMap()->GetCreature(
            data->GetObjectGuid(lowguid));

        if (!npcCreature)
        {
            PSendSysMessage(LANG_WAYPOINT_NPCNOTFOUND);
            SetSentErrorMessage(true);
            return false;
        }

        LOG_DEBUG(logging, "DEBUG: HandleWpModifyCommand - add -- npcCreature");

        // What to do:
        // Add the visual spawnpoint (DB only)
        // Adjust the waypoints
        // Respawn the owner of the waypoints
        LOG_DEBUG(logging, "DEBUG: HandleWpModifyCommand - add");

        Player* chr = m_session->GetPlayer();
        Map* map = chr->GetMap();

        if (npcCreature)
        {
            npcCreature->movement_gens.reset();
            if (npcCreature->isAlive()) // dead creature will reset movement
                                        // generator at respawn
            {
                npcCreature->SetDeathState(JUST_DIED);
                npcCreature->Respawn();
            }
        }

        // create the waypoint creature
        wpGuid = 0;
        auto wpCreature = new Creature;

        CreatureCreatePos pos(chr, chr->GetO());

        if (!wpCreature->Create(
                map->GenerateLocalLowGuid(HIGHGUID_UNIT), pos, waypointInfo))
        {
            PSendSysMessage(LANG_WAYPOINT_VP_NOTCREATED, VISUAL_WAYPOINT);
            delete wpCreature;
            return false;
        }

        wpCreature->SaveToDB(map->GetId(), (1 << map->GetSpawnMode()));
        // To call _LoadGoods(); _LoadQuests(); CreateTrainerSpells();
        wpCreature->LoadFromDB(wpCreature->GetGUIDLow(), map);
        if (!map->insert(wpCreature))
        {
            SendSysMessage("Failed to insert into map");
            delete wpCreature;
            return true;
        }
        wpGuid = wpCreature->GetGUIDLow();

        sWaypointMgr::Instance()->AddAfterNode(lowguid, point, chr->GetX(),
            chr->GetY(), chr->GetZ(), 0, 0, wpGuid);

        if (!wpGuid)
            return false;

        PSendSysMessage(LANG_WAYPOINT_ADDED_NO, point + 1);
        return true;
    } // add

    if (show == "del" && target)
    {
        PSendSysMessage("DEBUG: wp modify del, GUID: %u", lowguid);

        // Get the creature for which we read the waypoint
        CreatureData const* data =
            sObjectMgr::Instance()->GetCreatureData(lowguid);
        if (!data)
        {
            PSendSysMessage(LANG_WAYPOINT_CREATNOTFOUND, lowguid);
            SetSentErrorMessage(true);
            return false;
        }

        Creature* npcCreature = m_session->GetPlayer()->GetMap()->GetCreature(
            data->GetObjectGuid(lowguid));

        // wpCreature
        Creature* wpCreature = nullptr;
        if (wpGuid != 0)
        {
            wpCreature = m_session->GetPlayer()->GetMap()->GetCreature(
                ObjectGuid(HIGHGUID_UNIT, VISUAL_WAYPOINT, wpGuid));
            wpCreature->DeleteFromDB();
            wpCreature->AddObjectToRemoveList();
        }

        // What to do:
        // Remove the visual spawnpoint
        // Adjust the waypoints
        // Respawn the owner of the waypoints

        sWaypointMgr::Instance()->DeleteNode(lowguid, point);

        if (npcCreature)
        {
            // Any waypoints left?
            std::unique_ptr<QueryResult> result(WorldDatabase.PQuery(
                "SELECT point FROM creature_movement WHERE id = '%u'",
                lowguid));
            if (!result)
                npcCreature->set_default_movement_gen(movement::gen::random);
            else
                npcCreature->set_default_movement_gen(movement::gen::waypoint);
            npcCreature->movement_gens.reset();
            if (npcCreature->isAlive()) // dead creature will reset movement
                                        // generator at respawn
            {
                npcCreature->SetDeathState(JUST_DIED);
                npcCreature->Respawn();
            }
            npcCreature->SaveToDB();
        }

        PSendSysMessage(LANG_WAYPOINT_REMOVED);
        return true;
    } // del

    if (show == "move" && target)
    {
        PSendSysMessage("DEBUG: wp move, GUID: %u", lowguid);

        Player* chr = m_session->GetPlayer();
        Map* map = chr->GetMap();
        {
            // Get the creature for which we read the waypoint
            CreatureData const* data =
                sObjectMgr::Instance()->GetCreatureData(lowguid);
            if (!data)
            {
                PSendSysMessage(LANG_WAYPOINT_CREATNOTFOUND, lowguid);
                SetSentErrorMessage(true);
                return false;
            }

            Creature* npcCreature =
                m_session->GetPlayer()->GetMap()->GetCreature(
                    data->GetObjectGuid(lowguid));

            // wpCreature
            Creature* wpCreature = nullptr;
            // What to do:
            // Move the visual spawnpoint
            // Respawn the owner of the waypoints
            if (wpGuid != 0)
            {
                wpCreature = m_session->GetPlayer()->GetMap()->GetCreature(
                    ObjectGuid(HIGHGUID_UNIT, VISUAL_WAYPOINT, wpGuid));
                wpCreature->DeleteFromDB();
                wpCreature->AddObjectToRemoveList();
                // re-create
                auto wpCreature2 = new Creature;

                CreatureCreatePos pos(chr, chr->GetO());

                if (!wpCreature2->Create(
                        map->GenerateLocalLowGuid(HIGHGUID_UNIT), pos,
                        waypointInfo))
                {
                    PSendSysMessage(
                        LANG_WAYPOINT_VP_NOTCREATED, VISUAL_WAYPOINT);
                    delete wpCreature2;
                    return false;
                }

                wpCreature2->SaveToDB(map->GetId(), (1 << map->GetSpawnMode()));
                // To call _LoadGoods(); _LoadQuests(); CreateTrainerSpells();
                wpCreature2->LoadFromDB(wpCreature2->GetGUIDLow(), map);
                if (!map->insert(wpCreature2))
                {
                    SendSysMessage("Failed to insert into map");
                    delete wpCreature2;
                    return true;
                }
            }

            sWaypointMgr::Instance()->SetNodePosition(
                lowguid, point, chr->GetX(), chr->GetY(), chr->GetZ());

            if (npcCreature)
            {
                npcCreature->movement_gens.reset();
                if (npcCreature->isAlive()) // dead creature will reset movement
                                            // generator at respawn
                {
                    npcCreature->SetDeathState(JUST_DIED);
                    npcCreature->Respawn();
                }
            }
            PSendSysMessage(LANG_WAYPOINT_CHANGED);
        }
        return true;
    } // move

    // Create creature - npc that has the waypoint
    CreatureData const* data = sObjectMgr::Instance()->GetCreatureData(lowguid);
    if (!data)
    {
        PSendSysMessage(LANG_WAYPOINT_CREATNOTFOUND, lowguid);
        SetSentErrorMessage(true);
        return false;
    }

    // set in game textids not supported
    if (show == "textid1" || show == "textid2" || show == "textid3" ||
        show == "textid4" || show == "textid5")
    {
        return false;
    }

    sWaypointMgr::Instance()->SetNodeText(lowguid, point, show_str, arg_str);

    Creature* npcCreature = m_session->GetPlayer()->GetMap()->GetCreature(
        data->GetObjectGuid(lowguid));
    if (npcCreature)
    {
        npcCreature->set_default_movement_gen(movement::gen::waypoint);
        npcCreature->movement_gens.reset();
        if (npcCreature->isAlive()) // dead creature will reset movement
                                    // generator at respawn
        {
            npcCreature->SetDeathState(JUST_DIED);
            npcCreature->Respawn();
        }
    }
    PSendSysMessage(LANG_WAYPOINT_CHANGED_NO, show_str);

    return true;
}

/**
 * .wp show info | on | off
 *
 * info -> User has selected a visual waypoint before
 *
 * info <GUID> <WPNUM> -> User did not select a visual waypoint and
 *                        provided the GUID of the NPC and the number of
 *                        the waypoint.
 *
 * on -> User has selected an NPC; all visual waypoints for this
 *       NPC are added to the world
 *
 * on <GUID> -> User did not select an NPC - instead the GUID of the
 *              NPC is provided. All visual waypoints for this NPC
 *              are added from the world.
 *
 * off -> User has selected an NPC; all visual waypoints for this
 *        NPC are removed from the world.
 *
 * on <GUID> -> User did not select an NPC - instead the GUID of the
 *              NPC is provided. All visual waypoints for this NPC
 *              are removed from the world.
 *
 *
 */
bool ChatHandler::HandleWpShowCommand(char* args)
{
    LOG_DEBUG(logging, "DEBUG: HandleWpShowCommand");

    if (!*args)
        return false;

    CreatureInfo const* waypointInfo =
        ObjectMgr::GetCreatureTemplate(VISUAL_WAYPOINT);
    if (!waypointInfo || waypointInfo->GetHighGuid() != HIGHGUID_UNIT)
        return false; // must exist as normal creature in mangos.sql
                      // 'creature_template'

    // first arg: on, off, first, last
    char* show_str = strtok(args, " ");
    if (!show_str)
    {
        return false;
    }
    // second arg: GUID (optional, if a creature is selected)
    char* guid_str = strtok((char*)nullptr, " ");
    LOG_DEBUG(logging, "DEBUG: HandleWpShowCommand: show_str: %s guid_str: %s",
        show_str, guid_str);
    // if (!guid_str) {
    //    return false;
    //}

    // Did user provide a GUID
    // or did the user select a creature?
    // -> variable lowguid is filled with the GUID
    Creature* target = getSelectedCreature();
    // Did player provide a GUID?
    if (!guid_str)
    {
        LOG_DEBUG(logging, "DEBUG: HandleWpShowCommand: !guid_str");
        // No GUID provided
        // -> Player must have selected a creature

        if (!target)
        {
            SendSysMessage(LANG_SELECT_CREATURE);
            SetSentErrorMessage(true);
            return false;
        }
    }
    else
    {
        LOG_DEBUG(logging, "DEBUG: HandleWpShowCommand: GUID provided");
        // GUID provided
        // Warn if player also selected a creature
        // -> Creature selection is ignored <-
        if (target)
        {
            SendSysMessage(LANG_WAYPOINT_CREATSELECTED);
        }

        uint32 lowguid = atoi((char*)guid_str);

        CreatureData const* data =
            sObjectMgr::Instance()->GetCreatureData(lowguid);
        if (!data)
        {
            PSendSysMessage(LANG_WAYPOINT_CREATNOTFOUND, lowguid);
            SetSentErrorMessage(true);
            return false;
        }

        target = m_session->GetPlayer()->GetMap()->GetCreature(
            data->GetObjectGuid(lowguid));

        if (!target)
        {
            PSendSysMessage(LANG_WAYPOINT_CREATNOTFOUND, lowguid);
            SetSentErrorMessage(true);
            return false;
        }
    }

    uint32 lowguid = target->GetGUIDLow();

    std::string show = show_str;
    uint32 Maxpoint;

    LOG_DEBUG(logging, "DEBUG: HandleWpShowCommand: lowguid: %u show: %s",
        lowguid, show_str);

    // Show info for the selected waypoint
    if (show == "info")
    {
        PSendSysMessage("DEBUG: wp info, GUID: %u", lowguid);

        // Check if the user did specify a visual waypoint
        if (target->GetEntry() != VISUAL_WAYPOINT)
        {
            PSendSysMessage(LANG_WAYPOINT_VP_SELECT);
            SetSentErrorMessage(true);
            return false;
        }

        // PSendSysMessage("wp on, GUID: %u", lowguid);

        // pCreature->GetX();

        QueryResult* result = WorldDatabase.PQuery(
            "SELECT id, point, waittime, emote, spell, textid1, textid2, "
            "textid3, textid4, textid5, model1, model2 FROM creature_movement "
            "WHERE wpguid = %u",
            target->GetGUIDLow());
        if (!result)
        {
            // Since we compare float values, we have to deal with
            // some difficulties.
            // Here we search for all waypoints that only differ in one from 1
            // thousand
            // (0.001) - There is no other way to compare C++ floats with mySQL
            // floats
            // See also:
            // http://dev.mysql.com/doc/refman/5.0/en/problems-with-float.html
            const char* maxDIFF = "0.01";
            PSendSysMessage(LANG_WAYPOINT_NOTFOUNDSEARCH,
                target->GetObjectGuid().GetRawValue());

            result = WorldDatabase.PQuery(
                "SELECT id, point, waittime, emote, spell, textid1, textid2, "
                "textid3, textid4, textid5, model1, model2 FROM "
                "creature_movement WHERE (abs(position_x - %f) <= %s ) and "
                "(abs(position_y - %f) <= %s ) and (abs(position_z - %f) <= %s "
                ")",
                target->GetX(), maxDIFF, target->GetY(), maxDIFF,
                target->GetZ(), maxDIFF);
            if (!result)
            {
                PSendSysMessage(LANG_WAYPOINT_NOTFOUNDDBPROBLEM, lowguid);
                SetSentErrorMessage(true);
                return false;
            }
        }
        do
        {
            Field* fields = result->Fetch();
            uint32 wpGuid = fields[0].GetUInt32();
            uint32 point = fields[1].GetUInt32();
            int waittime = fields[2].GetUInt32();
            uint32 emote = fields[3].GetUInt32();
            uint32 spell = fields[4].GetUInt32();
            uint32 textid[MAX_WAYPOINT_TEXT];
            for (int i = 0; i < MAX_WAYPOINT_TEXT; ++i)
                textid[i] = fields[5 + i].GetUInt32();
            uint32 model1 = fields[10].GetUInt32();
            uint32 model2 = fields[11].GetUInt32();

            // Get the creature for which we read the waypoint
            Creature* wpCreature =
                m_session->GetPlayer()->GetMap()->GetCreature(
                    ObjectGuid(HIGHGUID_UNIT, VISUAL_WAYPOINT, wpGuid));

            PSendSysMessage(LANG_WAYPOINT_INFO_TITLE, point,
                (wpCreature ? wpCreature->GetName() : "<not found>"), wpGuid);
            PSendSysMessage(LANG_WAYPOINT_INFO_WAITTIME, waittime);
            PSendSysMessage(LANG_WAYPOINT_INFO_MODEL, 1, model1);
            PSendSysMessage(LANG_WAYPOINT_INFO_MODEL, 2, model2);
            PSendSysMessage(LANG_WAYPOINT_INFO_EMOTE, emote);
            PSendSysMessage(LANG_WAYPOINT_INFO_SPELL, spell);
            for (int i = 0; i < MAX_WAYPOINT_TEXT; ++i)
                PSendSysMessage(LANG_WAYPOINT_INFO_TEXT, i + 1, textid[i],
                    (textid[i] ? GetMangosString(textid[i]) : ""));

        } while (result->NextRow());
        // Cleanup memory
        delete result;
        return true;
    }

    if (show == "on")
    {
        PSendSysMessage("DEBUG: wp on, GUID: %u", lowguid);

        std::unique_ptr<QueryResult> result(WorldDatabase.PQuery(
            "SELECT point, position_x,position_y,position_z FROM "
            "creature_movement WHERE id = '%u'",
            lowguid));
        if (!result)
        {
            PSendSysMessage(LANG_WAYPOINT_NOTFOUND, lowguid);
            SetSentErrorMessage(true);
            return false;
        }
        // Delete all visuals for this NPC
        std::unique_ptr<QueryResult> result2(WorldDatabase.PQuery(
            "SELECT wpguid FROM creature_movement WHERE id = '%u' and wpguid "
            "<> 0",
            lowguid));
        if (result2)
        {
            bool hasError = false;
            do
            {
                Field* fields = result2->Fetch();
                uint32 wpGuid = fields[0].GetUInt32();
                Creature* pCreature =
                    m_session->GetPlayer()->GetMap()->GetCreature(
                        ObjectGuid(HIGHGUID_UNIT, VISUAL_WAYPOINT, wpGuid));

                if (!pCreature)
                {
                    PSendSysMessage(LANG_WAYPOINT_NOTREMOVED, wpGuid);
                    hasError = true;
                    WorldDatabase.PExecuteLog(
                        "DELETE FROM creature WHERE guid=%u", wpGuid);
                }
                else
                {
                    pCreature->DeleteFromDB();
                    pCreature->AddObjectToRemoveList();
                }

            } while (result2->NextRow());
            if (hasError)
            {
                PSendSysMessage(LANG_WAYPOINT_TOOFAR1);
                PSendSysMessage(LANG_WAYPOINT_TOOFAR2);
                PSendSysMessage(LANG_WAYPOINT_TOOFAR3);
            }
        }

        do
        {
            Player* chr = m_session->GetPlayer();
            Map* map = chr->GetMap();

            Field* fields = result->Fetch();
            uint32 point = fields[0].GetUInt32();
            CreatureCreatePos pos(map, fields[1].GetFloat(),
                fields[2].GetFloat(), fields[3].GetFloat(), chr->GetO());

            auto wpCreature = new Creature;

            if (!wpCreature->Create(map->GenerateLocalLowGuid(HIGHGUID_UNIT),
                    pos, waypointInfo))
            {
                PSendSysMessage(LANG_WAYPOINT_VP_NOTCREATED, VISUAL_WAYPOINT);
                delete wpCreature;
                return false;
            }

            wpCreature->SetVisibility(VISIBILITY_OFF);
            LOG_DEBUG(logging,
                "DEBUG: UPDATE creature_movement SET wpguid = '%u",
                wpCreature->GetGUIDLow());
            // set "wpguid" column to the visual waypoint
            WorldDatabase.PExecuteLog(
                "UPDATE creature_movement SET wpguid=%u WHERE id=%u and "
                "point=%u",
                wpCreature->GetGUIDLow(), lowguid, point);

            wpCreature->SaveToDB(map->GetId(), (1 << map->GetSpawnMode()));
            // To call _LoadGoods(); _LoadQuests(); CreateTrainerSpells();
            wpCreature->LoadFromDB(wpCreature->GetGUIDLow(), map);
            if (!map->insert(wpCreature))
            {
                SendSysMessage("Failed to insert into map");
                delete wpCreature;
                return true;
            }
        } while (result->NextRow());

        return true;
    }

    if (show == "first")
    {
        PSendSysMessage("DEBUG: wp first, GUID: %u", lowguid);

        std::unique_ptr<QueryResult> result(WorldDatabase.PQuery(
            "SELECT position_x,position_y,position_z FROM creature_movement "
            "WHERE point='1' AND id = '%u'",
            lowguid));
        if (!result)
        {
            PSendSysMessage(LANG_WAYPOINT_NOTFOUND, lowguid);
            SetSentErrorMessage(true);
            return false;
        }

        Player* chr = m_session->GetPlayer();
        Map* map = chr->GetMap();

        Field* fields = result->Fetch();
        CreatureCreatePos pos(map, fields[0].GetFloat(), fields[1].GetFloat(),
            fields[2].GetFloat(), chr->GetO());

        auto pCreature = new Creature;

        if (!pCreature->Create(
                map->GenerateLocalLowGuid(HIGHGUID_UNIT), pos, waypointInfo))
        {
            PSendSysMessage(LANG_WAYPOINT_VP_NOTCREATED, VISUAL_WAYPOINT);
            delete pCreature;
            return false;
        }

        pCreature->SaveToDB(map->GetId(), (1 << map->GetSpawnMode()));
        pCreature->LoadFromDB(pCreature->GetGUIDLow(), map);
        if (!map->insert(pCreature))
        {
            SendSysMessage("Failed to insert into map");
            delete pCreature;
            return true;
        }

        return true;
    }

    if (show == "last")
    {
        PSendSysMessage("DEBUG: wp last, GUID: %u", lowguid);

        std::unique_ptr<QueryResult> result(WorldDatabase.PQuery(
            "SELECT MAX(point) FROM creature_movement WHERE id = '%u'",
            lowguid));
        if (result)
            Maxpoint = (*result)[0].GetUInt32();
        else
            Maxpoint = 0;

        result.reset(WorldDatabase.PQuery(
            "SELECT position_x,position_y,position_z FROM creature_movement "
            "WHERE point ='%u' AND id = '%u'",
            Maxpoint, lowguid));
        if (!result)
        {
            PSendSysMessage(LANG_WAYPOINT_NOTFOUNDLAST, lowguid);
            SetSentErrorMessage(true);
            return false;
        }

        Player* chr = m_session->GetPlayer();
        Map* map = chr->GetMap();

        Field* fields = result->Fetch();
        CreatureCreatePos pos(map, fields[0].GetFloat(), fields[1].GetFloat(),
            fields[2].GetFloat(), chr->GetO());

        auto pCreature = new Creature;

        if (!pCreature->Create(
                map->GenerateLocalLowGuid(HIGHGUID_UNIT), pos, waypointInfo))
        {
            PSendSysMessage(LANG_WAYPOINT_NOTCREATED, VISUAL_WAYPOINT);
            delete pCreature;
            return false;
        }

        pCreature->SaveToDB(map->GetId(), (1 << map->GetSpawnMode()));
        pCreature->LoadFromDB(pCreature->GetGUIDLow(), map);
        if (!map->insert(pCreature))
        {
            SendSysMessage("Failed to insert into map");
            delete pCreature;
            return true;
        }
        return true;
    }

    if (show == "off")
    {
        std::unique_ptr<QueryResult> result(WorldDatabase.PQuery(
            "SELECT guid FROM creature WHERE id=%u", VISUAL_WAYPOINT));
        if (!result)
        {
            SendSysMessage(LANG_WAYPOINT_VP_NOTFOUND);
            SetSentErrorMessage(true);
            return false;
        }
        bool hasError = false;
        do
        {
            Field* fields = result->Fetch();
            uint32 wpGuid = fields[0].GetUInt32();
            Creature* pCreature = m_session->GetPlayer()->GetMap()->GetCreature(
                ObjectGuid(HIGHGUID_UNIT, VISUAL_WAYPOINT, wpGuid));
            if (!pCreature)
            {
                PSendSysMessage(LANG_WAYPOINT_NOTREMOVED, wpGuid);
                hasError = true;
                WorldDatabase.PExecuteLog(
                    "DELETE FROM creature WHERE guid=%u", wpGuid);
            }
            else
            {
                pCreature->DeleteFromDB();
                pCreature->AddObjectToRemoveList();
            }
        } while (result->NextRow());
        // set "wpguid" column to "empty" - no visual waypoint spawned
        WorldDatabase.PExecuteLog(
            "UPDATE creature_movement SET wpguid=0 WHERE wpguid <> 0");

        if (hasError)
        {
            PSendSysMessage(LANG_WAYPOINT_TOOFAR1);
            PSendSysMessage(LANG_WAYPOINT_TOOFAR2);
            PSendSysMessage(LANG_WAYPOINT_TOOFAR3);
        }

        SendSysMessage(LANG_WAYPOINT_VP_ALLREMOVED);
        return true;
    }

    PSendSysMessage("DEBUG: wpshow - no valid command found");

    return true;
} // HandleWpShowCommand

bool ChatHandler::HandleWpExportCommand(char* args)
{
    if (!*args)
        return false;

    // Next arg is: <GUID> <ARGUMENT>

    // Did user provide a GUID
    // or did the user select a creature?
    // -> variable lowguid is filled with the GUID of the NPC
    uint32 lowguid = 0;
    Creature* target = getSelectedCreature();
    char* arg_str = nullptr;
    if (target)
    {
        if (target->GetEntry() != VISUAL_WAYPOINT)
            lowguid = target->GetGUIDLow();
        else
        {
            std::unique_ptr<QueryResult> result(WorldDatabase.PQuery(
                "SELECT id FROM creature_movement WHERE wpguid = %u LIMIT 1",
                target->GetGUIDLow()));
            if (!result)
            {
                PSendSysMessage(
                    LANG_WAYPOINT_NOTFOUNDDBPROBLEM, target->GetGUIDLow());
                return true;
            }
            Field* fields = result->Fetch();
            lowguid = fields[0].GetUInt32();
            ;
        }

        arg_str = strtok(args, " ");
    }
    else
    {
        // user provided <GUID>
        char* guid_str = strtok(args, " ");
        if (!guid_str)
        {
            SendSysMessage(LANG_WAYPOINT_NOGUID);
            return false;
        }
        lowguid = atoi((char*)guid_str);

        arg_str = strtok((char*)nullptr, " ");
    }

    if (!arg_str)
    {
        PSendSysMessage(LANG_WAYPOINT_ARGUMENTREQ, "export");
        return false;
    }

    PSendSysMessage("DEBUG: wp export, GUID: %u", lowguid);

    std::unique_ptr<QueryResult> result(WorldDatabase.PQuery(
        //          0      1           2           3           4            5
        //          6       7         8      9      10       11       12
        //          13       14       15
        "SELECT point, position_x, position_y, position_z, orientation, "
        "model1, model2, waittime, emote, spell, textid1, textid2, textid3, "
        "textid4, textid5, id FROM creature_movement WHERE id = '%u' ORDER BY "
        "point",
        lowguid));

    if (!result)
    {
        PSendSysMessage(LANG_WAYPOINT_NOTHINGTOEXPORT);
        SetSentErrorMessage(true);
        return false;
    }

    std::ofstream outfile;
    outfile.open(arg_str);

    do
    {
        Field* fields = result->Fetch();

        outfile << "INSERT INTO creature_movement ";
        outfile << "(id, point, position_x, position_y, position_z, "
                   "orientation, model1, model2, waittime, emote, spell, "
                   "textid1, textid2, textid3, textid4, textid5) VALUES ";

        outfile << "( ";
        outfile << fields[15].GetUInt32(); // id
        outfile << ", ";
        outfile << fields[0].GetUInt32(); // point
        outfile << ", ";
        outfile << fields[1].GetFloat(); // position_x
        outfile << ", ";
        outfile << fields[2].GetFloat(); // position_y
        outfile << ", ";
        outfile << fields[3].GetUInt32(); // position_z
        outfile << ", ";
        outfile << fields[4].GetUInt32(); // orientation
        outfile << ", ";
        outfile << fields[5].GetUInt32(); // model1
        outfile << ", ";
        outfile << fields[6].GetUInt32(); // model2
        outfile << ", ";
        outfile << fields[7].GetUInt16(); // waittime
        outfile << ", ";
        outfile << fields[8].GetUInt32(); // emote
        outfile << ", ";
        outfile << fields[9].GetUInt32(); // spell
        outfile << ", ";
        outfile << fields[10].GetUInt32(); // textid1
        outfile << ", ";
        outfile << fields[11].GetUInt32(); // textid2
        outfile << ", ";
        outfile << fields[12].GetUInt32(); // textid3
        outfile << ", ";
        outfile << fields[13].GetUInt32(); // textid4
        outfile << ", ";
        outfile << fields[14].GetUInt32(); // textid5
        outfile << ");\n ";

    } while (result->NextRow());

    PSendSysMessage(LANG_WAYPOINT_EXPORTED);
    outfile.close();

    return true;
}

bool ChatHandler::HandleWpImportCommand(char* args)
{
    if (!*args)
        return false;

    char* arg_str = strtok(args, " ");
    if (!arg_str)
        return false;

    std::string line;
    std::ifstream infile(arg_str);
    if (infile.is_open())
    {
        while (!infile.eof())
        {
            getline(infile, line);
            // cout << line << endl;
            QueryResult* result = WorldDatabase.Query(line.c_str());
            delete result;
        }
        infile.close();
    }
    PSendSysMessage(LANG_WAYPOINT_IMPORTED);

    return true;
}

// rename characters
bool ChatHandler::HandleCharacterRenameCommand(char* args)
{
    Player* target;
    ObjectGuid target_guid;
    std::string target_name;
    if (!ExtractPlayerTarget(&args, &target, &target_guid, &target_name))
        return false;

    if (target)
    {
        // check online security
        if (HasLowerSecurity(target))
            return false;

        PSendSysMessage(LANG_RENAME_PLAYER, GetNameLink(target).c_str());
        target->SetAtLoginFlag(AT_LOGIN_RENAME);
        CharacterDatabase.PExecute(
            "UPDATE characters SET at_login = at_login | '1' WHERE guid = '%u'",
            target->GetGUIDLow());
    }
    else
    {
        // check offline security
        if (HasLowerSecurity(nullptr, target_guid))
            return false;

        std::string oldNameLink = playerLink(target_name);

        PSendSysMessage(LANG_RENAME_PLAYER_GUID, oldNameLink.c_str(),
            target_guid.GetCounter());
        CharacterDatabase.PExecute(
            "UPDATE characters SET at_login = at_login | '1' WHERE guid = '%u'",
            target_guid.GetCounter());
    }

    return true;
}

bool ChatHandler::HandleCharacterReputationCommand(char* args)
{
    Player* target;
    if (!ExtractPlayerTarget(&args, &target))
        return false;

    LocaleConstant loc = GetSessionDbcLocale();

    FactionStateList const& targetFSL =
        target->GetReputationMgr().GetStateList();
    for (const auto& elem : targetFSL)
    {
        FactionEntry const* factionEntry =
            sFactionStore.LookupEntry(elem.second.ID);

        ShowFactionListHelper(factionEntry, loc, &elem.second, target);
    }
    return true;
}

// change standstate
bool ChatHandler::HandleModifyStandStateCommand(char* args)
{
    uint32 anim_id;
    if (!ExtractUInt32(&args, anim_id))
        return false;

    if (!sEmotesStore.LookupEntry(anim_id))
        return false;

    m_session->GetPlayer()->HandleEmoteState(anim_id);

    return true;
}

bool ChatHandler::HandleHonorAddCommand(char* args)
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

    float amount = (float)atof(args);
    target->RewardHonor(nullptr, 1, amount);
    return true;
}

bool ChatHandler::HandleHonorAddKillCommand(char* /*args*/)
{
    Unit* target = getSelectedUnit();
    if (!target)
    {
        SendSysMessage(LANG_PLAYER_NOT_FOUND);
        SetSentErrorMessage(true);
        return false;
    }

    // check online security
    if (target->GetTypeId() == TYPEID_PLAYER &&
        HasLowerSecurity((Player*)target))
        return false;

    m_session->GetPlayer()->RewardHonor(target, 1);
    return true;
}

bool ChatHandler::HandleHonorUpdateCommand(char* /*args*/)
{
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

    target->UpdateHonorFields();
    return true;
}

bool ChatHandler::HandleLookupEventCommand(char* args)
{
    if (!*args)
        return false;

    std::string namepart = args;
    std::wstring wnamepart;

    // converting string that we try to find to lower case
    if (!Utf8toWStr(namepart, wnamepart))
        return false;

    wstrToLower(wnamepart);

    uint32 counter = 0;

    GameEventMgr::GameEventDataMap const& events =
        sGameEventMgr::Instance()->GetEventMap();

    for (uint32 id = 1; id < events.size(); ++id)
    {
        if (!sGameEventMgr::Instance()->IsValidEvent(id))
            continue;

        GameEventData const& eventData = events[id];

        std::string descr = eventData.description;
        if (descr.empty())
            continue;

        if (Utf8FitTo(descr, wnamepart))
        {
            char const* active = sGameEventMgr::Instance()->IsActiveEvent(id) ?
                                     GetMangosString(LANG_ACTIVE) :
                                     "";

            if (m_session)
                PSendSysMessage(LANG_EVENT_ENTRY_LIST_CHAT, id, id,
                    eventData.description.c_str(), active);
            else
                PSendSysMessage(LANG_EVENT_ENTRY_LIST_CONSOLE, id,
                    eventData.description.c_str(), active);

            ++counter;
        }
    }

    if (counter == 0)
        SendSysMessage(LANG_NOEVENTFOUND);

    return true;
}

bool ChatHandler::HandleEventListCommand(char* args)
{
    uint32 counter = 0;
    bool all = false;
    std::string arg = args;
    if (arg == "all")
        all = true;

    GameEventMgr::GameEventDataMap const& events =
        sGameEventMgr::Instance()->GetEventMap();

    char const* active = GetMangosString(LANG_ACTIVE);
    char const* inactive = GetMangosString(LANG_FACTION_INACTIVE);
    char const* state = "";

    for (uint32 event_id = 0; event_id < events.size(); ++event_id)
    {
        if (!sGameEventMgr::Instance()->IsValidEvent(event_id))
            continue;

        if (!sGameEventMgr::Instance()->IsActiveEvent(event_id))
        {
            if (!all)
                continue;
            state = inactive;
        }
        else
            state = active;

        GameEventData const& eventData = events[event_id];

        if (m_session)
            PSendSysMessage(LANG_EVENT_ENTRY_LIST_CHAT, event_id, event_id,
                eventData.description.c_str(), state);
        else
            PSendSysMessage(LANG_EVENT_ENTRY_LIST_CONSOLE, event_id,
                eventData.description.c_str(), state);

        ++counter;
    }

    if (counter == 0)
        SendSysMessage(LANG_NOEVENTFOUND);

    return true;
}

bool ChatHandler::HandleEventInfoCommand(char* args)
{
    if (!*args)
        return false;

    // id or [name] Shift-click form |color|Hgameevent:id|h[name]|h|r
    uint32 event_id;
    if (!ExtractUint32KeyFromLink(&args, "Hgameevent", event_id))
        return false;

    GameEventMgr::GameEventDataMap const& events =
        sGameEventMgr::Instance()->GetEventMap();

    if (!sGameEventMgr::Instance()->IsValidEvent(event_id))
    {
        SendSysMessage(LANG_EVENT_NOT_EXIST);
        SetSentErrorMessage(true);
        return false;
    }

    GameEventData const& eventData = events[event_id];

    char const* activeStr = sGameEventMgr::Instance()->IsActiveEvent(event_id) ?
                                GetMangosString(LANG_ACTIVE) :
                                "";

    std::string startTimeStr = TimeToTimestampStr(eventData.start);
    std::string endTimeStr = TimeToTimestampStr(eventData.end);

    uint32 delay = sGameEventMgr::Instance()->NextCheck(event_id);
    time_t nextTime = WorldTimer::time_no_syscall() + delay;
    std::string nextStr =
        nextTime >= eventData.start && nextTime < eventData.end ?
            TimeToTimestampStr(WorldTimer::time_no_syscall() + delay) :
            "-";

    std::string occurenceStr = secsToTimeString(eventData.occurence * MINUTE);
    std::string lengthStr = secsToTimeString(eventData.length * MINUTE);

    PSendSysMessage(LANG_EVENT_INFO, event_id, eventData.description.c_str(),
        activeStr, startTimeStr.c_str(), endTimeStr.c_str(),
        occurenceStr.c_str(), lengthStr.c_str(), nextStr.c_str());
    return true;
}

bool ChatHandler::HandleEventStartCommand(char* args)
{
    if (!*args)
        return false;

    // id or [name] Shift-click form |color|Hgameevent:id|h[name]|h|r
    uint32 event_id;
    if (!ExtractUint32KeyFromLink(&args, "Hgameevent", event_id))
        return false;

    GameEventMgr::GameEventDataMap const& events =
        sGameEventMgr::Instance()->GetEventMap();

    if (!sGameEventMgr::Instance()->IsValidEvent(event_id))
    {
        SendSysMessage(LANG_EVENT_NOT_EXIST);
        SetSentErrorMessage(true);
        return false;
    }

    GameEventData const& eventData = events[event_id];
    if (!eventData.isValid())
    {
        SendSysMessage(LANG_EVENT_NOT_EXIST);
        SetSentErrorMessage(true);
        return false;
    }

    if (sGameEventMgr::Instance()->IsActiveEvent(event_id))
    {
        PSendSysMessage(LANG_EVENT_ALREADY_ACTIVE, event_id);
        SetSentErrorMessage(true);
        return false;
    }

    PSendSysMessage(
        LANG_EVENT_STARTED, event_id, eventData.description.c_str());
    sGameEventMgr::Instance()->StartEvent(event_id, true);
    return true;
}

bool ChatHandler::HandleEventStopCommand(char* args)
{
    if (!*args)
        return false;

    // id or [name] Shift-click form |color|Hgameevent:id|h[name]|h|r
    uint32 event_id;
    if (!ExtractUint32KeyFromLink(&args, "Hgameevent", event_id))
        return false;

    GameEventMgr::GameEventDataMap const& events =
        sGameEventMgr::Instance()->GetEventMap();

    if (!sGameEventMgr::Instance()->IsValidEvent(event_id))
    {
        SendSysMessage(LANG_EVENT_NOT_EXIST);
        SetSentErrorMessage(true);
        return false;
    }

    GameEventData const& eventData = events[event_id];
    if (!eventData.isValid())
    {
        SendSysMessage(LANG_EVENT_NOT_EXIST);
        SetSentErrorMessage(true);
        return false;
    }

    if (!sGameEventMgr::Instance()->IsActiveEvent(event_id))
    {
        PSendSysMessage(LANG_EVENT_NOT_ACTIVE, event_id);
        SetSentErrorMessage(true);
        return false;
    }

    PSendSysMessage(
        LANG_EVENT_STOPPED, event_id, eventData.description.c_str());
    sGameEventMgr::Instance()->StopEvent(event_id, true);
    return true;
}

bool ChatHandler::HandleCombatStopCommand(char* args)
{
    Player* target;
    if (!ExtractPlayerTarget(&args, &target))
        return false;

    // check online security
    if (HasLowerSecurity(target))
        return false;

    target->CombatStop();
    target->getHostileRefManager().deleteReferences();
    return true;
}

void ChatHandler::HandleLearnSkillRecipesHelper(Player* player, uint32 skill_id)
{
    uint32 classmask = player->getClassMask();

    for (uint32 j = 0; j < sSkillLineAbilityStore.GetNumRows(); ++j)
    {
        SkillLineAbilityEntry const* skillLine =
            sSkillLineAbilityStore.LookupEntry(j);
        if (!skillLine)
            continue;

        // wrong skill
        if (skillLine->skillId != skill_id)
            continue;

        // not high rank
        if (skillLine->forward_spellid)
            continue;

        // skip racial skills
        if (skillLine->racemask != 0)
            continue;

        // skip wrong class skills
        if (skillLine->classmask && (skillLine->classmask & classmask) == 0)
            continue;

        SpellEntry const* spellInfo =
            sSpellStore.LookupEntry(skillLine->spellId);
        if (!spellInfo || !SpellMgr::IsSpellValid(spellInfo, player, false))
            continue;

        player->learnSpell(skillLine->spellId, false);
    }
}

bool ChatHandler::HandleLearnAllCraftsCommand(char* /*args*/)
{
    for (uint32 i = 0; i < sSkillLineStore.GetNumRows(); ++i)
    {
        SkillLineEntry const* skillInfo = sSkillLineStore.LookupEntry(i);
        if (!skillInfo)
            continue;

        if (skillInfo->categoryId == SKILL_CATEGORY_PROFESSION ||
            skillInfo->categoryId == SKILL_CATEGORY_SECONDARY)
        {
            HandleLearnSkillRecipesHelper(
                m_session->GetPlayer(), skillInfo->id);
        }
    }

    SendSysMessage(LANG_COMMAND_LEARN_ALL_CRAFT);
    return true;
}

bool ChatHandler::HandleLearnAllRecipesCommand(char* args)
{
    //  Learns all recipes of specified profession and sets skill to max
    //  Example: .learn all_recipes enchanting

    Player* target = getSelectedPlayer();
    if (!target)
    {
        SendSysMessage(LANG_PLAYER_NOT_FOUND);
        return false;
    }

    if (!*args)
        return false;

    std::wstring wnamepart;

    if (!Utf8toWStr(args, wnamepart))
        return false;

    // converting string that we try to find to lower case
    wstrToLower(wnamepart);

    std::string name;

    SkillLineEntry const* targetSkillInfo = nullptr;
    for (uint32 i = 1; i < sSkillLineStore.GetNumRows(); ++i)
    {
        SkillLineEntry const* skillInfo = sSkillLineStore.LookupEntry(i);
        if (!skillInfo)
            continue;

        if (skillInfo->categoryId != SKILL_CATEGORY_PROFESSION &&
            skillInfo->categoryId != SKILL_CATEGORY_SECONDARY)
            continue;

        int loc = GetSessionDbcLocale();
        name = skillInfo->name[loc];
        if (name.empty())
            continue;

        if (!Utf8FitTo(name, wnamepart))
        {
            loc = 0;
            for (; loc < MAX_LOCALE; ++loc)
            {
                if (loc == GetSessionDbcLocale())
                    continue;

                name = skillInfo->name[loc];
                if (name.empty())
                    continue;

                if (Utf8FitTo(name, wnamepart))
                    break;
            }
        }

        if (loc < MAX_LOCALE)
        {
            targetSkillInfo = skillInfo;
            break;
        }
    }

    if (!targetSkillInfo)
        return false;

    HandleLearnSkillRecipesHelper(target, targetSkillInfo->id);

    uint16 maxLevel = target->GetPureMaxSkillValue(targetSkillInfo->id);
    target->SetSkill(targetSkillInfo->id, maxLevel, maxLevel);
    PSendSysMessage(LANG_COMMAND_LEARN_ALL_RECIPES, name.c_str());
    return true;
}

bool ChatHandler::HandleLookupAccountEmailCommand(char* args)
{
    char* emailStr = ExtractQuotedOrLiteralArg(&args);
    if (!emailStr)
        return false;

    uint32 limit;
    if (!ExtractOptUInt32(&args, limit, 100))
        return false;

    std::string email = emailStr;
    LoginDatabase.escape_string(email);
    //                                                 0   1         2        3
    //                                                 4
    std::unique_ptr<QueryResult> result(LoginDatabase.PQuery(
        "SELECT id, username, last_ip, gmlevel, expansion FROM account WHERE "
        "email " _LIKE_ " " _CONCAT3_("'%%'", "'%s'", "'%%'"),
        email.c_str()));

    return ShowAccountListHelper(result.get(), &limit);
}

bool ChatHandler::HandleLookupAccountIpCommand(char* args)
{
    char* ipStr = ExtractQuotedOrLiteralArg(&args);
    if (!ipStr)
        return false;

    uint32 limit;
    if (!ExtractOptUInt32(&args, limit, 100))
        return false;

    std::string ip = ipStr;
    LoginDatabase.escape_string(ip);

    //                                                 0   1         2        3
    //                                                 4
    std::unique_ptr<QueryResult> result(LoginDatabase.PQuery(
        "SELECT id, username, last_ip, gmlevel, expansion FROM account WHERE "
        "last_ip " _LIKE_ " " _CONCAT3_("'%%'", "'%s'", "'%%'"),
        ip.c_str()));

    return ShowAccountListHelper(result.get(), &limit);
}

bool ChatHandler::HandleLookupAccountNameCommand(char* args)
{
    char* accountStr = ExtractQuotedOrLiteralArg(&args);
    if (!accountStr)
        return false;

    uint32 limit;
    if (!ExtractOptUInt32(&args, limit, 100))
        return false;

    std::string account = accountStr;
    if (!AccountMgr::normalizeString(account))
        return false;

    LoginDatabase.escape_string(account);
    //                                                 0   1         2        3
    //                                                 4
    std::unique_ptr<QueryResult> result(LoginDatabase.PQuery(
        "SELECT id, username, last_ip, gmlevel, expansion FROM account WHERE "
        "username " _LIKE_ " " _CONCAT3_("'%%'", "'%s'", "'%%'"),
        account.c_str()));

    return ShowAccountListHelper(result.get(), &limit);
}

bool ChatHandler::ShowAccountListHelper(
    QueryResult* result, uint32* limit, bool title, bool error)
{
    if (!result)
    {
        if (error)
            SendSysMessage(LANG_ACCOUNT_LIST_EMPTY);
        return true;
    }

    ///- Display the list of account/characters online
    if (!m_session && title) // not output header for online case
    {
        SendSysMessage(LANG_ACCOUNT_LIST_BAR);
        SendSysMessage(LANG_ACCOUNT_LIST_HEADER);
        SendSysMessage(LANG_ACCOUNT_LIST_BAR);
    }

    ///- Circle through accounts
    do
    {
        // check limit
        if (limit)
        {
            if (*limit == 0)
                break;
            --*limit;
        }

        Field* fields = result->Fetch();
        uint32 account = fields[0].GetUInt32();

        WorldSession* session = sWorld::Instance()->FindSession(account);
        Player* player = session ? session->GetPlayer() : nullptr;
        char const* char_name = player ? player->GetName() : " - ";

        if (m_session)
            PSendSysMessage(LANG_ACCOUNT_LIST_LINE_CHAT, account,
                fields[1].GetString(), char_name, fields[2].GetString(),
                fields[3].GetUInt32(), fields[4].GetUInt32());
        else
            PSendSysMessage(LANG_ACCOUNT_LIST_LINE_CONSOLE, account,
                fields[1].GetString(), char_name, fields[2].GetString(),
                fields[3].GetUInt32(), fields[4].GetUInt32());

    } while (result->NextRow());

    if (!m_session) // not output header for online case
        SendSysMessage(LANG_ACCOUNT_LIST_BAR);

    return true;
}

bool ChatHandler::HandleLookupPlayerIpCommand(char* args)
{
    char* ipStr = ExtractQuotedOrLiteralArg(&args);
    if (!ipStr)
        return false;

    uint32 limit;
    if (!ExtractOptUInt32(&args, limit, 100))
        return false;

    std::string ip = ipStr;
    LoginDatabase.escape_string(ip);

    std::unique_ptr<QueryResult> result(LoginDatabase.PQuery(
        "SELECT id,username FROM account WHERE last_ip " _LIKE_
        " " _CONCAT3_("'%%'", "'%s'", "'%%'"),
        ip.c_str()));

    return LookupPlayerSearchCommand(result.get(), &limit);
}

bool ChatHandler::HandleLookupPlayerAccountCommand(char* args)
{
    char* accountStr = ExtractQuotedOrLiteralArg(&args);
    if (!accountStr)
        return false;

    uint32 limit;
    if (!ExtractOptUInt32(&args, limit, 100))
        return false;

    std::string account = accountStr;
    if (!AccountMgr::normalizeString(account))
        return false;

    LoginDatabase.escape_string(account);

    std::unique_ptr<QueryResult> result(LoginDatabase.PQuery(
        "SELECT id,username FROM account WHERE username " _LIKE_
        " " _CONCAT3_("'%%'", "'%s'", "'%%'"),
        account.c_str()));

    return LookupPlayerSearchCommand(result.get(), &limit);
}

bool ChatHandler::HandleLookupPlayerEmailCommand(char* args)
{
    char* emailStr = ExtractQuotedOrLiteralArg(&args);
    if (!emailStr)
        return false;

    uint32 limit;
    if (!ExtractOptUInt32(&args, limit, 100))
        return false;

    std::string email = emailStr;
    LoginDatabase.escape_string(email);

    std::unique_ptr<QueryResult> result(LoginDatabase.PQuery(
        "SELECT id,username FROM account WHERE email " _LIKE_
        " " _CONCAT3_("'%%'", "'%s'", "'%%'"),
        email.c_str()));

    return LookupPlayerSearchCommand(result.get(), &limit);
}

bool ChatHandler::LookupPlayerSearchCommand(QueryResult* result, uint32* limit)
{
    if (!result)
    {
        PSendSysMessage(LANG_NO_PLAYERS_FOUND);
        SetSentErrorMessage(true);
        return false;
    }

    uint32 limit_original = limit ? *limit : 100;

    uint32 limit_local = limit_original;

    if (!limit)
        limit = &limit_local;

    do
    {
        if (limit && *limit == 0)
            break;

        Field* fields = result->Fetch();
        uint32 acc_id = fields[0].GetUInt32();
        std::string acc_name = fields[1].GetCppString();

        ///- Get the characters for account id
        std::unique_ptr<QueryResult> chars(CharacterDatabase.PQuery(
            "SELECT guid, name, race, class, level FROM characters WHERE "
            "account = %u",
            acc_id));
        if (chars)
        {
            if (chars->GetRowCount())
            {
                PSendSysMessage(
                    LANG_LOOKUP_PLAYER_ACCOUNT, acc_name.c_str(), acc_id);
                ShowPlayerListHelper(chars.get(), limit, true, false);
            }
        }
    } while (result->NextRow());

    if (*limit == limit_original) // empty accounts only
    {
        PSendSysMessage(LANG_NO_PLAYERS_FOUND);
        SetSentErrorMessage(true);
        return false;
    }

    return true;
}

void ChatHandler::ShowPoolListHelper(uint16 pool_id)
{
    PoolTemplateData const& pool_template =
        sPoolMgr::Instance()->GetPoolTemplate(pool_id);
    if (m_session)
        PSendSysMessage(LANG_POOL_ENTRY_LIST_CHAT, pool_id, pool_id,
            pool_template.description.c_str(), pool_template.AutoSpawn ? 1 : 0,
            pool_template.MaxLimit,
            sPoolMgr::Instance()->GetPoolCreatures(pool_id).size(),
            sPoolMgr::Instance()->GetPoolGameObjects(pool_id).size(),
            sPoolMgr::Instance()->GetPoolPools(pool_id).size());
    else
        PSendSysMessage(LANG_POOL_ENTRY_LIST_CONSOLE, pool_id,
            pool_template.description.c_str(), pool_template.AutoSpawn ? 1 : 0,
            pool_template.MaxLimit,
            sPoolMgr::Instance()->GetPoolCreatures(pool_id).size(),
            sPoolMgr::Instance()->GetPoolGameObjects(pool_id).size(),
            sPoolMgr::Instance()->GetPoolPools(pool_id).size());
}

bool ChatHandler::HandleLookupPoolCommand(char* args)
{
    if (!*args)
        return false;

    std::string namepart = args;

    strToLower(namepart);

    uint32 counter = 0;

    // spawn pools for expected map or for not initialized shared pools state
    // for non-instanceable maps
    for (auto& pool : sPoolMgr::Instance()->GetPoolTemplateMap())
    {
        std::string desc = pool.second.description;
        strToLower(desc);

        if (desc.find(namepart) == std::wstring::npos)
            continue;

        ShowPoolListHelper(pool.first);
        ++counter;
    }

    if (counter == 0)
        SendSysMessage(LANG_NO_POOL);

    return true;
}

bool ChatHandler::HandlePoolListCommand(char* /*args*/)
{
    Player* player = m_session->GetPlayer();

    MapPersistentState* mapState = player->GetMap()->GetPersistentState();

    if (!mapState || !mapState->GetMapEntry()->Instanceable())
    {
        PSendSysMessage(LANG_POOL_LIST_NON_INSTANCE,
            mapState->GetMapEntry()->name[GetSessionDbcLocale()],
            mapState->GetMapId());
        SetSentErrorMessage(false);
        return false;
    }

    uint32 counter = 0;

    // spawn pools for expected map or for not initialized shared pools state
    // for non-instanceable maps
    for (auto& pool : sPoolMgr::Instance()->GetPoolTemplateMap())
    {
        if (pool.second.CanBeSpawnedAtMap(mapState->GetMapEntry()))
        {
            ShowPoolListHelper(pool.first);
            ++counter;
        }
    }

    if (counter == 0)
        PSendSysMessage(LANG_NO_POOL_FOR_MAP,
            mapState->GetMapEntry()->name[GetSessionDbcLocale()],
            mapState->GetMapId());

    return true;
}

bool ChatHandler::HandlePoolSpawnsCommand(char* args)
{
    Player* player = m_session->GetPlayer();

    MapPersistentState* mapState = player->GetMap()->GetPersistentState();
    if (!mapState)
        return false;

    // shared continent pools data expected too big for show
    uint32 pool_id = 0;
    if (!ExtractUint32KeyFromLink(&args, "Hpool", pool_id) &&
        !mapState->GetMapEntry()->Instanceable())
    {
        PSendSysMessage(LANG_POOL_SPAWNS_NON_INSTANCE,
            mapState->GetMapEntry()->name[GetSessionDbcLocale()],
            mapState->GetMapId());
        SetSentErrorMessage(false);
        return false;
    }

    SpawnedPoolData const& spawns = mapState->GetSpawnedPoolData();

    SpawnedPoolObjects const& crSpawns = spawns.GetSpawnedCreatures();
    for (const auto& crSpawn : crSpawns)
        if (!pool_id ||
            pool_id == sPoolMgr::Instance()->IsPartOfAPool<Creature>(crSpawn))
            if (CreatureData const* data =
                    sObjectMgr::Instance()->GetCreatureData(crSpawn))
                if (CreatureInfo const* info =
                        ObjectMgr::GetCreatureTemplate(data->id))
                    PSendSysMessage(LANG_CREATURE_LIST_CHAT, crSpawn,
                        PrepareStringNpcOrGoSpawnInformation<Creature>(crSpawn)
                            .c_str(),
                        crSpawn, info->Name, data->posX, data->posY, data->posZ,
                        data->mapid);

    SpawnedPoolObjects const& goSpawns = spawns.GetSpawnedGameobjects();
    for (const auto& goSpawn : goSpawns)
        if (!pool_id ||
            pool_id == sPoolMgr::Instance()->IsPartOfAPool<GameObject>(goSpawn))
            if (GameObjectData const* data =
                    sObjectMgr::Instance()->GetGOData(goSpawn))
                if (GameObjectInfo const* info =
                        ObjectMgr::GetGameObjectInfo(data->id))
                    PSendSysMessage(LANG_GO_LIST_CHAT, goSpawn,
                        PrepareStringNpcOrGoSpawnInformation<GameObject>(
                            goSpawn).c_str(),
                        goSpawn, info->name, data->posX, data->posY, data->posZ,
                        data->mapid);

    return true;
}

bool ChatHandler::HandlePoolInfoCommand(char* args)
{
    // id or [name] Shift-click form |color|Hpool:id|h[name]|h|r
    uint32 pool_id;
    if (!ExtractUint32KeyFromLink(&args, "Hpool", pool_id))
        return false;

    Player* player = m_session ? m_session->GetPlayer() : nullptr;

    MapPersistentState* mapState =
        player ? player->GetMap()->GetPersistentState() : nullptr;
    if (!mapState)
        return false;

    SpawnedPoolData const* spawns =
        mapState ? &mapState->GetSpawnedPoolData() : nullptr;

    std::string active_str = GetMangosString(LANG_ACTIVE);

    auto& map = sPoolMgr::Instance()->GetPoolTemplateMap();
    if (map.find(pool_id) == map.end())
    {
        SendSysMessage("No pool with that id exists.");
        return true;
    }

    PoolTemplateData const& pool_template =
        sPoolMgr::Instance()->GetPoolTemplate(pool_id);
    uint32 mother_pool_id = sPoolMgr::Instance()->IsPartOfAPool<Pool>(pool_id);
    if (!mother_pool_id)
        PSendSysMessage(LANG_POOL_INFO_HEADER, pool_id, pool_template.AutoSpawn,
            pool_template.MaxLimit);
    else
    {
        PoolTemplateData const& mother_template =
            sPoolMgr::Instance()->GetPoolTemplate(mother_pool_id);
        if (m_session)
            PSendSysMessage(LANG_POOL_INFO_HEADER_CHAT, pool_id, mother_pool_id,
                mother_pool_id, mother_template.description.c_str(),
                pool_template.AutoSpawn, pool_template.MaxLimit);
        else
            PSendSysMessage(LANG_POOL_INFO_HEADER_CONSOLE, pool_id,
                mother_pool_id, mother_template.description.c_str(),
                pool_template.AutoSpawn, pool_template.MaxLimit);
    }

    PoolGroup<Creature> const& poolCreatures =
        sPoolMgr::Instance()->GetPoolCreatures(pool_id);
    SpawnedPoolObjects const* crSpawns =
        spawns ? &spawns->GetSpawnedCreatures() : nullptr;

    PoolObjectList const& poolCreaturesEx =
        poolCreatures.GetExplicitlyChanced();
    if (!poolCreaturesEx.empty())
    {
        SendSysMessage(LANG_POOL_CHANCE_CREATURE_LIST_HEADER);
        for (const auto& elem : poolCreaturesEx)
        {
            if (CreatureData const* data =
                    sObjectMgr::Instance()->GetCreatureData(elem.guid))
            {
                if (CreatureInfo const* info =
                        ObjectMgr::GetCreatureTemplate(data->id))
                {
                    char const* active =
                        crSpawns &&
                                crSpawns->find(elem.guid) != crSpawns->end() ?
                            active_str.c_str() :
                            "";
                    if (m_session)
                        PSendSysMessage(LANG_POOL_CHANCE_CREATURE_LIST_CHAT,
                            elem.guid,
                            PrepareStringNpcOrGoSpawnInformation<Creature>(
                                elem.guid).c_str(),
                            elem.guid, info->Name, data->posX, data->posY,
                            data->posZ, data->mapid, elem.chance, active);
                    else
                        PSendSysMessage(LANG_POOL_CHANCE_CREATURE_LIST_CONSOLE,
                            elem.guid,
                            PrepareStringNpcOrGoSpawnInformation<Creature>(
                                elem.guid).c_str(),
                            info->Name, data->posX, data->posY, data->posZ,
                            data->mapid, elem.chance, active);
                }
            }
        }
    }

    PoolObjectList const& poolCreaturesEq = poolCreatures.GetEqualChanced();
    if (!poolCreaturesEq.empty())
    {
        SendSysMessage(LANG_POOL_CREATURE_LIST_HEADER);
        for (const auto& elem : poolCreaturesEq)
        {
            if (CreatureData const* data =
                    sObjectMgr::Instance()->GetCreatureData(elem.guid))
            {
                if (CreatureInfo const* info =
                        ObjectMgr::GetCreatureTemplate(data->id))
                {
                    char const* active =
                        crSpawns &&
                                crSpawns->find(elem.guid) != crSpawns->end() ?
                            active_str.c_str() :
                            "";
                    if (m_session)
                        PSendSysMessage(LANG_POOL_CREATURE_LIST_CHAT, elem.guid,
                            PrepareStringNpcOrGoSpawnInformation<Creature>(
                                elem.guid).c_str(),
                            elem.guid, info->Name, data->posX, data->posY,
                            data->posZ, data->mapid, active);
                    else
                        PSendSysMessage(LANG_POOL_CREATURE_LIST_CONSOLE,
                            elem.guid,
                            PrepareStringNpcOrGoSpawnInformation<Creature>(
                                elem.guid).c_str(),
                            info->Name, data->posX, data->posY, data->posZ,
                            data->mapid, active);
                }
            }
        }
    }

    PoolGroup<GameObject> const& poolGameObjects =
        sPoolMgr::Instance()->GetPoolGameObjects(pool_id);
    SpawnedPoolObjects const* goSpawns =
        spawns ? &spawns->GetSpawnedGameobjects() : nullptr;

    PoolObjectList const& poolGameObjectsEx =
        poolGameObjects.GetExplicitlyChanced();
    if (!poolGameObjectsEx.empty())
    {
        SendSysMessage(LANG_POOL_CHANCE_GO_LIST_HEADER);
        for (const auto& elem : poolGameObjectsEx)
        {
            if (GameObjectData const* data =
                    sObjectMgr::Instance()->GetGOData(elem.guid))
            {
                if (GameObjectInfo const* info =
                        ObjectMgr::GetGameObjectInfo(data->id))
                {
                    char const* active =
                        goSpawns &&
                                goSpawns->find(elem.guid) != goSpawns->end() ?
                            active_str.c_str() :
                            "";
                    if (m_session)
                        PSendSysMessage(LANG_POOL_CHANCE_GO_LIST_CHAT,
                            elem.guid,
                            PrepareStringNpcOrGoSpawnInformation<GameObject>(
                                elem.guid).c_str(),
                            elem.guid, info->name, data->posX, data->posY,
                            data->posZ, data->mapid, elem.chance, active);
                    else
                        PSendSysMessage(LANG_POOL_CHANCE_GO_LIST_CONSOLE,
                            elem.guid,
                            PrepareStringNpcOrGoSpawnInformation<GameObject>(
                                elem.guid).c_str(),
                            info->name, data->posX, data->posY, data->posZ,
                            data->mapid, elem.chance, active);
                }
            }
        }
    }

    PoolObjectList const& poolGameObjectsEq = poolGameObjects.GetEqualChanced();
    if (!poolGameObjectsEq.empty())
    {
        SendSysMessage(LANG_POOL_GO_LIST_HEADER);
        for (const auto& elem : poolGameObjectsEq)
        {
            if (GameObjectData const* data =
                    sObjectMgr::Instance()->GetGOData(elem.guid))
            {
                if (GameObjectInfo const* info =
                        ObjectMgr::GetGameObjectInfo(data->id))
                {
                    char const* active =
                        goSpawns &&
                                goSpawns->find(elem.guid) != goSpawns->end() ?
                            active_str.c_str() :
                            "";
                    if (m_session)
                        PSendSysMessage(LANG_POOL_GO_LIST_CHAT, elem.guid,
                            PrepareStringNpcOrGoSpawnInformation<GameObject>(
                                elem.guid).c_str(),
                            elem.guid, info->name, data->posX, data->posY,
                            data->posZ, data->mapid, active);
                    else
                        PSendSysMessage(LANG_POOL_GO_LIST_CONSOLE, elem.guid,
                            PrepareStringNpcOrGoSpawnInformation<GameObject>(
                                elem.guid).c_str(),
                            info->name, data->posX, data->posY, data->posZ,
                            data->mapid, active);
                }
            }
        }
    }

    PoolGroup<Pool> const& poolPools =
        sPoolMgr::Instance()->GetPoolPools(pool_id);
    SpawnedPoolPools const* poolSpawns =
        spawns ? &spawns->GetSpawnedPools() : nullptr;

    PoolObjectList const& poolPoolsEx = poolPools.GetExplicitlyChanced();
    if (!poolPoolsEx.empty())
    {
        SendSysMessage(LANG_POOL_CHANCE_POOL_LIST_HEADER);
        for (const auto& elem : poolPoolsEx)
        {
            PoolTemplateData const& itr_template =
                sPoolMgr::Instance()->GetPoolTemplate(elem.guid);
            char const* active =
                poolSpawns && poolSpawns->find(elem.guid) != poolSpawns->end() ?
                    active_str.c_str() :
                    "";
            if (m_session)
                PSendSysMessage(LANG_POOL_CHANCE_POOL_LIST_CHAT, elem.guid,
                    elem.guid, itr_template.description.c_str(),
                    itr_template.AutoSpawn ? 1 : 0, itr_template.MaxLimit,
                    sPoolMgr::Instance()->GetPoolCreatures(elem.guid).size(),
                    sPoolMgr::Instance()->GetPoolGameObjects(elem.guid).size(),
                    sPoolMgr::Instance()->GetPoolPools(elem.guid).size(),
                    elem.chance, active);
            else
                PSendSysMessage(LANG_POOL_CHANCE_POOL_LIST_CONSOLE, elem.guid,
                    itr_template.description.c_str(),
                    itr_template.AutoSpawn ? 1 : 0, itr_template.MaxLimit,
                    sPoolMgr::Instance()->GetPoolCreatures(elem.guid).size(),
                    sPoolMgr::Instance()->GetPoolGameObjects(elem.guid).size(),
                    sPoolMgr::Instance()->GetPoolPools(elem.guid).size(),
                    elem.chance, active);
        }
    }

    PoolObjectList const& poolPoolsEq = poolPools.GetEqualChanced();
    if (!poolPoolsEq.empty())
    {
        SendSysMessage(LANG_POOL_POOL_LIST_HEADER);
        for (const auto& elem : poolPoolsEq)
        {
            PoolTemplateData const& itr_template =
                sPoolMgr::Instance()->GetPoolTemplate(elem.guid);
            char const* active =
                poolSpawns && poolSpawns->find(elem.guid) != poolSpawns->end() ?
                    active_str.c_str() :
                    "";
            if (m_session)
                PSendSysMessage(LANG_POOL_POOL_LIST_CHAT, elem.guid, elem.guid,
                    itr_template.description.c_str(),
                    itr_template.AutoSpawn ? 1 : 0, itr_template.MaxLimit,
                    sPoolMgr::Instance()->GetPoolCreatures(elem.guid).size(),
                    sPoolMgr::Instance()->GetPoolGameObjects(elem.guid).size(),
                    sPoolMgr::Instance()->GetPoolPools(elem.guid).size(),
                    active);
            else
                PSendSysMessage(LANG_POOL_POOL_LIST_CONSOLE, elem.guid,
                    itr_template.description.c_str(),
                    itr_template.AutoSpawn ? 1 : 0, itr_template.MaxLimit,
                    sPoolMgr::Instance()->GetPoolCreatures(elem.guid).size(),
                    sPoolMgr::Instance()->GetPoolGameObjects(elem.guid).size(),
                    sPoolMgr::Instance()->GetPoolPools(elem.guid).size(),
                    active);
        }
    }
    return true;
}

/// Triggering corpses expire check in world
bool ChatHandler::HandleServerCorpsesCommand(char* /*args*/)
{
    sObjectAccessor::Instance()->RemoveOldCorpses();
    return true;
}

bool ChatHandler::HandleRepairitemsCommand(char* args)
{
    Player* target;
    if (!ExtractPlayerTarget(&args, &target))
        return false;

    // check online security
    if (HasLowerSecurity(target))
        return false;

    // Repair items
    target->durability(true, 1.0, true);

    PSendSysMessage(LANG_YOU_REPAIR_ITEMS, GetNameLink(target).c_str());
    if (needReportToTarget(target))
        ChatHandler(target).PSendSysMessage(
            LANG_YOUR_ITEMS_REPAIRED, GetNameLink().c_str());
    return true;
}

bool ChatHandler::HandleWaterwalkCommand(char* args)
{
    bool value;
    if (!ExtractOnOff(&args, value))
    {
        SendSysMessage(LANG_USE_BOL);
        SetSentErrorMessage(true);
        return false;
    }

    Player* player = getSelectedPlayer();

    if (!player)
    {
        PSendSysMessage(LANG_NO_CHAR_SELECTED);
        SetSentErrorMessage(true);
        return false;
    }

    // check online security
    if (HasLowerSecurity(player))
        return false;

    if (value)
        player->SetMovement(MOVE_WATER_WALK); // ON
    else
        player->SetMovement(MOVE_LAND_WALK); // OFF

    PSendSysMessage(LANG_YOU_SET_WATERWALK, args, GetNameLink(player).c_str());
    if (needReportToTarget(player))
        ChatHandler(player).PSendSysMessage(
            LANG_YOUR_WATERWALK_SET, args, GetNameLink().c_str());
    return true;
}

bool ChatHandler::HandleLookupTitleCommand(char* args)
{
    if (!*args)
        return false;

    // can be NULL in console call
    Player* target = getSelectedPlayer();

    // title name have single string arg for player name
    char const* targetName = target ? target->GetName() : "NAME";

    std::string namepart = args;
    std::wstring wnamepart;

    if (!Utf8toWStr(namepart, wnamepart))
        return false;

    // converting string that we try to find to lower case
    wstrToLower(wnamepart);

    uint32 counter = 0; // Counter for figure out that we found smth.

    // Search in CharTitles.dbc
    for (uint32 id = 0; id < sCharTitlesStore.GetNumRows(); id++)
    {
        CharTitlesEntry const* titleInfo = sCharTitlesStore.LookupEntry(id);
        if (titleInfo)
        {
            int loc = GetSessionDbcLocale();
            std::string name = titleInfo->name[loc];
            if (name.empty())
                continue;

            if (!Utf8FitTo(name, wnamepart))
            {
                loc = 0;
                for (; loc < MAX_LOCALE; ++loc)
                {
                    if (loc == GetSessionDbcLocale())
                        continue;

                    name = titleInfo->name[loc];
                    if (name.empty())
                        continue;

                    if (Utf8FitTo(name, wnamepart))
                        break;
                }
            }

            if (loc < MAX_LOCALE)
            {
                char const* knownStr = target && target->HasTitle(titleInfo) ?
                                           GetMangosString(LANG_KNOWN) :
                                           "";

                char const* activeStr =
                    target &&
                            target->GetUInt32Value(PLAYER_CHOSEN_TITLE) ==
                                titleInfo->bit_index ?
                        GetMangosString(LANG_ACTIVE) :
                        "";

                char titleNameStr[80];
                snprintf(titleNameStr, 80, name.c_str(), targetName);

                // send title in "id (idx:idx) - [namedlink locale]" format
                if (m_session)
                    PSendSysMessage(LANG_TITLE_LIST_CHAT, id,
                        titleInfo->bit_index, id, titleNameStr,
                        localeNames[loc], knownStr, activeStr);
                else
                    PSendSysMessage(LANG_TITLE_LIST_CONSOLE, id,
                        titleInfo->bit_index, titleNameStr, localeNames[loc],
                        knownStr, activeStr);

                ++counter;
            }
        }
    }
    if (counter == 0) // if counter == 0 then we found nth
        SendSysMessage(LANG_COMMAND_NOTITLEFOUND);
    return true;
}

bool ChatHandler::HandleTitlesAddCommand(char* args)
{
    // number or [name] Shift-click form |color|Htitle:title_id|h[name]|h|r
    uint32 id;
    if (!ExtractUint32KeyFromLink(&args, "Htitle", id))
        return false;

    if (id <= 0)
    {
        PSendSysMessage(LANG_INVALID_TITLE_ID, id);
        SetSentErrorMessage(true);
        return false;
    }

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

    CharTitlesEntry const* titleInfo = sCharTitlesStore.LookupEntry(id);
    if (!titleInfo)
    {
        PSendSysMessage(LANG_INVALID_TITLE_ID, id);
        SetSentErrorMessage(true);
        return false;
    }

    std::string tNameLink = GetNameLink(target);

    char const* targetName = target->GetName();
    char titleNameStr[80];
    snprintf(
        titleNameStr, 80, titleInfo->name[GetSessionDbcLocale()], targetName);

    target->SetTitle(titleInfo);
    PSendSysMessage(LANG_TITLE_ADD_RES, id, titleNameStr, tNameLink.c_str());

    return true;
}

bool ChatHandler::HandleTitlesRemoveCommand(char* args)
{
    // number or [name] Shift-click form |color|Htitle:title_id|h[name]|h|r
    uint32 id;
    if (!ExtractUint32KeyFromLink(&args, "Htitle", id))
        return false;

    if (id <= 0)
    {
        PSendSysMessage(LANG_INVALID_TITLE_ID, id);
        SetSentErrorMessage(true);
        return false;
    }

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

    CharTitlesEntry const* titleInfo = sCharTitlesStore.LookupEntry(id);
    if (!titleInfo)
    {
        PSendSysMessage(LANG_INVALID_TITLE_ID, id);
        SetSentErrorMessage(true);
        return false;
    }

    target->SetTitle(titleInfo, true);

    std::string tNameLink = GetNameLink(target);

    char const* targetName = target->GetName();
    char titleNameStr[80];
    snprintf(
        titleNameStr, 80, titleInfo->name[GetSessionDbcLocale()], targetName);

    PSendSysMessage(LANG_TITLE_REMOVE_RES, id, titleNameStr, tNameLink.c_str());

    if (!target->HasTitle(target->GetInt32Value(PLAYER_CHOSEN_TITLE)))
    {
        target->SetUInt32Value(PLAYER_CHOSEN_TITLE, 0);
        PSendSysMessage(LANG_CURRENT_TITLE_RESET, tNameLink.c_str());
    }

    return true;
}

// Edit Player KnownTitles
bool ChatHandler::HandleTitlesSetMaskCommand(char* args)
{
    if (!*args)
        return false;

    uint64 titles = 0;

    sscanf(args, UI64FMTD, &titles);

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

    uint64 titles2 = titles;

    for (uint32 i = 1; i < sCharTitlesStore.GetNumRows(); ++i)
        if (CharTitlesEntry const* tEntry = sCharTitlesStore.LookupEntry(i))
            titles2 &= ~(uint64(1) << tEntry->bit_index);

    titles &= ~titles2; // remove nonexistent titles

    target->SetUInt64Value(PLAYER__FIELD_KNOWN_TITLES, titles);
    SendSysMessage(LANG_DONE);

    if (!target->HasTitle(target->GetInt32Value(PLAYER_CHOSEN_TITLE)))
    {
        target->SetUInt32Value(PLAYER_CHOSEN_TITLE, 0);
        PSendSysMessage(LANG_CURRENT_TITLE_RESET, GetNameLink(target).c_str());
    }

    return true;
}

bool ChatHandler::HandleCharacterTitlesCommand(char* args)
{
    Player* target;
    if (!ExtractPlayerTarget(&args, &target))
        return false;

    LocaleConstant loc = GetSessionDbcLocale();
    char const* targetName = target->GetName();
    char const* knownStr = GetMangosString(LANG_KNOWN);

    // Search in CharTitles.dbc
    for (uint32 id = 0; id < sCharTitlesStore.GetNumRows(); id++)
    {
        CharTitlesEntry const* titleInfo = sCharTitlesStore.LookupEntry(id);
        if (titleInfo && target->HasTitle(titleInfo))
        {
            std::string name = titleInfo->name[loc];
            if (name.empty())
                continue;

            char const* activeStr =
                target &&
                        target->GetUInt32Value(PLAYER_CHOSEN_TITLE) ==
                            titleInfo->bit_index ?
                    GetMangosString(LANG_ACTIVE) :
                    "";

            char titleNameStr[80];
            snprintf(titleNameStr, 80, name.c_str(), targetName);

            // send title in "id (idx:idx) - [namedlink locale]" format
            if (m_session)
                PSendSysMessage(LANG_TITLE_LIST_CHAT, id, titleInfo->bit_index,
                    id, titleNameStr, localeNames[loc], knownStr, activeStr);
            else
                PSendSysMessage(LANG_TITLE_LIST_CONSOLE, id,
                    titleInfo->bit_index, name.c_str(), localeNames[loc],
                    knownStr, activeStr);
        }
    }
    return true;
}

bool ChatHandler::HandleTitlesCurrentCommand(char* args)
{
    // number or [name] Shift-click form |color|Htitle:title_id|h[name]|h|r
    uint32 id;
    if (!ExtractUint32KeyFromLink(&args, "Htitle", id))
        return false;

    if (id <= 0)
    {
        PSendSysMessage(LANG_INVALID_TITLE_ID, id);
        SetSentErrorMessage(true);
        return false;
    }

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

    CharTitlesEntry const* titleInfo = sCharTitlesStore.LookupEntry(id);
    if (!titleInfo)
    {
        PSendSysMessage(LANG_INVALID_TITLE_ID, id);
        SetSentErrorMessage(true);
        return false;
    }

    std::string tNameLink = GetNameLink(target);

    target->SetTitle(titleInfo); // to be sure that title now known
    target->SetUInt32Value(PLAYER_CHOSEN_TITLE, titleInfo->bit_index);

    PSendSysMessage(LANG_TITLE_CURRENT_RES, id,
        titleInfo->name[GetSessionDbcLocale()], tNameLink.c_str());

    return true;
}

bool ChatHandler::HandleMmapPathCommand(char* args)
{
    if (!MMAP::MMapFactory::createOrGetMMapManager()->GetNavMesh(
            m_session->GetPlayer()->GetMapId()))
    {
        PSendSysMessage("NavMesh not loaded for current map.");
        return true;
    }

    PSendSysMessage("mmap path:");

    // units
    Player* player = m_session->GetPlayer();
    Unit* target = getSelectedUnit();
    if (!player || !target)
    {
        PSendSysMessage("Invalid target/source selection.");
        return true;
    }

    if ((target->GetTransport() || player->GetTransport()) &&
        player->GetTransport() != target->GetTransport())
    {
        SendSysMessage(
            "Paths on transports can only be built if target and source are on "
            "the same transport.");
        return true;
    }

    char* para = strtok(args, " ");

    bool useStraightPath = false;
    if (para && strcmp(para, "true") == 0)
        useStraightPath = true;

    // unit locations
    float x, y, z;
    if (player->GetTransport())
        player->m_movementInfo.transport.pos.Get(x, y, z);
    else
        player->GetPosition(x, y, z);

    // path
    PathFinder path(target);
    path.setUseStraightPath(useStraightPath);

    std::string ms;
    {
        scope_performance_timer t("", &ms);
        path.calculate(x, y, z);
    }

    PointsArray pointPath = path.getPath();
    PSendSysMessage("%s's path to %s:", target->GetName(), player->GetName());
    PSendSysMessage(
        "Building %s", useStraightPath ? "StraightPath" : "SmoothPath");
    PSendSysMessage(
        "length " SIZEFMTD " type %u", pointPath.size(), path.getPathType());

    Vector3 start = path.getStartPosition();
    Vector3 end = path.getEndPosition();
    Vector3 actualEnd = path.getActualEndPosition();

    PSendSysMessage("start      (%.3f, %.3f, %.3f)", start.x, start.y, start.z);
    PSendSysMessage("end        (%.3f, %.3f, %.3f)", end.x, end.y, end.z);
    PSendSysMessage(
        "actual end (%.3f, %.3f, %.3f)", actualEnd.x, actualEnd.y, actualEnd.z);
    PSendSysMessage("took       %s", ms.c_str());

    if (!player->isGameMaster())
        PSendSysMessage("Enable GM mode to see the path points.");

    // this entry visible only to GM's with "gm on"
    static const uint32 WAYPOINT_NPC_ENTRY = 1;
    for (auto& elem : pointPath)
    {
        G3D::Vector3 pos(elem.x, elem.y, elem.z);
        if (Transport* trans = player->GetTransport())
            trans->CalculatePassengerPosition(pos.x, pos.y, pos.z);
        player->SummonCreature(WAYPOINT_NPC_ENTRY, pos.x, pos.y, pos.z, 0,
            TEMPSUMMON_TIMED_DESPAWN, 9000);
    }

    return true;
}

bool ChatHandler::HandleMmapLocCommand(char* /*args*/)
{
    PSendSysMessage("mmap tileloc:");

    Player* player = m_session->GetPlayer();

    // Transport location
    if (player->GetTransport())
    {
        auto mesh = MMAP::MMapFactory::createOrGetMMapManager()->GetNavMesh(
            player->GetTransport()->m_model->name);
        auto meshQuery =
            MMAP::MMapFactory::createOrGetMMapManager()->GetNavMeshQuery(
                player->GetTransport()->m_model->name, player->GetInstanceId(),
                false);
        if (!mesh || !meshQuery)
        {
            PSendSysMessage("NavMesh not loaded for current transport (%s)",
                player->GetTransport()->m_model->name.c_str());
            return true;
        }

        float x, y, z;
        player->m_movementInfo.pos.Get(x, y, z);
        float location[VERTEX_SIZE] = {x, y, z};
        float extents[VERTEX_SIZE] = {5000.0f, 5000.0f, 5000.0f};

        // navmesh poly -> navmesh tile location
        dtQueryFilter filter;
        dtPolyRef polyRef = INVALID_POLYREF;
        float point[3] = {0};
        meshQuery->findNearestPoly(location, extents, &filter, &polyRef, point);

        if (polyRef == INVALID_POLYREF)
            SendSysMessage(
                "NavMesh is loaded for transport, but your current location is "
                "NOT on the mesh.");
        else
            PSendSysMessage(
                "NavMesh is loaded for transport, and your current location "
                "was found on the mesh (%f, %f, %f)",
                point[0], point[1], point[2]);

        return true;
    }

    // grid tile location
    int32 gx = 32 - player->GetX() / MAP_CELL_SIZE;
    int32 gy = 32 - player->GetY() / MAP_CELL_SIZE;

    PSendSysMessage("%03u%02i%02i.mmtile", player->GetMapId(), gy, gx);
    PSendSysMessage("gridloc [%i,%i]", gx, gy);

    // calculate navmesh tile location
    const dtNavMesh* navmesh =
        MMAP::MMapFactory::createOrGetMMapManager()->GetNavMesh(
            player->GetMapId());
    const dtNavMeshQuery* navmeshquery =
        MMAP::MMapFactory::createOrGetMMapManager()->GetNavMeshQuery(
            player->GetMapId(), player->GetInstanceId(), false);
    if (!navmesh || !navmeshquery)
    {
        SendSysMessage("NavMesh not loaded for current map.");
        return true;
    }

    const float* min = navmesh->getParams()->orig;

    float x, y, z;
    player->GetPosition(x, y, z);
    float location[VERTEX_SIZE] = {y, z, x};
    float extents[VERTEX_SIZE] = {3.0f, 5.0f, 3.0f};

    int32 tilex = int32((y - min[0]) / MAP_CELL_SIZE);
    int32 tiley = int32((x - min[2]) / MAP_CELL_SIZE);

    PSendSysMessage("Calc   [%02i,%02i]", tilex, tiley);

    // navmesh poly -> navmesh tile location
    dtQueryFilter filter = dtQueryFilter();
    dtPolyRef polyRef = INVALID_POLYREF;
    navmeshquery->findNearestPoly(
        location, extents, &filter, &polyRef, nullptr);

    if (polyRef == INVALID_POLYREF)
        PSendSysMessage(
            "Dt     [??,??] (invalid poly, probably no tile loaded)");
    else
    {
        const dtMeshTile* tile;
        const dtPoly* poly;
        navmesh->getTileAndPolyByRef(polyRef, &tile, &poly);
        if (tile)
            PSendSysMessage(
                "Dt     [%02i,%02i]", tile->header->x, tile->header->y);
        else
            PSendSysMessage("Dt     [??,??] (no tile loaded)");
    }

    return true;
}

bool ChatHandler::HandleMmapLoadedTilesCommand(char* /*args*/)
{
    uint32 mapid = m_session->GetPlayer()->GetMapId();

    const dtNavMesh* navmesh =
        MMAP::MMapFactory::createOrGetMMapManager()->GetNavMesh(mapid);
    const dtNavMeshQuery* navmeshquery =
        MMAP::MMapFactory::createOrGetMMapManager()->GetNavMeshQuery(
            mapid, m_session->GetPlayer()->GetInstanceId(), false);
    if (!navmesh || !navmeshquery)
    {
        PSendSysMessage("NavMesh not loaded for current map.");
        return true;
    }

    PSendSysMessage("mmap loadedtiles:");

    for (int32 i = 0; i < navmesh->getMaxTiles(); ++i)
    {
        const dtMeshTile* tile = navmesh->getTile(i);
        if (!tile || !tile->header)
            continue;

        PSendSysMessage("[%02i,%02i]", tile->header->x, tile->header->y);
    }

    return true;
}

bool ChatHandler::HandleMmapStatsCommand(char* /*args*/)
{
    PSendSysMessage("mmap stats:");
    PSendSysMessage("  global mmap pathfinding is %sabled",
        sWorld::Instance()->getConfig(CONFIG_BOOL_MMAP_ENABLED) ? "en" : "dis");

    MMAP::MMapManager* manager = MMAP::MMapFactory::createOrGetMMapManager();
    PSendSysMessage(" %u maps loaded with %u tiles overall",
        manager->getLoadedMapsCount(), manager->getLoadedTilesCount());

    const dtNavMesh* navmesh =
        manager->GetNavMesh(m_session->GetPlayer()->GetMapId());
    if (!navmesh)
    {
        PSendSysMessage("NavMesh not loaded for current map.");
        return true;
    }

    uint32 tileCount = 0;
    uint32 nodeCount = 0;
    uint32 polyCount = 0;
    uint32 vertCount = 0;
    uint32 triCount = 0;
    uint32 triVertCount = 0;
    uint32 dataSize = 0;
    for (int32 i = 0; i < navmesh->getMaxTiles(); ++i)
    {
        const dtMeshTile* tile = navmesh->getTile(i);
        if (!tile || !tile->header)
            continue;

        tileCount++;
        nodeCount += tile->header->bvNodeCount;
        polyCount += tile->header->polyCount;
        vertCount += tile->header->vertCount;
        triCount += tile->header->detailTriCount;
        triVertCount += tile->header->detailVertCount;
        dataSize += tile->dataSize;
    }

    PSendSysMessage("Navmesh stats on current map:");
    PSendSysMessage(" %u tiles loaded", tileCount);
    PSendSysMessage(" %u BVTree nodes", nodeCount);
    PSendSysMessage(" %u polygons (%u vertices)", polyCount, vertCount);
    PSendSysMessage(" %u triangles (%u vertices)", triCount, triVertCount);
    PSendSysMessage(" %.2f MB of data (not including pointers)",
        ((float)dataSize / sizeof(unsigned char)) / 1048576);

    return true;
}

//-----------------------Creature Group Commands-----------------------
// Create new group
bool ChatHandler::HandleNpcGroupCreateCommand(char* args)
{
    char* groupName = ExtractQuotedArg(&args);
    if (!groupName)
        return false;

    Map* pMap = m_session->GetPlayer()->GetMap();
    if (!pMap)
        return false;

    uint32 id = pMap->GetCreatureGroupMgr().CreateNewGroup(groupName, false);
    if (id == 0)
    {
        SendSysMessage("Group creation failed.");
        return true; // Do not display syntax
    }

    PSendSysMessage(
        "Group created with ID: %i. This change requires a server restart to "
        "take global effect.",
        id);

    return true;
}

// Delete a group by id
bool ChatHandler::HandleNpcGroupDeleteCommand(char* args)
{
    int32 id;
    if (!ExtractInt32(&args, id))
        return false;

    Map* pMap = m_session->GetPlayer()->GetMap();

    if (!pMap->GetCreatureGroupMgr().DeleteGroup(id))
        SendSysMessage("A group with that ID was not found in this map.");

    SendSysMessage(
        "Deleted group. This change requires a server restart to take global "
        "effect.");

    return true;
}

bool npc_group_list_compare(const std::pair<std::string, int32>& L,
    const std::pair<std::string, int32>& R)
{
    std::string tempL(L.first);
    std::string tempR(R.first);
    std::transform(tempL.begin(), tempL.end(), tempL.begin(), ::tolower);
    std::transform(tempR.begin(), tempR.end(), tempR.begin(), ::tolower);
    return tempL.compare(tempR) < 0;
}

// Lists all groups in map
bool ChatHandler::HandleNpcGroupListCommand(char* /*args*/)
{
    Map* pMap = m_session->GetPlayer()->GetMap();

    // Copy and sort the groups by group name
    typedef std::pair<std::string, int32> group;
    std::vector<group> grps;
    for (CreatureGroupMgr::group_map::const_iterator itr =
             pMap->GetCreatureGroupMgr().GetRawBegin();
         itr != pMap->GetCreatureGroupMgr().GetRawEnd(); ++itr)
        grps.push_back(
            group(itr->second->GetGroupEntry()->group_name, itr->first));
    std::sort(grps.begin(), grps.end(), npc_group_list_compare);

    PSendSysMessage("All groups in this map:");

    // Display them
    for (auto& grp : grps)
    {
        if (grp.second > 0)
            PSendSysMessage("%s Id: %i", grp.first.c_str(), grp.second);
        else
            PSendSysMessage(
                "%s Id: %i (temporary)", grp.first.c_str(), grp.second);
    }

    return true;
}

// Rename group
bool ChatHandler::HandleNpcGroupRenameCommand(char* args)
{
    int32 id;
    if (!ExtractInt32(&args, id))
        return false;

    char* groupName = ExtractQuotedArg(&args);
    if (!groupName)
        return false;

    Map* pMap = m_session->GetPlayer()->GetMap();

    CreatureGroup* pGroup = pMap->GetCreatureGroupMgr().GetGroup(id);
    if (!pGroup)
    {
        SendSysMessage("No group with that ID exists in this map.");
        return true; // Don't display syntax
    }
    pGroup->RenameGroup(groupName);

    SendSysMessage("Group renamed successfully.");

    return true;
}

// Add Member to Group
bool ChatHandler::HandleNpcGroupAddCommand(char* args)
{
    Map* pMap = m_session->GetPlayer()->GetMap();

    Creature* pCreature =
        pMap->GetCreature(m_session->GetPlayer()->GetTargetGuid());
    if (!pCreature)
        return false;

    int32 id;
    if (!ExtractInt32(&args, id))
        return false;

    CreatureGroup* pGroup = pMap->GetCreatureGroupMgr().GetGroup(id);
    if (!pGroup)
    {
        SendSysMessage("No group with that ID exists in this map.");
        return true; // Don't display syntax
    }

    if (pCreature->GetGroup() && pCreature->GetGroup()->GetId() == id)
    {
        SendSysMessage("That creature is already part of the specified group.");
        return true; // Don't display syntax
    }

    if (pCreature->GetGroup() != nullptr)
    {
        SendSysMessage("That creature is already part of another group.");
        return true; // Don't display syntax
    }

    pGroup->AddMember(pCreature, true);
    SendSysMessage(
        "Creature added to group successfully. Note that this change only "
        "becomes global after a server restart.");

    return true;
}

// Remove Member from Group
bool ChatHandler::HandleNpcGroupRemoveCommand(char* args)
{
    Map* pMap = m_session->GetPlayer()->GetMap();

    Creature* pCreature =
        pMap->GetCreature(m_session->GetPlayer()->GetTargetGuid());
    if (!pCreature)
        return false;

    int32 id;
    if (!ExtractInt32(&args, id))
        return false;

    CreatureGroup* pGroup = pMap->GetCreatureGroupMgr().GetGroup(id);
    if (!pGroup)
    {
        SendSysMessage("No group with that ID exists in this map.");
        return true; // Don't display syntax
    }

    if (pCreature->GetGroup() && pCreature->GetGroup()->GetId() != id)
    {
        SendSysMessage("That creature is not part of the specified group.");
        return true; // Don't display syntax
    }

    pGroup->RemoveMember(pCreature, true);
    SendSysMessage(
        "Creature removed from group successfully. Note that this change only "
        "becomes global after a server restart.");

    return true;
}

// Set or clear Leader of Group
bool ChatHandler::HandleNpcGroupLeaderCommand(char* args)
{
    Map* pMap = m_session->GetPlayer()->GetMap();

    Creature* pCreature =
        pMap->GetCreature(m_session->GetPlayer()->GetTargetGuid());
    if (!pCreature)
        return false;

    int32 id;
    if (!ExtractInt32(&args, id))
        return false;

    CreatureGroup* pGroup = pMap->GetCreatureGroupMgr().GetGroup(id);
    if (!pGroup)
    {
        SendSysMessage("No group with that ID exists in this map.");
        return true; // Don't display syntax
    }

    if (pCreature->GetGroup() && pCreature->GetGroup()->GetId() != id)
    {
        SendSysMessage("That creature is not part of the specified group.");
        return true; // Don't display syntax
    }

    if (pGroup->GetLeader() == pCreature)
    {
        pGroup->ClearLeader(true);
        SendSysMessage("Removed leader from group. group is now leader-less.");
    }
    else
    {
        if (pGroup->SetLeader(pCreature, true))
            PSendSysMessage("%s is now the leader of the group.",
                pCreature->GetGuidStr().c_str());
    }

    return true;
}

uint32 npc_group_flag_string_literal(std::string& str)
{
    std::transform(str.begin(), str.end(), str.begin(), ::toupper);
    if (str.compare("RESPAWN") == 0)
        return CREATURE_GROUP_FLAG_RESPAWN_ALL_ON_SURVIVOR;
    else if (str.compare("ASSIST") == 0)
        return CREATURE_GROUP_FLAG_CANNOT_ASSIST;
    else if (str.compare("LEADER_RESPAWN") == 0)
        return CREATURE_GROUP_FLAG_LEADER_RESPAWN_ALL;
    else if (str.compare("LEADER_DESPAWN") == 0)
        return CREATURE_GROUP_FLAG_LEADER_DESPAWN_ALL;
    else if (str.compare("GROUP_WAYPOINT") == 0)
        return CREATURE_GROUP_FLAG_GROUP_MOVEMENT;
    else if (str.compare("BE_ASSISTED") == 0)
        return CREATURE_GROUP_FLAG_CANNOT_BE_ASSISTED;
    else if (str.compare("GROUP_ASSIST") == 0)
        return CREATURE_GROUP_FLAG_CANNOT_ASSIST_OTHER_GRPS;
    else if (str.compare("NO_COMBAT") == 0)
        return CREATURE_GROUP_FLAG_MOVEMENT_NO_COMBAT;
    return 0;
};

std::string npc_group_get_descriptive_flags(CreatureGroup* pGroup)
{
    std::string str = "Flags: ";
    if (pGroup->HasFlag(CREATURE_GROUP_FLAG_RESPAWN_ALL_ON_SURVIVOR))
        str += "RESPAWN ";
    if (pGroup->HasFlag(CREATURE_GROUP_FLAG_CANNOT_ASSIST))
        str += "ASSIST ";
    if (pGroup->HasFlag(CREATURE_GROUP_FLAG_LEADER_RESPAWN_ALL))
        str += "LEADER_RESPAWN ";
    if (pGroup->HasFlag(CREATURE_GROUP_FLAG_LEADER_DESPAWN_ALL))
        str += "LEADER_DESPAWN ";
    if (pGroup->HasFlag(CREATURE_GROUP_FLAG_GROUP_MOVEMENT))
        str += "GROUP_WAYPOINT ";
    if (pGroup->HasFlag(CREATURE_GROUP_FLAG_CANNOT_BE_ASSISTED))
        str += "BE_ASSISTED ";
    if (pGroup->HasFlag(CREATURE_GROUP_FLAG_CANNOT_ASSIST_OTHER_GRPS))
        str += "GROUP_ASSIST ";
    if (pGroup->HasFlag(CREATURE_GROUP_FLAG_MOVEMENT_NO_COMBAT))
        str += "NO_COMBAT ";

    return str;
}

// Shows info about group by ID or target
bool ChatHandler::HandleNpcGroupInfoCommand(char* args)
{
    Map* pMap = m_session->GetPlayer()->GetMap();

    int32 id;
    ExtractOptInt32(&args, id, 0);
    Creature* pCreature =
        pMap->GetCreature(m_session->GetPlayer()->GetTargetGuid());

    if (id == 0 && pCreature == nullptr)
        return false;

    // Get group either by ID or by Target
    CreatureGroup* pGroup = nullptr;
    if (id)
    {
        pGroup = pMap->GetCreatureGroupMgr().GetGroup(id);
        if (!pGroup)
        {
            SendSysMessage("No group with that ID exists in this map.");
            return true; // Don't display syntax
        }
        PSendSysMessage("Group %i:", pGroup->GetId());
    }
    else
    {
        pGroup = pCreature->GetGroup();
        if (!pGroup)
        {
            SendSysMessage("Target does not have a group.");
            return true; // Don't display syntax
        }
        PSendSysMessage("Target's Group (%i):", pGroup->GetId());
    }

    PSendSysMessage("Name: %s", pGroup->GetGroupEntry()->group_name.c_str());
    PSendSysMessage("%s", npc_group_get_descriptive_flags(pGroup).c_str());
    if (Creature* pLeader = pGroup->GetLeader())
        PSendSysMessage("Leader: %s (%s)", pLeader->GetName(),
            pLeader->GetGuidStr().c_str());

    return true;
}

// Show all available flags
bool ChatHandler::HandleNpcGroupFlagListCommand(char* /*args*/)
{
    SendSysMessage("Flags are:");
    SendSysMessage(
        "RESPAWN -- When a mob in the group survives, they all respawn.");
    SendSysMessage(
        "ASSIST -- Mobs in this group cannot assist creatuers outside of the "
        "group.");
    SendSysMessage(
        "LEADER_RESPAWN -- If the leader survives, all mobs respawn.");
    SendSysMessage("LEADER_DESPAWN -- When the leader dies, all mobs despawn.");
    SendSysMessage(
        "GROUP_WAYPOINT -- The group moves in a formation, using group "
        "waypoints rather than normal waypoints.");
    SendSysMessage("BE_ASSISTED -- Group cannot be assited by other mobs.");
    SendSysMessage("GROUP_ASSIST -- Group cannot assist other groups.");
    SendSysMessage("NO_COMBAT -- Group cannot assist other groups.");
    return true;
}

// Add flag to group
bool ChatHandler::HandleNpcGroupFlagAddCommand(char* args)
{
    int32 id;
    if (!ExtractInt32(&args, id))
        return false;

    char* char_flag = ExtractLiteralArg(&args);
    if (!char_flag)
        return false;

    std::string flag_str(char_flag);
    uint32 flag = npc_group_flag_string_literal(flag_str);
    if (flag == 0)
    {
        SendSysMessage(
            "No such flag exists. Type \".npc group flag list\" to see all "
            "available flags.");
        return true; // Do not display syntax
    }

    Map* pMap = m_session->GetPlayer()->GetMap();
    CreatureGroup* pGroup = pMap->GetCreatureGroupMgr().GetGroup(id);
    if (!pGroup)
    {
        SendSysMessage("No group with that ID exists in this map.");
        return true; // Don't display syntax
    }

    pGroup->AddFlag(flag);
    SendSysMessage("Flag added. Group's current flags:");
    PSendSysMessage("%s", npc_group_get_descriptive_flags(pGroup).c_str());

    return true;
}

// Remove flag from group
bool ChatHandler::HandleNpcGroupFlagRemoveCommand(char* args)
{
    int32 id;
    if (!ExtractInt32(&args, id))
        return false;

    char* char_flag = ExtractLiteralArg(&args);
    if (!char_flag)
        return false;

    std::string str_flag(char_flag);
    uint32 flag = npc_group_flag_string_literal(str_flag);
    if (flag == 0)
    {
        SendSysMessage(
            "No such flag exists. Type \".npc group flag list\" to see all "
            "available flags.");
        return true; // Do not display syntax
    }

    Map* pMap = m_session->GetPlayer()->GetMap();
    CreatureGroup* pGroup = pMap->GetCreatureGroupMgr().GetGroup(id);
    if (!pGroup)
    {
        SendSysMessage("No group with that ID exists in this map.");
        return true; // Don't display syntax
    }

    pGroup->RemoveFlag(flag);
    SendSysMessage("Flag removed. Group's current flags:");
    PSendSysMessage("%s", npc_group_get_descriptive_flags(pGroup).c_str());

    return true;
}

// Show all npcs part of group. Members will get an arrow above their head and
// the leader hearts
bool ChatHandler::HandleNpcGroupShowCommand(char* args)
{
    int32 id;
    if (!ExtractInt32(&args, id))
        return false;

    Map* pMap = m_session->GetPlayer()->GetMap();
    CreatureGroup* pGroup = pMap->GetCreatureGroupMgr().GetGroup(id);
    if (!pGroup)
    {
        SendSysMessage("No group with that ID exists in this map.");
        return true; // Don't display syntax
    }

    for (auto creature : *pGroup)
    {
        if (pGroup->GetLeader() == creature)
            creature->AddAuraThroughNewHolder(20372, creature);
        else
            creature->AddAuraThroughNewHolder(20374, creature);
    }

    return true;
}

// Undo effect of show for specified group
bool ChatHandler::HandleNpcGroupUnshowCommand(char* args)
{
    int32 id;
    if (!ExtractInt32(&args, id))
        return false;

    Map* pMap = m_session->GetPlayer()->GetMap();
    CreatureGroup* pGroup = pMap->GetCreatureGroupMgr().GetGroup(id);
    if (!pGroup)
    {
        SendSysMessage("No group with that ID exists in this map.");
        return true; // Don't display syntax
    }

    for (auto& elem : *pGroup)
    {
        elem->remove_auras(20372);
        elem->remove_auras(20374);
    }

    return true;
}

// Teleport to nearest member of group
bool ChatHandler::HandleNpcGroupGoCommand(char* args)
{
    int32 id;
    if (!ExtractInt32(&args, id))
        return false;

    Map* pMap = m_session->GetPlayer()->GetMap();
    CreatureGroup* pGroup = pMap->GetCreatureGroupMgr().GetGroup(id);
    if (!pGroup)
    {
        SendSysMessage("No group with that ID exists in this map.");
        return true; // Don't display syntax
    }

    Creature* pClosest = nullptr;
    float last_dist = 100000;
    for (auto& elem : *pGroup)
    {
        float dist = m_session->GetPlayer()->GetDistance(elem);
        if (dist < last_dist)
        {
            pClosest = elem;
            last_dist = dist;
        }
    }
    if (pClosest)
        m_session->GetPlayer()->NearTeleportTo(pClosest->GetX(),
            pClosest->GetY(), pClosest->GetZ(), pClosest->GetO());
    else
        SendSysMessage("No group members found.");

    return true;
}

// Add group waypoint
bool ChatHandler::HandleNpcGroupWpAddCommand(char* args)
{
    int32 id;
    if (!ExtractInt32(&args, id))
        return false;

    Map* pMap = m_session->GetPlayer()->GetMap();
    CreatureGroup* pGroup = pMap->GetCreatureGroupMgr().GetGroup(id);
    if (!pGroup)
    {
        SendSysMessage("No group with that ID exists in this map.");
        return true; // Don't display syntax
    }

    if (!pGroup->HasFlag(CREATURE_GROUP_FLAG_GROUP_MOVEMENT))
    {
        SendSysMessage(
            "You need to enable the GROUP_WAYPOINT flag for that group before "
            "adding waypoints to it.");
        return true;
    }

    int32 delay = 0;
    ExtractOptInt32(&args, delay, 0);

    Player* player = m_session->GetPlayer();
    float X, Y, Z, O;
    player->GetPosition(X, Y, Z);
    O = player->GetO();

    DynamicWaypoint wp(X, Y, Z, O, delay);

    uint32 size = player->GetMap()
                      ->GetCreatureGroupMgr()
                      .GetMovementMgr()
                      .GetNumberOfWaypoints(id);
    player->GetMap()->GetCreatureGroupMgr().GetMovementMgr().AddWaypoint(
        id, wp);
    // Start movement if we just added the second waypoint
    if (size == 1 &&
        player->GetMap()
                ->GetCreatureGroupMgr()
                .GetMovementMgr()
                .GetNumberOfWaypoints(id) == 2)
        player->GetMap()->GetCreatureGroupMgr().ProcessGroupEvent(
            id, CREATURE_GROUP_EVENT_MOVEMENT_BEGIN);

    SendSysMessage(
        "Waypoint added. You need to restart the server for the effect to take "
        "global change.");
    return true;
}

bool ChatHandler::HandleNpcGroupWpMoveCommand(char* /*args*/)
{
    Creature* waypoint = getSelectedCreature();
    if (!waypoint)
        return false;

    Player* player = m_session->GetPlayer();
    waypoint->NearTeleportTo(
        player->GetX(), player->GetY(), player->GetZ(), player->GetO());
    player->GetMap()->GetCreatureGroupMgr().GetMovementMgr().MoveWp(waypoint);
    return true;
}

bool ChatHandler::HandleNpcGroupWpRemoveCommand(char* /*args*/)
{
    Creature* waypoint = getSelectedCreature();
    if (!waypoint)
        return false;

    Player* player = m_session->GetPlayer();
    player->GetMap()->GetCreatureGroupMgr().GetMovementMgr().RemoveWp(waypoint);
    return true;
}

bool ChatHandler::HandleNpcGroupWpShowCommand(char* args)
{
    int32 id;
    if (!ExtractInt32(&args, id))
        return false;

    Map* pMap = m_session->GetPlayer()->GetMap();
    CreatureGroup* pGroup = pMap->GetCreatureGroupMgr().GetGroup(id);
    if (!pGroup)
    {
        SendSysMessage("No group with that ID exists in this map.");
        return true; // Don't display syntax
    }

    if (!pGroup->HasFlag(CREATURE_GROUP_FLAG_GROUP_MOVEMENT))
    {
        SendSysMessage(
            "You need to enable the GROUP_WAYPOINT flag for that group before "
            "adding waypoints to it.");
        return true;
    }

    Player* player = m_session->GetPlayer();

    player->GetMap()->GetCreatureGroupMgr().GetMovementMgr().DisplayAllWps(
        id, player);
    return true;
}

bool ChatHandler::HandleNpcGroupWpUnshowCommand(char* args)
{
    int32 id;
    if (!ExtractInt32(&args, id))
        return false;

    Map* pMap = m_session->GetPlayer()->GetMap();
    CreatureGroup* pGroup = pMap->GetCreatureGroupMgr().GetGroup(id);
    if (!pGroup)
    {
        SendSysMessage("No group with that ID exists in this map.");
        return true; // Don't display syntax
    }

    if (!pGroup->HasFlag(CREATURE_GROUP_FLAG_GROUP_MOVEMENT))
    {
        SendSysMessage(
            "You need to enable the GROUP_WAYPOINT flag for that group before "
            "adding waypoints to it.");
        return true;
    }

    Player* player = m_session->GetPlayer();

    player->GetMap()->GetCreatureGroupMgr().GetMovementMgr().HideAllWps(id);
    return true;
}

bool ChatHandler::HandleNpcGroupWpEditCommand(char* args)
{
    Creature* waypoint = getSelectedCreature();
    if (!waypoint)
    {
        SendSysMessage(LANG_SELECT_CREATURE);
        return true;
    }

    Player* player = m_session->GetPlayer();

    char* type = ExtractLiteralArg(&args);
    if (!type)
        return false;

    if (strncmp(type, "delay", strlen(type)) == 0)
    {
        uint32 delay;
        if (!ExtractUInt32(&args, delay))
            return false;
        if (player->GetMap()->GetCreatureGroupMgr().GetMovementMgr().SetDelay(
                waypoint, delay))
            return true;
    }
    else if (strncmp(type, "run", strlen(type)) == 0)
    {
        bool run;
        if (!ExtractOnOff(&args, run))
            return false;
        if (player->GetMap()->GetCreatureGroupMgr().GetMovementMgr().SetRun(
                waypoint, run))
            return true;
    }
    return false;
}

bool ChatHandler::HandlePetCommand(char* args)
{
    Creature* cpet = getSelectedCreature();
    if (!cpet || !cpet->IsPet())
    {
        SendSysMessage("Error: select target pet before using command.");
        return true;
    }
    Pet* pet = static_cast<Pet*>(cpet);

    // Players can have access to .pet during tests
    if (pet->GetOwner() != m_session->GetPlayer() &&
        m_session->GetSecurity() == SEC_PLAYER)
    {
        SendSysMessage("Error: you cannot modify that pet.");
        return true;
    }

    char* type = ExtractLiteralArg(&args);
    if (!type)
    {
        SendSysMessage("Usage: .pet <command> [opts...]");
        SendSysMessage(
            "Valid subcommands are: loyalty, tp, level, maxhappy. First three"
            " should be followed by an unsigned integer.");
        return true;
    }

    if (strcmp(type, "loyalty") == 0)
    {
        uint32 lvl;
        if (!ExtractUInt32(&args, lvl) || lvl > BEST_FRIEND)
        {
            SendSysMessage("Error: loyalty range must be [0, 6]");
            return true;
        }
        pet->SetLoyaltyLevel(static_cast<LoyaltyLevel>(lvl));
    }
    else if (strcmp(type, "tp") == 0)
    {
        if (pet->GetLoyaltyLevel() == 0)
        {
            SendSysMessage("Error: loyalty level too low.");
            return true;
        }

        uint32 tp;
        if (!ExtractUInt32(&args, tp) ||
            tp > pet->getLevel() * (pet->GetLoyaltyLevel() - 1))
        {
            SendSysMessage(
                "Error: training points cannot exceed level * (loyalty - 1).");
            return true;
        }
        pet->SetTP(tp);
    }
    else if (strcmp(type, "maxhappy") == 0)
    {
        pet->SetPower(POWER_HAPPINESS, pet->GetMaxPower(POWER_HAPPINESS));
    }
    else if (strcmp(type, "level") == 0)
    {
        uint32 level = 0;
        if (!ExtractUInt32(&args, level) || level <= 0 ||
            level > m_session->GetPlayer()->getLevel())
        {
            SendSysMessage("Error: level range must be [1, player_level]");
            return true;
        }
        pet->GivePetLevel(level);
    }
    else
    {
        SendSysMessage("Error: unrecognized subcommand");
        return true;
    }

    return true;
}

/**
 * BEGIN Helper functions & types for HandleTpCommand
 */
enum class tp_targets
{
    none,
    player,
    location,
    creature,
    gameobject,
    coordinates,
    selection
};

static bool extract_tp_args(char* c,
    std::vector<std::pair<char, std::string>>& options, tp_targets& tp_target,
    std::string& str)
{
    /* Usage: .tp [options] [target] str...
      -e                       Next argument in str is (creature, gobj) id
      -g                       Next argument in str is (player, creature, gobj)
                               DB GUID
      -p                       Disabled partial matching for str...
      -t=<player>              Player to teleport; you if not present */

    tp_target = tp_targets::none;

    bool read_target = false;

    while (*c)
    {
        while (isspace(*c))
            ++c;

        if (!*c)
            break;

        if (*c == '-' && !isdigit(*(c + 1)))
        {
            if (!isalpha(*++c))
                return false;

            if (*c == 't')
            {
                if (*++c != '=')
                    return false;
                ++c; // eat =
                std::string tmp;
                while (isalnum(*c))
                    tmp += tolower(*c++);
                if (tmp.empty())
                    return false;
                tmp[0] = toupper(tmp[0]);
                options.emplace_back('t', tmp);
            }
            else if (*c == 'e' || *c == 'g' || *c == 'p')
                options.emplace_back(*c++, "");
            else
                return false;
        }
        else
        {
            auto valid_char = [](char c)
            {
                return isalnum(c) || c == '\'' || c == '(' || c == ')' ||
                       c == '[' || c == ']' || c == '-' || c == '.';
            };
            if (!valid_char(*c))
                return false;

            std::string tmp;
            while (valid_char(*c))
                tmp += tolower(*c++);
            if (!read_target)
            {
                read_target = true; // even if not a target, the chance to have
                                    // a target is now gone
                if (!tmp.compare("player") || !tmp.compare("p"))
                    tp_target = tp_targets::player;
                else if (!tmp.compare("location") || !tmp.compare("loc"))
                    tp_target = tp_targets::location;
                else if (!tmp.compare("creature") || !tmp.compare("c"))
                    tp_target = tp_targets::creature;
                else if (!tmp.compare("gameobject") || !tmp.compare("go"))
                    tp_target = tp_targets::gameobject;
                else if (!tmp.compare("coordinates") || !tmp.compare("coords"))
                    tp_target = tp_targets::coordinates;
                else if (!tmp.compare("selection") || !tmp.compare("sel"))
                    tp_target = tp_targets::selection;
                if (tp_target != tp_targets::none)
                    continue; // don't add to str if a target was read
            }

            str += tmp + " ";
        }
    }

    // some options are limited to certain target types
    for (auto p : options)
    {
        if (p.first == 'e' && tp_target != tp_targets::creature &&
            tp_target != tp_targets::gameobject)
            return false;
        if (p.first == 'g' && tp_target != tp_targets::creature &&
            tp_target != tp_targets::gameobject &&
            tp_target != tp_targets::player)
            return false;
    }

    return true;
}

static void obj_coords(
    WorldObject* obj, float& x, float& y, float& z, float& o, uint32& map)
{
    obj->GetPosition(x, y, z);
    o = obj->GetO();
    map = obj->GetMapId();
}

static void field_coords(
    Field* field, float& x, float& y, float& z, float& o, uint32& map)
{
    x = field[0].GetFloat();
    y = field[1].GetFloat();
    z = field[2].GetFloat();
    o = field[3].GetFloat();
    map = field[4].GetUInt32();
}

static bool tp_player_guid(uint32 guid, float& x, float& y, float& z, float& o,
    uint32& map, Player*& plr_tp_target)
{
    if (Player* p =
            ObjectAccessor::FindPlayer(ObjectGuid(HIGHGUID_PLAYER, guid)))
    {
        plr_tp_target = p;
        obj_coords(p, x, y, z, o, map);
        return true;
    }
    else
    {
        std::unique_ptr<QueryResult> res(CharacterDatabase.PQuery(
            "SELECT position_x, position_y, position_z, orientation, map FROM "
            "characters WHERE guid=%u",
            guid));
        if (res)
        {
            field_coords(res->Fetch(), x, y, z, o, map);
            return true;
        }
    }
    return false;
}

static bool tp_creature_guid(uint32 guid, uint32 entry, float& x, float& y,
    float& z, float& o, uint32& map)
{
    std::unique_ptr<QueryResult> res;

    if (guid && entry)
        res.reset(WorldDatabase.PQuery(
            "SELECT position_x, position_y, position_z, orientation, map FROM "
            "creature WHERE guid=%u AND id=%u LIMIT 1",
            guid, entry));
    else if (guid)
        res.reset(WorldDatabase.PQuery(
            "SELECT position_x, position_y, position_z, orientation, map FROM "
            "creature WHERE guid=%u LIMIT 1",
            guid));
    else
        res.reset(WorldDatabase.PQuery(
            "SELECT position_x, position_y, position_z, orientation, map FROM "
            "creature WHERE id=%u LIMIT 1",
            entry));

    if (!res)
        return false;

    field_coords(res->Fetch(), x, y, z, o, map);
    return true;
}

static bool tp_gameobject_guid(uint32 guid, uint32 entry, float& x, float& y,
    float& z, float& o, uint32& map)
{
    std::unique_ptr<QueryResult> res;

    if (guid && entry)
        res.reset(WorldDatabase.PQuery(
            "SELECT position_x, position_y, position_z, orientation, map FROM "
            "gameobject WHERE guid=%u AND id=%u LIMIT 1",
            guid, entry));
    else if (guid)
        res.reset(WorldDatabase.PQuery(
            "SELECT position_x, position_y, position_z, orientation, map FROM "
            "gameobject WHERE guid=%u LIMIT 1",
            guid));
    else
        res.reset(WorldDatabase.PQuery(
            "SELECT position_x, position_y, position_z, orientation, map FROM "
            "gameobject WHERE id=%u LIMIT 1",
            entry));

    if (!res)
        return false;

    field_coords(res->Fetch(), x, y, z, o, map);
    return true;
}

static bool tp_player(const std::string& str, bool partial, float& x, float& y,
    float& z, float& o, uint32& map, Player*& plr_tp_target)
{
    // NOTE: str is already escaped

    // mysql will automatically put fully-matching str search as the first
    // result, which means we can limit 1
    std::unique_ptr<QueryResult> res;
    if (partial)
        res.reset(CharacterDatabase.PQuery(
            "SELECT position_x, position_y, position_z, orientation, map, guid "
            "FROM characters WHERE name LIKE '%s%%' LIMIT 1",
            str.c_str()));
    else
        res.reset(CharacterDatabase.PQuery(
            "SELECT position_x, position_y, position_z, orientation, map, guid "
            "FROM characters WHERE name='%s' LIMIT 1",
            str.c_str()));
    if (!res)
        return false;

    Field* field = res->Fetch();
    if (Player* p = ObjectAccessor::FindPlayer(
            ObjectGuid(HIGHGUID_PLAYER, field[5].GetUInt32())))
    {
        plr_tp_target = p;
        obj_coords(p, x, y, z, o, map);
    }
    else
        field_coords(field, x, y, z, o, map);
    return true;
}

static bool tp_location(const std::string& str, bool partial, float& x,
    float& y, float& z, float& o, uint32& map)
{
    auto loc = sObjectMgr::Instance()->GetGameTele(str, partial);
    if (!loc)
        return false;

    x = loc->position_x;
    y = loc->position_y;
    z = loc->position_z;
    o = loc->orientation;
    map = loc->mapId;
    return true;
}

static bool tp_creature(const std::string& str, bool partial, float& x,
    float& y, float& z, float& o, uint32& map)
{
    // NOTE: str is already escaped

    std::unique_ptr<QueryResult> res;
    if (partial)
        res.reset(WorldDatabase.PQuery(
            "SELECT position_x, position_y, position_z, orientation, map FROM "
            "creature, creature_template WHERE "
            "creature.id=creature_template.entry AND name LIKE '%%%s%%' LIMIT "
            "1",
            str.c_str()));
    else
        res.reset(WorldDatabase.PQuery(
            "SELECT position_x, position_y, position_z, orientation, map FROM "
            "creature, creature_template WHERE "
            "creature.id=creature_template.entry AND name='%s' LIMIT 1",
            str.c_str()));
    if (!res)
        return false;

    field_coords(res->Fetch(), x, y, z, o, map);
    return true;
}

static bool tp_gameobject(const std::string& str, bool partial, float& x,
    float& y, float& z, float& o, uint32& map)
{
    // NOTE: str is already escaped

    std::unique_ptr<QueryResult> res;
    if (partial)
        res.reset(WorldDatabase.PQuery(
            "SELECT position_x, position_y, position_z, orientation, map FROM "
            "gameobject, gameobject_template WHERE "
            "gameobject.id=gameobject_template.entry AND name LIKE '%%%s%%' "
            "LIMIT 1",
            str.c_str()));
    else
        res.reset(WorldDatabase.PQuery(
            "SELECT position_x, position_y, position_z, orientation, map FROM "
            "gameobject, gameobject_template WHERE "
            "gameobject.id=gameobject_template.entry AND name='%s' LIMIT 1",
            str.c_str()));
    if (!res)
        return false;

    field_coords(res->Fetch(), x, y, z, o, map);
    return true;
}

static void tp_helper(Player* target, float x, float y, float z, float o,
    uint32 map, std::shared_ptr<DungeonPersistentState> state = nullptr);

static bool has_opt(std::vector<std::pair<char, std::string>>& options, char c)
{
    for (auto p : options)
        if (p.first == c)
            return true;
    return false;
}

static std::string extract_opt(
    std::vector<std::pair<char, std::string>>& options, char c)
{
    for (auto p : options)
        if (p.first == c)
            return p.second;
    return "";
}

/**
 * END Helper functions & types for HandleTpCommand & HandleSummonCommand
 */

bool ChatHandler::HandleTpCommand(char* args)
{
    // dev_server: enable expensive searching by default if no more than 5
    // players are online
    bool dev_server =
        sObjectAccessor::Instance()->GetPlayers().get().size() <= 5;

    std::vector<std::pair<char, std::string>> options;
    tp_targets tp_target;
    std::string str;
    Player* plr_tp_target = nullptr;
    if (!extract_tp_args(args, options, tp_target, str))
        return false;

    Player* me = m_session->GetPlayer();

    float x = me->GetX(), y = me->GetY(), z = me->GetZ(), o = me->GetO();
    uint32 map = me->GetMapId();

    Player* target = nullptr;
    if (has_opt(options, 't'))
    {
        std::string name = extract_opt(options, 't');
        target = ObjectAccessor::FindPlayerByName(name);
        if (!target)
            PSendSysMessage(
                "Error: could not find online -t target with name '%s'. Use "
                ".summon to tp offline players.",
                name.c_str());
    }
    else
        target = m_session->GetPlayer();
    if (!target)
        return true;

    std::stringstream ss;
    ss << str;

    // str has special meaning with flag -e and -g
    if (has_opt(options, 'e') || has_opt(options, 'g'))
    {
        uint32 entry = 0, guid = 0;
        for (auto p : options)
        {
            if (p.first == 'e')
                ss >> entry;
            else if (p.first == 'g')
                ss >> guid;
        }

        bool found = false;
        if (tp_target == tp_targets::player && guid)
            found = tp_player_guid(guid, x, y, z, o, map, plr_tp_target);
        else if (tp_target == tp_targets::creature && (guid || entry))
            found = tp_creature_guid(guid, entry, x, y, z, o, map);
        else if (tp_target == tp_targets::gameobject && (guid || entry))
            found = tp_gameobject_guid(guid, entry, x, y, z, o, map);
        if (!found)
        {
            SendSysMessage("Error: no target found.");
            return true;
        }
    }
    // str has special meaning when tp_target are coordinates
    else if (tp_target == tp_targets::coordinates)
    {
        ss >> x >> y >> z >> map >> o;
    }
    else if (tp_target == tp_targets::selection)
    {
        auto sel = getSelectedUnit();
        if (!sel)
        {
            SendSysMessage("Error: no selected target.");
            return true;
        }
        x = sel->GetX();
        y = sel->GetY();
        z = sel->GetZ();
        map = sel->GetMapId();
        o = sel->GetO();
    }
    else
    {
        std::string remaining_str, tmp;
        while (ss >> tmp)
            remaining_str += tmp + " ";
        if (remaining_str.empty())
            return false;

        // disable expensive searches on live server
        bool partial =
            !has_opt(
                options, 'p'); // if -p is PRESENT partial matching is DISABLED
        if ((tp_target == tp_targets::creature ||
                tp_target == tp_targets::gameobject) &&
            !dev_server && partial)
        {
            SendSysMessage(
                "Error: partial creature/GO searching is disabled for server "
                "with more than 5 players. Either use -g/-e or -p.");
            return true;
        }

        // remove last space & escape
        remaining_str.pop_back();
        std::string unsafe_str = remaining_str;
        WorldDatabase.escape_string(remaining_str);

        if (tp_target == tp_targets::player || tp_target == tp_targets::none)
        {
            if (tp_player(
                    remaining_str, partial, x, y, z, o, map, plr_tp_target))
                goto found_tp_target;
        }
        if (tp_target == tp_targets::location || tp_target == tp_targets::none)
        {
            // don't pass escaped str to tp_location; it does not query the DB
            if (tp_location(unsafe_str, partial, x, y, z, o, map))
                goto found_tp_target;
        }
        if (tp_target == tp_targets::creature ||
            (dev_server && tp_target == tp_targets::none))
        {
            if (tp_creature(remaining_str, partial, x, y, z, o, map))
                goto found_tp_target;
        }
        if (tp_target == tp_targets::gameobject ||
            (dev_server && tp_target == tp_targets::none))
        {
            if (tp_gameobject(remaining_str, partial, x, y, z, o, map))
                goto found_tp_target;
        }

        if (!dev_server && tp_target == tp_targets::none)
            SendSysMessage(
                "Error: no target found. Note that the server has more than 5 "
                "players online, disabling implicit creature & GO search!");
        else
            SendSysMessage("Error: no target found.");
        return true;

    found_tp_target:
        ;
    }

    auto map_entry = sMapStore.LookupEntry(map);
    if (!map_entry)
    {
        SendSysMessage("Error: invalid map id.");
        return true;
    }

    if (target != me && map_entry->Instanceable() && plr_tp_target)
    {
        SendSysMessage(
            "Error: cannot tp -t target to another player inside an instance. "
            "You need to go there yourself and use .summon instead.");
        return true;
    }

    if (plr_tp_target && map_entry->Instanceable() &&
        target->GetMap() != plr_tp_target->GetMap())
    {
        if (!target->isGameMaster())
        {
            SendSysMessage(
                "Error: can only tp into someone's instance if GM mode is on.");
            return true;
        }

        if (target->GetMapId() == plr_tp_target->GetMapId())
        {
            SendSysMessage(
                "Error: cannot tp to another person's instance if you're in "
                "the same map id yourself.");
            return true;
        }

        // Battleground or Arena
        if (map_entry->IsBattleGroundOrArena())
        {
            if (target->GetBattleGround() &&
                target->GetBattleGround() != plr_tp_target->GetBattleGround())
            {
                SendSysMessage("Error: cannot tp from one BG to another.");
                return true;
            }
            target->SetBattleGroundId(plr_tp_target->GetBattleGroundId(),
                plr_tp_target->GetBattleGroundTypeId());
            target->SetBattleGroundEntryPoint();
            tp_helper(target, x, y, z, o, map);
        }
        // Dungeon or Raid
        else if (auto bind = plr_tp_target->GetInstanceBind(map_entry->MapID,
                     plr_tp_target->GetMap()->GetDifficulty()))
        {
            if (plr_tp_target->GetMap()->GetDifficulty() !=
                target->GetDifficulty())
            {
                SendSysMessage(
                    "Error: target has another difficulty set than you.");
                return true;
            }

            target->UnbindFromInstance(
                map_entry->MapID, plr_tp_target->GetMap()->GetDifficulty());
            if (auto state = bind->state.lock())
                tp_helper(target, x, y, z, o, map, state);
        }
        // Unknown instance type
        else
        {
            SendSysMessage("Error: instanceable map of unknown type.");
            return true;
        }
    }
    else
    {
        tp_helper(target, x, y, z, o, map);
    }

    return true;
}

bool ChatHandler::HandleSummonCommand(char* args)
{
    // TODO: We cannot summon someone into a battleground or arena at the
    // moment,
    //       we can only use .summon if they're in the battleground or arena we
    //       are in

    // Usage: .summon [options] [name]

    Player* me = m_session->GetPlayer();
    std::stringstream ss(args);

    bool group = false;
    std::string name;

    // Extract options and names
    std::string tmp;
    while (ss >> tmp)
    {
        if (tmp.size() < 2)
            return false;

        if (tmp[0] == '-')
        {
            if (tmp[1] == 'g')
                group = true;
            else
                return false;
            continue;
        }

        if (!name.empty())
            return false;
        name = tmp;
    }

    // Check if selected target is valid if name is empty
    Player* target;
    if (name.empty())
    {
        Player* selected = getSelectedPlayer();
        if (!selected)
            return false;
        if (selected == m_session->GetPlayer() && !group)
            return false;
        target = selected;
    }
    else
    {
        for (auto& elem : name)
            elem = tolower(elem);
        name[0] = toupper(name[0]);

        target = sObjectAccessor::Instance()->FindPlayerByName(name, false);
        if (!target)
        {
            // Cannot TP offline player with group option
            if (group)
            {
                SendSysMessage(
                    "Error: target cannot be offline with group flag enabled.");
                return true;
            }

            CharacterDatabase.escape_string(name);
            std::unique_ptr<QueryResult> res(CharacterDatabase.PQuery(
                "SELECT guid FROM characters WHERE name='%s'", name.c_str()));
            if (!res)
            {
                SendSysMessage("Error: no player by that name.");
                return true;
            }

            CharacterDatabase.PExecute(
                "UPDATE characters SET position_x=%f, position_y=%f, "
                "position_z=%f, orientation=%f, map=%u WHERE name='%s'",
                me->GetX(), me->GetY(), me->GetZ(), me->GetO(), me->GetMapId(),
                name.c_str());

            PSendSysMessage(
                "Teleported offline player '%s' to your coordinates.",
                name.c_str());
            return true;
        }

        if (!target->IsInWorld())
        {
            SendSysMessage("Error: target is currently in a loading screen.");
            return true;
        }
    }

    std::vector<Player*> targets;
    if (target != me)
        targets.push_back(target);
    if (group)
    {
        if (Group* grp = target->GetGroup())
            for (auto member : grp->members(true))
                if (member != target)
                    targets.push_back(member);
    }

    std::weak_ptr<DungeonPersistentState> dungeon_state;
    if (me->GetMap()->Instanceable())
    {
        uint32 map_id = me->GetMap()->GetId();
        Difficulty difficulty = me->GetMap()->GetDifficulty();

        auto me_bind = me->GetInstanceBind(map_id, difficulty);
        if (!me_bind && !me->GetMap()->IsBattleGroundOrArena())
        {
            SendSysMessage(
                "Error: You're in an instance without a bind. Unable to "
                "teleport anyone to you.");
            return true;
        }

        if (me_bind)
            dungeon_state = me_bind->state;

        for (auto itr = targets.begin(); itr != targets.end();)
        {
            auto bind = (*itr)->GetInstanceBind(map_id, difficulty);

            if (me->GetMap()->IsBattleGroundOrArena() &&
                (*itr)->GetMap() != me->GetMap())
            {
                PSendSysMessage(
                    "Failed for '%s': player not in the same battleground, "
                    "cannot port him in.",
                    (*itr)->GetName());
                itr = targets.erase(itr);
            }
            else if (bind && me_bind &&
                     bind->state.lock() != me_bind->state.lock())
            {
                PSendSysMessage(
                    "Failed for '%s': player bound to another instance.",
                    (*itr)->GetName());
                itr = targets.erase(itr);
            }
            else
            {
                ++itr;
            }
        }
    }

    for (auto p : targets)
        tp_helper(p, me->GetX(), me->GetY(), me->GetZ(), me->GetO(),
            me->GetMapId(), dungeon_state.lock());

    return true;
}

static void tp_helper(Player* target, float x, float y, float z, float o,
    uint32 map, std::shared_ptr<DungeonPersistentState> state)
{
    // stop flight if flying
    if (target->IsTaxiFlying())
    {
        target->movement_gens.remove_all(movement::gen::flight);
        target->m_taxi.ClearTaxiDestinations();
    }

    if (!maps::verify_coords(x, y))
        return;

    if (state &&
        !target->GetInstanceBind(state->GetMapId(), state->GetDifficulty()))
        target->BindToInstance(state, !state->CanReset());

    target->TeleportTo(map, x, y, z, o);
}
