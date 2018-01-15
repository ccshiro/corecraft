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

#include "TargetedMovementGenerator.h"
#include "ByteBuffer.h"
#include "Creature.h"
#include "PathFinder.h"
#include "Player.h"
#include "World.h"
#include "pet_behavior.h"
#include "movement/MoveSpline.h"
#include "movement/MoveSplineInit.h"

static auto& chase_log = logging.get_logger("movegens.chase");
static auto& follow_log = logging.get_logger("movegens.follow");

namespace movement
{
ChaseMovementGenerator::ChaseMovementGenerator(ObjectGuid enemy)
  : state_(State::relaxing), spread_timer_(0), back_timer_(0), speed_(0),
    target_(enemy), waiting_path_id_(0), casted_spell_(false), evading_(false),
    run_mode_(false)
{
    current_target_ = enemy;
}

void ChaseMovementGenerator::start()
{
    speed_ = owner_->GetSpeed(MOVE_RUN);

    owner_->addUnitState(UNIT_STAT_CHASE | UNIT_STAT_CHASE_MOVE);
    owner_->StopMoving();
    waiting_path_id_ = 0;
    set_state(State::relaxing);
}

void ChaseMovementGenerator::stop()
{
    owner_->clearUnitState(UNIT_STAT_CHASE | UNIT_STAT_CHASE_MOVE);
    if (evading_)
    {
        evading_ = false;
        if (owner_->GetTypeId() == TYPEID_UNIT)
            static_cast<Creature*>(owner_)->stop_evade();
    }
}

bool ChaseMovementGenerator::update(uint32 diff, uint32)
{
    auto target = get_target();
    if (!target)
        return false;

    if (owner_->hasUnitState(UNIT_STAT_NOT_MOVE))
    {
        owner_->clearUnitState(UNIT_STAT_CHASE_MOVE);
        return false;
    }

    auto casting = owner_->IsCastedSpellPreventingMovementOrAttack();
    if (casting && !casted_spell_)
    {
        LOG_DEBUG(chase_log, "%s begun casting a spell, and %s moving",
            owner_->GetObjectGuid().GetString().c_str(),
            owner_->IsStopped() ? "was not" : "stopped");

        casted_spell_ = true;
        if (!owner_->IsStopped())
            owner_->StopMoving();
        return false;
    }
    else if (!casting && casted_spell_)
    {
        LOG_DEBUG(chase_log, "%s ended its spell cast, and %s its movement",
            owner_->GetObjectGuid().GetString().c_str(),
            owner_->IsStopped() ? "restarted" : "never stopped");

        casted_spell_ = false;
        if (owner_->IsStopped())
            set_state(State::relaxing);
        return false;
    }
    else if (casting)
    {
        return false;
    }

    if (target->GetTypeId() == TYPEID_PLAYER &&
        static_cast<Player*>(target)->IsKnockbacked())
    {
        if (state_ != State::relaxing)
            set_state(State::relaxing);
        if (!owner_->IsStopped())
            owner_->StopMoving();
        return false;
    }

    if (state_ != State::relaxing && current_target_ != target->GetObjectGuid())
    {
        set_target_loc(target);
        return false;
    }

    // If evading we need to keep checking if we can't get a path
    if (evading_)
    {
        // Try to make a new path if target moved
        if (waiting_path_id_ == 0 &&
            (std::abs(target_last_pos_.x - target->GetX()) > 0.5f ||
                std::abs(target_last_pos_.y - target->GetY()) > 0.5f ||
                std::abs(target_last_pos_.z - target->GetZ()) > 0.5f))
        {
            set_target_loc(target);
        }
        return false;
    }

    if (state_ == State::relaxing)
    {
        bool can_back = true;
        if (owner_->GetTypeId() == TYPEID_UNIT &&
            static_cast<Creature*>(owner_)->GetCreatureInfo()->flags_extra &
                CREATURE_FLAG_EXTRA_CHASE_GEN_NO_BACKING)
            can_back = false;

        if (target_oor(target))
            set_state(State::chasing);
        else if (can_back && target_deep_in_bounds(target))
            set_state(State::backing);
        else
        {
            if (spread_timer_ <= (int)diff)
            {
                spread_timer_ = irand(500, 3000);
                do_spread_if_needed(target);
            }
            else
                spread_timer_ -= diff;
        }

        if (owner_->GetTypeId() == TYPEID_UNIT &&
            !owner_->HasInArc(0.01f, target) &&
            !owner_->hasUnitState(UNIT_STAT_CANNOT_ROTATE))
            owner_->SetOrientation(owner_->GetAngle(target));
    }
    else if (state_ == State::chasing)
    {
        bool stopped = waiting_path_id_ == 0 && owner_->movespline->Finalized();
        bool moving = target->is_moving();

        // Don't consider NPC chasing us moving
        if (moving && target->GetTypeId() == TYPEID_UNIT)
        {
            if (auto gen = target->movement_gens.top())
                if (auto chase_gen = dynamic_cast<ChaseMovementGenerator*>(gen))
                    if (chase_gen->get_target() == owner_)
                        moving = false;
        }

        // Pause if target is moving and we caught up
        if (moving && target_deep_in_bounds(target))
        {
            set_state(State::pausing);
        }
        // Refresh end loc if target moved too much
        else if (!stopped)
        {
            if (!target->GetTransport() &&
                !target->IsWithinDist3d(target_last_pos_.x, target_last_pos_.y,
                    target_last_pos_.z, 4.0f))
                set_target_loc(target);
        }
        // If we stopped, get new end loc or go back to relaxing
        else // if (stopped)
        {
            if (moving)
            {
                bool bwd_player =
                    target->GetTypeId() == TYPEID_PLAYER &&
                    target->m_movementInfo.HasMovementFlag(MOVEFLAG_BACKWARD);

                // Relax if backpedaling player
                if (bwd_player && !target_oor(target))
                    set_state(State::relaxing);
                else
                    set_target_loc(target);
            }
            else
            {
                if (target_oor(target))
                    set_target_loc(target);
                else
                    set_state(State::relaxing);
            }
        }
    }
    else if (state_ == State::pausing)
    {
        if (!target_in_bounds(target))
            set_state(State::chasing);
    }
    else if (state_ == State::backing)
    {
        // Back Timer
        if (back_timer_ && back_timer_ <= (int)diff)
        {
            back_timer_ = 0;
            do_back_movement(target);
        }
        else if (back_timer_)
        {
            back_timer_ -= diff;
            if (!target_deep_in_bounds(target))
                set_state(State::relaxing);
        }
        else
        {
            // NOTE: Back movement never causes a waiting for path id
            if (owner_->movespline->Finalized())
                set_state(State::relaxing);
        }
    }
    else // if (state_ == State::spreading)
    {
        // NOTE: Spread movement never causes a waiting for path id
        if (owner_->movespline->Finalized())
            set_state(State::relaxing);
    }

    if (waiting_path_id_ == 0)
    {
        bool can_run = owner_->GetTypeId() == TYPEID_PLAYER ?
                           true :
                           static_cast<Creature*>(owner_)->CanRun();
        float speed =
            can_run ? owner_->GetSpeed(MOVE_RUN) : owner_->GetSpeed(MOVE_WALK);
        if (speed != speed_)
        {
            if (!owner_->movespline->Finalized())
            {
                owner_->StopMoving();
                set_target_loc(target);
            }
            speed_ = speed;
        }
    }

    return false;
}

void ChaseMovementGenerator::finished_path(
    std::vector<G3D::Vector3> path, uint32 id)
{
    if (waiting_path_id_ != id)
        return;

    waiting_path_id_ = 0;
    path_ = path;
    handle_path_update();
}

Unit* ChaseMovementGenerator::get_target() const
{
    auto guid = target_.IsEmpty() ? owner_->GetVictimGuid() : target_;
    return owner_->GetMap()->GetUnit(guid);
}

std::string ChaseMovementGenerator::debug_str() const
{
    auto target = owner_->GetMap()->GetUnit(current_target_);

    auto str =
        "state: " + std::string(stringify(state_)) + "; waiting id: " +
        std::to_string(waiting_path_id_) + "; path len: " +
        std::to_string(path_.size()) + "; spline done: " +
        (owner_->movespline->Finalized() ? "true" : "false") + "; spell: " +
        (casted_spell_ ? "true" : "false") + "; evade: " +
        (evading_ ? "true" : "false") + "; current target: " +
        (target ? target->GetObjectGuid().GetString().c_str() : "<none>");

    if (path_.size() >= 2)
    {
        char buf[256] = {0};
        snprintf(buf, 256,
            "; start: (%.2f, %.2f, %.2f); end: (%.2f, %.2f, %.2f)", path_[0].x,
            path_[0].y, path_[0].z, path_[1].x, path_[1].y, path_[1].z);
        str += buf;
    }
    return str;
}

void ChaseMovementGenerator::set_state(State state)
{
    LOG_DEBUG(chase_log, "%s state change: %s --> %s",
        owner_->GetObjectGuid().GetString().c_str(), stringify(state_),
        stringify(state));
    state_ = state;

    if (state_ == State::relaxing)
        spread_timer_ = irand(500, 3000);
    else if (state_ == State::pausing)
        owner_->StopMoving();
    else if (state_ == State::backing)
        back_timer_ = 1000;
}

void ChaseMovementGenerator::set_target_loc(Unit* target)
{
    path_.clear();

    if (owner_->hasUnitState(UNIT_STAT_NOT_MOVE))
        return;

    bool moving = target->is_moving(true);

    // Don't consider NPC chasing us moving
    if (moving && target->GetTypeId() == TYPEID_UNIT)
    {
        if (auto gen = target->movement_gens.top())
            if (auto chase_gen = dynamic_cast<ChaseMovementGenerator*>(gen))
                if (chase_gen->get_target() == owner_)
                    moving = false;
    }

    // NOTE: When backpedaling we don't predict destination, that's to make it
    //       easier for tanks to reposition NPCs
    bool bwd_player = target->GetTypeId() == TYPEID_PLAYER &&
                      target->m_movementInfo.HasMovementFlag(MOVEFLAG_BACKWARD);

    G3D::Vector3 pos;
    if (moving && !bwd_player)
        pos = target->predict_dest(500, false);
    else if (bwd_player)
        pos = target->GetPoint(target->GetAngle(owner_) - target->GetO(),
            (target->GetObjectBoundingRadius() +
                owner_->GetObjectBoundingRadius()) *
                0.5f,
            true, false, true);

    target_last_pos_ =
        G3D::Vector3(target->GetX(), target->GetY(), target->GetZ());

    current_target_ = target->GetObjectGuid();

    LOG_DEBUG(chase_log, "%s updated target location to (%.2f, %.2f, %.2f)",
        owner_->GetObjectGuid().GetString().c_str(), target_last_pos_.x,
        target_last_pos_.y, target_last_pos_.z);

    uint32 id;
    if (moving)
        id = movement::BuildRetailLikePath(path_, owner_, pos);
    else
        id = movement::BuildRetailLikePath(path_, owner_, target);
    if (!id)
        handle_path_update();
    else
        waiting_path_id_ = id;
}

void ChaseMovementGenerator::handle_path_update()
{
    // Ignore queued path if something now prevents our moving
    if (owner_->hasUnitState(UNIT_STAT_NOT_MOVE) ||
        owner_->IsCastedSpellPreventingMovementOrAttack() || casted_spell_)
    {
        return;
    }

    // If we're put in evade mode we need to stand still until we get a normal
    // path generated for us
    if (path_.empty())
    {
        // Evade pets if we can't get a path
        if (owner_->GetTypeId() == TYPEID_UNIT &&
            static_cast<Creature*>(owner_)->behavior())
        {
            if (static_cast<Creature*>(owner_)->behavior()->try_evade())
                return;
        }

        if (!evading_)
        {
            evading_ = true;
            if (owner_->GetTypeId() == TYPEID_UNIT &&
                owner_->GetCharmerOrOwnerPlayerOrPlayerItself() == nullptr)
                static_cast<Creature*>(owner_)->start_evade();
            movement::MoveSplineInit init(*owner_);
            init.Launch();
        }
        return;
    }

    movement::MoveSplineInit init(*owner_);
    init.MovebyPath(path_);
    bool can_run = owner_->GetTypeId() == TYPEID_PLAYER ?
                       true :
                       static_cast<Creature*>(owner_)->CanRun();
    init.SetWalk(can_run ? false : true);
    init.Launch();
    owner_->addUnitState(UNIT_STAT_CHASE_MOVE);

    // We got a normal path; clear evade mode!
    if (evading_)
    {
        evading_ = false;
        if (owner_->GetTypeId() == TYPEID_UNIT &&
            owner_->GetCharmerOrOwnerPlayerOrPlayerItself() == nullptr)
            static_cast<Creature*>(owner_)->stop_evade();
    }
}

void ChaseMovementGenerator::do_back_movement(Unit* target)
{
    float radius = 0.0f;

    // Prevent two NPCs backing OOR of each other
    if (auto chase =
            dynamic_cast<ChaseMovementGenerator*>(target->movement_gens.top()))
    {
        if (chase->get_target() == owner_)
            radius = -target->GetObjectBoundingRadius();
    }

    auto pos = target->GetPoint(owner_, radius, true, false, true);

    movement::MoveSplineInit init(*owner_);
    init.MoveTo(pos);
    init.SetWalk(true);
    init.SetFacing(target);
    init.Launch();
}

void ChaseMovementGenerator::do_spread_if_needed(Unit* target)
{
    // Move away from any NPC deep in our bounding box. There's no limit to the
    // angle moved; NPCs will eventually start spreading behind the target if
    // there's enough of them.

    Unit* spread_from = nullptr;

    for (auto& attacker : target->getAttackers())
    {
        if (attacker->GetTypeId() == TYPEID_UNIT && attacker != owner_ &&
            owner_->GetObjectBoundingRadius() - 2.0f <
                attacker->GetObjectBoundingRadius() &&
            attacker->movespline->Finalized() &&
            target_bounds_pct_dist(attacker, 0.15f))
        {
            spread_from = attacker;
            break;
        }
    }

    if (!spread_from)
        return;

    float my_angle = target->GetAngle(owner_);
    float his_angle = target->GetAngle(spread_from);
    float new_angle;
    if (his_angle > my_angle)
        new_angle = my_angle - frand(0.4f, 1.0f);
    else
        new_angle = my_angle + frand(0.4f, 1.0f);

    auto pos = target->GetPoint(new_angle - target->GetO(),
        owner_->GetObjectBoundingRadius(), true, false, true);
    LOG_DEBUG(chase_log,
        "%s (%s) spreading moving to the %s of %s (%s), new angle: %.2f",
        owner_->GetName(), owner_->GetObjectGuid().GetString().c_str(),
        (his_angle < my_angle ? "left" : "right"), spread_from->GetName(),
        spread_from->GetObjectGuid().GetString().c_str(), new_angle);

    spread_move(target, pos);
}

void ChaseMovementGenerator::spread_move(Unit* target, const G3D::Vector3& pos)
{
    movement::MoveSplineInit init(*owner_);
    init.MoveTo(pos);
    init.SetWalk(true);
    init.SetFacing(target);
    init.Launch();
    state_ = State::spreading;
}

bool ChaseMovementGenerator::target_oor(Unit* target) const
{
    return !owner_->CanReachWithMeleeAttack(target, -1.0f);
}

bool ChaseMovementGenerator::target_in_bounds(Unit* target) const
{
    return target_bounds_pct_dist(target, 1.0f);
}

bool ChaseMovementGenerator::target_deep_in_bounds(Unit* target) const
{
    return target_bounds_pct_dist(target, 0.3f);
}

bool ChaseMovementGenerator::target_bounds_pct_dist(
    Unit* target, float pct) const
{
    float radius =
        (target->GetObjectBoundingRadius() + owner_->GetObjectBoundingRadius());
    radius *= pct;

    float dx = target->GetX() - owner_->GetX();
    float dy = target->GetY() - owner_->GetY();
    float dz = target->GetZ() - owner_->GetZ();

    return dx * dx + dy * dy + dz * dz < radius * radius;
}

FollowMovementGenerator::FollowMovementGenerator(
    Unit* target, float dist, float angle)
  : state_(State::relaxing), speed_(0), target_(target->GetObjectGuid()),
    waiting_path_id_(0), offset_(dist), angle_(angle), last_x_(0), last_y_(0),
    casted_spell_(false), evading_(false), target_was_moving_(false)
{
}

void FollowMovementGenerator::start()
{
    speed_ = owner_->GetSpeed(MOVE_RUN);

    owner_->addUnitState(UNIT_STAT_FOLLOW | UNIT_STAT_FOLLOW_MOVE);
    owner_->StopMoving();
    state_ = State::relaxing;
    waiting_path_id_ = 0;
    last_x_ = 0;
    last_y_ = 0;
}

void FollowMovementGenerator::stop()
{
    owner_->clearUnitState(UNIT_STAT_FOLLOW | UNIT_STAT_FOLLOW_MOVE);

    if (evading_)
        evading_ = false;
}

bool FollowMovementGenerator::update(uint32, uint32)
{
    auto target = owner_->GetMap()->GetUnit(target_);

    if (!target)
        return true;

    if (owner_->hasUnitState(UNIT_STAT_NOT_MOVE))
    {
        owner_->clearUnitState(UNIT_STAT_CHASE_MOVE);
        return false;
    }

    auto casting = owner_->IsCastedSpellPreventingMovementOrAttack();
    if (casting && !casted_spell_)
    {
        LOG_DEBUG(follow_log, "%s begun casting a spell, and %s moving",
            owner_->GetObjectGuid().GetString().c_str(),
            owner_->IsStopped() ? "was not" : "stopped");

        casted_spell_ = true;
        if (!owner_->IsStopped())
            owner_->StopMoving();
        return false;
    }
    else if (!casting && casted_spell_)
    {
        LOG_DEBUG(follow_log, "%s ended its spell cast, and %s its movement",
            owner_->GetObjectGuid().GetString().c_str(),
            owner_->IsStopped() ? "restarted" : "never stopped");

        casted_spell_ = false;
        if (owner_->IsStopped())
        {
            state_ = State::relaxing;
            LOG_DEBUG(follow_log,
                "%s switched to State::relaxing after spellcast",
                owner_->GetObjectGuid().GetString().c_str());
        }
        return false;
    }
    else if (casting)
    {
        return false;
    }

    if (state_ == State::relaxing)
    {
        if (target_oor(target))
        {
            state_ = State::following;
            LOG_DEBUG(follow_log, "%s switched to State::following",
                owner_->GetObjectGuid().GetString().c_str());
        }
        else
        {
            last_x_ = target->GetX();
            last_y_ = target->GetY();
        }
    }
    else // if (state_ == State::following)
    {
        bool stopped = waiting_path_id_ == 0 && owner_->movespline->Finalized();
        if (!stopped &&
            owner_->IsWithinDist3d(end_.x, end_.y, end_.z, 0.8f, false))
            stopped = true;

        if (!stopped && !target->GetTransport() &&
            !target->IsWithinDist3d(target_last_pos_.x, target_last_pos_.y,
                target_last_pos_.z, 4.0f))
        {
            set_target_loc(target);
        }
        else if (target->is_moving())
        {
            if (stopped)
            {
                set_target_loc(target);
                target_was_moving_ = true;
            }
        }
        else
        {
            if (!stopped && target_was_moving_)
            {
                set_target_loc(target);
            }
            else if (stopped && !target_oor(target))
            {
                state_ = State::relaxing;
                LOG_DEBUG(follow_log, "%s switched to State::relaxing",
                    owner_->GetObjectGuid().GetString().c_str());

                // Face same direction as target
                if (owner_->GetTypeId() == TYPEID_UNIT &&
                    !owner_->hasUnitState(UNIT_STAT_CANNOT_ROTATE))
                {
                    float o = target->GetTransport() ?
                                  target->m_movementInfo.transport.pos.o :
                                  target->GetO();
                    owner_->SetFacingTo(o);
                }
            }
            else if (stopped)
            {
                set_target_loc(target);
            }

            target_was_moving_ = false;
        }
    }

    if (waiting_path_id_ == 0)
    {
        float speed = !owner_->IsWalking() ? owner_->GetSpeed(MOVE_RUN) :
                                             owner_->GetSpeed(MOVE_WALK);
        if (speed != speed_)
        {
            if (!owner_->movespline->Finalized())
            {
                owner_->StopMoving();
                set_target_loc(target);
            }
            speed_ = speed;
        }
    }

    return false;
}

void FollowMovementGenerator::finished_path(
    std::vector<G3D::Vector3> path, uint32 id)
{
    if (waiting_path_id_ != id)
        return;

    waiting_path_id_ = 0;
    path_ = path;
    handle_path_update();
}

bool FollowMovementGenerator::target_oor(Unit* target) const
{
    // If target hasn't moved since we last checked, we're in range
    if (last_x_ == target->GetX() && last_y_ == target->GetY())
        return false;

    float dist = offset_ * 1.2f;
    if (dist < 3)
        dist = 3;

    // Make a "fake" point to estimate where about we should stand
    // NOTE: this does not include bounding box, and as such will always be a
    // bit closer than the real point; this seems to work out well in practice
    float fake_x = target->GetX() + cos(angle_ + target->GetO()) * offset_;
    float fake_y = target->GetY() + sin(angle_ + target->GetO()) * offset_;
    float fake_z = target->GetZ();

    float dx2 = (fake_x - owner_->GetX()) * (fake_x - owner_->GetX());
    float dy2 = (fake_y - owner_->GetY()) * (fake_y - owner_->GetY());
    float dz2 = (fake_z - owner_->GetZ()) * (fake_z - owner_->GetZ());

    return dx2 + dy2 + dz2 > dist * dist;
}

void FollowMovementGenerator::set_target_loc(Unit* target)
{
    path_.clear();

    if (owner_->hasUnitState(UNIT_STAT_NOT_MOVE))
        return;

    target_last_pos_ =
        G3D::Vector3(target->GetX(), target->GetY(), target->GetZ());

    float o = target->GetTransport() ? target->m_movementInfo.transport.pos.o :
                                       target->GetO();

    auto predict = target->predict_dest(1000);
    auto pos = target->GetPointXYZ(predict, angle_ + o,
        offset_ + owner_->GetObjectBoundingRadius(), true, false, true);
    end_ = pos;

    LOG_DEBUG(follow_log, "%s updated target location to (%.2f, %.2f, %.2f)",
        owner_->GetObjectGuid().GetString().c_str(), pos.x, pos.y, pos.z);

    uint32 id = movement::BuildRetailLikePath(path_, owner_, pos);
    if (!id)
        handle_path_update();
    else
        waiting_path_id_ = id;
}

void FollowMovementGenerator::handle_path_update()
{
    // Ignore queued path if something now prevents our moving
    if (owner_->hasUnitState(UNIT_STAT_NOT_MOVE) ||
        owner_->IsCastedSpellPreventingMovementOrAttack() || casted_spell_)
        return;

    auto target = owner_->GetMap()->GetUnit(target_);
    if (!target)
        return;

    last_x_ = target->GetX();
    last_y_ = target->GetY();

    // If we're put in evade mode we need to stand still until we get a normal
    // path generated for us
    if (path_.empty())
    {
        if (!evading_)
        {
            evading_ = true;
            movement::MoveSplineInit init(*owner_);
            init.Launch();
        }
        return;
    }

    bool run = !target->IsWalking();
    bool far = false;
    float speed = target->GetSpeed(run ? MOVE_RUN : MOVE_WALK);

    if (path_.size() >= 2)
    {
        float x2 = (end_.x - owner_->GetX()) * (end_.x - owner_->GetX());
        float y2 = (end_.y - owner_->GetY()) * (end_.y - owner_->GetY());
        float z2 = (end_.z - owner_->GetZ()) * (end_.z - owner_->GetZ());
        float dist = (2.0f + speed) * (2.0f + speed);
        if (x2 + y2 + z2 > dist)
            far = true;
    }

    // Far away, more speed
    if (far)
        speed *= 1.3f;

    movement::MoveSplineInit init(*owner_);
    init.MovebyPath(path_);
    init.SetWalk(!run);
    init.SetVelocity(speed);
    init.Launch();
    owner_->addUnitState(UNIT_STAT_FOLLOW_MOVE);

    // We got a normal path; clear evade mode!
    evading_ = false;
}
}
