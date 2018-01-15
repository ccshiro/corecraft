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

#ifndef MANGOSSERVER_GUILD_H
#define MANGOSSERVER_GUILD_H

#include "Common.h"
#include "Item.h"
#include "ObjectAccessor.h"
#include "SharedDefines.h"
#include "inventory/guild_storage.h"
#include <unordered_map>

class Item;

#define GUILD_RANKS_MIN_COUNT 5
#define GUILD_RANKS_MAX_COUNT 10

enum GuildDefaultRanks
{
    // these ranks can be modified, but they cannot be deleted
    GR_GUILDMASTER = 0,
    GR_OFFICER = 1,
    GR_VETERAN = 2,
    GR_MEMBER = 3,
    GR_INITIATE = 4,
    // When promoting member server does: rank--;!
    // When demoting member server does: rank++;!
};

enum GuildRankRights
{
    GR_RIGHT_EMPTY = 0x00000040,
    GR_RIGHT_GCHATLISTEN = 0x00000041,
    GR_RIGHT_GCHATSPEAK = 0x00000042,
    GR_RIGHT_OFFCHATLISTEN = 0x00000044,
    GR_RIGHT_OFFCHATSPEAK = 0x00000048,
    GR_RIGHT_PROMOTE = 0x000000C0,
    GR_RIGHT_DEMOTE = 0x00000140,
    GR_RIGHT_INVITE = 0x00000050,
    GR_RIGHT_REMOVE = 0x00000060,
    GR_RIGHT_SETMOTD = 0x00001040,
    GR_RIGHT_EPNOTE = 0x00002040,
    GR_RIGHT_VIEWOFFNOTE = 0x00004040,
    GR_RIGHT_EOFFNOTE = 0x00008040,
    GR_RIGHT_MODIFY_GUILD_INFO = 0x00010040,
    GR_RIGHT_WITHDRAW_GOLD_LOCK = 0x00020000, // remove money withdraw capacity
    GR_RIGHT_WITHDRAW_REPAIR = 0x00040000,    // withdraw for repair
    GR_RIGHT_WITHDRAW_GOLD = 0x00080000,      // withdraw gold
    GR_RIGHT_ALL = 0x000DF1FF
};

enum Typecommand
{
    GUILD_CREATE_S = 0x00,
    GUILD_INVITE_S = 0x01,
    GUILD_QUIT_S = 0x03,
    // 0x05?
    GUILD_FOUNDER_S = 0x0E,
    GUILD_UNK1 = 0x13,
    GUILD_UNK2 = 0x14
};

enum CommandErrors
{
    ERR_PLAYER_NO_MORE_IN_GUILD = 0x00,
    ERR_GUILD_INTERNAL = 0x01,
    ERR_ALREADY_IN_GUILD = 0x02,
    ERR_ALREADY_IN_GUILD_S = 0x03,
    ERR_INVITED_TO_GUILD = 0x04,
    ERR_ALREADY_INVITED_TO_GUILD_S = 0x05,
    ERR_GUILD_NAME_INVALID = 0x06,
    ERR_GUILD_NAME_EXISTS_S = 0x07,
    ERR_GUILD_LEADER_LEAVE = 0x08,
    ERR_GUILD_PERMISSIONS = 0x08,
    ERR_GUILD_PLAYER_NOT_IN_GUILD = 0x09,
    ERR_GUILD_PLAYER_NOT_IN_GUILD_S = 0x0A,
    ERR_GUILD_PLAYER_NOT_FOUND_S = 0x0B,
    ERR_GUILD_NOT_ALLIED = 0x0C,
    ERR_GUILD_RANK_TOO_HIGH_S = 0x0D,
    ERR_GUILD_RANK_TOO_LOW_S = 0x0E,
    ERR_GUILD_RANKS_LOCKED = 0x11,
    ERR_GUILD_RANK_IN_USE = 0x12,
    ERR_GUILD_IGNORING_YOU_S = 0x13,
    ERR_GUILD_UNK1 = 0x14,
    ERR_GUILD_WITHDRAW_LIMIT = 0x18,
    ERR_GUILD_NOT_ENOUGH_MONEY = 0x19,
    ERR_GUILD_BANK_FULL = 0x1B,
    ERR_GUILD_ITEM_NOT_FOUND = 0x1C
};

enum GuildEvents
{
    GE_PROMOTION = 0x00,
    GE_DEMOTION = 0x01,
    GE_MOTD = 0x02,
    GE_JOINED = 0x03,
    GE_LEFT = 0x04,
    GE_REMOVED = 0x05,
    GE_LEADER_IS = 0x06,
    GE_LEADER_CHANGED = 0x07,
    GE_DISBANDED = 0x08,
    GE_TABARDCHANGE = 0x09,
    GE_RANK_CHANGED = 0x0A,
    GE_RANK_CREATED = 0x0B,
    GE_SIGNED_ON = 0x0C,
    GE_SIGNED_OFF = 0x0D,
    GE_BANK_BAGSLOTS_CHANGED = 0x0E,
    GE_BANK_TAB_PURCHASED = 0x0F,
    GE_BANK_TAB_UPDATED = 0x10,
    GE_BANK_SET_MONEY = 0x11,
    GE_BANK_MONEY_RELATIVE = 0x12, // Unsure how this one is used
    GE_BANK_TEXT_CHANGED = 0x13
};

enum PetitionTurns
{
    PETITION_TURN_OK = 0,
    PETITION_TURN_ALREADY_IN_GUILD = 2,
    PETITION_TURN_NEED_MORE_SIGNATURES = 4,
};

enum PetitionSigns
{
    PETITION_SIGN_OK = 0,
    PETITION_SIGN_ALREADY_SIGNED = 1,
    PETITION_SIGN_ALREADY_IN_GUILD = 2,
    PETITION_SIGN_CANT_SIGN_OWN = 3,
    PETITION_SIGN_NOT_SERVER = 4,
};

enum GuildBankRights
{
    GUILD_BANK_RIGHT_VIEW_TAB = 0x01,
    GUILD_BANK_RIGHT_PUT_ITEM = 0x02,
    GUILD_BANK_RIGHT_UPDATE_TEXT = 0x04,

    GUILD_BANK_RIGHT_DEPOSIT_ITEM =
        GUILD_BANK_RIGHT_VIEW_TAB | GUILD_BANK_RIGHT_PUT_ITEM,
    GUILD_BANK_RIGHT_FULL = 0xFF,
};

enum GuildBankEventLogTypes
{
    GUILD_BANK_LOG_DEPOSIT_ITEM = 1,
    GUILD_BANK_LOG_WITHDRAW_ITEM = 2,
    GUILD_BANK_LOG_MOVE_ITEM = 3,
    GUILD_BANK_LOG_DEPOSIT_MONEY = 4,
    GUILD_BANK_LOG_WITHDRAW_MONEY = 5,
    GUILD_BANK_LOG_REPAIR_MONEY = 6,
    GUILD_BANK_LOG_MOVE_ITEM2 = 7,
};

enum GuildEventLogTypes
{
    GUILD_EVENT_LOG_INVITE_PLAYER = 1,
    GUILD_EVENT_LOG_JOIN_GUILD = 2,
    GUILD_EVENT_LOG_PROMOTE_PLAYER = 3,
    GUILD_EVENT_LOG_DEMOTE_PLAYER = 4,
    GUILD_EVENT_LOG_UNINVITE_PLAYER = 5,
    GUILD_EVENT_LOG_LEAVE_GUILD = 6,
};

enum GuildEmblem
{
    ERR_GUILDEMBLEM_SUCCESS = 0,
    ERR_GUILDEMBLEM_INVALID_TABARD_COLORS = 1,
    ERR_GUILDEMBLEM_NOGUILD = 2,
    ERR_GUILDEMBLEM_NOTGUILDMASTER = 3,
    ERR_GUILDEMBLEM_NOTENOUGHMONEY = 4,
    ERR_GUILDEMBLEM_INVALIDVENDOR = 5
};

struct GuildEventLogEntry
{
    uint8 EventType;
    uint32 PlayerGuid1;
    uint32 PlayerGuid2;
    uint8 NewRank;
    uint64 TimeStamp;
};

struct GuildBankEventLogEntry
{
    uint8 EventType;
    uint32 PlayerGuid;
    uint32 ItemOrMoney;
    uint8 ItemStackCount;
    uint8 DestTabId;
    uint64 TimeStamp;

    bool isMoneyEvent() const
    {
        return EventType == GUILD_BANK_LOG_DEPOSIT_MONEY ||
               EventType == GUILD_BANK_LOG_WITHDRAW_MONEY ||
               EventType == GUILD_BANK_LOG_REPAIR_MONEY;
    }
};

struct MemberSlot
{
    void SetMemberStats(Player* player);
    void UpdateLogoutTime();
    void SetPNOTE(std::string pnote);
    void SetOFFNOTE(std::string offnote);
    void ChangeRank(uint32 newRank);

    ObjectGuid guid;
    uint32 accountId;
    std::string Name;
    uint32 RankId;
    uint8 Level;
    uint8 Class;
    uint32 ZoneId;
    uint64 LogoutTime;
    std::string Pnote;
    std::string OFFnote;
};

struct RankInfo
{
    RankInfo(std::string _name, uint32 _rights, uint32 _money)
      : Name(std::move(_name)), Rights(_rights), BankMoneyPerDay(_money)
    {
        for (uint8 i = 0; i < GUILD_BANK_MAX_TABS; ++i)
        {
            TabRight[i] = 0;
            TabSlotPerDay[i] = 0;
        }
    }

    std::string Name;
    uint32 Rights;
    uint32 BankMoneyPerDay;
    uint32 TabRight[GUILD_BANK_MAX_TABS];
    uint32 TabSlotPerDay[GUILD_BANK_MAX_TABS];
};

class Guild
{
public:
    Guild();
    ~Guild();

    bool Create(Player* leader, std::string gname);
    void CreateDefaultGuildRanks(int locale_idx);
    void Disband();

    typedef std::unordered_map<uint32, MemberSlot> MemberList;
    typedef std::vector<RankInfo> RankList;

    uint32 GetId() { return m_Id; }
    ObjectGuid GetLeaderGuid() const { return m_LeaderGuid; }
    std::string const& GetName() const { return m_Name; }
    std::string const& GetMOTD() const { return MOTD; }
    std::string const& GetGINFO() const { return GINFO; }

    uint32 GetCreatedYear() const { return m_CreatedYear; }
    uint32 GetCreatedMonth() const { return m_CreatedMonth; }
    uint32 GetCreatedDay() const { return m_CreatedDay; }

    int32 GetEmblemStyle() const { return m_EmblemStyle; }
    int32 GetEmblemColor() const { return m_EmblemColor; }
    int32 GetBorderStyle() const { return m_BorderStyle; }
    int32 GetBorderColor() const { return m_BorderColor; }
    int32 GetBackgroundColor() const { return m_BackgroundColor; }

    void SetLeader(ObjectGuid guid);
    bool AddMember(ObjectGuid plGuid, uint32 plRank);
    bool DelMember(ObjectGuid guid, bool isDisbanding = false);
    // lowest rank is the count of ranks - 1 (the highest rank_id in table)
    uint32 GetLowestRank() const { return m_Ranks.size() - 1; }

    void SetMOTD(std::string motd);
    void SetGINFO(std::string ginfo);
    void SetEmblem(int32 emblemStyle, int32 emblemColor, int32 borderStyle,
        int32 borderColor, int32 backgroundColor);

    uint32 GetMemberSize() const { return members.size(); }
    uint32 GetAccountsNumber();

    bool LoadGuildFromDB(QueryResult* guildDataResult);
    bool CheckGuildStructure();
    bool LoadRanksFromDB(QueryResult* guildRanksResult);
    bool LoadMembersFromDB(QueryResult* guildMembersResult);

    void SendMotd(WorldSession* player);

    void BroadcastToGuild(WorldSession* session, const std::string& msg,
        uint32 language = LANG_UNIVERSAL);
    void BroadcastToOfficers(WorldSession* session, const std::string& msg,
        uint32 language = LANG_UNIVERSAL);
    void BroadcastPacketToRank(WorldPacket* packet, uint32 rankId);
    void BroadcastPacket(WorldPacket* packet);

    void BroadcastEvent(GuildEvents event, ObjectGuid guid,
        char const* str1 = nullptr, char const* str2 = nullptr,
        char const* str3 = nullptr);
    void BroadcastEvent(GuildEvents event, char const* str1 = nullptr,
        char const* str2 = nullptr, char const* str3 = nullptr)
    {
        BroadcastEvent(event, ObjectGuid(), str1, str2, str3);
    }

    template <class Do>
    void BroadcastWorker(Do& _do, Player* except = nullptr)
    {
        for (auto& elem : members)
            if (Player* player = ObjectAccessor::FindPlayer(
                    ObjectGuid(HIGHGUID_PLAYER, elem.first)))
                if (player != except)
                    _do(player);
    }

    void CreateRank(std::string name, uint32 rights);
    void DelRank();
    std::string GetRankName(uint32 rankId);
    uint32 GetRankRights(uint32 rankId);
    uint32 GetRanksSize() const { return m_Ranks.size(); }

    void SetRankName(uint32 rankId, std::string name);
    void SetRankRights(uint32 rankId, uint32 rights);
    bool HasRankRight(uint32 rankId, uint32 right)
    {
        return ((GetRankRights(rankId) & right) != GR_RIGHT_EMPTY) ? true :
                                                                     false;
    }

    int32 GetRank(ObjectGuid guid)
    {
        MemberSlot* slot = GetMemberSlot(guid);
        return slot ? slot->RankId : -1;
    }

    MemberSlot* GetMemberSlot(ObjectGuid guid)
    {
        auto itr = members.find(guid.GetCounter());
        return itr != members.end() ? &itr->second : nullptr;
    }

    MemberSlot* GetMemberSlot(const std::string& name)
    {
        for (auto& elem : members)
            if (elem.second.Name == name)
                return &elem.second;

        return nullptr;
    }

    void Roster(Player* send_to = nullptr); // nullptr = broadcast
    void _Roster(Player* send_to);
    void Query(WorldSession* session);

    // Guild EventLog
    void LoadGuildEventLogFromDB();
    void DisplayGuildEventLog(WorldSession* session);
    void LogGuildEvent(uint8 EventType, ObjectGuid playerGuid1,
        ObjectGuid playerGuid2 = ObjectGuid(), uint8 newRank = 0);

    bool LoadBankRightsFromDB(QueryResult* result);
    // Setters return true if the rights actually changed
    bool set_bank_tab_rights(uint32 rank_id, uint8 tab_id,
        uint32 bank_rights_mask, uint32 withdrawals_per_day);
    bool set_bank_money_per_day(uint32 rank_id, uint32 money_per_day);
    uint32 get_bank_tab_rights(uint32 rank_id, uint8 tab_id) const
    {
        if (rank_id >= m_Ranks.size())
            return 0;
        return m_Ranks[rank_id].TabRight[tab_id];
    }
    uint32 get_bank_tab_withdrawals(uint32 rank_id, uint8 tab_id) const
    {
        if (rank_id >= m_Ranks.size())
            return 0;
        return m_Ranks[rank_id].TabSlotPerDay[tab_id];
    }
    uint32 get_bank_money_per_day(uint32 rank_id) const
    {
        if (rank_id >= m_Ranks.size())
            return 0;
        return m_Ranks[rank_id].BankMoneyPerDay;
    }

    inventory::guild_storage& storage() { return guild_bank_; }
    const inventory::guild_storage& storage() const { return guild_bank_; }

    void send_permissions(Player* player);
    // Sends a permission update for everyone with that rank (updates how much
    // bank money & tab withdrawals they have available)
    void send_permissions_for_rank(int rank_id);
    void set_default_tab_rights(uint8 rank_id, uint8 tab_id);

protected:
    void AddRank(const std::string& name, uint32 rights, uint32 money);
    void db_load_bank(uint32 tab_count, uint64 money);

    uint32 m_Id;
    std::string m_Name;
    ObjectGuid m_LeaderGuid;
    std::string MOTD;
    std::string GINFO;
    uint32 m_CreatedYear;
    uint32 m_CreatedMonth;
    uint32 m_CreatedDay;

    int32 m_EmblemStyle;
    int32 m_EmblemColor;
    int32 m_BorderStyle;
    int32 m_BorderColor;
    int32 m_BackgroundColor;
    uint32 m_accountsNumber; // 0 used as marker for need lazy calculation at
                             // request

    RankList m_Ranks;

    MemberList members;

    inventory::guild_storage guild_bank_;

    /*typedef std::vector<GuildBankTab*> TabListMap;
    TabListMap m_TabListMap;*/

    /** These are actually ordered lists. The first element is the oldest
     * entry.*/
    typedef std::list<GuildEventLogEntry> GuildEventLog;
    // typedef std::list<GuildBankEventLogEntry> GuildBankEventLog;
    GuildEventLog m_GuildEventLog;
    /*GuildBankEventLog m_GuildBankEventLog_Money;
    GuildBankEventLog m_GuildBankEventLog_Item[GUILD_BANK_MAX_TABS];*/

    uint32 m_GuildEventLogNextGuid;
    /*uint32 m_GuildBankEventLogNextGuid_Money;
    uint32 m_GuildBankEventLogNextGuid_Item[GUILD_BANK_MAX_TABS];*/

private:
    void UpdateAccountsNumber()
    {
        m_accountsNumber = 0;
    } // mark for lazy calculation at request in GetAccountsNumber
    void _ChangeRank(ObjectGuid guid, MemberSlot* slot, uint32 newRank);

    // used only from high level Swap/Move functions
    /*void   DisplayGuildBankContentUpdate(uint8 TabId, int32 slot1, int32 slot2
    = -1);
    void   DisplayGuildBankContentUpdate(uint8 TabId, GuildItemPosCountVec
    const& slots);*/

    // internal common parts for CanStore/StoreItem functions
    // void AppendDisplayGuildBankSlot( WorldPacket& data, GuildBankTab const
    // *tab, int32 slot );
};
#endif
