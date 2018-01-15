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

/** \file
    \ingroup u2w
*/

#include "WorldSession.h"
#include "BattleGroundMgr.h"
#include "Common.h"
#include "Group.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "logging.h"
#include "MapManager.h"
#include "ObjectMgr.h"
#include "Opcodes.h"
#include "Player.h"
#include "SocialMgr.h"
#include "WardenMac.h"
#include "WardenWin.h"
#include "World.h"
#include "WorldConnection.h"
#include "WorldPacket.h"
#include "lfg_tool_container.h"
#include "Config/Config.h"
#include "Database/DatabaseEnv.h"
#include <iostream>

// select opcodes appropriate for processing in Map::Update context for current
// session state
static bool MapSessionFilterHelper(
    WorldSession* session, OpcodeHandler const& opHandle)
{
    // we do not process thread-unsafe packets
    if (opHandle.packetProcessing == PROCESS_THREADUNSAFE)
        return false;

    // we do not process not loggined player packets
    Player* plr = session->GetPlayer();
    if (!plr)
        return false;

    // in Map::Update() we do not process packets where player is not in world!
    return plr->IsInWorld();
}

bool MapSessionFilter::Process(WorldPacket* packet)
{
    OpcodeHandler const& opHandle = opcodeTable[packet->opcode()];
    if (opHandle.packetProcessing == PROCESS_INPLACE)
        return true;

    // let's check if our opcode can be really processed in Map::Update()
    return MapSessionFilterHelper(m_pSession, opHandle);
}

// we should process ALL packets when player is not in world/logged in
// OR packet handler is not thread-safe!
bool WorldSessionFilter::Process(WorldPacket* packet)
{
    OpcodeHandler const& opHandle = opcodeTable[packet->opcode()];
    // check if packet handler is supposed to be safe
    if (opHandle.packetProcessing == PROCESS_INPLACE)
        return true;

    // let's check if our opcode can't be processed in Map::Update()
    return !MapSessionFilterHelper(m_pSession, opHandle);
}

/// WorldSession constructor
WorldSession::WorldSession(uint32 id, connection_ptr sock, AccountTypes sec,
    uint8 expansion, time_t mute_time, LocaleConstant locale)
  : client_tick_count(0), server_tick_count(0), lfg_auto_join(false),
    lfg_auto_invite(false), m_muteTime(mute_time), _player(nullptr),
    m_Socket(sock), _security(sec), _accountId(id), m_expansion(expansion),
    _logoutTime(0), m_inQueue(false), m_playerLoading(false),
    m_playerLogout(false), m_playerRecentlyLogout(false), m_playerSave(false),
    m_sessionDbcLocale(sWorld::Instance()->GetAvailableDbcLocale(locale)),
    m_sessionDbLocaleIndex(sObjectMgr::Instance()->GetIndexForLocale(locale)),
    m_latency(0), m_tutorialState(TUTORIALDATA_UNCHANGED),
    char_select_start_t_(0), m_Warden(nullptr), force_facing_update_(false),
    altf4_timer_(0), kicked_(false)
{
    char_select_timeout_ =
        sWorld::Instance()->getConfig(CONFIG_UINT32_CHARACTER_SCREEN_TIMEOUT) *
        60;

    if (sock)
    {
        m_Address = sock->GetRemoteAddress();
    }
}

/// WorldSession destructor
WorldSession::~WorldSession()
{
    /// - If have unclosed socket, close it
    if (m_Socket)
    {
        // NOTE: session cannot be deleted from world_connection, as that would
        // cause a dead-lock when the session's destructor accesses the
        // world_connection
        // to close the socket.
        // Instead we null it out in session's destructor, and delete the
        // session in World::UpdateSessions
        m_Socket->null_session();

        m_Socket->CloseSocket();
        m_Socket.reset();
    }

    delete m_Warden;
    m_Warden = nullptr;

    // remove from LFG tool
    sLfgToolContainer::Instance()->remove(this);
}

void WorldSession::SizeError(WorldPacket const& packet, uint32 size) const
{
    logging.error("Client (account %u) send packet %s (%u) with size " SIZEFMTD
                  " but expected %u (attempt crash server?), skipped",
        GetAccountId(), LookupOpcodeName(packet.opcode()), packet.opcode(),
        packet.size(), size);
}

/// Get the player name
char const* WorldSession::GetPlayerName() const
{
    return GetPlayer() ? GetPlayer()->GetName() : "<none>";
}

/// Send a packet to the client
void WorldSession::send_packet(const WorldPacket* packet)
{
    if (m_Socket)
    {
        WorldPacket copy(*packet);
        m_Socket->queue_packet(std::move(copy));
    }
}

/// Send a packet to the client
void WorldSession::send_packet(WorldPacket&& packet)
{
    if (m_Socket)
        m_Socket->queue_packet(std::move(packet));
}

/// Add an incoming packet to the queue
void WorldSession::QueuePacket(WorldPacket&& new_packet)
{
    std::lock_guard<std::mutex> guard(recv_queue_mutex_);

    // order: newest (front) to oldest (back)
    recv_queue_.push_front(std::move(new_packet));
}

/// Logging helper for unexpected opcodes
void WorldSession::LogUnexpectedOpcode(WorldPacket* packet, const char* reason)
{
    logging.error("SESSION: received unexpected opcode %s (0x%.4X) %s",
        LookupOpcodeName(packet->opcode()), packet->opcode(), reason);
}

/// Logging helper for unexpected opcodes
void WorldSession::LogUnprocessedTail(WorldPacket* packet)
{
    logging.error(
        "SESSION: opcode %s (0x%.4X) have unprocessed tail data (read stop "
        "at " SIZEFMTD " from " SIZEFMTD ")",
        LookupOpcodeName(packet->opcode()), packet->opcode(), packet->rpos(),
        packet->wpos());
}

/// Update the WorldSession (triggered by World update)
bool WorldSession::Update(PacketFilter& updater, uint32 diff)
{
    ///- Retrieve packets from the receive queue and call the appropriate
    /// handlers
    /// not process packets if socket already closed
    WorldPacket packet;
    try
    {
        while (m_Socket && !m_Socket->IsClosed())
        {
            {
                std::lock_guard<std::mutex> guard(recv_queue_mutex_);
                if (recv_queue_.empty())
                    break; // no more packets
                packet = std::move(recv_queue_.back());
                recv_queue_
                    .pop_back(); // order: newest (front) to oldest (back)
            }

            OpcodeHandler const& opHandle = opcodeTable[packet.opcode()];
            switch (opHandle.status)
            {
            case STATUS_LOGGEDIN:
                if (!_player)
                {
                    // skip STATUS_LOGGEDIN opcode unexpected errors if player
                    // logout sometime ago - this can be network lag delayed
                    // packets
                    if (!m_playerRecentlyLogout)
                        LogUnexpectedOpcode(
                            &packet, "the player has not logged in yet");
                }
                else if (_player->IsInWorld())
                    ExecuteOpcode(opHandle, &packet);

                // lag can cause STATUS_LOGGEDIN opcodes to arrive after the
                // player started a transfer
                break;
            case STATUS_LOGGEDIN_OR_RECENTLY_LOGGEDOUT:
                if (!_player && !m_playerRecentlyLogout)
                {
                    LogUnexpectedOpcode(&packet,
                        "the player has not logged in yet and not recently "
                        "logout");
                }
                else
                    // not expected _player or must checked in packet hanlder
                    ExecuteOpcode(opHandle, &packet);
                break;
            case STATUS_TRANSFER:
                if (!_player)
                    LogUnexpectedOpcode(
                        &packet, "the player has not logged in yet");
                else if (_player->IsInWorld())
                    LogUnexpectedOpcode(
                        &packet, "the player is still in world");
                else
                    ExecuteOpcode(opHandle, &packet);
                break;
            case STATUS_AUTHED:
                // prevent cheating with skip queue wait
                if (m_inQueue)
                {
                    LogUnexpectedOpcode(
                        &packet, "the player not pass queue yet");
                    break;
                }

                // single from authed time opcodes send in to after logout time
                // and before other STATUS_LOGGEDIN_OR_RECENTLY_LOGGOUT opcodes.
                if (packet.opcode() != CMSG_SET_ACTIVE_VOICE_CHANNEL)
                    m_playerRecentlyLogout = false;

                ExecuteOpcode(opHandle, &packet);
                break;
            case STATUS_NEVER:
                logging.error(
                    "SESSION: received not allowed opcode %s (0x%.4X)",
                    LookupOpcodeName(packet.opcode()), packet.opcode());
                break;
            case STATUS_UNHANDLED:
                LOG_DEBUG(logging,
                    "SESSION: received not handled opcode %s (0x%.4X)",
                    LookupOpcodeName(packet.opcode()), packet.opcode());
                break;
            default:
                logging.error(
                    "SESSION: received wrong-status-req opcode %s (0x%.4X)",
                    LookupOpcodeName(packet.opcode()), packet.opcode());
                break;
            }
        }

        // Handle facing packet if we have one pending
        if (m_Socket && !m_Socket->IsClosed() && saved_facing_.size() > 0)
        {
            packet = std::move(saved_facing_);
            saved_facing_.clear();
            force_facing_update_ = true;
            HandleMovementOpcodes(packet);
            force_facing_update_ = false;
        }
    }
    catch (ByteBufferException&)
    {
        logging.error(
            "WorldSession::Update ByteBufferException occured while "
            "parsing a packet (opcode: %u) from client %s, accountid=%i.",
            packet.opcode(), GetRemoteAddress().c_str(), GetAccountId());
        if (logging.get_logger().get_level() > LogLevel::debug)
        {
            LOG_DEBUG(logging, "Dumping error causing packet:");
            packet.hexlike();
        }

        if (sWorld::Instance()->getConfig(
                CONFIG_BOOL_KICK_PLAYER_ON_BAD_PACKET))
        {
            LOG_DEBUG(logging,
                "Disconnecting session [account id %u / address %s] for "
                "badly formatted packet.",
                GetAccountId(), GetRemoteAddress().c_str());

            KickPlayer();
        }
    }

    // Log the user off if they've been idle for a certain period of time
    if (char_select_timeout_ > 0 && char_select_start_t_ > 0 &&
        !m_playerLoading)
        if ((char_select_start_t_ + char_select_timeout_) <=
            WorldTimer::time_no_syscall())
        {
            KickPlayer();
            return false;
        }

    ///- Cleanup socket pointer if need
    if (m_Socket && m_Socket->IsClosed())
    {
        m_Socket.reset();
    }

    if (m_Socket && !m_Socket->IsClosed() && m_Warden)
        m_Warden->Update(diff);

    // check if we are safe to proceed with logout
    // logout procedure should happen only in World::UpdateSessions() method!!!
    if (updater.ProcessLogout())
    {
        if (!m_Socket && _player && _player->IsInWorld() && !kicked_)
        {
            if (altf4_timer_ == 0)
            {
                altf4_timer_ = WorldTimer::time_no_syscall() + 20;
                // Remove movement flags; should appear standing in the air
                _player->m_movementInfo.SetMovementFlags(0);
                _player->StopMoving();
                _player->SendHeartBeat();
            }
            if (altf4_timer_ >= WorldTimer::time_no_syscall())
                return true;
        }

        ///- If necessary, log the player out
        time_t currTime = WorldTimer::time_no_syscall();
        if (!m_Socket || (ShouldLogOut(currTime) && !m_playerLoading))
            LogoutPlayer(true);

        if (!m_Socket)
            return false; // Will remove this session from the world session map
    }

    return true;
}

/// %Log the player out
void WorldSession::LogoutPlayer(bool Save)
{
    // finish pending transfers before starting the logout
    while (_player && _player->IsBeingTeleportedFar())
        HandleMoveWorldportAckOpcode();

    m_playerLogout = true;
    m_playerSave = Save;

    if (!m_inQueue &&
        (!m_Socket || (_player && m_Socket->IsClosed()) || !_player))
    {
        // Allow the user to skip the login queue within a certain (admin
        // defined) timeframe
        sWorld::Instance()->add_recently_logged_off(GetAccountId());
    }

    if (_player)
    {
        LOG_DEBUG(logging,
            "Account: %d (IP: %s) Logout Character:[%s] (guid: %u)",
            GetAccountId(), GetRemoteAddress().c_str(), _player->GetName(),
            _player->GetGUIDLow());

        if (ObjectGuid lootGuid = GetPlayer()->GetLootGuid())
            DoLootRelease(lootGuid);

        // If the player just died before logging out, turn into ghost
        if (_player->GetDeathTimer())
        {
            _player->getHostileRefManager().deleteReferences();
            _player->BuildPlayerRepop();
            _player->RepopAtGraveyard();
        }
        else if (_player->HasAuraType(SPELL_AURA_SPIRIT_OF_REDEMPTION))
        {
            // this will kill character by SPELL_AURA_SPIRIT_OF_REDEMPTION
            _player->remove_auras(SPELL_AURA_MOD_SHAPESHIFT);
            //_player->SetDeathPvP(*); set at SPELL_AURA_SPIRIT_OF_REDEMPTION
            // apply time
            _player->KillPlayer();
            _player->BuildPlayerRepop();
            _player->RepopAtGraveyard();
        }
        else if (!_player->getAttackers().empty())
        {
            _player->CombatStop();
            _player->getHostileRefManager().setOnlineOfflineState(false);
        }

        // drop a flag if player is carrying it
        if (BattleGround* bg = _player->GetBattleGround())
            bg->EventPlayerLoggedOut(_player);

        // FG: finish pending transfers after starting the logout
        // this should fix players beeing able to logout and login back with
        // full hp at death position
        while (_player->IsBeingTeleportedFar())
            HandleMoveWorldportAckOpcode();

        ///- Reset the online field in the account table
        // no point resetting online in character table here as
        // Player::SaveToDB() will set it to 1 since player has not been removed
        // from world at this stage
        // No SQL injection as AccountID is uint32
        static SqlStatementID id;

        SqlStatement stmt = LoginDatabase.CreateStatement(
            id, "UPDATE account SET active_realm_id = ? WHERE id = ?");
        stmt.PExecute(uint32(0), GetAccountId());

        ///- If the player is in a guild, update the guild roster and broadcast
        /// a logout message to other guild members
        if (Guild* guild =
                sGuildMgr::Instance()->GetGuildById(_player->GetGuildId()))
        {
            if (MemberSlot* slot =
                    guild->GetMemberSlot(_player->GetObjectGuid()))
            {
                slot->SetMemberStats(_player);
                slot->UpdateLogoutTime();
            }

            guild->BroadcastEvent(
                GE_SIGNED_OFF, _player->GetObjectGuid(), _player->GetName());
        }

        ///- Remove pet
        _player->RemovePet(PET_SAVE_AS_CURRENT);

        ///- empty buyback items and save the player in the database
        // some save parts only correctly work in case player present in
        // map/player_lists (pets, etc)
        if (Save)
            _player->SaveToDB();

        ///- Leave all channels before player delete...
        _player->CleanupChannels();

        // Remove spying GM from group
        if (_player->spying_on_)
        {
            if (auto group =
                    sObjectMgr::Instance()->GetGroupById(_player->spying_on_))
                group->remove_spy(_player);
        }

        ///- If the player is in a group (or invited), remove him. If the group
        /// if then only 1 person, disband the group.
        _player->UninviteFromGroup();

        // remove player from the group if he is:
        // a) in group; b) not in raid group; c) logging out normally (not being
        // kicked or disconnected)
        if (_player->GetGroup() && !_player->GetGroup()->isRaidGroup() &&
            m_Socket)
            _player->RemoveFromGroup();

        ///- Send update to group
        if (auto grp = _player->GetGroup())
        {
            grp->SendUpdate();

            // Is player leader of his group?
            if (grp->IsLeader(_player->GetObjectGuid()))
            {
                // If it's a BG group, just pass leader on right away
                if (grp->isBGGroup())
                    grp->PassLeaderOnward();
                // Otherwise, mark group to get a new leader if player doesn't
                // relog
                else
                    sObjectMgr::Instance()->AddOfflineLeaderGroup(grp);
            }
        }

        ///- Broadcast a logout message to the player's friends
        sSocialMgr::Instance()->SendFriendStatus(
            _player, FRIEND_OFFLINE, _player->GetObjectGuid(), true);
        sSocialMgr::Instance()->RemovePlayerSocial(_player->GetGUIDLow());

        ///- Remove the player from the world
        // the player may not be in the world when logging out
        // e.g if he got disconnected during a transfer to another map
        // calls to GetMap in this case may cause crashes
        // NOTE: Player::RepopAtGraveyard will already have queued an erase
        if (_player->IsInWorld())
        {
            Map* _map = _player->GetMap();
            _map->erase(_player, true);
        }
        else
        {
            _player->CleanupsBeforeDelete();
            sObjectAccessor::Instance()->RemoveObject(_player);
            delete _player;
        }

        SetPlayer(nullptr);

        ///- Send the 'logout complete' packet to the client
        WorldPacket data(SMSG_LOGOUT_COMPLETE, 0);
        send_packet(std::move(data));

        ///- Since each account can only have one online character at any given
        /// time, ensure all characters for active account are marked as offline
        // No SQL injection as AccountId is uint32

        static SqlStatementID updChars;

        stmt = CharacterDatabase.CreateStatement(
            updChars, "UPDATE characters SET online = 0 WHERE account = ?");
        stmt.PExecute(GetAccountId());
    }

    sLfgToolContainer::Instance()->remove(this);

    m_playerLogout = false;
    m_playerSave = false;
    m_playerRecentlyLogout = true;
    LogoutRequest(0);
}

void WorldSession::DisconnectPlayer()
{
    if (m_Socket)
        m_Socket->CloseSocket();
}

void WorldSession::KickPlayer()
{
    if (m_Socket)
        m_Socket->CloseSocket();
    kicked_ = true;
}

void WorldSession::SendAreaTriggerMessage(const char* Text, ...)
{
    va_list ap;
    char szStr[1024];
    szStr[0] = '\0';

    va_start(ap, Text);
    vsnprintf(szStr, 1024, Text, ap);
    va_end(ap);

    uint32 length = strlen(szStr) + 1;
    WorldPacket data(SMSG_AREA_TRIGGER_MESSAGE, 4 + length);
    data << length;
    data << szStr;
    send_packet(std::move(data));
}

void WorldSession::SendNotification(const char* format, ...)
{
    if (format)
    {
        va_list ap;
        char szStr[1024];
        szStr[0] = '\0';
        va_start(ap, format);
        vsnprintf(szStr, 1024, format, ap);
        va_end(ap);

        WorldPacket data(SMSG_NOTIFICATION, (strlen(szStr) + 1));
        data << szStr;
        send_packet(std::move(data));
    }
}

void WorldSession::SendNotification(int32 string_id, ...)
{
    char const* format = GetMangosString(string_id);
    if (format)
    {
        va_list ap;
        char szStr[1024];
        szStr[0] = '\0';
        va_start(ap, string_id);
        vsnprintf(szStr, 1024, format, ap);
        va_end(ap);

        WorldPacket data(SMSG_NOTIFICATION, (strlen(szStr) + 1));
        data << szStr;
        send_packet(std::move(data));
    }
}

const char* WorldSession::GetMangosString(int32 entry) const
{
    return sObjectMgr::Instance()->GetMangosString(
        entry, GetSessionDbLocaleIndex());
}

#ifndef OPTIMIZED_BUILD
void WorldSession::Handle_NULL(WorldPacket& recvPacket)
{
    LOG_DEBUG(logging, "SESSION: received unimplemented opcode %s (0x%.4X)",
        LookupOpcodeName(recvPacket.opcode()), recvPacket.opcode());
}
#else
void WorldSession::Handle_NULL(WorldPacket&)
{
}
#endif

void WorldSession::Handle_EarlyProccess(WorldPacket& recvPacket)
{
    logging.error(
        "SESSION: received opcode %s (0x%.4X) that must be processed in "
        "WorldSocket::OnRead",
        LookupOpcodeName(recvPacket.opcode()), recvPacket.opcode());
}

void WorldSession::Handle_ServerSide(WorldPacket& recvPacket)
{
    logging.error("SESSION: received server-side opcode %s (0x%.4X)",
        LookupOpcodeName(recvPacket.opcode()), recvPacket.opcode());
}

void WorldSession::Handle_Deprecated(WorldPacket& recvPacket)
{
    logging.error("SESSION: received deprecated opcode %s (0x%.4X)",
        LookupOpcodeName(recvPacket.opcode()), recvPacket.opcode());
}

void WorldSession::SendAuthWaitQue(uint32 position)
{
    if (position == 0)
    {
        WorldPacket packet(SMSG_AUTH_RESPONSE, 1);
        packet << uint8(AUTH_OK);
        send_packet(std::move(packet));
    }
    else
    {
        WorldPacket packet(SMSG_AUTH_RESPONSE, 1 + 4);
        packet << uint8(AUTH_WAIT_QUEUE);
        packet << uint32(position);
        send_packet(std::move(packet));
    }
}

void WorldSession::LoadTutorialsData()
{
    for (auto& elem : m_Tutorials)
        elem = 0;

    std::unique_ptr<QueryResult> result(CharacterDatabase.PQuery(
        "SELECT tut0,tut1,tut2,tut3,tut4,tut5,tut6,tut7 FROM "
        "character_tutorial WHERE account = '%u'",
        GetAccountId()));

    if (!result)
    {
        m_tutorialState = TUTORIALDATA_NEW;
        return;
    }

    do
    {
        Field* fields = result->Fetch();

        for (int iI = 0; iI < 8; ++iI)
            m_Tutorials[iI] = fields[iI].GetUInt32();
    } while (result->NextRow());

    m_tutorialState = TUTORIALDATA_UNCHANGED;
}

void WorldSession::SendTutorialsData()
{
    WorldPacket data(SMSG_TUTORIAL_FLAGS, 4 * 8);
    for (auto& elem : m_Tutorials)
        data << elem;
    send_packet(std::move(data));
}

void WorldSession::SaveTutorialsData()
{
    static SqlStatementID updTutorial;
    static SqlStatementID insTutorial;

    switch (m_tutorialState)
    {
    case TUTORIALDATA_CHANGED:
    {
        SqlStatement stmt = CharacterDatabase.CreateStatement(updTutorial,
            "UPDATE character_tutorial SET tut0=?, tut1=?, tut2=?, tut3=?, "
            "tut4=?, tut5=?, tut6=?, tut7=? WHERE account = ?");
        for (auto& elem : m_Tutorials)
            stmt.addUInt32(elem);

        stmt.addUInt32(GetAccountId());
        stmt.Execute();
    }
    break;

    case TUTORIALDATA_NEW:
    {
        SqlStatement stmt = CharacterDatabase.CreateStatement(insTutorial,
            "INSERT INTO character_tutorial "
            "(account,tut0,tut1,tut2,tut3,tut4,tut5,tut6,tut7) VALUES (?, ?, "
            "?, ?, ?, ?, ?, ?, ?)");

        stmt.addUInt32(GetAccountId());
        for (auto& elem : m_Tutorials)
            stmt.addUInt32(elem);

        stmt.Execute();
    }
    break;
    case TUTORIALDATA_UNCHANGED:
        break;
    }

    m_tutorialState = TUTORIALDATA_UNCHANGED;
}

void WorldSession::InitWarden(BigNumber* K, std::string os)
{
    if (!sWorld::Instance()->getConfig(CONFIG_BOOL_WARDEN_ENABLED))
        return;

    // Note: Validity of OS string is verified when logged in
    if (os == "Win")
    {
        m_Warden = new WardenWin;
        m_Warden->Init(this, K);
    }
    else if (os == "OSX")
    {
        // OSX not supported at the moment
        // m_Warden = new WardenMac;
        // m_Warden->Init(this, K);
    }
}

void WorldSession::ExecuteOpcode(
    OpcodeHandler const& opHandle, WorldPacket* packet)
{
    // need prevent do internal far teleports in handlers because some handlers
    // do lot steps
    // or call code that can do far teleports in some conditions unexpectedly
    // for generic way work code
    if (_player)
        _player->SetCanDelayTeleport(true);

    (this->*opHandle.handler)(*packet);

    if (_player)
    {
        // can be not set in fact for login opcode, but this not create
        // porblems.
        _player->SetCanDelayTeleport(false);

        // we should execute delayed teleports only for alive(!) players
        // because we don't want player's ghost teleported from graveyard
        if (_player->ShouldExecuteDelayedTeleport())
            _player->TeleportTo(
                _player->m_teleport_dest, _player->m_teleport_options);
    }

    if (packet->rpos() < packet->wpos() &&
        logging.get_logger().get_level() == LogLevel::debug)
        LogUnprocessedTail(packet);
}

std::string WorldSession::GetPlayerInfo() const
{
    std::ostringstream ss;

    ss << GetPlayerName() << " (Guid: "
       << (_player != nullptr ? _player->GetObjectGuid().GetRawValue() : 0)
       << ", Account: " << GetAccountId() << ")";

    return ss.str();
}

void WorldSession::translate_movement_timestamp(MovementInfo& info)
{
    auto t = (int64)server_tick_count +
             ((int64)info.time - (int64)client_tick_count);

    if (t < 0)
        t = 0;

    info.time = (uint32)t;
}
