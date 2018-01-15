/*
 * Copyright (C) 2005-2012 MaNGOS <http://getmangos.com/>
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

#include "BattleGround.h"
#include "BattleGroundMgr.h"
#include "Common.h"
#include "Corpse.h"
#include "logging.h"
#include "MapManager.h"
#include "MapPersistentStateMgr.h"
#include "ObjectMgr.h"
#include "Opcodes.h"
#include "Player.h"
#include "Spell.h"
#include "Transport.h"
#include "movement/WaypointMovementGenerator.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "maps/visitors.h"
#include <utility>

void WorldSession::HandleMoveWorldportAckOpcode(WorldPacket& /*recv_data*/)
{
    HandleMoveWorldportAckOpcode();
}

void WorldSession::HandleMoveWorldportAckOpcode()
{
    // ignore unexpected far teleports
    if (!GetPlayer()->IsBeingTeleportedFar())
        return;

    // Set in control after teleport
    GetPlayer()->SetClientControl(GetPlayer(), 1);

    // get start teleport coordinates (will used later in fail case)
    WorldLocation old_loc;
    GetPlayer()->GetPosition(old_loc);

    // get the teleport destination
    WorldLocation& loc = GetPlayer()->GetTeleportDest();

    // possible errors in the coordinate validity check (only cheating case
    // possible)
    if (!maps::verify_coords(loc.coord_x, loc.coord_y))
    {
        logging.error(
            "WorldSession::HandleMoveWorldportAckOpcode: %s was teleported far "
            "to a not valid location "
            "(map:%u, x:%f, y:%f, z:%f) We port him to his homebind instead..",
            GetPlayer()->GetGuidStr().c_str(), loc.mapid, loc.coord_x,
            loc.coord_y, loc.coord_z);
        // stop teleportation else we would try this again and again in
        // LogoutPlayer...
        GetPlayer()->SetSemaphoreTeleportFar(false);
        // and teleport the player to a valid place
        GetPlayer()->TeleportToHomebind();
        return;
    }

    // get the destination map entry, not the current one, this will fix
    // homebind and reset greeting
    MapEntry const* mEntry = sMapStore.LookupEntry(loc.mapid);

    Map* map = nullptr;

    // prevent crash at attempt landing to not existed battleground instance
    if (mEntry->IsBattleGroundOrArena())
    {
        if (GetPlayer()->GetBattleGroundId())
            map = sMapMgr::Instance()->FindMap(
                loc.mapid, GetPlayer()->GetBattleGroundId());

        if (!map)
        {
            LOG_DEBUG(logging,
                "WorldSession::HandleMoveWorldportAckOpcode: %s was teleported "
                "far to nonexisten battleground instance "
                " (map:%u, x:%f, y:%f, z:%f) Trying to port him to his "
                "previous place..",
                GetPlayer()->GetGuidStr().c_str(), loc.mapid, loc.coord_x,
                loc.coord_y, loc.coord_z);

            GetPlayer()->SetSemaphoreTeleportFar(false);

            // Teleport to previous place, if cannot be ported back TP to
            // homebind place
            if (!GetPlayer()->TeleportTo(old_loc))
            {
                LOG_DEBUG(logging,
                    "WorldSession::HandleMoveWorldportAckOpcode: %s cannot be "
                    "ported to his previous place, teleporting him to his "
                    "homebind place...",
                    GetPlayer()->GetGuidStr().c_str());
                GetPlayer()->TeleportToHomebind();
            }
            return;
        }
    }

    InstanceTemplate const* mInstance =
        ObjectMgr::GetInstanceTemplate(loc.mapid);

    // reset instance validity, except if going to an instance inside an
    // instance
    if (GetPlayer()->m_InstanceValid == false && !mInstance)
        GetPlayer()->m_InstanceValid = true;

    GetPlayer()->SetSemaphoreTeleportFar(false);

    // relocate the player to the teleport destination
    if (!map)
        map = sMapMgr::Instance()->CreateMap(loc.mapid, GetPlayer());

    GetPlayer()->SetMap(map);
    GetPlayer()->Relocate(loc.coord_x, loc.coord_y, loc.coord_z);
    GetPlayer()->SetOrientation(loc.orientation);

    GetPlayer()->SendInitialPacketsBeforeAddToMap();
    // the CanEnter checks are done in TeleporTo but conditions may change
    // while the player is in transit, for example the map may get full
    if (!GetPlayer()->GetMap()->insert(GetPlayer()))
    {
        // if player wasn't added to map, reset his map pointer!
        GetPlayer()->ResetMap();

        LOG_DEBUG(logging,
            "WorldSession::HandleMoveWorldportAckOpcode: %s was teleported far "
            "but couldn't be added to map "
            " (map:%u, x:%f, y:%f, z:%f) Trying to port him to his previous "
            "place..",
            GetPlayer()->GetGuidStr().c_str(), loc.mapid, loc.coord_x,
            loc.coord_y, loc.coord_z);

        // Teleport to previous place, if cannot be ported back TP to homebind
        // place
        if (!GetPlayer()->TeleportTo(old_loc))
        {
            LOG_DEBUG(logging,
                "WorldSession::HandleMoveWorldportAckOpcode: %s cannot be "
                "ported to his previous place, teleporting him to his homebind "
                "place...",
                GetPlayer()->GetGuidStr().c_str());
            GetPlayer()->TeleportToHomebind();
        }
        return;
    }

    // battleground state prepare (in case join to BG), at relogin/tele player
    // not invited
    // only add to bg group and object, if the player was invited (else he
    // entered through command)
    if (_player->InBattleGround())
    {
        // cleanup setting if outdated
        if (!mEntry->IsBattleGroundOrArena())
        {
            // We're not in BG
            _player->SetBattleGroundId(0, BATTLEGROUND_TYPE_NONE);
            // reset destination bg team
            _player->SetBGTeam(TEAM_NONE);
        }
        // join to bg case
        else if (BattleGround* bg = _player->GetBattleGround())
        {
            bg->AddPlayer(_player);
        }
    }

    auto myself = _player;
    myself->queue_action(0, [myself]()
        {
            myself->SendInitialPacketsAfterAddToMap();

            // Continue flight if we were flying
            if (myself->movement_gens.top_id() == movement::gen::flight)
            {
                if (!myself->InBattleGround())
                {
                    myself->movement_gens.top()->start();
                    return;
                }
                else
                {
                    // Don't continue for battlegrounds
                    myself->movement_gens.remove_all(movement::gen::flight);
                    myself->m_taxi.ClearTaxiDestinations();
                }
            }
        });

    // Don't do stuff below if we're continuing a taxi flight
    if (_player->movement_gens.top_id() == movement::gen::flight &&
        !_player->InBattleGround())
        return;

    if (mEntry->IsRaid() && mInstance)
    {
        if (time_t timeReset = sMapPersistentStateMgr::Instance()
                                   ->GetScheduler()
                                   .GetResetTimeFor(mEntry->MapID))
        {
            uint32 timeleft = uint32(timeReset - WorldTimer::time_no_syscall());
            GetPlayer()->SendInstanceResetWarning(mEntry->MapID, timeleft);
        }
    }

    // mount allow check (remove after add to map for correct speed @ client)
    if (!mEntry->IsMountAllowed())
    {
        auto plr = _player;
        plr->queue_action(0, [plr]()
            {
                plr->remove_auras(SPELL_AURA_MOUNTED);
            });
    }

    // honorless target
    if (GetPlayer()->pvpInfo.inHostileArea)
        GetPlayer()->CastSpell(GetPlayer(), 2479, true);

    // Clear pet guid (it's possible the pet was queued to be spawned when the
    // teleport happend, making us have a pet guid, but no pet).
    _player->SetPetGuid(ObjectGuid());

    // resummon pet (except for when the target map is an arena)
    if (!map->IsBattleArena())
        _player->ResummonPetTemporaryUnSummonedIfAny();

    // lets process all delayed operations on successful teleport
    GetPlayer()->ProcessDelayedOperations();

    // Send battlefield status packets
    sBattleGroundMgr::Instance()->send_status_packets(_player, true);

    // Clear our client visibility list as it's no longer valid after we've
    // zoned.
    GetPlayer()->m_clientGUIDs.clear();

    // Resend water walk after teleport if player is dead
    if (!GetPlayer()->isAlive())
        GetPlayer()->SetMovement(MOVE_WATER_WALK);

    _player->ClearTeleportOptions();
}

void WorldSession::HandleMoveTeleportAckOpcode(WorldPacket& recv_data)
{
    LOG_DEBUG(logging, "MSG_MOVE_TELEPORT_ACK");

    ObjectGuid guid;

    recv_data >> guid;

    uint32 counter, time;
    recv_data >> counter >> time;
    LOG_DEBUG(logging, "Guid: %s", guid.GetString().c_str());
    LOG_DEBUG(logging, "Counter %u, time %u", counter, time / IN_MILLISECONDS);

    Unit* moving = _player->GetMovingUnit();
    Player* pl_moving =
        moving->GetTypeId() == TYPEID_PLAYER ? (Player*)moving : nullptr;

    if (!pl_moving || !pl_moving->IsBeingTeleportedNear())
        return;

    if (guid != pl_moving->GetObjectGuid())
        return;

    pl_moving->SetSemaphoreTeleportNear(false);

    WorldLocation const& dest = pl_moving->GetTeleportDest();

    pl_moving->SetPosition(
        dest.coord_x, dest.coord_y, dest.coord_z, dest.orientation, true);

    // tell everyone else (TODO: this might be unnecessary to forward)
    WorldPacket data(MSG_MOVE_TELEPORT_ACK, 16);
    data << guid << counter << time;
    pl_moving->SendMessageToSet(&data, false);

    uint32 old_zone, new_zone;
    old_zone = pl_moving->GetZoneId();
    pl_moving->UpdateZoneAreaCache();
    new_zone = pl_moving->GetZoneId();

    // new zone?
    if (old_zone != new_zone)
    {
        // honorless target
        if (pl_moving->pvpInfo.inHostileArea)
            pl_moving->CastSpell(pl_moving, 2479, true);
    }

    // resummon pet
    GetPlayer()->ResummonPetTemporaryUnSummonedIfAny();

    // lets process all delayed operations on successful teleport
    GetPlayer()->ProcessDelayedOperations();
}

void WorldSession::HandleMovementOpcodes(WorldPacket& recv_data)
{
    uint32 opcode = recv_data.opcode();

    Unit* moving = _player->GetMovingUnit();

    assert(moving);

    Player* pl_moving =
        moving->GetTypeId() == TYPEID_PLAYER ? (Player*)moving : nullptr;

    if (opcode == MSG_MOVE_FALL_LAND)
        _player->SetKnockbacked(false);

    // ignore, waiting processing in WorldSession::HandleMoveWorldportAckOpcode
    // and WorldSession::HandleMoveTeleportAck
    if (pl_moving && pl_moving->IsBeingTeleported())
    {
        recv_data.rpos(recv_data.wpos()); // prevent warnings spam
        return;
    }

    // Skip movement packets from client that lost control (except falling and
    // landing)
    if (!_player->InControl() && opcode != MSG_MOVE_FALL_LAND &&
        opcode != MSG_MOVE_HEARTBEAT)
    {
        recv_data.rpos(recv_data.wpos()); // prevent warnings spam
        return;
    }

    // Skip the packet if the client-side mover does not correlate to the
    // server-side one
    // (i.e. the client has not yet realized the mover has changed)
    if (moving->GetObjectGuid() != _player->GetClientMovingUnit())
    {
        recv_data.rpos(recv_data.wpos()); // prevent warnings spam
        return;
    }

    // ignore if we're charging or lost control
    if (pl_moving &&
        (_player->movement_gens.top_id() == movement::gen::charge ||
            ((pl_moving->hasUnitState(UNIT_STAT_FLEEING_MOVE) ||
                 pl_moving->hasUnitState(UNIT_STAT_STUNNED) ||
                 pl_moving->hasUnitState(
                     UNIT_STAT_CONFUSED)) // Don't accept packets if we're
                                          // fleeing/stunned/confused...
                &&
                opcode != MSG_MOVE_HEARTBEAT && opcode != MSG_MOVE_FALL_LAND &&
                opcode != MSG_MOVE_STOP &&
                opcode != MSG_MOVE_STOP_STRAFE && // ...except for land and stop
                                                  // packets
                opcode != MSG_MOVE_STOP_TURN && opcode != MSG_MOVE_STOP_PITCH &&
                opcode != MSG_MOVE_STOP_SWIM))) // ...more stop packets
    {
        recv_data.rpos(recv_data.wpos()); // prevent warnings spam
        return;
    }

    if (opcode == MSG_MOVE_SET_FACING && !force_facing_update_)
    {
        saved_facing_ = std::move(recv_data);
        return;
    }

    // If we get a non-facing packet, make sure we clear the queued facing
    // packet (if we have one), as to not screw up the order
    if (opcode != MSG_MOVE_SET_FACING)
        saved_facing_.clear();

    if (pl_moving && !pl_moving->move_validator)
        return;
    if (pl_moving)
        pl_moving->move_validator->new_movement_packet();

    float prev_xyzo[4];
    moving->GetPosition(prev_xyzo[0], prev_xyzo[1], prev_xyzo[2]);
    prev_xyzo[3] = moving->GetO();

    /* extract packet */
    MovementInfo movementInfo;
    recv_data >> movementInfo;
    /*----------------*/

    if (!VerifyMovementInfo(movementInfo))
        return;

    translate_movement_timestamp(movementInfo);

    // fall damage generation (ignore in flight case that can be triggered also
    // at lags in moment teleportation to another map).
    if (opcode == MSG_MOVE_FALL_LAND && pl_moving && !pl_moving->IsTaxiFlying())
        pl_moving->HandleFall(movementInfo);

    /* process position-change */
    HandleMoverRelocation(movementInfo);

    if (pl_moving)
        pl_moving->UpdateFallInformationIfNeed(movementInfo, opcode);

    // Save Players Run/Walk Mode (note: _saves_ it; doesn't affect the client)
    if (pl_moving)
    {
        if (opcode == MSG_MOVE_SET_RUN_MODE)
            pl_moving->SetRunMode(true, false);
        else if (opcode == MSG_MOVE_SET_WALK_MODE)
            pl_moving->SetRunMode(false, false);
    }

    // Movement anti-cheat validation (note: do the validation after the player
    // structure has been updated)
    if (pl_moving &&
        !pl_moving->move_validator->validate_movement(movementInfo, opcode) &&
        sWorld::Instance()->getConfig(CONFIG_BOOL_ANTI_CHEAT_KICK_ENABLED))
    {
        // Revert positon and disconnect player
        pl_moving->SetPosition(
            prev_xyzo[0], prev_xyzo[1], prev_xyzo[2], prev_xyzo[3]);
        if (pl_moving->GetSession())
            pl_moving->GetSession()->KickPlayer();
        return;
    }

    // FIXME: This is a hack but makes it look good client-side.
    // With the root flag there it will glide once the stun/root is over
    if (pl_moving && opcode == MSG_MOVE_STOP)
        movementInfo.RemoveMovementFlag(MOVEFLAG_ROOT);

    // Interrupt casting on start movement
    bool mv = opcode == MSG_MOVE_START_FORWARD ||
              opcode == MSG_MOVE_START_BACKWARD ||
              opcode == MSG_MOVE_START_STRAFE_LEFT ||
              opcode == MSG_MOVE_START_STRAFE_RIGHT;
    if (pl_moving && mv)
    {
        if (auto generic = pl_moving->GetCurrentSpell(CURRENT_GENERIC_SPELL))
        {
            if (generic->m_spellInfo->InterruptFlags &
                SPELL_INTERRUPT_FLAG_MOVEMENT)
                pl_moving->InterruptSpell(CURRENT_GENERIC_SPELL, false);
        }
        if (auto channeled =
                pl_moving->GetCurrentSpell(CURRENT_CHANNELED_SPELL))
        {
            if (channeled->m_spellInfo->ChannelInterruptFlags &
                    CHANNEL_FLAG_MOVEMENT &&
                !(channeled->GetCastTime() == 0 &&
                    channeled->getState() == SPELL_STATE_PREPARING))
                pl_moving->InterruptSpell(CURRENT_CHANNELED_SPELL);
        }
    }

    WorldPacket data(opcode, recv_data.size());
    data << moving->GetPackGUID(); // write guid
    movementInfo.Write(data);      // write data
    moving->SendMessageToSetExcept(&data, _player);

    // Send movement packet to all players in range. Translate the movementInfo
    // timestamp (which is now server time) to client time.
    // TODO: This makes movement look better, but we have strange glitches when
    // the character first appears.
    /* auto sz = recv_data.size();
    auto saved_time = movementInfo.time;
    maps::visitors::simple<Player>{}(moving,
        moving->GetMap()->GetVisibilityDistance(),
        [this, opcode, saved_time, &movementInfo, moving, sz](Player* p)
        {
            if (unlikely(_player == p))
                return;

            auto t = (int64)p->GetSession()->client_tick_count +
                     ((int64)movementInfo.time -
                         (int64)p->GetSession()->server_tick_count);
            if (t < 0)
                t = 0;
            movementInfo.time = (uint32)t;

            WorldPacket data(opcode, sz);
            data << moving->GetPackGUID(); // write guid
            movementInfo.Write(data);      // write data
            p->GetSession()->send_packet(std::move(data));

            movementInfo.time = saved_time;
        }); */
}

void WorldSession::HandleMoveTimeSkippedOpcode(WorldPacket& recv_data)
{
    LOG_DEBUG(logging, "WORLD: Time Lag/Synchronization Resent/Update");

    recv_data >> Unused<uint64>(); // Player guid
    recv_data >> Unused<uint32>(); // Time (diff or exact?)
}

void WorldSession::HandleForceSpeedChangeAckOpcodes(WorldPacket& recv_data)
{
    uint32 opcode = recv_data.opcode();

    /* extract packet */
    ObjectGuid guid;
    MovementInfo movementInfo;
    float newspeed;

    recv_data >> guid;
    recv_data >> Unused<uint32>(); // counter or moveEvent
    recv_data >> movementInfo;
    recv_data >> newspeed;

    // now can skip not our packet
    if (_player->GetObjectGuid() != guid)
        return;
    /*----------------*/

    // client ACK send one packet for mounted/run case and need skip all except
    // last from its
    // in other cases anti-cheat check can be fail in false case
    UnitMoveType move_type;
    UnitMoveType force_move_type;

    static char const* move_type_name[MAX_MOVE_TYPE] = {"Walk", "Run",
        "RunBack", "Swim", "SwimBack", "TurnRate", "Flight", "FlightBack"};

    switch (opcode)
    {
    case CMSG_FORCE_WALK_SPEED_CHANGE_ACK:
        move_type = MOVE_WALK;
        force_move_type = MOVE_WALK;
        break;
    case CMSG_FORCE_RUN_SPEED_CHANGE_ACK:
        move_type = MOVE_RUN;
        force_move_type = MOVE_RUN;
        break;
    case CMSG_FORCE_RUN_BACK_SPEED_CHANGE_ACK:
        move_type = MOVE_RUN_BACK;
        force_move_type = MOVE_RUN_BACK;
        break;
    case CMSG_FORCE_SWIM_SPEED_CHANGE_ACK:
        move_type = MOVE_SWIM;
        force_move_type = MOVE_SWIM;
        break;
    case CMSG_FORCE_SWIM_BACK_SPEED_CHANGE_ACK:
        move_type = MOVE_SWIM_BACK;
        force_move_type = MOVE_SWIM_BACK;
        break;
    case CMSG_FORCE_TURN_RATE_CHANGE_ACK:
        move_type = MOVE_TURN_RATE;
        force_move_type = MOVE_TURN_RATE;
        break;
    case CMSG_FORCE_FLIGHT_SPEED_CHANGE_ACK:
        move_type = MOVE_FLIGHT;
        force_move_type = MOVE_FLIGHT;
        break;
    case CMSG_FORCE_FLIGHT_BACK_SPEED_CHANGE_ACK:
        move_type = MOVE_FLIGHT_BACK;
        force_move_type = MOVE_FLIGHT_BACK;
        break;
    default:
        logging.error(
            "WorldSession::HandleForceSpeedChangeAck: Unknown move type "
            "opcode: %u",
            opcode);
        return;
    }

    // skip all forced speed changes except last and unexpected
    // in run/mounted case used one ACK and it must be
    // skipped.m_forced_speed_changes[MOVE_RUN} store both.
    if (_player->m_forced_speed_changes[force_move_type] > 0)
    {
        --_player->m_forced_speed_changes[force_move_type];
        if (_player->m_forced_speed_changes[force_move_type] > 0)
            return;
    }

    if (!_player->GetTransport() &&
        fabs(_player->GetSpeed(move_type) - newspeed) > 0.01f)
    {
        if (_player->GetSpeed(move_type) >
            newspeed) // must be greater - just correct
        {
            logging.error(
                "%sSpeedChange player %s is NOT correct (must be %f instead "
                "%f), force set to correct value",
                move_type_name[move_type], _player->GetName(),
                _player->GetSpeed(move_type), newspeed);
            _player->SetSpeedRate(
                move_type, _player->GetSpeedRate(move_type), true);
        }
        else // must be lesser - cheating
        {
            LOG_DEBUG(logging,
                "Player %s from account id %u kicked for incorrect speed (must "
                "be %f instead %f)",
                _player->GetName(), _player->GetSession()->GetAccountId(),
                _player->GetSpeed(move_type), newspeed);
            _player->GetSession()->KickPlayer();
        }
    }
}

void WorldSession::HandleSetActiveMoverOpcode(WorldPacket& recv_data)
{
    // NOTE: This opcode is the client saying it has now updated which
    // target it is moving. This means that _player is now steering
    // the guid we receive.

    ObjectGuid guid;
    recv_data >> guid;

    if (_player->GetMovingUnit()->GetObjectGuid() != guid)
    {
        logging.error(
            "HandleSetActiveMoverOpcode: incorrect mover guid: mover is %s and "
            "should be %s",
            _player->GetMovingUnit()->GetGuidStr().c_str(),
            guid.GetString().c_str());
        return;
    }

    _player->SetClientMovingUnit(guid);
}

void WorldSession::HandleMoveNotActiveMoverOpcode(WorldPacket& recv_data)
{
    // NOTE: This opcode is a bit fuzzy, but from what I can tell it is the
    // client
    // saying it is no longer moving itself. It then presents the guid it was
    // moving
    // (this is the guid of _player in all cases I've observed), as well as a
    // movement
    // info of where it was when the movement stopped.
    // From my testing this packet and its sent data seems completely irrelevant
    // and
    // safe to ignore. When the mover changes CMSG_SET_ACTIVE_MOVER will be sent
    // by the new mover, followed by a movement packet that sets the location of
    // where
    // the moved target is.

    ObjectGuid old_mover_guid;
    MovementInfo mi;

    recv_data >> old_mover_guid;
    recv_data >> mi;
}

void WorldSession::HandleMountSpecialAnimOpcode(WorldPacket& /*recvdata*/)
{
    WorldPacket data(SMSG_MOUNTSPECIAL_ANIM, 8);
    data << GetPlayer()->GetObjectGuid();

    GetPlayer()->SendMessageToSet(&data, false);
}

void WorldSession::HandleMoveKnockBackAck(WorldPacket& recv_data)
{
    // ignore, waiting processing in WorldSession::HandleMoveWorldportAckOpcode
    // and WorldSession::HandleMoveTeleportAck
    if (_player->IsBeingTeleported())
    {
        recv_data.rpos(recv_data.wpos()); // prevent warnings spam
        return;
    }

    ObjectGuid guid;
    MovementInfo movementInfo;

    recv_data >> guid;
    recv_data >> Unused<uint32>(); // knockback packets counter
    recv_data >> movementInfo;

    if (guid != _player->GetObjectGuid())
    {
        logging.error(
            "WorldSession::HandleMoveKnockBackAck: received a GUID that's not "
            "the player!");
        return;
    }

    if (!_player->move_validator)
        return;
    _player->move_validator->new_movement_packet();

    if (!VerifyMovementInfo(movementInfo))
        return;

    translate_movement_timestamp(movementInfo);

    float prev_xyzo[4];
    _player->GetPosition(prev_xyzo[0], prev_xyzo[1], prev_xyzo[2]);
    prev_xyzo[3] = _player->GetO();

    HandleMoverRelocation(movementInfo);

    // Movement anti-cheat validation (note: do the validation after the player
    // structure has been updated)
    if (!_player->move_validator->validate_movement(
            movementInfo, recv_data.opcode()) &&
        sWorld::Instance()->getConfig(CONFIG_BOOL_ANTI_CHEAT_KICK_ENABLED))
    {
        // Revert positon and disconnect player
        _player->SetPosition(
            prev_xyzo[0], prev_xyzo[1], prev_xyzo[2], prev_xyzo[3]);
        KickPlayer();
        return;
    }

    // Interrupt spells on knockback start
    if (auto generic = _player->GetCurrentSpell(CURRENT_GENERIC_SPELL))
    {
        if (generic->m_spellInfo->InterruptFlags &
            SPELL_INTERRUPT_FLAG_MOVEMENT)
            _player->InterruptSpell(CURRENT_GENERIC_SPELL, false);
    }
    if (auto channeled = _player->GetCurrentSpell(CURRENT_CHANNELED_SPELL))
    {
        if (channeled->m_spellInfo->ChannelInterruptFlags &
                CHANNEL_FLAG_MOVEMENT &&
            !(channeled->GetCastTime() == 0 &&
                channeled->getState() == SPELL_STATE_PREPARING))
            _player->InterruptSpell(CURRENT_CHANNELED_SPELL);
    }

    _player->SetKnockbacked(true);

    WorldPacket data(MSG_MOVE_KNOCK_BACK, recv_data.size() + 15);
    data << _player->GetPackGUID(); // write guid
    movementInfo.Write(data);       // write data
    data << movementInfo.jump.sinAngle;
    data << movementInfo.jump.cosAngle;
    data << movementInfo.jump.xyspeed;
    data << movementInfo.jump.zspeed;
    _player->SendMessageToSetExcept(&data, _player);
}

void WorldSession::HandleMoveHoverAck(WorldPacket& recv_data)
{
    MovementInfo movementInfo;

    recv_data >> Unused<uint64>(); // guid
    recv_data >> Unused<uint32>(); // unk
    recv_data >> movementInfo;
    recv_data >> Unused<uint32>(); // unk2
}

void WorldSession::HandleMoveWaterWalkAck(WorldPacket& recv_data)
{
    MovementInfo movementInfo;

    recv_data.read_skip<uint64>(); // guid
    recv_data.read_skip<uint32>(); // unk
    recv_data >> movementInfo;
    recv_data >> Unused<uint32>(); // unk2
}

void WorldSession::HandleSummonResponseOpcode(WorldPacket& recv_data)
{
    if (!_player->isAlive() || _player->isInCombat())
        return;

    ObjectGuid summonerGuid;
    bool agree;
    recv_data >> summonerGuid;
    recv_data >> agree;

    _player->SummonIfPossible(agree);
}

bool WorldSession::VerifyMovementInfo(
    MovementInfo const& movementInfo, ObjectGuid const& guid) const
{
    // ignore wrong guid (player attempt cheating own session for not own guid
    // possible...)
    if (guid != _player->GetMovingUnit()->GetObjectGuid())
        return false;

    return VerifyMovementInfo(movementInfo);
}

bool WorldSession::VerifyMovementInfo(MovementInfo const& movementInfo) const
{
    if (!maps::verify_coords(movementInfo.pos.x, movementInfo.pos.y))
        return false;

    if (movementInfo.HasMovementFlag(MOVEFLAG_ONTRANSPORT))
    {
        // transports size limited
        // (also received at zeppelin/lift leave by some reason with t_* as
        // absolute in continent coordinates, can be safely skipped)
        if (movementInfo.transport.pos.x > 50 ||
            movementInfo.transport.pos.y > 50 ||
            movementInfo.transport.pos.z > 100)
            return false;

        if (!maps::verify_coords(
                movementInfo.pos.x + movementInfo.transport.pos.x,
                movementInfo.pos.y + movementInfo.transport.pos.y))
        {
            return false;
        }
    }

    return true;
}

void WorldSession::HandleMoverRelocation(MovementInfo& movementInfo)
{
    Unit* moving = _player->GetMovingUnit();

    if (Player* pl_moving =
            moving->GetTypeId() == TYPEID_PLAYER ? (Player*)moving : nullptr)
    {
        if (movementInfo.HasMovementFlag(MOVEFLAG_ONTRANSPORT))
        {
            if (!pl_moving->GetTransport())
            {
                if (Transport* transport = pl_moving->GetMap()->GetTransport(
                        movementInfo.transport.guid))
                    transport->AddPassenger(pl_moving);
            }
            else if (pl_moving->GetTransport()->GetObjectGuid() !=
                     movementInfo.transport.guid)
            {
                pl_moving->GetTransport()->RemovePassenger(pl_moving);
                if (Transport* transport = pl_moving->GetMap()->GetTransport(
                        movementInfo.transport.guid))
                    transport->AddPassenger(pl_moving);
                else
                    movementInfo.transport.Reset();
            }
        }
        else if (pl_moving->GetTransport()) // if we were on a transport, leave
        {
            pl_moving->GetTransport()->RemovePassenger(pl_moving);
        }

        if (movementInfo.HasMovementFlag(MOVEFLAG_SWIMMING) !=
            pl_moving->IsInWater())
        {
            // now client not include swimming flag in case jumping under water
            pl_moving->SetInWater(
                !pl_moving->IsInWater() ||
                pl_moving->GetTerrain()->IsUnderWater(movementInfo.pos.x,
                    movementInfo.pos.y, movementInfo.pos.z));
        }

        pl_moving->SetPosition(movementInfo.pos.x, movementInfo.pos.y,
            movementInfo.pos.z, movementInfo.pos.o);
        pl_moving->m_movementInfo = movementInfo;

        if (movementInfo.pos.z < -500.0f)
        {
            if (pl_moving->InBattleGround() && pl_moving->GetBattleGround() &&
                pl_moving->GetBattleGround()->HandlePlayerUnderMap(_player))
            {
                // do nothing, the handle already did if returned true
            }
            else
            {
                // NOTE: this is actually called many times while falling
                // even after the player has been teleported away
                // TODO: discard movement packets after the player is rooted
                if (pl_moving->isAlive())
                {
                    pl_moving->EnvironmentalDamage(
                        DAMAGE_FALL_TO_VOID, pl_moving->GetMaxHealth());
                    // pl can be alive if GM/etc
                    if (!pl_moving->isAlive())
                    {
                        // change the death state to CORPSE to prevent the death
                        // timer from
                        // starting in the next player update
                        pl_moving->KillPlayer();
                        pl_moving->BuildPlayerRepop();
                    }
                }

                // cancel the death timer here if started
                pl_moving->RepopAtGraveyard();
            }
        }
    }
    else // creature charmed
    {
        if (moving->IsInWorld())
            moving->GetMap()->relocate((Creature*)moving, movementInfo.pos.x,
                movementInfo.pos.y, movementInfo.pos.z, movementInfo.pos.o);
    }
}
