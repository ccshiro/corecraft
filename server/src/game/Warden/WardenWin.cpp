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

#include "WardenWin.h"
#include "ByteBuffer.h"
#include "Common.h"
#include "logging.h"
#include "Opcodes.h"
#include "Player.h"
#include "ProgressBar.h"
#include "Util.h"
#include "WardenDataStorage.h"
#include "WardenModuleWin.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "Auth/Hmac.h"
#include "Auth/WardenKeyGeneration.h"
#include "Database/DatabaseEnv.h"
#include <openssl/md5.h>

static auto& logger = logging.get_logger("anticheat.warden");

// DYNAMIC CHECKS START
// FIXME: Only for Windows & Patch 2.4.3. Replace for different client versions.

#define PLAYER_BASE_STATIC_ADDRESS 0x00E29D28

#define SPEED_START_OFFSET 0xC6C
#define SPEED_END_OFFSET 0xC84
#define SPEED_LENGTH SPEED_END_OFFSET - SPEED_START_OFFSET + 4 // 28
#define TRACKING_OFFSET 0x3AC8
#define TRACKING_LENGTH 8
#define MOVEMENT_STATE_OFFSET 0x0C23 // wowemuhack targetting
// #define TRACK_CREATURES              0x3AC8
// #define TRACK_RESOURCES              0x3ACC
// #define HIDDEN_OFFSET                0x3D78

// Speeds are:
// C68 = Current speed
// C6C = walk speed
// C70 = run speed
// C74 = backwards speed
// C78 = swim speed
// C7C = sim backwards speed
// C80 = fly speed
// C84 = fly backwards speed

// DYNAMIC CHECKS END

WardenWin::WardenWin() : player_base_(0)
{
    slow_check_delay_timer_ =
        sWorld::Instance()->getConfig(CONFIG_UINT32_WARDEN_SLOW_CHECKS_DELAY) *
        IN_MILLISECONDS;
    if (slow_check_delay_timer_ != 0)
        slow_check_delay_timer_ =
            slow_check_delay_timer_ /
            5; // 20% of the delay timer for the first check

    WardenData temp = {0, 0, std::vector<uint8>(), 0, 0, "", WARDEN_ACTION_LOG};
    check_ = temp;
    memset(valid_dynamic_checks_, 0, sizeof(valid_dynamic_checks_));
}

WardenWin::~WardenWin()
{
}

void WardenWin::Init(WorldSession* pClient, BigNumber* K)
{
    session_ = pClient;
    // Generate Warden Key
    SHA1Randx WK(K->AsByteArray(), K->GetNumBytes());
    WK.generate(InputKey, 16);
    WK.generate(OutputKey, 16);
    /*
    Seed: 4D808D2C77D905C41A6380EC08586AFE (0x05 packet)
    Hash: 568C054C781A972A6037A2290C22B52571A06F4E (0x04 packet)
    Module MD5: 79C0768D657977D697E10BAD956CCED1
    New session_ Key: 7F 96 EE FD A5 B6 3D 20 A4 DF 8E 00 CB F4 83 04
    New Server Key: C2 B7 AD ED FC CC A9 C2 BF B3 F8 56 02 BA 80 9B
    */
    uint8 mod_seed[16] = {0x4D, 0x80, 0x8D, 0x2C, 0x77, 0xD9, 0x05, 0xC4, 0x1A,
        0x63, 0x80, 0xEC, 0x08, 0x58, 0x6A, 0xFE};

    memcpy(Seed, mod_seed, 16);

    iCrypto.Init(InputKey);
    oCrypto.Init(OutputKey);
    LOG_DEBUG(logging, "Server side warden for client %u initializing...",
        pClient->GetAccountId());
    LOG_DEBUG(
        logging, "  C->S Key: %s", ByteArrayToHexStr(InputKey, 16).c_str());
    LOG_DEBUG(
        logging, "  S->C Key: %s", ByteArrayToHexStr(OutputKey, 16).c_str());
    LOG_DEBUG(logging, "  Seed: %s", ByteArrayToHexStr(Seed, 16).c_str());
    LOG_DEBUG(logging, "Loading Module...");

    Module = GetModuleForClient(session_);

    LOG_DEBUG(logging, "  Module Key: %s",
        ByteArrayToHexStr(Module->Key, 16).c_str());
    LOG_DEBUG(
        logging, "  Module ID: %s", ByteArrayToHexStr(Module->ID, 16).c_str());
    RequestModule();
}

ClientWardenModule* WardenWin::GetModuleForClient(WorldSession* /*session*/)
{
    auto mod = new ClientWardenModule;

    uint32 len = sizeof(Module_79C0768D657977D697E10BAD956CCED1_Data);

    // data assign
    mod->CompressedSize = len;
    mod->CompressedData = new uint8[len];
    memcpy(
        mod->CompressedData, Module_79C0768D657977D697E10BAD956CCED1_Data, len);
    memcpy(mod->Key, Module_79C0768D657977D697E10BAD956CCED1_Key, 16);

    // md5 hash
    MD5_CTX ctx;
    MD5_Init(&ctx);
    MD5_Update(&ctx, mod->CompressedData, len);
    MD5_Final((uint8*)&mod->ID, &ctx);

    return mod;
}

void WardenWin::InitializeModule()
{
    LOG_DEBUG(logging, "Initialize module");

    // Create packet structure
    WardenInitModuleRequest Request;
    Request.Command1 = WARDEN_SMSG_MODULE_INITIALIZE;
    Request.Size1 = 20;
    Request.CheckSumm1 = BuildChecksum(&Request.Unk1, 20);
    Request.Unk1 = 1;
    Request.Unk2 = 0;
    Request.Type = 1;
    Request.String_library1 = 0;
    Request.Function1[0] = 0x00024F80; // 0x00400000 + 0x00024F80 SFileOpenFile
    Request.Function1[1] =
        0x000218C0; // 0x00400000 + 0x000218C0 SFileGetFileSize
    Request.Function1[2] = 0x00022530; // 0x00400000 + 0x00022530 SFileReadFile
    Request.Function1[3] = 0x00022910; // 0x00400000 + 0x00022910 SFileCloseFile

    Request.Command2 = WARDEN_SMSG_MODULE_INITIALIZE;
    Request.Size2 = 8;
    Request.CheckSumm2 = BuildChecksum(&Request.Unk2, 8);
    Request.Unk3 = 4;
    Request.Unk4 = 0;
    Request.String_library2 = 0;
    Request.Function2 =
        0x00419D40; // 0x00400000 + 0x00419D40 FrameScript::GetText
    Request.Function2_set = 1;

    Request.Command3 = WARDEN_SMSG_MODULE_INITIALIZE;
    Request.Size3 = 8;
    Request.CheckSumm3 = BuildChecksum(&Request.Unk5, 8);
    Request.Unk5 = 1;
    Request.Unk6 = 1;
    Request.String_library3 = 0;
    Request.Function3 =
        0x0046AE20; // 0x00400000 + 0x0046AE20 PerformanceCounter
    Request.Function3_set = 1;

    // Encrypt with warden RC4 key.
    EncryptData((uint8*)&Request, sizeof(WardenInitModuleRequest));

    WorldPacket pkt(SMSG_WARDEN_DATA, sizeof(WardenInitModuleRequest));
    pkt.append((uint8*)&Request, sizeof(WardenInitModuleRequest));
    session_->send_packet(std::move(pkt));
}

void WardenWin::RequestHash()
{
    LOG_DEBUG(logging, "Request hash");

    // Create packet structure
    WardenHashRequest Request;
    Request.Command = WARDEN_SMSG_HASH_REQUEST;
    memcpy(Request.Seed, Seed, 16);

    // Encrypt with warden RC4 key.
    EncryptData((uint8*)&Request, sizeof(WardenHashRequest));

    WorldPacket pkt(SMSG_WARDEN_DATA, sizeof(WardenHashRequest));
    pkt.append((uint8*)&Request, sizeof(WardenHashRequest));
    session_->send_packet(std::move(pkt));
}

void WardenWin::HandleHashResult(ByteBuffer& buff)
{
    buff.rpos(buff.wpos());

    const uint8 validHash[20] = {0x56, 0x8C, 0x05, 0x4C, 0x78, 0x1A, 0x97, 0x2A,
        0x60, 0x37, 0xA2, 0x29, 0x0C, 0x22, 0xB5, 0x25, 0x71, 0xA0, 0x6F, 0x4E};

    // verify key not equal kick player
    if (memcmp(buff.contents() + 1, validHash, sizeof(validHash)) != 0)
    {
        logger.info(
            "%s failed a request hash reply. Actions taken: Kicked Player.",
            session_->GetPlayerInfo().c_str());
        session_->KickPlayer();
        return;
    }

    LOG_DEBUG(logging, "Request hash reply: succeed");

    // client 7F96EEFDA5B63D20A4DF8E00CBF48304
    const uint8 client_key[16] = {0x7F, 0x96, 0xEE, 0xFD, 0xA5, 0xB6, 0x3D,
        0x20, 0xA4, 0xDF, 0x8E, 0x00, 0xCB, 0xF4, 0x83, 0x04};

    // server C2B7ADEDFCCCA9C2BFB3F85602BA809B
    const uint8 server_key[16] = {0xC2, 0xB7, 0xAD, 0xED, 0xFC, 0xCC, 0xA9,
        0xC2, 0xBF, 0xB3, 0xF8, 0x56, 0x02, 0xBA, 0x80, 0x9B};

    // change keys here
    memcpy(InputKey, client_key, 16);
    memcpy(OutputKey, server_key, 16);

    iCrypto.Init(InputKey);
    oCrypto.Init(OutputKey);

    m_initialized = true;
}

void WardenWin::RequestData()
{
    LOG_DEBUG(logging, "Request data");

    // Fill normal checks
    if (pending_checks_.empty())
        pending_checks_.assign(WardenDataStorage.static_checks.begin(),
            WardenDataStorage.static_checks.end());

    // Use slow checks if the timer has expired, otherwise use normal checks
    uint32 slow_delay =
        sWorld::Instance()->getConfig(CONFIG_UINT32_WARDEN_SLOW_CHECKS_DELAY);
    bool use_slow_checks = slow_delay != 0 && slow_check_delay_timer_ == 0;

    if (use_slow_checks && pending_slow_checks_.empty())
        pending_slow_checks_.assign(
            WardenDataStorage.static_slow_checks.begin(),
            WardenDataStorage.static_slow_checks.end());

    // If the config is incorrectly configured and we don't have any
    // slow-checks, we use normal checks
    if (pending_slow_checks_.empty())
        use_slow_checks = false;

    // Fill dynamic checks
    fill_pending_dyn_checks();

    // ServerTicks = WorldTimer::getMSTime();

    current_checks_.clear();

    for (uint32 i = 0;
         i < sWorld::Instance()->getConfig(CONFIG_UINT32_WARDEN_NUM_CHECKS);
         ++i)
    {
        if (use_slow_checks)
        {
            // Slow checks
            if (pending_slow_checks_.empty())
                break;

            current_checks_.push_back(
                check_identifier(pending_slow_checks_.back(), false));
            pending_slow_checks_.pop_back();
        }
        else
        {
            // Normal checks
            if (pending_checks_.empty())
                break;

            current_checks_.push_back(
                check_identifier(pending_checks_.back(), false));
            pending_checks_.pop_back();
        }
    }

    // Only include dynamic checks for non slow check packets
    if (!use_slow_checks)
        build_dynamic_checks();

    ByteBuffer buff;
    buff << uint8(WARDEN_SMSG_CHEAT_CHECKS_REQUEST);

    /* This doesn't apply for 2.4.3
    for (int i = 0; i < 5; ++i)                             // for now include 5
    random checks
    {
        // TODO: This code is no longer relevant and needs to be rewritten if
    used!
        id = irand(1, maxid - 1);
        wd = WardenDataStorage.GetWardenDataById(id);
        SendDataId.push_back(id);
        switch (wd->Type)
        {
            case MPQ_CHECK:
            case LUA_STR_CHECK:
            case DRIVER_CHECK:
                buff << uint8(wd->str.size());
                buff.append(wd->str.c_str(), wd->str.size());
                break;
            default:
                break;
        }
    }*/

    uint8 xorByte = InputKey[0];

    buff << uint8(0x00);
    buff << uint8(TIMING_CHECK ^ xorByte); // check TIMING_CHECK

    uint8 index = 1;

    for (auto& elem : current_checks_)
    {
        const WardenData* wd;
        if (elem.dynamic)
            wd = get_dynamic_check(elem.id);
        else
            wd = WardenDataStorage.GetWardenDataById(elem.id);
        assert(wd);

        uint8 type = wd->Type;
        buff << uint8(type ^ xorByte);
        switch (type)
        {
        case MEM_CHECK:
        {
            buff << uint8(0x00);
            buff << uint32(wd->Address);
            buff << uint8(wd->Length);
            break;
        }
        case PAGE_CHECK_A:
        case PAGE_CHECK_B:
        {
            // FIXME: This check is not working fully yet.
            // It works if your data is nothing but zero bytes,
            // but otherwise it doesn't. Might be an indication
            // that the data is xor'd, for example? It needs more
            // reverersing to determine the cause.
            uint32 seed = static_cast<uint32>(rand32());
            buff << uint32(seed);
            HmacHash hmac(4, (uint8*)&seed);
            hmac.UpdateData(&wd->Data[0], wd->Data.size());
            hmac.Finalize();
            buff.append(hmac.GetDigest(), hmac.GetLength());
            buff << uint32(wd->Address);
            buff << uint8(wd->Data.size());
            break;
        }
        case MPQ_CHECK:
        case LUA_STR_CHECK:
        {
            buff << uint8(index++);
            break;
        }
        case DRIVER_CHECK:
        {
            buff.append(&wd->Data[0], wd->Data.size());
            buff << uint8(index++);
            break;
        }
        case MODULE_CHECK:
        {
            uint32 seed = static_cast<uint32>(rand32());
            buff << uint32(seed);
            HmacHash hmac(4, (uint8*)&seed);
            hmac.UpdateData(wd->str);
            hmac.Finalize();
            buff.append(hmac.GetDigest(), hmac.GetLength());
            break;
        }
        /*case PROC_CHECK:
        {
            buff.append(&wd->Data[0], wd->Data.size());
            buff << uint8(index++);
            buff << uint8(index++);
            buff << uint32(wd->Address);
            buff << uint8(wd->Length);
            break;
        }*/
        default:
            break; // should never happen
        }
    }
    buff << uint8(xorByte);
    buff.hexlike();

    // Encrypt with warden RC4 key.
    EncryptData(const_cast<uint8*>(buff.contents()), buff.size());

    WorldPacket pkt(SMSG_WARDEN_DATA, buff.size());
    pkt.append(buff);
    session_->send_packet(std::move(pkt));

    m_WardenDataSent = true;
    if (use_slow_checks)
    {
        slow_check_delay_timer_ = slow_delay * IN_MILLISECONDS;
        LOG_DEBUG(logging,
            "Warden: Sent slow checks. Time until next slow checks: %u "
            "seconds.",
            slow_check_delay_timer_);
    }

    if (logging.get_logger().get_level() == LogLevel::debug)
    {
        std::stringstream stream;
        stream << "Sent check ids: ";
        for (auto& elem : current_checks_)
            stream << elem << " ";
        LOG_DEBUG(logging, "%s", stream.str().c_str());
    }
}

bool WardenWin::assert_enough_data_left(ByteBuffer& buff, uint32 size)
{
    if (buff.rpos() + size > buff.size())
    {
        logger.info(
            "%s sent a packet with invalid size indicating packet tampering. "
            "Action taken: Kicked Player.",
            session_->GetPlayerInfo().c_str());
        session_->KickPlayer();
        buff.rpos(buff.size());
        return false;
    }
    return true;
}

void WardenWin::HandleData(ByteBuffer& buff)
{
    LOG_DEBUG(logging, "Handle data");

    m_WardenDataSent = false;
    m_WardenKickTimer = 0;

    uint16 Length;
    buff >> Length;
    uint32 Checksum;
    buff >> Checksum;

    if (!IsValidCheckSum(Checksum, buff.contents() + buff.rpos(), Length))
    {
        buff.rpos(buff.wpos());
        logger.info(
            "%s sent a reponse with an invalid check sum. Actions taken: "
            "Kicked Player.",
            session_->GetPlayerInfo().c_str());
        session_->KickPlayer();
        return;
    }

    bool failed_timing_check = false;

    // TIMING_CHECK
    {
        uint8 result;
        buff >> result;

        if (result == 0x00)
        {
            failed_timing_check = true;
        }

        uint32 newClientTicks;
        buff >> newClientTicks;

        // FIXME: Timing Check needs to be investigated further
        /*uint32 ticksNow = WorldTimer::getMSTime();
        uint32 ourTicks = newClientTicks + (ticksNow - ServerTicks);

        LOG_DEBUG(logging,"ServerTicks %u", ticksNow);         // now
        LOG_DEBUG(logging,"RequestTicks %u", ServerTicks);     // at request
        LOG_DEBUG(logging,"Ticks %u", newClientTicks);         // at response
        LOG_DEBUG(logging,"Ticks diff %u", ourTicks - newClientTicks);*/
    }

    std::vector<check_identifier> failed_checks;

    for (auto itr = current_checks_.begin(); itr != current_checks_.end();
         ++itr)
    {
        const WardenData* rd;
        const WardenDataResult* rs;
        if (itr->dynamic)
        {
            rd = get_dynamic_check(itr->id);

            // Skip the checking of results for warden checks that do not have a
            // given result
            if (should_ignore_result(rd, buff, failed_checks))
                continue;

            // Check the dynamic result
            if (!check_dynamic_result(rd, buff))
            {
                LOG_DEBUG(logging,
                    "RESULT DYNAMIC_CHECK fail CheckId %s account Id %u",
                    itr->printable().c_str(), session_->GetAccountId());
                failed_checks.push_back(*itr);
            }
            else
            {
                LOG_DEBUG(logging,
                    "RESULT DYNAMIC_CHECK passed CheckId %s account Id %u",
                    itr->printable().c_str(), session_->GetAccountId());
            }
            // Don't continue processing for dynamic checks
            continue;
        }
        else
        {
            rd = WardenDataStorage.GetWardenDataById(itr->id);
            rs = WardenDataStorage.GetWardenResultById(itr->id);
        }

        assert(rd);

        uint8 type = rd->Type;
        switch (type)
        {
        case MEM_CHECK:
        {
            assert(rs);

            if (!assert_enough_data_left(buff, 1 + rd->Length))
                return;

            uint8 Mem_Result;
            buff >> Mem_Result;

            if (Mem_Result != 0)
            {
                LOG_DEBUG(logging,
                    "RESULT MEM_CHECK not 0x00, CheckId %s account Id %u",
                    itr->printable().c_str(), session_->GetAccountId());
                failed_checks.push_back(*itr);
                continue;
            }

            // XXX: Do we need to verify that the size sent from the client is
            // actually the size we expect?
            if (memcmp(buff.contents() + buff.rpos(), &rs->res[0],
                    rd->Length) != 0)
            {
                LOG_DEBUG(logging,
                    "RESULT MEM_CHECK fail CheckId %s account Id %u",
                    itr->printable().c_str(), session_->GetAccountId());
                failed_checks.push_back(*itr);
                buff.rpos(buff.rpos() + rd->Length);
                continue;
            }

            buff.rpos(buff.rpos() + rd->Length);
            LOG_DEBUG(logging,
                "RESULT MEM_CHECK passed CheckId %s account Id %u",
                itr->printable().c_str(), session_->GetAccountId());
            break;
        }
        case PAGE_CHECK_A:
        case PAGE_CHECK_B:
        case DRIVER_CHECK:
        case MODULE_CHECK:
        {
            if (!assert_enough_data_left(buff, 1))
                return;

            uint8 byte;
            buff >> byte;

            static const uint8 not_found = 0xE9;
            if (byte != not_found)
            {
                LOG_DEBUG(logging, "%s failed, CheckId %s account Id %u",
                    (type == PAGE_CHECK_A ?
                                  "PAGE_CHECK_A" :
                                  type == PAGE_CHECK_B ?
                                  "PAGE_CHECK B" :
                                  type == MODULE_CHECK ?
                                  "MODULE_CHECK" :
                                  type == DRIVER_CHECK ? "DRIVER_CHECK" : ""),
                    itr->printable().c_str(), session_->GetAccountId());
                failed_checks.push_back(*itr);
                continue;
            }

            LOG_DEBUG(logging, "%s passed, CheckId %s account Id %u",
                (type == PAGE_CHECK_A ? "PAGE_CHECK_A" : type == PAGE_CHECK_B ?
                                        "PAGE_CHECK B" :
                                        type == MODULE_CHECK ?
                                        "MODULE_CHECK" :
                                        type == DRIVER_CHECK ? "DRIVER_CHECK" :
                                                               ""),
                itr->printable().c_str(), session_->GetAccountId());
            break;
        }
        case LUA_STR_CHECK:
        {
            uint8 Lua_Result;
            buff >> Lua_Result;

            if (Lua_Result != 0)
            {
                LOG_DEBUG(logging,
                    "RESULT LUA_STR_CHECK fail, CheckId %s account Id %u",
                    itr->printable().c_str(), session_->GetAccountId());
                failed_checks.push_back(*itr);
                continue;
            }

            uint8 luaStrLen;
            buff >> luaStrLen;

            if (luaStrLen != 0)
            {
                auto str = new char[luaStrLen + 1];
                memset(str, 0, luaStrLen + 1);
                memcpy(str, buff.contents() + buff.rpos(), luaStrLen);
                LOG_DEBUG(logging, "Lua string: %s", str);
                delete[] str;
            }
            buff.rpos(buff.rpos() + luaStrLen); // skip string
            LOG_DEBUG(logging,
                "RESULT LUA_STR_CHECK passed, CheckId %s account Id %u",
                itr->printable().c_str(), session_->GetAccountId());
            break;
        }
        case MPQ_CHECK:
        {
            uint8 Mpq_Result;
            buff >> Mpq_Result;

            if (Mpq_Result != 0)
            {
                LOG_DEBUG(logging, "RESULT MPQ_CHECK not 0x00 account id %u",
                    session_->GetAccountId());
                failed_checks.push_back(*itr);
                continue;
            }

            // XXX: Do we need to verify that the size sent from the client is
            // actually the size we expect?
            if (memcmp(buff.contents() + buff.rpos(), &rs->res[0], 20) !=
                0) // SHA1
            {
                LOG_DEBUG(logging,
                    "RESULT MPQ_CHECK fail, CheckId %s account Id %u",
                    itr->printable().c_str(), session_->GetAccountId());
                failed_checks.push_back(*itr);
                buff.rpos(buff.rpos() + 20); // 20 bytes SHA1
                continue;
            }

            buff.rpos(buff.rpos() + 20); // 20 bytes SHA1
            LOG_DEBUG(logging,
                "RESULT MPQ_CHECK passed, CheckId %s account Id %u",
                itr->printable().c_str(), session_->GetAccountId());
            break;
        }
        default: // should never happens
            break;
        }
    }

    if (failed_timing_check || failed_checks.size() > 0)
    {
        punish(failed_checks, failed_timing_check);
    }
}

void WardenWin::fill_pending_dyn_checks()
{
    if (!pending_dyn_checks_.empty())
        return;

    if (sWorld::Instance()->getConfig(CONFIG_UINT32_WARDEN_NUM_DYN_CHECKS) == 0)
        return;

    pending_dyn_checks_.reserve(WARDEN_DYN_MAX_CHECKS - 1);

    pending_dyn_checks_.push_back(WARDEN_DYN_CHECK_SPEEDS);
    pending_dyn_checks_.push_back(WARDEN_DYN_CHECK_TRACKING);
    pending_dyn_checks_.push_back(WARDEN_DYN_CHECK_MOVEMENT_STATE);
}

void WardenWin::build_dynamic_checks()
{
    uint32 max_checks =
        sWorld::Instance()->getConfig(CONFIG_UINT32_WARDEN_NUM_DYN_CHECKS);
    if (max_checks == 0)
        return;

    if (!session_->GetPlayer() || !session_->GetPlayer()->IsInWorld())
        return;

    // Send nothing but a request for our player base if it's not valid
    if (!player_base_)
    {
        current_checks_.push_back(
            check_identifier(WARDEN_DYN_CHECK_PLAYER_BASE, true));
        return;
    }

    for (uint32 i = 0; i < max_checks; ++i)
    {
        if (pending_dyn_checks_.empty())
            break;

        uint32 id = pending_dyn_checks_.back();
        current_checks_.push_back(check_identifier(id, true));
        pending_dyn_checks_.pop_back();

        valid_dynamic_checks_[id - 1] = true;
    }
}

const WardenData* WardenWin::get_dynamic_check(uint32 id)
{
    assert(id < WARDEN_DYN_MAX_CHECKS);

    check_.id = id;
    switch (check_.id)
    {
    case WARDEN_DYN_CHECK_PLAYER_BASE:
        check_.Type = MEM_CHECK;
        check_.Address = PLAYER_BASE_STATIC_ADDRESS;
        check_.Length = 4;
        break;
    case WARDEN_DYN_CHECK_SPEEDS:
        check_.Type = MEM_CHECK;
        check_.Address = player_base_ + SPEED_START_OFFSET;
        check_.Length = SPEED_LENGTH;
        check_.action = WARDEN_ACTION_BAN;
        break;
    case WARDEN_DYN_CHECK_TRACKING:
        check_.Type = MEM_CHECK;
        check_.Address = player_base_ + TRACKING_OFFSET;
        check_.Length = TRACKING_LENGTH;
        check_.action = WARDEN_ACTION_BAN_WAVE;
        break;
    case WARDEN_DYN_CHECK_MOVEMENT_STATE:
        check_.Type = MEM_CHECK;
        check_.Address = player_base_ + MOVEMENT_STATE_OFFSET;
        check_.Length = 1;
        check_.action = WARDEN_ACTION_BAN_WAVE;
        break;
    }

    return &check_;
}

static inline bool float_matches(float f1, ByteBuffer& buff)
{
    static const float epsilon = 0.01f;
    float f2;
    buff >> f2;
    return f1 - epsilon < f2 && f1 + epsilon > f2;
}

bool WardenWin::check_dynamic_result(const WardenData* check, ByteBuffer& buff)
{
    assert(check->id < WARDEN_DYN_MAX_CHECKS);

    // Dynamic checks are only MEM_CHECKS
    if (check->Type != MEM_CHECK)
        return true; // Don't take action in this case

    if (!assert_enough_data_left(buff, 1 + check->Length))
        return true; // Don't take action in this case

    uint8 mem_result;
    buff >> mem_result;
    if (mem_result != 0)
        return false;

    if (valid_dynamic_checks_[check->id - 1] == false)
    {
        logger.info(
            "DynCheck: %s had his dynamic check (id: %u) skipped because his "
            "state for that specific check became invalid. "
            "If this happens repeatedly it could be someone who figured out "
            "how to bypass dynamic checks.",
            session_->GetPlayerInfo().c_str(), check->id);
        return true; // Don't take action in this case
    }

    Player* player = session_->GetPlayer();
    if (!player)
        return true; // Don't take action in this case

    switch (check->id)
    {
    case WARDEN_DYN_CHECK_SPEEDS:
    {
        if (!float_matches(player->GetSpeed(MOVE_WALK), buff))
            return false;
        if (!float_matches(player->GetSpeed(MOVE_RUN), buff))
            return false;
        if (!float_matches(player->GetSpeed(MOVE_RUN_BACK), buff))
            return false;
        if (!float_matches(player->GetSpeed(MOVE_SWIM), buff))
            return false;
        if (!float_matches(player->GetSpeed(MOVE_SWIM_BACK), buff))
            return false;
        if (!float_matches(player->GetSpeed(MOVE_FLIGHT), buff))
            return false;
        if (!float_matches(player->GetSpeed(MOVE_FLIGHT_BACK), buff))
            return false;
        break;
    }
    case WARDEN_DYN_CHECK_TRACKING:
    {
        uint32 creature_mask = player->GetUInt32Value(PLAYER_TRACK_CREATURES);
        uint32 resource_mask = player->GetUInt32Value(PLAYER_TRACK_RESOURCES);

        uint32 client_creature_mask, client_resource_mask;
        buff >> client_creature_mask;
        buff >> client_resource_mask;

        if (creature_mask != client_creature_mask ||
            resource_mask != client_resource_mask)
            return false;
        break;
    }
    case WARDEN_DYN_CHECK_MOVEMENT_STATE: // This check targets wowemuhacker in
                                          // particular
        {
            // Bit meanings:
            //  8 |  7 |  6 | 5  | 4  |  3 |  2 |  1
            // ---------------------------------------
            // ?? | HO | SF | WW | ?? | ?? | FL | AF
            // The 8th bit is always turned on if the value is set by mangos
            // and non-modified. When wowemuhacker modifies it it overwrites
            // that bit and sets it to 0.
            uint8 movement_flag;
            buff >> movement_flag;
            if ((movement_flag & (1 << 7)) == 0)
                return false;
            break;
        }
    }

    return true;
}

bool WardenWin::should_ignore_result(const WardenData* check, ByteBuffer& buff,
    std::vector<check_identifier>& failed_checks)
{
    switch (check->id)
    {
    case WARDEN_DYN_CHECK_PLAYER_BASE:
    {
        uint8 mem_result;
        buff >> mem_result;
        if (mem_result != 0)
        {
            LOG_DEBUG(logging,
                "RESULT MEM_CHECK not 0x00, CheckId dyn:%u account Id %u",
                WARDEN_DYN_CHECK_PLAYER_BASE, session_->GetAccountId());
            failed_checks.push_back(
                check_identifier(WARDEN_DYN_CHECK_PLAYER_BASE, true));
            return true;
        }

        buff >> player_base_;
        LOG_DEBUG(logging,
            "Warden: Retrieved player base from client. Address: 0x%08X.",
            player_base_);

        if (player_base_ == 0)
        {
            session_->KickPlayer();
            logger.info(
                "%s sent 0 as player base. Action taken: Kicked Player.",
                session_->GetPlayerInfo().c_str());
        }

        return true;
    }
    default:
        break;
    }
    return false;
}

void WardenWin::invalidate_dynamic(WardenDynamicCheck check)
{
    if (check == WARDEN_DYN_CHECK_PLAYER_BASE)
    {
        // Invalidate all dynamic checks when the base pointer is invalidated
        player_base_ = 0;
        memset(valid_dynamic_checks_, 0, sizeof(valid_dynamic_checks_));
    }
    else if (check < WARDEN_DYN_MAX_CHECKS - 1)
        valid_dynamic_checks_[check - 1] = false;
}
