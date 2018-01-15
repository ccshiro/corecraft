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

#include "Common.h"
#include "ObjectGuid.h"
#include <G3D/Vector3.h>
#include <boost/thread.hpp>

class Player;

// NOTE1: Both checked out tickets and the ticket_mgr is handled in a single
// thread,
//        and therefore no contention arises. The ticket_mgr may not be accessed
//        from
//        anywhere but the handlers of opcodes marked with the
//        PROCESS_THREADUNSAFE
//        PacketProcessing method.
// Note2: This class relies on HandleMessagechatOpcode to be
// PROCESS_THREADUNSAFE,
//        to avoid the need for locking. If you change this fact, you need to
//        revamp
//        the ticket_mgr AND ticket (which too are assumed to be handled single
//        threaded)
//        to introduce locking both for the system's data (ticket_mgr) and the
//        individual
//        modifying of each checked out ticket.
// Note3: The max length of a ticket the client sends is 500 bytes, the max
// length
//        of a chat message is 255 bytes. Since the client sends it in utf8, the
//        amount
//        of characters vary. The client is able to receive chat messages longer
//        than 255 bytes, however. In fact, it can receive up to

class ticket
{
public:
    ticket(uint32 acc, ObjectGuid guid, std::string name, time_t create,
        std::string t, uint32 map, G3D::Vector3 point, std::string note)
      : account_id(acc), creator_guid(guid), online_on(guid),
        creator_name(std::move(name)), created(create), last_updated(created),
        text(std::move(t)), create_map(map), create_point(point),
        player_note(std::move(note))
    {
        /* empty */
    }

    uint32 account_id;
    ObjectGuid creator_guid; // Character used to create ticket
    ObjectGuid
        online_on; // The character the player is online on (empty if offline)
    std::string creator_name;

    time_t created;
    time_t last_updated;

    std::string text;

    uint32 create_map;
    G3D::Vector3 create_point;

    std::string player_note; // GM-set note about the player. This persists
                             // between tickets.

    // Data regarding the expedition of the ticket
    AccountTypes gm_level =
        SEC_TICKET_GM;   // Level of GM needed to expedite ticket
    ObjectGuid gm;       // GM currently servicing this ticket
    std::string comment; // Comment from previous GMs looking at the ticket
    std::string backlog; // Backlog of conversation

    bool waiting_pong = false; // True if waiting for GM's pong
    time_t pingpong_timer = 0; // Timestamp when ping/pong timer was started
};

class ticket_mgr
{
    ticket_mgr() {}

public:
    ticket_mgr(const ticket_mgr&) = delete;

    inline static ticket_mgr& instance();

    // req: only call on server startup; cannot be used to reload
    void load();

    bool create(Player* player, std::string text); // Returns false if that
                                                   // player already has a
                                                   // registered ticket
    void edit(Player* player, std::string text);
    void destroy(Player* player);

    std::shared_ptr<ticket> checkout(
        Player* gm, // Checks out first ticket eligible for GM
        bool try_lower_sec = false);
    void checkin(std::shared_ptr<ticket> ticket);
    void resolve(
        std::shared_ptr<ticket> ticket, Player* gm, std::string mail = "");

    void update();

    // req: gm must have a checked out ticket with player
    void player_whisper(std::string text, Player* player, Player* gm);

    ticket* get_ticket(Player* player);

    void send_nullticket(Player* player);
    void send_ticket(Player* player, const ticket& t);

private:
    std::vector<ticket> tickets_; // Tickets that are not checked out
    std::vector<std::shared_ptr<ticket>>
        checked_out_; // Tickets currently being expedited by some GM
    std::vector<uint32> destroyed_tickets_; // Ids of tickets destroyed while
                                            // checked out (if checked in they
                                            // will be discared, if resolved
                                            // they'll be saved)

    std::map<uint32 /*acc*/, std::string> player_notes_; // GMs can make a note
                                                         // per player account
                                                         // that is persistent
                                                         // across tickets and
                                                         // server restarts

    bool has_ticket(Player* player);
    void send_text(const ticket& t, Player* gm);
    void save_player_note(const ticket& t);
};

ticket_mgr& ticket_mgr::instance()
{
    static ticket_mgr inst;
    return inst;
}
