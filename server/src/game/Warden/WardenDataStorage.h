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

#ifndef _WARDEN_DATA_STORAGE_H
#define _WARDEN_DATA_STORAGE_H

#include "Auth/BigNumber.h"
#include <map>
#include <string>
#include <vector>

enum WardenAction
{
    WARDEN_ACTION_LOG = 0,
    WARDEN_ACTION_KICK = 1,
    WARDEN_ACTION_BAN_WAVE = 2,
    WARDEN_ACTION_BAN = 3,
    MAX_WARDEN_ACTION
};

enum WardenDynamicCheck
{
    WARDEN_DYN_CHECK_PLAYER_BASE = 1,
    WARDEN_DYN_CHECK_SPEEDS,
    WARDEN_DYN_CHECK_TRACKING,
    WARDEN_DYN_CHECK_MOVEMENT_STATE,
    WARDEN_DYN_MAX_CHECKS,
};

struct WardenData
{
    uint32 id;
    uint8 Type;
    std::vector<uint8> Data;
    uint32 Address;  // PROC_CHECK, MEM_CHECK, PAGE_CHECK
    uint8 Length;    // PROC_CHECK, MEM_CHECK, PAGE_CHECK
    std::string str; // LUA, MPQ, DRIVER
    WardenAction action;
};

struct WardenDataResult
{
    std::vector<uint8> res; // MEM_CHECK
};

struct check_identifier
{
    check_identifier(uint32 i, bool dyn) : id(i), dynamic(dyn) {}
    uint32 id;
    bool dynamic;

    std::string printable() const
    {
        std::ostringstream ss;
        ss << (dynamic ? "dyn:" : "") << id;
        return ss.str();
    }

    // For sorting in std::map
    bool operator<(const check_identifier& rhs) const
    {
        if (dynamic != rhs.dynamic)
            return dynamic < rhs.dynamic;
        return id < rhs.id;
    }
};

inline std::ostream& operator<<(std::ostream& os, const check_identifier& check)
{
    os << check.printable();
    return os;
}

class CWardenDataStorage
{
public:
    CWardenDataStorage() {}
    ~CWardenDataStorage();

    std::vector<uint32> static_checks;
    std::vector<uint32> static_slow_checks;
    void Init();

    const WardenData* GetWardenDataById(uint32 Id) const;
    const WardenDataResult* GetWardenResultById(uint32 Id) const;
    const std::string& GetComment(check_identifier id);

private:
    std::map<uint32, WardenData*> data_map_;
    std::map<uint32, WardenDataResult*> result_map_;
    std::map<check_identifier, std::string> comment_map_;

    void LoadWardenDataResult();
};

extern CWardenDataStorage WardenDataStorage;

#endif
