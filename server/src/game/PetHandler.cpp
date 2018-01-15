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
#include "CreatureAI.h"
#include "logging.h"
#include "ObjectMgr.h"
#include "Opcodes.h"
#include "Pet.h"
#include "Spell.h"
#include "SpellMgr.h"
#include "Util.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "pet_behavior.h"
#include "movement/IdleMovementGenerator.h"
#include "movement/TargetedMovementGenerator.h"

void WorldSession::HandlePetAction(WorldPacket& recv_data)
{
    ObjectGuid petGuid;
    uint32 data;
    ObjectGuid targetGuid;
    recv_data >> petGuid;
    recv_data >> data;
    recv_data >> targetGuid;

    uint32 spellid = UNIT_ACTION_BUTTON_ACTION(data);
    uint8 flag = UNIT_ACTION_BUTTON_TYPE(data); // delete = 0x07 CastSpell = C1

    LOG_DEBUG(logging,
        "HandlePetAction: %s flag is %u, spellid is %u, target %s.",
        petGuid.GetString().c_str(), uint32(flag), spellid,
        targetGuid.GetString().c_str());

    // used also for charmed creature/player
    Unit* pet = _player->GetMap()->GetUnit(petGuid);
    if (!pet)
    {
        logging.error(
            "HandlePetAction: %s not exist.", petGuid.GetString().c_str());
        return;
    }

    if (GetPlayer()->GetObjectGuid() != pet->GetCharmerOrOwnerGuid())
    {
        logging.error("HandlePetAction: %s isn't controlled by %s.",
            petGuid.GetString().c_str(), GetPlayer()->GetGuidStr().c_str());
        return;
    }

    if (!pet->isAlive())
        return;

    if (pet->GetTypeId() == TYPEID_PLAYER)
    {
        // controlled player can only do melee attack
        // NOTE: code below is not safe without this condition
        if (!(flag == ACT_COMMAND && spellid == COMMAND_ATTACK))
            return;
    }
    else if (((Creature*)pet)->IsPet())
    {
        // pet can have action bar disabled
        if (((Pet*)pet)->GetModeFlags() & PET_MODE_DISABLE_ACTIONS)
            return;
        if (!static_cast<Pet*>(pet)->behavior())
        {
            logging.error(
                "WorldSession::HandlePetAction: (GUID: %u TypeId: %u) is a pet "
                "but does not have pet_behavior!",
                pet->GetGUIDLow(), pet->GetTypeId());
            return;
        }
    }

    CharmInfo* charmInfo = pet->GetCharmInfo();
    if (!charmInfo)
    {
        logging.error(
            "WorldSession::HandlePetAction: object (GUID: %u TypeId: %u) is "
            "considered pet-like but doesn't have a charminfo!",
            pet->GetGUIDLow(), pet->GetTypeId());
        return;
    }

    switch (flag)
    {
    case ACT_COMMAND: // 0x07
        switch (spellid)
        {
        case COMMAND_STAY: // flat=1792  //STAY
            if (static_cast<Creature*>(pet)->behavior())
            {
                static_cast<Creature*>(pet)->behavior()->attempt_stay();
            }
            else
            {
                if (!pet->movement_gens.has(movement::gen::stopped))
                    pet->movement_gens.push(
                        new movement::StoppedMovementGenerator());
                charmInfo->SetCommandState(COMMAND_STAY);
                charmInfo->SaveStayPosition(
                    pet->GetX(), pet->GetY(), pet->GetZ());
            }
            break;
        case COMMAND_FOLLOW: // spellid=1792  //FOLLOW
            if (pet->GetTypeId() == TYPEID_UNIT &&
                static_cast<Creature*>(pet)->behavior())
            {
                static_cast<Creature*>(pet)->behavior()->attempt_follow();
            }
            else
            {
                pet->AttackStop();
                pet->InterruptNonMeleeSpells(false);
                if (!pet->movement_gens.has(movement::gen::follow))
                    pet->movement_gens.push(
                        new movement::FollowMovementGenerator(_player));
                pet->movement_gens.remove_all(movement::gen::stopped);
                charmInfo->SetCommandState(COMMAND_FOLLOW);
                charmInfo->SetIsReturning(true);
            }
            break;
        case COMMAND_ATTACK: // spellid=1792  // ATTACK
        {
            // NOTE: pet can be TYPEID_PLAYER for this (flag,command) as well
            Unit* TargetUnit = _player->GetMap()->GetUnit(targetGuid);
            if (!TargetUnit)
                return;

            if (pet->GetTypeId() == TYPEID_UNIT &&
                static_cast<Creature*>(pet)->IsPet() &&
                static_cast<Pet*>(pet)->getPetType() == SUMMON_PET &&
                roll_chance_i(10))
            {
                static_cast<Pet*>(pet)->SendPetTalk(PET_TALK_ORDERED_ATTACK);
            }

            if (pet->GetTypeId() == TYPEID_UNIT &&
                static_cast<Creature*>(pet)->behavior())
            {
                static_cast<Creature*>(pet)->behavior()->attempt_attack(
                    TargetUnit);
            }
            else
            {
                // not let attack friendly units.
                if (GetPlayer()->IsFriendlyTo(TargetUnit))
                    return;

                // This is true if pet has no target or has target but targets
                // differs.
                if (pet->getVictim() != TargetUnit)
                {
                    if (pet->getVictim())
                    {
                        pet->AttackStop();
                        pet->InterruptNonMeleeSpells(false);
                    }

                    if (pet->hasUnitState(UNIT_STAT_CONTROLLED))
                    {
                        pet->Attack(TargetUnit, true);
                        charmInfo->SetIsReturning(false);
                        pet->SendPetAIReaction();
                    }
                    else if (pet->GetTypeId() == TYPEID_UNIT)
                    {
                        if (((Creature*)pet)->AI())
                            ((Creature*)pet)->AI()->AttackStart(TargetUnit);

                        pet->SendPetAIReaction();
                    }
                }
            }
            break;
        }
        case COMMAND_ABANDON: // abandon (hunter pet) or dismiss (summoned pet)
            if (((Creature*)pet)->IsPet())
            {
                Pet* p = (Pet*)pet;
                if (p->getPetType() == HUNTER_PET)
                    p->Unsummon(PET_SAVE_AS_DELETED, _player);
                else
                {
                    // dismissing a summoned pet is like killing them (this
                    // prevents returning a soulshard...)
                    // can still be charmed
                    if (_player->GetCharm() == p)
                        _player->Uncharm();
                    p->Unsummon(PET_SAVE_DISMISS_PET);
                }
            }
            else // charmed
                _player->Uncharm();
            break;
        default:
            logging.error("WORLD: unknown PET flag Action %i and spellid %i.",
                uint32(flag), spellid);
        }
        break;
    case ACT_REACTION: // 0x6
        switch (spellid)
        {
        case REACT_PASSIVE:
            if (static_cast<Creature*>(pet)->behavior())
            {
                static_cast<Creature*>(pet)->behavior()->attempt_passive();
            }
            else
            {
                pet->AttackStop();
                pet->InterruptNonMeleeSpells(false);
            }
        case REACT_DEFENSIVE:
        case REACT_AGGRESSIVE:
            charmInfo->SetReactState(ReactStates(spellid));
            break;
        }
        break;
    case ACT_DISABLED: // 0x81    spell (disabled), ignore
    case ACT_PASSIVE:  // 0x01
    case ACT_ENABLED:  // 0xC1    spell
    {
        Unit* unit_target = nullptr;
        if (targetGuid)
            unit_target = _player->GetMap()->GetUnit(targetGuid);

        // do not cast unknown spells
        SpellEntry const* spellInfo = sSpellStore.LookupEntry(spellid);
        if (!spellInfo)
        {
            logging.error("WORLD: unknown PET spell id %i", spellid);
            return;
        }

        if (pet->GetCharmInfo() &&
            pet->GetCharmInfo()->GetGlobalCooldownMgr().HasGlobalCooldown(
                spellInfo))
            return;

        for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
        {
            if ((spellInfo->EffectImplicitTargetA[i] ==
                        TARGET_ALL_ENEMY_IN_AREA ||
                    spellInfo->EffectImplicitTargetA[i] ==
                        TARGET_ALL_ENEMY_IN_AREA_INSTANT ||
                    spellInfo->EffectImplicitTargetA[i] ==
                        TARGET_ALL_ENEMY_IN_AREA_CHANNELED) &&
                (spellInfo->Targets & TARGET_FLAG_DEST_LOCATION) == 0)
                return;
        }

        // do not cast not learned spells
        if (!pet->HasSpell(spellid) || IsPassiveSpell(spellInfo))
            return;

        if (spellInfo->Targets & TARGET_FLAG_DEST_LOCATION && !unit_target)
            return;

        SpellCastTargets targets;

        if (spellInfo->Targets & TARGET_FLAG_DEST_LOCATION)
        {
            // validity of unit_target verified above
            targets.setDestination(
                unit_target->GetX(), unit_target->GetY(), unit_target->GetZ());
            unit_target = nullptr;
        }

        if (unit_target)
            targets.setUnitTarget(unit_target);

        // TYPEID checked at top of function for this action type
        Spell::attempt_pet_cast(
            static_cast<Creature*>(pet), spellInfo, targets, true);
        break;
    }
    default:
        logging.error("WORLD: unknown PET flag Action %i and spellid %i.",
            uint32(flag), spellid);
    }
}

void WorldSession::HandlePetStopAttack(WorldPacket& recv_data)
{
    ObjectGuid petGuid;
    recv_data >> petGuid;

    Unit* pet = GetPlayer()->GetMap()->GetUnit(
        petGuid); // pet or controlled creature/player
    if (!pet)
    {
        logging.error("%s doesn't exist.", petGuid.GetString().c_str());
        return;
    }

    if (GetPlayer()->GetObjectGuid() != pet->GetCharmerOrOwnerGuid())
    {
        logging.error("HandlePetStopAttack: %s isn't charm/pet of %s.",
            petGuid.GetString().c_str(), GetPlayer()->GetGuidStr().c_str());
        return;
    }

    if (!pet->isAlive())
        return;

    pet->InterruptNonMeleeSpells(false);
    pet->AttackStop();
}

void WorldSession::HandlePetNameQueryOpcode(WorldPacket& recv_data)
{
    uint32 petnumber;
    ObjectGuid petguid;

    recv_data >> petnumber;
    recv_data >> petguid;

    SendPetNameQuery(petguid, petnumber);
}

void WorldSession::SendPetNameQuery(ObjectGuid petguid, uint32 petnumber)
{
    Creature* pet = _player->GetMap()->GetAnyTypeCreature(petguid);
    if (!pet || !pet->GetCharmInfo() ||
        pet->GetCharmInfo()->GetPetNumber() != petnumber)
        return;

    char const* name = pet->GetName();

    // creature pets have localization like other creatures
    if (!pet->GetOwnerGuid().IsPlayer())
    {
        int loc_idx = GetSessionDbLocaleIndex();
        sObjectMgr::Instance()->GetCreatureLocaleStrings(
            pet->GetEntry(), loc_idx, &name);
    }

    WorldPacket data(SMSG_PET_NAME_QUERY_RESPONSE, (4 + 4 + strlen(name) + 1));
    data << uint32(petnumber);
    data << name;
    data << uint32(pet->GetUInt32Value(UNIT_FIELD_PET_NAME_TIMESTAMP));

    if (pet->IsPet() && ((Pet*)pet)->GetDeclinedNames())
    {
        data << uint8(1);
        for (int i = 0; i < MAX_DECLINED_NAME_CASES; ++i)
            data << ((Pet*)pet)->GetDeclinedNames()->name[i];
    }
    else
        data << uint8(0);

    _player->GetSession()->send_packet(std::move(data));
}

void WorldSession::HandlePetSetAction(WorldPacket& recv_data)
{
    ObjectGuid petGuid;
    uint8 count;

    recv_data >> petGuid;

    Creature* pet = _player->GetMap()->GetAnyTypeCreature(petGuid);

    if (!pet || (pet != _player->GetPet() && pet != _player->GetCharm()))
    {
        logging.error("HandlePetSetAction: Unknown pet or pet owner.");
        return;
    }

    // pet can have action bar disabled
    if (pet->IsPet() && ((Pet*)pet)->GetModeFlags() & PET_MODE_DISABLE_ACTIONS)
        return;

    CharmInfo* charmInfo = pet->GetCharmInfo();
    if (!charmInfo)
    {
        logging.error(
            "WorldSession::HandlePetSetAction: object (GUID: %u TypeId: %u) is "
            "considered pet-like but doesn't have a charminfo!",
            pet->GetGUIDLow(), pet->GetTypeId());
        return;
    }

    count = (recv_data.size() == 24) ? 2 : 1;

    uint32 position[2];
    uint32 data[2];
    bool move_command = false;

    for (uint8 i = 0; i < count; ++i)
    {
        recv_data >> position[i];
        recv_data >> data[i];

        uint8 act_state = UNIT_ACTION_BUTTON_TYPE(data[i]);

        // ignore invalid position
        if (position[i] >= MAX_UNIT_ACTION_BAR_INDEX)
            return;

        // in the normal case, command and reaction buttons can only be moved,
        // not removed
        // at moving count ==2, at removing count == 1
        // ignore attempt to remove command|reaction buttons (not possible at
        // normal case)
        if (act_state == ACT_COMMAND || act_state == ACT_REACTION)
        {
            if (count == 1)
                return;

            move_command = true;
        }
    }

    // check swap (at command->spell swap client remove spell first in another
    // packet, so check only command move correctness)
    if (move_command)
    {
        uint8 act_state_0 = UNIT_ACTION_BUTTON_TYPE(data[0]);
        if (act_state_0 == ACT_COMMAND || act_state_0 == ACT_REACTION)
        {
            uint32 spell_id_0 = UNIT_ACTION_BUTTON_ACTION(data[0]);
            UnitActionBarEntry const* actionEntry_1 =
                charmInfo->GetActionBarEntry(position[1]);
            if (!actionEntry_1 || spell_id_0 != actionEntry_1->GetAction() ||
                act_state_0 != actionEntry_1->GetType())
                return;
        }

        uint8 act_state_1 = UNIT_ACTION_BUTTON_TYPE(data[1]);
        if (act_state_1 == ACT_COMMAND || act_state_1 == ACT_REACTION)
        {
            uint32 spell_id_1 = UNIT_ACTION_BUTTON_ACTION(data[1]);
            UnitActionBarEntry const* actionEntry_0 =
                charmInfo->GetActionBarEntry(position[0]);
            if (!actionEntry_0 || spell_id_1 != actionEntry_0->GetAction() ||
                act_state_1 != actionEntry_0->GetType())
                return;
        }
    }

    for (uint8 i = 0; i < count; ++i)
    {
        uint32 spell_id = UNIT_ACTION_BUTTON_ACTION(data[i]);
        uint8 act_state = UNIT_ACTION_BUTTON_TYPE(data[i]);

        LOG_DEBUG(logging,
            "Player %s has changed pet spell action. Position: %u, Spell: %u, "
            "State: 0x%X",
            _player->GetName(), position[i], spell_id, uint32(act_state));

        // if it's act for spell (en/disable/cast) and there is a spell given (0
        // = remove spell) which pet doesn't know, don't add
        if (!((act_state == ACT_ENABLED || act_state == ACT_DISABLED ||
                  act_state == ACT_PASSIVE) &&
                spell_id && !pet->HasSpell(spell_id)))
        {
            // sign for autocast
            if (act_state == ACT_ENABLED && spell_id)
            {
                if (pet->isCharmed())
                    charmInfo->ToggleCreatureAutocast(spell_id, true);
                else
                    ((Pet*)pet)->ToggleAutocast(spell_id, true);
            }
            // sign for no/turn off autocast
            else if (act_state == ACT_DISABLED && spell_id)
            {
                if (pet->isCharmed())
                    charmInfo->ToggleCreatureAutocast(spell_id, false);
                else
                    ((Pet*)pet)->ToggleAutocast(spell_id, false);
            }

            charmInfo->SetActionBar(
                position[i], spell_id, ActiveStates(act_state));
        }
    }
}

void WorldSession::HandlePetRename(WorldPacket& recv_data)
{
    ObjectGuid petGuid;
    uint8 isdeclined;

    std::string name;
    DeclinedName declinedname;

    recv_data >> petGuid;
    recv_data >> name;
    recv_data >> isdeclined;

    Pet* pet = _player->GetMap()->GetPet(petGuid);
    // check it!
    if (!pet || pet->getPetType() != HUNTER_PET ||
        !pet->HasByteFlag(UNIT_FIELD_BYTES_2, 2, UNIT_CAN_BE_RENAMED) ||
        pet->GetOwnerGuid() != _player->GetObjectGuid() || !pet->GetCharmInfo())
        return;

    PetNameInvalidReason res = ObjectMgr::CheckPetName(name);
    if (res != PET_NAME_SUCCESS)
    {
        SendPetNameInvalid(res, name, nullptr);
        return;
    }

    if (sObjectMgr::Instance()->IsReservedName(name))
    {
        SendPetNameInvalid(PET_NAME_RESERVED, name, nullptr);
        return;
    }

    pet->SetName(name);

    if (_player->GetGroup())
        _player->SetGroupUpdateFlag(GROUP_UPDATE_FLAG_PET_NAME);

    pet->RemoveByteFlag(UNIT_FIELD_BYTES_2, 2, UNIT_CAN_BE_RENAMED);

    if (isdeclined)
    {
        for (int i = 0; i < MAX_DECLINED_NAME_CASES; ++i)
        {
            recv_data >> declinedname.name[i];
        }

        std::wstring wname;
        Utf8toWStr(name, wname);
        if (!ObjectMgr::CheckDeclinedNames(
                GetMainPartOfName(wname, 0), declinedname))
        {
            SendPetNameInvalid(PET_NAME_DECLENSION_DOESNT_MATCH_BASE_NAME, name,
                &declinedname);
            return;
        }
    }

    CharacterDatabase.BeginTransaction();
    if (isdeclined)
    {
        for (int i = 0; i < MAX_DECLINED_NAME_CASES; ++i)
            CharacterDatabase.escape_string(declinedname.name[i]);
        CharacterDatabase.PExecute(
            "DELETE FROM character_pet_declinedname WHERE owner = '%u' AND id "
            "= '%u'",
            _player->GetGUIDLow(), pet->GetCharmInfo()->GetPetNumber());
        CharacterDatabase.PExecute(
            "INSERT INTO character_pet_declinedname (id, owner, genitive, "
            "dative, accusative, instrumental, prepositional) VALUES "
            "('%u','%u','%s','%s','%s','%s','%s')",
            pet->GetCharmInfo()->GetPetNumber(), _player->GetGUIDLow(),
            declinedname.name[0].c_str(), declinedname.name[1].c_str(),
            declinedname.name[2].c_str(), declinedname.name[3].c_str(),
            declinedname.name[4].c_str());
    }

    CharacterDatabase.escape_string(name);
    CharacterDatabase.PExecute(
        "UPDATE character_pet SET name = '%s', renamed = '1' WHERE owner = "
        "'%u' AND id = '%u'",
        name.c_str(), _player->GetGUIDLow(),
        pet->GetCharmInfo()->GetPetNumber());
    CharacterDatabase.CommitTransaction();

    pet->SetUInt32Value(
        UNIT_FIELD_PET_NAME_TIMESTAMP, uint32(WorldTimer::time_no_syscall()));
}

void WorldSession::HandlePetAbandon(WorldPacket& recv_data)
{
    ObjectGuid guid;
    recv_data >> guid; // pet guid

    if (!_player->IsInWorld())
        return;

    // pet/charmed
    if (Creature* pet = _player->GetMap()->GetAnyTypeCreature(guid))
    {
        if (pet->IsPet())
        {
            if (pet->GetObjectGuid() == _player->GetPetGuid())
                pet->ModifyPower(POWER_HAPPINESS, -50000);

            ((Pet*)pet)->Unsummon(PET_SAVE_AS_DELETED, _player);
        }
        else if (pet->GetObjectGuid() == _player->GetCharmGuid())
        {
            _player->Uncharm();
        }
    }
}

void WorldSession::HandlePetUnlearnOpcode(WorldPacket& recvPacket)
{
    ObjectGuid guid;
    recvPacket >> guid; // Pet guid

    Pet* pet = _player->GetPet();

    if (!pet || guid != pet->GetObjectGuid())
    {
        logging.error("HandlePetUnlearnOpcode. %s isn't pet of %s .",
            guid.GetString().c_str(), GetPlayer()->GetGuidStr().c_str());
        return;
    }

    if (pet->getPetType() != HUNTER_PET || pet->m_spells.size() <= 1)
        return;

    CharmInfo* charmInfo = pet->GetCharmInfo();
    if (!charmInfo)
    {
        logging.error(
            "WorldSession::HandlePetUnlearnOpcode: %s is considered pet-like "
            "but doesn't have a charminfo!",
            pet->GetGuidStr().c_str());
        return;
    }

    uint32 cost = pet->resetTalentsCost();

    // XXX
    inventory::transaction trans;
    trans.remove(cost);
    if (!GetPlayer()->storage().finalize(trans))
    {
        GetPlayer()->SendBuyError(BUY_ERR_NOT_ENOUGHT_MONEY, nullptr, 0, 0);
        return;
    }

    auto create_spells =
        sObjectMgr::Instance()->GetPetCreateSpellEntry(pet->GetEntry());

    // Lambda: returns true if spell is a create spell
    auto skip_spell = [create_spells](uint32 spell_id)
    {
        if (!create_spells)
            return false;
        for (int i = 0; i < 4; ++i)
        {
            auto info = sSpellStore.LookupEntry(create_spells->spellid[i]);
            // Is this spell is a create spell, or learned by a create spell
            if (info &&
                (info->Id == spell_id ||
                    ((info->Effect[0] == SPELL_EFFECT_LEARN_SPELL ||
                         info->Effect[0] == SPELL_EFFECT_LEARN_PET_SPELL) &&
                        info->EffectTriggerSpell[0] == spell_id)))
            {
                return true;
            }
        }
        return false;
    };

    for (auto itr = pet->m_spells.begin(); itr != pet->m_spells.end();)
    {
        uint32 spell_id = itr->first; // Pet::removeSpell can invalidate
                                      // iterator at erase NEW spell
        ++itr;

        if (!skip_spell(spell_id))
            pet->unlearnSpell(spell_id, false);
    }

    pet->SetTP(pet->getLevel() * (pet->GetLoyaltyLevel() - 1));

    for (int i = 0; i < MAX_UNIT_ACTION_BAR_INDEX; ++i)
        if (UnitActionBarEntry const* ab = charmInfo->GetActionBarEntry(i))
            if (ab->GetAction() && ab->IsActionBarForSpell())
                if (!skip_spell(ab->GetAction()))
                    charmInfo->SetActionBar(i, 0, ACT_DISABLED);

    // relearn pet passives
    pet->LearnPetPassives();

    pet->m_resetTalentsTime = WorldTimer::time_no_syscall();
    pet->m_resetTalentsCost = cost;

    GetPlayer()->PetSpellInitialize();
}

void WorldSession::HandlePetSpellAutocastOpcode(WorldPacket& recvPacket)
{
    ObjectGuid guid;
    uint32 spellid;
    uint8 state; // 1 for on, 0 for off
    recvPacket >> guid >> spellid >> state;

    Creature* pet = _player->GetMap()->GetAnyTypeCreature(guid);
    if (!pet ||
        (guid != _player->GetPetGuid() && guid != _player->GetCharmGuid()))
    {
        logging.error("HandlePetSpellAutocastOpcode. %s isn't pet of %s .",
            guid.GetString().c_str(), GetPlayer()->GetGuidStr().c_str());
        return;
    }

    // do not add not learned spells/ passive spells
    if (!pet->HasSpell(spellid) || IsPassiveSpell(spellid))
        return;

    CharmInfo* charmInfo = pet->GetCharmInfo();
    if (!charmInfo)
    {
        logging.error(
            "WorldSession::HandlePetSpellAutocastOpcod: %s is considered "
            "pet-like but doesn't have a charminfo!",
            guid.GetString().c_str());
        return;
    }

    if (pet->isCharmed())
        // state can be used as boolean
        pet->GetCharmInfo()->ToggleCreatureAutocast(spellid, state);
    else
        ((Pet*)pet)->ToggleAutocast(spellid, state);

    charmInfo->SetSpellAutocast(spellid, state);
}

void WorldSession::HandlePetCastSpellOpcode(WorldPacket& recvPacket)
{
    ObjectGuid guid;
    uint32 spellid;

    recvPacket >> guid >> spellid;

    Creature* pet = _player->GetMap()->GetAnyTypeCreature(guid);

    if (!pet ||
        (guid != _player->GetPetGuid() && guid != _player->GetCharmGuid()))
    {
        logging.error("HandlePetCastSpellOpcode: %s isn't pet of %s .",
            guid.GetString().c_str(), GetPlayer()->GetGuidStr().c_str());
        return;
    }

    SpellEntry const* spellInfo = sSpellStore.LookupEntry(spellid);
    if (!spellInfo)
    {
        logging.error("WORLD: unknown PET spell id %i", spellid);
        return;
    }

    SpellCastTargets targets;

    recvPacket >> targets.ReadForCaster(pet);

    // For AoEs the client sends the creature as an explicit target, this would
    // fail in CheckCast
    if (targets.getUnitTargetGuid() == pet->GetObjectGuid() &&
        IsAreaOfEffectSpell(spellInfo) &&
        (spellInfo->Targets & TARGET_FLAG_DEST_LOCATION) == 0 &&
        !spellInfo->HasTargetType(TARGET_CHAIN_DAMAGE))
    {
        SpellCastTargets t;
        t.setSource(pet->GetX(), pet->GetY(), pet->GetZ());
        t.setDestination(pet->GetX(), pet->GetY(), pet->GetZ());
        targets = t;
    }
    else if (spellInfo->HasTargetType(TARGET_CHAIN_DAMAGE))
    {
        if (auto target = _player->GetMap()->GetUnit(_player->GetTargetGuid()))
            targets.setUnitTarget(target);
    }

    Spell::attempt_pet_cast(pet, spellInfo, targets, true);
}

void WorldSession::SendPetNameInvalid(
    uint32 error, const std::string& name, DeclinedName* declinedName)
{
    WorldPacket data(SMSG_PET_NAME_INVALID, 4 + name.size() + 1 + 1);
    data << uint32(error);
    data << name;
    if (declinedName)
    {
        data << uint8(1);
        for (uint32 i = 0; i < MAX_DECLINED_NAME_CASES; ++i)
            data << declinedName->name[i];
    }
    else
        data << uint8(0);
    send_packet(std::move(data));
}
