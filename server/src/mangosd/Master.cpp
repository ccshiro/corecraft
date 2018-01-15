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
    \ingroup mangosd
*/

#include "Master.h"
#include "CliThread.h"
#include "Common.h"
#include "DBCStores.h"
#include "FreezeDetectionThread.h"
#include "logging.h"
#include "MassMailMgr.h"
#include "SystemConfig.h"
#include "Timer.h"
#include "Util.h"
#include "World.h"
#include "WorldConnection.h"
#include "WorldServer.h"
#include "WorldThread.h"
#include "ra_server.h"
#include "Config/Config.h"
#include "Database/DatabaseEnv.h"
#include "Platform/CompilerDefs.h"
#include "Policies/Singleton.h"
#include <boost/bind.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/thread/thread.hpp>

Master::Master()
{
}

Master::~Master()
{
}

/// Main function
int Master::Run()
{
    /// worldd PID file creation
    std::string pidfile = sConfig::Instance()->GetStringDefault("PidFile", "");
    if (!pidfile.empty())
    {
        uint32 pid = CreatePIDFile(pidfile);
        if (!pid)
        {
            logging.error("Cannot create PID file %s.\n", pidfile.c_str());

            return 1;
        }

        logging.info("Daemon PID: %u\n", pid);
    }

    ///- Start the databases
    if (!_StartDB())
    {
        return 1;
    }

    ///- Initialize the World
    sWorld::Instance()->SetInitialWorldSettings();

    // server loaded successfully => enable async DB requests
    // this is done to forbid any async transactions during server startup!
    CharacterDatabase.AllowAsyncTransactions();
    WorldDatabase.AllowAsyncTransactions();
    LoginDatabase.AllowAsyncTransactions();

    ///- Catch termination signals
    _HookSignals();

    ///- Launch WorldRunnable thread
    // This thread was marked as highest priority when using ACE. See comment on
    // the freeze thread in this file.
    boost::thread world_thread(WorldThread);

    // set realmbuilds depend on mangosd expected builds, and set server online
    {
        std::string builds = AcceptableClientBuildsListStr();
        LoginDatabase.escape_string(builds);
        LoginDatabase.DirectPExecute(
            "UPDATE realmlist SET realmflags = realmflags & ~(%u), population "
            "= 0, realmbuilds = '%s'  WHERE id = '%u'",
            REALM_FLAG_OFFLINE, builds.c_str(), realmID);
    }

    boost::thread* cliThread = nullptr;
    if (sConfig::Instance()->GetBoolDefault("Console.Enable", true))
    {
        ///- Launch CliRunnable thread
        void CLIRun();
        cliThread = new boost::thread(CLIRun);
    }

    bool using_ra = false;
    if (sConfig::Instance()->GetBoolDefault("Ra.Enable", false))
    {
        std::string addr =
            sConfig::Instance()->GetStringDefault("Ra.IP", "0.0.0.0");
        std::string port = boost::lexical_cast<std::string>(
            sConfig::Instance()->GetIntDefault("Ra.Port", 3443));

        sRAServer::Instance()->start(addr, port);
        using_ra = true;
    }

    ///- Start up freeze detection thread
    std::unique_ptr<FreezeDetectionThread> freezedetector;
    if (uint32 delay =
            sConfig::Instance()->GetIntDefault("MaxCoreStuckTime", 0))
    {
        freezedetector.reset(new FreezeDetectionThread(delay));
        freezedetector->start();
    }

    ///- Launch the world listener socket
    std::string bind_ip =
        sConfig::Instance()->GetStringDefault("BindIP", "0.0.0.0");
    std::string bind_port =
        sConfig::Instance()->GetStringDefault("WorldServerPort", "8085");

    if (!sWorldServer::Instance()->start(bind_ip, bind_port))
    {
        logging.error("Shutting down due to network error");

        World::StopNow(ERROR_EXIT_CODE);
    }

    ///- Stop freeze protection before shutdown tasks
    if (freezedetector)
        freezedetector->stop();

    ///- Set server offline in realmlist
    LoginDatabase.DirectPExecute(
        "UPDATE realmlist SET realmflags = realmflags | %u WHERE id = '%u'",
        REALM_FLAG_OFFLINE, realmID);

    ///- Remove signal handling before leaving
    _UnhookSignals();

    // when the main thread closes the singletons get unloaded
    // since worldrunnable uses them, it will crash if unloaded after master
    world_thread.join();

    // Shutdown Remote Administration if it was started
    if (using_ra)
    {
        sRAServer::Instance()->stop();
        sRAServer::Instance()->join();
    }

    ///- Clean account database before leaving
    clearOnlineAccounts();

    // send all still queued mass mails (before DB connections shutdown)
    sMassMailMgr::Instance()->Update(true);

    ///- Wait for DB delay threads to end
    CharacterDatabase.HaltDelayThread();
    WorldDatabase.HaltDelayThread();
    LoginDatabase.HaltDelayThread();

    logging.info("Halting process...");

    if (cliThread)
    {
        cliThread->join();
        delete cliThread;
    }

    // Delete all singletons to make memory debugging with tools such as
    // Valgrind easier
    void clean_singletons();
    clean_singletons();

    ///- Exit the process with specified return value
    return World::GetExitCode();
}

/// Initialize connection to the databases
bool Master::_StartDB()
{
    ///- Get world database info from configuration file
    std::string dbstring =
        sConfig::Instance()->GetStringDefault("WorldDatabaseInfo", "");
    int nConnections =
        sConfig::Instance()->GetIntDefault("WorldDatabaseConnections", 1);
    if (dbstring.empty())
    {
        logging.error("Database not specified in configuration file");
        return false;
    }
    logging.info("World Database total connections: %i", nConnections + 1);

    ///- Initialise the world database
    if (!WorldDatabase.Initialize(dbstring.c_str(), nConnections))
    {
        logging.error("Cannot connect to world database %s", dbstring.c_str());
        return false;
    }

    dbstring =
        sConfig::Instance()->GetStringDefault("CharacterDatabaseInfo", "");
    nConnections =
        sConfig::Instance()->GetIntDefault("CharacterDatabaseConnections", 1);
    if (dbstring.empty())
    {
        logging.error("Character Database not specified in configuration file");

        ///- Wait for already started DB delay threads to end
        WorldDatabase.HaltDelayThread();
        return false;
    }
    logging.info("Character Database total connections: %i", nConnections + 1);

    ///- Initialise the Character database
    if (!CharacterDatabase.Initialize(dbstring.c_str(), nConnections))
    {
        logging.error(
            "Cannot connect to Character database %s", dbstring.c_str());

        ///- Wait for already started DB delay threads to end
        WorldDatabase.HaltDelayThread();
        return false;
    }

    ///- Get login database info from configuration file
    dbstring = sConfig::Instance()->GetStringDefault("LoginDatabaseInfo", "");
    nConnections =
        sConfig::Instance()->GetIntDefault("LoginDatabaseConnections", 1);
    if (dbstring.empty())
    {
        logging.error("Login database not specified in configuration file");

        ///- Wait for already started DB delay threads to end
        WorldDatabase.HaltDelayThread();
        CharacterDatabase.HaltDelayThread();
        return false;
    }

    ///- Initialise the login database
    logging.info("Login Database total connections: %i", nConnections + 1);
    if (!LoginDatabase.Initialize(dbstring.c_str(), nConnections))
    {
        logging.error("Cannot connect to login database %s", dbstring.c_str());

        ///- Wait for already started DB delay threads to end
        WorldDatabase.HaltDelayThread();
        CharacterDatabase.HaltDelayThread();
        return false;
    }

    ///- Get the realm Id from the configuration file
    realmID = sConfig::Instance()->GetIntDefault("RealmID", 0);
    if (!realmID)
    {
        logging.error("Realm ID not defined in configuration file");

        ///- Wait for already started DB delay threads to end
        WorldDatabase.HaltDelayThread();
        CharacterDatabase.HaltDelayThread();
        LoginDatabase.HaltDelayThread();
        return false;
    }

    logging.info("Realm running as realm ID %d", realmID);

    ///- Clean the database before starting
    clearOnlineAccounts();

    return true;
}

/// Clear 'online' status for all accounts with characters in this realm
void Master::clearOnlineAccounts()
{
    // Cleanup online status for characters hosted at current realm
    /// \todo Only accounts with characters logged on *this* realm should have
    /// online status reset. Move the online column from 'account' to
    /// 'realmcharacters'?
    LoginDatabase.PExecute(
        "UPDATE account SET active_realm_id = 0 WHERE active_realm_id = '%u'",
        realmID);

    CharacterDatabase.Execute(
        "UPDATE characters SET online = 0 WHERE online<>0");

    // Battleground instance ids reset at server restart
    CharacterDatabase.Execute(
        "UPDATE character_battleground_data SET instance_id = 0");
}

/// Handle termination signals
void Master::_OnSignal(int s)
{
    switch (s)
    {
    case SIGINT:
        World::StopNow(RESTART_EXIT_CODE);
        break;
    case SIGTERM:
#if PLATFORM == PLATFORM_WINDOWS
    case SIGBREAK:
#endif
        World::StopNow(SHUTDOWN_EXIT_CODE);
        break;
    }

    signal(s, _OnSignal);
}

/// Define hook '_OnSignal' for all termination signals
void Master::_HookSignals()
{
    signal(SIGINT, _OnSignal);
    signal(SIGTERM, _OnSignal);
#if PLATFORM == PLATFORM_WINDOWS
    signal(SIGBREAK, _OnSignal);
#endif
}

/// Unhook the signals before leaving
void Master::_UnhookSignals()
{
    signal(SIGINT, nullptr);
    signal(SIGTERM, nullptr);
#if PLATFORM == PLATFORM_WINDOWS
    signal(SIGBREAK, 0);
#endif
}

// All singletons in mangos (minus the one included above)
#include "ban_wave.h"                 // sort off
#include "battlefield_queue.h"        // sort off
#include "FirstKills.h"               // sort off
#include "loot_selection.h"           // sort off
#include "ObjectAccessor.h"           // sort off
#include "pet_template.h"             // sort off
#include "AccountMgr.h"               // sort off
#include "AddonHandler.h"             // sort off
#include "AuctionHouseMgr.h"          // sort off
#include "BattleGroundMgr.h"          // sort off
#include "ChannelMgr.h"               // sort off
#include "ConditionMgr.h"             // sort off
#include "CreatureEventAIMgr.h"       // sort off
#include "CreatureTextMgr.h"          // sort off
#include "GameEventMgr.h"             // sort off
#include "GridMap.h"                  // sort off
#include "GuildMgr.h"                 // sort off
#include "MapManager.h"               // sort off
#include "MapPersistentStateMgr.h"    // sort off
#include "ObjectMgr.h"                // sort off
#include "OutdoorPvP/OutdoorPvPMgr.h" // sort off
#include "ScriptMgr.h"                // sort off
#include "SmartScriptMgr.h"           // sort off
#include "SocialMgr.h"                // sort off
#include "SpellMgr.h"                 // sort off
#include "movement/WaypointManager.h" // sort off
#include "ra_server.h"                // sort off
#include "PoolManager.h"              // sort off
#include "action_limit.h"             // sort off

void clean_singletons()
{
    if (sWorld::Instance()->getConfig(
            CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_CHANNEL))
    {
        delete channelMgr(ALLIANCE);
    }
    else
    {
        delete channelMgr(ALLIANCE);
        delete channelMgr(HORDE);
    }

    delete &*sBanWave::Instance();
    delete &*sBattlefieldQueue::Instance();
    delete &*sFirstKills::Instance();
    delete &*sLootSelection::Instance();
    delete &*sObjectAccessor::Instance();
    delete &*sPetTemplates::Instance();
    delete &*sAccountMgr::Instance();
    delete &*sAddOnHandler::Instance();
    delete &*sAuctionMgr::Instance();
    delete &*sBattleGroundMgr::Instance();
    delete &*sConditionMgr::Instance();
    delete &*sEventAIMgr::Instance();
    delete &*sCreatureTextMgr::Instance();
    delete &*sGameEventMgr::Instance();
    delete &*sTerrainMgr::Instance();
    delete &*sGuildMgr::Instance();
    delete &*sMapMgr::Instance();
    delete &*sMapPersistentStateMgr::Instance();
    delete &*sMassMailMgr::Instance();
    delete &*sObjectMgr::Instance();
    delete &*sOutdoorPvPMgr::Instance();
    delete &*sScriptMgr::Instance();
    delete &*sSmartScriptMgr::Instance();
    delete &*sSmartWaypointMgr::Instance();
    delete &*sSocialMgr::Instance();
    delete &*sSpellMgr::Instance();
    delete &*sWaypointMgr::Instance();
    delete &*sWorld::Instance();
    delete &*sWorldServer::Instance();
    delete &*sRAServer::Instance();
    delete &*sConfig::Instance();
    delete &*sPoolMgr::Instance();
    delete &*sActionLimit::Instance();

    // sMaster not deleted since delete happens from within sMaster;
    // however it does not allocate any resources
}
