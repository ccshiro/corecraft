#include "inventory/guild_storage.h"
#include "Guild.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "World.h"
#include "Database/DatabaseEnv.h"

static inline inventory::copper guild_tab_price(uint8 tab_id)
{
    switch (tab_id)
    {
    case 0:
        return inventory::gold(100);
    case 1:
        return inventory::gold(250);
    case 2:
        return inventory::gold(500);
    case 3:
        return inventory::gold(1000);
    case 4:
        return inventory::gold(2500);
    case 5:
        return inventory::gold(5000);
    }
    return 0;
}

inventory::guild_storage::~guild_storage()
{
    for (auto& elem : items_)
    {
        std::vector<Item*>& item = elem.second;
        for (auto& item_i : item)
        {
            if (item_i && item_i->IsInWorld())
                item_i->RemoveFromWorld();
            delete item_i;
            item_i = nullptr;
        }
    }
    items_.clear();
}

void inventory::guild_storage::load_tabs(uint8 owned_tab_count)
{
    // TODO: we probably want to select the # of owned tabs in here, and not in
    // the actual guild load code??

    // We initialize all descriptions and owned tabs to their correct size,
    // descriptions are empty at this point
    descriptions_.resize(owned_tab_count);
    tab_count_ = owned_tab_count;
    for (uint8 i = 0; i < owned_tab_count; i++)
    {
        items_[i].resize(max_guild_tab_size, nullptr);
    }

    // Load all tab descriptions that aren't empty
    std::unique_ptr<QueryResult> result(CharacterDatabase.PQuery(
        "SELECT TabId, TabName, TabIcon, TabText FROM guild_bank_tab WHERE "
        "guildid='%u' ORDER BY TabId",
        guild_->GetId()));
    if (!result)
        return;

    do
    {
        Field* fields = result->Fetch();
        uint8 tab = fields[0].GetUInt8();
        if (tab >= owned_tab_count)
        {
            logging.error(
                "guild %u has a description for tab %u which it does not own",
                guild_->GetId(), tab);
            continue;
        }

        descriptions_[tab].name = fields[1].GetCppString();
        descriptions_[tab].icon = fields[2].GetCppString();
        descriptions_[tab].description = fields[3].GetCppString();
    } while (result->NextRow());
}

void inventory::guild_storage::load(uint8 owned_tab_count, copper gold)
{
    copper_ = gold;

    load_tabs(owned_tab_count);
    load_logs();

    std::unique_ptr<QueryResult> result(CharacterDatabase.PQuery(
        "SELECT data, TabId, SlotId, item_guid, item_entry FROM "
        "guild_bank_item JOIN item_instance ON item_guid = guid WHERE "
        "guildid='%u' ORDER BY TabId",
        guild_->GetId()));
    if (!result)
        return;

    do
    {
        Field* fields = result->Fetch();
        uint8 tab = fields[1].GetUInt8();
        uint8 index = fields[2].GetUInt8();
        uint32 item_guid = fields[3].GetUInt32();
        uint32 item_entry = fields[4].GetUInt32();

        slot item_slot(guild_slot, tab, index);

        // Just as for personal storage (check their ::load()), prototype can be
        // invalid at this point.
        const ItemPrototype* prototype =
            ObjectMgr::GetItemPrototype(item_entry);
        if (!prototype)
        {
            // NOTE: If more item related tables are added you need to fix both
            // this function, the personal one _AND_ Item::db_* functions
            logging.error(
                "guild_storage::load(): Item with invalid prototype (GUID: %u "
                "id: %u) in guild bank, skipped.",
                item_guid, item_entry);
            CharacterDatabase.PExecute(
                "DELETE FROM guild_bank_item WHERE guildid='%u' AND TabId='%u' "
                "AND SlotId='%u'",
                guild_->GetId(), item_slot.bag(), item_slot.index());
            CharacterDatabase.PExecute(
                "DELETE FROM item_instance WHERE guid = '%u'", item_guid);
            CharacterDatabase.PExecute(
                "DELETE FROM item_text WHERE guid = '%u'", item_guid);
            CharacterDatabase.PExecute(
                "DELETE FROM character_gifts WHERE item_guid = '%u'",
                item_guid);
            CharacterDatabase.PExecute(
                "DELETE FROM item_loot WHERE guid = '%u'", item_guid);
            continue;
        }

        auto item = new Item(prototype);

        if (!item->LoadFromDB(item_guid, fields))
        {
            CharacterDatabase.PExecute(
                "DELETE FROM guild_bank_item WHERE guildid='%u' AND TabId='%u' "
                "AND SlotId='%u'",
                guild_->GetId(), item_slot.bag(), item_slot.index());
            logging.error(
                "guild_storage::load(): Item not loadable from the databse "
                "(GUID: %u id: %u) in guild bank, skipped.",
                item_guid, item_entry);
            item->db_delete();
            delete item;
            continue;
        }

        if (!item_slot.valid())
        {
            CharacterDatabase.PExecute(
                "DELETE FROM guild_bank_item WHERE guildid='%u' AND TabId='%u' "
                "AND SlotId='%u'",
                guild_->GetId(), item_slot.bag(), item_slot.index());
            logging.error(
                "guild_storage::load(): Invalid slot for item (GUID: %u id: "
                "%u) in guild bank, skipped.",
                item_guid, item_entry);
            item->db_delete();
            delete item;
            continue;
        }

        // All went well, put the item into our storage
        items_[item_slot.bag()][item_slot.index()] = item;
        item->AddToWorld();

    } while (result->NextRow());
}

void inventory::guild_storage::load_logs()
{
    std::unique_ptr<QueryResult> result(CharacterDatabase.PQuery(
        "SELECT TabId, EventType, PlayerGuid, ItemOrMoney, ItemStackCount, "
        "DestTabId, TimeStamp FROM guild_bank_eventlog WHERE guildid = %u "
        "ORDER BY TimeStamp ASC",
        guild_->GetId()));
    if (!result)
        return;

    do
    {
        Field* fields = result->Fetch();

        uint8 tab_id = fields[0].GetUInt8();
        uint8 event_type = fields[1].GetUInt8();
        uint32 guid = fields[2].GetUInt32();
        uint32 item_money = fields[3].GetUInt32();
        uint32 stack_count = fields[4].GetUInt32();
        uint32 dest_tab = fields[5].GetUInt32();
        uint64 timestamp = fields[6].GetUInt64();

        // Make sure tab id is valid (i.e. we have access to that tab
        if (tab_id > GUILD_BANK_MAX_TABS ||
            (tab_id != GUILD_BANK_MAX_TABS && tab_id >= tab_count_))
        {
            logging.error(
                "guild_storage::load_logs(): Invalid tab_id in "
                "guild_bank_eventlog for guild %u",
                guild_->GetId());
            CharacterDatabase.PExecute(
                "DELETE FROM guild_bank_eventlog WHERE guilid = %u AND TabId = "
                "%u",
                guild_->GetId(), tab_id);
            continue;
        }

        // GUILD_BANK_MAX_TABS is the indicator that this log deals with gold
        if (tab_id == GUILD_BANK_MAX_TABS)
        {
            gold_log_entry e;
            e.event_type = event_type;
            e.guid = guid;
            e.timestamp = timestamp;
            e.amount = item_money;
            gold_log_.push_back(e);
        }
        // Otherwise it's the tab id
        else
        {
            item_log_entry e;
            e.event_type = event_type;
            e.guid = guid;
            e.timestamp = timestamp;
            e.item_id = item_money;
            e.stack_count = stack_count;
            e.dest_tab = dest_tab;
            tab_logs_[tab_id].push_back(e);
        }

    } while (result->NextRow());

    // Logs should never be able to exceed limit, but just in case we have code
    // to clean it up:
    while (gold_log_.size() > GUILD_BANK_LOG_LIMIT)
    {
        CharacterDatabase.PExecute(
            "DELETE FROM guild_bank_eventlog WHERE guildid = %u AND TabId = %u "
            "AND TimeStamp = " UI64FMTD " LIMIT 1",
            guild_->GetId(), GUILD_BANK_MAX_TABS, gold_log_.front().timestamp);
        gold_log_.pop_front();
    }
    for (int i = 0; i < GUILD_BANK_MAX_TABS; ++i)
        while (tab_logs_[i].size() > GUILD_BANK_LOG_LIMIT)
        {
            CharacterDatabase.PExecute(
                "DELETE FROM guild_bank_eventlog WHERE guildid = %u AND TabId "
                "= %u AND TimeStamp = " UI64FMTD " LIMIT 1",
                guild_->GetId(), i, tab_logs_[i].front().timestamp);
            tab_logs_[i].pop_front();
        }
}

bool inventory::guild_storage::withdraw_money(Player* player, copper c)
{
    if (c.get() > remaining_money(player->GetObjectGuid()))
    {
        player->GetSession()->SendGuildCommandResult(
            0, "", ERR_GUILD_WITHDRAW_LIMIT);
        return false;
    }
    if (copper_.get() < c.get())
    {
        player->GetSession()->SendGuildCommandResult(
            0, "", ERR_GUILD_NOT_ENOUGH_MONEY);
        return false;
    }

    inventory::transaction trans;
    trans.add(c);
    if (!player->storage().verify(trans))
    {
        player->SendEquipError(EQUIP_ERR_TOO_MUCH_GOLD, nullptr);
        return false;
    }
    player->storage().finalize(trans);

    copper_ -= c;
    log_gold(GUILD_BANK_LOG_WITHDRAW_MONEY, player->GetObjectGuid(), c.get());

    CharacterDatabase.BeginTransaction();

    CharacterDatabase.PExecute(
        "UPDATE guild SET BankMoney = %u WHERE guildid = %u", copper_.get(),
        guild_->GetId());
    if (guild_->GetRank(player->GetObjectGuid()) != GR_GUILDMASTER)
    {
        withdrawal_map_[player->GetObjectGuid()].money += c.get();
        CharacterDatabase.PExecute(
            "UPDATE guild_member SET bank_withdrawn_money = %u WHERE guildid = "
            "%u AND guid = %u",
            withdrawal_map_[player->GetObjectGuid()].money, guild_->GetId(),
            player->GetGUIDLow());
    }

    player->SaveGoldToDB();

    CharacterDatabase.CommitTransaction();

    // send_remaining_gold_withdrawal(player); -- The client will request for
    // this once he gets the BroadcastEvent packet
    std::ostringstream ss;
    ss << std::hex << copper_.get();
    guild_->BroadcastEvent(GE_BANK_SET_MONEY, ss.str().c_str());

    return true;
}

bool inventory::guild_storage::deposit_money(Player* player, copper c)
{
    copper copy = copper_;
    copy += c;
    if (copy.spill())
    {
        player->GetSession()->SendGuildCommandResult(
            0, "", ERR_GUILD_BANK_FULL);
        return false;
    }

    inventory::transaction trans;
    trans.remove(c);
    if (!player->storage().verify(trans))
    {
        player->SendEquipError(EQUIP_ERR_NOT_ENOUGH_MONEY, nullptr);
        return false;
    }
    player->storage().finalize(trans);

    copper_ += c;
    log_gold(GUILD_BANK_LOG_DEPOSIT_MONEY, player->GetObjectGuid(), c.get());

    CharacterDatabase.BeginTransaction();

    CharacterDatabase.PExecute(
        "UPDATE guild SET BankMoney = %u WHERE guildid = %u", copper_.get(),
        guild_->GetId());
    player->SaveGoldToDB();

    CharacterDatabase.CommitTransaction();

    // send_remaining_gold_withdrawal(player); -- The client will request for
    // this once he gets the BroadcastEvent packet
    std::ostringstream ss;
    ss << std::hex << copper_.get();
    guild_->BroadcastEvent(GE_BANK_SET_MONEY, ss.str().c_str());

    return true;
}

void inventory::guild_storage::send_bank_list(WorldSession* session,
    uint8 tab_id, bool send_tab_info, bool send_content) const
{
    bool sending_info_only = tab_id == 0 && send_content == false;

    // We need to either have that tab_id, or be sending base info of the bank
    if (tab_id >= tab_count_ && !sending_info_only)
        return;

    int rank = guild_->GetRank(session->GetPlayer()->GetObjectGuid());
    if (rank == -1)
        return;

    if (rank != GR_GUILDMASTER && !sending_info_only &&
        (guild_->get_bank_tab_rights(rank, tab_id) &
            GUILD_BANK_RIGHT_VIEW_TAB) == 0)
        return;

    WorldPacket data(
        SMSG_GUILD_BANK_LIST, sending_info_only ? 128 : 1024); // XXX: size

    data << uint64(copper_.get());
    data << uint8(tab_id);
    data << uint32(
        remaining_withdrawals(session->GetPlayer()->GetObjectGuid(), tab_id));
    data << uint8(send_tab_info);

    if (send_tab_info)
    {
        data << uint8(tab_count_); // Number of purchased tabs
        for (int i = 0; i < tab_count_; ++i)
        {
            data << descriptions_[i].name.c_str();
            data << descriptions_[i].icon.c_str();
        }
    }

    if (!send_content)
        data << uint8(0);
    else
        data << uint8(max_guild_tab_size);

    if (send_content)
    {
        // Render content
        for (int i = 0; i < max_guild_tab_size; ++i)
            display_append_slot(data, slot(guild_slot, tab_id, i));
    }

    session->send_packet(std::move(data));
}

void inventory::guild_storage::log_gold(
    uint8 etype, ObjectGuid player, uint32 amount)
{
    gold_log_entry e;
    e.event_type = etype;
    e.guid = player.GetCounter();
    e.timestamp = WorldTimer::time_no_syscall();
    e.amount = amount;

    if (gold_log_.size() == GUILD_BANK_LOG_LIMIT)
    {
        CharacterDatabase.PExecute(
            "DELETE FROM guild_bank_eventlog WHERE guildid = %u AND TabId = %u "
            "AND TimeStamp = " UI64FMTD " LIMIT 1",
            guild_->GetId(), GUILD_BANK_MAX_TABS, gold_log_.front().timestamp);
        gold_log_.pop_front();
    }

    gold_log_.push_back(e);
    CharacterDatabase.PExecute(
        "INSERT INTO guild_bank_eventlog (TabId, guildid, EventType, "
        "PlayerGuid, ItemOrMoney, TimeStamp) VALUES(%u, %u, %u, %u, "
        "%u, " UI64FMTD ")",
        GUILD_BANK_MAX_TABS, guild_->GetId(), e.event_type, e.guid, e.amount,
        e.timestamp);
}

void inventory::guild_storage::log_item(uint8 etype, uint8 tab_id,
    ObjectGuid player, uint32 item_id, uint32 stack_count, uint32 dest_tab)
{
    if (tab_id >= tab_count_)
        return;

    item_log_entry e;
    e.event_type = etype;
    e.guid = player.GetCounter();
    e.timestamp = WorldTimer::time_no_syscall();
    e.item_id = item_id;
    e.stack_count = stack_count;
    e.dest_tab = dest_tab;

    if (tab_logs_[tab_id].size() == GUILD_BANK_LOG_LIMIT)
    {
        CharacterDatabase.PExecute(
            "DELETE FROM guild_bank_eventlog WHERE guildid = %u AND TabId = %u "
            "AND TimeStamp = " UI64FMTD " LIMIT 1",
            guild_->GetId(), tab_id, tab_logs_[tab_id].front().timestamp);
        tab_logs_[tab_id].pop_front();
    }

    tab_logs_[tab_id].push_back(e);
    CharacterDatabase.PExecute(
        "INSERT INTO guild_bank_eventlog (TabId, guildid, EventType, "
        "PlayerGuid, ItemOrMoney, ItemStackCount, DestTabId, TimeStamp) "
        "VALUES(%u, %u, %u, %u, %u, %u, %u, " UI64FMTD ")",
        tab_id, guild_->GetId(), e.event_type, e.guid, e.item_id, e.stack_count,
        e.dest_tab, e.timestamp);
}

void inventory::guild_storage::display_log(
    WorldSession* session, uint8 tab_id) const
{
    if (tab_id != GUILD_BANK_MAX_TABS && tab_id >= tab_count_)
        return;

    // Client sends GUILD_BANK_MAX_TABS for log information about gold
    if (tab_id == GUILD_BANK_MAX_TABS)
    {
        WorldPacket data(MSG_GUILD_BANK_LOG_QUERY, 256);
        data << uint8(GUILD_BANK_MAX_TABS);
        data << uint8(gold_log_.size()); // Log size

        for (const auto& elem : gold_log_)
        {
            data << uint8(elem.event_type); // Event type
            data << ObjectGuid(HIGHGUID_PLAYER, elem.guid);
            data << elem.amount; // Money
            data << uint32(
                WorldTimer::time_no_syscall() - elem.timestamp); // Passed time
        }

        session->send_packet(std::move(data));
    }
    else
    {
        WorldPacket data(MSG_GUILD_BANK_LOG_QUERY); // XXX Size
        data << uint8(tab_id);
        data << uint8(tab_logs_[tab_id].size()); // Log size

        for (const auto& elem : tab_logs_[tab_id])
        {
            data << uint8(elem.event_type); // Event type
            data << ObjectGuid(HIGHGUID_PLAYER, uint32(elem.guid));
            data << uint32(elem.item_id);    // Item id
            data << uint8(elem.stack_count); // Stack count
            if (elem.event_type == GUILD_BANK_LOG_MOVE_ITEM ||
                elem.event_type == GUILD_BANK_LOG_MOVE_ITEM2)
                data << uint8(elem.dest_tab); // Destination tab id
            data << uint32(
                WorldTimer::time_no_syscall() - elem.timestamp); // Passed time
        }

        session->send_packet(std::move(data));
    }
}

void inventory::guild_storage::display_append_slot(
    WorldPacket& data, slot s) const
{
    Item* item = get(s);

    data << uint8(s.index());
    data << uint32(item ? item->GetEntry() : 0);
    if (item)
    {
        data << uint32(item->GetItemRandomPropertyId());
        if (item->GetItemRandomPropertyId())
            data << uint32(item->GetItemSuffixFactor());

        data << uint8(item->GetCount());
        data << uint32(0);
        data << uint8(0); // XXX: abs(pItem->GetSpellCharges()) according to TC

        size_t enchantment_count_pos = data.wpos();
        data << uint8(0);

        uint8 count = 0;
        for (uint32 i = PERM_ENCHANTMENT_SLOT; i < MAX_ENCHANTMENT_SLOT; ++i)
        {
            if (uint32 ench_id = item->GetEnchantmentId(EnchantmentSlot(i)))
            {
                data << uint8(i);
                data << uint32(ench_id);
                ++count;
            }
        }

        if (count > 0)
            data.put<uint8>(enchantment_count_pos, count);
    }
}

void inventory::guild_storage::send_remaining_gold_withdrawal(
    Player* player) const
{
    WorldPacket data(MSG_GUILD_BANK_MONEY_WITHDRAWAL_AVAILABLE, 4);
    data << remaining_money(player->GetObjectGuid());
    player->GetSession()->send_packet(std::move(data));
    LOG_DEBUG(logging,
        "MSG_GUILD_BANK_MONEY_WITHDRAWAL_AVAILABLE (Server -> Client)");
}

void inventory::guild_storage::send_tab_description(
    WorldSession* session, uint8 tab_id) const
{
    // We escape the input to allow direct usage with user-input
    if (tab_id >= tab_count_)
        return;

    WorldPacket data(MSG_QUERY_GUILD_BANK_TEXT);
    data << uint8(tab_id);
    data << descriptions_[tab_id].description;

    if (session)
        session->send_packet(std::move(data));
    else
        guild_->BroadcastPacket(&data);
}

bool inventory::guild_storage::has_tab_rights(
    Player* player, uint8 tab_id, uint32 flag) const
{
    int rank = guild_->GetRank(player->GetObjectGuid());
    if (rank == -1 || (guild_->get_bank_tab_rights(rank, tab_id) & flag) == 0)
        return false;
    return true;
}

bool inventory::guild_storage::has_rank_rights(
    Player* player, uint32 flag) const
{
    int rank = guild_->GetRank(player->GetObjectGuid());
    if (rank == -1 || (guild_->GetRankRights(rank) & flag) == 0)
        return false;
    return true;
}

uint32 inventory::guild_storage::remaining_money(
    ObjectGuid guid, bool repair) const
{
    auto itr = withdrawal_map_.find(guid);
    if (itr != withdrawal_map_.end())
    {
        int rank = guild_->GetRank(guid);
        if (rank == -1)
            return 0; // Invalid rank
        if (rank == GR_GUILDMASTER)
            return 0xFFFFFFFF; // Guild master has no limit
        if ((guild_->GetRankRights(rank) &
                (repair ? GR_RIGHT_WITHDRAW_REPAIR : GR_RIGHT_WITHDRAW_GOLD)) ==
            0)
            return 0; // Rank does not have the right to take any money

        // Get how much they've withdrawn from this tab and subtract that from
        // the maximum
        uint32 total = guild_->get_bank_money_per_day(rank);
        uint32 extracted = itr->second.money;
        if (total <= extracted) // Can be less if amount has changed recently
            return 0;
        return total - extracted;
    }
    return 0;
}

uint32 inventory::guild_storage::remaining_withdrawals(
    ObjectGuid guid, uint8 tab_id) const
{
    if (tab_id >= tab_count_)
        return 0;

    auto itr = withdrawal_map_.find(guid);
    if (itr != withdrawal_map_.end())
    {
        int rank = guild_->GetRank(guid);
        if (rank == -1)
            return 0; // Invalid rank
        if (rank == GR_GUILDMASTER)
            return 0xFFFFFFFF; // Guild master has no limit
        if ((guild_->get_bank_tab_rights(rank, tab_id) &
                GUILD_BANK_RIGHT_VIEW_TAB) == 0)
            return 0;

        // Get how many they've withdrawn from this tab and subtract that from
        // the maximum
        uint32 total = guild_->get_bank_tab_withdrawals(rank, tab_id);
        uint32 extracted = itr->second.items[tab_id];
        if (total <= extracted) // Can be less if amount has changed recently
            return 0;
        return total - extracted;
    }
    return 0;
}

void inventory::guild_storage::attempt_purchase_tab(
    Player* player, uint8 tab_id)
{
    // Note: This function deals with user-input; tab_id has to be verified
    if (guild_->GetRank(player->GetObjectGuid()) != GR_GUILDMASTER)
        return;

    // Attempt to buy a tab must always be the next tab.
    // Unsure why we even get the tab id sent by the client
    if (tab_count_ != tab_id)
        return;

    if (tab_id >= GUILD_BANK_MAX_TABS)
        return;

    inventory::transaction trans;
    copper cost = guild_tab_price(tab_id);
    trans.remove(cost);
    LOG_DEBUG(logging,
        "Purchasing tab with id %u for a total of: %s. Player has: %s.", tab_id,
        cost.str().c_str(), player->storage().money().str().c_str());
    if (!player->storage().finalize(trans))
        return;

    CharacterDatabase.BeginTransaction();

    CharacterDatabase.PExecute(
        "INSERT INTO guild_bank_tab (guildid, TabId, TabName, TabIcon) "
        "VALUES(%u, %u, \"\", \"\")",
        guild_->GetId(), tab_id);
    // Insert tab rights for each rank
    guild_->set_default_tab_rights(GR_GUILDMASTER, tab_id);
    for (uint32 i = 1; i < guild_->GetRanksSize();
         ++i) // 1 to skip the guild master rank
    {
        CharacterDatabase.PExecute(
            "INSERT INTO guild_bank_right (guildid, TabId, rid, gbright, "
            "SlotPerDay) VALUES(%u, %u, %u, %u, %u)",
            guild_->GetId(), tab_id, i, 0, 0);
        guild_->set_default_tab_rights(i, tab_id);
    }

    player->SaveGoldToDB();

    CharacterDatabase.CommitTransaction();

    items_[tab_id].resize(max_guild_tab_size, nullptr);
    descriptions_.push_back(tab_description());

    ++tab_count_;
    guild_->BroadcastEvent(GE_BANK_TAB_PURCHASED);
    guild_->send_permissions(player);
}

void inventory::guild_storage::attempt_update_tab_name(Player* player,
    uint8 tab_id, const std::string& name, const std::string& icon)
{
    // Note: This function deals with user-input; tab_id has to be verifie &
    // name + icon escaped

    if (guild_->GetRank(player->GetObjectGuid()) != GR_GUILDMASTER)
        return;

    if (tab_id >= tab_count_)
        return;

    // XXX: Validate name, client has a bunch of restrictions we need to apply
    // here as well
    if (name.empty() || name.length() > 16)
        return;

    // XXX: TODO: Verify if icon is a valid icon
    if (icon.empty() || icon.length() > 100)
        return;

    if (descriptions_[tab_id].name == name &&
        descriptions_[tab_id].icon == icon)
        return;

    if (guild_->GetRank(player->GetObjectGuid()) != GR_GUILDMASTER)
        return;

    descriptions_[tab_id].name = name;
    descriptions_[tab_id].icon = icon;

    static SqlStatementID update;
    SqlStatement stmt = CharacterDatabase.CreateStatement(update,
        "UPDATE guild_bank_tab SET TabName = ?, TabIcon = ? WHERE guildid = ? "
        "AND TabId = ?");
    stmt.PExecute(name.c_str(), icon.c_str(), guild_->GetId(), tab_id);

    char buf[2];
    sprintf(buf, "%u", tab_id);
    guild_->BroadcastEvent(
        GE_BANK_TAB_UPDATED, buf, name.c_str(), icon.c_str());
}

void inventory::guild_storage::attempt_set_tab_description(
    Player* player, uint8 tab_id, const std::string& description)
{
    if (!has_tab_rights(player, tab_id, GUILD_BANK_RIGHT_UPDATE_TEXT))
        return;

    if (tab_id >= tab_count_)
        return;

    if (description.empty() ||
        description.length() > 494) // Client-tested length
        return;

    if (descriptions_[tab_id].description == description)
        return;

    descriptions_[tab_id].description = description;

    static SqlStatementID update;
    SqlStatement stmt = CharacterDatabase.CreateStatement(update,
        "UPDATE guild_bank_tab SET TabText = ? WHERE guildid = ? AND TabId = "
        "?");
    stmt.PExecute(description.c_str(), guild_->GetId(), tab_id);

    send_tab_description(nullptr, tab_id);
}

void inventory::guild_storage::attempt_repair(Player* player, copper cost)
{
    uint32 available = remaining_money(player->GetObjectGuid(), true);
    if (available < cost.get())
    {
        player->GetSession()->SendGuildCommandResult(
            0, "", ERR_GUILD_WITHDRAW_LIMIT);
        return;
    }
    if (copper_.get() < cost.get())
    {
        player->GetSession()->SendGuildCommandResult(
            0, "", ERR_GUILD_NOT_ENOUGH_MONEY);
        return;
    }

    copper_ -= cost;
    player->durability(true, 1.0, true);
    log_gold(GUILD_BANK_LOG_REPAIR_MONEY, player->GetObjectGuid(), cost.get());

    CharacterDatabase.BeginTransaction();

    CharacterDatabase.PExecute(
        "UPDATE guild SET BankMoney = %u WHERE guildid = %u", copper_.get(),
        guild_->GetId());
    if (guild_->GetRank(player->GetObjectGuid()) != GR_GUILDMASTER)
    {
        withdrawal_map_[player->GetObjectGuid()].money += cost.get();
        CharacterDatabase.PExecute(
            "UPDATE guild_member SET bank_withdrawn_money = %u WHERE guildid = "
            "%u AND guid = %u",
            withdrawal_map_[player->GetObjectGuid()].money, guild_->GetId(),
            player->GetGUIDLow());
    }

    player->SaveInventoryAndGoldToDB();

    CharacterDatabase.CommitTransaction();

    // send_remaining_gold_withdrawal(player); -- The client will request for
    // this once he gets the BroadcastEvent packet
    std::ostringstream ss;
    ss << std::hex << copper_.get();
    guild_->BroadcastEvent(GE_BANK_SET_MONEY, ss.str().c_str());
}

void inventory::guild_storage::reset_withdrawal_limits(bool db)
{
    for (auto& elem : withdrawal_map_)
    {
        elem.second.money = 0;
        for (int i = 0; i < GUILD_BANK_MAX_TABS; ++i)
            elem.second.items[i] = 0;
    }

    if (db)
    {
        CharacterDatabase.PExecute(
            "UPDATE guild_member SET bank_withdrawn_money = 0, "
            "bank_withdrawals_tab0 = 0, bank_withdrawals_tab1 = 0, "
            "bank_withdrawals_tab2 = 0, "
            "bank_withdrawals_tab3 = 0, bank_withdrawals_tab4 = 0, "
            "bank_withdrawals_tab5 = 0 WHERE guilid = %u",
            guild_->GetId());
    }

    // XXX: Send to client
}

InventoryResult inventory::guild_storage::can_store(slot /*dst*/, Item* item,
    Player* /*player*/, bool /*skip_unique_check*/) const
{
    if (item->IsSoulBound())
        return EQUIP_ERR_CANNOT_TRADE_THAT;

    if (item->HasGeneratedLoot())
        return EQUIP_ERR_ITEM_DOESNT_GO_TO_SLOT;

    return EQUIP_ERR_OK;
}

void inventory::guild_storage::swap(
    Player* player, inventory::slot dst, inventory::slot src, uint32 src_split)
{
    if (src.personal() && dst.valid() &&
        !dst.personal()) // Player to bank, given slot
    {
        if (!src.valid() || dst.bag() >= tab_count_ ||
            player->storage().get(src) == nullptr)
        {
            player->SendEquipError(
                EQUIP_ERR_INT_BAG_ERROR, player->storage().get(src));
            return;
        }

        if (!has_tab_rights(player, dst.bag(),
                GUILD_BANK_RIGHT_VIEW_TAB | GUILD_BANK_RIGHT_DEPOSIT_ITEM))
            return;

        player_to_bank(player, dst, src, src_split);
    }
    else if (src.personal() && !dst.valid()) // Player to bank, auto store
    {
        if (!src.valid() ||
            dst.bag() >= tab_count_ || // dst carries target tab_id
            player->storage().get(src) == nullptr)
        {
            player->SendEquipError(
                EQUIP_ERR_INT_BAG_ERROR, player->storage().get(src));
            return;
        }

        if (!has_tab_rights(player, dst.bag(),
                GUILD_BANK_RIGHT_VIEW_TAB | GUILD_BANK_RIGHT_DEPOSIT_ITEM))
            return;

        player_to_bank_auto(player, dst.bag(),
            src); // Even though it's invalid we still carry the tab_id here
    }
    else if (!src.personal() && !dst.personal()) // Bank to Bank
    {
        if (!src.valid() || src.bag() >= tab_count_ || !dst.valid() ||
            dst.bag() >= tab_count_ || get(src) == nullptr)
        {
            player->SendEquipError(
                EQUIP_ERR_INT_BAG_ERROR, player->storage().get(src));
            return;
        }

        if (!has_tab_rights(player, src.bag(),
                GUILD_BANK_RIGHT_VIEW_TAB | GUILD_BANK_RIGHT_DEPOSIT_ITEM) ||
            !has_tab_rights(player, dst.bag(),
                GUILD_BANK_RIGHT_VIEW_TAB | GUILD_BANK_RIGHT_DEPOSIT_ITEM))
            return;

        internal_swap(player, dst, src, src_split);
    }
    else if (!src.personal() && dst.valid() &&
             dst.personal()) // Bank to Player, given slot
    {
        if (!src.valid() || src.bag() >= tab_count_ || get(src) == nullptr)
        {
            player->SendEquipError(
                EQUIP_ERR_INT_BAG_ERROR, player->storage().get(src));
            return;
        }
        if (get(src)->GetCount() >
            remaining_withdrawals(player->GetObjectGuid(), src.bag()))
        {
            player->GetSession()->SendGuildCommandResult(
                0, "", ERR_GUILD_WITHDRAW_LIMIT);
            return;
        }

        bank_to_player(player, dst, src, src_split);
    }
}

void inventory::guild_storage::player_to_bank(
    Player* player, inventory::slot dst, inventory::slot src, uint32 src_split)
{
    // Note: The client actually does us the benefit of, if dst contains an
    // item, sending src_split of 0 if they cannot
    // stack, and src_split of no more than we can stack. So if we have 18
    // src_split will be sent as 2 if max stack is 20,
    // we of course need to verify the actions but we can use the fact that the
    // client calculates it for us to save some complexity.

    Item* src_item = player->storage().get(
        src); // Validity of slot & src_item verified previously
    Item* dst_item = get(dst);

    InventoryResult err;
    err = player->storage().can_delete_item(src);
    if (err != EQUIP_ERR_OK)
        return player->SendEquipError(err, src_item, dst_item);

    err = can_store(dst, src_item, player, false);
    if (err != EQUIP_ERR_OK)
        return player->SendEquipError(err, src_item, dst_item);

    if (src_split > src_item->GetCount())
        return; // The client won't send an ill-informed packet like this one

    if (dst_item)
    {
        if (src_split == 0)
        {
            InventoryResult err =
                player->storage().can_store(src, dst_item, player, false);
            if (err != EQUIP_ERR_OK)
                return player->SendEquipError(err, src_item, dst_item);
            if (remaining_withdrawals(player->GetObjectGuid(), dst.bag()) <=
                dst_item->GetCount())
                return player->GetSession()->SendGuildCommandResult(
                    0, "", ERR_GUILD_WITHDRAW_LIMIT);
            return do_player_swap(player, dst, src);
        }
        else
        {
            if (dst_item->GetEntry() != src_item->GetEntry())
                return;
            if (dst_item->GetCount() + src_split >
                dst_item->GetProto()->Stackable)
                return;
            return do_player_stack(player, dst, src, src_split);
        }
    }
    else
    {
        // No destination item, we're free to just place it in there
        if (src_split > 0)
        {
            if (src_split == src_item->GetCount()) // > checked above
                return; // The client won't send an ill-informed packet like
                        // this one
            do_player_partial_swap(player, dst, src, src_split);
        }
        else
        {
            do_player_swap(player, dst, src);
        }
    }
}

void inventory::guild_storage::bank_to_player(
    Player* player, inventory::slot dst, inventory::slot src, uint32 src_split)
{
    Item* src_item =
        get(src); // Validity of slot & src_item verified previously
    Item* dst_item = player->storage().get(dst);
    uint32 count = src_item->GetCount();

    InventoryResult err;
    err = player->storage().can_store(dst, src_item, player, false);
    if (err != EQUIP_ERR_OK)
        return player->SendEquipError(err, src_item, dst_item);

    if (src_split > src_item->GetCount())
        return; // The client won't send an ill-informed packet like this one

    if (dst_item)
    {
        if (src_split == 0)
        {
            InventoryResult err = can_store(src, dst_item, player, false);
            if (err != EQUIP_ERR_OK)
                return player->SendEquipError(err, src_item, dst_item);
            err = player->storage().can_delete_item(dst);
            if (err != EQUIP_ERR_OK)
                return player->SendEquipError(err, src_item, dst_item);
            do_player_swap(player, dst, src);
        }
        else
        {
            if (dst_item->GetEntry() != src_item->GetEntry())
                return;
            if (dst_item->GetCount() + src_split >
                dst_item->GetProto()->Stackable)
                return;
            do_player_stack(player, dst, src, src_split);
        }
    }
    else
    {
        // No destination item, we're free to just place it in there
        if (src_split > 0)
        {
            if (src_split == src_item->GetCount()) // > checked above
                return; // The client won't send an ill-informed packet like
                        // this one
            do_player_partial_swap(player, dst, src, src_split);
        }
        else
        {
            do_player_swap(player, dst, src);
        }
    }

    // Update withdrawal amount for player
    withdrawal_map_[player->GetObjectGuid()].items[src.bag()] += count;
}

void inventory::guild_storage::player_to_bank_auto(
    Player* player, uint8 tab_id, inventory::slot src)
{
    Item* src_item = player->storage().get(src); // Verified to exist previously
    uint32 remaining_count = src_item->GetCount();

    InventoryResult err = player->storage().can_delete_item(src);
    if (err != EQUIP_ERR_OK)
    {
        player->SendEquipError(err, player->storage().get(src));
        return;
    }

    std::vector<uint8> available_bags;
    available_bags.push_back(tab_id);
    std::vector<slot> slots =
        autostore_slots(player, src_item, src_item->GetCount(),
            available_bags); // calls can_store for all slots for us
    if (slots.empty())
    {
        player->GetSession()->SendGuildCommandResult(
            0, "", ERR_GUILD_BANK_FULL);
        return;
    }

    CharacterDatabase.BeginTransaction();
    player->storage().save(); // The storage must be at its latest revision when
                              // we begin poking the DB (cannot rollback from
                              // now on)

    for (auto itr = slots.begin(); itr != slots.end() && remaining_count > 0;
         ++itr)
    {
        Item* other_item = get(*itr);
        if (other_item)
        {
            // Append to other item's count
            uint32 free_count =
                src_item->GetProto()->Stackable - other_item->GetCount();
            uint32 add_count =
                free_count >= remaining_count ? remaining_count : free_count;
            other_item->SetCount(other_item->GetCount() + add_count);
            other_item->db_save();
            remaining_count -= add_count;
        }
        else
        {
            // If we get to an empty slot it means we're done with stacking and
            // should just create a new item
            if (Item* new_item =
                    Item::CreateItem(src_item->GetEntry(), remaining_count,
                        nullptr, src_item->GetItemRandomPropertyId()))
            {
                new_item->slot(*itr);
                new_item->AddToWorld();
                new_item->db_save();
                items_[itr->bag()][itr->index()] = new_item;
                CharacterDatabase.PExecute(
                    "INSERT INTO guild_bank_item (guildid, TabId, SlotId, "
                    "item_guid, item_entry) VALUES(%u, %u, %u, %u, %u)",
                    guild_->GetId(), itr->bag(), itr->index(),
                    new_item->GetGUIDLow(), new_item->GetEntry());
            }
            remaining_count = 0;
        }
    }

    log_item(GUILD_BANK_LOG_DEPOSIT_ITEM, tab_id, player->GetObjectGuid(),
        src_item->GetEntry(), src_item->GetCount(), 0);
    send_bank_list(
        player->GetSession(), tab_id, false, true); // XXX: BroadcastEvent

    CharacterDatabase.PExecute(
        "DELETE FROM character_inventory WHERE item = %u",
        src_item->GetGUIDLow());
    player->storage().pop_item(src_item, false);
    player->storage().on_item_removed(src_item->GetEntry());
    src_item->db_delete();
    delete src_item;

    CharacterDatabase.CommitTransaction();
}

void inventory::guild_storage::internal_swap(
    Player* player, inventory::slot dst, inventory::slot src, uint32 src_split)
{
    Item* src_item = get(src); // Verified to exist previously
    Item* dst_item = get(dst);
    uint32 src_count = src_item->GetCount();
    uint32 dst_count = dst_item ? dst_item->GetCount() : 0;
    uint32 item_id = src_item->GetEntry();

    // No need to call can_store, if it's internal all slots can store all items

    Item* new_item = nullptr;
    bool destroy_src = false;
    bool update_slots = false;

    if (src_split > 0)
    {
        if (dst_item)
        {
            // We're stacking on top of dst_item
            if (dst_item->GetEntry() != src_item->GetEntry())
                return;
            if (dst_item->GetCount() + src_split >
                dst_item->GetProto()->Stackable)
                return;
            dst_item->SetCount(dst_item->GetCount() + src_split);
        }
        else
        {
            // We're splitting off some of src into an empty slot
            if (src_item->GetCount() == src_split)
                return;
            new_item = Item::CreateItem(src_item->GetEntry(), src_split,
                nullptr, src_item->GetItemRandomPropertyId());
            if (!new_item)
                return;
        }
        if (src_split < src_item->GetCount())
            src_item->SetCount(src_item->GetCount() - src_split);
        else
            destroy_src = true;
    }
    else
    {
        items_[src.bag()][src.index()] = dst_item;
        if (dst_item)
            dst_item->slot(src);
        items_[dst.bag()][dst.index()] = src_item;
        src_item->slot(dst);
        update_slots = true;
    }

    // Carry out all the operations

    CharacterDatabase.BeginTransaction();

    if (src_count != src_item->GetCount())
        src_item->db_save();
    if (dst_item && dst_count != dst_item->GetCount())
        dst_item->db_save();

    if (new_item)
    {
        CharacterDatabase.PExecute(
            "INSERT INTO guild_bank_item (guildid, TabId, SlotId, item_guid, "
            "item_entry) VALUES(%u, %u, %u, %u, %u)",
            guild_->GetId(), dst.bag(), dst.index(), new_item->GetGUIDLow(),
            new_item->GetEntry());
        new_item->AddToWorld();
        new_item->slot(dst);
        new_item->db_save();
        items_[dst.bag()][dst.index()] = new_item;
    }

    if (destroy_src)
    {
        CharacterDatabase.PExecute(
            "DELETE FROM guild_bank_item WHERE item_guid = %u",
            src_item->GetGUIDLow());
        items_[src.bag()][src.index()] = nullptr;
        src_item->RemoveFromWorld();
        src_item->db_delete();
        delete src_item;
        src_item = nullptr;
    }

    if (update_slots)
    {
        if (src_item)
            CharacterDatabase.PExecute(
                "UPDATE guild_bank_item SET TabId = %u, SlotId = %u WHERE "
                "item_guid = %u",
                dst.bag(), dst.index(), src_item->GetGUIDLow());
        if (dst_item)
            CharacterDatabase.PExecute(
                "UPDATE guild_bank_item SET TabId = %u, SlotId = %u WHERE "
                "item_guid = %u",
                src.bag(), src.index(), dst_item->GetGUIDLow());
    }

    // Log moving of items
    if (update_slots && dst_item && dst.bag() != src.bag())
        log_item(GUILD_BANK_LOG_MOVE_ITEM, dst.bag(), player->GetObjectGuid(),
            dst_item->GetEntry(), dst_item->GetCount(), src.bag());
    if (dst.bag() != src.bag())
        log_item(GUILD_BANK_LOG_MOVE_ITEM, src.bag(), player->GetObjectGuid(),
            item_id, src_split ? src_split : 1, dst.bag());

    CharacterDatabase.CommitTransaction();

    send_bank_list(
        player->GetSession(), dst.bag(), false, true); // XXX: BroadcastEvent
}

void inventory::guild_storage::do_player_swap(
    Player* player, inventory::slot dst, inventory::slot src)
{
    Item* src_item =
        src.personal() ?
            player->storage().get(src) :
            get(src); // Validity of slot & src_item verified previously
    Item* dst_item = src.personal() ? get(dst) : player->storage().get(dst);

    CharacterDatabase.BeginTransaction();
    player->storage().save(); // The storage must be at its latest revision when
                              // we begin poking the DB (cannot rollback from
                              // now on)

    if (src.personal())
    {
        // We delete first so no primary key constraints mess with our
        // transaction
        CharacterDatabase.PExecute(
            "DELETE FROM character_inventory WHERE item = %u",
            src_item->GetGUIDLow());
        if (dst_item)
            CharacterDatabase.PExecute(
                "DELETE FROM guild_bank_item WHERE item_guid = %u",
                dst_item->GetGUIDLow());

        player->storage().pop_item(
            src_item, true); // true == don't remove from world
        player->storage().on_item_removed(src_item->GetEntry());
        items_[dst.bag()][dst.index()] = src_item;
        CharacterDatabase.PExecute(
            "INSERT INTO guild_bank_item (guildid, TabId, SlotId, item_guid, "
            "item_entry) VALUES(%u, %u, %u, %u, %u)",
            guild_->GetId(), dst.bag(), dst.index(), src_item->GetGUIDLow(),
            src_item->GetEntry());
        src_item->slot(dst);
        src_item->db_save();

        player->storage().items_[src.bag()][src.index()] = dst_item;
        if (dst_item)
        {
            CharacterDatabase.PExecute(
                "INSERT INTO character_inventory (guid, bag, `index`, item, "
                "item_template) VALUES(%u, %u, %u, %u, %u)",
                player->GetGUIDLow(), src.bag(), src.index(),
                dst_item->GetGUIDLow(), dst_item->GetEntry());
            dst_item->RemoveFromWorld(); // Added back by put_item
            player->storage().put_item(dst_item, src, false);
            player->storage().on_item_added(
                dst_item->GetEntry(), dst_item->GetCount());
            dst_item->db_save();
        }
    }
    else // For non personal we do the exact opposite of what we did for
         // personal
    {
        // We delete first so no primary key constraints mess with our
        // transaction
        CharacterDatabase.PExecute(
            "DELETE FROM guild_bank_item WHERE item_guid = %u",
            src_item->GetGUIDLow());
        if (dst_item)
            CharacterDatabase.PExecute(
                "DELETE FROM character_inventory WHERE item = %u",
                dst_item->GetGUIDLow());

        src_item->RemoveFromWorld(); // Readded by put_item

        items_[src.bag()][src.index()] = dst_item;
        if (dst_item)
        {
            CharacterDatabase.PExecute(
                "INSERT INTO guild_bank_item (guildid, TabId, SlotId, "
                "item_guid, item_entry) VALUES(%u, %u, %u, %u, %u)",
                guild_->GetId(), src.bag(), src.index(), dst_item->GetGUIDLow(),
                dst_item->GetEntry());
            player->storage().pop_item(
                dst_item, true); // true == don't remove from world
            player->storage().on_item_removed(dst_item->GetEntry());
            dst_item->slot(src);
            dst_item->db_save();
        }

        CharacterDatabase.PExecute(
            "INSERT INTO character_inventory (guid, bag, `index`, item, "
            "item_template) VALUES(%u, %u, %u, %u, %u)",
            player->GetGUIDLow(), dst.bag(), dst.index(),
            src_item->GetGUIDLow(), src_item->GetEntry());
        player->storage().put_item(src_item, dst, false);
        player->storage().on_item_added(
            src_item->GetEntry(), src_item->GetCount());
        src_item->db_save();
    }

    log_item(src.personal() ? GUILD_BANK_LOG_DEPOSIT_ITEM :
                              GUILD_BANK_LOG_WITHDRAW_ITEM,
        src.personal() ? dst.bag() : src.bag(), player->GetObjectGuid(),
        src_item->GetEntry(), src_item->GetCount(), 0);
    send_bank_list(player->GetSession(), src.personal() ? dst.bag() : src.bag(),
        false, true); // XXX: BroadcastEvent

    CharacterDatabase.CommitTransaction();
}

void inventory::guild_storage::do_player_stack(
    Player* player, inventory::slot dst, inventory::slot src, uint32 src_count)
{
    Item* src_item =
        src.personal() ?
            player->storage().get(src) :
            get(src); // Validity of slot & src_item verified previously
    Item* dst_item =
        src.personal() ? get(dst) : player->storage().get(
                                        dst); // Validity verified previously

    CharacterDatabase.BeginTransaction();
    player->storage().save(); // The storage must be at its latest revision when
                              // we begin poking the DB (cannot rollback from
                              // now on)

    dst_item->SetCount(dst_item->GetCount() + src_count);
    dst_item->db_save();

    if (src_item->GetCount() > src_count)
    {
        src_item->SetCount(src_item->GetCount() - src_count);
        src_item->db_save();
    }
    else
    {
        // src_item ran out of stacks and needs to be deleted
        if (src.personal())
        {
            CharacterDatabase.PExecute(
                "DELETE FROM character_inventory WHERE item = %u",
                src_item->GetGUIDLow());
            player->storage().pop_item(src_item, false);
            player->storage().on_item_removed(src_item->GetEntry());
        }
        else
        {
            CharacterDatabase.PExecute(
                "DELETE FROM guild_bank_item WHERE item_guid = %u",
                src_item->GetGUIDLow());
            src_item->RemoveFromWorld();
            items_[src.bag()][src.index()] = nullptr;
        }
        src_item->slot(slot());
        src_item->db_delete();
        delete src_item;
    }

    log_item(src.personal() ? GUILD_BANK_LOG_DEPOSIT_ITEM :
                              GUILD_BANK_LOG_WITHDRAW_ITEM,
        src.personal() ? dst.bag() : src.bag(), player->GetObjectGuid(),
        dst_item->GetEntry(), src_count,
        0); // Note: can't use src_item, might be deleted
    send_bank_list(player->GetSession(), src.personal() ? dst.bag() : src.bag(),
        false, true); // XXX: BroadcastEvent

    CharacterDatabase.CommitTransaction();
}

void inventory::guild_storage::do_player_partial_swap(
    Player* player, inventory::slot dst, inventory::slot src, uint32 src_count)
{
    assert(get(dst) == nullptr);
    Item* src_item =
        src.personal() ?
            player->storage().get(src) :
            get(src); // Validity of slot & src_item verified previously
    assert(src_count < src_item->GetCount());

    Item* new_item = Item::CreateItem(src_item->GetEntry(), src_count, nullptr,
        src_item->GetItemRandomPropertyId());
    if (!new_item)
        return player->SendEquipError(EQUIP_ERR_INT_BAG_ERROR, src_item);

    CharacterDatabase.BeginTransaction();
    player->storage().save(); // The storage must be at its latest revision when
                              // we begin poking the DB (cannot rollback from
                              // now on)

    src_item->SetCount(src_item->GetCount() - src_count);
    src_item->db_save();

    if (src.personal())
    {
        items_[dst.bag()][dst.index()] = new_item;
        CharacterDatabase.PExecute(
            "INSERT INTO guild_bank_item (guildid, TabId, SlotId, item_guid, "
            "item_entry) VALUES(%u, %u, %u, %u, %u)",
            guild_->GetId(), dst.bag(), dst.index(), new_item->GetGUIDLow(),
            new_item->GetEntry());
        new_item->slot(dst);
        new_item->AddToWorld();
        new_item->db_save();
    }
    else
    {
        CharacterDatabase.PExecute(
            "INSERT INTO character_inventory (guid, bag, `index`, item, "
            "item_template) VALUES(%u, %u, %u, %u, %u)",
            player->GetGUIDLow(), dst.bag(), dst.index(),
            new_item->GetGUIDLow(), new_item->GetEntry());
        player->storage().put_item(new_item, dst, false);
        player->storage().on_item_added(
            new_item->GetEntry(), new_item->GetCount());
        new_item->db_save();
    }

    log_item(src.personal() ? GUILD_BANK_LOG_DEPOSIT_ITEM :
                              GUILD_BANK_LOG_WITHDRAW_ITEM,
        src.personal() ? dst.bag() : src.bag(), player->GetObjectGuid(),
        src_item->GetEntry(), src_count, 0);
    send_bank_list(player->GetSession(), src.personal() ? dst.bag() : src.bag(),
        false, true); // XXX: BroadcastEvent

    CharacterDatabase.CommitTransaction();
}

void inventory::guild_storage::on_disband()
{
    for (auto& elem : items_)
    {
        std::vector<Item*>& items = elem.second;
        for (auto& item : items)
        {
            if (!item)
                continue;
            item->RemoveFromWorld();
            item->db_delete();
            delete item;
            item = nullptr;
        }
    }

    items_.clear();

    CharacterDatabase.PExecute(
        "DELETE FROM guild_bank_eventlog WHERE guildid = %u", guild_->GetId());
    CharacterDatabase.PExecute(
        "DELETE FROM guild_bank_item WHERE guildid = %u", guild_->GetId());
    CharacterDatabase.PExecute(
        "DELETE FROM guild_bank_right WHERE guildid = %u", guild_->GetId());
    CharacterDatabase.PExecute(
        "DELETE FROM guild_bank_tab WHERE guildid = %u", guild_->GetId());
}
