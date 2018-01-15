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

#include "Group.h"
#include "BattleGround.h"
#include "Common.h"
#include "Formulas.h"
#include "LootMgr.h"
#include "MapManager.h"
#include "MapPersistentStateMgr.h"
#include "ObjectAccessor.h"
#include "ObjectGuid.h"
#include "ObjectMgr.h"
#include "Opcodes.h"
#include "Player.h"
#include "Util.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "lfg_tool_container.h"

//===================================================
//============== Group ==============================
//===================================================

Group::Group()
  : m_Id(0), m_groupType(GROUPTYPE_NORMAL), m_difficulty(REGULAR_DIFFICULTY),
    m_bgGroup(nullptr), m_lootMethod(FREE_FOR_ALL),
    m_lootThreshold(ITEM_QUALITY_UNCOMMON), m_subGroupsCounts(nullptr),
    m_currentLootIndex(0)
{
}

Group::~Group()
{
    if (m_bgGroup)
    {
        LOG_DEBUG(logging, "Group::~Group: battleground group being deleted.");
        if (m_bgGroup->GetBgRaid(ALLIANCE) == this)
            m_bgGroup->SetBgRaid(ALLIANCE, nullptr);
        else if (m_bgGroup->GetBgRaid(HORDE) == this)
            m_bgGroup->SetBgRaid(HORDE, nullptr);
        else
            logging.error(
                "Group::~Group: battleground group is not linked to the "
                "correct battleground.");
    }

    for (auto& elem : m_instanceBinds)
        for (auto itr = elem.begin(); itr != elem.begin(); ++itr)
            if (auto state = itr->second.state.lock())
                state->UnbindGroup(this);

    // Sub group counters clean up
    if (m_subGroupsCounts)
        delete[] m_subGroupsCounts;
}

bool Group::Create(ObjectGuid guid, const char* name)
{
    m_leaderGuid = guid;
    m_leaderName = name;

    m_groupType = isBGGroup() ? GROUPTYPE_RAID : GROUPTYPE_NORMAL;

    if (m_groupType == GROUPTYPE_RAID)
        _initRaidSubGroupsCounter();

    m_lootMethod = GROUP_LOOT;
    m_lootThreshold = ITEM_QUALITY_UNCOMMON;
    m_looterGuid = guid;

    m_difficulty = DUNGEON_DIFFICULTY_NORMAL;
    if (!isBGGroup())
    {
        m_Id = sObjectMgr::Instance()->GenerateGroupLowGuid();

        Player* leader = sObjectMgr::Instance()->GetPlayer(guid);
        if (leader)
            m_difficulty = leader->GetDifficulty();

        // store group in database
        CharacterDatabase.BeginTransaction();
        CharacterDatabase.PExecute(
            "DELETE FROM groups WHERE groupId ='%u'", m_Id);
        CharacterDatabase.PExecute(
            "DELETE FROM group_member WHERE groupId ='%u'", m_Id);

        CharacterDatabase.PExecute(
            "INSERT INTO "
            "groups(groupId,leaderGuid,mainTank,mainAssistant,lootMethod,"
            "looterGuid,lootThreshold,icon1,icon2,icon3,icon4,icon5,icon6,"
            "icon7,icon8,isRaid,difficulty) "
            "VALUES('%u','%u','%u','%u','%u','%u','%u','" UI64FMTD
            "','" UI64FMTD "','" UI64FMTD "','" UI64FMTD "','" UI64FMTD
            "','" UI64FMTD "','" UI64FMTD "','" UI64FMTD "','%u','%u')",
            m_Id, m_leaderGuid.GetCounter(), m_mainTankGuid.GetCounter(),
            m_mainAssistantGuid.GetCounter(), uint32(m_lootMethod),
            m_looterGuid.GetCounter(), uint32(m_lootThreshold),
            m_targetIcons[0].GetRawValue(), m_targetIcons[1].GetRawValue(),
            m_targetIcons[2].GetRawValue(), m_targetIcons[3].GetRawValue(),
            m_targetIcons[4].GetRawValue(), m_targetIcons[5].GetRawValue(),
            m_targetIcons[6].GetRawValue(), m_targetIcons[7].GetRawValue(),
            isRaidGroup(), m_difficulty);
    }

    if (!AddMember(guid, name))
        return false;

    // Adopt creators instance binds when group is created
    if (!isBGGroup())
    {
        if (Player* creator = ObjectAccessor::FindPlayer(guid))
        {
            for (int i = 0; i < MAX_DIFFICULTY; ++i)
            {
                auto& map = creator->GetInstanceBindsMap((Difficulty)i);
                for (auto bind : map)
                    if (auto state = bind.second.state.lock())
                        BindToInstance(state, bind.second.perm);
            }
        }

        CharacterDatabase.CommitTransaction();
    }

    return true;
}

bool Group::LoadGroupFromDB(Field* fields)
{
    //                                          0         1              2
    //                                          3           4              5
    //                                          6      7      8      9      10
    //                                          11     12     13      14
    //                                          15          16
    // result = CharacterDatabase.Query("SELECT mainTank, mainAssistant,
    // lootMethod, looterGuid, lootThreshold, icon1, icon2, icon3, icon4, icon5,
    // icon6, icon7, icon8, isRaid, difficulty, leaderGuid, groupId FROM
    // groups");

    m_Id = fields[16].GetUInt32();
    m_leaderGuid = ObjectGuid(HIGHGUID_PLAYER, fields[15].GetUInt32());

    // group leader not exist
    if (!sObjectMgr::Instance()->GetPlayerNameByGUID(
            m_leaderGuid, m_leaderName))
        return false;

    m_groupType = fields[13].GetBool() ? GROUPTYPE_RAID : GROUPTYPE_NORMAL;

    if (m_groupType == GROUPTYPE_RAID)
        _initRaidSubGroupsCounter();

    uint32 diff = fields[14].GetUInt8();
    if (diff >= MAX_DIFFICULTY)
        diff = DUNGEON_DIFFICULTY_NORMAL;

    m_difficulty = Difficulty(diff);
    m_mainTankGuid = ObjectGuid(HIGHGUID_PLAYER, fields[0].GetUInt32());
    m_mainAssistantGuid = ObjectGuid(HIGHGUID_PLAYER, fields[1].GetUInt32());
    m_lootMethod = LootMethod(fields[2].GetUInt8());
    m_looterGuid = ObjectGuid(HIGHGUID_PLAYER, fields[3].GetUInt32());
    m_lootThreshold = ItemQualities(fields[4].GetUInt16());

    for (int i = 0; i < TARGET_ICON_COUNT; ++i)
        m_targetIcons[i] = ObjectGuid(fields[5 + i].GetUInt64());

    return true;
}

bool Group::LoadMemberFromDB(uint32 guidLow, uint8 subgroup, bool assistant)
{
    MemberSlot member;
    member.guid = ObjectGuid(HIGHGUID_PLAYER, guidLow);

    // skip nonexistent member
    if (!sObjectMgr::Instance()->GetPlayerNameByGUID(member.guid, member.name))
        return false;

    member.group = subgroup;
    member.assistant = assistant;
    m_memberSlots.push_back(member);

    SubGroupCounterIncrease(subgroup);
    m_lootIndexes[member.guid] = m_currentLootIndex;

    return true;
}

void Group::ConvertToRaid()
{
    m_groupType = GROUPTYPE_RAID;

    _initRaidSubGroupsCounter();

    if (!isBGGroup())
        CharacterDatabase.PExecute(
            "UPDATE groups SET isRaid = 1 WHERE groupId='%u'", m_Id);
    SendUpdate();

    // update quest related GO states (quest activity dependent from raid
    // membership)
    for (member_citerator citr = m_memberSlots.begin();
         citr != m_memberSlots.end(); ++citr)
        if (Player* player = sObjectMgr::Instance()->GetPlayer(citr->guid))
            player->UpdateForQuestWorldObjects();
}

bool Group::AddInvite(Player* player)
{
    if (!player || player->GetGroupInvite())
        return false;
    Group* group = player->GetGroup();
    if (group && group->isBGGroup())
        group = player->GetOriginalGroup();
    if (group)
        return false;

    RemoveInvite(player);

    m_invitees.insert(player);

    player->SetGroupInvite(this);

    return true;
}

bool Group::AddLeaderInvite(Player* player)
{
    if (!AddInvite(player))
        return false;

    m_leaderGuid = player->GetObjectGuid();
    m_leaderName = player->GetName();
    return true;
}

uint32 Group::RemoveInvite(Player* player)
{
    m_invitees.erase(player);

    player->SetGroupInvite(nullptr);
    return GetMembersCount();
}

void Group::RemoveAllInvites()
{
    for (const auto& elem : m_invitees)
        (elem)->SetGroupInvite(nullptr);

    m_invitees.clear();
}

Player* Group::GetInvited(ObjectGuid guid) const
{
    for (const auto& elem : m_invitees)
        if ((elem)->GetObjectGuid() == guid)
            return (elem);

    return nullptr;
}

Player* Group::GetInvited(const std::string& name) const
{
    for (const auto& elem : m_invitees)
    {
        if ((elem)->GetName() == name)
            return (elem);
    }
    return nullptr;
}

bool Group::AddMember(ObjectGuid guid, const char* name)
{
    if (!_addMember(guid, name))
        return false;

    SendUpdate();

    if (Player* player = sObjectMgr::Instance()->GetPlayer(guid))
    {
        if (!IsLeader(player->GetObjectGuid()) && !isBGGroup())
        {
            player->UpdateInstanceBindsOnGroupJoinLeave();

            if (player->getLevel() >= LEVELREQUIREMENT_HEROIC &&
                player->GetDifficulty() != GetDifficulty())
            {
                player->SetDifficulty(GetDifficulty());
                player->SendDungeonDifficulty(true);
            }
        }
        player->SetGroupUpdateFlag(GROUP_UPDATE_FULL);
        UpdatePlayerOutOfRange(player);

        // quest related GO state dependent from raid membership
        if (isRaidGroup())
            player->UpdateForQuestWorldObjects();

        m_lootIndexes[player->GetObjectGuid()] = m_currentLootIndex;

        // Force health to be resent (percentage -> value)
        player->resend_health();
    }

    return true;
}

uint32 Group::RemoveMember(ObjectGuid guid, uint8 method)
{
    // remove member and change leader (if need) only if strong more 2 members
    // _before_ member remove
    if (GetMembersCount() >
        uint32(isBGGroup() ? 1 : 2)) // in BG group case allow 1 members group
    {
        bool leaderChanged = _removeMember(guid);

        if (Player* player = sObjectMgr::Instance()->GetPlayer(guid))
        {
            // quest related GO state dependent from raid membership
            if (isRaidGroup())
                player->UpdateForQuestWorldObjects();

            if (method == 1)
            {
                WorldPacket data(SMSG_GROUP_UNINVITE, 0);
                player->GetSession()->send_packet(std::move(data));
            }

            // we already removed player from group and in player->GetGroup() is
            // his original group!
            if (Group* group = player->GetGroup())
            {
                group->SendUpdate();
            }
            else
            {
                WorldPacket data(SMSG_GROUP_LIST, 24);
                data << uint64(0) << uint64(0) << uint64(0);
                player->GetSession()->send_packet(std::move(data));
            }

            // Force health to be resent (value -> percentage)
            player->resend_health();
        }

        if (leaderChanged)
        {
            WorldPacket data(
                SMSG_GROUP_SET_LEADER, (m_memberSlots.front().name.size() + 1));
            data << m_memberSlots.front().name;
            BroadcastPacket(&data, true);
        }

        SendUpdate();
    }
    // if group before remove <= 2 disband it
    else
    {
        // The guy being removed should clear his instance binds, but the last
        // guy should not
        if (!isBGGroup())
            if (Player* player = ObjectAccessor::FindPlayer(guid))
                player->UpdateInstanceBindsOnGroupJoinLeave();

        Disband(false);
    }

    return m_memberSlots.size();
}

void Group::ChangeLeader(ObjectGuid guid)
{
    auto slot = _getMemberCSlot(guid);
    if (slot == m_memberSlots.end())
        return;

    _setLeader(guid);

    WorldPacket data(SMSG_GROUP_SET_LEADER, slot->name.size() + 1);
    data << slot->name;
    BroadcastPacket(&data, true);
    SendUpdate();
}

void Group::Disband(bool hideDestroy)
{
    Player* player;

    for (member_citerator citr = m_memberSlots.begin();
         citr != m_memberSlots.end(); ++citr)
    {
        player = sObjectMgr::Instance()->GetPlayer(citr->guid);
        if (!player)
            continue;

        // we cannot call _removeMember because it would invalidate member
        // iterator
        // if we are removing player from battleground raid
        if (isBGGroup())
            player->RemoveFromBattleGroundRaid();
        else
        {
            // we can remove player who is in battleground from his original
            // group
            if (player->GetOriginalGroup() == this)
                player->SetOriginalGroup(nullptr);
            else
                player->SetGroup(nullptr);
        }

        // quest related GO state dependent from raid membership
        if (isRaidGroup())
            player->UpdateForQuestWorldObjects();

        if (!player->GetSession())
            continue;

        if (!hideDestroy)
        {
            WorldPacket data(SMSG_GROUP_DESTROYED, 0);
            player->GetSession()->send_packet(std::move(data));
        }

        // we already removed player from group and in player->GetGroup() is his
        // original group, send update
        if (Group* group = player->GetGroup())
        {
            group->SendUpdate();
        }
        else
        {
            WorldPacket data(SMSG_GROUP_LIST, 24);
            data << uint64(0) << uint64(0) << uint64(0);
            player->GetSession()->send_packet(std::move(data));
        }

        // Force health to be resent (value -> percentage)
        player->resend_health();
    }
    m_memberSlots.clear();

    RemoveAllInvites();

    if (!isBGGroup())
    {
        CharacterDatabase.BeginTransaction();
        CharacterDatabase.PExecute(
            "DELETE FROM groups WHERE groupId='%u'", m_Id);
        CharacterDatabase.PExecute(
            "DELETE FROM group_member WHERE groupId='%u'", m_Id);
        CharacterDatabase.CommitTransaction();

        // Clear group binds
        for (auto& elem : m_instanceBinds)
        {
            // UnbindFromInstance modifies m_instanceBinds, make a copy first
            BoundInstancesMap copy(elem.begin(), elem.end());
            for (auto p : copy)
                if (auto state = p.second.state.lock())
                    UnbindFromInstance(state.get());
        }
    }

    m_leaderGuid.Clear();
    m_leaderName = "";
}

void Group::ClearTargetIcon(ObjectGuid target)
{
    for (int i = 0; i < TARGET_ICON_COUNT; ++i)
        if (m_targetIcons[i] == target)
        {
            std::lock_guard<std::mutex> guard(group_mutex_);
            _setTargetIcon(i, ObjectGuid());
            return;
        }
}

void Group::SetTargetIcon(uint8 id, ObjectGuid targetGuid)
{
    std::lock_guard<std::mutex> guard(group_mutex_);

    if (id >= TARGET_ICON_COUNT)
        return;

    _setTargetIcon(id, targetGuid);
}

// Must hold mutex to call
void Group::_setTargetIcon(uint8 id, ObjectGuid targetGuid)
{
    // clean other icons
    if (targetGuid)
        for (int i = 0; i < TARGET_ICON_COUNT; ++i)
            if (m_targetIcons[i] == targetGuid)
                _setTargetIcon(i, ObjectGuid());

    m_targetIcons[id] = targetGuid;

    WorldPacket data(MSG_RAID_TARGET_UPDATE, (1 + 1 + 8));
    data << uint8(0); // 0 - send one target, 1 - send full list (gaps possible;
                      // zerod out in client)
    data << uint8(id);
    data << targetGuid;
    BroadcastPacket(&data, true);
}

static void GetDataForXPAtKill_helper(Player* player, Unit const* victim,
    uint32& sum_level, Player*& member_with_max_level,
    Player*& not_gray_member_with_max_level)
{
    sum_level += player->getLevel();
    if (!member_with_max_level ||
        member_with_max_level->getLevel() < player->getLevel())
        member_with_max_level = player;

    uint32 gray_level = MaNGOS::XP::GetGrayLevel(player->getLevel());
    if (victim->getLevel() > gray_level &&
        (!not_gray_member_with_max_level ||
            not_gray_member_with_max_level->getLevel() < player->getLevel()))
        not_gray_member_with_max_level = player;
}

void Group::GetDataForXPAtKill(Unit const* victim, uint32& count,
    uint32& sum_level, Player*& member_with_max_level,
    Player*& not_gray_member_with_max_level, Player* additional)
{
    for (auto member : members(true))
    {
        if (!member->isAlive())
            continue;

        // will proccesed later
        if (member == additional)
            continue;

        if (!member->IsAtGroupRewardDistance(victim)) // at req. distance
            continue;

        ++count;
        GetDataForXPAtKill_helper(member, victim, sum_level,
            member_with_max_level, not_gray_member_with_max_level);
    }

    if (additional)
    {
        if (additional->IsAtGroupRewardDistance(victim)) // at req. distance
        {
            ++count;
            GetDataForXPAtKill_helper(additional, victim, sum_level,
                member_with_max_level, not_gray_member_with_max_level);
        }
    }
}

void Group::SendTargetIconList(WorldSession* session)
{
    if (!session)
        return;

    WorldPacket data(MSG_RAID_TARGET_UPDATE, (1 + TARGET_ICON_COUNT * 9));
    data << uint8(1); // 1 - send full list (gaps possible; zerod out in
                      // client), 0 - send one target

    for (int i = 0; i < TARGET_ICON_COUNT; ++i)
    {
        if (!m_targetIcons[i])
            continue;

        data << uint8(i);
        data << m_targetIcons[i];
    }

    session->send_packet(std::move(data));
}

void Group::SendUpdate()
{
    for (auto player : members(true, true))
    {
        if (!player->GetSession())
            continue;
        auto citr = _getMemberCSlot(player->GetObjectGuid());
        uint8 group = 0;
        uint8 flags = 0;
        bool spy = std::find(m_spies.begin(), m_spies.end(),
                       player->GetObjectGuid()) != m_spies.end();
        if (citr != m_memberSlots.end())
        {
            group = citr->group;
            flags = citr->assistant ? 0x01 : 0x00;
        }
        else
            group = player->spy_subgroup_;

        // guess size
        WorldPacket data(
            SMSG_GROUP_LIST, (1 + 1 + 1 + 1 + 8 + 4 + GetMembersCount() * 20));
        data << uint8(!spy ? m_groupType : GROUPTYPE_RAID); // group type
        data << uint8(isBGGroup() ? 1 : 0); // 2.0.x, isBattleGroundGroup?
        data << uint8(group);               // groupid
        data << uint8(flags);               // 0x2 main assist, 0x4 main tank
        data << GetObjectGuid();            // group guid
        if (spy)
            data << uint32(GetMembersCount());
        else
            data << uint32(GetMembersCount() - 1);
        for (member_citerator citr2 = m_memberSlots.begin();
             citr2 != m_memberSlots.end(); ++citr2)
        {
            if (player->GetObjectGuid() == citr2->guid)
                continue;
            Player* member = sObjectMgr::Instance()->GetPlayer(citr2->guid);
            uint8 onlineState =
                (member && !member->GetSession()->PlayerLogout()) ?
                    MEMBER_STATUS_ONLINE :
                    MEMBER_STATUS_OFFLINE;
            onlineState = onlineState | ((isBGGroup()) ? MEMBER_STATUS_PVP : 0);

            data << citr2->name;
            data << citr2->guid;
            // online-state
            data << uint8(onlineState);
            data << uint8(citr2->group); // groupid
            data << uint8(
                citr2->assistant ? 0x01 : 0); // 0x2 main assist, 0x4 main tank
        }

        data << m_leaderGuid; // leader guid
        if (GetMembersCount() - 1)
        {
            data << uint8(m_lootMethod); // loot method
            data << (m_lootMethod != MASTER_LOOT ? ObjectGuid() :
                                                   m_looterGuid); // looter guid
            data << uint8(m_lootThreshold); // loot threshold
            data << uint8(m_difficulty);    // Heroic Mod Group
        }
        player->GetSession()->send_packet(std::move(data));
    }
}

void Group::UpdatePlayerOutOfRange(Player* pPlayer)
{
    if (!pPlayer || !pPlayer->IsInWorld())
        return;

    if (pPlayer->GetGroupUpdateFlag() == GROUP_UPDATE_FLAG_NONE)
        return;

    WorldPacket data;
    pPlayer->GetSession()->BuildPartyMemberStatsChangedPacket(pPlayer, &data);

    for (auto player : members(false))
        if (player != pPlayer && !player->HaveAtClient(pPlayer))
            player->GetSession()->send_packet(&data);
}

void Group::BroadcastPacket(WorldPacket* packet, bool ignorePlayersInBGRaid,
    int group, ObjectGuid ignore, WorldObject* source, float maximum_dist)
{
    for (auto pl : members(false, true))
    {
        bool spy = std::find(m_spies.begin(), m_spies.end(),
                       pl->GetObjectGuid()) != m_spies.end();

        if ((ignore && pl->GetObjectGuid() == ignore) ||
            (ignorePlayersInBGRaid && pl->GetGroup() != this && !spy))
            continue;

        if (maximum_dist > 0.0f && source &&
            !source->IsWithinDistInMap(pl, 200.0f))
            continue;

        auto subgroup = pl->GetSubGroup();
        if (spy)
            subgroup =
                m_groupType == GROUPTYPE_NORMAL ? group : pl->spy_subgroup_;

        if (pl->GetSession() && (group == -1 || subgroup == group))
            pl->GetSession()->send_packet(packet);
    }
}

void Group::BroadcastReadyCheck(WorldPacket* packet)
{
    for (auto pl : members(false))
    {
        if (pl->GetSession())
            if (IsLeader(pl->GetObjectGuid()) ||
                IsAssistant(pl->GetObjectGuid()))
                pl->GetSession()->send_packet(packet);
    }
}

void Group::OfflineReadyCheck()
{
    for (member_citerator citr = m_memberSlots.begin();
         citr != m_memberSlots.end(); ++citr)
    {
        Player* pl = sObjectMgr::Instance()->GetPlayer(citr->guid);
        if (!pl || !pl->GetSession())
        {
            WorldPacket data(MSG_RAID_READY_CHECK_CONFIRM, 9);
            data << citr->guid;
            data << uint8(0);
            BroadcastReadyCheck(&data);
        }
    }
}

bool Group::_addMember(ObjectGuid guid, const char* name, bool isAssistant)
{
    // get first not-full group
    uint8 groupid = 0;
    if (m_subGroupsCounts)
    {
        bool groupFound = false;
        for (; groupid < MAX_RAID_SUBGROUPS; ++groupid)
        {
            if (m_subGroupsCounts[groupid] < MAX_GROUP_SIZE)
            {
                groupFound = true;
                break;
            }
        }
        // We are raid group and no one slot is free
        if (!groupFound)
            return false;
    }

    return _addMember(guid, name, isAssistant, groupid);
}

bool Group::_addMember(
    ObjectGuid guid, const char* name, bool isAssistant, uint8 group)
{
    if (IsFull())
        return false;

    if (!guid)
        return false;

    Player* player = sObjectMgr::Instance()->GetPlayer(guid, false);

    MemberSlot member;
    member.guid = guid;
    member.name = name;
    member.group = group;
    member.assistant = isAssistant;
    m_memberSlots.push_back(member);

    SubGroupCounterIncrease(group);

    if (player)
    {
        player->SetGroupInvite(nullptr);
        // if player is in group and he is being added to BG raid group, then
        // call SetBattleGroundRaid()
        if (player->GetGroup() && isBGGroup())
            player->SetBattleGroundRaid(this, group);
        // if player is in bg raid and we are adding him to normal group, then
        // call SetOriginalGroup()
        else if (player->GetGroup())
            player->SetOriginalGroup(this, group);
        // if player is not in group, then call set group
        else
            player->SetGroup(this, group);
    }

    if (!isBGGroup())
    {
        // insert into group table
        CharacterDatabase.PExecute(
            "INSERT INTO group_member(groupId,memberGuid,assistant,subgroup) "
            "VALUES('%u','%u','%u','%u')",
            m_Id, member.guid.GetCounter(), ((member.assistant == 1) ? 1 : 0),
            member.group);
    }

    return true;
}

bool Group::_removeMember(ObjectGuid guid)
{
    Player* player = sObjectMgr::Instance()->GetPlayer(guid);
    if (player)
    {
        // if we are removing player from battleground raid
        if (isBGGroup())
            player->RemoveFromBattleGroundRaid();
        else
        {
            // we can remove player who is in battleground from his original
            // group
            if (player->GetOriginalGroup() == this)
                player->SetOriginalGroup(nullptr);
            else
                player->SetGroup(nullptr);

            player->UpdateInstanceBindsOnGroupJoinLeave();
        }
    }

    auto slot = _getMemberWSlot(guid);
    if (slot != m_memberSlots.end())
    {
        SubGroupCounterDecrease(slot->group);

        m_memberSlots.erase(slot);
    }

    if (!isBGGroup())
        CharacterDatabase.PExecute(
            "DELETE FROM group_member WHERE memberGuid='%u'",
            guid.GetCounter());

    if (m_leaderGuid == guid) // leader was removed
    {
        if (GetMembersCount() > 0)
            _setLeader(m_memberSlots.front().guid);
        return true;
    }

    return false;
}

void Group::_setLeader(ObjectGuid guid)
{
    auto slot = _getMemberCSlot(guid);
    if (slot == m_memberSlots.end())
        return;

    if (!isBGGroup())
    {
        // update the group leader
        uint32 slot_lowguid = slot->guid.GetCounter();
        CharacterDatabase.PExecute(
            "UPDATE groups SET leaderGuid='%u' WHERE groupId='%u'",
            slot_lowguid, m_Id);

        // Switch leader in LFG tool
        if (auto player = ObjectAccessor::FindPlayer(m_leaderGuid, false))
        {
            if (auto session = player->GetSession())
            {
                Player* new_leader =
                    ObjectAccessor::FindPlayer(slot->guid, false);
                if (new_leader && new_leader->GetSession())
                    sLfgToolContainer::Instance()->group_leader_switch(
                        session, new_leader->GetSession());
                else
                    sLfgToolContainer::Instance()->remove(session);
            }
        }
    }

    m_leaderGuid = slot->guid;
    m_leaderName = slot->name;
}

bool Group::_setMembersGroup(ObjectGuid guid, uint8 group)
{
    auto slot = _getMemberWSlot(guid);
    if (slot == m_memberSlots.end())
        return false;

    slot->group = group;

    SubGroupCounterIncrease(group);

    if (!isBGGroup())
        CharacterDatabase.PExecute(
            "UPDATE group_member SET subgroup='%u' WHERE memberGuid='%u'",
            group, guid.GetCounter());

    return true;
}

bool Group::_setAssistantFlag(ObjectGuid guid, const bool& state)
{
    auto slot = _getMemberWSlot(guid);
    if (slot == m_memberSlots.end())
        return false;

    slot->assistant = state;
    if (!isBGGroup())
        CharacterDatabase.PExecute(
            "UPDATE group_member SET assistant='%u' WHERE memberGuid='%u'",
            (state == true) ? 1 : 0, guid.GetCounter());
    return true;
}

bool Group::_setMainTank(ObjectGuid guid)
{
    if (m_mainTankGuid == guid)
        return false;

    if (guid)
    {
        auto slot = _getMemberCSlot(guid);
        if (slot == m_memberSlots.end())
            return false;

        if (m_mainAssistantGuid == guid)
            _setMainAssistant(ObjectGuid());
    }

    m_mainTankGuid = guid;

    if (!isBGGroup())
        CharacterDatabase.PExecute(
            "UPDATE groups SET mainTank='%u' WHERE groupId='%u'",
            m_mainTankGuid.GetCounter(), m_Id);

    return true;
}

bool Group::_setMainAssistant(ObjectGuid guid)
{
    if (m_mainAssistantGuid == guid)
        return false;

    if (guid)
    {
        auto slot = _getMemberWSlot(guid);
        if (slot == m_memberSlots.end())
            return false;

        if (m_mainTankGuid == guid)
            _setMainTank(ObjectGuid());
    }

    m_mainAssistantGuid = guid;

    if (!isBGGroup())
        CharacterDatabase.PExecute(
            "UPDATE groups SET mainAssistant='%u' WHERE groupId='%u'",
            m_mainAssistantGuid.GetCounter(), m_Id);

    return true;
}

bool Group::SameSubGroup(Player const* member1, Player const* member2) const
{
    if (!member1 || !member2)
        return false;
    if (member1->GetGroup() != this || member2->GetGroup() != this)
        return false;
    else
        return member1->GetSubGroup() == member2->GetSubGroup();
}

// allows setting subgroup for offline members
void Group::ChangeMembersGroup(ObjectGuid guid, uint8 group)
{
    if (!isRaidGroup() || !IsMember(guid))
        return;

    Player* player = sObjectMgr::Instance()->GetPlayer(guid);

    if (!player)
    {
        uint8 prevSubGroup = GetMemberGroup(guid);
        if (prevSubGroup == group)
            return;

        if (_setMembersGroup(guid, group))
        {
            SubGroupCounterDecrease(prevSubGroup);
            SendUpdate();
        }
    }
    else
        // This methods handles itself groupcounter decrease
        ChangeMembersGroup(player, group);
}

// only for online members
void Group::ChangeMembersGroup(Player* player, uint8 group)
{
    if (!player || !isRaidGroup() || !IsMember(player->GetObjectGuid()))
        return;

    uint8 prevSubGroup = player->GetSubGroup();
    if (prevSubGroup == group)
        return;

    if (_setMembersGroup(player->GetObjectGuid(), group))
    {
        if (player->GetGroup() == this)
            player->GetGroupRef().setSubGroup(group);
        // if player is in BG raid, it is possible that he is also in normal
        // raid - and that normal raid is stored in m_originalGroup reference
        else
        {
            prevSubGroup = player->GetOriginalSubGroup();
            player->GetOriginalGroupRef().setSubGroup(group);
        }
        SubGroupCounterDecrease(prevSubGroup);

        SendUpdate();
    }
}

void Group::SwapMembers(ObjectGuid one, ObjectGuid two)
{
    if (!isRaidGroup() || !IsMember(one) || !IsMember(two))
        return;

    auto one_slot = _getMemberWSlot(one);
    auto two_slot = _getMemberWSlot(two);
    if (one_slot == m_memberSlots.end() || two_slot == m_memberSlots.end())
        return;

    if (one_slot->group == two_slot->group)
        return;

    std::swap(one_slot->group, two_slot->group);

    if (!isBGGroup())
    {
        CharacterDatabase.PExecute(
            "UPDATE group_member SET subgroup='%u' WHERE memberGuid='%u'",
            one_slot->group, one.GetCounter());
        CharacterDatabase.PExecute(
            "UPDATE group_member SET subgroup='%u' WHERE memberGuid='%u'",
            two_slot->group, two.GetCounter());
    }

    // Lambda to change group references for online player
    auto update_ref = [this](ObjectGuid guid, uint8 group)
    {
        Player* player = sObjectMgr::Instance()->GetPlayer(guid);
        if (!player)
            return;
        if (player->GetGroup() == this)
            player->GetGroupRef().setSubGroup(group);
        // if player is in BG raid, it is possible that he is also in normal
        // raid - and that normal raid is stored in m_originalGroup reference
        else
            player->GetOriginalGroupRef().setSubGroup(group);
    };

    update_ref(one, one_slot->group);
    update_ref(two, two_slot->group);

    SendUpdate();

    return;
}

void Group::SetDifficulty(Difficulty difficulty)
{
    m_difficulty = difficulty;
    if (!isBGGroup())
        CharacterDatabase.PExecute(
            "UPDATE groups SET difficulty = %u WHERE groupId='%u'",
            m_difficulty, m_Id);

    for (auto player : members(false))
    {
        if (!player->GetSession() ||
            player->getLevel() < LEVELREQUIREMENT_HEROIC)
            continue;
        player->SetDifficulty(difficulty);
        player->SendDungeonDifficulty(true);
    }
}

bool Group::InCombatToInstance(uint32 instanceId)
{
    for (auto player : members(true))
    {
        if (player->getAttackers().size() &&
            player->GetInstanceId() == instanceId)
            return true;
    }
    return false;
}

void Group::ResetInstances(InstanceResetMethod method, Player* SendMsgTo)
{
    if (isBGGroup())
        return;

    // method can be INSTANCE_RESET_ALL, INSTANCE_RESET_CHANGE_DIFFICULTY,
    // INSTANCE_RESET_GROUP_DISBAND

    // First we check if we have any offline or zoning players, everything will
    // fail if we do
    bool haveOffline = false;
    bool haveZoning = false;
    for (member_citerator itr = m_memberSlots.begin();
         itr != m_memberSlots.end(); ++itr)
    {
        Player* member = sObjectMgr::Instance()->GetPlayer(itr->guid, false);
        if (!member)
        {
            haveOffline = true;
            break;
        }
        else if (!member->IsInWorld())
        {
            haveZoning = true;
            break;
        }
    }

    for (int d = DUNGEON_DIFFICULTY_NORMAL; d <= DUNGEON_DIFFICULTY_HEROIC; d++)
    {
        Difficulty diff = static_cast<Difficulty>(d);

        for (auto itr = m_instanceBinds[diff].begin();
             itr != m_instanceBinds[diff].end();)
        {
            auto state = itr->second.state.lock();
            if (!state)
            {
                itr = m_instanceBinds[diff].erase(itr);
                continue;
            }

            const MapEntry* entry = sMapStore.LookupEntry(itr->first);
            if (!entry || !state->CanReset())
            {
                ++itr;
                continue;
            }

            // Skip past heroic and raid maps if we reset all instances
            if (method == INSTANCE_RESET_ALL &&
                (entry->map_type == MAP_RAID ||
                    diff == DUNGEON_DIFFICULTY_HEROIC))
            {
                ++itr;
                continue;
            }

            // Check if the map has any players in it
            // The reason to split this off from the Reset call below is so we
            // can
            // check if maps that can't be reset (raids) have players in them
            // too
            Map* map = sMapMgr::Instance()->FindMap(
                state->GetMapId(), state->GetInstanceId());
            bool havePlayers = map ? map->HavePlayers() : false;

            // Reset the map unless we have zoning or offline players
            if (map && map->IsDungeon() &&
                !(method == INSTANCE_RESET_GROUP_DISBAND && !state->CanReset()))
            {
                if (!havePlayers && !haveOffline && !haveZoning)
                    ((DungeonMap*)map)->Reset(method);

                // If there are any players inside the dungeon warn them the
                // leader is trying to reset the map
                if (havePlayers && method != INSTANCE_RESET_GROUP_DISBAND)
                    ((DungeonMap*)map)->SendResetFailedNotify();
            }

            if (SendMsgTo)
            {
                if (havePlayers)
                    SendMsgTo->SendResetInstanceFailed(
                        INSTANCE_RESET_FAIL_INSIDE, state->GetMapId());
                else if (haveOffline)
                    SendMsgTo->SendResetInstanceFailed(
                        INSTANCE_RESET_FAIL_OFFLINE, state->GetMapId());
                else if (haveZoning)
                    SendMsgTo->SendResetInstanceFailed(
                        INSTANCE_RESET_FAIL_ZONING, state->GetMapId());
                else
                    SendMsgTo->SendResetInstanceSuccess(state->GetMapId());
            }

            if ((!havePlayers && !haveOffline && !haveZoning) ||
                method == INSTANCE_RESET_GROUP_DISBAND)
            {
                CharacterDatabase.PExecute(
                    "DELETE FROM group_instance WHERE instance = '%u'",
                    state->GetInstanceId());
                itr = m_instanceBinds[diff].erase(itr);
                state->UnbindAllPlayers();
                state->UnbindGroup(this);
            }
            else
                ++itr;
        }
    }
}

static void RewardGroupAtKill_helper(Player* pGroupGuy, Unit* pVictim,
    uint32 count, bool PvP, float group_rate, uint32 sum_level, bool is_dungeon,
    Player* not_gray_member_with_max_level, Player* member_with_max_level,
    uint32 xp)
{
    // honor from player tagets handled in Player::hk_distribute_honor()
    if (pGroupGuy->isAlive() && !PvP)
        pGroupGuy->RewardHonor(pVictim, count);

    // xp and reputation only in !PvP case
    if (!PvP)
    {
        float rate = group_rate * float(pGroupGuy->getLevel()) / sum_level;

        // if is in dungeon then all receive full reputation at kill
        // rewarded any alive/dead/near_corpse group member
        pGroupGuy->RewardReputation(pVictim, is_dungeon ? 1.0f : rate);

        // XP updated only for alive group member
        if (pGroupGuy->isAlive() && not_gray_member_with_max_level &&
            pGroupGuy->getLevel() <= not_gray_member_with_max_level->getLevel())
        {
            uint32 itr_xp =
                (member_with_max_level == not_gray_member_with_max_level) ?
                    uint32(xp * rate) :
                    uint32((xp * rate / 2) + 1);

            pGroupGuy->GiveXP(itr_xp, pVictim, group_rate);
            if (Pet* pet = pGroupGuy->GetPet())
                pet->GivePetXP(itr_xp / 2);
        }
    }

    // If it's inside a battleground (PvP && is_dungeon) quest can be completed
    if (PvP && !is_dungeon)
        return;

    // quest objectives updated only for alive group member or dead but with not
    // released body
    if (pGroupGuy->isAlive() ||
        !pGroupGuy->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_GHOST))
    {
        if (pVictim->GetTypeId() == TYPEID_UNIT)
            if (CreatureInfo const* normalInfo =
                    ObjectMgr::GetCreatureTemplate(pVictim->GetEntry()))
                pGroupGuy->KilledMonster(normalInfo, pVictim->GetObjectGuid());
    }
}

/** Provide rewards to group members at unit kill
 *
 * @param pVictim       Killed unit
 * @param player_tap    Player who tap unit if online, it can be group member or
 *can be not if leaved after tap but before kill target
 *
 * Rewards received by group members and player_tap
 */
void Group::RewardGroupAtKill(Unit* pVictim, Player* player_tap)
{
    if (unlikely(pVictim->GetTypeId() == TYPEID_UNIT &&
                 static_cast<Creature*>(pVictim)->GetCreatureType() ==
                     CREATURE_TYPE_CRITTER))
        return;

    bool PvP = pVictim->isCharmedOwnedByPlayerOrPlayer();

    // prepare data for near group iteration (PvP and !PvP cases)
    uint32 xp = 0;

    uint32 count = 0;
    uint32 sum_level = 0;
    Player* member_with_max_level = nullptr;
    Player* not_gray_member_with_max_level = nullptr;

    GetDataForXPAtKill(pVictim, count, sum_level, member_with_max_level,
        not_gray_member_with_max_level, player_tap);

    if (member_with_max_level)
    {
        /// not get Xp in PvP or no not gray players in group
        xp = (PvP || !not_gray_member_with_max_level) ?
                 0 :
                 MaNGOS::XP::Gain(not_gray_member_with_max_level, pVictim);

        /// skip in check PvP case (for speed, not used)
        bool is_raid =
            PvP ? true : sMapStore.LookupEntry(pVictim->GetMapId())->IsRaid() &&
                             isRaidGroup();
        bool is_dungeon =
            PvP ? true :
                  sMapStore.LookupEntry(pVictim->GetMapId())->IsDungeon();
        float group_rate = MaNGOS::XP::xp_in_group_rate(count, is_raid);

        for (auto pGroupGuy : members(true))
        {
            // will proccessed later
            if (pGroupGuy == player_tap)
                continue;

            if (!pGroupGuy->IsAtGroupRewardDistance(pVictim))
                continue; // member (alive or dead) or his corpse at req.
                          // distance

            RewardGroupAtKill_helper(pGroupGuy, pVictim, count, PvP, group_rate,
                sum_level, is_dungeon, not_gray_member_with_max_level,
                member_with_max_level, xp);
        }

        if (player_tap)
        {
            // member (alive or dead) or his corpse at req. distance
            if (player_tap->IsAtGroupRewardDistance(pVictim))
                RewardGroupAtKill_helper(player_tap, pVictim, count, PvP,
                    group_rate, sum_level, is_dungeon,
                    not_gray_member_with_max_level, member_with_max_level, xp);
        }
    }
}

ObjectGuid Group::GetNextGroupLooter(const std::set<ObjectGuid>& looters)
{
    std::lock_guard<std::mutex> guard(group_mutex_);

    // Find an object guid of an online looter that has the lowest current
    // looting index
    auto foundObj = m_lootIndexes.end();
    for (const auto& looter : looters)
    {
        Player* plr = sObjectAccessor::Instance()->FindPlayer(looter);
        if (!plr)
            continue;

        auto find = m_lootIndexes.find(looter);
        if (find != m_lootIndexes.end())
        {
            if (foundObj == m_lootIndexes.end())
                foundObj = find;
            else if (find->second < foundObj->second)
                foundObj = find;
        }
        else
            m_lootIndexes[plr->GetObjectGuid()] = m_currentLootIndex;
    }

    if (foundObj != m_lootIndexes.end())
    {
        foundObj->second = ++m_currentLootIndex;
        return foundObj->first;
    }

    return ObjectGuid();
}

InstanceGroupBind* Group::GetInstanceBind(uint32 mapid, Difficulty difficulty)
{
    auto itr = m_instanceBinds[difficulty].find(mapid);

    if (itr != m_instanceBinds[difficulty].end())
        return &itr->second;

    return nullptr;
}

void Group::BindToInstance(
    std::shared_ptr<DungeonPersistentState> state, bool perm)
{
    auto prev = GetInstanceBind(state->GetMapId(), state->GetDifficulty());
    if (prev)
    {
        auto prev_state = prev->state.lock();
        if (prev_state && prev_state != state)
            UnbindFromInstance(prev_state.get());
        else if (prev->perm == perm)
            return; // everything's the same, no need to update
    }

    state->BindGroup(this);

    InstanceGroupBind bind;
    bind.perm = perm;
    bind.state = state;
    m_instanceBinds[state->GetDifficulty()][state->GetMapId()] = bind;
    CharacterDatabase.PExecute(
        "INSERT INTO group_instance (gid, instance, perm) VALUES(%u, %u, %u) "
        "ON DUPLICATE KEY UPDATE perm=%u",
        GetId(), state->GetInstanceId(), perm, perm);
}

void Group::UnbindFromInstance(DungeonPersistentState* state)
{
    auto prev = GetInstanceBind(state->GetMapId(), state->GetDifficulty());
    auto prev_state = prev->state.lock();
    if (!prev || prev_state.get() != state)
        return;

    CharacterDatabase.PExecute(
        "DELETE FROM group_instance WHERE gid=%u AND instance=%u", GetId(),
        state->GetInstanceId());
    m_instanceBinds[state->GetDifficulty()].erase(state->GetMapId());

    state->UnbindGroup(this); // NOTE: do last, can destroy state
}

void Group::ClearInstanceBindOnDestruction(DungeonPersistentState* state)
{
    m_instanceBinds[state->GetDifficulty()].erase(state->GetMapId());
}

void Group::PassLeaderOnward()
{
    auto curr_leader = GetLeaderGuid();

    // Try assistants first
    for (auto& member : m_memberSlots)
    {
        if (member.guid == curr_leader)
            continue;

        if (member.assistant)
        {
            if (sObjectMgr::Instance()->GetPlayer(member.guid, false) !=
                nullptr)
            {
                ChangeLeader(member.guid);
                return;
            }
        }
    }

    // Settle for anyone
    for (auto& member : m_memberSlots)
    {
        if (member.guid == curr_leader)
            continue;

        if (sObjectMgr::Instance()->GetPlayer(member.guid, false) != nullptr)
        {
            ChangeLeader(member.guid);
            return;
        }
    }
}

void Group::add_spy(Player* player)
{
    m_spies.push_back(player->GetObjectGuid());
    SendUpdate();

    if (player->GetSession())
    {
        SendTargetIconList(player->GetSession());
    }
}

void Group::remove_spy(Player* player)
{
    auto itr =
        std::find(m_spies.begin(), m_spies.end(), player->GetObjectGuid());
    if (itr != m_spies.end())
    {
        m_spies.erase(itr);

        if (player->GetSession())
        {
            WorldPacket data(SMSG_GROUP_UNINVITE, 0);
            player->GetSession()->send_packet(std::move(data));

            data.initialize(SMSG_GROUP_LIST, 24);
            data << uint64(0) << uint64(0) << uint64(0);
            player->GetSession()->send_packet(std::move(data));
        }
    }
}

// ===================================================
//                   Group Iterator
// ===================================================
GroupIteratorWrapper::_InnerItr::_InnerItr(const _InnerItr& other)
{
    ref_ = other.ref_;
    spies_ = other.spies_;
    spies_index_ = other.spies_index_;
    in_world_ = other.in_world_;
}

GroupIteratorWrapper::_InnerItr::_InnerItr(
    GroupReference* ref, const std::vector<ObjectGuid>* spies, bool in_world)
  : ref_(ref), spies_(spies), spies_index_(0), in_world_(in_world)
{
    if ((spies_ && spies_->empty()) || ref_ == nullptr)
        spies_ = nullptr;

    if (in_world_)
    {
        Player* p = operator*();
        if (!p || !p->IsInWorld())
            operator++();
    }
}

GroupIteratorWrapper::_InnerItr& GroupIteratorWrapper::_InnerItr::operator++()
{
    Player* p = advance_pointer();
    if (p && in_world_)
    {
        // Keep iterating until we find a player in world or nullptr
        if (!p->IsInWorld())
            return operator++();
    }
    return *this;
}

GroupIteratorWrapper::_InnerItr GroupIteratorWrapper::_InnerItr::operator++(int)
{
    auto tmp = *this;
    operator++();
    return tmp;
}

bool GroupIteratorWrapper::_InnerItr::operator==(const _InnerItr& rhs) const
{
    if (spies_ != rhs.spies_)
        return false;
    if (!spies_ && !rhs.spies_)
        return ref_ == rhs.ref_;
    return ref_ == rhs.ref_ && spies_index_ == rhs.spies_index_;
}

bool GroupIteratorWrapper::_InnerItr::operator!=(const _InnerItr& rhs) const
{
    return !operator==(rhs);
}

Player* GroupIteratorWrapper::_InnerItr::operator*()
{
    Player* player = nullptr;
    if (ref_)
    {
        player = ref_->getSource();
    }
    else if (spies_)
    {
        assert(spies_index_ > 0);
        player = ObjectAccessor::FindPlayer((*spies_)[spies_index_ - 1], false);
    }

    return player;
}

Player* GroupIteratorWrapper::_InnerItr::advance_pointer()
{
    Player* player = nullptr;

    if (ref_)
    {
        ref_ = ref_->next();
        if (ref_)
            player = ref_->getSource();
    }

    if (!player && spies_)
    {
        while (!player && spies_index_ < spies_->size())
            player =
                ObjectAccessor::FindPlayer((*spies_)[spies_index_++], false);
        if (!player)
            spies_ = nullptr;
    }

    return player;
}

GroupIteratorWrapper::_InnerItr GroupIteratorWrapper::begin()
{
    return _InnerItr(group_->m_memberMgr.getFirst(),
        spies_ ? &group_->m_spies : nullptr, in_world_);
}

GroupIteratorWrapper::_InnerItr GroupIteratorWrapper::end()
{
    return _InnerItr(nullptr, nullptr, true);
}
