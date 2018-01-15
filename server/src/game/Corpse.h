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

#ifndef MANGOSSERVER_CORPSE_H
#define MANGOSSERVER_CORPSE_H

#include "Common.h"
#include "LootMgr.h"
#include "Object.h"
#include "maps/map_grid.h"

class Field;

enum CorpseType
{
    CORPSE_BONES = 0,
    CORPSE_RESURRECTABLE_PVE = 1,
    CORPSE_RESURRECTABLE_PVP = 2
};
#define MAX_CORPSE_TYPE 3

// Value equal client resurrection dialog show radius.
#define CORPSE_RECLAIM_RADIUS 39

enum CorpseFlags
{
    CORPSE_FLAG_NONE = 0x00,
    CORPSE_FLAG_BONES = 0x01,
    CORPSE_FLAG_UNK1 = 0x02,
    CORPSE_FLAG_UNK2 = 0x04,
    CORPSE_FLAG_HIDE_HELM = 0x08,
    CORPSE_FLAG_HIDE_CLOAK = 0x10,
    CORPSE_FLAG_LOOTABLE = 0x20
};

class Corpse : public WorldObject
{
public:
    explicit Corpse(CorpseType type = CORPSE_BONES);
    ~Corpse();

    void AddToWorld() override;
    void RemoveFromWorld() override;

    void Update(uint32 diff, uint32 time);

    bool Create(uint32 guidlow);
    bool Create(uint32 guidlow, Player* owner);

    void SaveToDB();
    bool LoadFromDB(uint32 guid, Field* fields);

    void DeleteFromDB();

    ObjectGuid const& GetOwnerGuid() const
    {
        return GetGuidValue(CORPSE_FIELD_OWNER);
    }

    time_t const& GetGhostTime() const { return m_time; }
    void ResetGhostTime() { m_time = WorldTimer::time_no_syscall(); }
    CorpseType GetType() const { return m_type; }

    bool IsHostileTo(Unit const* unit) const override;
    bool IsFriendlyTo(Unit const* unit) const override;

    bool isVisibleForInState(Player const* u, WorldObject const* viewPoint,
        bool inVisibleList) const override;

    Loot loot; // remove insignia ONLY at BG
    Player* lootRecipient;
    bool lootForBody;

    bool IsExpired(time_t t) const;

    void OnLootOpen(LootType lootType, Player* looter) override;

    void SetOwnerLevel(uint32 level) { m_ownerLevel = level; }
    uint32 GetOwnerLevel() const { return m_ownerLevel; }

    void SetTime(time_t t) { m_time = t; }
    void SetType(CorpseType t) { m_type = t; }

private:
    CorpseType m_type;
    time_t m_time;
    uint32 m_ownerLevel;
};

#endif
