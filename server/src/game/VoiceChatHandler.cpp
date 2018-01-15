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
#include "logging.h"
#include "Opcodes.h"
#include "WorldPacket.h"
#include "WorldSession.h"

void WorldSession::HandleVoiceSessionEnableOpcode(WorldPacket& recv_data)
{
    // uint8 isVoiceEnabled, uint8 isMicrophoneEnabled
    recv_data.read_skip<uint8>();
    recv_data.read_skip<uint8>();
}

void WorldSession::HandleChannelVoiceOnOpcode(WorldPacket& recv_data)
{
    // Enable Voice button in channel context menu
    recv_data.hexlike();
}

void WorldSession::HandleSetActiveVoiceChannel(WorldPacket& recv_data)
{
    recv_data.read_skip<uint32>();
    recv_data.read_skip<char*>();
}
