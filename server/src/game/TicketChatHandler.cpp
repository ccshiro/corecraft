/*
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

// XXX: Make sure all extracted strings ONLY contain proper english. Do the same
// for client-sent tickets.

#include "Chat.h"
#include "Common.h"
#include "GMTicketMgr.h"
#include "logging.h"
#include "ObjectAccessor.h"
#include "Player.h"

bool ChatHandler::HandleTicketCheckin(char* args)
{
    Player* gm = m_session->GetPlayer();

    if (!gm->checked_out_ticket)
    {
        SendSysMessage("TICKET ERROR You do not have a checked out ticket.");
        return true;
    }

    char* opt = ExtractLiteralArg(&args);
    if (opt && strcmp(opt, "bump") == 0)
    {
        if (gm->checked_out_ticket->gm_level < SEC_FULL_GM)
            gm->checked_out_ticket->gm_level = static_cast<AccountTypes>(
                static_cast<int>(gm->checked_out_ticket->gm_level) + 1);
    }

    ticket_mgr::instance().checkin(gm->checked_out_ticket);
    gm->checked_out_ticket.reset();

    return true;
}

bool ChatHandler::HandleTicketCheckout(char* args)
{
    Player* gm = m_session->GetPlayer();

    if (gm->checked_out_ticket)
    {
        SendSysMessage(
            "TICKET ERROR You have already checked out another ticket.");
        return true;
    }

    bool all_sec = false;

    char* str = ExtractLiteralArg(&args);
    if (str && strcmp(str, "all") == 0)
        all_sec = true;

    gm->checked_out_ticket = ticket_mgr::instance().checkout(gm, all_sec);
    return true;
}

bool ChatHandler::HandleTicketComment(char* args)
{
    Player* gm = m_session->GetPlayer();

    if (!gm->checked_out_ticket)
    {
        SendSysMessage("TICKET ERROR You do not have a checked out ticket.");
        return true;
    }

    char* note = ExtractQuotedArg(&args);
    if (!note || strlen(note) >= 240)
    {
        SendSysMessage(
            "TICKET ERROR Your player note cannot exceed 240 characters.");
        return true;
    }

    gm->checked_out_ticket->comment.assign(note);

    return true;
}

bool ChatHandler::HandleTicketPnote(char* args)
{
    Player* gm = m_session->GetPlayer();

    if (!gm->checked_out_ticket)
    {
        SendSysMessage("TICKET ERROR You do not have a checked out ticket.");
        return true;
    }

    char* comment = ExtractQuotedArg(&args);
    if (!comment || strlen(comment) >= 240)
    {
        SendSysMessage(
            "TICKET ERROR Your ticket comment cannot exceed 240 characters.");
        return true;
    }

    gm->checked_out_ticket->player_note.assign(comment);
    return true;
}

bool ChatHandler::HandleTicketPong(char* /*args*/)
{
    Player* gm = m_session->GetPlayer();

    if (!gm->checked_out_ticket)
    {
        SendSysMessage("TICKET ERROR You do not have a checked out ticket.");
        return true;
    }

    gm->checked_out_ticket->waiting_pong = false;
    gm->checked_out_ticket->pingpong_timer = WorldTimer::time_no_syscall();

    return true;
}

bool ChatHandler::HandleTicketResolve(char* args)
{
    Player* gm = m_session->GetPlayer();

    if (!gm->checked_out_ticket)
    {
        SendSysMessage("TICKET ERROR You do not have a checked out ticket.");
        return true;
    }

    char* mail = ExtractQuotedArg(&args);

    ticket_mgr::instance().resolve(
        gm->checked_out_ticket, gm, mail != nullptr ? mail : "");
    gm->checked_out_ticket.reset();

    return true;
}

bool ChatHandler::HandleTicketSay(char* args)
{
    Player* gm = m_session->GetPlayer();

    if (!gm->checked_out_ticket)
    {
        SendSysMessage("TICKET ERROR You do not have a checked out ticket.");
        return true;
    }

    // the client sends tickets in the form of: .ticket say ""
    // the server sends them in the form of: TICKET YOU %s or TICKET HER %s
    // meaning we need not check length
    char* msg = ExtractQuotedArg(&args);
    if (!msg)
    {
        SendSysMessage(
            "TICKET ERROR You must specify a string enclosed in quotes to "
            "write.");
        return true;
    }

    Player* target = sObjectAccessor::Instance()->FindPlayer(
        gm->checked_out_ticket->online_on, true);
    if (!target)
    {
        SendSysMessage(
            "TICKET ERROR The player that registered this ticket is not "
            "online.");
        return true;
    }

    PSendSysMessage("TICKET YOU %s", msg);

    gm->Whisper(msg, LANG_UNIVERSAL, target);

    std::stringstream ss;
    ss << "<" << gm->GetName() << "> " << msg << "\n";
    gm->checked_out_ticket->backlog.append(ss.str());

    return true;
}
