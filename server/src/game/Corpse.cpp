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

#include "Corpse.h"
#include "GossipDef.h"
#include "ObjectAccessor.h"
#include "ObjectGuid.h"
#include "ObjectMgr.h"
#include "Opcodes.h"
#include "Player.h"
#include "UpdateMask.h"
#include "World.h"
#include "loot_distributor.h"
#include "Database/DatabaseEnv.h"

Corpse::Corpse(CorpseType type) : WorldObject()
{
    m_objectType |= TYPEMASK_CORPSE;
    m_objectTypeId = TYPEID_CORPSE;
    // 2.3.2 - 0x58
    m_updateFlag =
        (UPDATEFLAG_LOWGUID | UPDATEFLAG_HIGHGUID | UPDATEFLAG_HAS_POSITION);

    m_valuesCount = CORPSE_END;

    m_type = type;

    m_time = WorldTimer::time_no_syscall();

    lootForBody = false;

    m_ownerLevel = 1;
}

Corpse::~Corpse()
{
    if (GetType() != CORPSE_BONES)
        sObjectAccessor::Instance()->RemoveCorpse(this);
}

void Corpse::AddToWorld()
{
    // Register corpse for GUID lookup
    if (!IsInWorld())
        sObjectAccessor::Instance()->AddObject(this);

    Object::AddToWorld();
}

void Corpse::RemoveFromWorld()
{
    // Deregister corpse from GUID lookup
    if (IsInWorld())
        sObjectAccessor::Instance()->RemoveObject(this);

    Object::RemoveFromWorld();
}

void Corpse::Update(uint32 /*diff*/, uint32 /*time*/)
{
    if (GetType() != CORPSE_BONES)
        return; // normal corpse removed in World::Update

    if (IsExpired(WorldTimer::time_no_syscall()))
        AddObjectToRemoveList();
}

bool Corpse::Create(uint32 guidlow)
{
    Object::_Create(guidlow, 0, HIGHGUID_CORPSE);
    return true;
}

bool Corpse::Create(uint32 guidlow, Player* owner)
{
    assert(owner);

    WorldObject::_Create(guidlow, HIGHGUID_CORPSE);
    if (auto corpse_safeloc = sObjectMgr::Instance()->GetCorpseSafeLoc(
            owner->GetX(), owner->GetY(), owner->GetZ(), owner->GetMapId(),
            owner->GetTeam()))
        Relocate(corpse_safeloc->x, corpse_safeloc->y, corpse_safeloc->z);
    else
        Relocate(owner->GetX(), owner->GetY(), owner->GetZ());
    SetOrientation(owner->GetO());

    // we need to assign owner's map for corpse
    // in other way we will get a crash in Corpse::SaveToDB()
    SetMap(owner->GetMap());

    if (!IsPositionValid())
    {
        logging.error(
            "Corpse (guidlow %d, owner %s) not created. Suggested coordinates "
            "isn't valid (X: %f Y: %f)",
            guidlow, owner->GetName(), owner->GetX(), owner->GetY());
        return false;
    }

    SetObjectScale(DEFAULT_OBJECT_SCALE);
    SetFloatValue(CORPSE_FIELD_POS_X, GetX());
    SetFloatValue(CORPSE_FIELD_POS_Y, GetY());
    SetFloatValue(CORPSE_FIELD_POS_Z, GetZ());
    SetFloatValue(CORPSE_FIELD_FACING, GetO());
    SetGuidValue(CORPSE_FIELD_OWNER, owner->GetObjectGuid());

    return true;
}

void Corpse::SaveToDB()
{
    // bones should not be saved to DB (would be deleted on startup anyway)
    assert(GetType() != CORPSE_BONES);

    // prevent DB data inconsistence problems and duplicates
    CharacterDatabase.BeginTransaction();
    DeleteFromDB();

    std::ostringstream ss;
    ss << "INSERT INTO corpse "
          "(guid,player,position_x,position_y,position_z,orientation,map,time,"
          "corpse_type,instance) VALUES (" << GetGUIDLow() << ", "
       << GetOwnerGuid().GetCounter() << ", " << GetX() << ", " << GetY()
       << ", " << GetZ() << ", " << GetO() << ", " << GetMapId() << ", "
       << uint64(m_time) << ", " << uint32(GetType()) << ", "
       << int(GetInstanceId()) << ")";
    CharacterDatabase.Execute(ss.str().c_str());
    CharacterDatabase.CommitTransaction();
}

void Corpse::DeleteFromDB()
{
    // bones should not be saved to DB (would be deleted on startup anyway)
    assert(GetType() != CORPSE_BONES);

    // all corpses (not bones)
    static SqlStatementID id;

    SqlStatement stmt = CharacterDatabase.CreateStatement(
        id, "DELETE FROM corpse WHERE player = ? AND corpse_type <> '0'");
    stmt.PExecute(GetOwnerGuid().GetCounter());
}

bool Corpse::LoadFromDB(uint32 lowguid, Field* fields)
{
    ////                                                    0            1
    /// 2                  3                  4                  5 6
    // QueryResult *result = CharacterDatabase.Query("SELECT corpse.guid,
    // player, corpse.position_x, corpse.position_y, corpse.position_z,
    // corpse.orientation, corpse.map,"
    ////   7     8            9         10      11    12     13           14
    /// 15              16       17
    //    "time, corpse_type, instance, gender, race, class, playerBytes,
    //    playerBytes2, equipmentCache, guildId, playerFlags FROM corpse"
    uint32 playerLowGuid = fields[1].GetUInt32();
    float positionX = fields[2].GetFloat();
    float positionY = fields[3].GetFloat();
    float positionZ = fields[4].GetFloat();
    float orientation = fields[5].GetFloat();
    uint32 mapid = fields[6].GetUInt32();

    Object::_Create(lowguid, 0, HIGHGUID_CORPSE);

    m_time = time_t(fields[7].GetUInt64());
    m_type = CorpseType(fields[8].GetUInt32());

    if (m_type >= MAX_CORPSE_TYPE)
    {
        logging.error("%s Owner %s have wrong corpse type (%i), not load.",
            GetGuidStr().c_str(), GetOwnerGuid().GetString().c_str(), m_type);
        return false;
    }

    uint32 instanceid = fields[9].GetUInt32();
    uint8 gender = fields[10].GetUInt8();
    uint8 race = fields[11].GetUInt8();
    uint8 _class = fields[12].GetUInt8();
    uint32 playerBytes = fields[13].GetUInt32();
    uint32 playerBytes2 = fields[14].GetUInt32();
    uint32 guildId = fields[16].GetUInt32();
    uint32 playerFlags = fields[17].GetUInt32();

    ObjectGuid guid = ObjectGuid(HIGHGUID_CORPSE, lowguid);
    ObjectGuid playerGuid = ObjectGuid(HIGHGUID_PLAYER, playerLowGuid);

    // overwrite possible wrong/corrupted guid
    SetGuidValue(OBJECT_FIELD_GUID, guid);
    SetGuidValue(CORPSE_FIELD_OWNER, playerGuid);

    SetObjectScale(DEFAULT_OBJECT_SCALE);

    PlayerInfo const* info =
        sObjectMgr::Instance()->GetPlayerInfo(race, _class);
    if (!info)
    {
        logging.error("Player %u has incorrect race/class pair.", GetGUIDLow());
        return false;
    }
    SetUInt32Value(CORPSE_FIELD_DISPLAY_ID,
        gender == GENDER_FEMALE ? info->displayId_f : info->displayId_m);

    // Load equipment
    Tokens data = StrSplit(fields[15].GetCppString(), " ");
    /* XXX */
    for (uint8 slot = inventory::equipment_start;
         slot < inventory::equipment_end; slot++)
    {
        uint32 visualbase = slot * 2;
        uint32 item_id = GetUInt32ValueFromArray(data, visualbase);
        const ItemPrototype* proto = ObjectMgr::GetItemPrototype(item_id);
        if (!proto)
        {
            SetUInt32Value(CORPSE_FIELD_ITEM + slot, 0);
            continue;
        }

        SetUInt32Value(CORPSE_FIELD_ITEM + slot,
            proto->DisplayInfoID | (proto->InventoryType << 24));
    }

    uint8 skin = (uint8)(playerBytes);
    uint8 face = (uint8)(playerBytes >> 8);
    uint8 hairstyle = (uint8)(playerBytes >> 16);
    uint8 haircolor = (uint8)(playerBytes >> 24);
    uint8 facialhair = (uint8)(playerBytes2);
    SetUInt32Value(CORPSE_FIELD_BYTES_1,
        ((0x00) | (race << 8) | (gender << 16) | (skin << 24)));
    SetUInt32Value(CORPSE_FIELD_BYTES_2,
        ((face) | (hairstyle << 8) | (haircolor << 16) | (facialhair << 24)));

    SetUInt32Value(CORPSE_FIELD_GUILD, guildId);

    uint32 flags = CORPSE_FLAG_UNK2;
    if (playerFlags & PLAYER_FLAGS_HIDE_HELM)
        flags |= CORPSE_FLAG_HIDE_HELM;
    if (playerFlags & PLAYER_FLAGS_HIDE_CLOAK)
        flags |= CORPSE_FLAG_HIDE_CLOAK;
    SetUInt32Value(CORPSE_FIELD_FLAGS, flags);

    // no need to mark corpse as lootable, because corpses are not saved in
    // battle grounds

    // place
    SetLocationInstanceId(instanceid);
    SetLocationMapId(mapid);
    Relocate(positionX, positionY, positionZ);
    SetOrientation(orientation);

    if (!IsPositionValid())
    {
        logging.error(
            "%s Owner %s not created. Suggested coordinates isn't valid (X: %f "
            "Y: %f)",
            GetGuidStr().c_str(), GetOwnerGuid().GetString().c_str(), GetX(),
            GetY());
        return false;
    }

    return true;
}

bool Corpse::isVisibleForInState(
    Player const* u, WorldObject const* viewPoint, bool inVisibleList) const
{
    return IsInWorld() && u->IsInWorld() &&
           IsWithinDistInMap(viewPoint,
               GetMap()->GetVisibilityDistance() +
                   (inVisibleList ? World::GetVisibleObjectGreyDistance() :
                                    0.0f),
               false);
}

bool Corpse::IsHostileTo(Unit const* unit) const
{
    if (Player* owner = sObjectMgr::Instance()->GetPlayer(GetOwnerGuid()))
        return owner->IsHostileTo(unit);
    else
        return false;
}

bool Corpse::IsFriendlyTo(Unit const* unit) const
{
    if (Player* owner = sObjectMgr::Instance()->GetPlayer(GetOwnerGuid()))
        return owner->IsFriendlyTo(unit);
    else
        return true;
}

bool Corpse::IsExpired(time_t t) const
{
    if (m_type == CORPSE_BONES)
    {
        // two minutes in BGs, significantly longer elsewhere
        time_t expiry = 2 * MINUTE;
        if (IsInWorld() && !GetMap()->IsBattleGroundOrArena())
            expiry = 1 * HOUR * MINUTE;

        return m_time < t - expiry;
    }
    else
        return m_time < t - 3 * DAY;
}

void Corpse::OnLootOpen(LootType lootType, Player* looter)
{
    if (m_lootDistributor || lootType != LOOT_INSIGNIA)
        return;

    m_lootDistributor = new loot_distributor(this, LOOT_INSIGNIA);
    m_lootDistributor->generate_loot(looter);
}
