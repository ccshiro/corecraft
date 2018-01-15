#include "movement_validator.h"
#include "Player.h"
#include "Timer.h"

static auto& logger = logging.get_logger("anticheat.movement");

movement_validator::movement_validator(Player* player)
  : player_(player), last_update_(WorldTimer::getMSTime()), speed_(0.0f),
    skip_next_packet_(false), was_falling_(false), falling_start_(0),
    last_jump_(0), falling_z_delta_(0.0f), knockback_vertical_speed_(0.0f),
    knockback_(false), fallen_since_knock_(false), moved_total_time_(0),
    moved_total_dist_(0.0f), moved_average_speed_(0.0f),
    falling_levitate_(false)
{
    player->GetPosition(prev_xyz_[0], prev_xyz_[1], prev_xyz_[2]);
}

void movement_validator::new_movement_packet()
{
    speed_ = current_speed(player_->m_movementInfo);
}

bool movement_validator::validate_movement(MovementInfo& info, uint32 opcode)
{
    // This timestamp is validated and converted to server time before we get
    // it, so it's not client input at this point. (See
    // WorldSession::translate_timestamp_movement())
    // XXX: THIS IS NOT TRUE ANYMORE! FIX PLEASE!
    uint32 ms = info.time;

    // If we're not moving ourself we just nullify all pending checks and keep
    // returning true until we once again gain control of our character.
    if (!player_->InControl())
    {
        falling_start_ = 0;
        falling_z_delta_ = 0.0f;
        knockback_ = false;
        reset_speedhack_check();
        player_->GetPosition(prev_xyz_[0], prev_xyz_[1], prev_xyz_[2]);
        return true;
    }

    if (ms - last_update_ > 1000)
    {
        // We need to reset falling too if update hasn't come for a
        // bit, this could be if we were feared while jumping
        falling_start_ = 0;
        falling_levitate_ = false;
        falling_z_delta_ = 0.0f;
        reset_speedhack_check();
        last_update_ = ms;
    }

    if (!verify_movement_flags(info, opcode, ms))
    {
        log_offense();
        return false;
    }

    // Use the highest speed, essentially:
    // if we just stopped moving, the speed of the last update
    // if we just started moving, the speed of this update
    float speed = current_speed(info);
    speed = std::max(speed, speed_);

    // Keep discarding packets while we're on a transport or charging
    if (player_->GetTransport() ||
        player_->movement_gens.top_id() == movement::gen::charge)
        skip_next_packet_ = true;

    bool skip_detection = skip_next_packet_ ||
                          info.HasMovementFlag(MOVEFLAG_GRAVITY) ||
                          was_falling_;

    if (!skip_detection)
    {
        if (!teleport_cheat_check(opcode, speed, ms))
        {
            log_offense();
            return false;
        }

        if (!speedhack_cheat_check(speed, ms))
        {
            log_offense();
            return false;
        }
    }

    if (skip_detection || speed == 0.0f)
    {
        reset_speedhack_check();
    }

    // We skip the next packet after a fall
    if (info.HasMovementFlag(MOVEFLAG_FALLING_UNK1) ||
        info.HasMovementFlag(MOVEFLAG_GRAVITY))
        was_falling_ = true;
    else
        was_falling_ = false;

    skip_next_packet_ = false;
    last_update_ = ms;
    player_->GetPosition(prev_xyz_[0], prev_xyz_[1], prev_xyz_[2]);

    return true;
}

void movement_validator::reset_speedhack_check()
{
    // Stopped moving, invalidate long-running speed checks
    moved_total_time_ = 0;
    moved_total_dist_ = 0.0f;
    moved_average_speed_ = 0.0f;
}

void movement_validator::knock_back(float vertical_speed)
{
    knockback_vertical_speed_ = vertical_speed;
    knockback_ = true;
    fallen_since_knock_ = false;
    // Reset all falling flags. This is important if we get knockbacked in a
    // knockback
    falling_start_ = 0;
    falling_levitate_ = false;
    falling_z_delta_ = 0.0f;
    reset_speedhack_check();
}

void movement_validator::log_offense()
{
    WorldSession* session = player_->GetSession();
    if (!session)
        return;

    // TODO: Use "session->GetPlayerInfo().c_str()" (function not merged yet)
    logger.info("MovementValidator: Kicked player %s with reasoning: \"%s\".",
        player_->GetName(), offense_.c_str());
}

float movement_validator::current_speed(MovementInfo& info)
{
    if (!info.HasMovementFlag(movementFlagsMask) &&
        !info.HasMovementFlag(
            MOVEFLAG_GRAVITY)) // Count short-falling as moving
        return 0.0f;

    float speed;
    if (info.HasMovementFlag(MOVEFLAG_FLYING2))
        speed = player_->GetSpeed(MOVE_FLIGHT);
    else if (info.HasMovementFlag(MOVEFLAG_SWIMMING))
        speed = player_->GetSpeed(MOVE_SWIM);
    else
        speed = player_->GetSpeed(
            info.HasMovementFlag(MOVEFLAG_WALK_MODE) ? MOVE_WALK : MOVE_RUN);
    return speed;
}

bool movement_validator::verify_movement_flags(
    MovementInfo& info, uint32 opcode, uint32 time_ms)
{
    // NOTE: CAN_FLY is not actually if we have flying enabled, that's
    // MOVEFLAG_FLYING (why??) FLYING2 is if we're ACTUALLY flying
    if (info.HasMovementFlag(MOVEFLAG_FLYING2) ||
        info.HasMovementFlag(MOVEFLAG_FLYING))
    {
        if (!player_->HasAuraType(SPELL_AURA_FLY) &&
            !player_->HasAuraType(SPELL_AURA_MOD_FLIGHT_SPEED_MOUNTED) &&
            !player_->gm_flying())
        {
            std::stringstream ss;
            ss << "Has flags: { "
               << (info.HasMovementFlag(MOVEFLAG_FLYING2) ? "flying " : "")
               << (info.HasMovementFlag(MOVEFLAG_FLYING) ? "can fly " : "")
               << "} but does not "
               << "have a flight aura.";
            offense_ = ss.str();
            return false;
        }
    }

    if (opcode == MSG_MOVE_JUMP)
        last_jump_ = time_ms;

    // Falling verification
    if (info.HasMovementFlag(MOVEFLAG_GRAVITY))
    {
        // Side-Note: If we're free-falling we don't get the package right away
        // Which means falling_start_ should've been a lower timestamp
        if (!falling_start_)
            falling_start_ = time_ms;
        fallen_since_knock_ = true;

        // Reset time counter if we were levitating and no longer are
        if (falling_levitate_ && !player_->HasAuraType(SPELL_AURA_FEATHER_FALL))
        {
            falling_levitate_ = false;
            falling_start_ = time_ms;
        }

        uint32 delta = time_ms - falling_start_;

        if (opcode == MSG_MOVE_JUMP && delta > 0)
        {
            offense_ = "Sending jump opcode while falling";
            return false;
        }

        // Was falling initiated with a jump?
        bool jump_fall = last_jump_ >= falling_start_;

        falling_z_delta_ += prev_xyz_[2] - player_->GetZ();

        // Calculate expected falling distance
        static const float g =
            8.5f; // Very imprecise, we just want to check they actually fall
        float fallen_seconds = static_cast<float>(delta) / 1000.0f;
        float expected_dist;

        // Slow fall and levitate give constant falling speed
        if (player_->HasAuraType(SPELL_AURA_FEATHER_FALL))
        {
            expected_dist = 3.0f * fallen_seconds;
            falling_levitate_ = true;
        }
        else
            expected_dist = 0.5f * g * fallen_seconds * fallen_seconds;

        if (jump_fall) // Decrease expected delta if fall was initiated with a
                       // jump
            expected_dist -= 6.5f; // g/2 + 2
        if (knockback_)
        {
            // Callibrate expected distance based on knockback velocity
            float f = knockback_vertical_speed_ /
                      (g + 1.0f); // Bring up accuracy a tad
            expected_dist -= 0.5f * g * f * f;
        }

        LOG_DEBUG(logging,
            "[Falling Anti-Cheat]: Been falling for: %f sec. Initated with a "
            "jump: %s. Initiated with a knockback: %s "
            "Fallen total yards: %f. Least expected distance: %f yards.",
            fallen_seconds, jump_fall ? "YES" : "NO", knockback_ ? "YES" : "NO",
            falling_z_delta_, expected_dist);

        // Only take action if we've been falling for at least a second
        if (delta > 1000)
        {
            if (falling_z_delta_ < expected_dist)
            {
                std::stringstream ss;
                ss << "Falling flag on, yet not falling. (Data: been falling "
                      "for: " << fallen_seconds
                   << " seconds. Has fallen total of: " << falling_z_delta_
                   << " yards. "
                   << "Expected a falling of at least: " << expected_dist
                   << " yards. Fall " << (jump_fall ? "WAS " : "WAS NOT ")
                   << "initiated with a jump. Fall "
                   << (knockback_ ? "WAS " : "WAS NOT ")
                   << "initiated with a knocback";
                offense_ = ss.str();
                return false;
            }
        }
    }
    else
    {
        falling_start_ = 0;
        falling_levitate_ = false;
        falling_z_delta_ = 0.0f;
        if (fallen_since_knock_)
            knockback_ = false;
    }

    return true;
}

bool movement_validator::teleport_cheat_check(
    uint32 opcode, float speed, uint32 time_ms)
{
    uint32 delta_ms = time_ms - last_update_;
    float max_distance = speed * (delta_ms / 1000.0f);
    float moved_distance =
        player_->GetDistance(prev_xyz_[0], prev_xyz_[1], prev_xyz_[2]);

    // Ignore really short falls. The client sometimes sends them without any
    // start movement
    if (speed == 0.0f && opcode == MSG_MOVE_FALL_LAND && moved_distance < 4.0f)
        return true;

    // We make max distance much higher than expected,
    // as per packet detection is very inaccurate.
    // This doesn't work for moderate speed hacking, of course,
    // but our concern lies in trying to detect teleportation
    max_distance *= 2.0f;

    // FIXME: This produces some really dumb values at some times,
    // where micro movements are more leniant than really long movements,
    // but without it we get random DCs because the client can send really
    // inaccurate micro movements. TODO: Write test cases to see if this
    // is exploitable, and if so adjust the code accordingly.
    // If delta ms is low we need to increase the distance based on
    // speed as well, for small stuttering type of movement patterns.
    if (delta_ms < 25)
        max_distance += 2.0f * speed;
    else if (delta_ms < 100)
        max_distance += speed;
    else if (delta_ms < 200)
        max_distance += speed / 4.0f;

    LOG_DEBUG(logging,
        "teleport_cheat_check: moved_distance: %f max_distance: %f speed: %f "
        "delta: %u ms",
        moved_distance, max_distance, speed, delta_ms);

    if (moved_distance > max_distance + 0.1f)
    {
        std::stringstream ss;
        ss << "Per-packet detection. Either an attempted teleporation or a "
              "very highly turned up speed hack."
           << "(Data: Max accepted distance at time of movement: "
           << max_distance << " actually moved distance: " << moved_distance
           << " speed: " << speed << " time since last packet: " << delta_ms
           << " milliseconds)";
        offense_ = ss.str();
        return false;
    }

    return true;
}

bool movement_validator::speedhack_cheat_check(float speed, uint32 time_ms)
{
    moved_total_dist_ +=
        player_->GetDistance(prev_xyz_[0], prev_xyz_[1], prev_xyz_[2]);
    uint32 delta = time_ms - last_update_;
    moved_total_time_ += delta;

    float pct_of_new = 1.0f, pct_of_old = 0.0f;
    if (moved_total_time_ > 0)
    {
        pct_of_new =
            static_cast<float>(delta) / static_cast<float>(moved_total_time_);
        pct_of_old = 1.0f - pct_of_new;
    }

    moved_average_speed_ =
        moved_average_speed_ * pct_of_old + speed * pct_of_new;

    // Skip validation if we haven't been moving for more than a second
    if (moved_total_time_ < 1000)
        return true;

    float max_distance = moved_average_speed_ * (moved_total_time_ / 1000.0f);

    // Decrease sensitivity
    max_distance *= 1.1f;

    // Add leniancy for short updates
    float leniency =
        2.0f * ((5000 - static_cast<int32>(moved_total_time_)) / 1000.0f);
    if (leniency < 0)
        leniency = 0.0f;

    LOG_DEBUG(logging,
        "speedhack_cheat_check: moved total time: %f secs moved total dist: %f "
        "average speed: %f max dist: %f leniency: %f",
        moved_total_time_ / 1000.0f, moved_total_dist_, moved_average_speed_,
        max_distance, leniency);

    if (moved_total_dist_ > max_distance + leniency)
    {
        std::stringstream ss;
        ss << "Speed-hack detection. The client was going further than deemed "
              "possible with his speed."
           << "(Data: total dist: " << moved_total_dist_
           << " expected dist: " << max_distance + leniency
           << " speed: " << moved_average_speed_
           << " had been moving for: " << (moved_total_time_ / 1000.0f)
           << " seconds)";
        offense_ = ss.str();
        return false;
    }

    return true;
}
