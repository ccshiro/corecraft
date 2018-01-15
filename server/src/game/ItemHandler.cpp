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

#include "Chat.h"
#include "Common.h"
#include "ConditionMgr.h"
#include "Item.h"
#include "logging.h"
#include "ObjectMgr.h"
#include "Opcodes.h"
#include "Player.h"
#include "UpdateData.h"
#include "WorldPacket.h"
#include "WorldSession.h"

void WorldSession::HandleSplitItemOpcode(WorldPacket& recv_data)
{
    uint8 src_bag, src_index, dst_bag, dst_index, count;
    recv_data >> src_bag >> src_index >> dst_bag >> dst_index >> count;

    // This opcode is not capable of simply swapping items
    if (count == 0)
        return;

    inventory::slot dst_slot(inventory::personal_slot, dst_bag, dst_index);
    inventory::slot src_slot(inventory::personal_slot, src_bag, src_index);

    InventoryResult err = _player->storage().swap(dst_slot, src_slot, count);
    if (err != EQUIP_ERR_OK)
        _player->SendEquipError(err, _player->storage().get(src_slot),
            _player->storage().get(dst_slot));
}

void WorldSession::HandleSwapInvItemOpcode(WorldPacket& recv_data)
{
    uint8 src_index, dst_index;
    recv_data >> src_index >> dst_index;

    inventory::slot dst_slot(
        inventory::personal_slot, inventory::main_bag, dst_index);
    inventory::slot src_slot(
        inventory::personal_slot, inventory::main_bag, src_index);

    InventoryResult err = _player->storage().swap(dst_slot, src_slot);
    if (err != EQUIP_ERR_OK)
        _player->SendEquipError(err, _player->storage().get(src_slot),
            _player->storage().get(dst_slot));
}

void WorldSession::HandleAutoEquipItemSlotOpcode(WorldPacket& recv_data)
{
    ObjectGuid item_guid;
    uint8 dst_index;
    recv_data >> item_guid >> dst_index;
    inventory::slot dst_slot(
        inventory::personal_slot, inventory::main_bag, dst_index);
    if (!dst_slot.valid() || !dst_slot.equipment())
        return;

    Item* item = _player->GetItemByGuid(item_guid, false); // Don't include bank
    if (!item)
        return;

    InventoryResult err = _player->storage().swap(dst_slot, item->slot());
    if (err != EQUIP_ERR_OK)
        _player->SendEquipError(err, item);
}

void WorldSession::HandleSwapItem(WorldPacket& recv_data)
{
    uint8 dst_bag, dst_index, src_bag, src_index;
    recv_data >> dst_bag >> dst_index >> src_bag >> src_index;

    inventory::slot dst_slot(inventory::personal_slot, dst_bag, dst_index);
    inventory::slot src_slot(inventory::personal_slot, src_bag, src_index);

    InventoryResult err = _player->storage().swap(dst_slot, src_slot);
    if (err != EQUIP_ERR_OK)
        _player->SendEquipError(err, _player->storage().get(src_slot),
            _player->storage().get(dst_slot));
}

void WorldSession::HandleAutoEquipItemOpcode(WorldPacket& recv_data)
{
    uint8 src_bag, src_index;
    recv_data >> src_bag >> src_index;
    inventory::slot src_slot(inventory::personal_slot, src_bag, src_index);

    inventory::slot dst_slot =
        _player->storage().find_auto_equip_slot(src_slot);
    if (!dst_slot.valid())
    {
        _player->SendEquipError(EQUIP_ERR_NO_EQUIPMENT_SLOT_AVAILABLE,
            _player->storage().get(src_slot));
        return;
    }

    InventoryResult err = _player->storage().swap(dst_slot, src_slot);
    if (err != EQUIP_ERR_OK)
        _player->SendEquipError(err, _player->storage().get(src_slot));
}

void WorldSession::HandleDestroyItemOpcode(WorldPacket& recv_data)
{
    uint8 bag, index, count, data1, data2, data3;
    recv_data >> bag >> index >> count >> data1 >> data2 >> data3;

    inventory::slot slot(inventory::personal_slot, bag, index);

    if (Item* item = _player->storage().get(slot))
    {
        // Make sure the player can "reach" this item (if it's in a bank, e.g.)
        // since this deletion is a player-initiated action
        if (!_player->storage().can_reach_item(item))
            return _player->SendEquipError(
                EQUIP_ERR_TOO_FAR_AWAY_FROM_BANK, item);

        if (count == 0)
        {
            inventory::transaction trans(true,
                inventory::transaction::send_self,
                inventory::transaction::add_loot, true); // Include bank
            trans.destroy(item);
            if (!_player->storage().finalize(trans))
                _player->SendEquipError(
                    static_cast<InventoryResult>(trans.error()), item);
        }
        else
        {
            if (count >= item->GetCount())
                return;
            InventoryResult err = _player->storage().remove_count(item, count);
            if (err != EQUIP_ERR_OK)
                _player->SendEquipError(err, item);
        }

        // Destroying an item interrupts your current cast
        _player->InterruptNonMeleeSpells(false);
    }
}

// Only _static_ data send in this packet !!!
void WorldSession::HandleItemQuerySingleOpcode(WorldPacket& recv_data)
{
    uint32 item;
    recv_data >> item;

    LOG_DEBUG(logging, "STORAGE: Item Query = %u", item);

    ItemPrototype const* pProto = ObjectMgr::GetItemPrototype(item);
    if (pProto)
    {
        int loc_idx = GetSessionDbLocaleIndex();

        std::string name = pProto->Name1;
        std::string description = pProto->Description;
        sObjectMgr::Instance()->GetItemLocaleStrings(
            pProto->ItemId, loc_idx, &name, &description);

        // guess size
        WorldPacket data(SMSG_ITEM_QUERY_SINGLE_RESPONSE, 600);
        data << pProto->ItemId;
        data << pProto->Class;
        data << pProto->SubClass;
        data << uint32(-1); // new 2.0.3, not exist in wdb cache?
        data << name;
        data << uint8(0x00); // pProto->Name2; // blizz not send name there,
                             // just uint8(0x00); <-- \0 = empty string = empty
                             // name...
        data << uint8(0x00); // pProto->Name3; // blizz not send name there,
                             // just uint8(0x00);
        data << uint8(0x00); // pProto->Name4; // blizz not send name there,
                             // just uint8(0x00);
        data << pProto->DisplayInfoID;
        data << pProto->Quality;
        data << pProto->Flags;
        data << pProto->BuyPrice;
        data << pProto->SellPrice;
        data << pProto->InventoryType;
        data << pProto->AllowableClass;
        data << pProto->AllowableRace;
        data << pProto->ItemLevel;
        data << pProto->RequiredLevel;
        data << pProto->RequiredSkill;
        data << pProto->RequiredSkillRank;
        data << pProto->RequiredSpell;
        data << pProto->RequiredHonorRank;
        data << pProto->RequiredCityRank;
        data << pProto->RequiredReputationFaction;
        data << pProto->RequiredReputationRank;
        data << pProto->MaxCount;
        data << pProto->Stackable;
        data << pProto->ContainerSlots;
        for (int i = 0; i < MAX_ITEM_PROTO_STATS; ++i)
        {
            data << pProto->ItemStat[i].ItemStatType;
            data << pProto->ItemStat[i].ItemStatValue;
        }
        for (int i = 0; i < MAX_ITEM_PROTO_DAMAGES; ++i)
        {
            data << pProto->Damage[i].DamageMin;
            data << pProto->Damage[i].DamageMax;
            data << pProto->Damage[i].DamageType;
        }

        // resistances (7)
        data << pProto->Armor;
        data << pProto->HolyRes;
        data << pProto->FireRes;
        data << pProto->NatureRes;
        data << pProto->FrostRes;
        data << pProto->ShadowRes;
        data << pProto->ArcaneRes;

        data << pProto->Delay;
        data << pProto->AmmoType;
        data << pProto->RangedModRange;

        for (int s = 0; s < MAX_ITEM_PROTO_SPELLS; ++s)
        {
            // send DBC data for cooldowns in same way as it used in
            // Spell::SendSpellCooldown
            // use `item_template` or if not set then only use spell cooldowns
            SpellEntry const* spell =
                sSpellStore.LookupEntry(pProto->Spells[s].SpellId);
            if (spell)
            {
                bool db_data = pProto->Spells[s].SpellCooldown >= 0 ||
                               pProto->Spells[s].SpellCategoryCooldown >= 0;

                data << pProto->Spells[s].SpellId;
                data << pProto->Spells[s].SpellTrigger;
                data << uint32(-abs(pProto->Spells[s].SpellCharges));

                if (db_data)
                {
                    data << uint32(pProto->Spells[s].SpellCooldown);
                    data << uint32(pProto->Spells[s].SpellCategory);
                    data << uint32(pProto->Spells[s].SpellCategoryCooldown);
                }
                else
                {
                    data << uint32(spell->RecoveryTime);
                    data << uint32(spell->Category);
                    data << uint32(spell->CategoryRecoveryTime);
                }
            }
            else
            {
                data << uint32(0);
                data << uint32(0);
                data << uint32(0);
                data << uint32(-1);
                data << uint32(0);
                data << uint32(-1);
            }
        }
        data << pProto->Bonding;
        data << description;
        data << pProto->PageText;
        data << pProto->LanguageID;
        data << pProto->PageMaterial;
        data << pProto->StartQuest;
        data << pProto->LockID;
        data << pProto->Material;
        data << pProto->Sheath;
        data << pProto->RandomProperty;
        data << pProto->RandomSuffix;
        data << pProto->Block;
        data << pProto->ItemSet;
        data << pProto->MaxDurability;
        data << pProto->Area;
        data << pProto->Map; // Added in 1.12.x & 2.0.1 client branch
        data << pProto->BagFamily;
        data << pProto->TotemCategory;
        for (int s = 0; s < MAX_ITEM_PROTO_SOCKETS; ++s)
        {
            data << pProto->Socket[s].Color;
            data << pProto->Socket[s].Content;
        }
        data << uint32(pProto->socketBonus);
        data << uint32(pProto->GemProperties);
        data << int32(pProto->RequiredDisenchantSkill);
        data << float(pProto->ArmorDamageModifier);
        data << uint32(
            pProto->Duration); // added in 2.4.2.8209, duration (seconds)
        send_packet(std::move(data));
    }
    else
    {
        LOG_DEBUG(logging,
            "WORLD: CMSG_ITEM_QUERY_SINGLE - NO item INFO! (ENTRY: %u)", item);
        WorldPacket data(SMSG_ITEM_QUERY_SINGLE_RESPONSE, 4);
        data << uint32(item | 0x80000000);
        send_packet(std::move(data));
    }
}

void WorldSession::HandleReadItemOpcode(WorldPacket& recv_data)
{
    uint8 bag, index;
    recv_data >> bag >> index;
    inventory::slot slot(inventory::personal_slot, bag, index);
    Item* item;
    if ((item = _player->storage().get(slot)) == nullptr ||
        !_player->storage().can_reach_item(item))
        return;

    if (item && item->GetProto()->PageText)
    {
        WorldPacket data;

        InventoryResult msg = _player->can_use_item(item->GetProto());
        if (msg == EQUIP_ERR_OK)
        {
            data.initialize(SMSG_READ_ITEM_OK, 8);
        }
        else
        {
            data.initialize(SMSG_READ_ITEM_FAILED, 8);
            _player->SendEquipError(msg, item);
        }

        data << ObjectGuid(item->GetObjectGuid());
        send_packet(std::move(data));
    }
    else
        _player->SendEquipError(EQUIP_ERR_ITEM_NOT_FOUND, nullptr);
}

void WorldSession::HandlePageQuerySkippedOpcode(WorldPacket& recv_data)
{
    uint32 item_id;
    ObjectGuid guid;
    recv_data >> item_id >> guid;
}

void WorldSession::HandleSellItemOpcode(WorldPacket& recv_data)
{
    ObjectGuid vendor_guid;
    ObjectGuid item_guid;
    uint8 count;

    recv_data >> vendor_guid >> item_guid >> count;

    Creature* pCreature =
        GetPlayer()->GetNPCIfCanInteractWith(vendor_guid, UNIT_NPC_FLAG_VENDOR);
    if (!pCreature)
        return;

    Item* item = _player->GetItemByGuid(item_guid, false);
    if (!item)
    {
        _player->SendSellError(
            SELL_ERR_CANT_FIND_ITEM, pCreature, item_guid, 0);
        return;
    }

    int err = _player->storage().sell_item(item->slot(), count);
    if (err != -1)
    {
        _player->SendSellError(
            static_cast<SellResult>(err), pCreature, item_guid, 0);
        return;
    }

    // Selling an item interrupts your current cast
    _player->InterruptNonMeleeSpells(false);
}

void WorldSession::HandleBuybackItem(WorldPacket& recv_data)
{
    ObjectGuid vendor_guid;
    uint32 index;
    recv_data >> vendor_guid >> index;

    if (!_player->GetNPCIfCanInteractWith(vendor_guid, UNIT_NPC_FLAG_VENDOR))
        return;

    LOG_DEBUG(logging, "HandleBuybackItem slot: %u", index);
    _player->storage().buyback_item(
        inventory::slot(inventory::personal_slot, inventory::main_bag, index));
}

void WorldSession::HandleBuyItemInSlotOpcode(WorldPacket& recv_data)
{
    ObjectGuid vendor_guid;
    ObjectGuid bag_guid;
    uint32 item_id;
    uint8 bag_index, count;
    recv_data >> vendor_guid >> item_id >> bag_guid >> bag_index >> count;

    inventory::slot dst;
    if (bag_guid.GetRawValue() ==
        static_cast<uint64>(_player->GetGUIDLow())) // For some reason the guid
                                                    // of our character is used
                                                    // to signalize our backpack
    {
        dst = inventory::slot(
            inventory::personal_slot, inventory::main_bag, bag_index);
    }
    else
    {
        Item* bag_item = _player->GetItemByGuid(bag_guid, false);
        if (!bag_item || !bag_item->slot().bagslot())
            return;

        dst = inventory::slot(
            inventory::personal_slot, bag_item->slot().index(), bag_index);
    }

    GetPlayer()->BuyItemFromVendor(vendor_guid, item_id, count, dst);
}

void WorldSession::HandleBuyItemOpcode(WorldPacket& recv_data)
{
    ObjectGuid vendor_guid;
    uint32 item_id;
    uint8 count, unk1;

    recv_data >> vendor_guid >> item_id >> count >> unk1;

    GetPlayer()->BuyItemFromVendor(vendor_guid, item_id, count);
}

void WorldSession::HandleListInventoryOpcode(WorldPacket& recv_data)
{
    ObjectGuid vendor_guid;
    recv_data >> vendor_guid;

    if (!_player->isAlive())
        return;

    SendListInventory(vendor_guid);
}

void WorldSession::SendListInventory(ObjectGuid vendor_guid)
{
    Creature* pCreature =
        GetPlayer()->GetNPCIfCanInteractWith(vendor_guid, UNIT_NPC_FLAG_VENDOR);

    if (!pCreature)
    {
        // Don't send can't find vendor; this error is only legitimately
        // triggered
        // when the server thinks you're out of range but the client doesn't
        return;
    }

    // remove fake death
    if (GetPlayer()->hasUnitState(UNIT_STAT_DIED))
        GetPlayer()->remove_auras(SPELL_AURA_FEIGN_DEATH);

    // Stop movement
    if (pCreature->GetGroup() != nullptr &&
        pCreature->GetGroup()->HasFlag(CREATURE_GROUP_FLAG_GROUP_MOVEMENT))
    {
        pCreature->GetMap()->GetCreatureGroupMgr().PauseMovementOfGroup(
            pCreature->GetGroup()->GetId(), 10000);
    }
    else if (!pCreature->IsStopped())
        pCreature->StopMoving();

    VendorItemData const* vItems = pCreature->GetVendorItems();
    VendorItemData const* tItems = pCreature->GetVendorTemplateItems();

    if (!vItems && !tItems)
    {
        WorldPacket data(SMSG_LIST_INVENTORY, (8 + 1 + 1));
        data << ObjectGuid(vendor_guid);
        data << uint8(0); // count==0, next will be error code
        data << uint8(0); // "Vendor has no inventory"
        send_packet(std::move(data));
        return;
    }

    uint8 customitems = vItems ? vItems->GetItemCount() : 0;
    uint8 numitems = customitems + (tItems ? tItems->GetItemCount() : 0);

    uint8 count = 0;

    WorldPacket data(SMSG_LIST_INVENTORY, (8 + 1 + numitems * 8 * 4));
    data << ObjectGuid(vendor_guid);

    size_t count_pos = data.wpos();
    data << uint8(count);

    float discountMod = _player->GetReputationPriceDiscount(pCreature);

    for (int i = 0; i < numitems; ++i)
    {
        VendorItem const* crItem = i < customitems ?
                                       vItems->GetItem(i) :
                                       tItems->GetItem(i - customitems);

        if (crItem)
        {
            uint32 itemId = crItem->item;
            ItemPrototype const* pProto = ObjectMgr::GetItemPrototype(itemId);
            if (pProto)
            {
                if (!_player->isGameMaster())
                {
                    // class wrong item skip only for bindable case
                    if ((pProto->AllowableClass & _player->getClassMask()) ==
                            0 &&
                        pProto->Bonding == BIND_WHEN_PICKED_UP)
                        continue;

                    // race wrong item skip always
                    if ((pProto->AllowableRace & _player->getRaceMask()) == 0)
                        continue;

                    // check conditions
                    const ConditionList* conditions =
                        sConditionMgr::Instance()->GetVendorItemConditions(
                            pCreature->GetEntry(), itemId);
                    if (conditions &&
                        !sConditionMgr::Instance()->IsObjectMeetToConditions(
                            _player, pCreature, conditions))
                        continue;
                }

                ++count;

                // reputation discount
                uint32 price = uint32(floor(pProto->BuyPrice * discountMod));

                data << uint32(i + 1);
                data << uint32(itemId);
                data << uint32(pProto->DisplayInfoID);
                data << uint32(
                    crItem->maxcount <= 0 ?
                        0xFFFFFFFF :
                        pCreature->GetVendorItemCurrentCount(crItem));
                data << uint32(price);
                data << uint32(pProto->MaxDurability);
                data << uint32(pProto->BuyCount);
                data << uint32(crItem->ExtendedCost);
            }
        }
    }

    if (count == 0)
    {
        data << uint8(0); // "Vendor has no inventory"
        send_packet(std::move(data));
        return;
    }

    data.put<uint8>(count_pos, count);
    send_packet(std::move(data));
}

// Stores an item in the first available slot of a bag
void WorldSession::HandleAutoStoreBagItemOpcode(WorldPacket& recv_data)
{
    uint8 src_bag, src_index, dst_bag;
    recv_data >> src_bag >> src_index >> dst_bag;
    inventory::slot src_slot(inventory::personal_slot, src_bag, src_index);

    // FIXME: It's possible this would allow items to be absorbed into sensible
    // stacks, which we don't support at the moment (we require an empty slot)
    inventory::slot dst_slot =
        _player->storage().find_empty_slot_in_bag(dst_bag);
    if (!dst_slot.valid())
    {
        _player->SendEquipError(
            EQUIP_ERR_BAG_FULL, _player->storage().get(src_slot));
        return;
    }

    InventoryResult err = _player->storage().swap(dst_slot, src_slot);
    if (err != EQUIP_ERR_OK)
        _player->SendEquipError(err, _player->storage().get(src_slot),
            _player->storage().get(dst_slot));
}

bool WorldSession::CheckBanker(ObjectGuid guid)
{
    // GM case
    if (guid == GetPlayer()->GetObjectGuid())
    {
        // command case will return only if player have real access to command
        if (!ChatHandler(GetPlayer()).FindCommand("bank"))
        {
            LOG_DEBUG(logging, "%s attempt open bank in cheating way.",
                guid.GetString().c_str());
            return false;
        }
    }
    // banker case
    else
    {
        if (!GetPlayer()->GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_BANKER))
        {
            LOG_DEBUG(logging,
                "Banker %s not found or you can't interact with him.",
                guid.GetString().c_str());
            return false;
        }
    }

    return true;
}

void WorldSession::HandleBuyBankSlotOpcode(WorldPacket& recv_data)
{
    ObjectGuid guid;
    recv_data >> guid;

    if (!CheckBanker(guid))
        return;

    uint32 slot = _player->GetBankBagSlotCount();
    ++slot;

    LOG_DEBUG(logging, "PLAYER: Buy bank bag slot, slot number = %u", slot);
    BankBagSlotPricesEntry const* slotEntry =
        sBankBagSlotPricesStore.LookupEntry(slot);

    WorldPacket data(SMSG_BUY_BANK_SLOT_RESULT, 4);

    if (!slotEntry)
    {
        data << uint32(ERR_BANKSLOT_FAILED_TOO_MANY);
        send_packet(std::move(data));
        return;
    }

    inventory::transaction trans;
    trans.remove(inventory::copper(slotEntry->price));
    if (!_player->storage().finalize(trans))
    {
        data << uint32(ERR_BANKSLOT_INSUFFICIENT_FUNDS);
        send_packet(std::move(data));
        return;
    }

    _player->SetBankBagSlotCount(slot);

    data << uint32(ERR_BANKSLOT_OK);
    send_packet(std::move(data));
}

// This opcode is used to attempty auto-storing an item into the bank from the
// inventory
void WorldSession::HandleAutoBankItemOpcode(WorldPacket& recv_data)
{
    uint8 src_bag, src_index;
    recv_data >> src_bag >> src_index;
    inventory::slot src_slot(inventory::personal_slot, src_bag, src_index);

    auto item = _player->storage().get(src_slot);
    if (!item)
    {
        _player->SendEquipError(EQUIP_ERR_INT_BAG_ERROR, nullptr);
        return;
    }

    _player->storage().auto_store_item(item, true);
}

// This opcode is used to attempty auto-retrieving an item from the bank into
// the inventory
void WorldSession::HandleAutoStoreBankItemOpcode(WorldPacket& recv_data)
{
    uint8 src_bag, src_index;
    recv_data >> src_bag >> src_index;
    inventory::slot src_slot(inventory::personal_slot, src_bag, src_index);

    auto item = _player->storage().get(src_slot);
    if (!item)
    {
        _player->SendEquipError(EQUIP_ERR_INT_BAG_ERROR, nullptr);
        return;
    }

    _player->storage().auto_store_item(item, false);
}

void WorldSession::HandleSetAmmoOpcode(WorldPacket& recv_data)
{
    if (!GetPlayer()->isAlive())
    {
        GetPlayer()->SendEquipError(EQUIP_ERR_YOU_ARE_DEAD, nullptr, nullptr);
        return;
    }

    uint32 item;

    recv_data >> item;

    if (!item)
        GetPlayer()->RemoveAmmo();
    else
        GetPlayer()->SetAmmo(item);
}

void WorldSession::SendEnchantmentLog(
    ObjectGuid targetGuid, ObjectGuid casterGuid, uint32 itemId, uint32 spellId)
{
    WorldPacket data(
        SMSG_ENCHANTMENTLOG, (8 + 8 + 4 + 4 + 1)); // last check 2.0.10
    data << ObjectGuid(targetGuid);
    data << ObjectGuid(casterGuid);
    data << uint32(itemId);
    data << uint32(spellId);
    data << uint8(0);
    send_packet(std::move(data));
}

void WorldSession::HandleItemNameQueryOpcode(WorldPacket& recv_data)
{
    uint32 itemid;
    recv_data >> itemid;
    recv_data.read_skip<uint64>(); // guid

    LOG_DEBUG(logging, "Player %s: CMSG_ITEM_NAME_QUERY: item_id: %u",
        _player->GetName(), itemid);
    if (ItemPrototype const* pProto = ObjectMgr::GetItemPrototype(itemid))
    {
        int loc_idx = GetSessionDbLocaleIndex();

        std::string name = pProto->Name1;
        sObjectMgr::Instance()->GetItemLocaleStrings(
            pProto->ItemId, loc_idx, &name);
        // guess size
        WorldPacket data(SMSG_ITEM_NAME_QUERY_RESPONSE, (4 + 10));
        data << uint32(pProto->ItemId);
        data << name;
        data << uint32(pProto->InventoryType);
        send_packet(std::move(data));
        return;
    }
    else
    {
        // listed in dbc or not expected to exist unknown item
        if (sItemStore.LookupEntry(itemid))
            logging.error(
                "WORLD: CMSG_ITEM_NAME_QUERY for item %u failed (item listed "
                "in Item.dbc but not exist in DB)",
                itemid);
        else
            logging.error(
                "WORLD: CMSG_ITEM_NAME_QUERY for item %u failed (unknown item, "
                "not listed in Item.dbc)",
                itemid);
    }
}

void WorldSession::HandleWrapItemOpcode(WorldPacket& recv_data)
{
    uint8 paper_bag, paper_index, item_bag, item_index;
    recv_data >> paper_bag >> paper_index >> item_bag >> item_index;
    LOG_DEBUG(logging, "paper: %u:%u, item: %u:%u", paper_bag, paper_index,
        item_bag, item_index);
    inventory::slot paper_slot(
        inventory::personal_slot, paper_bag, paper_index);
    inventory::slot item_slot(inventory::personal_slot, item_bag, item_index);

    Item* paper = _player->storage().get(paper_slot);
    Item* item = _player->storage().get(item_slot);
    if (!item || !paper || item == paper ||
        (paper->GetProto()->Flags & ITEM_FLAG_WRAPPER) == 0)
        return _player->SendEquipError(EQUIP_ERR_ITEM_NOT_FOUND, paper);

    if (paper_slot.equipment() || item_slot.equipment())
        return _player->SendEquipError(
            EQUIP_ERR_EQUIPPED_CANT_BE_WRAPPED, item);

    if (item->GetGuidValue(ITEM_FIELD_GIFTCREATOR) ||
        item->HasFlag(ITEM_FIELD_FLAGS, ITEM_DYNFLAG_WRAPPED))
        return _player->SendEquipError(EQUIP_ERR_WRAPPED_CANT_BE_WRAPPED, item);

    if (item->IsBag())
        return _player->SendEquipError(EQUIP_ERR_BAGS_CANT_BE_WRAPPED, item);

    if (item->IsSoulBound())
        return _player->SendEquipError(EQUIP_ERR_BOUND_CANT_BE_WRAPPED, item);

    if (item->GetMaxStackCount() != 1)
        return _player->SendEquipError(
            EQUIP_ERR_STACKABLE_CANT_BE_WRAPPED, item);

    if (item->GetProto()->MaxCount > 0)
        return _player->SendEquipError(EQUIP_ERR_UNIQUE_CANT_BE_WRAPPED, item);

    uint32 paper_entry = paper->GetEntry();
    InventoryResult err = _player->storage().remove_count(paper, 1);
    if (err != EQUIP_ERR_OK)
        return _player->SendEquipError(err, item, paper);

    uint32 item_entry = item->GetEntry();
    uint32 flags = item->GetUInt32Value(ITEM_FIELD_FLAGS);
    item->SetEntry(paper_entry);

    switch (item->GetEntry())
    {
    case 5042:
        item->SetEntry(5043);
        break;
    case 5048:
        item->SetEntry(5044);
        break;
    case 17303:
        item->SetEntry(17302);
        break;
    case 17304:
        item->SetEntry(17305);
        break;
    case 17307:
        item->SetEntry(17308);
        break;
    case 21830:
        item->SetEntry(21831);
        break;
    }

    item->SetGuidValue(ITEM_FIELD_GIFTCREATOR, _player->GetObjectGuid());
    item->SetUInt32Value(ITEM_FIELD_FLAGS, ITEM_DYNFLAG_WRAPPED);
    item->mark_for_save();

    CharacterDatabase.PExecute(
        "INSERT INTO character_gifts VALUES ('%u', '%u', '%u', '%u')",
        item->GetOwnerGuid().GetCounter(), item->GetGUIDLow(), item_entry,
        flags);
    // XXX: Do we need to save character's inventory right away? Considering we
    // insert into gifts
}

void WorldSession::HandleSocketOpcode(WorldPacket& recv_data)
{
    // XXX: Sockets added by Blacksmithing, how do they change this

    ObjectGuid item_guid;
    ObjectGuid gem_guids[MAX_GEM_SOCKETS];

    recv_data >> item_guid;
    for (auto& gem_guid : gem_guids)
        recv_data >> gem_guid;

    Item* target = _player->GetItemByGuid(item_guid, false);
    if (!target)
        return;
    const ItemPrototype* prototype = target->GetProto();

    // Make sure each gem is unique
    for (int i = 0; i < MAX_GEM_SOCKETS; ++i)
        for (int j = 0; j < MAX_GEM_SOCKETS; ++j)
            if (gem_guids[i] != 0 && gem_guids[j] != 0 && i != j &&
                gem_guids[i] == gem_guids[j])
                return;

    // Retrieve all the gems from the characters' inventory
    bool found_a_gem = false;
    Item* gems[MAX_GEM_SOCKETS] = {
        nullptr}; // Initialize to 0, might be skipped in our loop
    const GemPropertiesEntry* gem_properties[MAX_GEM_SOCKETS] = {
        nullptr}; // Initialize to 0, might be skipped in our loop
    for (int i = 0; i < MAX_GEM_SOCKETS; ++i)
    {
        if (!gem_guids[i])
            continue;
        // Make sure we a) have that item, b) it's a gem, and c) it has an entry
        // in our gem properties store
        if ((gems[i] = _player->GetItemByGuid(gem_guids[i], false)) ==
                nullptr ||
            gems[i]->GetProto()->GemProperties == 0 ||
            (gem_properties[i] = sGemPropertiesStore.LookupEntry(
                 gems[i]->GetProto()->GemProperties)) == nullptr)
            return;
        found_a_gem = true;
    }

    if (!found_a_gem)
        return;

    // Verify that our gem goes in the specified slot of this item
    for (int i = 0; i < MAX_GEM_SOCKETS; ++i)
    {
        if (!gems[i])
            continue;

        // Must have a socket at targeted location
        if (prototype->Socket[i].Color == 0)
            return;

        // Meta gems only go into meta-gem slots, and normal sockets don't go
        // into meta-gem slots
        if ((prototype->Socket[i].Color == SOCKET_COLOR_META ||
                gem_properties[i]->color == SOCKET_COLOR_META) &&
            prototype->Socket[i].Color != gem_properties[i]->color)
            return;
    }

    // Get the effects the current gem has applied, and the ones the new gems
    // will have
    uint32 old_effects[MAX_GEM_SOCKETS], new_effects[MAX_GEM_SOCKETS];
    for (int i = 0; i < MAX_GEM_SOCKETS; ++i)
    {
        old_effects[i] = target->GetEnchantmentId(
            static_cast<EnchantmentSlot>(SOCK_ENCHANTMENT_SLOT + i));
        new_effects[i] =
            (gems[i]) ? gem_properties[i]->spellitemenchantement : 0;
    }

    // Note: Each gem has its own enchantment. We can get the gem from the
    // enchant like this:
    // sSpellItemEnchantmentStore.LookupEntry(enchant_id)->GemID
    // This also implies that comparing enchantment ids is the same as comparing
    // GemIDs
    // As well as the fact that items only need be marked by the enchantment not
    // the Gem ID itself

    // Verify that our gems do not violate uniqueness inside target
    for (int i = 0; i < MAX_GEM_SOCKETS; ++i)
    {
        if (!gems[i] ||
            (gems[i]->GetProto()->Flags & ITEM_FLAG_UNIQUE_EQUIPPED) == 0)
            continue;
        // Gem is unique-equipped, verify it against all the other ones
        for (int j = 0; j < MAX_GEM_SOCKETS; j++)
        {
            if (new_effects[i] == old_effects[j])
                return _player->SendEquipError(
                    EQUIP_ERR_ITEM_UNIQUE_EQUIPPABLE_SOCKETED, target);
            if (i != j && new_effects[i] == new_effects[j])
                return _player->SendEquipError(
                    EQUIP_ERR_ITEM_UNIQUE_EQUIPPABLE_SOCKETED, target);
        }
    }

    // Compare against other equipped items' gems if target is already equipped
    if (target->slot().equipment())
    {
        int mask = inventory::personal_storage::iterator::equipment;
        for (inventory::personal_storage::iterator itr =
                 _player->storage().begin(mask);
             itr != _player->storage().end(); ++itr)
        {
            Item* item = *itr;
            for (int i = 0; i < MAX_GEM_SOCKETS; ++i)
            {
                uint32 enchant = item->GetEnchantmentId(
                    static_cast<EnchantmentSlot>(SOCK_ENCHANTMENT_SLOT + i));
                if (!enchant)
                    continue;
                // Compare this gem against our gems
                for (int j = 0; j < MAX_GEM_SOCKETS; j++)
                {
                    if (!gems[j] ||
                        (gems[j]->GetProto()->Flags &
                            ITEM_FLAG_UNIQUE_EQUIPPED) == 0)
                        continue;
                    // Gem is unique, check if their enchantment ids match
                    if (new_effects[j] == enchant)
                        return _player->SendEquipError(
                            EQUIP_ERR_ITEM_MAX_COUNT_EQUIPPED_SOCKETED, target);
                }
            }
        }
    }

    // All checks have passed, it's time to toggle on the effect of these gems
    inventory::transaction trans(false);
    for (int i = 0; i < MAX_GEM_SOCKETS; ++i)
    {
        if (gems[i] == nullptr)
            continue; // Gem has not changed in this slot

        // Consume the gem
        trans.destroy(gems[i]);

        EnchantmentSlot ench_slot =
            static_cast<EnchantmentSlot>(SOCK_ENCHANTMENT_SLOT + i);

        // Toggle off the previous enchantment
        if (target->slot().equipment())
        {
            if (gem_properties[i]->color == SOCKET_COLOR_META)
            {
                if (target->meta_toggled_on)
                {
                    _player->ApplyEnchantment(
                        target, ench_slot, false, target->slot(), false);
                    target->meta_toggled_on =
                        false; // mark meta-gem as toggled off
                }
            }
            else
            {
                _player->ApplyEnchantment(
                    target, ench_slot, false, target->slot(), false);
            }
        }

        // Insert the gem into the item (read note above)
        target->SetEnchantment(ench_slot, new_effects[i], 0, 0);

        // Toggle on the new enchantment (skip meta-gems)
        if (target->slot().equipment() &&
            gem_properties[i]->color != SOCKET_COLOR_META)
            _player->ApplyEnchantment(
                target, ench_slot, true, target->slot(), true);
    }
    // This operation cannot fail, as we verified we have the item above
    _player->storage().finalize(trans);

    target->mark_for_save();

    // Update bonus effects the item gives the player if all colors match
    // NOTE: This needs to be updated even if not equipped to properly set the
    // enchantment id of the bonus!
    target->update_gem_bonus();

    // Update bonuses & meta gems only if the item is equipped
    if (!target->slot().equipment())
        return;

    // Make gems appear for people inspecting the player
    target->visualize_gems();

    // Update all meta-gem effects after sockets have changed
    _player->update_meta_gem();
}

void WorldSession::HandleCancelTempEnchantmentOpcode(WorldPacket& recv_data)
{
    uint32 equipment_slot;
    recv_data >> equipment_slot;
    inventory::slot slot(inventory::personal_slot, inventory::main_bag,
        static_cast<uint8>(equipment_slot));
    Item* item = _player->storage().get(slot);
    if (!slot.equipment() || !item ||
        !item->GetEnchantmentId(TEMP_ENCHANTMENT_SLOT))
        return;

    _player->ApplyEnchantment(
        item, TEMP_ENCHANTMENT_SLOT, false, item->slot(), false);
    item->ClearEnchantment(TEMP_ENCHANTMENT_SLOT);
}
