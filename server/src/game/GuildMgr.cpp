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

#include "GuildMgr.h"
#include "Guild.h"
#include "logging.h"
#include "ObjectGuid.h"
#include "ProgressBar.h"
#include "World.h"
#include "Database/DatabaseEnv.h"
#include "Policies/Singleton.h"

GuildMgr::GuildMgr()
  : last_reset_time_(WorldTimer::time_no_syscall()) // FIXME: This value should
                                                    // be loaded from the
                                                    // database,
// or a server restart will screw with the reset interval
{
}

GuildMgr::~GuildMgr()
{
    for (auto& elem : m_GuildMap)
        delete elem.second;
}

void GuildMgr::AddGuild(Guild* guild)
{
    m_GuildMap[guild->GetId()] = guild;
}

void GuildMgr::RemoveGuild(uint32 guildId)
{
    m_GuildMap.erase(guildId);
}

Guild* GuildMgr::GetGuildById(uint32 guildId) const
{
    auto itr = m_GuildMap.find(guildId);
    if (itr != m_GuildMap.end())
        return itr->second;

    return nullptr;
}

Guild* GuildMgr::GetGuildByName(std::string const& name) const
{
    for (const auto& elem : m_GuildMap)
        if (elem.second->GetName() == name)
            return elem.second;

    return nullptr;
}

Guild* GuildMgr::GetGuildByLeader(ObjectGuid const& guid) const
{
    for (const auto& elem : m_GuildMap)
        if (elem.second->GetLeaderGuid() == guid)
            return elem.second;

    return nullptr;
}

std::string GuildMgr::GetGuildNameById(uint32 guildId) const
{
    auto itr = m_GuildMap.find(guildId);
    if (itr != m_GuildMap.end())
        return itr->second->GetName();

    return "";
}

void GuildMgr::LoadGuilds()
{
    uint32 count = 0;

    //                                                    0             1
    //                                                    2          3
    //                                                    4           5 6
    QueryResult* result = CharacterDatabase.Query(
        "SELECT "
        "guild.guildid,guild.name,leaderguid,EmblemStyle,EmblemColor,"
        "BorderStyle,BorderColor,"
        //   7               8    9    10         11        12
        "BackgroundColor,info,motd,createdate,BankMoney,(SELECT "
        "COUNT(guild_bank_tab.guildid) FROM guild_bank_tab WHERE "
        "guild_bank_tab.guildid = guild.guildid) "
        "FROM guild ORDER BY guildid ASC");

    if (!result)
    {
        logging.info("Loaded %u guild definitions\n", count);
        return;
    }

    // load guild ranks
    QueryResult* guildRanksResult = CharacterDatabase.Query(
        "SELECT guildid,rid,rname,rights,BankMoneyPerDay FROM guild_rank ORDER "
        "BY guildid ASC, rid ASC");

    // load guild members
    QueryResult* guildMembersResult = CharacterDatabase.Query(
        "SELECT "
        "guildid,guild_member.guid,rank,pnote,offnote,bank_withdrawn_money,"
        "bank_withdrawals_tab0,bank_withdrawals_tab1,bank_withdrawals_tab2,"
        "bank_withdrawals_tab3,bank_withdrawals_tab4,bank_withdrawals_tab5,"
        "characters.name, characters.level, characters.class, characters.zone, "
        "characters.logout_time, characters.account "
        "FROM guild_member LEFT JOIN characters ON characters.guid = "
        "guild_member.guid ORDER BY guildid ASC");

    // load guild bank tab rights
    QueryResult* guildBankTabRightsResult = CharacterDatabase.Query(
        "SELECT guildid,TabId,rid,gbright,SlotPerDay FROM guild_bank_right "
        "ORDER BY guildid ASC, TabId ASC");

    BarGoLink bar(result->GetRowCount());

    do
    {
        // Field *fields = result->Fetch();

        bar.step();
        ++count;

        auto newGuild = new Guild;
        if (!newGuild->LoadGuildFromDB(result) ||
            !newGuild->LoadRanksFromDB(guildRanksResult) ||
            !newGuild->LoadMembersFromDB(guildMembersResult) ||
            !newGuild->LoadBankRightsFromDB(guildBankTabRightsResult) ||
            !newGuild->CheckGuildStructure())
        {
            newGuild->Disband();
            delete newGuild;
            continue;
        }

        newGuild->LoadGuildEventLogFromDB();
        AddGuild(newGuild);
    } while (result->NextRow());

    delete result;
    delete guildRanksResult;
    delete guildMembersResult;
    delete guildBankTabRightsResult;

    // delete unused LogGuid records in guild_eventlog and guild_bank_eventlog
    // table
    // you can comment these lines if you don't plan to change
    // CONFIG_UINT32_GUILD_EVENT_LOG_COUNT and
    // CONFIG_UINT32_GUILD_BANK_EVENT_LOG_COUNT
    CharacterDatabase.PExecute(
        "DELETE FROM guild_eventlog WHERE LogGuid > '%u'",
        sWorld::Instance()->getConfig(CONFIG_UINT32_GUILD_EVENT_LOG_COUNT));
    CharacterDatabase.PExecute(
        "DELETE FROM guild_bank_eventlog WHERE LogGuid > '%u'",
        sWorld::Instance()->getConfig(
            CONFIG_UINT32_GUILD_BANK_EVENT_LOG_COUNT));

    logging.info("Loaded %u guild definitions\n", count);
}

void GuildMgr::reset_storages_if_midnight()
{
    // Resources obtained from localtime() are contained in static data, no need
    // to release
    // This also means it's not thread-safe, and also that we need to extract
    // the info we need between the calls
    time_t temp = WorldTimer::time_no_syscall();
    std::tm* tm_s = std::localtime(&temp);
    if (!tm_s)
        return;
    int mday_local = tm_s->tm_mday;
    tm_s = std::localtime(&last_reset_time_);
    if (!tm_s)
        return;
    int mday_last = tm_s->tm_mday;

    // If we've passed midnight and day has changed (note: both decrease and
    // increase is possible),
    // it's time for us to update all the storages
    if (mday_local != mday_last)
    {
        LOG_DEBUG(logging,
            "GuildMgr: We've passed the midnight mark and it's now a new day.");
        reset_storages();
    }
}

void GuildMgr::reset_storages()
{
    LOG_DEBUG(logging, "GuildMgr: Resetting withdrawal limits for all guilds.");

    for (auto& elem : m_GuildMap)
        elem.second->storage().reset_withdrawal_limits(false);

    // Update the DB for ALL guilds
    CharacterDatabase.PExecute(
        "UPDATE guild_member SET bank_withdrawn_money = 0, "
        "bank_withdrawals_tab0 = 0, bank_withdrawals_tab1 = 0, "
        "bank_withdrawals_tab2 = 0, "
        "bank_withdrawals_tab3 = 0, bank_withdrawals_tab4 = 0, "
        "bank_withdrawals_tab5 = 0");

    last_reset_time_ = WorldTimer::time_no_syscall();
}
