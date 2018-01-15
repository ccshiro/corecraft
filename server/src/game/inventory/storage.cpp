#include "inventory/storage.h"
#include "Item.h"
#include "ItemPrototype.h"
#include "Player.h"
#include <stdexcept>

inventory::storage::~storage()
{
    for (auto& elem : items_)
    {
        std::vector<Item*>& item = elem.second;
        for (auto& item_i : item)
        {
            delete item_i;
            item_i = nullptr;
        }
    }
    items_.clear();
}

uint32 inventory::storage::item_count(uint32 item_id, bool exclude_bank,
    Item* exclude_item, bool exclude_buyback) const
{
    uint32 count = 0;
    for (const auto& elem : items_)
    {
        if (exclude_bank && slot(personal_slot, elem.first, 0).extra_bank_bag())
            continue;

        const std::vector<Item*>& item = elem.second;
        for (std::size_t i = 0; i < item.size(); ++i)
        {
            if (item[i] && item[i]->GetProto()->ItemId == item_id)
            {
                if (exclude_item && item[i] == exclude_item)
                    continue;
                if (exclude_bank &&
                    slot(personal_slot, elem.first, i).main_bank())
                    continue;
                if (exclude_buyback &&
                    slot(personal_slot, elem.first, i).buyback())
                    continue;
                count += item[i]->GetCount();
            }
        }
    }
    return count;
}

Item* inventory::storage::get(inventory::slot s) const
{
    if (!s.valid())
        return nullptr;

    auto bag_itr = items_.find(s.bag());
    if (bag_itr == items_.end())
        return nullptr;

    if (s.index() >= bag_itr->second.size())
        return nullptr;

    return bag_itr->second[s.index()];
}

bool inventory::storage::has_item(Item* item) const
{
    for (const auto& elem : items_)
    {
        const std::vector<Item*>& container = elem.second;
        for (auto& container_i : container)
        {
            if (container_i == item)
                return true;
        }
    }
    return false;
}

const std::vector<Item*>& inventory::storage::items_subscript(uint8 bag) const
{
    auto itr = items_.find(bag);
    if (itr == items_.end())
        throw std::out_of_range(
            "inventory::storage::items_subscript is only for when you know a "
            "bag exists.");
    return itr->second;
}

void inventory::storage::send_received_item(Player* player, Item* item,
    uint32 item_id, uint32 count, bool from_npc, bool created, bool broadcast)
{
    WorldPacket data(SMSG_ITEM_PUSH_RESULT, 8 + (4 * 10));
    data << player->GetObjectGuid();
    data << uint32(from_npc); // 0 = looted, 1 = from npc
    data << uint32(created);  // 0 = received, 1 = created
    data << uint32(1);        // IsShowChatMessage
    if (item)
    {
        data << uint8(item->slot().bag());    // bag
        data << uint32(item->slot().index()); // index
    }
    else
    {
        // When added to a stack, index is 0xFFFFFFFF
        data << uint8(0) << uint32(-1);
    }
    data << uint32(item_id);
    data << uint32(item ? item->GetItemSuffixFactor() : 0);
    data << uint32(item ? item->GetItemRandomPropertyId() : 0);
    data << uint32(count);                     // How many new items were added
    data << uint32(item_count(item_id, true)); // How many of this item we have
                                               // in our inventory (the one
                                               // we're sending included)

    if (broadcast && player->GetGroup())
        player->GetGroup()->BroadcastPacket(&data, true);
    else
        player->GetSession()->send_packet(std::move(data));
}

std::vector<inventory::slot> inventory::storage::autostore_slots(Player* player,
    Item* item, uint32 count, std::vector<uint8> available_bags,
    bool bank) const
{
    std::vector<inventory::slot> slots;
    inventory::slot first_empty_slot = slot();

    uint32 remaining_count = count;
    uint32 max_stack_size = item->GetProto()->Stackable;

    for (auto bag : available_bags)
    {
        auto find = items_.find(bag);
        if (find == items_.end())
            continue;
        const std::vector<Item*>& items = find->second;

        for (size_t i = 0; i < items.size(); ++i)
        {
            // We need some special rules for the main_bag (255)
            if (bag == main_bag)
            {
                bool is_backpack = i >= slot_start && i < slot_end;
                bool is_bank = i >= bank_slot_start && i < bank_slot_end;
                if (is_bank && !bank)
                    continue;
                if (!is_backpack && !is_bank)
                    continue;
            }

            Item* other_item = items[i];

            // For items that don't stack only empty slots are valid, we pick
            // the first empty one and return
            if (max_stack_size <= 1)
            {
                if (other_item)
                    continue;
                slot current = slot(get_slot_type(), bag, i);
                if (can_store(current, item, player, true) !=
                    EQUIP_ERR_OK) // Skip unique check, code that stores has to
                                  // deal with that
                    continue;
                slots.push_back(current);
                return slots;
            }

            // The following is logic that deals with items that do stack:

            // We need to remember the first empty slot in case we can't fit
            // everything into pre-existing items
            if (!first_empty_slot.valid() && !other_item)
            {
                slot current = slot(get_slot_type(), bag, i);
                if (can_store(current, item, player, true) ==
                    EQUIP_ERR_OK) // Skip unique check, code that stores has to
                                  // deal with that
                    first_empty_slot = current;
            }

            if (other_item && item->GetEntry() == other_item->GetEntry())
            {
                uint32 free_count = max_stack_size - other_item->GetCount();
                if (!free_count)
                    continue;

                slots.push_back(slot(get_slot_type(), bag, i));
                if (remaining_count <= free_count)
                    return slots;
                remaining_count -= free_count;
            }
        }
    }

    if (remaining_count > 0 && first_empty_slot.valid())
    {
        slots.push_back(first_empty_slot);
        return slots;
    }

    // If we get here it means we can't fully store the item
    return std::vector<slot>();
}

// XXX
bool inventory::storage::verify(transaction& /*trans*/) const
{
    return false;
}
bool inventory::storage::finalize(transaction& /*trans*/)
{
    return false;
}
