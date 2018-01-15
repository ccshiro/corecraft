#ifndef MANGOS__GAME__INVENTORY__TRADE_H
#define MANGOS__GAME__INVENTORY__TRADE_H

#include "Common.h"
#include "ObjectGuid.h"
#include "inventory/slot.h"

class Item;
class Player;
class Spell;
namespace inventory
{
class transaction;
};

#define MAX_TRADE_SLOTS 6

namespace inventory
{
// There's two sides to each trade, one for each player
struct trade_side
{
    trade_side() : accepted(false), money(0), trade_spell(0) {}

    bool accepted;
    uint32 money;
    ObjectGuid items[MAX_TRADE_SLOTS];
    ObjectGuid non_traded; // Client refers to this as MAX_TRADE_SLOTS
    uint32 trade_spell;    // Spell to be cast on the non-traded slot
    ObjectGuid cast_item;  // Item that is used for the non-traded cast (not the
                           // target, but the caster item)
};

class trade
{
public:
    trade(Player* player_one, Player* player_two);
    // Clears both players' trade and deletes itself
    void cancel();

    /* Getters */
    Player* player_one() const { return players_[0]; }
    Player* player_two() const { return players_[1]; }
    bool finished() const { return finished_; }
    // Returns the item in the OTHER player's non-traded slot, as that is the
    // spell target for our player
    Item* spell_target(Player* caster) const;

    void set_spell(Player* caster, ObjectGuid cast_item, uint32 spell_id);

    /* All these actions are player-actions, and are being verified */
    void set_gold(Player* player, uint32 gold);
    void put_item(Player* player, uint8 trade_slot, slot src_slot);
    void pop_item(Player* player, uint8 trade_slot);
    void accept(Player* player);
    void unaccept(Player* player);

    /* Callbacks */
    void on_relocation(Player* player);

private:
    int player_index(Player* player) const;
    int other_index(int index) const { return index == 1 ? 0 : 1; }
    // When player A or B has their trade side changed, we send the update
    // to both Player A and B. send_trade does this, we just specify index of
    // who changed
    void send_trade(int index) const;
    void send_trade_for(int target_index, int index) const;

    // Goes through with the trade, called when both parties have accepted
    void finalize();
    // Helpers for finalize()
    void spell_reagents(
        Spell* spell, transaction& trans); // Add spell reagents to transaction
    // Calls Item::set_in_trade(false) for all items in our trade. Used by
    // cancel() and finalize()
    void clear_all_items();

    bool finished_;
    Player* players_[2];
    trade_side trade_sides_[2];
};
}

#endif // MANGOS__GAME__INVENTORY__TRADE_H
