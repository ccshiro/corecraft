/*
 * Copyright (C) 2005-2012 MaNGOS <http://getmangos.com/>
 * Copyright (C) 2013 Corecraft <https://www.worldofcorecraft.com/>
 * Copyright (C) 2013 TrinityCore <http://www.trinitycore.org/>
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

#include "AuthCodes.h"
#include "Common.h"
#include "logging.h"
#include "RealmList.h"
#include "connection.h"
#include "totp.h"
#include "Auth/Sha1.h"
#include "Config/Config.h"
#include "Database/DatabaseEnv.h"
#include <openssl/md5.h>
#include <utility>

enum AccountFlags
{
    ACCOUNT_FLAG_GM = 0x00000001,
    ACCOUNT_FLAG_TRIAL = 0x00000008,
    ACCOUNT_FLAG_PROPASS = 0x00800000,
};

void connection::init_srp6()
{
    // FIXME: N and g should really be static constants, but that requires
    // change in the BigNumber
    // library, as that library modifies the internal state inside its read
    // functions.
    N_.SetHexStr(
        "894B645E89E1535BBDAD5B8B290650530801B18EBFBF5E8FAB3C82872A3E9BB7");
    g_.SetDword(7);
}

void connection::handle_logon_challenge()
{
    if (!process_challenge())
        return;

    output_buffer_.clear();
    output_buffer_.reserve(128);
    output_buffer_ << uint8(CMD_AUTH_LOGON_CHALLENGE) << uint8(0);

    std::unique_ptr<QueryResult> result(LoginDatabase.PQuery(
        "SELECT sha_pass_hash, id, locked, last_ip, gmlevel, v, s, tokenkey "
        "FROM account WHERE username = '%s'",
        escaped_username_.c_str()));
    if (!result)
    {
        LOG_DEBUG(logging,
            "[AuthChallenge] User with ip %s tried to login with an invalid "
            "username: %s.",
            escaped_ip_address_.c_str(), escaped_username_.c_str());
        output_buffer_ << uint8(WOW_FAIL_UNKNOWN_ACCOUNT);
        return send();
    }

    account_id_ = (*result)[1].GetUInt32();
    token_key_ = std::move((*result)[7].GetCppString());
    last_ip_ = (*result)[3].GetString();

    // Check if account is locked to a certain IP address
    if ((*result)[2].GetUInt8() == 1 && last_ip_ != escaped_ip_address_)
    {
        LOG_DEBUG(logging,
            "[AuthChallenge] User %s has chosen to lock his account to an IP "
            "address (%s), but the attempted login comes from another IP "
            "address (%s).",
            escaped_username_.c_str(), (*result)[3].GetString(),
            escaped_ip_address_.c_str());
        output_buffer_ << uint8(WOW_FAIL_SUSPENDED);
        return send();
    }

    // Check if account is banned or suspended
    std::unique_ptr<QueryResult> ban_result(LoginDatabase.PQuery(
        "SELECT bandate, unbandate FROM account_banned WHERE "
        "id = %u AND active = 1 AND (unbandate > UNIX_TIMESTAMP() OR unbandate "
        "= bandate)",
        (*result)[1].GetUInt32()));
    if (ban_result)
    {
        bool permanent =
            (*ban_result)[0].GetUInt64() == (*ban_result)[1].GetUInt64();
        if (permanent)
            output_buffer_ << uint8(WOW_FAIL_BANNED);
        else
            output_buffer_ << uint8(WOW_FAIL_SUSPENDED);
        LOG_DEBUG(logging,
            "[AuthChallenge] %s tried to login to a %s account (%s)",
            escaped_ip_address_.c_str(), (permanent ? "banned" : "suspended"),
            escaped_username_.c_str());
        return send();
    }

    logon_srp6(result.get());

    state_ = STATE_WAITING_FOR_LOGON_PROOF;
    return send();
}

void connection::handle_reconnect_challenge()
{
    if (!process_challenge())
        return;

    std::unique_ptr<QueryResult> result(LoginDatabase.PQuery(
        "SELECT id, sessionkey FROM account WHERE username = '%s'",
        escaped_username_.c_str()));
    if (!result)
    {
        LOG_DEBUG(logging,
            "User with IP-address '%s' tried to reconnect to an invalid "
            "account.",
            escaped_username_.c_str());
        stop();
        return;
    }

    account_id_ = (*result)[0].GetUInt32();

    K_.SetHexStr((*result)[1].GetString());

    // Send response
    output_buffer_.clear();
    output_buffer_.reserve(64);

    output_buffer_ << uint8(CMD_AUTH_RECONNECT_CHALLENGE) << uint8(0);
    // 16 random bytes
    reconnect_rand_bytes_.SetRand(16 * 8);
    output_buffer_.append(reconnect_rand_bytes_.AsByteArray(), 16);
    // 16 zero bytes
    output_buffer_ << uint64(0) << uint64(0);

    state_ = STATE_WAITING_FOR_RECONNECT_PROOF;
    send();
}

bool connection::process_challenge()
{
    EndianConvert(logon_challenge_.game);
    EndianConvert(logon_challenge_.version.build);
    EndianConvert(logon_challenge_.platform);
    EndianConvert(logon_challenge_.os);
    EndianConvert(logon_challenge_.country);
    EndianConvert(logon_challenge_.timezone);
    EndianConvert(logon_challenge_.ip);

    build_ = logon_challenge_.version.build;

    escaped_username_ = username_;
    LoginDatabase.escape_string(escaped_username_);

    // FIXME: ipv6
    escaped_ip_address_ = remote_address();
    LoginDatabase.escape_string(escaped_ip_address_);

    switch (logon_challenge_.os)
    {
    case 0x57696E: // Win
        os_ = "Win";
        break;
    case 0x4F5358: // OSX
        os_ = "OSX";
        break;
    default:
        LOG_DEBUG(logging,
            "[AuthChallenge] Account %s supplied invalid OS. (Supplied value: "
            "0x%08X)",
            escaped_username_.c_str(), logon_challenge_.os);
        return false;
    }

    // Check if the build number is an accepted build
    // NOTE: We discard the patch transfer funcionality that mangos' realmd had
    if (FindBuildInfo(logon_challenge_.version.build) == NULL)
    {
        output_buffer_ << uint8(WOW_FAIL_VERSION_INVALID);
        LOG_DEBUG(logging,
            "[AuthChallenge] User with ip '%s' tried to login with an invalid "
            "build version",
            escaped_ip_address_.c_str());
        send();
        return false;
    }

    std::unique_ptr<QueryResult> result(LoginDatabase.PQuery(
        "SELECT unbandate FROM ip_banned WHERE "
        "(unbandate = bandate OR unbandate > UNIX_TIMESTAMP()) AND ip = '%s'",
        escaped_ip_address_.c_str()));
    if (result)
    {
        output_buffer_ << uint8(WOW_FAIL_BANNED);
        LOG_DEBUG(logging, "[AuthChallenge] Banned ip %s tries to login!",
            escaped_ip_address_.c_str());
        send();
        return false;
    }

    return true;
}

void connection::logon_srp6(QueryResult* result)
{
    std::string sha_pass_hash = (*result)[0].GetCppString();
    std::string db_v = (*result)[5].GetCppString();
    std::string db_s = (*result)[6].GetCppString();

    // Check if we need to calculate new v and s (multiply by 2, bytes are
    // stored as a hex-string)
    if (db_v.size() != 2 * V_S_BYTE_SIZE || db_s.size() != 2 * V_S_BYTE_SIZE)
    {
        calc_v_s(sha_pass_hash);
    }
    else
    {
        v_.SetHexStr(db_v.c_str());
        s_.SetHexStr(db_s.c_str());
    }

    b_.SetRand(19 * 8);
    BigNumber gmod = g_.ModExp(b_, N_);
    B_ = ((v_ * 3) + gmod) % N_;

    assert(gmod.GetNumBytes() <= 32);

    BigNumber unk3;
    unk3.SetRand(16 * 8);

    output_buffer_ << uint8(WOW_SUCCESS);

    // B may be calculated < 32B so we force minimal length to 32B
    output_buffer_.append(B_.AsByteArray(32), 32);
    output_buffer_ << uint8(1);
    output_buffer_.append(g_.AsByteArray(), 1);
    output_buffer_ << uint8(32);
    output_buffer_.append(N_.AsByteArray(32), 32);
    output_buffer_.append(s_.AsByteArray(), s_.GetNumBytes()); // 32 bytes
    output_buffer_.append(unk3.AsByteArray(16), 16);

    // TODO: Security flags are unused at the moment
    if (!token_key_.empty() && escaped_ip_address_ != last_ip_)
        security_flags_ |= SECURITY_TOTP;

    output_buffer_ << uint8(security_flags_); // security flags (0x0...0x04)

    if (security_flags_ & SECURITY_PIN) // PIN input
    {
        output_buffer_ << uint32(0);
        output_buffer_ << uint64(0) << uint64(0); // 16 bytes hash?
    }

    if (security_flags_ & SECURITY_MATRIX) // Matrix input
    {
        output_buffer_ << uint8(0);
        output_buffer_ << uint8(0);
        output_buffer_ << uint8(0);
        output_buffer_ << uint8(0);
        output_buffer_ << uint64(0);
    }

    if (security_flags_ & SECURITY_TOTP) // Security token input
    {
        output_buffer_ << uint8(1);
    }

    uint8 security_level = (*result)[4].GetUInt8();
    account_security_level_ = security_level <= SEC_FULL_GM ?
                                  static_cast<AccountTypes>(security_level) :
                                  SEC_FULL_GM;

    localization_name_.clear();
    for (int i = 3; i >= 0; --i)
        localization_name_ += ((unsigned char*)&logon_challenge_.country)[i];

    LOG_DEBUG(logging, "[AuthChallenge] Account %s is using '%s' locale (%u)",
        escaped_username_.c_str(), localization_name_.c_str(),
        GetLocaleByName(localization_name_));
}

void connection::calc_v_s(const std::string& sha_pass_hash)
{
    s_.SetRand(V_S_BYTE_SIZE * 8);

    BigNumber I;
    I.SetHexStr(sha_pass_hash.c_str());

    // In case of leading zeros in the sha pass hash, restore them
    uint8 mDigest[SHA_DIGEST_LENGTH];
    memset(mDigest, 0, SHA_DIGEST_LENGTH);
    if (I.GetNumBytes() <= SHA_DIGEST_LENGTH)
        memcpy(mDigest, I.AsByteArray(), I.GetNumBytes());

    std::reverse(mDigest, mDigest + SHA_DIGEST_LENGTH);

    Sha1Hash sha;
    sha.UpdateData(s_.AsByteArray(), s_.GetNumBytes());
    sha.UpdateData(mDigest, SHA_DIGEST_LENGTH);
    sha.Finalize();
    BigNumber x;
    x.SetBinary(sha.GetDigest(), sha.GetLength());
    v_ = g_.ModExp(x, N_);

    const char* v_hex, *s_hex;
    v_hex = v_.AsHexStr();
    s_hex = s_.AsHexStr();
    LoginDatabase.PExecute(
        "UPDATE account SET v = '%s', s = '%s' WHERE username = '%s'", v_hex,
        s_hex, escaped_username_.c_str());
    OPENSSL_free((void*)v_hex);
    OPENSSL_free((void*)s_hex);
}

void connection::handle_logon_proof()
{
    // NOTE: We have discarded the patch tranasfer functionality that mangos'
    // realmd had

    output_buffer_.clear();
    output_buffer_.reserve(32);

    Sha1Hash sha;

    if (proof_srp6(sha) && verify_token())
    {
        LOG_DEBUG(logging, "User %s was successfully authenticated.",
            escaped_username_.c_str());

        // FIXME: Ipv6
        const char* K_hex = K_.AsHexStr();
        LoginDatabase.DirectPExecute(
            "UPDATE account SET sessionkey = '%s', last_ip = '%s', last_login "
            "= NOW(), locale = '%u', os = '%s', failed_logins = 0 WHERE "
            "username = '%s'",
            K_hex, escaped_ip_address_.c_str(),
            GetLocaleByName(localization_name_), os_.c_str(),
            escaped_username_.c_str());
        OPENSSL_free((void*)K_hex);

        state_ = STATE_AUTHENTICATED;

        // Send the SRP6 proof of the shared secret to the client
        if (build_ > 6005) // > 1.12.2
        {
            output_buffer_ << uint8(CMD_AUTH_LOGON_PROOF) << uint8(0);
            output_buffer_.append(sha.GetDigest(), sha.GetLength());
            output_buffer_ << uint32(ACCOUNT_FLAG_PROPASS) << uint32(0)
                           << uint16(0);
            send();
        }
        else
        {
            output_buffer_ << uint8(CMD_AUTH_LOGON_PROOF) << uint8(0);
            output_buffer_.append(sha.GetDigest(), sha.GetLength());
            output_buffer_ << uint32(0);
            send();
        }
    }
    else
    {
        if (build_ > 6005) // > 1.12.2
        {
            output_buffer_ << uint8(CMD_AUTH_LOGON_PROOF)
                           << uint8(WOW_FAIL_UNKNOWN_ACCOUNT) << uint8(3)
                           << uint8(0);
            send();
        }
        else
        {
            output_buffer_ << uint8(CMD_AUTH_LOGON_PROOF)
                           << uint8(WOW_FAIL_UNKNOWN_ACCOUNT);
            send();
        }

        // Handle failed login attempts (FIXME: Needs rewrite, see comment
        // below)

        // Increment failed login attempts
        LoginDatabase.PExecute(
            "UPDATE account SET failed_logins = failed_logins + 1 WHERE "
            "username = '%s'",
            escaped_username_.c_str());

        uint32 max_failed_logins =
            sConfig::Instance()->GetIntDefault("WrongPass.MaxCount", 0);
        if (max_failed_logins == 0)
            return;

        std::unique_ptr<QueryResult> failed_res(LoginDatabase.PQuery(
            "SELECT failed_logins FROM account WHERE username = '%s'",
            escaped_username_.c_str()));
        if (!failed_res)
            return;

        uint32 failed_logins = (*failed_res)[0].GetUInt32();
        if (max_failed_logins > failed_logins)
            return;

        uint32 ban_time =
            sConfig::Instance()->GetIntDefault("WrongPass.BanTime", 600);

        // FIXME: This makes zero fucking sense. Failed logins should be per
        // connection and not associated to an account.

        // FIXME: Ipv6
        LoginDatabase.PExecute(
            "INSERT INTO ip_banned VALUES ('%s', UNIX_TIMESTAMP(), "
            "UNIX_TIMESTAMP()+'%u', 'MaNGOS realmd', 'Failed login autoban')",
            escaped_ip_address_.c_str(), ban_time);
        LOG_DEBUG(logging,
            "[AuthChallenge] IP %s got banned for '%u' seconds because account "
            "%s failed to authenticate '%u' times",
            escaped_ip_address_.c_str(), ban_time, escaped_username_.c_str(),
            failed_logins);
    }
}

bool connection::verify_token()
{
    if (!(security_flags_ & SECURITY_TOTP))
        return true;

    /* Generate 3 tokens, one from 30 seconds ago (1 key ago), one for now, and
     * one in another 30 seconds (1 key forward).
     * Might seem unnecessary, but anecdotal evidence suggests it is needed as
     * the time is divided by 30 in ::GenerateToken(). */

    unsigned int incoming_token = atoi(client_token_);

    bool success = incoming_token == TOTP::GenerateToken(token_key_.c_str()) ||
                   incoming_token == TOTP::GenerateToken(
                                         token_key_.c_str(), time(NULL) - 30) ||
                   incoming_token ==
                       TOTP::GenerateToken(token_key_.c_str(), time(NULL) + 30);

    if (!success)
        LOG_DEBUG(logging,
            "[AuthChallenge] User '%s' failed TOTP token validation from IP "
            "address %s (last successful IP: %s).",
            escaped_username_.c_str(), escaped_ip_address_.c_str(),
            last_ip_.c_str());

    return success;
}

bool connection::proof_srp6(Sha1Hash& sha)
{
    BigNumber A;

    A.SetBinary(logon_proof_.A, 32);

    // SRP safeguard: abort if A==0
    if (A.isZero())
    {
        LOG_DEBUG(logging,
            "User with IP '%s' tried to login to account %s using an A of 0",
            escaped_ip_address_.c_str(), escaped_username_.c_str());
        return false;
    }

    sha.UpdateBigNumbers(&A, &B_, NULL);
    sha.Finalize();
    BigNumber u;
    u.SetBinary(sha.GetDigest(), 20);
    BigNumber S = (A * (v_.ModExp(u, N_))).ModExp(b_, N_);

    uint8 t[32];
    uint8 t1[16];
    uint8 vK[40];
    memcpy(t, S.AsByteArray(32), 32);
    for (int i = 0; i < 16; ++i)
    {
        t1[i] = t[i * 2];
    }
    sha.Initialize();
    sha.UpdateData(t1, 16);
    sha.Finalize();
    for (int i = 0; i < 20; ++i)
    {
        vK[i * 2] = sha.GetDigest()[i];
    }
    for (int i = 0; i < 16; ++i)
    {
        t1[i] = t[i * 2 + 1];
    }
    sha.Initialize();
    sha.UpdateData(t1, 16);
    sha.Finalize();
    for (int i = 0; i < 20; ++i)
    {
        vK[i * 2 + 1] = sha.GetDigest()[i];
    }
    K_.SetBinary(vK, 40);

    uint8 hash[20];

    sha.Initialize();
    sha.UpdateBigNumbers(&N_, NULL);
    sha.Finalize();
    memcpy(hash, sha.GetDigest(), 20);
    sha.Initialize();
    sha.UpdateBigNumbers(&g_, NULL);
    sha.Finalize();
    for (int i = 0; i < 20; ++i)
    {
        hash[i] ^= sha.GetDigest()[i];
    }
    BigNumber t3;
    t3.SetBinary(hash, 20);

    sha.Initialize();
    sha.UpdateData(username_);
    sha.Finalize();
    uint8 t4[SHA_DIGEST_LENGTH];
    memcpy(t4, sha.GetDigest(), SHA_DIGEST_LENGTH);

    sha.Initialize();
    sha.UpdateBigNumbers(&t3, NULL);
    sha.UpdateData(t4, SHA_DIGEST_LENGTH);
    sha.UpdateBigNumbers(&s_, &A, &B_, &K_, NULL);
    sha.Finalize();
    BigNumber M;
    M.SetBinary(sha.GetDigest(), 20);

    bool success = memcmp(M.AsByteArray(), logon_proof_.M1, 20) == 0;

    if (success)
    {
        // Finish SRP6
        sha.Initialize();
        sha.UpdateBigNumbers(&A, &M, &K_, NULL);
        sha.Finalize();
    }
    else
    {
        LOG_DEBUG(logging,
            "User with IP '%s' tried to login to account %s using an incorrect "
            "password.",
            escaped_ip_address_.c_str(), escaped_username_.c_str());
    }

    return success;
}

void connection::handle_reconnect_proof()
{
    if (escaped_username_.empty() || !reconnect_rand_bytes_.GetNumBytes() ||
        !K_.GetNumBytes())
    {
        LOG_DEBUG(logging,
            "User with IP-address '%s' provided reconnect proof without the "
            "challenge first.",
            escaped_ip_address_.c_str());
        stop();
        return;
    }

    BigNumber t1;
    t1.SetBinary(reconnect_proof_.R1, 16);

    Sha1Hash sha;
    sha.Initialize();
    sha.UpdateData(username_);
    sha.UpdateBigNumbers(&t1, &reconnect_rand_bytes_, &K_, NULL);
    sha.Finalize();

    if (memcmp(sha.GetDigest(), reconnect_proof_.R2, SHA_DIGEST_LENGTH) == 0)
    {
        // Send success response
        output_buffer_.clear();
        output_buffer_.reserve(4);

        output_buffer_ << uint8(CMD_AUTH_RECONNECT_PROOF) << uint8(0)
                       << uint16(0);
        send();

        state_ = STATE_AUTHENTICATED;
    }
    else
    {
        logging.error(
            "[ERROR] user %s tried to reconnect, but session was invalid.",
            escaped_username_.c_str());
        stop();
    }
}

void connection::handle_realmlist()
{
    // TODO: Mangos does some retarded query here, but I removed it, check what
    // the impact of removing it is
    sRealmList::Instance()->UpdateIfNeed();

    // Create the realmlist packet for this user (it's user specific because
    // each packet holds the # of characters in that realm for this user)

    output_buffer_.clear();
    output_buffer_.reserve(1024);
    output_buffer_ << static_cast<uint8>(CMD_REALM_LIST);
    output_buffer_ << static_cast<uint16>(
        0); // Reserve 2 bytes for size, see below when we rewrite it
    LoadRealmlist(output_buffer_, account_id_);
    output_buffer_.put(
        1, static_cast<uint16>(output_buffer_.size() -
                               3)); // Rewrite the size, subtract the header

    send();
}

void connection::LoadRealmlist(ByteBuffer& pkt, uint32 acctid)
{
    switch (build_)
    {
    case 5875: // 1.12.1
    case 6005: // 1.12.2
    {
        pkt << uint32(0); // unused value
        pkt << uint8(sRealmList::Instance()->size());

        for (RealmList::RealmMap::const_iterator i =
                 sRealmList::Instance()->begin();
             i != sRealmList::Instance()->end(); ++i)
        {
            uint8 AmountOfCharacters;

            // No SQL injection. id of realm is controlled by the database.
            std::unique_ptr<QueryResult> result(LoginDatabase.PQuery(
                "SELECT numchars FROM realmcharacters WHERE realmid = '%d' AND "
                "acctid='%u'",
                i->second.m_ID, acctid));
            if (result)
            {
                Field* fields = result->Fetch();
                AmountOfCharacters = fields[0].GetUInt8();
            }
            else
                AmountOfCharacters = 0;

            bool ok_build = std::find(i->second.realmbuilds.begin(),
                                i->second.realmbuilds.end(),
                                build_) != i->second.realmbuilds.end();

            RealmBuildInfo const* buildInfo =
                ok_build ? FindBuildInfo(build_) : NULL;
            if (!buildInfo)
                buildInfo = &i->second.realmBuildInfo;

            uint8 realmflags = i->second.realmflags & ~REALM_FLAG_SPECIFYBUILD;
            std::string name = i->first;

            // Show offline state for unsupported client builds and locked
            // realms (1.x clients not support locked state show)
            if (!ok_build ||
                (i->second.allowedSecurityLevel > account_security_level_))
                realmflags = RealmFlags(realmflags | REALM_FLAG_OFFLINE);

            pkt << uint32(i->second.icon); // realm type
            pkt << uint8(realmflags);      // realmflags
            pkt << name;                   // name
            pkt << i->second.address;      // address
            pkt << float(i->second.populationLevel);
            pkt << uint8(AmountOfCharacters);
            pkt << uint8(i->second.timezone); // realm category
            pkt << uint8(0x00);               // unk, may be realm number/id?
        }

        pkt << uint16(0x0002); // unused value (why 2?)
        break;
    }

    case 8606:  // 2.4.3
    case 10505: // 3.2.2a
    case 11159: // 3.3.0a
    case 11403: // 3.3.2
    case 11723: // 3.3.3a
    case 12340: // 3.3.5a
    default:    // and later
    {
        pkt << uint32(0); // unused value
        pkt << uint16(sRealmList::Instance()->size());

        for (RealmList::RealmMap::const_iterator i =
                 sRealmList::Instance()->begin();
             i != sRealmList::Instance()->end(); ++i)
        {
            uint8 AmountOfCharacters;

            // No SQL injection. id of realm is controlled by the database.
            std::unique_ptr<QueryResult> result(LoginDatabase.PQuery(
                "SELECT numchars FROM realmcharacters WHERE realmid = '%d' AND "
                "acctid='%u'",
                i->second.m_ID, acctid));
            if (result)
            {
                Field* fields = result->Fetch();
                AmountOfCharacters = fields[0].GetUInt8();
            }
            else
                AmountOfCharacters = 0;

            bool ok_build = std::find(i->second.realmbuilds.begin(),
                                i->second.realmbuilds.end(),
                                build_) != i->second.realmbuilds.end();

            RealmBuildInfo const* buildInfo =
                ok_build ? FindBuildInfo(build_) : NULL;
            if (!buildInfo)
                buildInfo = &i->second.realmBuildInfo;

            uint8 lock =
                (i->second.allowedSecurityLevel > account_security_level_) ? 1 :
                                                                             0;

            RealmFlags realmFlags = i->second.realmflags;

            // Show offline state for unsupported client builds
            if (!ok_build)
                realmFlags = RealmFlags(realmFlags | REALM_FLAG_OFFLINE);

            if (!buildInfo)
                realmFlags = RealmFlags(realmFlags & ~REALM_FLAG_SPECIFYBUILD);

            pkt << uint8(i->second.icon); // realm type (this is second column
                                          // in Cfg_Configs.dbc)
            pkt << uint8(lock);           // flags, if 0x01, then realm locked
            pkt << uint8(realmFlags);     // see enum RealmFlags
            pkt << i->first;              // name
            pkt << i->second.address;     // address
            pkt << float(i->second.populationLevel);
            pkt << uint8(AmountOfCharacters);
            pkt << uint8(
                i->second.timezone); // realm category (Cfg_Categories.dbc)
            pkt << uint8(0x2C);      // unk, may be realm number/id?

            if (realmFlags & REALM_FLAG_SPECIFYBUILD)
            {
                pkt << uint8(buildInfo->major_version);
                pkt << uint8(buildInfo->minor_version);
                pkt << uint8(buildInfo->bugfix_version);
                pkt << uint16(build_);
            }
        }

        pkt << uint16(0x0010); // unused value (why 10?)
        break;
    }
    }
}
