#ifndef _GROUP_PULL_MGR
#define _GROUP_PULL_MGR

#include "Common.h"
#include "CreatureGroup.h"
#include "CreatureGroupMovement.h"
#include "Platform/Define.h"
#include <map>
#include <unordered_map>
#include <vector>

class Creature;
class Map;
class Unit;

enum CREATURE_GROUP_EVENT
{
    CREATURE_GROUP_EVENT_AGGRO = 0,
    CREATURE_GROUP_EVENT_EVADE,
    CREATURE_GROUP_EVENT_DEATH,
    CREATURE_GROUP_EVENT_MOVEMENT_BEGIN,
    CREATURE_GROUP_EVENT_MOVEMENT_UPDATE,
    CREATURE_GROUP_EVENT_MOVEMENT_PAUSE,
    CREATURE_GROUP_EVENT_MOVEMENT_RESUME,
    CREATURE_GROUP_EVENT_RESPAWN
};

enum CREATURE_GROUP_SPECIAL_FLAGS
{
    CREATURE_GROUP_FLAG_NONE = 0,
    CREATURE_GROUP_FLAG_RESPAWN_ALL_ON_SURVIVOR = 0x01,
    CREATURE_GROUP_FLAG_CANNOT_ASSIST = 0x02,
    CREATURE_GROUP_FLAG_LEADER_RESPAWN_ALL = 0x04,
    CREATURE_GROUP_FLAG_LEADER_DESPAWN_ALL = 0x08,
    CREATURE_GROUP_FLAG_GROUP_MOVEMENT = 0x10,
    CREATURE_GROUP_FLAG_CANNOT_BE_ASSISTED = 0x20,
    CREATURE_GROUP_FLAG_CANNOT_ASSIST_OTHER_GRPS =
        0x40, // Can not assist other creature groups, but non-grouped mobs work
              // fine
    // Group moves together but do not aggro/evade together
    CREATURE_GROUP_FLAG_MOVEMENT_NO_COMBAT = 0x80,
};

enum DelayNotifyType
{
    DELAY_NOTIFY_UPDATE = 0,
    DELAY_NOTIFY_RESUME
};

struct DelayNotify
{
    DelayNotifyType type;
    int32 grpId;
};

// Group ids in the GroupMgr are only assured to be unique per map,
// if sampled across different maps, collisions are to be expected
class MANGOS_DLL_SPEC CreatureGroupMgr
{
public:
    // Static data, loaded from SQL on server startup
    typedef std::multimap<uint32 /* map id */, creature_group_entry>
        sql_group_map;
    typedef std::unordered_map<uint32 /* low guid */, int32 /* group id */>
        sql_lowguid_map;
    static sql_group_map sqlGroups;
    static sql_lowguid_map sqlGroupMembers;

    typedef std::unordered_map<int32 /* group id */, CreatureGroup*> group_map;

private:
    Map* m_owningMap;
    group_map m_groupMap;
    int32 m_nextTemporaryId;
    CreatureGroupMovement m_creatureGroupMovement;

    // FIXME: This should probably be switched into a hashmap with grpId as the
    // key
    typedef std::vector<std::pair<uint32 /* time remaining */, DelayNotify>>
        delay_notify;
    delay_notify m_movementDelayNotifier;
    bool m_mgrUpdate;
    delay_notify m_queuedDelay;

public:
    CreatureGroupMgr(Map* map);
    ~CreatureGroupMgr();

    void UpdateGroupMgr(const uint32& diff);
    void ScheduleMovementUpdate(int32 grpId, uint32 delay);

    int32 CreateNewGroup(const std::string& name, bool temporary);
    CreatureGroup* GetGroup(int32 id);
    // Requires restart of server to take global effect if the group is not
    // temporary
    bool DeleteGroup(int32 id);

    void ProcessGroupEvent(int32 groupId, CREATURE_GROUP_EVENT groupEvent,
        Unit* pTarget = nullptr, uint32 timeout = 0);
    // Adds creature to group if it's part of a group. Returns group_id
    void OnAddToWorld(Creature* pCreature);
    void OnRemoveFromWorld(Creature* pCreature);

    // Sets up formation for creature (call manually for temporary groups when
    // you create them)
    void AddToFormation(Creature* pCreature);
    void RemoveFromFormation(Creature* pCreature);

    const group_map::const_iterator GetRawBegin() { return m_groupMap.begin(); }
    const group_map::const_iterator GetRawEnd() { return m_groupMap.end(); }

    CreatureGroupMovement& GetMovementMgr() { return m_creatureGroupMovement; }

    static void LoadAllGroupsFromDb();

    bool GetCreatureVectorForMovement(
        CreatureGroup* pGroup, std::vector<Creature*>& creatures);

    void ClearCurrentPauses(int32 grpId);
    void PauseMovementOfGroup(int32 grpId, uint32 delayInMilliseconds);
    bool GetRespawnPositionOfCreature(int32 grpId, Creature* pCreature,
        float& x, float& y, float& z, float& o);
};

#endif
