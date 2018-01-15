/*
 * Copyright (C) 2015 corecraft <https://www.worldofcorecraft.com/>
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

#include "generators.h"
#include "Creature.h"
#include "CreatureAI.h"
#include "logging.h"
#include "pet_template.h"
#include "Unit.h"
#include "movement/IdleMovementGenerator.h"
#include "movement/RandomMovementGenerator.h"
#include "movement/TargetedMovementGenerator.h"
#include "movement/WaypointMovementGenerator.h"
#include <algorithm>

static std::map<movement::gen, int> default_prio{
    {movement::gen::idle, 0}, {movement::gen::waypoint, 10},
    {movement::gen::gwp, 10}, {movement::gen::random, 20},
    {movement::gen::random_waterair, 20}, {movement::gen::follow, 30},
    {movement::gen::home, 40}, {movement::gen::chase, 50},
    {movement::gen::stopped, 60}, {movement::gen::point, 70},
    {movement::gen::run_in_fear, 80}, {movement::gen::distract, 90},
    {movement::gen::controlled, 100}, {movement::gen::fleeing, 110},
    {movement::gen::confused, 120}, {movement::gen::root, 130},
    {movement::gen::stun, 140}, {movement::gen::fall, 150},
    {movement::gen::charge, 160}, {movement::gen::flight, 170},
    {movement::gen::facing, 180}, {movement::gen::spline, 190},
};

static auto& logger = logging.get_logger("movegens");

movement::GeneratorQueue::~GeneratorQueue()
{
    for (auto itr = begin(); itr != end(); ++itr)
        delete *itr;
}

movement::Generator* movement::GeneratorQueue::top() const
{
    if (!prio_queue_.empty())
        return prio_queue_.front();
    return nullptr;
}

bool movement::GeneratorQueue::has(gen g, int prio) const
{
    for (auto& gen : prio_queue_)
        if (gen->id() == g && (!prio || prio == gen->priority()))
            return true;
    return false;
}

movement::Generator* movement::GeneratorQueue::get(gen g) const
{
    for (auto& gen : prio_queue_)
        if (gen->id() == g)
            return gen;
    return nullptr;
}

void movement::GeneratorQueue::mod_priority(Generator* gen, int new_prio)
{
    if (std::find(begin(), end(), gen) == end())
        return;

    gen->_set_prio(new_prio);
    _sort();
    _start_stop();
}

void movement::GeneratorQueue::queue_movement_inform(
    movement::gen type, uint32 id)
{
    informs_.emplace_back(type, id);
}

void movement::GeneratorQueue::push(
    Generator* generator, uint32 auto_remove, int prio)
{
    if (std::find(begin(), end(), generator) != end())
        throw std::runtime_error("Tried inserting generator twice");

    generator->_set_owner(owner_);
    if (prio == 0)
        generator->_set_prio(default_prio[generator->id()]);
    else
        generator->_set_prio(prio);
    generator->set_auto_remove(auto_remove);

    prio_queue_.push_back(generator);
    generator->pushed();
    _sort();

    LOG_DEBUG(logger,
        "%s push generator <%s> with prio %d and auto remove: %u (new top: %s)",
        owner_->GetObjectGuid().GetString().c_str(),
        generator_name(generator->id()), generator->priority(), auto_remove,
        top() == generator ? "yes" : "no");
}

void movement::GeneratorQueue::pop_top()
{
    if (!empty())
        remove(top());
}

void movement::GeneratorQueue::remove(Generator* generator, bool removed)
{
    auto itr = std::find(begin(), end(), generator);
    if (itr != end())
    {
        bool was_top = itr == prio_queue_.begin();
        prio_queue_.erase(itr);
        _remove(generator, was_top, removed);
    }
}

void movement::GeneratorQueue::remove_all(movement::gen type)
{
    remove_if([type](auto* gen)
        {
            return gen->id() == type;
        });
}

void movement::GeneratorQueue::reset()
{
    // Clear current generators
    pop_top();
    for (auto& gen : *this)
        delete gen;
    prio_queue_.clear();

    // Stop movement
    owner_->StopMoving();

    // Owner has no movement generator if dead
    if (owner_->isDead())
        return;

    // Pets with pet behavior and follow do not have a default generator
    if (owner_->GetTypeId() == TYPEID_UNIT &&
        static_cast<Creature*>(owner_)->IsPet())
    {
        if (static_cast<Creature*>(owner_)->behavior() != nullptr)
        {
            if ((static_cast<Creature*>(owner_)->get_template()->pet_flags &
                    PET_FLAGS_DISABLE_FOLLOW) == 0)
                return;
        }
    }

    // Set new default generator
    if (owner_->GetTypeId() == TYPEID_UNIT &&
        !owner_->hasUnitState(UNIT_STAT_CONTROLLED))
    {
        auto g = static_cast<Creature*>(owner_)->get_default_movement_gen();
        if (g == gen::waypoint)
            push(new WaypointMovementGenerator());
        else if (g == gen::random)
            push(new RandomMovementGenerator());
        else if (g == gen::random_waterair)
            push(new RandomWaterAirMovementGenerator());
        else
            push(new IdleMovementGenerator());
    }
    else if (owner_->GetTypeId() == TYPEID_PLAYER)
    {
        push(new IdleMovementGenerator());
    }
}

void movement::GeneratorQueue::on_event(movement_event e)
{
    LOG_DEBUG(logger, "%s movement event %u happened",
        owner_->GetObjectGuid().GetString().c_str(), e);

    // Pop all movement generators on death, on respawn we'll call reset()
    if (e == EVENT_DEATH)
    {
        for (auto& gen : prio_queue_)
        {
            gen->stop();
            delete gen;
        }
        prio_queue_.clear();
        owner_->StopMoving();
        return;
    }

    remove_if([&e](const Generator* g)
        {
            return g->is_removed_by_event(e);
        });
}

void movement::GeneratorQueue::update(uint32 diff)
{
    if (likely(!empty()))
    {
#ifndef NDEBUG
        auto sz = prio_queue_.size();
#endif
        for (auto& gen : *this)
            gen->top_diff_ += diff;

        auto top_diff = top()->top_diff_;
        top()->top_diff_ = 0;

        if (top()->update(diff, top_diff))
        {
            assert(sz == prio_queue_.size());
            remove(top(), false);
        }
    }

    if (unlikely(!informs_.empty() && owner_->GetTypeId() == TYPEID_UNIT))
    {
        auto copy = std::move(informs_);
        informs_.clear();
        auto cowner = static_cast<Creature*>(owner_);
        for (auto& p : copy)
            if (auto ai = cowner->AI())
                ai->MovementInform(p.first, p.second);
    }
}

void movement::GeneratorQueue::_remove(
    Generator* gen, bool was_top, bool removed)
{
    LOG_DEBUG(logger, "%s remove generator <%s> with prio %d (was top: %s)",
        owner_->GetObjectGuid().GetString().c_str(), generator_name(gen->id()),
        gen->priority(), was_top ? "yes" : "no");

    if (was_top)
    {
        gen->stop();
        if (!removed)
            gen->finished();
        gen->_set_running(false);
    }

    gen->removed(!has(gen->id()));

    if (was_top && !empty())
    {
        auto new_gen = prio_queue_.front();
        new_gen->start();
        new_gen->_set_running(true);
    }

    _update_control_state();

    delete gen;
}

void movement::GeneratorQueue::_sort()
{
    auto compare = [](const Generator* const& a, const Generator* const& b)
    {
        return a->priority() > b->priority();
    };
    std::stable_sort(begin(), end(), compare);
    _start_stop();
}

void movement::GeneratorQueue::_start_stop()
{
    for (auto itr = begin(); itr != end(); ++itr)
    {
        if (*itr != top())
        {
            if ((*itr)->running())
            {
                (*itr)->stop();
                (*itr)->_set_running(false);
            }
        }
    }

    if (!empty())
    {
        if (!top()->running())
        {
            top()->start();
            top()->_set_running(true);
        }
    }

    _update_control_state();
}

void movement::GeneratorQueue::_update_control_state()
{
    // Update owner's control state
    if (owner_->GetTypeId() == TYPEID_PLAYER)
    {
        bool control = true;
        for (auto* gen : *this)
            if (gen->prevents_control())
            {
                control = false;
                break;
            }

        if (in_control_ != control)
        {
            static_cast<Player*>(owner_)->SetClientControl(
                owner_, control ? 1 : 0);
            in_control_ = control;
        }
    }

    // Update charmer's control state
    auto charmer = owner_->GetCharmer();
    if (charmer && charmer->GetTypeId() == TYPEID_PLAYER)
    {
        // Only applicable if charmer is also moving owner
        if (static_cast<Player*>(charmer)->GetMovingUnit() == owner_)
        {
            bool control = true;
            for (auto* gen : *this)
                if (gen->id() != movement::gen::controlled &&
                    gen->prevents_control())
                {
                    control = false;
                    break;
                }
            static_cast<Player*>(charmer)->SetClientControl(
                owner_, control ? 1 : 0);
        }
    }
}

int movement::get_default_priority(movement::gen g)
{
    return default_prio[g];
}

const char* movement::generator_name(gen g)
{
    switch (g)
    {
    case gen::idle:
        return "idle";
    case gen::random:
        return "random";
    case gen::waypoint:
        return "waypoint";
    case gen::random_waterair:
        return "random water/air";
    case gen::confused:
        return "confused";
    case gen::chase:
        return "chase";
    case gen::home:
        return "home";
    case gen::flight:
        return "flight";
    case gen::point:
        return "point";
    case gen::fleeing:
        return "fleeing";
    case gen::distract:
        return "distract";
    case gen::run_in_fear:
        return "run in fear";
    case gen::follow:
        return "follow";
    case gen::fall:
        return "fall";
    case gen::charge:
        return "charge";
    case gen::gwp:
        return "group waypoint";
    case gen::stopped:
        return "stopped";
    case gen::stun:
        return "stun";
    case gen::root:
        return "root";
    case gen::controlled:
        return "controlled";
    case gen::facing:
        return "facing";
    case gen::spline:
        return "spline";
    case gen::max:
        break;
    }

    return "invalid";
}
