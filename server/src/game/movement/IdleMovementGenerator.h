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

#ifndef GAME__MOVEMENT__IDLE_MOVEMENT_GENERATOR_H
#define GAME__MOVEMENT__IDLE_MOVEMENT_GENERATOR_H

#include "generators.h"
#include "ObjectGuid.h"

class AuraHolder;
class Unit;

namespace movement
{
class MANGOS_DLL_SPEC IdleMovementGenerator : public Generator
{
public:
    IdleMovementGenerator() : x_(0), y_(0), z_(0), o_(0), waiting_path_id_(0) {}
    IdleMovementGenerator(float x, float y, float z, float o)
      : x_(x), y_(y), z_(z), o_(o), waiting_path_id_(0)
    {
    }

    void pushed() override;
    void start() override;
    void stop() override;
    std::string debug_str() const override;

    gen id() const override { return gen::idle; }

    void finished_path(std::vector<G3D::Vector3> path, uint32 id) override;

    float x_;
    float y_;
    float z_;
    float o_;

private:
    uint32 waiting_path_id_;
};

class MANGOS_DLL_SPEC FaceMovementGenerator : public Generator
{
public:
    FaceMovementGenerator(float o) : o_(o) {}

    void start() override;
    bool update(uint32, uint32) override;

    gen id() const override { return gen::facing; }

private:
    float o_;
};

class MANGOS_DLL_SPEC DistractMovementGenerator : public Generator
{
public:
    DistractMovementGenerator(uint32 timer) : m_timer{timer} {}

    gen id() const override { return gen::distract; }

    void start() override;
    void removed(bool solo) override;
    bool update(uint32, uint32 top_diff) override;

private:
    uint32 m_timer;
};

class MANGOS_DLL_SPEC StunMovementGenerator : public Generator
{
public:
    StunMovementGenerator(AuraHolder* aura = nullptr) : aura_(aura) {}

    gen id() const override { return gen::stun; }

    // Not nullptr if owned by AuraHolder
    AuraHolder* holder() const { return aura_; }

    void start() override;
    void removed(bool solo) override;

private:
    AuraHolder* aura_;
};

class MANGOS_DLL_SPEC RootMovementGenerator : public Generator
{
public:
    RootMovementGenerator(AuraHolder* aura = nullptr) : aura_(aura) {}

    gen id() const override { return gen::root; }

    // Not nullptr if owned by AuraHolder
    AuraHolder* holder() const { return aura_; }

    void start() override;
    void removed(bool solo) override;

private:
    AuraHolder* aura_;
};

class MANGOS_DLL_SPEC ControlledMovementGenerator : public Generator
{
public:
    ControlledMovementGenerator(Unit* caster, AuraHolder* aura = nullptr);

    gen id() const override { return gen::controlled; }

    // Not nullptr if owned by AuraHolder
    AuraHolder* holder() const { return aura_; }

    void pushed() override;
    void removed(bool solo) override;

    bool prevents_control() const override { return true; }

private:
    ObjectGuid caster_;
    AuraHolder* aura_;
    bool caster_steering_;
};

class MANGOS_DLL_SPEC StoppedMovementGenerator : public Generator
{
public:
    // timeout: automatically remove in X milliseconds if > 0
    // id: movement inform once finished(); if id < 0 only movement informs if
    //     timer reached 0 (i.e. not if it's popped preemptively)
    StoppedMovementGenerator(uint32 timeout = 0, int id = 0)
      : timer_(timeout), id_(id)
    {
    }

    gen id() const override { return gen::stopped; }

    void start() override;
    void finished() override;
    bool update(uint32 diff, uint32) override;

    std::string debug_str() const override;

private:
    uint32 timer_;
    int id_;
};
}

#endif
