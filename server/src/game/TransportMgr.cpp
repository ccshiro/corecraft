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

#include "TransportMgr.h"
#include "MapManager.h"
#include "ObjectMgr.h"
#include "ProgressBar.h"
#include "Transport.h"
#include "Database/DatabaseEnv.h"
#include "movement/MoveSpline.h"

TransportTemplate::~TransportTemplate()
{
    // Collect shared pointers into a set to avoid deleting the same memory more
    // than once
    std::set<TransportSpline*> splines;
    for (auto& elem : keyFrames)
        splines.insert(elem.Spline);

    for (const auto& spline : splines)
        delete spline;
}

TransportMgr::TransportMgr()
{
}

TransportMgr::~TransportMgr()
{
}

void TransportMgr::Unload()
{
    _transportTemplates.clear();
}

void TransportMgr::LoadTransportTemplates()
{
    std::unique_ptr<QueryResult> result(WorldDatabase.Query(
        "SELECT entry FROM gameobject_template WHERE type = 15 ORDER BY entry "
        "ASC"));

    if (!result)
    {
        logging.info(
            "Loaded 0 transport templates. DB table `gameobject_template` "
            "has no transports!\n");
        return;
    }

    uint32 count = 0;
    BarGoLink bar(result->GetRowCount());

    do
    {
        bar.step();
        Field* fields = result->Fetch();
        uint32 entry = fields[0].GetUInt32();
        auto goInfo = sObjectMgr::Instance()->GetGameObjectInfo(entry);
        if (!goInfo)
        {
            logging.error(
                "Transport %u has no associated GameObjectTemplate from "
                "`gameobject_template`, skipped.",
                entry);
            continue;
        }

        if (goInfo->moTransport.taxiPathId >= sTaxiPathNodesByPath.size())
        {
            logging.error(
                "Transport %u (name: %s) has an invalid path specified in "
                "`gameobject_template`.`data0` (%u) field, skipped.",
                entry, goInfo->name, goInfo->moTransport.taxiPathId);
            continue;
        }

        TransportTemplate& transport = _transportTemplates[entry];
        transport.entry = entry;
        GeneratePath(goInfo, &transport);

        // In TBC there are no instanced transports
        assert(!transport.inInstance);

        ++count;
    } while (result->NextRow());

    logging.info("Loaded %u transport templates", count);
}

class SplineRawInitializer
{
public:
    SplineRawInitializer(movement::PointsArray& points) : _points(points) {}

    void operator()(uint8& mode, bool& cyclic, movement::PointsArray& points,
        int& lo, int& hi) const
    {
        mode = movement::SplineBase::ModeCatmullrom;
        cyclic = false;
        points.assign(_points.begin(), _points.end());
        lo = 1;
        hi = points.size() - 2;
    }

    movement::PointsArray& _points;
};

void TransportMgr::GeneratePath(
    GameObjectInfo const* goInfo, TransportTemplate* transport)
{
    uint32 pathId = goInfo->moTransport.taxiPathId;
    TaxiPathNodeList const& path = sTaxiPathNodesByPath[pathId];
    std::vector<KeyFrame>& keyFrames = transport->keyFrames;
    movement::PointsArray splinePath, allPoints;
    bool mapChange = false;
    for (size_t i = 0; i < path.size(); ++i)
        allPoints.push_back(G3D::Vector3(path[i].x, path[i].y, path[i].z));

    // Add extra points to allow derivative calculations for all path nodes
    allPoints.insert(
        allPoints.begin(), allPoints.front().lerp(allPoints[1], -0.2f));
    allPoints.push_back(
        allPoints.back().lerp(allPoints[allPoints.size() - 2], -0.2f));
    allPoints.push_back(
        allPoints.back().lerp(allPoints[allPoints.size() - 2], -1.0f));

    SplineRawInitializer initer(allPoints);
    TransportSpline orientationSpline;
    orientationSpline.init_spline_custom(initer);
    orientationSpline.initLengths();

    for (size_t i = 0; i < path.size(); ++i)
    {
        if (!mapChange)
        {
            TaxiPathNodeEntry const& node_i = path[i];
            if (i != path.size() - 1 &&
                (node_i.actionFlag & 1 || node_i.mapid != path[i + 1].mapid))
            {
                keyFrames.back().Teleport = true;
                mapChange = true;
            }
            else
            {
                KeyFrame k(node_i);
                G3D::Vector3 h;
                orientationSpline.evaluate_derivative(i + 1, 0.0f, h);
                k.InitialOrientation = Position::NormalizeOrientation(
                    std::atan2(h.y, h.x) + float(M_PI));

                keyFrames.push_back(k);
                splinePath.push_back(
                    G3D::Vector3(node_i.x, node_i.y, node_i.z));
                transport->mapsUsed.insert(k.Node->mapid);
            }
        }
        else
            mapChange = false;
    }

    if (splinePath.size() >= 2)
    {
        // Remove special catmull-rom spline points
        if (!keyFrames.front().IsStopFrame() &&
            !keyFrames.front().Node->arrivalEventID &&
            !keyFrames.front().Node->departureEventID)
        {
            splinePath.erase(splinePath.begin());
            keyFrames.erase(keyFrames.begin());
        }
        if (!keyFrames.back().IsStopFrame() &&
            !keyFrames.back().Node->arrivalEventID &&
            !keyFrames.back().Node->departureEventID)
        {
            splinePath.pop_back();
            keyFrames.pop_back();
        }
    }

    assert(!keyFrames.empty());

    if (transport->mapsUsed.size() > 1)
    {
#ifndef NDEBUG
        for (const auto& elem : transport->mapsUsed)
            assert(!sMapStore.LookupEntry(elem)->Instanceable());
#endif

        transport->inInstance = false;
    }
    else
        transport->inInstance =
            sMapStore.LookupEntry(*transport->mapsUsed.begin())->Instanceable();

    // last to first is always "teleport", even for closed paths
    keyFrames.back().Teleport = true;

    const float speed = float(goInfo->moTransport.moveSpeed);
    const float accel = float(goInfo->moTransport.accelRate);
    const float accel_dist = 0.5f * speed * speed / accel;

    transport->accelTime = speed / accel;
    transport->accelDist = accel_dist;

    int32 firstStop = -1;
    int32 lastStop = -1;

    // first cell is arrived at by teleportation :S
    keyFrames[0].DistFromPrev = 0;
    keyFrames[0].Index = 1;
    if (keyFrames[0].IsStopFrame())
    {
        firstStop = 0;
        lastStop = 0;
    }

    // find the rest of the distances between key points
    // Every path segment has its own spline
    size_t start = 0;
    for (size_t i = 1; i < keyFrames.size(); ++i)
    {
        if (keyFrames[i - 1].Teleport || i + 1 == keyFrames.size())
        {
            size_t extra = !keyFrames[i - 1].Teleport ? 1 : 0;
            auto spline = new TransportSpline();
            spline->init_spline(&splinePath[start], i - start + extra,
                movement::SplineBase::ModeCatmullrom);
            spline->initLengths();
            for (size_t j = start; j < i + extra; ++j)
            {
                keyFrames[j].Index = j - start + 1;
                keyFrames[j].DistFromPrev =
                    float(spline->length(j - start, j + 1 - start));
                if (j > 0)
                    keyFrames[j - 1].NextDistFromPrev =
                        keyFrames[j].DistFromPrev;
                keyFrames[j].Spline = spline;
            }

            if (keyFrames[i - 1].Teleport)
            {
                keyFrames[i].Index = i - start + 1;
                keyFrames[i].DistFromPrev = 0.0f;
                keyFrames[i - 1].NextDistFromPrev = 0.0f;
                keyFrames[i].Spline = spline;
            }

            start = i;
        }

        if (keyFrames[i].IsStopFrame())
        {
            // remember first stop frame
            if (firstStop == -1)
                firstStop = i;
            lastStop = i;
        }
    }

    keyFrames.back().NextDistFromPrev = keyFrames.front().DistFromPrev;

    if (firstStop == -1 || lastStop == -1)
        firstStop = lastStop = 0;

    // at stopping keyframes, we define distSinceStop == 0,
    // and distUntilStop is to the next stopping keyframe.
    // this is required to properly handle cases of two stopping frames in a row
    // (yes they do exist)
    float tmpDist = 0.0f;
    for (size_t i = 0; i < keyFrames.size(); ++i)
    {
        int32 j = (i + lastStop) % keyFrames.size();
        if (keyFrames[j].IsStopFrame() || j == lastStop)
            tmpDist = 0.0f;
        else
            tmpDist += keyFrames[j].DistFromPrev;
        keyFrames[j].DistSinceStop = tmpDist;
    }

    tmpDist = 0.0f;
    for (int32 i = int32(keyFrames.size()) - 1; i >= 0; i--)
    {
        int32 j = (i + firstStop) % keyFrames.size();
        tmpDist += keyFrames[(j + 1) % keyFrames.size()].DistFromPrev;
        keyFrames[j].DistUntilStop = tmpDist;
        if (keyFrames[j].IsStopFrame() || j == firstStop)
            tmpDist = 0.0f;
    }

    for (auto& keyFrame : keyFrames)
    {
        float total_dist = keyFrame.DistSinceStop + keyFrame.DistUntilStop;
        if (total_dist < 2 * accel_dist) // won't reach full speed
        {
            if (keyFrame.DistSinceStop <
                keyFrame.DistUntilStop) // is still accelerating
            {
                // calculate accel+brake time for this short segment
                float segment_time =
                    2.0f * std::sqrt((keyFrame.DistUntilStop +
                                         keyFrame.DistSinceStop) /
                                     accel);
                // substract acceleration time
                keyFrame.TimeTo = segment_time -
                                  std::sqrt(2 * keyFrame.DistSinceStop / accel);
            }
            else // slowing down
                keyFrame.TimeTo = std::sqrt(2 * keyFrame.DistUntilStop / accel);
        }
        else if (keyFrame.DistSinceStop <
                 accel_dist) // still accelerating (but will reach full speed)
        {
            // calculate accel + cruise + brake time for this long segment
            float segment_time =
                (keyFrame.DistUntilStop + keyFrame.DistSinceStop) / speed +
                (speed / accel);
            // substract acceleration time
            keyFrame.TimeTo =
                segment_time - std::sqrt(2 * keyFrame.DistSinceStop / accel);
        }
        else if (keyFrame.DistUntilStop <
                 accel_dist) // already slowing down (but reached full speed)
            keyFrame.TimeTo = std::sqrt(2 * keyFrame.DistUntilStop / accel);
        else // at full speed
            keyFrame.TimeTo =
                (keyFrame.DistUntilStop / speed) + (0.5f * speed / accel);
    }

    // calculate tFrom times from tTo times
    float segmentTime = 0.0f;
    for (size_t i = 0; i < keyFrames.size(); ++i)
    {
        int32 j = (i + lastStop) % keyFrames.size();
        if (keyFrames[j].IsStopFrame() || j == lastStop)
            segmentTime = keyFrames[j].TimeTo;
        keyFrames[j].TimeFrom = segmentTime - keyFrames[j].TimeTo;
    }

    // calculate path times
    keyFrames[0].ArriveTime = 0;
    float curPathTime = 0.0f;
    if (keyFrames[0].IsStopFrame())
    {
        curPathTime = float(keyFrames[0].Node->delay);
        keyFrames[0].DepartureTime = uint32(curPathTime * IN_MILLISECONDS);
    }

    for (size_t i = 1; i < keyFrames.size(); ++i)
    {
        curPathTime += keyFrames[i - 1].TimeTo;
        if (keyFrames[i].IsStopFrame())
        {
            keyFrames[i].ArriveTime = uint32(curPathTime * IN_MILLISECONDS);
            keyFrames[i - 1].NextArriveTime = keyFrames[i].ArriveTime;
            curPathTime += float(keyFrames[i].Node->delay);
            keyFrames[i].DepartureTime = uint32(curPathTime * IN_MILLISECONDS);
        }
        else
        {
            curPathTime -= keyFrames[i].TimeTo;
            keyFrames[i].ArriveTime = uint32(curPathTime * IN_MILLISECONDS);
            keyFrames[i - 1].NextArriveTime = keyFrames[i].ArriveTime;
            keyFrames[i].DepartureTime = keyFrames[i].ArriveTime;
        }
    }

    keyFrames.back().NextArriveTime = keyFrames.back().DepartureTime;

    transport->pathTime = keyFrames.back().DepartureTime;
}

void TransportMgr::AddPathNodeToTransport(
    uint32 transportEntry, uint32 timeSeg, TransportAnimationEntry const* node)
{
    TransportAnimation& animNode = _transportAnimations[transportEntry];
    if (animNode.TotalTime < timeSeg)
        animNode.TotalTime = timeSeg;

    animNode.Path[timeSeg] = node;
}

Transport* TransportMgr::CreateTransport(
    uint32 entry, uint32 /*guid*/ /*= 0*/, Map* map /*= NULL*/)
{
    // In TBC there are no instanced transports
    assert(!map);

    TransportTemplate const* tInfo = GetTransportTemplate(entry);
    if (!tInfo)
    {
        logging.error(
            "Transport %u will not be loaded, `transport_template` entry "
            "missing",
            entry);
        return nullptr;
    }

    // create transport...
    auto trans = new Transport();

    // ...at first waypoint
    TaxiPathNodeEntry const* startNode = tInfo->keyFrames.begin()->Node;
    uint32 mapId = startNode->mapid;
    float x = startNode->x;
    float y = startNode->y;
    float z = startNode->z;
    float o = tInfo->keyFrames.begin()->InitialOrientation;

    // initialize the gameobject base
    // uint32 guidLow = guid ? guid :
    // sObjectMgr::Instance()->GenerateLowGuid(HIGHGUID_MO_TRANSPORT);
    if (!trans->Create(entry, mapId, x, y, z, o, GO_ANIMPROGRESS_DEFAULT))
    {
        delete trans;
        return nullptr;
    }

    if (MapEntry const* mapEntry = sMapStore.LookupEntry(mapId))
    {
        if (mapEntry->Instanceable() != tInfo->inInstance)
        {
            logging.error(
                "Transport %u (name: %s) attempted creation in instance map "
                "(id: %u) but it is not an instanced transport!",
                entry, trans->GetName(), mapId);
            delete trans;
            return nullptr;
        }
    }

    trans->SetMap(map ? map : sMapMgr::Instance()->CreateMap(mapId, nullptr));

    // Passengers will be loaded once a player is near

    trans->GetMap()->insert(trans);
    return trans;
}

void TransportMgr::SpawnContinentTransports()
{
    if (_transportTemplates.empty())
        return;

    std::unique_ptr<QueryResult> result(
        WorldDatabase.Query("SELECT entry FROM transports"));

    uint32 count = 0;

    if (result)
    {
        BarGoLink bar(result->GetRowCount());
        do
        {
            bar.step();
            Field* fields = result->Fetch();
            uint32 entry = fields[0].GetUInt32();

            if (TransportTemplate const* tInfo = GetTransportTemplate(entry))
                if (!tInfo->inInstance)
                    if (Transport* trans = CreateTransport(entry))
                    {
                        _continentTransports.push_back(trans);
                        ++count;
                    }

        } while (result->NextRow());
    }

    logging.info("Spawned %u continent transports\n", count);
}

Transport* TransportMgr::GetContinentTransport(ObjectGuid guid) const
{
    for (const auto& elem : _continentTransports)
        if ((elem)->GetObjectGuid() == guid)
            return elem;
    return nullptr;
}

void TransportMgr::UpdateTransports(uint32 diff)
{
    for (auto& elem : _continentTransports)
    {
        if (!(elem)->IsInWorld())
            continue;

        elem->Update(elem->GetUpdateTracker().timeElapsed(), diff);
        elem->GetUpdateTracker().Reset();
    }
}

TransportAnimationEntry const* TransportAnimation::GetAnimNode(
    uint32 time) const
{
    if (Path.empty())
        return nullptr;

    for (auto itr2 = Path.rbegin(); itr2 != Path.rend(); ++itr2)
        if (time >= itr2->first)
            return itr2->second;

    return Path.begin()->second;
}
