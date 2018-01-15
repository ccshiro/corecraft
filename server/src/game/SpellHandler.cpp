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

#include "Common.h"
#include "DBCStores.h"
#include "logging.h"
#include "ObjectMgr.h"
#include "Opcodes.h"
#include "ScriptMgr.h"
#include "Spell.h"
#include "SpellAuras.h"
#include "SpellMgr.h"
#include "Totem.h"
#include "WorldPacket.h"
#include "WorldSession.h"

void WorldSession::HandleUseItemOpcode(WorldPacket& recv_data)
{
    uint8 bag, index;
    uint8 spell_count; // number of spells at item, not used
    uint8 cast_count;  // next cast if exists (single or not)
    ObjectGuid item_guid;
    recv_data >> bag >> index >> spell_count >> cast_count >> item_guid;
    inventory::slot slot(inventory::personal_slot, bag, index);

    // Packet not fully read: Any preemptive return needs to edit read pos

    // ignore for remote control state
    if (!_player->IsMovingSelf())
    {
        recv_data.rpos(recv_data.wpos());
        return;
    }

    // Note: We get the item from our inventory, so the item is always ours.
    Item* item = _player->storage().get(slot);
    if (!item)
    {
        recv_data.rpos(recv_data.wpos());
        _player->SendEquipError(EQUIP_ERR_ITEM_NOT_FOUND, nullptr);
        return;
    }

    if (item->GetObjectGuid() != item_guid)
    {
        recv_data.rpos(recv_data.wpos());
        _player->SendEquipError(EQUIP_ERR_ITEM_NOT_FOUND, nullptr);
        return;
    }

    if (item->in_trade())
    {
        recv_data.rpos(recv_data.wpos());
        return; // No need for an error, a legit client cannot do this
    }

    const ItemPrototype* proto = item->GetProto();

    // some item classes can be used only in equipped state
    if (proto->InventoryType != INVTYPE_NON_EQUIP && !item->IsEquipped())
    {
        recv_data.rpos(recv_data.wpos());
        _player->SendEquipError(EQUIP_ERR_ITEM_NOT_FOUND, item);
        return;
    }

    InventoryResult msg = _player->can_use_item(proto);
    if (msg != EQUIP_ERR_OK)
    {
        recv_data.rpos(recv_data.wpos());
        _player->SendEquipError(msg, item);
        return;
    }

    // Don't allow consumables without the usable in arena flag inside an arena
    if (proto->Class == ITEM_CLASS_CONSUMABLE &&
        !(proto->Flags & ITEM_FLAG_USEABLE_IN_ARENA) && _player->InArena())
    {
        recv_data.rpos(recv_data.wpos());
        _player->SendEquipError(EQUIP_ERR_NOT_DURING_ARENA_MATCH, item);
        return;
    }

    // Check if item has a non-combat spell; in which case we can't use it if in
    // combat
    if (_player->isInCombat())
    {
        for (int i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
        {
            if (const SpellEntry* spell_info =
                    sSpellStore.LookupEntry(proto->Spells[i].SpellId))
            {
                if (IsNonCombatSpell(spell_info))
                {
                    recv_data.rpos(recv_data.wpos());
                    _player->SendEquipError(EQUIP_ERR_NOT_IN_COMBAT, item);
                    return;
                }
            }
        }
    }

    // All checks have passed. Below is code for actually using the item.

    // Bind the item if it's bound when used
    if (proto->Bonding == BIND_WHEN_USE && !item->IsSoulBound())
    {
        item->SetBinding(true);
        item->mark_for_save();
    }

    SpellCastTargets targets;
    recv_data >> targets.ReadForCaster(_player);
    targets.Update(_player);

    if (!item->IsTargetValidForItemUse(targets.getUnitTarget()))
    {
        // Remove the gray-out of the item for the client, but don't report a
        // visible equip error
        _player->SendEquipError(EQUIP_ERR_NONE, item);

        // Find the first spell that is triggered on use
        uint32 spell_id = 0;
        for (int i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
        {
            if (proto->Spells[i].SpellTrigger == ITEM_SPELLTRIGGER_ON_USE)
            {
                spell_id = proto->Spells[i].SpellId;
                break;
            }
        }

        // Use that spell id to report the invalid targets status
        if (const SpellEntry* spell_info = sSpellStore.LookupEntry(spell_id))
            Spell::SendCastResult(
                _player, spell_info, cast_count, SPELL_FAILED_BAD_TARGETS);
        return;
    }

    // Note: The script is responsible for sending equipment errors to remove
    // the gray-out of the client if it returns true
    if (!sScriptMgr::Instance()->OnItemUse(_player, item, targets))
    {
        // No script intercepted the processing of this spell, which means we
        // process it with our default code
        _player->CastItemUseSpell(item, targets, cast_count);
    }
}

#define OPEN_CHEST 11437
#define OPEN_SAFE 11535
#define OPEN_CAGE 11792
#define OPEN_BOOTY_CHEST 5107
#define OPEN_STRONGBOX 8517

void WorldSession::HandleOpenItemOpcode(WorldPacket& recv_data)
{
    uint8 bag, index;
    recv_data >> bag >> index;
    inventory::slot slot(inventory::personal_slot, bag, index);

    // Can only open items that we have on us (as opposed to in our bank, for
    // example)
    if (!slot.on_body())
        return;

    // ignore for remote control state
    if (!_player->IsMovingSelf())
        return;

    Item* item = _player->storage().get(slot);
    if (!item)
        return;

    // Must be able to delete (consume) this item to open it
    InventoryResult err = _player->storage().can_delete_item(slot);
    if (err != EQUIP_ERR_OK)
        return _player->SendEquipError(err, item);

    // Process gifts (wrapped items)
    if (item->GetGuidValue(ITEM_FIELD_GIFTCREATOR) ||
        item->HasFlag(ITEM_FIELD_FLAGS, ITEM_DYNFLAG_WRAPPED))
    {
        std::unique_ptr<QueryResult> result(CharacterDatabase.PQuery(
            "SELECT entry, flags FROM character_gifts WHERE item_guid = %u",
            item->GetGUIDLow()));
        if (!result)
        {
            // No content was found in the database, delete item and fail
            inventory::transaction trans(false);
            trans.destroy(item);
            _player->storage().finalize(trans);
            return;
        }

        Field* fields = result->Fetch();
        item->SetGuidValue(ITEM_FIELD_GIFTCREATOR, ObjectGuid());
        item->SetEntry(fields[0].GetUInt32());
        item->RemoveFlag(ITEM_FIELD_FLAGS, ITEM_DYNFLAG_WRAPPED);
        item->SetUInt32Value(ITEM_FIELD_FLAGS, fields[1].GetUInt32());
        item->mark_for_save();

        // Delete gifts from the database
        static SqlStatementID delete_gift;
        SqlStatement stmt = CharacterDatabase.CreateStatement(
            delete_gift, "DELETE FROM character_gifts WHERE item_guid = ?");
        stmt.PExecute(item->GetGUIDLow());

        // Return, no more gift-related processing
        return;
    }

    const ItemPrototype* prototype = item->GetProto();

    // If we have a lock id and we're not flagged as unlocked we check if the
    // lock does indeed
    // require lockpicking, if so, we can't open it right now
    if (prototype->LockID &&
        !item->HasFlag(ITEM_FIELD_FLAGS, ITEM_DYNFLAG_UNLOCKED))
    {
        const LockEntry* lock_info = sLockStore.LookupEntry(prototype->LockID);
        if (!lock_info)
            return logging.error(
                "WorldSession::HandleOpenItemOpcode: Lock Info not found in "
                "Lock Store for item with id: %u",
                item->GetEntry());
        // Requires lockpicking?
        if (lock_info->Skill[1] || lock_info->Skill[0])
            return _player->SendEquipError(EQUIP_ERR_ITEM_LOCKED, item);
    }

    // This opcode is not usable for disenchanting or jewelcrafting, only for
    // opening container-esque
    // items. Which means all we need to check is if this item has entries in
    // LootTemplates_Item or money loot
    const LootTemplate* t = LootTemplates_Item.GetLootFor(item->GetEntry());
    uint32 money = item->GetProto()->MaxMoneyLoot;
    if (!t && !money)
        return _player->SendEquipError(EQUIP_ERR_LOOT_CANT_LOOT_THAT_NOW, item);

    _player->SendLoot(item->GetObjectGuid(), LOOT_CORPSE);
}

void WorldSession::HandleGameObjectUseOpcode(WorldPacket& recv_data)
{
    ObjectGuid guid;

    recv_data >> guid;

    // ignore for remote control state
    if (!_player->IsMovingSelf())
        return;

    GameObject* obj = GetPlayer()->GetMap()->GetGameObject(guid);
    if (!obj)
        return;

    // Additional check preventing exploits (ie loot despawned chests)
    if (!obj->isSpawned())
    {
        logging.error(
            "HandleGameObjectUseOpcode: CMSG_GAMEOBJ_USE for despawned "
            "GameObject (Entry %u), didn't expect this to happen.",
            obj->GetEntry());
        return;
    }

    // Never expect this opcode for some type GO's
    if (obj->GetGoType() == GAMEOBJECT_TYPE_GENERIC)
    {
        logging.error(
            "HandleGameObjectUseOpcode: CMSG_GAMEOBJ_USE for not allowed "
            "GameObject type %u (Entry %u), didn't expect this to happen.",
            obj->GetGoType(), obj->GetEntry());
        return;
    }

    // Never expect this opcode for non intractable GO's
    if (obj->HasFlag(GAMEOBJECT_FLAGS, GO_FLAG_NO_INTERACT))
    {
        logging.error(
            "HandleGameObjectUseOpcode: CMSG_GAMEOBJ_USE for GameObject (Entry "
            "%u) with non intractable flag (Flags %u), didn't expect this to "
            "happen.",
            obj->GetEntry(), obj->GetUInt32Value(GAMEOBJECT_FLAGS));
        return;
    }

    obj->Use(_player);
}

void WorldSession::HandleCastSpellOpcode(WorldPacket& recvPacket)
{
    uint32 spellId;
    uint8 cast_count;
    recvPacket >> spellId;
    recvPacket >> cast_count;

    // ignore for remote control state (for player case)
    Unit* moving = _player->GetMovingUnit();
    if (moving != _player && moving->GetTypeId() == TYPEID_PLAYER)
    {
        recvPacket.rpos(recvPacket.wpos()); // prevent spam at ignore packet
        return;
    }

    LOG_DEBUG(logging,
        "WORLD: got cast spell packet, spellId - %u, cast_count: %u data "
        "length = %i",
        spellId, cast_count, (uint32)recvPacket.size());

    SpellEntry const* spellInfo = sSpellStore.LookupEntry(spellId);

    if (!spellInfo)
    {
        logging.error("WORLD: unknown spell id %u", spellId);
        recvPacket.rpos(recvPacket.wpos()); // prevent spam at ignore packet
        return;
    }

    // Auto repeat spells ignored if casted already (need to be cancelled with
    // CMSG_CANCEL_AUTO_REPEAT_SPELL). This makes "/cast !Auto Shot" work
    // correctly.
    if (spellInfo->HasAttribute(SPELL_ATTR_EX2_AUTOREPEAT_FLAG) &&
        moving->GetCurrentSpell(CURRENT_AUTOREPEAT_SPELL))
    {
        recvPacket.rpos(recvPacket.wpos()); // prevent spam at ignore packet
        return;
    }

    if (moving->GetTypeId() == TYPEID_PLAYER)
    {
        // not have spell in spellbook or spell passive and not casted by client
        if (!((Player*)moving)->HasActiveSpell(spellId) ||
            IsPassiveSpell(spellInfo))
        {
            logging.error(
                "World: Player %u casts spell %u which he shouldn't have",
                moving->GetGUIDLow(), spellId);
            // cheater? kick? ban?
            recvPacket.rpos(recvPacket.wpos()); // prevent spam at ignore packet
            return;
        }
    }
    else
    {
        // not have spell in spellbook or spell passive and not casted by client
        if (!((Creature*)moving)->HasSpell(spellId) ||
            IsPassiveSpell(spellInfo))
        {
            // cheater? kick? ban?
            recvPacket.rpos(recvPacket.wpos()); // prevent spam at ignore packet
            return;
        }
    }

    // client provided targets
    SpellCastTargets targets;

    recvPacket >> targets.ReadForCaster(_player);

    // auto-selection buff level base at target level (in spellInfo)
    if (Unit* target = targets.getUnitTarget())
    {
        // if rank not found then function return NULL but in explicit cast case
        // original spell can be casted and later failed with appropriate error
        // message
        if (SpellEntry const* actualSpellInfo =
                sSpellMgr::Instance()->SelectAuraRankForLevel(
                    spellInfo, target->getLevel()))
            spellInfo = actualSpellInfo;
    }

    auto spell = new Spell(_player, spellInfo, false);
    spell->m_cast_count = cast_count; // set count of casts
    spell->prepare(&targets);
}

void WorldSession::HandleCancelCastOpcode(WorldPacket& recvPacket)
{
    uint32 spellId;

    recvPacket >> spellId;

    Unit* moving = _player->GetMovingUnit();

    if (!_player->InControl())
        return;

    // FIXME: hack, ignore unexpected client cancel Deadly Throw cast
    if (spellId == 26679)
        return;

    if (moving->IsNonMeleeSpellCasted(false))
    {
        // Don't allow interrupting channeled spells with no cast time that
        // haven't been started yet
        auto sp = moving->GetCurrentSpell(CURRENT_CHANNELED_SPELL);
        if (sp && sp->m_spellInfo->Id == spellId && sp->GetCastTime() == 0 &&
            sp->getState() == SPELL_STATE_PREPARING)
            return;

        moving->InterruptNonMeleeSpells(false, spellId);
    }
}

void WorldSession::HandleCancelAuraOpcode(WorldPacket& recvPacket)
{
    uint32 spellId;
    recvPacket >> spellId;

    SpellEntry const* spellInfo = sSpellStore.LookupEntry(spellId);
    if (!spellInfo)
        return;

    if (spellInfo->HasAttribute(SPELL_ATTR_CANT_CANCEL))
        return;

    if (IsPassiveSpell(spellInfo))
        return;

    if (!IsPositiveSpell(spellId))
    {
        // ignore for remote control state
        if (!_player->IsMovingSelf())
        {
            // except own aura spells
            bool allow = false;
            for (int k = 0; k < MAX_EFFECT_INDEX; ++k)
            {
                if (spellInfo->EffectApplyAuraName[k] ==
                        SPELL_AURA_MOD_POSSESS ||
                    spellInfo->EffectApplyAuraName[k] ==
                        SPELL_AURA_MOD_POSSESS_PET)
                {
                    allow = true;
                    break;
                }
            }

            // this also include case when aura not found
            if (!allow)
                return;
        }
        else
            return;
    }

    // channeled spell case (it currently casted then)
    if (IsChanneledSpell(spellInfo))
    {
        if (auto sp = _player->GetCurrentSpell(CURRENT_CHANNELED_SPELL))
            if (sp->m_spellInfo->Id == spellId)
            {
                // Don't allow interrupting channeled spells with no cast time
                // that haven't been started yet
                if (sp->GetCastTime() == 0 &&
                    sp->getState() == SPELL_STATE_PREPARING)
                    return;
                _player->InterruptSpell(CURRENT_CHANNELED_SPELL);
            }
        return;
    }

    AuraHolder* holder = _player->get_aura(spellId);

    // not own area auras can't be cancelled (note: maybe need to check for aura
    // on holder and not general on spell)
    if (holder && holder->GetCasterGuid() != _player->GetObjectGuid() &&
        HasAreaAuraEffect(holder->GetSpellProto()))
        return;

    // non channeled case
    _player->remove_auras(
        spellId, Unit::aura_no_op_true, AURA_REMOVE_BY_CANCEL);
}

void WorldSession::HandlePetCancelAuraOpcode(WorldPacket& recvPacket)
{
    ObjectGuid guid;
    uint32 spellId;

    recvPacket >> guid;
    recvPacket >> spellId;

    // ignore for remote control state
    if (!_player->IsMovingSelf())
        return;

    SpellEntry const* spellInfo = sSpellStore.LookupEntry(spellId);
    if (!spellInfo)
    {
        logging.error("WORLD: unknown PET spell id %u", spellId);
        return;
    }

    Creature* pet = GetPlayer()->GetMap()->GetAnyTypeCreature(guid);

    if (!pet)
    {
        logging.error("HandlePetCancelAuraOpcode - %s not exist.",
            guid.GetString().c_str());
        return;
    }

    if (guid != GetPlayer()->GetPetGuid() &&
        guid != GetPlayer()->GetCharmGuid())
    {
        logging.error("HandlePetCancelAura. %s isn't pet of %s",
            guid.GetString().c_str(), GetPlayer()->GetGuidStr().c_str());
        return;
    }

    if (!pet->isAlive())
    {
        pet->SendPetActionFeedback(FEEDBACK_PET_DEAD);
        return;
    }

    pet->remove_auras(spellId);

    pet->AddCreatureSpellCooldown(spellId);
}

void WorldSession::HandleCancelGrowthAuraOpcode(WorldPacket& /*recvPacket*/)
{
    // nothing do
}

void WorldSession::HandleCancelAutoRepeatSpellOpcode(
    WorldPacket& /*recvPacket*/)
{
    // cancel and prepare for deleting
    // do not send SMSG_CANCEL_AUTO_REPEAT! client will send this Opcode again
    // (loop)
    _player->GetMovingUnit()->InterruptSpell(
        CURRENT_AUTOREPEAT_SPELL, true, nullptr, false);
}

void WorldSession::HandleCancelChanneling(WorldPacket& recv_data)
{
    recv_data.read_skip<uint32>(); // spellid, not used

    // ignore for remote control state (for player case)
    Unit* moving = _player->GetMovingUnit();
    if (moving != _player && moving->GetTypeId() == TYPEID_PLAYER)
        return;

    // Don't allow interrupting channeled spells with no cast time that
    // haven't been started yet
    auto sp = moving->GetCurrentSpell(CURRENT_CHANNELED_SPELL);
    if (sp && sp->GetCastTime() == 0 && sp->getState() == SPELL_STATE_PREPARING)
        return;

    _player->InterruptSpell(CURRENT_CHANNELED_SPELL);
}

void WorldSession::HandleTotemDestroyed(WorldPacket& recvPacket)
{
    uint8 slotId;

    recvPacket >> slotId;

    // ignore for remote control state
    if (!_player->IsMovingSelf())
        return;

    if (int(slotId) >= MAX_TOTEM_SLOT)
        return;

    if (Totem* totem = GetPlayer()->GetTotem(TotemSlot(slotId)))
        totem->UnSummon();
}

void WorldSession::HandleSelfResOpcode(WorldPacket& /*recv_data*/)
{
    LOG_DEBUG(logging, "WORLD: CMSG_SELF_RES"); // empty opcode

    if (_player->GetUInt32Value(PLAYER_SELF_RES_SPELL))
    {
        SpellEntry const* spellInfo = sSpellStore.LookupEntry(
            _player->GetUInt32Value(PLAYER_SELF_RES_SPELL));
        if (spellInfo)
            _player->CastSpell(_player, spellInfo, false);

        _player->SetUInt32Value(PLAYER_SELF_RES_SPELL, 0);
    }
}
