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

#ifndef REALMD__CONNECTION_H
#define REALMD__CONNECTION_H

#include "ByteBuffer.h"
#include "Common.h"
#include "Auth/BigNumber.h"
#include <boost/asio.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/noncopyable.hpp>
#include <string>

class QueryResult;
class Sha1Hash;

#define MAX_USERNAME_LEN 16
#define MIN_USERNAME_LEN 1

enum connection_state
{
    STATE_NEW,
    STATE_WAITING_FOR_LOGON_PROOF,
    STATE_WAITING_FOR_RECONNECT_PROOF,
    STATE_AUTHENTICATED
};

#if defined(__GNUC__)
#pragma pack(1)
#else
#pragma pack(push, 1)
#endif

struct version_data
{
    uint8 major;
    uint8 minor;
    uint8 bugfix;
    uint16 build;
};

// client -> server, initial login request
struct logon_challenge
{
    // uint8        cmd;     // the receive loop will have consumed this byte
    // already, so it's commented out
    uint8 error;
    uint16 size;
    uint32 game;
    version_data version;
    uint32 platform;
    uint32 os;
    uint32 country;
    uint32 timezone;
    uint32 ip;
    uint8 name_len;
};

// server -> client, send preparations for the proof
struct logon_challenge_reply
{
    uint8 cmd;
    uint8 unk;
    uint8 error;
    uint8 B[32];
    uint8 G_len;
    uint8 G;
    uint8 N_len;
    uint8 N[32];
    uint8 s[32];
};

// client -> server, prove knowledge of the shared secret
struct logon_proof
{
    // uint8        cmd;     // the receive loop will have consumed this byte
    // already, so it's commented out
    uint8 A[32];
    uint8 M1[20];
    uint8 crc[20];
    uint8 number_of_keys;
    uint8 security_flags; // 0x00-0x04
};

// server -> client, prove knowledge of the shared secret
struct logon_proof_reply
{
    uint8 cmd;
    uint8 error;
    uint8 M2[20];
    uint32 flags; // see enum AccountFlags
    uint32 survey;
    uint16 extra_flags; // some flags (AccountMsgAvailable = 0x01)
};

// client -> server
struct reconnect_proof
{
    // uint8        cmd;     // the receive loop will have consumed this byte
    // already, so it's commented out
    uint8 R1[16];
    uint8 R2[20];
    uint8 R3[20];
    uint8 number_of_keys;
};

#if defined(__GNUC__)
#pragma pack()
#else
#pragma pack(pop)
#endif

class connection : private boost::noncopyable,
                   public boost::enable_shared_from_this<connection>
{
public:
    connection(boost::asio::io_service& io);

    boost::asio::ip::tcp::socket& socket() { return socket_; }
    void start();
    void stop();
    bool is_closed() const { return !socket_.is_open(); }
    std::string remote_address() const;

private:
    static const int V_S_BYTE_SIZE = 32;

    void handle_read(const boost::system::error_code& e);
    void handle_write(const boost::system::error_code& e);

    void handle_read_cmd(const boost::system::error_code& e);
    void handle_read_logon_challenge(const boost::system::error_code& e);
    void handle_read_username(const boost::system::error_code& e);
    void handle_read_logon_proof(const boost::system::error_code& e);
    void handle_read_token_size(const boost::system::error_code& e);
    void handle_read_token(const boost::system::error_code& e);
    void handle_read_reconnect_proof(const boost::system::error_code& e);
    void handle_read_realmlist(const boost::system::error_code& e);

    void handle_logon_challenge();
    void handle_reconnect_challenge();
    void handle_logon_proof();
    void handle_reconnect_proof();
    void handle_realmlist();

    // logon challenge and reconnect challenge send the same packet, so we have
    // one process function
    bool process_challenge();
    void LoadRealmlist(ByteBuffer& pkt, uint32 acctid);

    // Send the current output_buffer_
    void send();

    boost::asio::ip::tcp::socket socket_;

    uint32 account_id_;
    connection_state state_;
    std::string escaped_username_;
    std::string escaped_ip_address_;
    AccountTypes account_security_level_;
    std::string localization_name_;
    std::string os_;
    std::string token_key_;
    std::string last_ip_;
    uint16 build_;
    uint8 security_flags_;
    uint8_t client_token_size_;
    char client_token_[257];

    uint8 cmd_buffer_;
    char username_[MAX_USERNAME_LEN + 1];
    union
    {
        logon_challenge
            logon_challenge_; // Used for both logon and reconnect challenge
        logon_proof logon_proof_;
        reconnect_proof reconnect_proof_;
        char realmlist_data_[4];
    };
    ByteBuffer output_buffer_;

    // SRP6 related stuff
    void init_srp6();
    void logon_srp6(QueryResult* result);
    bool proof_srp6(Sha1Hash& sha);
    void calc_v_s(const std::string& sha_pass_hash);

    // TOTP token verification
    bool verify_token();

    BigNumber N_, g_, s_, v_, B_, b_, K_;
    BigNumber reconnect_rand_bytes_;
};

#endif
