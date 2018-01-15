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

#include "Warden.h"
#include "ByteBuffer.h"
#include "Chat.h"
#include "Common.h"
#include "logging.h"
#include "Opcodes.h"
#include "Player.h"
#include "Util.h"
#include "WardenWin.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "ban_wave.h"
#include <openssl/md5.h>
#include <openssl/sha.h>

static auto& logger = logging.get_logger("anticheat.warden");

Warden::Warden()
  : session_(nullptr), iCrypto(16), oCrypto(16),
    m_WardenCheckTimer(10000 /*10 sec*/), m_WardenDataSent(false),
    m_WardenKickTimer(0), Module(nullptr), m_initialized(false),
    slow_check_delay_timer_(0)
{
}

Warden::~Warden()
{
    if (Module)
        delete[] Module->CompressedData;
    delete Module;
    Module = nullptr;
    m_initialized = false;
}

void Warden::SendModuleToClient()
{
    LOG_DEBUG(logging, "Send module to client");

    // Create packet structure
    WardenModuleTransfer pkt;

    uint32 size_left = Module->CompressedSize;
    uint32 pos = 0;
    uint16 burst_size;
    while (size_left > 0)
    {
        burst_size = size_left < 500 ? size_left : 500;
        pkt.Command = WARDEN_SMSG_MODULE_CACHE;
        pkt.DataSize = burst_size;
        memcpy(pkt.Data, &Module->CompressedData[pos], burst_size);
        size_left -= burst_size;
        pos += burst_size;

        EncryptData((uint8*)&pkt, burst_size + 3);
        WorldPacket pkt1(SMSG_WARDEN_DATA, burst_size + 3);
        pkt1.append((uint8*)&pkt, burst_size + 3);
        session_->send_packet(std::move(pkt1));
    }
}

void Warden::RequestModule()
{
    LOG_DEBUG(logging, "Request module");

    // Create packet structure
    WardenModuleUse Request;
    Request.Command = WARDEN_SMSG_MODULE_USE;

    memcpy(Request.Module_Id, Module->ID, 16);
    memcpy(Request.Module_Key, Module->Key, 16);
    Request.Size = Module->CompressedSize;

    // Encrypt with warden RC4 key.
    EncryptData((uint8*)&Request, sizeof(WardenModuleUse));

    WorldPacket pkt(SMSG_WARDEN_DATA, sizeof(WardenModuleUse));
    pkt.append((uint8*)&Request, sizeof(WardenModuleUse));
    session_->send_packet(std::move(pkt));
}

void Warden::Update(const uint32 diff)
{
    if (m_initialized)
    {
        if (m_WardenDataSent)
        {
            uint32 maxClientResponseDelay = sWorld::Instance()->getConfig(
                CONFIG_UINT32_WARDEN_CLIENT_RESPONSE_DELAY);
            if (maxClientResponseDelay == 0)
                return; // Never kicked for not responding

            if (m_WardenKickTimer > maxClientResponseDelay * IN_MILLISECONDS)
            {
                logger.info(
                    "%s did not respond to his checks. Actions taken: Kicked "
                    "Player.",
                    session_->GetPlayerInfo().c_str());
                session_->KickPlayer();
            }
            else
                m_WardenKickTimer += diff;
        }
        else if (m_WardenCheckTimer > 0)
        {
            if (diff >= m_WardenCheckTimer)
            {
                RequestData();
                m_WardenCheckTimer =
                    sWorld::Instance()->getConfig(
                        CONFIG_UINT32_WARDEN_CLIENT_CHECK_HOLDOFF) *
                    IN_MILLISECONDS;
            }
            else
                m_WardenCheckTimer -= diff;
        }

        // Update slow check delay timer
        if (slow_check_delay_timer_ <= diff)
            slow_check_delay_timer_ = 0;
        else
            slow_check_delay_timer_ -= diff;
    }
}

void Warden::DecryptData(uint8* Buffer, uint32 Len)
{
    iCrypto.UpdateData(Len, Buffer);
}

void Warden::EncryptData(uint8* Buffer, uint32 Len)
{
    oCrypto.UpdateData(Len, Buffer);
}

bool Warden::IsValidCheckSum(
    uint32 checksum, const uint8* Data, const uint16 Length)
{
    uint32 newchecksum = BuildChecksum(Data, Length);

    if (checksum != newchecksum)
    {
        LOG_DEBUG(logging, "CHECKSUM IS NOT VALID");
        return false;
    }
    else
    {
        LOG_DEBUG(logging, "CHECKSUM IS VALID");
        return true;
    }
}

uint32 Warden::BuildChecksum(const uint8* data, uint32 dataLen)
{
    uint8 hash[20];
    SHA1(data, dataLen, hash);
    uint32 checkSum = 0;
    for (uint8 i = 0; i < 5; ++i)
    {
        auto p = reinterpret_cast<uint32_t*>(&hash[i * 4]);
        checkSum = checkSum ^ *p;
    }
    return checkSum;
}

void WorldSession::HandleWardenDataOpcode(WorldPacket& recv_data)
{
    if (!m_Warden)
    {
        logging.error(
            "WorldSession::HandleWardenDataOpcode: Account %u sent warden "
            "packet but does not have a server-side Warden Module.",
            GetAccountId());
        return;
    }

    m_Warden->DecryptData(
        const_cast<uint8*>(recv_data.contents()), recv_data.size());
    uint8 warden_operation;
    recv_data >> warden_operation;
    LOG_DEBUG(logging, "Got packet, warden operation code: %02X, size: %u",
        warden_operation, uint32(recv_data.size()));
    recv_data.hexlike();

    switch (warden_operation)
    {
    case WARDEN_CMSG_MODULE_MISSING:
        m_Warden->SendModuleToClient();
        break;
    case WARDEN_CMSG_MODULE_OK:
        m_Warden->RequestHash();
        break;
    case WARDEN_CMSG_CHEAT_CHECKS_RESULT:
        m_Warden->HandleData(recv_data);
        break;
    case WARDEN_CMSG_MEM_CHECKS_RESULT:
        LOG_DEBUG(logging, "NYI WARDEN_CMSG_MEM_CHECKS_RESULT received!");
        break;
    case WARDEN_CMSG_HASH_RESULT:
        m_Warden->HandleHashResult(recv_data);
        m_Warden->InitializeModule();
        break;
    case WARDEN_CMSG_MODULE_FAILED:
        LOG_DEBUG(logging, "NYI WARDEN_CMSG_MODULE_FAILED received!");
        break;
    default:
        LOG_DEBUG(logging,
            "Got unknown warden operation code: %02X of size: %u.",
            warden_operation, uint32(recv_data.size() - 1));
        break;
    }
}

void Warden::punish(
    const std::vector<check_identifier>& check_ids, bool failed_timing_check)
{
    bool tagged_by_ban_wave = false;
    if (sBanWave::Instance()->has_ban_on(session_->GetPlayerName()))
        tagged_by_ban_wave = true;

    std::pair<WardenAction, std::string> highest_action =
        std::make_pair(WARDEN_ACTION_LOG, "");

    if (failed_timing_check)
        highest_action = punish(check_identifier(0, false), tagged_by_ban_wave);

    for (const auto& check_id : check_ids)
    {
        std::pair<WardenAction, std::string> action =
            punish(check_id, tagged_by_ban_wave);
        if (action.first > highest_action.first)
            highest_action = action;
    }

    // Do not take action if Warden debugging is turned on
    if (sWorld::Instance()->debugging_warden())
        return;

    switch (highest_action.first)
    {
    case WARDEN_ACTION_LOG:
        break; // Do nothing
    case WARDEN_ACTION_KICK:
        session_->KickPlayer();
        break;
    case WARDEN_ACTION_BAN_WAVE:
    {
        sBanWave::Instance()->add_ban(
            session_->GetPlayerName(), highest_action.second, "Warden");
        break;
    }
    case WARDEN_ACTION_BAN:
    {
        sWorld::Instance()->BanAccount(BAN_CHARACTER, session_->GetPlayerName(),
            sWorld::Instance()->getConfig(CONFIG_UINT32_WARDEN_BAN_TIME),
            highest_action.second, "Warden");
        break;
    }
    default:
        break;
    }
}

std::pair<WardenAction, std::string> Warden::punish(
    check_identifier check, bool ban_waved)
{
    Player* player = session_->GetPlayer(); // Can be NULL

    if (check.id == 0 && check.dynamic == false)
    {
        if (sWorld::Instance()->debugging_warden())
        {
            if (player)
                ChatHandler(player).SendSysMessage(
                    "[Warden]: You were caught for an incorrect TIMING_CHECK "
                    "response. Action would've been: Ban Wave.");
        }
        else
        {
            if (!ban_waved)
                logger.info(
                    "%s failed timing check. Actions taken: Added Player to "
                    "Ban Wave.",
                    session_->GetPlayerInfo().c_str());
        }
        return std::make_pair(
            WARDEN_ACTION_BAN_WAVE, WardenDataStorage.GetComment(check));
    }

    const WardenData* data;
    if (check.dynamic)
        data = get_dynamic_check(check.id);
    else
        data = WardenDataStorage.GetWardenDataById(check.id);
    assert(data);

    WardenAction action = data->action;

    if (sWorld::Instance()->debugging_warden())
    {
        if (player)
        {
            ChatHandler(player).PSendSysMessage(
                "[Warden]: You were caught failing check with id: %s (comment: "
                "%s). Action would've been: %s.",
                check.printable().c_str(),
                WardenDataStorage.GetComment(check).c_str(),
                (action == WARDEN_ACTION_LOG ?
                        "Logging" :
                        action == WARDEN_ACTION_KICK ?
                        "Kick" :
                        action == WARDEN_ACTION_BAN_WAVE ? "Ban Wave" :
                                                           "Immediate Ban"));
        }
    }
    else if (!ban_waved || action == WARDEN_ACTION_BAN)
    {
        logger.info(
            "%s failed check with id: %s (comment: %s). Actions taken: %s.",
            session_->GetPlayerInfo().c_str(), check.printable().c_str(),
            WardenDataStorage.GetComment(check).c_str(),
            (action == WARDEN_ACTION_LOG ? "Logged Offense" :
                                           action == WARDEN_ACTION_KICK ?
                                           "Kicked Player" :
                                           action == WARDEN_ACTION_BAN_WAVE ?
                                           "Added Player to Ban Wave" :
                                           "Banned Player"));
    }

    return std::make_pair(action, WardenDataStorage.GetComment(check));
}
