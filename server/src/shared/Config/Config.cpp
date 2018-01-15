/*
 * Copyright (C) 2005-2012 MaNGOS <http://getmangos.com/>
 * Coypright (C) 2013 Corecraft <https://www.worldofcorecraft.com/>
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

#include "Config.h"
#include "logging.h"
#include <boost/program_options.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <fstream>

template <typename T>
static T get_value_helper(const std::string& filename,
    const std::string& option_name, const T default_value)
{
    // FIXME: This actually parses the config file each time you call it.
    // But config files are cached in World anyway, so it shouldn't matter.
    using namespace boost::program_options;

    options_description desc;
    desc.add_options()(
        option_name.c_str(), value<T>()->default_value(default_value));

    variables_map config;
    store(parse_config_file<char>(filename.c_str(), desc, true), config);

    return config[option_name].as<T>();
}

bool Config::SetSource(const std::string& filename)
{
    mFilename = filename;

    std::ifstream in(filename.c_str());

    mValidSource = in.good();

    return mValidSource;
}

std::string Config::GetStringDefault(const char* name, const std::string& def)
{
    if (!mValidSource)
        return def;

    std::string str = get_value_helper(mFilename, name, def);
    // Pop quotes if needed
    if (!str.empty() && str[0] == '"')
    {
        str.erase(str.begin());
        if (!str.empty() && str[str.size() - 1] == '"')
            str.erase(str.end() - 1);
    }
    return str;
}

bool Config::GetBoolDefault(const char* name, bool def)
{
    if (!mValidSource)
        return def;

    return get_value_helper(mFilename, name, def);
}

int32 Config::GetIntDefault(const char* name, int32 def)
{
    if (!mValidSource)
        return def;

    return get_value_helper(mFilename, name, def);
}

float Config::GetFloatDefault(const char* name, float def)
{
    if (!mValidSource)
        return def;

    return get_value_helper(mFilename, name, def);
}

void Config::LoadLogLevels()
{
    if (!mValidSource)
        return;

    boost::property_tree::ptree pt;
    boost::property_tree::ini_parser::read_ini(mFilename, pt);
    for (auto& section : pt)
    {
        if (section.first == "LOGGERS")
        {
            for (auto& key : section.second)
            {
                int level = key.second.get_value<int>();
                if (level >= 0 && level <= 6)
                    logging.get_logger(key.first).set_level(
                        static_cast<LogLevel>(level));
            }
            return;
        }
    }
}
