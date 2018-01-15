#ifndef CREATURE_GROUP_MOVEMENT_H_
#define CREATURE_GROUP_MOVEMENT_H_

#include "Common.h"
#include "ObjectGuid.h"
#include "Platform/Define.h"
#include <unordered_map>
#include <vector>

class Creature;
struct DynamicWaypoint;
class Map;
class Player;

struct FormationOffset
{
    FormationOffset()
    {
        lowGuid = 0;
        dist = angle = 0.;
    }

    uint32 lowGuid;
    float dist;
    float angle;
};

struct GroupWaypoint
{
    GroupWaypoint()
    {
        point = 0;
        x = y = z = o = 0.;
        delay = 0;
        run = false;
        mmap = false;
    }

    uint32 point;
    float x, y, z, o;
    uint32 delay;
    bool run;
    bool mmap;
};

struct GroupMovementData
{
    GroupMovementData()
    {
        leaderGuid = nextPoint = currentPoint = 0;
        paused = false;
        delayed = false;
    }

    uint32 leaderGuid;
    uint32 nextPoint;
    uint32 currentPoint;
    bool paused;
    bool delayed;
};

struct GroupWaypointSqlData
{
    int32 group_id;
    uint32 point;
    float position_x;
    float position_y;
    float position_z;
    float orientation;
    uint32 delay;
    bool run;
    bool mmap;
};

struct DisplayedWaypoint
{
    int32 grpId;
    uint32 point;
    ObjectGuid wpGuid;
};

class MANGOS_DLL_SPEC CreatureGroupMovement
{
    CreatureGroupMovement(const CreatureGroupMovement&);
    CreatureGroupMovement& operator=(const CreatureGroupMovement&);

public:
    static void LoadWaypointsFromDb();

    explicit CreatureGroupMovement(Map* map) : m_owningMap(map) {}
    ~CreatureGroupMovement();

    void LoadCreatureGroupMovement(int32 grpId);

    void AddCreature(int32 grpId, Creature* creature);
    void SetNewFormation(int32 grpId, Creature* creature);
    void ResetAllFormations(int32 grpId);
    void RemoveCreature(int32 grpId, Creature* creature);

    void StartMovement(int32 grpId, const std::vector<Creature*>& creatures);
    // RestartMovement: used after returning home post combat
    void RestartMovement(int32 grpId, const std::vector<Creature*>& creatures);
    void UpdateMovement(int32 grpId, const std::vector<Creature*>& creatures,
        bool notfiyEvent = false);

    // These methods requires a restart to take global effect:
    void AddWaypoint(int32 grpId, const DynamicWaypoint& wp, bool mmap = false);
    void DeleteWaypoint(int32 grpId, uint32 point);
    void MoveWaypoint(
        int32 grpId, uint32 point, float x, float y, float z, float o);

    void GetWaypoints(int32 grpId, std::vector<GroupWaypoint>& waypoints);

    bool IsLeader(int32 grpId, Creature* creature);

    void PauseMovement(int32 grpId, const std::vector<Creature*>& creatures,
        bool force = false);
    void TryResumeMovement(
        int32 grpId, const std::vector<Creature*>& creatures);
    // Allows resuming movement; does not resume it
    void ClearPausedFlag(int32 grpId);

    uint32 GetNumberOfWaypoints(int32 grpId);

    // Removes group from movement system (waypoints and movement data)
    void RemoveGroupData(int32 grpId);

    // GM commands (for editing): -- Require restart to take global effect
    bool DisplayAllWps(int32 grpId, Player* summoner);
    bool HideAllWps(int32 grpId);
    bool RemoveWp(Creature* waypoint);
    bool MoveWp(Creature* waypoint);
    bool SetRun(Creature* waypoint, bool run);
    bool SetDelay(Creature* waypoint, uint32 delay);

    bool GetMemberOffset(int32 grpId, uint32 lowGuid, FormationOffset& offset);

private:
    // Static data
    typedef std::unordered_map<int32 /* group id */,
        std::vector<GroupWaypointSqlData>> sql_group_waypoint_map;
    static sql_group_waypoint_map sqlGroupWaypointMap;

    Map* m_owningMap;

    typedef std::unordered_map<int32 /* group id */,
        std::vector<FormationOffset>> formation_map;
    typedef std::unordered_map<int32 /* group id */, std::vector<GroupWaypoint>>
        waypoint_map;
    typedef std::unordered_map<int32 /* group id */, GroupMovementData*>
        movement_map;
    formation_map m_formationMap;
    // Waypoint Map is always sorted so the point numbers come in order
    waypoint_map m_waypointMap;
    movement_map m_movementDataMap;

    void InitializeFormationOffset(
        int32 grpId, FormationOffset& offset, Creature* creature);

    GroupWaypoint GetAdjustedWaypoint(
        int32 grpId, uint32 point, FormationOffset& offset, Creature* ref);

    void InternalDoMovement(int32 grpId, std::vector<Creature*> creatures);

    // Creation-related data (for GMs)
    typedef std::unordered_map<uint32 /* low guid */, DisplayedWaypoint>
        displayed_wps;
    displayed_wps m_displayedWps;

    // Start & Restart Movement helper functions
    Creature* get_leader(int32 grpId, const std::vector<Creature*>& creatures);
    uint32 closest_point(
        Creature* leader, const std::vector<GroupWaypoint>& wps);
    GroupMovementData* make_or_get_move_data(int32 grpId);
};

#endif
