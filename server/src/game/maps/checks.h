/**
 * checks.h
 *
 * Here are checks meant to be used together with visitor functors which you can
 * find in visitors.h. Only common checks are put here.
 *
 * NOTE: If a check is a one-off thing, you should just use a lambda instead of
 *       adding code here.
 */

#ifndef GAME__MAPS__CHECKS_H
#define GAME__MAPS__CHECKS_H

#include "framework/grid/grid.h"
#include <boost/logic/tribool.hpp>
#include <vector>

namespace maps
{
namespace checks
{
struct hurt_friend
{
    // Only targets less than max % hp will be considered
    hurt_friend(const Unit* obj, bool not_self = false, float max = 1.0f)
      : obj_{obj}, not_self_{not_self}, max_{max}
    {
    }

    bool operator()(Unit* u)
    {
        if ((u != obj_ || !not_self_) &&
            !u->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE) &&
            !u->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE) &&
            u->isAlive() && u->isInCombat() && !obj_->IsHostileTo(u) &&
            u->GetMaxHealth() > 0)
        {
            float perc = (float)u->GetHealth() / (float)u->GetMaxHealth();
            if (perc < max_)
            {
                max_ = perc;
                return true;
            }
        }
        return false;
    }

    const Unit* obj_;
    float max_;
    bool not_self_;
};

struct missing_buff
{
    missing_buff(const WorldObject* obj, uint32 spell_id)
      : obj_{obj}, spell_id_{spell_id}
    {
    }

    bool operator()(Unit* u)
    {
        return u->isAlive() && u->isInCombat() && !obj_->IsHostileTo(u) &&
               !u->has_aura(spell_id_);
    }

    WorldObject const* obj_;
    uint32 spell_id_;
};

struct friendly_status
{
    enum status
    {
        friendly,
        not_friendly,
        hostile,
        not_hostile
    };

    friendly_status(const WorldObject* obj, status s) : obj_{obj}, status_{s} {}

    bool operator()(Unit* u)
    {
        if (!u->isAlive())
            return false;
        if (status_ == friendly && obj_->IsFriendlyTo(u))
            return true;
        else if (status_ == not_friendly && !obj_->IsFriendlyTo(u))
            return true;
        else if (status_ == not_hostile && !obj_->IsHostileTo(u))
            return true;
        else if (status_ == hostile && obj_->IsHostileTo(u))
            return true;
        return false;
    }

    WorldObject const* obj_;
    status status_;
};

BOOST_TRIBOOL_THIRD_STATE(alive_or_dead);
struct entry_guid
{
    // if either entry or guid is 0, it is always considered a match
    entry_guid(uint32 entry, uint32 guid, const WorldObject* ignore = nullptr,
        boost::logic::tribool alive = alive_or_dead)
      : entry_{entry}, guid_{guid}, ignore_{ignore}, alive_{alive}
    {
    }

    bool operator()(const WorldObject* elem)
    {
        if (alive_)
        {
            if (elem->GetTypeId() != TYPEID_UNIT &&
                elem->GetTypeId() != TYPEID_PLAYER)
                return false;
            if (!static_cast<const Unit*>(elem)->isAlive())
                return false;
        }
        else if (!alive_)
        {
            if (elem->GetTypeId() != TYPEID_UNIT &&
                elem->GetTypeId() != TYPEID_PLAYER)
                return false;
            if (static_cast<const Unit*>(elem)->isAlive())
                return false;
        }
        // else: intermediate, either state is okay

        if ((entry_ == 0 || elem->GetEntry() == entry_) &&
            (guid_ == 0 || elem->GetGUIDLow() == guid_) && ignore_ != elem)
            return true;
        return false;
    }

    uint32 entry_;
    uint32 guid_;
    const WorldObject* ignore_;
    boost::logic::tribool alive_;
};

struct friendly_crowd_controlled
{
    friendly_crowd_controlled(const Unit* obj) : obj_{obj} {}

    bool operator()(const Unit* u)
    {
        return u->isAlive() && u->isInCombat() && !obj_->IsHostileTo(u) &&
               (u->isCharmed() || u->isFrozen() ||
                   u->hasUnitState(UNIT_STAT_CAN_NOT_REACT));
    }

    const Unit* obj_;
};
}
}

#endif
