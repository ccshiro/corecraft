/*
 * Copyright (C) 2008-2012 TrinityCore <http://www.trinitycore.org/>
 * Copyright (C) 2005-2009 MaNGOS <http://getmangos.com/>
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

#include "GameObjectModel.h"
#include "Creature.h"
#include "DBCStores.h"
#include "GameObject.h"
#include "logging.h"
#include "TemporarySummon.h"
#include "VMapDefinitions.h"
#include "VMapFactory.h"
#include "World.h"
#include "WorldModel.h"
#include "G3D/CoordinateFrame.h"
#include <unordered_map>

using G3D::Vector3;
using G3D::Ray;
using G3D::AABox;

struct GameobjectModelData
{
    GameobjectModelData(std::string name_, AABox box)
      : bound(std::move(box)), name(std::move(name_))
    {
    }

    AABox bound;
    std::string name;
};

typedef std::unordered_map<uint32, GameobjectModelData> ModelList;
ModelList model_list;

void LoadGameObjectModelList()
{
    FILE* model_list_file =
        fopen((sWorld::Instance()->GetDataPath() + "vmaps/" +
                  VMAP::GAMEOBJECT_MODELS).c_str(),
            "rb");
    if (!model_list_file)
        return;

    uint32 name_length, displayId;
    char buff[500];
    while (!feof(model_list_file))
    {
        Vector3 v1, v2;
        if (fread(&displayId, sizeof(uint32), 1, model_list_file) != 1)
            if (feof(model_list_file)) // EOF flag is only set after failed
                                       // reading attempt
                break;

        if (fread(&name_length, sizeof(uint32), 1, model_list_file) != 1 ||
            name_length >= sizeof(buff) ||
            fread(&buff, sizeof(char), name_length, model_list_file) !=
                name_length ||
            fread(&v1, sizeof(Vector3), 1, model_list_file) != 1 ||
            fread(&v2, sizeof(Vector3), 1, model_list_file) != 1)
        {
            logging.error(
                "File '%s' seems to be corrupted!", VMAP::GAMEOBJECT_MODELS);
            break;
        }

        model_list.insert(ModelList::value_type(
            displayId, GameobjectModelData(
                           std::string(buff, name_length), AABox(v1, v2))));
    }
    fclose(model_list_file);
}

GameObjectModel::~GameObjectModel()
{
    if (iModel)
        ((VMAP::VMapManager2*)VMAP::VMapFactory::createOrGetVMapManager())
            ->releaseModelInstance(name);
}

bool GameObjectModel::initialize(
    const GameObject& go, const GameObjectDisplayInfoEntry& info)
{
    ModelList::const_iterator it = model_list.find(info.Displayid);
    if (it == model_list.end())
        return false;

    G3D::AABox mdl_box(it->second.bound);
    // ignore models with no bounds
    if (mdl_box == G3D::AABox::zero())
    {
        std::cout << "Model " << it->second.name
                  << " has zero bounds, loading skipped" << std::endl;
        return false;
    }

    iModel =
        ((VMAP::VMapManager2*)VMAP::VMapFactory::createOrGetVMapManager())
            ->acquireModelInstance(
                sWorld::Instance()->GetDataPath() + "vmaps/", it->second.name);

    if (!iModel)
        return false;

    iTransport = go.IsTransport();

    name = it->second.name;
    // flags = VMAP::MOD_M2;
    // adtId = 0;
    // ID = 0;
    if (go.IsTransport())
        iPos = Vector3(0, 0, 0);
    else
        iPos = Vector3(go.GetX(), go.GetY(), go.GetZ());
    iScale = go.GetFloatValue(OBJECT_FIELD_SCALE_X);
    iInvScale = 1.f / iScale;

    G3D::Matrix3 rotation;
    if (go.IsTransport())
        rotation = G3D::Matrix3::identity();
    else
        rotation = G3D::Matrix3::fromEulerAnglesZYX(go.GetO(), 0, 0);
    iInvRot = rotation.inverse();
    // transform bounding box:
    mdl_box = AABox(mdl_box.low() * iScale, mdl_box.high() * iScale);

    AABox rotated_bounds;
    // FIXME: Use the proper box below, needs a lot of refactoring...
    for (int i = 0; i < 8; ++i)
        rotated_bounds.merge(rotation * mdl_box.corner(i));

    G3D::CoordinateFrame frame(rotation);
    frame = frame + iPos;
    iBoundBox = frame.toWorldSpace(mdl_box);

    iBound = rotated_bounds + iPos;

#ifdef SPAWN_CORNERS
    // test:
    for (int i = 0; i < 8; ++i)
    {
        Vector3 pos(iBoundBox.corner(i));
        if (Creature* c = const_cast<GameObject&>(go).SummonCreature(
                3681, pos.x, pos.y, pos.z, 0, TEMPSUMMON_MANUAL_DESPAWN, 0))
        {
            c->setFaction(35);
            c->SetFloatValue(OBJECT_FIELD_SCALE_X, 0.1f);
            c->SetDefaultMovementType(IDLE_MOTION_TYPE);
            c->GetMotionMaster()->Initialize();
            c->SetDeathState(JUST_DIED);
            c->Respawn();
        }
    }
#endif

    return true;
}

GameObjectModel* GameObjectModel::Create(const GameObject& go)
{
    const GameObjectDisplayInfoEntry* info =
        sGameObjectDisplayInfoStore.LookupEntry(go.GetGOInfo()->displayId);
    if (!info)
        return nullptr;

    auto mdl = new GameObjectModel();
    if (!mdl->initialize(go, *info))
    {
        delete mdl;
        return nullptr;
    }

    return mdl;
}

bool GameObjectModel::intersectRay(
    const G3D::Ray& ray, float& MaxDist, bool StopAtFirstHit) const
{
    if (!iTransport)
    {
        if (!enabled)
            return false;

        float time = ray.intersectionTime(iBoundBox);
        if (time == G3D::inf())
            return false;
        if (time > MaxDist)
            return false;
        MaxDist = time;
        return true;
    }
    else
    {
        // child bounds are defined in object space:
        /*Vector3 p = iInvRot * (ray.origin() - iPos) * iInvScale;
        Ray modRay(p, iInvRot * ray.direction());*/
        float distance = MaxDist /** iInvScale*/;
        bool hit = iModel->IntersectRay(ray, distance, StopAtFirstHit);
        if (hit)
        {
            // distance *= iScale;
            MaxDist = distance;
        }
        return hit;
    }
}
