#ifndef GAME__LOOT_DISTRIBUTOR_H
#define GAME__LOOT_DISTRIBUTOR_H

#include "GameObject.h"
#include "LootMgr.h"
#include "loot_selection.h"
#include "ObjectGuid.h"
#include "ObjectMgr.h"
#include "Player.h"
#include <map>
#include <set>
#include <unordered_map>
#include <vector>

class Group;
struct PlayerLootSlotData;

class Roll
{
public:
    enum roll_type
    {
        // Client recognized types
        ROLL_PASS = 0,
        ROLL_NEED = 1,
        ROLL_GREED = 2,

        MAX_ROLL_FROM_CLIENT = 3,

        // Internal types
        ROLL_PENDING
    };

private:
    struct individual_roll_data
    {
        individual_roll_data()
          : roll_type_(ROLL_PENDING), roll_result_(0), auto_passing_(false)
        {
        }

        roll_type roll_type_;
        uint32 roll_result_;
        bool auto_passing_;
    };

public:
    typedef std::unordered_map<ObjectGuid, individual_roll_data> roll_map;

    Roll(uint32 loot_slot, LootItem* item)
      : loot_slot_(loot_slot), item_(item), finished_(false),
        pending_roll_count_(0)
    {
    }

    // Calculate everyones' rolls and decided the winner
    void end_roll();
    ObjectGuid winner() const { return winner_; }
    uint32 loot_slot() const { return loot_slot_; }
    uint32 players_roll(ObjectGuid looter) const;
    roll_type players_roll_type(ObjectGuid looter) const;

    // Returns true if roll should be sent to clients
    bool place_players_roll(ObjectGuid looter, roll_type roll);

    void add_roller(ObjectGuid player);
    void add_auto_passer(ObjectGuid player);
    bool has_roller(ObjectGuid player) const
    {
        return rolls_.find(player) != rolls_.end();
    }
    LootItem* item() { return item_; }

    bool finished() const { return finished_; }
    bool all_rolls_placed() const { return pending_roll_count_ == 0; }

    roll_map::const_iterator begin() const { return rolls_.begin(); }
    roll_map::const_iterator end() const { return rolls_.end(); }

    const std::vector<ObjectGuid>& send_to() const { return send_to_; }

private:
    roll_map rolls_;
    uint32 loot_slot_;
    ObjectGuid winner_;
    LootItem* item_;
    bool finished_;
    uint32 pending_roll_count_;

    std::vector<ObjectGuid> send_to_;
};

class loot_recipient_mgr
{
public:
    explicit loot_recipient_mgr(Object* owner)
      : owner_(owner), tapping_group_id_(0)
    {
    }

    void attempt_add_tap(Player* player);
    void add_solo_tap(Player* player)
    {
        loot_recipients_.insert(player->GetObjectGuid());
    }
    bool has_tap(const Player* player) const
    {
        return loot_recipients_.find(player->GetObjectGuid()) !=
               loot_recipients_.end();
    }
    std::set<ObjectGuid>* taps() { return &loot_recipients_; }
    Group* group() const;
    // Note: Not only is this a cheaper check than group(), but it also keeps
    // returning
    // true even if the group is disbanded (which is most of the time the
    // behavior we want)
    bool party_loot() const { return tapping_group_id_ != 0; }

    Player* first_valid_player() const;

    // Call on Evade or similar circumstances
    void reset();
    bool empty() const { return loot_recipients_.empty(); }

private:
    Object* owner_;
    uint32 tapping_group_id_;
    std::set<ObjectGuid> loot_recipients_;

    // Adds only one player
    void add_tap(Player* player);
    // Adds player + everyone in his group that is close enough to receive loot
    void add_first_tap(Player* player);
};

// loot_distributor creates and manage a loot and then makes sure the right
// functions
// are used to distribute this loot in the best possible manner. This class
// exists
// one per drop and is created when a creature spawns or a chest is opened (it
// is owned by that object too).
class loot_distributor
{
public:
    // Lootee: unit, chest, etc that will get (or is getting) looted
    loot_distributor(Object* lootee, LootType loot_type);
    ~loot_distributor();

    // LootOwner cannot be null for fishing game objects or items (or no loot
    // will be generated)
    void generate_loot(Player* loot_owner = nullptr);
    bool display_loot(Player* player);
    void close_loot_display(Player* player);

    Object* lootee() { return lootee_; }
    loot_recipient_mgr* recipient_mgr() { return &recipient_mgr_; }
    const Loot* loot() const { return loot_; }
    LootType loot_type() const { return loot_type_; }
    Player* loot_owner() const;

    void update_rolls(const uint32 diff)
    {
        if (loot_ && !pending_rolls_.empty())
            _update_rolls(diff);
    }

    // Interrupt a loot session prematurely
    void cancel_loot_session();

    // Player actions
    void attempt_master_loot_handout(
        Player* master_looter, Player* target, uint8 loot_slot);
    void attempt_loot_money(Player* looter);
    void attempt_loot_item(Player* looter, uint8 loot_slot);
    void attempt_place_roll(Player* roller, uint8 loot_slot, uint8 roll_type);

    // Forcing attributes (used for Items loaded from DB)
    void force_gold(uint32 gold);
    void force_add_item(
        const LootItem& item); // Will interrupt all current looters (if any)
    void force_loot_owner(ObjectGuid guid) { loot_owner_ = guid; }

    void auto_store_all_loot(Player* player);

    bool can_view_loot(const Player* looter) const;

    uint32 loot_viewers_count() const
    {
        if (loot_)
            return loot_->looters_count();
        return 0;
    }

    void set_dungeon_loot(bool set) { dungeon_loot_ = set; }

    /*
     * LOOT SESSION
     *   A loot session generally has the order these functions appear in:
     */
    // Begin loot session, happens when a creature dies or an object is opened
    void start_loot_session();

private: // Only we should continue the chain of looting. The entry point for
         // any outsider is StartLootSession()
    // Select those that can loot and "make it sparkle" for them (only for dead
    // creatures)
    void select_and_notify_looters();
    // DisplayLoot() called by someone who interacts with us
    // Start rolls if any of the items should be rolled for
    void start_rolls();
    // All Rolls get placed / UpdateRolls() times out
    // Finish the rolls and award the loot
    void finish_rolls();
    // Successfully ends a loot session (Note: do not call until all rolled for
    // items have been successfully looted)
    void end_loot_session();

    inline bool rollable_loot_type() const;
    inline bool anyone_can_view_loot() const;
    inline bool owner_only_loot() const;
    inline bool needs_loot_owner() const;
    // Checks item count and sees if we can loot more of that item
    inline bool uniqueness_check(Player* looter, LootItem* item) const;
    bool within_loot_dist(Player* looter) const;

    LootSlotType players_loot_slot(
        Player* looter, LootItem* item, bool quest_item) const;

    // Group settings stay as they were when the drop happened:
    LootMethod
        loot_method_; // Group Loot Method as it was at the time of the drop
    ItemQualities loot_threshold_; // Loot Quality Threshold as it was at the
                                   // time of the drop
    ObjectGuid master_looter_;     // Master Looter at the time of the drop

    ObjectGuid loot_owner_; // The player that owns the loot (can be null if
                            // loot is not owned by anyone in particular; for
                            // some loot types owner is completely irrelevant,
                            // as well)
    Object* lootee_; // Object that's getting looted (since we're owned by this
                     // object there's no danger in saving this pointer)
    ObjectGuid lootee_guid_; // Object's guid that's getting looted

    Loot* loot_;                    // The loot we're distributing
    LootType loot_type_;            // Type of loot (like corpse, fishing, etc)
    std::set<LootItem*> ffa_items_; // Items that can be looted by anyone who
                                    // has a tap (e.g., items below threshold
                                    // that has been checked by
                                    // chosen_group_looter_ if using group
                                    // looting rules)
    bool loot_checked_by_chosen_one_; // If loot was checked out by the picked
                                      // looter but there's only silver
                                      // remaining, this will let us know to
                                      // show the loot to everyone else

    std::set<ObjectGuid> acceptable_loot_viewers_; // Players who are allowed to
                                                   // open the loot window of
                                                   // our loot
    loot_recipient_mgr recipient_mgr_;             // A list of whom can loot us

    ObjectGuid chosen_group_looter_; // He who got the loot assigned to him; not
                                     // valid for all loot methods or loot types

    bool rolls_started_;      // True if rolls for items have been started
    bool has_rollable_items_; // True if we have items we can roll for
    std::vector<Roll*>
        pending_rolls_; // Rolls we have not given out the loot for yet
    uint32 roll_timer_; // Time remaining until timeout

    bool dungeon_loot_; // Chests looted in dungeons or raids need looters to be
                        // on recipient list

    void apply_loot_selection(loot_selection_type type);
    void generate_GO_loot(Player* lootOwner);
    void generate_item_loot(Player* lootOwner);
    void generate_corpse_loot(Player* lootOwner);
    void generate_creature_loot(Player* lootOwner);
    bool display_GO_loot(Player* player);
    bool display_item_loot(Player* player);
    bool display_corpse_loot(Player* player);
    bool display_creature_loot(Player* player);

    void complete_roll(Roll* roll);
    Roll* get_roll_for_item(const LootItem* item) const;

    // Network Code Related To Looting
    void build_loot_view(Player* looter, WorldPacket& packet,
        std::set<uint8>& availalbe_loot_indices);
    void send_master_looting_list();

    WorldPacket build_roll_start_packet(const Roll& r) const;
    void send_roll_all_passed(const Roll& r) const;
    void send_player_roll(
        const Roll& r, ObjectGuid roller, bool auto_pass = false) const;
    void send_roll_won(const Roll& r) const;

    void _update_rolls(const uint32 diff);

    // Internal helper to split up function
    LootSlotType _players_loot_slot_different_modes(
        Player* looter, LootItem* item, bool quest_item) const;
};

inline bool loot_distributor::rollable_loot_type() const
{
    return (lootee_guid_.IsCreature() && loot_type_ == LOOT_CORPSE) ||
           (lootee_guid_.IsGameObject() &&
               ((GameObject*)lootee_)->GetGoType() == GAMEOBJECT_TYPE_CHEST &&
               static_cast<GameObject*>(lootee_)
                   ->GetGOInfo()
                   ->chest.groupLootRules); // go_type_chest includes mines and
                                            // herbs, so we must make sure that
                                            // this item has group loot rules
}

inline bool loot_distributor::anyone_can_view_loot() const
{
    if (loot_type_ == LOOT_PICKPOCKETING || loot_type_ == LOOT_INSIGNIA)
        return true;
    // NOTE: Cheat also includes mines and herbs
    if (lootee_guid_.IsGameObject() &&
        static_cast<GameObject*>(lootee_)->GetGoType() == GAMEOBJECT_TYPE_CHEST)
    {
        // If the chest was looted in a dungeon or raid, the chest requires
        // people looting to be part of the recipients
        if (static_cast<GameObject*>(lootee_)
                ->GetGOInfo()
                ->chest.groupLootRules &&
            dungeon_loot_)
            return false;
        return true; // Otherwise, anyone can view the loot
    }
    return false;
}

inline bool loot_distributor::owner_only_loot() const
{
    return loot_type_ == LOOT_DISENCHANTING || loot_type_ == LOOT_FISHING ||
           loot_type_ == LOOT_FISHINGHOLE || loot_type_ == LOOT_FISHING_FAIL ||
           loot_type_ == LOOT_PROSPECTING || lootee_guid_.IsItem();
}

inline bool loot_distributor::needs_loot_owner() const
{
    return lootee_guid_.IsItem() || loot_type_ == LOOT_FISHING ||
           loot_type_ == LOOT_FISHINGHOLE || loot_type_ == LOOT_FISHING_FAIL;
}

inline bool loot_distributor::uniqueness_check(
    Player* looter, LootItem* item) const
{
    const ItemPrototype* item_proto = ObjectMgr::GetItemPrototype(item->itemid);
    if (!item_proto)
        return false;
    // Check uniqueness
    if (item_proto->MaxCount > 0)
        if (looter->storage().item_count(item_proto->ItemId) >=
            item_proto->MaxCount)
            return false;
    return true;
}

#endif
