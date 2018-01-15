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

#include "BattleGroundMgr.h"
#include "Chat.h"
#include "Common.h"
#include "DBCStores.h"
#include "GossipDef.h"
#include "Group.h"
#include "Language.h"
#include "logging.h"
#include "MapManager.h"
#include "ObjectGuid.h"
#include "ObjectMgr.h"
#include "Opcodes.h"
#include "PathFinder.h"
#include "Player.h"
#include "SpellMgr.h"
#include "Unit.h"
#include "VMapFactory.h"
#include "World.h"
#include "WorldPacket.h"
#include "battlefield_queue.h"
#include "loot_distributor.h"
#include "threaded_maps.h"
#include "profiling/map_updates.h"
#include "Database/DatabaseEnv.h"
#include "G3D/Vector3.h"
#include <cfloat>
#include <fstream>

#ifdef PERF_SAMPLING_MAP_UPDATE
namespace profiling
{
extern map_update map_update_;
}
#endif

bool ChatHandler::HandleDebugSendResetFailCommand(char* args)
{
    if (!*args)
        return false;

    uint32 reason;
    if (!ExtractUInt32(&args, reason))
        return false;

    uint32 map;
    if (!ExtractUInt32(&args, map))
        return false;

    m_session->GetPlayer()->SendResetInstanceFailed(
        InstanceResetFailMsg(reason), map);
    return true;
}

bool ChatHandler::HandleDebugSendDifficultyCommand(char* args)
{
    if (!*args)
        return false;

    uint32 diff, value, ingroup;
    if (!ExtractUInt32(&args, diff))
        return false;
    if (!ExtractUInt32(&args, value) || value > 255)
        return false;
    if (!ExtractUInt32(&args, ingroup))
        return false;

    WorldPacket data(MSG_SET_DUNGEON_DIFFICULTY, 12);
    data << diff;
    data << uint8(value);
    data << ingroup;
    m_session->send_packet(std::move(data));

    return true;
}

bool ChatHandler::HandleDebugSendAuctionHouseError(char* args)
{
    uint32 action, error;
    if (!ExtractUInt32(&args, action) || !ExtractUInt32(&args, error))
    {
        SendSysMessage(
            "Usage: .debug send auctionerror <AuctionAction> <AuctionError>");
        return true;
    }

    if (error == AUCTION_OK || error == AUCTION_ERR_INVENTORY ||
        error == AUCTION_ERR_HIGHER_BID)
    {
        SendSysMessage("Cannot use those errors.");
        return true;
    }

    m_session->SendAuctionCommandResult(nullptr,
        static_cast<AuctionAction>(action), static_cast<AuctionError>(error));
    return true;
}

bool ChatHandler::HandleDebugSendSpellFailCommand(char* args)
{
    if (!*args)
        return false;

    uint32 failnum;
    if (!ExtractUInt32(&args, failnum) || failnum > 255)
        return false;

    uint32 failarg1;
    if (!ExtractOptUInt32(&args, failarg1, 0))
        return false;

    uint32 failarg2;
    if (!ExtractOptUInt32(&args, failarg2, 0))
        return false;

    WorldPacket data(SMSG_CAST_FAILED, 5);
    data << uint32(133);
    data << uint8(failnum);
    if (failarg1 || failarg2)
        data << uint32(failarg1);
    if (failarg2)
        data << uint32(failarg2);

    m_session->send_packet(std::move(data));

    return true;
}

bool ChatHandler::HandleDebugSendPoiCommand(char* args)
{
    Player* pPlayer = m_session->GetPlayer();
    Unit* target = getSelectedUnit();
    if (!target)
    {
        SendSysMessage(LANG_SELECT_CHAR_OR_CREATURE);
        return true;
    }

    uint32 icon;
    if (!ExtractUInt32(&args, icon))
        return false;

    uint32 flags;
    if (!ExtractUInt32(&args, flags))
        return false;

    LOG_DEBUG(logging, "Command : POI, NPC = %u, icon = %u flags = %u",
        target->GetGUIDLow(), icon, flags);
    pPlayer->PlayerTalkClass->SendPointOfInterest(
        target->GetX(), target->GetY(), Poi_Icon(icon), flags, 30, "Test POI");
    return true;
}

bool ChatHandler::HandleDebugSendEquipErrorCommand(char* args)
{
    if (!*args)
        return false;

    uint8 msg = atoi(args);
    m_session->GetPlayer()->SendEquipError(
        InventoryResult(msg), nullptr, nullptr);
    return true;
}

bool ChatHandler::HandleDebugSendSellErrorCommand(char* args)
{
    if (!*args)
        return false;

    uint8 msg = atoi(args);
    m_session->GetPlayer()->SendSellError(
        SellResult(msg), nullptr, ObjectGuid(), 0);
    return true;
}

bool ChatHandler::HandleDebugSendBuyErrorCommand(char* args)
{
    if (!*args)
        return false;

    uint8 msg = atoi(args);
    m_session->GetPlayer()->SendBuyError(BuyResult(msg), nullptr, 0, 0);
    return true;
}

bool ChatHandler::HandleDebugSendOpcodeCommand(char* /*args*/)
{
    Unit* unit = getSelectedUnit();
    if (!unit || (unit->GetTypeId() != TYPEID_PLAYER))
        unit = m_session->GetPlayer();

    std::ifstream ifs("opcode.txt");
    if (ifs.bad())
        return false;

    uint32 opcode;
    ifs >> opcode;

    WorldPacket data(opcode, 0);

    while (!ifs.eof())
    {
        std::string type;
        ifs >> type;

        if (type == "")
            break;

        if (type == "uint8")
        {
            uint16 val1;
            ifs >> val1;
            data << uint8(val1);
        }
        else if (type == "uint16")
        {
            uint16 val2;
            ifs >> val2;
            data << val2;
        }
        else if (type == "uint32")
        {
            uint32 val3;
            ifs >> val3;
            data << val3;
        }
        else if (type == "uint64")
        {
            uint64 val4;
            ifs >> val4;
            data << val4;
        }
        else if (type == "float")
        {
            float val5;
            ifs >> val5;
            data << val5;
        }
        else if (type == "string")
        {
            std::string val6;
            ifs >> val6;
            data << val6;
        }
        else if (type == "pguid")
        {
            data << unit->GetPackGUID();
        }
        else
        {
            LOG_DEBUG(
                logging, "Sending opcode: unknown type '%s'", type.c_str());
            break;
        }
    }
    ifs.close();
    LOG_DEBUG(logging, "Sending opcode %u", data.opcode());
    data.hexlike();
    ((Player*)unit)->GetSession()->send_packet(std::move(data));
    PSendSysMessage(LANG_COMMAND_OPCODESENT, data.opcode(), unit->GetName());
    return true;
}

bool ChatHandler::HandleDebugUpdateWorldStateCommand(char* args)
{
    uint32 world;
    if (!ExtractUInt32(&args, world))
        return false;

    uint32 state;
    if (!ExtractUInt32(&args, state))
        return false;

    m_session->GetPlayer()->SendUpdateWorldState(world, state);
    return true;
}

bool ChatHandler::HandleDebugPathingAdtCommand(char* args)
{
    uint32 n;
    float dist;
    bool tp = true;

    if (!ExtractUInt32(&args, n))
        return false;

    ExtractOptFloat(&args, dist, 4);

    char* str = ExtractLiteralArg(&args);
    if (str && strcmp(str, "no") == 0)
        tp = false;

    Player* p = m_session->GetPlayer();
    float x, y, z, o;
    p->GetPosition(x, y, z);
    o = p->GetO();

    uint32 max_n = n;
    while (n--)
    {
        x = x + dist * cos(o);
        y = y + dist * sin(o);

        if (auto grid = const_cast<TerrainInfo*>(p->GetMap()->GetTerrain())
                            ->GetGrid(x, y))
            z = grid->getHeight(x, y);

        if (tp)
        {
            p->queue_action((max_n - n) * 300, [p, x, y, z, o]()
                {
                    p->NearTeleportTo(x, y, z, o);
                });
        }
    }

    return true;
}

bool ChatHandler::HandleDebugPathingPathCommand(char* args)
{
    Player* p = m_session->GetPlayer();

    Unit* u = getSelectedUnit();
    if (!u || u == p)
        return false;

    bool reverse = false;
    char* lit = ExtractLiteralArg(&args);
    if (lit && !strcmp(lit, "reverse"))
        reverse = true;

    std::vector<G3D::Vector3> points;
    bool in_place;

    auto render_points = [](Unit* u, std::vector<G3D::Vector3> path, uint32)
    {
        static const uint32 WAYPOINT_NPC_ENTRY = 1;
        for (auto& elem : path)
            u->SummonCreature(WAYPOINT_NPC_ENTRY, elem.x, elem.y, elem.z, 0,
                TEMPSUMMON_TIMED_DESPAWN, 9000);
    };

    std::string ms;
    {
        scope_performance_timer t("", &ms);
        if (!reverse)
            in_place = movement::BuildRetailLikePath(
                           points, p, u, 0.0f, render_points) == 0;
        else
            in_place = movement::BuildRetailLikePath(
                           points, u, p, 0.0f, render_points) == 0;
    }

    if (in_place)
        PSendSysMessage("Path generated in: %s (points: %u).", ms.c_str(),
            (uint32)points.size());
    else
        PSendSysMessage(
            "Queued path for NavMesh generation. Took: %s", ms.c_str());

    if (in_place)
        render_points(p, points, 0);

    return true;
}

bool ChatHandler::HandleDebugPathingPositionCommand(char* args)
{
    Player* p = m_session->GetPlayer();

    Unit* tar = getSelectedUnit();
    if (tar == p)
        tar = nullptr;

    float ori, dist;
    if (!tar)
    {
        if (!ExtractFloat(&args, ori))
            return false;
    }
    if (!ExtractFloat(&args, dist))
        return false;

    bool normalize = false;
    char* lit = ExtractLiteralArg(&args);
    if (lit && !strcmp(lit, "norm"))
        normalize = true;

    G3D::Vector3 point;
    std::string ms;
    {
        scope_performance_timer t("", &ms);
        if (tar)
        {
            point = tar->GetPoint(p, dist, normalize);
        }
        else
            point = p->GetPoint(ori, dist, normalize);
    }

    PSendSysMessage("Point found in %s (normalized Z: %s).", ms.c_str(),
        normalize ? "yes" : "no");

    static const uint32 WAYPOINT_NPC_ENTRY = 1;
    p->SummonCreature(WAYPOINT_NPC_ENTRY, point.x, point.y, point.z, 0,
        TEMPSUMMON_TIMED_DESPAWN, 9000);

    return true;
}

bool ChatHandler::HandleDebugPathingWmoCommand(char* args)
{
    uint32 n;
    float dist;
    bool tp = true;

    if (!ExtractUInt32(&args, n))
        return false;

    ExtractOptFloat(&args, dist, 4);

    char* str = ExtractLiteralArg(&args);
    if (str && strcmp(str, "no") == 0)
        tp = false;

    Player* p = m_session->GetPlayer();
    float x, y, z, o;

    p->GetPosition(x, y, z);
    o = p->GetO();

    auto vmgr = VMAP::VMapFactory::createOrGetVMapManager();

    std::string ms;
    {
        scope_performance_timer t("", &ms);

        uint32 max_n = n;
        while (n--)
        {
            float old_x = x, old_y = y, old_z = z;

            x = x + dist * cos(o);
            y = y + dist * sin(o);

            z = vmgr->getWmoHeight(p->GetMapId(), x, y, z + 6.0f, 20.0f);

            if (z <= VMAP_INVALID_HEIGHT)
            {
                PSendSysMessage(
                    "Unable to find height of position; you're probably not "
                    "on-top of a WMO, or dist is too high. Finished: %u "
                    "points.",
                    max_n - (n + 1));
                return true;
            }

            if (!vmgr->isInWmoLineOfSight(
                    p->GetMapId(), old_x, old_y, old_z + 2.0f, x, y, z + 2.0f))
            {
                PSendSysMessage(
                    "Error, there seems to be something in the way. Finished: "
                    "%u points.",
                    max_n - (n + 1));
                return true;
            }

            if (tp)
            {
                p->queue_action((max_n - n) * 300, [p, x, y, z, o]()
                    {
                        p->NearTeleportTo(x, y, z, o);
                    });
            }
        }
    }

    PSendSysMessage("Finished in %s.", ms.c_str());

    return true;
}

bool ChatHandler::HandleDebugPlayCinematicCommand(char* args)
{
    // USAGE: .debug play cinematic #cinematicid
    // #cinematicid - ID decimal number from CinemaicSequences.dbc (1st column)
    uint32 dwId;
    if (!ExtractUInt32(&args, dwId))
        return false;

    if (!sCinematicSequencesStore.LookupEntry(dwId))
    {
        PSendSysMessage(LANG_CINEMATIC_NOT_EXIST, dwId);
        SetSentErrorMessage(true);
        return false;
    }

    m_session->GetPlayer()->SendCinematicStart(dwId);
    return true;
}

// Play sound
bool ChatHandler::HandleDebugPlaySoundCommand(char* args)
{
    // USAGE: .debug playsound #soundid
    // #soundid - ID decimal number from SoundEntries.dbc (1st column)
    uint32 dwSoundId;
    if (!ExtractUInt32(&args, dwSoundId))
        return false;

    if (!sSoundEntriesStore.LookupEntry(dwSoundId))
    {
        PSendSysMessage(LANG_SOUND_NOT_EXIST, dwSoundId);
        SetSentErrorMessage(true);
        return false;
    }

    Unit* unit = getSelectedUnit();
    if (!unit)
    {
        SendSysMessage(LANG_SELECT_CHAR_OR_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    if (m_session->GetPlayer()->GetSelectionGuid())
        unit->PlayDistanceSound(dwSoundId, m_session->GetPlayer());
    else
        unit->PlayDirectSound(dwSoundId, m_session->GetPlayer());

    PSendSysMessage(LANG_YOU_HEAR_SOUND, dwSoundId);
    return true;
}

// Send notification in channel
bool ChatHandler::HandleDebugSendChannelNotifyCommand(char* args)
{
    const char* name = "test";

    uint32 code;
    if (!ExtractUInt32(&args, code) || code > 255)
        return false;

    WorldPacket data(SMSG_CHANNEL_NOTIFY, (1 + 10));
    data << uint8(code); // notify type
    data << name;        // channel name
    data << uint32(0);
    data << uint32(0);
    m_session->send_packet(std::move(data));
    return true;
}

// Send notification in chat
bool ChatHandler::HandleDebugSendChatMsgCommand(char* args)
{
    const char* msg = "testtest";

    uint32 type;
    if (!ExtractUInt32(&args, type) || type > 255)
        return false;

    WorldPacket data;
    ChatHandler::FillMessageData(&data, m_session, type, 0, "chan",
        m_session->GetPlayer()->GetObjectGuid(), msg, m_session->GetPlayer());
    m_session->send_packet(std::move(data));
    return true;
}

bool ChatHandler::HandleDebugSendQuestPartyMsgCommand(char* args)
{
    uint32 msg;
    if (!ExtractUInt32(&args, msg))
        return false;

    m_session->GetPlayer()->SendPushToPartyResponse(
        m_session->GetPlayer(), msg);
    return true;
}

bool ChatHandler::HandleDebugGetLootRecipientCommand(char* /*args*/)
{
    Creature* target = getSelectedCreature();
    if (!target)
        return false;

    if (!target->GetLootDistributor() ||
        target->GetLootDistributor()->loot_type() == LOOT_PICKPOCKETING)
    {
        SendSysMessage("Target has no corpse loot distributor, yet.");
        return true;
    }

    // List of tappers sorted alphabetically
    std::set<std::string> tappers; // sorted container
    for (const auto& elem :
        *target->GetLootDistributor()->recipient_mgr()->taps())
        if (Player* plr = sObjectMgr::Instance()->GetPlayer(elem))
            tappers.insert(plr->GetName());

    std::string tapper_str = "List of all tappers: ";
    for (auto itr = tappers.begin(); itr != tappers.end();)
    {
        tapper_str += *itr;
        if ((++itr) == tappers.end())
            tapper_str += ".";
        else
            tapper_str += ", ";
    }

    PSendSysMessage("%s", tapper_str.c_str());

    Group* group = target->GetLootDistributor()->recipient_mgr()->group();
    PSendSysMessage("Group: %i (leader: %s). Recipients: " SIZEFMTD
                    ". Type: %s.",
        group ? group->GetId() : 0, group ? group->GetLeaderName() : "N/A",
        target->GetLootDistributor()->recipient_mgr()->taps()->size(),
        target->GetLootDistributor()->loot_type() == LOOT_SKINNING ?
            "skinning" :
            "corpse");

    if (target->isDead() && target->GetLootDistributor()->loot())
    {
        SendSysMessage("Loot on corpse is:");
        for (uint32 i = 0; i < target->GetLootDistributor()->loot()->size();
             ++i)
        {
            const LootItem* item =
                target->GetLootDistributor()->loot()->get_slot_item(i);
            const ItemPrototype* proto =
                sItemStorage.LookupEntry<ItemPrototype>(item->itemid);
            if (proto)
            {
                // TODO: Use of GetItemLocaleStrings() to report name based on
                // client language
                PSendSysMessage(
                    "%d - |cffffffff|Hitem:%d:0:0:0:0:0:0:0|h[%s]|h|r -- "
                    "Count: %i. Condition id: %i. TC conditions: %s. Quest: "
                    "%s. One Per Player: %s. Looted: %s. Blocked: %s.",
                    proto->ItemId, proto->ItemId, proto->Name1, item->count,
                    item->conditionId, (item->has_conditions ? "yes" : "no"),
                    (item->needs_quest ? "yes" : "no"),
                    (item->one_per_player ? "yes" : "no"),
                    (item->is_looted ? "yes" : "no"),
                    (item->is_blocked ? "yes" : "no"));
            }
        }
    }

    return true;
}

bool ChatHandler::HandleDebugSendQuestInvalidMsgCommand(char* args)
{
    uint32 msg = atol(args);
    m_session->GetPlayer()->SendCanTakeQuestResponse(msg);
    return true;
}

bool ChatHandler::HandleDebugPvPCommand(char* /*args*/)
{
    if (!sBattleGroundMgr::Instance()->debugging())
    {
        SendSysMessage(
            "Skirmish arenas and battlegrounds have been set to pop as soon as "
            "a 1v1 match-up can be found.");
        sBattleGroundMgr::Instance()->set_debugging(true);
    }
    else
    {
        SendSysMessage(
            "Skirmish arena and battleground debugging has been turned OFF.");
        sBattleGroundMgr::Instance()->set_debugging(false);
    }
    return true;
}

bool ChatHandler::HandleDebugSpellCheckCommand(char* /*args*/)
{
    logging.info(
        "Check expected in code spell properties base at table 'spell_check' "
        "content...");
    sSpellMgr::Instance()->CheckUsedSpells("spell_check");
    return true;
}

// show animation
bool ChatHandler::HandleDebugAnimCommand(char* args)
{
    uint32 emote_id;
    if (!ExtractUInt32(&args, emote_id))
        return false;

    m_session->GetPlayer()->HandleEmoteCommand(emote_id);
    return true;
}

bool ChatHandler::HandleDebugSamplingMapCommand(char* args)
{
#ifndef PERF_SAMPLING_MAP_UPDATE
    SendSysMessage("CMake script must be run with -DENABLE_SAMPLING=1");
    return true;
#endif

    uint32 m;
    if (!ExtractUInt32(&args, m))
    {
        SendSysMessage("Usage: .debug sampling map <mapid>");
        return true;
    }

    int map_id = (int)m;

    PSendSysMessage(
        "Snapshot for map with id %d written to `map_updates.txt`", map_id);

#ifdef PERF_SAMPLING_MAP_UPDATE
    profiling::map_update_.start(map_id);
#endif

    return true;
}

bool ChatHandler::HandleDebugSetAuraStateCommand(char* args)
{
    int32 state;
    if (!ExtractInt32(&args, state))
        return false;

    Unit* unit = getSelectedUnit();
    if (!unit)
    {
        SendSysMessage(LANG_SELECT_CHAR_OR_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    if (!state)
    {
        // reset all states
        for (int i = 1; i <= 32; ++i)
            unit->ModifyAuraState(AuraState(i), false);
        return true;
    }

    unit->ModifyAuraState(AuraState(abs(state)), state > 0);
    return true;
}

bool ChatHandler::HandleSetValueHelper(
    Object* target, uint32 field, char* typeStr, char* valStr)
{
    ObjectGuid guid = target->GetObjectGuid();

    // not allow access to nonexistent or critical for work field
    if (field >= target->GetValuesCount() || field <= OBJECT_FIELD_ENTRY)
    {
        PSendSysMessage(LANG_TOO_BIG_INDEX, field, guid.GetString().c_str(),
            target->GetValuesCount());
        return false;
    }

    uint32 base; // 0 -> float
    if (!typeStr)
        base = 10;
    else if (strncmp(typeStr, "int", strlen(typeStr)) == 0)
        base = 10;
    else if (strncmp(typeStr, "hex", strlen(typeStr)) == 0)
        base = 16;
    else if (strncmp(typeStr, "bit", strlen(typeStr)) == 0)
        base = 2;
    else if (strncmp(typeStr, "float", strlen(typeStr)) == 0)
        base = 0;
    else
        return false;

    if (base)
    {
        uint32 iValue;
        if (!ExtractUInt32Base(&valStr, iValue, base))
            return false;

        LOG_DEBUG(logging, GetMangosString(LANG_SET_UINT),
            guid.GetString().c_str(), field, iValue);
        target->SetUInt32Value(field, iValue);
        PSendSysMessage(
            LANG_SET_UINT_FIELD, guid.GetString().c_str(), field, iValue);
    }
    else
    {
        float fValue;
        if (!ExtractFloat(&valStr, fValue))
            return false;

        LOG_DEBUG(logging, GetMangosString(LANG_SET_FLOAT),
            guid.GetString().c_str(), field, fValue);
        target->SetFloatValue(field, fValue);
        PSendSysMessage(
            LANG_SET_FLOAT_FIELD, guid.GetString().c_str(), field, fValue);
    }

    return true;
}

bool ChatHandler::HandleDebugSetItemValueCommand(char* args)
{
    uint32 guid;
    if (!ExtractUInt32(&args, guid))
        return false;

    uint32 field;
    if (!ExtractUInt32(&args, field))
        return false;

    char* typeStr = ExtractOptNotLastArg(&args);
    if (!typeStr)
        return false;

    char* valStr = ExtractLiteralArg(&args);
    if (!valStr)
        return false;

    Item* item =
        m_session->GetPlayer()->GetItemByGuid(ObjectGuid(HIGHGUID_ITEM, guid));
    if (!item)
        return false;

    return HandleSetValueHelper(item, field, typeStr, valStr);
}

bool ChatHandler::HandleDebugSetValueCommand(char* args)
{
    Unit* target = getSelectedUnit();
    if (!target)
    {
        SendSysMessage(LANG_SELECT_CHAR_OR_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    uint32 field;
    if (!ExtractUInt32(&args, field))
        return false;

    char* typeStr = ExtractOptNotLastArg(&args);
    if (!typeStr)
        return false;

    char* valStr = ExtractLiteralArg(&args);
    if (!valStr)
        return false;

    return HandleSetValueHelper(target, field, typeStr, valStr);
}

bool ChatHandler::HandleGetValueHelper(
    Object* target, uint32 field, char* typeStr)
{
    ObjectGuid guid = target->GetObjectGuid();

    if (field >= target->GetValuesCount())
    {
        PSendSysMessage(LANG_TOO_BIG_INDEX, field, guid.GetString().c_str(),
            target->GetValuesCount());
        return false;
    }

    uint32 base; // 0 -> float
    if (!typeStr)
        base = 10;
    else if (strncmp(typeStr, "int", strlen(typeStr)) == 0)
        base = 10;
    else if (strncmp(typeStr, "hex", strlen(typeStr)) == 0)
        base = 16;
    else if (strncmp(typeStr, "bit", strlen(typeStr)) == 0)
        base = 2;
    else if (strncmp(typeStr, "float", strlen(typeStr)) == 0)
        base = 0;
    else
        return false;

    if (base)
    {
        uint32 iValue = target->GetUInt32Value(field);

        switch (base)
        {
        case 2:
        {
            // starting 0 if need as required bitstring format
            std::string res;
            res.reserve(1 + 32 + 1);
            res = iValue & (1 << (32 - 1)) ? "0" : " ";
            for (int i = 32; i > 0; --i)
                res += iValue & (1 << (i - 1)) ? "1" : "0";
            LOG_DEBUG(logging, GetMangosString(LANG_GET_BITSTR),
                guid.GetString().c_str(), field, res.c_str());
            PSendSysMessage(LANG_GET_BITSTR_FIELD, guid.GetString().c_str(),
                field, res.c_str());
            break;
        }
        case 16:
            LOG_DEBUG(logging, GetMangosString(LANG_GET_HEX),
                guid.GetString().c_str(), field, iValue);
            PSendSysMessage(
                LANG_GET_HEX_FIELD, guid.GetString().c_str(), field, iValue);
            break;
        case 10:
        default:
            LOG_DEBUG(logging, GetMangosString(LANG_GET_UINT),
                guid.GetString().c_str(), field, iValue);
            PSendSysMessage(
                LANG_GET_UINT_FIELD, guid.GetString().c_str(), field, iValue);
        }
    }
    else
    {
        float fValue = target->GetFloatValue(field);
        LOG_DEBUG(logging, GetMangosString(LANG_GET_FLOAT),
            guid.GetString().c_str(), field, fValue);
        PSendSysMessage(
            LANG_GET_FLOAT_FIELD, guid.GetString().c_str(), field, fValue);
    }

    return true;
}

bool ChatHandler::HandleDebugGetItemValueCommand(char* args)
{
    uint32 guid;
    if (!ExtractUInt32(&args, guid))
        return false;

    uint32 field;
    if (!ExtractUInt32(&args, field))
        return false;

    char* typeStr = ExtractLiteralArg(&args);
    if (!typeStr && *args) // optional arg but check format fail case
        return false;

    Item* item =
        m_session->GetPlayer()->GetItemByGuid(ObjectGuid(HIGHGUID_ITEM, guid));
    if (!item)
        return false;

    return HandleGetValueHelper(item, field, typeStr);
}

bool ChatHandler::HandleDebugGetValueCommand(char* args)
{
    Unit* target = getSelectedUnit();
    if (!target)
    {
        SendSysMessage(LANG_SELECT_CHAR_OR_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    uint32 field;
    if (!ExtractUInt32(&args, field))
        return false;

    char* typeStr = ExtractLiteralArg(&args);
    if (!typeStr && *args) // optional arg but check format fail case
        return false;

    return HandleGetValueHelper(target, field, typeStr);
}

bool ChatHandler::HandleDebugItemDisplay(char* args)
{
    Creature* target = getSelectedCreature();
    if (!target)
    {
        SendSysMessage(LANG_SELECT_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    uint32 itemSlot;
    if (!ExtractUInt32(&args, itemSlot) || itemSlot >= MAX_VIRTUAL_ITEM_SLOT)
        return false;

    uint32 itemId;
    if (!ExtractUInt32(&args, itemId))
        return false;

    bool raw = false;
    bool reset = false;
    uint32 info0 = 0;
    uint32 info1 = 0;

    while (char* extraStr = ExtractLiteralArg(&args))
    {
        bool validFlag = false;
        if (strcmp(extraStr, "--raw") == 0 || strcmp(extraStr, "-r") == 0)
        {
            raw = true;
            validFlag = true;
        }
        else if (strcmp(extraStr, "--info0") == 0 ||
                 strcmp(extraStr, "-i0") == 0)
        {
            uint32 info;
            if (ExtractUInt32(&args, info))
            {
                info0 = info;
                validFlag = true;
            }
        }
        else if (strcmp(extraStr, "--info1") == 0 ||
                 strcmp(extraStr, "-i1") == 0)
        {
            uint32 info;
            if (ExtractUInt32(&args, info))
            {
                info1 = info;
                validFlag = true;
            }
        }
        else if (strcmp(extraStr, "--reset") == 0 ||
                 strcmp(extraStr, "-res") == 0)
        {
            reset = true;
            validFlag = true;
        }

        if (!validFlag)
        {
            SendSysMessage("Unrecognized flag. Possible flags are:");
            SendSysMessage("-r --raw : Use raw equipment.");
            SendSysMessage(
                "-i0 --info0 : Info 0 data (only used in conjunction with "
                "raw).");
            SendSysMessage(
                "-i1 --info1 : Info 1 data (only used in conjunction with "
                "raw).");
            SendSysMessage("-res --reset : Resets slot to default value.");
            SetSentErrorMessage(true);
            return false;
        }
    }

    if (reset)
        target->LoadEquipment(target->GetCreatureInfo()->equipmentId, true);
    else if (raw)
        target->SetVirtualItemRaw(
            (VirtualItemSlot)itemSlot, itemId, info0, info1);
    else
        target->SetVirtualItem((VirtualItemSlot)itemSlot, itemId);

    return true;
}

bool ChatHandler::HandleDebugItemDurabilityCommand(char* args)
{
    char* c_id = ExtractKeyFromLink(&args, "Hitem");
    if (!c_id)
    {
        SendSysMessage(
            "Usage: \".debug itemdurability <entry or item link> <relative "
            "value>");
        return true;
    }

    uint32 item_id = 0;
    if (!ExtractUInt32(&c_id, item_id)) // [name] manual form
    {
        std::string item_name = c_id;
        WorldDatabase.escape_string(item_name);
        std::unique_ptr<QueryResult> result(WorldDatabase.PQuery(
            "SELECT entry FROM item_template WHERE name = '%s'",
            item_name.c_str()));
        if (!result)
        {
            PSendSysMessage(LANG_COMMAND_COULDNOTFIND, c_id);
            SetSentErrorMessage(true);
            return false;
        }
        item_id = result->Fetch()->GetUInt16();
    }

    int32 value;
    if (!ExtractInt32(&args, value))
    {
        SendSysMessage(
            "Usage: \".debug itemdurability <entry or item link> <relative "
            "value>");
        return true;
    }

    Player* player = m_session->GetPlayer();
    Item* item = nullptr;

    const int mask = inventory::personal_storage::iterator::all;
    for (inventory::personal_storage::iterator itr =
             player->storage().begin(mask);
         itr != player->storage().end(); ++itr)
    {
        if ((*itr)->GetEntry() == item_id)
        {
            item = *itr;
            break;
        }
    }

    if (!item)
    {
        PSendSysMessage(
            "Item with item id %u was not found in your inventory.", item_id);
        return true;
    }

    player->durability(item, false, value);
    return true;
}

bool ChatHandler::HandlerDebugModValueHelper(
    Object* target, uint32 field, char* typeStr, char* valStr)
{
    ObjectGuid guid = target->GetObjectGuid();

    // not allow access to nonexistent or critical for work field
    if (field >= target->GetValuesCount() || field <= OBJECT_FIELD_ENTRY)
    {
        PSendSysMessage(LANG_TOO_BIG_INDEX, field, guid.GetString().c_str(),
            target->GetValuesCount());
        return false;
    }

    uint32 type; // 0 -> float 1 -> int add 2-> bit or 3 -> bit and  4 -> bit
                 // and not
    if (strncmp(typeStr, "int", strlen(typeStr)) == 0)
        type = 1;
    else if (strncmp(typeStr, "float", strlen(typeStr)) == 0)
        type = 0;
    else if (strncmp(typeStr, "|=", strlen("|=") + 1) == 0) // exactly copy
        type = 2;
    else if (strncmp(typeStr, "&=", strlen("&=") + 1) == 0) // exactly copy
        type = 3;
    else if (strncmp(typeStr, "&=~", strlen("&=~") + 1) == 0) // exactly copy
        type = 4;
    else
        return false;

    if (type)
    {
        uint32 iValue;
        if (!ExtractUInt32Base(&valStr, iValue, type == 1 ? 10 : 16))
            return false;

        uint32 value = target->GetUInt32Value(field);

        switch (type)
        {
        default:
        case 1: // int +
            value = uint32(int32(value) + int32(iValue));
            LOG_DEBUG(logging, GetMangosString(LANG_CHANGE_INT32),
                guid.GetString().c_str(), field, iValue, value, value);
            PSendSysMessage(LANG_CHANGE_INT32_FIELD, guid.GetString().c_str(),
                field, iValue, value, value);
            break;
        case 2: // |= bit or
            value |= iValue;
            LOG_DEBUG(logging, GetMangosString(LANG_CHANGE_HEX),
                guid.GetString().c_str(), field, typeStr, iValue, value);
            PSendSysMessage(LANG_CHANGE_HEX_FIELD, guid.GetString().c_str(),
                field, typeStr, iValue, value);
            break;
        case 3: // &= bit and
            value &= iValue;
            LOG_DEBUG(logging, GetMangosString(LANG_CHANGE_HEX),
                guid.GetString().c_str(), field, typeStr, iValue, value);
            PSendSysMessage(LANG_CHANGE_HEX_FIELD, guid.GetString().c_str(),
                field, typeStr, iValue, value);
            break;
        case 4: // &=~ bit and not
            value &= ~iValue;
            LOG_DEBUG(logging, GetMangosString(LANG_CHANGE_HEX),
                guid.GetString().c_str(), field, typeStr, iValue, value);
            PSendSysMessage(LANG_CHANGE_HEX_FIELD, guid.GetString().c_str(),
                field, typeStr, iValue, value);
            break;
        }

        target->SetUInt32Value(field, value);
    }
    else
    {
        float fValue;
        if (!ExtractFloat(&valStr, fValue))
            return false;

        float value = target->GetFloatValue(field);

        value += fValue;

        LOG_DEBUG(logging, GetMangosString(LANG_CHANGE_FLOAT),
            guid.GetString().c_str(), field, fValue, value);
        PSendSysMessage(LANG_CHANGE_FLOAT_FIELD, guid.GetString().c_str(),
            field, fValue, value);

        target->SetFloatValue(field, value);
    }

    return true;
}

bool ChatHandler::HandleDebugModItemValueCommand(char* args)
{
    uint32 guid;
    if (!ExtractUInt32(&args, guid))
        return false;

    uint32 field;
    if (!ExtractUInt32(&args, field))
        return false;

    char* typeStr = ExtractLiteralArg(&args);
    if (!typeStr)
        return false;

    char* valStr = ExtractLiteralArg(&args);
    if (!valStr)
        return false;

    Item* item =
        m_session->GetPlayer()->GetItemByGuid(ObjectGuid(HIGHGUID_ITEM, guid));
    if (!item)
        return false;

    return HandlerDebugModValueHelper(item, field, typeStr, valStr);
}

bool ChatHandler::HandleDebugModValueCommand(char* args)
{
    Unit* target = getSelectedUnit();
    if (!target)
    {
        SendSysMessage(LANG_SELECT_CHAR_OR_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    uint32 field;
    if (!ExtractUInt32(&args, field))
        return false;

    char* typeStr = ExtractLiteralArg(&args);
    if (!typeStr && *args) // optional arg but check format fail case
        return false;

    char* valStr = ExtractLiteralArg(&args);
    if (!valStr)
        return false;

    return HandlerDebugModValueHelper(target, field, typeStr, valStr);
}

bool ChatHandler::HandleDebugSpellCoefsCommand(char* args)
{
    uint32 spellid = ExtractSpellIdFromLink(&args);
    if (!spellid)
        return false;

    SpellEntry const* spellEntry = sSpellStore.LookupEntry(spellid);
    if (!spellEntry)
        return false;

    SpellBonusEntry const* bonus =
        sSpellMgr::Instance()->GetSpellBonusData(spellid);

    float direct_calc =
        CalculateDefaultCoefficient(spellEntry, SPELL_DIRECT_DAMAGE);
    float dot_calc = CalculateDefaultCoefficient(spellEntry, DOT);

    bool isDirectHeal = false;
    for (int i = 0; i < 3; ++i)
    {
        // Heals (Also count Mana Shield and Absorb effects as heals)
        if (spellEntry->Effect[i] == SPELL_EFFECT_HEAL ||
            spellEntry->Effect[i] == SPELL_EFFECT_HEAL_MAX_HEALTH ||
            (spellEntry->Effect[i] == SPELL_EFFECT_APPLY_AURA &&
                (spellEntry->EffectApplyAuraName[i] ==
                        SPELL_AURA_SCHOOL_ABSORB ||
                    spellEntry->EffectApplyAuraName[i] ==
                        SPELL_AURA_PERIODIC_HEAL)))
        {
            isDirectHeal = true;
            break;
        }
    }

    bool isDotHeal = false;
    for (int i = 0; i < 3; ++i)
    {
        // Periodic Heals
        if (spellEntry->Effect[i] == SPELL_EFFECT_APPLY_AURA &&
            spellEntry->EffectApplyAuraName[i] == SPELL_AURA_PERIODIC_HEAL)
        {
            isDotHeal = true;
            break;
        }
    }

    char const* directHealStr = GetMangosString(LANG_DIRECT_HEAL);
    char const* directDamageStr = GetMangosString(LANG_DIRECT_DAMAGE);
    char const* dotHealStr = GetMangosString(LANG_DOT_HEAL);
    char const* dotDamageStr = GetMangosString(LANG_DOT_DAMAGE);

    PSendSysMessage(LANG_SPELLCOEFS, spellid,
        isDirectHeal ? directHealStr : directDamageStr, direct_calc,
        direct_calc * 1.88f, bonus ? bonus->direct_damage : 0.0f,
        bonus ? bonus->ap_bonus : 0.0f);
    PSendSysMessage(LANG_SPELLCOEFS, spellid,
        isDotHeal ? dotHealStr : dotDamageStr, dot_calc, dot_calc * 1.88f,
        bonus ? bonus->dot_damage : 0.0f, bonus ? bonus->ap_dot_bonus : 0.0f);

    return true;
}

bool ChatHandler::HandleDebugSpellModsCommand(char* args)
{
    char* typeStr = ExtractLiteralArg(&args);
    if (!typeStr)
        return false;

    uint16 opcode;
    if (strncmp(typeStr, "flat", strlen(typeStr)) == 0)
        opcode = SMSG_SET_FLAT_SPELL_MODIFIER;
    else if (strncmp(typeStr, "pct", strlen(typeStr)) == 0)
        opcode = SMSG_SET_PCT_SPELL_MODIFIER;
    else
        return false;

    uint32 effidx;
    if (!ExtractUInt32(&args, effidx) || effidx >= 64)
        return false;

    uint32 spellmodop;
    if (!ExtractUInt32(&args, spellmodop) || spellmodop >= MAX_SPELLMOD)
        return false;

    int32 value;
    if (!ExtractInt32(&args, value))
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

    PSendSysMessage(LANG_YOU_CHANGE_SPELLMODS,
        opcode == SMSG_SET_FLAT_SPELL_MODIFIER ? "flat" : "pct", spellmodop,
        value, effidx, GetNameLink(chr).c_str());
    if (needReportToTarget(chr))
        ChatHandler(chr).PSendSysMessage(LANG_YOURS_SPELLMODS_CHANGED,
            GetNameLink().c_str(),
            opcode == SMSG_SET_FLAT_SPELL_MODIFIER ? "flat" : "pct", spellmodop,
            value, effidx);

    WorldPacket data(opcode, (1 + 1 + 2 + 2));
    data << uint8(effidx);
    data << uint8(spellmodop);
    data << int32(value);
    chr->GetSession()->send_packet(std::move(data));

    return true;
}

bool ChatHandler::HandleDebugThreadedMaps(char* args)
{
    if (!sMapMgr::Instance()->get_threaded_maps())
    {
        SendSysMessage("Threaded maps not enabled.");
        return true;
    }

    char* opt = ExtractQuotedOrLiteralArg(&args);
    if (opt)
    {
        if (strcmp(opt, "-r") != 0 && strcmp(opt, "--reset") != 0)
        {
            SendSysMessage(
                "Unrecognized option, valid options are: -r --reset");
        }
        else
        {
            sMapMgr::Instance()->get_threaded_maps()->reset_performance_log();
            SendSysMessage("Performance log was successfully reset.");
        }
        return true;
    }

    std::string str =
        sMapMgr::Instance()->get_threaded_maps()->performance_log();
    PSendSysMessage("%s", str.c_str());
    return true;
}

bool ChatHandler::HandleDebugThreatlist(char* args)
{
    Unit* unit = getSelectedUnit();
    if (!unit)
    {
        SendSysMessage("No selected unit.");
        return true;
    }

    if (!unit->CanHaveThreatList())
    {
        SendSysMessage("Unit cannot have threat-list.");
        return true;
    }

    // Optional arguments:
    // -n NUM:    how many entries are printed
    // -o OFFSET: what threat offset to start at
    int n = -1, o = 0;
    char* opt;
    while ((opt = ExtractLiteralArg(&args)) != nullptr)
    {
        if (strcmp(opt, "-n") == 0)
        {
            if (!ExtractInt32(&args, n))
            {
                SendSysMessage("Usage:");
                SendSysMessage(".debug threatlist [-n NUM] [-o OFFSET]");
                return true;
            }
        }
        else if (strcmp(opt, "-o") == 0)
        {
            if (!ExtractInt32(&args, o))
            {
                SendSysMessage("Usage:");
                SendSysMessage(".debug threatlist [-n NUM] [-o OFFSET]");
                return true;
            }
        }
        else
        {
            SendSysMessage("Usage:");
            SendSysMessage(".debug threatlist [-n NUM] [-o OFFSET]");
            return true;
        }
    }

    PSendSysMessage("Threatlist of %s:", unit->GetName());
    auto& tl = unit->getThreatManager().getThreatList();
    float victim_threat = 0.0f;
    if (auto v = unit->getVictim())
        victim_threat = unit->getThreatManager().getThreat(v);
    int i = 0, c = 0;
    for (auto ref : tl)
    {
        ++i;
        if (o > 0 && (i - 1) < o)
            continue;
        if (n > -1 && c >= n)
            continue;

        int threat_pct = 0;
        float threat = ref->getThreat();
        if (victim_threat > 0)
            threat_pct = 100.0f * threat / victim_threat;

        ++c;
        if (ref->getUntauntableThreat() > 0)
            PSendSysMessage("%s: %.1f (%.1f of that is not tauntable) (%d%%)",
                ref->getTarget()->GetName(), ref->getThreat(),
                ref->getUntauntableThreat(), threat_pct);
        else
            PSendSysMessage("%s: %.1f (%d%%)", ref->getTarget()->GetName(),
                ref->getThreat(), threat_pct);
    }

    return true;
}

bool ChatHandler::HandleDebugWmolistCommand(char* args)
{
    float dist;
    if (!ExtractFloat(&args, dist) || dist <= 0.1f || dist >= 1000.0f)
        return false;

    Player* p = m_session->GetPlayer();

    float x1, y1, z1, x2, y2, z2;
    p->GetPosition(x1, y1, z1);
    p->GetPosition(x2, y2, z2);
    z1 = z2 = z1 + 2;

    x2 = x2 + dist * cos(p->GetO());
    y2 = y2 + dist * sin(p->GetO());

    auto vmgr = VMAP::VMapFactory::createOrGetVMapManager();

    bool spell_los = false, los = false;
    spell_los = vmgr->isInWmoLineOfSight(p->GetMapId(), x1, y1, z1, x2, y2, z2);
    los = vmgr->isInM2LineOfSight(p->GetMapId(), x1, y1, z1, x2, y2, z2) &&
          vmgr->isInWmoLineOfSight(p->GetMapId(), x1, y1, z1, x2, y2, z2);

    PSendSysMessage("LoS: %s, Spell LoS: %s", los ? "yes" : "BLOCKED",
        spell_los ? "yes" : "BLOCKED");

    // Send list of WMOs our ray pass through
    auto v = vmgr->getModelNames(p->GetMapId(), x1, y1, z1, x2, y2, z2);
    std::string s("Models:\n");
    for (auto e : v)
        s += e + "\n";
    PSendSysMessage("%s", s.c_str());

    return true;
}

bool ChatHandler::HandleDebugLoSCommand(char* /*args*/)
{
    Player* p = m_session->GetPlayer();

    if (Unit* unit = getSelectedUnit())
    {
        std::string ms1, ms2, ms3;
        bool los, spell_los, go_los;

        {
            scope_performance_timer t("", &ms3);
            go_los = p->GetMap()->isInDynLineOfSight(p->GetX(), p->GetY(),
                p->GetZ() + 2.0f, unit->GetX(), unit->GetY(),
                unit->GetZ() + 2.0f);
        }

        PSendSysMessage("LoS checks for %s:", unit->GetGuidStr().c_str());

        {
            scope_performance_timer t("", &ms1);
            los = p->IsWithinLOSInMap(unit);
        }
        PSendSysMessage(
            "M2 & WMO LoS: %s (%s)", los ? "yes" : "BLOCKED", ms1.c_str());

        {
            scope_performance_timer t("", &ms2);
            spell_los = p->IsWithinWmoLOSInMap(unit);
        }
        PSendSysMessage(
            "WMO LoS: %s (%s)", spell_los ? "yes" : "BLOCKED", ms2.c_str());

        PSendSysMessage(
            "GO LoS: %s (%s -- this cost was part of the previous 2 LoS checks "
            "as well!)",
            go_los ? "yes" : "BLOCKED", ms3.c_str());
    }
    return true;
}

bool ChatHandler::HandleDebugBattlefieldQueueCommand(char* args)
{
    int opt = 0;
    ExtractOptInt32(&args, opt, 0);
    std::string debug;

    if (opt == 0)
        debug = sBattlefieldQueue::Instance()->summary_debug();
    else if (opt == 1)
        debug = sBattlefieldQueue::Instance()->debug(false);
    else if (opt == 2)
        debug = sBattlefieldQueue::Instance()->debug(true);
    else
        debug = "Invalid option, append 0, 1 or 2.";

    PSendSysMessage("%s", debug.c_str());
    return true;
}

bool ChatHandler::HandleDebugWardenCommand(char* args)
{
    bool on;
    if (!ExtractOnOff(&args, on))
    {
        PSendSysMessage("Debugging of Warden is: %s.",
            sWorld::Instance()->debugging_warden() ? "ON" : "OFF");
        SendSysMessage("Command usage: .debug warden on/off.");
        return true;
    }
    sWorld::Instance()->set_warden_debug(on);
    PSendSysMessage("Debugging of Warden turned: %s.",
        sWorld::Instance()->debugging_warden() ? "ON" : "OFF");
    return true;
}
