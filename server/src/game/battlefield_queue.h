#ifndef GAME__BATTLEFIELD_QUEUE_H
#define GAME__BATTLEFIELD_QUEUE_H

#include "ObjectGuid.h"
#include "SharedDefines.h"
#include "battlefield_arena_rating.h"
#include "battlefield_specification.h"
#include "Policies/Singleton.h"
#include <algorithm>
#include <cstddef>
#include <list>
#include <map>
#include <set>
#include <typeinfo>
#include <unordered_map>
#include <utility>
#include <vector>

class Player;

namespace battlefield
{
// FIXME:
// Minimal interface of a circular buffer.
// Replace with boost when we have that in master.
template <typename T, int size_>
class temp_circular_buffer
{
public:
    temp_circular_buffer() : index_(0) { buffer_.reserve(size_); }

    size_t size() const { return buffer_.size(); }
    bool full() const { return buffer_.size() == size_; }

    void clear()
    {
        buffer_.clear();
        index_ = 0;
    }
    void push_back(const T& val)
    {
        if (buffer_.size() < size_)
        {
            buffer_.push_back(val);
            // No need to manipulate index while we fill up
        }
        else
        {
            buffer_[index_++] = val;
            if (index_ == size_)
                index_ = 0;
        }
    }

    typename std::vector<T>::iterator begin() { return buffer_.begin(); }
    typename std::vector<T>::iterator end() { return buffer_.end(); }

    T& operator[](size_t index) { return buffer_[index]; }

private:
    std::vector<T> buffer_;
    size_t index_;
};

namespace queue_result
{
static const int32 joined_eye = 7;
static const int32 joined_all_arenas = 6;
static const int32 joined_basin = 3;
static const int32 joined_warsong = 2;
static const int32 joined_valley = 1;
static const int32 error_ineligible =
    0; // "Your group has joined a battleground queue, but you are not eligible"
static const int32 error_silent = -1;
static const int32 error_deserter = -2; // "You cannot join the battleground yet
                                        // because you or one of your party
                                        // members is flagged as a Deserter"
static const int32 error_same_team = -3; // "Your group is not in the same team"
static const int32 error_max_queue =
    -4; // "You can only be queued for 3 battles at once"
static const int32 error_rated_queue =
    -5; // "You cannot queue for a rated match while queued for other battles"
static const int32 error_rated_other = -6; // "You cannot queue for another
                                           // battle while queued for a rated
                                           // arena match"
static const int32 error_team_left = -7; // "Your team has left the arena queue"
}

enum team
{
    alliance = 0,
    horde = 1
};

class queue_entry
{
public:
    queue_entry(type t, const Player* leader, std::set<const Player*>& members,
        bool join_as_group, uint32 client_id = 0);

    // upgrade queue for a specific battleground to a queue for all
    // battlegrounds (this is called when the specific bg ends)
    void upgrade(uint32 client_id)
    {
        if (client_id == client_id_)
            client_id_ = 0;
    }
    uint32 client_id() const { return client_id_; }
    specification get_specification() const { return spec_; }
    team get_team() const { return team_; }
    ObjectGuid leader() const { return leader_; }
    time_t timestamp() const { return queued_time_; }
    size_t online_size() const;
    bool can_pop() const;

    bool empty() const { return players_.empty(); }
    size_t size() const { return players_.size(); }
    void erase(ObjectGuid player);

private:
    friend class queue;
    ObjectGuid leader_;
    std::vector<ObjectGuid> players_;
    specification spec_;
    team team_;
    uint32 client_id_;
    time_t queued_time_;
    bool join_as_group_;
    uint32 rating_;
    uint32 arena_team_id_;

    void calc_bracket_level(const Player* leader);
};

class queue
{
public:
    typedef std::vector<const queue_entry*> entry_list;
    typedef std::vector<ObjectGuid> player_list;
    typedef std::vector<std::pair<ObjectGuid, int32>> result;

    queue() : players_count_(0) {}
    ~queue();

    bool empty() const { return queue_.empty(); }
    size_t size() const { return players_count_; }
    entry_list current_queues(ObjectGuid player) const;
    void remove_player(ObjectGuid player, type t,
        std::vector<ObjectGuid>* out_removed_players = nullptr);
    player_list upgrade_battlefield(
        const specification& spec, uint32 client_instance_id);

    result push(queue_entry& entry);
    player_list pop(specification spec, team t, int max, int specific_bg = 0);
    player_list dual_pop(specification spec);
    arena_rating::distributor* pop_rated_arena(const specification& spec);
    player_list av_pop(const specification& spec, int alliance_max,
        int horde_max, int client_id);
    // The first in the pair is always invited as Alliance and the second in the
    // pair as Horde
    std::pair<player_list, player_list> pop_skirmish(const specification& spec);

    time_t average_wait_time(const specification& spec);

    // debug: returns exact state of queue
    std::string debug(bool print_players);
    // summary_debug: returns a summary of the queue's state
    std::string summary_debug();

private:
    typedef std::list<queue_entry*>::iterator iterator;
    typedef std::unordered_map<ObjectGuid, std::vector<queue_entry*>>
        player_map;

    void push_back(queue_entry& entry);
    void erase(iterator itr);
    void remove(queue_entry* val);

    void player_delete(queue_entry* entry, ObjectGuid player);
    void players_delete(queue_entry* entry);
    void bg_push(queue_entry& entry, result& out);
    void rated_push(queue_entry& entry, result& out);

    bool rated_team_checks(queue_entry* team_one, queue_entry* team_two);
    std::pair<player_list, player_list> get_skirmish_lists(
        const specification& spec, const std::vector<iterator>& team_one,
        const std::vector<iterator>& team_two, size_t size);

    void pop_wait_time(const specification& spec, time_t delta);

    typedef std::pair<std::vector<iterator>, size_t> select_result;
    select_result select_entries(specification spec, team t, int max,
        int client_id, const std::vector<iterator>* ignore_entries = nullptr);
    void av_pick_entries(
        const std::vector<battlefield::queue::iterator>& entries, int desired,
        int& found_entries, int& found_players);

    player_map players_;
    std::list<queue_entry*> queue_;
    size_t players_count_;

    static const time_t average_wait_time_undefined =
        2 * 60 * 60; // Counted as undefined if nothing has popped in this time
    struct average_wait_time_entry
    {
        temp_circular_buffer<time_t, 10> player_deltas_;
        time_t last_pop_;
    };
    typedef std::map<battlefield::specification, average_wait_time_entry,
        specification_compare> average_wait_map;
    average_wait_map average_wait_times_; // Only fresh pops of bgs count
                                          // towards this, to reduce abusive
                                          // manipulability (queue, get invite,
                                          // leave queue, repeat)
};

/**
 * Helper Functions
 */
inline bool fits_random_bg(type t)
{
    switch (t)
    {
    case alterac_valley:
    case warsong_gulch:
    case arathi_basin:
    case eye_of_the_storm:
        return true;
    default:
        break;
    }
    return false;
}

inline int32 queue_result_for_type(type t)
{
    using namespace queue_result;
    switch (t)
    {
    case alterac_valley:
        return joined_valley;
    case warsong_gulch:
        return joined_warsong;
    case arathi_basin:
        return joined_basin;
    case eye_of_the_storm:
        return joined_eye;

    case rated_2v2:
    case rated_3v3:
    case rated_5v5:
    case skirmish_2v2:
    case skirmish_3v3:
    case skirmish_5v5:
        return joined_all_arenas;
    default:
        break;
    }
    return error_silent;
}

} // namespace battlefield

// FIXME: The queue should not be a singleton, but apart of the battlegroundmgr
#define sBattlefieldQueue MaNGOS::Singleton<battlefield::queue>

#endif
