/*
 * Copyright (C) 2005-2012 MaNGOS <http://getmangos.com/>
 * Copyright (C) 2015 corecraft <https://www.worldofcorecraft.com>
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

#ifndef GAME__MOVEMENT__POINT_MOVEMENT_GENERATOR_H
#define GAME__MOVEMENT__POINT_MOVEMENT_GENERATOR_H

#include "Creature.h"
#include "generators.h"
#include "G3D/Vector3.h"

#define GWP_POINT_ID 1299709

namespace movement
{
class MANGOS_DLL_SPEC PointMovementGenerator : public Generator
{
public:
    PointMovementGenerator(
        uint32 _id, float _x, float _y, float _z, bool _generatePath, bool run)
      : id_(_id), i_x(_x), i_y(_y), i_z(_z), i_waitingPathId(0),
        m_generatePath(_generatePath), i_run(run)
    {
    }

    gen id() const override { return gen::point; }

    void start() override;
    void stop() override;
    void finished() override;
    bool update(uint32 diff, uint32) override;
    std::string debug_str() const override;

    bool GetDestination(float& x, float& y, float& z) const
    {
        x = i_x;
        y = i_y;
        z = i_z;
        return true;
    }
    uint32 GetId() const { return id_; }
    bool UsingMmaps() const { return m_generatePath; }

    void finished_path(std::vector<G3D::Vector3> path, uint32 id) override;

private:
    uint32 id_;
    float i_x, i_y, i_z;
    uint32 i_waitingPathId;
    bool m_generatePath;
    bool i_run;
};

class GwpMovementGenerator : public PointMovementGenerator
{
public:
    using PointMovementGenerator::PointMovementGenerator;

    gen id() const override { return gen::gwp; }
};

class FallMovementGenerator : public Generator
{
public:
    FallMovementGenerator() {}

    gen id() const override { return gen::fall; }

    void start() override;
    void stop() override;
    void finished() override;
    bool update(uint32 diff, uint32) override;
};

class MANGOS_DLL_SPEC ChargeMovementGenerator : public Generator
{
public:
    ChargeMovementGenerator(
        std::vector<G3D::Vector3> path, ObjectGuid chargedTarget, float speed)
      : chargedTarget_(std::move(chargedTarget)), speed_(speed),
        path_(std::move(path))
    {
    }

    gen id() const override { return gen::charge; }

    void start() override;
    void stop() override;
    void finished() override;
    bool update(uint32 diff, uint32) override;

private:
    ObjectGuid chargedTarget_;
    float speed_;
    std::vector<G3D::Vector3> path_;
};
}

#endif
