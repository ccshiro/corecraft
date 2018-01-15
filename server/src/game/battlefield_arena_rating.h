#ifndef GAME__BATTLEFIELD_ARENA_RATING
#define GAME__BATTLEFIELD_ARENA_RATING

#include "ArenaTeam.h"
#include "ObjectGuid.h"
#include "SharedDefines.h"
#include <utility>
#include <vector>

// Rating distribution is based on: http://www.wowwiki.com/Arena_team_rating

namespace battlefield
{
namespace arena_rating
{
class distributor
{
public:
    struct team_entry
    {
        team_entry(std::vector<ObjectGuid> p, ArenaTeam* team,
            int32 adjusted_rating, Team t)
          : players(std::move(p)), arena_team_id(team->GetId()),
            team_rating(team->GetRating()),
            adjusted_team_rating(adjusted_rating), team(t)
        {
        }
        std::vector<ObjectGuid> players;
        uint32 arena_team_id;
        int32 team_rating; // ratings are signed for calculational purposes
        int32 adjusted_team_rating;
        Team team;
    };

    distributor(team_entry team_one, team_entry team_two)
      : team_one_(std::move(team_one)), team_two_(std::move(team_two))
    {
    }

    // Return Rating change of winning team (first) and losing team (second)
    std::pair<int32, int32> match_over(uint32 winning_team_id);

    team_entry* team_one() { return &team_one_; }
    team_entry* team_two() { return &team_two_; }

private:
    team_entry team_one_;
    team_entry team_two_;
    std::pair<int32, int32> team_rating(
        const team_entry& winners, const team_entry& losers);
    void player_rating(ArenaTeam* team, ArenaTeamMember* member,
        const team_entry& opponents, bool won);

    void draw(const team_entry& entry, const team_entry& opponents);
};

inline uint32 adjusted_team_rating(
    const std::vector<ObjectGuid>& players, ArenaTeam* team)
{
    if (players.empty())
        return 0;
    uint32 accumulated_personal = 0;
    for (const auto& player : players)
    {
        if (ArenaTeamMember* member = team->GetMember(player))
            accumulated_personal += member->personal_rating;
        else
            return 0;
    }
    accumulated_personal /= players.size();
    // Patch 2.4.2: "If the average personal rating of the players queuing for a
    // game is more than 150 points below
    // the team's rating, the team will be queued against an opponent matching
    // or similar to the average personal rating."
    return accumulated_personal + 150 < team->GetRating() ?
               accumulated_personal :
               team->GetRating();
}

inline uint32 max_rating_diff(time_t time_in_queue)
{
    // FIXME: Made up formula
    // Background: The spreading must be able to reach up to 800
    // in difference or having a rating +/- cap of 32 makes no sense.
    // Formula: Starts at 50, expands by 50 each 2.5 minutes.
    // After 40 minutes it has expanded to 850 and stops expanding
    uint32 diff = 50;
    uint32 time_increase = 50.0f * (time_in_queue / 150.0f);
    if (time_increase > 800)
        time_increase = 800;
    diff += time_increase;
    return diff;
}
}
}

#endif
