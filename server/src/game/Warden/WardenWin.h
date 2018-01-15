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

#ifndef _WARDEN_WIN_H
#define _WARDEN_WIN_H

#include "ByteBuffer.h"
#include "Timer.h"
#include "Warden.h"
#include "Auth/BigNumber.h"
#include "Auth/SARC4.h"
#include <iostream>
#include <map>
#include <vector>

class WorldSession;
enum WardenCheckType
{
    MEM_CHECK = 0xF3, // byte moduleNameIndex + uint Offset + byte Len (check to
                      // ensure memory isn't modified)
    PAGE_CHECK_A = 0xB2, // uint Seed + byte[20] SHA1 + uint Addr + byte Len
                         // (scans all pages for specified hash)
    PAGE_CHECK_B = 0xBF, // uint Seed + byte[20] SHA1 + uint Addr + byte Len
                         // (scans only pages starts with MZ+PE headers for
                         // specified hash)
    MPQ_CHECK =
        0x98, // byte fileNameIndex (check to ensure MPQ file isn't modified)
    LUA_STR_CHECK =
        0x8B, // byte luaNameIndex (check to ensure LUA string isn't used)
    DRIVER_CHECK = 0x71, // uint Seed + byte[20] SHA1 + byte driverNameIndex
                         // (check to ensure driver isn't loaded)
    TIMING_CHECK =
        0x57,          // empty (check to ensure GetTickCount() isn't detoured)
    PROC_CHECK = 0x7E, // uint Seed + byte[20] SHA1 + byte moluleNameIndex +
                       // byte procNameIndex + uint Offset + byte Len (check to
                       // ensure proc isn't detoured)
    MODULE_CHECK = 0xD9, // uint Seed + byte[20] SHA1 (check to ensure module
                         // isn't injected)
};

#if defined(__GNUC__)
#pragma pack(1)
#else
#pragma pack(push, 1)
#endif

struct WardenInitModuleRequest
{
    uint8 Command1;
    uint16 Size1;
    uint32 CheckSumm1;
    uint8 Unk1;
    uint8 Unk2;
    uint8 Type;
    uint8 String_library1;
    uint32 Function1[4];

    uint8 Command2;
    uint16 Size2;
    uint32 CheckSumm2;
    uint8 Unk3;
    uint8 Unk4;
    uint8 String_library2;
    uint32 Function2;
    uint8 Function2_set;

    uint8 Command3;
    uint16 Size3;
    uint32 CheckSumm3;
    uint8 Unk5;
    uint8 Unk6;
    uint8 String_library3;
    uint32 Function3;
    uint8 Function3_set;
};

#if defined(__GNUC__)
#pragma pack()
#else
#pragma pack(pop)
#endif

class WardenWin : public Warden
{
public:
    WardenWin();
    ~WardenWin();

    void Init(WorldSession* pClient, BigNumber* K) override;
    ClientWardenModule* GetModuleForClient(WorldSession* session) override;
    void InitializeModule() override;
    void RequestHash() override;
    void HandleHashResult(ByteBuffer& buff) override;
    void RequestData() override;
    void HandleData(ByteBuffer& buff) override;

    void invalidate_dynamic(WardenDynamicCheck check) override;

private:
    std::vector<check_identifier> current_checks_;
    std::vector<uint32> pending_checks_;
    std::vector<uint32> pending_slow_checks_;
    std::vector<uint32> pending_dyn_checks_;

    //
    // Below is all functionaliy related to Dynamic Checks
    //

    // Contains the current dynamic check and result that's being tested against
    // Since they change they cannot be static, and to avoid unnecessary
    // allocation/deallocation they are saved & modified internally
    uint32 player_base_;
    WardenData check_;
    bool valid_dynamic_checks_[WARDEN_DYN_MAX_CHECKS - 1];

    const WardenData* get_dynamic_check(uint32 id) override;
    bool check_dynamic_result(
        const WardenData* check, ByteBuffer& buff) override;

    void fill_pending_dyn_checks();
    void build_dynamic_checks();
    bool should_ignore_result(const WardenData* check, ByteBuffer& buff,
        std::vector<check_identifier>& failed_checks);

    // Helper function for packet correctness
    bool assert_enough_data_left(ByteBuffer& buff, uint32 size);
};

#endif
