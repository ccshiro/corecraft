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
#include "Creature.h"
#include "CreatureAI.h"
#include "GameObjectAI.h"
#include "GossipDef.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "Language.h"
#include "logging.h"
#include "ObjectMgr.h"
#include "Opcodes.h"
#include "Pet.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SpellMgr.h"
#include "UpdateMask.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "Database/DatabaseEnv.h"

enum StableResultCode
{
    STABLE_ERR_MONEY = 0x01,        // "you don't have enough money"
    STABLE_ERR_STABLE = 0x06,       // currently used in most fail cases
    STABLE_SUCCESS_STABLE = 0x08,   // stable success
    STABLE_SUCCESS_UNSTABLE = 0x09, // unstable/swap success
    STABLE_SUCCESS_BUY_SLOT = 0x0A, // buy slot success
};

void WorldSession::HandleTabardVendorActivateOpcode(WorldPacket& recv_data)
{
    ObjectGuid guid;
    recv_data >> guid;

    Creature* unit = GetPlayer()->GetNPCIfCanInteractWith(
        guid, UNIT_NPC_FLAG_TABARDDESIGNER);
    if (!unit)
    {
        LOG_DEBUG(logging,
            "WORLD: HandleTabardVendorActivateOpcode - %s not found or you "
            "can't interact with him.",
            guid.GetString().c_str());
        return;
    }

    // remove fake death
    if (GetPlayer()->hasUnitState(UNIT_STAT_DIED))
        GetPlayer()->remove_auras(SPELL_AURA_FEIGN_DEATH);

    SendTabardVendorActivate(guid);
}

void WorldSession::SendTabardVendorActivate(ObjectGuid guid)
{
    WorldPacket data(MSG_TABARDVENDOR_ACTIVATE, 8);
    data << ObjectGuid(guid);
    send_packet(std::move(data));
}

void WorldSession::HandleBankerActivateOpcode(WorldPacket& recv_data)
{
    ObjectGuid guid;

    recv_data >> guid;

    if (!CheckBanker(guid))
        return;

    // remove fake death
    if (GetPlayer()->hasUnitState(UNIT_STAT_DIED))
        GetPlayer()->remove_auras(SPELL_AURA_FEIGN_DEATH);

    SendShowBank(guid);
}

void WorldSession::SendShowBank(ObjectGuid guid)
{
    WorldPacket data(SMSG_SHOW_BANK, 8);
    data << ObjectGuid(guid);
    send_packet(std::move(data));
}

void WorldSession::HandleTrainerListOpcode(WorldPacket& recv_data)
{
    ObjectGuid guid;

    recv_data >> guid;

    SendTrainerList(guid);
}

void WorldSession::SendTrainerList(ObjectGuid guid)
{
    std::string str = GetMangosString(LANG_NPC_TAINER_HELLO);
    SendTrainerList(guid, str);
}

static void SendTrainerSpellHelper(WorldPacket& data,
    TrainerSpell const* tSpell, TrainerSpellState state, float fDiscountMod,
    bool can_learn_primary_prof, uint32 reqLevel)
{
    bool primary_prof_first_rank =
        sSpellMgr::Instance()->IsPrimaryProfessionFirstRankSpell(tSpell->spell);

    SpellChainNode const* chain_node =
        sSpellMgr::Instance()->GetSpellChainNode(tSpell->spell);

    data << uint32(tSpell->spell);
    data << uint8(
        state == TRAINER_SPELL_GREEN_DISABLED ? TRAINER_SPELL_GREEN : state);
    data << uint32(floor(tSpell->spellCost * fDiscountMod));

    data << uint32(primary_prof_first_rank && can_learn_primary_prof ? 1 : 0);
    // primary prof. learn confirmation dialog
    data << uint32(primary_prof_first_rank ? 1 : 0); // must be equal prev.
                                                     // field to have learn
                                                     // button in enabled state
    data << uint8(reqLevel);
    data << uint32(tSpell->reqSkill);
    data << uint32(tSpell->reqSkillValue);
    data << uint32(chain_node ?
                       (chain_node->prev ? chain_node->prev : chain_node->req) :
                       0);
    data << uint32(chain_node && chain_node->prev ? chain_node->req : 0);
    data << uint32(0);
}

void WorldSession::SendTrainerList(ObjectGuid guid, const std::string& strTitle)
{
    Creature* unit =
        GetPlayer()->GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_TRAINER);
    if (!unit)
    {
        LOG_DEBUG(logging,
            "WORLD: SendTrainerList - %s not found or you can't interact with "
            "him.",
            guid.GetString().c_str());
        return;
    }

    // remove fake death
    if (GetPlayer()->hasUnitState(UNIT_STAT_DIED))
        GetPlayer()->remove_auras(SPELL_AURA_FEIGN_DEATH);

    // trainer list loaded at check;
    if (!unit->IsTrainerOf(_player, true))
        return;

    CreatureInfo const* ci = unit->GetCreatureInfo();
    if (!ci)
        return;

    TrainerSpellData const* cSpells = unit->GetTrainerSpells();
    TrainerSpellData const* tSpells = unit->GetTrainerTemplateSpells();

    if (!cSpells && !tSpells)
    {
        LOG_DEBUG(logging,
            "WORLD: SendTrainerList - Training spells not found for %s",
            guid.GetString().c_str());
        return;
    }

    uint32 maxcount = (cSpells ? cSpells->spellList.size() : 0) +
                      (tSpells ? tSpells->spellList.size() : 0);
    uint32 trainer_type = cSpells && cSpells->trainerType ?
                              cSpells->trainerType :
                              (tSpells ? tSpells->trainerType : 0);

    WorldPacket data(
        SMSG_TRAINER_LIST, 8 + 4 + 4 + maxcount * 38 + strTitle.size() + 1);
    data << ObjectGuid(guid);
    data << uint32(trainer_type);

    size_t count_pos = data.wpos();
    data << uint32(maxcount);

    // reputation discount
    float fDiscountMod = _player->GetReputationPriceDiscount(unit);
    bool can_learn_primary_prof =
        GetPlayer()->GetFreePrimaryProfessionPoints() > 0;

    uint32 count = 0;

    if (cSpells)
    {
        for (const auto& elem : cSpells->spellList)
        {
            TrainerSpell const* tSpell = &elem.second;

            uint32 reqLevel = 0;
            if (!_player->IsSpellFitByClassAndRace(tSpell->spell, &reqLevel))
                continue;

            reqLevel = tSpell->isProvidedReqLevel ?
                           tSpell->reqLevel :
                           std::max(reqLevel, tSpell->reqLevel);

            TrainerSpellState state =
                _player->GetTrainerSpellState(tSpell, reqLevel);

            SendTrainerSpellHelper(data, tSpell, state, fDiscountMod,
                can_learn_primary_prof, reqLevel);

            ++count;
        }
    }

    if (tSpells)
    {
        for (const auto& elem : tSpells->spellList)
        {
            TrainerSpell const* tSpell = &elem.second;

            uint32 reqLevel = 0;
            if (!_player->IsSpellFitByClassAndRace(tSpell->spell, &reqLevel))
                continue;

            reqLevel = tSpell->isProvidedReqLevel ?
                           tSpell->reqLevel :
                           std::max(reqLevel, tSpell->reqLevel);

            TrainerSpellState state =
                _player->GetTrainerSpellState(tSpell, reqLevel);

            SendTrainerSpellHelper(data, tSpell, state, fDiscountMod,
                can_learn_primary_prof, reqLevel);

            ++count;
        }
    }

    data << strTitle;

    data.put<uint32>(count_pos, count);
    send_packet(std::move(data));
}

void WorldSession::HandleTrainerBuySpellOpcode(WorldPacket& recv_data)
{
    ObjectGuid guid;
    uint32 spellId = 0;

    recv_data >> guid >> spellId;
    Creature* unit =
        GetPlayer()->GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_TRAINER);
    if (!unit)
    {
        LOG_DEBUG(logging,
            "WORLD: HandleTrainerBuySpellOpcode - %s not found or you can't "
            "interact with him.",
            guid.GetString().c_str());
        return;
    }

    // remove fake death
    if (GetPlayer()->hasUnitState(UNIT_STAT_DIED))
        GetPlayer()->remove_auras(SPELL_AURA_FEIGN_DEATH);

    if (!unit->IsTrainerOf(_player, true))
        return;

    // check present spell in trainer spell list
    TrainerSpellData const* cSpells = unit->GetTrainerSpells();
    TrainerSpellData const* tSpells = unit->GetTrainerTemplateSpells();

    if (!cSpells && !tSpells)
        return;

    // Try find spell in npc_trainer
    TrainerSpell const* trainer_spell =
        cSpells ? cSpells->Find(spellId) : nullptr;

    // Not found, try find in npc_trainer_template
    if (!trainer_spell && tSpells)
        trainer_spell = tSpells->Find(spellId);

    // Not found anywhere, cheating?
    if (!trainer_spell)
        return;

    // can't be learn, cheat? Or double learn with lags...
    uint32 reqLevel = 0;
    if (!_player->IsSpellFitByClassAndRace(trainer_spell->spell, &reqLevel))
        return;

    reqLevel = trainer_spell->isProvidedReqLevel ?
                   trainer_spell->reqLevel :
                   std::max(reqLevel, trainer_spell->reqLevel);
    if (_player->GetTrainerSpellState(trainer_spell, reqLevel) !=
        TRAINER_SPELL_GREEN)
        return;

    // apply reputation discount
    uint32 nSpellCost = uint32(floor(
        trainer_spell->spellCost * _player->GetReputationPriceDiscount(unit)));

    // subtract money
    // XXX
    inventory::transaction trans;
    trans.remove(nSpellCost);
    if (!_player->storage().finalize(trans))
        return;

    WorldPacket data(SMSG_PLAY_SPELL_VISUAL, 12); // visual effect on trainer
    data << ObjectGuid(guid);
    data << uint32(0xB3); // index from SpellVisualKit.dbc
    send_packet(std::move(data));

    data.initialize(SMSG_PLAY_SPELL_IMPACT, 12); // visual effect on player
    data << _player->GetObjectGuid();
    data << uint32(0x016A); // index from SpellVisualKit.dbc
    send_packet(std::move(data));

    // learn explicitly
    _player->learnSpell(trainer_spell->spell, false);

    data.initialize(SMSG_TRAINER_BUY_SUCCEEDED, 12);
    data << ObjectGuid(guid);
    data << uint32(spellId); // should be same as in packet from client
    send_packet(std::move(data));
}

void WorldSession::HandleGossipHelloOpcode(WorldPacket& recv_data)
{
    ObjectGuid guid;
    recv_data >> guid;

    Creature* pCreature =
        GetPlayer()->GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_NONE);
    if (!pCreature)
    {
        LOG_DEBUG(logging,
            "WORLD: HandleGossipHelloOpcode - %s not found or you can't "
            "interact with him.",
            guid.GetString().c_str());
        return;
    }

    // Show NPC's reputation in client if necessary
    if (const FactionTemplateEntry* faction_template =
            sFactionTemplateStore.LookupEntry(pCreature->getFaction()))
        _player->GetReputationMgr().SetVisible(faction_template);

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

    if (pCreature->isSpiritGuide())
        pCreature->SendAreaSpiritHealerQueryOpcode(_player);

    if (!sScriptMgr::Instance()->OnGossipHello(_player, pCreature))
    {
        _player->PrepareGossipMenu(
            pCreature, pCreature->GetCreatureInfo()->GossipMenuId);
        _player->SendPreparedGossip(pCreature);
    }
}

void WorldSession::HandleGossipSelectOptionOpcode(WorldPacket& recv_data)
{
    uint32 gossipListId;
    uint32 menuId;
    ObjectGuid guid;
    std::string code;

    recv_data >> guid >> menuId >> gossipListId;

    if (_player->PlayerTalkClass->GossipOptionCoded(gossipListId))
    {
        recv_data >> code;
        LOG_DEBUG(logging, "Gossip code: %s", code.c_str());
    }

    // remove fake death
    if (GetPlayer()->hasUnitState(UNIT_STAT_DIED))
        GetPlayer()->remove_auras(SPELL_AURA_FEIGN_DEATH);

    uint32 sender = _player->PlayerTalkClass->GossipOptionSender(gossipListId);
    uint32 action = _player->PlayerTalkClass->GossipOptionAction(gossipListId);
    uint32 id = _player->PlayerTalkClass->GossipOptionId(
        gossipListId); // gossip_menu_option.id

    if (guid.IsAnyTypeCreature())
    {
        Creature* pCreature =
            GetPlayer()->GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_NONE);

        if (!pCreature)
        {
            LOG_DEBUG(logging,
                "WORLD: HandleGossipSelectOptionOpcode - %s not found or you "
                "can't interact with it.",
                guid.GetString().c_str());
            return;
        }

        // Save the last banker we interacted with, used by personal_storage to
        // verify we're at a bank
        if (pCreature->isBanker())
            _player->last_interacted_banker(pCreature->GetObjectGuid());

        if (pCreature->AI() &&
            pCreature->AI()->OnGossipSelect(_player, sender, id, menuId,
                code.empty() ? nullptr : code.c_str()))
            return;
        if (!sScriptMgr::Instance()->OnGossipSelect(_player, pCreature, sender,
                action, code.empty() ? nullptr : code.c_str()))
            _player->OnGossipSelect(pCreature, gossipListId, menuId);
    }
    else if (guid.IsGameObject())
    {
        GameObject* pGo = GetPlayer()->GetGameObjectIfCanInteractWith(guid);

        if (!pGo)
        {
            LOG_DEBUG(logging,
                "WORLD: HandleGossipSelectOptionOpcode - %s not found or you "
                "can't interact with it.",
                guid.GetString().c_str());
            return;
        }

        if (pGo->AI() &&
            pGo->AI()->OnGossipSelect(_player, sender, action, menuId,
                code.empty() ? nullptr : code.c_str()))
            return;
        if (!sScriptMgr::Instance()->OnGossipSelect(_player, pGo, sender,
                action, code.empty() ? nullptr : code.c_str()))
            _player->OnGossipSelect(pGo, gossipListId, menuId);
    }
}

void WorldSession::HandleSpiritHealerActivateOpcode(WorldPacket& recv_data)
{
    ObjectGuid guid;

    recv_data >> guid;

    Creature* unit =
        GetPlayer()->GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_SPIRITHEALER);
    if (!unit)
    {
        LOG_DEBUG(logging,
            "WORLD: HandleSpiritHealerActivateOpcode - %s not found or you "
            "can't interact with him.",
            guid.GetString().c_str());
        return;
    }

    // remove fake death
    if (GetPlayer()->hasUnitState(UNIT_STAT_DIED))
        GetPlayer()->remove_auras(SPELL_AURA_FEIGN_DEATH);

    SendSpiritResurrect();
}

void WorldSession::SendSpiritResurrect()
{
    _player->ResurrectPlayer(0.5f, true);

    _player->durability(true, -0.25, true);

    // get corpse nearest graveyard
    WorldSafeLocsEntry const* corpseGrave = nullptr;
    Corpse* corpse = _player->GetCorpse();
    if (corpse)
        corpseGrave = sObjectMgr::Instance()->GetClosestGraveyard(
            corpse->GetX(), corpse->GetY(), corpse->GetZ(), corpse->GetMapId(),
            _player->GetTeam());

    // now can spawn bones
    _player->SpawnCorpseBones();

    // teleport to nearest from corpse graveyard, if different from nearest to
    // player ghost
    if (corpseGrave)
    {
        WorldSafeLocsEntry const* ghostGrave =
            sObjectMgr::Instance()->GetClosestGraveyard(_player->GetX(),
                _player->GetY(), _player->GetZ(), _player->GetMapId(),
                _player->GetTeam());

        if (corpseGrave != ghostGrave)
            _player->TeleportTo(corpseGrave->map_id, corpseGrave->x,
                corpseGrave->y, corpseGrave->z, _player->GetO());
        // or update at original position
        else
        {
            _player->GetCamera().UpdateVisibilityForOwner();
            _player->UpdateObjectVisibility();
        }
    }
    // or update at original position
    else
    {
        _player->GetCamera().UpdateVisibilityForOwner();
        _player->UpdateObjectVisibility();
    }
}

void WorldSession::HandleBinderActivateOpcode(WorldPacket& recv_data)
{
    ObjectGuid npcGuid;
    recv_data >> npcGuid;

    if (!GetPlayer()->IsInWorld() || !GetPlayer()->isAlive())
        return;

    Creature* unit =
        GetPlayer()->GetNPCIfCanInteractWith(npcGuid, UNIT_NPC_FLAG_INNKEEPER);
    if (!unit)
    {
        LOG_DEBUG(logging,
            "WORLD: HandleBinderActivateOpcode - %s not found or you can't "
            "interact with him.",
            npcGuid.GetString().c_str());
        return;
    }

    // remove fake death
    if (GetPlayer()->hasUnitState(UNIT_STAT_DIED))
        GetPlayer()->remove_auras(SPELL_AURA_FEIGN_DEATH);

    SendBindPoint(unit);
}

void WorldSession::SendBindPoint(Creature* npc)
{
    // prevent set homebind to instances in any case
    if (GetPlayer()->GetMap()->Instanceable())
        return;

    // send spell for bind 3286 bind magic
    npc->CastSpell(_player, 3286, true); // Bind

    WorldPacket data(SMSG_TRAINER_BUY_SUCCEEDED, (8 + 4));
    data << npc->GetObjectGuid();
    data << uint32(3286); // Bind
    send_packet(std::move(data));

    _player->PlayerTalkClass->CloseGossip();
}

void WorldSession::HandleListStabledPetsOpcode(WorldPacket& recv_data)
{
    ObjectGuid npcGUID;

    recv_data >> npcGUID;

    Creature* unit = GetPlayer()->GetNPCIfCanInteractWith(
        npcGUID, UNIT_NPC_FLAG_STABLEMASTER);
    if (!unit)
    {
        LOG_DEBUG(logging,
            "WORLD: HandleListStabledPetsOpcode - %s not found or you can't "
            "interact with him.",
            npcGUID.GetString().c_str());
        return;
    }

    // remove fake death
    if (GetPlayer()->hasUnitState(UNIT_STAT_DIED))
        GetPlayer()->remove_auras(SPELL_AURA_FEIGN_DEATH);

    SendStablePet(npcGUID);
}

void WorldSession::SendStablePet(ObjectGuid guid)
{
    WorldPacket data(MSG_LIST_STABLED_PETS, 200); // guess size
    data << guid;

    size_t wpos = data.wpos();
    data << uint8(0); // place holder for slot show number

    data << uint8(GetPlayer()->m_stableSlots);

    uint8 num = 0; // counter for place holder

    auto write_data = [&num, &data](PetDbData& d)
    {
        data << uint32(d.guid);
        data << uint32(d.id);
        data << uint32(d.level);
        data << d.name;
        data << uint32(d.loyalty);
        if (d.slot == PET_SAVE_NOT_IN_SLOT)
            data << uint8(1);
        else
            data << uint8(d.slot + 1);

        ++num;
    };

    PetDbData* current = nullptr;
    for (auto& d : _player->_pet_store)
        if (!d.deleted)
        {
            if (d.slot == PET_SAVE_AS_CURRENT || d.slot == PET_SAVE_NOT_IN_SLOT)
                current = &d;
            if (d.slot == PET_SAVE_AS_CURRENT)
                break;
        }
    if (current)
        write_data(*current);

    for (auto& d : _player->_pet_store)
    {
        if (!d.deleted && d.slot >= PET_SAVE_FIRST_STABLE_SLOT &&
            d.slot <= PET_SAVE_LAST_STABLE_SLOT)
            write_data(d);
    }

    data.put<uint8>(wpos, num); // set real data to placeholder
    send_packet(std::move(data));
}

void WorldSession::SendStableResult(uint8 res)
{
    WorldPacket data(SMSG_STABLE_RESULT, 1);
    data << uint8(res);
    send_packet(std::move(data));
}

bool WorldSession::CheckStableMaster(ObjectGuid guid)
{
    // spell case or GM
    if (guid == GetPlayer()->GetObjectGuid())
    {
        // command case will return only if player have real access to command
        if (!ChatHandler(GetPlayer()).FindCommand("stable"))
        {
            LOG_DEBUG(logging, "%s attempt open stable in cheating way.",
                guid.GetString().c_str());
            return false;
        }
    }
    // stable master case
    else
    {
        if (!GetPlayer()->GetNPCIfCanInteractWith(
                guid, UNIT_NPC_FLAG_STABLEMASTER))
        {
            LOG_DEBUG(logging,
                "Stablemaster %s not found or you can't interact with him.",
                guid.GetString().c_str());
            return false;
        }
    }

    return true;
}

void WorldSession::HandleStablePet(WorldPacket& recv_data)
{
    ObjectGuid npcGUID;

    recv_data >> npcGUID;

    // Don't allow switching if pet is currently being loaded
    if (_player->GetPet() == nullptr && _player->GetPetGuid())
    {
        SendStableResult(STABLE_ERR_STABLE);
        return;
    }

    if (!GetPlayer()->isAlive())
    {
        SendStableResult(STABLE_ERR_STABLE);
        return;
    }

    if (!CheckStableMaster(npcGUID))
    {
        SendStableResult(STABLE_ERR_STABLE);
        return;
    }

    // remove fake death
    if (GetPlayer()->hasUnitState(UNIT_STAT_DIED))
        GetPlayer()->remove_auras(SPELL_AURA_FEIGN_DEATH);

    PetDbData* pet_data = nullptr;
    auto pet = _player->GetPet();

    for (auto& d : _player->_pet_store)
        if (!d.deleted)
        {
            if (d.slot == PET_SAVE_AS_CURRENT || d.slot == PET_SAVE_NOT_IN_SLOT)
                pet_data = &d;
            if (d.slot == PET_SAVE_AS_CURRENT)
                break;
        }

    if (!pet_data ||
        (pet && pet->GetCharmInfo()->GetPetNumber() != pet_data->guid))
    {
        SendStableResult(STABLE_ERR_STABLE);
        return;
    }

    // Find first free stable slot
    std::set<uint32> free_slots;
    for (uint32 i = PET_SAVE_FIRST_STABLE_SLOT; i <= PET_SAVE_LAST_STABLE_SLOT;
         ++i)
        free_slots.insert(i);
    for (auto& d : _player->_pet_store)
        if (!d.deleted)
            free_slots.erase(d.slot);

    // Move pet into stable slot, unsummon if it's present
    if (!free_slots.empty() &&
        *free_slots.begin() <= GetPlayer()->m_stableSlots)
    {
        if (pet)
        {
            pet->Unsummon(PetSaveMode(*free_slots.begin()), _player);
        }
        else
        {
            pet_data->slot = *free_slots.begin();
            pet_data->needs_save = true;
            _player->SetTemporaryUnsummonedPetNumber(0);
        }
        SendStableResult(STABLE_SUCCESS_STABLE);
    }
    else
        SendStableResult(STABLE_ERR_STABLE);
}

void WorldSession::HandleUnstablePet(WorldPacket& recv_data)
{
    ObjectGuid npcGUID;
    uint32 petnumber;

    recv_data >> npcGUID >> petnumber;

    // Unstable pet should not be usable with a current pet (the swap opcode is
    // for that)
    if (_player->GetPet() || _player->GetTemporaryUnsummonedPetNumber() != 0)
    {
        SendStableResult(STABLE_ERR_STABLE);
        return;
    }

    // Don't allow switching if pet is currently being loaded
    if (_player->GetPet() == nullptr && _player->GetPetGuid())
    {
        SendStableResult(STABLE_ERR_STABLE);
        return;
    }

    if (!CheckStableMaster(npcGUID))
    {
        SendStableResult(STABLE_ERR_STABLE);
        return;
    }

    // remove fake death
    if (GetPlayer()->hasUnitState(UNIT_STAT_DIED))
        GetPlayer()->remove_auras(SPELL_AURA_FEIGN_DEATH);

    PetDbData* pet_data = nullptr;

    for (auto& d : _player->_pet_store)
    {
        if (!d.deleted && d.slot >= PET_SAVE_FIRST_STABLE_SLOT &&
            d.slot <= PET_SAVE_LAST_STABLE_SLOT && d.guid == petnumber)
        {
            pet_data = &d;
            break;
        }
    }

    if (!pet_data)
    {
        SendStableResult(STABLE_ERR_STABLE);
        return;
    }

    CreatureInfo const* creatureInfo =
        ObjectMgr::GetCreatureTemplate(pet_data->id);
    if (!creatureInfo || !creatureInfo->isTameable())
    {
        SendStableResult(STABLE_ERR_STABLE);
        return;
    }

    // Don't allow unstabling pet if we already have a pet
    for (auto& d : _player->_pet_store)
    {
        if (!d.deleted &&
            (d.slot == PET_SAVE_AS_CURRENT || d.slot == PET_SAVE_NOT_IN_SLOT))
        {
            SendStableResult(STABLE_ERR_STABLE);
            return;
        }
    }

    // If pet is dead, force reviving it with revive pet
    if (pet_data->dead)
    {
        pet_data->slot = PET_SAVE_NOT_IN_SLOT;
        pet_data->needs_save = true;
        SendStableResult(STABLE_SUCCESS_UNSTABLE);
        return;
    }

    if (_player->IsMounted())
    {
        pet_data->slot = PET_SAVE_NOT_IN_SLOT;
        pet_data->needs_save = true;
        _player->SetTemporaryUnsummonedPetNumber(pet_data->guid);
    }
    else
    {
        auto newpet = new Pet(HUNTER_PET);
        if (!newpet->LoadPetFromDB(_player, pet_data->id, petnumber))
        {
            delete newpet;
            SendStableResult(STABLE_ERR_STABLE);
            return;
        }
    }

    SendStableResult(STABLE_SUCCESS_UNSTABLE);
}

void WorldSession::HandleBuyStableSlot(WorldPacket& recv_data)
{
    ObjectGuid npcGUID;

    recv_data >> npcGUID;

    if (!CheckStableMaster(npcGUID))
    {
        SendStableResult(STABLE_ERR_STABLE);
        return;
    }

    // remove fake death
    if (GetPlayer()->hasUnitState(UNIT_STAT_DIED))
        GetPlayer()->remove_auras(SPELL_AURA_FEIGN_DEATH);

    if (GetPlayer()->m_stableSlots < MAX_PET_STABLES)
    {
        StableSlotPricesEntry const* SlotPrice =
            sStableSlotPricesStore.LookupEntry(GetPlayer()->m_stableSlots + 1);

        // XXX
        inventory::transaction trans;
        trans.remove(SlotPrice->Price);
        if (!_player->storage().finalize(trans))
        {
            SendStableResult(STABLE_ERR_MONEY);
        }
        else
        {
            ++GetPlayer()->m_stableSlots;
            SendStableResult(STABLE_SUCCESS_BUY_SLOT);
        }
    }
    else
        SendStableResult(STABLE_ERR_STABLE);
}

void WorldSession::HandleStableRevivePet(WorldPacket& /* recv_data */)
{
    LOG_DEBUG(logging, "HandleStableRevivePet: Not implemented");
}

void WorldSession::HandleStableSwapPet(WorldPacket& recv_data)
{
    ObjectGuid npcGUID;
    uint32 pet_number;

    recv_data >> npcGUID >> pet_number;

    if (!CheckStableMaster(npcGUID))
    {
        SendStableResult(STABLE_ERR_STABLE);
        return;
    }

    // Don't allow switching if pet is currently being loaded
    if (_player->GetPet() == nullptr && _player->GetPetGuid())
    {
        SendStableResult(STABLE_ERR_STABLE);
        return;
    }

    // remove fake death
    if (GetPlayer()->hasUnitState(UNIT_STAT_DIED))
        GetPlayer()->remove_auras(SPELL_AURA_FEIGN_DEATH);

    // Find pet we're swapping with
    PetDbData* new_pet_data = nullptr;
    for (auto& d : _player->_pet_store)
    {
        if (!d.deleted && d.guid == pet_number)
        {
            new_pet_data = &d;
            break;
        }
    }

    if (!new_pet_data)
    {
        SendStableResult(STABLE_ERR_STABLE);
        return;
    }

    // Get current pet
    PetDbData* pet_data = nullptr;
    auto pet = _player->GetPet();

    for (auto& d : _player->_pet_store)
        if (!d.deleted)
        {
            if (d.slot == PET_SAVE_AS_CURRENT || d.slot == PET_SAVE_NOT_IN_SLOT)
                pet_data = &d;
            if (d.slot == PET_SAVE_AS_CURRENT)
                break;
        }

    if (!pet_data ||
        (pet && pet->GetCharmInfo()->GetPetNumber() != pet_data->guid))
    {
        SendStableResult(STABLE_ERR_STABLE);
        return;
    }

    CreatureInfo const* creatureInfo =
        ObjectMgr::GetCreatureTemplate(new_pet_data->id);
    if (!creatureInfo || !creatureInfo->isTameable())
    {
        SendStableResult(STABLE_ERR_STABLE);
        return;
    }

    // Move current pet into stable
    if (pet)
    {
        pet->Unsummon(PetSaveMode(new_pet_data->slot), _player);
    }
    else
    {
        pet_data->slot = new_pet_data->slot;
        pet_data->needs_save = true;
        _player->SetTemporaryUnsummonedPetNumber(0);
    }

    // If pet is dead, force reviving it with revive pet
    if (new_pet_data->dead)
    {
        new_pet_data->slot = PET_SAVE_NOT_IN_SLOT;
        new_pet_data->needs_save = true;
        SendStableResult(STABLE_SUCCESS_UNSTABLE);
        return;
    }

    // Summon unstabled pet
    if (_player->IsMounted())
    {
        new_pet_data->slot = PET_SAVE_NOT_IN_SLOT;
        new_pet_data->needs_save = true;
        _player->SetTemporaryUnsummonedPetNumber(new_pet_data->guid);
    }
    else
    {
        auto newpet = new Pet;
        if (!newpet->LoadPetFromDB(_player, new_pet_data->id, pet_number))
        {
            delete newpet;
            SendStableResult(STABLE_ERR_STABLE);
            return;
        }
    }

    SendStableResult(STABLE_SUCCESS_UNSTABLE);
}

void WorldSession::HandleRepairItemOpcode(WorldPacket& recv_data)
{
    /*XXX:*/
    ObjectGuid npcGuid;
    ObjectGuid itemGuid;
    uint8 guildBank; // new in 2.3.2, bool that means from guild bank money

    recv_data >> npcGuid >> itemGuid >> guildBank;

    Creature* unit =
        GetPlayer()->GetNPCIfCanInteractWith(npcGuid, UNIT_NPC_FLAG_REPAIR);
    if (!unit)
    {
        LOG_DEBUG(logging,
            "WORLD: HandleRepairItemOpcode - %s not found or you can't "
            "interact with him.",
            npcGuid.GetString().c_str());
        return;
    }

    // remove fake death -- Why? XXX
    // if(GetPlayer()->hasUnitState(UNIT_STAT_DIED))
    //    GetPlayer()->remove_auras(SPELL_AURA_FEIGN_DEATH);

    // reputation discount
    float discount_mod = _player->GetReputationPriceDiscount(unit);

    inventory::copper cost(0);
    Item* repaired_item = nullptr;
    if (itemGuid)
    {
        if (Item* item = _player->GetItemByGuid(itemGuid))
        {
            cost = _player->repair_cost(item, discount_mod);
            repaired_item = item;
            LOG_DEBUG(logging, "REPAIR: %s trying to repair item %u. Cost: %s.",
                _player->GetName(), item->GetGUIDLow(), cost.str().c_str());
        }
    }
    else
    {
        cost = _player->repair_cost(discount_mod);
        LOG_DEBUG(logging, "REPAIR: %s trying to repair all items. Cost: %s.",
            _player->GetName(), cost.str().c_str());
    }

    if (cost.get() == 0)
        return;

    // Note: No errors are reported. A proper client will never send a packet
    // for a repair it cannot afford.

    if (guildBank)
    {
        // Guild bank repairs are always the entire inventory
        if (repaired_item)
            return;

        uint32 guild_id = _player->GetGuildId();
        Guild* guild;
        if (!guild_id ||
            (guild = sGuildMgr::Instance()->GetGuildById(guild_id)) == nullptr)
            return;
        guild->storage().attempt_repair(_player, cost);
    }
    else
    {
        inventory::transaction trans;
        trans.remove(cost);
        if (_player->storage().finalize(trans))
        {
            if (repaired_item)
                _player->durability(repaired_item, true, 1.0);
            else
                _player->durability(true, 1.0, true);
        }
    }
}
