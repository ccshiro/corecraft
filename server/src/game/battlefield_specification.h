#ifndef GAME__BATTLEFIELD_SPECIFICATION_H
#define GAME__BATTLEFIELD_SPECIFICATION_H

#include "SharedDefines.h"
#include <stdexcept>
#include <utility>

namespace battlefield
{
// TODO: make this an enumeration class when we support C++11
enum type
{
    random_battleground,
    alterac_valley,
    warsong_gulch,
    arathi_basin,
    eye_of_the_storm,
    rated_2v2,
    rated_3v3,
    rated_5v5,
    skirmish_2v2,
    skirmish_3v3,
    skirmish_5v5
};

class bracket
{
public:
    bracket() : level_(0), level_increase_(10) {}
    bracket(int low, int increase = 10) : level_(low), level_increase_(increase)
    {
    }

    int min() const { return level_; }
    int max() const { return level_ + level_increase_ - 1; }

    void operator++() { level_ += level_increase_; }
    void operator--() { level_ -= level_increase_; }
    bracket operator++(int)
    {
        bracket temp(*this);
        ++*this;
        return temp;
    }
    bracket operator--(int)
    {
        bracket temp(*this);
        --*this;
        return temp;
    }

    // Comparison operators
    bool operator<(const bracket& rhs) const { return level_ < rhs.level_; }
    bool operator<=(const bracket& rhs) const { return level_ <= rhs.level_; }
    bool operator>(const bracket& rhs) const { return level_ > rhs.level_; }
    bool operator>=(const bracket& rhs) const { return level_ >= rhs.level_; }
    bool operator==(const bracket& rhs) const { return level_ == rhs.level_; }
    bool operator!=(const bracket& rhs) const { return level_ != rhs.level_; }

    explicit operator bool() const { return level_ != 0; }

private:
    int level_;
    int level_increase_;
};

class specification
{
public:
    explicit specification(type t, bracket b = bracket(0))
      : type_(t), bracket_(std::move(b))
    {
        type_construction(t);
    }
    explicit specification(type t, int player_level) : type_(t)
    {
        type_construction(t);
        bracket_construction(player_level);
    }

    explicit specification(BattleGroundTypeId blizzard_id, int player_level,
        ArenaType arena_size = ARENA_TYPE_NONE, bool rated = false)
      : blizzard_id_(blizzard_id), arena_size_(arena_size), rated_(rated)
    {
        blizzard_construction(blizzard_id, arena_size, rated);
        bracket_construction(player_level);
    }
    explicit specification(BattleGroundTypeId blizzard_id,
        bracket b = bracket(0), ArenaType arena_size = ARENA_TYPE_NONE,
        bool rated = false)
      : blizzard_id_(blizzard_id), arena_size_(arena_size), rated_(rated),
        bracket_(std::move(b))
    {
        blizzard_construction(blizzard_id, arena_size, rated);
    }

    BattleGroundTypeId blizzard_id() const { return blizzard_id_; }
    type get_type() const { return type_; }
    bool arena() const { return blizzard_id_ == BATTLEGROUND_AA; }
    ArenaType arena_size() const { return arena_size_; }
    bool rated() const { return rated_; }

    // return 0 for non arena
    uint32 get_max_queue_size() const
    {
        switch (type_)
        {
        case rated_2v2:
        case skirmish_2v2:
            return 2;
        case rated_3v3:
        case skirmish_3v3:
            return 3;
        case rated_5v5:
        case skirmish_5v5:
            return 5;
        default:
            break;
        }
        return 0;
    }

    bracket min_bracket() const
    {
        switch (type_)
        {
        case alterac_valley:
            return bracket(51);
        case warsong_gulch:
            return bracket(10);
        case arathi_basin:
            return bracket(10);
        case eye_of_the_storm:
            return bracket(60); // Todo: Should add a minimum level for
                                // battlegrounds so we can support EotS being
                                // 61-69 without exceptions in the bracket code
        // Arenas
        case rated_2v2:
        case rated_3v3:
        case rated_5v5:
            return bracket(70);
        case skirmish_2v2:
        case skirmish_3v3:
        case skirmish_5v5:
            return bracket(10, 5);
        default:
            break;
        }
        return bracket(0);
    }
    bracket max_bracket() const
    {
        switch (type_)
        {
        case alterac_valley:
            return bracket(61);
        case warsong_gulch:
            return bracket(70);
        case arathi_basin:
            return bracket(70);
        case eye_of_the_storm:
            return bracket(70);
        // Arenas
        case rated_2v2:
        case rated_3v3:
        case rated_5v5:
            return bracket(70);
        case skirmish_2v2:
        case skirmish_3v3:
        case skirmish_5v5:
            return bracket(70, 5);
        default:
            break;
        }
        return bracket(0);
    }

    bracket get_bracket() const { return bracket_; }

    bool operator==(const specification& rhs) const
    {
        return type_ == rhs.type_ && bracket_ == rhs.bracket_;
    }
    bool operator!=(const specification& rhs) const { return !(*this == rhs); }

private:
    type type_;
    BattleGroundTypeId blizzard_id_;
    ArenaType arena_size_;
    bool rated_;
    bracket bracket_;

    void type_construction(type t)
    {
        switch (t)
        {
        case random_battleground:
            blizzard_id_ = BATTLEGROUND_TYPE_NONE;
            arena_size_ = ARENA_TYPE_NONE;
            rated_ = false;
            break;
        case alterac_valley:
            blizzard_id_ = BATTLEGROUND_AV;
            arena_size_ = ARENA_TYPE_NONE;
            rated_ = false;
            break;
        case warsong_gulch:
            blizzard_id_ = BATTLEGROUND_WS;
            arena_size_ = ARENA_TYPE_NONE;
            rated_ = false;
            break;
        case arathi_basin:
            blizzard_id_ = BATTLEGROUND_AB;
            arena_size_ = ARENA_TYPE_NONE;
            rated_ = false;
            break;
        case eye_of_the_storm:
            blizzard_id_ = BATTLEGROUND_EY;
            arena_size_ = ARENA_TYPE_NONE;
            rated_ = false;
            break;
        case rated_2v2:
            blizzard_id_ = BATTLEGROUND_AA;
            arena_size_ = ARENA_TYPE_2v2;
            rated_ = true;
            break;
        case rated_3v3:
            blizzard_id_ = BATTLEGROUND_AA;
            arena_size_ = ARENA_TYPE_3v3;
            rated_ = true;
            break;
        case rated_5v5:
            blizzard_id_ = BATTLEGROUND_AA;
            arena_size_ = ARENA_TYPE_5v5;
            rated_ = true;
            break;
        case skirmish_2v2:
            blizzard_id_ = BATTLEGROUND_AA;
            arena_size_ = ARENA_TYPE_2v2;
            rated_ = false;
            break;
        case skirmish_3v3:
            blizzard_id_ = BATTLEGROUND_AA;
            arena_size_ = ARENA_TYPE_3v3;
            rated_ = false;
            break;
        case skirmish_5v5:
            blizzard_id_ = BATTLEGROUND_AA;
            arena_size_ = ARENA_TYPE_5v5;
            rated_ = false;
            break;
        }
    }

    void blizzard_construction(
        BattleGroundTypeId blizzard_id, ArenaType arena_size, bool rated)
    {
        switch (blizzard_id)
        {
        case BATTLEGROUND_TYPE_NONE:
            type_ = random_battleground;
            break;
        case BATTLEGROUND_AV:
            type_ = alterac_valley;
            break;
        case BATTLEGROUND_WS:
            type_ = warsong_gulch;
            break;
        case BATTLEGROUND_AB:
            type_ = arathi_basin;
            break;
        case BATTLEGROUND_EY:
            type_ = eye_of_the_storm;
            break;

        case BATTLEGROUND_AA:
        case BATTLEGROUND_NA:
        case BATTLEGROUND_BE:
        case BATTLEGROUND_RL:
            if (rated)
            {
                switch (arena_size)
                {
                case ARENA_TYPE_2v2:
                    type_ = rated_2v2;
                    break;
                case ARENA_TYPE_3v3:
                    type_ = rated_3v3;
                    break;
                case ARENA_TYPE_5v5:
                    type_ = rated_5v5;
                    break;
                default:
                    throw std::runtime_error("Invalid arena_size");
                }
            }
            else
            {
                switch (arena_size)
                {
                case ARENA_TYPE_2v2:
                    type_ = skirmish_2v2;
                    break;
                case ARENA_TYPE_3v3:
                    type_ = skirmish_3v3;
                    break;
                case ARENA_TYPE_5v5:
                    type_ = skirmish_5v5;
                    break;
                default:
                    throw std::runtime_error("Invalid arena_size");
                }
            }
            break;
        }
    }

    // Must be called after type_ has been calculated
    void bracket_construction(int player_level)
    {
        // Truncate the player's level
        int min_level, increase;
        if (type_ == skirmish_2v2 || type_ == skirmish_3v3 ||
            type_ == skirmish_5v5)
        {
            min_level = (player_level / 5) * 5;
            increase = 5;
        }
        else if (type_ != alterac_valley)
        {
            min_level = (player_level / 10) * 10;
            increase = 10;
        }
        else
        {
            // Alterac Valley
            int i = 60;
            for (; player_level > i; i += 10)
                ;
            min_level = i - 9;
            increase = 10;
        }
        bracket temp = bracket(min_level, increase);
        if (temp < min_bracket())
            bracket_ = min_bracket();
        else
            bracket_ = temp;
    }
};

// For use in sorted containers, such as std::map
struct specification_compare
{
    bool operator()(const specification& x, const specification& y) const
    {
        return x.get_type() == y.get_type() ?
                   x.get_bracket() < y.get_bracket() :
                   x.get_type() < y.get_type();
    }
};
}

#endif
