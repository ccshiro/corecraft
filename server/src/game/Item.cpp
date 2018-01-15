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

#include "Item.h"
#include "ItemEnchantmentMgr.h"
#include "ObjectGuid.h"
#include "ObjectMgr.h"
#include "Spell.h"
#include "WorldPacket.h"
#include "loot_distributor.h"
#include "Database/DatabaseEnv.h"

void AddItemsSetItem(Player* player, Item* item)
{
    ItemPrototype const* proto = item->GetProto();
    uint32 setid = proto->ItemSet;

    ItemSetEntry const* set = sItemSetStore.LookupEntry(setid);

    if (!set)
    {
        logging.error(
            "Item set %u for item (id %u) not found, mods not applied.", setid,
            proto->ItemId);
        return;
    }

    if (set->required_skill_id &&
        player->GetSkillValue(set->required_skill_id) <
            set->required_skill_value)
        return;

    ItemSetEffect* eff = nullptr;

    for (auto& elem : player->ItemSetEff)
    {
        if (elem && elem->setid == setid)
        {
            eff = elem;
            break;
        }
    }

    if (!eff)
    {
        eff = new ItemSetEffect;
        memset(eff, 0, sizeof(ItemSetEffect));
        eff->setid = setid;

        size_t x = 0;
        for (; x < player->ItemSetEff.size(); x++)
            if (!player->ItemSetEff[x])
                break;

        if (x < player->ItemSetEff.size())
            player->ItemSetEff[x] = eff;
        else
            player->ItemSetEff.push_back(eff);
    }

    ++eff->item_count;

    for (uint32 x = 0; x < 8; x++)
    {
        if (!set->spells[x])
            continue;
        // not enough for  spell
        if (set->items_to_triggerspell[x] > eff->item_count)
            continue;

        uint32 z = 0;
        for (; z < 8; z++)
            if (eff->spells[z] && eff->spells[z]->Id == set->spells[x])
                break;

        if (z < 8)
            continue;

        // new spell
        for (uint32 y = 0; y < 8; y++)
        {
            if (!eff->spells[y]) // free slot
            {
                SpellEntry const* spellInfo =
                    sSpellStore.LookupEntry(set->spells[x]);
                if (!spellInfo)
                {
                    logging.error(
                        "WORLD: unknown spell id %u in items set %u effects",
                        set->spells[x], setid);
                    break;
                }

                // spell casted only if fit form requirement, in other case will
                // casted at form change
                player->ApplyEquipSpell(spellInfo, nullptr, true);
                eff->spells[y] = spellInfo;
                break;
            }
        }
    }
}

void RemoveItemsSetItem(Player* player, ItemPrototype const* proto)
{
    uint32 setid = proto->ItemSet;

    ItemSetEntry const* set = sItemSetStore.LookupEntry(setid);

    if (!set)
    {
        logging.error("Item set #%u for item #%u not found, mods not removed.",
            setid, proto->ItemId);
        return;
    }

    ItemSetEffect* eff = nullptr;
    size_t setindex = 0;
    for (; setindex < player->ItemSetEff.size(); setindex++)
    {
        if (player->ItemSetEff[setindex] &&
            player->ItemSetEff[setindex]->setid == setid)
        {
            eff = player->ItemSetEff[setindex];
            break;
        }
    }

    // can be in case now enough skill requirement for set appling but set has
    // been appliend when skill requirement not enough
    if (!eff)
        return;

    --eff->item_count;

    for (uint32 x = 0; x < 8; x++)
    {
        if (!set->spells[x])
            continue;

        // enough for spell
        if (set->items_to_triggerspell[x] <= eff->item_count)
            continue;

        for (uint32 z = 0; z < 8; z++)
        {
            if (eff->spells[z] && eff->spells[z]->Id == set->spells[x])
            {
                // spell can be not active if not fit form requirement
                player->ApplyEquipSpell(eff->spells[z], nullptr, false);
                eff->spells[z] = nullptr;
                break;
            }
        }
    }

    if (!eff->item_count) // all items of a set were removed
    {
        assert(eff == player->ItemSetEff[setindex]);
        delete eff;
        player->ItemSetEff[setindex] = nullptr;
    }
}

bool ItemCanGoIntoBag(
    ItemPrototype const* pProto, ItemPrototype const* pBagProto)
{
    if (!pProto || !pBagProto)
        return false;

    switch (pBagProto->Class)
    {
    case ITEM_CLASS_CONTAINER:
        switch (pBagProto->SubClass)
        {
        case ITEM_SUBCLASS_CONTAINER:
            return true;
        case ITEM_SUBCLASS_SOUL_CONTAINER:
            if (!(pProto->BagFamily & BAG_FAMILY_MASK_SOUL_SHARDS))
                return false;
            return true;
        case ITEM_SUBCLASS_HERB_CONTAINER:
            if (!(pProto->BagFamily & BAG_FAMILY_MASK_HERBS))
                return false;
            return true;
        case ITEM_SUBCLASS_ENCHANTING_CONTAINER:
            if (!(pProto->BagFamily & BAG_FAMILY_MASK_ENCHANTING_SUPP))
                return false;
            return true;
        case ITEM_SUBCLASS_MINING_CONTAINER:
            if (!(pProto->BagFamily & BAG_FAMILY_MASK_MINING_SUPP))
                return false;
            return true;
        case ITEM_SUBCLASS_ENGINEERING_CONTAINER:
            if (!(pProto->BagFamily & BAG_FAMILY_MASK_ENGINEERING_SUPP))
                return false;
            return true;
        case ITEM_SUBCLASS_GEM_CONTAINER:
            if (!(pProto->BagFamily & BAG_FAMILY_MASK_GEMS))
                return false;
            return true;
        case ITEM_SUBCLASS_LEATHERWORKING_CONTAINER:
            if (!(pProto->BagFamily & BAG_FAMILY_MASK_LEATHERWORKING_SUPP))
                return false;
            return true;
        default:
            return false;
        }
    case ITEM_CLASS_QUIVER:
        switch (pBagProto->SubClass)
        {
        case ITEM_SUBCLASS_QUIVER:
            if (!(pProto->BagFamily & BAG_FAMILY_MASK_ARROWS))
                return false;
            return true;
        case ITEM_SUBCLASS_AMMO_POUCH:
            if (!(pProto->BagFamily & BAG_FAMILY_MASK_BULLETS))
                return false;
            return true;
        default:
            return false;
        }
    }
    return false;
}

Item::Item(const ItemPrototype* prototype)
  : meta_toggled_on{false}, should_save_{false}, in_db_{false},
    in_trade_{false}, m_lootState{ITEM_LOOT_NONE}, temp_ench_dur_{0}
{
    if (prototype && prototype->InventoryType == INVTYPE_BAG)
    {
        m_objectType |= TYPEMASK_ITEM | TYPEMASK_CONTAINER;
        m_objectTypeId = TYPEID_CONTAINER;
        m_valuesCount = CONTAINER_END;
    }
    else
    {
        m_objectType |= TYPEMASK_ITEM;
        m_objectTypeId = TYPEID_ITEM;
        m_valuesCount = ITEM_END;
    }
    // 2.3.2 - 0x18
    m_updateFlag = (UPDATEFLAG_LOWGUID | UPDATEFLAG_HIGHGUID);
}

Item::~Item()
{
    for (auto& s : referencing_spells_)
        s->set_cast_item(nullptr);
}

bool Item::Create(uint32 guidlow, uint32 itemid, Player const* owner)
{
    Object::_Create(guidlow, 0, HIGHGUID_ITEM);

    SetEntry(itemid);
    SetObjectScale(DEFAULT_OBJECT_SCALE);

    SetGuidValue(
        ITEM_FIELD_OWNER, owner ? owner->GetObjectGuid() : ObjectGuid());
    SetGuidValue(ITEM_FIELD_CONTAINED, ObjectGuid());

    ItemPrototype const* itemProto = ObjectMgr::GetItemPrototype(itemid);
    if (!itemProto)
        return false;

    SetUInt32Value(ITEM_FIELD_STACK_COUNT, 1);
    SetUInt32Value(ITEM_FIELD_MAXDURABILITY, itemProto->MaxDurability);
    SetUInt32Value(ITEM_FIELD_DURABILITY, itemProto->MaxDurability);

    for (int i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
        SetSpellCharges(i, itemProto->Spells[i].SpellCharges);

    SetUInt32Value(ITEM_FIELD_DURATION, itemProto->Duration);

    // Fill in bag-related data
    if (itemProto->InventoryType == INVTYPE_BAG)
    {
        SetUInt32Value(CONTAINER_FIELD_NUM_SLOTS, itemProto->ContainerSlots);
        for (uint8 i = 0; i < inventory::max_bag_size; ++i)
            SetGuidValue(CONTAINER_FIELD_SLOT_1 + (i * 2), ObjectGuid());
    }

    return true;
}

void Item::UpdateDuration(Player* owner, uint32 diff)
{
    if (!GetUInt32Value(ITEM_FIELD_DURATION))
        return;

    if (GetUInt32Value(ITEM_FIELD_DURATION) <= diff)
    {
        // XXX:
        inventory::transaction destroy(false);
        destroy.destroy(this);
        owner->storage().finalize(destroy);
        return;
    }

    SetUInt32Value(
        ITEM_FIELD_DURATION, GetUInt32Value(ITEM_FIELD_DURATION) - diff);
    should_save_ = true;
}

void Item::SendEnchDurations(Player* owner)
{
    // last checked 2.0.10
    WorldPacket data(SMSG_ITEM_ENCHANT_TIME_UPDATE, 8 + 4 + 4 + 8);
    data << GetObjectGuid();
    data << uint32(TEMP_ENCHANTMENT_SLOT);
    data << uint32(GetEnchantmentDuration(TEMP_ENCHANTMENT_SLOT) / 1000);
    data << owner->GetObjectGuid();
    owner->SendDirectMessage(std::move(data));
}

void Item::UpdateEnchDurations(Player* owner, uint32 diff)
{
    // NOTE: The client reacts poorly to the duration changing in the temporary
    // enchantments
    // column. Instead we only change it when writing to the DB, and keep a
    // "local" counter.

    if (!temp_ench_dur_)
        return;

    if (temp_ench_dur_ <= diff)
    {
        if (IsEquipped())
            owner->ApplyEnchantment(this, TEMP_ENCHANTMENT_SLOT, false, slot());
        ClearEnchantment(TEMP_ENCHANTMENT_SLOT);
        diff = temp_ench_dur_;
    }

    temp_ench_dur_ -= diff;
    should_save_ = true;
}

bool Item::LoadFromDB(uint32 guidLow, Field* fields, ObjectGuid ownerGuid)
{
    Object::_Create(guidLow, 0, HIGHGUID_ITEM);
    in_db_ = true;

    if (!LoadValues(fields[0].GetString()))
    {
        logging.error(
            "Item #%d have broken data in `data` field. Can't be loaded.",
            guidLow);
        return false;
    }

    bool need_save = false; // need explicit save data at load fixes

    // overwrite possible wrong/corrupted guid
    ObjectGuid new_item_guid = ObjectGuid(HIGHGUID_ITEM, guidLow);
    if (GetGuidValue(OBJECT_FIELD_GUID) != new_item_guid)
    {
        SetGuidValue(OBJECT_FIELD_GUID, new_item_guid);
        need_save = true;
    }

    ItemPrototype const* proto = GetProto();
    if (!proto)
        return false;

    // update max durability (and durability) if need
    if (proto->MaxDurability != GetUInt32Value(ITEM_FIELD_MAXDURABILITY))
    {
        SetUInt32Value(ITEM_FIELD_MAXDURABILITY, proto->MaxDurability);
        if (GetUInt32Value(ITEM_FIELD_DURABILITY) > proto->MaxDurability)
            SetUInt32Value(ITEM_FIELD_DURABILITY, proto->MaxDurability);

        need_save = true;
    }

    // recalculate suffix factor
    if (GetItemRandomPropertyId() < 0)
    {
        if (UpdateItemSuffixFactor())
            need_save = true;
    }

    // update duration if need, and remove if not need
    if ((proto->Duration == 0) != (GetUInt32Value(ITEM_FIELD_DURATION) == 0))
    {
        SetUInt32Value(ITEM_FIELD_DURATION, proto->Duration);
        need_save = true;
    }

    temp_ench_dur_ =
        GetUInt32Value(ITEM_FIELD_ENCHANTMENT_1_1 +
                       TEMP_ENCHANTMENT_SLOT * MAX_ENCHANTMENT_OFFSET +
                       ENCHANTMENT_DURATION_OFFSET);

    // set correct owner
    if (ownerGuid && GetOwnerGuid() != ownerGuid)
    {
        SetOwnerGuid(ownerGuid);
        need_save = true;
    }

    // set correct wrapped state
    if (HasFlag(ITEM_FIELD_FLAGS, ITEM_DYNFLAG_WRAPPED))
    {
        // wrapped item must be wrapper (used version that not stackable)
        if (!(proto->Flags & ITEM_FLAG_WRAPPER) || GetMaxStackCount() > 1)
        {
            RemoveFlag(ITEM_FIELD_FLAGS, ITEM_DYNFLAG_WRAPPED);
            need_save = true;

            static SqlStatementID delGifts;

            // also cleanup for sure gift table
            SqlStatement stmt = CharacterDatabase.CreateStatement(
                delGifts, "DELETE FROM character_gifts WHERE item_guid = ?");
            stmt.PExecute(GetGUIDLow());
        }
    }

    // Fill in bag-related data
    if (proto->InventoryType == INVTYPE_BAG)
    {
        SetUInt32Value(CONTAINER_FIELD_NUM_SLOTS, proto->ContainerSlots);
        for (uint8 i = 0; i < inventory::max_bag_size; ++i)
            SetGuidValue(CONTAINER_FIELD_SLOT_1 + (i * 2), ObjectGuid());
    }

    if (need_save) // normal item changed state set not work at loading
    {
        static SqlStatementID updItem;

        SqlStatement stmt = CharacterDatabase.CreateStatement(updItem,
            "UPDATE item_instance SET data = ?, owner_guid = ? WHERE guid = ?");

        std::ostringstream ss;
        for (uint16 i = 0; i < m_valuesCount; ++i)
            ss << GetUInt32Value(i) << " ";

        stmt.addString(ss);
        stmt.addUInt32(GetOwnerGuid().GetCounter());
        stmt.addUInt32(guidLow);
        stmt.Execute();
    }

    return true;
}

void Item::LoadLootFromDB(Field* fields)
{
    uint32 item_id = fields[1].GetUInt32();
    uint32 item_amount = fields[2].GetUInt32();
    uint32 item_suffix = fields[3].GetUInt32();
    int32 item_propid = fields[4].GetInt32();

    if (!GetOwnerGuid())
        return;

    if (!m_lootDistributor)
        m_lootDistributor = new loot_distributor(this, LOOT_NORMAL_ITEM);

    // money value special case
    if (item_id == 0)
    {
        SetLootState(ITEM_LOOT_UNCHANGED);
        m_lootDistributor->force_gold(item_amount);
        return;
    }

    // normal item case
    ItemPrototype const* proto = ObjectMgr::GetItemPrototype(item_id);

    if (!proto)
    {
        CharacterDatabase.PExecute(
            "DELETE FROM item_loot WHERE guid = '%u' AND itemid = '%u'",
            GetGUIDLow(), item_id);
        logging.error(
            "Item::LoadLootFromDB: %s has an unknown item (id: #%u) in "
            "item_loot, deleted.",
            GetOwnerGuid().GetString().c_str(), item_id);
        return;
    }

    m_lootDistributor->force_add_item(
        LootItem(item_id, item_amount, item_suffix, item_propid));
    m_lootDistributor->force_loot_owner(GetOwnerGuid());

    SetLootState(ITEM_LOOT_UNCHANGED);
}

ItemPrototype const* Item::GetProto() const
{
    return ObjectMgr::GetItemPrototype(GetEntry());
}

Player* Item::GetOwner() const
{
    return sObjectMgr::Instance()->GetPlayer(GetOwnerGuid());
}

uint32 Item::GetSkill()
{
    const ItemPrototype* proto = GetProto();
    if (!proto)
        return 0;
    return proto->GetSkill();
}

uint32 Item::GetSpell()
{
    ItemPrototype const* proto = GetProto();

    switch (proto->Class)
    {
    case ITEM_CLASS_WEAPON:

        switch (proto->SubClass)
        {
        case ITEM_SUBCLASS_WEAPON_AXE:
            return 196;
        case ITEM_SUBCLASS_WEAPON_AXE2:
            return 197;
        case ITEM_SUBCLASS_WEAPON_BOW:
            return 264;
        case ITEM_SUBCLASS_WEAPON_GUN:
            return 266;
        case ITEM_SUBCLASS_WEAPON_MACE:
            return 198;
        case ITEM_SUBCLASS_WEAPON_MACE2:
            return 199;
        case ITEM_SUBCLASS_WEAPON_POLEARM:
            return 200;
        case ITEM_SUBCLASS_WEAPON_SWORD:
            return 201;
        case ITEM_SUBCLASS_WEAPON_SWORD2:
            return 202;
        case ITEM_SUBCLASS_WEAPON_STAFF:
            return 227;
        case ITEM_SUBCLASS_WEAPON_DAGGER:
            return 1180;
        case ITEM_SUBCLASS_WEAPON_THROWN:
            return 2567;
        case ITEM_SUBCLASS_WEAPON_SPEAR:
            return 3386;
        case ITEM_SUBCLASS_WEAPON_CROSSBOW:
            return 5011;
        case ITEM_SUBCLASS_WEAPON_WAND:
            return 5009;
        default:
            return 0;
        }
    case ITEM_CLASS_ARMOR:
        switch (proto->SubClass)
        {
        case ITEM_SUBCLASS_ARMOR_CLOTH:
            return 9078;
        case ITEM_SUBCLASS_ARMOR_LEATHER:
            return 9077;
        case ITEM_SUBCLASS_ARMOR_MAIL:
            return 8737;
        case ITEM_SUBCLASS_ARMOR_PLATE:
            return 750;
        case ITEM_SUBCLASS_ARMOR_SHIELD:
            return 9116;
        default:
            return 0;
        }
    }
    return 0;
}

int32 Item::GenerateItemRandomPropertyId(uint32 item_id)
{
    ItemPrototype const* itemProto =
        sItemStorage.LookupEntry<ItemPrototype>(item_id);

    if (!itemProto)
        return 0;

    // item must have one from this field values not null if it can have random
    // enchantments
    if ((!itemProto->RandomProperty) && (!itemProto->RandomSuffix))
        return 0;

    // Random Property case
    if (itemProto->RandomProperty)
    {
        uint32 randomPropId = GetItemEnchantMod(itemProto->RandomProperty);
        ItemRandomPropertiesEntry const* random_id =
            sItemRandomPropertiesStore.LookupEntry(randomPropId);
        if (!random_id)
        {
            logging.error(
                "Enchantment id #%u used but it doesn't have records in "
                "'ItemRandomProperties.dbc'",
                randomPropId);
            return 0;
        }

        return random_id->ID;
    }
    // Random Suffix case
    else
    {
        uint32 randomPropId = GetItemEnchantMod(itemProto->RandomSuffix);
        ItemRandomSuffixEntry const* random_id =
            sItemRandomSuffixStore.LookupEntry(randomPropId);
        if (!random_id)
        {
            logging.error(
                "Enchantment id #%u used but it doesn't have records in "
                "sItemRandomSuffixStore.",
                randomPropId);
            return 0;
        }

        return -int32(random_id->ID);
    }
}

void Item::SetItemRandomProperties(int32 randomPropId)
{
    if (!randomPropId)
        return;

    if (randomPropId > 0)
    {
        ItemRandomPropertiesEntry const* item_rand =
            sItemRandomPropertiesStore.LookupEntry(randomPropId);
        if (item_rand)
        {
            if (GetInt32Value(ITEM_FIELD_RANDOM_PROPERTIES_ID) !=
                int32(item_rand->ID))
            {
                SetInt32Value(ITEM_FIELD_RANDOM_PROPERTIES_ID, item_rand->ID);
                should_save_ = true;
            }
            for (uint32 i = PROP_ENCHANTMENT_SLOT_2;
                 i < PROP_ENCHANTMENT_SLOT_2 + 3; ++i)
                SetEnchantment(EnchantmentSlot(i),
                    item_rand->enchant_id[i - PROP_ENCHANTMENT_SLOT_2], 0, 0);
        }
    }
    else
    {
        ItemRandomSuffixEntry const* item_rand =
            sItemRandomSuffixStore.LookupEntry(-randomPropId);
        if (item_rand)
        {
            if (GetInt32Value(ITEM_FIELD_RANDOM_PROPERTIES_ID) !=
                    -int32(item_rand->ID) ||
                !GetItemSuffixFactor())
            {
                SetInt32Value(
                    ITEM_FIELD_RANDOM_PROPERTIES_ID, -int32(item_rand->ID));
                UpdateItemSuffixFactor();
                should_save_ = true;
            }

            for (uint32 i = PROP_ENCHANTMENT_SLOT_0;
                 i < PROP_ENCHANTMENT_SLOT_0 + 3; ++i)
                SetEnchantment(EnchantmentSlot(i),
                    item_rand->enchant_id[i - PROP_ENCHANTMENT_SLOT_0], 0, 0);
        }
    }
}

bool Item::UpdateItemSuffixFactor()
{
    uint32 suffixFactor = GenerateEnchSuffixFactor(GetEntry());
    if (GetItemSuffixFactor() == suffixFactor)
        return false;
    SetUInt32Value(ITEM_FIELD_PROPERTY_SEED, suffixFactor);
    return true;
}

bool Item::IsEquipped() const
{
    // XXX: stupid proxy function
    return slot_.equipment();
}

bool Item::IsFitToSpellRequirements(SpellEntry const* spellInfo) const
{
    ItemPrototype const* proto = GetProto();

    if (spellInfo->EquippedItemClass != -1) // -1 == any item class
    {
        if (spellInfo->EquippedItemClass != int32(proto->Class))
            return false; //  wrong item class

        if (spellInfo->EquippedItemSubClassMask != 0) // 0 == any subclass
        {
            if ((spellInfo->EquippedItemSubClassMask &
                    (1 << proto->SubClass)) == 0)
                return false; // subclass not present in mask
        }
    }

    // Only check for item enchantments (TARGET_FLAG_ITEM), all other spells are
    // either NPC spells
    // or spells where slot requirements are already handled with AttributesEx3
    // fields
    // and special code (Titan's Grip, Windfury Attack). Check clearly not
    // applicable for Lava Lash.
    if (spellInfo->EquippedItemInventoryTypeMask != 0 &&
        (spellInfo->Targets & TARGET_FLAG_ITEM)) // 0 == any inventory type
    {
        if ((spellInfo->EquippedItemInventoryTypeMask &
                (1 << proto->InventoryType)) == 0)
            return false; // inventory type not present in mask
    }

    return true;
}

bool Item::IsTargetValidForItemUse(Unit* pUnitTarget)
{
    ItemRequiredTargetMapBounds bounds =
        sObjectMgr::Instance()->GetItemRequiredTargetMapBounds(
            GetProto()->ItemId);

    if (bounds.first == bounds.second)
        return true;

    if (!pUnitTarget)
        return false;

    for (auto itr = bounds.first; itr != bounds.second; ++itr)
        if (itr->second.IsFitToRequirements(pUnitTarget))
            return true;

    return false;
}

void Item::SetEnchantment(
    EnchantmentSlot slot, uint32 id, uint32 duration, uint32 charges)
{
    // Better lost small time at check in comparison lost time at item save to
    // DB.
    if ((GetEnchantmentId(slot) == id) &&
        (GetEnchantmentDuration(slot) == duration) &&
        (GetEnchantmentCharges(slot) == charges))
        return;

    // Update soul-bound status if it changes due to this enchantment
    if (!IsSoulBound())
    {
        if (const SpellItemEnchantmentEntry* ench_entry =
                sSpellItemEnchantmentStore.LookupEntry(id))
            if (ench_entry->slot & ENCHANTMENT_CAN_SOULBOUND)
                SetBinding(true);
    }

    SetUInt32Value(ITEM_FIELD_ENCHANTMENT_1_1 + slot * MAX_ENCHANTMENT_OFFSET +
                       ENCHANTMENT_ID_OFFSET,
        id);
    SetUInt32Value(ITEM_FIELD_ENCHANTMENT_1_1 + slot * MAX_ENCHANTMENT_OFFSET +
                       ENCHANTMENT_DURATION_OFFSET,
        duration);
    SetUInt32Value(ITEM_FIELD_ENCHANTMENT_1_1 + slot * MAX_ENCHANTMENT_OFFSET +
                       ENCHANTMENT_CHARGES_OFFSET,
        charges);

    if (slot == TEMP_ENCHANTMENT_SLOT)
    {
        temp_ench_dur_ = duration;
        if (duration > 0)
        {
            if (Player* owner = GetOwner())
                owner->TrackEnchDurations(this, true);
        }
    }

    should_save_ = true;
}

void Item::SetEnchantmentDuration(EnchantmentSlot slot, uint32 duration)
{
    if (GetUInt32Value(ITEM_FIELD_ENCHANTMENT_1_1 +
                       slot * MAX_ENCHANTMENT_OFFSET +
                       ENCHANTMENT_DURATION_OFFSET) == duration)
        return;

    if (slot == TEMP_ENCHANTMENT_SLOT)
        temp_ench_dur_ = duration;

    SetUInt32Value(ITEM_FIELD_ENCHANTMENT_1_1 + slot * MAX_ENCHANTMENT_OFFSET +
                       ENCHANTMENT_DURATION_OFFSET,
        duration);
    should_save_ = true;
}

void Item::SetEnchantmentCharges(EnchantmentSlot slot, uint32 charges)
{
    if (GetEnchantmentCharges(slot) == charges)
        return;

    SetUInt32Value(ITEM_FIELD_ENCHANTMENT_1_1 + slot * MAX_ENCHANTMENT_OFFSET +
                       ENCHANTMENT_CHARGES_OFFSET,
        charges);
    should_save_ = true;
}

void Item::ClearEnchantment(EnchantmentSlot slot)
{
    if (!GetEnchantmentId(slot))
        return;

    for (uint8 x = 0; x < 3; ++x)
        SetUInt32Value(
            ITEM_FIELD_ENCHANTMENT_1_1 + slot * MAX_ENCHANTMENT_OFFSET + x, 0);

    if (slot == TEMP_ENCHANTMENT_SLOT)
        temp_ench_dur_ = 0;

    should_save_ = true;
}

void Item::update_gem_bonus()
{
    Player* owner = GetOwner();
    uint32 socket_bonus = GetProto()->socketBonus;
    if (!owner || socket_bonus == 0)
        return;

    // Clear socket bonus if it's active currently
    if (GetEnchantmentId(BONUS_ENCHANTMENT_SLOT))
    {
        if (slot().equipment())
            owner->ApplyEnchantment(
                this, BONUS_ENCHANTMENT_SLOT, false, slot());
        SetEnchantment(BONUS_ENCHANTMENT_SLOT, 0, 0, 0);
    }

    for (int i = 0; i < MAX_GEM_SOCKETS; ++i)
    {
        uint32 socket_color = GetProto()->Socket[i].Color;
        if (socket_color == 0)
            break; // No more sockets in this item

        uint32 ench_id = GetEnchantmentId(
            static_cast<EnchantmentSlot>(SOCK_ENCHANTMENT_SLOT + i));
        if (!ench_id)
            return;

        const SpellItemEnchantmentEntry* entry =
            sSpellItemEnchantmentStore.LookupEntry(ench_id);
        if (!entry)
            return;

        // We don't need to check conditions, because only meta-gems have
        // conditions, and they are activation conditions, not relevant to gem
        // bonuses

        uint32 gem_color =
            sGemPropertiesStore.LookupEntry(ObjectMgr::GetItemPrototype(
                                                entry->GemID)->GemProperties)
                ->color;

        if ((socket_color & gem_color) == 0)
            return;
    }

    // All socket colors match, apply bonus
    SetEnchantment(BONUS_ENCHANTMENT_SLOT, socket_bonus, 0, 0);
    if (slot().equipment())
        owner->ApplyEnchantment(this, BONUS_ENCHANTMENT_SLOT, true, slot());
}

bool Item::IsLimitedToAnotherMapOrZone(
    uint32 cur_mapId, uint32 cur_zoneId) const
{
    ItemPrototype const* proto = GetProto();
    return proto && ((proto->Map && proto->Map != cur_mapId) ||
                        (proto->Area && proto->Area != cur_zoneId));
}

uint32 Item::GetEnchantmentDuration(EnchantmentSlot slot)
{
    if (slot == TEMP_ENCHANTMENT_SLOT)
        return temp_ench_dur_;
    return GetUInt32Value(ITEM_FIELD_ENCHANTMENT_1_1 +
                          slot * MAX_ENCHANTMENT_OFFSET +
                          ENCHANTMENT_DURATION_OFFSET);
}

// Though the client has the information in the item's data field,
// we have to send SMSG_ITEM_TIME_UPDATE to display the remaining
// time.
void Item::SendDuration(Player* owner)
{
    uint32 duration = GetUInt32Value(ITEM_FIELD_DURATION);
    if (!duration)
        return;

    WorldPacket data(SMSG_ITEM_TIME_UPDATE, (8 + 4));
    data << ObjectGuid(GetObjectGuid());
    data << uint32(duration);
    owner->GetSession()->send_packet(std::move(data));
}

Item* Item::CreateItem(
    uint32 item, uint32 count, Player const* player, uint32 randomPropertyId)
{
    if (count < 1)
        return nullptr; // don't create item at zero count

    if (ItemPrototype const* pProto = ObjectMgr::GetItemPrototype(item))
    {
        if (count > pProto->GetMaxStackSize())
            count = pProto->GetMaxStackSize();

        assert(count != 0 &&
               "pProto->Stackable == 0 but checked at loading already");
        /*XXX:*/
        auto pItem = new Item(pProto);
        if (pItem->Create(
                sObjectMgr::Instance()->GenerateItemLowGuid(), item, player))
        {
            pItem->SetCount(count);
            if (uint32 randId = randomPropertyId ?
                                    randomPropertyId :
                                    Item::GenerateItemRandomPropertyId(item))
                pItem->SetItemRandomProperties(randId);

            return pItem;
        }
        else
            delete pItem;
    }
    return nullptr;
}

Item* Item::CloneItem(uint32 count, Player const* player) const
{
    Item* newItem =
        CreateItem(GetEntry(), count, player, GetItemRandomPropertyId());
    if (!newItem)
        return nullptr;

    newItem->SetGuidValue(ITEM_FIELD_CREATOR, GetGuidValue(ITEM_FIELD_CREATOR));
    newItem->SetGuidValue(
        ITEM_FIELD_GIFTCREATOR, GetGuidValue(ITEM_FIELD_GIFTCREATOR));
    newItem->SetUInt32Value(ITEM_FIELD_FLAGS, GetUInt32Value(ITEM_FIELD_FLAGS));
    newItem->SetUInt32Value(
        ITEM_FIELD_DURATION, GetUInt32Value(ITEM_FIELD_DURATION));
    return newItem;
}

void Item::AddToClientUpdateList()
{
    if (Player* pl = GetOwner())
        pl->GetMap()->AddUpdateObject(this);
}

void Item::RemoveFromClientUpdateList()
{
    if (Player* pl = GetOwner())
        pl->GetMap()->RemoveUpdateObject(this);
}

void Item::BuildUpdateData(UpdateDataMapType& update_players)
{
    if (Player* pl = GetOwner())
        BuildUpdateDataForPlayer(pl, update_players);

    ClearUpdateMask(false);
}

bool ItemRequiredTarget::IsFitToRequirements(Unit* pUnitTarget) const
{
    if (pUnitTarget->GetTypeId() != TYPEID_UNIT)
        return false;

    if (pUnitTarget->GetEntry() != m_uiTargetEntry)
        return false;

    switch (m_uiType)
    {
    case ITEM_TARGET_TYPE_CREATURE:
        return pUnitTarget->isAlive();
    case ITEM_TARGET_TYPE_DEAD:
        return !pUnitTarget->isAlive();
    default:
        return false;
    }
}

void Item::SetLootState(ItemLootUpdateState state)
{
    // ITEM_LOOT_NONE -> ITEM_LOOT_TEMPORARY -> ITEM_LOOT_NONE
    // ITEM_LOOT_NONE -> ITEM_LOOT_NEW -> ITEM_LOOT_NONE
    // ITEM_LOOT_NONE -> ITEM_LOOT_NEW -> ITEM_LOOT_UNCHANGED [<->
    // ITEM_LOOT_CHANGED] -> ITEM_LOOT_REMOVED -> ITEM_LOOT_NONE
    switch (state)
    {
    case ITEM_LOOT_NONE:
    case ITEM_LOOT_NEW:
        assert(false); // not used in state change calls return;
    case ITEM_LOOT_TEMPORARY:
        assert(m_lootState ==
               ITEM_LOOT_NONE); // called only for not generated yet loot case
        m_lootState = ITEM_LOOT_TEMPORARY;
        break;
    case ITEM_LOOT_CHANGED:
        // new loot must stay in new state until saved, temporary must stay
        // until remove
        if (m_lootState != ITEM_LOOT_NEW && m_lootState != ITEM_LOOT_TEMPORARY)
            m_lootState = m_lootState == ITEM_LOOT_NONE ? ITEM_LOOT_NEW : state;
        break;
    case ITEM_LOOT_UNCHANGED:
        // expected that called after DB update or load
        if (m_lootState == ITEM_LOOT_REMOVED)
            m_lootState = ITEM_LOOT_NONE;
        // temporary must stay until remove (ignore any changes)
        else if (m_lootState != ITEM_LOOT_TEMPORARY)
            m_lootState = ITEM_LOOT_UNCHANGED;
        break;
    case ITEM_LOOT_REMOVED:
        // if loot not saved then it existence in past can be just ignored
        if (m_lootState == ITEM_LOOT_NEW || m_lootState == ITEM_LOOT_TEMPORARY)
        {
            m_lootState = ITEM_LOOT_NONE;
            return;
        }

        m_lootState = ITEM_LOOT_REMOVED;
        break;
    }

    if (m_lootState != ITEM_LOOT_NONE && m_lootState != ITEM_LOOT_UNCHANGED &&
        m_lootState != ITEM_LOOT_TEMPORARY)
        should_save_ = true;
}

void Item::OnLootOpen(LootType lootType, Player* looter)
{
    if (!HasGeneratedLoot())
    {
        DeleteLootDistributor();

        switch (lootType)
        {
        case LOOT_DISENCHANTING:
            m_lootDistributor = new loot_distributor(this, lootType);
            m_lootDistributor->generate_loot(looter);
            SetLootState(ITEM_LOOT_TEMPORARY);
            break;
        case LOOT_PROSPECTING:
            m_lootDistributor = new loot_distributor(this, lootType);
            m_lootDistributor->generate_loot(looter);
            SetLootState(ITEM_LOOT_TEMPORARY);
            break;
        default:
            m_lootDistributor = new loot_distributor(this, LOOT_NORMAL_ITEM);
            m_lootDistributor->generate_loot(looter);
            SetLootState(ITEM_LOOT_CHANGED);
            break;
        }
    }
}

bool Item::can_put_in_trade() const
{
    // Can't trade if item has been looted and its loot generated
    if (HasGeneratedLoot())
        return false;

    if (IsSoulBound())
        return false;

    // Can't trade if no one owns this item
    Player* owner = GetOwner();
    if (!owner)
        return false;

    InventoryResult res = owner->storage().can_delete_item(slot_);
    if (res != EQUIP_ERR_OK)
        return false;

    // Can't trade if item is currently looted
    if (owner->GetLootGuid() == GetObjectGuid())
        return false;

    return true;
}

void Item::db_insert(Player* owner)
{
    static SqlStatementID insert_item;

    SetEnchantmentDuration(TEMP_ENCHANTMENT_SLOT, temp_ench_dur_);

    std::ostringstream ss;
    for (uint16 i = 0; i < m_valuesCount; ++i)
        ss << GetUInt32Value(i) << " ";

    SqlStatement stmt = CharacterDatabase.CreateStatement(insert_item,
        "INSERT INTO item_instance (guid, owner_guid, data) VALUES (?, ?, ?)");
    stmt.PExecute(GetGUIDLow(), GetOwnerGuid().GetCounter(), ss.str().c_str());

    db_save_loot(owner);
    should_save_ = false;
    in_db_ = true;
}

void Item::db_update(Player* owner)
{
    static SqlStatementID update_item;
    static SqlStatementID update_gifts;

    SetEnchantmentDuration(TEMP_ENCHANTMENT_SLOT, temp_ench_dur_);

    std::ostringstream ss;
    for (uint16 i = 0; i < m_valuesCount; ++i)
        ss << GetUInt32Value(i) << " ";

    SqlStatement stmt = CharacterDatabase.CreateStatement(update_item,
        "UPDATE item_instance SET data = ?, owner_guid = ? WHERE guid = ?");
    stmt.PExecute(ss.str().c_str(), GetOwnerGuid().GetCounter(), GetGUIDLow());

    if (HasFlag(ITEM_FIELD_FLAGS, ITEM_DYNFLAG_WRAPPED))
    {
        stmt = CharacterDatabase.CreateStatement(update_gifts,
            "UPDATE character_gifts SET guid = ? WHERE item_guid = ?");
        stmt.PExecute(GetOwnerGuid().GetCounter(), GetGUIDLow());
    }

    db_save_loot(owner);
    should_save_ = false;
}

void Item::db_delete()
{
    // We must remove ourselves from any pending updates
    RemoveFromClientUpdateList();

    if (!in_db_)
        return;

    // NOTE: If more item related tables are added you need to fix both here in
    // the Item::db_* functions
    //       AND also in the inventory::personal_storage::load() & the
    //       inventoy::guild_storage::load() function (edge case where prototype
    //       doesn't exist)
    static SqlStatementID delete_item_text;
    static SqlStatementID delete_item_instance;
    static SqlStatementID delete_character_gifts;
    static SqlStatementID delete_item_loot;

    if (uint32 item_text_id = GetUInt32Value(ITEM_FIELD_ITEM_TEXT_ID))
    {
        SqlStatement stmt = CharacterDatabase.CreateStatement(
            delete_item_text, "DELETE FROM item_text WHERE id = ?");
        stmt.PExecute(item_text_id);
    }

    SqlStatement stmt = CharacterDatabase.CreateStatement(
        delete_item_instance, "DELETE FROM item_instance WHERE guid = ?");
    stmt.PExecute(GetGUIDLow());

    if (HasFlag(ITEM_FIELD_FLAGS, ITEM_DYNFLAG_WRAPPED))
    {
        stmt = CharacterDatabase.CreateStatement(delete_character_gifts,
            "DELETE FROM character_gifts WHERE item_guid = ?");
        stmt.PExecute(GetGUIDLow());
    }

    if (HasSavedLoot())
    {
        stmt = CharacterDatabase.CreateStatement(
            delete_item_loot, "DELETE FROM item_loot WHERE guid = ?");
        stmt.PExecute(GetGUIDLow());
    }

    in_db_ = false;
}

void Item::db_save_loot(Player* owner)
{
    if (m_lootState == ITEM_LOOT_CHANGED || m_lootState == ITEM_LOOT_REMOVED)
    {
        static SqlStatementID delete_item_loot;

        SqlStatement stmt = CharacterDatabase.CreateStatement(
            delete_item_loot, "DELETE FROM item_loot WHERE guid = ?");
        stmt.PExecute(GetGUIDLow());
    }

    ItemLootUpdateState previous = m_lootState;
    if (m_lootState != ITEM_LOOT_NONE && m_lootState != ITEM_LOOT_TEMPORARY)
        SetLootState(ITEM_LOOT_UNCHANGED);

    if (!owner)
        owner = GetOwner();
    if ((previous != ITEM_LOOT_NEW && previous != ITEM_LOOT_CHANGED) || !owner)
        return;

    if (!m_lootDistributor || !m_lootDistributor->loot())
        return;

    static SqlStatementID insert_loot_gold;
    static SqlStatementID insert_loot_items;

    // Money is indicated by an itemid that is 0
    if (uint32 gold = m_lootDistributor->loot()->gold())
    {
        SqlStatement stmt = CharacterDatabase.CreateStatement(insert_loot_gold,
            "INSERT INTO item_loot (guid, owner_guid, itemid, amount, suffix, "
            "property) VALUES (?, ?, 0, ?, 0, 0)");
        stmt.PExecute(GetGUIDLow(), owner->GetGUIDLow(), gold);
    }

    SqlStatement stmt = CharacterDatabase.CreateStatement(insert_loot_items,
        "INSERT INTO item_loot (guid, owner_guid, itemid, amount, suffix, "
        "property) VALUES (?, ?, ?, ?, ?, ?)");

    // Insert all items and quest items
    for (size_t i = 0; i < m_lootDistributor->loot()->size(); ++i)
    {
        const LootItem* item = m_lootDistributor->loot()->get_slot_item(i);
        if (!item)
            continue;

        if (item->is_blocked || item->is_looted)
            continue;

        stmt.addUInt32(GetGUIDLow());
        stmt.addUInt32(owner->GetGUIDLow());
        stmt.addUInt32(item->itemid);
        stmt.addUInt8(item->count);
        stmt.addUInt32(item->randomSuffix);
        stmt.addInt32(item->randomPropertyId);

        stmt.Execute();
    }
}

bool Item::DropSpellCharge()
{
    const ItemPrototype* prototype = GetProto();

    bool empty = false,
         deletable = (prototype->ExtraFlags & ITEM_EXTRA_NON_CONSUMABLE) == 0;

    for (int i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
    {
        if (prototype->Spells[i].SpellId == 0 ||
            prototype->Spells[i].SpellCharges == 0)
            continue;

        if (prototype->Spells[i].SpellCharges > 0)
            deletable = false;

        if (int32 charges = GetSpellCharges(i))
        {
            // Item has charges left, deplete one
            if (prototype->Stackable == 1)
            {
                SetSpellCharges(i, charges > 0 ? --charges : ++charges);
                mark_for_save();
                empty = (GetSpellCharges(i) == 0);
            }
            else
            {
                empty =
                    true; // If item stacks it cannot have more than one charge
            }
        }
    }

    return empty && deletable;
}

void Item::on_stack_to(Item* target)
{
    for (auto& s : referencing_spells_)
        s->set_cast_item(target);
    referencing_spells_.clear();
}

void Item::visualize_gems(Player* owner) const
{
    if (!owner)
        owner = GetOwner();

    if (!owner || GetOwnerGuid() != owner->GetObjectGuid() ||
        !slot().equipment())
        return;

    uint32 base = item_field_offset(slot().index());
    owner->SetUInt32Value(
        base + PLAYER_VISIBLE_ITEM_ENCHANTS + SOCK_ENCHANTMENT_SLOT,
        GetEnchantmentId(SOCK_ENCHANTMENT_SLOT));
    owner->SetUInt32Value(
        base + PLAYER_VISIBLE_ITEM_ENCHANTS + SOCK_ENCHANTMENT_SLOT_2,
        GetEnchantmentId(SOCK_ENCHANTMENT_SLOT_2));
    owner->SetUInt32Value(
        base + PLAYER_VISIBLE_ITEM_ENCHANTS + SOCK_ENCHANTMENT_SLOT_3,
        GetEnchantmentId(SOCK_ENCHANTMENT_SLOT_3));
}
