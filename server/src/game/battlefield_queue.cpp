// unit tests make some of the mangos includes inactive
// and replace them by small simple dummies
#ifdef UNIT_TEST_COMPILE
#include <unit_test_types.h> // sort off
#endif

#include "battlefield_queue.h"
#include "BattleGroundMgr.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "World.h"
#include "battlefield_arena_rating.h"
#include <numeric>
#include <sstream>

enum
{
    SPELL_DESERTER = 26013,
};

/**
 * Queue Entry
 */
battlefield::queue_entry::queue_entry(type t, const Player* leader,
    std::set<const Player*>& members, bool join_as_group, uint32 client_id)
  : spec_(t, leader->getLevel()), client_id_(client_id),
    join_as_group_(join_as_group), rating_(0), arena_team_id_(0)
{
    team_ = leader->GetTeam() == ALLIANCE ? alliance : horde;

    // Insert the leader into members (in case our caller didn't)
    members.insert(leader);

    // Transfer to our vector
    players_.reserve(members.size());
    for (const auto& member : members)
        players_.push_back((member)->GetObjectGuid());

    leader_ = leader->GetObjectGuid();

    queued_time_ = WorldTimer::time_no_syscall();

    if (spec_.arena() && spec_.rated())
    {
        // Note: All players not being a part of the same arena team returns 0,
        // and is later handled in the queue's push
        if (ArenaTeam* team =
                sObjectMgr::Instance()->GetArenaTeamById(leader->GetArenaTeamId(
                    t == rated_2v2 ? 0 : t == rated_3v3 ? 1 : 2)))
        {
            rating_ = arena_rating::adjusted_team_rating(players_, team);
            arena_team_id_ = team->GetId();
        }
    }
}

void battlefield::queue_entry::erase(ObjectGuid player)
{
    auto find = std::find(players_.begin(), players_.end(), player);
    if (find != players_.end())
        players_.erase(find);
}

size_t battlefield::queue_entry::online_size() const
{
    size_t count = 0;
    for (const auto& elem : players_)
        if (ObjectAccessor::FindPlayer(elem, false))
            count++;

    return count;
}

bool battlefield::queue_entry::can_pop() const
{
    // FIXME: We should merge the getting of the player pointer into one place
    // to not cause so much locking
    for (const auto& elem : players_)
    {
        // We cannot pop if any of our members has a pending invite or is in a
        // running battleground
        if (sBattleGroundMgr::Instance()->has_pending_invite(elem))
            return false;
        if (Player* player = ObjectAccessor::FindPlayer(elem, false))
            if (player->GetBattleGround())
                return false;
    }
    return true;
}

/**
 * Queue
 */
battlefield::queue::~queue()
{
    while (!queue_.empty())
        erase(queue_.begin());
}

battlefield::queue::entry_list battlefield::queue::current_queues(
    ObjectGuid player) const
{
    battlefield::queue::entry_list queue;

    auto itr = players_.find(player);
    if (itr == players_.end())
        return queue;

    queue.reserve(itr->second.size());
    for (auto& elem : itr->second)
        queue.push_back(elem);

    return queue;
}

void battlefield::queue::remove_player(
    ObjectGuid player, type t, std::vector<ObjectGuid>* out_removed_players)
{
    auto itr = players_.find(player);
    if (itr == players_.end())
        return;

    queue_entry* entry = nullptr;
    size_t index = 0;
    for (; index < itr->second.size() && !entry; ++index)
    {
        if (itr->second[index]->get_specification().get_type() == t)
        {
            entry = itr->second[index];
            break;
        }
    }

    if (!entry)
        return;

    if (entry->size() <= 1 ||
        entry->get_specification().get_type() == rated_2v2 ||
        entry->get_specification().get_type() == rated_3v3 ||
        entry->get_specification().get_type() == rated_5v5)
    {
        if (out_removed_players)
        {
            out_removed_players->reserve(entry->players_.size());
            for (auto& elem : entry->players_)
                out_removed_players->push_back(elem);
        }

        // If this is a solo queue or a rated arena queue leaving the queue
        // removes all of us
        remove(entry);
    }
    else
    {
        if (out_removed_players)
            out_removed_players->push_back(player);

        // Only this player is removed from the queue
        entry->erase(player);
        itr->second.erase(itr->second.begin() +
                          index); // Remove this entry from player's entries
        // Remove player from players_ map if he's no longer in any queue
        if (itr->second.empty())
            players_.erase(itr);
    }
}

battlefield::queue::player_list battlefield::queue::upgrade_battlefield(
    const specification& spec, uint32 client_instance_id)
{
    player_list list;
    for (auto entry : queue_)
    {
        if (entry->spec_ == spec && entry->client_id_ == client_instance_id)
        {
            entry->upgrade(client_instance_id);
            for (auto& elem : entry->players_)
                list.push_back(elem);
        }
    }
    return list;
}

void battlefield::queue::push_back(queue_entry& entry)
{
    auto new_entry = new queue_entry(entry);
    queue_.push_back(new_entry);
    for (auto& elem : entry.players_)
        players_[elem].push_back(queue_.back());
}

void battlefield::queue::erase(iterator itr)
{
    players_delete(*itr);
    delete *itr;
    queue_.erase(itr);
}

void battlefield::queue::remove(queue_entry* val)
{
    auto itr = std::find(queue_.begin(), queue_.end(), val);
    if (itr != queue_.end())
        erase(itr);
}

void battlefield::queue::player_delete(queue_entry* entry, ObjectGuid player)
{
    auto find = players_.find(player);
    if (find != players_.end())
    {
        for (auto itr = find->second.begin(); itr != find->second.end();)
        {
            if (*itr == entry)
                itr = find->second.erase(itr);
            else
                ++itr;
        }
    }
    entry->erase(player);
}

void battlefield::queue::players_delete(queue_entry* entry)
{
    for (std::size_t i = 0; i < entry->players_.size(); ++i)
    {
        auto find = players_.find(entry->players_[i]);
        if (find != players_.end())
        {
            for (auto itr = find->second.begin(); itr != find->second.end();)
            {
                if (*itr == entry)
                    itr = find->second.erase(itr);
                else
                    ++itr;
            }
            // Remove player from players_ if he no longer has any queues
            if (find->second.empty())
                players_.erase(find);
        }
    }
    entry->players_.clear();
}

battlefield::queue::result battlefield::queue::push(queue_entry& entry)
{
    result results;
    results.reserve(entry.size());
    switch (entry.get_specification().get_type())
    {
    case random_battleground:
    case alterac_valley:
    case warsong_gulch:
    case arathi_basin:
    case eye_of_the_storm:
    case skirmish_2v2:
    case skirmish_3v3:
    case skirmish_5v5:
        bg_push(entry, results);
        break;
    case rated_2v2:
    case rated_3v3:
    case rated_5v5:
        rated_push(entry, results);
        break;
    }
    return results;
}

void battlefield::queue::bg_push(queue_entry& entry, result& out)
{
    if (entry.get_specification().get_max_queue_size() != 0 &&
        entry.players_.size() > entry.get_specification().get_max_queue_size())
    {
        out.push_back(
            std::make_pair(entry.leader(), queue_result::error_silent));
        return;
    }

    // Check validity of all players
    for (auto itr = entry.players_.begin(); itr != entry.players_.end();)
    {
        ObjectGuid guid = *itr;
        Player* player = ObjectAccessor::FindPlayer(guid, false);
        if (!player)
        {
            itr = entry.players_.erase(itr);
            continue;
        }

        const battlefield::bracket bracket =
            entry.get_specification().get_bracket();
        int level = static_cast<int>(player->getLevel());
        if (level < bracket.min() || level > bracket.max()) // min & max
        {
            out.push_back(std::make_pair(guid, queue_result::error_ineligible));
            itr = entry.players_.erase(itr);
            continue;
        }

        if (player->has_aura(SPELL_DESERTER) &&
            !entry.get_specification().arena())
        {
            out.clear();
            out.push_back(
                std::make_pair(entry.leader(), queue_result::error_deserter));
            return;
        }

        int queue_size = 0; // Amount of queues player is in

        // Checks against your current battleground or invite, if you have
        // either
        BattleGround* bg = player->GetBattleGround();
        if (!bg)
            bg = sBattleGroundMgr::Instance()->get_pending_invite(guid);
        if (bg)
        {
            ++queue_size;
            type current_type = bg->get_specification().get_type();
            if (bg->isArena() && bg->isRated())
            {
                out.push_back(
                    std::make_pair(guid, queue_result::error_rated_other));
                itr = entry.players_.erase(itr);
                continue;
            }
            // We can never be queued to the same battleground we're already
            // inside
            if (current_type == entry.get_specification().get_type())
            {
                out.push_back(
                    std::make_pair(guid, queue_result::error_ineligible));
                itr = entry.players_.erase(itr);
                continue;
            }
        }

        // Checks against the queues we're already in
        auto find = players_.find(guid);
        if (find != players_.end())
        {
            // Player cannot be in a rated queue and join another queue
            bool cont = false;
            for (auto& elem : find->second)
            {
                if (elem->get_specification().get_type() == rated_2v2 ||
                    elem->get_specification().get_type() == rated_3v3 ||
                    elem->get_specification().get_type() == rated_5v5)
                {
                    out.push_back(
                        std::make_pair(guid, queue_result::error_rated_other));
                    itr = entry.players_.erase(itr);
                    cont = true;
                    break;
                }
            }
            if (cont)
                continue;

            // A player can only be in 3 consecutive queues (we also keep in
            // mind overwriting queues with the one we're adding)
            queue_size += find->second.size();
            for (auto& elem : find->second)
            {
                if (elem->get_specification().get_type() ==
                    entry.get_specification().get_type())
                    --queue_size; // Same queue type found
            }
            if (queue_size >= 3)
            {
                out.push_back(
                    std::make_pair(guid, queue_result::error_max_queue));
                itr = entry.players_.erase(itr);
                continue;
            }
        }

        // Passed all checks
        if (entry.join_as_group_)
            out.push_back(std::make_pair(guid,
                queue_result_for_type(entry.get_specification().get_type())));
        ++itr;
    }

    if (entry.empty())
        return;

    // Passed validity checks. Remove the players' current queue of the type
    // they're joining
    for (size_t i = 0; i < entry.players_.size(); ++i)
        remove_player(entry.players_[i], entry.get_specification().get_type());

    push_back(entry);
}

void battlefield::queue::rated_push(queue_entry& entry, result& out)
{
    if (entry.get_specification().get_max_queue_size() != 0 &&
        entry.players_.size() > entry.get_specification().get_max_queue_size())
    {
        out.push_back(
            std::make_pair(entry.leader(), queue_result::error_silent));
        return;
    }

    uint32 queueing_team_id = 0;
    size_t valid_players = 0;

    // Check validity of all players
    for (size_t i = 0; i < entry.players_.size(); ++i)
    {
        ObjectGuid guid = entry.players_[i];
        Player* player = ObjectAccessor::FindPlayer(guid, false);
        if (!player)
        {
            out.clear();
            for (auto& elem : entry.players_)
                out.push_back(std::make_pair(elem, queue_result::error_silent));
            return;
        }

        // Note we fail if an error happens to anyone, and we report that error
        // to everyone

        int level = static_cast<int>(player->getLevel());
        const bracket b = entry.get_specification().get_bracket();
        if (level < b.min() || level > b.max()) // min & max
        {
            out.clear();
            for (auto& elem : entry.players_)
                out.push_back(
                    std::make_pair(elem, queue_result::error_rated_queue));
            return;
        }

        // We can never be in an on-going battleground, arena or a pending
        // invite when we queue for rated
        if (player->GetBattleGround() ||
            sBattleGroundMgr::Instance()->get_pending_invite(guid))
        {
            out.clear();
            for (auto& elem : entry.players_)
                out.push_back(
                    std::make_pair(elem, queue_result::error_rated_queue));
            return;
        }

        // Cannot be in a queue already
        auto find = players_.find(guid);
        if (find != players_.end())
        {
            out.clear();
            for (auto& elem : entry.players_)
                out.push_back(
                    std::make_pair(elem, queue_result::error_rated_queue));
            return;
        }

        // All players must be in the same arena team for the slot we're trying
        // to queue
        uint32 team_id = player->GetArenaTeamId(
            entry.get_specification().get_type() == rated_2v2 ?
                0 :
                entry.get_specification().get_type() == rated_3v3 ? 1 : 2);
        if (team_id == 0 ||
            (queueing_team_id != 0 && team_id != queueing_team_id))
        {
            out.clear();
            for (auto& elem : entry.players_)
                out.push_back(
                    std::make_pair(elem, queue_result::error_same_team));
            return;
        }

        if (!queueing_team_id)
            queueing_team_id = team_id;

        ++valid_players;

        out.push_back(std::make_pair(
            guid, queue_result_for_type(entry.get_specification().get_type())));
    }

    ArenaTeam* team = nullptr;
    if (!queueing_team_id ||
        !(team = sObjectMgr::Instance()->GetArenaTeamById(queueing_team_id)))
    {
        out.clear();
        for (auto& elem : entry.players_)
            out.push_back(std::make_pair(elem, queue_result::error_silent));
        return;
    }

    // Check for fractions of our team, and make sure our queueing does not
    // clash with their activity
    for (auto itr = team->m_membersBegin(); itr != team->m_membersEnd(); ++itr)
    {
        // If this member is already in a queue, overwrite it for him
        auto find = players_.find(itr->guid);
        if (find != players_.end())
        {
            // Get first queue item. If player is in queue for a rated arena, he
            // can only have one.
            queue_entry* inner_entry = nullptr;
            if (!find->second.empty())
                inner_entry = *find->second.begin();

            // If they're in queue for a rated, we need to remove all of them
            // from the queue
            if (inner_entry && inner_entry->spec_.arena() &&
                inner_entry->spec_.rated())
            {
                // Add all players in the inner_entry to our out vector so they
                // are informed about their leaving the queue
                for (auto p_itr = inner_entry->players_.begin();
                     p_itr != inner_entry->players_.end(); ++itr)
                    out.push_back(
                        std::make_pair(*p_itr, queue_result::error_team_left));
                // Erase the inner_entry
                remove(inner_entry);
                break; // No need to progress further, we can't have more
                       // fractions doing arena
            }
        }

        // Players cannot be in an already rated arena (or have a pending invite
        // to one) using our team
        Player* player = sObjectMgr::Instance()->GetPlayer(itr->guid, false);
        if (!player)
            continue;
        BattleGround* bg = player->GetBattleGround() ?
                               player->GetBattleGround() :
                               sBattleGroundMgr::Instance()->get_pending_invite(
                                   player->GetObjectGuid());
        if (bg && bg->isArena() && bg->isRated() &&
            bg->GetTeamIndexByTeamId(player->GetBGTeam()) == queueing_team_id)
        {
            out.clear();
            for (auto& elem : entry.players_)
                out.push_back(std::make_pair(elem, queue_result::error_silent));
            return;
        }
    }

    switch (entry.spec_.get_type())
    {
    case rated_2v2:
        if (valid_players == 2)
            break;
    case rated_3v3:
        if (valid_players == 3)
            break;
    case rated_5v5:
        if (valid_players == 5)
            break;
    default:
        // Improper size or type.
        out.clear();
        for (auto& elem : entry.players_)
            out.push_back(std::make_pair(elem, queue_result::error_silent));
        return;
    }

    push_back(entry);
}

battlefield::queue::player_list battlefield::queue::pop(
    specification spec, team t, int max, int client_id)
{
    std::pair<std::vector<battlefield::queue::iterator>, size_t> result =
        select_entries(spec, t, max, client_id);
    std::vector<battlefield::queue::iterator>& found = result.first;

    if (found.empty())
        return player_list();

    player_list players;
    for (auto entry_itr : found)
    {
        // Note: we're looping over a vector of found iterators, not entries!

        queue_entry* entry = *entry_itr;
        for (auto& elem : entry->players_)
            if (ObjectAccessor::FindPlayer(elem, false))
                players.push_back(elem);
        erase(entry_itr);
    }

    return players;
}

battlefield::queue::player_list battlefield::queue::dual_pop(specification spec)
{
    int min;
    switch (spec.get_type())
    {
    // TODO: move this data into the spec?
    case alterac_valley:
        min = 20;
        break;
    case warsong_gulch:
        min = 10;
        break;
    case arathi_basin:
        min = 15;
        break;
    case eye_of_the_storm:
        min = 15;
        break;
        break;
    case random_battleground:
    default:
        return player_list();
    }

    if (sBattleGroundMgr::Instance()->debugging())
        min = 1;

    select_result res_alliance = select_entries(spec, alliance, min, 0);
    if (res_alliance.second != static_cast<std::size_t>(min))
        return player_list();
    select_result res_horde = select_entries(spec, horde, min, 0);
    if (res_horde.second != static_cast<std::size_t>(min))
        return player_list();

    player_list players;

    // Found entries are not the same size, even though they represent the same
    // amount of players, we need two separate loops
    std::vector<battlefield::queue::iterator>& found_alliance =
        res_alliance.first;
    for (auto entry_itr : found_alliance)
    {
        // Note: we're looping over a vector of found iterators, not entries!

        queue_entry* entry = *entry_itr;
        for (auto itr = entry->players_.begin(); itr != entry->players_.end();
             ++itr)
        {
            if (ObjectAccessor::FindPlayer(*itr, false))
            {
                players.push_back(*itr);
                pop_wait_time(entry->spec_,
                    WorldTimer::time_no_syscall() - entry->timestamp());
            }
        }
        erase(entry_itr);
    }

    std::vector<battlefield::queue::iterator>& found_horde = res_horde.first;
    for (auto entry_itr : found_horde)
    {
        // Note: we're looping over a vector of found iterators, not entries!

        queue_entry* entry = *entry_itr;
        for (auto itr = entry->players_.begin(); itr != entry->players_.end();
             ++itr)
        {
            if (ObjectAccessor::FindPlayer(*itr, false))
            {
                players.push_back(*itr);
                pop_wait_time(entry->spec_,
                    WorldTimer::time_no_syscall() - entry->timestamp());
            }
        }
        erase(entry_itr);
    }

    return players;
}

void battlefield::queue::av_pick_entries(
    const std::vector<battlefield::queue::iterator>& entries, int desired,
    int& found_entries, int& found_players)
{
    found_entries = 0;
    found_players = 0;
    for (const auto& entrie : entries)
    {
        queue_entry* entry = *entrie;
        const std::size_t online = entry->online_size();
        if ((online + found_players) > static_cast<std::size_t>(desired))
            break;
        found_players += online;
        ++found_entries;
    }
}

battlefield::queue::player_list battlefield::queue::av_pop(
    const specification& spec, int alliance_max, int horde_max, int client_id)
{
    select_result res_alliance =
        select_entries(spec, alliance, alliance_max, client_id);
    select_result res_horde = select_entries(spec, horde, horde_max, client_id);

    const std::vector<battlefield::queue::iterator>& alliance_entries =
        res_alliance.first;
    size_t alliance_size = res_alliance.second;
    const std::vector<battlefield::queue::iterator>& horde_entries =
        res_horde.first;
    size_t horde_size = res_horde.second;

    // diff =  3 --> alliance needs 3 or 4 players more than horde
    // diff = -3 --> horde needs 3 or 4 players more than alliance
    // diff =  0 --> teams can differ by 1
    const int diff = alliance_max - horde_max;

    // Early abort when no one is found, or when only the dominant team returns
    // results
    if (!alliance_size && !horde_size)
        return player_list();
    if (diff > 0 && !alliance_size)
        return player_list();
    if (diff < 0 && !horde_size)
        return player_list();

    // Try to get the maximum amount of players possible, lower by 1 every time
    // it fails
    // until we can't try anymore

    bool done = false;
    int i = std::max(horde_size, alliance_size); // TODO: we can eliminate some
                                                 // iterations by choosing a
                                                 // better starting i, but it's
                                                 // really insignificant
    int h_entry_count = 0;
    int a_entry_count = 0;
    do
    {
        int h_count = 0;
        int a_count = 0;

        if (diff > 0) // alliance has less players than horde
        {
            av_pick_entries(alliance_entries, i, a_entry_count, a_count);
            if (a_count != i)
                i = a_count;
            if (i - diff + 1 > 0)
                av_pick_entries(
                    horde_entries, i - diff + 1, h_entry_count, h_count);
        }
        else
        {
            av_pick_entries(horde_entries, i, h_entry_count, h_count);
            if (h_count != i)
                i = h_count;
            if (i + diff + 1 > 0)
                av_pick_entries(
                    alliance_entries, i + diff + 1, a_entry_count, a_count);
        }

        const int select_diff = a_count - h_count;
        const int new_diff = diff - select_diff;

        // Teams are equal now
        if (new_diff == 0)
            done = true;
        // Teams are different only by 1 player, that's fine
        else if (abs(new_diff) == 1)
            done = true;
        // Teams are not equal, but the weaker team got more players (alliance)
        else if (diff > 0 && new_diff > 0 && new_diff < diff)
            done = true;
        // Teams are not equal, but the weaker team got more players (horde)
        else if (diff < 0 && new_diff < 0 && new_diff > diff)
            done = true;

        --i; // TODO: We can optimize this to cause less iterations but it's
             // insignificant and the logic is complex
    } while (!done && i > 0);

    player_list players;
    players.reserve(a_entry_count + h_entry_count);
    for (int i = 0; i < a_entry_count; ++i)
    {
        iterator entry_itr = alliance_entries[i];
        const queue_entry* entry = *entry_itr;

        for (const auto& elem : entry->players_)
            if (ObjectAccessor::FindPlayer(elem, false))
                players.push_back(elem);
        erase(entry_itr);
    }
    for (int i = 0; i < h_entry_count; ++i)
    {
        iterator entry_itr = horde_entries[i];
        const queue_entry* entry = *entry_itr;

        for (const auto& elem : entry->players_)
            if (ObjectAccessor::FindPlayer(elem, false))
                players.push_back(elem);
        erase(entry_itr);
    }
    return players;
}

battlefield::queue::select_result battlefield::queue::select_entries(
    specification spec, team t, int max, int client_id,
    const std::vector<iterator>* ignore_entries)
{
    std::vector<battlefield::queue::iterator> found;

    // Note: double loop, double condition. count keeps track of found online
    // players and aborts the loop if max is reached
    std::size_t count = 0;
    for (auto itr = queue_.begin();
         itr != queue_.end() && count < static_cast<std::size_t>(max); ++itr)
    {
        queue_entry* entry = *itr;
        // Skip for incorrect spec, client_id, team or group size
        if (entry->get_specification() != spec || entry->get_team() != t)
            continue;
        if (client_id && entry->client_id() &&
            entry->client_id() != static_cast<uint32>(client_id))
            continue;
        if (ignore_entries &&
            std::find(ignore_entries->begin(), ignore_entries->end(), itr) !=
                ignore_entries->end())
            continue;
        if (!entry->can_pop())
            continue;

        size_t online = entry->online_size();
        if ((count + online) > static_cast<std::size_t>(max))
            continue;

        count += online;
        found.push_back(itr);
    }

    return std::make_pair(found, count);
}

battlefield::arena_rating::distributor* battlefield::queue::pop_rated_arena(
    const specification& spec)
{
    // Remove any entries with all its participants offline
    for (auto itr = queue_.begin(); itr != queue_.end();)
    {
        auto old = itr;
        ++itr;

        queue_entry* entry = *old;

        if (!entry->spec_.arena() || !entry->spec_.rated())
            continue;

        if (entry->online_size() == 0)
            erase(old); // std::list, only old is invalidated
    }

    for (auto itr = queue_.begin(); itr != queue_.end(); ++itr)
    {
        queue_entry* entry = *itr;
        if (!entry->spec_.arena() || !entry->spec_.rated())
            continue;
        if (!entry->can_pop())
            continue;
        // We found an entry. We need to search the entire queue again for
        // a team that matches their rating. We cannot assume that previously
        // tested teams will not match, because rating expansion works in
        // one direction.
        for (auto i_itr = queue_.begin(); i_itr != queue_.end(); ++i_itr)
        {
            queue_entry* inner_entry = *i_itr;
            if (inner_entry == entry)
                continue;
            if (entry->spec_ != inner_entry->spec_)
                continue;
            if (!inner_entry->can_pop())
                continue;

            // Same type & size found. We need to test them for popability
            if (!rated_team_checks(entry, inner_entry))
                continue;

            // Teams are a match.
            ArenaTeam* one =
                sObjectMgr::Instance()->GetArenaTeamById(entry->arena_team_id_);
            ArenaTeam* two = sObjectMgr::Instance()->GetArenaTeamById(
                inner_entry->arena_team_id_);
            if (!one || !two)
                return nullptr;

            // Oppose our teams if they're same-faction and convert to blizzard
            // team values
            Team t_one = entry->get_team() == alliance ? ALLIANCE : HORDE;
            Team t_two = inner_entry->get_team() == alliance ? ALLIANCE : HORDE;
            if (t_one == t_two)
                t_two = t_two == ALLIANCE ? HORDE : ALLIANCE; // Invert team two

            arena_rating::distributor::team_entry team_one(
                entry->players_, one, entry->rating_, t_one);
            arena_rating::distributor::team_entry team_two(
                inner_entry->players_, two, inner_entry->rating_, t_two);
            auto dist = new arena_rating::distributor(team_one, team_two);

            // Update average wait time
            for (size_t i = 0; i < entry->players_.size(); ++i)
                pop_wait_time(
                    spec, WorldTimer::time_no_syscall() - entry->timestamp());
            for (size_t i = 0; i < inner_entry->players_.size(); ++i)
                pop_wait_time(spec,
                    WorldTimer::time_no_syscall() - inner_entry->timestamp());

            // Remove entries from the queue
            erase(i_itr);
            erase(itr);

            return dist;
        }
    }
    return nullptr;
}

bool battlefield::queue::rated_team_checks(
    queue_entry* team_one, queue_entry* team_two)
{
    // We're comparing from team_one's perspective. Their expansion is what
    // matters.
    uint32 max_diff = arena_rating::max_rating_diff(
        WorldTimer::time_no_syscall() - team_one->timestamp());
    // TODO: Limit pairing of teams that have met recently if needed
    uint32 diff = std::max(team_one->rating_, team_two->rating_) -
                  std::min(team_one->rating_, team_two->rating_);
    return diff <= max_diff;
}

std::pair<battlefield::queue::player_list, battlefield::queue::player_list>
battlefield::queue::pop_skirmish(const specification& spec)
{
    // Note about return value: The first in the pair is always invited as
    // Alliance and the second in the pair as Horde

    int min;
    switch (spec.get_type())
    {
    case skirmish_2v2:
        min = 2;
        break;
    case skirmish_3v3:
        min = 3;
        break;
    case skirmish_5v5:
        min = 5;
        break;
    default:
        return std::make_pair(player_list(), player_list());
    }

    if (sBattleGroundMgr::Instance()->debugging())
        min = 1;

    select_result res_alliance = select_entries(spec, alliance, min, 0);
    select_result res_horde = select_entries(spec, horde, min, 0);

    if (res_alliance.second == static_cast<std::size_t>(min) &&
        res_horde.second == static_cast<std::size_t>(min))
    {
        // Both alliance and horde have a valid team
        return get_skirmish_lists(
            spec, res_alliance.first, res_horde.first, min);
    }
    else if (res_alliance.second != static_cast<std::size_t>(min) &&
             res_horde.second != static_cast<std::size_t>(min))
    {
        // Neither alliance or horde has a single valid team
        return std::make_pair(player_list(), player_list());
    }
    else if (res_alliance.second == static_cast<std::size_t>(min))
    {
        // Alliance has a valid team, try to select one more from their pool
        select_result res_alliance_two =
            select_entries(spec, alliance, min, 0, &res_alliance.first);
        if (res_alliance_two.second == static_cast<std::size_t>(min))
        {
            // Found a valid same-faction match
            return get_skirmish_lists(
                spec, res_alliance.first, res_alliance_two.first, min);
        }
    }
    else if (res_horde.second == static_cast<std::size_t>(min))
    {
        // Horde has a valid team, try to select one more from their pool
        select_result res_horde_two =
            select_entries(spec, horde, min, 0, &res_horde.first);
        if (res_horde_two.second == static_cast<std::size_t>(min))
        {
            // Found a valid same-faction match
            return get_skirmish_lists(
                spec, res_horde.first, res_horde_two.first, min);
        }
    }

    // Only found one valid team, cannot pop
    return std::make_pair(player_list(), player_list());
}

std::pair<battlefield::queue::player_list, battlefield::queue::player_list>
battlefield::queue::get_skirmish_lists(const specification& spec,
    const std::vector<iterator>& team_one,
    const std::vector<iterator>& team_two, size_t size)
{
    player_list one, two;
    one.reserve(size);
    two.reserve(size);
    for (auto i_itr : team_one)
    {
        queue_entry* entry = *i_itr;
        one.insert(one.end(), entry->players_.begin(), entry->players_.end());
        for (size_t i = 0; i < entry->players_.size(); ++i)
            pop_wait_time(
                spec, WorldTimer::time_no_syscall() - entry->timestamp());
        erase(i_itr);
    }
    for (auto i_itr : team_two)
    {
        queue_entry* entry = *i_itr;
        two.insert(two.end(), entry->players_.begin(), entry->players_.end());
        for (size_t i = 0; i < entry->players_.size(); ++i)
            pop_wait_time(
                spec, WorldTimer::time_no_syscall() - entry->timestamp());
        erase(i_itr);
    }
    return std::make_pair(one, two);
}

void battlefield::queue::pop_wait_time(const specification& spec, time_t delta)
{
    average_wait_times_[spec].player_deltas_.push_back(delta);
    average_wait_times_[spec].last_pop_ = WorldTimer::time_no_syscall();
}

time_t battlefield::queue::average_wait_time(const specification& spec)
{
    auto itr = average_wait_times_.find(spec);
    if (itr != average_wait_times_.end())
    {
        if (!itr->second.player_deltas_.full())
        {
            // Circular buffer not full
            return 0;
        }
        else if (itr->second.last_pop_ + average_wait_time_undefined <
                 WorldTimer::time_no_syscall())
        {
            // Average wait time no longer valid. Mark as undefined.
            itr->second.player_deltas_.clear();
            return 0;
        }
        else
        {
            time_t t = std::accumulate(itr->second.player_deltas_.begin(),
                itr->second.player_deltas_.end(), static_cast<time_t>(0));
            return t / 10; // Last 10 players in queue
        }
    }
    return 0;
}

// TODO: Use non hand-written code for proper hr/min/sec formatting
static inline std::string time_format_func(time_t diff)
{
    std::stringstream ss;
    if (diff / 3600 > 0)
    {
        ss << diff / 3600 << " hr ";
        diff -= (diff / 3600) * 3600;
    }
    if (diff / 60 > 0)
    {
        ss << diff / 60 << " min ";
        diff -= (diff / 60) * 60;
    }
    if (diff)
        ss << diff << " sec ";
    return ss.str();
}

static std::string bgtype2str(battlefield::type t)
{
    switch (t)
    {
    case battlefield::random_battleground:
        return "Random BG";
    case battlefield::alterac_valley:
        return "AV";
    case battlefield::warsong_gulch:
        return "WSG";
    case battlefield::arathi_basin:
        return "AB";
    case battlefield::eye_of_the_storm:
        return "EotS";
    case battlefield::rated_2v2:
        return "Rated 2v2";
    case battlefield::rated_3v3:
        return "Rated 3v3";
    case battlefield::rated_5v5:
        return "Rated 5v5";
    case battlefield::skirmish_2v2:
        return "Skirmish 2v2";
    case battlefield::skirmish_3v3:
        return "Skirmish 3v3";
    case battlefield::skirmish_5v5:
        return "Skirmish 5v5";
    }
    return "";
}

static std::string bgbracket2str(battlefield::bracket b)
{
    return std::to_string(b.min()) + "-" + std::to_string(b.max());
}

std::string battlefield::queue::debug(bool print_players)
{
    std::stringstream ss;
    ss << "BG Queue:\n";
    int count = 1;
    for (auto& elem : queue_)
    {
        ss << count++ << ". ";
        ss << bgtype2str((elem)->get_specification().get_type()) << " ("
           << bgbracket2str((elem)->get_specification().get_bracket()) << ")";
        ss << " (" << ((elem)->get_team() == alliance ? "A" : "H") << ") ";
        ss << time_format_func(
                  WorldTimer::time_no_syscall() - (elem)->queued_time_)
           << "| "; // time_format_func() includes space after string
        if (print_players)
        {
            int online = 0;
            for (size_t i = 0; i < (elem)->size(); ++i)
            {
                if (Player* p = ObjectAccessor::FindPlayer((elem)->players_[i]))
                {
                    ss << p->GetName() << " ";
                    ++online;
                }
            }
            if (static_cast<std::size_t>(online) < (elem)->size())
                ss << "(Offline: " << (elem)->size() - online << ")";
        }
        else
        {
            ss << "size: " << (elem)->size();
        }
        ss << "\n";
    }
    return ss.str();
}

std::string battlefield::queue::summary_debug()
{
    std::stringstream ss;
    ss << "BG Queue Summary:\n";

    struct summary_t
    {
        summary_t(type t, bracket b) : bg_type(t), bg_bracket(std::move(b)) {}
        type bg_type;
        bracket bg_bracket;
        int ally_cnt = 0;
        int horde_cnt = 0;
    };

    std::vector<summary_t> summary;
    int offline_size = 0;
    for (auto e : queue_)
    {
        auto size = e->online_size();
        offline_size += e->size() - size;
        if (size == 0)
            continue;

        // Create or find current entry for type, bracket pair
        auto itr =
            std::find_if(summary.begin(), summary.end(), [e](const summary_t& o)
                {
                    return o.bg_type == e->get_specification().get_type() &&
                           o.bg_bracket == e->get_specification().get_bracket();
                });
        if (itr == summary.end())
        {
            summary.emplace_back(e->get_specification().get_type(),
                e->get_specification().get_bracket());
            itr = summary.begin() + (summary.size() - 1);
        }

        if (e->get_team() == battlefield::alliance)
            itr->ally_cnt += size;
        else
            itr->horde_cnt += size;
    }

    // Sort by total count in ascending order
    std::sort(summary.begin(), summary.end(),
        [](const summary_t& a, const summary_t& b)
        {
            return a.ally_cnt + a.horde_cnt < b.ally_cnt + b.horde_cnt;
        });

    for (auto e : summary)
    {
        ss << bgtype2str(e.bg_type) << " (" << bgbracket2str(e.bg_bracket)
           << ") | "
           << "A: " << e.ally_cnt << " H: " << e.horde_cnt << "\n";
    }

    if (offline_size > 0)
        ss << "People offline in queue (not counted in summary): "
           << offline_size << "\n";

    return ss.str();
}
