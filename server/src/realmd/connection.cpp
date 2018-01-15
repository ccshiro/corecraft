/*
 * Copyright (C) 2013 CoreCraft <https://www.worldofcorecraft.com/>
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

#include "connection.h"
#include "AuthCodes.h"
#include "logging.h"
#include "Util.h"
#include <boost/bind.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <algorithm>
#include <cstring>

connection::connection(boost::asio::io_service& io)
  : socket_{io}, account_id_{0}, state_{STATE_NEW},
    account_security_level_{SEC_PLAYER}, build_{0}, security_flags_{0},
    client_token_size_{0}
{
    init_srp6();
}

void connection::start()
{
    boost::asio::async_read(socket_,
        boost::asio::buffer(reinterpret_cast<char*>(&cmd_buffer_),
                                sizeof(cmd_buffer_)),
        boost::bind(&connection::handle_read_cmd, shared_from_this(),
                                boost::asio::placeholders::error));
}

void connection::stop()
{
    if (is_closed())
    {
        LOG_DEBUG(logging, "Trying to close an already closed connection");
        return;
    }
    LOG_DEBUG(logging, "Stop connection for %s", remote_address().c_str());
    boost::system::error_code ignored_ec;
    socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored_ec);
    socket_.close();
}

std::string connection::remote_address() const
{
    if (socket_.is_open())
    {
        boost::system::error_code ec;
        auto endpoint = socket_.remote_endpoint(ec);
        if (ec)
        {
            logging.error(
                "connection::remote_address() error: %s", ec.message().c_str());
            return "<unable to get address>";
        }
        return endpoint.address().to_string();
    }
    else
    {
        return "<socket not connected>";
    }
}

void connection::handle_read_cmd(const boost::system::error_code& e)
{
    if (e)
    {
        stop();
        return;
    }

    // Read the next part of the packet if we have a valid state for this
    // command
    // otherwise we kill the connection
    switch (cmd_buffer_)
    {
    case CMD_AUTH_LOGON_CHALLENGE:
    case CMD_AUTH_RECONNECT_CHALLENGE:
        if (state_ != STATE_NEW)
        {
            logging.error("%s sent invalid %02x command during state %d",
                remote_address().c_str(), cmd_buffer_, state_);
            stop();
            return;
        }
        // Read the logon packet
        boost::asio::async_read(
            socket_,
            boost::asio::buffer(reinterpret_cast<char*>(&logon_challenge_),
                sizeof(logon_challenge_)),
            boost::bind(&connection::handle_read_logon_challenge,
                shared_from_this(), boost::asio::placeholders::error));
        break;
    case CMD_AUTH_LOGON_PROOF:
        if (state_ != STATE_WAITING_FOR_LOGON_PROOF)
        {
            logging.error("%s sent invalid %02x command during state %d",
                remote_address().c_str(), cmd_buffer_, state_);
            stop();
            return;
        }
        // Read the logon proof
        boost::asio::async_read(
            socket_, boost::asio::buffer(reinterpret_cast<char*>(&logon_proof_),
                         sizeof(logon_proof_)),
            boost::bind(&connection::handle_read_logon_proof,
                shared_from_this(), boost::asio::placeholders::error));
        break;
    case CMD_AUTH_RECONNECT_PROOF:
        if (state_ != STATE_WAITING_FOR_RECONNECT_PROOF)
        {
            logging.error("%s sent invalid %02x command during state %d",
                remote_address().c_str(), cmd_buffer_, state_);
            stop();
            return;
        }
        // Read the reconnect proof
        boost::asio::async_read(
            socket_,
            boost::asio::buffer(reinterpret_cast<char*>(&reconnect_proof_),
                sizeof(reconnect_proof_)),
            boost::bind(&connection::handle_read_reconnect_proof,
                shared_from_this(), boost::asio::placeholders::error));
        break;
    case CMD_REALM_LIST:
        if (state_ != STATE_AUTHENTICATED)
        {
            logging.error("%s sent invalid %02x command during state %d",
                remote_address().c_str(), cmd_buffer_, state_);
            stop();
            return;
        }
        boost::asio::async_read(socket_,
            boost::asio::buffer(&realmlist_data_, sizeof(realmlist_data_)),
            boost::bind(&connection::handle_read_realmlist, shared_from_this(),
                                    boost::asio::placeholders::error));
        break;
    default:
        // all other opcodes are not supported
        stop();
        return;
    }
}

void connection::handle_read_realmlist(const boost::system::error_code& e)
{
    if (e)
    {
        stop();
        return;
    }
    handle_realmlist();
}

void connection::handle_read_logon_challenge(const boost::system::error_code& e)
{
    if (e)
    {
        stop();
        return;
    }

    // TODO: what about logon_challenge_.size ??

    if (logon_challenge_.name_len > MAX_USERNAME_LEN ||
        logon_challenge_.name_len < MIN_USERNAME_LEN)
    {
        // TODO: maybe write a proper error to the client? can't really happen
        // with a legit client  I think
        stop();
        return;
    }

    // Read the username
    username_[logon_challenge_.name_len] = '\0';
    boost::asio::async_read(socket_,
        boost::asio::buffer(username_, logon_challenge_.name_len),
        boost::bind(&connection::handle_read_username, shared_from_this(),
                                boost::asio::placeholders::error));
}

void connection::handle_read_username(const boost::system::error_code& e)
{
    if (e)
    {
        stop();
        return;
    }

    // If some faggot sent us 0 bytes we kill the connection
    if (std::strlen(username_) != logon_challenge_.name_len)
    {
        stop();
        return;
    }

    switch (cmd_buffer_)
    {
    case CMD_AUTH_LOGON_CHALLENGE:
        handle_logon_challenge();
        break;
    case CMD_AUTH_RECONNECT_CHALLENGE:
        handle_reconnect_challenge();
        break;
    default:
        assert(false);
    }
}

void connection::handle_write(const boost::system::error_code& e)
{
    if (e)
    {
        stop();
        return;
    }

    boost::asio::async_read(socket_,
        boost::asio::buffer(reinterpret_cast<char*>(&cmd_buffer_),
                                sizeof(cmd_buffer_)),
        boost::bind(&connection::handle_read_cmd, shared_from_this(),
                                boost::asio::placeholders::error));
}

void connection::handle_read_logon_proof(const boost::system::error_code& e)
{
    if (e)
    {
        stop();
        return;
    }

    if (security_flags_ & SECURITY_TOTP)
    {
        boost::asio::async_read(socket_,
            boost::asio::buffer(&client_token_size_,
                                    sizeof(client_token_size_)),
            boost::bind(&connection::handle_read_token_size, shared_from_this(),
                                    boost::asio::placeholders::error));
    }
    else
    {
        handle_logon_proof();
    }
}

void connection::handle_read_token_size(const boost::system::error_code& e)
{
    // an honest client will never send a token of size 0, so we
    // can kill the connection if it does
    if (e || client_token_size_ == 0)
    {
        stop();
        return;
    }

    client_token_[client_token_size_] = '\0';
    boost::asio::async_read(socket_,
        boost::asio::buffer(&client_token_, client_token_size_),
        boost::bind(&connection::handle_read_token, shared_from_this(),
                                boost::asio::placeholders::error));
}

void connection::handle_read_token(const boost::system::error_code& e)
{
    if (e || client_token_size_ == 0)
    {
        stop();
        return;
    }
    handle_logon_proof();
}

void connection::handle_read_reconnect_proof(const boost::system::error_code& e)
{
    if (e)
    {
        stop();
        return;
    }

    handle_reconnect_proof();
}

void connection::send()
{
    boost::asio::async_write(
        socket_, boost::asio::buffer(
                     reinterpret_cast<const char*>(output_buffer_.contents()),
                     output_buffer_.size()),
        boost::bind(&connection::handle_write, shared_from_this(),
            boost::asio::placeholders::error));
}
