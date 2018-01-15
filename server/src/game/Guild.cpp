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

#include "Guild.h"
#include "Chat.h"
#include "GuildMgr.h"
#include "Language.h"
#include "ObjectMgr.h"
#include "Opcodes.h"
#include "Player.h"
#include "SocialMgr.h"
#include "Util.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "Database/DatabaseEnv.h"

// XXX: Ranks creation needs to insert into bank tab rights
// XXX: Reordering of ranks needs to update rank ids in DB tables

//// MemberSlot ////////////////////////////////////////////
void MemberSlot::SetMemberStats(Player* player)
{
    Name = player->GetName();
    Level = player->getLevel();
    Class = player->getClass();
    ZoneId = player->IsInWorld() ? player->GetZoneId() : player->GetZoneId();
}

void MemberSlot::UpdateLogoutTime()
{
    LogoutTime = WorldTimer::time_no_syscall();
}

void MemberSlot::SetPNOTE(std::string pnote)
{
    Pnote = pnote;

    // pnote now can be used for encoding to DB
    CharacterDatabase.escape_string(pnote);
    CharacterDatabase.PExecute(
        "UPDATE guild_member SET pnote = '%s' WHERE guid = '%u'", pnote.c_str(),
        guid.GetCounter());
}

void MemberSlot::SetOFFNOTE(std::string offnote)
{
    OFFnote = offnote;

    // offnote now can be used for encoding to DB
    CharacterDatabase.escape_string(offnote);
    CharacterDatabase.PExecute(
        "UPDATE guild_member SET offnote = '%s' WHERE guid = '%u'",
        offnote.c_str(), guid.GetCounter());
}

void MemberSlot::ChangeRank(uint32 newRank)
{
    RankId = newRank;

    Player* player = sObjectMgr::Instance()->GetPlayer(guid);
    // If player not online data in data field will be loaded from guild tabs no
    // need to update it !!
    if (player)
        player->SetRank(newRank);

    CharacterDatabase.PExecute(
        "UPDATE guild_member SET rank='%u' WHERE guid='%u'", newRank,
        guid.GetCounter());
}

//// Guild /////////////////////////////////////////////////

Guild::Guild()
  : m_Id(0), m_CreatedYear(0), m_CreatedMonth(0), m_CreatedDay(0),
    m_EmblemStyle(-1), m_EmblemColor(-1), m_BorderStyle(-1), m_BorderColor(-1),
    m_BackgroundColor(-1), m_accountsNumber(0), guild_bank_(this),
    m_GuildEventLogNextGuid(0)
{
    /*XXX m_GuildBankEventLogNextGuid_Money = 0;
    for (uint8 i = 0; i < GUILD_BANK_MAX_TABS; ++i)
        m_GuildBankEventLogNextGuid_Item[i] = 0; */
}

Guild::~Guild()
{
}

bool Guild::Create(Player* leader, std::string gname)
{
    if (sGuildMgr::Instance()->GetGuildByName(gname))
        return false;

    WorldSession* lSession = leader->GetSession();
    if (!lSession)
        return false;

    m_LeaderGuid = leader->GetObjectGuid();
    m_Name = gname;
    GINFO = "";
    MOTD = "";
    m_Id = sObjectMgr::Instance()->GenerateGuildId();

    // creating data
    time_t now = time(nullptr);
    tm local = *(localtime(&now)); // dereference and assign
    m_CreatedDay = local.tm_mday;
    m_CreatedMonth = local.tm_mon + 1;
    m_CreatedYear = local.tm_year + 1900;

    LOG_DEBUG(logging, "GUILD: creating guild %s to leader: %s", gname.c_str(),
        m_LeaderGuid.GetString().c_str());

    // gname already assigned to Guild::name, use it to encode string for DB
    CharacterDatabase.escape_string(gname);

    std::string dbGINFO = GINFO;
    std::string dbMOTD = MOTD;
    CharacterDatabase.escape_string(dbGINFO);
    CharacterDatabase.escape_string(dbMOTD);

    CharacterDatabase.BeginTransaction();
    // CharacterDatabase.PExecute("DELETE FROM guild WHERE guildid='%u'", Id); -
    // MAX(guildid)+1 not exist
    CharacterDatabase.PExecute(
        "DELETE FROM guild_member WHERE guildid='%u'", m_Id);
    CharacterDatabase.PExecute(
        "INSERT INTO guild "
        "(guildid,name,leaderguid,info,motd,createdate,EmblemStyle,EmblemColor,"
        "BorderStyle,BorderColor,BackgroundColor,BankMoney) "
        "VALUES('%u','%s','%u', '%s', '%s','" UI64FMTD
        "','%d','%d','%d','%d','%d','0')",
        m_Id, gname.c_str(), m_LeaderGuid.GetCounter(), dbGINFO.c_str(),
        dbMOTD.c_str(), uint64(now), m_EmblemStyle, m_EmblemColor,
        m_BorderStyle, m_BorderColor, m_BackgroundColor);
    CharacterDatabase.CommitTransaction();

    CreateDefaultGuildRanks(lSession->GetSessionDbLocaleIndex());

    return AddMember(m_LeaderGuid, (uint32)GR_GUILDMASTER);
}

void Guild::CreateDefaultGuildRanks(int locale_idx)
{
    CharacterDatabase.PExecute(
        "DELETE FROM guild_rank WHERE guildid='%u'", m_Id);
    CharacterDatabase.PExecute(
        "DELETE FROM guild_bank_right WHERE guildid = '%u'", m_Id);

    CreateRank(
        sObjectMgr::Instance()->GetMangosString(LANG_GUILD_MASTER, locale_idx),
        GR_RIGHT_ALL);
    CreateRank(
        sObjectMgr::Instance()->GetMangosString(LANG_GUILD_OFFICER, locale_idx),
        GR_RIGHT_ALL);
    CreateRank(
        sObjectMgr::Instance()->GetMangosString(LANG_GUILD_VETERAN, locale_idx),
        GR_RIGHT_GCHATLISTEN | GR_RIGHT_GCHATSPEAK);
    CreateRank(
        sObjectMgr::Instance()->GetMangosString(LANG_GUILD_MEMBER, locale_idx),
        GR_RIGHT_GCHATLISTEN | GR_RIGHT_GCHATSPEAK);
    CreateRank(sObjectMgr::Instance()->GetMangosString(
                   LANG_GUILD_INITIATE, locale_idx),
        GR_RIGHT_GCHATLISTEN | GR_RIGHT_GCHATSPEAK);
}

bool Guild::AddMember(ObjectGuid plGuid, uint32 plRank)
{
    Player* pl = sObjectMgr::Instance()->GetPlayer(plGuid);
    if (pl)
    {
        if (pl->GetGuildId() != 0)
            return false;
    }
    else
    {
        if (Player::GetGuildIdFromDB(plGuid) != 0) // player already in guild
            return false;
    }

    // remove all player signs from another petitions
    // this will be prevent attempt joining player to many guilds and corrupt
    // guild data integrity
    Player::RemovePetitionsAndSigns(plGuid, 9);

    uint32 lowguid = plGuid.GetCounter();

    // fill player data
    MemberSlot newmember;

    newmember.guid = plGuid;

    if (pl)
    {
        newmember.accountId = pl->GetSession()->GetAccountId();
        newmember.Name = pl->GetName();
        newmember.Level = pl->getLevel();
        newmember.Class = pl->getClass();
        newmember.ZoneId = pl->GetZoneId();
    }
    else
    {
        //                                                     0    1     2
        //                                                     3    4
        std::unique_ptr<QueryResult> result(CharacterDatabase.PQuery(
            "SELECT name,level,class,zone,account FROM characters WHERE guid = "
            "'%u'",
            lowguid));
        if (!result)
            return false; // player doesn't exist

        Field* fields = result->Fetch();
        newmember.Name = fields[0].GetCppString();
        newmember.Level = fields[1].GetUInt8();
        newmember.Class = fields[2].GetUInt8();
        newmember.ZoneId = fields[3].GetUInt32();
        newmember.accountId = fields[4].GetInt32();
        if (newmember.Level < 1 ||
            !((1 << (newmember.Class - 1)) & CLASSMASK_ALL_PLAYABLE))
        {
            logging.error(
                "%s has a broken data in field `characters` table, cannot add "
                "him to guild.",
                plGuid.GetString().c_str());
            return false;
        }
    }

    newmember.RankId = plRank;
    newmember.OFFnote = (std::string) "";
    newmember.Pnote = (std::string) "";
    newmember.LogoutTime = WorldTimer::time_no_syscall();

    // Empty withdrawal limits data
    storage().set_withdrawal_limits(
        newmember.guid, inventory::guild_storage::withdrawal_data());

    members[lowguid] = newmember;

    static SqlStatementID add_member_id;
    SqlStatement stmt = CharacterDatabase.CreateStatement(add_member_id,
        "INSERT INTO guild_member (guildid, guid, rank, bank_withdrawn_money, "
        "bank_withdrawals_tab0, "
        "bank_withdrawals_tab1, bank_withdrawals_tab2, bank_withdrawals_tab3, "
        "bank_withdrawals_tab4, bank_withdrawals_tab5, pnote, offnote) "
        "VALUES(?, ?, ?, 0, 0, 0, 0, 0, 0, 0, ?, ?)");
    stmt.addUInt32(m_Id);
    stmt.addUInt32(lowguid);
    stmt.addUInt32(newmember.RankId);
    stmt.addString(newmember.Pnote);
    stmt.addString(newmember.OFFnote);
    stmt.Execute();

    // If player not in game data in data field will be loaded from guild
    // tables, no need to update it!!
    if (pl)
    {
        pl->SetInGuild(m_Id);
        pl->SetRank(newmember.RankId);
        pl->SetGuildIdInvited(0);

        if (auto session = pl->GetSession())
            SendMotd(session);
    }

    UpdateAccountsNumber();

    return true;
}

void Guild::SetMOTD(std::string motd)
{
    MOTD = motd;

    // motd now can be used for encoding to DB
    CharacterDatabase.escape_string(motd);
    CharacterDatabase.PExecute(
        "UPDATE guild SET motd='%s' WHERE guildid='%u'", motd.c_str(), m_Id);
}

void Guild::SetGINFO(std::string ginfo)
{
    GINFO = ginfo;

    // ginfo now can be used for encoding to DB
    CharacterDatabase.escape_string(ginfo);
    CharacterDatabase.PExecute(
        "UPDATE guild SET info='%s' WHERE guildid='%u'", ginfo.c_str(), m_Id);
}

bool Guild::LoadGuildFromDB(QueryResult* guildDataResult)
{
    if (!guildDataResult)
        return false;

    Field* fields = guildDataResult->Fetch();

    m_Id = fields[0].GetUInt32();
    m_Name = fields[1].GetCppString();
    m_LeaderGuid = ObjectGuid(HIGHGUID_PLAYER, fields[2].GetUInt32());
    m_EmblemStyle = fields[3].GetInt32();
    m_EmblemColor = fields[4].GetInt32();
    m_BorderStyle = fields[5].GetInt32();
    m_BorderColor = fields[6].GetInt32();
    m_BackgroundColor = fields[7].GetInt32();
    GINFO = fields[8].GetCppString();
    MOTD = fields[9].GetCppString();
    time_t time = time_t(fields[10].GetUInt64());
    uint64 money = fields[11].GetUInt64();

    uint32 purchasedTabs = fields[12].GetUInt32();
    if (purchasedTabs > GUILD_BANK_MAX_TABS)
        purchasedTabs = GUILD_BANK_MAX_TABS;

    db_load_bank(purchasedTabs, money);

    if (time > 0)
    {
        tm local = *(localtime(&time)); // dereference and assign
        m_CreatedDay = local.tm_mday;
        m_CreatedMonth = local.tm_mon + 1;
        m_CreatedYear = local.tm_year + 1900;
    }

    return true;
}

void Guild::db_load_bank(uint32 tab_count, uint64 money)
{
    guild_bank_.load(tab_count, inventory::copper(money));
}

bool Guild::CheckGuildStructure()
{
    // Repair the structure of guild
    // If the guildmaster doesn't exist or isn't the member of guild
    // attempt to promote another member
    int32 GM_rights = GetRank(m_LeaderGuid);
    if (GM_rights == -1)
    {
        if (DelMember(m_LeaderGuid))
            return false; // guild will disbanded and deleted in caller
    }
    else if (GM_rights != GR_GUILDMASTER)
        SetLeader(m_LeaderGuid);

    // Allow only 1 guildmaster, set other to officer
    for (auto& elem : members)
        if (elem.second.RankId == GR_GUILDMASTER &&
            m_LeaderGuid != elem.second.guid)
            elem.second.ChangeRank(GR_OFFICER);

    return true;
}

bool Guild::LoadRanksFromDB(QueryResult* guildRanksResult)
{
    if (!guildRanksResult)
    {
        logging.error(
            "Guild %u has broken `guild_rank` data, creating new...", m_Id);
        CreateDefaultGuildRanks(0);
        return true;
    }

    Field* fields;
    bool broken_ranks = false;

    // GUILD RANKS are sequence starting from 0 = GUILD_MASTER (ALL PRIVILEGES)
    // to max 9 (lowest privileges)
    // the lower rank id is considered higher rank - so promotion does rank--
    // and demotion does rank++
    // between ranks in sequence cannot be gaps - so 0,1,2,4 cannot be
    // min ranks count is 5 and max is 10.

    do
    {
        fields = guildRanksResult->Fetch();
        // condition that would be true when all ranks in QueryResult will be
        // processed and guild without ranks is being processed
        if (!fields)
            break;

        uint32 guildId = fields[0].GetUInt32();
        if (guildId < m_Id)
        {
            // there is in table guild_rank record which doesn't have guildid in
            // guild table, report error
            logging.error(
                "Guild %u does not exist but it has a record in guild_rank "
                "table, deleting it!",
                guildId);
            CharacterDatabase.PExecute(
                "DELETE FROM guild_rank WHERE guildid = '%u'", guildId);
            continue;
        }

        if (guildId >
            m_Id) // we loaded all ranks for this guild already, break cycle
            break;

        uint32 rankID = fields[1].GetUInt32();
        std::string rankName = fields[2].GetCppString();
        uint32 rankRights = fields[3].GetUInt32();
        uint32 rankMoney = fields[4].GetUInt32();

        if (rankID != m_Ranks.size()) // guild_rank.ids are sequence 0,1,2,3..
            broken_ranks = true;

        // first rank is guildmaster, prevent loss leader rights
        if (m_Ranks.empty())
            rankRights |= GR_RIGHT_ALL;

        AddRank(rankName, rankRights, rankMoney);
    } while (guildRanksResult->NextRow());

    if (m_Ranks.size() < GUILD_RANKS_MIN_COUNT) // if too few ranks, renew them
    {
        m_Ranks.clear();
        logging.error(
            "Guild %u has broken `guild_rank` data, creating new...", m_Id);
        CreateDefaultGuildRanks(0); // 0 is default locale_idx
        broken_ranks = false;
    }
    // guild_rank have wrong numbered ranks, repair
    if (broken_ranks)
    {
        logging.error(
            "Guild %u has broken `guild_rank` data, repairing...", m_Id);
        CharacterDatabase.BeginTransaction();
        CharacterDatabase.PExecute(
            "DELETE FROM guild_rank WHERE guildid='%u'", m_Id);
        for (size_t i = 0; i < m_Ranks.size(); ++i)
        {
            std::string name = m_Ranks[i].Name;
            uint32 rights = m_Ranks[i].Rights;
            CharacterDatabase.escape_string(name);
            CharacterDatabase.PExecute(
                "INSERT INTO guild_rank (guildid,rid,rname,rights) VALUES "
                "('%u', '%u', '%s', '%u')",
                m_Id, uint32(i), name.c_str(), rights);
        }
        CharacterDatabase.CommitTransaction();
    }

    return true;
}

bool Guild::LoadMembersFromDB(QueryResult* guildMembersResult)
{
    if (!guildMembersResult)
        return false;

    do
    {
        Field* fields = guildMembersResult->Fetch();
        // this condition will be true when all rows in QueryResult are
        // processed and new guild without members is going to be loaded -
        // prevent crash
        if (!fields)
            break;
        uint32 guildId = fields[0].GetUInt32();
        if (guildId < m_Id)
        {
            // there is in table guild_member record which doesn't have guildid
            // in guild table, report error
            logging.error(
                "Guild %u does not exist but it has a record in guild_member "
                "table, deleting it!",
                guildId);
            CharacterDatabase.PExecute(
                "DELETE FROM guild_member WHERE guildid = '%u'", guildId);
            continue;
        }

        if (guildId > m_Id)
            // we loaded all members for this guild already, break cycle
            break;

        MemberSlot newmember;
        uint32 lowguid = fields[1].GetUInt32();
        newmember.guid = ObjectGuid(HIGHGUID_PLAYER, lowguid);
        newmember.RankId = fields[2].GetUInt32();
        // don't allow member to have not existing rank!
        if (newmember.RankId >= m_Ranks.size())
            newmember.RankId = GetLowestRank();

        newmember.Pnote = fields[3].GetCppString();
        newmember.OFFnote = fields[4].GetCppString();

        inventory::guild_storage::withdrawal_data data;
        data.money = fields[5].GetUInt32();
        for (int i = 0; i < GUILD_BANK_MAX_TABS; ++i)
            data.items[i] = fields[6 + i].GetUInt32();
        storage().set_withdrawal_limits(newmember.guid, data);

        newmember.Name = fields[12].GetCppString();
        newmember.Level = fields[13].GetUInt8();
        newmember.Class = fields[14].GetUInt8();
        newmember.ZoneId = fields[15].GetUInt32();
        newmember.LogoutTime = fields[16].GetUInt64();
        newmember.accountId = fields[17].GetInt32();

        // this code will remove not existing character guids from guild
        if (newmember.Level < 1) // can be at broken `data` field
        {
            logging.error(
                "%s has a broken data in field `characters`.`data`, deleting "
                "him from guild!",
                newmember.guid.GetString().c_str());
            CharacterDatabase.PExecute(
                "DELETE FROM guild_member WHERE guid = '%u'", lowguid);
            continue;
        }
        if (!newmember.ZoneId)
        {
            logging.error(
                "%s has broken zone-data", newmember.guid.GetString().c_str());
            // here it will also try the same, to get the zone from
            // characters-table, but additional it tries to find
            // the zone through xy coords .. this is a bit redundant, but
            // shouldn't be called often
            newmember.ZoneId = Player::GetZoneIdFromDB(newmember.guid);
        }
        if (!((1 << (newmember.Class - 1)) &
                CLASSMASK_ALL_PLAYABLE)) // can be at broken `class` field
        {
            logging.error(
                "%s has a broken data in field `characters`.`class`, deleting "
                "him from guild!",
                newmember.guid.GetString().c_str());
            CharacterDatabase.PExecute(
                "DELETE FROM guild_member WHERE guid = '%u'", lowguid);
            continue;
        }

        members[lowguid] = newmember;

    } while (guildMembersResult->NextRow());

    if (members.empty())
        return false;

    UpdateAccountsNumber();

    return true;
}

void Guild::SetLeader(ObjectGuid guid)
{
    MemberSlot* slot = GetMemberSlot(guid);
    if (!slot)
        return;

    m_LeaderGuid = guid;
    slot->ChangeRank(GR_GUILDMASTER);

    CharacterDatabase.PExecute(
        "UPDATE guild SET leaderguid='%u' WHERE guildid='%u'",
        guid.GetCounter(), m_Id);
}

/**
 * Remove character from guild
 *
 * @param guid          Character that removed from guild
 * @param isDisbanding  Flag set if function called from Guild::Disband, so not
 *need update DB in per-member mode only or leader update
 *
 * @return true, if guild need to be disband and erase (no members or can't
 *setup leader)
 */
bool Guild::DelMember(ObjectGuid guid, bool isDisbanding)
{
    uint32 lowguid = guid.GetCounter();

    // guild master can be deleted when loading guild and guid doesn't exist in
    // characters table
    // or when he is removed from guild by gm command
    if (m_LeaderGuid == guid && !isDisbanding)
    {
        MemberSlot* oldLeader = nullptr;
        MemberSlot* best = nullptr;
        ObjectGuid newLeaderGUID;
        for (auto& elem : members)
        {
            if (elem.first == lowguid)
            {
                oldLeader = &(elem.second);
                continue;
            }

            if (!best || best->RankId > elem.second.RankId)
            {
                best = &(elem.second);
                newLeaderGUID = ObjectGuid(HIGHGUID_PLAYER, elem.first);
            }
        }

        if (!best)
            return true;

        SetLeader(newLeaderGUID);

        // If player not online data in data field will be loaded from guild
        // tabs no need to update it !!
        if (Player* newLeader =
                sObjectMgr::Instance()->GetPlayer(newLeaderGUID))
            newLeader->SetRank(GR_GUILDMASTER);

        // when leader non-exist (at guild load with deleted leader only) not
        // send broadcasts
        if (oldLeader)
        {
            BroadcastEvent(
                GE_LEADER_CHANGED, oldLeader->Name.c_str(), best->Name.c_str());
            BroadcastEvent(GE_LEFT, guid, oldLeader->Name.c_str());
        }
    }

    members.erase(lowguid);

    Player* player = sObjectMgr::Instance()->GetPlayer(guid);
    // If player not online data in data field will be loaded from guild tabs no
    // need to update it !!
    if (player)
    {
        player->SetInGuild(0);
        player->SetRank(0);
    }

    CharacterDatabase.PExecute(
        "DELETE FROM guild_member WHERE guid = '%u'", lowguid);
    storage().erase_withdrawal_limits(guid);

    if (!isDisbanding)
        UpdateAccountsNumber();

    return members.empty();
}

void Guild::SendMotd(WorldSession* player)
{
    WorldPacket data(SMSG_GUILD_EVENT, 1 + 1 + GetMOTD().size() + 1);
    data << uint8(GE_MOTD);
    data << uint8(1);
    data << GetMOTD();
    player->send_packet(std::move(data));
}

void Guild::BroadcastToGuild(
    WorldSession* session, const std::string& msg, uint32 language)
{
    if (session && session->GetPlayer() &&
        HasRankRight(session->GetPlayer()->GetRank(), GR_RIGHT_GCHATSPEAK))
    {
        WorldPacket data;
        ChatHandler::FillMessageData(
            &data, session, CHAT_MSG_GUILD, language, msg.c_str());

        for (MemberList::const_iterator itr = members.begin();
             itr != members.end(); ++itr)
        {
            Player* pl = ObjectAccessor::FindPlayer(
                ObjectGuid(HIGHGUID_PLAYER, itr->first));

            if (pl && pl->GetSession() &&
                HasRankRight(pl->GetRank(), GR_RIGHT_GCHATLISTEN) &&
                !pl->GetSocial()->HasIgnore(
                    session->GetPlayer()->GetObjectGuid()))
                pl->GetSession()->send_packet(&data);
        }
    }
}

void Guild::BroadcastToOfficers(
    WorldSession* session, const std::string& msg, uint32 language)
{
    if (session && session->GetPlayer() &&
        HasRankRight(session->GetPlayer()->GetRank(), GR_RIGHT_OFFCHATSPEAK))
    {
        for (MemberList::const_iterator itr = members.begin();
             itr != members.end(); ++itr)
        {
            WorldPacket data;
            ChatHandler::FillMessageData(
                &data, session, CHAT_MSG_OFFICER, language, msg.c_str());

            Player* pl = ObjectAccessor::FindPlayer(
                ObjectGuid(HIGHGUID_PLAYER, itr->first));

            if (pl && pl->GetSession() &&
                HasRankRight(pl->GetRank(), GR_RIGHT_OFFCHATLISTEN) &&
                !pl->GetSocial()->HasIgnore(
                    session->GetPlayer()->GetObjectGuid()))
                pl->GetSession()->send_packet(std::move(data));
        }
    }
}

void Guild::BroadcastPacket(WorldPacket* packet)
{
    for (MemberList::const_iterator itr = members.begin(); itr != members.end();
         ++itr)
    {
        Player* player =
            ObjectAccessor::FindPlayer(ObjectGuid(HIGHGUID_PLAYER, itr->first));
        if (player)
            player->GetSession()->send_packet(packet);
    }
}

void Guild::BroadcastPacketToRank(WorldPacket* packet, uint32 rankId)
{
    for (MemberList::const_iterator itr = members.begin(); itr != members.end();
         ++itr)
    {
        if (itr->second.RankId == rankId)
        {
            Player* player = ObjectAccessor::FindPlayer(
                ObjectGuid(HIGHGUID_PLAYER, itr->first));
            if (player)
                player->GetSession()->send_packet(packet);
        }
    }
}

void Guild::CreateRank(std::string name_, uint32 rights)
{
    if (m_Ranks.size() >= GUILD_RANKS_MAX_COUNT)
        return;

    // ranks are sequence 0,1,2,... where 0 means guildmaster
    uint32 new_rank_id = m_Ranks.size();

    AddRank(name_, rights, 0);

    // existing records in db should be deleted before calling this procedure
    // and m_PurchasedTabs must be loaded already

    m_Ranks[new_rank_id].BankMoneyPerDay = 0;
    for (uint32 i = 0; i < storage().tab_count(); ++i)
    {
        // create bank rights with 0
        CharacterDatabase.PExecute(
            "INSERT INTO guild_bank_right (guildid,TabId,rid) VALUES "
            "('%u','%u','%u')",
            m_Id, i, new_rank_id);
        m_Ranks[new_rank_id].TabRight[i] = 0;
        m_Ranks[new_rank_id].TabSlotPerDay[i] = 0;
    }
    // name now can be used for encoding to DB
    CharacterDatabase.escape_string(name_);
    CharacterDatabase.PExecute(
        "INSERT INTO guild_rank (guildid,rid,rname,rights) VALUES ('%u', '%u', "
        "'%s', '%u')",
        m_Id, new_rank_id, name_.c_str(), rights);
}

void Guild::AddRank(const std::string& name_, uint32 rights, uint32 money)
{
    m_Ranks.push_back(RankInfo(name_, rights, money));
}

void Guild::DelRank()
{
    // client won't allow to have less than GUILD_RANKS_MIN_COUNT ranks in guild
    if (m_Ranks.size() <= GUILD_RANKS_MIN_COUNT)
        return;

    // delete lowest guild_rank
    uint32 rank = GetLowestRank();
    CharacterDatabase.PExecute(
        "DELETE FROM guild_rank WHERE rid>='%u' AND guildid='%u'", rank, m_Id);
    CharacterDatabase.PExecute(
        "DELETE FROM guild_bank_right WHERE rid>='%u' AND guildid='%u'", rank,
        m_Id);

    m_Ranks.pop_back();
}

std::string Guild::GetRankName(uint32 rankId)
{
    if (rankId >= m_Ranks.size())
        return "<unknown>";

    return m_Ranks[rankId].Name;
}

uint32 Guild::GetRankRights(uint32 rankId)
{
    if (rankId >= m_Ranks.size())
        return 0;

    return m_Ranks[rankId].Rights;
}

void Guild::SetRankName(uint32 rankId, std::string name_)
{
    if (rankId >= m_Ranks.size())
        return;

    m_Ranks[rankId].Name = name_;

    // name now can be used for encoding to DB
    CharacterDatabase.escape_string(name_);
    CharacterDatabase.PExecute(
        "UPDATE guild_rank SET rname='%s' WHERE rid='%u' AND guildid='%u'",
        name_.c_str(), rankId, m_Id);
}

void Guild::SetRankRights(uint32 rankId, uint32 rights)
{
    if (rankId >= m_Ranks.size())
        return;

    m_Ranks[rankId].Rights = rights;

    CharacterDatabase.PExecute(
        "UPDATE guild_rank SET rights='%u' WHERE rid='%u' AND guildid='%u'",
        rights, rankId, m_Id);
}

/**
 * Disband guild including cleanup structures and DB
 *
 * Note: guild object need deleted after this in caller code.
 */
void Guild::Disband()
{
    BroadcastEvent(GE_DISBANDED);

    while (!members.empty())
    {
        MemberList::const_iterator itr = members.begin();
        DelMember(ObjectGuid(HIGHGUID_PLAYER, itr->first), true);
    }

    CharacterDatabase.BeginTransaction();
    CharacterDatabase.PExecute("DELETE FROM guild WHERE guildid = '%u'", m_Id);
    CharacterDatabase.PExecute(
        "DELETE FROM guild_rank WHERE guildid = '%u'", m_Id);

    // Delete everything the bank holds
    storage().on_disband();

    CharacterDatabase.PExecute(
        "DELETE FROM guild_eventlog WHERE guildid = '%u'", m_Id);
    CharacterDatabase.CommitTransaction();
    sGuildMgr::Instance()->RemoveGuild(m_Id);
}

void Guild::Roster(Player* send_to /*= NULL*/)
{
    if (send_to)
    {
        _Roster(send_to);
    }
    else
    {
        for (MemberList::const_iterator itr = members.begin();
             itr != members.end(); ++itr)
        {
            Player* player = ObjectAccessor::FindPlayer(
                ObjectGuid(HIGHGUID_PLAYER, itr->first));
            if (player)
                _Roster(player);
        }
    }
}

void Guild::_Roster(Player* send_to)
{
    WorldPacket data(
        SMSG_GUILD_ROSTER,
        (4 + MOTD.length() + 1 + GINFO.length() + 1 + 4 +
            m_Ranks.size() * (4 + 4 + GUILD_BANK_MAX_TABS * (4 + 4)) +
            members.size() * 50));
    data << uint32(members.size());
    data << MOTD;
    data << GINFO;

    data << uint32(m_Ranks.size());
    for (size_t i = 0; i < m_Ranks.size(); ++i)
    {
        auto& rank = m_Ranks[i];

        data << uint32(rank.Rights);

        // Send special, hard-coded values for guild master's bank access
        if (i == GR_GUILDMASTER)
        {
            data << uint32(0xFFFFFFFF);
            for (int i = 0; i < GUILD_BANK_MAX_TABS; ++i)
                data << uint32(GUILD_BANK_RIGHT_FULL) << uint32(0xFFFFFFFF);
            continue;
        }

        data << uint32(rank.BankMoneyPerDay); // count of: withdraw
                                              // gold(gold/day) Note: in game
                                              // set gold, in packet set bronze.
        for (int i = 0; i < GUILD_BANK_MAX_TABS; ++i)
        {
            data << uint32(rank.TabRight[i]); // for TAB_i rights: view tabs =
                                              // 0x01, deposit items =0x02
            data << uint32(rank.TabSlotPerDay[i]); // for TAB_i count of:
                                                   // withdraw items(stack/day)
        }
    }
    for (MemberList::const_iterator itr = members.begin(); itr != members.end();
         ++itr)
    {
        if (Player* pl = ObjectAccessor::FindPlayer(
                ObjectGuid(HIGHGUID_PLAYER, itr->first)))
        {
            data << pl->GetObjectGuid();
            data << uint8(1);
            data << pl->GetName();
            data << uint32(itr->second.RankId);
            data << uint8(pl->getLevel());
            data << uint8(pl->getClass());
            data << uint8(0); // new 2.4.0
            data << uint32(pl->GetZoneId());
            data << itr->second.Pnote;
            if (HasRankRight(send_to->GetRank(), GR_RIGHT_VIEWOFFNOTE))
                data << itr->second.OFFnote;
            else
                data << "";
        }
        else
        {
            data << ObjectGuid(HIGHGUID_PLAYER, itr->first);
            data << uint8(0);
            data << itr->second.Name;
            data << uint32(itr->second.RankId);
            data << uint8(itr->second.Level);
            data << uint8(itr->second.Class);
            data << uint8(0); // new 2.4.0
            data << uint32(itr->second.ZoneId);
            data << float(
                float(WorldTimer::time_no_syscall() - itr->second.LogoutTime) /
                DAY);
            data << itr->second.Pnote;
            if (HasRankRight(send_to->GetRank(), GR_RIGHT_VIEWOFFNOTE))
                data << itr->second.OFFnote;
            else
                data << "";
        }
    }

    send_to->GetSession()->send_packet(std::move(data));
}

void Guild::Query(WorldSession* session)
{
    WorldPacket data(
        SMSG_GUILD_QUERY_RESPONSE, (8 * 32 + 200)); // we can only guess size

    data << uint32(m_Id);
    data << m_Name;

    for (size_t i = 0; i < GUILD_RANKS_MAX_COUNT; ++i) // show always 10 ranks
    {
        if (i < m_Ranks.size())
            data << m_Ranks[i].Name;
        else
            data << uint8(0); // null string
    }

    data << int32(m_EmblemStyle);
    data << int32(m_EmblemColor);
    data << int32(m_BorderStyle);
    data << int32(m_BorderColor);
    data << int32(m_BackgroundColor);

    session->send_packet(std::move(data));
}

void Guild::SetEmblem(int32 emblemStyle, int32 emblemColor, int32 borderStyle,
    int32 borderColor, int32 backgroundColor)
{
    m_EmblemStyle = emblemStyle;
    m_EmblemColor = emblemColor;
    m_BorderStyle = borderStyle;
    m_BorderColor = borderColor;
    m_BackgroundColor = backgroundColor;

    CharacterDatabase.PExecute(
        "UPDATE guild SET EmblemStyle=%d, EmblemColor=%d, BorderStyle=%d, "
        "BorderColor=%d, BackgroundColor=%d WHERE guildid = %u",
        m_EmblemStyle, m_EmblemColor, m_BorderStyle, m_BorderColor,
        m_BackgroundColor, m_Id);
}

/**
 * Return the number of accounts that are in the guild after possible update if
 * required
 * A player may have many characters in the guild, but with the same account
 */
uint32 Guild::GetAccountsNumber()
{
    // not need recalculation
    if (m_accountsNumber)
        return m_accountsNumber;

    // We use a set to be sure each element will be unique
    std::set<uint32> accountsIdSet;
    for (MemberList::const_iterator itr = members.begin(); itr != members.end();
         ++itr)
        accountsIdSet.insert(itr->second.accountId);

    m_accountsNumber = accountsIdSet.size();

    return m_accountsNumber;
}

// *************************************************
// Guild Eventlog part
// *************************************************
// Display guild eventlog
void Guild::DisplayGuildEventLog(WorldSession* session)
{
    // Sending result
    WorldPacket data(MSG_GUILD_EVENT_LOG_QUERY, 0);
    // count, max count == 100
    data << uint8(m_GuildEventLog.size());
    for (GuildEventLog::const_iterator itr = m_GuildEventLog.begin();
         itr != m_GuildEventLog.end(); ++itr)
    {
        // Event type
        data << uint8(itr->EventType);
        // Player 1
        data << ObjectGuid(HIGHGUID_PLAYER, itr->PlayerGuid1);
        // Player 2 not for left/join guild events
        if (itr->EventType != GUILD_EVENT_LOG_JOIN_GUILD &&
            itr->EventType != GUILD_EVENT_LOG_LEAVE_GUILD)
            data << ObjectGuid(HIGHGUID_PLAYER, itr->PlayerGuid2);
        // New Rank - only for promote/demote guild events
        if (itr->EventType == GUILD_EVENT_LOG_PROMOTE_PLAYER ||
            itr->EventType == GUILD_EVENT_LOG_DEMOTE_PLAYER)
            data << uint8(itr->NewRank);
        // Event timestamp
        data << uint32(WorldTimer::time_no_syscall() - itr->TimeStamp);
    }
    session->send_packet(std::move(data));
}

// Load guild eventlog from DB
void Guild::LoadGuildEventLogFromDB()
{
    //                                                     0        1          2
    //                                                     3            4 5
    std::unique_ptr<QueryResult> result(CharacterDatabase.PQuery(
        "SELECT LogGuid, EventType, PlayerGuid1, PlayerGuid2, NewRank, "
        "TimeStamp FROM guild_eventlog WHERE guildid=%u ORDER BY TimeStamp "
        "DESC,LogGuid DESC LIMIT %u",
        m_Id, GUILD_EVENTLOG_MAX_RECORDS));
    if (!result)
        return;
    bool isNextLogGuidSet = false;
    // uint32 configCount =
    // sWorld::Instance()->getConfig(CONFIG_UINT32_GUILD_EVENT_LOG_COUNT);
    // First event in list will be the oldest and the latest event is last event
    // in list
    do
    {
        Field* fields = result->Fetch();
        if (!isNextLogGuidSet)
        {
            m_GuildEventLogNextGuid = fields[0].GetUInt32();
            isNextLogGuidSet = true;
        }
        // Fill entry
        GuildEventLogEntry NewEvent;
        NewEvent.EventType = fields[1].GetUInt8();
        NewEvent.PlayerGuid1 = fields[2].GetUInt32();
        NewEvent.PlayerGuid2 = fields[3].GetUInt32();
        NewEvent.NewRank = fields[4].GetUInt8();
        NewEvent.TimeStamp = fields[5].GetUInt64();

        // There can be a problem if more events have same TimeStamp the ORDER
        // can be broken when fields[0].GetUInt32() == configCount, but
        // events with same timestamp can appear when there is lag, and we
        // naively suppose that mangos isn't laggy
        // but if problem appears, player will see set of guild events that have
        // same timestamp in bad order

        // Add entry to list
        m_GuildEventLog.push_front(NewEvent);

    } while (result->NextRow());
}

// Add entry to guild eventlog
void Guild::LogGuildEvent(uint8 EventType, ObjectGuid playerGuid1,
    ObjectGuid playerGuid2, uint8 newRank)
{
    GuildEventLogEntry NewEvent;
    // Create event
    NewEvent.EventType = EventType;
    NewEvent.PlayerGuid1 = playerGuid1.GetCounter();
    NewEvent.PlayerGuid2 = playerGuid2.GetCounter();
    NewEvent.NewRank = newRank;
    NewEvent.TimeStamp = uint32(WorldTimer::time_no_syscall());
    // Count new LogGuid
    m_GuildEventLogNextGuid =
        (m_GuildEventLogNextGuid + 1) %
        sWorld::Instance()->getConfig(CONFIG_UINT32_GUILD_EVENT_LOG_COUNT);
    // Check max records limit
    if (m_GuildEventLog.size() >= GUILD_EVENTLOG_MAX_RECORDS)
        m_GuildEventLog.pop_front();
    // Add event to list
    m_GuildEventLog.push_back(NewEvent);
    // Save event to DB
    CharacterDatabase.PExecute(
        "DELETE FROM guild_eventlog WHERE guildid='%u' AND LogGuid='%u'", m_Id,
        m_GuildEventLogNextGuid);
    CharacterDatabase.PExecute(
        "INSERT INTO guild_eventlog (guildid, LogGuid, EventType, PlayerGuid1, "
        "PlayerGuid2, NewRank, TimeStamp) VALUES "
        "('%u','%u','%u','%u','%u','%u','" UI64FMTD "')",
        m_Id, m_GuildEventLogNextGuid, uint32(NewEvent.EventType),
        NewEvent.PlayerGuid1, NewEvent.PlayerGuid2, uint32(NewEvent.NewRank),
        NewEvent.TimeStamp);
}

void Guild::BroadcastEvent(GuildEvents event, ObjectGuid guid,
    char const* str1 /*=NULL*/, char const* str2 /*=NULL*/,
    char const* str3 /*=NULL*/)
{
    uint8 strCount = !str1 ? 0 : (!str2 ? 1 : (!str3 ? 2 : 3));

    WorldPacket data(SMSG_GUILD_EVENT, 1 + 1 + 1 * strCount + (!guid ? 0 : 8));
    data << uint8(event);
    data << uint8(strCount);

    if (str3)
    {
        data << str1;
        data << str2;
        data << str3;
    }
    else if (str2)
    {
        data << str1;
        data << str2;
    }
    else if (str1)
        data << str1;

    if (guid)
        data << ObjectGuid(guid);

    BroadcastPacket(&data);
}

bool Guild::LoadBankRightsFromDB(QueryResult* result)
{
    if (!result)
        return true;

    // FIXME: This function is horrible
    // The result we get contains the ENTIRE bank rights table's content, and we
    // need to
    // loop through and look for all the ones that have our guild id specified
    // They are ordered, however, so if we go above our guild id it means we've
    // found
    // all belonging to us

    do
    {
        Field* fields = result->Fetch();

        // Someone could've taken our results. See comment above
        if (!fields)
            break;

        uint32 guild_id = fields[0].GetUInt32();

        if (guild_id < m_Id)
        {
            // This should never happen (unless some SQL dev with sleezy fingers
            // has been touching my private parts)
            // If it does it means we've managed to not clean up resources of a
            // deleted guild
            logging.error(
                "Guild %u has been deleted, but still has information in the "
                "`guild_bank_right` table. This means someone has tampered "
                "with the data manually; shouldn't do that!",
                guild_id);
            CharacterDatabase.PExecute(
                "DELETE FROM guild_bank_right WHERE guildid = %u", guild_id);
            continue;
        }

        if (guild_id > m_Id) // See comment above
            break;

        uint8 tab_id = fields[1].GetUInt8();
        uint32 rank_id = fields[2].GetUInt32();
        uint8 rights = fields[3].GetUInt8();
        uint32 withdrawals_per_day = fields[4].GetUInt32();

        if (rank_id == GR_GUILDMASTER)
        {
            logging.error(
                "`guild_bank_right` has rid=0 (guild master) in its table, "
                "which is improper & unnecessary. Deleting.");
            CharacterDatabase.PExecute(
                "DELETE FROM guild_bank_right WHERE guildid = %u AND rid = %u "
                "AND tab_id = %u",
                guild_id, rank_id, tab_id);
            continue;
        }

        if (rank_id >= m_Ranks.size() || tab_id >= storage().tab_count())
            continue;

        m_Ranks[rank_id].TabRight[tab_id] = rights;
        m_Ranks[rank_id].TabSlotPerDay[tab_id] = withdrawals_per_day;

    } while (result->NextRow());

    // Always put GM to full-rights in all available tabs
    for (int i = 0; i < storage().tab_count(); ++i)
    {
        m_Ranks[GR_GUILDMASTER].TabRight[i] = GUILD_BANK_RIGHT_FULL;
        m_Ranks[GR_GUILDMASTER].TabSlotPerDay[i] = 0xFFFFFFFF;
    }

    return true;
}

bool Guild::set_bank_tab_rights(uint32 rank_id, uint8 tab_id,
    uint32 bank_rights_mask, uint32 withdrawals_per_day)
{
    if (rank_id >= m_Ranks.size() || tab_id >= storage().tab_count())
        return false;

    // Can't change guild master's permissions
    if (rank_id == GR_GUILDMASTER)
        return false;

    // 100,000 is confirmed to be the max count in retail
    if (withdrawals_per_day > 100000)
        withdrawals_per_day = 100000;

    if (m_Ranks[rank_id].TabRight[tab_id] == bank_rights_mask &&
        m_Ranks[rank_id].TabSlotPerDay[tab_id] == withdrawals_per_day)
        return false;

    m_Ranks[rank_id].TabRight[tab_id] = bank_rights_mask;
    m_Ranks[rank_id].TabSlotPerDay[tab_id] = withdrawals_per_day;

    CharacterDatabase.PExecute(
        "UPDATE guild_bank_right SET gbright = %u, SlotPerDay = %u WHERE "
        "guildid = %u AND TabId = %u AND rid = %u",
        bank_rights_mask, withdrawals_per_day, m_Id, tab_id, rank_id);

    return true;
}

bool Guild::set_bank_money_per_day(uint32 rank_id, uint32 money_per_day)
{
    if (rank_id >= m_Ranks.size())
        return false;

    // Can't change guild master's permissions
    if (rank_id == GR_GUILDMASTER)
        return false;

    // 100,000 is confirmed to be the max count in retail
    if (money_per_day > inventory::gold(100000).get())
        money_per_day = inventory::gold(100000).get();

    if (m_Ranks[rank_id].BankMoneyPerDay == money_per_day)
        return false;

    m_Ranks[rank_id].BankMoneyPerDay = money_per_day;

    CharacterDatabase.PExecute(
        "UPDATE guild_rank SET BankMoneyPerDay = %u WHERE rid = %u AND guildid "
        "= %u",
        money_per_day, rank_id, m_Id);

    return true;
}

void Guild::send_permissions_for_rank(int rank_id)
{
    // Disallow sending for Guild Master and invalid rank ids
    if (rank_id <= 0 || static_cast<unsigned int>(rank_id) >= m_Ranks.size())
        return;

    for (MemberList::const_iterator itr = members.begin(); itr != members.end();
         ++itr)
    {
        if (itr->second.RankId != static_cast<unsigned int>(rank_id))
            continue;

        if (Player* player = ObjectAccessor::FindPlayer(
                ObjectGuid(HIGHGUID_PLAYER, itr->first)))
            send_permissions(player);
    }
}

void Guild::send_permissions(Player* player)
{
    int rank = GetRank(player->GetObjectGuid());
    if (rank == -1)
        return;

    WorldPacket data(MSG_GUILD_PERMISSIONS, 64);
    data << uint32(rank);
    data << uint32(GetRankRights(rank));
    data << uint32(storage().remaining_money(
        player->GetObjectGuid())); // Remaining money withdrawal
    data << uint8(storage().tab_count());
    for (int i = 0; i < storage().tab_count(); ++i)
    {
        data << uint32(get_bank_tab_rights(rank, i));
        data << uint32(storage().remaining_withdrawals(
            player->GetObjectGuid(), i)); // Remaining withdrawals of this tab
    }

    LOG_DEBUG(logging, "MSG_GUILD_PERMISSIONS (Server -> Client)");
    player->GetSession()->send_packet(std::move(data));
}

void Guild::set_default_tab_rights(uint8 rank_id, uint8 tab_id)
{
    if (rank_id > m_Ranks.size())
        return;

    m_Ranks[rank_id].TabRight[tab_id] =
        rank_id == GR_GUILDMASTER ? GUILD_BANK_RIGHT_FULL : 0;
    m_Ranks[rank_id].TabSlotPerDay[tab_id] =
        rank_id == GR_GUILDMASTER ? 0xFFFFFFFF : 0;
}
