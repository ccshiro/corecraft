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

#include "MoveSplineInit.h"
#include "MoveSpline.h"
#include "PathFinder.h"
#include "Transport.h"
#include "packet_builder.h"
#include "../Player.h"
#include "../Unit.h"

namespace movement
{
UnitMoveType SelectSpeedType(uint32 moveFlags, Unit& unit)
{
    bool npc = unit.GetTypeId() == TYPEID_UNIT &&
               !(unit.GetCharmerOrOwnerGuid() &&
                   unit.GetCharmerOrOwnerGuid().IsPlayer());

    // NPCs either use run or walk speed
    if (npc)
        return (moveFlags & MOVEFLAG_WALK_MODE) ? MOVE_WALK : MOVE_RUN;

    // Players & Player Controlled Creatures
    if (moveFlags & MOVEFLAG_FLYING)
    {
        if (moveFlags &
            MOVEFLAG_BACKWARD /*&& speed_obj.flight >= speed_obj.flight_back*/)
            return MOVE_FLIGHT_BACK;
        else
            return MOVE_FLIGHT;
    }
    else if (moveFlags & MOVEFLAG_SWIMMING)
    {
        if (moveFlags &
            MOVEFLAG_BACKWARD /*&& speed_obj.swim >= speed_obj.swim_back*/)
            return MOVE_SWIM_BACK;
        else
            return MOVE_SWIM;
    }
    else if (moveFlags & MOVEFLAG_WALK_MODE)
    {
        // if ( speed_obj.run > speed_obj.walk )
        return MOVE_WALK;
    }
    else if (moveFlags &
             MOVEFLAG_BACKWARD /*&& speed_obj.run >= speed_obj.run_back*/)
        return MOVE_RUN_BACK;

    return MOVE_RUN;
}

int32 MoveSplineInit::Launch()
{
    MoveSpline& move_spline = *unit.movespline;

    bool transport = unit.GetTransport() != nullptr;

    // First point is always our current position; needs to be computed if
    // current spline wasn't finished
    Vector3 real_position;
    if (!move_spline.Finalized() && move_spline.transport == transport)
        real_position = move_spline.ComputePosition();
    else if (transport)
        unit.m_movementInfo.transport.pos.Get(
            real_position.x, real_position.y, real_position.z);
    else
        real_position = G3D::Vector3(unit.GetX(), unit.GetY(), unit.GetZ());

    if (args.path
            .empty()) // For empty paths our path becomes: {real_pos, real_pos}
        MoveTo(real_position);
    args.path[0] = real_position; // Make first point always real pos

    move_spline.transport = transport;

    uint32 moveFlags = unit.m_movementInfo.GetMovementFlags();
    if (args.flags.runmode)
        moveFlags &= ~MOVEFLAG_WALK_MODE;
    else
        moveFlags |= MOVEFLAG_WALK_MODE;

    moveFlags |= (MOVEFLAG_SPLINE_ENABLED | MOVEFLAG_FORWARD);

    if (args.velocity == 0.f)
        args.velocity = unit.GetSpeed(SelectSpeedType(moveFlags, unit));

    if (!args.Validate())
        return 0;

    unit.m_movementInfo.SetMovementFlags(moveFlags);
    move_spline.Initialize(args);

    // NOTE: This isn't a very nice way to structure it, but saved turning
    // packet
    // need to be cleared before we send movespline
    if (unit.GetTypeId() == TYPEID_PLAYER &&
        static_cast<Player&>(unit).GetSession())
        static_cast<Player&>(unit).GetSession()->clear_saved_facing();

    WorldPacket data(SMSG_MONSTER_MOVE, 64);
    data << unit.GetPackGUID();
    if (transport)
    {
        data.opcode(SMSG_MONSTER_MOVE_TRANSPORT);
        data << unit.GetTransport()->GetPackGUID();
    }
    PacketBuilder::WriteMonsterMove(move_spline, data);
    unit.SendMessageToSet(&data, true);

    return move_spline.Duration();
}

void MoveSplineInit::Stop()
{
    MoveSpline& move_spline = *unit.movespline;

    // No need to stop if we are not moving
    if (move_spline.Finalized())
        return;

    bool transport = unit.GetTransport() != nullptr;

    Vector3 real_position;
    if (move_spline.transport == transport)
        real_position = move_spline.ComputePosition();
    else if (transport)
        unit.m_movementInfo.transport.pos.Get(
            real_position.x, real_position.y, real_position.z);
    else
        real_position = G3D::Vector3(unit.GetX(), unit.GetY(), unit.GetZ());

    args.flags = MoveSplineFlag::Done;
    unit.m_movementInfo.RemoveMovementFlag(
        MOVEFLAG_FORWARD | MOVEFLAG_SPLINE_ENABLED);
    move_spline.transport = transport;
    move_spline.Initialize(args);

    WorldPacket data(SMSG_MONSTER_MOVE, 64);
    data << unit.GetPackGUID();
    if (transport)
    {
        data.opcode(SMSG_MONSTER_MOVE_TRANSPORT);
        data << unit.GetTransport()->GetPackGUID();
    }
    PacketBuilder::WriteStopMovement(real_position, args.splineId, data);
    unit.SendMessageToSet(&data, true);
}

MoveSplineInit::MoveSplineInit(Unit& m) : unit(m)
{
    // mix existing state into new
    args.flags.runmode =
        !unit.m_movementInfo.HasMovementFlag(MOVEFLAG_WALK_MODE);
    args.flags.flying = unit.m_movementInfo.HasMovementFlag(
        MOVEFLAG_FLYING | MOVEFLAG_LEVITATING);
}

void MoveSplineInit::SetFacing(const Unit* target)
{
    args.flags.EnableFacingTarget();
    args.facing.target = target->GetObjectGuid().GetRawValue();
}

void MoveSplineInit::SetFacing(float angle)
{
    args.facing.angle = G3D::wrap(angle, 0.f, (float)G3D::twoPi());
    args.flags.EnableFacingAngle();
}
}
