/*
 * Copyright (C) 2008-2014 TrinityCore <http://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef TRANSPORTMGR_H
#define TRANSPORTMGR_H

#include "DBCStores.h"
#include "ObjectGuid.h"
#include "Policies/Singleton.h"
#include "movement/spline.h"
#include <set>
#include <unordered_map>
#include <vector>

struct GameObjectInfo;
struct KeyFrame;
class Map;
class Transport;
struct TransportTemplate;

typedef movement::Spline<double> TransportSpline;
typedef std::vector<KeyFrame> KeyFrameVec;
typedef std::unordered_map<uint32, TransportTemplate> TransportTemplates;
typedef std::set<Transport*> TransportSet;
typedef std::unordered_map<uint32, TransportSet> TransportMap;
typedef std::unordered_map<uint32, std::set<uint32>> TransportInstanceMap;

struct KeyFrame
{
    explicit KeyFrame(TaxiPathNodeEntry const& _node)
      : Index(0), Node(&_node), InitialOrientation(0.0f), DistSinceStop(-1.0f),
        DistUntilStop(-1.0f), DistFromPrev(-1.0f), TimeFrom(0.0f), TimeTo(0.0f),
        Teleport(false), ArriveTime(0), DepartureTime(0), Spline(nullptr),
        NextDistFromPrev(0.0f), NextArriveTime(0)
    {
    }

    uint32 Index;
    TaxiPathNodeEntry const* Node;
    float InitialOrientation;
    float DistSinceStop;
    float DistUntilStop;
    float DistFromPrev;
    float TimeFrom;
    float TimeTo;
    bool Teleport;
    uint32 ArriveTime;
    uint32 DepartureTime;
    TransportSpline* Spline;

    // Data needed for next frame
    float NextDistFromPrev;
    uint32 NextArriveTime;

    bool IsTeleportFrame() const { return Teleport; }
    bool IsStopFrame() const { return Node->actionFlag == 2; }
};

struct TransportTemplate
{
    TransportTemplate()
      : inInstance(false), pathTime(0), accelTime(0.0f), accelDist(0.0f),
        entry(0)
    {
    }
    ~TransportTemplate();

    std::set<uint32> mapsUsed;
    bool inInstance;
    uint32 pathTime;
    KeyFrameVec keyFrames;
    float accelTime;
    float accelDist;
    uint32 entry;
};

typedef std::map<uint32, TransportAnimationEntry const*> TransportPathContainer;

struct TransportAnimation
{
    TransportAnimation() : TotalTime(0) {}

    TransportPathContainer Path;
    uint32 TotalTime;

    TransportAnimationEntry const* GetAnimNode(uint32 time) const;
};

typedef std::map<uint32, TransportAnimation> TransportAnimationContainer;

class TransportMgr
{
    friend void LoadDBCStores(std::string const&);

public:
    void Unload();

    void LoadTransportTemplates();

    // Creates a transport using given GameObject template entry
    Transport* CreateTransport(
        uint32 entry, uint32 guid = 0, Map* map = nullptr);

    // Spawns all continent transports, used at core startup
    void SpawnContinentTransports();

    TransportTemplate const* GetTransportTemplate(uint32 entry) const
    {
        auto itr = _transportTemplates.find(entry);
        if (itr != _transportTemplates.end())
            return &itr->second;
        return nullptr;
    }

    TransportAnimation const* GetTransportAnimInfo(uint32 entry) const
    {
        auto itr = _transportAnimations.find(entry);
        if (itr != _transportAnimations.end())
            return &itr->second;

        return nullptr;
    }

    Transport* GetContinentTransport(ObjectGuid guid) const;

    void UpdateTransports(uint32 diff);

private:
    friend class MaNGOS::UnlockedSingleton<TransportMgr>;
    TransportMgr();
    ~TransportMgr();
    TransportMgr(const TransportMgr&) = delete;
    TransportMgr& operator=(const TransportMgr&) = delete;

    // Generates and precaches a path for transport to avoid generation each
    // time transport instance is created
    void GeneratePath(
        GameObjectInfo const* goInfo, TransportTemplate* transport);

    void AddPathNodeToTransport(uint32 transportEntry, uint32 timeSeg,
        TransportAnimationEntry const* node);

    // Container storing transport templates
    TransportTemplates _transportTemplates;

    // Container storing transport entries to create for instanced maps
    TransportInstanceMap _instanceTransports;

    TransportAnimationContainer _transportAnimations;

    std::vector<Transport*> _continentTransports;
};

#define sTransportMgr MaNGOS::UnlockedSingleton<TransportMgr>

#endif // TRANSPORTMGR_H
