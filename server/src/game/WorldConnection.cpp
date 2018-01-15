#include "WorldConnection.h"
#include "AddonHandler.h"
#include "logging.h"
#include "Opcodes.h"
#include "Util.h"
#include "World.h"
#include "netstat.h"
#include "Auth/BigNumber.h"
#include "Auth/Sha1.h"
#include "Database/DatabaseEnv.h"
#include <boost/bind.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <algorithm>
#include <chrono>
#include <cstring>

static auto& net_log = logging.get_logger("net");
static auto& recv_op_log = logging.get_logger("net.opcode.recv");
static auto& send_op_log = logging.get_logger("net.opcode.send");

world_connection::world_connection(boost::asio::io_service& io)
  : socket_{io}, session_{nullptr}, seed_{static_cast<uint32>(rand32())},
    is_closing_{false}, timer_{io}, send_in_progress_{false},
    timer_started_{false}, last_ping_{0}, spam_ping_count_{0}
{
}

world_connection::~world_connection()
{
}

void world_connection::start()
{
    LOG_DEBUG(net_log, "Start connection for %s", remote_address().c_str());

    WorldPacket packet(SMSG_AUTH_CHALLENGE, 4);
    packet << seed_;
    queue_packet(std::move(packet));

    boost::asio::async_read(socket_,
        boost::asio::buffer(reinterpret_cast<char*>(&header_), sizeof(header_)),
        boost::bind(&world_connection::handle_read_header, shared_from_this(),
                                boost::asio::placeholders::error));
}

void world_connection::hard_stop()
{
    if (!socket_.is_open())
    {
        LOG_DEBUG(net_log, "Trying to close an already closed connection");
        return;
    }
    LOG_DEBUG(net_log, "Stop connection for %s", remote_address().c_str());
    boost::system::error_code ignored_ec;
    socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored_ec);
    socket_.close();
}

void world_connection::stop(bool hard /*= false*/)
{
    std::lock_guard<std::mutex> lock(mutex_);
    is_closing_ = true;

    if (hard)
        hard_stop();
}

bool world_connection::is_closed()
{
    std::lock_guard<std::mutex> lock(mutex_);
    return is_closing_ || !socket_.is_open();
}

std::string world_connection::remote_address() const
{
    if (socket_.is_open())
    {
        boost::system::error_code ec;
        auto endpoint = socket_.remote_endpoint(ec);
        if (ec)
        {
            logging.error("world_connection::remote_address() error: %s",
                ec.message().c_str());
            return "<unable to get address>";
        }
        return endpoint.address().to_string();
    }
    else
    {
        return "<socket not connected>";
    }
}

void world_connection::null_session()
{
    // NOTE: session cannot be deleted from world_connection, as that would
    // cause a dead-lock when the session's destructor accesses the
    // world_connection
    // to close the socket.
    // Instead we null it out in session's destructor, and delete the session in
    // World::UpdateSessions
    std::lock_guard<std::mutex> lock(mutex_);
    session_ = nullptr;
}

void world_connection::handle_read_header(const boost::system::error_code& e)
{
    if (e)
    {
        hard_stop();
        return;
    }

    // decrypt header and verify validity
    auth_crypter_.DecryptRecv(
        reinterpret_cast<uint8*>(&header_), sizeof(header_));
    EndianConvertReverse(header_.size);
    EndianConvert(header_.opcode);
    LOG_DEBUG(recv_op_log, "recv opcode 0x%03X (%s)", header_.opcode,
        LookupOpcodeName(header_.opcode));
    if (header_.size < 4 || header_.size > 10240 || header_.opcode > 10240)
    {
        logging.error("Received an invalid packet header (size %hu; cmd %u)",
            header_.size, header_.opcode);
        hard_stop();
        return;
    }

    std::size_t data_size = static_cast<std::size_t>(header_.size) -
                            4; // The 32 bit opcode is included in the size, but
                               // we already have that
    packet_.clear();
    packet_.opcode(header_.opcode);

    if (header_.size == 4)
    {
        // for this header only packet we can just call the data read handler
        // without requesting any more data
        handle_read_data(boost::system::error_code());
    }
    else
    {
        packet_.data().resize(data_size);
        boost::asio::async_read(socket_,
            boost::asio::buffer(reinterpret_cast<char*>(packet_.contents()),
                                    data_size),
            boost::bind(&world_connection::handle_read_data, shared_from_this(),
                                    boost::asio::placeholders::error));
    }
}

void world_connection::handle_read_data(const boost::system::error_code& e)
{
    if (e)
    {
        hard_stop();
        return;
    }

    try
    {
        switch (packet_.opcode())
        {
        case CMSG_PING:
            handle_ping(packet_);
            break;
        case CMSG_AUTH_SESSION:
            handle_auth_session(packet_);
            break;
        case CMSG_KEEP_ALIVE:
            /* We just ignore this packet */
            break;
        default:
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (session_)
            {
                if (!is_closing_)
                {
                    NETSTAT_LOG_RECV(packet_);
                    session_->QueuePacket(std::move(packet_));
                }
            }
            break;
        }
        }
    }
    catch (ByteBufferException&)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        logging.error(
            "WorldSocket::ProcessIncoming ByteBufferException occured while "
            "parsing an instant handled packet (opcode: %u) from client %s, "
            "accountid=%i.",
            packet_.opcode(), remote_address().c_str(),
            session_ ? session_->GetAccountId() : -1);

        if (logging.get_logger().get_level() == LogLevel::debug)
        {
            LOG_DEBUG(net_log, "Dumping error-causing packet:");
            packet_.hexlike();
        }

        if (sWorld::Instance()->getConfig(
                CONFIG_BOOL_KICK_PLAYER_ON_BAD_PACKET))
        {
            LOG_DEBUG(net_log,
                "Disconnecting session [account id %i / address %s] for badly "
                "formatted packet.",
                session_ ? session_->GetAccountId() : -1,
                remote_address().c_str());
            hard_stop();
        }
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (is_closing_ || !socket_.is_open())
            return;
    }

    boost::asio::async_read(socket_,
        boost::asio::buffer(reinterpret_cast<char*>(&header_), sizeof(header_)),
        boost::bind(&world_connection::handle_read_header, shared_from_this(),
                                boost::asio::placeholders::error));
}

void world_connection::queue_packet(WorldPacket&& packet)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (is_closing_ || !socket_.is_open())
        return;

    LOG_DEBUG(send_op_log, "send opcode 0x%03X (%s)", packet.opcode(),
        LookupOpcodeName(packet.opcode()));

    assert(packet.initialized());

    // NOTE: packet has space reserved for the header at the front of the buffer
    output_header* header = reinterpret_cast<output_header*>(&packet.data()[0]);

    // fill in and encrypt header
    header->size = static_cast<uint16>(
        packet.size() - 2); // the 2 bytes for the size not included
    header->opcode = packet.opcode();

    EndianConvert(header->opcode);
    EndianConvertReverse(header->size);
    auth_crypter_.EncryptSend(
        reinterpret_cast<uint8_t*>(header), sizeof(output_header));

    NETSTAT_LOG_SEND(packet);

    queue_.push_back(std::move(packet.data()));
    if (!send_in_progress_)
    {
        // Flush queue on pong packet, otherwise start a timer
        if (packet.opcode() == SMSG_PONG)
        {
            // Cancel pending timer
            if (timer_started_)
                timer_.cancel();
            flush_queue();
        }
        else if (!timer_started_)
        {
            timer_started_ = true;

            timer_.expires_from_now(std::chrono::milliseconds(50));

            timer_.async_wait(boost::bind(&world_connection::handle_send_timer,
                shared_from_this(), boost::asio::placeholders::error));
        }
    }
}

// YOU MUST HOLD THE LOCK ON mutex_ TO CALL THIS
void world_connection::flush_queue()
{
    assert(!queue_.empty() && currently_sending_.empty());

    NETSTAT_ON_WRITE();

    std::swap(queue_, currently_sending_);
    send_in_progress_ = true;

    std::vector<boost::asio::const_buffer> buffers;
    buffers.reserve(currently_sending_.size());

    for (auto& packet : currently_sending_)
        buffers.push_back(
            boost::asio::buffer(&packet.data()[0], packet.size()));

    boost::asio::async_write(socket_, buffers,
        boost::bind(&world_connection::handle_write, shared_from_this(),
                                 boost::asio::placeholders::error));
}

void world_connection::handle_send_timer(const boost::system::error_code& e)
{
    std::lock_guard<std::mutex> lock(mutex_);
    timer_started_ = false;

    if (e == boost::asio::error::operation_aborted)
        return;

    if (!send_in_progress_ && queue_.size() > 0)
        flush_queue();
}

void world_connection::handle_write(const boost::system::error_code& e)
{
    std::lock_guard<std::mutex> lock(mutex_);

    assert(send_in_progress_);

    currently_sending_.clear();
    send_in_progress_ = false;

    if (e)
    {
        hard_stop();
        return;
    }

    if (queue_.size() > 0)
        flush_queue();
}

void world_connection::handle_ping(WorldPacket& packet)
{
    uint32 ping, latency;
    packet >> ping >> latency;
    LOG_DEBUG(net_log,
        "world_connection::handle_ping: Received with ping: %u and latency: %u",
        ping, latency);

    // Mangos code marks ping packets that come more frequently than 27 seconds
    // in between as too often
    time_t now = WorldTimer::time_no_syscall();
    if (now - last_ping_ < 27)
    {
        uint32 max =
            sWorld::Instance()->getConfig(CONFIG_UINT32_MAX_OVERSPEED_PINGS);
        if (max && ++spam_ping_count_ > max)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (session_ && session_->GetSecurity() == SEC_PLAYER)
            {
                logging.error(
                    "world_connection::handle_ping: Player kicked for spam "
                    "pinging. Address = %s",
                    GetRemoteAddress().c_str());
                hard_stop();
                return;
            }
        }
    }
    else
        spam_ping_count_ = 0;

    last_ping_ = now;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (session_)
            session_->SetLatency(latency);
        else
        {
            logging.error(
                "world_connection::handle_ping: Player sent CMSG_PING without "
                "a valid session. Address = %s",
                GetRemoteAddress().c_str());
            hard_stop();
            return;
        }
    } // Release lock before we call queue_packet

    WorldPacket response(SMSG_PONG, 4);
    response << ping;
    queue_packet(std::move(response));
}

void world_connection::handle_auth_session(WorldPacket& recvPacket)
{
    if (session_)
    {
        logging.error(
            "WorldSocket::ProcessIncoming: Player send CMSG_AUTH_SESSION "
            "again");
        hard_stop();
        return;
    }

    uint8 digest[20];
    uint32 clientSeed, id, security;
    uint32 unk2;
    uint32 BuiltNumberClient;
    uint8 expansion = 0;
    LocaleConstant locale;
    std::string account;
    Sha1Hash sha1;
    BigNumber v, s, g, N, K;
    WorldPacket packet, SendAddonPacked;

    // Read the content of the packet
    recvPacket >> BuiltNumberClient;
    recvPacket >> unk2;
    recvPacket >> account;
    recvPacket >> clientSeed;
    recvPacket.read(digest, 20);

    LOG_DEBUG(net_log,
        "WorldSocket::HandleAuthSession: client %u, unk2 %u, account %s, "
        "clientseed %u",
        BuiltNumberClient, unk2, account.c_str(), clientSeed);

    // Check the version of client trying to connect
    if (!IsAcceptableClientBuild(BuiltNumberClient))
    {
        packet.initialize(SMSG_AUTH_RESPONSE, 1);
        packet << uint8(AUTH_VERSION_MISMATCH);

        queue_packet(std::move(packet));

        logging.error(
            "WorldSocket::HandleAuthSession: Sent Auth Response (version "
            "mismatch).");
        hard_stop();
        return;
    }

    // Get the account information from the realmd database
    std::string safe_account =
        account; // Duplicate, else will screw the SHA hash verification below
    LoginDatabase.escape_string(safe_account);
    // No SQL injection, username escaped.

    QueryResult* result = LoginDatabase.PQuery(
        "SELECT "
        "id, "         // 0
        "gmlevel, "    // 1
        "sessionkey, " // 2
        "last_ip, "    // 3
        "locked, "     // 4
        "v, "          // 5
        "s, "          // 6
        "expansion, "  // 7
        "mutetime, "   // 8
        "locale, "     // 9
        "os "          // 10
        "FROM account "
        "WHERE username = '%s'",
        safe_account.c_str());

    // Stop if the account is not found
    if (!result)
    {
        packet.initialize(SMSG_AUTH_RESPONSE, 1);
        packet << uint8(AUTH_UNKNOWN_ACCOUNT);

        queue_packet(std::move(packet));

        logging.error(
            "WorldSocket::HandleAuthSession: Sent Auth Response (unknown "
            "account).");
        hard_stop();
        return;
    }

    Field* fields = result->Fetch();

    expansion = ((sWorld::Instance()->getConfig(CONFIG_UINT32_EXPANSION) >
                     fields[7].GetUInt8()) ?
                     fields[7].GetUInt8() :
                     sWorld::Instance()->getConfig(CONFIG_UINT32_EXPANSION));

    N.SetHexStr(
        "894B645E89E1535BBDAD5B8B290650530801B18EBFBF5E8FAB3C82872A3E9BB7");
    g.SetDword(7);

    v.SetHexStr(fields[5].GetString());
    s.SetHexStr(fields[6].GetString());

    const char* sStr = s.AsHexStr(); // Must be freed by OPENSSL_free()
    const char* vStr = v.AsHexStr(); // Must be freed by OPENSSL_free()

    LOG_DEBUG(net_log,
        "WorldSocket::HandleAuthSession: (s,v) check s: %s v: %s", sStr, vStr);

    OPENSSL_free((void*)sStr);
    OPENSSL_free((void*)vStr);

    ///- Re-check ip locking (same check as in realmd).
    if (fields[4].GetUInt8() == 1) // if ip is locked
    {
        if (strcmp(fields[3].GetString(), remote_address().c_str()))
        {
            packet.initialize(SMSG_AUTH_RESPONSE, 1);
            packet << uint8(AUTH_FAILED);
            queue_packet(std::move(packet));

            delete result;
            LOG_DEBUG(net_log,
                "WorldSocket::HandleAuthSession: Sent Auth Response (Account "
                "IP differs).");
            hard_stop();
            return;
        }
    }

    id = fields[0].GetUInt32();
    security = fields[1].GetUInt16();
    if (security > SEC_FULL_GM) // prevent invalid security settings in DB
        security = SEC_FULL_GM;

    K.SetHexStr(fields[2].GetString());

    time_t mutetime = time_t(fields[8].GetUInt64());

    locale = LocaleConstant(fields[9].GetUInt8());
    if (locale >= MAX_LOCALE)
        locale = LOCALE_enUS;

    std::string os = fields[10].GetCppString();
    if (os.compare("Win") != 0 && os.compare("OSX") != 0)
    {
        delete result;
        LOG_DEBUG(net_log,
            "WorldSocket::HandleAuthSession: Invalid OS specified in the "
            "database. Value specified was: %s",
            os.c_str());
        hard_stop();
        return;
    }

    delete result;

    // Re-check account ban (same check as in realmd)
    QueryResult* banresult = LoginDatabase.PQuery(
        "SELECT 1 FROM account_banned WHERE id = %u AND active = 1 AND "
        "(unbandate > UNIX_TIMESTAMP() OR unbandate = bandate)"
        "UNION "
        "SELECT 1 FROM ip_banned WHERE (unbandate = bandate OR unbandate > "
        "UNIX_TIMESTAMP()) AND ip = '%s'",
        id, remote_address().c_str());

    if (banresult) // if account banned
    {
        packet.initialize(SMSG_AUTH_RESPONSE, 1);
        packet << uint8(AUTH_BANNED);
        queue_packet(std::move(packet));

        delete banresult;

        logging.error(
            "WorldSocket::HandleAuthSession: Sent Auth Response (Account "
            "banned).");
        hard_stop();
        return;
    }

    // Check locked state for server
    AccountTypes allowedAccountType =
        sWorld::Instance()->GetPlayerSecurityLimit();

    if (allowedAccountType > SEC_PLAYER &&
        AccountTypes(security) < allowedAccountType)
    {
        WorldPacket Packet(SMSG_AUTH_RESPONSE, 1);
        Packet << uint8(AUTH_UNAVAILABLE);

        queue_packet(std::move(packet));

        LOG_DEBUG(net_log,
            "WorldSocket::HandleAuthSession: User tries to login but his "
            "security level is not enough");
        hard_stop();
        return;
    }

    // Check that Key and account name are the same on client and server
    Sha1Hash sha;

    uint32 t = 0;
    uint32 seed = seed_;

    sha.UpdateData(account);
    sha.UpdateData((uint8*)&t, 4);
    sha.UpdateData((uint8*)&clientSeed, 4);
    sha.UpdateData((uint8*)&seed, 4);
    sha.UpdateBigNumbers(&K, NULL);
    sha.Finalize();

    if (memcmp(sha.GetDigest(), digest, 20))
    {
        packet.initialize(SMSG_AUTH_RESPONSE, 1);
        packet << uint8(AUTH_FAILED);

        queue_packet(std::move(packet));

        logging.error(
            "WorldSocket::HandleAuthSession: Sent Auth Response "
            "(authentification failed).");
        hard_stop();
        return;
    }

    // Defensive mode: If it's on we only allow players that have a character
    // over the level threshold
    if (sWorld::Instance()->defmode_level() != 0 && security == SEC_PLAYER)
    {
        std::unique_ptr<QueryResult> result(CharacterDatabase.PQuery(
            "SELECT guid FROM characters WHERE account=%u AND LEVEL>=%u LIMIT "
            "1",
            id, sWorld::Instance()->defmode_level()));
        if (!result)
        {
            sWorld::Instance()->defmode_stats(1, 0);
            LOG_DEBUG(net_log,
                "WorldSocket::HandleAuthSession: Defensive mod is on and "
                "player with account id %u did not have a character over the "
                "level threshold (%u).",
                id, sWorld::Instance()->defmode_level());
            hard_stop();
            return;
        }
        sWorld::Instance()->defmode_stats(0, 1);
    }

    std::string address = remote_address();

    LOG_DEBUG(net_log,
        "WorldSocket::HandleAuthSession: Client '%s' authenticated "
        "successfully from %s.",
        account.c_str(), address.c_str());

    // Update the last_ip in the database
    // No SQL injection, username escaped.
    static SqlStatementID updAccount;

    SqlStatement stmt = LoginDatabase.CreateStatement(
        updAccount, "UPDATE account SET last_ip = ? WHERE username = ?");
    stmt.PExecute(address.c_str(), account.c_str());

    auto shared_ptr = std::make_shared<WorldSession>(id, shared_from_this(),
        AccountTypes(security), expansion, mutetime, locale);

    // session_ keeps a raw reference to the pointer, it does not have shared
    // ownership and is notified when the session is no longer reliable
    session_ = shared_ptr.get();

    auth_crypter_.Init(&K);

    session_->LoadTutorialsData();

    // Initialize Warden system only if it is enabled in the config
    if (sWorld::Instance()->getConfig(CONFIG_BOOL_WARDEN_ENABLED))
        session_->InitWarden(&K, os);

    // FIXME: This should either become asynchronous or be removed. The reason
    // for its existance is however somewhat fuzzy atm
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    sWorld::Instance()->AddSession(std::move(shared_ptr));

    // Create and send the Addon packet
    if (sAddOnHandler::Instance()->BuildAddonPacket(
            &recvPacket, &SendAddonPacked))
        queue_packet(std::move(SendAddonPacked));
}
