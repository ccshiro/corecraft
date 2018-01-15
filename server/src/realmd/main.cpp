/*
 * Copyright (C) 2005-2012 MaNGOS <http://getmangos.com/>
 * Copyright (C) 2013 Corecraft <https://www.worldofcorecraft.com/>
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

/// \addtogroup realmd Realm Daemon
/// @{
/// \file

#include "Common.h"
#include "logging.h"
#include "RealmList.h"
#include "SystemConfig.h"
#include "Util.h"
#include "server.h"
#include "Config/Config.h"
#include "Database/DatabaseEnv.h"
#include <boost/asio.hpp>
#include <boost/version.hpp>
#include <openssl/crypto.h>
#include <openssl/opensslv.h>

//#include "revision.h"
//#include "revision_nr.h"
//#include "revision_sql.h"

bool StartDB();
void sql_ping_callback(const boost::system::error_code& e,
    boost::asio::deadline_timer& ping_sql_timer);

bool stopEvent = false; ///< Setting it to true stops the server

DatabaseType LoginDatabase; ///< Accessor to the realm server database

extern int main(int /*argc*/, char** /*argv*/)
{
    const char* cfg_file = _REALMD_CONFIG;

    if (!sConfig::Instance()->SetSource(cfg_file))
    {
        logging.error("Could not find configuration file %s.", cfg_file);

        return 1;
    }

    sConfig::Instance()->LoadLogLevels();

    ///- Check the version of the configuration file
    uint32 confVersion = sConfig::Instance()->GetIntDefault("ConfVersion", 0);
    if (confVersion < _REALMDCONFVERSION)
    {
        logging.error(
            "******************************************************************"
            "***********");
        logging.error(
            " WARNING: Your realmd.conf version indicates your conf file is "
            "out of date!");
        logging.error(
            "          Please check for updates, as your current default "
            "values may cause");
        logging.error("          strange behavior.");
        logging.error(
            "******************************************************************"
            "***********");
    }

    LOG_DEBUG(logging, "%s (Library: %s)", OPENSSL_VERSION_TEXT,
        SSLeay_version(SSLEAY_VERSION));
    if (SSLeay() < 0x009080bfL)
    {
        LOG_DEBUG(logging,
            "WARNING: Outdated version of OpenSSL lib. Logins to server may "
            "not work!");
        LOG_DEBUG(
            logging, "WARNING: Minimal required version [OpenSSL 0.9.8k]");
    }

    LOG_DEBUG(logging, "Using boost version: %s", BOOST_LIB_VERSION);

    ///- Initialize the database connection
    if (!StartDB())
    {
        return 1;
    }

    ///- Get the list of realms for the server
    sRealmList::Instance()->Initialize(
        sConfig::Instance()->GetIntDefault("RealmsStateUpdateDelay", 20));
    if (sRealmList::Instance()->size() == 0)
    {
        logging.error("No valid realms specified.");

        return 1;
    }

    // cleanup query
    // set expired bans to inactive
    LoginDatabase.BeginTransaction();
    LoginDatabase.Execute(
        "UPDATE account_banned SET active = 0 WHERE "
        "unbandate<=UNIX_TIMESTAMP() AND unbandate<>bandate");
    LoginDatabase.Execute(
        "DELETE FROM ip_banned WHERE unbandate<=UNIX_TIMESTAMP() AND "
        "unbandate<>bandate");
    LoginDatabase.CommitTransaction();

    // server has started up successfully => enable async DB requests
    LoginDatabase.AllowAsyncTransactions();

    int32 ping_interval = sConfig::Instance()->GetIntDefault("MaxPingTime", 30);

    boost::asio::io_service io_service; // Our io service

    // Add an asynchronous timer that keeps pinging the sql server to our io
    // service
    boost::asio::deadline_timer ping_sql_timer(
        io_service, boost::posix_time::seconds(ping_interval));
    ping_sql_timer.async_wait(boost::bind(&sql_ping_callback,
        boost::asio::placeholders::error, boost::ref(ping_sql_timer)));

    // Listen for signals
    boost::asio::signal_set signals(io_service, SIGINT, SIGTERM);
    signals.async_wait(
        boost::bind(&boost::asio::io_service::stop, &io_service));

    // Create & start our realm server, passing it our io service
    std::string bind_ip =
        sConfig::Instance()->GetStringDefault("BindIP", "0.0.0.0");
    std::string bind_port = sConfig::Instance()->GetStringDefault(
        "RealmServerPort", DEFAULT_REALMSERVER_PORT);
    server realm_server(io_service);
    realm_server.start(bind_ip, bind_port);

    // Run the io service
    io_service.run();

    ///- Wait for the delay thread to exit
    LoginDatabase.HaltDelayThread();

    logging.info("Halting process...");
    return 0;
}

/// Initialize connection to the database
bool StartDB()
{
    std::string dbstring =
        sConfig::Instance()->GetStringDefault("LoginDatabaseInfo", "");
    if (dbstring.empty())
    {
        logging.error("Database not specified");
        return false;
    }

    logging.info("Login Database total connections: %i", 1 + 1);

    if (!LoginDatabase.Initialize(dbstring.c_str()))
    {
        logging.error("Cannot connect to database");
        return false;
    }

    /*    if (!LoginDatabase.CheckRequiredField(
                "realmd_db_version", REVISION_DB_REALMD))
        {
            ///- Wait for already started DB delay threads to end
            LoginDatabase.HaltDelayThread();
            return false;
        }*/

    return true;
}

void sql_ping_callback(const boost::system::error_code& e,
    boost::asio::deadline_timer& ping_sql_timer)
{
    if (!e)
    {
        LoginDatabase.Ping();
        int32 ping_interval =
            sConfig::Instance()->GetIntDefault("MaxPingTime", 30);
        ping_sql_timer.expires_from_now(
            boost::posix_time::seconds(ping_interval));
        ping_sql_timer.async_wait(boost::bind(&sql_ping_callback,
            boost::asio::placeholders::error, boost::ref(ping_sql_timer)));
    }
}
