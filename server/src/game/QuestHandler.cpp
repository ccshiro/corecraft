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
#include "GossipDef.h"
#include "Group.h"
#include "logging.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Opcodes.h"
#include "Player.h"
#include "QuestDef.h"
#include "ScriptMgr.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"

void WorldSession::HandleQuestgiverStatusQueryOpcode(WorldPacket& recv_data)
{
    ObjectGuid guid;
    recv_data >> guid;
    uint8 dialogStatus = DIALOG_STATUS_NONE;

    Object* questgiver =
        _player->GetObjectByTypeMask(guid, TYPEMASK_CREATURE_OR_GAMEOBJECT);
    if (!questgiver)
    {
        LOG_DEBUG(logging,
            "Error in CMSG_QUESTGIVER_STATUS_QUERY, called for not found "
            "questgiver %s",
            guid.GetString().c_str());
        return;
    }

    switch (questgiver->GetTypeId())
    {
    case TYPEID_UNIT:
    {
        Creature* cr_questgiver = (Creature*)questgiver;

        if (!cr_questgiver->IsHostileTo(
                _player)) // not show quest status to enemies
        {
            dialogStatus =
                sScriptMgr::Instance()->GetDialogStatus(_player, cr_questgiver);

            if (dialogStatus > 6)
                dialogStatus =
                    getDialogStatus(_player, cr_questgiver, DIALOG_STATUS_NONE);
        }
        break;
    }
    case TYPEID_GAMEOBJECT:
    {
        GameObject* go_questgiver = (GameObject*)questgiver;
        dialogStatus =
            sScriptMgr::Instance()->GetDialogStatus(_player, go_questgiver);

        if (dialogStatus > 6)
            dialogStatus =
                getDialogStatus(_player, go_questgiver, DIALOG_STATUS_NONE);

        break;
    }
    default:
        logging.error("QuestGiver called for unexpected type %u",
            questgiver->GetTypeId());
        break;
    }

    // inform client about status of quest
    _player->PlayerTalkClass->SendQuestGiverStatus(dialogStatus, guid);
}

void WorldSession::HandleQuestgiverHelloOpcode(WorldPacket& recv_data)
{
    ObjectGuid guid;
    recv_data >> guid;

    Creature* pCreature =
        GetPlayer()->GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_NONE);
    if (!pCreature)
    {
        LOG_DEBUG(logging,
            "WORLD: HandleQuestgiverHelloOpcode - for %s to %s not found or "
            "you can't interact with him.",
            _player->GetGuidStr().c_str(), guid.GetString().c_str());
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

    if (sScriptMgr::Instance()->OnGossipHello(_player, pCreature))
        return;

    _player->PrepareGossipMenu(
        pCreature, pCreature->GetCreatureInfo()->GossipMenuId);
    _player->SendPreparedGossip(pCreature);
}

void WorldSession::HandleQuestgiverAcceptQuestOpcode(WorldPacket& recv_data)
{
    ObjectGuid guid;
    uint32 quest;
    recv_data >> guid >> quest;

    if (!CanInteractWithQuestGiver(guid))
        return;

    Object* pObject = _player->GetObjectByTypeMask(
        guid, TYPEMASK_CREATURE_GAMEOBJECT_PLAYER_OR_ITEM);

    // no or incorrect quest giver
    if (!pObject ||
        (pObject->GetTypeId() != TYPEID_PLAYER && !pObject->HasQuest(quest)) ||
        (pObject->GetTypeId() == TYPEID_PLAYER &&
            !((Player*)pObject)->CanShareQuest(quest)))
    {
        _player->PlayerTalkClass->CloseGossip();
        _player->ClearDividerGuid();
        return;
    }

    WorldObject* wo_giver = pObject->GetTypeId() != TYPEID_ITEM ?
                                static_cast<WorldObject*>(pObject) :
                                nullptr;

    Quest const* qInfo = sObjectMgr::Instance()->GetQuestTemplate(quest);
    if (qInfo)
    {
        // prevent cheating
        if (!GetPlayer()->CanTakeQuest(qInfo, true, wo_giver))
        {
            _player->PlayerTalkClass->CloseGossip();
            _player->ClearDividerGuid();
            return;
        }

        if (Player* pPlayer =
                ObjectAccessor::FindPlayer(_player->GetDividerGuid()))
        {
            pPlayer->SendPushToPartyResponse(
                _player, QUEST_PARTY_MSG_ACCEPT_QUEST);
            _player->ClearDividerGuid();
        }

        if (_player->CanAddQuest(qInfo, true))
        {
            _player->AddQuest(qInfo,
                pObject); // pObject (if it item) can be destroyed at call

            if (qInfo->HasQuestFlag(QUEST_FLAGS_PARTY_ACCEPT))
            {
                if (Group* group = _player->GetGroup())
                {
                    for (auto member : group->members(true))
                    {
                        if (member == _player) // not self
                            continue;

                        if (member->CanTakeQuest(qInfo, true, wo_giver))
                        {
                            member->SetDividerGuid(_player->GetObjectGuid());

                            // need confirmation that any gossip window will
                            // close
                            member->PlayerTalkClass->CloseGossip();

                            _player->SendQuestConfirmAccept(qInfo, member);
                        }
                    }
                }
            }

            if (_player->CanCompleteQuest(quest))
                _player->CompleteQuest(quest);

            _player->PlayerTalkClass->CloseGossip();

            if (qInfo->GetSrcSpell() > 0)
                _player->CastSpell(_player, qInfo->GetSrcSpell(), true);

            return;
        }
    }

    _player->PlayerTalkClass->CloseGossip();
}

void WorldSession::HandleQuestgiverQueryQuestOpcode(WorldPacket& recv_data)
{
    ObjectGuid guid;
    uint32 quest;
    recv_data >> guid >> quest;

    // Verify that the guid is valid and is a questgiver or involved in the
    // requested quest
    Object* pObject = _player->GetObjectByTypeMask(
        guid, TYPEMASK_CREATURE_GAMEOBJECT_OR_ITEM);
    if (!pObject ||
        (!pObject->HasQuest(quest) && !pObject->HasInvolvedQuest(quest)))
    {
        _player->PlayerTalkClass->CloseGossip();
        return;
    }

    if (Quest const* pQuest = sObjectMgr::Instance()->GetQuestTemplate(quest))
        _player->PlayerTalkClass->SendQuestGiverQuestDetails(
            pQuest, pObject->GetObjectGuid(), true);
}

void WorldSession::HandleQuestQueryOpcode(WorldPacket& recv_data)
{
    uint32 quest;
    recv_data >> quest;
    Quest const* pQuest = sObjectMgr::Instance()->GetQuestTemplate(quest);
    if (pQuest)
    {
        _player->PlayerTalkClass->SendQuestQueryResponse(pQuest);
    }
}

void WorldSession::HandleQuestgiverChooseRewardOpcode(WorldPacket& recv_data)
{
    uint32 quest, reward;
    ObjectGuid guid;
    recv_data >> guid >> quest >> reward;

    if (reward >= QUEST_REWARD_CHOICES_COUNT)
    {
        logging.error(
            "Error in CMSG_QUESTGIVER_CHOOSE_REWARD - %s (guid: %u) tried to "
            "get invalid reward (%u) (probably packet hacking)",
            _player->GetGuidStr().c_str(), _player->GetGUIDLow(), reward);
        return;
    }

    if (!CanInteractWithQuestGiver(guid))
        return;

    Object* pObject =
        _player->GetObjectByTypeMask(guid, TYPEMASK_CREATURE_OR_GAMEOBJECT);
    if (!pObject)
        return;

    if (!pObject->HasInvolvedQuest(quest))
        return;

    Quest const* pQuest = sObjectMgr::Instance()->GetQuestTemplate(quest);
    if (pQuest)
    {
        if (_player->RewardQuest(pQuest, reward, pObject))
        {
            // Send next quest
            if (Quest const* nextquest = _player->GetNextQuest(guid, pQuest))
            {
                _player->PlayerTalkClass->SendQuestGiverQuestDetails(
                    nextquest, guid, true);
            }
        }
        else
        {
            // Resend offer reward
            _player->PlayerTalkClass->SendQuestGiverOfferReward(
                pQuest, guid, true);
        }
    }
}

void WorldSession::HandleQuestgiverRequestRewardOpcode(WorldPacket& recv_data)
{
    uint32 quest;
    ObjectGuid guid;
    recv_data >> guid >> quest;

    if (!CanInteractWithQuestGiver(guid))
        return;

    Object* pObject =
        _player->GetObjectByTypeMask(guid, TYPEMASK_CREATURE_OR_GAMEOBJECT);
    if (!pObject || !pObject->HasInvolvedQuest(quest))
        return;

    if (_player->CanCompleteQuest(quest))
        _player->CompleteQuest(quest);

    if (_player->GetQuestStatus(quest) != QUEST_STATUS_COMPLETE)
        return;

    if (Quest const* pQuest = sObjectMgr::Instance()->GetQuestTemplate(quest))
        _player->PlayerTalkClass->SendQuestGiverOfferReward(pQuest, guid, true);
}

void WorldSession::HandleQuestgiverCancel(WorldPacket& /*recv_data*/)
{
    _player->PlayerTalkClass->CloseGossip();
}

void WorldSession::HandleQuestLogSwapQuest(WorldPacket& recv_data)
{
    uint8 slot1, slot2;
    recv_data >> slot1 >> slot2;

    if (slot1 == slot2 || slot1 >= MAX_QUEST_LOG_SIZE ||
        slot2 >= MAX_QUEST_LOG_SIZE)
        return;

    GetPlayer()->SwapQuestSlot(slot1, slot2);
}

void WorldSession::HandleQuestLogRemoveQuest(WorldPacket& recv_data)
{
    uint8 slot;
    recv_data >> slot;

    if (slot < MAX_QUEST_LOG_SIZE)
    {
        if (uint32 quest = _player->GetQuestSlotQuestId(slot))
        {
            if (!_player->TakeQuestSourceItem(quest, true))
                return; // can't un-equip some items, reject quest cancel

            if (const Quest* pQuest =
                    sObjectMgr::Instance()->GetQuestTemplate(quest))
            {
                if (pQuest->HasSpecialFlag(QUEST_SPECIAL_FLAG_TIMED))
                    _player->RemoveTimedQuest(quest);

                inventory::transaction trans(false,
                    inventory::transaction::send_self,
                    inventory::transaction::add_loot, true); // Include bank
                for (int i = 0; i < QUEST_SOURCE_ITEM_IDS_COUNT; ++i)
                {
                    // Delete items with quest binding that are directly
                    // required by the quest
                    if (pQuest->ReqItemId[i])
                    {
                        const ItemPrototype* proto =
                            ObjectMgr::GetItemPrototype(pQuest->ReqItemId[i]);
                        if (proto && proto->Bonding == BIND_QUEST_ITEM)
                        {
                            if (uint32 count = _player->storage().item_count(
                                    pQuest->ReqItemId[i]))
                                trans.destroy(pQuest->ReqItemId[i], count);
                        }
                    }
                    // Delete items with quest binding that are indirectly
                    // required by the quest
                    // (For example, items that when activated turns into the
                    // item actually needed by the quest)
                    if (pQuest->ReqSourceId[i])
                    {
                        const ItemPrototype* proto =
                            ObjectMgr::GetItemPrototype(pQuest->ReqSourceId[i]);
                        if (proto && proto->Bonding == BIND_QUEST_ITEM)
                        {
                            if (uint32 count = _player->storage().item_count(
                                    pQuest->ReqSourceId[i]))
                                trans.destroy(pQuest->ReqSourceId[i], count);
                        }
                    }
                }
                _player->storage().finalize(trans);
            }

            _player->SetQuestStatus(quest, QUEST_STATUS_NONE);
        }

        _player->SetQuestSlot(slot, 0);
    }
}

void WorldSession::HandleQuestConfirmAccept(WorldPacket& recv_data)
{
    uint32 quest;
    recv_data >> quest;

    if (const Quest* pQuest = sObjectMgr::Instance()->GetQuestTemplate(quest))
    {
        if (!pQuest->HasQuestFlag(QUEST_FLAGS_PARTY_ACCEPT))
            return;

        Player* pOriginalPlayer =
            ObjectAccessor::FindPlayer(_player->GetDividerGuid());

        if (!pOriginalPlayer)
            return;

        if (pQuest->IsAllowedInRaid())
        {
            if (!_player->IsInSameRaidWith(pOriginalPlayer))
                return;
        }
        else
        {
            if (!_player->IsInSameGroupWith(pOriginalPlayer))
                return;
        }

        if (_player->CanAddQuest(pQuest, true))
            _player->AddQuest(pQuest,
                nullptr); // NULL, this prevent DB script from duplicate running

        _player->ClearDividerGuid();
    }
}

void WorldSession::HandleQuestgiverCompleteQuest(WorldPacket& recv_data)
{
    uint32 quest;
    ObjectGuid guid;
    recv_data >> guid >> quest;

    if (!CanInteractWithQuestGiver(guid))
        return;

    // All ok, continue
    if (Quest const* pQuest = sObjectMgr::Instance()->GetQuestTemplate(quest))
    {
        if (_player->GetQuestStatus(quest) != QUEST_STATUS_COMPLETE)
        {
            if (pQuest->IsRepeatable())
                _player->PlayerTalkClass->SendQuestGiverRequestItems(pQuest,
                    guid, _player->CanCompleteRepeatableQuest(pQuest), false);
            else
                _player->PlayerTalkClass->SendQuestGiverRequestItems(pQuest,
                    guid, _player->CanRewardQuest(pQuest, false), false);
        }
        else
            _player->PlayerTalkClass->SendQuestGiverRequestItems(
                pQuest, guid, _player->CanRewardQuest(pQuest, false), false);
    }
}

void WorldSession::HandleQuestgiverQuestAutoLaunch(WorldPacket& /*recvPacket*/)
{
}

void WorldSession::HandlePushQuestToParty(WorldPacket& recvPacket)
{
    uint32 questId;
    recvPacket >> questId;

    if (Quest const* pQuest = sObjectMgr::Instance()->GetQuestTemplate(questId))
    {
        if (Group* group = _player->GetGroup())
        {
            for (auto member : group->members(true))
            {
                if (member == _player) // skip self
                    continue;

                _player->SendPushToPartyResponse(
                    member, QUEST_PARTY_MSG_SHARING_QUEST);

                if (!_player->IsWithinDistInMap(member, 10))
                {
                    _player->SendPushToPartyResponse(
                        member, QUEST_PARTY_MSG_TOO_FAR);
                    continue;
                }

                if (!member->SatisfyQuestStatus(pQuest, false))
                {
                    _player->SendPushToPartyResponse(
                        member, QUEST_PARTY_MSG_HAVE_QUEST);
                    continue;
                }

                if (member->GetQuestStatus(questId) == QUEST_STATUS_COMPLETE)
                {
                    _player->SendPushToPartyResponse(
                        member, QUEST_PARTY_MSG_FINISH_QUEST);
                    continue;
                }

                if (!member->CanTakeQuest(pQuest, false, nullptr))
                {
                    _player->SendPushToPartyResponse(
                        member, QUEST_PARTY_MSG_CANT_TAKE_QUEST);
                    continue;
                }

                if (!member->SatisfyQuestLog(false))
                {
                    _player->SendPushToPartyResponse(
                        member, QUEST_PARTY_MSG_LOG_FULL);
                    continue;
                }

                if (member->GetDividerGuid())
                {
                    _player->SendPushToPartyResponse(
                        member, QUEST_PARTY_MSG_BUSY);
                    continue;
                }

                member->PlayerTalkClass->SendQuestGiverQuestDetails(
                    pQuest, _player->GetObjectGuid(), true);
                member->SetDividerGuid(_player->GetObjectGuid());
            }
        }
    }
}

void WorldSession::HandleQuestPushResult(WorldPacket& recvPacket)
{
    ObjectGuid guid;
    uint8 msg;
    recvPacket >> guid >> msg;

    if (Player* pPlayer = ObjectAccessor::FindPlayer(_player->GetDividerGuid()))
    {
        WorldPacket data(MSG_QUEST_PUSH_RESULT, (8 + 1));
        data << ObjectGuid(guid);
        data << uint8(msg); // valid values: 0-8
        pPlayer->GetSession()->send_packet(std::move(data));
        _player->ClearDividerGuid();
    }
}

uint32 WorldSession::getDialogStatus(
    Player* pPlayer, Object* questgiver, uint32 defstatus)
{
    uint32 dialogStatus = defstatus;

    QuestRelationsMapBounds rbounds;
    QuestRelationsMapBounds irbounds;

    switch (questgiver->GetTypeId())
    {
    case TYPEID_UNIT:
    {
        rbounds = sObjectMgr::Instance()->GetCreatureQuestRelationsMapBounds(
            questgiver->GetEntry());
        irbounds =
            sObjectMgr::Instance()->GetCreatureQuestInvolvedRelationsMapBounds(
                questgiver->GetEntry());
        break;
    }
    case TYPEID_GAMEOBJECT:
    {
        rbounds = sObjectMgr::Instance()->GetGOQuestRelationsMapBounds(
            questgiver->GetEntry());
        irbounds = sObjectMgr::Instance()->GetGOQuestInvolvedRelationsMapBounds(
            questgiver->GetEntry());
        break;
    }
    default:
        // it's impossible, but check ^)
        logging.error("Warning: GetDialogStatus called for unexpected type %u",
            questgiver->GetTypeId());
        return DIALOG_STATUS_NONE;
    }

    auto qgiver_wo = static_cast<WorldObject*>(questgiver);

    for (auto itr = irbounds.first; itr != irbounds.second; ++itr)
    {
        uint32 dialogStatusNew = 0;
        uint32 quest_id = itr->second;
        Quest const* pQuest =
            sObjectMgr::Instance()->GetQuestTemplate(quest_id);

        if (!pQuest || !pQuest->IsActive())
            continue;

        QuestStatus status = pPlayer->GetQuestStatus(quest_id);

        if ((status == QUEST_STATUS_COMPLETE &&
                !pPlayer->GetQuestRewardStatus(quest_id)) ||
            (pQuest->IsAutoComplete() &&
                pPlayer->CanTakeQuest(pQuest, false, qgiver_wo)))
        {
            if (pQuest->IsAutoComplete() && pQuest->IsRepeatable())
                dialogStatusNew = DIALOG_STATUS_REWARD_REP;
            else
                dialogStatusNew = DIALOG_STATUS_REWARD;
        }
        else if (status == QUEST_STATUS_INCOMPLETE)
            dialogStatusNew = DIALOG_STATUS_INCOMPLETE;

        if (dialogStatusNew > dialogStatus)
            dialogStatus = dialogStatusNew;
    }

    for (auto itr = rbounds.first; itr != rbounds.second; ++itr)
    {
        uint32 dialogStatusNew = 0;
        uint32 quest_id = itr->second;
        Quest const* pQuest =
            sObjectMgr::Instance()->GetQuestTemplate(quest_id);

        if (!pQuest || !pQuest->IsActive())
            continue;

        QuestStatus status = pPlayer->GetQuestStatus(quest_id);

        if (status == QUEST_STATUS_NONE)
        {
            if (pPlayer->CanSeeStartQuest(pQuest, qgiver_wo))
            {
                if (pPlayer->SatisfyQuestLevel(pQuest, false))
                {
                    if (pQuest->IsAutoComplete() ||
                        (pQuest->IsRepeatable() &&
                            pPlayer->getQuestStatusMap()[quest_id].m_rewarded))
                    {
                        dialogStatusNew = DIALOG_STATUS_REWARD_REP;
                    }
                    else if (int32(pPlayer->getLevel()) <=
                             int32(pPlayer->GetQuestLevelForPlayer(pQuest)) +
                                 sWorld::Instance()->getConfig(
                                     CONFIG_INT32_QUEST_LOW_LEVEL_HIDE_DIFF))
                    {
                        if (pQuest->HasQuestFlag(QUEST_FLAGS_DAILY))
                            dialogStatusNew = DIALOG_STATUS_AVAILABLE_REP;
                        else
                            dialogStatusNew = DIALOG_STATUS_AVAILABLE;
                    }
                    else
                        dialogStatusNew = DIALOG_STATUS_CHAT;
                }
                else
                    dialogStatusNew = DIALOG_STATUS_UNAVAILABLE;
            }
        }

        if (dialogStatusNew > dialogStatus)
            dialogStatus = dialogStatusNew;
    }

    return dialogStatus;
}

void WorldSession::SendQuestgiverStatusMultiple()
{
    uint32 count = 0;

    WorldPacket data(SMSG_QUESTGIVER_STATUS_MULTIPLE, 4);
    size_t count_pos = data.wpos();
    data << uint32(count); // placeholder

    for (const auto& elem : _player->m_clientGUIDs)
    {
        uint8 dialogStatus = DIALOG_STATUS_NONE;

        if (elem.IsAnyTypeCreature())
        {
            // need also pet quests case support
            Creature* questgiver =
                GetPlayer()->GetMap()->GetAnyTypeCreature(elem);

            if (!questgiver || questgiver->IsHostileTo(_player))
                continue;

            if (!questgiver->HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_QUESTGIVER))
                continue;

            dialogStatus =
                sScriptMgr::Instance()->GetDialogStatus(_player, questgiver);

            if (dialogStatus > 6)
                dialogStatus =
                    getDialogStatus(_player, questgiver, DIALOG_STATUS_NONE);

            data << questgiver->GetObjectGuid();
            data << uint8(dialogStatus);
            ++count;
        }
        else if (elem.IsGameObject())
        {
            GameObject* questgiver = GetPlayer()->GetMap()->GetGameObject(elem);

            if (!questgiver)
                continue;

            if (questgiver->GetGoType() != GAMEOBJECT_TYPE_QUESTGIVER)
                continue;

            dialogStatus =
                sScriptMgr::Instance()->GetDialogStatus(_player, questgiver);

            if (dialogStatus > 6)
                dialogStatus =
                    getDialogStatus(_player, questgiver, DIALOG_STATUS_NONE);

            data << questgiver->GetObjectGuid();
            data << uint8(dialogStatus);
            ++count;
        }
    }

    data.put<uint32>(count_pos, count); // write real count
    send_packet(std::move(data));
}

void WorldSession::HandleQuestgiverStatusMultipleQuery(
    WorldPacket& /*recvPacket*/)
{
    SendQuestgiverStatusMultiple();
}

bool WorldSession::CanInteractWithQuestGiver(const ObjectGuid& guid)
{
    if (guid.IsCreature())
    {
        Creature* pCreature =
            _player->GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_QUESTGIVER);
        if (!pCreature)
        {
            LOG_DEBUG(logging, "WORLD: %s cannot interact with %s.",
                _player->GetGuidStr().c_str(), guid.GetString().c_str());
            return false;
        }
    }
    else if (guid.IsGameObject())
    {
        GameObject* pGo = _player->GetGameObjectIfCanInteractWith(
            guid, GAMEOBJECT_TYPE_QUESTGIVER);
        if (!pGo)
        {
            LOG_DEBUG(logging, "WORLD: %s cannot interact with %s.",
                _player->GetGuidStr().c_str(), guid.GetString().c_str());
            return false;
        }
    }
    else if (!_player->isAlive())
    {
        LOG_DEBUG(logging, "WORLD: %s is dead, requested guid was %s",
            _player->GetGuidStr().c_str(), guid.GetString().c_str());
        return false;
    }

    return true;
}
