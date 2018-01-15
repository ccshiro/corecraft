#ifndef _NPC_GROUP_H
#define _NPC_GROUP_H

#include "Common.h"
#include "ObjectGuid.h"
#include "Platform/Define.h"
#include <string>
#include <vector>

class Creature;
// Sql structs
struct creature_group_entry
{
    creature_group_entry()
    {
        id = map = special_flags = leader_guid = movement_leader_guid = size =
            0;
    }

    int32 id;
    uint32 map;
    uint32 special_flags;
    uint32 leader_guid;
    uint32 movement_leader_guid;
    std::string group_name;

    uint32 size; // derived from number of entries in creature_group_members
};

class MANGOS_DLL_SPEC CreatureGroup
{
    // In-game data
    std::vector<Creature*> m_members;
    Creature* m_leader;

    bool m_temporary;
    creature_group_entry* pGroupData;

public:
    CreatureGroup(creature_group_entry* groupData, bool temporary);
    ~CreatureGroup();

    int32 GetId() { return pGroupData->id; }

    void AddMember(Creature* pCreature, bool permanentChange);
    void RemoveMember(Creature* pCreature, bool permanentChange);
    bool SetLeader(Creature* pCreature, bool permanentChange);
    void ClearLeader(bool permanentChange);

    // Leaders adds the possibility of extra functionality
    Creature* GetLeader() const { return m_leader; }

    bool IsTemporaryGroup() { return m_temporary; }
    creature_group_entry* GetGroupEntry() { return pGroupData; }

    void RenameGroup(const std::string& name);

    bool HasFlag(uint32 flag) { return pGroupData->special_flags & flag; }
    void AddFlag(uint32 flag);
    void RemoveFlag(uint32 flag);

    const std::vector<Creature*>& GetMembers() const { return m_members; }

    std::vector<Creature*>::iterator begin() { return m_members.begin(); }
    std::vector<Creature*>::iterator end() { return m_members.end(); }

    uint32 GetSize() { return m_members.end() - m_members.begin(); }
    uint32 GetMaxSize() { return pGroupData->size; }
};

#endif
