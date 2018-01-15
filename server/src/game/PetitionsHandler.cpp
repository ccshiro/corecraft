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

#include "ArenaTeam.h"
#include "Common.h"
#include "GossipDef.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "Language.h"
#include "logging.h"
#include "ObjectMgr.h"
#include "Opcodes.h"
#include "SocialMgr.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"

/*enum PetitionType // dbc data
{
    PETITION_TYPE_GUILD      = 1,
    PETITION_TYPE_ARENA_TEAM = 3
};*/

// Charters ID in item_template
#define GUILD_CHARTER 5863
#define GUILD_CHARTER_COST 1000 // 10 S
#define ARENA_TEAM_CHARTER_2v2 23560
#define ARENA_TEAM_CHARTER_2v2_COST 800000 // 80 G
#define ARENA_TEAM_CHARTER_3v3 23561
#define ARENA_TEAM_CHARTER_3v3_COST 1200000 // 120 G
#define ARENA_TEAM_CHARTER_5v5 23562
#define ARENA_TEAM_CHARTER_5v5_COST 2000000 // 200 G
#define CHARTER_DISPLAY_ID 16161

void WorldSession::HandlePetitionBuyOpcode(WorldPacket& recv_data)
{
    ObjectGuid guidNPC;
    uint32 clientIndex; // 1 for guild and arenaslot+1 for arenas in client
    std::string name;

    recv_data >> guidNPC;          // NPC GUID
    recv_data.read_skip<uint32>(); // 0
    recv_data.read_skip<uint64>(); // 0
    recv_data >> name;             // name
    recv_data.read_skip<uint32>(); // 0
    recv_data.read_skip<uint32>(); // 0
    recv_data.read_skip<uint32>(); // 0
    recv_data.read_skip<uint32>(); // 0
    recv_data.read_skip<uint32>(); // 0
    recv_data.read_skip<uint32>(); // 0
    recv_data.read_skip<uint32>(); // 0
    recv_data.read_skip<uint32>(); // 0
    recv_data.read_skip<uint32>(); // 0
    recv_data.read_skip<uint32>(); // 0
    recv_data.read_skip<uint16>(); // 0
    recv_data.read_skip<uint8>();  // 0

    recv_data >> clientIndex;      // index
    recv_data.read_skip<uint32>(); // 0

    LOG_DEBUG(logging, "Petitioner %s tried sell petition: name %s",
        guidNPC.GetString().c_str(), name.c_str());

    // prevent cheating
    Creature* pCreature =
        GetPlayer()->GetNPCIfCanInteractWith(guidNPC, UNIT_NPC_FLAG_PETITIONER);
    if (!pCreature)
    {
        LOG_DEBUG(logging,
            "WORLD: HandlePetitionBuyOpcode - %s not found or you can't "
            "interact with him.",
            guidNPC.GetString().c_str());
        return;
    }

    // remove fake death
    if (GetPlayer()->hasUnitState(UNIT_STAT_DIED))
        GetPlayer()->remove_auras(SPELL_AURA_FEIGN_DEATH);

    uint32 charterid = 0;
    uint32 cost = 0;
    uint32 type = 0;
    if (pCreature->isTabardDesigner())
    {
        // if tabard designer, then trying to buy a guild charter.
        // do not let if already in guild.
        if (_player->GetGuildId())
            return;

        charterid = GUILD_CHARTER;
        cost = GUILD_CHARTER_COST;
        type = 9;
    }
    else
    {
        // TODO: find correct opcode
        if (_player->getLevel() <
            sWorld::Instance()->getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL))
        {
            SendNotification(LANG_ARENA_ONE_TOOLOW,
                sWorld::Instance()->getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL));
            return;
        }

        switch (
            clientIndex) // arenaSlot+1 as received from client (1 from 3 case)
        {
        case 1:
            charterid = ARENA_TEAM_CHARTER_2v2;
            cost = ARENA_TEAM_CHARTER_2v2_COST;
            type = 2; // 2v2
            break;
        case 2:
            charterid = ARENA_TEAM_CHARTER_3v3;
            cost = ARENA_TEAM_CHARTER_3v3_COST;
            type = 3; // 3v3
            break;
        case 3:
            charterid = ARENA_TEAM_CHARTER_5v5;
            cost = ARENA_TEAM_CHARTER_5v5_COST;
            type = 5; // 5v5
            break;
        default:
            LOG_DEBUG(logging, "unknown selection at buy arena petition: %u",
                clientIndex);
            return;
        }

        if (_player->GetArenaTeamId(
                clientIndex - 1)) // arenaSlot+1 as received from client
        {
            SendArenaTeamCommandResult(
                ERR_ARENA_TEAM_CREATE_S, name, "", ERR_ALREADY_IN_ARENA_TEAM);
            return;
        }
    }

    if (type == 9)
    {
        if (sGuildMgr::Instance()->GetGuildByName(name))
        {
            SendGuildCommandResult(
                GUILD_CREATE_S, name, ERR_GUILD_NAME_EXISTS_S);
            return;
        }
        if (sObjectMgr::Instance()->IsReservedName(name) ||
            !ObjectMgr::IsValidCharterName(name))
        {
            SendGuildCommandResult(
                GUILD_CREATE_S, name, ERR_GUILD_NAME_INVALID);
            return;
        }
    }
    else
    {
        if (sObjectMgr::Instance()->GetArenaTeamByName(name))
        {
            SendArenaTeamCommandResult(ERR_ARENA_TEAM_CREATE_S, name, "",
                ERR_ARENA_TEAM_NAME_EXISTS_S);
            return;
        }
        if (sObjectMgr::Instance()->IsReservedName(name) ||
            !ObjectMgr::IsValidCharterName(name))
        {
            SendArenaTeamCommandResult(
                ERR_ARENA_TEAM_CREATE_S, name, "", ERR_ARENA_TEAM_NAME_INVALID);
            return;
        }
    }

    ItemPrototype const* pProto = ObjectMgr::GetItemPrototype(charterid);
    if (!pProto)
    {
        _player->SendBuyError(BUY_ERR_CANT_FIND_ITEM, nullptr, charterid, 0);
        return;
    }

    // The code below (add_items()) do not support stackable items, but no
    // charters are able to stack
    if (pProto->Stackable > 1)
    {
        logging.error(
            "HandlePeitionBuyOpcode: Tried to buy a charter that stacks. "
            "Should not be possible. Please consult (file: %s line: %d) if "
            "this assumption has changed.",
            __FILE__, __LINE__);
        _player->SendBuyError(BUY_ERR_CANT_FIND_ITEM, nullptr, charterid, 0);
        return;
    }

    if (_player->storage().money().get() < cost)
    { // player hasn't got enough money
        _player->SendBuyError(
            BUY_ERR_NOT_ENOUGHT_MONEY, pCreature, charterid, 0);
        return;
    }

    // XXX
    inventory::transaction trans;
    trans.add(charterid, pProto->BuyCount);
    trans.remove(inventory::copper(cost));

    if (!_player->storage().finalize(trans))
    {
        _player->SendEquipError(static_cast<InventoryResult>(trans.error()),
            nullptr, nullptr, charterid);
        return;
    }

    Item* charter = trans.added_items()[0];
    charter->SetUInt32Value(ITEM_FIELD_ENCHANTMENT_1_1, charter->GetGUIDLow());
    // ITEM_FIELD_ENCHANTMENT_1_1 is guild/arenateam id
    // ITEM_FIELD_ENCHANTMENT_1_1+1 is current signatures count (showed on item)
    charter->mark_for_save();

    // a petition is invalid, if both the owner and the type matches
    // we checked above, if this player is in an arenateam, so this must be data
    // corruption
    std::unique_ptr<QueryResult> result(CharacterDatabase.PQuery(
        "SELECT petitionguid FROM petition WHERE ownerguid = '%u'  AND type = "
        "'%u'",
        _player->GetGUIDLow(), type));

    std::ostringstream ssInvalidPetitionGUIDs;

    if (result)
    {
        do
        {
            Field* fields = result->Fetch();
            ssInvalidPetitionGUIDs << "'" << fields[0].GetUInt32() << "' , ";
        } while (result->NextRow());
    }

    // delete petitions with the same guid as this one
    ssInvalidPetitionGUIDs << "'" << charter->GetGUIDLow() << "'";

    LOG_DEBUG(logging, "Invalid petition GUIDs: %s",
        ssInvalidPetitionGUIDs.str().c_str());
    CharacterDatabase.escape_string(name);
    CharacterDatabase.BeginTransaction();
    CharacterDatabase.PExecute(
        "DELETE FROM petition WHERE petitionguid IN ( %s )",
        ssInvalidPetitionGUIDs.str().c_str());
    CharacterDatabase.PExecute(
        "DELETE FROM petition_sign WHERE petitionguid IN ( %s )",
        ssInvalidPetitionGUIDs.str().c_str());
    CharacterDatabase.PExecute(
        "INSERT INTO petition (ownerguid, petitionguid, name, type) VALUES "
        "('%u', '%u', '%s', '%u')",
        _player->GetGUIDLow(), charter->GetGUIDLow(), name.c_str(), type);
    CharacterDatabase.CommitTransaction();
}

void WorldSession::HandlePetitionShowSignOpcode(WorldPacket& recv_data)
{
    uint8 signs = 0;
    ObjectGuid petitionguid;
    recv_data >> petitionguid; // petition guid

    // solve (possible) some strange compile problems with explicit use
    // GUID_LOPART(petitionguid) at some GCC versions (wrong code optimization
    // in compiler?)
    uint32 petitionguid_low = petitionguid.GetCounter();

    std::unique_ptr<QueryResult> result(CharacterDatabase.PQuery(
        "SELECT type FROM petition WHERE petitionguid = '%u'",
        petitionguid_low));
    if (!result)
    {
        logging.error("any petition on server...");
        return;
    }
    Field* fields = result->Fetch();
    uint32 type = fields[0].GetUInt32();

    // if guild petition and has guild => error, return;
    if (type == 9 && _player->GetGuildId())
        return;

    result.reset(CharacterDatabase.PQuery(
        "SELECT playerguid FROM petition_sign WHERE petitionguid = '%u'",
        petitionguid_low));

    // result==NULL also correct in case no sign yet
    if (result)
        signs = (uint8)result->GetRowCount();

    WorldPacket data(
        SMSG_PETITION_SHOW_SIGNATURES, (8 + 8 + 4 + 1 + signs * 12));
    data << ObjectGuid(petitionguid); // petition guid
    data << _player->GetObjectGuid(); // owner guid
    data << uint32(petitionguid_low); // guild guid (in mangos always same as
                                      // GUID_LOPART(petitionguid)
    data << uint8(signs);             // sign's count

    for (uint8 i = 1; i <= signs; ++i)
    {
        Field* fields2 = result->Fetch();
        ObjectGuid signerGuid =
            ObjectGuid(HIGHGUID_PLAYER, fields2[0].GetUInt32());

        data << ObjectGuid(signerGuid); // Player GUID
        data << uint32(0);              // there 0 ...

        result->NextRow();
    }
    send_packet(std::move(data));
}

void WorldSession::HandlePetitionQueryOpcode(WorldPacket& recv_data)
{
    uint32 guildguid;
    ObjectGuid petitionguid;
    recv_data >>
        guildguid; // in mangos always same as GUID_LOPART(petitionguid)
    recv_data >> petitionguid; // petition guid
    SendPetitionQueryOpcode(petitionguid);
}

void WorldSession::SendPetitionQueryOpcode(ObjectGuid petitionguid)
{
    uint32 petitionLowGuid = petitionguid.GetCounter();

    ObjectGuid ownerGuid;
    uint32 type;
    std::string name = "NO_NAME_FOR_GUID";
    /*uint8 signs = 0;*/

    std::unique_ptr<QueryResult> result(CharacterDatabase.PQuery(
        "SELECT ownerguid, name, "
        /*"  (SELECT COUNT(playerguid) FROM petition_sign WHERE
           petition_sign.petitionguid = '%u') AS signs, "*/
        "  type "
        "FROM petition WHERE petitionguid = '%u'",
        /*petitionLowGuid,*/ petitionLowGuid));

    if (result)
    {
        Field* fields = result->Fetch();
        ownerGuid = ObjectGuid(HIGHGUID_PLAYER, fields[0].GetUInt32());
        name = fields[1].GetCppString();
        type = fields[2].GetUInt32();
        /*signs     = fields[2].GetUInt8();
        type      = fields[3].GetUInt32();*/
    }
    else
    {
        LOG_DEBUG(logging, "CMSG_PETITION_QUERY failed for petition (GUID: %u)",
            petitionLowGuid);
        return;
    }

    WorldPacket data(SMSG_PETITION_QUERY_RESPONSE,
        (4 + 8 + name.size() + 1 + 1 + 4 * 13 + 2));
    data << uint32(petitionLowGuid); // guild/team guid (in mangos always same
                                     // as GUID_LOPART(petition guid)
    data << ObjectGuid(ownerGuid);   // charter owner guid
    data << name;                    // name (guild/arena team)
    data << uint8(0);                // 1
    if (type == 9)
    {
        data << uint32(9);
        data << uint32(9);
        data << uint32(0); // bypass client - side limitation, a different value
                           // is needed here for each petition
    }
    else
    {
        data << uint32(type - 1);
        data << uint32(type - 1);
        data << uint32(type); // bypass client - side limitation, a different
                              // value is needed here for each petition
    }
    data << uint32(0); // 5
    data << uint32(0); // 6
    data << uint32(0); // 7
    data << uint32(0); // 8
    data << uint16(0); // 9 2 bytes field
    data << uint32(0); // 10
    data << uint32(0); // 11
    data << uint32(0); // 13 count of next strings?
    data << uint32(0); // 14

    if (type == 9)
        data << uint32(0); // 15 0 - guild, 1 - arena team
    else
        data << uint32(1);

    send_packet(std::move(data));
}

void WorldSession::HandlePetitionRenameOpcode(WorldPacket& recv_data)
{
    ObjectGuid petitionGuid;
    uint32 type;
    std::string newname;

    recv_data >> petitionGuid; // guid
    recv_data >> newname;      // new name

    Item* item = _player->GetItemByGuid(petitionGuid);
    if (!item)
        return;

    std::unique_ptr<QueryResult> result(CharacterDatabase.PQuery(
        "SELECT type FROM petition WHERE petitionguid = '%u'",
        petitionGuid.GetCounter()));

    if (result)
    {
        Field* fields = result->Fetch();
        type = fields[0].GetUInt32();
    }
    else
    {
        LOG_DEBUG(logging, "CMSG_PETITION_QUERY failed for petition: %s",
            petitionGuid.GetString().c_str());
        return;
    }

    if (type == 9)
    {
        if (sGuildMgr::Instance()->GetGuildByName(newname))
        {
            SendGuildCommandResult(
                GUILD_CREATE_S, newname, ERR_GUILD_NAME_EXISTS_S);
            return;
        }
        if (sObjectMgr::Instance()->IsReservedName(newname) ||
            !ObjectMgr::IsValidCharterName(newname))
        {
            SendGuildCommandResult(
                GUILD_CREATE_S, newname, ERR_GUILD_NAME_INVALID);
            return;
        }
    }
    else
    {
        if (sObjectMgr::Instance()->GetArenaTeamByName(newname))
        {
            SendArenaTeamCommandResult(ERR_ARENA_TEAM_CREATE_S, newname, "",
                ERR_ARENA_TEAM_NAME_EXISTS_S);
            return;
        }
        if (sObjectMgr::Instance()->IsReservedName(newname) ||
            !ObjectMgr::IsValidCharterName(newname))
        {
            SendArenaTeamCommandResult(ERR_ARENA_TEAM_CREATE_S, newname, "",
                ERR_ARENA_TEAM_NAME_INVALID);
            return;
        }
    }

    std::string db_newname = newname;
    CharacterDatabase.escape_string(db_newname);
    CharacterDatabase.PExecute(
        "UPDATE petition SET name = '%s' WHERE petitionguid = '%u'",
        db_newname.c_str(), petitionGuid.GetCounter());

    LOG_DEBUG(logging, "Petition %s renamed to '%s'",
        petitionGuid.GetString().c_str(), newname.c_str());

    WorldPacket data(MSG_PETITION_RENAME, (8 + newname.size() + 1));
    data << ObjectGuid(petitionGuid);
    data << newname;
    send_packet(std::move(data));
}

void WorldSession::HandlePetitionSignOpcode(WorldPacket& recv_data)
{
    Field* fields;
    ObjectGuid petitionGuid;
    uint8 unk;
    recv_data >> petitionGuid; // petition guid
    recv_data >> unk;

    uint32 petitionLowGuid = petitionGuid.GetCounter();

    std::unique_ptr<QueryResult> result(CharacterDatabase.PQuery(
        "SELECT ownerguid, "
        "  (SELECT COUNT(playerguid) FROM petition_sign WHERE "
        "petition_sign.petitionguid = '%u') AS signs, "
        "  type "
        "FROM petition WHERE petitionguid = '%u'",
        petitionLowGuid, petitionLowGuid));

    if (!result)
    {
        logging.error("any petition on server...");
        return;
    }

    fields = result->Fetch();
    uint32 ownerLowGuid = fields[0].GetUInt32();
    ObjectGuid ownerGuid = ObjectGuid(HIGHGUID_PLAYER, ownerLowGuid);
    uint8 signs = fields[1].GetUInt8();
    uint32 type = fields[2].GetUInt32();

    if (ownerGuid == _player->GetObjectGuid())
        return;

    // not let enemies sign guild charter
    if (!sWorld::Instance()->getConfig(
            CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_GUILD) &&
        GetPlayer()->GetTeam() !=
            sObjectMgr::Instance()->GetPlayerTeamByGUID(ownerGuid))
    {
        if (type != 9)
            SendArenaTeamCommandResult(
                ERR_ARENA_TEAM_INVITE_SS, "", "", ERR_ARENA_TEAM_NOT_ALLIED);
        else
            SendGuildCommandResult(GUILD_CREATE_S, "", ERR_GUILD_NOT_ALLIED);
        return;
    }

    if (type != 9)
    {
        if (_player->getLevel() <
            sWorld::Instance()->getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL))
        {
            SendArenaTeamCommandResult(ERR_ARENA_TEAM_CREATE_S, "",
                _player->GetName(), ERR_ARENA_TEAM_TARGET_TOO_LOW_S);
            return;
        }

        if (!IsArenaTypeValid(ArenaType(type)))
            return;

        uint8 slot = ArenaTeam::GetSlotByType(ArenaType(type));
        if (slot >= MAX_ARENA_SLOT)
            return;

        if (_player->GetArenaTeamId(slot))
        {
            SendArenaTeamCommandResult(ERR_ARENA_TEAM_INVITE_SS, "",
                _player->GetName(), ERR_ALREADY_IN_ARENA_TEAM_S);
            return;
        }

        if (_player->GetArenaTeamIdInvited())
        {
            SendArenaTeamCommandResult(ERR_ARENA_TEAM_INVITE_SS, "",
                _player->GetName(), ERR_ALREADY_INVITED_TO_ARENA_TEAM_S);
            return;
        }
    }
    else
    {
        if (_player->GetGuildId())
        {
            SendGuildCommandResult(
                GUILD_INVITE_S, _player->GetName(), ERR_ALREADY_IN_GUILD_S);
            return;
        }
        if (_player->GetGuildIdInvited())
        {
            SendGuildCommandResult(GUILD_INVITE_S, _player->GetName(),
                ERR_ALREADY_INVITED_TO_GUILD_S);
            return;
        }
    }

    if (++signs > type) // client signs maximum
        return;

    // client doesn't allow to sign petition two times by one character, but not
    // check sign by another character from same account
    // not allow sign another player from already sign player account
    result.reset(CharacterDatabase.PQuery(
        "SELECT playerguid FROM petition_sign WHERE player_account = '%u' AND "
        "petitionguid = '%u'",
        GetAccountId(), petitionLowGuid));

    if (result)
    {
        WorldPacket data(SMSG_PETITION_SIGN_RESULTS, (8 + 8 + 4));
        data << ObjectGuid(petitionGuid);
        data << ObjectGuid(_player->GetObjectGuid());
        data << uint32(PETITION_SIGN_ALREADY_SIGNED);

        // close at signer side
        send_packet(&data);

        // update for owner if online
        if (Player* owner = sObjectMgr::Instance()->GetPlayer(ownerGuid))
            owner->GetSession()->send_packet(&data);
        return;
    }

    CharacterDatabase.PExecute(
        "INSERT INTO petition_sign (ownerguid,petitionguid, playerguid, "
        "player_account) VALUES ('%u', '%u', '%u','%u')",
        ownerLowGuid, petitionLowGuid, _player->GetGUIDLow(), GetAccountId());

    LOG_DEBUG(logging, "PETITION SIGN: %s by %s",
        petitionGuid.GetString().c_str(), _player->GetGuidStr().c_str());

    WorldPacket data(SMSG_PETITION_SIGN_RESULTS, (8 + 8 + 4));
    data << ObjectGuid(petitionGuid);
    data << ObjectGuid(_player->GetObjectGuid());
    data << uint32(PETITION_SIGN_OK);

    // close at signer side
    send_packet(&data);

    // update signs count on charter, required testing...
    // Item *item = _player->GetItemByGuid(petitionguid));
    // if(item)
    //    item->SetUInt32Value(ITEM_FIELD_ENCHANTMENT_1_1+1, signs);

    // update for owner if online
    if (Player* owner = sObjectMgr::Instance()->GetPlayer(ownerGuid))
        owner->GetSession()->send_packet(&data);
}

void WorldSession::HandlePetitionDeclineOpcode(WorldPacket& recv_data)
{
    ObjectGuid petitionGuid;
    recv_data >> petitionGuid; // petition guid

    LOG_DEBUG(logging, "Petition %s declined by %s",
        petitionGuid.GetString().c_str(), _player->GetGuidStr().c_str());

    uint32 petitionLowGuid = petitionGuid.GetCounter();

    std::unique_ptr<QueryResult> result(CharacterDatabase.PQuery(
        "SELECT ownerguid FROM petition WHERE petitionguid = '%u'",
        petitionLowGuid));
    if (!result)
        return;

    Field* fields = result->Fetch();
    ObjectGuid ownerguid = ObjectGuid(HIGHGUID_PLAYER, fields[0].GetUInt32());

    Player* owner = sObjectMgr::Instance()->GetPlayer(ownerguid);
    if (owner) // petition owner online
    {
        WorldPacket data(MSG_PETITION_DECLINE, 8);
        data << _player->GetObjectGuid();
        owner->GetSession()->send_packet(std::move(data));
    }
}

void WorldSession::HandleOfferPetitionOpcode(WorldPacket& recv_data)
{
    ObjectGuid petitionGuid;
    ObjectGuid playerGuid;
    uint32 junk;
    recv_data >> junk;         // this is not petition type!
    recv_data >> petitionGuid; // petition guid
    recv_data >> playerGuid;   // player guid

    Player* player = ObjectAccessor::FindPlayer(playerGuid);
    if (!player)
        return;

    /// Get petition type and check
    std::unique_ptr<QueryResult> result(CharacterDatabase.PQuery(
        "SELECT type FROM petition WHERE petitionguid = '%u'",
        petitionGuid.GetCounter()));
    if (!result)
        return;

    Field* fields = result->Fetch();
    uint32 type = fields[0].GetUInt32();

    LOG_DEBUG(logging, "OFFER PETITION: type %u petition %s to %s", type,
        petitionGuid.GetString().c_str(), playerGuid.GetString().c_str());

    if (!sWorld::Instance()->getConfig(
            CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_GUILD) &&
        GetPlayer()->GetTeam() != player->GetTeam())
    {
        if (type != 9)
            SendArenaTeamCommandResult(
                ERR_ARENA_TEAM_INVITE_SS, "", "", ERR_ARENA_TEAM_NOT_ALLIED);
        else
            SendGuildCommandResult(GUILD_CREATE_S, "", ERR_GUILD_NOT_ALLIED);
        return;
    }

    if (type != 9)
    {
        if (player->getLevel() <
            sWorld::Instance()->getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL))
        {
            // player is too low level to join an arena team
            SendArenaTeamCommandResult(ERR_ARENA_TEAM_CREATE_S, "",
                player->GetName(), ERR_ARENA_TEAM_TARGET_TOO_LOW_S);
            return;
        }

        if (!IsArenaTypeValid(ArenaType(type)))
            return;

        uint8 slot = ArenaTeam::GetSlotByType(ArenaType(type));
        if (slot >= MAX_ARENA_SLOT)
            return;

        if (player->GetArenaTeamId(slot))
        {
            // player is already in an arena team
            SendArenaTeamCommandResult(ERR_ARENA_TEAM_CREATE_S, "",
                player->GetName(), ERR_ALREADY_IN_ARENA_TEAM_S);
            return;
        }

        if (player->GetArenaTeamIdInvited())
        {
            SendArenaTeamCommandResult(ERR_ARENA_TEAM_INVITE_SS, "",
                _player->GetName(), ERR_ALREADY_INVITED_TO_ARENA_TEAM_S);
            return;
        }
    }
    else
    {
        if (player->GetGuildId())
        {
            SendGuildCommandResult(
                GUILD_INVITE_S, _player->GetName(), ERR_ALREADY_IN_GUILD_S);
            return;
        }

        if (player->GetGuildIdInvited())
        {
            SendGuildCommandResult(GUILD_INVITE_S, _player->GetName(),
                ERR_ALREADY_INVITED_TO_GUILD_S);
            return;
        }
    }

    /// Get petition signs count
    uint8 signs = 0;
    result.reset(CharacterDatabase.PQuery(
        "SELECT playerguid FROM petition_sign WHERE petitionguid = '%u'",
        petitionGuid.GetCounter()));
    // result==NULL also correct charter without signs
    if (result)
        signs = (uint8)result->GetRowCount();

    /// Send response
    WorldPacket data(
        SMSG_PETITION_SHOW_SIGNATURES, (8 + 8 + 4 + signs + signs * 12));
    data << ObjectGuid(petitionGuid);             // petition guid
    data << ObjectGuid(_player->GetObjectGuid()); // owner guid
    data << uint32(petitionGuid.GetCounter()); // guild guid (in mangos always
                                               // same as low part of petition
                                               // guid)
    data << uint8(signs);                      // sign's count

    for (uint8 i = 1; i <= signs; ++i)
    {
        Field* fields2 = result->Fetch();
        ObjectGuid signerGuid =
            ObjectGuid(HIGHGUID_PLAYER, fields2[0].GetUInt32());

        data << ObjectGuid(signerGuid); // Player GUID
        data << uint32(0);              // there 0 ...

        result->NextRow();
    }

    player->GetSession()->send_packet(std::move(data));
}

void WorldSession::HandleTurnInPetitionOpcode(WorldPacket& recv_data)
{
    ObjectGuid petitionGuid;

    recv_data >> petitionGuid;

    LOG_DEBUG(logging, "Petition %s turned in by %s",
        petitionGuid.GetString().c_str(), _player->GetGuidStr().c_str());

    /// Collect petition info data
    ObjectGuid ownerGuid;
    uint32 type;
    std::string name;

    // data
    std::unique_ptr<QueryResult> result(CharacterDatabase.PQuery(
        "SELECT ownerguid, name, type FROM petition WHERE petitionguid = '%u'",
        petitionGuid.GetCounter()));
    if (result)
    {
        Field* fields = result->Fetch();
        ownerGuid = ObjectGuid(HIGHGUID_PLAYER, fields[0].GetUInt32());
        name = fields[1].GetCppString();
        type = fields[2].GetUInt32();
    }
    else
    {
        logging.error(
            "CMSG_TURN_IN_PETITION: petition table not have data for guid %u!",
            petitionGuid.GetCounter());
        return;
    }

    if (type == 9)
    {
        if (_player->GetGuildId())
        {
            WorldPacket data(SMSG_TURN_IN_PETITION_RESULTS, 4);
            data << uint32(PETITION_TURN_ALREADY_IN_GUILD); // already in guild
            _player->GetSession()->send_packet(std::move(data));
            return;
        }
    }
    else
    {
        if (!IsArenaTypeValid(ArenaType(type)))
            return;

        uint8 slot = ArenaTeam::GetSlotByType(ArenaType(type));
        if (slot >= MAX_ARENA_SLOT)
            return;

        if (_player->GetArenaTeamId(slot))
        {
            // data.initialize(SMSG_TURN_IN_PETITION_RESULTS, 4);
            // data << (uint32)PETITION_TURN_ALREADY_IN_GUILD;   // already in
            // guild
            //_player->GetSession()->send_packet(std::move(data));
            SendArenaTeamCommandResult(
                ERR_ARENA_TEAM_CREATE_S, name, "", ERR_ALREADY_IN_ARENA_TEAM);
            return;
        }
    }

    if (_player->GetObjectGuid() != ownerGuid)
        return;

    // signs
    result.reset(CharacterDatabase.PQuery(
        "SELECT playerguid FROM petition_sign WHERE petitionguid = '%u'",
        petitionGuid.GetCounter()));
    uint8 signs = result ? (uint8)result->GetRowCount() : 0;

    uint32 count =
        type == 9 ?
            sWorld::Instance()->getConfig(CONFIG_UINT32_MIN_PETITION_SIGNS) :
            type - 1;
    if (signs < count)
    {
        WorldPacket data(SMSG_TURN_IN_PETITION_RESULTS, 4);
        data << uint32(
            PETITION_TURN_NEED_MORE_SIGNATURES); // need more signatures...
        send_packet(std::move(data));
        return;
    }

    if (type == 9)
    {
        if (sGuildMgr::Instance()->GetGuildByName(name))
        {
            SendGuildCommandResult(
                GUILD_CREATE_S, name, ERR_GUILD_NAME_EXISTS_S);
            return;
        }
    }
    else
    {
        if (sObjectMgr::Instance()->GetArenaTeamByName(name))
        {
            SendArenaTeamCommandResult(ERR_ARENA_TEAM_CREATE_S, name, "",
                ERR_ARENA_TEAM_NAME_EXISTS_S);
            return;
        }
    }

    // and at last charter item check
    Item* item = _player->GetItemByGuid(petitionGuid);
    if (!item)
        return;

    // OK!

    // XXX
    // delete charter item
    inventory::transaction trans;
    trans.remove(item);
    _player->storage().finalize(trans);

    if (type == 9) // create guild
    {
        auto guild = new Guild;
        if (!guild->Create(_player, name))
        {
            delete guild;
            return;
        }

        // register guild and add guildmaster
        sGuildMgr::Instance()->AddGuild(guild);

        SendGuildCommandResult(GUILD_CREATE_S, guild->GetName(), 0);
        SendGuildCommandResult(GUILD_FOUNDER_S, guild->GetName(), 0);

        // add members
        for (uint8 i = 0; i < signs; ++i)
        {
            Field* fields = result->Fetch();

            ObjectGuid signGuid =
                ObjectGuid(HIGHGUID_PLAYER, fields[0].GetUInt32());
            if (!signGuid)
                continue;

            if (auto player = ObjectAccessor::FindPlayer(signGuid, false))
                if (player->GetSession())
                    player->GetSession()->SendGuildCommandResult(
                        GUILD_FOUNDER_S, guild->GetName(), 0);

            guild->AddMember(signGuid, guild->GetLowestRank());
            result->NextRow();
        }
    }
    else // or arena team
    {
        auto at = new ArenaTeam;
        if (!at->Create(_player->GetObjectGuid(), ArenaType(type), name))
        {
            logging.error("PetitionsHandler: arena team create failed.");
            delete at;
            return;
        }

        uint32 icon, iconcolor, border, bordercolor, backgroud;
        recv_data >> backgroud >> icon >> iconcolor >> border >> bordercolor;

        at->SetEmblem(backgroud, icon, iconcolor, border, bordercolor);

        // register team and add captain
        sObjectMgr::Instance()->AddArenaTeam(at);
        LOG_DEBUG(logging, "PetitonsHandler: arena team added to objmrg");

        // add members
        for (uint8 i = 0; i < signs; ++i)
        {
            Field* fields = result->Fetch();
            ObjectGuid memberGUID =
                ObjectGuid(HIGHGUID_PLAYER, fields[0].GetUInt32());
            if (!memberGUID)
                continue;

            LOG_DEBUG(logging, "PetitionsHandler: adding arena member %s",
                memberGUID.GetString().c_str());
            at->AddMember(memberGUID);
            result->NextRow();
        }
    }

    CharacterDatabase.BeginTransaction();
    CharacterDatabase.PExecute("DELETE FROM petition WHERE petitionguid = '%u'",
        petitionGuid.GetCounter());
    CharacterDatabase.PExecute(
        "DELETE FROM petition_sign WHERE petitionguid = '%u'",
        petitionGuid.GetCounter());
    CharacterDatabase.CommitTransaction();

    // created
    LOG_DEBUG(logging, "TURN IN PETITION %s", petitionGuid.GetString().c_str());

    WorldPacket data(SMSG_TURN_IN_PETITION_RESULTS, 4);
    data << uint32(PETITION_TURN_OK);
    send_packet(std::move(data));
}

void WorldSession::HandlePetitionShowListOpcode(WorldPacket& recv_data)
{
    ObjectGuid guid;
    recv_data >> guid;

    SendPetitionShowList(guid);
}

void WorldSession::SendPetitionShowList(ObjectGuid guid)
{
    Creature* pCreature =
        GetPlayer()->GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_PETITIONER);
    if (!pCreature)
    {
        LOG_DEBUG(logging,
            "WORLD: HandlePetitionShowListOpcode - %s not found or you can't "
            "interact with him.",
            guid.GetString().c_str());
        return;
    }

    // remove fake death
    if (GetPlayer()->hasUnitState(UNIT_STAT_DIED))
        GetPlayer()->remove_auras(SPELL_AURA_FEIGN_DEATH);

    uint8 count = 0;
    if (pCreature->isTabardDesigner())
        count = 1;
    else
        count = 3;

    WorldPacket data(SMSG_PETITION_SHOWLIST, 8 + 1 + 4 * 6);
    data << ObjectGuid(guid); // npc guid
    data << uint8(count);     // count
    if (count == 1)
    {
        data << uint32(1);                  // index
        data << uint32(GUILD_CHARTER);      // charter entry
        data << uint32(CHARTER_DISPLAY_ID); // charter display id
        data << uint32(GUILD_CHARTER_COST); // charter cost
        data << uint32(0);                  // unknown
        data << uint32(9);                  // required signs?
    }
    else
    {
        // 2v2
        data << uint32(1);                           // index
        data << uint32(ARENA_TEAM_CHARTER_2v2);      // charter entry
        data << uint32(CHARTER_DISPLAY_ID);          // charter display id
        data << uint32(ARENA_TEAM_CHARTER_2v2_COST); // charter cost
        data << uint32(2);                           // unknown
        data << uint32(2);                           // required signs?
        // 3v3
        data << uint32(2);                           // index
        data << uint32(ARENA_TEAM_CHARTER_3v3);      // charter entry
        data << uint32(CHARTER_DISPLAY_ID);          // charter display id
        data << uint32(ARENA_TEAM_CHARTER_3v3_COST); // charter cost
        data << uint32(3);                           // unknown
        data << uint32(3);                           // required signs?
        // 5v5
        data << uint32(3);                           // index
        data << uint32(ARENA_TEAM_CHARTER_5v5);      // charter entry
        data << uint32(CHARTER_DISPLAY_ID);          // charter display id
        data << uint32(ARENA_TEAM_CHARTER_5v5_COST); // charter cost
        data << uint32(5);                           // unknown
        data << uint32(5);                           // required signs?
    }
    // for(uint8 i = 0; i < count; ++i)
    //{
    //    data << uint32(i);                        // index
    //    data << uint32(GUILD_CHARTER);            // charter entry
    //    data << uint32(CHARTER_DISPLAY_ID);       // charter display id
    //    data << uint32(GUILD_CHARTER_COST+i);     // charter cost
    //    data << uint32(0);                        // unknown
    //    data << uint32(9);                        // required signs?
    //}
    send_packet(std::move(data));
}
