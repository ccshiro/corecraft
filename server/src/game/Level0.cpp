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

#include "AccountMgr.h"
#include "Chat.h"
#include "Common.h"
#include "Language.h"
#include "ObjectAccessor.h"
#include "Opcodes.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SystemConfig.h"
#include "Util.h"
#include "World.h"
#include "Database/DatabaseEnv.h"

//#include "revision.h"
//#include "revision_nr.h"

bool ChatHandler::HandleHelpCommand(char* args)
{
    if (!*args)
    {
        ShowHelpForCommand(getCommandTable(), "help");
        ShowHelpForCommand(getCommandTable(), "");
    }
    else
    {
        if (!ShowHelpForCommand(getCommandTable(), args))
            SendSysMessage(LANG_NO_CMD);
    }

    return true;
}

bool ChatHandler::HandleCommandsCommand(char* /*args*/)
{
    ShowHelpForCommand(getCommandTable(), "");
    return true;
}

bool ChatHandler::HandleAccountCommand(char* args)
{
    // let show subcommands at unexpected data in args
    if (*args)
        return false;

    AccountTypes gmlevel = GetAccessLevel();
    PSendSysMessage(LANG_ACCOUNT_LEVEL, uint32(gmlevel));
    return true;
}

bool ChatHandler::HandleStartCommand(char* /*args*/)
{
    Player* chr = m_session->GetPlayer();

    if (chr->IsTaxiFlying())
    {
        SendSysMessage(LANG_YOU_IN_FLIGHT);
        SetSentErrorMessage(true);
        return false;
    }

    if (chr->isInCombat())
    {
        SendSysMessage(LANG_YOU_IN_COMBAT);
        SetSentErrorMessage(true);
        return false;
    }

    // cast spell Stuck
    chr->CastSpell(chr, 7355, false);
    return true;
}

bool ChatHandler::HandleServerInfoCommand(char* /*args*/)
{
    uint32 activeClientsNum = sWorld::Instance()->GetActiveSessionCount();
    uint32 queuedClientsNum = sWorld::Instance()->GetQueuedSessionCount();
    uint32 maxActiveClientsNum = sWorld::Instance()->GetMaxActiveSessionCount();
    uint32 maxQueuedClientsNum = sWorld::Instance()->GetMaxQueuedSessionCount();
    uint32 ally = sObjectAccessor::Instance()->alliance_online;
    uint32 horde = sObjectAccessor::Instance()->horde_online;
    uint32 total = ally + horde;
    float ally_pct = (total == 0 ? 0.0f : (float)ally / (float)total) * 100.0f;
    float horde_pct =
        (total == 0 ? 0.0f : (float)horde / (float)total) * 100.0f;
    std::string str = secsToTimeString(sWorld::Instance()->GetUptime());

    std::string server("MaNGOS One (");
#ifdef OPTIMIZED_BUILD
    server += "Optimized";
#else
    server += "Debug";
#endif
#ifdef NDEBUG
    server += ", No Asserts";
#else
    server += ", Using Asserts";
#endif
    server += ") for ";
    server += _ENDIAN_PLATFORM;
    PSendSysMessage("%s", server.c_str());

    PSendSysMessage("ScriptDev2 loaded: %s.",
        sScriptMgr::Instance()->IsScriptLibraryLoaded() ? "Yes" : "No");

    PSendSysMessage(LANG_CONNECTED_USERS, activeClientsNum, maxActiveClientsNum,
        queuedClientsNum, maxQueuedClientsNum);
    PSendSysMessage("Alliance: %u (%.1f%%). Horde: %u (%.1f%%).", ally,
        ally_pct, horde, horde_pct);
    PSendSysMessage(LANG_UPTIME, str.c_str());

    return true;
}

bool ChatHandler::HandleDismountCommand(char* /*args*/)
{
    // If player is not mounted, so go out :)
    if (!m_session->GetPlayer()->IsMounted())
    {
        SendSysMessage(LANG_CHAR_NON_MOUNTED);
        SetSentErrorMessage(true);
        return false;
    }

    if (m_session->GetPlayer()->IsTaxiFlying())
    {
        SendSysMessage(LANG_YOU_IN_FLIGHT);
        SetSentErrorMessage(true);
        return false;
    }

    m_session->GetPlayer()->Unmount();
    m_session->GetPlayer()->remove_auras(SPELL_AURA_MOUNTED);
    return true;
}

bool ChatHandler::HandleGMListIngameCommand(char* /*args*/)
{
    std::vector<std::string> names;

    {
        HashMapHolder<Player>::LockedContainer m =
            sObjectAccessor::Instance()->GetPlayers();
        for (HashMapHolder<Player>::MapType::const_iterator itr =
                 m.get().begin();
             itr != m.get().end(); ++itr)
        {
            AccountTypes itr_sec = itr->second->GetSession()->GetSecurity();
            if ((itr->second->isGameMaster() ||
                    (itr_sec > SEC_PLAYER &&
                        itr_sec <= (AccountTypes)sWorld::Instance()->getConfig(
                                       CONFIG_UINT32_GM_LEVEL_IN_GM_LIST))) &&
                (!m_session ||
                    itr->second->IsVisibleGloballyFor(m_session->GetPlayer())))
                names.push_back(GetNameLink(itr->second));
        }
    }

    if (!names.empty())
    {
        SendSysMessage(LANG_GMS_ON_SRV);

        for (auto& name : names)
            PSendSysMessage("%s", name.c_str());
    }
    else
        SendSysMessage(LANG_GMS_NOT_LOGGED);

    return true;
}

bool ChatHandler::HandleAccountPasswordCommand(char* args)
{
    // allow use from RA, but not from console (not have associated account id)
    if (!GetAccountId())
    {
        SendSysMessage(LANG_RA_ONLY_COMMAND);
        SetSentErrorMessage(true);
        return false;
    }

    // allow or quoted string with possible spaces or literal without spaces
    char* old_pass = ExtractQuotedOrLiteralArg(&args);
    char* new_pass = ExtractQuotedOrLiteralArg(&args);
    char* new_pass_c = ExtractQuotedOrLiteralArg(&args);

    if (!old_pass || !new_pass || !new_pass_c)
        return false;

    std::string password_old = old_pass;
    std::string password_new = new_pass;
    std::string password_new_c = new_pass_c;

    if (password_new != password_new_c)
    {
        SendSysMessage(LANG_NEW_PASSWORDS_NOT_MATCH);
        SetSentErrorMessage(true);
        return false;
    }

    if (!sAccountMgr::Instance()->CheckPassword(GetAccountId(), password_old))
    {
        SendSysMessage(LANG_COMMAND_WRONGOLDPASSWORD);
        SetSentErrorMessage(true);
        return false;
    }

    AccountOpResult result =
        sAccountMgr::Instance()->ChangePassword(GetAccountId(), password_new);

    switch (result)
    {
    case AOR_OK:
        SendSysMessage(LANG_COMMAND_PASSWORD);
        break;
    case AOR_PASS_TOO_LONG:
        SendSysMessage(LANG_PASSWORD_TOO_LONG);
        SetSentErrorMessage(true);
        return false;
    case AOR_NAME_NOT_EXIST: // not possible case, don't want get account name
                             // for output
    default:
        SendSysMessage(LANG_COMMAND_NOTCHANGEPASSWORD);
        SetSentErrorMessage(true);
        return false;
    }

    // OK, but avoid normal report for hide passwords, but log use command for
    // anyone
    LogCommand(".account password *** *** ***");
    SetSentErrorMessage(true);
    return false;
}

bool ChatHandler::HandleAccountLockCommand(char* args)
{
    // allow use from RA, but not from console (not have associated account id)
    if (!GetAccountId())
    {
        SendSysMessage(LANG_RA_ONLY_COMMAND);
        SetSentErrorMessage(true);
        return false;
    }

    bool value;
    if (!ExtractOnOff(&args, value))
    {
        SendSysMessage(LANG_USE_BOL);
        SetSentErrorMessage(true);
        return false;
    }

    if (value)
    {
        LoginDatabase.PExecute(
            "UPDATE account SET locked = '1' WHERE id = '%u'", GetAccountId());
        PSendSysMessage(LANG_COMMAND_ACCLOCKLOCKED);
    }
    else
    {
        LoginDatabase.PExecute(
            "UPDATE account SET locked = '0' WHERE id = '%u'", GetAccountId());
        PSendSysMessage(LANG_COMMAND_ACCLOCKUNLOCKED);
    }

    return true;
}

/// Display the 'Message of the day' for the realm
bool ChatHandler::HandleServerMotdCommand(char* /*args*/)
{
    PSendSysMessage(LANG_MOTD_CURRENT, sWorld::Instance()->GetMotd());
    return true;
}
