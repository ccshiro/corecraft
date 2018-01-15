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

#include "WardenDataStorage.h"
#include "Common.h"
#include "logging.h"
#include "ProgressBar.h"
#include "Util.h"
#include "WardenWin.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "Database/DatabaseEnv.h"
#include <cctype>

CWardenDataStorage WardenDataStorage;

static const char* WardenDynamicComments[WARDEN_DYN_MAX_CHECKS - 1] = {
    "Retrival of Player Base Pointer", "Speedhack", "Tracking hack",
    "Movement state hack (fly, etc)"};

CWardenDataStorage::~CWardenDataStorage()
{
    auto itr1 = data_map_.begin();
    for (; itr1 != data_map_.end(); ++itr1)
        delete itr1->second;

    auto itr2 = result_map_.begin();
    for (; itr2 != result_map_.end(); ++itr2)
        delete itr2->second;
}

void CWardenDataStorage::Init()
{
    LoadWardenDataResult();
}

static int hex_digit(char c)
{
    switch (std::tolower(c))
    {
    case 'a':
        return 0x0a;
    case 'b':
        return 0x0b;
    case 'c':
        return 0x0c;
    case 'd':
        return 0x0d;
    case 'e':
        return 0x0e;
    case 'f':
        return 0x0f;
    default:
        if (std::isdigit(c))
            return c - '0'; // use boost::lexical_cast later on
        else
            throw std::runtime_error("hex digits must be 0-9 a-f, faggot");
    }
}

static std::vector<uint8> hex_to_bin(const std::string& input)
{
    if (input.size() & 1)
        throw std::runtime_error(
            "hex strings must have a multiple of 2 as size, faggot");

    std::vector<uint8> output;
    output.reserve(input.size() / 2);

    for (std::string::const_iterator itr = input.begin(); itr != input.end();
         itr += 2)
    {
        int hi = hex_digit(*itr) << 4;
        int lo = hex_digit(*(itr + 1));
        output.push_back(hi | lo);
    }

    return output;
}

void CWardenDataStorage::LoadWardenDataResult()
{
    //                                                0     1        2       3
    //                                                4          5         6
    //                                                7         8
    QueryResult* result = WorldDatabase.Query(
        "SELECT `id`, `check`, `data`, `str`, `address`, `length`, `result`, "
        "`action`, `comment` FROM warden_check");

    uint32 count = 0;

    if (!result)
    {
        logging.info("Loaded 0 Warden Checks\n");
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        ++count;
        bar.step();

        Field* fields = result->Fetch();

        std::vector<uint8> result;
        std::vector<uint8> data;

        uint32 id = fields[0].GetUInt32();
        uint8 type = fields[1].GetUInt8();
        std::string temp_data = fields[2].GetCppString();
        if (temp_data.size() > 0)
            data = hex_to_bin(temp_data);
        std::string str = fields[3].GetCppString();
        uint32 address = fields[4].GetUInt32();
        uint8 length = fields[5].GetUInt8();
        if (length > 0)
            result = hex_to_bin(fields[6].GetCppString());
        uint32 action = fields[7].GetUInt32();
        std::string comment = fields[8].GetCppString();

        if (fields[5].GetUInt32() > length)
        {
            logging.error(
                "Warden: Check Id: %u Error: Check is using too long length "
                "for the result.",
                id);
            continue;
        }

        if (result.size() != length)
        {
            logging.error(
                "Warden: Check Id: %u Error: Result length does not match the "
                "table length.",
                id);
            continue;
        }

        if (action >= MAX_WARDEN_ACTION)
        {
            logging.error(
                "Warden: Check Id: %u Error: Warden action is not a valid "
                "action.",
                id);
            continue;
        }

        switch (type)
        {
        case MEM_CHECK:
        case PAGE_CHECK_A:
        case PAGE_CHECK_B:
        case MODULE_CHECK:
            break;
        default:
            logging.error(
                "Warden: Check Id: %u Error: Unsupported check type.", id);
            continue;
        }

        if (type == MEM_CHECK && result.empty())
        {
            logging.error(
                "Warden: Check Id: %u Error: MEM_CHECK cannot have an empty "
                "result.",
                id);
            continue;
        }

        if ((type == PAGE_CHECK_A || type == PAGE_CHECK_B) && data.empty())
        {
            logging.error(
                "Warden: Check Id: %u Error: PAGE_CHECK cannot have empty "
                "data.",
                id);
            continue;
        }

        if (type == MODULE_CHECK && str.empty())
        {
            logging.error(
                "Warden: Check Id: %u Error: MODULE_CHECK cannot have an empty "
                "str.",
                id);
            continue;
        }

        auto wd = new WardenData();
        wd->id = id;
        wd->Type = type;
        wd->Data = data;
        wd->str = str;
        wd->Address = address;
        wd->Length = length;
        wd->action = static_cast<WardenAction>(action);

        data_map_[id] = wd;

        if (type == MEM_CHECK /* || type == MPQ_CHECK*/)
        {
            auto wr = new WardenDataResult();
            wr->res = result;
            result_map_[id] = wr;
        }

        // Module names are expected to be sent in all-caps
        if (type == MODULE_CHECK)
        {
            for (auto& elem : wd->str)
                elem = std::toupper(elem);
        }

        comment_map_[check_identifier(id, false)] = comment;

        if (type == PAGE_CHECK_A || type == PAGE_CHECK_B)
            static_slow_checks.push_back(id);
        else
            static_checks.push_back(id);

    } while (result->NextRow());

    delete result;

    logging.info("Loaded " SIZEFMTD " static Warden checks (" SIZEFMTD
                 " skipped due to errors). Normal checks: " SIZEFMTD
                 ". Slow checks: " SIZEFMTD
                 ". Also using %d dynamic Warden checks.\n",
        data_map_.size(), count - data_map_.size(), static_checks.size(),
        static_slow_checks.size(), WARDEN_DYN_MAX_CHECKS - 1);

    for (int i = 1; i < WARDEN_DYN_MAX_CHECKS - 1; ++i)
        comment_map_[check_identifier(i, true)] = WardenDynamicComments[i - 1];
}

const WardenData* CWardenDataStorage::GetWardenDataById(uint32 Id) const
{
    auto itr = data_map_.find(Id);
    if (itr != data_map_.end())
        return itr->second;
    return nullptr;
}

const WardenDataResult* CWardenDataStorage::GetWardenResultById(uint32 Id) const
{
    auto itr = result_map_.find(Id);
    if (itr != result_map_.end())
        return itr->second;
    return nullptr;
}

const std::string& CWardenDataStorage::GetComment(check_identifier id)
{
    return comment_map_[id];
}
