
#include "CreatureGroup.h"
#include "Creature.h"
#include "CreatureGroupMgr.h"
#include "Map.h"
#include "World.h"
#include "Database/DatabaseEnv.h"
#include <algorithm>

CreatureGroup::CreatureGroup(creature_group_entry* groupData, bool temporary)
  : m_leader{nullptr}, m_temporary(temporary)
{
    if (m_temporary)
    {
        // Copy the data if temporary
        pGroupData = new creature_group_entry(*groupData);
    }
    else
    {
        // Save pointer if permanent
        pGroupData = groupData;
    }
}

CreatureGroup::~CreatureGroup()
{
    if (m_temporary)
        delete pGroupData;
}

void CreatureGroup::AddMember(Creature* pCreature, bool permanentChange)
{
    assert(pCreature->GetGroup() == nullptr);

    pCreature->SetGroup(this);

    if (std::find(m_members.begin(), m_members.end(), pCreature) ==
        m_members.end())
    {
        m_members.push_back(pCreature);
        if (pGroupData->leader_guid == pCreature->GetGUIDLow())
            m_leader = pCreature;

        if (permanentChange && !m_temporary)
        {
            // Note that a permanent insertion requires a restart.
            WorldDatabase.PExecute(
                "INSERT INTO creature_group_members (group_id, creature_guid) "
                "VALUES(%i, %i)",
                GetId(), pCreature->GetGUIDLow());
        }
    }
}

void CreatureGroup::RemoveMember(Creature* pCreature, bool permanentChange)
{
    assert(pCreature->GetGroup() == this);

    pCreature->SetGroup(nullptr);

    m_members.erase(std::remove(m_members.begin(), m_members.end(), pCreature),
        m_members.end());
    if (pGroupData->leader_guid == pCreature->GetGUIDLow())
    {
        m_leader = nullptr;
        if (permanentChange && !m_temporary)
        {
            WorldDatabase.PExecute(
                "UPDATE creature_groups SET leader_guid=0 WHERE id=%i",
                GetId());
            pGroupData->leader_guid = 0;
        }
    }

    if (permanentChange && !m_temporary)
    {
        // Note that a permanent deletion requires a restart.
        WorldDatabase.PExecute(
            "DELETE FROM creature_group_members WHERE group_id=%i AND "
            "creature_guid=%i",
            GetId(), pCreature->GetGUIDLow());
    }
}

void CreatureGroup::RenameGroup(const std::string& name)
{
    if (!m_temporary)
    {
        std::string temp(name);
        WorldDatabase.escape_string(temp);
        WorldDatabase.PExecute(
            "UPDATE creature_groups SET name=\"%s\" WHERE id=%i", temp.c_str(),
            GetId());
    }
    pGroupData->group_name = name;
}

bool CreatureGroup::SetLeader(Creature* pCreature, bool permanentChange)
{
    if (std::find(m_members.begin(), m_members.end(), pCreature) ==
        m_members.end())
        return false;

    m_leader = pCreature;

    if (permanentChange && !m_temporary)
    {
        WorldDatabase.PExecute(
            "UPDATE creature_groups SET leader_guid=%i WHERE id=%i",
            pCreature->GetGUIDLow(), GetId());
        pGroupData->leader_guid = pCreature->GetGUIDLow();
    }

    return true;
}

void CreatureGroup::ClearLeader(bool permanentChange)
{
    if (permanentChange && !m_temporary)
    {
        WorldDatabase.PExecute(
            "UPDATE creature_groups SET leader_guid=0 WHERE id=%i", GetId());
        pGroupData->leader_guid = 0;
    }
    m_leader = nullptr;
}

void CreatureGroup::AddFlag(uint32 flag)
{
    if (!m_temporary)
        WorldDatabase.PExecute(
            "UPDATE creature_groups SET special_flags = special_flags | %i "
            "WHERE id=%i",
            flag, GetId());
    pGroupData->special_flags |= flag;
}

void CreatureGroup::RemoveFlag(uint32 flag)
{
    if (!m_temporary)
        WorldDatabase.PExecute(
            "UPDATE creature_groups SET special_flags = special_flags & ~%i "
            "WHERE id=%i",
            flag, GetId());
    pGroupData->special_flags &= ~flag;
}
