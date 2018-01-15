#include "inventory/trade.h"
#include "Item.h"
#include "Player.h"
#include "Spell.h"
#include "Database/DatabaseEnv.h"
#include <memory>

inventory::trade::trade(Player* player_one, Player* player_two)
  : finished_(false)
{
    LOG_DEBUG(logging, "Created Trade between %s and %s", player_one->GetName(),
        player_two->GetName());
    players_[0] = player_one;
    players_[1] = player_two;
    player_one->trade(this);
    player_two->trade(this);
}

int inventory::trade::player_index(Player* player) const
{
    if (players_[0] == player)
        return 0;
    else if (players_[1] == player)
        return 1;
    else
        return -1;
}

Item* inventory::trade::spell_target(Player* caster) const
{
    // This returns the item in the OTHER player's non-traded slot, as that is
    // the spell target for our player
    int index = player_index(caster);
    if (index == -1)
        return nullptr;

    if (!trade_sides_[other_index(index)].non_traded)
        return nullptr;

    return players_[other_index(index)]->GetItemByGuid(
        trade_sides_[other_index(index)].non_traded);
}

void inventory::trade::set_spell(
    Player* caster, ObjectGuid cast_item, uint32 spell_id)
{
    int index = player_index(caster);
    if (index == -1)
        return;

    trade_sides_[other_index(index)].trade_spell = spell_id;
    trade_sides_[other_index(index)].cast_item = cast_item;

    // Clear acceptance of players when trade-content changes
    trade_sides_[0].accepted = false;
    trade_sides_[1].accepted = false;

    send_trade(other_index(index));
}

void inventory::trade::cancel()
{
    LOG_DEBUG(logging, "Trade Cancelled");

    clear_all_items();

    players_[0]->trade(nullptr);
    players_[1]->trade(nullptr);

    // Close trade window for players
    if (!players_[0]->GetSession()->PlayerRecentlyLoggedOut())
        players_[0]->GetSession()->SendTradeStatus(
            TRADE_STATUS_TRADE_CANCELLED);
    if (!players_[1]->GetSession()->PlayerRecentlyLoggedOut())
        players_[1]->GetSession()->SendTradeStatus(
            TRADE_STATUS_TRADE_CANCELLED);

    delete this;
}

void inventory::trade::set_gold(Player* player, uint32 gold)
{
    int index = player_index(player);
    if (index == -1)
        return;

    inventory::copper c(gold);
    if (c.spill() || players_[index]->storage().money().get() < c.get())
        return;

    trade_sides_[index].money = gold;

    // Clear acceptance of players when trade-content changes
    trade_sides_[0].accepted = false;
    trade_sides_[1].accepted = false;

    send_trade(index);
}

void inventory::trade::put_item(Player* player, uint8 trade_slot, slot src_slot)
{
    int index = player_index(player);
    if (index == -1)
        return;

    if (trade_slot > MAX_TRADE_SLOTS)
        return;

    // Check that player has an item in that slot
    Item* item = players_[index]->storage().get(src_slot);
    if (!item)
        return;
    // Check so the item is tradeable at all
    if (trade_slot != MAX_TRADE_SLOTS && !item->can_put_in_trade())
        return;
    // Make sure it's not already in the trade window
    for (int i = 0; i < MAX_TRADE_SLOTS; ++i)
        if (trade_sides_[index].items[i] == item->GetObjectGuid())
            return;
    if (trade_sides_[index].non_traded == item->GetObjectGuid())
        return;

    // Checks passed, put the item in the slot
    if (trade_slot == MAX_TRADE_SLOTS)
        trade_sides_[index].non_traded = item->GetObjectGuid();
    else
        trade_sides_[index].items[trade_slot] = item->GetObjectGuid();

    item->set_in_trade(true);

    // Clear acceptance of players when trade-content changes
    trade_sides_[0].accepted = false;
    trade_sides_[1].accepted = false;

    send_trade(index);
}

void inventory::trade::pop_item(Player* player, uint8 trade_slot)
{
    int index = player_index(player);
    if (index == -1)
        return;

    ObjectGuid guid;

    if (trade_slot > MAX_TRADE_SLOTS)
        return;

    if (trade_slot == MAX_TRADE_SLOTS)
    {
        if (!trade_sides_[index].non_traded)
            return;
        guid = trade_sides_[index].non_traded;
        trade_sides_[index].non_traded = ObjectGuid();
    }
    else
    {
        if (!trade_sides_[index].items[trade_slot])
            return;
        guid = trade_sides_[index].items[trade_slot];
        trade_sides_[index].items[trade_slot] = ObjectGuid();
    }

    if (Item* item = players_[index]->GetItemByGuid(guid, false))
        item->set_in_trade(false);

    // Clear acceptance of players when trade-content changes
    trade_sides_[0].accepted = false;
    trade_sides_[1].accepted = false;

    send_trade(index);
}

void inventory::trade::accept(Player* player)
{
    int index = player_index(player);
    if (index == -1)
        return;

    trade_sides_[index].accepted = true;
    players_[other_index(index)]->GetSession()->SendTradeStatus(
        TRADE_STATUS_TRADE_ACCEPT);

    if (trade_sides_[0].accepted && trade_sides_[1].accepted)
        finalize();
}

void inventory::trade::unaccept(Player* player)
{
    int index = player_index(player);
    if (index == -1)
        return;

    trade_sides_[index].accepted = false;
    players_[other_index(index)]->GetSession()->SendTradeStatus(
        TRADE_STATUS_BACK_TO_TRADE);
}

void inventory::trade::on_relocation(Player* player)
{
    int index = player_index(player);
    if (index == -1)
        return;

    if (!players_[index]->IsWithinDistInMap(
            players_[index == 1 ? 0 : 1], 10.0f, true))
        cancel();
}

void inventory::trade::send_trade(int index) const
{
    // We need to refresh both trade windows for P1 & P2 for everything to show
    // up correctly
    // (everything can show up by sending just P1 to P2 and P2 to P1, however
    // removing
    // of accept won't show up properly if we do that; i.e. it will still be
    // green)
    send_trade_for(0, index);
    send_trade_for(1, index);
    send_trade_for(0, other_index(index));
    send_trade_for(1, other_index(index));
}

void inventory::trade::finalize()
{
    LOG_DEBUG(logging, "=== Finalizing Trade ===");
    finished_ =
        true; // Set before so spells will be executed when we trigger them

    // trade_sides[0] goes to player two, and trade_sides_[1] goes to player one
    // We keep one remove transaction and one add transaction to be able to
    // remove all before we add anything back
    inventory::transaction player_one_remove, player_two_remove, player_one_add,
        player_two_add;

    // Transferal of items
    for (int i = 0; i < MAX_TRADE_SLOTS; ++i)
    {
        Item* item;
        if (trade_sides_[0].items[i])
        {
            item = players_[0]->GetItemByGuid(trade_sides_[0].items[i], false);
            if (!item)
                return cancel(); // Something went really wrong

            player_two_add.add(item);
            player_one_remove.remove(item);
            LOG_DEBUG(logging, "P1 -> P2: %dx[%s]", item->GetCount(),
                item->GetProto()->Name1);
        }
        if (trade_sides_[1].items[i])
        {
            item = players_[1]->GetItemByGuid(trade_sides_[1].items[i], false);
            if (!item)
                return cancel(); // Something went really wrong

            player_one_add.add(item);
            player_two_remove.remove(item);
            LOG_DEBUG(logging, "P2 -> P1: %dx[%s]", item->GetCount(),
                item->GetProto()->Name1);
        }
    }

    // Transferal of money
    if (trade_sides_[0].money > 0)
    {
        player_two_add.add(trade_sides_[0].money);
        player_one_remove.remove(trade_sides_[0].money);
        LOG_DEBUG(logging, "P1 -> P2: %s",
            inventory::copper(trade_sides_[0].money).str().c_str());
    }
    if (trade_sides_[1].money > 0)
    {
        player_one_add.add(trade_sides_[1].money);
        player_two_remove.remove(trade_sides_[1].money);
        LOG_DEBUG(logging, "P2 -> P1: %s",
            inventory::copper(trade_sides_[1].money).str().c_str());
    }

    // Casting of spells on non-traded items
    std::unique_ptr<Spell> spell_pone;
    std::unique_ptr<Spell> spell_ptwo;
    SpellCastTargets targets_pone, targets_ptwo;
    if (trade_sides_[0].non_traded &&
        trade_sides_[0].trade_spell) // spell cast by player two (spell_ptwo)
    {
        const SpellEntry* info =
            sSpellStore.LookupEntry(trade_sides_[0].trade_spell);
        Item* item =
            players_[0]->GetItemByGuid(trade_sides_[0].non_traded, false);
        // Cast item is found in the caster's bag
        Item* cast_item =
            players_[1]->GetItemByGuid(trade_sides_[0].cast_item, false);
        if (!info || !item || (trade_sides_[0].cast_item && !cast_item))
            return cancel(); // Something went really wrong

        spell_ptwo.reset(new Spell(players_[1], info, true));
        if (cast_item)
            spell_ptwo->set_cast_item(cast_item);
        targets_ptwo.setTradeItemTarget(players_[1]);
        spell_ptwo->m_targets = targets_ptwo;

        SpellCastResult res = spell_ptwo->CheckCast(true);
        if (res != SPELL_CAST_OK)
            return cancel();

        spell_reagents(spell_ptwo.get(), player_two_remove);
    }
    if (trade_sides_[1].non_traded &&
        trade_sides_[1].trade_spell) // spell cast by player one (spell_pone)
    {
        const SpellEntry* info =
            sSpellStore.LookupEntry(trade_sides_[1].trade_spell);
        Item* item =
            players_[1]->GetItemByGuid(trade_sides_[1].non_traded, false);
        // Cast item is found in the caster's bag
        Item* cast_item =
            players_[0]->GetItemByGuid(trade_sides_[1].cast_item, false);
        if (!info || !item || (trade_sides_[1].cast_item && !cast_item))
            return cancel(); // Something went really wrong

        SpellCastTargets targets;
        spell_pone.reset(new Spell(players_[0], info, true));
        if (cast_item)
            spell_pone->set_cast_item(cast_item);
        targets_pone.setTradeItemTarget(players_[0]);
        spell_pone->m_targets = targets_pone;

        SpellCastResult res = spell_pone->CheckCast(true);
        LOG_DEBUG(logging,
            "P1 casts %s on non-traded of P2 (Id: %d) | CheckCast Result: %d",
            info->SpellName[0], info->Id, res);
        if (res != SPELL_CAST_OK)
            return cancel();

        spell_reagents(spell_pone.get(), player_one_remove);
    }

    // Check if trade is completely empty
    if (player_one_remove.empty() && player_one_add.empty() &&
        player_two_remove.empty() && player_two_add.empty() && !spell_pone &&
        !spell_ptwo)
        return cancel();

    // We must clear all items from being in trade before we verify the
    // operations, as the removes will fail otherwise (can't remove an item in
    // trade)
    clear_all_items();

    // Verify remove transactions
    if (!player_one_remove.empty() &&
        !players_[0]->storage().verify(player_one_remove))
    {
        LOG_DEBUG(logging, ">> Failed removing items from P1. (Error: %d)",
            player_one_remove.error());
        players_[0]->SendEquipError(
            static_cast<InventoryResult>(player_one_remove.error()), nullptr);
        return cancel();
    }
    if (!player_two_remove.empty() &&
        !players_[1]->storage().verify(player_two_remove))
    {
        LOG_DEBUG(logging, ">> Failed removing items from P2. (Error: %d)",
            player_two_remove.error());
        players_[1]->SendEquipError(
            static_cast<InventoryResult>(player_two_remove.error()), nullptr);
        return cancel();
    }

    // Verify add transaction
    if (!player_one_add.empty() &&
        !players_[0]->storage().verify(player_one_add))
    {
        LOG_DEBUG(logging, ">> Failed adding items to P1. (Error: %d)",
            player_one_add.error());
        players_[0]->SendEquipError(
            static_cast<InventoryResult>(player_one_add.error()), nullptr);
        return cancel();
    }
    if (!player_two_add.empty() &&
        !players_[1]->storage().verify(player_two_add))
    {
        LOG_DEBUG(logging, ">> Failed adding items to P2. (Error: %d)",
            player_two_add.error());
        players_[1]->SendEquipError(
            static_cast<InventoryResult>(player_two_add.error()), nullptr);
        return cancel();
    }

    // Every check passed, we can now carry out the effects

    // Then we carry out spells. They must be the first action so that the cast
    // item is still not destroyed
    if (spell_pone)
    {
        Spell* spell = spell_pone.release();
        spell->prepare(&targets_pone);
        // NULL out m_CastItem of Spell as it has served its purpose and will
        // potentially be destroyed momentarily
        if (auto item = spell->get_cast_item())
        {
            // Pull a charge if item is not in the removal of player one
            spell->ClearCastItem();
            if (!player_one_remove.has(item))
                if (item->DropSpellCharge())
                    players_[0]->storage().remove_count(item, 1);
        }
    }
    if (spell_ptwo)
    {
        Spell* spell = spell_ptwo.release();
        spell->prepare(&targets_ptwo);
        // NULL out m_CastItem of Spell as it has served its purpose and will
        // potentially be destroyed momentarily
        if (auto item = spell->get_cast_item())
        {
            // Pull a charge if item is not in the removal of player two
            spell->ClearCastItem();
            if (!player_two_remove.has(item))
                if (item->DropSpellCharge())
                    players_[1]->storage().remove_count(item, 1);
        }
    }

    // Carry out removals
    if (!player_one_remove.empty())
        players_[0]->storage().finalize(player_one_remove);
    if (!player_two_remove.empty())
        players_[1]->storage().finalize(player_two_remove);

    // Save removals to database, need to be carried out before adds, or inserts
    // for player two may fail.
    // NOTE: We use one single SQL transaction
    CharacterDatabase.BeginTransaction();
    players_[0]->SaveInventoryAndGoldToDB();
    players_[1]->SaveInventoryAndGoldToDB();

    // And lastly, the additions
    if (!player_one_add.empty())
        players_[0]->storage().finalize(player_one_add);
    if (!player_two_add.empty())
        players_[1]->storage().finalize(player_two_add);

    // Save the adds to database (same transaction as removals)
    players_[0]->SaveInventoryAndGoldToDB();
    players_[1]->SaveInventoryAndGoldToDB();
    CharacterDatabase.CommitTransaction();

    players_[0]->GetSession()->SendTradeStatus(TRADE_STATUS_TRADE_COMPLETE);
    players_[1]->GetSession()->SendTradeStatus(TRADE_STATUS_TRADE_COMPLETE);

    players_[0]->trade(nullptr);
    players_[1]->trade(nullptr);

    LOG_DEBUG(logging, ">> Trade Completed Successfully");

    delete this;
}

void inventory::trade::spell_reagents(Spell* spell, transaction& trans)
{
    // Consider if cast item will be destroyed by our spell casting or not
    if (auto item = spell->get_cast_item())
    {
        for (int i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
        {
            // SpellCharges < 0 means the item will be destroyed upon no more
            // charges
            if (item->GetProto()->Spells[i].SpellCharges < 0)
            {
                if (std::abs(item->GetSpellCharges(i)) <= 1)
                {
                    if (item->GetCount() == 1)
                    {
                        trans.destroy(item);
                        LOG_DEBUG(logging,
                            "Spell will destroy cast item with Id %u if the "
                            "trade succeeds",
                            item->GetEntry());
                    }
                    break;
                }
            }
        }
    }

    // Mark all reagents of the spell as to be consumed
    // NOTE: spell is triggered, so Spell::TakeReagents will not do anything
    for (int i = 0; i < MAX_SPELL_REAGENTS; ++i)
    {
        if (spell->m_spellInfo->Reagent[i] <= 0)
            continue;

        trans.destroy(spell->m_spellInfo->Reagent[i],
            spell->m_spellInfo->ReagentCount[i]);
        LOG_DEBUG(logging,
            "Spell will destroy reagent with Id %u and Count: %u if the trade "
            "succeeds",
            spell->m_spellInfo->Reagent[i],
            spell->m_spellInfo->ReagentCount[i]);
    }
}

void inventory::trade::clear_all_items()
{
    for (int i = 0; i < MAX_TRADE_SLOTS; ++i)
    {
        if (Item* item =
                players_[0]->GetItemByGuid(trade_sides_[0].items[i], false))
            item->set_in_trade(false);
        if (Item* item =
                players_[1]->GetItemByGuid(trade_sides_[1].items[i], false))
            item->set_in_trade(false);
    }

    if (Item* item =
            players_[0]->GetItemByGuid(trade_sides_[0].non_traded, false))
        item->set_in_trade(false);
    if (Item* item =
            players_[1]->GetItemByGuid(trade_sides_[1].non_traded, false))
        item->set_in_trade(false);
}

void inventory::trade::send_trade_for(int target_index, int index) const
{
    WorldPacket data(SMSG_TRADE_STATUS_EXTENDED, 256);

    data << uint8(target_index != index); // True: Sending trade-window of other
                                          // party; False: Sending our own
                                          // trade-window
    data << uint32(0); // Some form of client-expected index. Whatever is sent
                       // in TRADE_STATUS_OPEN_WINDOW must also be sent here.
                       // (XXX)
    data << uint32(MAX_TRADE_SLOTS + 1); // Unknown
    data << uint32(MAX_TRADE_SLOTS + 1); // Unknown. Why twice?
    data << uint32(trade_sides_[index].money);
    data << uint32(trade_sides_[other_index(index)]
                       .trade_spell); // Spell casted on non-traded item

    for (int i = 0; i < MAX_TRADE_SLOTS + 1; ++i)
    {
        ObjectGuid guid = i == MAX_TRADE_SLOTS ?
                              trade_sides_[index].non_traded :
                              trade_sides_[index].items[i];
        if (!guid)
            continue;

        Item* item = players_[index]->GetItemByGuid(guid, false);
        if (!item)
            continue; // XXX: This is actually a pretty grave error, we need to
                      // make sure this doesn't happen

        data << uint8(i);

        const ItemPrototype* proto = item->GetProto();

        data << uint32(proto->ItemId);
        data << uint32(proto->DisplayInfoID);
        data << uint32(item->GetCount());

        data << uint32(item->HasFlag(ITEM_FIELD_FLAGS, ITEM_DYNFLAG_WRAPPED));
        data << item->GetGuidValue(ITEM_FIELD_GIFTCREATOR);

        data << uint32(item->GetEnchantmentId(PERM_ENCHANTMENT_SLOT));
        for (int slot = SOCK_ENCHANTMENT_SLOT;
             slot < SOCK_ENCHANTMENT_SLOT + MAX_GEM_SOCKETS; ++slot)
            data << uint32(
                item->GetEnchantmentId(static_cast<EnchantmentSlot>(slot)));

        data << item->GetGuidValue(ITEM_FIELD_CREATOR);
        data << uint32(item->GetSpellCharges());
        data << uint32(item->GetItemSuffixFactor());
        data << uint32(item->GetItemRandomPropertyId());
        data << uint32(item->GetProto()->LockID);

        data << uint32(item->GetUInt32Value(ITEM_FIELD_MAXDURABILITY));
        data << uint32(item->GetUInt32Value(ITEM_FIELD_DURABILITY));
    }

    players_[target_index]->GetSession()->send_packet(std::move(data));
}
