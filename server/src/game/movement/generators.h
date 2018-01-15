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

#ifndef GAME__MOVEMENT__GENERATORS_H
#define GAME__MOVEMENT__GENERATORS_H

#include "Common.h"
#include "G3D/Vector3.h"
#include "Platform/Define.h"
#include <vector>

class Unit;

namespace movement
{
// Enumeration of generators
enum class gen
{
    idle = 0,            // IdleMovementGenerator.h
    random = 1,          // RandomMovementGenerator.h
    waypoint = 2,        // WaypointMovementGenerator.h
    random_waterair = 3, // RandomMovementGenerator.h
    confused = 4,        // ConfusedMovementGenerator.h
    chase = 5,           // TargetedMovementGenerator.h
    home = 6,            // HomeMovementGenerator.h
    flight = 7,          // WaypointMovementGenerator.h
    point = 8,           // PointMovementGenerator.h
    fleeing = 9,         // FleeingMovementGenerator.h
    distract = 10,       // IdleMovementGenerator.h
    run_in_fear = 11,    // FleeingMovementGenerator.h
    follow = 12,         // TargetedMovementGenerator.h
    fall = 13,           // PointMovementGenerator.h
    charge = 14,         // PointMovementGenerator.h
    gwp = 15,            // CreatureGroupMovement.h
    stopped = 16,        // IdleMovementGenerator.h
    stun = 17,           // IdleMovementGenerator.h
    root = 18,           // IdleMovementGenerator.h
    controlled = 19,     // IdleMovementGenerator.h
    facing = 20,         // IdleMovementGenerator.h
    spline = 21,         // WaypointMovementGenerator.h

    max
};

int get_default_priority(gen g);
const char* generator_name(gen g);

enum movement_event
{
    EVENT_NONE = 0,
    EVENT_ENTER_COMBAT = 0x0001,
    EVENT_LEAVE_COMBAT = 0x0002,
    EVENT_DEATH = 0x0004,
};

class Generator
{
    friend class GeneratorQueue;
    void _set_owner(Unit* own) { owner_ = own; }
    void _set_running(bool state) { running_ = state; }
    void _set_prio(int prio) { priority_ = prio; }

public:
    Generator()
      : owner_(nullptr), priority_(0), auto_remove_(0), top_diff_(0),
        running_(false)
    {
    }

    virtual ~Generator() {}

    void set_auto_remove(uint32 auto_remove) { auto_remove_ = auto_remove; }

    Unit* owner() const { return owner_; }

    // Enum class gen: identifier
    virtual gen id() const = 0;

    int priority() const { return priority_; }

    // Called when a threaded path has been generated
    virtual void finished_path(std::vector<G3D::Vector3>, uint32) {}

    bool is_removed_by_event(uint32 e) const { return auto_remove_ & e; }

    bool running() const { return running_; }

    // Calbacks for derivatives

    // Called when movement generator is pushed. At this point generator has
    // owner_ set etc. This callback can be used to save state of the owner_
    // as soon as the generator is pushed (used or not).
    virtual void pushed() {}

    // Called when movement generator is popped. Different from finished() in
    // that it's always invoked, whether the generator was completed or not.
    // solo: false if other generators with same id() still remain
    virtual void removed(bool /*solo*/) {}

    // Called when movement generator should start moving unit; can be invoked
    // more than once (i.e., generator goes "cold", then is later on brought
    // back for continued exectuion). Do NOT add/remove from movement queue.
    virtual void start() {}

    // Called when movement generator should stop moving unit; called both when
    // generator goes cold and when the movement is finished. Do NOT add/remove
    // from movement queue.
    virtual void stop() {}

    // Called when generator finishes its movement task, i.e. when update()
    // returns true. This is not called if the generator is manually removed.
    // This is where you should add new generators if needed.
    virtual void finished() {}

    // Return true when generator is done and should be discarded. Do NOT
    // add/remove from movement queue.
    // diff: the time since last world update
    // top_diff: the time since this generator in particular was top(); equal
    //           to diff if if was top() last update
    virtual bool update(uint32 /*diff*/, uint32 /*top_diff*/) { return false; }

    // Returns true if generator prevents control (ie, Player::SetClienControl)
    virtual bool prevents_control() const { return false; }

    virtual std::string debug_str() const { return ""; }

protected:
    Unit* owner_;

private:
    // Don't edit priority while generator is in queue without calling _sort()
    int priority_;
    uint32 auto_remove_;
    uint32 top_diff_; // for update's top_diff
    bool running_;
};

// Priority queue to decide which movement generator is active
class GeneratorQueue
{
public:
    GeneratorQueue(Unit* owner) : owner_(owner), in_control_(true) {}
    ~GeneratorQueue();

    Generator* top() const;
    bool empty() const { return prio_queue_.empty(); }
    size_t size() const { return prio_queue_.size(); }
    gen top_id() const { return empty() ? gen::max : top()->id(); }
    bool has(gen g, int prio = 0) const;
    Generator* get(gen g) const;

    void mod_priority(Generator* gen, int new_prio);
    // Changing queue during a Generator::update is invalid; instead we queue
    // movement informs to the AI for the queue to dispatch
    void queue_movement_inform(movement::gen type, uint32 id);

    // Queue becomes owner of pushed generator; delete on remove()
    // prio == 0 means use default priority
    void push(Generator* generator, uint32 auto_remove = 0, int prio = 0);
    void pop_top();
    // removed: if false, it was remove because of update() returning false
    void remove(Generator* generator, bool removed = true);
    template <typename F>
    void remove_if(F f);
    void remove_all(movement::gen type);

    void reset();
    void on_event(movement_event e);
    void update(uint32 diff);

    std::vector<Generator*>::iterator begin() { return prio_queue_.begin(); }
    std::vector<Generator*>::iterator end() { return prio_queue_.end(); }

private:
    Unit* owner_;
    std::vector<Generator*> prio_queue_;
    std::vector<std::pair<movement::gen, uint32>> informs_;
    bool in_control_;

    // Call _remove() after changing prio_queue_ (deletes generator)
    // removed: the generator was removed before it was allowed to finish
    void _remove(Generator* gen, bool was_top, bool removed);
    void _sort();
    void _start_stop();
    void _update_control_state();
};

template <typename F>
void GeneratorQueue::remove_if(F f)
{
    auto copy = prio_queue_;
    for (auto& gen : copy)
    {
        if (f(gen))
            remove(gen);
    }
}
}

#endif
