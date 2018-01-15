#ifndef MANGOS__GAME__INVENTORY__GUILD_STORAGE_H
#define MANGOS__GAME__INVENTORY__GUILD_STORAGE_H

#include "Common.h"
#include "ObjectGuid.h"
#include "SharedDefines.h"
#include "inventory/storage.h"
#include <list>
#include <string>
#include <vector>

class Guild;
class Player;
class WorldPacket;
class WorldSession;

// Limit for money & per tab-logging
#define GUILD_BANK_LOG_LIMIT 25

namespace inventory
{
class guild_storage : public storage
{
public:
    struct tab_description
    {
        std::string name;
        std::string icon;
        std::string description;
    };
    struct withdrawal_data
    {
        withdrawal_data() : money(0) { memset(&items, 0, sizeof(items)); }
        uint32 money;
        uint32 items[GUILD_BANK_MAX_TABS];
    };

    guild_storage(Guild* guild) : guild_(guild), tab_count_(0) {}
    ~guild_storage();

    void load(uint8 owned_tab_count, copper gold);

    bool withdraw_money(Player* player, copper c);
    bool deposit_money(Player* player, copper c);
    void swap(Player* player, inventory::slot dst, inventory::slot src,
        uint32 src_split = 0);

    // Send & Display functions
    void send_bank_list(WorldSession* session, uint8 tab_id, bool send_tab_info,
        bool send_content) const;
    void display_log(WorldSession* session, uint8 tab_id) const;
    void send_remaining_gold_withdrawal(Player* player) const;
    // if session == NULL it's broadcasted to everyone in the guild; escapes
    // tab_id
    void send_tab_description(WorldSession* session, uint8 tab_id) const;

    // User Actions (all of these functions deal with unverified user-input)
    void attempt_purchase_tab(Player* player, uint8 tab_id);
    void attempt_update_tab_name(Player* player, uint8 tab_id,
        const std::string& name, const std::string& icon);
    void attempt_set_tab_description(
        Player* player, uint8 tab_id, const std::string& description);
    void attempt_repair(Player* player, copper cost);

    uint8 tab_count() const { return tab_count_; }

    void reset_withdrawal_limits(bool db);
    void set_withdrawal_limits(ObjectGuid player, const withdrawal_data& data)
    {
        withdrawal_map_[player] = data;
    }
    void erase_withdrawal_limits(ObjectGuid player)
    {
        auto itr = withdrawal_map_.find(player);
        if (itr != withdrawal_map_.end())
            withdrawal_map_.erase(itr);
    }

    // Public helper functions for permission checking
    uint32 remaining_money(ObjectGuid guid, bool repair = false) const;
    uint32 remaining_withdrawals(ObjectGuid guid, uint8 tab_id) const;

    void on_disband();

private:
    struct bank_log_entry
    {
        uint8 event_type;
        uint32 guid;
        time_t timestamp;
    };
    struct gold_log_entry : bank_log_entry
    {
        uint32 amount;
    };
    struct item_log_entry : bank_log_entry
    {
        uint32 item_id;
        uint32 stack_count;
        uint32 dest_tab;
    };

    void log_gold(uint8 etype, ObjectGuid player, uint32 amount);
    void log_item(uint8 etype, uint8 tab_id, ObjectGuid player, uint32 item_id,
        uint32 stack_count, uint32 dest_tab);

    void load_tabs(uint8 owned_tab_count);
    void load_logs();

    InventoryResult can_store(slot dst, Item* item, Player* player,
        bool skip_unique_check) const override;
    slot_type get_slot_type() const override { return guild_slot; }

    void display_append_slot(WorldPacket& data, slot s) const;

    // Helper functions for permission checking
    bool has_tab_rights(Player* player, uint8 tab_id, uint32 flag) const;
    bool has_rank_rights(Player* player, uint32 flag) const;

    // Helper functions for swap()
    void do_player_swap(
        Player* player, inventory::slot dst, inventory::slot src);
    void do_player_stack(Player* player, inventory::slot dst,
        inventory::slot src, uint32 src_count);
    // dst must not contain an item for do_player_partial_swap(), use
    // do_player_stack() for that
    void do_player_partial_swap(Player* player, inventory::slot dst,
        inventory::slot src, uint32 src_count);
    void player_to_bank(Player* player, inventory::slot dst,
        inventory::slot src, uint32 src_split);
    void bank_to_player(Player* player, inventory::slot dst,
        inventory::slot src, uint32 src_split);
    void player_to_bank_auto(Player* player, uint8 tab_id, inventory::slot src);
    void internal_swap(Player* player, inventory::slot dst, inventory::slot src,
        uint32 src_split);

    Guild* guild_;
    uint8 tab_count_;
    std::vector<tab_description> descriptions_; // Aligns with indices in
                                                // items_, i.e.: items_[i] =>
                                                // descriptions_[i]

    typedef std::map<ObjectGuid /*player*/, withdrawal_data> withdrawal_map;
    withdrawal_map withdrawal_map_;

    std::list<gold_log_entry>
        gold_log_; // Contains up to 25 elements, ordered by oldest first
    std::list<item_log_entry> tab_logs_[GUILD_BANK_MAX_TABS]; // Each tab
                                                              // contains up to
                                                              // 25 elements,
                                                              // ordered by
                                                              // oldest first
};

} // namespace inventory

#endif // MANGOS__GAME__INVENTORY__GUILD_STORAGE_H
