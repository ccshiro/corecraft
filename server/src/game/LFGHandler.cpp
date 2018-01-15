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

#include "logging.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "lfg_tool_container.h"

void WorldSession::HandleLfgSetAutoJoinOpcode(WorldPacket& /*recv_data*/)
{
    lfg_auto_join = true;

    // NOTE: STATUS_AUTHED, so _player can be nullptr
    if (_player)
        sLfgToolContainer::Instance()->attempt_join(_player);
}

void WorldSession::HandleLfgClearAutoJoinOpcode(WorldPacket& /*recv_data*/)
{
    lfg_auto_join = false;
}

void WorldSession::HandleLfmSetAutoFillOpcode(WorldPacket& /*recv_data*/)
{
    lfg_auto_invite = true;

    // NOTE: STATUS_AUTHED, so _player can be nullptr
    if (_player)
        sLfgToolContainer::Instance()->attempt_invite(_player);
}

void WorldSession::HandleLfmClearAutoFillOpcode(WorldPacket& /*recv_data*/)
{
    lfg_auto_invite = false;
}

void WorldSession::HandleLfgClearOpcode(WorldPacket& /*recv_data*/)
{
    for (int i = 0; i < MAX_LOOKING_FOR_GROUP_SLOT; ++i)
        _player->m_lookingForGroup.slots[i].Clear();

    if (_player->m_lookingForGroup.more.Empty())
        sLfgToolContainer::Instance()->remove(this);
}

void WorldSession::HandleLfmClearOpcode(WorldPacket& /*recv_data */)
{
    _player->m_lookingForGroup.more.Clear();
    if (_player->m_lookingForGroup.Empty())
        sLfgToolContainer::Instance()->remove(this);
}

void WorldSession::HandleSetLfmOpcode(WorldPacket& recv_data)
{
    uint32 tmp;
    recv_data >> tmp;

    uint32 entry = (tmp & 0xFFFF);
    uint32 type = ((tmp >> 24) & 0xFFFF);

    _player->m_lookingForGroup.more.Set(entry, type);
    LOG_DEBUG(
        logging, "LFM set: tmp: %u, zone: %u, type: %u", tmp, entry, type);

    sLfgToolContainer::Instance()->insert(this);

    if (lfg_auto_invite)
        sLfgToolContainer::Instance()->attempt_invite(_player);
}

void WorldSession::HandleSetLfgOpcode(WorldPacket& recv_data)
{
    uint32 slot, tmp;
    recv_data >> slot >> tmp;

    uint32 entry = (tmp & 0xFFFF);
    uint32 type = ((tmp >> 24) & 0xFFFF);

    if (slot >= MAX_LOOKING_FOR_GROUP_SLOT)
        return;

    _player->m_lookingForGroup.slots[slot].Set(entry, type);
    LOG_DEBUG(logging, "LFG set: looknumber: %u, tmp: %u, type: %u, entry: %u",
        slot, tmp, type, entry);

    sLfgToolContainer::Instance()->insert(this);

    if (lfg_auto_join)
        sLfgToolContainer::Instance()->attempt_join(_player);
}

void WorldSession::HandleSetLfgCommentOpcode(WorldPacket& recv_data)
{
    std::string comment;
    recv_data >> comment;
    LOG_DEBUG(logging, "LFG comment %s", comment.c_str());

    _player->m_lookingForGroup.comment = comment;
}

void WorldSession::HandleLookingForGroup(WorldPacket& recv_data)
{
    LOG_DEBUG(logging, "MSG_LOOKING_FOR_GROUP");

    uint32 type, entry, unk;
    recv_data >> type >> entry >> unk;
    LOG_DEBUG(logging, "MSG_LOOKING_FOR_GROUP: type: %u, entry: %u, unk: %u",
        type, entry, unk);

    sLfgToolContainer::Instance()->send_tool_state(_player, entry, type);
}
