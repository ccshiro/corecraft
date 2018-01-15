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
#include "Corpse.h"
#include "GameObject.h"
#include "Group.h"
#include "logging.h"
#include "LootMgr.h"
#include "Object.h"
#include "ObjectAccessor.h"
#include "ObjectGuid.h"
#include "Player.h"
#include "Util.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "loot_distributor.h"

auto& ac_logger = logging.get_logger("anticheat");

void WorldSession::HandleAutostoreLootItemOpcode(WorldPacket& recv_data)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    uint8 loot_slot;
    recv_data >> loot_slot;

    ObjectGuid guid = player->GetLootGuid();
    if (!guid)
        return;

    Object* target = nullptr;
    if (guid.IsItem())
        target = player->GetItemByGuid(guid);
    else
        target = player->GetMap()->GetWorldObject(guid);

    if (!target)
        return;

    if (loot_distributor* ld = target->GetLootDistributor())
        ld->attempt_loot_item(player, loot_slot);
}

void WorldSession::HandleLootMoneyOpcode(WorldPacket& /*recv_data*/)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    ObjectGuid guid = player->GetLootGuid();
    if (!guid)
        return;

    Object* target = nullptr;
    if (guid.IsItem())
        target = player->GetItemByGuid(guid);
    else
        target = player->GetMap()->GetWorldObject(guid);

    if (!target)
        return;

    if (loot_distributor* ld = target->GetLootDistributor())
        ld->attempt_loot_money(player);
}

void WorldSession::HandleLootOpcode(WorldPacket& recv_data)
{
    ObjectGuid guid;
    recv_data >> guid;

    // Check possible cheat
    if (!_player->isAlive())
        return;

    WorldObject* obj = _player->GetMap()->GetWorldObject(guid);

    // This opcode is only used for instant loots, i.e. not ones handled through
    // spells
    if ((obj && obj->GetTypeId() == TYPEID_UNIT) ||
        (!guid.IsCreature() && !guid.IsCorpse()))
    {
        // Don't log skinning as a cheating attempt; it can happen with a legit
        // client that hasn't received the skinning flag yet
        if (!obj)
        {
            ac_logger.info(
                "%s tried to cheat by opening %s through HandleLootOpcode; "
                "this loot have casting times, which would be bypassed by "
                "this!",
                _player->GetObjectGuid().GetString().c_str(),
                guid.GetString().c_str());
            logging.error(
                "%s tried to cheat by opening %s through HandleLootOpcode; "
                "this loot have casting times, which would be bypassed by "
                "this!",
                _player->GetObjectGuid().GetString().c_str(),
                guid.GetString().c_str());
        }

        if (!obj ||
            (obj && obj->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_SKINNABLE)))
        {
            _player->SendLootRelease(guid);
            return;
        }
    }

    // Check distance & in map status
    if (!obj || !_player->IsWithinDistInMap(obj, 7.0f))
    {
        _player->SendLootRelease(guid);
        return;
    }

    // Corpse must've had insignia removed before being lootable
    if (obj->GetTypeId() == TYPEID_CORPSE &&
        !static_cast<Corpse*>(obj)->HasFlag(
            CORPSE_FIELD_DYNAMIC_FLAGS, CORPSE_DYNFLAG_LOOTABLE))
    {
        _player->SendLootRelease(guid);
        return;
    }

    // Break stealth/invis/feign death
    _player->remove_auras_on_event(AURA_INTERRUPT_FLAG_INTERACT);
    _player->InterruptNonMeleeSpells(false);

    _player->SendLoot(guid, guid.IsCorpse() ? LOOT_INSIGNIA : LOOT_CORPSE);
}

void WorldSession::HandleLootReleaseOpcode(WorldPacket& recv_data)
{
    // cheaters can modify lguid to prevent correct apply loot release code and
    // re-loot
    // use internal stored guid
    recv_data.read_skip<uint64>(); // guid;
    if (!GetPlayer())
        return;

    if (ObjectGuid lootGuid = GetPlayer()->GetLootGuid())
        DoLootRelease(lootGuid);
}

void WorldSession::DoLootRelease(ObjectGuid guid)
{
    // XXX
    Player* player = GetPlayer();
    if (!player)
        return;
    Object* lootee = nullptr;

    player->SetLootGuid(ObjectGuid());
    player->SendLootRelease(guid);

    player->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_LOOTING);

    if (!player->IsInWorld())
        return;

    if (guid.IsItem())
        lootee = player->GetItemByGuid(guid);
    else
        lootee = player->GetMap()->GetWorldObject(guid);

    if (!lootee || !lootee->GetLootDistributor() ||
        !lootee->GetLootDistributor()->loot())
        return;

    // Close loot display before the rest of the code (to properly mark the loot
    // as looted if that's the case)
    lootee->GetLootDistributor()->close_loot_display(player);

    switch (guid.GetHigh())
    {
    case HIGHGUID_GAMEOBJECT:
    {
        GameObject* go = static_cast<GameObject*>(lootee);

        auto loot = go->GetLootDistributor()->loot();
        bool looted = loot->looted();
        // quest items remaining are ignored for looted status, unless the GO is
        // a quest-only GO
        if (!looted && !go->HasFlag(GAMEOBJECT_FLAGS, GO_FLAG_INTERACT_COND) &&
            loot->do_only_quest_items_remain())
            looted = true;

        // not check distance for GO in case owned GO (fishing bobber case, for
        // example) or Fishing hole GO
        if (((go->GetOwnerGuid() != _player->GetObjectGuid() &&
                 go->GetGoType() != GAMEOBJECT_TYPE_FISHINGHOLE) &&
                !go->IsWithinDistInMap(_player, INTERACTION_DISTANCE + 3.0f)))
            return;

        if (go->GetGoType() == GAMEOBJECT_TYPE_DOOR)
        {
            // locked doors are opened with spelleffect openlock, prevent remove
            // its as looted
            go->UseDoorOrButton();
        }
        else if (looted || go->GetGoType() == GAMEOBJECT_TYPE_FISHINGNODE)
        {
            // GO is mineral vein? so it is not removed after its looted
            if (go->GetGoType() == GAMEOBJECT_TYPE_CHEST)
            {
                uint32 go_min = go->GetGOInfo()->chest.minSuccessOpens;
                uint32 go_max = go->GetGOInfo()->chest.maxSuccessOpens;

                // only vein pass this check
                if (go_min != 0 && go_max > go_min)
                {
                    float amount_rate = sWorld::Instance()->getConfig(
                        CONFIG_FLOAT_RATE_MINING_AMOUNT);
                    float min_amount = go_min * amount_rate;
                    float max_amount = go_max * amount_rate;

                    go->AddUse();
                    float uses = float(go->GetUseCount());

                    if (uses < max_amount)
                    {
                        if (uses >= min_amount)
                        {
                            float chance_rate = sWorld::Instance()->getConfig(
                                CONFIG_FLOAT_RATE_MINING_NEXT);

                            int32 ReqValue = 175;
                            LockEntry const* lockInfo = sLockStore.LookupEntry(
                                go->GetGOInfo()->chest.lockId);
                            if (lockInfo)
                                ReqValue = lockInfo->Skill[0];
                            float skill =
                                float(player->GetSkillValue(SKILL_MINING)) /
                                (ReqValue + 25);
                            double chance = pow(0.8 * chance_rate,
                                4 * (1 / double(max_amount)) * double(uses));
                            if (roll_chance_f(float(100.0f * chance + skill)))
                            {
                                go->SetLootState(GO_READY);
                            }
                            else // not have more uses
                                go->SetLootState(GO_JUST_DEACTIVATED);
                        }
                        else // 100% chance until min uses
                            go->SetLootState(GO_READY);
                    }
                    else // max uses already
                        go->SetLootState(GO_JUST_DEACTIVATED);
                }
                else // not vein
                    go->SetLootState(GO_JUST_DEACTIVATED);
            }
            else if (go->GetGoType() == GAMEOBJECT_TYPE_FISHINGHOLE)
            {                 // The fishing hole used once more
                go->AddUse(); // if the max usage is reached, will be despawned
                              // at next tick
                if (go->GetUseCount() >=
                    urand(go->GetGOInfo()->fishinghole.minSuccessOpens,
                        go->GetGOInfo()->fishinghole.maxSuccessOpens))
                {
                    go->SetLootState(GO_JUST_DEACTIVATED);
                }
                else
                    go->SetLootState(GO_READY);
            }
            else // not chest (or vein/herb/etc)
                go->SetLootState(GO_JUST_DEACTIVATED);

            go->DeleteLootDistributor();
        }
        else
            // not fully looted object
            go->SetLootState(GO_ACTIVATED);
        break;
    }
    case HIGHGUID_CORPSE: // ONLY remove insignia at BG
    {
        Corpse* corpse = (Corpse*)lootee;

        if (corpse->GetLootDistributor()->loot()->looted())
        {
            corpse->DeleteLootDistributor();
            corpse->RemoveFlag(
                CORPSE_FIELD_DYNAMIC_FLAGS, CORPSE_DYNFLAG_LOOTABLE);
        }
        break;
    }
    case HIGHGUID_ITEM:
    {
        Item* pItem = (Item*)lootee;

        switch (pItem->GetLootDistributor()->loot_type())
        {
        // temporary loot in stacking items, clear loot state, no auto loot move
        case LOOT_PROSPECTING:
        {
            uint32 itemid = pItem->GetProto()->ItemId;

            // In case player managed to somehow move the ore, or split it, just
            // consume 5 stacks from anywhere
            if (pItem->GetCount() < 5 ||
                _player->storage().remove_count(pItem, 5) != EQUIP_ERR_OK)
            {
                inventory::transaction trans(false);
                trans.destroy(itemid, 5);
                player->storage().finalize(trans);
            }

            pItem->DeleteLootDistributor();
            pItem->SetLootState(ITEM_LOOT_REMOVED);
            break;
        }
        // temporary loot, auto loot move
        case LOOT_DISENCHANTING:
        {
            if (!pItem->GetLootDistributor()->loot()->looted())
                pItem->GetLootDistributor()->auto_store_all_loot(player);
            pItem->DeleteLootDistributor();
            pItem->SetLootState(ITEM_LOOT_REMOVED);

            inventory::transaction trans(false);
            trans.destroy(pItem);
            player->storage().finalize(trans);
            break;
        }
        // normal persistence loot
        default:
        {
            // must be destroyed only if no loot
            if (pItem->GetLootDistributor()->loot()->looted())
            {
                pItem->SetLootState(ITEM_LOOT_REMOVED);

                inventory::transaction trans(false);
                trans.destroy(pItem);
                player->storage().finalize(trans);
            }
            break;
        }
        }
        return; // item can be looted only single player
    }
    case HIGHGUID_UNIT:
    case HIGHGUID_PET:
    {
        Creature* pCreature = (Creature*)lootee;

        if (pCreature->isAlive())
            return; // Don't handle pickpocketing except to send a release
                    // packet back (which we do above)

        if (!pCreature->IsWithinDistInMap(_player, INTERACTION_DISTANCE))
            return;

        if (pCreature->GetLootDistributor()->loot()->looted())
        {
            if (pCreature->GetLootDistributor()->loot_type() == LOOT_CORPSE &&
                !guid.IsPet()) // NOTE: We don't have a skinning distributor for
                               // pets, so for them we go directly to finished
                pCreature->PrepareSkinningLoot();
            else
                pCreature->FinishedLooting();
        }
        break;
    }
    default:
    {
        logging.error(
            "%s is unsupported for looting.", guid.GetString().c_str());
        return;
    }
    }
}

void WorldSession::HandleLootMasterGiveOpcode(WorldPacket& recv_data)
{
    uint8 slot_id;
    ObjectGuid guid;
    ObjectGuid target_guid;

    recv_data >> guid >> slot_id >> target_guid;

    if (!_player->GetGroup())
    {
        _player->SendLootRelease(GetPlayer()->GetLootGuid());
        return;
    }

    Player* target = ObjectAccessor::FindPlayer(target_guid);
    if (!target)
        return;

    WorldObject* obj = _player->GetMap()->GetWorldObject(guid);
    if (!obj)
        return;

    if (loot_distributor* ld = obj->GetLootDistributor())
        ld->attempt_master_loot_handout(_player, target, slot_id);
}
