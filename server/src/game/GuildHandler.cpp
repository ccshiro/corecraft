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

#include "Common.h"
#include "GossipDef.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "logging.h"
#include "ObjectMgr.h"
#include "Opcodes.h"
#include "SocialMgr.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"

static auto& gm_logger = logging.get_logger("gm.command");

void WorldSession::HandleGuildQueryOpcode(WorldPacket& recvPacket)
{
    uint32 guildId;
    recvPacket >> guildId;

    if (Guild* guild = sGuildMgr::Instance()->GetGuildById(guildId))
    {
        guild->Query(this);
        return;
    }

    SendGuildCommandResult(GUILD_CREATE_S, "", ERR_GUILD_PLAYER_NOT_IN_GUILD);
}

void WorldSession::HandleGuildCreateOpcode(WorldPacket& recvPacket)
{
    std::string gname;
    recvPacket >> gname;

    if (GetPlayer()->GetGuildId()) // already in guild
        return;

    auto guild = new Guild;
    if (!guild->Create(GetPlayer(), gname))
    {
        delete guild;
        return;
    }

    sGuildMgr::Instance()->AddGuild(guild);
}

void WorldSession::HandleGuildInviteOpcode(WorldPacket& recvPacket)
{
    std::string Invitedname, plname;
    Player* player = nullptr;

    recvPacket >> Invitedname;

    if (normalizePlayerName(Invitedname))
        player = ObjectAccessor::FindPlayerByName(Invitedname.c_str());

    if (!player)
    {
        SendGuildCommandResult(
            GUILD_INVITE_S, Invitedname, ERR_GUILD_PLAYER_NOT_FOUND_S);
        return;
    }

    Guild* guild =
        sGuildMgr::Instance()->GetGuildById(GetPlayer()->GetGuildId());
    if (!guild)
    {
        SendGuildCommandResult(
            GUILD_CREATE_S, "", ERR_GUILD_PLAYER_NOT_IN_GUILD);
        return;
    }

    // OK result but not send invite
    if (player->GetSocial()->HasIgnore(GetPlayer()->GetObjectGuid()))
        return;

    // not let enemies sign guild charter
    if (!sWorld::Instance()->getConfig(
            CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_GUILD) &&
        player->GetTeam() != GetPlayer()->GetTeam())
    {
        SendGuildCommandResult(
            GUILD_INVITE_S, Invitedname, ERR_GUILD_NOT_ALLIED);
        return;
    }

    if (player->GetGuildId())
    {
        plname = player->GetName();
        SendGuildCommandResult(GUILD_INVITE_S, plname, ERR_ALREADY_IN_GUILD_S);
        return;
    }

    if (player->GetGuildIdInvited())
    {
        plname = player->GetName();
        SendGuildCommandResult(
            GUILD_INVITE_S, plname, ERR_ALREADY_INVITED_TO_GUILD_S);
        return;
    }

    if (!guild->HasRankRight(GetPlayer()->GetRank(), GR_RIGHT_INVITE))
    {
        SendGuildCommandResult(GUILD_INVITE_S, "", ERR_GUILD_PERMISSIONS);
        return;
    }

    LOG_DEBUG(logging, "Player %s Invited %s to Join his Guild",
        GetPlayer()->GetName(), Invitedname.c_str());

    player->SetGuildIdInvited(GetPlayer()->GetGuildId());
    // Put record into guildlog
    guild->LogGuildEvent(GUILD_EVENT_LOG_INVITE_PLAYER,
        GetPlayer()->GetObjectGuid(), player->GetObjectGuid());

    WorldPacket data(SMSG_GUILD_INVITE, (8 + 10)); // guess size
    data << GetPlayer()->GetName();
    data << guild->GetName();
    player->GetSession()->send_packet(std::move(data));
}

void WorldSession::HandleGuildRemoveOpcode(WorldPacket& recvPacket)
{
    std::string plName;
    recvPacket >> plName;

    if (!normalizePlayerName(plName))
        return;

    Guild* guild =
        sGuildMgr::Instance()->GetGuildById(GetPlayer()->GetGuildId());
    if (!guild)
    {
        SendGuildCommandResult(
            GUILD_CREATE_S, "", ERR_GUILD_PLAYER_NOT_IN_GUILD);
        return;
    }

    if (!guild->HasRankRight(GetPlayer()->GetRank(), GR_RIGHT_REMOVE))
    {
        SendGuildCommandResult(GUILD_INVITE_S, "", ERR_GUILD_PERMISSIONS);
        return;
    }

    MemberSlot* slot = guild->GetMemberSlot(plName);
    if (!slot)
    {
        SendGuildCommandResult(
            GUILD_INVITE_S, plName, ERR_GUILD_PLAYER_NOT_IN_GUILD_S);
        return;
    }

    if (slot->RankId == GR_GUILDMASTER)
    {
        SendGuildCommandResult(GUILD_QUIT_S, "", ERR_GUILD_LEADER_LEAVE);
        return;
    }

    // do not allow to kick player with same or higher rights
    if (GetPlayer()->GetRank() >= slot->RankId)
    {
        SendGuildCommandResult(GUILD_QUIT_S, plName, ERR_GUILD_RANK_TOO_HIGH_S);
        return;
    }

    // Player himself should get the text event too
    guild->BroadcastEvent(GE_REMOVED, plName.c_str(), _player->GetName());

    // possible last member removed, do cleanup, and no need events
    if (guild->DelMember(slot->guid))
    {
        guild->Disband();
        delete guild;
        return;
    }

    // Put record into guild log
    guild->LogGuildEvent(GUILD_EVENT_LOG_UNINVITE_PLAYER,
        GetPlayer()->GetObjectGuid(), slot->guid);
}

void WorldSession::HandleGuildAcceptOpcode(WorldPacket& /*recvPacket*/)
{
    Guild* guild;
    Player* player = GetPlayer();

    guild = sGuildMgr::Instance()->GetGuildById(player->GetGuildIdInvited());
    if (!guild || player->GetGuildId())
        return;

    // not let enemies sign guild charter
    if (!sWorld::Instance()->getConfig(
            CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_GUILD) &&
        player->GetTeam() !=
            sObjectMgr::Instance()->GetPlayerTeamByGUID(guild->GetLeaderGuid()))
        return;

    if (!guild->AddMember(GetPlayer()->GetObjectGuid(), guild->GetLowestRank()))
        return;

    // Put record into guild log
    guild->LogGuildEvent(
        GUILD_EVENT_LOG_JOIN_GUILD, GetPlayer()->GetObjectGuid());

    guild->BroadcastEvent(
        GE_JOINED, player->GetObjectGuid(), player->GetName());
}

void WorldSession::HandleGuildDeclineOpcode(WorldPacket& /*recvPacket*/)
{
    GetPlayer()->SetGuildIdInvited(0);
    GetPlayer()->SetInGuild(0);
}

void WorldSession::HandleGuildInfoOpcode(WorldPacket& /*recvPacket*/)
{
    Guild* guild =
        sGuildMgr::Instance()->GetGuildById(GetPlayer()->GetGuildId());
    if (!guild)
    {
        SendGuildCommandResult(
            GUILD_CREATE_S, "", ERR_GUILD_PLAYER_NOT_IN_GUILD);
        return;
    }

    WorldPacket data(SMSG_GUILD_INFO, (5 * 4 + guild->GetName().size() + 1));
    data << guild->GetName();
    data << uint32(guild->GetCreatedDay());
    data << uint32(guild->GetCreatedMonth());
    data << uint32(guild->GetCreatedYear());
    data << uint32(guild->GetMemberSize());     // amount of chars
    data << uint32(guild->GetAccountsNumber()); // amount of accounts
    send_packet(std::move(data));
}

void WorldSession::HandleGuildRosterOpcode(WorldPacket& /*recvPacket*/)
{
    if (Guild* guild =
            sGuildMgr::Instance()->GetGuildById(_player->GetGuildId()))
        guild->Roster(_player);
}

void WorldSession::HandleGuildPromoteOpcode(WorldPacket& recvPacket)
{
    std::string plName;
    recvPacket >> plName;

    if (!normalizePlayerName(plName))
        return;

    Guild* guild =
        sGuildMgr::Instance()->GetGuildById(GetPlayer()->GetGuildId());
    if (!guild)
    {
        SendGuildCommandResult(
            GUILD_CREATE_S, "", ERR_GUILD_PLAYER_NOT_IN_GUILD);
        return;
    }
    if (!guild->HasRankRight(GetPlayer()->GetRank(), GR_RIGHT_PROMOTE))
    {
        SendGuildCommandResult(GUILD_INVITE_S, "", ERR_GUILD_PERMISSIONS);
        return;
    }

    MemberSlot* slot = guild->GetMemberSlot(plName);
    if (!slot)
    {
        SendGuildCommandResult(
            GUILD_INVITE_S, plName, ERR_GUILD_PLAYER_NOT_IN_GUILD_S);
        return;
    }

    if (slot->guid == GetPlayer()->GetObjectGuid())
    {
        SendGuildCommandResult(GUILD_INVITE_S, "", ERR_GUILD_NAME_INVALID);
        return;
    }

    // allow to promote only to lower rank than member's rank
    // guildmaster's rank = 0
    // GetPlayer()->GetRank() + 1 is highest rank that current player can
    // promote to
    if (GetPlayer()->GetRank() + 1 >= slot->RankId)
    {
        SendGuildCommandResult(
            GUILD_INVITE_S, plName, ERR_GUILD_RANK_TOO_HIGH_S);
        return;
    }

    uint32 newRankId =
        slot->RankId - 1; // when promoting player, rank is decreased

    slot->ChangeRank(newRankId);
    // Put record into guild log
    guild->LogGuildEvent(GUILD_EVENT_LOG_PROMOTE_PLAYER,
        GetPlayer()->GetObjectGuid(), slot->guid, newRankId);

    guild->BroadcastEvent(GE_PROMOTION, _player->GetName(), plName.c_str(),
        guild->GetRankName(newRankId).c_str());
}

void WorldSession::HandleGuildDemoteOpcode(WorldPacket& recvPacket)
{
    std::string plName;
    recvPacket >> plName;

    if (!normalizePlayerName(plName))
        return;

    Guild* guild =
        sGuildMgr::Instance()->GetGuildById(GetPlayer()->GetGuildId());

    if (!guild)
    {
        SendGuildCommandResult(
            GUILD_CREATE_S, "", ERR_GUILD_PLAYER_NOT_IN_GUILD);
        return;
    }

    if (!guild->HasRankRight(GetPlayer()->GetRank(), GR_RIGHT_DEMOTE))
    {
        SendGuildCommandResult(GUILD_INVITE_S, "", ERR_GUILD_PERMISSIONS);
        return;
    }

    MemberSlot* slot = guild->GetMemberSlot(plName);

    if (!slot)
    {
        SendGuildCommandResult(
            GUILD_INVITE_S, plName, ERR_GUILD_PLAYER_NOT_IN_GUILD_S);
        return;
    }

    if (slot->guid == GetPlayer()->GetObjectGuid())
    {
        SendGuildCommandResult(GUILD_INVITE_S, "", ERR_GUILD_NAME_INVALID);
        return;
    }

    // do not allow to demote same or higher rank
    if (GetPlayer()->GetRank() >= slot->RankId)
    {
        SendGuildCommandResult(
            GUILD_INVITE_S, plName, ERR_GUILD_RANK_TOO_HIGH_S);
        return;
    }

    // do not allow to demote lowest rank
    if (slot->RankId >= guild->GetLowestRank())
    {
        SendGuildCommandResult(
            GUILD_INVITE_S, plName, ERR_GUILD_RANK_TOO_LOW_S);
        return;
    }

    uint32 newRankId =
        slot->RankId + 1; // when demoting player, rank is increased

    slot->ChangeRank(newRankId);
    // Put record into guild log
    guild->LogGuildEvent(GUILD_EVENT_LOG_DEMOTE_PLAYER,
        GetPlayer()->GetObjectGuid(), slot->guid, newRankId);

    guild->BroadcastEvent(GE_DEMOTION, _player->GetName(), plName.c_str(),
        guild->GetRankName(slot->RankId).c_str());
}

void WorldSession::HandleGuildLeaveOpcode(WorldPacket& /*recvPacket*/)
{
    Guild* guild = sGuildMgr::Instance()->GetGuildById(_player->GetGuildId());
    if (!guild)
    {
        SendGuildCommandResult(
            GUILD_CREATE_S, "", ERR_GUILD_PLAYER_NOT_IN_GUILD);
        return;
    }

    if (_player->GetObjectGuid() == guild->GetLeaderGuid() &&
        guild->GetMemberSize() > 1)
    {
        SendGuildCommandResult(GUILD_QUIT_S, "", ERR_GUILD_LEADER_LEAVE);
        return;
    }

    if (_player->GetObjectGuid() == guild->GetLeaderGuid())
    {
        guild->Disband();
        delete guild;
        return;
    }

    SendGuildCommandResult(
        GUILD_QUIT_S, guild->GetName(), ERR_PLAYER_NO_MORE_IN_GUILD);

    if (guild->DelMember(_player->GetObjectGuid()))
    {
        guild->Disband();
        delete guild;
        return;
    }

    // Put record into guild log
    guild->LogGuildEvent(GUILD_EVENT_LOG_LEAVE_GUILD, _player->GetObjectGuid());

    guild->BroadcastEvent(
        GE_LEFT, _player->GetObjectGuid(), _player->GetName());
}

void WorldSession::HandleGuildDisbandOpcode(WorldPacket& /*recvPacket*/)
{
    Guild* guild =
        sGuildMgr::Instance()->GetGuildById(GetPlayer()->GetGuildId());
    if (!guild)
    {
        SendGuildCommandResult(
            GUILD_CREATE_S, "", ERR_GUILD_PLAYER_NOT_IN_GUILD);
        return;
    }

    if (GetPlayer()->GetObjectGuid() != guild->GetLeaderGuid())
    {
        SendGuildCommandResult(GUILD_INVITE_S, "", ERR_GUILD_PERMISSIONS);
        return;
    }

    guild->Disband();
    delete guild;

    LOG_DEBUG(logging, "WORLD: Guild Successfully Disbanded");
}

void WorldSession::HandleGuildLeaderOpcode(WorldPacket& recvPacket)
{
    std::string name;
    recvPacket >> name;

    Player* oldLeader = GetPlayer();

    if (!normalizePlayerName(name))
        return;

    Guild* guild = sGuildMgr::Instance()->GetGuildById(oldLeader->GetGuildId());

    if (!guild)
    {
        SendGuildCommandResult(
            GUILD_CREATE_S, "", ERR_GUILD_PLAYER_NOT_IN_GUILD);
        return;
    }

    if (oldLeader->GetObjectGuid() != guild->GetLeaderGuid())
    {
        SendGuildCommandResult(GUILD_INVITE_S, "", ERR_GUILD_PERMISSIONS);
        return;
    }

    MemberSlot* oldSlot = guild->GetMemberSlot(oldLeader->GetObjectGuid());
    if (!oldSlot)
    {
        SendGuildCommandResult(GUILD_INVITE_S, "", ERR_GUILD_PERMISSIONS);
        return;
    }

    MemberSlot* slot = guild->GetMemberSlot(name);
    if (!slot)
    {
        SendGuildCommandResult(
            GUILD_INVITE_S, name, ERR_GUILD_PLAYER_NOT_IN_GUILD_S);
        return;
    }

    guild->SetLeader(slot->guid);
    oldSlot->ChangeRank(GR_OFFICER);

    guild->BroadcastEvent(
        GE_LEADER_CHANGED, oldLeader->GetName(), name.c_str());
}

void WorldSession::HandleGuildMOTDOpcode(WorldPacket& recvPacket)
{
    std::string MOTD;

    if (!recvPacket.empty())
        recvPacket >> MOTD;
    else
        MOTD = "";

    Guild* guild =
        sGuildMgr::Instance()->GetGuildById(GetPlayer()->GetGuildId());
    if (!guild)
    {
        SendGuildCommandResult(
            GUILD_CREATE_S, "", ERR_GUILD_PLAYER_NOT_IN_GUILD);
        return;
    }
    if (!guild->HasRankRight(GetPlayer()->GetRank(), GR_RIGHT_SETMOTD))
    {
        SendGuildCommandResult(GUILD_INVITE_S, "", ERR_GUILD_PERMISSIONS);
        return;
    }

    guild->SetMOTD(MOTD);

    guild->BroadcastEvent(GE_MOTD, MOTD.c_str());
}

void WorldSession::HandleGuildSetPublicNoteOpcode(WorldPacket& recvPacket)
{
    std::string name, PNOTE;
    recvPacket >> name;

    if (!normalizePlayerName(name))
        return;

    Guild* guild =
        sGuildMgr::Instance()->GetGuildById(GetPlayer()->GetGuildId());

    if (!guild)
    {
        SendGuildCommandResult(
            GUILD_CREATE_S, "", ERR_GUILD_PLAYER_NOT_IN_GUILD);
        return;
    }

    if (!guild->HasRankRight(GetPlayer()->GetRank(), GR_RIGHT_EPNOTE))
    {
        SendGuildCommandResult(GUILD_INVITE_S, "", ERR_GUILD_PERMISSIONS);
        return;
    }

    MemberSlot* slot = guild->GetMemberSlot(name);
    if (!slot)
    {
        SendGuildCommandResult(
            GUILD_INVITE_S, name, ERR_GUILD_PLAYER_NOT_IN_GUILD_S);
        return;
    }

    recvPacket >> PNOTE;

    slot->SetPNOTE(PNOTE);

    guild->Roster(_player);
}

void WorldSession::HandleGuildSetOfficerNoteOpcode(WorldPacket& recvPacket)
{
    std::string plName, OFFNOTE;
    recvPacket >> plName;

    if (!normalizePlayerName(plName))
        return;

    Guild* guild =
        sGuildMgr::Instance()->GetGuildById(GetPlayer()->GetGuildId());

    if (!guild)
    {
        SendGuildCommandResult(
            GUILD_CREATE_S, "", ERR_GUILD_PLAYER_NOT_IN_GUILD);
        return;
    }
    if (!guild->HasRankRight(GetPlayer()->GetRank(), GR_RIGHT_EOFFNOTE))
    {
        SendGuildCommandResult(GUILD_INVITE_S, "", ERR_GUILD_PERMISSIONS);
        return;
    }

    MemberSlot* slot = guild->GetMemberSlot(plName);
    if (!slot)
    {
        SendGuildCommandResult(
            GUILD_INVITE_S, plName, ERR_GUILD_PLAYER_NOT_IN_GUILD_S);
        return;
    }

    recvPacket >> OFFNOTE;

    slot->SetOFFNOTE(OFFNOTE);

    guild->Roster(_player);
}

void WorldSession::HandleGuildRankOpcode(WorldPacket& recvPacket)
{
    std::string rankname;
    uint32 rankId;
    uint32 rights, MoneyPerDay;

    Guild* guild =
        sGuildMgr::Instance()->GetGuildById(GetPlayer()->GetGuildId());
    if (!guild)
    {
        recvPacket.rpos(recvPacket.wpos()); // set to end to avoid warnings spam
        SendGuildCommandResult(
            GUILD_CREATE_S, "", ERR_GUILD_PLAYER_NOT_IN_GUILD);
        return;
    }

    if (GetPlayer()->GetObjectGuid() != guild->GetLeaderGuid())
    {
        recvPacket.rpos(recvPacket.wpos()); // set to end to avoid warnings spam
        SendGuildCommandResult(GUILD_INVITE_S, "", ERR_GUILD_PERMISSIONS);
        return;
    }

    recvPacket >> rankId;
    recvPacket >> rights;
    recvPacket >> rankname;
    recvPacket >> MoneyPerDay;

    if (rankId >= guild->GetRanksSize())
        return;

    bool bank_rights_changes = false;

    for (int i = 0; i < GUILD_BANK_MAX_TABS; ++i)
    {
        uint32 BankRights;
        uint32 BankSlotPerDay;

        recvPacket >> BankRights;
        recvPacket >> BankSlotPerDay;

        if (guild->set_bank_tab_rights(
                rankId, static_cast<uint8>(i), BankRights, BankSlotPerDay))
            bank_rights_changes = true;
    }

    LOG_DEBUG(logging, "WORLD: Changed RankName to %s , Rights to 0x%.4X",
        rankname.c_str(), rights);

    if (guild->set_bank_money_per_day(rankId, MoneyPerDay))
        bank_rights_changes = true;
    guild->SetRankName(rankId, rankname);

    if (rankId == GR_GUILDMASTER) // prevent loss leader rights (FIXME: GM
                                  // shouldnt even have a rank entry, it should
                                  // always retrieve every permission in
                                  // permission checks)
        rights = GR_RIGHT_ALL;

    guild->SetRankRights(rankId, rights);

    guild->Query(this);
    guild->Roster(); // broadcast for tab rights update

    // If bank rights got changed, we need to resend permissions for everyone
    // with that rank
    if (bank_rights_changes)
    {
        // FIXME: This is not a nice way to make the client requery for the
        // permissions, but it's the only one I've managed to make work
        std::ostringstream ss;
        ss << std::hex << guild->storage().money().get();
        guild->BroadcastEvent(GE_BANK_SET_MONEY, ss.str().c_str());
    }
}

void WorldSession::HandleGuildAddRankOpcode(WorldPacket& recvPacket)
{
    std::string rankname;
    recvPacket >> rankname;

    Guild* guild =
        sGuildMgr::Instance()->GetGuildById(GetPlayer()->GetGuildId());
    if (!guild)
    {
        SendGuildCommandResult(
            GUILD_CREATE_S, "", ERR_GUILD_PLAYER_NOT_IN_GUILD);
        return;
    }

    if (GetPlayer()->GetObjectGuid() != guild->GetLeaderGuid())
    {
        SendGuildCommandResult(GUILD_INVITE_S, "", ERR_GUILD_PERMISSIONS);
        return;
    }

    if (guild->GetRanksSize() >=
        GUILD_RANKS_MAX_COUNT) // client not let create more 10 than ranks
        return;

    guild->CreateRank(rankname, GR_RIGHT_GCHATLISTEN | GR_RIGHT_GCHATSPEAK);

    guild->Query(this);
    guild->Roster(); // broadcast for tab rights update
}

void WorldSession::HandleGuildDelRankOpcode(WorldPacket& /*recvPacket*/)
{
    Guild* guild =
        sGuildMgr::Instance()->GetGuildById(GetPlayer()->GetGuildId());
    if (!guild)
    {
        SendGuildCommandResult(
            GUILD_CREATE_S, "", ERR_GUILD_PLAYER_NOT_IN_GUILD);
        return;
    }

    if (GetPlayer()->GetObjectGuid() != guild->GetLeaderGuid())
    {
        SendGuildCommandResult(GUILD_INVITE_S, "", ERR_GUILD_PERMISSIONS);
        return;
    }

    guild->DelRank();

    guild->Query(this);
    guild->Roster(); // broadcast for tab rights update
}

void WorldSession::SendGuildCommandResult(
    uint32 typecmd, const std::string& str, uint32 cmdresult)
{
    WorldPacket data(SMSG_GUILD_COMMAND_RESULT, (8 + str.size() + 1));
    data << typecmd;
    data << str;
    data << cmdresult;
    send_packet(std::move(data));
}

void WorldSession::HandleGuildChangeInfoTextOpcode(WorldPacket& recvPacket)
{
    std::string GINFO;
    recvPacket >> GINFO;

    Guild* guild =
        sGuildMgr::Instance()->GetGuildById(GetPlayer()->GetGuildId());
    if (!guild)
    {
        SendGuildCommandResult(
            GUILD_CREATE_S, "", ERR_GUILD_PLAYER_NOT_IN_GUILD);
        return;
    }

    if (!guild->HasRankRight(
            GetPlayer()->GetRank(), GR_RIGHT_MODIFY_GUILD_INFO))
    {
        SendGuildCommandResult(GUILD_CREATE_S, "", ERR_GUILD_PERMISSIONS);
        return;
    }

    guild->SetGINFO(GINFO);
}

void WorldSession::HandleSaveGuildEmblemOpcode(WorldPacket& recvPacket)
{
    ObjectGuid vendorGuid;
    int32 EmblemStyle, EmblemColor, BorderStyle, BorderColor, BackgroundColor;

    recvPacket >> vendorGuid;
    recvPacket >> EmblemStyle >> EmblemColor >> BorderStyle >> BorderColor >>
        BackgroundColor;

    Creature* pCreature = GetPlayer()->GetNPCIfCanInteractWith(
        vendorGuid, UNIT_NPC_FLAG_TABARDDESIGNER);
    if (!pCreature)
    {
        //"That's not an emblem vendor!"
        SendSaveGuildEmblem(ERR_GUILDEMBLEM_INVALIDVENDOR);
        LOG_DEBUG(logging,
            "WORLD: HandleSaveGuildEmblemOpcode - %s not found or you can't "
            "interact with him.",
            vendorGuid.GetString().c_str());
        return;
    }

    // remove fake death
    if (GetPlayer()->hasUnitState(UNIT_STAT_DIED))
        GetPlayer()->remove_auras(SPELL_AURA_FEIGN_DEATH);

    Guild* guild =
        sGuildMgr::Instance()->GetGuildById(GetPlayer()->GetGuildId());
    if (!guild)
    {
        //"You are not part of a guild!";
        SendSaveGuildEmblem(ERR_GUILDEMBLEM_NOGUILD);
        return;
    }

    if (guild->GetLeaderGuid() != GetPlayer()->GetObjectGuid())
    {
        //"Only guild leaders can create emblems."
        SendSaveGuildEmblem(ERR_GUILDEMBLEM_NOTGUILDMASTER);
        return;
    }

    // Verify that it's a valid tabard setup
    // TODO: These values probably exists in some shitty DBC somewhere, but I'm
    // too lazy for that.
    // Client tested values (2.4.3):
    // NOTE: I have not verified these ranged are intact! I only checked the min
    // and max value.
    // 170 Icons [0,169]
    // 17 Icon Colors [0,16]
    // 6 Borders [0,5]
    // 17 Border Colors [0,16]
    // 51 Backgrounds [0,51]
    auto in_range = [](int32 min, int32 max, int32 i)
    {
        return min <= i && i <= max;
    };
    if (!in_range(0, 169, EmblemStyle) || !in_range(0, 16, EmblemColor) ||
        !in_range(0, 5, BorderStyle) || !in_range(0, 16, BorderColor) ||
        !in_range(0, 51, BackgroundColor))
    {
        SendSaveGuildEmblem(ERR_GUILDEMBLEM_INVALID_TABARD_COLORS);
        return;
    }

    // XXX
    inventory::transaction trans;
    trans.remove(inventory::gold(10));
    if (!GetPlayer()->storage().finalize(trans))
    {
        SendSaveGuildEmblem(ERR_GUILDEMBLEM_NOTENOUGHMONEY);
        return;
    }

    guild->SetEmblem(
        EmblemStyle, EmblemColor, BorderStyle, BorderColor, BackgroundColor);

    //"Guild Emblem saved."
    SendSaveGuildEmblem(ERR_GUILDEMBLEM_SUCCESS);

    guild->Query(this);
}

void WorldSession::HandleGuildEventLogQueryOpcode(WorldPacket& /* recvPacket */)
{
    if (uint32 GuildId = GetPlayer()->GetGuildId())
        if (Guild* pGuild = sGuildMgr::Instance()->GetGuildById(GuildId))
            pGuild->DisplayGuildEventLog(this);
}

/******  GUILD BANK  *******/

void WorldSession::HandleGuildBankMoneyAvailable(WorldPacket& /*recv_data*/)
{
    uint32 guild_id = _player->GetGuildId();
    Guild* guild;
    if (!guild_id ||
        (guild = sGuildMgr::Instance()->GetGuildById(guild_id)) == nullptr)
        return;

    guild->storage().send_remaining_gold_withdrawal(_player);
}

void WorldSession::HandleGuildPermissions(WorldPacket& /*recv_data*/)
{
    uint32 guild_id = _player->GetGuildId();
    int32 rank_id;
    Guild* guild;
    if (!guild_id ||
        (guild = sGuildMgr::Instance()->GetGuildById(guild_id)) == nullptr ||
        (rank_id = guild->GetRank(_player->GetObjectGuid())) == -1)
        return;

    guild->send_permissions(_player);
}

/* Called when clicking on Guild bank gameobject */
void WorldSession::HandleGuildBankerActivate(WorldPacket& recv_data)
{
    ObjectGuid vault_guid;
    uint8 send_tab_info;
    recv_data >> vault_guid >> send_tab_info;

    if (!_player->GetGameObjectIfCanInteractWith(
            vault_guid, GAMEOBJECT_TYPE_GUILD_BANK))
        return;

    uint32 guild_id = _player->GetGuildId();
    Guild* guild;
    if (!guild_id ||
        (guild = sGuildMgr::Instance()->GetGuildById(guild_id)) == nullptr)
    {
        SendGuildCommandResult(GUILD_UNK1, "", ERR_GUILD_PLAYER_NOT_IN_GUILD);
        return;
    }

    guild->storage().send_bank_list(this, 0, send_tab_info, false);
}

/* Called when opening guild bank tab only (first one) */
void WorldSession::HandleGuildBankQueryTab(WorldPacket& recv_data)
{
    ObjectGuid vault_guid;
    uint8 tab_id, unk;
    recv_data >> vault_guid >> tab_id >> unk;

    if (!_player->GetGameObjectIfCanInteractWith(
            vault_guid, GAMEOBJECT_TYPE_GUILD_BANK))
        return;

    uint32 guild_id = _player->GetGuildId();
    Guild* guild;
    if (!guild_id ||
        (guild = sGuildMgr::Instance()->GetGuildById(guild_id)) == nullptr)
        return;

    guild->storage().send_bank_list(this, tab_id, false, true);
}

void WorldSession::HandleGuildBankDepositMoney(WorldPacket& recv_data)
{
    ObjectGuid vault_guid;
    uint32 money;
    recv_data >> vault_guid >> money;

    uint32 guild_id = _player->GetGuildId();
    Guild* guild;
    if (!guild_id ||
        (guild = sGuildMgr::Instance()->GetGuildById(guild_id)) == nullptr)
        return;

    if (!_player->GetGameObjectIfCanInteractWith(
            vault_guid, GAMEOBJECT_TYPE_GUILD_BANK))
        return;

    if (!guild->storage().deposit_money(_player, money))
        return;

    // logging money
    if (_player->GetSession()->GetSecurity() > SEC_PLAYER &&
        sWorld::Instance()->getConfig(CONFIG_BOOL_GM_LOG_TRADE))
    {
        gm_logger.info(
            "GM %s (Account: %u) deposit money (Amount: %u) to guild bank "
            "(Guild ID %u)",
            _player->GetName(), _player->GetSession()->GetAccountId(), money,
            guild_id);
    }
}

void WorldSession::HandleGuildBankWithdrawMoney(WorldPacket& recv_data)
{
    ObjectGuid vault_guid;
    uint32 money;
    recv_data >> vault_guid >> money;

    uint32 guild_id = _player->GetGuildId();
    Guild* guild;
    if (!guild_id ||
        (guild = sGuildMgr::Instance()->GetGuildById(guild_id)) == nullptr)
        return;

    if (!GetPlayer()->GetGameObjectIfCanInteractWith(
            vault_guid, GAMEOBJECT_TYPE_GUILD_BANK))
        return;

    guild->storage().withdraw_money(_player, money);
}

void WorldSession::HandleGuildBankSwapItems(WorldPacket& recv_data)
{
    ObjectGuid vault_guid;
    uint8 bank_to_bank;
    recv_data >> vault_guid >> bank_to_bank;

    uint32 guild_id = _player->GetGuildId();
    Guild* guild;
    if (!_player->GetGameObjectIfCanInteractWith(
            vault_guid, GAMEOBJECT_TYPE_GUILD_BANK) ||
        !guild_id ||
        (guild = sGuildMgr::Instance()->GetGuildById(guild_id)) == nullptr)
    {
        recv_data.rpos(recv_data.wpos());
        return;
    }

    if (bank_to_bank)
    {
        uint8 dst_tab, dst_index, src_tab, src_index, split_amount;
        uint32 item_id;

        recv_data >> dst_tab >> dst_index;
        recv_data.read_skip<uint32>(); // Always 0
        recv_data >> src_tab >> src_index;
        recv_data >> item_id;
        recv_data.read_skip<uint8>(); // Always 0
        recv_data >> split_amount;

        inventory::slot dst(inventory::guild_slot, dst_tab, dst_index);
        inventory::slot src(inventory::guild_slot, src_tab, src_index);

        guild->storage().swap(_player, dst, src, split_amount);
    }
    else
    {
        // Player to Bank or Bank to Player.
        // item_id == 0
        //
        // bank_to_player == false:
        //      if bank_index == 0xFF => auto store

        uint8 bank_tab, bank_index, bank_to_player;
        uint32 item_id;

        recv_data >> bank_tab >> bank_index >> item_id >> bank_to_player;
        if (bank_to_player)
        {
            uint32 count, unk;
            uint8 to_player;
            recv_data >> count >> to_player >> unk;
        }
        else // player to bank
        {
            uint8 player_bag, player_index, to_player, split_amount;
            recv_data >> player_bag >> player_index >> to_player >>
                split_amount;

            inventory::slot dst, src;

            dst = bank_index == 0xFF ?
                      inventory::slot(inventory::invalid_slot, bank_tab, 0) :
                      inventory::slot(
                          inventory::guild_slot, bank_tab, bank_index);
            src = inventory::slot(
                inventory::personal_slot, player_bag, player_index);

            // If we're swapping from bank to player, the slots stay the same
            // but src becomes dst and vice-versa
            if (to_player)
                std::swap(src, dst);

            guild->storage().swap(_player, dst, src, split_amount);
        }
    }
}

void WorldSession::HandleGuildBankBuyTab(WorldPacket& recv_data)
{
    ObjectGuid vault_guid;
    uint8 tab_id;
    recv_data >> vault_guid >> tab_id;

    if (!_player->GetGameObjectIfCanInteractWith(
            vault_guid, GAMEOBJECT_TYPE_GUILD_BANK))
        return;

    uint32 guild_id = _player->GetGuildId();
    Guild* guild;
    if (!guild_id ||
        (guild = sGuildMgr::Instance()->GetGuildById(guild_id)) == nullptr)
        return;

    guild->storage().attempt_purchase_tab(_player, tab_id);
}

void WorldSession::HandleGuildBankUpdateTab(WorldPacket& recv_data)
{
    ObjectGuid vault_guid;
    uint8 tab_id;
    std::string name, icon;
    recv_data >> vault_guid >> tab_id >> name >> icon;

    if (!_player->GetGameObjectIfCanInteractWith(
            vault_guid, GAMEOBJECT_TYPE_GUILD_BANK))
        return;

    uint32 guild_id = _player->GetGuildId();
    Guild* guild;
    if (!guild_id ||
        (guild = sGuildMgr::Instance()->GetGuildById(guild_id)) == nullptr)
        return;

    guild->storage().attempt_update_tab_name(_player, tab_id, name, icon);
}

void WorldSession::HandleGuildBankLogQuery(WorldPacket& recv_data)
{
    LOG_DEBUG(logging, "MSG_GUILD_BANK_LOG_QUERY (Client -> Server)");

    uint8 tab_id;
    recv_data >> tab_id;

    uint32 guild_id = _player->GetGuildId();
    Guild* guild;
    if (!guild_id ||
        (guild = sGuildMgr::Instance()->GetGuildById(guild_id)) == nullptr)
        return;

    guild->storage().display_log(this, tab_id);
}

void WorldSession::HandleQueryGuildBankTabText(WorldPacket& recv_data)
{
    LOG_DEBUG(logging, "MSG_QUERY_GUILD_BANK_TEXT");

    uint8 tab_id;
    recv_data >> tab_id;

    uint32 guild_id = _player->GetGuildId();
    Guild* guild;
    if (!guild_id ||
        (guild = sGuildMgr::Instance()->GetGuildById(guild_id)) == nullptr)
        return;

    guild->storage().send_tab_description(
        this, tab_id); // Escapes user-input for us
}

void WorldSession::HandleSetGuildBankTabText(WorldPacket& recv_data)
{
    uint8 tab_id;
    std::string description;
    recv_data >> tab_id >> description;

    uint32 guild_id = _player->GetGuildId();
    Guild* guild;
    if (!guild_id ||
        (guild = sGuildMgr::Instance()->GetGuildById(guild_id)) == nullptr)
        return;

    guild->storage().attempt_set_tab_description(_player, tab_id, description);
}

void WorldSession::SendSaveGuildEmblem(uint32 msg)
{
    WorldPacket data(MSG_SAVE_GUILD_EMBLEM, 4);
    data << uint32(msg); // not part of guild
    send_packet(std::move(data));
}
