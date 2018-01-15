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

#ifndef _WARDEN_MAC_H
#define _WARDEN_MAC_H

#include "ByteBuffer.h"
#include "Warden.h"
#include "Auth/BigNumber.h"
#include "Auth/SARC4.h"
#include <map>

class WorldSession;

class WardenMac : public Warden
{
public:
    WardenMac();
    ~WardenMac();

    void Init(WorldSession* pClient, BigNumber* K) override;
    ClientWardenModule* GetModuleForClient(WorldSession* session) override;
    void InitializeModule() override;
    void RequestHash() override;
    void HandleHashResult(ByteBuffer& buff) override;
    void RequestData() override;
    void HandleData(ByteBuffer& buff) override;

private:
    WardenData* get_dynamic_check(uint32 /*id*/) override { return nullptr; }
    bool check_dynamic_result(
        const WardenData* /*check*/, ByteBuffer& /*buff*/) override
    {
        return false;
    }
};

#endif
