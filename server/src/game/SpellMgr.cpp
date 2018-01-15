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

#include "SpellMgr.h"
#include "BattleGroundMgr.h"
#include "Chat.h"
#include "DBCStores.h"
#include "MapManager.h"
#include "ObjectMgr.h"
#include "ProgressBar.h"
#include "Spell.h"
#include "SpellAuraDefines.h"
#include "Unit.h"
#include "World.h"

SpellMgr::SpellMgr()
{
}

SpellMgr::~SpellMgr()
{
}

bool IsPartiallyResistable(const SpellEntry* spell)
{
    if (spell->HasAttribute(SPELL_ATTR_EX3_CANT_MISS) ||
        spell->HasAttribute(SPELL_ATTR_EX4_NOT_RESISTABLE))
        return false;

    uint32 school_mask = GetSpellSchoolMask(spell);
    if ((school_mask & SPELL_SCHOOL_MASK_NORMAL) != 0)
        return false;

    if (spell->DmgClass == SPELL_DAMAGE_CLASS_NONE)
        return false;

    // Having an extra aura component on the spell makes it binary
    // resistable (for the most part)
    for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        if (spell->Effect[i] != SPELL_EFFECT_APPLY_AURA)
            continue;
        switch (spell->EffectApplyAuraName[i])
        {
        // auras that force the spell to be binary
        case SPELL_AURA_MOD_CONFUSE:
        case SPELL_AURA_MOD_FEAR:
        case SPELL_AURA_MOD_ROOT:
        case SPELL_AURA_MOD_DECREASE_SPEED:
        case SPELL_AURA_PERIODIC_LEECH:
        case SPELL_AURA_PERIODIC_MANA_LEECH:
            return false;

            // NOTE: the following auras have been confirmed to still be
            // partially resistable
            // SPELL_AURA_MOD_DAMAGE_TAKEN (as seen in Leothera's chaos blast)
        }
    }

    // DoTs and direct damage are partially resistable
    if (spell->HasApplyAuraName(SPELL_AURA_PERIODIC_DAMAGE) ||
        IsDamagingSpell(spell))
        return true;

    // Everything else is binary
    return false;
}

int32 GetSpellDuration(SpellEntry const* spellInfo)
{
    if (!spellInfo)
        return 0;
    SpellDurationEntry const* du =
        sSpellDurationStore.LookupEntry(spellInfo->DurationIndex);
    if (!du)
        return 0;
    return (du->Duration[0] == -1) ? -1 : abs(du->Duration[0]);
}

int32 GetSpellMaxDuration(SpellEntry const* spellInfo)
{
    if (!spellInfo)
        return 0;
    SpellDurationEntry const* du =
        sSpellDurationStore.LookupEntry(spellInfo->DurationIndex);
    if (!du)
        return 0;
    return (du->Duration[2] == -1) ? -1 : abs(du->Duration[2]);
}

int32 CalculateSpellDuration(SpellEntry const* spellInfo, Unit const* caster)
{
    int32 duration = GetSpellDuration(spellInfo);

    if (duration != -1 && caster)
    {
        int32 maxduration = GetSpellMaxDuration(spellInfo);

        if (duration != maxduration && caster->GetTypeId() == TYPEID_PLAYER)
            duration += int32((maxduration - duration) *
                              ((Player*)caster)->GetComboPoints() / 5);

        if (Player* modOwner = caster->GetSpellModOwner())
        {
            modOwner->ApplySpellMod(spellInfo->Id, SPELLMOD_DURATION, duration);

            if (duration < 0)
                duration = 0;
        }
        if (spellInfo->HasAttribute(SPELL_ATTR_EX5_APPLIES_HASTE_BONUS))
            duration =
                int32(duration * caster->GetFloatValue(UNIT_MOD_CAST_SPEED));
    }

    return duration;
}

uint32 GetSpellCastTime(
    SpellEntry const* spellInfo, Spell const* spell, bool peak)
{
    if (spell)
    {
        // some triggered spells have data only usable for client
        if (spell->IsTriggeredSpellWithRedundentData())
            return 0;
    }

    SpellCastTimesEntry const* spellCastTimeEntry =
        sSpellCastTimesStore.LookupEntry(spellInfo->CastingTimeIndex);

    // not all spells have cast time index and this is all is pasiive abilities
    if (!spellCastTimeEntry)
        return 0;

    int32 castTime = spellCastTimeEntry->CastTime;

    if (spellInfo->HasAttribute(SPELL_ATTR_RANGED) &&
        (!spell || !spell->IsAutoRepeat()))
        castTime += 500;

    if (spell)
    {
        if (Player* modOwner = spell->GetCaster()->GetSpellModOwner())
            modOwner->ApplySpellMod(
                spellInfo->Id, SPELLMOD_CASTING_TIME, castTime, spell, peak);

        if (!spellInfo->HasAttribute(SPELL_ATTR_ABILITY) &&
            !spellInfo->HasAttribute(SPELL_ATTR_TRADESPELL))
            castTime =
                int32(castTime *
                      spell->GetCaster()->GetFloatValue(UNIT_MOD_CAST_SPEED));
        else
        {
            if (spell->IsRangedSpell() && !spell->IsAutoRepeat())
                castTime = int32(
                    castTime *
                    spell->GetCaster()->m_modAttackSpeedPct[RANGED_ATTACK]);
        }
    }

    return (castTime > 0) ? uint32(castTime) : 0;
}

uint32 GetSpellCastTimeForBonus(
    SpellEntry const* spellProto, DamageEffectType damagetype)
{
    uint32 CastingTime = !IsChanneledSpell(spellProto) ?
                             GetSpellCastTime(spellProto) :
                             GetSpellDuration(spellProto);

    if (CastingTime > 7000)
        CastingTime = 7000;
    if (CastingTime < 1500)
        CastingTime = 1500;

    if (damagetype == DOT && !IsChanneledSpell(spellProto))
        CastingTime = 3500;

    int32 overTime = 0;
    uint8 effects = 0;
    bool DirectDamage = false;
    bool AreaEffect = false;

    for (uint32 i = 0; i < MAX_EFFECT_INDEX; ++i)
        if (IsAreaEffectTarget(Targets(spellProto->EffectImplicitTargetA[i])) ||
            IsAreaEffectTarget(Targets(spellProto->EffectImplicitTargetB[i])))
            AreaEffect = true;

    for (uint32 i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        switch (spellProto->Effect[i])
        {
        case SPELL_EFFECT_SCHOOL_DAMAGE:
        case SPELL_EFFECT_POWER_DRAIN:
        case SPELL_EFFECT_HEALTH_LEECH:
        case SPELL_EFFECT_ENVIRONMENTAL_DAMAGE:
        case SPELL_EFFECT_POWER_BURN:
        case SPELL_EFFECT_HEAL:
            DirectDamage = true;
            break;
        case SPELL_EFFECT_APPLY_AURA:
            switch (spellProto->EffectApplyAuraName[i])
            {
            case SPELL_AURA_PERIODIC_DAMAGE:
            case SPELL_AURA_PERIODIC_HEAL:
            case SPELL_AURA_PERIODIC_LEECH:
                if (GetSpellDuration(spellProto))
                    overTime = GetSpellDuration(spellProto);
                break;
            // Penalty for additional effects
            case SPELL_AURA_DUMMY:
                ++effects;
                break;
            case SPELL_AURA_MOD_DECREASE_SPEED:
                ++effects;
                break;
            case SPELL_AURA_MOD_CONFUSE:
            case SPELL_AURA_MOD_STUN:
            case SPELL_AURA_MOD_ROOT:
                // -10% per effect
                effects += 2;
                break;
            default:
                break;
            }
            break;
        default:
            break;
        }
    }

    // Combined Spells with Both Over Time and Direct Damage
    if (overTime > 0 && CastingTime > 0 && DirectDamage)
    {
        // mainly for DoTs which are 3500 here otherwise
        uint32 OriginalCastTime = GetSpellCastTime(spellProto);
        if (OriginalCastTime > 7000)
            OriginalCastTime = 7000;
        if (OriginalCastTime < 1500)
            OriginalCastTime = 1500;
        // Portion to Over Time
        float PtOT = (overTime / 15000.0f) /
                     ((overTime / 15000.0f) + (OriginalCastTime / 3500.0f));

        if (damagetype == DOT)
            CastingTime = uint32(CastingTime * PtOT);
        else if (PtOT < 1.0f)
            CastingTime = uint32(CastingTime * (1 - PtOT));
        else
            CastingTime = 0;
    }

    // Area Effect Spells receive only half of bonus
    if (AreaEffect)
        CastingTime /= 2;

    // 50% for damage and healing spells for leech spells from damage bonus and
    // 0% from healing
    for (int j = 0; j < MAX_EFFECT_INDEX; ++j)
    {
        if (spellProto->Effect[j] == SPELL_EFFECT_HEALTH_LEECH ||
            (spellProto->Effect[j] == SPELL_EFFECT_APPLY_AURA &&
                spellProto->EffectApplyAuraName[j] ==
                    SPELL_AURA_PERIODIC_LEECH))
        {
            CastingTime /= 2;
            break;
        }
    }

    // -5% of total per any additional effect (multiplicative)
    for (int i = 0; i < effects; ++i)
        CastingTime *= 0.95f;

    return CastingTime;
}

uint16 GetSpellAuraMaxTicks(SpellEntry const* spellInfo)
{
    int32 DotDuration = GetSpellDuration(spellInfo);
    if (DotDuration == 0)
        return 1;

    // 200% limit
    if (DotDuration > 30000)
        DotDuration = 30000;

    for (int j = 0; j < MAX_EFFECT_INDEX; ++j)
    {
        if (spellInfo->Effect[j] == SPELL_EFFECT_APPLY_AURA &&
            (spellInfo->EffectApplyAuraName[j] == SPELL_AURA_PERIODIC_DAMAGE ||
                spellInfo->EffectApplyAuraName[j] == SPELL_AURA_PERIODIC_HEAL ||
                spellInfo->EffectApplyAuraName[j] == SPELL_AURA_PERIODIC_LEECH))
        {
            if (spellInfo->EffectAmplitude[j] != 0)
                return DotDuration / spellInfo->EffectAmplitude[j];
            break;
        }
    }

    return 6;
}

uint16 GetSpellAuraMaxTicks(uint32 spellId)
{
    SpellEntry const* spellInfo = sSpellStore.LookupEntry(spellId);
    if (!spellInfo)
    {
        logging.error("GetSpellAuraMaxTicks: Spell %u not exist!", spellId);
        return 1;
    }

    return GetSpellAuraMaxTicks(spellInfo);
}

float CalculateDefaultCoefficient(
    SpellEntry const* spellProto, DamageEffectType const damagetype)
{
    // Damage over Time spells bonus calculation
    float DotFactor = 1.0f;
    if (damagetype == DOT)
    {
        if (!IsChanneledSpell(spellProto))
            DotFactor = GetSpellDuration(spellProto) / 15000.0f;

        if (uint16 DotTicks = GetSpellAuraMaxTicks(spellProto))
            DotFactor /= DotTicks;
    }

    // Distribute Damage over multiple effects, reduce by AoE
    float coeff = GetSpellCastTimeForBonus(spellProto, damagetype) / 3500.0f;

    return coeff * DotFactor;
}

WeaponAttackType GetWeaponAttackType(SpellEntry const* spellInfo)
{
    if (!spellInfo)
        return BASE_ATTACK;

    switch (spellInfo->DmgClass)
    {
    case SPELL_DAMAGE_CLASS_MELEE:
        if (spellInfo->HasAttribute(SPELL_ATTR_EX3_REQ_OFFHAND))
            return OFF_ATTACK;
        else
            return BASE_ATTACK;
        break;
    case SPELL_DAMAGE_CLASS_RANGED:
        return RANGED_ATTACK;
        break;
    default:
        // Wands
        if (spellInfo->HasAttribute(SPELL_ATTR_EX2_AUTOREPEAT_FLAG))
            return RANGED_ATTACK;
        else
            return BASE_ATTACK;
        break;
    }
}

bool IsPassiveSpell(uint32 spellId)
{
    SpellEntry const* spellInfo = sSpellStore.LookupEntry(spellId);
    if (!spellInfo)
        return false;
    return IsPassiveSpell(spellInfo);
}

int32 CompareAuraRanks(uint32 spellId_1, uint32 spellId_2)
{
    SpellEntry const* spellInfo_1 = sSpellStore.LookupEntry(spellId_1);
    SpellEntry const* spellInfo_2 = sSpellStore.LookupEntry(spellId_2);
    if (!spellInfo_1 || !spellInfo_2)
        return 0;
    if (spellId_1 == spellId_2)
        return 0;

    for (int32 i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        if (spellInfo_1->Effect[i] != 0 && spellInfo_2->Effect[i] != 0 &&
            spellInfo_1->Effect[i] == spellInfo_2->Effect[i])
        {
            int32 diff = spellInfo_1->EffectBasePoints[i] -
                         spellInfo_2->EffectBasePoints[i];
            if (spellInfo_1->CalculateSimpleValue(SpellEffectIndex(i)) < 0 &&
                spellInfo_2->CalculateSimpleValue(SpellEffectIndex(i)) < 0)
                return -diff;
            else
                return diff;
        }
    }
    return 0;
}

bool IsPositiveTarget(uint32 targetA, uint32 targetB)
{
    switch (targetA)
    {
    // non-positive targets
    case TARGET_CHAIN_DAMAGE:
    case TARGET_ALL_ENEMY_IN_AREA:
    case TARGET_ALL_ENEMY_IN_AREA_INSTANT:
    case TARGET_IN_FRONT_OF_CASTER:
    case TARGET_ALL_ENEMY_IN_AREA_CHANNELED:
    case TARGET_CURRENT_ENEMY_COORDINATES:
    case TARGET_SINGLE_ENEMY:
        return false;
    // positive or dependent
    case TARGET_CASTER_COORDINATES:
        return (targetB == TARGET_ALL_PARTY ||
                targetB == TARGET_ALL_FRIENDLY_UNITS_AROUND_CASTER);
    default:
        break;
    }
    if (targetB)
        return IsPositiveTarget(targetB, 0);
    return true;
}

bool IsExplicitPositiveTarget(uint32 targetA)
{
    // positive targets that in target selection code expect target in
    // m_targers, so not that auto-select target by spell data by m_caster and
    // etc
    switch (targetA)
    {
    case TARGET_SINGLE_FRIEND:
    case TARGET_SINGLE_PARTY:
    case TARGET_CHAIN_HEAL:
    case TARGET_SINGLE_FRIEND_2:
    case TARGET_AREAEFFECT_PARTY_AND_CLASS:
        return true;
    default:
        break;
    }
    return false;
}

bool IsExplicitNegativeTarget(uint32 targetA)
{
    // non-positive targets that in target selection code expect target in
    // m_targers, so not that auto-select target by spell data by m_caster and
    // etc
    switch (targetA)
    {
    case TARGET_CHAIN_DAMAGE:
    case TARGET_CURRENT_ENEMY_COORDINATES:
    case TARGET_SINGLE_ENEMY:
        return true;
    default:
        break;
    }
    return false;
}

bool IsPositiveEffect(SpellEntry const* spellproto, SpellEffectIndex effIndex)
{
    if (spellproto->HasAttribute(SPELL_ATTR_CUSTOM_POSITIVE))
        return true;

    switch (spellproto->Effect[effIndex])
    {
    case SPELL_EFFECT_DUMMY:
        // some explicitly required dummy effect sets
        switch (spellproto->Id)
        {
        case 28441: // AB Effect 000
            return false;
        default:
            break;
        }
        break;
    // always positive effects (check before target checks that provided
    // non-positive result in some case for positive effects)
    case SPELL_EFFECT_HEAL:
    case SPELL_EFFECT_LEARN_SPELL:
    case SPELL_EFFECT_SKILL_STEP:
    case SPELL_EFFECT_HEAL_PCT:
    case SPELL_EFFECT_ENERGIZE_PCT:
    case SPELL_EFFECT_QUEST_COMPLETE:
        return true;

    // non-positive aura use
    case SPELL_EFFECT_APPLY_AURA:
    case SPELL_EFFECT_APPLY_AREA_AURA_FRIEND:
    case SPELL_EFFECT_APPLY_AREA_AURA_ENEMY:
    {
        switch (spellproto->EffectApplyAuraName[effIndex])
        {
        case SPELL_AURA_DUMMY:
        {
            // dummy aura can be positive or negative dependent from casted
            // spell
            switch (spellproto->Id)
            {
            case 13139: // net-o-matic special effect
            case 23445: // evil twin
            case 30529: // Recently In Game (Karazhan, Chess Event)
            case 35679: // Protectorate Demolitionist
            case 38637: // Nether Exhaustion (red)
            case 38638: // Nether Exhaustion (green)
            case 38639: // Nether Exhaustion (blue)
            case 44689: // Relay Race Accept Hidden Debuff - DND
                return false;
            // some spells have unclear target modes for selection, so just make
            // effect positive
            case 27184:
            case 27190:
            case 27191:
            case 27201:
            case 27202:
            case 27203:
            // Netrherspite's beams on himself (Karazhan)
            case 30463:
            case 30464:
            case 30465:
                return true;
            default:
                break;
            }
        }
        break;
        case SPELL_AURA_PERIODIC_HEAL:
            return true;
        case SPELL_AURA_MOD_DAMAGE_DONE: // dependant from base point sign
                                         // (negative -> negative)
        case SPELL_AURA_MOD_RESISTANCE:
        case SPELL_AURA_MOD_STAT:
        case SPELL_AURA_MOD_SKILL:
        case SPELL_AURA_MOD_DODGE_PERCENT:
        case SPELL_AURA_MOD_HEALING_PCT:
        case SPELL_AURA_MOD_HEALING_DONE:
            if (spellproto->CalculateSimpleValue(effIndex) < 0)
                return false;
            break;
        case SPELL_AURA_MOD_DAMAGE_TAKEN: // dependant from base point sign
                                          // (positive -> negative)
            if (spellproto->CalculateSimpleValue(effIndex) < 0)
                return true;
            // let check by target modes (for Amplify Magic cases/etc)
            break;
        case SPELL_AURA_MOD_SPELL_CRIT_CHANCE:
        case SPELL_AURA_MOD_INCREASE_HEALTH_PERCENT:
        case SPELL_AURA_MOD_DAMAGE_PERCENT_DONE:
            if (spellproto->Id == 39089 ||
                spellproto->Id ==
                    39092) // Positive & Negative Charge DMG buff (Mechanar)
                return false;
            if (spellproto->CalculateSimpleValue(effIndex) > 0)
                return true; // some expected positive spells have
                             // SPELL_ATTR_EX_NEGATIVE or unclear target modes
            break;
        case SPELL_AURA_ADD_TARGET_TRIGGER:
            return true;
        case SPELL_AURA_PERIODIC_TRIGGER_SPELL:
            if (spellproto->Id != spellproto->EffectTriggerSpell[effIndex])
            {
                uint32 spellTriggeredId =
                    spellproto->EffectTriggerSpell[effIndex];
                SpellEntry const* spellTriggeredProto =
                    sSpellStore.LookupEntry(spellTriggeredId);

                if (spellTriggeredProto)
                {
                    // non-positive targets of main spell return early
                    for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
                    {
                        // if non-positive trigger cast targeted to positive
                        // target this main cast is non-positive
                        // this will place this spell auras as debuffs
                        if (spellTriggeredProto->Effect[i] &&
                            IsPositiveTarget(
                                spellTriggeredProto->EffectImplicitTargetA[i],
                                spellTriggeredProto
                                    ->EffectImplicitTargetB[i]) &&
                            !IsPositiveEffect(
                                spellTriggeredProto, SpellEffectIndex(i)))
                            return false;
                    }
                }
            }
            break;
        case SPELL_AURA_PROC_TRIGGER_SPELL:
            // many positive auras have negative triggered spells at damage for
            // example and this not make it negative (it can be canceled for
            // example)
            break;
        case SPELL_AURA_MOD_STUN: // have positive and negative spells, we can't
                                  // sort its correctly at this moment.
            if (effIndex == EFFECT_INDEX_0 &&
                spellproto->Effect[EFFECT_INDEX_1] == 0 &&
                spellproto->Effect[EFFECT_INDEX_2] == 0)
                return false; // but all single stun aura spells is negative
            break;
        case SPELL_AURA_MOD_PACIFY_SILENCE:
            if (spellproto->Id == 24740) // Wisp Costume
                return true;
            return false;
        case SPELL_AURA_MOD_ROOT:
        case SPELL_AURA_MOD_SILENCE:
        case SPELL_AURA_GHOST:
        case SPELL_AURA_PERIODIC_LEECH:
        case SPELL_AURA_MOD_STALKED:
        case SPELL_AURA_PERIODIC_DAMAGE_PERCENT:
            return false;
        case SPELL_AURA_PERIODIC_DAMAGE: // used in positive spells also.
            switch (spellproto->Id)
            {
            case 37469: // Poison Cloud (Karazhan)
            case 37465: // Rain of Fire (Karazhan)
                return false;

            default:
                break;
            }

            // part of negative spell if casted at self (prevent cancel)
            if (spellproto->EffectImplicitTargetA[effIndex] == TARGET_SELF ||
                spellproto->EffectImplicitTargetA[effIndex] == TARGET_SELF2)
                return false;
            break;
        case SPELL_AURA_MOD_DECREASE_SPEED: // used in positive spells also
            // part of positive spell if casted at self
            if ((spellproto->EffectImplicitTargetA[effIndex] == TARGET_SELF ||
                    spellproto->EffectImplicitTargetA[effIndex] ==
                        TARGET_SELF2) &&
                spellproto->SpellFamilyName == SPELLFAMILY_GENERIC)
                return false;
            // but not this if this first effect (don't found better check)
            if (spellproto->HasAttribute(SPELL_ATTR_UNK26) &&
                effIndex == EFFECT_INDEX_0)
                return false;
            break;
        case SPELL_AURA_MOD_CASTING_SPEED_NOT_STACK: // If speed is increased:
                                                     // positive
            if (spellproto->CalculateSimpleValue(effIndex) > 0)
                return true;
            break;
        case SPELL_AURA_TRANSFORM:
            // some spells negative
            switch (spellproto->Id)
            {
            case 36897: // Transporter Malfunction (race mutation to horde)
            case 36899: // Transporter Malfunction (race mutation to alliance)
                return false;
            }
            break;
        case SPELL_AURA_MOD_SCALE:
            // some spells negative
            switch (spellproto->Id)
            {
            case 802:   // Mutate Bug, wrongly negative by target modes
            case 30531: // Soul Transfer
                return true;
            case 36900: // Soul Split: Evil!
            case 36901: // Soul Split: Good
            case 36893: // Transporter Malfunction (decrease size case)
            case 36895: // Transporter Malfunction (increase size case)
                return false;
            }
            break;
        case SPELL_AURA_MECHANIC_IMMUNITY:
        {
            // non-positive immunities
            switch (spellproto->EffectMiscValue[effIndex])
            {
            case MECHANIC_BANDAGE:
            case MECHANIC_SHIELD:
            case MECHANIC_MOUNT:
            case MECHANIC_INVULNERABILITY:
                return false;
            default:
                break;
            }
        }
        break;
        case SPELL_AURA_ADD_FLAT_MODIFIER: // mods
        case SPELL_AURA_ADD_PCT_MODIFIER:
        {
            // non-positive mods
            switch (spellproto->EffectMiscValue[effIndex])
            {
            case SPELLMOD_COST: // dependent from bas point sign (negative ->
                                // positive)
                if (spellproto->CalculateSimpleValue(effIndex) > 0)
                    return false;
                break;
            default:
                break;
            }
        }
        break;
        case SPELL_AURA_FORCE_REACTION:
        {
            if (spellproto->Id ==
                42792) // Recently Dropped Flag (prevent cancel)
                return false;
            break;
        }
        default:
            break;
        }
        break;
    }
    default:
        break;
    }

    // non-positive targets
    if (!IsPositiveTarget(spellproto->EffectImplicitTargetA[effIndex],
            spellproto->EffectImplicitTargetB[effIndex]))
        return false;

    // AttributesEx check
    if (spellproto->HasAttribute(SPELL_ATTR_EX_NEGATIVE))
        return false;

    // ok, positive
    return true;
}

bool CanBeCastOnDeadTargets(const SpellEntry* spellInfo)
{
    // TODO: Is it possible we only need SPELL_ATTR_EX2_CAN_TARGET_DEAD?
    return spellInfo->HasAttribute(SPELL_ATTR_EX2_CAN_TARGET_DEAD) ||
           IsDeathOnlySpell(spellInfo) || IsDeathPersistentSpell(spellInfo) ||
           IsPassiveSpell(spellInfo) ||
           spellInfo->HasEffect(SPELL_EFFECT_RESURRECT) ||
           spellInfo->HasEffect(SPELL_EFFECT_RESURRECT_NEW) ||
           spellInfo->HasEffect(SPELL_EFFECT_SELF_RESURRECT) ||
           spellInfo->HasEffect(SPELL_EFFECT_SKIN_PLAYER_CORPSE) ||
           spellInfo->HasEffect(SPELL_EFFECT_SKINNING) ||
           spellInfo->HasEffect(SPELL_EFFECT_SKIN_PLAYER_CORPSE) ||
           spellInfo->Id == 20577; // Cannibalize
}

bool IsPositiveSpell(uint32 spellId)
{
    SpellEntry const* spellproto = sSpellStore.LookupEntry(spellId);
    if (!spellproto)
        return false;

    return IsPositiveSpell(spellproto);
}

bool IsPositiveSpell(SpellEntry const* spellproto)
{
    // spells with at least one negative effect are considered negative
    // some self-applied spells have negative effects but in self casting case
    // negative check ignored.
    for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
        if (spellproto->Effect[i] &&
            !IsPositiveEffect(spellproto, SpellEffectIndex(i)))
            return false;
    return true;
}

SpellCastResult GetErrorAtShapeshiftedCast(
    SpellEntry const* spellInfo, uint32 form)
{
    // talents that learn spells can have stance requirements that need ignore
    // (this requirement only for client-side stance show in talent description)
    if (GetTalentSpellCost(spellInfo->Id) > 0 &&
        (spellInfo->Effect[EFFECT_INDEX_0] == SPELL_EFFECT_LEARN_SPELL ||
            spellInfo->Effect[EFFECT_INDEX_1] == SPELL_EFFECT_LEARN_SPELL ||
            spellInfo->Effect[EFFECT_INDEX_2] == SPELL_EFFECT_LEARN_SPELL))
        return SPELL_CAST_OK;

    uint32 stanceMask = (form ? 1 << (form - 1) : 0);

    if (stanceMask &
        spellInfo->StancesNot) // can explicitly not be casted in this stance
        return SPELL_FAILED_NOT_SHAPESHIFT;

    if (stanceMask &
        spellInfo->Stances) // can explicitly be casted in this stance
        return SPELL_CAST_OK;

    bool actAsShifted = false;
    if (form > 0)
    {
        SpellShapeshiftFormEntry const* shapeInfo =
            sSpellShapeshiftFormStore.LookupEntry(form);
        if (!shapeInfo)
        {
            logging.error(
                "GetErrorAtShapeshiftedCast: unknown shapeshift %u", form);
            return SPELL_CAST_OK;
        }
        actAsShifted = !(shapeInfo->flags1 &
                           1); // shapeshift acts as normal form for spells
    }

    if (actAsShifted)
    {
        if (spellInfo->HasAttribute(
                SPELL_ATTR_NOT_SHAPESHIFT)) // not while shapeshifted
            return SPELL_FAILED_NOT_SHAPESHIFT;
        else if (spellInfo->Stances != 0) // needs other shapeshift
            return SPELL_FAILED_ONLY_SHAPESHIFT;
    }
    else
    {
        // needs shapeshift
        if (!spellInfo->HasAttribute(SPELL_ATTR_EX2_NOT_NEED_SHAPESHIFT) &&
            spellInfo->Stances != 0)
            return SPELL_FAILED_ONLY_SHAPESHIFT;
    }

    return SPELL_CAST_OK;
}

void SpellMgr::LoadSpellTargetPositions()
{
    mSpellTargetPositions.clear(); // need for reload case

    uint32 count = 0;

    //                                                0   1           2
    //                                                3                  4 5
    QueryResult* result = WorldDatabase.Query(
        "SELECT id, target_map, target_position_x, target_position_y, "
        "target_position_z, target_orientation FROM spell_target_position");
    if (!result)
    {
        logging.info("Loaded %u spell target destination coordinates\n", count);
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        Field* fields = result->Fetch();

        bar.step();

        uint32 Spell_ID = fields[0].GetUInt32();

        SpellTargetPosition st;

        st.target_mapId = fields[1].GetUInt32();
        st.target_X = fields[2].GetFloat();
        st.target_Y = fields[3].GetFloat();
        st.target_Z = fields[4].GetFloat();
        st.target_Orientation = fields[5].GetFloat();

        MapEntry const* mapEntry = sMapStore.LookupEntry(st.target_mapId);
        if (!mapEntry)
        {
            logging.error(
                "Spell (ID:%u) target map (ID: %u) does not exist in "
                "`Map.dbc`.",
                Spell_ID, st.target_mapId);
            continue;
        }

        if (st.target_X == 0 && st.target_Y == 0 && st.target_Z == 0)
        {
            logging.error(
                "Spell (ID:%u) target coordinates not provided.", Spell_ID);
            continue;
        }

        SpellEntry const* spellInfo = sSpellStore.LookupEntry(Spell_ID);
        if (!spellInfo)
        {
            logging.error(
                "Spell (ID:%u) listed in `spell_target_position` does not "
                "exist.",
                Spell_ID);
            continue;
        }

        bool found = false;
        for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
        {
            if (spellInfo->EffectImplicitTargetA[i] ==
                    TARGET_TABLE_X_Y_Z_COORDINATES ||
                spellInfo->EffectImplicitTargetB[i] ==
                    TARGET_TABLE_X_Y_Z_COORDINATES)
            {
                found = true;
                break;
            }
        }
        if (!found)
        {
            logging.error(
                "Spell (Id: %u) listed in `spell_target_position` does not "
                "have target TARGET_TABLE_X_Y_Z_COORDINATES (17).",
                Spell_ID);
            continue;
        }

        mSpellTargetPositions[Spell_ID] = st;
        ++count;

    } while (result->NextRow());

    delete result;

    logging.info("Loaded %u spell target destination coordinates\n", count);
}

template <typename EntryType, typename WorkerType, typename StorageType>
struct SpellRankHelper
{
    SpellRankHelper(SpellMgr& _mgr, StorageType& _storage)
      : mgr(_mgr), worker(_storage), customRank(0)
    {
    }
    void RecordRank(EntryType& entry, uint32 spell_id)
    {
        const SpellEntry* spell = sSpellStore.LookupEntry(spell_id);
        if (!spell)
        {
            logging.error("Spell %u listed in `%s` does not exist", spell_id,
                worker.TableName());
            return;
        }

        uint32 first_id = mgr.GetFirstSpellInChain(spell_id);

        // most spell ranks expected same data
        if (first_id)
        {
            firstRankSpells.insert(first_id);

            if (first_id != spell_id)
            {
                if (!worker.IsValidCustomRank(entry, spell_id, first_id))
                    return;
                // for later check that first rank also added
                else
                {
                    firstRankSpellsWithCustomRanks.insert(first_id);
                    ++customRank;
                }
            }
        }

        worker.AddEntry(entry, spell);
    }
    void FillHigherRanks()
    {
        // check that first rank added for custom ranks
        for (const auto& elem : firstRankSpellsWithCustomRanks)
            if (!worker.HasEntry(elem))
                logging.error(
                    "Spell %u must be listed in `%s` as first rank for listed "
                    "custom ranks of spell but not found!",
                    elem, worker.TableName());

        // fill absent non first ranks data base at first rank data
        for (const auto& elem : firstRankSpells)
        {
            if (worker.SetStateToEntry(elem))
                mgr.doForHighRanks(elem, worker);
        }
    }
    std::set<uint32> firstRankSpells;
    std::set<uint32> firstRankSpellsWithCustomRanks;

    SpellMgr& mgr;
    WorkerType worker;
    uint32 customRank;
};

struct DoSpellProcEvent
{
    DoSpellProcEvent(SpellProcEventMap& _spe_map)
      : spe_map(_spe_map), customProc(0), count(0)
    {
    }
    void operator()(uint32 spell_id)
    {
        SpellProcEventEntry const& spe = state->second;
        // add ranks only for not filled data (some ranks have ppm data
        // different for ranks for example)
        SpellProcEventMap::const_iterator spellItr = spe_map.find(spell_id);
        if (spellItr == spe_map.end())
            spe_map[spell_id] = spe;
        // if custom rank data added then it must be same except ppm
        else
        {
            SpellProcEventEntry const& r_spe = spellItr->second;
            if (spe.schoolMask != r_spe.schoolMask)
                logging.error(
                    "Spell %u listed in `spell_proc_event` as custom rank have "
                    "different schoolMask from first rank in chain",
                    spell_id);

            if (spe.spellFamilyName != r_spe.spellFamilyName)
                logging.error(
                    "Spell %u listed in `spell_proc_event` as custom rank have "
                    "different spellFamilyName from first rank in chain",
                    spell_id);

            for (int32 i = 0; i < MAX_EFFECT_INDEX; ++i)
            {
                if (spe.spellFamilyMask[i] != r_spe.spellFamilyMask[i])
                {
                    logging.error(
                        "Spell %u listed in `spell_proc_event` as custom rank "
                        "have different spellFamilyMask from first rank in "
                        "chain",
                        spell_id);
                    break;
                }
            }

            if (spe.procFlags != r_spe.procFlags)
                logging.error(
                    "Spell %u listed in `spell_proc_event` as custom rank have "
                    "different procFlags from first rank in chain",
                    spell_id);

            if (spe.procEx != r_spe.procEx)
                logging.error(
                    "Spell %u listed in `spell_proc_event` as custom rank have "
                    "different procEx from first rank in chain",
                    spell_id);

            // only ppm allowed has been different from first rank

            if (spe.customChance != r_spe.customChance)
                logging.error(
                    "Spell %u listed in `spell_proc_event` as custom rank have "
                    "different customChance from first rank in chain",
                    spell_id);

            if (spe.cooldown != r_spe.cooldown)
                logging.error(
                    "Spell %u listed in `spell_proc_event` as custom rank have "
                    "different cooldown from first rank in chain",
                    spell_id);
        }
    }

    const char* TableName() { return "spell_proc_event"; }
    bool IsValidCustomRank(
        SpellProcEventEntry const& spe, uint32 entry, uint32 first_id)
    {
        // let have independent data in table for spells with ppm rates (exist
        // rank dependent ppm rate spells)
        if (!spe.ppmRate)
        {
            logging.error(
                "Spell %u listed in `spell_proc_event` is not first rank (%u) "
                "in chain",
                entry, first_id);
            // prevent loading since it won't have an effect anyway
            return false;
        }
        return true;
    }
    void AddEntry(SpellProcEventEntry const& spe, SpellEntry const* spell)
    {
        spe_map[spell->Id] = spe;

        bool isCustom = false;

        if (spe.procFlags == 0)
        {
            if (spell->procFlags == 0)
                logging.error(
                    "Spell %u listed in `spell_proc_event` probally not "
                    "triggered spell (no proc flags)",
                    spell->Id);
        }
        else
        {
            if (spell->procFlags == spe.procFlags)
                logging.error(
                    "Spell %u listed in `spell_proc_event` has exactly same "
                    "proc flags as in spell.dbc, field value redundant",
                    spell->Id);
            else
                isCustom = true;
        }

        if (spe.customChance == 0)
        {
            /* enable for re-check cases, 0 chance ok for some cases because in
            some cases it set by another spell/talent spellmod)
            if (spell->procChance==0 && !spe.ppmRate)
                logging.error("Spell %u listed in
            `spell_proc_event` probally not triggered spell (no chance or ppm)",
            spell->Id);
            */
        }
        else
        {
            if (spell->procChance == spe.customChance)
                logging.error(
                    "Spell %u listed in `spell_proc_event` has exactly same "
                    "custom chance as in spell.dbc, field value redundant",
                    spell->Id);
            else
                isCustom = true;
        }

        // totally redundant record
        if (!spe.schoolMask && !spe.procFlags && !spe.procEx && !spe.ppmRate &&
            !spe.customChance && !spe.cooldown)
        {
            bool empty = !spe.spellFamilyName ? true : false;
            for (int32 i = 0; i < MAX_EFFECT_INDEX; ++i)
            {
                if (spe.spellFamilyMask[i])
                {
                    empty = false;
                    break;
                }
            }
            if (empty)
                logging.error(
                    "Spell %u listed in `spell_proc_event` doesn't have any "
                    "useful data",
                    spell->Id);
        }

        if (isCustom)
            ++customProc;
        else
            ++count;
    }

    bool HasEntry(uint32 spellId) { return spe_map.count(spellId) > 0; }
    bool SetStateToEntry(uint32 spellId)
    {
        return (state = spe_map.find(spellId)) != spe_map.end();
    }
    SpellProcEventMap& spe_map;
    SpellProcEventMap::const_iterator state;

    uint32 customProc;
    uint32 count;
};

void SpellMgr::LoadSpellProcEvents()
{
    mSpellProcEventMap.clear(); // need for reload case

    //                                                0      1           2
    //                                                3                 4
    //                                                5                 6
    //                                                7       8        9 10
    QueryResult* result = WorldDatabase.Query(
        "SELECT entry, SchoolMask, SpellFamilyName, SpellFamilyMask0, "
        "SpellFamilyMask1, SpellFamilyMask2, procFlags, procEx, ppmRate, "
        "CustomChance, Cooldown FROM spell_proc_event");
    if (!result)
    {
        BarGoLink bar(1);
        bar.step();
        logging.info("");
        logging.info(">> No spell proc event conditions loaded");
        return;
    }

    SpellRankHelper<SpellProcEventEntry, DoSpellProcEvent, SpellProcEventMap>
        rankHelper(*this, mSpellProcEventMap);

    BarGoLink bar(result->GetRowCount());
    do
    {
        Field* fields = result->Fetch();

        bar.step();

        uint32 entry = fields[0].GetUInt32();

        SpellProcEventEntry spe;

        spe.schoolMask = fields[1].GetUInt32();
        spe.spellFamilyName = fields[2].GetUInt32();

        for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
            spe.spellFamilyMask[i] = ClassFamilyMask(fields[3 + i].GetUInt64());

        spe.procFlags = fields[6].GetUInt32();
        spe.procEx = fields[7].GetUInt32();
        spe.ppmRate = fields[8].GetFloat();
        spe.customChance = fields[9].GetFloat();
        spe.cooldown = fields[10].GetUInt32();

        rankHelper.RecordRank(spe, entry);

    } while (result->NextRow());

    rankHelper.FillHigherRanks();

    delete result;

    logging.info(
        "Loaded %u extra spell proc event conditions +%u custom proc (inc. "
        "+%u custom ranks)\n",
        rankHelper.worker.count, rankHelper.worker.customProc,
        rankHelper.customRank);
}

struct DoSpellProcItemEnchant
{
    DoSpellProcItemEnchant(
        SpellProcItemEnchantMap& procMap_, float ppm_, uint32 flags_)
      : procMap(procMap_), ppm(ppm_), flags(flags_)
    {
    }
    void operator()(uint32 spell_id)
    {
        procMap[spell_id] = SpellItemProcEntry{ppm, flags};
    }

    SpellProcItemEnchantMap& procMap;
    float ppm;
    uint32 flags;
};

void SpellMgr::LoadSpellProcItemEnchant()
{
    mSpellProcItemEnchantMap.clear(); // need for reload case

    uint32 count = 0;

    //                                                              0      1 2
    std::unique_ptr<QueryResult> result(WorldDatabase.Query(
        "SELECT entry, ppmRate, flags FROM spell_proc_item_enchant"));
    if (!result)
    {
        logging.info("Loaded %u proc item enchant definitions\n", count);
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        Field* fields = result->Fetch();

        bar.step();

        uint32 entry = fields[0].GetUInt32();
        float ppmRate = fields[1].GetFloat();
        uint32 flags = fields[2].GetUInt32();

        const SpellEntry* spellInfo = sSpellStore.LookupEntry(entry);

        if (!spellInfo)
        {
            logging.error(
                "Spell %u listed in `spell_proc_item_enchant` does not exist",
                entry);
            continue;
        }

        uint32 first_id = GetFirstSpellInChain(entry);

        if (first_id != entry)
        {
            logging.error(
                "Spell %u listed in `spell_proc_item_enchant` is not first "
                "rank (%u) in chain",
                entry, first_id);
            // prevent loading since it won't have an effect anyway
            continue;
        }

        mSpellProcItemEnchantMap[entry] = SpellItemProcEntry{ppmRate, flags};

        // also add to high ranks
        DoSpellProcItemEnchant worker(mSpellProcItemEnchantMap, ppmRate, flags);
        doForHighRanks(entry, worker);

        ++count;
    } while (result->NextRow());

    logging.info("Loaded %u proc item enchant definitions\n", count);
}

struct DoSpellBonuses
{
    DoSpellBonuses(
        SpellBonusMap& _spellBonusMap, SpellBonusEntry const& _spellBonus)
      : spellBonusMap(_spellBonusMap), spellBonus(_spellBonus)
    {
    }
    void operator()(uint32 spell_id)
    {
        // don't overwrite separately defined high rank
        if (spellBonusMap.find(spell_id) == spellBonusMap.end())
            spellBonusMap[spell_id] = spellBonus;
    }

    SpellBonusMap& spellBonusMap;
    SpellBonusEntry const& spellBonus;
};

void SpellMgr::LoadSpellBonuses()
{
    mSpellBonusMap.clear(); // need for reload case
    uint32 count = 0;
    //                                                0      1             2 3
    QueryResult* result = WorldDatabase.Query(
        "SELECT entry, direct_bonus, dot_bonus, ap_bonus, ap_dot_bonus FROM "
        "spell_bonus_data");
    if (!result)
    {
        logging.info("Loaded %u spell bonus data\n", count);
        return;
    }

    BarGoLink bar(result->GetRowCount());
    do
    {
        Field* fields = result->Fetch();
        bar.step();
        uint32 entry = fields[0].GetUInt32();

        SpellEntry const* spell = sSpellStore.LookupEntry(entry);
        if (!spell)
        {
            logging.error(
                "Spell %u listed in `spell_bonus_data` does not exist", entry);
            continue;
        }

        SpellBonusEntry sbe;

        sbe.direct_damage = fields[1].GetFloat();
        sbe.dot_damage = fields[2].GetFloat();
        sbe.ap_bonus = fields[3].GetFloat();
        sbe.ap_dot_bonus = fields[4].GetFloat();

        bool need_dot = false;
        bool need_direct = false;
        uint32 x = 0; // count all, including empty, meaning: not all existing
                      // effect is DoTs/HoTs
        for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
        {
            if (!spell->Effect[i])
            {
                ++x;
                continue;
            }

            // DoTs/HoTs
            switch (spell->EffectApplyAuraName[i])
            {
            case SPELL_AURA_PERIODIC_DAMAGE:
            case SPELL_AURA_PERIODIC_DAMAGE_PERCENT:
            case SPELL_AURA_PERIODIC_LEECH:
            case SPELL_AURA_PERIODIC_HEAL:
            case SPELL_AURA_OBS_MOD_HEALTH:
            case SPELL_AURA_PERIODIC_MANA_LEECH:
            case SPELL_AURA_OBS_MOD_MANA:
            case SPELL_AURA_POWER_BURN_MANA:
                need_dot = true;
                ++x;
                break;
            default:
                break;
            }
        }

        // TODO: maybe add explicit list possible direct damage spell effects...
        if (x < MAX_EFFECT_INDEX)
            need_direct = true;

        // Check if direct_bonus is needed in `spell_bonus_data`
        float direct_calc = 0;
        float direct_diff = 1000.0f; // for have big diff if no DB field value
        if (sbe.direct_damage)
        {
            direct_calc =
                CalculateDefaultCoefficient(spell, SPELL_DIRECT_DAMAGE);
            direct_diff = std::abs(sbe.direct_damage - direct_calc);
        }

        // Check if dot_bonus is needed in `spell_bonus_data`
        float dot_calc = 0;
        float dot_diff = 1000.0f; // for have big diff if no DB field value
        if (sbe.dot_damage)
        {
            dot_calc = CalculateDefaultCoefficient(spell, DOT);
            dot_diff = std::abs(sbe.dot_damage - dot_calc);
        }

        if (direct_diff < 0.02f && !need_dot && !sbe.ap_bonus &&
            !sbe.ap_dot_bonus)
            logging.error(
                "`spell_bonus_data` entry for spell %u `direct_bonus` not "
                "needed (data from table: %f, calculated %f, difference of %f) "
                "and `dot_bonus` also not used",
                entry, sbe.direct_damage, direct_calc, direct_diff);
        else if (direct_diff < 0.02f && dot_diff < 0.02f && !sbe.ap_bonus &&
                 !sbe.ap_dot_bonus)
        {
            logging.error(
                "`spell_bonus_data` entry for spell %u `direct_bonus` not "
                "needed (data from table: %f, calculated %f, difference of %f) "
                "and ",
                entry, sbe.direct_damage, direct_calc, direct_diff);
            logging.error(
                "                                  ... `dot_bonus` not needed "
                "(data from table: %f, calculated %f, difference of %f)",
                sbe.dot_damage, dot_calc, dot_diff);
        }
        else if (!need_direct && dot_diff < 0.02f && !sbe.ap_bonus &&
                 !sbe.ap_dot_bonus)
            logging.error(
                "`spell_bonus_data` entry for spell %u `dot_bonus` not needed "
                "(data from table: %f, calculated %f, difference of %f) and "
                "direct also not used",
                entry, sbe.dot_damage, dot_calc, dot_diff);
        else if (!need_direct && sbe.direct_damage)
            logging.error(
                "`spell_bonus_data` entry for spell %u `direct_bonus` not used "
                "(spell not have non-periodic affects)",
                entry);
        else if (!need_dot && sbe.dot_damage)
            logging.error(
                "`spell_bonus_data` entry for spell %u `dot_bonus` not used "
                "(spell not have periodic affects)",
                entry);

        if (!need_direct && sbe.ap_bonus)
            logging.error(
                "`spell_bonus_data` entry for spell %u `ap_bonus` not used "
                "(spell not have non-periodic affects)",
                entry);
        else if (!need_dot && sbe.ap_dot_bonus)
            logging.error(
                "`spell_bonus_data` entry for spell %u `ap_dot_bonus` not used "
                "(spell not have periodic affects)",
                entry);

        mSpellBonusMap[entry] = sbe;

        // also add to high ranks
        DoSpellBonuses worker(mSpellBonusMap, sbe);
        doForHighRanks(entry, worker);

        ++count;

    } while (result->NextRow());

    delete result;

    logging.info("Loaded %u extra spell bonus data\n", count);
}

bool SpellMgr::IsSpellProcEventCanTriggeredBy(
    SpellProcEventEntry const* spellProcEvent, uint32 EventProcFlag,
    SpellEntry const* procSpell, uint32 procFlags, uint32 procExtra)
{
    // No extra req need
    uint32 procEvent_procEx = PROC_EX_NONE;

    // check prockFlags for condition
    if ((procFlags & EventProcFlag) == 0)
        return false;

    // Always trigger for this
    if (EventProcFlag &
        (PROC_FLAG_KILLED | PROC_FLAG_KILL | PROC_FLAG_ON_TRAP_ACTIVATION))
        return true;

    if (spellProcEvent) // Exist event data
    {
        // Store extra req
        procEvent_procEx = spellProcEvent->procEx;

        // For melee triggers
        if (procSpell == nullptr)
        {
            // Check (if set) for school (melee attack have Normal school)
            if (spellProcEvent->schoolMask &&
                (spellProcEvent->schoolMask & SPELL_SCHOOL_MASK_NORMAL) == 0)
                return false;
        }
        else // For spells need check school/spell family/family mask
        {
            // Check (if set) for school
            if (spellProcEvent->schoolMask &&
                (spellProcEvent->schoolMask & procSpell->SchoolMask) == 0)
                return false;

            // Check (if set) for spellFamilyName
            if (spellProcEvent->spellFamilyName &&
                (spellProcEvent->spellFamilyName != procSpell->SpellFamilyName))
                return false;
        }
    }

    // Check for extra req (if none) and hit/crit
    if (procEvent_procEx == PROC_EX_NONE)
    {
        // Don't allow proc from periodic heal if no extra requirement is
        // defined
        if (EventProcFlag &
                (PROC_FLAG_ON_DO_PERIODIC | PROC_FLAG_ON_TAKE_PERIODIC) &&
            (procExtra & PROC_EX_PERIODIC_POSITIVE))
            return false;

        // No extra req, so can trigger for (damage/healing present) and
        // hit/crit
        if (procExtra & (PROC_EX_NORMAL_HIT | PROC_EX_CRITICAL_HIT))
            return true;
    }
    else // all spells hits here only if resist/reflect/immune/evade
    {
        // Exist req for PROC_EX_EX_TRIGGER_ALWAYS
        if (procEvent_procEx & PROC_EX_EX_TRIGGER_ALWAYS)
            return true;
        // PROC_FLAG_ON_TAKE_PERIODIC, is negative by default, or positive if
        // procEx contains PROC_EX_PERIODIC_POSITIVE
        // These are mutually exclusive
        if (procFlags &
                (PROC_FLAG_ON_DO_PERIODIC | PROC_FLAG_ON_TAKE_PERIODIC) &&
            ((procEvent_procEx & PROC_EX_PERIODIC_POSITIVE &&
                 !(procExtra & PROC_EX_PERIODIC_POSITIVE)) ||
                (!(procEvent_procEx & PROC_EX_PERIODIC_POSITIVE) &&
                    procExtra & PROC_EX_PERIODIC_POSITIVE)))
            return false;
        // Don't allow on-damage procs if the procSpell does not have a damage
        // component (note: actual damage done is irrelevant)
        if (procEvent_procEx & PROC_EX_DAMAGING_SPELL &&
            !does_direct_damage(procSpell))
            return false;
        // Check Extra Requirement like
        // (hit/crit/miss/resist/parry/dodge/block/immune/reflect/absorb and
        // other)
        if (procEvent_procEx & procExtra)
            return true;
    }
    return false;
}

void SpellMgr::LoadSpellElixirs()
{
    mSpellElixirs.clear(); // need for reload case

    uint32 count = 0;

    //                                                0      1
    QueryResult* result =
        WorldDatabase.Query("SELECT entry, mask FROM spell_elixir");
    if (!result)
    {
        logging.info("Loaded %u spell elixir definitions\n", count);
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        Field* fields = result->Fetch();

        bar.step();

        uint32 entry = fields[0].GetUInt32();
        uint8 mask = fields[1].GetUInt8();

        SpellEntry const* spellInfo = sSpellStore.LookupEntry(entry);

        if (!spellInfo)
        {
            logging.error(
                "Spell %u listed in `spell_elixir` does not exist", entry);
            continue;
        }

        mSpellElixirs[entry] = mask;

        ++count;
    } while (result->NextRow());

    delete result;

    logging.info("Loaded %u spell elixir definitions\n", count);
}

struct DoSpellThreat
{
    DoSpellThreat(SpellThreatMap& _threatMap) : threatMap(_threatMap), count(0)
    {
    }
    void operator()(uint32 spell_id)
    {
        SpellThreatEntry const& ste = state->second;
        // add ranks only for not filled data (spells adding flat threat are
        // usually different for ranks)
        SpellThreatMap::const_iterator spellItr = threatMap.find(spell_id);
        if (spellItr == threatMap.end())
            threatMap[spell_id] = ste;

        // just assert that entry is not redundant
        else
        {
            SpellThreatEntry const& r_ste = spellItr->second;
            if (ste.threat == r_ste.threat &&
                ste.multiplier == r_ste.multiplier &&
                ste.ap_bonus == r_ste.ap_bonus)
                logging.error(
                    "Spell %u listed in `spell_threat` as custom rank has same "
                    "data as Rank 1, so redundant",
                    spell_id);
        }
    }
    const char* TableName() { return "spell_threat"; }
    bool IsValidCustomRank(
        SpellThreatEntry const& ste, uint32 entry, uint32 first_id)
    {
        if (!ste.threat)
        {
            logging.error(
                "Spell %u listed in `spell_threat` is not first rank (%u) in "
                "chain and has no threat",
                entry, first_id);
            // prevent loading unexpected data
            return false;
        }
        return true;
    }
    void AddEntry(SpellThreatEntry const& ste, SpellEntry const* spell)
    {
        threatMap[spell->Id] = ste;

        // flat threat bonus and attack power bonus currently only work properly
        // when all
        // effects have same targets, otherwise, we'd need to seperate it by
        // effect index
        if (ste.threat || ste.ap_bonus != 0.f)
        {
            const uint32* targetA = spell->EffectImplicitTargetA;
            if ((targetA[EFFECT_INDEX_1] &&
                    targetA[EFFECT_INDEX_1] != targetA[EFFECT_INDEX_0]) ||
                (targetA[EFFECT_INDEX_2] &&
                    targetA[EFFECT_INDEX_2] != targetA[EFFECT_INDEX_0]))
                logging.error(
                    "Spell %u listed in `spell_threat` has effects with "
                    "different targets, threat may be assigned incorrectly",
                    spell->Id);
        }
        ++count;
    }
    bool HasEntry(uint32 spellId) { return threatMap.count(spellId) > 0; }
    bool SetStateToEntry(uint32 spellId)
    {
        return (state = threatMap.find(spellId)) != threatMap.end();
    }

    SpellThreatMap& threatMap;
    SpellThreatMap::const_iterator state;
    uint32 count;
};

void SpellMgr::LoadSpellThreats()
{
    mSpellThreatMap.clear(); // need for reload case

    //                                                0      1       2 3
    QueryResult* result = WorldDatabase.Query(
        "SELECT entry, Threat, multiplier, ap_bonus FROM spell_threat");
    if (!result)
    {
        BarGoLink bar(1);
        bar.step();
        logging.info("");
        logging.info(">> No spell threat entries loaded.");
        return;
    }

    SpellRankHelper<SpellThreatEntry, DoSpellThreat, SpellThreatMap> rankHelper(
        *this, mSpellThreatMap);

    BarGoLink bar(result->GetRowCount());

    do
    {
        Field* fields = result->Fetch();

        bar.step();

        uint32 entry = fields[0].GetUInt32();

        SpellThreatEntry ste;
        ste.threat = fields[1].GetUInt16();
        ste.multiplier = fields[2].GetFloat();
        ste.ap_bonus = fields[3].GetFloat();

        rankHelper.RecordRank(ste, entry);

    } while (result->NextRow());

    rankHelper.FillHigherRanks();

    delete result;

    logging.info("Loaded %u spell threat entries\n", rankHelper.worker.count);
}

bool SpellMgr::IsRankSpellDueToSpell(
    SpellEntry const* spellInfo_1, uint32 spellId_2) const
{
    SpellEntry const* spellInfo_2 = sSpellStore.LookupEntry(spellId_2);
    if (!spellInfo_1 || !spellInfo_2)
        return false;
    if (spellInfo_1->Id == spellId_2)
        return false;

    return GetFirstSpellInChain(spellInfo_1->Id) ==
           GetFirstSpellInChain(spellId_2);
}

bool SpellMgr::canStackSpellRanksInSpellBook(SpellEntry const* spellInfo) const
{
    if (IsPassiveSpell(spellInfo)) // ranked passive spell
        return false;

    // HACK: Faerie Fire (Feral) is one spell that should always have all ranks
    // present despite being a "melee"-ish spell
    if (spellInfo->SpellFamilyName == SPELLFAMILY_DRUID)
    {
        uint32 spell_id = spellInfo->Id;
        if (spell_id == 16857 || spell_id == 17390 || spell_id == 17391 ||
            spell_id == 17392 || spell_id == 27011)
            return true;
    }

    if (spellInfo->powerType != POWER_MANA &&
        spellInfo->powerType != POWER_HEALTH)
        return false;
    if (IsProfessionOrRidingSpell(spellInfo->Id))
        return false;

    if (IsSkillBonusSpell(spellInfo->Id))
        return false;

    // All stance spells. if any better way, change it.
    for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        switch (spellInfo->SpellFamilyName)
        {
        case SPELLFAMILY_PALADIN:
            // Paladin aura Spell
            if (spellInfo->Effect[i] == SPELL_EFFECT_APPLY_AREA_AURA_PARTY)
                return false;
            break;
        case SPELLFAMILY_DRUID:
            // Druid form Spell
            if (spellInfo->Effect[i] == SPELL_EFFECT_APPLY_AURA &&
                spellInfo->EffectApplyAuraName[i] == SPELL_AURA_MOD_SHAPESHIFT)
                return false;
            break;
        case SPELLFAMILY_ROGUE:
            // Rogue Stealth
            if (spellInfo->Effect[i] == SPELL_EFFECT_APPLY_AURA &&
                spellInfo->EffectApplyAuraName[i] == SPELL_AURA_MOD_SHAPESHIFT)
                return false;
            break;
        }
    }
    return true;
}

bool SpellMgr::IsProfessionOrRidingSpell(uint32 spellId)
{
    SpellEntry const* spellInfo = sSpellStore.LookupEntry(spellId);
    if (!spellInfo)
        return false;

    if (spellInfo->Effect[EFFECT_INDEX_1] != SPELL_EFFECT_SKILL)
        return false;

    uint32 skill = spellInfo->EffectMiscValue[EFFECT_INDEX_1];

    return IsProfessionOrRidingSkill(skill);
}

bool SpellMgr::IsProfessionSpell(uint32 spellId)
{
    SpellEntry const* spellInfo = sSpellStore.LookupEntry(spellId);
    if (!spellInfo)
        return false;

    if (spellInfo->Effect[EFFECT_INDEX_1] != SPELL_EFFECT_SKILL)
        return false;

    uint32 skill = spellInfo->EffectMiscValue[EFFECT_INDEX_1];

    return IsProfessionSkill(skill);
}

bool SpellMgr::IsPrimaryProfessionSpell(uint32 spellId)
{
    SpellEntry const* spellInfo = sSpellStore.LookupEntry(spellId);
    if (!spellInfo)
        return false;

    if (spellInfo->Effect[EFFECT_INDEX_1] != SPELL_EFFECT_SKILL)
        return false;

    uint32 skill = spellInfo->EffectMiscValue[EFFECT_INDEX_1];

    return IsPrimaryProfessionSkill(skill);
}

bool SpellMgr::IsPrimaryProfessionFirstRankSpell(uint32 spellId) const
{
    return IsPrimaryProfessionSpell(spellId) && GetSpellRank(spellId) == 1;
}

bool SpellMgr::IsSkillBonusSpell(uint32 spellId) const
{
    SkillLineAbilityMapBounds bounds = GetSkillLineAbilityMapBounds(spellId);

    for (SkillLineAbilityMap::const_iterator _spell_idx = bounds.first;
         _spell_idx != bounds.second; ++_spell_idx)
    {
        SkillLineAbilityEntry const* pAbility = _spell_idx->second;
        if (!pAbility ||
            pAbility->learnOnGetSkill !=
                ABILITY_LEARNED_ON_GET_PROFESSION_SKILL)
            continue;

        if (pAbility->req_skill_value > 0)
            return true;
    }

    return false;
}

SpellEntry const* SpellMgr::SelectAuraRankForLevel(
    SpellEntry const* spellInfo, uint32 level) const
{
    // fast case
    if (level + 10 >= spellInfo->spellLevel)
        return spellInfo;

    // ignore selection for passive spells
    if (IsPassiveSpell(spellInfo))
        return spellInfo;

    bool needRankSelection = false;
    for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        // for simple aura in check apply to any non caster based targets, in
        // rank search mode to any explicit targets
        if (((spellInfo->Effect[i] == SPELL_EFFECT_APPLY_AURA &&
                 (IsExplicitPositiveTarget(
                      spellInfo->EffectImplicitTargetA[i]) ||
                     IsAreaEffectPossitiveTarget(
                         Targets(spellInfo->EffectImplicitTargetA[i])))) ||
                spellInfo->Effect[i] == SPELL_EFFECT_APPLY_AREA_AURA_PARTY) &&
            IsPositiveEffect(spellInfo, SpellEffectIndex(i)))
        {
            needRankSelection = true;
            break;
        }
    }

    // not required (rank check more slow so check it here)
    if (!needRankSelection || GetSpellRank(spellInfo->Id) == 0)
        return spellInfo;

    for (uint32 nextSpellId = spellInfo->Id; nextSpellId != 0;
         nextSpellId = GetPrevSpellInChain(nextSpellId))
    {
        SpellEntry const* nextSpellInfo = sSpellStore.LookupEntry(nextSpellId);
        if (!nextSpellInfo)
            break;

        // if found appropriate level
        if (level + 10 >= spellInfo->spellLevel)
            return nextSpellInfo;

        // one rank less then
    }

    // not found
    return nullptr;
}

bool SpellChain_PerSpellChecks(SpellChainMap::const_iterator& itr,
    uint32 firstId, uint32 prev, uint32 rank, std::string& outError)
{
    std::stringstream ss;

    // First id (all spells in chain must have the same)
    if (itr->second.first != firstId)
    {
        ss << "`spell_id` " << itr->first
           << " has incorrect `first_spell` -- expected: " << firstId;
        outError = ss.str();
        return false;
    }

    // Confirm Previous Id
    if (itr->second.prev != prev)
    {
        ss << "`spell_id` " << itr->first
           << " has incorrect `prev_spell` -- expected: " << prev;
        outError = ss.str();
        return false;
    }

    // Confirm Expected Rank
    if (itr->second.rank != rank)
    {
        ss << "`spell_id` " << itr->first
           << " has incorrect `rank` -- expected: " << rank;
        outError = ss.str();
        return false;
    }

    return true;
}

bool SpellChain_VerifyEntry(uint32 spellId, const SpellChainMap& spellChainMap,
    std::vector<uint32>& spellIdsInChain, std::string& outError)
{
    std::stringstream ss;
    SpellChainMap::const_iterator itr = spellChainMap.find(spellId);
    if (itr == spellChainMap.end())
    {
        ss << "spell with id " << spellId << " not found in `spell_chain`";
        outError = ss.str();
        return false;
    }

    uint32 firstId = itr->second.first;
    uint32 prev = itr->second.prev, rank = itr->second.rank;
    if (firstId == itr->first && prev != 0 && rank != 1)
    {
        ss << "spell with id " << spellId
           << " is first_spell yet has incorrect prev_spell and/or first_spell "
              "in `spell_chain` -- expected 0 and " << spellId;
        outError = ss.str();
        return false;
    }

    do
    {
        if (!SpellChain_PerSpellChecks(itr, firstId, prev, rank, outError))
            return false;

        // Add spell id
        spellIdsInChain.push_back(itr->first);

        // If we have more than one next_spell we need to recursively verify
        // those chains
        if (itr->second.next.size() > 1)
        {
            for (const auto& elem : itr->second.next)
                if (!SpellChain_VerifyEntry(
                        elem, spellChainMap, spellIdsInChain, outError))
                    return false;
            return true; // Done with this chain
        }

        // Update rank and prev
        ++rank;
        prev = itr->first;

        // If we only have one next we can continue as per usual
        if (!itr->second.next.empty())
            itr = spellChainMap.find(itr->second.next[0]);
        else
            break; // No nexts left
    } while (itr != spellChainMap.end());

    return true;
}

void SpellMgr::LoadSpellChains()
{
    mSpellChains.clear(); // need for reload case

    // NOTE: All the dbcs have been squashed together into the spell_chain
    // table.
    // In other words: talents, spellskilllines and the previously titled
    // "customs" are all in this table now
    // A next has also been added. Since the skill line is a list in most but
    // not all cases, the implementation
    // means a node has a first, a previous and one or more next

    uint32 loaded_spells = 0;

    QueryResult* result = WorldDatabase.Query(
        "SELECT spell_id, prev_spell, next_spell, first_spell, rank, req_spell "
        "FROM spell_chain");
    if (!result)
    {
        logging.info("Loaded 0 spell chain records\n");
        logging.error("`spell_chains` table is empty!");
        return;
    }

    BarGoLink bar(result->GetRowCount());
    do
    {
        bar.step();
        Field* fields = result->Fetch();

        uint32 spell_id = fields[0].GetUInt32();

        SpellChainNode node;
        node.prev = fields[1].GetUInt32();
        node.first = fields[3].GetUInt32();
        node.rank = fields[4].GetUInt8();
        node.req = fields[5].GetUInt32();

        // Interpret the text blob containing the next spells and insert them
        // into the node.next vector
        // Format of blob is: "spellId1 spellId2 spellId3"
        const char* next_str = fields[2].GetString();
        if (next_str && next_str[0] != 0)
        {
            char buf[16] = {0};
            for (size_t i = 0, j = 0; j < 15; ++i)
            {
                if (next_str[i] == ' ' || next_str[i] == 0)
                {
                    uint32 id = atoi(buf);
                    if (id)
                        node.next.push_back(id);
                    else
                        logging.error(
                            "Spell %u listed in `spell_chain` has an "
                            "incorrectly formated `next_spell` blob (Id cannot "
                            "be 0. Leave blob as null instead)",
                            spell_id);
                    memset(buf, 0, 16);
                    j = 0;

                    if (next_str[i] == 0)
                        break;
                }
                else
                    buf[j++] = next_str[i];
            }
        }

        // The spell itself must exist for it to be added
        if (!sSpellStore.LookupEntry(spell_id))
        {
            logging.error(
                "Spell %u listed in `spell_chain` does not exist", spell_id);
            continue;
        }

        // Previous or next cannot be self
        if (node.prev == spell_id ||
            std::find(node.next.begin(), node.next.end(), spell_id) !=
                node.next.end())
        {
            logging.error(
                "Spell %u (prev: %u, first: %u, rank: %d, req: %u) listed in "
                "`spell_chain` has prev or next pointing to itself.",
                spell_id, node.prev, node.first, node.rank, node.req);
            continue;
        }

        // Check so all referred to spells exist (previous, first, req, next)
        if (node.prev != 0 && !sSpellStore.LookupEntry(node.prev))
        {
            logging.error(
                "Spell %u (prev: %u, first: %u, rank: %d, req: %u) listed in "
                "`spell_chain` has non-existant previous rank spell.",
                spell_id, node.prev, node.first, node.rank, node.req);
            continue;
        }
        if (!sSpellStore.LookupEntry(node.first))
        {
            logging.error(
                "Spell %u (prev: %u, first: %u, rank: %d, req: %u) listed in "
                "`spell_chain` has non-existant first rank spell.",
                spell_id, node.prev, node.first, node.rank, node.req);
            continue;
        }
        if (node.req != 0 && !sSpellStore.LookupEntry(node.req))
        {
            logging.error(
                "Spell %u (prev: %u, first: %u, rank: %d, req: %u) listed in "
                "`spell_chain` has non-existant required spell.",
                spell_id, node.prev, node.first, node.rank, node.req);
            continue;
        }
        bool passedNext = true;
        for (std::vector<uint32>::iterator itr = node.next.begin();
             itr != node.next.end() && passedNext; ++itr)
            if (!sSpellStore.LookupEntry(*itr))
                passedNext = false;
        if (!passedNext)
        {
            logging.error(
                "Spell %u (prev: %u, first: %u, rank: %d, req: %u) listed in "
                "`spell_chain` has non-existant next spell.",
                spell_id, node.prev, node.first, node.rank, node.req);
            continue;
        }

        // Check basic chain integrity.
        if ((spell_id == node.first) != (node.rank <= 1) ||
            (spell_id == node.first) != (node.prev == 0) ||
            (node.rank <= 1) != (node.prev == 0))
        {
            logging.error(
                "Spell %u (prev: %u, first: %u, rank: %d, req: %u) listed in "
                "`spell_chain` has incompatible chain data.",
                spell_id, node.prev, node.first, node.rank, node.req);
            continue;
        }

        // Extra integrity checks for talents
        if (TalentSpellPos const* pos = GetTalentSpellPos(spell_id))
        {
            if (node.rank != pos->rank + 1)
            {
                logging.error(
                    "Talent %u (prev: %u, first: %u, rank: %d, req: %u) listed "
                    "in `spell_chain` has wrong rank.",
                    spell_id, node.prev, node.first, node.rank, node.req);
                continue;
            }

            if (TalentEntry const* talentEntry =
                    sTalentStore.LookupEntry(pos->talent_id))
            {
                if (node.first != talentEntry->RankID[0])
                {
                    logging.error(
                        "Talent %u (prev: %u, first: %u, rank: %d, req: %u) "
                        "listed in `spell_chain` has wrong first rank spell.",
                        spell_id, node.prev, node.first, node.rank, node.req);
                    continue;
                }

                if (node.rank > 1 &&
                    node.prev != talentEntry->RankID[node.rank - 1 - 1])
                {
                    logging.error(
                        "Talent %u (prev: %u, first: %u, rank: %d, req: %u) "
                        "listed in `spell_chain` has wrong prev rank spell.",
                        spell_id, node.prev, node.first, node.rank, node.req);
                    continue;
                }

                if (node.req != talentEntry->DependsOnSpell)
                {
                    logging.error(
                        "Talent %u (prev: %u, first: %u, rank: %d, req: %u) "
                        "listed in `spell_chain` has wrong required spell.",
                        spell_id, node.prev, node.first, node.rank, node.req);
                    continue;
                }

                if (node.rank < 5 &&
                    talentEntry->RankID[node.rank] && // Does talent have any
                                                      // following ranks?
                    ((node.next.empty() ||
                         node.next.size() > 1) || // A talent can only have one
                                                  // rank following it
                        node.next[0] !=
                            talentEntry
                                ->RankID[node.rank])) // Wrong talent specified
                {
                    logging.error(
                        "Talent %u (prev: %u, first: %u, rank: %d, req: %u) "
                        "listed in `spell_chain` has wrong next spell(s).",
                        spell_id, node.prev, node.first, node.rank, node.req);
                    continue;
                }
            }
        }

        mSpellChains[spell_id] = node;

        ++loaded_spells;
    } while (result->NextRow());

    delete result;

    // Make sure all entries belong to a legit chain
    typedef std::map<uint32, std::vector<uint32>> conf_chain;
    conf_chain confirmedChains; // Chains already verified
    for (SpellChainMap::const_iterator itr = mSpellChains.begin();
         itr != mSpellChains.end(); ++itr)
    {
        uint32 spell_id = itr->first;
        conf_chain::iterator conf_itr = confirmedChains.find(itr->second.first);
        if (conf_itr != confirmedChains.end())
        {
            if (std::find(conf_itr->second.begin(), conf_itr->second.end(),
                    spell_id) == conf_itr->second.end())
                logging.error(
                    "Spell %u listed in `spell_chain` has a `first_spell` that "
                    "links to a complete chain, but this spell is not a part "
                    "of it.",
                    spell_id);
            continue;
        }

        // Go through the entire chain. Starting at first
        uint32 first_id = itr->second.first;
        std::vector<uint32> idsInChain;
        std::string error;
        if (SpellChain_VerifyEntry(first_id, mSpellChains, idsInChain, error))
            confirmedChains[first_id] = idsInChain;
        else
        {
            logging.error(
                "Chain in `spell_chain` with `first_spell` %u failed integrity "
                "check. Reason: %s",
                spell_id, error.c_str());
        }
    }

    logging.info("Loaded %u `spell_chain` entries\n", loaded_spells);
}

void SpellMgr::LoadSpellLearnSkills()
{
    mSpellLearnSkills.clear(); // need for reload case

    // search auto-learned skills and add its to map also for use in unlearn
    // spells/talents
    uint32 dbc_count = 0;
    BarGoLink bar(sSpellStore.GetNumRows());
    for (uint32 spell = 0; spell < sSpellStore.GetNumRows(); ++spell)
    {
        bar.step();
        SpellEntry const* entry = sSpellStore.LookupEntry(spell);

        if (!entry)
            continue;

        for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
        {
            if (entry->Effect[i] == SPELL_EFFECT_SKILL)
            {
                SpellLearnSkillNode dbc_node;
                dbc_node.skill = entry->EffectMiscValue[i];
                dbc_node.step =
                    entry->CalculateSimpleValue(SpellEffectIndex(i));
                if (dbc_node.skill != SKILL_RIDING)
                    dbc_node.value = 1;
                else
                    dbc_node.value = dbc_node.step * 75;
                dbc_node.maxvalue = dbc_node.step * 75;

                mSpellLearnSkills[spell] = dbc_node;
                ++dbc_count;
                break;
            }
        }
    }

    logging.info("Loaded %u Spell Learn Skills from DBC\n", dbc_count);
}

void SpellMgr::LoadSpellLearnSpells()
{
    mSpellLearnSpells.clear(); // need for reload case

    //                                                0      1        2
    QueryResult* result = WorldDatabase.Query(
        "SELECT entry, SpellID, Active FROM spell_learn_spell");
    if (!result)
    {
        logging.info("Loaded 0 spell learn spells\n");
        logging.error("`spell_learn_spell` table is empty!");
        return;
    }

    uint32 count = 0;

    BarGoLink bar(result->GetRowCount());
    do
    {
        bar.step();
        Field* fields = result->Fetch();

        uint32 spell_id = fields[0].GetUInt32();

        SpellLearnSpellNode node;
        node.spell = fields[1].GetUInt32();
        node.active = fields[2].GetBool();
        node.autoLearned = false;

        if (!sSpellStore.LookupEntry(spell_id))
        {
            logging.error(
                "Spell %u listed in `spell_learn_spell` does not exist",
                spell_id);
            continue;
        }

        if (!sSpellStore.LookupEntry(node.spell))
        {
            logging.error(
                "Spell %u listed in `spell_learn_spell` learning nonexistent "
                "spell %u",
                spell_id, node.spell);
            continue;
        }

        if (GetTalentSpellCost(node.spell))
        {
            logging.error(
                "Spell %u listed in `spell_learn_spell` attempt learning "
                "talent spell %u, skipped",
                spell_id, node.spell);
            continue;
        }

        mSpellLearnSpells.insert(
            SpellLearnSpellMap::value_type(spell_id, node));

        ++count;
    } while (result->NextRow());

    delete result;

    // search auto-learned spells and add its to map also for use in unlearn
    // spells/talents
    uint32 dbc_count = 0;
    for (uint32 spell = 0; spell < sSpellStore.GetNumRows(); ++spell)
    {
        SpellEntry const* entry = sSpellStore.LookupEntry(spell);

        if (!entry)
            continue;

        for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
        {
            if (entry->Effect[i] == SPELL_EFFECT_LEARN_SPELL)
            {
                SpellLearnSpellNode dbc_node;
                dbc_node.spell = entry->EffectTriggerSpell[i];
                dbc_node.active = true; // all dbc based learned spells is
                                        // active (show in spell book or hide by
                                        // client itself)

                // ignore learning nonexistent spells (broken/outdated/or
                // generic learning spell 483
                if (!sSpellStore.LookupEntry(dbc_node.spell))
                    continue;

                // talent or passive spells or skill-step spells auto-casted and
                // not need dependent learning,
                // pet teaching spells don't must be dependent learning (casted)
                // other required explicit dependent learning
                dbc_node.autoLearned =
                    entry->EffectImplicitTargetA[i] == TARGET_PET ||
                    GetTalentSpellCost(spell) > 0 || IsPassiveSpell(entry) ||
                    IsSpellHaveEffect(entry, SPELL_EFFECT_SKILL_STEP);

                SpellLearnSpellMapBounds db_node_bounds =
                    GetSpellLearnSpellMapBounds(spell);

                bool found = false;
                for (SpellLearnSpellMap::const_iterator itr =
                         db_node_bounds.first;
                     itr != db_node_bounds.second; ++itr)
                {
                    if (itr->second.spell == dbc_node.spell)
                    {
                        logging.error(
                            "Spell %u auto-learn spell %u in spell.dbc then "
                            "the record in `spell_learn_spell` is redundant, "
                            "please fix DB.",
                            spell, dbc_node.spell);
                        found = true;
                        break;
                    }
                }

                if (!found) // add new spell-spell pair if not found
                {
                    mSpellLearnSpells.insert(
                        SpellLearnSpellMap::value_type(spell, dbc_node));
                    ++dbc_count;
                }
            }
        }
    }

    logging.info(
        "Loaded %u spell learn spells + %u found in DBC\n", count, dbc_count);
}

void SpellMgr::LoadSpellScriptTarget()
{
    mSpellScriptTarget.clear(); // need for reload case

    uint32 count = 0;

    QueryResult* result = WorldDatabase.Query(
        "SELECT entry,type,targetEntry FROM spell_script_target");

    if (!result)
    {
        logging.error(
            "Loaded 0 SpellScriptTarget. DB table `spell_script_target` is "
            "empty.\n");
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        Field* fields = result->Fetch();
        bar.step();

        uint32 spellId = fields[0].GetUInt32();
        uint32 type = fields[1].GetUInt32();
        uint32 targetEntry = fields[2].GetUInt32();

        SpellEntry const* spellProto = sSpellStore.LookupEntry(spellId);

        if (!spellProto)
        {
            logging.error(
                "Table `spell_script_target`: spellId %u listed for "
                "TargetEntry %u does not exist.",
                spellId, targetEntry);
            continue;
        }

        bool targetfound = false;
        for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
        {
            if (spellProto->EffectImplicitTargetA[i] == TARGET_SCRIPT ||
                spellProto->EffectImplicitTargetB[i] == TARGET_SCRIPT ||
                spellProto->EffectImplicitTargetA[i] ==
                    TARGET_SCRIPT_COORDINATES ||
                spellProto->EffectImplicitTargetB[i] ==
                    TARGET_SCRIPT_COORDINATES ||
                spellProto->EffectImplicitTargetA[i] ==
                    TARGET_FOCUS_OR_SCRIPTED_GAMEOBJECT ||
                spellProto->EffectImplicitTargetB[i] ==
                    TARGET_FOCUS_OR_SCRIPTED_GAMEOBJECT ||
                spellProto->EffectImplicitTargetA[i] ==
                    TARGET_AREAEFFECT_INSTANT ||
                spellProto->EffectImplicitTargetB[i] ==
                    TARGET_AREAEFFECT_INSTANT ||
                spellProto->EffectImplicitTargetA[i] ==
                    TARGET_AREAEFFECT_CUSTOM ||
                spellProto->EffectImplicitTargetB[i] ==
                    TARGET_AREAEFFECT_CUSTOM ||
                spellProto->EffectImplicitTargetA[i] ==
                    TARGET_AREAEFFECT_GO_AROUND_DEST ||
                spellProto->EffectImplicitTargetB[i] ==
                    TARGET_AREAEFFECT_GO_AROUND_DEST)
            {
                targetfound = true;
                break;
            }
        }
        if (!targetfound)
        {
            logging.error(
                "Table `spell_script_target`: spellId %u listed for "
                "TargetEntry %u does not have any implicit target "
                "TARGET_SCRIPT(38) or TARGET_SCRIPT_COORDINATES (46) or "
                "TARGET_FOCUS_OR_SCRIPTED_GAMEOBJECT (40).",
                spellId, targetEntry);
            continue;
        }

        if (type >= MAX_SPELL_TARGET_TYPE)
        {
            logging.error(
                "Table `spell_script_target`: target type %u for TargetEntry "
                "%u is incorrect.",
                type, targetEntry);
            continue;
        }

        // Checks by target type
        switch (type)
        {
        case SPELL_TARGET_TYPE_GAMEOBJECT:
        {
            if (!targetEntry)
                break;

            if (!sGOStorage.LookupEntry<GameObjectInfo>(targetEntry))
            {
                logging.error(
                    "Table `spell_script_target`: gameobject template entry %u "
                    "does not exist.",
                    targetEntry);
                continue;
            }
            break;
        }
        default:
            if (!targetEntry)
            {
                logging.error(
                    "Table `spell_script_target`: target entry == 0 for not GO "
                    "target type (%u).",
                    type);
                continue;
            }
            if (const CreatureInfo* cInfo =
                    sCreatureStorage.LookupEntry<CreatureInfo>(targetEntry))
            {
                if (spellId == 30427 && !cInfo->SkinLootId)
                {
                    logging.error(
                        "Table `spell_script_target` has creature %u as a "
                        "target of spellid 30427, but this creature has no "
                        "skinlootid. Gas extraction will not work!",
                        cInfo->Entry);
                    continue;
                }
            }
            else
            {
                logging.error(
                    "Table `spell_script_target`: creature template entry %u "
                    "does not exist.",
                    targetEntry);
                continue;
            }
            break;
        }

        mSpellScriptTarget.insert(SpellScriptTarget::value_type(
            spellId, SpellTargetEntry(SpellTargetType(type), targetEntry)));

        ++count;
    } while (result->NextRow());

    delete result;

    // Check all spells
    /* Disabled (lot errors at this moment)
    for(uint32 i = 1; i < sSpellStore.nCount; ++i)
    {
        SpellEntry const * spellInfo = sSpellStore.LookupEntry(i);
        if(!spellInfo)
            continue;

        bool found = false;
        for(int j = 0; j < MAX_EFFECT_INDEX; ++j)
        {
            if( spellInfo->EffectImplicitTargetA[j] == TARGET_SCRIPT ||
    spellInfo->EffectImplicitTargetA[j] != TARGET_SELF &&
    spellInfo->EffectImplicitTargetB[j] == TARGET_SCRIPT )
            {
                SpellScriptTarget::const_iterator lower =
    GetBeginSpellScriptTarget(spellInfo->Id);
                SpellScriptTarget::const_iterator upper =
    GetEndSpellScriptTarget(spellInfo->Id);
                if(lower==upper)
                {
                    logging.error("Spell (ID: %u) has effect
    EffectImplicitTargetA/EffectImplicitTargetB = %u (TARGET_SCRIPT), but does
    not have record in `spell_script_target`",spellInfo->Id,TARGET_SCRIPT);
                    break;                                  // effects of spell
                }
            }
        }
    }
    */

    logging.info("Loaded %u Spell Script Targets\n", count);
}

void SpellMgr::LoadSpellPetAuras()
{
    mSpellPetAuraMap.clear(); // need for reload case

    uint32 count = 0;

    //                                                0      1    2
    QueryResult* result =
        WorldDatabase.Query("SELECT spell, pet, aura FROM spell_pet_auras");
    if (!result)
    {
        logging.info("Loaded %u spell pet auras\n", count);
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        Field* fields = result->Fetch();

        bar.step();

        uint32 spell = fields[0].GetUInt32();
        uint32 pet = fields[1].GetUInt32();
        uint32 aura = fields[2].GetUInt32();

        SpellPetAuraMap::iterator itr = mSpellPetAuraMap.find(spell);
        if (itr != mSpellPetAuraMap.end())
        {
            itr->second.AddAura(pet, aura);
        }
        else
        {
            SpellEntry const* spellInfo = sSpellStore.LookupEntry(spell);
            if (!spellInfo)
            {
                logging.error(
                    "Spell %u listed in `spell_pet_auras` does not exist",
                    spell);
                continue;
            }
            int i = 0;
            for (; i < MAX_EFFECT_INDEX; ++i)
                if ((spellInfo->Effect[i] == SPELL_EFFECT_APPLY_AURA &&
                        spellInfo->EffectApplyAuraName[i] ==
                            SPELL_AURA_DUMMY) ||
                    spellInfo->Effect[i] == SPELL_EFFECT_DUMMY)
                    break;

            if (i == MAX_EFFECT_INDEX)
            {
                logging.error(
                    "Spell %u listed in `spell_pet_auras` does not have dummy "
                    "aura or dummy effect",
                    spell);
                continue;
            }

            SpellEntry const* spellInfo2 = sSpellStore.LookupEntry(aura);
            if (!spellInfo2)
            {
                logging.error(
                    "Aura %u listed in `spell_pet_auras` does not exist", aura);
                continue;
            }

            PetAura pa(pet, aura,
                spellInfo->EffectImplicitTargetA[i] == TARGET_PET,
                spellInfo->CalculateSimpleValue(SpellEffectIndex(i)));
            mSpellPetAuraMap[spell] = pa;
        }

        ++count;
    } while (result->NextRow());

    delete result;

    logging.info("Loaded %u spell pet auras\n", count);
}

/// Some checks for spells, to prevent adding deprecated/broken spells for
/// trainers, spell book, etc
bool SpellMgr::IsSpellValid(SpellEntry const* spellInfo, Player* pl, bool msg)
{
    // not exist
    if (!spellInfo)
        return false;

    bool need_check_reagents = false;

    // check effects
    for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        switch (spellInfo->Effect[i])
        {
        case SPELL_EFFECT_NONE:
            continue;

        // craft spell for crafting nonexistent item (break client recipes list
        // show)
        case SPELL_EFFECT_CREATE_ITEM:
        {
            if (!ObjectMgr::GetItemPrototype(spellInfo->EffectItemType[i]))
            {
                if (msg)
                {
                    if (pl)
                        ChatHandler(pl).PSendSysMessage(
                            "Craft spell %u create item (Entry: %u) but item "
                            "does not exist in item_template.",
                            spellInfo->Id, spellInfo->EffectItemType[i]);
                    else
                        logging.error(
                            "Craft spell %u create item (Entry: %u) but item "
                            "does not exist in item_template.",
                            spellInfo->Id, spellInfo->EffectItemType[i]);
                }
                return false;
            }

            need_check_reagents = true;
            break;
        }
        case SPELL_EFFECT_LEARN_SPELL:
        {
            SpellEntry const* spellInfo2 =
                sSpellStore.LookupEntry(spellInfo->EffectTriggerSpell[i]);
            if (!IsSpellValid(spellInfo2, pl, msg))
            {
                if (msg)
                {
                    if (pl)
                        ChatHandler(pl).PSendSysMessage(
                            "Spell %u learn to broken spell %u, and then...",
                            spellInfo->Id, spellInfo->EffectTriggerSpell[i]);
                    else
                        logging.error(
                            "Spell %u learn to invalid spell %u, and then...",
                            spellInfo->Id, spellInfo->EffectTriggerSpell[i]);
                }
                return false;
            }
            break;
        }
        }
    }

    if (need_check_reagents)
    {
        for (int j = 0; j < MAX_SPELL_REAGENTS; ++j)
        {
            if (spellInfo->Reagent[j] > 0 &&
                !ObjectMgr::GetItemPrototype(spellInfo->Reagent[j]))
            {
                if (msg)
                {
                    if (pl)
                        ChatHandler(pl).PSendSysMessage(
                            "Craft spell %u requires reagent item (Entry: %u) "
                            "but item does not exist in item_template.",
                            spellInfo->Id, spellInfo->Reagent[j]);
                    else
                        logging.error(
                            "Craft spell %u requires reagent item (Entry: %u) "
                            "but item does not exist in item_template.",
                            spellInfo->Id, spellInfo->Reagent[j]);
                }
                return false;
            }
        }
    }

    return true;
}

void SpellMgr::LoadSpellAreas()
{
    mSpellAreaMap.clear(); // need for reload case
    mSpellAreaForQuestMap.clear();
    mSpellAreaForActiveQuestMap.clear();
    mSpellAreaForQuestEndMap.clear();
    mSpellAreaForAuraMap.clear();

    uint32 count = 0;

    //                                                0      1     2
    //                                                3                   4
    //                                                5           6         7 8
    QueryResult* result = WorldDatabase.Query(
        "SELECT spell, area, quest_start, quest_start_active, quest_end, "
        "aura_spell, racemask, gender, autocast FROM spell_area");

    if (!result)
    {
        logging.info("Loaded %u spell area requirements\n", count);
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        Field* fields = result->Fetch();

        bar.step();

        uint32 spell = fields[0].GetUInt32();
        SpellArea spellArea;
        spellArea.spellId = spell;
        spellArea.areaId = fields[1].GetUInt32();
        spellArea.questStart = fields[2].GetUInt32();
        spellArea.questStartCanActive = fields[3].GetBool();
        spellArea.questEnd = fields[4].GetUInt32();
        spellArea.auraSpell = fields[5].GetInt32();
        spellArea.raceMask = fields[6].GetUInt32();
        spellArea.gender = Gender(fields[7].GetUInt8());
        spellArea.autocast = fields[8].GetBool();

        if (!sSpellStore.LookupEntry(spell))
        {
            logging.error(
                "Spell %u listed in `spell_area` does not exist", spell);
            continue;
        }

        {
            bool ok = true;
            SpellAreaMapBounds sa_bounds =
                GetSpellAreaMapBounds(spellArea.spellId);
            for (SpellAreaMap::const_iterator itr = sa_bounds.first;
                 itr != sa_bounds.second; ++itr)
            {
                if (spellArea.spellId != itr->second.spellId)
                    continue;
                if (spellArea.areaId != itr->second.areaId)
                    continue;
                if (spellArea.questStart != itr->second.questStart)
                    continue;
                if (spellArea.auraSpell != itr->second.auraSpell)
                    continue;
                if ((spellArea.raceMask & itr->second.raceMask) == 0)
                    continue;
                if (spellArea.gender != itr->second.gender)
                    continue;

                // duplicate by requirements
                ok = false;
                break;
            }

            if (!ok)
            {
                logging.error(
                    "Spell %u listed in `spell_area` already listed with "
                    "similar requirements.",
                    spell);
                continue;
            }
        }

        if (spellArea.areaId && !GetAreaEntryByAreaID(spellArea.areaId))
        {
            logging.error(
                "Spell %u listed in `spell_area` have wrong area (%u) "
                "requirement",
                spell, spellArea.areaId);
            continue;
        }

        if (spellArea.questStart &&
            !sObjectMgr::Instance()->GetQuestTemplate(spellArea.questStart))
        {
            logging.error(
                "Spell %u listed in `spell_area` have wrong start quest (%u) "
                "requirement",
                spell, spellArea.questStart);
            continue;
        }

        if (spellArea.questEnd)
        {
            if (!sObjectMgr::Instance()->GetQuestTemplate(spellArea.questEnd))
            {
                logging.error(
                    "Spell %u listed in `spell_area` have wrong end quest (%u) "
                    "requirement",
                    spell, spellArea.questEnd);
                continue;
            }

            if (spellArea.questEnd == spellArea.questStart &&
                !spellArea.questStartCanActive)
            {
                logging.error(
                    "Spell %u listed in `spell_area` have quest (%u) "
                    "requirement for start and end in same time",
                    spell, spellArea.questEnd);
                continue;
            }
        }

        if (spellArea.auraSpell)
        {
            SpellEntry const* spellInfo =
                sSpellStore.LookupEntry(abs(spellArea.auraSpell));
            if (!spellInfo)
            {
                logging.error(
                    "Spell %u listed in `spell_area` have wrong aura spell "
                    "(%u) requirement",
                    spell, abs(spellArea.auraSpell));
                continue;
            }

            switch (spellInfo->EffectApplyAuraName[EFFECT_INDEX_0])
            {
            case SPELL_AURA_DUMMY:
            case SPELL_AURA_GHOST:
                break;
            default:
                logging.error(
                    "Spell %u listed in `spell_area` have aura spell "
                    "requirement (%u) without dummy/phase/ghost aura in effect "
                    "0",
                    spell, abs(spellArea.auraSpell));
                continue;
            }

            if (uint32(abs(spellArea.auraSpell)) == spellArea.spellId)
            {
                logging.error(
                    "Spell %u listed in `spell_area` have aura spell (%u) "
                    "requirement for itself",
                    spell, abs(spellArea.auraSpell));
                continue;
            }

            // not allow autocast chains by auraSpell field (but allow use as
            // alternative if not present)
            if (spellArea.autocast && spellArea.auraSpell > 0)
            {
                bool chain = false;
                SpellAreaForAuraMapBounds saBound =
                    GetSpellAreaForAuraMapBounds(spellArea.spellId);
                for (SpellAreaForAuraMap::const_iterator itr = saBound.first;
                     itr != saBound.second; ++itr)
                {
                    if (itr->second->autocast && itr->second->auraSpell > 0)
                    {
                        chain = true;
                        break;
                    }
                }

                if (chain)
                {
                    logging.error(
                        "Spell %u listed in `spell_area` have aura spell (%u) "
                        "requirement that itself autocast from aura",
                        spell, spellArea.auraSpell);
                    continue;
                }

                SpellAreaMapBounds saBound2 =
                    GetSpellAreaMapBounds(spellArea.auraSpell);
                for (SpellAreaMap::const_iterator itr2 = saBound2.first;
                     itr2 != saBound2.second; ++itr2)
                {
                    if (itr2->second.autocast && itr2->second.auraSpell > 0)
                    {
                        chain = true;
                        break;
                    }
                }

                if (chain)
                {
                    logging.error(
                        "Spell %u listed in `spell_area` have aura spell (%u) "
                        "requirement that itself autocast from aura",
                        spell, spellArea.auraSpell);
                    continue;
                }
            }
        }

        if (spellArea.raceMask &&
            (spellArea.raceMask & RACEMASK_ALL_PLAYABLE) == 0)
        {
            logging.error(
                "Spell %u listed in `spell_area` have wrong race mask (%u) "
                "requirement",
                spell, spellArea.raceMask);
            continue;
        }

        if (spellArea.gender != GENDER_NONE &&
            spellArea.gender != GENDER_FEMALE &&
            spellArea.gender != GENDER_MALE)
        {
            logging.error(
                "Spell %u listed in `spell_area` have wrong gender (%u) "
                "requirement",
                spell, spellArea.gender);
            continue;
        }

        SpellArea const* sa =
            &mSpellAreaMap.insert(SpellAreaMap::value_type(spell, spellArea))
                 ->second;

        // for search by current zone/subzone at zone/subzone change
        if (spellArea.areaId)
            mSpellAreaForAreaMap.insert(
                SpellAreaForAreaMap::value_type(spellArea.areaId, sa));

        // for search at quest start/reward
        if (spellArea.questStart)
        {
            if (spellArea.questStartCanActive)
                mSpellAreaForActiveQuestMap.insert(
                    SpellAreaForQuestMap::value_type(spellArea.questStart, sa));
            else
                mSpellAreaForQuestMap.insert(
                    SpellAreaForQuestMap::value_type(spellArea.questStart, sa));
        }

        // for search at quest start/reward
        if (spellArea.questEnd)
            mSpellAreaForQuestEndMap.insert(
                SpellAreaForQuestMap::value_type(spellArea.questEnd, sa));

        // for search at aura apply
        if (spellArea.auraSpell)
            mSpellAreaForAuraMap.insert(
                SpellAreaForAuraMap::value_type(abs(spellArea.auraSpell), sa));

        ++count;
    } while (result->NextRow());

    delete result;

    logging.info("Loaded %u spell area requirements\n", count);
}

SpellCastResult SpellMgr::GetSpellAllowedInLocationError(
    SpellEntry const* spellInfo, uint32 map_id, uint32 zone_id, uint32 area_id,
    Player const* player)
{
    // normal case
    if (spellInfo->AreaId > 0 && spellInfo->AreaId != zone_id &&
        spellInfo->AreaId != area_id)
        return SPELL_FAILED_REQUIRES_AREA;

    // continent limitation (virtual continent), ignore for GM
    if (spellInfo->HasAttribute(SPELL_ATTR_EX4_CAST_ONLY_IN_OUTLAND) &&
        !(player && player->isGameMaster()))
    {
        uint32 v_map = GetVirtualMapForMapAndZone(map_id, zone_id);
        MapEntry const* mapEntry = sMapStore.LookupEntry(v_map);
        if (!mapEntry || mapEntry->addon < 1 || !mapEntry->IsContinent())
            return SPELL_FAILED_REQUIRES_AREA;
    }

    // raid instance limitation
    if (spellInfo->HasAttribute(SPELL_ATTR_EX6_NOT_IN_RAID_INSTANCE))
    {
        MapEntry const* mapEntry = sMapStore.LookupEntry(map_id);
        if (!mapEntry || mapEntry->IsRaid())
            return SPELL_FAILED_REQUIRES_AREA;
    }

    // DB base check (if non empty then must fit at least single for allow)
    SpellAreaMapBounds saBounds = GetSpellAreaMapBounds(spellInfo->Id);
    if (saBounds.first != saBounds.second)
    {
        for (SpellAreaMap::const_iterator itr = saBounds.first;
             itr != saBounds.second; ++itr)
        {
            if (itr->second.IsFitToRequirements(player, zone_id, area_id))
                return SPELL_CAST_OK;
        }
        return SPELL_FAILED_REQUIRES_AREA;
    }

    // bg spell checks

    // do not allow spells to be cast in arenas
    // - with SPELL_ATTR_EX4_NOT_USABLE_IN_ARENA flag
    // - with greater than 15 min CD
    if (spellInfo->HasAttribute(SPELL_ATTR_EX4_NOT_USABLE_IN_ARENA) ||
        (GetSpellRecoveryTime(spellInfo) > 15 * MINUTE * IN_MILLISECONDS &&
            !spellInfo->HasAttribute(SPELL_ATTR_EX4_USABLE_IN_ARENA)))
        if (player && player->InArena())
            return SPELL_FAILED_NOT_IN_ARENA;

    // Spell casted only on battleground
    if (spellInfo->HasAttribute(SPELL_ATTR_EX3_BATTLEGROUND))
        if (!player || !player->InBattleGround())
            return SPELL_FAILED_ONLY_BATTLEGROUNDS;

    switch (spellInfo->Id)
    {
    // a trinket in alterac valley allows to teleport to the boss
    case 22564: // recall
    case 22563: // recall
    {
        if (!player)
            return SPELL_FAILED_REQUIRES_AREA;
        BattleGround* bg = player->GetBattleGround();
        return map_id == 30 && bg && bg->GetStatus() != STATUS_WAIT_JOIN ?
                   SPELL_CAST_OK :
                   SPELL_FAILED_REQUIRES_AREA;
    }
    case 23333: // Warsong Flag
    case 23335: // Silverwing Flag
        return map_id == 489 && player && player->InBattleGround() ?
                   SPELL_CAST_OK :
                   SPELL_FAILED_REQUIRES_AREA;
    case 34976: // Netherstorm Flag
        return map_id == 566 && player && player->InBattleGround() ?
                   SPELL_CAST_OK :
                   SPELL_FAILED_REQUIRES_AREA;
    case 2584:  // Waiting to Resurrect
    case 42792: // Recently Dropped Flag
    case 43681: // Inactive
    {
        return player && player->InBattleGround() ?
                   SPELL_CAST_OK :
                   SPELL_FAILED_ONLY_BATTLEGROUNDS;
    }
    case 22011: // Spirit Heal Channel
    case 22012: // Spirit Heal
    case 24171: // Resurrection Impact Visual
    case 44535: // Spirit Heal (mana)
    {
        MapEntry const* mapEntry = sMapStore.LookupEntry(map_id);
        if (!mapEntry)
            return SPELL_FAILED_REQUIRES_AREA;
        return mapEntry->IsBattleGround() ? SPELL_CAST_OK :
                                            SPELL_FAILED_ONLY_BATTLEGROUNDS;
    }
    case 44521: // Preparation
    {
        if (!player)
            return SPELL_FAILED_REQUIRES_AREA;

        BattleGround* bg = player->GetBattleGround();
        return bg && bg->GetStatus() == STATUS_WAIT_JOIN ?
                   SPELL_CAST_OK :
                   SPELL_FAILED_ONLY_BATTLEGROUNDS;
    }
    case 32724: // Gold Team (Alliance)
    case 32725: // Green Team (Alliance)
    case 35774: // Gold Team (Horde)
    case 35775: // Green Team (Horde)
    {
        return player && player->InArena() ? SPELL_CAST_OK :
                                             SPELL_FAILED_ONLY_IN_ARENA;
    }
    case 32728: // Arena Preparation
    {
        if (!player)
            return SPELL_FAILED_REQUIRES_AREA;
        if (!player->InArena())
            return SPELL_FAILED_REQUIRES_AREA;

        BattleGround* bg = player->GetBattleGround();
        return bg && bg->GetStatus() == STATUS_WAIT_JOIN ?
                   SPELL_CAST_OK :
                   SPELL_FAILED_ONLY_IN_ARENA;
    }
    }

    return SPELL_CAST_OK;
}

void SpellMgr::LoadSkillLineAbilityMap()
{
    mSkillLineAbilityMap.clear();

    BarGoLink bar(sSkillLineAbilityStore.GetNumRows());
    uint32 count = 0;

    for (uint32 i = 0; i < sSkillLineAbilityStore.GetNumRows(); ++i)
    {
        bar.step();
        SkillLineAbilityEntry const* SkillInfo =
            sSkillLineAbilityStore.LookupEntry(i);
        if (!SkillInfo)
            continue;

        mSkillLineAbilityMap.insert(
            SkillLineAbilityMap::value_type(SkillInfo->spellId, SkillInfo));
        ++count;
    }

    logging.info("Loaded %u SkillLineAbility MultiMap Data\n", count);
}

void SpellMgr::LoadSkillRaceClassInfoMap()
{
    mSkillRaceClassInfoMap.clear();

    BarGoLink bar(sSkillRaceClassInfoStore.GetNumRows());
    uint32 count = 0;

    for (uint32 i = 0; i < sSkillRaceClassInfoStore.GetNumRows(); ++i)
    {
        bar.step();
        SkillRaceClassInfoEntry const* skillRCInfo =
            sSkillRaceClassInfoStore.LookupEntry(i);
        if (!skillRCInfo)
            continue;

        // not all skills really listed in ability skills list
        if (!sSkillLineStore.LookupEntry(skillRCInfo->skillId))
            continue;

        mSkillRaceClassInfoMap.insert(SkillRaceClassInfoMap::value_type(
            skillRCInfo->skillId, skillRCInfo));

        ++count;
    }

    logging.info("Loaded %u SkillRaceClassInfo MultiMap Data\n", count);
}

void SpellMgr::CheckUsedSpells(char const* table)
{
    uint32 countSpells = 0;
    uint32 countMasks = 0;

    //                                                 0       1               2
    //                                                 3         4           5
    //                                                 6          7          8
    //                                                 9    10
    std::unique_ptr<QueryResult> result(WorldDatabase.PQuery(
        "SELECT "
        "spellid,SpellFamilyName,SpellFamilyMask,SpellIcon,SpellVisual,"
        "SpellCategory,EffectType,EffectAura,EffectIdx,Name,Code FROM %s",
        table));

    if (!result)
    {
        BarGoLink bar(1);

        bar.step();

        logging.info("");
        logging.error("`%s` table is empty!", table);
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        Field* fields = result->Fetch();

        bar.step();

        uint32 spell = fields[0].GetUInt32();
        int32 family = fields[1].GetInt32();
        uint64 familyMask = fields[2].GetUInt64();
        int32 spellIcon = fields[3].GetInt32();
        int32 spellVisual = fields[4].GetInt32();
        int32 category = fields[5].GetInt32();
        int32 effectType = fields[6].GetInt32();
        int32 auraType = fields[7].GetInt32();
        int32 effectIdx = fields[8].GetInt32();
        std::string name = fields[9].GetCppString();
        std::string code = fields[10].GetCppString();

        // checks of correctness requirements itself

        if (family < -1 || family > SPELLFAMILY_UNK3)
        {
            logging.error(
                "Table '%s' for spell %u have wrong SpellFamily value(%u), "
                "skipped.",
                table, spell, family);
            continue;
        }

        // TODO: spellIcon check need dbc loading
        if (spellIcon < -1)
        {
            logging.error(
                "Table '%s' for spell %u have wrong SpellIcon value(%u), "
                "skipped.",
                table, spell, spellIcon);
            continue;
        }

        // TODO: spellVisual check need dbc loading
        if (spellVisual < -1)
        {
            logging.error(
                "Table '%s' for spell %u have wrong SpellVisual value(%u), "
                "skipped.",
                table, spell, spellVisual);
            continue;
        }

        // TODO: for spellCategory better check need dbc loading
        if (category < -1 || (category >= 0 &&
                                 sSpellCategoryStore.find(category) ==
                                     sSpellCategoryStore.end()))
        {
            logging.error(
                "Table '%s' for spell %u have wrong SpellCategory value(%u), "
                "skipped.",
                table, spell, category);
            continue;
        }

        if (effectType < -1 || effectType >= TOTAL_SPELL_EFFECTS)
        {
            logging.error(
                "Table '%s' for spell %u have wrong SpellEffect type "
                "value(%u), skipped.",
                table, spell, effectType);
            continue;
        }

        if (auraType < -1 || auraType >= TOTAL_AURAS)
        {
            logging.error(
                "Table '%s' for spell %u have wrong SpellAura type value(%u), "
                "skipped.",
                table, spell, auraType);
            continue;
        }

        if (effectIdx < -1 || effectIdx >= 3)
        {
            logging.error(
                "Table '%s' for spell %u have wrong EffectIdx value(%u), "
                "skipped.",
                table, spell, effectIdx);
            continue;
        }

        // now checks of requirements

        if (spell)
        {
            ++countSpells;

            SpellEntry const* spellEntry = sSpellStore.LookupEntry(spell);
            if (!spellEntry)
            {
                logging.error("Spell %u '%s' not exist but used in %s.", spell,
                    name.c_str(), code.c_str());
                continue;
            }

            if (family >= 0 && spellEntry->SpellFamilyName != uint32(family))
            {
                logging.error("Spell %u '%s' family(%u) <> %u but used in %s.",
                    spell, name.c_str(), spellEntry->SpellFamilyName, family,
                    code.c_str());
                continue;
            }

            if (familyMask != UI64LIT(0xFFFFFFFFFFFFFFFF))
            {
                if (familyMask == UI64LIT(0x0000000000000000))
                {
                    if (spellEntry->SpellFamilyFlags)
                    {
                        logging.error("Spell %u '%s' not fit to (" UI64FMTD
                                      ") but used in %s.",
                            spell, name.c_str(), familyMask, code.c_str());
                        continue;
                    }
                }
                else
                {
                    if (!spellEntry->IsFitToFamilyMask(familyMask))
                    {
                        logging.error("Spell %u '%s' not fit to (" I64FMT
                                      ") but used in %s.",
                            spell, name.c_str(), familyMask, code.c_str());
                        continue;
                    }
                }
            }

            if (spellIcon >= 0 && spellEntry->SpellIconID != uint32(spellIcon))
            {
                logging.error("Spell %u '%s' icon(%u) <> %u but used in %s.",
                    spell, name.c_str(), spellEntry->SpellIconID, spellIcon,
                    code.c_str());
                continue;
            }

            if (spellVisual >= 0 &&
                spellEntry->SpellVisual != uint32(spellVisual))
            {
                logging.error("Spell %u '%s' visual(%u) <> %u but used in %s.",
                    spell, name.c_str(), spellEntry->SpellVisual, spellVisual,
                    code.c_str());
                continue;
            }

            if (category >= 0 && spellEntry->Category != uint32(category))
            {
                logging.error(
                    "Spell %u '%s' category(%u) <> %u but used in %s.", spell,
                    name.c_str(), spellEntry->Category, category, code.c_str());
                continue;
            }

            if (effectIdx >= EFFECT_INDEX_0)
            {
                if (effectType >= 0 &&
                    spellEntry->Effect[effectIdx] != uint32(effectType))
                {
                    logging.error(
                        "Spell %u '%s' effect%d <> %u but used in %s.", spell,
                        name.c_str(), effectIdx + 1, effectType, code.c_str());
                    continue;
                }

                if (auraType >= 0 &&
                    spellEntry->EffectApplyAuraName[effectIdx] !=
                        uint32(auraType))
                {
                    logging.error("Spell %u '%s' aura%d <> %u but used in %s.",
                        spell, name.c_str(), effectIdx + 1, auraType,
                        code.c_str());
                    continue;
                }
            }
            else
            {
                if (effectType >= 0 &&
                    !IsSpellHaveEffect(spellEntry, SpellEffects(effectType)))
                {
                    logging.error(
                        "Spell %u '%s' not have effect %u but used in %s.",
                        spell, name.c_str(), effectType, code.c_str());
                    continue;
                }

                if (auraType >= 0 &&
                    !IsSpellHaveAura(spellEntry, AuraType(auraType)))
                {
                    logging.error(
                        "Spell %u '%s' not have aura %u but used in %s.", spell,
                        name.c_str(), auraType, code.c_str());
                    continue;
                }
            }
        }
        else
        {
            ++countMasks;

            bool found = false;
            for (uint32 spellId = 1; spellId < sSpellStore.GetNumRows();
                 ++spellId)
            {
                SpellEntry const* spellEntry = sSpellStore.LookupEntry(spellId);
                if (!spellEntry)
                    continue;

                if (family >= 0 &&
                    spellEntry->SpellFamilyName != uint32(family))
                    continue;

                if (familyMask != UI64LIT(0xFFFFFFFFFFFFFFFF))
                {
                    if (familyMask == UI64LIT(0x0000000000000000))
                    {
                        if (spellEntry->SpellFamilyFlags)
                            continue;
                    }
                    else
                    {
                        if (!spellEntry->IsFitToFamilyMask(familyMask))
                            continue;
                    }
                }

                if (spellIcon >= 0 &&
                    spellEntry->SpellIconID != uint32(spellIcon))
                    continue;

                if (spellVisual >= 0 &&
                    spellEntry->SpellVisual != uint32(spellVisual))
                    continue;

                if (category >= 0 && spellEntry->Category != uint32(category))
                    continue;

                if (effectIdx >= 0)
                {
                    if (effectType >= 0 &&
                        spellEntry->Effect[effectIdx] != uint32(effectType))
                        continue;

                    if (auraType >= 0 &&
                        spellEntry->EffectApplyAuraName[effectIdx] !=
                            uint32(auraType))
                        continue;
                }
                else
                {
                    if (effectType >= 0 &&
                        !IsSpellHaveEffect(
                            spellEntry, SpellEffects(effectType)))
                        continue;

                    if (auraType >= 0 &&
                        !IsSpellHaveAura(spellEntry, AuraType(auraType)))
                        continue;
                }

                found = true;
                break;
            }

            if (!found)
            {
                if (effectIdx >= 0)
                    logging.error(
                        "Spells '%s' not found for family %i (" I64FMT
                        ") icon(%i) visual(%i) category(%i) effect%d(%i) "
                        "aura%d(%i) but used in %s",
                        name.c_str(), family, familyMask, spellIcon,
                        spellVisual, category, effectIdx + 1, effectType,
                        effectIdx + 1, auraType, code.c_str());
                else
                    logging.error(
                        "Spells '%s' not found for family %i (" I64FMT
                        ") icon(%i) visual(%i) category(%i) effect(%i) "
                        "aura(%i) but used in %s",
                        name.c_str(), family, familyMask, spellIcon,
                        spellVisual, category, effectType, auraType,
                        code.c_str());
                continue;
            }
        }

    } while (result->NextRow());

    logging.info("");
    logging.info(
        ">> Checked %u spells and %u spell masks", countSpells, countMasks);
}

bool IsSpellProcOnCast(SpellEntry const* entry)
{
    // Surge of Light procs on hit (TODO: Wtb a good proc system)
    if (entry->procFlags == PROC_FLAG_SUCCESSFUL_NEGATIVE_SPELL_HIT)
        return false;

    for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        if (entry->EffectApplyAuraName[i] != SPELL_AURA_ADD_PCT_MODIFIER &&
            entry->EffectApplyAuraName[i] != SPELL_AURA_ADD_FLAT_MODIFIER)
            continue;
        // Presence of Mind, Nature's Grace, etc
        if (entry->EffectMiscValue[i] == SPELLMOD_CASTING_TIME)
            return true;
        // Clearcasting, etc
        if (entry->EffectMiscValue[i] == SPELLMOD_COST)
            return true;
    }

    return false;
}

DiminishingGroup GetDiminishingReturnsGroupForSpell(
    SpellEntry const* spellproto, bool triggered)
{
    // Explicit Diminishing Groups
    switch (spellproto->SpellFamilyName)
    {
    case SPELLFAMILY_GENERIC:
        // some generic arena related spells have by some strange reason
        // MECHANIC_TURN
        if (spellproto->Mechanic == MECHANIC_TURN)
            return DIMINISHING_NONE;
        break;
    case SPELLFAMILY_ROGUE:
    {
        // Kidney Shot
        if (spellproto->IsFitToFamilyMask(UI64LIT(0x00000200000)))
            return DIMINISHING_KIDNEYSHOT;
        // Blind
        else if (spellproto->IsFitToFamilyMask(UI64LIT(0x00001000000)))
            return DIMINISHING_BLIND_CYCLONE;
        break;
    }
    case SPELLFAMILY_HUNTER:
    {
        // Freezing Trap
        if (spellproto->IsFitToFamilyMask(UI64LIT(0x00000000008)))
            return DIMINISHING_FREEZE;
        break;
    }
    case SPELLFAMILY_WARLOCK:
    {
        // Seduction (patch 1.4: seduction moved to fear DR category)
        if (spellproto->Id == 6358)
            return DIMINISHING_FEAR;
        // Curse of Tongues
        if (spellproto->IsFitToFamilyMask(UI64LIT(0x00080000000)) &&
            spellproto->SpellIconID == 692)
            return DIMINISHING_LIMITONLY;
        break;
    }
    case SPELLFAMILY_DRUID:
    {
        // Cyclone
        if (spellproto->IsFitToFamilyMask(UI64LIT(0x02000000000)))
            return DIMINISHING_BLIND_CYCLONE;
        // Feral Charge Immobilize does not have any DR, even with itself
        if (spellproto->SpellIconID == 1559 &&
            spellproto->HasApplyAuraName(SPELL_AURA_MOD_ROOT))
            return DIMINISHING_NONE;
        // Nature's grasp entangling roots is not considered root proc
        switch (spellproto->Id)
        {
        case 19975:
        case 19974:
        case 19973:
        case 19972:
        case 19971:
        case 27010:
            return DIMINISHING_CONTROL_ROOT;
        }
        break;
    }
    case SPELLFAMILY_WARRIOR:
    {
        // Hamstring - limit duration to 10s in PvP
        if (spellproto->IsFitToFamilyMask(UI64LIT(0x00000000002)))
            return DIMINISHING_LIMITONLY;
        break;
    }
    case SPELLFAMILY_PALADIN:
    {
        // In 2.4: Turn Undead (rank 3) became Turn Evil, which is subject to DR
        if (spellproto->Id == 10326)
            return DIMINISHING_FEAR;
        break;
    }
    default:
        break;
    }

    // Get by mechanic
    uint32 mechanic = GetAllSpellMechanicMask(spellproto);
    if (!mechanic)
        return DIMINISHING_NONE;

    if (mechanic & (1 << (MECHANIC_STUN - 1)))
        return triggered ? DIMINISHING_TRIGGER_STUN : DIMINISHING_CONTROL_STUN;
    if (mechanic & (1 << (MECHANIC_SLEEP - 1)))
        return DIMINISHING_SLEEP;
    if (mechanic &
        ((1 << (MECHANIC_POLYMORPH - 1)) | (1 << (MECHANIC_KNOCKOUT - 1)) |
            (1 << (MECHANIC_SAPPED - 1))))
        return DIMINISHING_POLYMORPH_KNOCKOUT;
    if (mechanic & (1 << (MECHANIC_ROOT - 1)))
        return triggered ? DIMINISHING_TRIGGER_ROOT : DIMINISHING_CONTROL_ROOT;
    if (mechanic & (1 << (MECHANIC_FEAR - 1)))
        return DIMINISHING_FEAR;
    if (mechanic & (1 << (MECHANIC_CHARM - 1)))
        return DIMINISHING_CHARM;
    // Silence did not have DR pre patch 3.0.8
    /* if (mechanic & (1 << (MECHANIC_SILENCE - 1)))
        return DIMINISHING_SILENCE; */
    if (mechanic & (1 << (MECHANIC_DISARM - 1)))
        return DIMINISHING_DISARM;
    if (mechanic & (1 << (MECHANIC_FREEZE - 1)))
        return DIMINISHING_FREEZE;
    if (mechanic & (1 << (MECHANIC_BANISH - 1)))
        return DIMINISHING_BANISH;
    if (mechanic & (1 << (MECHANIC_HORROR - 1)))
        return DIMINISHING_DEATHCOIL;

    return DIMINISHING_NONE;
}

bool IsDiminishingReturnsGroupDurationLimited(DiminishingGroup group)
{
    switch (group)
    {
    case DIMINISHING_CONTROL_STUN:
    case DIMINISHING_TRIGGER_STUN:
    case DIMINISHING_KIDNEYSHOT:
    case DIMINISHING_SLEEP:
    case DIMINISHING_CONTROL_ROOT:
    case DIMINISHING_TRIGGER_ROOT:
    case DIMINISHING_FEAR:
    case DIMINISHING_CHARM:
    case DIMINISHING_POLYMORPH_KNOCKOUT:
    case DIMINISHING_FREEZE:
    case DIMINISHING_BLIND_CYCLONE:
    case DIMINISHING_BANISH:
    case DIMINISHING_LIMITONLY:
        return true;
    default:
        return false;
    }
    return false;
}

DiminishingReturnsType GetDiminishingReturnsGroupType(DiminishingGroup group)
{
    switch (group)
    {
    case DIMINISHING_BLIND_CYCLONE:
    case DIMINISHING_CONTROL_STUN:
    case DIMINISHING_TRIGGER_STUN:
    case DIMINISHING_KIDNEYSHOT:
        return DRTYPE_ALL;
    case DIMINISHING_SLEEP:
    case DIMINISHING_CONTROL_ROOT:
    case DIMINISHING_TRIGGER_ROOT:
    case DIMINISHING_FEAR:
    case DIMINISHING_CHARM:
    case DIMINISHING_POLYMORPH_KNOCKOUT:
    case DIMINISHING_SILENCE:
    case DIMINISHING_DISARM:
    case DIMINISHING_DEATHCOIL:
    case DIMINISHING_FREEZE:
    case DIMINISHING_BANISH:
        return DRTYPE_PLAYER;
    default:
        break;
    }

    return DRTYPE_NONE;
}

bool SpellArea::IsFitToRequirements(
    Player const* player, uint32 newZone, uint32 newArea) const
{
    if (gender != GENDER_NONE)
    {
        // not in expected gender
        if (!player || gender != player->getGender())
            return false;
    }

    if (raceMask)
    {
        // not in expected race
        if (!player || !(raceMask & player->getRaceMask()))
            return false;
    }

    if (areaId)
    {
        // not in expected zone
        if (newZone != areaId && newArea != areaId)
            return false;
    }

    if (questStart)
    {
        // not in expected required quest state
        if (!player ||
            ((!questStartCanActive || !player->IsActiveQuest(questStart)) &&
                !player->GetQuestRewardStatus(questStart)))
            return false;
    }

    if (questEnd)
    {
        // not in expected forbidden quest state
        if (!player || player->GetQuestRewardStatus(questEnd))
            return false;
    }

    if (auraSpell)
    {
        // not have expected aura
        if (!player)
            return false;
        if (auraSpell > 0)
            // have expected aura
            return player->has_aura(auraSpell);
        else
            // not have expected aura
            return !player->has_aura(-auraSpell);
    }

    return true;
}

void SpellMgr::LoadSpellAffects()
{
    mSpellAffectMap.clear(); // need for reload case

    uint32 count = 0;

    //                                                0      1         2
    QueryResult* result = WorldDatabase.Query(
        "SELECT entry, effectId, SpellFamilyMask FROM spell_affect");
    if (!result)
    {
        logging.info("Loaded %u spell affect definitions\n", count);
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        Field* fields = result->Fetch();

        bar.step();

        uint32 entry = fields[0].GetUInt32();
        uint8 effectId = fields[1].GetUInt8();

        SpellEntry const* spellInfo = sSpellStore.LookupEntry(entry);

        if (!spellInfo)
        {
            logging.error(
                "Spell %u listed in `spell_affect` does not exist", entry);
            continue;
        }

        if (effectId >= MAX_EFFECT_INDEX)
        {
            logging.error(
                "Spell %u listed in `spell_affect` have invalid effect index "
                "(%u)",
                entry, effectId);
            continue;
        }

        if (spellInfo->Effect[effectId] != SPELL_EFFECT_APPLY_AURA ||
            (spellInfo->EffectApplyAuraName[effectId] !=
                    SPELL_AURA_ADD_FLAT_MODIFIER &&
                spellInfo->EffectApplyAuraName[effectId] !=
                    SPELL_AURA_ADD_PCT_MODIFIER &&
                spellInfo->EffectApplyAuraName[effectId] !=
                    SPELL_AURA_ADD_TARGET_TRIGGER &&
                spellInfo->EffectApplyAuraName[effectId] !=
                    SPELL_AURA_IGNORE_COMBAT_RESULT &&
                spellInfo->EffectApplyAuraName[effectId] !=
                    SPELL_AURA_OVERRIDE_CLASS_SCRIPTS))
        {
            logging.error(
                "Spell %u listed in `spell_affect` have not "
                "SPELL_AURA_ADD_FLAT_MODIFIER (%u) or "
                "SPELL_AURA_ADD_PCT_MODIFIER (%u) or "
                "SPELL_AURA_ADD_TARGET_TRIGGER (%u) or "
                "SPELL_AURA_IGNORE_COMBAT_RESULT (%u) or "
                "SPELL_AURA_OVERRIDE_CLASS_SCRIPTS (%u) for effect index (%u)",
                entry, SPELL_AURA_ADD_FLAT_MODIFIER,
                SPELL_AURA_ADD_PCT_MODIFIER, SPELL_AURA_ADD_TARGET_TRIGGER,
                SPELL_AURA_IGNORE_COMBAT_RESULT,
                SPELL_AURA_OVERRIDE_CLASS_SCRIPTS, effectId);
            continue;
        }

        uint64 spellAffectMask = fields[2].GetUInt64();

        // Spell.dbc have own data for low part of SpellFamilyMask
        if (spellInfo->EffectItemType[effectId])
        {
            if (static_cast<uint64>(spellInfo->EffectItemType[effectId]) ==
                spellAffectMask)
            {
                logging.error(
                    "Spell %u listed in `spell_affect` have redundant (same "
                    "with EffectItemType%d) data for effect index (%u) and not "
                    "needed, skipped.",
                    entry, effectId + 1, effectId);
                continue;
            }
        }

        mSpellAffectMap.insert(SpellAffectMap::value_type(
            (entry << 8) + effectId, spellAffectMask));

        ++count;
    } while (result->NextRow());

    delete result;

    logging.info("Loaded %u spell affect definitions\n", count);

    for (uint32 id = 0; id < sSpellStore.GetNumRows(); ++id)
    {
        SpellEntry const* spellInfo = sSpellStore.LookupEntry(id);
        if (!spellInfo)
            continue;

        for (int effectId = 0; effectId < MAX_EFFECT_INDEX; ++effectId)
        {
            if (spellInfo->Effect[effectId] != SPELL_EFFECT_APPLY_AURA ||
                (spellInfo->EffectApplyAuraName[effectId] !=
                        SPELL_AURA_ADD_FLAT_MODIFIER &&
                    spellInfo->EffectApplyAuraName[effectId] !=
                        SPELL_AURA_ADD_PCT_MODIFIER &&
                    spellInfo->EffectApplyAuraName[effectId] !=
                        SPELL_AURA_ADD_TARGET_TRIGGER &&
                    spellInfo->EffectApplyAuraName[effectId] !=
                        SPELL_AURA_IGNORE_COMBAT_RESULT))
                continue;

            if (spellInfo->EffectItemType[effectId] != 0)
                continue;

            if (mSpellAffectMap.find((id << 8) + effectId) !=
                mSpellAffectMap.end())
                continue;

            logging.error("Spell %u (%s) misses spell_affect for effect %u", id,
                spellInfo->SpellName[sWorld::Instance()->GetDefaultDbcLocale()],
                effectId);
        }
    }
}

float GetSpellRadius(const SpellEntry* info)
{
    float e1 = 0, e2 = 0, e3 = 0;
    if (IsAreaOfEffectIndex(info, EFFECT_INDEX_0))
        e1 = GetSpellRadius(sSpellRadiusStore.LookupEntry(
            info->EffectRadiusIndex[EFFECT_INDEX_0]));
    if (IsAreaOfEffectIndex(info, EFFECT_INDEX_1))
        e2 = GetSpellRadius(sSpellRadiusStore.LookupEntry(
            info->EffectRadiusIndex[EFFECT_INDEX_1]));
    if (IsAreaOfEffectIndex(info, EFFECT_INDEX_2))
        e3 = GetSpellRadius(sSpellRadiusStore.LookupEntry(
            info->EffectRadiusIndex[EFFECT_INDEX_2]));
    return std::max(std::max(e1, e2), e3);
}

bool IsSpellReflectable(
    const SpellEntry* info, const spell_trigger_type& trigger_type)
{
    // Exceptions that violate the rules below
    switch (info->SpellFamilyName)
    {
    case SPELLFAMILY_HUNTER:
        // Freezing Trap Effect
        if (info->SpellFamilyFlags & 0x8)
            return true;
        break;
    case SPELLFAMILY_PALADIN:
        // Holy shock's damage component is reflectable
        if (info->SpellFamilyFlags & 0x200000 &&
            info->HasEffect(SPELL_EFFECT_SCHOOL_DAMAGE))
            return true;
        break;
    }

    if (info->DmgClass != SPELL_DAMAGE_CLASS_MAGIC)
        return false;

    if (trigger_type.triggered() &&
        !info->HasAttribute(SPELL_ATTR_EX5_SINGLE_TARGET_SPELL) &&
        !(info->SpellFamilyName == SPELLFAMILY_MAGE &&
            info->SpellFamilyFlags &
                0x200000)) // Arcane Missiles exception. TODO: Find general rule
        return false;

    // Incompatible attributes
    if (info->HasAttribute(SPELL_ATTR_ABILITY) ||
        info->HasAttribute(SPELL_ATTR_UNAFFECTED_BY_INVULNERABILITY) ||
        IsPassiveSpell(info) ||
        info->HasAttribute(SPELL_ATTR_EX2_CANT_REFLECTED))
        return false;

    // Positive spells cannot be reflected
    // Dispels can be dual purpose (all affected spells have dispel in effidx 1)
    if (IsPositiveSpell(info) &&
        !(info->Effect[0] == SPELL_EFFECT_DISPEL &&
            info->EffectImplicitTargetA[0] == TARGET_DUELVSPLAYER))
        return false;

    // projectile AoE is reflectable, every other type of AoE isn't
    if (IsAreaOfEffectSpell(info) && info->speed == 0)
        return false;

    return true;
}

bool IsSpellReflectIgnored(const SpellEntry* info)
{
    uint32 first_id = sSpellMgr::Instance()->GetFirstSpellInChain(info->Id);

    switch (first_id)
    {
    case 605:  // Priest's mind control
    case 3355: // Hunter's Freezing Trap Effect (r1-3)
    case 14308:
    case 14309:
        return true;
    }

    return false;
}

bool CanSpellHitFriendlyTarget(const SpellEntry* info)
{
    if (IsPositiveSpell(info))
        return true;

    // For non-positive spells we base it on target type if we can cast on
    // spells (TODO: This probably needs more conditions)
    if (info->HasTargetType(TARGET_PET) || info->HasTargetType(TARGET_SCRIPT))
        return true;

    for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        // Spell effects that can always hit friendly targets
        switch (info->Effect[i])
        {
        case SPELL_EFFECT_DUEL:
            return true;
        }
    }

    return false;
}

bool IsAffectableByLvlDmgCalc(const SpellEntry* info, SpellEffectIndex index)
{
    if (!info->HasAttribute(SPELL_ATTR_LEVEL_DAMAGE_CALCULATION))
        return false;

    if (!info->spellLevel)
        return false;

    if (info->EffectRealPointsPerLevel[index] != 0)
        return false; // scaling built into spell data

    if (IsAuraApplyEffect(info, index) ||
        info->Effect[index] == SPELL_EFFECT_PERSISTENT_AREA_AURA)
    {
        switch (info->EffectApplyAuraName[index])
        {
        case SPELL_AURA_PERIODIC_DAMAGE:
        case SPELL_AURA_PERIODIC_LEECH:
        case SPELL_AURA_PERIODIC_HEAL:
        case SPELL_AURA_DAMAGE_SHIELD:
        case SPELL_AURA_PERIODIC_MANA_LEECH:
        case SPELL_AURA_SCHOOL_ABSORB:
        case SPELL_AURA_PERIODIC_TRIGGER_SPELL_WITH_VALUE:
            return true;
        default:
            break;
        }
    }
    else
    {
        switch (info->Effect[index])
        {
        // case SPELL_EFFECT_WEAPON_PERCENT_DAMAGE: <- not affected!
        case SPELL_EFFECT_POWER_DRAIN:
        case SPELL_EFFECT_HEAL:
        case SPELL_EFFECT_SCHOOL_DAMAGE:
        case SPELL_EFFECT_WEAPON_DAMAGE_NOSCHOOL:
        case SPELL_EFFECT_WEAPON_DAMAGE:
        case SPELL_EFFECT_POWER_BURN:
        case SPELL_EFFECT_NORMALIZED_WEAPON_DMG:
        case SPELL_EFFECT_HEALTH_LEECH:
            return true;
        default:
            break;
        }
    }

    return false;
}

void SpellMgr::LoadSpellLevelCalc()
{
    std::unique_ptr<QueryResult> result(WorldDatabase.PQuery(
        "SELECT spell_level, mana_factor, damage_factor FROM spell_level_calc "
        "ORDER BY spell_level"));

    if (!result)
    {
        logging.error("Table `spell_level_calc` is empty\n");
        return;
    }

    BarGoLink bar(result->GetRowCount());

    uint32 expected_level = 0;
    do
    {
        bar.step();
        Field* f = result->Fetch();

        uint32 level = f[0].GetUInt32();
        float mana_factor = f[1].GetFloat();
        float damage_factor = f[2].GetFloat();

        if (expected_level++ != level)
        {
            logging.error(
                "Table `spell_level_calc` has invalid data. Ids must be "
                "consecutive, and start at 0.");
            return;
        }

        level_calc_.push_back(std::make_pair(mana_factor, damage_factor));
    } while (result->NextRow());

    logging.info("Loaded %u spell calculation levels\n", expected_level);
}

void SpellMgr::LoadSpellDependencies()
{
    // In case of a reload
    mSpellDepMap.clear();
    mSpellRevDepMap.clear();

    std::unique_ptr<QueryResult> result(WorldDatabase.PQuery(
        "SELECT dependency_id, spell_id FROM spell_dependencies"));
    if (!result)
    {
        logging.info("Table `spell_dependencies` is empty\n");
        return;
    }

    BarGoLink bar(result->GetRowCount());
    uint32 count = 0;

    do
    {
        bar.step();
        Field* f = result->Fetch();

        uint32 dependency_id = f[0].GetUInt32();
        uint32 spell_id = f[1].GetUInt32();

        if (!sSpellStore.LookupEntry(spell_id))
        {
            logging.error(
                "spell_dependencies: entry spell_id=%u is not a spell that "
                "exists in spell_dbc",
                spell_id);
            continue;
        }

        mSpellDepMap.insert(std::make_pair(dependency_id, spell_id));
        mSpellRevDepMap.insert(std::make_pair(spell_id, dependency_id));
        ++count;
    } while (result->NextRow());

    logging.info("Loaded %u spell dependencies\n", count);
}

void SpellMgr::LoadSpellProcExceptions()
{
    // In case of a reload
    mSpellProcExceptionMap.clear();

    std::unique_ptr<QueryResult> result(WorldDatabase.PQuery(
        "SELECT sid, list, white FROM spell_proc_exception"));
    if (!result)
    {
        logging.info("Table `spell_proc_exception` is empty\n");
        return;
    }

    BarGoLink bar(result->GetRowCount());
    uint32 count = 0;

    do
    {
        bar.step();
        Field* fields = result->Fetch();

        uint32 spell_id = fields[0].GetUInt32();

        spell_proc_exception exc;
        exc.whitelist = fields[2].GetBool();

        std::string list = fields[1].GetCppString();
        std::stringstream ss(list);

        uint32 id;
        while (ss >> id)
            exc.ids.push_back(id);

        if (!sSpellStore.LookupEntry(spell_id))
        {
            logging.error(
                "spell_proc_exception: entry sid=%u is not a spell that exists "
                "in spell_dbc",
                spell_id);
            continue;
        }

        if (GetFirstSpellInChain(spell_id) != spell_id)
        {
            logging.error(
                "spell_proc_exception: entry sid=%u is not the first rank in "
                "its spell chain",
                spell_id);
            continue;
        }

        if (exc.ids.empty())
        {
            logging.error(
                "spell_proc_exception: entry sid=%u defines empty list",
                spell_id);
            continue;
        }

        bool ok = true;
        std::set<uint32> tmp;
        for (auto id : exc.ids)
        {
            if (tmp.count(id))
            {
                logging.error(
                    "spell_proc_exception: entry sid=%u defines exception (%u) "
                    "multiple times",
                    spell_id, id);
                ok = false;
            }
            tmp.insert(id);
            if (!sSpellStore.LookupEntry(id))
            {
                logging.error(
                    "spell_proc_exception: entry sid=%u defines invalid "
                    "exception (%u)",
                    spell_id, id);
                ok = false;
            }
            if (GetFirstSpellInChain(id) != id)
            {
                logging.error(
                    "spell_proc_exception: entry sid=%u, spell in list (%u) is "
                    "not the first rank in its spell chain",
                    spell_id, id);
                continue;
            }
        }

        if (!ok)
            continue;

        mSpellProcExceptionMap[spell_id] = exc;
        ++count;
    } while (result->NextRow());

    logging.info("Loaded %u spell proc exceptions\n", count);
}

void multipart_walk_children(std::set<uint32>& chain,
    const SpellMultipartAuraMap& multipart_map,
    const std::vector<uint32>& children)
{
    for (const auto& child : children)
    {
        if (chain.count(child))
        {
            throw std::runtime_error(
                "Invalid data in `spell_multipart_auras`; possible "
                "cyclic reference");
        }
        chain.insert(child);
        auto itr = multipart_map.find(child);
        if (itr != multipart_map.end())
        {
            if (chain.count(itr->first))
                throw std::runtime_error(
                    "Invalid data in `spell_multipart_auras`; possible "
                    "cyclic reference");
            chain.insert(itr->first);
            // Recursive call
            multipart_walk_children(chain, multipart_map, itr->second);
        }
    }
};

void SpellMgr::LoadSpellMultipartAuras()
{
    mSpellMultipartAuraMap.set_empty_key(0);

    std::unique_ptr<QueryResult> result(WorldDatabase.PQuery(
        "SELECT parent, child FROM spell_multipart_auras"));
    if (!result)
    {
        logging.info("Table `spell_multipart_auras` is empty\n");
        return;
    }

    BarGoLink bar(result->GetRowCount());
    uint32 count = 0;

    do
    {
        bar.step();
        Field* fields = result->Fetch();

        uint32 parent = fields[0].GetUInt32();
        uint32 child = fields[1].GetUInt32();

        if (!sSpellStore.LookupEntry(parent) || !sSpellStore.LookupEntry(child))
        {
            logging.error(
                "Table `spell_multipart_auras` has invalid entry (%u, %u)\n",
                parent, child);
            continue;
        }

        mSpellMultipartAuraMap[parent].push_back(child);
        ++count;
    } while (result->NextRow());

    // Check for potential cyclic references.
    // Any multipart setup that results in the same aura being applied twice is
    // considered invalid.
    std::set<uint32> chain;
    for (const auto& pair : mSpellMultipartAuraMap)
    {
        chain.clear();
        auto parent = pair.first;
        chain.insert(parent);
        multipart_walk_children(chain, mSpellMultipartAuraMap, pair.second);
    }

    logging.info("Loaded %u spell multipart auras\n", count);
}
