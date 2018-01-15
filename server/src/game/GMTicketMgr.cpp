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

#include "GMTicketMgr.h"
#include "Chat.h"
#include "logging.h"
#include "Mail.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ProgressBar.h"
#include "Timer.h"
#include "Util.h"
#include "WorldSession.h"
#include "Database/DatabaseEnv.h"

void ticket_mgr::load()
{
    std::unique_ptr<QueryResult> result(
        CharacterDatabase.Query("SELECT account_id, note FROM player_note"));

    uint32 count = 0;
    if (result)
    {
        BarGoLink bar(result->GetRowCount());
        do
        {
            bar.step();
            Field* f = result->Fetch();
            player_notes_[f[0].GetUInt32()] = f[1].GetCppString();
            ++count;
        } while (result->NextRow());
    }

    logging.info("Loaded %u tickets\n", count);
}

bool ticket_mgr::create(Player* player, std::string text)
{
    if (has_ticket(player))
        return false;

    uint32 acc_id = player->GetSession()->GetAccountId();

    tickets_.emplace_back(acc_id, player->GetObjectGuid(), player->GetName(),
        WorldTimer::time_no_syscall(), text, player->GetMapId(),
        G3D::Vector3(player->GetX(), player->GetY(), player->GetZ()),
        player_notes_[acc_id]);

    return true;
}

void ticket_mgr::edit(Player* player, std::string text)
{
    // Pending tickets
    auto itr =
        std::find_if(tickets_.begin(), tickets_.end(), [player](const ticket& t)
            {
                return t.account_id == player->GetSession()->GetAccountId();
            });

    if (itr != tickets_.end())
    {
        itr->text = text;
    }
    // Checked out tickets
    else
    {
        auto itr = std::find_if(checked_out_.begin(), checked_out_.end(),
            [player](const std::shared_ptr<ticket>& ptr)
            {
                return ptr->account_id == player->GetSession()->GetAccountId();
            });
        if (itr != checked_out_.end())
        {
            (*itr)->text = text;

            if (Player* gm =
                    sObjectAccessor::Instance()->FindPlayer((*itr)->gm, false))
            {
                ChatHandler(gm).SendSysMessage("TICKET TEXTCLEAR");
                send_text(*(*itr), gm);
            }
        }
    }
}

void ticket_mgr::destroy(Player* player)
{
    // Pending tickets
    auto itr =
        std::find_if(tickets_.begin(), tickets_.end(), [player](const ticket& t)
            {
                return t.account_id == player->GetSession()->GetAccountId();
            });

    if (itr != tickets_.end())
    {
        tickets_.erase(itr);
    }
    // Checked out tickets
    else
    {
        auto itr = std::find_if(checked_out_.begin(), checked_out_.end(),
            [player](const std::shared_ptr<ticket>& ptr)
            {
                return ptr->account_id == player->GetSession()->GetAccountId();
            });
        if (itr != checked_out_.end())
        {
            if (Player* gm =
                    sObjectAccessor::Instance()->FindPlayer((*itr)->gm, false))
            {
                gm->checked_out_ticket.reset();
                ChatHandler(gm).SendSysMessage("TICKET DELETE");
            }
            checked_out_.erase(itr);
        }
    }
}

std::shared_ptr<ticket> ticket_mgr::checkout(Player* gm, bool try_lower_sec)
{
    auto handler = ChatHandler(gm);

    auto itr =
        std::find_if(tickets_.begin(), tickets_.end(), [gm](const ticket& t)
            {
                return t.gm_level == gm->GetSession()->GetSecurity();
            });

    if (try_lower_sec && itr == tickets_.end())
        itr =
            std::find_if(tickets_.begin(), tickets_.end(), [gm](const ticket& t)
                {
                    return gm->GetSession()->GetSecurity() >= t.gm_level;
                });

    if (itr == tickets_.end())
    {
        handler.PSendSysMessage("TICKET EMPTY %u",
            try_lower_sec ? 1 : gm->GetSession()->GetSecurity());
        return nullptr;
    }

    auto ptr = std::make_shared<ticket>(*itr);
    tickets_.erase(itr);
    checked_out_.push_back(ptr);

    Player* online_on = ptr->online_on.IsEmpty() ?
                            nullptr :
                            ObjectAccessor::FindPlayer(ptr->online_on, false);

    // Send the data about the ticket
    handler.SendSysMessage("TICKET NEW");
    handler.PSendSysMessage("TICKET NAME %s %s", ptr->creator_name.c_str(),
        online_on != nullptr ? online_on->GetName() : "NIL");
    handler.PSendSysMessage("TICKET PNOTE %s", ptr->player_note.c_str());
    handler.PSendSysMessage("TICKET COMMENT %s", ptr->comment.c_str());
    handler.PSendSysMessage("TICKET LOC %u %f %f %f", ptr->create_map,
        ptr->create_point.x, ptr->create_point.y, ptr->create_point.z);

    send_text(*ptr, gm);

    // Start ping timer
    ptr->pingpong_timer = WorldTimer::time_no_syscall();

    ptr->gm = gm->GetObjectGuid();

    return ptr;
}

void ticket_mgr::checkin(std::shared_ptr<ticket> ticket)
{
    auto itr = std::find(checked_out_.begin(), checked_out_.end(), ticket);
    if (itr == checked_out_.end())
    {
        logging.error(
            "ticket_mgr::checkin: Tried to checkin a ticket that was not "
            "checked out.");
        return;
    }

    // Clear data that is not relevant for the next checkout session
    ticket->gm.Clear();
    ticket->waiting_pong = false;

    save_player_note(*ticket);

    checked_out_.erase(itr);
    tickets_.insert(tickets_.begin(), *ticket);
}

void ticket_mgr::resolve(
    std::shared_ptr<ticket> ticket, Player* gm, std::string mail)
{
    auto itr = std::find(checked_out_.begin(), checked_out_.end(), ticket);
    if (itr == checked_out_.end())
    {
        logging.error(
            "ticket_mgr::resolve: Tried to resolve a ticket that was not "
            "checked out.");
        return;
    }

    checked_out_.erase(itr);

    save_player_note(*ticket);

    std::string total_mail;

    if (!mail.empty())
    {
        std::stringstream ss;
        ss << "Dear " << ticket->creator_name << ",\n\n"
           << "I am writing in response to your recent GM ticket. After "
              "inspection of the matter I came to the following conclusion:\n\n"
           << mail << "\n\n"
           << "Kind Regards,\n"
           << "Game Master " << gm->GetName();

        total_mail = ss.str();

        Player* creator = ObjectAccessor::FindPlayer(ticket->creator_guid);

        MailDraft draft("Regarding your ticket.", total_mail);
        draft.SendMailTo(MailReceiver(creator, ticket->creator_guid),
            MailSender(gm, MAIL_STATIONERY_GM));
    }

    static SqlStatementID ticket_stmt_id;
    auto note_stmt = CharacterDatabase.CreateStatement(ticket_stmt_id,
        "INSERT INTO ticket_history (resolve_time, ticket_text, player_acc, "
        "resolve_acc, backlog, resolve_mail) VALUES(?, ?, ?, ?, ?, ?)");

    note_stmt.PExecute(WorldTimer::time_no_syscall(), ticket->text.c_str(),
        ticket->account_id, gm->GetSession()->GetAccountId(),
        ticket->backlog.c_str(), total_mail.c_str());

    // XXX: Keep a count of GMs that resolve a lot of tickets?

    // Alert player (if he's online) that the ticket is gone
    if (auto player = ObjectAccessor::FindPlayer(ticket->online_on))
        send_nullticket(player);
}

void ticket_mgr::update()
{
    for (auto itr = checked_out_.begin(); itr != checked_out_.end();)
    {
        auto ptr = *itr;
        Player* gm = ObjectAccessor::FindPlayer(ptr->gm, false);

        if (!gm)
        {
            checkin(ptr);
            itr = checked_out_.begin();
            continue;
        }

        if (ptr->waiting_pong)
        {
            if (ptr->pingpong_timer + 30 <= WorldTimer::time_no_syscall())
            {
                ChatHandler(gm).SendSysMessage(
                    "TICKET ERROR You did not respond to the TICKET PING "
                    "message. The ticket has been forcefully checked in by the "
                    "server.");
                ChatHandler(gm).SendSysMessage("TICKET DELETE");
                if (Player* gm = ObjectAccessor::FindPlayer(ptr->gm, false))
                    gm->checked_out_ticket.reset(), checkin(ptr);
                itr = checked_out_.begin();
                continue;
            }
        }
        else
        {
            if (ptr->pingpong_timer + 120 <= WorldTimer::time_no_syscall())
            {
                ChatHandler(gm).SendSysMessage("TICKET PING");
                ptr->waiting_pong = true;
                ptr->pingpong_timer = WorldTimer::time_no_syscall();
            }
        }

        ++itr;
    }
}

void ticket_mgr::player_whisper(std::string text, Player* player, Player* gm)
{
    std::stringstream ss;
    ss << "<" << player->GetName() << "> " << text << "\n";
    gm->checked_out_ticket->backlog.append(ss.str());

    auto handler = ChatHandler(gm);

    // The client does not have a receive limit
    handler.PSendSysMessage("TICKET HER %s", text.c_str());
}

ticket* ticket_mgr::get_ticket(Player* player)
{
    for (auto& ticket : tickets_)
        if (ticket.account_id == player->GetSession()->GetAccountId())
            return &ticket;

    for (auto& ptr : checked_out_)
        if (ptr->account_id == player->GetSession()->GetAccountId())
            return ptr.get();

    return nullptr;
}

void ticket_mgr::send_nullticket(Player* player)
{
    WorldPacket data(SMSG_GMTICKET_GETTICKET, 4);
    data << uint32(0x0A); // no actual ticket
    player->SendDirectMessage(std::move(data));
}

void ticket_mgr::send_ticket(Player* player, const ticket& t)
{
    WorldPacket data(SMSG_GMTICKET_GETTICKET, 4);

    data << uint32(0x06); // has ticket data
    data << t.text;
    data << uint8(0x07); // ticket category
    data << float(0);    // unk1. tickets in queue, maybe?
    data << float(0); // unk2. if unk2 > unk1 => "We are currently experiencing
                      // a high volume of petitions."
    data << float(0); // unk3, 0 - "Your ticket will be serviced soon", 1 -
                      // "Wait time currently unavailable"
    data << uint8(0); // unk4
    data << uint8(0); // unk5. if unk4 == 2 and unk5 == 1 => "Your ticket has
                      // been escalated."

    player->SendDirectMessage(std::move(data));
}

bool ticket_mgr::has_ticket(Player* player)
{
    for (auto& ticket : tickets_)
        if (ticket.account_id == player->GetSession()->GetAccountId())
            return true;

    for (auto& ptr : checked_out_)
        if (ptr->account_id == player->GetSession()->GetAccountId())
            return true;

    return false;
}

void ticket_mgr::send_text(const ticket& t, Player* gm)
{
    if (t.text.empty())
        return;

    auto handler = ChatHandler(gm);

    std::string::size_type i = 0;
    while (i != std::string::npos && i < t.text.size())
    {
        std::string::size_type next_newline = utf8findascii(t.text, i, '\n');
        std::string::size_type off = next_newline == std::string::npos ?
                                         t.text.size() - i :
                                         next_newline - i;
        std::string text_block = t.text.substr(i, off);

        handler.PSendSysMessage("TICKET TEXT %s", text_block.c_str());

        if (next_newline != std::string::npos)
            i = next_newline + 1;
        else
            i = std::string::npos;
    }
}

void ticket_mgr::save_player_note(const ticket& t)
{
    // Save player note if it has changed
    if (player_notes_[t.account_id].compare(t.player_note) != 0)
    {
        player_notes_[t.account_id] = t.player_note;

        static SqlStatementID note_del_stmt_id;
        static SqlStatementID note_ins_stmt_id;

        CharacterDatabase.BeginTransaction();

        auto del_stmt = CharacterDatabase.CreateStatement(
            note_del_stmt_id, "DELETE FROM player_note WHERE account_id=?");
        del_stmt.PExecute(t.account_id);

        if (!t.player_note.empty())
        {
            auto ins_stmt = CharacterDatabase.CreateStatement(note_ins_stmt_id,
                "INSERT INTO player_note (account_id, note) VALUES(?, ?)");
            ins_stmt.PExecute(t.account_id, t.player_note.c_str());
        }

        CharacterDatabase.CommitTransaction();
    }
}
