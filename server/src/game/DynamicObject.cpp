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

#include "DynamicObject.h"
#include "Common.h"
#include "GameObject.h"
#include "ObjectAccessor.h"
#include "Opcodes.h"
#include "SpecialVisCreature.h"
#include "Totem.h"
#include "TemporarySummon.h"
#include "SpellMgr.h"
#include "UpdateMask.h"
#include "World.h"
#include "Database/DatabaseEnv.h"
#include "maps/visitors.h"

DynamicObject::DynamicObject() : WorldObject()
{
    m_objectType |= TYPEMASK_DYNAMICOBJECT;
    m_objectTypeId = TYPEID_DYNAMICOBJECT;
    // 2.3.2 - 0x58
    m_updateFlag =
        (UPDATEFLAG_LOWGUID | UPDATEFLAG_HIGHGUID | UPDATEFLAG_HAS_POSITION);

    m_valuesCount = DYNAMICOBJECT_END;
}

void DynamicObject::AddToWorld()
{
    Object::AddToWorld();
}

void DynamicObject::RemoveFromWorld()
{
    if (IsInWorld())
        GetViewPoint().Event_RemovedFromWorld();

    Object::RemoveFromWorld();
}

bool DynamicObject::Create(uint32 guidlow, Unit* caster, uint32 spellId,
    SpellEffectIndex effIndex, float x, float y, float z, int32 duration,
    float radius, DynamicObjectType type)
{
    WorldObject::_Create(guidlow, HIGHGUID_DYNAMICOBJECT);
    SetMap(caster->GetMap());
    Relocate(x, y, z);

    if (!IsPositionValid())
    {
        logging.error(
            "DynamicObject (spell %u eff %u) not created. Suggested "
            "coordinates isn't valid (X: %f Y: %f)",
            spellId, effIndex, GetX(), GetY());
        return false;
    }

    SetEntry(spellId);
    SetObjectScale(DEFAULT_OBJECT_SCALE);

    SetGuidValue(DYNAMICOBJECT_CASTER, caster->GetObjectGuid());

    /* Bytes field, so it's really 4 bit fields. These flags are unknown, but we
    do know that 0x00000001 is set for most.
       Farsight for example, does not have this flag, instead it has 0x80000002.
       Flags are set dynamically with some conditions, so one spell may have
    different flags set, depending on those conditions.
       The size of the visual may be controlled to some degree with these flags.

    uint32 bytes = 0x00000000;
    bytes |= 0x01;
    bytes |= 0x00 << 8;
    bytes |= 0x00 << 16;
    bytes |= 0x00 << 24;
    */
    SetByteValue(DYNAMICOBJECT_BYTES, 0, type);

    SetUInt32Value(DYNAMICOBJECT_SPELLID, spellId);
    SetFloatValue(DYNAMICOBJECT_RADIUS, radius);
    SetFloatValue(DYNAMICOBJECT_POS_X, x);
    SetFloatValue(DYNAMICOBJECT_POS_Y, y);
    SetFloatValue(DYNAMICOBJECT_POS_Z, z);
    SetUInt32Value(
        DYNAMICOBJECT_CASTTIME, WorldTimer::getMSTime()); // new 2.4.0

    SpellEntry const* spellProto = sSpellStore.LookupEntry(spellId);
    if (!spellProto)
    {
        logging.error(
            "DynamicObject (spell: %u x: %f y: %f) not created. Spell not "
            "exist!",
            spellId, GetX(), GetY());
        return false;
    }

    m_aliveDuration = duration;
    m_radius = radius;
    m_effIndex = effIndex;
    m_spellId = spellId;
    m_positive = IsPositiveEffect(spellProto, m_effIndex);

    return true;
}

Unit* DynamicObject::GetCaster() const
{
    // can be not found in some cases
    return ObjectAccessor::GetUnit(*this, GetCasterGuid());
}

void DynamicObject::Update(uint32 update_diff, uint32 p_time)
{
    // caster can be not in world at time dynamic object update, but dynamic
    // object not yet deleted in Unit destructor
    Unit* caster = GetCaster();
    if (!caster)
    {
        Delete();
        return;
    }

    if (unlikely(has_queued_actions()))
        update_queued_actions(update_diff);

    // have radius and work as persistent effect
    if (m_radius)
    {
        m_visitTimer.Update(update_diff);
        if (m_visitTimer.Passed())
        {
            m_visitTimer.Reset(1000);

            Unit* check = caster;
            if (auto owner = caster->GetOwner())
                check = owner;

            maps::visitors::simple<Player, Creature, Pet, SpecialVisCreature,
                TemporarySummon, Totem>{}(this, m_radius,
                [this, check](auto&& target)
                {
                    if (!target->isAlive() || target->IsTaxiFlying())
                        return;

                    if (target->GetTypeId() == TYPEID_UNIT &&
                        ((Creature*)target)->IsTotem())
                        return;

                    // XXX: Explicit this due to bug in GCC.
                    if (!this->IsWithinDistInMap(target, this->GetRadius()))
                        return;

                    // Check targets for not_selectable unit flag and remove
                    if (target->HasFlag(UNIT_FIELD_FLAGS,
                            UNIT_FLAG_NON_ATTACKABLE |
                                UNIT_FLAG_NOT_SELECTABLE |
                                UNIT_FLAG_NOT_PLAYER_ATTACKABLE))
                        return;

                    // Evade target
                    if (target->GetTypeId() == TYPEID_UNIT &&
                        ((Creature*)target)->IsInEvadeMode())
                        return;

                    // Check player targets and remove if in GM mode or GM
                    // invisibility (for not
                    // self casting case)
                    if (target->GetTypeId() == TYPEID_PLAYER &&
                        target != check &&
                        (((Player*)target)->isGameMaster() ||
                            ((Player*)target)->GetVisibility() ==
                                VISIBILITY_OFF))
                        return;

                    // for player casts use less strict negative and more
                    // stricted positive
                    // targeting
                    if (check->GetTypeId() == TYPEID_PLAYER)
                    {
                        if (check->IsFriendlyTo(target) != m_positive)
                            return;

                        // if target is player, apply extra checks for negative
                        // dyn. objects
                        if (!m_positive &&
                            target->GetCharmerOrOwnerPlayerOrPlayerItself() !=
                                nullptr)
                        {
                            // sanctuaries
                            if (auto area =
                                    GetAreaEntryByAreaID(target->GetAreaId()))
                                if (area->flags & AREA_FLAG_SANCTUARY)
                                    return;

                            // owner has pvp flag off
                            if (check->ShouldIgnoreTargetBecauseOfPvpFlag(
                                    target))
                                return;
                        }
                    }
                    else
                    {
                        if (check->IsHostileTo(target) == m_positive)
                            return;
                    }

                    // XXX: Explicit this due to bug in GCC.
                    if (this->IsAffecting(target))
                        return;

                    // XXX: Explicit this due to bug in GCC.
                    SpellEntry const* spellInfo =
                        sSpellStore.LookupEntry(this->GetSpellId());
                    SpellEffectIndex eff_index = this->GetEffIndex();

                    // XXX: Explicit this due to bug in GCC.
                    if (spellInfo->HasAttribute(
                            SPELL_ATTR_CUSTOM1_AOE_FORCE_LOS) &&
                        !this->IsWithinWmoLOSInMap(target))
                        return;

                    // Check target immune to spell or aura
                    if (target->IsImmuneToSpell(spellInfo) ||
                        target->IsImmuneToSpellEffect(spellInfo, eff_index))
                        return;

                    // Apply PersistentAreaAura on target
                    // in case 2 dynobject overlap areas for same spell, same
                    // holder is
                    // selected, so dynobjects share holder
                    // XXX: Explicit this due to bug in GCC.
                    AuraHolder* holder =
                        target->get_aura(spellInfo->Id, this->GetCasterGuid());

                    if (holder)
                    {
                        if (!holder->GetAura(eff_index))
                        {
                            // XXX: Explicit this due to bug in GCC.
                            auto Aur =
                                new PersistentAreaAura(spellInfo, eff_index,
                                    nullptr, holder, target, this->GetCaster());
                            holder->AddAura(Aur, eff_index);
                            target->AddAuraToModList(Aur);
                            Aur->ApplyModifier(true, true);
                        }
                        // XXX: Explicit this due to bug in GCC.
                        else if (holder->GetAuraDuration() >= 0 &&
                                 uint32(holder->GetAuraDuration()) <
                                     this->GetDuration())
                        {
                            // XXX: Explicit this due to bug in GCC.
                            holder->SetAuraDuration(this->GetDuration());
                            holder->UpdateAuraDuration();
                        }
                    }
                    else
                    {
                        // XXX: Explicit this due to bug in GCC.
                        holder = CreateAuraHolder(
                            spellInfo, target, this->GetCaster());
                        auto Aur = new PersistentAreaAura(spellInfo, eff_index,
                            nullptr, holder, target, this->GetCaster());
                        holder->SetAuraDuration(this->GetDuration());
                        holder->AddAura(Aur, eff_index);
                        target->AddAuraHolder(holder);

                        // Invoke combat on persistent area aura application
                        // XXX: Explicit this due to bug in GCC.
                        if (!spellInfo->HasAttribute(
                                SPELL_ATTR_CUSTOM_NO_INITIAL_BLAST))
                        {
                            if (Unit* caster = this->GetCaster())
                            {
                                if (!spellInfo->HasAttribute(
                                        SPELL_ATTR_EX3_NO_INITIAL_AGGRO) &&
                                    !spellInfo->HasAttribute(
                                        SPELL_ATTR_EX_NO_THREAT) &&
                                    !spellInfo->HasAttribute(
                                        SPELL_ATTR_EX3_DONT_AFFECT_COMBAT) &&
                                    !IsPositiveSpell(spellInfo->Id))
                                {
                                    if (!target->isInCombat())
                                        target->AttackedBy(caster);

                                    target->AddThreat(caster);
                                    target->SetInCombatWith(caster);
                                    caster->SetInCombatWith(target);
                                }

                                // do on-hit-procs if aura has full duration
                                // (basically on cast)
                                if (holder->GetAuraDuration() ==
                                    holder->GetAuraMaxDuration())
                                {
                                    caster->ProcDamageAndSpell(target,
                                        PROC_FLAG_SUCCESSFUL_NEGATIVE_SPELL_HIT |
                                            PROC_FLAG_SUCCESSFUL_AOE_SPELL_HIT,
                                        PROC_FLAG_TAKEN_NEGATIVE_SPELL_HIT |
                                            PROC_FLAG_TAKEN_AOE_SPELL_HIT,
                                        PROC_EX_NORMAL_HIT, proc_amount(true),
                                        BASE_ATTACK, holder->GetSpellProto());
                                }
                            }
                        }
                    }

                    // XXX: Explicit this due to bug in GCC.
                    this->AddAffected(target);
                });
        }
    }

    if (m_aliveDuration > int32(p_time))
    {
        m_aliveDuration -= p_time;
    }
    else
    {
        Delete();
    }
}

void DynamicObject::Delete()
{
    if (auto caster = GetCaster())
        caster->RemoveDynObjectWithGUID(GetObjectGuid());
    if (!IsInWorld())
        return;
    SendObjectDeSpawnAnim(GetObjectGuid());
    AddObjectToRemoveList();
}

void DynamicObject::Delay(int32 delaytime)
{
    m_aliveDuration -= delaytime;
    for (auto iter = m_affected.begin(); iter != m_affected.end();)
    {
        Unit* target = GetMap()->GetUnit((*iter));
        if (target)
        {
            AuraHolder* holder = target->get_aura(m_spellId, GetCasterGuid());
            if (!holder)
            {
                ++iter;
                continue;
            }

            bool foundAura = false;
            for (int32 i = m_effIndex + 1; i < MAX_EFFECT_INDEX; ++i)
            {
                if ((holder->GetSpellProto()->Effect[i] ==
                            SPELL_EFFECT_PERSISTENT_AREA_AURA ||
                        holder->GetSpellProto()->Effect[i] ==
                            SPELL_EFFECT_ADD_FARSIGHT) &&
                    holder->GetAura(SpellEffectIndex(i)))
                {
                    foundAura = true;
                    break;
                }
            }

            if (foundAura)
            {
                ++iter;
                continue;
            }

            target->DelaySpellAuraHolder(m_spellId, delaytime, GetCasterGuid());
            ++iter;
        }
        else
            m_affected.erase(iter++);
    }
}

bool DynamicObject::isVisibleForInState(
    Player const* u, WorldObject const* viewPoint, bool inVisibleList) const
{
    if (!IsInWorld() || !u->IsInWorld())
        return false;

    // always seen by owner
    if (GetCasterGuid() == u->GetObjectGuid())
        return true;

    // normal case
    return IsWithinDistInMap(viewPoint,
        GetMap()->GetVisibilityDistance() +
            (inVisibleList ? World::GetVisibleObjectGreyDistance() : 0.0f),
        false);
}

bool DynamicObject::IsHostileTo(Unit const* unit) const
{
    if (Unit* owner = GetCaster())
        return owner->IsHostileTo(unit);
    else
        return false;
}

bool DynamicObject::IsFriendlyTo(Unit const* unit) const
{
    if (Unit* owner = GetCaster())
        return owner->IsFriendlyTo(unit);
    else
        return true;
}
