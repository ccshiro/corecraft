#include "battlefield_arena_rating.h"
#include "ObjectMgr.h"

std::pair<int32, int32> battlefield::arena_rating::distributor::match_over(
    uint32 winning_team_id)
{
    LOG_DEBUG(logging,
        "Rated Match: Winning team id: %u. Team id 1: %u. Team id 2: %u.",
        winning_team_id, team_one_.arena_team_id, team_two_.arena_team_id);

    std::pair<int32, int32> change;

    if (team_one_.arena_team_id == winning_team_id)
    {
        change = team_rating(team_one_, team_two_);
    }
    else if (team_two_.arena_team_id == winning_team_id)
    {
        change = team_rating(team_two_, team_one_);
    }
    else if (winning_team_id == 0)
    {
        // The game was a draw. Both teams lose 16 points.
        draw(team_one_, team_two_);
        draw(team_two_, team_one_);
    }
    else
    {
        assert(false &&
               "Calling match_over() with winning team > 0 yet not part of "
               "battlefield::arena_rating::distributor instantiation");
    }

    sObjectMgr::Instance()->update_arena_rankings();

    if (ArenaTeam* team =
            sObjectMgr::Instance()->GetArenaTeamById(team_one_.arena_team_id))
    {
        team->SaveToDB();
        team->NotifyStatsChanged();
    }

    if (ArenaTeam* team =
            sObjectMgr::Instance()->GetArenaTeamById(team_two_.arena_team_id))
    {
        team->SaveToDB();
        team->NotifyStatsChanged();
    }

    return change;
}

std::pair<int32, int32> battlefield::arena_rating::distributor::team_rating(
    const team_entry& winners, const team_entry& losers)
{
    // The team rating is based on the ELO rating system using a K-factor of 32
    float winners_chance =
        1 / (1 + pow(10, (losers.team_rating - winners.team_rating) / 400.0f));
    uint32 score_diff = floor((32.0f * (1.0f - winners_chance)) +
                              0.5f); // Round number at this point so resulting
                                     // +/- do not vary for the teams

    // Calculate resulting ratings

    uint32 winners_rating = winners.team_rating + score_diff;
    uint32 losers_rating =
        static_cast<uint32>(losers.team_rating) >= score_diff ?
            static_cast<uint32>(losers.team_rating) - score_diff :
            0;
    LOG_DEBUG(logging,
        "Rated Match: Rating won/lost: %u. Winners new rating: %u. Losers new "
        "rating: %u.",
        score_diff, winners_rating, losers_rating);

    // Update the ratings for the team, as well as the personal ratings of each
    // participating player

    if (ArenaTeam* team =
            sObjectMgr::Instance()->GetArenaTeamById(winners.arena_team_id))
    {
        team->RegisterFinishedMatch(
            losers.arena_team_id, losers.players, winners_rating, true);
        // Update personal ratings
        for (const auto& elem : winners.players)
        {
            if (ArenaTeamMember* member = team->GetMember(elem))
                player_rating(team, member, losers, true);
        }
    }

    if (ArenaTeam* team =
            sObjectMgr::Instance()->GetArenaTeamById(losers.arena_team_id))
    {
        team->RegisterFinishedMatch(
            winners.arena_team_id, winners.players, losers_rating, false);
        // Update personal ratings
        for (const auto& elem : losers.players)
        {
            if (ArenaTeamMember* member = team->GetMember(elem))
                player_rating(team, member, winners, false);
        }
    }

    return std::make_pair(
        static_cast<int32>(winners_rating) - winners.team_rating,
        static_cast<int32>(losers_rating) - losers.team_rating);
}

void battlefield::arena_rating::distributor::player_rating(ArenaTeam* team,
    ArenaTeamMember* member, const team_entry& opponents, bool won)
{
    // "If the opposing team was queued based on their average personal rating,
    // your personal rating will be adjusted based on their average personal
    // rating."
    // - http://www.wowwiki.com/Arena_team_rating
    // Our adjusted rating is average personal rating if they queued using that,
    // and team rating if that was their queued as rating

    float chance =
        1 / (1 + pow(10, (opponents.adjusted_team_rating -
                             static_cast<int32>(member->personal_rating)) /
                             400.0f));
    int32 score_diff;
    if (won)
        score_diff = floor((32.0f * (1.0f - chance)) + 0.5f);
    else
        score_diff = floor((32.0f * (0.0f - chance)) + 0.5f);

    uint32 new_personal_rating;
    if (score_diff >= 0)
        new_personal_rating = member->personal_rating + score_diff;
    else
        new_personal_rating =
            member->personal_rating >
                    static_cast<uint32>(std::abs(score_diff)) ?
                member->personal_rating + static_cast<uint32>(score_diff) :
                0;

    team->PersonalRatingUpdate(member->guid, new_personal_rating, won);
}

void battlefield::arena_rating::distributor::draw(
    const team_entry& entry, const team_entry& opponents)
{
    uint32 new_rating = entry.team_rating > 16 ? entry.team_rating - 16 : 0;

    if (ArenaTeam* team =
            sObjectMgr::Instance()->GetArenaTeamById(entry.arena_team_id))
    {
        team->RegisterFinishedMatch(
            opponents.arena_team_id, entry.players, new_rating, false);
        // Update personal ratings
        for (const auto& elem : entry.players)
        {
            if (ArenaTeamMember* member = team->GetMember(elem))
            {
                uint32 new_personal_rating = member->personal_rating > 16 ?
                                                 member->personal_rating - 16 :
                                                 0;
                team->PersonalRatingUpdate(elem, new_personal_rating, false);
            }
        }
    }
}
