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

/// \addtogroup mangosd Mangos Daemon
/// @{
/// \file

#include "Common.h"
#include "logging.h"
#include "Master.h"
#include "ProgressBar.h"
#include "SystemConfig.h"
#include "Config/Config.h"
#include "Database/DatabaseEnv.h"
#include <boost/program_options.hpp>
#include <openssl/crypto.h>
#include <openssl/opensslv.h>
#include <iostream>

DatabaseType WorldDatabase;     ///< Accessor to the world database
DatabaseType CharacterDatabase; ///< Accessor to the character database
DatabaseType LoginDatabase;     ///< Accessor to the realm/login database

uint32 realmID; ///< Id of the realm

/// Launch the mangos server
extern int main(int argc, char** argv)
{
    ///- Command line parsing
    std::string cfg_file(_MANGOSD_CONFIG);

    using namespace boost::program_options;
    options_description desc("Options recognized by MaNGOS");
    desc.add_options()("help,h,?", "displays this help")(
        "config,c", value<std::string>(), "overrides default config file");

    variables_map vm;
    store(parse_command_line(argc, argv, desc), vm);
    notify(vm);

    if (vm.count("help"))
    {
        std::cout << desc << "\n";
        return 0;
    }
    if (vm.count("config"))
    {
        cfg_file = vm["config"].as<std::string>();
    }

    if (!sConfig::Instance()->SetSource(cfg_file))
    {
        logging.error(
            "Could not find configuration file %s.", cfg_file.c_str());

        return 1;
    }

    // Loggers
    sConfig::Instance()->LoadLogLevels();

    //    logging.info("%s [world-daemon]",
    //        _FULLVERSION(REVISION_DATE, REVISION_TIME, REVISION_NR,
    //        REVISION_ID));
    logging.info("<Ctrl-C> to stop.");
    logging.info(
        "\n\n"
        "MM   MM         MM   MM  MMMMM   MMMM   MMMMM\n"
        "MM   MM         MM   MM MMM MMM MM  MM MMM MMM\n"
        "MMM MMM         MMM  MM MMM MMM MM  MM MMM\n"
        "MM M MM         MMMM MM MMM     MM  MM  MMM\n"
        "MM M MM  MMMMM  MM MMMM MMM     MM  MM   MMM\n"
        "MM M MM M   MMM MM  MMM MMMMMMM MM  MM    MMM\n"
        "MM   MM     MMM MM   MM MM  MMM MM  MM     MMM\n"
        "MM   MM MMMMMMM MM   MM MMM MMM MM  MM MMM MMM\n"
        "MM   MM MM  MMM MM   MM  MMMMMM  MMMM   MMMMM\n"
        "        MM  MMM http://getmangos.com\n"
        "        MMMMMM\n\n");
    logging.info("Using configuration file %s.", cfg_file.c_str());

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

    ///- Set progress bars show mode
    BarGoLink::SetOutputState(
        sConfig::Instance()->GetBoolDefault("ShowProgressBars", true));

    ///- and run the 'Master'
    /// \todo Why do we need this 'Master'? Can't all of this be in the Main as
    /// for Realmd?
    return sMaster::Instance()->Run();

    // at sMaster return function exist with codes
    // 0 - normal shutdown
    // 1 - shutdown at error
    // 2 - restart command used, this code can be used by restarter for restart
    // mangosd
}

/// @}
