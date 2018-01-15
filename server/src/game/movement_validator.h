#ifndef GAME__MOVEMENT_VALIDATOR_H
#define GAME__MOVEMENT_VALIDATOR_H

#include "Common.h"
#include <string>

struct MovementInfo;
class Player;

// A anti-cheat class to catch people using teleportation hacks,
// speed hacks, fly hacks, and so on. Exists one per player.
class movement_validator
{
public:
    movement_validator(Player* player);

    void new_movement_packet();
    bool validate_movement(MovementInfo& info, uint32 opcode);

    // Used when we've just teleported, charged, etc
    void ignore_next_packet() { skip_next_packet_ = true; }

    void knock_back(float vertical_speed);

private:
    void log_offense();
    float current_speed(MovementInfo& info);

    bool verify_movement_flags(
        MovementInfo& info, uint32 opcode, uint32 time_ms);

    // teleport_cheat_check checks each packet and allows much leeway
    bool teleport_cheat_check(uint32 opcode, float speed, uint32 time_ms);
    // speedhack_cheat_check checks over long periods of time and catches
    // smaller speed differences.
    bool speedhack_cheat_check(float speed, uint32 time_ms);

    void reset_speedhack_check();

    Player* player_;
    std::string offense_;
    uint32 last_update_;
    float prev_xyz_[3];
    float speed_;
    bool skip_next_packet_;
    uint32 was_falling_;
    uint32 falling_start_;
    uint32 last_jump_;
    float falling_z_delta_;
    float knockback_vertical_speed_;
    bool knockback_;
    bool fallen_since_knock_; // We can be receiving a movement packet before
                              // the client knows it's knock-backed
    uint32
        moved_total_time_; // milliseconds we've been moving consecutively for
    float moved_total_dist_;
    float moved_average_speed_;
    bool falling_levitate_; // If we were levitating during the fall
};

#endif
