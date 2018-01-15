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
#include "Item.h"
#include "Language.h"
#include "logging.h"
#include "ObjectAccessor.h"
#include "Opcodes.h"
#include "Player.h"
#include "SocialMgr.h"
#include "Spell.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "inventory/trade.h"

void WorldSession::SendTradeStatus(TradeStatus status)
{
    WorldPacket data;

    switch (status)
    {
    case TRADE_STATUS_BEGIN_TRADE:
        data.initialize(SMSG_TRADE_STATUS, 16);
        data << uint32(status);
        data << uint64(0);
        break;
    case TRADE_STATUS_OPEN_WINDOW:
        data.initialize(SMSG_TRADE_STATUS, 8);
        data << uint32(status);
        data << uint32(0); // added in 2.4.0
        break;
    case TRADE_STATUS_CLOSE_WINDOW:
        data.initialize(SMSG_TRADE_STATUS, 16);
        data << uint32(status);
        data << uint32(0);
        data << uint8(0);
        data << uint32(0);
        break;
    case TRADE_STATUS_ONLY_CONJURED:
        data.initialize(SMSG_TRADE_STATUS, 8);
        data << uint32(status);
        data << uint8(0);
        break;
    default:
        data.initialize(SMSG_TRADE_STATUS, 4);
        data << uint32(status);
        break;
    }

    send_packet(std::move(data));
}

void WorldSession::HandleIgnoreTradeOpcode(WorldPacket& /*recv_data*/)
{
}

void WorldSession::HandleBusyTradeOpcode(WorldPacket& /*recv_data*/)
{
    if (auto trade = _player->trade())
    {
        auto other = (trade->player_one() == _player) ? trade->player_two() :
                                                        trade->player_one();

        _player->trade(nullptr);
        other->trade(nullptr);

        delete trade;

        other->GetSession()->SendTradeStatus(TRADE_STATUS_BUSY);
    }
}

// The client requesting a trade send this opcode
void WorldSession::HandleInitiateTradeOpcode(WorldPacket& recv_data)
{
    ObjectGuid target_guid;
    recv_data >> target_guid;

    if (_player->trade() || _player->GetObjectGuid() == target_guid)
        return;

    if (!_player->isAlive())
        return SendTradeStatus(TRADE_STATUS_YOU_DEAD);

    if (_player->hasUnitState(UNIT_STAT_STUNNED))
        return SendTradeStatus(TRADE_STATUS_YOU_STUNNED);

    if (isLogingOut())
        return SendTradeStatus(TRADE_STATUS_YOU_LOGOUT);

    if (_player->IsTaxiFlying())
        return SendTradeStatus(TRADE_STATUS_TARGET_TOO_FAR);

    Player* target = _player->GetMap()->GetPlayer(target_guid);
    if (!target)
        return SendTradeStatus(TRADE_STATUS_NO_TARGET);

    if (target->trade())
        return SendTradeStatus(TRADE_STATUS_BUSY);

    if (!target->HaveAtClient(_player))
        return SendTradeStatus(TRADE_STATUS_BUSY);

    if (!target->isAlive())
        return SendTradeStatus(TRADE_STATUS_TARGET_DEAD);

    if (target->IsTaxiFlying() || !target->IsWithinDist(_player, 10.0f, true))
        return SendTradeStatus(TRADE_STATUS_TARGET_TOO_FAR);

    if (target->hasUnitState(UNIT_STAT_STUNNED))
        return SendTradeStatus(TRADE_STATUS_TARGET_STUNNED);

    if (target->GetSession()->isLogingOut())
        return SendTradeStatus(TRADE_STATUS_TARGET_LOGOUT);

    if (target->GetSocial()->HasIgnore(_player->GetObjectGuid()))
        return SendTradeStatus(TRADE_STATUS_IGNORE_YOU);

    if (_player->GetTeam() != target->GetTeam())
        return SendTradeStatus(TRADE_STATUS_WRONG_FACTION);

    // Every check has passed, we're free to create a new trade.
    // It assumes memory management of itself. We need not worry
    // beyond the point of creation.
    new inventory::trade(_player, target);

    WorldPacket data(SMSG_TRADE_STATUS, 12);
    data << uint32(TRADE_STATUS_BEGIN_TRADE);
    data << ObjectGuid(_player->GetObjectGuid());
    target->GetSession()->send_packet(std::move(data));
}

// This opcode is sent by the client that was requested a trade upon,
// to signalize he accepts the trade.
void WorldSession::HandleBeginTradeOpcode(WorldPacket& /*recv_data*/)
{
    if (_player->trade())
    {
        // One of these is _player
        _player->trade()->player_one()->GetSession()->SendTradeStatus(
            TRADE_STATUS_OPEN_WINDOW);
        _player->trade()->player_two()->GetSession()->SendTradeStatus(
            TRADE_STATUS_OPEN_WINDOW);
    }
}

void WorldSession::HandleAcceptTradeOpcode(WorldPacket& recv_data)
{
    recv_data.read_skip<uint32>(); // Unknown. Got 7 each time when I tried.

    if (_player->trade())
        _player->trade()->accept(_player);
}

void WorldSession::HandleUnacceptTradeOpcode(WorldPacket& /*recv_data*/)
{
    if (_player->trade())
        _player->trade()->unaccept(_player);
}

void WorldSession::HandleCancelTradeOpcode(WorldPacket& /*recv_data*/)
{
    // FIXME: This opcode can be sent with status
    // (STATUS_LOGGEDIN_OR_RECENTLY_LOGGEDOUT)
    // which means it can be sent when logged out by the client. Is there a
    // reason why we
    // allow this and not just discard the packet as we get it if not logged in?
    if (!_player)
        return;

    if (_player->trade())
        _player->trade()->cancel();
}

void WorldSession::HandleSetTradeGoldOpcode(WorldPacket& recv_data)
{
    uint32 gold;
    recv_data >> gold;

    if (_player->trade())
        _player->trade()->set_gold(_player, gold);
}

void WorldSession::HandleSetTradeItemOpcode(WorldPacket& recv_data)
{
    uint8 trade_slot, bag, index;
    recv_data >> trade_slot >> bag >> index;
    inventory::slot src(inventory::personal_slot, bag, index);

    if (_player->trade())
        _player->trade()->put_item(_player, trade_slot, src);
}

void WorldSession::HandleClearTradeItemOpcode(WorldPacket& recv_data)
{
    uint8 trade_slot;
    recv_data >> trade_slot;

    if (_player->trade())
        _player->trade()->pop_item(_player, trade_slot);
}
