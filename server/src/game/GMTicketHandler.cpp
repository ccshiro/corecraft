/*
 * Copyright (C) 2005-2012 MaNGOS <http://getmangos.com/>
 * Copyright (C) 2014 Corecraft <https://www.worldofcorecraft.com>
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
#include "GMTicketMgr.h"
#include "Language.h"
#include "logging.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "WorldPacket.h"

void WorldSession::HandleGMTicketGetTicketOpcode(WorldPacket& /*recv_data*/)
{
    // SendQueryTimeResponse();

    if (auto t = ticket_mgr::instance().get_ticket(_player))
    {
        t->online_on = _player->GetObjectGuid();
        ticket_mgr::instance().send_ticket(_player, *t);
    }
    else
    {
        ticket_mgr::instance().send_nullticket(_player);
    }
}

void WorldSession::HandleGMTicketUpdateTextOpcode(WorldPacket& recv_data)
{
    std::string text;
    recv_data >> text;

    ticket_mgr::instance().edit(_player, text);
}

void WorldSession::HandleGMTicketDeleteTicketOpcode(WorldPacket& /*recv_data*/)
{
    ticket_mgr::instance().destroy(_player);

    WorldPacket data(SMSG_GMTICKET_DELETETICKET, 4);
    data << uint32(9); // XXX
    send_packet(std::move(data));

    // SendGMTicketGetTicket(0x0A);
}

void WorldSession::HandleGMTicketCreateOpcode(WorldPacket& recv_data)
{
    // last checked 2.4.3
    std::string text = "";

    recv_data.read_skip<uint32>(); // map
    recv_data.read_skip<float>();  // x
    recv_data.read_skip<float>();  // y
    recv_data.read_skip<float>();  // z

    recv_data >> text;

    for (size_t i = 0; i < text.size(); ++i)
        LOG_DEBUG(logging, "char %u: %c", (int)i, text[i]);

    recv_data.read_skip<uint32>(); // unk1, 0
    recv_data.read_skip<uint32>(); // unk2, 1
    recv_data.read_skip<uint32>(); // unk3, 0

    if (GetSecurity() != SEC_PLAYER)
    {
        WorldPacket data(SMSG_GMTICKET_CREATE, 4);
        data << uint32(3); // 2 - "Error creating GM ticket"
        send_packet(std::move(data));
        ChatHandler(_player).SendSysMessage("Only players can create tickets.");
        return;
    }

    if (!ticket_mgr::instance().create(_player, text))
    {
        WorldPacket data(SMSG_GMTICKET_CREATE, 4);
        data << uint32(1); // 1 - You already have GM ticket
        send_packet(std::move(data));
        return;
    }

    // SendQueryTimeResponse();

    WorldPacket data(SMSG_GMTICKET_CREATE, 4);
    data << uint32(
        2); // 2 - nothing appears (3-error creating, 5-error updating)
    send_packet(std::move(data));
}

void WorldSession::HandleGMTicketSystemStatusOpcode(WorldPacket& /*recv_data*/)
{
    WorldPacket data(SMSG_GMTICKET_SYSTEMSTATUS, 4);
    data << uint32(sWorld::Instance()->getConfig(
        CONFIG_BOOL_TICKET_SYSTEM_ENABLED)); // 1 - ticket system enabled, 0 -
                                             // ticket system disabled

    send_packet(std::move(data));
}

void WorldSession::HandleGMSurveySubmitOpcode(WorldPacket& recv_data)
{
    // GM survey is shown after SMSG_GM_TICKET_STATUS_UPDATE with status = 3
    uint32 x;
    recv_data >> x; // answer range? (6 = 0-5?)
    LOG_DEBUG(logging, "SURVEY: X = %u", x);

    uint8 result[10];
    memset(result, 0, sizeof(result));
    for (auto& elem : result)
    {
        uint32 questionID;
        recv_data >> questionID; // GMSurveyQuestions.dbc
        if (!questionID)
            break;

        uint8 value;
        std::string unk_text;
        recv_data >> value;    // answer
        recv_data >> unk_text; // always empty?

        elem = value;
        LOG_DEBUG(logging, "SURVEY: ID %u, value %u, text %s", questionID,
            value, unk_text.c_str());
    }

    std::string comment;
    recv_data >> comment; // addional comment
    LOG_DEBUG(logging, "SURVEY: comment %s", comment.c_str());

    // TODO: chart this data in some way
}
