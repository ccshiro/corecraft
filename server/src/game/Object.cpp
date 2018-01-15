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

#include "Object.h"
#include "ByteBuffer.h"
#include "Creature.h"
#include "DetourNavMesh.h"
#include "DetourNavMeshQuery.h"
#include "DynamicObject.h"
#include "GameObjectModel.h"
#include "logging.h"
#include "MapManager.h"
#include "MoveMap.h"
#include "MoveMapSharedDefines.h"
#include "ObjectGuid.h"
#include "ObjectMgr.h"
#include "ObjectPosSelector.h"
#include "Opcodes.h"
#include "Player.h"
#include "SharedDefines.h"
#include "SpecialVisCreature.h"
#include "TemporarySummon.h"
#include "Totem.h"
#include "Transport.h"
#include "UpdateData.h"
#include "UpdateMask.h"
#include "Util.h"
#include "VMapFactory.h"
#include "World.h"
#include "WorldPacket.h"
#include "loot_distributor.h"
#include "maps/callbacks.h"
#include "maps/checks.h"
#include "maps/visitors.h"
#include "movement/packet_builder.h"
#include "vmap/DynamicTree.h"

#define ZONE_AREA_CACHE_TIMER \
    10 * IN_MILLISECONDS // How often zone/area cache is updated

////////////////////////////////////////////////////////////
// Methods of struct MovementInfo

void MovementInfo::Read(ByteBuffer& data)
{
    uint32 prev_flags = moveFlags;

    data >> moveFlags;
    data >> moveFlags2;
    data >> time;
    data >> pos.x;
    data >> pos.y;
    data >> pos.z;
    data >> pos.o;

    if (HasMovementFlag(MOVEFLAG_ONTRANSPORT))
    {
        data >> transport.guid;
        data >> transport.pos.x;
        data >> transport.pos.y;
        data >> transport.pos.z;
        data >> transport.pos.o;
        data >> transport.time;
    }

    if (HasMovementFlag(MOVEFLAG_SWIMMING | MOVEFLAG_FLYING2))
    {
        data >> pitch;
    }

    data >> fallTime;

    if (HasMovementFlag(MOVEFLAG_GRAVITY))
    {
        data >> jump.zspeed;
        data >> jump.sinAngle;
        data >> jump.cosAngle;
        data >> jump.xyspeed;
    }

    if (HasMovementFlag(MOVEFLAG_SPLINE_ELEVATION))
    {
        data >> splineElevation;
    }

    // MOVEMENTFLAG_ROOT cannot be set by the client, together with
    // MOVEMENTFLAG_FORWARD
    // it triggers a client-side bug that freezes the client for a short period
    // of time
    if ((prev_flags & MOVEFLAG_ROOT) == 0 && HasMovementFlag(MOVEFLAG_ROOT))
        RemoveMovementFlag(MOVEFLAG_ROOT);
    else if ((prev_flags & MOVEFLAG_ROOT) != 0 &&
             HasMovementFlag(MOVEFLAG_ROOT))
        RemoveMovementFlag(MovementFlags(MOVEFLAG_MASK_MOVING));
}

void MovementInfo::Write(ByteBuffer& data) const
{
    data << moveFlags;
    data << moveFlags2;
    data << time;
    data << pos.x;
    data << pos.y;
    data << pos.z;
    data << pos.o;

    if (HasMovementFlag(MOVEFLAG_ONTRANSPORT))
    {
        data << transport.guid;
        data << transport.pos.x;
        data << transport.pos.y;
        data << transport.pos.z;
        data << transport.pos.o;
        data << transport.time;
    }

    if (HasMovementFlag(MOVEFLAG_SWIMMING | MOVEFLAG_FLYING2))
    {
        data << pitch;
    }

    data << fallTime;

    if (HasMovementFlag(MOVEFLAG_GRAVITY))
    {
        data << jump.zspeed;
        data << jump.sinAngle;
        data << jump.cosAngle;
        data << jump.xyspeed;
    }

    if (HasMovementFlag(MOVEFLAG_SPLINE_ELEVATION))
    {
        data << splineElevation;
    }
}

////////////////////////////////////////////////////////////
// Start of Object

Object::Object()
  : m_objectType(TYPEMASK_OBJECT), m_objectTypeId(TYPEID_OBJECT),
    m_uint32Values(nullptr), _changedFields(nullptr), m_valuesCount(0),
    m_objectUpdated(false), m_lootDistributor(nullptr), m_inWorld(false),
    m_forceDynForEveryone(false)
{
}

Object::~Object()
{
    if (IsInWorld())
    {
        ///- Do NOT call RemoveFromWorld here, if the object is a player it will
        /// crash
        logging.error(
            "Object::~Object (GUID: %u TypeId: %u) deleted but still in "
            "world!!",
            GetGUIDLow(), GetTypeId());
        assert(false);
    }

    if (m_objectUpdated)
    {
        logging.error(
            "Object::~Object (GUID: %u TypeId: %u) deleted but still have "
            "updated status!!",
            GetGUIDLow(), GetTypeId());
        assert(false);
    }

    if (m_uint32Values)
    {
        // LOG_DEBUG(logging,"Object desctr 1 check (%p)",(void*)this);
        delete[] m_uint32Values;
        delete[] _changedFields;
        // LOG_DEBUG(logging,"Object desctr 2 check (%p)",(void*)this);
    }

    delete m_lootDistributor;
    m_lootDistributor = nullptr;
}

void Object::_InitValues()
{
    m_uint32Values = new uint32[m_valuesCount];
    memset(m_uint32Values, 0, m_valuesCount * sizeof(uint32));

    _changedFields = new bool[m_valuesCount];
    memset(_changedFields, 0, m_valuesCount * sizeof(bool));

    m_objectUpdated = false;
}

void Object::_Create(uint32 guidlow, uint32 entry, HighGuid guidhigh)
{
    if (!m_uint32Values)
        _InitValues();

    ObjectGuid guid = ObjectGuid(guidhigh, entry, guidlow);
    SetGuidValue(OBJECT_FIELD_GUID, guid);
    SetUInt32Value(OBJECT_FIELD_TYPE, m_objectType);
    m_PackGUID.Set(guid);
}

void Object::SetObjectScale(float newScale)
{
    SetFloatValue(OBJECT_FIELD_SCALE_X, newScale);
}

void Object::SendForcedObjectUpdate(bool ignore_normal_update)
{
    if (!m_inWorld ||
        (!m_objectUpdated &&
            !ignore_normal_update)) // FIXME: "ignore_normal_update" is a hack
                                    // and should be removed as soon as
                                    // possible. See ForceUpdateDynflag() and
                                    // ForceUpdateDynflagForPlayer()
        return;

    UpdateDataMapType update_players;

    m_objectUpdated = true;
    BuildUpdateData(update_players);
    RemoveFromClientUpdateList();

    for (auto& update_player : update_players)
        update_player.second.SendPacket(update_player.first->GetSession());
}

void Object::BuildMovementUpdateBlock(UpdateData* data, uint8 flags) const
{
    ByteBuffer buf(500);

    buf << uint8(UPDATETYPE_MOVEMENT);
    buf << GetObjectGuid();

    BuildMovementUpdate(&buf, flags);

    data->AddUpdateBlock(buf);
}

void Object::BuildCreateUpdateBlockForPlayer(
    UpdateData* data, Player* target) const
{
    if (!target)
        return;

    uint8 updatetype = UPDATETYPE_CREATE_OBJECT;
    uint8 updateFlags = m_updateFlag;

    /** lower flag1 **/
    if (target == this) // building packet for yourself
        updateFlags |= UPDATEFLAG_SELF;

    if (updateFlags & UPDATEFLAG_HAS_POSITION)
    {
        // UPDATETYPE_CREATE_OBJECT2 dynamic objects, corpses...
        if (isType(TYPEMASK_DYNAMICOBJECT) || isType(TYPEMASK_CORPSE) ||
            isType(TYPEMASK_PLAYER))
            updatetype = UPDATETYPE_CREATE_OBJECT2;

        // UPDATETYPE_CREATE_OBJECT2 for pets...
        if (target->GetPetGuid() == GetObjectGuid())
            updatetype = UPDATETYPE_CREATE_OBJECT2;

        // UPDATETYPE_CREATE_OBJECT2 for some gameobject types...
        if (isType(TYPEMASK_GAMEOBJECT))
        {
            switch (((GameObject*)this)->GetGoType())
            {
            case GAMEOBJECT_TYPE_TRAP:
            case GAMEOBJECT_TYPE_DUEL_ARBITER:
            case GAMEOBJECT_TYPE_FLAGSTAND:
            case GAMEOBJECT_TYPE_FLAGDROP:
                updatetype = UPDATETYPE_CREATE_OBJECT2;
                break;
            case GAMEOBJECT_TYPE_TRANSPORT:
                updateFlags |= UPDATEFLAG_TRANSPORT;
                break;
            default:
                break;
            }
        }

        if (isType(TYPEMASK_UNIT))
        {
            if (static_cast<const Unit*>(this)->getVictim() &&
                (dynamic_cast<const Creature*>(this) == nullptr ||
                    !const_cast<Creature*>(static_cast<const Creature*>(this))
                         ->AI() ||
                    !const_cast<Creature*>(static_cast<const Creature*>(this))
                         ->AI()
                         ->IsPacified()))
                updateFlags |= UPDATEFLAG_HAS_ATTACKING_TARGET;
        }
    }

    // LOG_DEBUG(logging,"BuildCreateUpdate: update-type: %u, object-type: %u
    // got
    // updateFlags: %X", updatetype, m_objectTypeId, updateFlags);

    ByteBuffer buf(500);
    buf << uint8(updatetype);
    buf << GetPackGUID();
    buf << uint8(m_objectTypeId);

    BuildMovementUpdate(&buf, updateFlags);

    UpdateMask updateMask;
    updateMask.SetCount(m_valuesCount);
    _SetCreateBits(&updateMask, target);
    BuildValuesUpdate(updatetype, &buf, &updateMask, target);
    data->AddUpdateBlock(buf);
}

void Object::SendCreateUpdateToPlayer(Player* player)
{
    // send create update to player
    UpdateData upd;
    WorldPacket packet;

    BuildCreateUpdateBlockForPlayer(&upd, player);
    upd.SendPacket(player->GetSession());
}

void Object::BuildValuesUpdateBlockForPlayer(
    UpdateData* data, Player* target) const
{
    ByteBuffer buf(500);

    buf << uint8(UPDATETYPE_VALUES);
    buf << GetPackGUID();

    UpdateMask updateMask;
    updateMask.SetCount(m_valuesCount);

    _SetUpdateBits(&updateMask, target);
    BuildValuesUpdate(UPDATETYPE_VALUES, &buf, &updateMask, target);

    data->AddUpdateBlock(buf);
}

void Object::BuildOutOfRangeUpdateBlock(UpdateData* data) const
{
    data->AddOutOfRangeGUID(GetObjectGuid());
}

void Object::DestroyForPlayer(Player* target) const
{
    assert(target);

    WorldPacket data(SMSG_DESTROY_OBJECT, 8);
    data << GetObjectGuid();
    target->GetSession()->send_packet(std::move(data));
}

void Object::BuildMovementUpdate(ByteBuffer* data, uint8 updateFlags) const
{
    *data << uint8(updateFlags); // update flags

    // 0x20
    if (updateFlags & UPDATEFLAG_LIVING)
    {
        Unit* unit = ((Unit*)this);

        if (GetTypeId() == TYPEID_PLAYER)
        {
            Player* player = ((Player*)unit);
            if (player->GetTransport())
                player->m_movementInfo.AddMovementFlag(MOVEFLAG_ONTRANSPORT);
            else
                player->m_movementInfo.RemoveMovementFlag(MOVEFLAG_ONTRANSPORT);
        }

        // Update movement info time
        unit->m_movementInfo.time = WorldTimer::getMSTime();
        // Write movement info
        unit->m_movementInfo.Write(*data);

        // Unit speeds
        *data << float(unit->GetSpeed(MOVE_WALK));
        *data << float(unit->GetSpeed(MOVE_RUN));
        *data << float(unit->GetSpeed(MOVE_RUN_BACK));
        *data << float(unit->GetSpeed(MOVE_SWIM));
        *data << float(unit->GetSpeed(MOVE_SWIM_BACK));
        *data << float(unit->GetSpeed(MOVE_FLIGHT));
        *data << float(unit->GetSpeed(MOVE_FLIGHT_BACK));
        *data << float(unit->GetSpeed(MOVE_TURN_RATE));

        // 0x08000000
        if (unit->m_movementInfo.GetMovementFlags() & MOVEFLAG_SPLINE_ENABLED)
            movement::PacketBuilder::WriteCreate(*unit->movespline, *data);
    }
    // 0x40
    else if (updateFlags & UPDATEFLAG_HAS_POSITION)
    {
        // 0x02
        if (updateFlags & UPDATEFLAG_TRANSPORT &&
            ((GameObject*)this)->GetGoType() == GAMEOBJECT_TYPE_MO_TRANSPORT)
        {
            *data << float(0);
            *data << float(0);
            *data << float(0);
            *data << float(((WorldObject*)this)->GetO());
        }
        else
        {
            *data << float(((WorldObject*)this)->GetX());
            *data << float(((WorldObject*)this)->GetY());
            *data << float(((WorldObject*)this)->GetZ());
            *data << float(((WorldObject*)this)->GetO());
        }
    }

    // 0x8
    if (updateFlags & UPDATEFLAG_LOWGUID)
    {
        switch (GetTypeId())
        {
        case TYPEID_OBJECT:
        case TYPEID_ITEM:
        case TYPEID_CONTAINER:
        case TYPEID_GAMEOBJECT:
        case TYPEID_DYNAMICOBJECT:
        case TYPEID_CORPSE:
            *data << uint32(GetGUIDLow()); // GetGUIDLow()
            break;
        case TYPEID_UNIT:
            *data << uint32(0x0000000B); // unk, can be 0xB or 0xC
            break;
        case TYPEID_PLAYER:
            if (updateFlags & UPDATEFLAG_SELF)
                *data << uint32(0x00000015); // unk, can be 0x15 or 0x22
            else
                *data << uint32(0x00000008); // unk, can be 0x7 or 0x8
            break;
        // unk
        default:
            *data << uint32(0x00000000);
            break;
        }
    }

    // 0x10
    if (updateFlags & UPDATEFLAG_HIGHGUID)
    {
        switch (GetTypeId())
        {
        case TYPEID_OBJECT:
        case TYPEID_ITEM:
        case TYPEID_CONTAINER:
        case TYPEID_GAMEOBJECT:
        case TYPEID_DYNAMICOBJECT:
        case TYPEID_CORPSE:
            *data << uint32(GetObjectGuid().GetHigh()); // GetGUIDHigh()
            break;
        // unk
        default:
            *data << uint32(0x00000000);
            break;
        }
    }

    // 0x4
    if (updateFlags &
        UPDATEFLAG_HAS_ATTACKING_TARGET) // packed guid (current target guid)
    {
        if (((Unit*)this)->getVictim())
            *data << ((Unit*)this)->getVictim()->GetPackGUID();
        else
            data->appendPackGUID(0);
    }

    // 0x2
    if (updateFlags & UPDATEFLAG_TRANSPORT)
    {
        *data << uint32(WorldTimer::getMSTime()); // ms time
    }
}

void Object::BuildValuesUpdate(uint8 updatetype, ByteBuffer* data,
    UpdateMask* updateMask, Player* target) const
{
    if (!target)
        return;

    bool IsActivateToQuest = false;
    bool IsPerCasterAuraState = false;

    if (updatetype == UPDATETYPE_CREATE_OBJECT ||
        updatetype == UPDATETYPE_CREATE_OBJECT2)
    {
        if (isType(TYPEMASK_GAMEOBJECT) && !((GameObject*)this)->IsTransport())
        {
            if (((GameObject*)this)->ActivateToQuest(target) ||
                target->isGameMaster())
                IsActivateToQuest = true;

            updateMask->SetBit(GAMEOBJECT_DYN_FLAGS);
        }
        else if (isType(TYPEMASK_UNIT))
        {
            if (((Unit*)this)->HasAuraState(AURA_STATE_CONFLAGRATE))
            {
                IsPerCasterAuraState = true;
                updateMask->SetBit(UNIT_FIELD_AURASTATE);
            }
        }
    }
    else // case UPDATETYPE_VALUES
    {
        if (isType(TYPEMASK_GAMEOBJECT) && !((GameObject*)this)->IsTransport())
        {
            if (((GameObject*)this)->ActivateToQuest(target) ||
                target->isGameMaster())
                IsActivateToQuest = true;

            updateMask->SetBit(GAMEOBJECT_DYN_FLAGS);
            updateMask->SetBit(GAMEOBJECT_ANIMPROGRESS);
        }
        else if (isType(TYPEMASK_UNIT))
        {
            if (((Unit*)this)->HasAuraState(AURA_STATE_CONFLAGRATE))
            {
                IsPerCasterAuraState = true;
                updateMask->SetBit(UNIT_FIELD_AURASTATE);
            }
        }
    }

    // FIXME: This is a hack. See ForceUpdateDynflagForPlayer()
    bool unset_dyn_bit = false;
    if (GetTypeId() == TYPEID_UNIT) // Creature only
    {
        auto find = m_forceUpdateDynflags.find(target->GetObjectGuid());
        if (find != m_forceUpdateDynflags.end())
        {
            updateMask->SetBit(UNIT_DYNAMIC_FLAGS);
            unset_dyn_bit = true;
            m_forceUpdateDynflags.erase(find);
        }
        if (m_forceDynForEveryone)
        {
            updateMask->SetBit(UNIT_DYNAMIC_FLAGS);
        }
    }

    assert(updateMask && updateMask->GetCount() == m_valuesCount);

    *data << (uint8)updateMask->GetBlockCount();
    data->append(updateMask->GetMask(), updateMask->GetLength());

    // 2 specialized loops for speed optimization in non-unit case
    if (isType(TYPEMASK_UNIT)) // unit (creature/player) case
    {
        for (uint16 index = 0; index < m_valuesCount; ++index)
        {
            if (updateMask->GetBit(index))
            {
                if (index == UNIT_NPC_FLAGS)
                {
                    uint32 appendValue = m_uint32Values[index];

                    if (GetTypeId() == TYPEID_UNIT)
                    {
                        if (appendValue & UNIT_NPC_FLAG_TRAINER)
                        {
                            if (!((Creature*)this)->IsTrainerOf(target, false))
                                appendValue &=
                                    ~(UNIT_NPC_FLAG_TRAINER |
                                        UNIT_NPC_FLAG_TRAINER_CLASS |
                                        UNIT_NPC_FLAG_TRAINER_PROFESSION);
                        }

                        if (appendValue & UNIT_NPC_FLAG_STABLEMASTER)
                        {
                            if (target->getClass() != CLASS_HUNTER)
                                appendValue &= ~UNIT_NPC_FLAG_STABLEMASTER;
                        }

                        if (appendValue & UNIT_NPC_FLAG_FLIGHTMASTER)
                        {
                            // HACK: Don't show flag flightmaster for opposite
                            // faction @ the stair of destiny
                            // TODO: Find general rule
                            if ((static_cast<const Creature*>(this)
                                            ->getFaction() == 1760 &&
                                    target->GetTeam() != HORDE) ||
                                (static_cast<const Creature*>(this)
                                            ->getFaction() == 1756 &&
                                    target->GetTeam() != ALLIANCE))
                                appendValue &= ~UNIT_NPC_FLAG_FLIGHTMASTER;
                        }

                        if (appendValue & UNIT_NPC_FLAG_QUESTGIVER)
                        {
                            if (((Creature*)this)->GetReactionTo(target) <=
                                REP_HOSTILE)
                                appendValue &= ~UNIT_NPC_FLAG_QUESTGIVER;
                        }
                    }

                    *data << uint32(appendValue);
                }
                else if (index == UNIT_FIELD_AURASTATE)
                {
                    if (IsPerCasterAuraState)
                    {
                        // IsPerCasterAuraState set if related pet caster aura
                        // state set already
                        if (((Unit*)this)
                                ->HasAuraStateForCaster(AURA_STATE_CONFLAGRATE,
                                    target->GetObjectGuid()))
                            *data << m_uint32Values[index];
                        else
                            *data << (m_uint32Values[index] &
                                      ~(1 << (AURA_STATE_CONFLAGRATE - 1)));
                    }
                    else
                        *data << m_uint32Values[index];
                }
                // FIXME: Some values at server stored in float format but must
                // be sent to client in uint32 format
                else if (index >= UNIT_FIELD_BASEATTACKTIME &&
                         index <= UNIT_FIELD_RANGEDATTACKTIME)
                {
                    // convert from float to uint32 and send
                    *data << uint32(
                        m_floatValues[index] < 0 ? 0 : m_floatValues[index]);
                }

                // there are some float values which may be negative or can't
                // get negative due to other checks
                else if ((index >= UNIT_FIELD_NEGSTAT0 &&
                             index <= UNIT_FIELD_NEGSTAT4) ||
                         (index >= UNIT_FIELD_RESISTANCEBUFFMODSPOSITIVE &&
                             index <=
                                 (UNIT_FIELD_RESISTANCEBUFFMODSPOSITIVE + 6)) ||
                         (index >= UNIT_FIELD_RESISTANCEBUFFMODSNEGATIVE &&
                             index <=
                                 (UNIT_FIELD_RESISTANCEBUFFMODSNEGATIVE + 6)) ||
                         (index >= UNIT_FIELD_POSSTAT0 &&
                             index <= UNIT_FIELD_POSSTAT4))
                {
                    *data << uint32(m_floatValues[index]);
                }

                // Gamemasters should be always able to select units - remove
                // not selectable flag
                else if (index == UNIT_FIELD_FLAGS && target->isGameMaster())
                {
                    *data << (m_uint32Values[index] &
                              ~UNIT_FLAG_NOT_SELECTABLE);
                }
                // Set lootable status and tap status for allowed and unallowed
                // players
                else if (index == UNIT_DYNAMIC_FLAGS &&
                         GetTypeId() == TYPEID_UNIT)
                {
                    /* === LOOTING === */
                    // FIXME: This is a hack to get around the fact that mangos
                    // do not consider
                    // dynamic flags per player. We simply get around that by
                    // evaluating it for
                    // the particular player once it's built, and overwrite what
                    // would've been sent
                    // This should be implemented properly, as right now a
                    // work-around to force
                    // push flags to a certain player is required. (See:
                    // ForceUpdateDynflagForPlayer())
                    uint32 moddedIndex = m_uint32Values[index];
                    const Creature* cThis = static_cast<const Creature*>(this);

                    // Loot status only applies when the creature is dead
                    if (cThis->isDead())
                    {
                        if (target->IsAllowedToLoot(cThis))
                            moddedIndex |= UNIT_DYNFLAG_LOOTABLE;
                        else
                            moddedIndex &= ~UNIT_DYNFLAG_LOOTABLE;
                    }
                    else
                        moddedIndex &= ~UNIT_DYNFLAG_LOOTABLE;

                    if (!HasFlag(UNIT_DYNAMIC_FLAGS, UNIT_DYNFLAG_TAPPED) ||
                        target->HasTapOn(cThis))
                        moddedIndex &= ~UNIT_DYNFLAG_TAPPED;
                    else
                        moddedIndex |= UNIT_DYNFLAG_TAPPED;

                    /* === (SPELL_AURA_MOD_STALKED) Hunter's Mark & Mind Vision
                     * === */
                    if (moddedIndex & UNIT_DYNFLAG_TRACK_UNIT)
                    {
                        auto& al =
                            cThis->GetAurasByType(SPELL_AURA_MOD_STALKED);
                        bool can_see = false;
                        for (auto a : al)
                            if (a->GetCasterGuid() == target->GetObjectGuid())
                                can_see = true;
                        if (!can_see)
                            moddedIndex &= ~UNIT_DYNFLAG_TRACK_UNIT;
                    }

                    /* SPELL_AURA_EMPATHY: Hunter's Beast Lore */
                    if (moddedIndex & UNIT_DYNFLAG_SPECIALINFO)
                    {
                        // Remove UNIT_DYNFLAG_SPECIALINFO for everyone but the
                        // owner of a SPELL_AURA_EMPATHY on *this
                        if (const_cast<Creature*>(cThis)->get_aura(
                                SPELL_AURA_EMPATHY, target->GetObjectGuid()) ==
                            nullptr)
                            moddedIndex &= ~UNIT_DYNFLAG_SPECIALINFO;
                    }

                    *data << moddedIndex;
                }
                // Health and Max Health should be sent as percentage for
                // anything but members of your parties
                else if (index == UNIT_FIELD_HEALTH ||
                         index == UNIT_FIELD_MAXHEALTH)
                {
                    bool see_exact = false;
                    if (GetTypeId() == TYPEID_PLAYER)
                    {
                        const Player* p = static_cast<const Player*>(this);
                        if (p == target || (p->GetGroup() &&
                                               p->GetGroup()->IsMember(
                                                   target->GetObjectGuid())))
                            see_exact = true;
                    }
                    else if (const Player* owner =
                                 static_cast<const Creature*>(this)
                                     ->GetCharmerOrOwnerPlayerOrPlayerItself())
                    {
                        // If the pet is our pet, or if it's owner by someone in
                        // our group, we can see the exact value
                        const Group* g;
                        if (owner == target ||
                            ((g = static_cast<const Player*>(owner)
                                      ->GetGroup()) != nullptr &&
                                g->IsMember(target->GetObjectGuid())))
                            see_exact = true;
                    }

                    // Hunter's Beast Lore
                    if (GetTypeId() == TYPEID_UNIT &&
                        const_cast<Creature*>(
                            static_cast<const Creature*>(this))
                                ->get_aura(SPELL_AURA_EMPATHY,
                                    target->GetObjectGuid()) != nullptr)
                    {
                        see_exact = true;
                    }

                    uint32 max = static_cast<const Unit*>(this)->GetMaxHealth();
                    uint32 curr = static_cast<const Unit*>(this)->GetHealth();

                    if (!see_exact)
                    {
                        curr = (static_cast<float>(curr) / max) * 100;
                        if (static_cast<const Unit*>(this)->GetHealth() == 0)
                            curr = 0;
                        else if (curr == 0)
                            curr = 1; // Need to be 1% until the mob is actually
                                      // dead

                        max = 100;
                    }

                    if (index == UNIT_FIELD_HEALTH)
                        *data << curr;
                    else
                        *data << max;
                }
                else
                {
                    // send in current format (float as float, uint32 as uint32)
                    *data << m_uint32Values[index];
                }
            }
        }
    }
    else if (isType(TYPEMASK_GAMEOBJECT)) // gameobject case
    {
        for (uint16 index = 0; index < m_valuesCount; ++index)
        {
            if (updateMask->GetBit(index))
            {
                // send in current format (float as float, uint32 as uint32)
                if (index == GAMEOBJECT_DYN_FLAGS)
                {
                    // GAMEOBJECT_TYPE_DUNGEON_DIFFICULTY can have lo flag = 2
                    //      most likely related to "can enter map" and then
                    //      should be 0 if can not enter

                    if (IsActivateToQuest)
                    {
                        switch (((GameObject*)this)->GetGoType())
                        {
                        case GAMEOBJECT_TYPE_QUESTGIVER:
                            *data << uint16(GO_DYNFLAG_LO_ACTIVATE);
                            *data << uint16(0);
                            break;
                        case GAMEOBJECT_TYPE_CHEST:
                        case GAMEOBJECT_TYPE_GENERIC:
                        case GAMEOBJECT_TYPE_SPELL_FOCUS:
                        case GAMEOBJECT_TYPE_GOOBER:
                            *data << uint16(
                                GO_DYNFLAG_LO_ACTIVATE | GO_DYNFLAG_LO_SPARKLE);
                            *data << uint16(0);
                            break;
                        default:
                            *data << uint32(0); // unknown, not happen.
                            break;
                        }
                    }
                    else
                        *data << uint32(0); // disable quest object
                }
                else
                    *data << m_uint32Values[index]; // other cases
            }
        }
    }
    else // other objects case (no special index checks)
    {
        for (uint16 index = 0; index < m_valuesCount; ++index)
        {
            if (updateMask->GetBit(index))
            {
                // send in current format (float as float, uint32 as uint32)
                *data << m_uint32Values[index];
            }
        }
    }

    if (unset_dyn_bit)
        updateMask->UnsetBit(UNIT_DYNAMIC_FLAGS);
}

void Object::ClearUpdateMask(bool remove)
{
    if (_changedFields)
        memset(_changedFields, 0, m_valuesCount * sizeof(bool));

    if (m_objectUpdated)
    {
        if (remove)
            RemoveFromClientUpdateList();
        m_objectUpdated = false;
    }
}

bool Object::LoadValues(const char* data)
{
    if (!m_uint32Values)
        _InitValues();

    Tokens tokens = StrSplit(data, " ");

    if (tokens.size() != m_valuesCount)
        return false;

    Tokens::iterator iter;
    int index;
    for (iter = tokens.begin(), index = 0; index < m_valuesCount;
         ++iter, ++index)
    {
        m_uint32Values[index] = atol((*iter).c_str());
        _changedFields[index] = true;
    }

    return true;
}

void Object::_SetUpdateBits(UpdateMask* updateMask, Player* /*target*/) const
{
    bool* indexes = _changedFields;
    for (uint16 index = 0; index < m_valuesCount; ++index, ++indexes)
    {
        if (*indexes)
            updateMask->SetBit(index);
    }
}

void Object::_SetCreateBits(UpdateMask* updateMask, Player* /*target*/) const
{
    for (uint16 index = 0; index < m_valuesCount; ++index)
    {
        if (GetUInt32Value(index) != 0)
            updateMask->SetBit(index);
    }
}

void Object::SetInt32Value(uint16 index, int32 value)
{
    assert(index < m_valuesCount || PrintIndexError(index, true));

    if (m_int32Values[index] != value)
    {
        m_int32Values[index] = value;
        _changedFields[index] = true;
        MarkForClientUpdate();
    }
}

void Object::SetUInt32Value(uint16 index, uint32 value)
{
    assert(index < m_valuesCount || PrintIndexError(index, true));

    if (m_uint32Values[index] != value)
    {
        m_uint32Values[index] = value;
        _changedFields[index] = true;
        MarkForClientUpdate();
    }
}

void Object::SetUInt64Value(uint16 index, const uint64& value)
{
    assert(index + 1 < m_valuesCount || PrintIndexError(index, true));
    if (*((uint64*)&(m_uint32Values[index])) != value)
    {
        m_uint32Values[index] = *((uint32*)&value);
        m_uint32Values[index + 1] = *(((uint32*)&value) + 1);
        _changedFields[index] = true;
        _changedFields[index + 1] = true;
        MarkForClientUpdate();
    }
}

void Object::SetFloatValue(uint16 index, float value)
{
    assert(index < m_valuesCount || PrintIndexError(index, true));

    if (m_floatValues[index] != value)
    {
        m_floatValues[index] = value;
        _changedFields[index] = true;
        MarkForClientUpdate();
    }
}

void Object::SetByteValue(uint16 index, uint8 offset, uint8 value)
{
    assert(index < m_valuesCount || PrintIndexError(index, true));

    if (offset > 4)
    {
        logging.error("Object::SetByteValue: wrong offset %u", offset);
        return;
    }

    if (uint8(m_uint32Values[index] >> (offset * 8)) != value)
    {
        m_uint32Values[index] &= ~uint32(uint32(0xFF) << (offset * 8));
        m_uint32Values[index] |= uint32(uint32(value) << (offset * 8));
        _changedFields[index] = true;
        MarkForClientUpdate();
    }
}

void Object::SetUInt16Value(uint16 index, uint8 offset, uint16 value)
{
    assert(index < m_valuesCount || PrintIndexError(index, true));

    if (offset > 2)
    {
        logging.error("Object::SetUInt16Value: wrong offset %u", offset);
        return;
    }

    if (uint16(m_uint32Values[index] >> (offset * 16)) != value)
    {
        m_uint32Values[index] &= ~uint32(uint32(0xFFFF) << (offset * 16));
        m_uint32Values[index] |= uint32(uint32(value) << (offset * 16));
        _changedFields[index] = true;
        MarkForClientUpdate();
    }
}

void Object::SetStatFloatValue(uint16 index, float value)
{
    if (value < 0)
        value = 0.0f;

    SetFloatValue(index, value);
}

void Object::SetStatInt32Value(uint16 index, int32 value)
{
    if (value < 0)
        value = 0;

    SetUInt32Value(index, uint32(value));
}

void Object::ApplyModUInt32Value(uint16 index, int32 val, bool apply)
{
    int32 cur = GetUInt32Value(index);
    cur += (apply ? val : -val);
    if (cur < 0)
        cur = 0;
    SetUInt32Value(index, cur);
}

void Object::ApplyModInt32Value(uint16 index, int32 val, bool apply)
{
    int32 cur = GetInt32Value(index);
    cur += (apply ? val : -val);
    SetInt32Value(index, cur);
}

void Object::ApplyModSignedFloatValue(uint16 index, float val, bool apply)
{
    float cur = GetFloatValue(index);
    cur += (apply ? val : -val);
    SetFloatValue(index, cur);
}

void Object::ApplyModPositiveFloatValue(uint16 index, float val, bool apply)
{
    float cur = GetFloatValue(index);
    cur += (apply ? val : -val);
    if (cur < 0)
        cur = 0;
    SetFloatValue(index, cur);
}

void Object::UpdateValueIndex(uint16 index)
{
    assert(index < m_valuesCount || PrintIndexError(index, true));
    _changedFields[index] = true;
    MarkForClientUpdate();
}

void Object::SetFlag(uint16 index, uint32 newFlag)
{
    assert(index < m_valuesCount || PrintIndexError(index, true));
    uint32 oldval = m_uint32Values[index];
    uint32 newval = oldval | newFlag;

    if (oldval != newval)
    {
        m_uint32Values[index] = newval;
        _changedFields[index] = true;
        MarkForClientUpdate();
    }
}

void Object::RemoveFlag(uint16 index, uint32 oldFlag)
{
    assert(index < m_valuesCount || PrintIndexError(index, true));
    uint32 oldval = m_uint32Values[index];
    uint32 newval = oldval & ~oldFlag;

    if (oldval != newval)
    {
        m_uint32Values[index] = newval;
        _changedFields[index] = true;
        MarkForClientUpdate();
    }
}

void Object::SetByteFlag(uint16 index, uint8 offset, uint8 newFlag)
{
    assert(index < m_valuesCount || PrintIndexError(index, true));

    if (offset > 4)
    {
        logging.error("Object::SetByteFlag: wrong offset %u", offset);
        return;
    }

    if (!(uint8(m_uint32Values[index] >> (offset * 8)) & newFlag))
    {
        m_uint32Values[index] |= uint32(uint32(newFlag) << (offset * 8));
        _changedFields[index] = true;
        MarkForClientUpdate();
    }
}

void Object::RemoveByteFlag(uint16 index, uint8 offset, uint8 oldFlag)
{
    assert(index < m_valuesCount || PrintIndexError(index, true));

    if (offset > 4)
    {
        logging.error("Object::RemoveByteFlag: wrong offset %u", offset);
        return;
    }

    if (uint8(m_uint32Values[index] >> (offset * 8)) & oldFlag)
    {
        m_uint32Values[index] &= ~uint32(uint32(oldFlag) << (offset * 8));
        _changedFields[index] = true;
        MarkForClientUpdate();
    }
}

void Object::SetShortFlag(uint16 index, bool highpart, uint16 newFlag)
{
    assert(index < m_valuesCount || PrintIndexError(index, true));

    if (!(uint16(m_uint32Values[index] >> (highpart ? 16 : 0)) & newFlag))
    {
        m_uint32Values[index] |= uint32(uint32(newFlag) << (highpart ? 16 : 0));
        MarkForClientUpdate();
    }
}

void Object::RemoveShortFlag(uint16 index, bool highpart, uint16 oldFlag)
{
    assert(index < m_valuesCount || PrintIndexError(index, true));

    if (uint16(m_uint32Values[index] >> (highpart ? 16 : 0)) & oldFlag)
    {
        m_uint32Values[index] &=
            ~uint32(uint32(oldFlag) << (highpart ? 16 : 0));
        MarkForClientUpdate();
    }
}

bool Object::PrintIndexError(uint32 index, bool set) const
{
    logging.error(
        "Attempt %s nonexistent value field: %u (count: %u) for object typeid: "
        "%u type mask: %u",
        (set ? "set value to" : "get value from"), index, m_valuesCount,
        GetTypeId(), m_objectType);

    // ASSERT must fail after function call
    return false;
}

bool Object::PrintEntryError(char const* descr) const
{
    logging.error(
        "Object Type %u, Entry %u (lowguid %u) with invalid call for %s",
        GetTypeId(), GetEntry(), GetObjectGuid().GetCounter(), descr);

    // always false for continue assert fail
    return false;
}

void Object::BuildUpdateDataForPlayer(
    Player* pl, UpdateDataMapType& update_players)
{
    auto iter = update_players.find(pl);

    if (iter == update_players.end())
    {
        std::pair<UpdateDataMapType::iterator, bool> p = update_players.insert(
            UpdateDataMapType::value_type(pl, UpdateData()));
        assert(p.second);
        iter = p.first;
    }

    BuildValuesUpdateBlockForPlayer(&iter->second, iter->first);
}

void Object::AddToClientUpdateList()
{
    logging.error(
        "Unexpected call of Object::AddToClientUpdateList for object (TypeId: "
        "%u Update fields: %u)",
        GetTypeId(), m_valuesCount);
    assert(false);
}

void Object::RemoveFromClientUpdateList()
{
    logging.error(
        "Unexpected call of Object::RemoveFromClientUpdateList for object "
        "(TypeId: %u Update fields: %u)",
        GetTypeId(), m_valuesCount);
    assert(false);
}

void Object::BuildUpdateData(UpdateDataMapType& /*update_players */)
{
    logging.error(
        "Unexpected call of Object::BuildUpdateData for object (TypeId: %u "
        "Update fields: %u)",
        GetTypeId(), m_valuesCount);
    assert(false);
}

void Object::MarkForClientUpdate()
{
    if (m_inWorld)
    {
        if (!m_objectUpdated)
        {
            AddToClientUpdateList();
            m_objectUpdated = true;
        }
    }
}

void Object::ForceUpdateDynflag()
{
    m_forceDynForEveryone = true;
    SendForcedObjectUpdate(true);
    m_forceDynForEveryone = false;
}

void Object::ForceUpdateDynflagForPlayer(Player* plr)
{
    assert(GetTypeId() == TYPEID_UNIT);

    // FIXME: This is a hack
    // Mangos does not support _changedFields for different players, in other
    // words
    // there's no way to have per-player flags. We resolve this by calculating
    // the
    // resulting flag for UNIT_DYNAMIC_FLAGS for the player that calls the
    // BuildValuesUpdate()
    // function. This is a less than ideal work-around until the underlying
    // issue can be
    // resolved. This function acts as a way to "force" an update for that
    // particular player,
    // in case the player's dynamic flags for the mob has been changed, but not
    // the Objects.
    m_forceUpdateDynflags.insert(plr->GetObjectGuid());
    if (plr->GetSession())
    {
        UpdateData d;
        BuildValuesUpdateBlockForPlayer(&d, plr);
        d.SendPacket(plr->GetSession());
    }
}

WorldObject::WorldObject()
  : cached_zone_(0), cached_area_(0), m_currMap(nullptr), m_mapId(0),
    m_InstanceId(0), m_losHeight(2.0f), m_isActiveObject(false),
    m_transport(nullptr), pending_map_insert(false)
{
}

void WorldObject::CleanupsBeforeDelete()
{
    RemoveFromWorld();

    if (Transport* transport = GetTransport())
        transport->RemovePassenger(this);
}

void WorldObject::_Create(uint32 guidlow, HighGuid guidhigh)
{
    Object::_Create(guidlow, 0, guidhigh);
}

void WorldObject::Relocate(float x, float y, float z)
{
    m_position.x = x;
    m_position.y = y;
    m_position.z = z;

    if (isType(TYPEMASK_UNIT))
        ((Unit*)this)->m_movementInfo.pos.Set(x, y, z, GetO());

    if (m_currMap)
        UpdateZoneAreaCache();
}

void WorldObject::SetOrientation(float orientation)
{
    m_position.o = orientation;

    if (isType(TYPEMASK_UNIT))
        ((Unit*)this)->m_movementInfo.pos.o = orientation;
}

void WorldObject::UpdateZoneAreaCache()
{
    GetTerrain()->GetZoneAndAreaId(
        cached_zone_, cached_area_, m_position.x, m_position.y, m_position.z);
}

InstanceData* WorldObject::GetInstanceData() const
{
    return GetMap()->GetInstanceData();
}

// slow
float WorldObject::GetDistance(const WorldObject* obj) const
{
    float dx = GetX() - obj->GetX();
    float dy = GetY() - obj->GetY();
    float dz = GetZ() - obj->GetZ();
    float sizefactor =
        GetObjectBoundingRadius() + obj->GetObjectBoundingRadius();
    float dist = sqrt((dx * dx) + (dy * dy) + (dz * dz)) - sizefactor;
    return (dist > 0 ? dist : 0);
}

float WorldObject::GetDistance2d(float x, float y) const
{
    float dx = GetX() - x;
    float dy = GetY() - y;
    float sizefactor = GetObjectBoundingRadius();
    float dist = sqrt((dx * dx) + (dy * dy)) - sizefactor;
    return (dist > 0 ? dist : 0);
}

float WorldObject::GetDistance(float x, float y, float z) const
{
    float dx = GetX() - x;
    float dy = GetY() - y;
    float dz = GetZ() - z;
    float sizefactor = GetObjectBoundingRadius();
    float dist = sqrt((dx * dx) + (dy * dy) + (dz * dz)) - sizefactor;
    return (dist > 0 ? dist : 0);
}

float WorldObject::GetDistance2d(const WorldObject* obj) const
{
    float dx = GetX() - obj->GetX();
    float dy = GetY() - obj->GetY();
    float sizefactor =
        GetObjectBoundingRadius() + obj->GetObjectBoundingRadius();
    float dist = sqrt((dx * dx) + (dy * dy)) - sizefactor;
    return (dist > 0 ? dist : 0);
}

float WorldObject::GetDistanceZ(const WorldObject* obj) const
{
    float dz = fabs(GetZ() - obj->GetZ());
    float sizefactor =
        GetObjectBoundingRadius() + obj->GetObjectBoundingRadius();
    float dist = dz - sizefactor;
    return (dist > 0 ? dist : 0);
}

bool WorldObject::IsWithinDist3d(
    float x, float y, float z, float dist2compare, bool inclBounding) const
{
    float dx = GetX() - x;
    float dy = GetY() - y;
    float dz = GetZ() - z;
    float distsq = dx * dx + dy * dy + dz * dz;

    float sizefactor = inclBounding ? GetObjectBoundingRadius() : 0;
    float maxdist = dist2compare + sizefactor;

    return distsq < maxdist * maxdist;
}

bool WorldObject::IsWithinDist2d(
    float x, float y, float dist2compare, bool inclBounding) const
{
    float dx = GetX() - x;
    float dy = GetY() - y;
    float distsq = dx * dx + dy * dy;

    float sizefactor = inclBounding ? GetObjectBoundingRadius() : 0;
    float maxdist = dist2compare + sizefactor;

    return distsq < maxdist * maxdist;
}

bool WorldObject::_IsWithinDist(WorldObject const* obj, float dist2compare,
    bool is3D, bool inclBoundingBox) const
{
    float dx = GetX() - obj->GetX();
    float dy = GetY() - obj->GetY();
    float distsq = dx * dx + dy * dy;
    if (is3D)
    {
        float dz = GetZ() - obj->GetZ();
        distsq += dz * dz;
    }

    float maxdist = dist2compare;
    if (inclBoundingBox)
        maxdist += GetObjectBoundingRadius() + obj->GetObjectBoundingRadius();

    return distsq < maxdist * maxdist;
}

bool WorldObject::IsWithinLOSInMap(const WorldObject* obj) const
{
    if (!IsInMap(obj))
        return false;

    float x, y, z;

    if (GetTransport() || obj->GetTransport())
    {
        if (GetTransport() != obj->GetTransport())
            return false;

        obj->m_movementInfo.transport.pos.Get(x, y, z);
    }
    else
    {
        obj->GetPosition(x, y, z);
    }

    return IsWithinLOS(x, y, z + obj->GetLosHeight());
}

bool WorldObject::IsWithinLOS(float x, float y, float z) const
{
    if (IsInWorld())
    {
        if (auto transport = GetTransport())
        {
            float me_x, me_y, me_z;
            m_movementInfo.transport.pos.Get(me_x, me_y, me_z);
            return transport->IsInLineOfSight(
                G3D::Vector3(me_x, me_y, me_z + m_losHeight),
                G3D::Vector3(x, y, z));
        }

        return GetMap()->isInLineOfSight(
            GetX(), GetY(), GetZ() + m_losHeight, x, y, z);
    }

    return true;
}

bool WorldObject::IsWithinWmoLOSInMap(const WorldObject* obj) const
{
    if (!IsInMap(obj))
        return false;

    float x, y, z;

    if (GetTransport() || obj->GetTransport())
    {
        if (GetTransport() != obj->GetTransport())
            return false;

        obj->m_movementInfo.transport.pos.Get(x, y, z);
    }
    else
    {
        obj->GetPosition(x, y, z);
    }

    return IsWithinWmoLOS(x, y, z + obj->GetLosHeight());
}

bool WorldObject::IsWithinWmoLOS(float x, float y, float z) const
{
    if (IsInWorld())
    {
        if (auto transport = GetTransport())
        {
            float me_x, me_y, me_z;
            m_movementInfo.transport.pos.Get(me_x, me_y, me_z);
            return transport->IsInLineOfSight(
                G3D::Vector3(me_x, me_y, me_z + m_losHeight),
                G3D::Vector3(x, y, z));
        }

        return GetMap()->isInWmoLineOfSight(
            GetX(), GetY(), GetZ() + m_losHeight, x, y, z);
    }

    return true;
}

bool WorldObject::GetDistanceOrder(WorldObject const* obj1,
    WorldObject const* obj2, bool is3D /* = true */) const
{
    float dx1 = GetX() - obj1->GetX();
    float dy1 = GetY() - obj1->GetY();
    float distsq1 = dx1 * dx1 + dy1 * dy1;
    if (is3D)
    {
        float dz1 = GetZ() - obj1->GetZ();
        distsq1 += dz1 * dz1;
    }

    float dx2 = GetX() - obj2->GetX();
    float dy2 = GetY() - obj2->GetY();
    float distsq2 = dx2 * dx2 + dy2 * dy2;
    if (is3D)
    {
        float dz2 = GetZ() - obj2->GetZ();
        distsq2 += dz2 * dz2;
    }

    return distsq1 < distsq2;
}

bool WorldObject::IsInRange(WorldObject const* obj, float minRange,
    float maxRange, bool is3D /* = true */) const
{
    float dx = GetX() - obj->GetX();
    float dy = GetY() - obj->GetY();
    float distsq = dx * dx + dy * dy;
    if (is3D)
    {
        float dz = GetZ() - obj->GetZ();
        distsq += dz * dz;
    }

    float sizefactor =
        GetObjectBoundingRadius() + obj->GetObjectBoundingRadius();

    // check only for real range
    if (minRange > 0.0f)
    {
        float mindist = minRange + sizefactor;
        if (distsq < mindist * mindist)
            return false;
    }

    float maxdist = maxRange + sizefactor;
    return distsq < maxdist * maxdist;
}

bool WorldObject::IsInRange2d(
    float x, float y, float minRange, float maxRange) const
{
    float dx = GetX() - x;
    float dy = GetY() - y;
    float distsq = dx * dx + dy * dy;

    float sizefactor = GetObjectBoundingRadius();

    // check only for real range
    if (minRange > 0.0f)
    {
        float mindist = minRange + sizefactor;
        if (distsq < mindist * mindist)
            return false;
    }

    float maxdist = maxRange + sizefactor;
    return distsq < maxdist * maxdist;
}

bool WorldObject::IsInRange3d(
    float x, float y, float z, float minRange, float maxRange) const
{
    float dx = GetX() - x;
    float dy = GetY() - y;
    float dz = GetZ() - z;
    float distsq = dx * dx + dy * dy + dz * dz;

    float sizefactor = GetObjectBoundingRadius();

    // check only for real range
    if (minRange > 0.0f)
    {
        float mindist = minRange + sizefactor;
        if (distsq < mindist * mindist)
            return false;
    }

    float maxdist = maxRange + sizefactor;
    return distsq < maxdist * maxdist;
}

float WorldObject::GetAngle(const WorldObject* obj) const
{
    if (!obj)
        return 0.0f;

    // Rework the assert, when more cases where such a call can happen have been
    // fixed
    // assert(obj != this || PrintEntryError("GetAngle (for self)"));
    if (obj == this)
    {
        logging.error(
            "INVALID CALL for GetAngle for %s", obj->GetGuidStr().c_str());
        return 0.0f;
    }
    return GetAngle(obj->GetX(), obj->GetY());
}

// Return angle in range 0..2*pi
float WorldObject::GetAngle(const float x, const float y) const
{
    float dx = x - GetX();
    float dy = y - GetY();

    float ang = atan2(dy, dx); // returns value between -Pi..Pi
    ang = (ang >= 0) ? ang : 2 * M_PI_F + ang;
    return ang;
}

bool WorldObject::HasInArc(const float arcangle, const WorldObject* obj) const
{
    // always have self in arc
    if (obj == this)
        return true;

    float arc = arcangle;

    // move arc to range 0.. 2*pi
    arc = MapManager::NormalizeOrientation(arc);

    float angle = GetAngle(obj);
    angle -= m_position.o;

    // move angle to range -pi ... +pi
    angle = MapManager::NormalizeOrientation(angle);
    if (angle > M_PI_F)
        angle -= 2.0f * M_PI_F;

    float lborder = -1 * (arc / 2.0f); // in range -pi..0
    float rborder = (arc / 2.0f);      // in range 0..pi
    return ((angle >= lborder) && (angle <= rborder));
}

bool WorldObject::HasInArc(const float arcangle, const float posAngle) const
{
    float arc = arcangle, angle = posAngle;

    // move arc to range 0.. 2*pi
    arc = MapManager::NormalizeOrientation(arc);

    angle -= m_position.o;

    // move angle to range -pi ... +pi
    angle = MapManager::NormalizeOrientation(angle);
    if (angle > M_PI_F)
        angle -= 2.0f * M_PI_F;

    float lborder = -1 * (arc / 2.0f); // in range -pi..0
    float rborder = (arc / 2.0f);      // in range 0..pi
    return ((angle >= lborder) && (angle <= rborder));
}

bool WorldObject::isInFrontInMap(
    WorldObject const* target, float distance, float arc) const
{
    return IsWithinDistInMap(target, distance) && HasInArc(arc, target);
}

bool WorldObject::isInBackInMap(
    WorldObject const* target, float distance, float arc) const
{
    return IsWithinDistInMap(target, distance) &&
           !HasInArc(2 * M_PI_F - arc, target);
}

bool WorldObject::isInFront(
    WorldObject const* target, float distance, float arc) const
{
    return IsWithinDist(target, distance) && HasInArc(arc, target);
}

bool WorldObject::isInBack(
    WorldObject const* target, float distance, float arc) const
{
    return IsWithinDist(target, distance) &&
           !HasInArc(2 * M_PI_F - arc, target);
}

void WorldObject::UpdateGroundPositionZ(float x, float y, float& z) const
{
    float new_z = GetMap()->GetHeight(x, y, z);
    if (new_z > INVALID_HEIGHT)
        z = new_z + 0.05f; // just to be sure that we are not a few pixel under
                           // the surface
}

void WorldObject::UpdateAllowedPositionZ(float x, float y, float& z) const
{
    switch (GetTypeId())
    {
    case TYPEID_UNIT:
    {
        // non fly unit don't must be in air
        // non swim unit must be at ground (mostly speedup, because it don't
        // must be in water and water level check less fast
        if (!((Creature const*)this)->CanFly())
        {
            bool canSwim = ((Creature const*)this)->CanSwim();
            float ground_z = z;
            float max_z =
                canSwim ?
                    GetTerrain()->GetWaterOrGroundLevel(x, y, z, &ground_z,
                        !((Unit const*)this)
                             ->HasAuraType(SPELL_AURA_WATER_WALK)) :
                    ((ground_z = GetMap()->GetHeight(x, y, z)));
            if (max_z > INVALID_HEIGHT)
            {
                if (z > max_z)
                    z = max_z;
                else if (z < ground_z)
                    z = ground_z;
            }
        }
        else
        {
            float ground_z = GetMap()->GetHeight(x, y, z);
            if (z < ground_z)
                z = ground_z;
        }
        break;
    }
    case TYPEID_PLAYER:
    {
        // for server controlled moves player work same as creature (but it can
        // always swim)
        if (!((Player const*)this)->CanFly())
        {
            float ground_z = z;
            float max_z =
                GetTerrain()->GetWaterOrGroundLevel(x, y, z, &ground_z,
                    !((Unit const*)this)->HasAuraType(SPELL_AURA_WATER_WALK));
            if (max_z > INVALID_HEIGHT)
            {
                if (z > max_z)
                    z = max_z;
                else if (z < ground_z)
                    z = ground_z;
            }
        }
        else
        {
            float ground_z = GetMap()->GetHeight(x, y, z);
            if (z < ground_z)
                z = ground_z;
        }
        break;
    }
    default:
    {
        float ground_z = GetMap()->GetHeight(x, y, z);
        if (ground_z > INVALID_HEIGHT)
            z = ground_z;
        break;
    }
    }
}

bool WorldObject::IsPositionValid() const
{
    return maps::verify_coords(m_position.x, m_position.y);
}

void WorldObject::MonsterSayF(
    const char* format, uint32 language, Unit* target, ...)
{
    va_list args;

    va_start(args, target);
    int size = vsnprintf(nullptr, 0, format, args);
    va_end(args);

    if (size <= 0)
        return;

    auto buffer = new char[size + 1];
    va_start(args, target);
    vsnprintf(buffer, size + 1, format, args);
    va_end(args);

    MonsterSay(buffer, language, target);
    delete buffer;
}

void WorldObject::MonsterSay(const char* text, uint32 language, Unit* target)
{
    WorldPacket data(SMSG_MESSAGECHAT, 200);
    BuildMonsterChat(&data, GetObjectGuid(), CHAT_MSG_MONSTER_SAY, text,
        language, GetName(), target ? target->GetObjectGuid() : ObjectGuid(),
        target ? target->GetName() : "");
    SendMessageToSetInRange(&data,
        sWorld::Instance()->getConfig(CONFIG_FLOAT_LISTEN_RANGE_SAY), true);
}

void WorldObject::MonsterYell(const char* text, uint32 language, Unit* target)
{
    WorldPacket data(SMSG_MESSAGECHAT, 200);
    BuildMonsterChat(&data, GetObjectGuid(), CHAT_MSG_MONSTER_YELL, text,
        language, GetName(), target ? target->GetObjectGuid() : ObjectGuid(),
        target ? target->GetName() : "");
    SendMessageToSetInRange(&data,
        sWorld::Instance()->getConfig(CONFIG_FLOAT_LISTEN_RANGE_YELL), true);
}

void WorldObject::MonsterTextEmote(
    const char* text, Unit* target, bool IsBossEmote)
{
    WorldPacket data(SMSG_MESSAGECHAT, 200);
    BuildMonsterChat(&data, GetObjectGuid(),
        IsBossEmote ? CHAT_MSG_RAID_BOSS_EMOTE : CHAT_MSG_MONSTER_EMOTE, text,
        LANG_UNIVERSAL, GetName(),
        target ? target->GetObjectGuid() : ObjectGuid(),
        target ? target->GetName() : "");
    SendMessageToSetInRange(&data,
        sWorld::Instance()->getConfig(IsBossEmote ?
                                          CONFIG_FLOAT_LISTEN_RANGE_YELL :
                                          CONFIG_FLOAT_LISTEN_RANGE_TEXTEMOTE),
        true);
}

void WorldObject::MonsterWhisper(
    const char* text, Unit* target, bool IsBossWhisper)
{
    if (!target || target->GetTypeId() != TYPEID_PLAYER)
        return;

    WorldPacket data(SMSG_MESSAGECHAT, 200);
    BuildMonsterChat(&data, GetObjectGuid(),
        IsBossWhisper ? CHAT_MSG_RAID_BOSS_WHISPER : CHAT_MSG_MONSTER_WHISPER,
        text, LANG_UNIVERSAL, GetName(), target->GetObjectGuid(),
        target->GetName());
    ((Player*)target)->GetSession()->send_packet(std::move(data));
}

namespace MaNGOS
{
class MonsterChatBuilder
{
public:
    MonsterChatBuilder(WorldObject const& obj, ChatMsg msgtype, int32 textId,
        uint32 language, Unit* target)
      : i_object(obj), i_msgtype(msgtype), i_textId(textId),
        i_language(language), i_target(target)
    {
    }
    void operator()(WorldPacket& data, int32 loc_idx)
    {
        char const* text =
            sObjectMgr::Instance()->GetMangosString(i_textId, loc_idx);

        WorldObject::BuildMonsterChat(&data, i_object.GetObjectGuid(),
            i_msgtype, text, i_language, i_object.GetNameForLocaleIdx(loc_idx),
            i_target ? i_target->GetObjectGuid() : ObjectGuid(),
            i_target ? i_target->GetNameForLocaleIdx(loc_idx) : "");
    }

private:
    WorldObject const& i_object;
    ChatMsg i_msgtype;
    int32 i_textId;
    uint32 i_language;
    Unit* i_target;
};
} // namespace MaNGOS

void WorldObject::MonsterSay(int32 textId, uint32 language, Unit* target)
{
    float range = sWorld::Instance()->getConfig(CONFIG_FLOAT_LISTEN_RANGE_SAY);
    MaNGOS::MonsterChatBuilder say_build(
        *this, CHAT_MSG_MONSTER_SAY, textId, language, target);
    auto say_do = maps::callbacks::make_localize_packet(say_build);
    maps::visitors::camera_owner{}(this, range, say_do);
}

void WorldObject::MonsterYell(int32 textId, uint32 language, Unit* target)
{
    float range = sWorld::Instance()->getConfig(CONFIG_FLOAT_LISTEN_RANGE_YELL);
    MaNGOS::MonsterChatBuilder say_build(
        *this, CHAT_MSG_MONSTER_YELL, textId, language, target);
    auto say_do = maps::callbacks::make_localize_packet(say_build);
    maps::visitors::camera_owner{}(this, range, say_do);
}

void WorldObject::MonsterYellToZone(int32 textId, uint32 language, Unit* target)
{
    MaNGOS::MonsterChatBuilder say_build(
        *this, CHAT_MSG_MONSTER_YELL, textId, language, target);
    auto say_do = maps::callbacks::make_localize_packet(say_build);

    uint32 zoneid = GetZoneId();

    Map::PlayerList const& pList = GetMap()->GetPlayers();
    for (const auto& elem : pList)
        if (elem.getSource()->GetZoneId() == zoneid)
            say_do(elem.getSource());
}

void WorldObject::MonsterTextEmote(int32 textId, Unit* target, bool IsBossEmote)
{
    float range = sWorld::Instance()->getConfig(
        IsBossEmote ? CONFIG_FLOAT_LISTEN_RANGE_YELL :
                      CONFIG_FLOAT_LISTEN_RANGE_TEXTEMOTE);

    MaNGOS::MonsterChatBuilder say_build(*this,
        IsBossEmote ? CHAT_MSG_RAID_BOSS_EMOTE : CHAT_MSG_MONSTER_EMOTE, textId,
        LANG_UNIVERSAL, target);
    auto say_do = maps::callbacks::make_localize_packet(say_build);
    maps::visitors::camera_owner{}(this, range, say_do);
}

void WorldObject::MonsterWhisper(int32 textId, Unit* target, bool IsBossWhisper)
{
    if (!target || target->GetTypeId() != TYPEID_PLAYER)
        return;

    uint32 loc_idx = ((Player*)target)->GetSession()->GetSessionDbLocaleIndex();
    char const* text = sObjectMgr::Instance()->GetMangosString(textId, loc_idx);

    WorldPacket data(SMSG_MESSAGECHAT, 200);
    BuildMonsterChat(&data, GetObjectGuid(),
        IsBossWhisper ? CHAT_MSG_RAID_BOSS_WHISPER : CHAT_MSG_MONSTER_WHISPER,
        text, LANG_UNIVERSAL, GetNameForLocaleIdx(loc_idx),
        target->GetObjectGuid(), "");

    ((Player*)target)->GetSession()->send_packet(std::move(data));
}

void WorldObject::BuildMonsterChat(WorldPacket* data, ObjectGuid senderGuid,
    uint8 msgtype, char const* text, uint32 language, char const* name,
    ObjectGuid targetGuid, char const* targetName)
{
    *data << uint8(msgtype);
    *data << uint32(language);
    *data << ObjectGuid(senderGuid);
    *data << uint32(0); // 2.1.0
    *data << uint32(strlen(name) + 1);
    *data << name;
    *data << ObjectGuid(targetGuid); // Unit Target
    if (targetGuid && !targetGuid.IsPlayer())
    {
        *data << uint32(strlen(targetName) + 1); // target name length
        *data << targetName;                     // target name
    }
    *data << uint32(strlen(text) + 1);
    *data << text;
    *data << uint8(0); // ChatTag
}

void WorldObject::SendMessageToSet(WorldPacket* data, bool self)
{
    // if object is in world, map for it already created!
    if (likely(IsInWorld()))
        GetMap()->broadcast_message(this, data, self);
}

void WorldObject::SendMessageToSetInRange(
    WorldPacket* data, float dist, bool self)
{
    // if object is in world, map for it already created!
    if (likely(IsInWorld()))
        GetMap()->broadcast_message(this, data, self, dist, false, true);
}

void WorldObject::SendMessageToSetExcept(WorldPacket* data, const Player* skip)
{
    // if object is in world, map for it already created!
    if (likely(IsInWorld()))
        GetMap()->broadcast_message(this, data, true, 0, false, false, skip);
}

void WorldObject::SendObjectDeSpawnAnim(ObjectGuid guid)
{
    WorldPacket data(SMSG_GAMEOBJECT_DESPAWN_ANIM, 8);
    data << ObjectGuid(guid);
    SendMessageToSet(&data, true);
}

void WorldObject::SendGameObjectCustomAnim(
    ObjectGuid guid, uint32 animId /*= 0*/)
{
    WorldPacket data(SMSG_GAMEOBJECT_CUSTOM_ANIM, 8 + 4);
    data << ObjectGuid(guid);
    data << uint32(animId);
    SendMessageToSet(&data, true);
}

void WorldObject::SetMap(Map* map)
{
    assert(map);
    m_currMap = map;
    // lets save current map's Id/instanceId
    m_mapId = map->GetId();
    m_InstanceId = map->GetInstanceId();
}

TerrainInfo const* WorldObject::GetTerrain() const
{
    assert(m_currMap);
    return m_currMap->GetTerrain();
}

void WorldObject::AddObjectToRemoveList()
{
    GetMap()->AddObjectToRemoveList(this);
}

Creature* WorldObject::SummonCreature(uint32 id, float x, float y, float z,
    float ang, TempSummonType spwtype, uint32 despwtime, uint32 summon_options,
    int level)
{
    CreatureInfo const* cinfo = ObjectMgr::GetCreatureTemplate(id);
    if (!cinfo)
    {
        logging.error(
            "WorldObject::SummonCreature: Creature (Entry: %u) not existed for "
            "summoner: %s. ",
            id, GetGuidStr().c_str());
        return nullptr;
    }

    auto pCreature = new TemporarySummon(GetObjectGuid(), summon_options);

    Team team = TEAM_NONE;
    if (GetTypeId() == TYPEID_PLAYER)
        team = ((Player*)this)->GetTeam();

    CreatureCreatePos pos(GetMap(), x, y, z, ang);

    if (x == 0.0f && y == 0.0f && z == 0.0f)
        pos = CreatureCreatePos(this, GetO(), CONTACT_DISTANCE, ang);

    if (!pCreature->Create(GetMap()->GenerateLocalLowGuid(cinfo->GetHighGuid()),
            pos, cinfo, team))
    {
        delete pCreature;
        return nullptr;
    }

    if (level > 0)
        pCreature->SetLevel(level);

    pCreature->SetSummonPoint(pos);

    // Active state set before added to map
    if (summon_options & SUMMON_OPT_ACTIVE)
        pCreature->SetActiveObjectState(true);

    if (!pCreature->Summon(spwtype, despwtime))
    {
        delete pCreature;
        return nullptr;
    }

    auto owner_guid = GetObjectGuid();
    pCreature->queue_action(0, [pCreature, owner_guid]()
        {
            auto caster = pCreature->GetMap()->GetWorldObject(owner_guid);
            if (!caster)
                return;
            if (caster->GetTypeId() == TYPEID_UNIT &&
                static_cast<Creature*>(caster)->AI())
                static_cast<Creature*>(caster)->AI()->JustSummoned(pCreature);
            if (pCreature->AI())
                pCreature->AI()->SummonedBy(caster);
        });

    if ((summon_options & SUMMON_OPT_NOT_COMBAT_SUMMON) == 0 &&
        GetTypeId() == TYPEID_UNIT &&
        static_cast<Creature*>(this)->isInCombat())
    {
        static_cast<Creature*>(this)->combat_summons.push_back(
            pCreature->GetObjectGuid());
    }

    // return the creature therewith the summoner has access to it
    return pCreature;
}

GameObject* WorldObject::SummonGameObject(uint32 entry, float x, float y,
    float z, float ang, float rotation0, float rotation1, float rotation2,
    float rotation3, uint32 respawnTime, bool summonWild)
{
    if (!IsInWorld())
        return nullptr;

    GameObjectInfo const* goinfo =
        sObjectMgr::Instance()->GetGameObjectInfo(entry);
    if (!goinfo)
    {
        logging.error("Gameobject template %u not found in database!", entry);
        return nullptr;
    }
    Map* map = GetMap();
    auto go = new GameObject();
    if (!go->Create(map->GenerateLocalLowGuid(HIGHGUID_GAMEOBJECT), entry, map,
            x, y, z, ang, rotation0, rotation1, rotation2, rotation3, 100,
            GO_STATE_READY))
    {
        delete go;
        return nullptr;
    }

    go->SetRespawnTime(respawnTime);
    go->SetTemporary(true);

    if (!summonWild &&
        (GetTypeId() == TYPEID_PLAYER || GetTypeId() == TYPEID_UNIT))
        ((Unit*)this)->AddGameObject(go);
    else
        go->SetSpawnedByDefault(false);

    map->insert(go);

    return go;
}

Creature* WorldObject::FindNearestCreature(
    uint32 entry, float range, bool alive) const
{
    return maps::visitors::yield_best_match<Creature>{}(
        this, range, maps::checks::entry_guid{entry, 0, this, alive});
}

GameObject* WorldObject::FindNearestGameObject(uint32 entry, float range) const
{
    return maps::visitors::yield_best_match<GameObject>{}(
        this, range, maps::checks::entry_guid{entry, 0});
}

Player* WorldObject::FindNearestPlayer(float dist) const
{
    float range = dist;
    return maps::visitors::yield_best_match<Player>{}(this, dist,
        [this, range](Player* p) mutable
        {
            if (p->isAlive() && !p->isGameMaster() &&
                IsWithinDistInMap(p, range))
            {
                range = GetDistance(p);
                return true;
            }
            return false;
        });
}

G3D::Vector2 WorldObject::GetPoint2d(float rel_ori, float dist) const
{
    rel_ori += GetO();

    G3D::Vector2 pos;
    pos.x = GetX() + dist * cos(rel_ori);
    pos.y = GetY() + dist * sin(rel_ori);

    return pos;
}

G3D::Vector3 WorldObject::GetPoint(float rel_ori, float dist, bool normalize_z,
    bool extended_z, bool local) const
{
    G3D::Vector3 pos;
    if (GetTransport())
    {
        m_movementInfo.transport.pos.Get(pos.x, pos.y, pos.z);
        rel_ori += m_movementInfo.transport.pos.o;
    }
    else
    {
        GetPosition(pos.x, pos.y, pos.z);
        rel_ori += GetO();
    }

    return GetPointXYZ(pos, rel_ori, dist, normalize_z, extended_z, local);
}

G3D::Vector3 WorldObject::GetPoint(WorldObject* obj, float dist,
    bool normalize_z, bool extended_z, bool local) const
{
    // target and *this needs to be at the same transport
    if ((GetTransport() || obj->GetTransport()) &&
        GetTransport() != obj->GetTransport())
        return GetTransport() ? G3D::Vector3(m_movementInfo.transport.pos.x,
                                    m_movementInfo.transport.pos.y,
                                    m_movementInfo.transport.pos.z) :
                                G3D::Vector3(GetX(), GetY(), GetZ());

    return GetPoint(GetAngle(obj) - GetO(),
        obj->GetObjectBoundingRadius() + GetObjectBoundingRadius() + dist,
        normalize_z, extended_z, local);
}

G3D::Vector3 WorldObject::GetPointXYZ(const G3D::Vector3& start,
    float abs_angle, float dist, bool normalize_z, bool extended_z,
    bool local) const
{
    if (!IsInWorld())
        return start;

    auto mgr = MMAP::MMapFactory::createOrGetMMapManager();

    auto trans = GetTransport();

    auto mesh = trans ? mgr->GetNavMesh(trans->m_model->name) :
                        mgr->GetNavMesh(GetMapId());
    auto query =
        trans ?
            mgr->GetNavMeshQuery(trans->m_model->name, GetInstanceId(), false) :
            mgr->GetNavMeshQuery(GetMapId(), GetInstanceId(), false);
    if (!mesh || !query)
        return start;

    bool unit = GetTypeId() == TYPEID_UNIT || GetTypeId() == TYPEID_PLAYER;
    bool water_check =
        trans == nullptr && // don't do water checks on transport
        ((normalize_z && unit) ||
            (GetTypeId() == TYPEID_UNIT &&
                reinterpret_cast<const Creature*>(this)->IsPlayerPet()));

    // Handle water points differently
    if (water_check)
    {
        auto status = static_cast<const Unit*>(this)->GetLiquidStatus(
            start.x, start.y, start.z, MAP_ALL_LIQUIDS);
        if (status != LIQUID_MAP_NO_WATER && status != LIQUID_MAP_ABOVE_WATER)
        {
            G3D::Vector3 res;
            res.x = start.x + dist * cos(abs_angle);
            res.y = start.y + dist * sin(abs_angle);
            res.z = start.z;
            float ground = GetMap()->GetHeight(res.x, res.y, res.z + 10.0f);
            if (ground > res.z)
                res.z = ground;
            // Only use point if result is in water as well
            status = static_cast<const Unit*>(this)->GetLiquidStatus(
                res.x, res.y, res.z, MAP_ALL_LIQUIDS);
            if (status != LIQUID_MAP_NO_WATER &&
                status != LIQUID_MAP_ABOVE_WATER)
                return res;
        }
    }

    auto end_point = G3D::Vector3(start.x + dist * (float)cos(abs_angle),
        start.y + dist * (float)sin(abs_angle), start.z);

    // GO models are not part of recast data, we need to manually shorten the
    // distance if a LoS blocking GO (mainly doors) is between start and end
    float collision_dist;
    if (GetMap()->get_dyn_tree()->checkCollision(collision_dist, start.x,
            start.y, start.z + 2, end_point.x, end_point.y, end_point.z + 2))
    {
        collision_dist -= 0.5f; // Margin for errors
        if (collision_dist <= 0)
            return start;
        dist = collision_dist;
        end_point = G3D::Vector3(start.x + dist * (float)cos(abs_angle),
            start.y + dist * (float)sin(abs_angle), start.z);
    }

    float curr_loc[] = {start.y, start.z, start.x};

    // findNearestPoly data
    float extents[] = {5.0f, extended_z ? 20.0f : 5.0f, 5.0f};
    dtQueryFilter filter;
    filter.setIncludeFlags(NAV_GROUND);
    dtPolyRef ref;
    float out_point[3] = {0};

    // raycast data
    float hit = 0, normal[3] = {0};
    int count = 0;
    dtPolyRef path[16] = {0};
    float end[] = {end_point.y, end_point.z, end_point.x};

    // closestPointOnPoly
    dtPolyRef end_polygon;
    float closest[3] = {0};
    bool posOverPoly;

    if (!dtStatusSucceed(query->findNearestPoly(
            curr_loc, extents, &filter, &ref, out_point)) ||
        ref == 0)
        return start;

    if (!dtStatusSucceed(query->raycast(
            ref, curr_loc, end, &filter, &hit, normal, path, &count, 16)) ||
        count <= 0 || count > 16)
        return start;

    end_polygon = path[count - 1];

    // find closest point to end on polygon
    if (!dtStatusSucceed(
            query->closestPointOnPoly(end_polygon, end, closest, &posOverPoly)))
        return start;

    G3D::Vector3 point(closest[2], closest[0], closest[1]);

    // Normalize Z value with ADT, WMO, and M2 data
    if (normalize_z)
    {
        float old_z = point.z;
        bool vmap_point = false;

        if (trans)
        {
            float prev_z = point.z;
            point.z += 4.0f;
            float height = trans->GetHeight(point);
            if (height < INVALID_HEIGHT)
            {
                point.z = height;
                vmap_point = true;
            }
            else
                point.z = prev_z;
        }
        else
        {
            auto vmgr = VMAP::VMapFactory::createOrGetVMapManager();
            float vmap_height = vmgr->getWmoHeight(
                GetMapId(), point.x, point.y, point.z + 2.0f, 8.0f);
            if (vmap_height < VMAP_INVALID_HEIGHT)
                vmap_height = vmgr->getM2Height(
                    GetMapId(), point.x, point.y, point.z + 2.0f, 8.0f);

            float grid_height = INVALID_HEIGHT_VALUE;
            if (auto grid = const_cast<TerrainInfo*>(GetMap()->GetTerrain())
                                ->GetGrid(point.x, point.y))
                grid_height = grid->getHeight(point.x, point.y);

            // WMOs can exists beneath the grid; this is not an ideal solution,
            // but we try to figure out which one to prefer.
            if ((grid_height > vmap_height && point.z + 4.0f > grid_height) ||
                (grid_height > INVALID_HEIGHT &&
                    vmap_height < VMAP_INVALID_HEIGHT))
            {
                point.z = grid_height;
            }
            else
            {
                point.z = vmap_height;
                vmap_point = true;
            }
        }

        // There might be a hole in VMAP data if old z is much higher up than
        // new z
        // FIXME: This is a hack that should be removed eventually
        if (vmap_point && old_z - 3.0f > point.z)
            point.z = old_z;
    }

    // Transfer coordinates from object space to world space if need be
    if (trans && !local)
        trans->CalculatePassengerPosition(point.x, point.y, point.z);

    return point;
}

bool WorldObject::same_floor(const WorldObject* obj) const
{
    if (obj->GetMap() != GetMap())
        return false;

    if (std::abs(obj->GetZ() - GetZ()) < 5.0f)
        return true;

    // Only applicable if either of obj or this is in a WMO
    auto at_wmo = [](const WorldObject* o)
    {
        // For players we need to actually check WMO data as they can jump and
        // do all kinds of crazy movement
        if (o->GetTypeId() == TYPEID_PLAYER)
        {
            auto vmgr = VMAP::VMapFactory::createOrGetVMapManager();
            auto height = vmgr->getWmoHeight(
                o->GetMapId(), o->GetX(), o->GetY(), o->GetZ() + 2.0f, 50.0f);
            return height > VMAP_INVALID_HEIGHT;
        }
        // For NPCs we can cheat by just doing a cheap heightmap lookup
        auto grid = const_cast<TerrainInfo*>(o->GetMap()->GetTerrain())
                        ->GetGrid(o->GetX(), o->GetY());
        if (grid &&
            std::abs(o->GetZ() - grid->getHeight(o->GetX(), o->GetY())) < 2.0f)
            return false;
        return true;
    };
    if (!at_wmo(this) && !at_wmo(obj))
        return true;

    return false;
}

void WorldObject::PlayDistanceSound(uint32 sound_id, Player* target /*= NULL*/)
{
    WorldPacket data(SMSG_PLAY_OBJECT_SOUND, 4 + 8);
    data << uint32(sound_id);
    data << GetObjectGuid();
    if (target)
        target->SendDirectMessage(std::move(data));
    else
        SendMessageToSet(&data, true);
}

void WorldObject::PlayDirectSound(uint32 sound_id, Player* target /*= NULL*/)
{
    WorldPacket data(SMSG_PLAY_SOUND, 4);
    data << uint32(sound_id);
    if (target)
        target->SendDirectMessage(std::move(data));
    else
        SendMessageToSet(&data, true);
}

void WorldObject::UpdateVisibilityAndView()
{
    GetViewPoint().Call_UpdateVisibilityForOwner();
    UpdateObjectVisibility();
    GetViewPoint().Event_ViewPointVisibilityChanged();
}

void WorldObject::UpdateObjectVisibility()
{
    GetMap()->UpdateObjectVisibility(this);
}

void WorldObject::AddToClientUpdateList()
{
    GetMap()->AddUpdateObject(this);
}

void WorldObject::RemoveFromClientUpdateList()
{
    GetMap()->RemoveUpdateObject(this);
}

void WorldObject::BuildUpdateData(UpdateDataMapType& update_players)
{
    // If player is too far away from his camera, he would not get updates, so
    // we opt for doing *this separately
    if (isType(TYPEMASK_PLAYER))
        BuildUpdateDataForPlayer((Player*)this, update_players);

    maps::visitors::simple<Camera>{}.visit_2d(this,
        GetMap()->GetVisibilityDistance(), [this, &update_players](Camera* cam)
        {
            auto owner = cam->GetOwner();
            if (owner != this && owner->HaveAtClient(this))
                BuildUpdateDataForPlayer(owner, update_players);
        });

    ClearUpdateMask(false);
}

bool WorldObject::IsControlledByPlayer() const
{
    switch (GetTypeId())
    {
    case TYPEID_GAMEOBJECT:
        return ((GameObject*)this)->GetOwnerGuid().IsPlayer();
    case TYPEID_UNIT:
    case TYPEID_PLAYER:
        return ((Unit*)this)->IsCharmerOrOwnerPlayerOrPlayerItself();
    case TYPEID_DYNAMICOBJECT:
        return ((DynamicObject*)this)->GetCasterGuid().IsPlayer();
    case TYPEID_CORPSE:
        return true;
    default:
        return false;
    }
}

bool WorldObject::PrintCoordinatesError(
    float x, float y, float z, char const* descr) const
{
    logging.error(
        "%s with invalid %s coordinates: mapid = %uu, x = %f, y = %f, z = %f",
        GetGuidStr().c_str(), descr, GetMapId(), x, y, z);
    return false; // always false for continue assert fail
}

void WorldObject::SetActiveObjectState(bool active)
{
    // Players cannot be inactive
    if (unlikely(isType(TYPEMASK_PLAYER)))
    {
        m_isActiveObject = true;
        return;
    }

    // Can only really happen if a DB script is working incorrectly
    if (unlikely(m_isActiveObject == active))
        return;

    if (IsInWorld())
    {
        if (isActiveObject() && !active)
            GetMap()->remove_active_entity(this);
        else if (!isActiveObject() && active)
            GetMap()->add_active_entity(this);
    }

    m_isActiveObject = active;
}

void Object::DeleteLootDistributor()
{
    delete m_lootDistributor;
    m_lootDistributor = nullptr;
}

void Object::resend_health()
{
    _changedFields[UNIT_FIELD_HEALTH] = true;
    _changedFields[UNIT_FIELD_MAXHEALTH] = true;
    MarkForClientUpdate();
}

void WorldObject::update_queued_actions(uint32 diff)
{
    std::vector<std::function<void()>> execute;
    for (auto itr = queued_actions_.begin(); itr != queued_actions_.end();)
    {
        if (itr->timer <= diff)
        {
            execute.push_back(itr->func);
            itr = queued_actions_.erase(itr);
        }
        else
        {
            itr->timer -= diff;
            ++itr;
        }
    }

    for (auto itr = queued_tick_actions_.begin();
         itr != queued_tick_actions_.end();)
    {
        if (itr->timer <= 1)
        {
            execute.push_back(itr->func);
            itr = queued_tick_actions_.erase(itr);
        }
        else
        {
            itr->timer -= 1;
            ++itr;
        }
    }

    for (auto& func : execute)
        func();
}
