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

#ifndef MANGOS_WAYPOINTMANAGER_H
#define MANGOS_WAYPOINTMANAGER_H

#include "Common.h"
#include "Policies/Singleton.h"
#include <G3D/Vector3.h>
#include <sparsehash/dense_hash_map>
#include <string>
#include <unordered_map>
#include <vector>

#define MAX_WAYPOINT_TEXT 5

namespace movement
{
struct WaypointBehavior
{
    uint32 emote;
    uint32 spell;
    int32 textid[MAX_WAYPOINT_TEXT];
    uint32 model1;
    uint32 model2;

    bool isEmpty();
    WaypointBehavior() {}
    WaypointBehavior(const WaypointBehavior& b);
};

struct WaypointNode
{
    float x;
    float y;
    float z;
    float orientation;
    uint32 delay;
    bool run;
    uint32 script_id; // Added may 2010. WaypointBehavior w/DB data should in
                      // time be removed.
    WaypointBehavior* behavior;
    WaypointNode()
      : x(0.0f), y(0.0f), z(0.0f), orientation(0.0f), delay(0), script_id(0),
        behavior(nullptr)
    {
    }
    WaypointNode(float _x, float _y, float _z, float _o, uint32 _delay,
        uint32 _script_id, WaypointBehavior* _behavior)
      : x(_x), y(_y), z(_z), orientation(_o), delay(_delay),
        script_id(_script_id), behavior(_behavior)
    {
    }
};

typedef std::vector<WaypointNode> WaypointPath;

class WaypointManager
{
public:
    WaypointManager() { splines_.set_empty_key(0); }
    ~WaypointManager() { Unload(); }

    void Load();
    void Unload();

    void Cleanup();

    WaypointPath* GetPath(uint32 id)
    {
        auto itr = m_pathMap.find(id);
        return itr != m_pathMap.end() ? &itr->second : nullptr;
    }

    WaypointPath* GetPathTemplate(uint32 entry)
    {
        auto itr = m_pathTemplateMap.find(entry);
        return itr != m_pathTemplateMap.end() ? &itr->second : nullptr;
    }

    const std::vector<G3D::Vector3>* GetSpline(uint32 id) const
    {
        auto itr = splines_.find(id);
        if (itr != splines_.end())
            return &itr->second;
        return nullptr;
    }

    void AddLastNode(uint32 id, float x, float y, float z, float o,
        uint32 delay, uint32 wpGuid);
    void AddAfterNode(uint32 id, uint32 point, float x, float y, float z,
        float o, uint32 delay, uint32 wpGuid);
    uint32 GetLastPoint(uint32 id, uint32 default_notfound);
    void DeleteNode(uint32 id, uint32 point);
    void DeletePath(uint32 id);
    void SetNodePosition(uint32 id, uint32 point, float x, float y, float z);
    void SetNodeText(
        uint32 id, uint32 point, const char* text_field, const char* text);
    void CheckTextsExistance(std::set<int32>& ids);

private:
    void _addNode(uint32 id, uint32 point, float x, float y, float z, float o,
        uint32 delay, uint32 wpGuid);
    void _clearPath(WaypointPath& path);

    typedef std::unordered_map<uint32, WaypointPath> WaypointPathMap;
    WaypointPathMap m_pathMap;
    typedef std::unordered_map<uint32, WaypointPath> WaypointPathTemplateMap;
    WaypointPathTemplateMap m_pathTemplateMap;

    // Flying splines
    void LoadSplines();
    google::dense_hash_map<uint32 /*id*/, std::vector<G3D::Vector3>> splines_;
};
}

#define sWaypointMgr MaNGOS::UnlockedSingleton<movement::WaypointManager>

#endif
