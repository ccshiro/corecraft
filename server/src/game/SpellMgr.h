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

#ifndef _SPELLMGR_H
#define _SPELLMGR_H

#include "Common.h"
#include "DBCStores.h"
#include "DBCStructure.h"
#include "SQLStorages.h"
#include "SharedDefines.h"
#include "SpellAuraDefines.h"
#include <map>
#include <unordered_map>
#include <unordered_set>

class Player;
class Spell;
struct SpellModifier;
class Unit;
class spell_trigger_type;

// only used in code
enum SpellCategories
{
    SPELLCATEGORY_HEALTH_MANA_POTIONS = 4,
    SPELLCATEGORY_DEVOUR_MAGIC = 12
};

// Spell clasification
enum SpellSpecific
{
    SPELL_SPECIFIC_NONE = 0,
    SPELL_SEAL = 1,
    SPELL_BLESSING = 2,
    SPELL_PALADIN_AURA = 3,
    SPELL_STING = 4,
    SPELL_CURSE = 5,
    SPELL_ASPECT = 6,
    SPELL_TRACKING = 7,
    SPELL_WARLOCK_ARMOR = 8,
    SPELL_MAGE_ARMOR = 9,
    SPELL_SHAMAN_SHIELD = 10,
    SPELL_MAGE_POLYMORPH = 11,
    SPELL_WARRIOR_SHOUT = 12,
    SPELL_JUDGEMENT = 13,
    SPELL_BATTLE_ELIXIR = 14,
    SPELL_GUARDIAN_ELIXIR = 15,
    SPELL_FLASK = 16,
    SPELL_WELL_FED = 17,
    SPELL_FOOD = 18,
    SPELL_DRINK = 19,
    SPELL_FOOD_AND_DRINK = 20,
    SPELL_SCREECH = 21,
    SPELL_CORRUPTION = 22,
    SPELL_AMPLIFY_DAMPEN = 23,
    SPELL_FAERIE_FIRE = 24,
    SPELL_MANGLE = 25,
    SPELL_KARAZHAN_BOOKS = 26,
};

// returns: true if partial resistance applies, false if binary resistance
// applies
bool IsPartiallyResistable(const SpellEntry* spell);

// Different spell properties
inline float GetSpellRadius(SpellRadiusEntry const* radius)
{
    return (radius ? radius->Radius : 0);
}
uint32 GetSpellCastTime(SpellEntry const* spellInfo,
    Spell const* spell = nullptr, bool peak = false);
uint32 GetSpellCastTimeForBonus(
    SpellEntry const* spellProto, DamageEffectType damagetype);
float CalculateDefaultCoefficient(
    SpellEntry const* spellProto, DamageEffectType const damagetype);
inline float GetSpellMinRange(SpellRangeEntry const* range)
{
    return (range ? range->minRange : 0);
}
inline float GetSpellMaxRange(SpellRangeEntry const* range)
{
    return (range ? range->maxRange : 0);
}
inline uint32 GetSpellRecoveryTime(SpellEntry const* spellInfo)
{
    return spellInfo->RecoveryTime > spellInfo->CategoryRecoveryTime ?
               spellInfo->RecoveryTime :
               spellInfo->CategoryRecoveryTime;
}
int32 GetSpellDuration(SpellEntry const* spellInfo);
int32 GetSpellMaxDuration(SpellEntry const* spellInfo);
int32 CalculateSpellDuration(
    SpellEntry const* spellInfo, Unit const* caster = nullptr);
uint16 GetSpellAuraMaxTicks(SpellEntry const* spellInfo);
uint16 GetSpellAuraMaxTicks(uint32 spellId);
WeaponAttackType GetWeaponAttackType(SpellEntry const* spellInfo);

float GetSpellRadius(const SpellEntry* info);

bool IsSpellReflectable(
    const SpellEntry* info, const spell_trigger_type& trigger_type);
// the spell IsSpellReflectable(), but the effect will just be ignored
bool IsSpellReflectIgnored(const SpellEntry* info);

bool CanSpellHitFriendlyTarget(const SpellEntry* info);

bool IsAffectableByLvlDmgCalc(const SpellEntry* info, SpellEffectIndex index);

inline bool IsDamagingEffect(const SpellEntry* info, SpellEffectIndex index)
{
    switch (info->Effect[index])
    {
    case SPELL_EFFECT_SCHOOL_DAMAGE:
    case SPELL_EFFECT_WEAPON_DAMAGE_NOSCHOOL:
    case SPELL_EFFECT_WEAPON_PERCENT_DAMAGE:
    case SPELL_EFFECT_WEAPON_DAMAGE:
    case SPELL_EFFECT_NORMALIZED_WEAPON_DMG:
        return true;

    default:
        break;
    }

    return false;
}

inline bool IsDamagingSpell(const SpellEntry* info)
{
    for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        if (IsDamagingEffect(info, SpellEffectIndex(i)))
            return true;
    }

    return false;
}

inline bool TriggersDamagingSpell(const SpellEntry* info)
{
    for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        switch (info->Effect[i])
        {
        case SPELL_EFFECT_TRIGGER_MISSILE:
        case SPELL_EFFECT_TRIGGER_SPELL:
        case SPELL_EFFECT_TRIGGER_SPELL_WITH_VALUE:
            if (auto triggered =
                    sSpellStore.LookupEntry(info->EffectTriggerSpell[i]))
                if (IsDamagingSpell(triggered))
                    return true;
        default:
            break;
        }
    }

    return false;
}

inline bool IsSpellHaveEffect(SpellEntry const* spellInfo, SpellEffects effect)
{
    for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
        if (SpellEffects(spellInfo->Effect[i]) == effect)
            return true;
    return false;
}

inline bool IsAuraApplyEffect(
    SpellEntry const* spellInfo, SpellEffectIndex effecIdx)
{
    switch (spellInfo->Effect[effecIdx])
    {
    case SPELL_EFFECT_APPLY_AURA:
    case SPELL_EFFECT_APPLY_AREA_AURA_PARTY:
    case SPELL_EFFECT_APPLY_AREA_AURA_PET:
    case SPELL_EFFECT_APPLY_AREA_AURA_FRIEND:
    case SPELL_EFFECT_APPLY_AREA_AURA_ENEMY:
    case SPELL_EFFECT_APPLY_AREA_AURA_OWNER:
        return true;
    }
    return false;
}

inline bool IsSpellAppliesAura(SpellEntry const* spellInfo,
    uint32 effectMask = ((1 << EFFECT_INDEX_0) | (1 << EFFECT_INDEX_1) |
                         (1 << EFFECT_INDEX_2)))
{
    for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
        if (effectMask & (1 << i))
            if (IsAuraApplyEffect(spellInfo, SpellEffectIndex(i)))
                return true;

    return false;
}

inline bool IsEffectPartOfDamageCalculation(
    SpellEntry const* spellInfo, SpellEffectIndex effecIdx)
{
    switch (spellInfo->Effect[effecIdx])
    {
    case SPELL_EFFECT_SCHOOL_DAMAGE:
    case SPELL_EFFECT_HEALTH_LEECH:
    case SPELL_EFFECT_WEAPON_DAMAGE_NOSCHOOL:
    case SPELL_EFFECT_WEAPON_PERCENT_DAMAGE:
    case SPELL_EFFECT_WEAPON_DAMAGE:
    case SPELL_EFFECT_NORMALIZED_WEAPON_DMG:
        return true;
    default:
        return false;
    }
}

inline bool IsPeriodicRegenerateEffect(
    SpellEntry const* spellInfo, SpellEffectIndex effecIdx)
{
    switch (AuraType(spellInfo->EffectApplyAuraName[effecIdx]))
    {
    case SPELL_AURA_PERIODIC_ENERGIZE:
    case SPELL_AURA_PERIODIC_HEAL:
    case SPELL_AURA_PERIODIC_HEALTH_FUNNEL:
        return true;
    default:
        return false;
    }
}

inline bool IsSpellHaveAura(SpellEntry const* spellInfo, AuraType aura)
{
    for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
        if (AuraType(spellInfo->EffectApplyAuraName[i]) == aura)
            return true;
    return false;
}

inline bool IsSpellLastAuraEffect(
    SpellEntry const* spellInfo, SpellEffectIndex effecIdx)
{
    for (int i = effecIdx + 1; i < MAX_EFFECT_INDEX; ++i)
        if (spellInfo->EffectApplyAuraName[i])
            return false;
    return true;
}

inline bool IsSealSpell(SpellEntry const* spellInfo)
{
    // Collection of all the seal family flags. No other paladin spell has any
    // of those.
    return spellInfo->IsFitToFamily(
               SPELLFAMILY_PALADIN, UI64LIT(0x000004000A000200)) &&
           // avoid counting target triggered effect as seal for avoid remove it
           // or seal by it.
           spellInfo->EffectImplicitTargetA[0] == TARGET_SELF;
}

inline bool IsElementalShield(SpellEntry const* spellInfo)
{
    // family flags 10 (Lightning), 42 (Earth), 37 (Water), proc shield from T2
    // 8 pieces bonus
    return (spellInfo->SpellFamilyFlags & UI64LIT(0x42000000400)) ||
           spellInfo->Id == 23552;
}

int32 CompareAuraRanks(uint32 spellId_1, uint32 spellId_2);

// Spells with the attribute: "Only one {BuffType} per {class} can be active on
// any one target"
inline bool is_one_buff_per_caster_spell(const SpellEntry* spell)
{
    switch (spell->SpellSpecific)
    {
    case SPELL_BLESSING:
    case SPELL_PALADIN_AURA:
    case SPELL_STING:
    case SPELL_CURSE:
    case SPELL_ASPECT:
    case SPELL_WARRIOR_SHOUT:
    case SPELL_JUDGEMENT:
    case SPELL_CORRUPTION:
        return true;
    }
    return false;
}

// Spells with the attribute: "Only one {BuffType} can be active [on a target]
// at any one time"
inline bool is_exclusive_buff_spell(const SpellEntry* spell)
{
    switch (spell->SpellSpecific)
    {
    case SPELL_SEAL:
    case SPELL_TRACKING:
    case SPELL_WARLOCK_ARMOR:
    case SPELL_MAGE_ARMOR:
    case SPELL_SHAMAN_SHIELD:
    case SPELL_MAGE_POLYMORPH:
    case SPELL_WELL_FED:
    case SPELL_AMPLIFY_DAMPEN:
    case SPELL_MANGLE:
    case SPELL_KARAZHAN_BOOKS:
        return true;
    }
    return false;
}

// Spells with the attribute: "Counts as both a X and a Y".
inline bool is_group_exclusive_buff_spell(
    const SpellEntry* spell, std::vector<SpellSpecific>& out)
{
    switch (spell->SpellSpecific)
    {
    case SPELL_BATTLE_ELIXIR:
        out.push_back(SPELL_BATTLE_ELIXIR);
        out.push_back(SPELL_FLASK);
        return true;
    case SPELL_GUARDIAN_ELIXIR:
        out.push_back(SPELL_GUARDIAN_ELIXIR);
        out.push_back(SPELL_FLASK);
        return true;
    case SPELL_FLASK:
        out.push_back(SPELL_BATTLE_ELIXIR);
        out.push_back(SPELL_GUARDIAN_ELIXIR);
        out.push_back(SPELL_FLASK);
        return true;
        /* NOTE: Food & Drink & FOOD_AND_DRINK are skipped for now; the common
        cases is already handled by normal
        buff stacking rules, if they end up being needed, mangos defined their
        rules like this previously: */
        /*
        case SPELL_FOOD: return spellSpec2==SPELL_FOOD ||
        spellSpec2==SPELL_FOOD_AND_DRINK;
        case SPELL_DRINK: return spellSpec2==SPELL_DRINK ||
        spellSpec2==SPELL_FOOD_AND_DRINK;
        case SPELL_FOOD_AND_DRINK: return spellSpec2==SPELL_FOOD ||
        spellSpec2==SPELL_DRINK || spellSpec2==SPELL_FOOD_AND_DRINK;
        */
    }
    return false;
}

// Spells that only care about other spells with the same specific
inline bool is_same_spell_spec_only(uint32 spec)
{
    switch (spec)
    {
    case SPELL_ASPECT:
    case SPELL_SHAMAN_SHIELD:
    case SPELL_SCREECH:
    case SPELL_FAERIE_FIRE:
    case SPELL_WARLOCK_ARMOR:
    case SPELL_MAGE_ARMOR:
        return true;
    default:
        break;
    }
    return false;
}

// Spells that don't care about stacking except for spell specific
inline bool is_spell_spec_only(uint32 spec)
{
    switch (spec)
    {
    case SPELL_BATTLE_ELIXIR:
    case SPELL_GUARDIAN_ELIXIR:
    case SPELL_FLASK:
        return true;
    default:
        break;
    }
    return false;
}

bool IsPassiveSpell(uint32 spellId);
inline bool IsPassiveSpell(const SpellEntry* spellInfo)
{
    return spellInfo->HasAttribute(SPELL_ATTR_PASSIVE);
}

inline bool IsSpellRemoveAllMovementAndControlLossEffects(
    SpellEntry const* spellProto)
{
    return spellProto->EffectApplyAuraName[EFFECT_INDEX_0] ==
               SPELL_AURA_MECHANIC_IMMUNITY &&
           spellProto->EffectMiscValue[EFFECT_INDEX_0] == 1 &&
           spellProto->EffectApplyAuraName[EFFECT_INDEX_1] == 0 &&
           spellProto->EffectApplyAuraName[EFFECT_INDEX_2] == 0 &&
           spellProto->HasAttribute(SPELL_ATTR_EX_DISPEL_AURAS_ON_IMMUNITY);
}

inline bool IsDeathOnlySpell(SpellEntry const* spellInfo)
{
    return spellInfo->HasAttribute(SPELL_ATTR_EX3_CAST_ON_DEAD) ||
           spellInfo->Id == 2584;
}

inline bool IsDeathPersistentSpell(SpellEntry const* spellInfo)
{
    return spellInfo->HasAttribute(SPELL_ATTR_EX3_DEATH_PERSISTENT);
}

// note: this does not apply to auras, but only to new spell casts
bool CanBeCastOnDeadTargets(const SpellEntry* spellInfo);

inline bool IsNonCombatSpell(SpellEntry const* spellInfo)
{
    return spellInfo->HasAttribute(SPELL_ATTR_CANT_USED_IN_COMBAT);
}

bool IsPositiveSpell(uint32 spellId);
bool IsPositiveSpell(SpellEntry const* spellproto);
bool IsPositiveEffect(SpellEntry const* spellInfo, SpellEffectIndex effIndex);
bool IsPositiveTarget(uint32 targetA, uint32 targetB);

bool IsExplicitPositiveTarget(uint32 targetA);
bool IsExplicitNegativeTarget(uint32 targetA);

inline bool IsCasterSourceTarget(uint32 target)
{
    switch (target)
    {
    case TARGET_SELF:
    case TARGET_PET:
    case TARGET_ALL_PARTY_AROUND_CASTER:
    case TARGET_IN_FRONT_OF_CASTER:
    case TARGET_MASTER:
    case TARGET_MINION:
    case TARGET_ALL_PARTY:
    case TARGET_ALL_PARTY_AROUND_CASTER_2:
    case TARGET_SELF_FISHING:
    case TARGET_TOTEM_EARTH:
    case TARGET_TOTEM_WATER:
    case TARGET_TOTEM_AIR:
    case TARGET_TOTEM_FIRE:
    case TARGET_AREAEFFECT_GO_AROUND_DEST:
    case TARGET_SELF2:
    case TARGET_NONCOMBAT_PET:
        return true;
    default:
        break;
    }
    return false;
}

inline bool IsSpellWithCasterSourceTargetsOnly(SpellEntry const* spellInfo)
{
    for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        uint32 targetA = spellInfo->EffectImplicitTargetA[i];
        if (targetA && !IsCasterSourceTarget(targetA))
            return false;

        uint32 targetB = spellInfo->EffectImplicitTargetB[i];
        if (targetB && !IsCasterSourceTarget(targetB))
            return false;

        if (!targetA && !targetB)
            return false;
    }
    return true;
}

inline bool IsPointEffectTarget(Targets target)
{
    switch (target)
    {
    case TARGET_INNKEEPER_COORDINATES:
    case TARGET_TABLE_X_Y_Z_COORDINATES:
    case TARGET_CASTER_COORDINATES:
    case TARGET_SCRIPT_COORDINATES:
    case TARGET_CURRENT_ENEMY_COORDINATES:
    case TARGET_DUELVSPLAYER_COORDINATES:
    case TARGET_DYNAMIC_OBJECT_COORDINATES:
    case TARGET_POINT_AT_NORTH:
    case TARGET_POINT_AT_SOUTH:
    case TARGET_POINT_AT_EAST:
    case TARGET_POINT_AT_WEST:
    case TARGET_POINT_AT_NE:
    case TARGET_POINT_AT_NW:
    case TARGET_POINT_AT_SE:
    case TARGET_POINT_AT_SW:
        return true;
    default:
        break;
    }
    return false;
}

inline bool IsAreaEffectPossitiveTarget(Targets target)
{
    switch (target)
    {
    case TARGET_ALL_PARTY_AROUND_CASTER:
    case TARGET_ALL_FRIENDLY_UNITS_AROUND_CASTER:
    case TARGET_ALL_FRIENDLY_UNITS_IN_AREA:
    case TARGET_ALL_PARTY:
    case TARGET_ALL_PARTY_AROUND_CASTER_2:
    case TARGET_AREAEFFECT_PARTY:
    case TARGET_ALL_RAID_AROUND_CASTER:
    case TARGET_AREAEFFECT_PARTY_AND_CLASS:
        return true;
    default:
        break;
    }
    return false;
}

inline bool IsAreaEffectTarget(Targets target)
{
    switch (target)
    {
    case TARGET_AREAEFFECT_INSTANT:
    case TARGET_AREAEFFECT_CUSTOM:
    case TARGET_ALL_ENEMY_IN_AREA:
    case TARGET_ALL_ENEMY_IN_AREA_INSTANT:
    case TARGET_ALL_PARTY_AROUND_CASTER:
    case TARGET_IN_FRONT_OF_CASTER:
    case TARGET_ALL_ENEMY_IN_AREA_CHANNELED:
    case TARGET_ALL_FRIENDLY_UNITS_AROUND_CASTER:
    case TARGET_ALL_FRIENDLY_UNITS_IN_AREA:
    case TARGET_ALL_PARTY:
    case TARGET_ALL_PARTY_AROUND_CASTER_2:
    case TARGET_AREAEFFECT_PARTY:
    case TARGET_AREAEFFECT_GO_AROUND_DEST:
    case TARGET_ALL_RAID_AROUND_CASTER:
    case TARGET_AREAEFFECT_PARTY_AND_CLASS:
    case TARGET_LARGE_FRONTAL_CONE:
    case TARGET_NARROW_FRONTAL_CONE:
        return true;
    default:
        break;
    }
    return false;
}

inline bool IsAreaOfEffectIndex(const SpellEntry* info, SpellEffectIndex i)
{
    return IsAreaEffectTarget(Targets(info->EffectImplicitTargetA[i])) ||
           IsAreaEffectTarget(Targets(info->EffectImplicitTargetB[i]));
}

inline bool IsAreaOfEffectSpell(SpellEntry const* spellInfo)
{
    if (IsAreaOfEffectIndex(spellInfo, EFFECT_INDEX_0))
        return true;
    if (IsAreaOfEffectIndex(spellInfo, EFFECT_INDEX_1))
        return true;
    if (IsAreaOfEffectIndex(spellInfo, EFFECT_INDEX_2))
        return true;
    return false;
}

inline bool IsAreaAuraEffect(uint32 effect)
{
    if (effect == SPELL_EFFECT_APPLY_AREA_AURA_PARTY ||
        effect == SPELL_EFFECT_APPLY_AREA_AURA_FRIEND ||
        effect == SPELL_EFFECT_APPLY_AREA_AURA_ENEMY ||
        effect == SPELL_EFFECT_APPLY_AREA_AURA_PET ||
        effect == SPELL_EFFECT_APPLY_AREA_AURA_OWNER)
        return true;
    return false;
}

inline bool HasAreaAuraEffect(SpellEntry const* spellInfo)
{
    for (int32 i = 0; i < MAX_EFFECT_INDEX; ++i)
        if (IsAreaAuraEffect(spellInfo->Effect[i]))
            return true;
    return false;
}

inline bool HasAuraWithTriggerEffect(SpellEntry const* spellInfo)
{
    for (int32 i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        switch (spellInfo->Effect[i])
        {
        case SPELL_AURA_PERIODIC_TRIGGER_SPELL:
        case SPELL_AURA_PROC_TRIGGER_SPELL:
        case SPELL_AURA_PROC_TRIGGER_DAMAGE:
        case SPELL_AURA_PROC_TRIGGER_SPELL_WITH_VALUE:
            return true;
        }
    }
    return false;
}

inline bool IsDispelSpell(SpellEntry const* spellInfo)
{
    return IsSpellHaveEffect(spellInfo, SPELL_EFFECT_DISPEL);
}

inline bool isSpellBreakStealth(SpellEntry const* spellInfo)
{
    return !spellInfo->HasAttribute(SPELL_ATTR_EX_NOT_BREAK_STEALTH);
}

inline bool IsAutoRepeatRangedSpell(SpellEntry const* spellInfo)
{
    return spellInfo->HasAttribute(SPELL_ATTR_RANGED) &&
           spellInfo->HasAttribute(SPELL_ATTR_EX2_AUTOREPEAT_FLAG);
}

inline bool IsSpellRequiresRangedAP(SpellEntry const* spellInfo)
{
    return (spellInfo->SpellFamilyName == SPELLFAMILY_HUNTER &&
            spellInfo->DmgClass != SPELL_DAMAGE_CLASS_MELEE);
}

SpellCastResult GetErrorAtShapeshiftedCast(
    SpellEntry const* spellInfo, uint32 form);

inline bool IsChanneledSpell(SpellEntry const* spellInfo)
{
    return spellInfo->HasAttribute(SPELL_ATTR_EX_CHANNELED_1) ||
           spellInfo->HasAttribute(SPELL_ATTR_EX_CHANNELED_2);
}

inline bool IsChainTargetSpell(const SpellEntry* spellInfo)
{
    return spellInfo->EffectChainTarget[0] > 1 ||
           spellInfo->EffectChainTarget[0] > 2 ||
           spellInfo->EffectChainTarget[0] > 3;
}

inline bool IsNeedCastSpellAtFormApply(
    SpellEntry const* spellInfo, ShapeshiftForm form)
{
    if ((!spellInfo->HasAttribute(SPELL_ATTR_PASSIVE) &&
            !spellInfo->HasAttribute(SPELL_ATTR_UNK7)) ||
        !form)
        return false;

    // passive spells with SPELL_ATTR_EX2_NOT_NEED_SHAPESHIFT are already active
    // without shapeshift, do no recast!
    return (spellInfo->Stances & (1 << (form - 1)) &&
            !spellInfo->HasAttribute(SPELL_ATTR_EX2_NOT_NEED_SHAPESHIFT));
}

inline bool NeedsComboPoints(SpellEntry const* spellInfo)
{
    return spellInfo->HasAttribute(SPELL_ATTR_EX_REQ_TARGET_COMBO_POINTS) ||
           spellInfo->HasAttribute(SPELL_ATTR_EX_REQ_COMBO_POINTS);
}

inline SpellSchoolMask GetSpellSchoolMask(SpellEntry const* spellInfo)
{
    return SpellSchoolMask(spellInfo->SchoolMask);
}

inline uint32 GetSpellMechanicMask(
    SpellEntry const* spellInfo, uint32 effectMask)
{
    uint32 mask = 0;
    if (spellInfo->Mechanic)
        mask |= 1 << (spellInfo->Mechanic - 1);

    for (uint32 i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        if (!(effectMask & (1 << i)))
            continue;

        if (spellInfo->EffectMechanic[i])
            mask |= 1 << (spellInfo->EffectMechanic[i] - 1);
    }

    return mask;
}

inline uint32 GetAllSpellMechanicMask(SpellEntry const* spellInfo)
{
    uint32 mask = 0;
    if (spellInfo->Mechanic)
        mask |= 1 << (spellInfo->Mechanic - 1);
    for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
        if (spellInfo->EffectMechanic[i])
            mask |= 1 << (spellInfo->EffectMechanic[i] - 1);
    return mask;
}

inline uint32 GetDispelMask(DispelType dispel)
{
    // If dispell all
    if (dispel == DISPEL_ALL)
        return DISPEL_ALL_MASK;
    else
        return (1 << dispel);
}

inline bool IsAuraAddedBySpell(uint32 auraType, uint32 spellId)
{
    SpellEntry const* spellproto = sSpellStore.LookupEntry(spellId);
    if (!spellproto)
        return false;

    for (int i = 0; i < 3; i++)
        if (spellproto->EffectApplyAuraName[i] == auraType)
            return true;
    return false;
}

inline bool IsCrowdControlSpell(SpellEntry const* spellInfo)
{
    for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
        if (spellInfo->EffectApplyAuraName[i] == SPELL_AURA_MOD_STUN ||
            spellInfo->EffectApplyAuraName[i] == SPELL_AURA_MOD_CONFUSE ||
            spellInfo->EffectApplyAuraName[i] == SPELL_AURA_MOD_FEAR ||
            spellInfo->EffectApplyAuraName[i] == SPELL_AURA_MOD_CHARM ||
            spellInfo->EffectApplyAuraName[i] == SPELL_AURA_MOD_POSSESS)
            return true;

    return false;
}

/* Returns true if spell proc should only happen on cast */
bool IsSpellProcOnCast(SpellEntry const* entry);

// Diminishing Returns interaction with spells
DiminishingGroup GetDiminishingReturnsGroupForSpell(
    SpellEntry const* spellproto, bool triggered);
bool IsDiminishingReturnsGroupDurationLimited(DiminishingGroup group);
DiminishingReturnsType GetDiminishingReturnsGroupType(DiminishingGroup group);

// Spell affects related declarations (accessed using SpellMgr functions)
typedef std::map<uint32, uint64> SpellAffectMap;

// Spell proc event related declarations (accessed using SpellMgr functions)
enum ProcFlags
{
    PROC_FLAG_NONE = 0x00000000,

    PROC_FLAG_KILLED = 0x00000001, // 00 Killed by aggressor
    PROC_FLAG_KILL =
        0x00000002, // 01 Kill target (in most cases need XP/Honor reward, see
                    // Unit::IsTriggeredAtSpellProcEvent for additinoal check)

    PROC_FLAG_SUCCESSFUL_MELEE_HIT =
        0x00000004, // 02 Successful melee auto attack
    PROC_FLAG_TAKEN_MELEE_HIT =
        0x00000008, // 03 Taken damage from melee auto attack hit

    PROC_FLAG_SUCCESSFUL_MELEE_SPELL_HIT =
        0x00000010, // 04 Successful attack by Spell that use melee weapon
    PROC_FLAG_TAKEN_MELEE_SPELL_HIT =
        0x00000020, // 05 Taken damage by Spell that use melee weapon

    PROC_FLAG_SUCCESSFUL_RANGED_HIT =
        0x00000040, // 06 Successful Ranged auto attack
    PROC_FLAG_TAKEN_RANGED_HIT =
        0x00000080, // 07 Taken damage from ranged auto attack

    PROC_FLAG_SUCCESSFUL_RANGED_SPELL_HIT = 0x00000100, // 08 Successful Ranged
                                                        // attack by Spell that
                                                        // use ranged weapon
    PROC_FLAG_TAKEN_RANGED_SPELL_HIT =
        0x00000200, // 09 Taken damage by Spell that use ranged weapon

    PROC_FLAG_SUCCESSFUL_POSITIVE_AOE_HIT =
        0x00000400, // 10 Successful AoE (not 100% shure unused)
    PROC_FLAG_TAKEN_POSITIVE_AOE =
        0x00000800, // 11 Taken AoE      (not 100% shure unused)

    PROC_FLAG_SUCCESSFUL_AOE_SPELL_HIT = 0x00001000, // 12 Successful AoE damage
                                                     // spell hit (not 100%
                                                     // shure unused)
    PROC_FLAG_TAKEN_AOE_SPELL_HIT = 0x00002000, // 13 Taken AoE damage spell hit
                                                // (not 100% shure unused)

    PROC_FLAG_SUCCESSFUL_POSITIVE_SPELL =
        0x00004000, // 14 Successful cast positive spell (by default only on
                    // healing)
    PROC_FLAG_TAKEN_POSITIVE_SPELL =
        0x00008000, // 15 Taken positive spell hit (by default only on healing)

    PROC_FLAG_SUCCESSFUL_NEGATIVE_SPELL_HIT =
        0x00010000, // 16 Successful negative spell cast (by default only on
                    // damage)
    PROC_FLAG_TAKEN_NEGATIVE_SPELL_HIT =
        0x00020000, // 17 Taken negative spell (by default only on damage)

    PROC_FLAG_ON_DO_PERIODIC =
        0x00040000, // 18 Successful do periodic (damage / healing, determined
                    // by PROC_EX_PERIODIC_POSITIVE or negative if no procEx)
    PROC_FLAG_ON_TAKE_PERIODIC =
        0x00080000, // 19 Taken spell periodic (damage / healing, determined by
                    // PROC_EX_PERIODIC_POSITIVE or negative if no procEx)

    PROC_FLAG_TAKEN_ANY_DAMAGE = 0x00100000,   // 20 Taken any damage
    PROC_FLAG_ON_TRAP_ACTIVATION = 0x00200000, // 21 On trap activation

    PROC_FLAG_TAKEN_OFFHAND_HIT =
        0x00400000, // 22 Taken off-hand melee attacks(not used)
    PROC_FLAG_SUCCESSFUL_OFFHAND_HIT =
        0x00800000 // 23 Successful off-hand melee attacks
};

#define MELEE_BASED_TRIGGER_MASK                                             \
    (PROC_FLAG_SUCCESSFUL_MELEE_HIT | PROC_FLAG_TAKEN_MELEE_HIT |            \
        PROC_FLAG_SUCCESSFUL_MELEE_SPELL_HIT |                               \
        PROC_FLAG_TAKEN_MELEE_SPELL_HIT | PROC_FLAG_SUCCESSFUL_RANGED_HIT |  \
        PROC_FLAG_TAKEN_RANGED_HIT | PROC_FLAG_SUCCESSFUL_RANGED_SPELL_HIT | \
        PROC_FLAG_TAKEN_RANGED_SPELL_HIT)

#define NEGATIVE_TRIGGER_MASK                                        \
    (MELEE_BASED_TRIGGER_MASK | PROC_FLAG_SUCCESSFUL_AOE_SPELL_HIT | \
        PROC_FLAG_TAKEN_AOE_SPELL_HIT |                              \
        PROC_FLAG_SUCCESSFUL_NEGATIVE_SPELL_HIT |                    \
        PROC_FLAG_TAKEN_NEGATIVE_SPELL_HIT)

enum ProcFlagsEx
{
    PROC_EX_NONE = 0x0000000, // If none can tigger on Hit/Crit only (passive
                              // spells MUST defined by SpellFamily flag)
    PROC_EX_NORMAL_HIT =
        0x0000001, // If set only from normal hit (only damage spells)
    PROC_EX_CRITICAL_HIT = 0x0000002,
    PROC_EX_MISS = 0x0000004,
    PROC_EX_RESIST = 0x0000008,
    PROC_EX_DODGE = 0x0000010,
    PROC_EX_PARRY = 0x0000020,
    PROC_EX_BLOCK = 0x0000040,
    PROC_EX_EVADE = 0x0000080,
    PROC_EX_IMMUNE = 0x0000100,
    PROC_EX_DEFLECT = 0x0000200,
    PROC_EX_ABSORB = 0x0000400,
    PROC_EX_REFLECT = 0x0000800,
    PROC_EX_INTERRUPT =
        0x0001000, // Melee hit result can be Interrupt (not used)
    PROC_EX_RESERVED1 = 0x0002000,
    PROC_EX_RESERVED2 = 0x0004000,
    PROC_EX_RESERVED3 = 0x0008000,
    PROC_EX_EX_TRIGGER_ALWAYS = 0x0010000, // If set trigger always ( no matter
                                           // another flags) used for drop
                                           // charges
    PROC_EX_EX_ONE_TIME_TRIGGER =
        0x0020000, // If set trigger always but only one time (not used)
    PROC_EX_PERIODIC_POSITIVE = 0x0040000, // For periodic heal
    PROC_EX_DIRECT_ONLY = 0x0080000,       // Only Direct damage/heal
    PROC_EX_DAMAGING_SPELL =
        0x0100000, // Spell must have a damage component to it
    PROC_EX_ON_CAST_CRIT = 0x0200000
};

struct SpellProcEventEntry
{
    uint32 schoolMask;
    uint32 spellFamilyName; // if nonzero - for matching proc condition based on
                            // candidate spell's SpellFamilyNamer value
    ClassFamilyMask spellFamilyMask[MAX_EFFECT_INDEX]; // if nonzero - for
                                                       // matching proc
                                                       // condition based on
                                                       // candidate spell's
                                                       // SpellFamilyFlags
                                                       // (like auras 107 and
                                                       // 108 do)
    uint32 procFlags; // bitmask for matching proc event
    uint32 procEx;    // proc Extend info (see ProcFlagsEx)
    float ppmRate; // for melee (ranged?) damage spells - proc rate per minute.
                   // if zero, falls back to flat chance from Spell.dbc
    float customChance; // Owerride chance (in most cases for debug only)
    uint32 cooldown; // hidden cooldown used for some spell proc events, applied
                     // to _triggered_spell_
};

struct SpellBonusEntry
{
    float direct_damage;
    float dot_damage;
    float ap_bonus;
    float ap_dot_bonus;
};

typedef std::unordered_map<uint32, SpellProcEventEntry> SpellProcEventMap;
typedef std::unordered_map<uint32, SpellBonusEntry> SpellBonusMap;

#define ELIXIR_BATTLE_MASK 0x01
#define ELIXIR_GUARDIAN_MASK 0x02
#define ELIXIR_FLASK_MASK (ELIXIR_BATTLE_MASK | ELIXIR_GUARDIAN_MASK)
#define ELIXIR_UNSTABLE_MASK 0x04
#define ELIXIR_SHATTRATH_MASK 0x08
#define ELIXIR_WELL_FED 0x10 // Some foods have SPELLFAMILY_POTION

struct SpellThreatEntry
{
    uint16 threat;
    float multiplier;
    float ap_bonus;
};

enum SpellItemProcFlags
{
    SPELL_ITEM_ENCH_PROC_ALL = 0x0,                 // Procs on everything
    SPELL_ITEM_ENCH_PROC_ON_MELEE = 0x1,            // On auto-attacks
    SPELL_ITEM_ENCH_PROC_ANY_SPELL = 0x2,           // On any spell
    SPELL_ITEM_ENCH_PROC_ON_NEXT_MELEE_SPELL = 0x4, // On next swing spells
};

struct SpellItemProcEntry
{
    float ppm;
    uint32 flags;
};

typedef std::map<uint32, uint8> SpellElixirMap;
typedef std::map<uint32, SpellItemProcEntry> SpellProcItemEnchantMap;
typedef std::map<uint32, SpellThreatEntry> SpellThreatMap;

// Spell script target related declarations (accessed using SpellMgr functions)
enum SpellTargetType
{
    SPELL_TARGET_TYPE_GAMEOBJECT = 0,
    SPELL_TARGET_TYPE_CREATURE = 1,
    SPELL_TARGET_TYPE_DEAD = 2
};

#define MAX_SPELL_TARGET_TYPE 3

struct SpellTargetEntry
{
    SpellTargetEntry(SpellTargetType type_, uint32 targetEntry_)
      : type(type_), targetEntry(targetEntry_)
    {
    }
    SpellTargetType type;
    uint32 targetEntry;
};

typedef std::multimap<uint32, SpellTargetEntry> SpellScriptTarget;
typedef std::pair<SpellScriptTarget::const_iterator,
    SpellScriptTarget::const_iterator> SpellScriptTargetBounds;

// coordinates for spells (accessed using SpellMgr functions)
struct SpellTargetPosition
{
    uint32 target_mapId;
    float target_X;
    float target_Y;
    float target_Z;
    float target_Orientation;
};

typedef std::unordered_map<uint32, SpellTargetPosition> SpellTargetPositionMap;

// Spell pet auras
class PetAura
{
public:
    PetAura() { auras.clear(); }

    PetAura(uint32 petEntry, uint32 aura, bool _removeOnChangePet, int _damage)
      : removeOnChangePet(_removeOnChangePet), damage(_damage)
    {
        auras[petEntry] = aura;
    }

    uint32 GetAura(uint32 petEntry) const
    {
        auto itr = auras.find(petEntry);
        if (itr != auras.end())
            return itr->second;
        else
        {
            auto itr2 = auras.find(0);
            if (itr2 != auras.end())
                return itr2->second;
            else
                return 0;
        }
    }

    void AddAura(uint32 petEntry, uint32 aura) { auras[petEntry] = aura; }

    bool IsRemovedOnChangePet() const { return removeOnChangePet; }

    int32 GetDamage() const { return damage; }

private:
    std::map<uint32, uint32> auras;
    bool removeOnChangePet;
    int32 damage;
};
typedef std::map<uint16, PetAura> SpellPetAuraMap;

struct SpellArea
{
    uint32 spellId;
    uint32 areaId;     // zone/subzone/or 0 is not limited to zone
    uint32 questStart; // quest start (quest must be active or rewarded for
                       // spell apply)
    uint32 questEnd; // quest end (quest don't must be rewarded for spell apply)
    int32 auraSpell; // spell aura must be applied for spell apply )if
                     // possitive) and it don't must be applied in other case
    uint32 raceMask; // can be applied only to races
    Gender gender;   // can be applied only to gender
    bool questStartCanActive; // if true then quest start can be active (not
                              // only rewarded)
    bool autocast; // if true then auto applied at area enter, in other case
                   // just allowed to cast

    // helpers
    bool IsFitToRequirements(
        Player const* player, uint32 newZone, uint32 newArea) const;
};

typedef std::multimap<uint32, SpellArea> SpellAreaMap;
typedef std::multimap<uint32, SpellArea const*> SpellAreaForQuestMap;
typedef std::multimap<uint32, SpellArea const*> SpellAreaForAuraMap;
typedef std::multimap<uint32, SpellArea const*> SpellAreaForAreaMap;
typedef std::pair<SpellAreaMap::const_iterator, SpellAreaMap::const_iterator>
    SpellAreaMapBounds;
typedef std::pair<SpellAreaForQuestMap::const_iterator,
    SpellAreaForQuestMap::const_iterator> SpellAreaForQuestMapBounds;
typedef std::pair<SpellAreaForAuraMap::const_iterator,
    SpellAreaForAuraMap::const_iterator> SpellAreaForAuraMapBounds;
typedef std::pair<SpellAreaForAreaMap::const_iterator,
    SpellAreaForAreaMap::const_iterator> SpellAreaForAreaMapBounds;

// Spell rank chain  (accessed using SpellMgr functions)
struct SpellChainNode
{
    uint32 prev;
    std::vector<uint32> next; // There can be multiple next spells (such as
                              // Blacksmith -> Armorsmith or Weaponsmith)
    uint32 first;
    uint32 req;
    uint8 rank;
};

typedef std::unordered_map<uint32, SpellChainNode> SpellChainMap;

// Spell learning properties (accessed using SpellMgr functions)
struct SpellLearnSkillNode
{
    uint16 skill;
    uint16 step;
    uint16 value;    // 0  - max skill value for player level
    uint16 maxvalue; // 0  - max skill value for player level
};

typedef std::map<uint32, SpellLearnSkillNode> SpellLearnSkillMap;

struct SpellLearnSpellNode
{
    uint32 spell;
    bool active; // show in spellbook or not
    bool autoLearned;
};

typedef std::multimap<uint32, SpellLearnSpellNode> SpellLearnSpellMap;
typedef std::pair<SpellLearnSpellMap::const_iterator,
    SpellLearnSpellMap::const_iterator> SpellLearnSpellMapBounds;

typedef std::multimap<uint32, SkillLineAbilityEntry const*> SkillLineAbilityMap;
typedef std::pair<SkillLineAbilityMap::const_iterator,
    SkillLineAbilityMap::const_iterator> SkillLineAbilityMapBounds;

typedef std::multimap<uint32, SkillRaceClassInfoEntry const*>
    SkillRaceClassInfoMap;
typedef std::pair<SkillRaceClassInfoMap::const_iterator,
    SkillRaceClassInfoMap::const_iterator> SkillRaceClassInfoMapBounds;

typedef std::map<uint32 /*dependency*/, uint32> SpellDepMap;
typedef std::map<uint32 /*child spell*/, uint32> SpellRevDepMap;
typedef std::pair<SpellDepMap::const_iterator, SpellDepMap::const_iterator>
    SpellDepBounds;
typedef std::pair<SpellRevDepMap::const_iterator,
    SpellRevDepMap::const_iterator> SpellRevDepBounds;

struct spell_proc_exception
{
    bool whitelist;
    std::vector<uint32> ids;
};
typedef std::unordered_map<uint32 /*spell*/, spell_proc_exception>
    SpellProcExceptionMap;

typedef google::dense_hash_map<uint32 /*spell*/, std::vector<uint32 /*spell*/>>
    SpellMultipartAuraMap;

inline bool IsPrimaryProfessionSkill(uint32 skill)
{
    SkillLineEntry const* pSkill = sSkillLineStore.LookupEntry(skill);
    if (!pSkill)
        return false;

    if (pSkill->categoryId != SKILL_CATEGORY_PROFESSION)
        return false;

    return true;
}

inline bool IsProfessionSkill(uint32 skill)
{
    return IsPrimaryProfessionSkill(skill) || skill == SKILL_FISHING ||
           skill == SKILL_COOKING || skill == SKILL_FIRST_AID;
}

inline bool IsProfessionOrRidingSkill(uint32 skill)
{
    return IsProfessionSkill(skill) || skill == SKILL_RIDING;
}

inline bool does_direct_damage(const SpellEntry* info)
{
    return info->HasEffect(SPELL_EFFECT_SCHOOL_DAMAGE) ||
           info->HasEffect(SPELL_EFFECT_ENVIRONMENTAL_DAMAGE) ||
           info->HasEffect(SPELL_EFFECT_WEAPON_DAMAGE_NOSCHOOL) ||
           info->HasEffect(SPELL_EFFECT_WEAPON_PERCENT_DAMAGE) ||
           info->HasEffect(SPELL_EFFECT_WEAPON_DAMAGE) ||
           info->HasEffect(SPELL_EFFECT_NORMALIZED_WEAPON_DMG) ||
           info->HasEffect(SPELL_EFFECT_HEALTH_LEECH);
}

class SpellMgr
{
    friend struct DoSpellBonuses;
    friend struct DoSpellProcEvent;
    friend struct DoSpellProcItemEnchant;

    // Constructors
public:
    SpellMgr();
    ~SpellMgr();

    // Accessors (const or static functions)
public:
    // Spell affects
    ClassFamilyMask GetSpellAffectMask(
        uint32 spellId, SpellEffectIndex effectId) const
    {
        auto itr = mSpellAffectMap.find((spellId << 8) + effectId);
        if (itr != mSpellAffectMap.end())
            return ClassFamilyMask(itr->second);
        if (SpellEntry const* spellEntry = sSpellStore.LookupEntry(spellId))
            return ClassFamilyMask(spellEntry->EffectItemType[effectId]);
        return ClassFamilyMask();
    }

    SpellElixirMap const& GetSpellElixirMap() const { return mSpellElixirs; }

    uint32 GetSpellElixirMask(uint32 spellid) const
    {
        auto itr = mSpellElixirs.find(spellid);
        if (itr == mSpellElixirs.end())
            return 0x0;

        return itr->second;
    }

    /*SpellSpecific GetSpellElixirSpecific(uint32 spellid) const
    {
        uint32 mask = GetSpellElixirMask(spellid);
        if((mask & ELIXIR_FLASK_MASK)==ELIXIR_FLASK_MASK)
            return SPELL_FLASK_ELIXIR;
        else if(mask & ELIXIR_BATTLE_MASK)
            return SPELL_BATTLE_ELIXIR;
        else if(mask & ELIXIR_GUARDIAN_MASK)
            return SPELL_GUARDIAN_ELIXIR;
        else if(mask & ELIXIR_WELL_FED)
            return SPELL_WELL_FED;
        else
            return SPELL_NORMAL;
    }*/

    SpellThreatEntry const* GetSpellThreatEntry(uint32 spellid) const
    {
        auto itr = mSpellThreatMap.find(spellid);
        if (itr != mSpellThreatMap.end())
            return &itr->second;

        return nullptr;
    }

    float GetSpellThreatMultiplier(SpellEntry const* spellInfo) const
    {
        if (!spellInfo)
            return 1.0f;

        if (SpellThreatEntry const* entry = GetSpellThreatEntry(spellInfo->Id))
            return entry->multiplier;

        return 1.0f;
    }

    // Spell proc events
    SpellProcEventEntry const* GetSpellProcEvent(uint32 spellId) const
    {
        auto itr = mSpellProcEventMap.find(spellId);
        if (itr != mSpellProcEventMap.end())
            return &itr->second;
        return nullptr;
    }

    // Spell procs from item enchants
    SpellItemProcEntry GetItemEnchantProcChance(uint32 spellid) const
    {
        auto itr = mSpellProcItemEnchantMap.find(spellid);
        if (itr == mSpellProcItemEnchantMap.end())
            return SpellItemProcEntry{0.0f, 0};

        return itr->second;
    }

    static bool IsSpellProcEventCanTriggeredBy(
        SpellProcEventEntry const* spellProcEvent, uint32 EventProcFlag,
        SpellEntry const* procSpell, uint32 procFlags, uint32 procExtra);

    // Spell bonus data
    SpellBonusEntry const* GetSpellBonusData(uint32 spellId) const
    {
        // Lookup data
        auto itr = mSpellBonusMap.find(spellId);
        if (itr != mSpellBonusMap.end())
            return &itr->second;

        return nullptr;
    }

    // Spell target coordinates
    SpellTargetPosition const* GetSpellTargetPosition(uint32 spell_id) const
    {
        auto itr = mSpellTargetPositions.find(spell_id);
        if (itr != mSpellTargetPositions.end())
            return &itr->second;
        return nullptr;
    }

    // Spell ranks chains
    SpellChainNode const* GetSpellChainNode(uint32 spell_id) const
    {
        auto itr = mSpellChains.find(spell_id);
        if (itr == mSpellChains.end())
            return nullptr;

        return &itr->second;
    }

    uint32 GetFirstSpellInChain(uint32 spell_id) const
    {
        if (SpellChainNode const* node = GetSpellChainNode(spell_id))
            return node->first;

        return spell_id;
    }

    uint32 GetPrevSpellInChain(uint32 spell_id) const
    {
        if (SpellChainNode const* node = GetSpellChainNode(spell_id))
            return node->prev;

        return 0;
    }

    bool GetNextSpellInChainBoundaries(uint32 spell_id,
        std::vector<uint32>::const_iterator& lower_bound,
        std::vector<uint32>::const_iterator& upper_bound) const
    {
        if (SpellChainNode const* node = GetSpellChainNode(spell_id))
        {
            lower_bound = node->next.begin();
            upper_bound = node->next.end();
            if (lower_bound == upper_bound)
                return false;
            return true;
        }

        return false;
    }

    template <typename Worker>
    void doForHighRanks(uint32 spellid, Worker& worker)
    {
        std::vector<uint32>::const_iterator itr, lower, upper;
        if (GetNextSpellInChainBoundaries(spellid, lower, upper))
        {
            for (itr = lower; itr != upper; ++itr)
            {
                worker(*itr);
                doForHighRanks(*itr, worker);
            }
        }
    }

    // Note: not use rank for compare to spell ranks: spell chains isn't linear
    // order
    // Use IsHighRankOfSpell instead
    uint8 GetSpellRank(uint32 spell_id) const
    {
        if (SpellChainNode const* node = GetSpellChainNode(spell_id))
            return node->rank;

        return 0;
    }

    uint8 IsHighRankOfSpell(uint32 spell1, uint32 spell2) const
    {
        auto itr = mSpellChains.find(spell1);

        uint32 rank2 = GetSpellRank(spell2);

        // not ordered correctly by rank value
        if (itr == mSpellChains.end() || !rank2 || itr->second.rank <= rank2)
            return false;

        // check present in same rank chain
        for (; itr != mSpellChains.end();
             itr = mSpellChains.find(itr->second.prev))
            if (itr->second.prev == spell2)
                return true;

        return false;
    }

    bool IsRankSpellDueToSpell(
        SpellEntry const* spellInfo_1, uint32 spellId_2) const;
    bool canStackSpellRanksInSpellBook(SpellEntry const* spellInfo) const;
    bool IsRankedSpellNonStackableInSpellBook(SpellEntry const* spellInfo) const
    {
        return !canStackSpellRanksInSpellBook(spellInfo) &&
               GetSpellRank(spellInfo->Id) != 0;
    }

    SpellEntry const* SelectAuraRankForLevel(
        SpellEntry const* spellInfo, uint32 Level) const;

    // Spell learning
    SpellLearnSkillNode const* GetSpellLearnSkill(uint32 spell_id) const
    {
        auto itr = mSpellLearnSkills.find(spell_id);
        if (itr != mSpellLearnSkills.end())
            return &itr->second;
        else
            return nullptr;
    }

    bool IsSpellLearnSpell(uint32 spell_id) const
    {
        return mSpellLearnSpells.find(spell_id) != mSpellLearnSpells.end();
    }

    SpellLearnSpellMapBounds GetSpellLearnSpellMapBounds(uint32 spell_id) const
    {
        return mSpellLearnSpells.equal_range(spell_id);
    }

    bool IsSpellLearnToSpell(uint32 spell_id1, uint32 spell_id2) const
    {
        SpellLearnSpellMapBounds bounds =
            GetSpellLearnSpellMapBounds(spell_id1);
        for (auto i = bounds.first; i != bounds.second; ++i)
            if (i->second.spell == spell_id2)
                return true;
        return false;
    }

    static bool IsProfessionOrRidingSpell(uint32 spellId);
    static bool IsProfessionSpell(uint32 spellId);
    static bool IsPrimaryProfessionSpell(uint32 spellId);
    bool IsPrimaryProfessionFirstRankSpell(uint32 spellId) const;

    bool IsSkillBonusSpell(uint32 spellId) const;

    // Spell script targets
    SpellScriptTargetBounds GetSpellScriptTargetBounds(uint32 spell_id) const
    {
        return mSpellScriptTarget.equal_range(spell_id);
    }

    // Spell correctness for client using
    static bool IsSpellValid(
        SpellEntry const* spellInfo, Player* pl = nullptr, bool msg = true);

    SkillLineAbilityMapBounds GetSkillLineAbilityMapBounds(
        uint32 spell_id) const
    {
        return mSkillLineAbilityMap.equal_range(spell_id);
    }

    SkillRaceClassInfoMapBounds GetSkillRaceClassInfoMapBounds(
        uint32 skill_id) const
    {
        return mSkillRaceClassInfoMap.equal_range(skill_id);
    }

    PetAura const* GetPetAura(uint32 spell_id)
    {
        SpellPetAuraMap::const_iterator itr = mSpellPetAuraMap.find(spell_id);
        if (itr != mSpellPetAuraMap.end())
            return &itr->second;
        else
            return nullptr;
    }

    SpellCastResult GetSpellAllowedInLocationError(SpellEntry const* spellInfo,
        uint32 map_id, uint32 zone_id, uint32 area_id,
        Player const* player = nullptr);

    SpellAreaMapBounds GetSpellAreaMapBounds(uint32 spell_id) const
    {
        return mSpellAreaMap.equal_range(spell_id);
    }

    SpellAreaForQuestMapBounds GetSpellAreaForQuestMapBounds(
        uint32 quest_id, bool active) const
    {
        if (active)
            return mSpellAreaForActiveQuestMap.equal_range(quest_id);
        else
            return mSpellAreaForQuestMap.equal_range(quest_id);
    }

    SpellAreaForQuestMapBounds GetSpellAreaForQuestEndMapBounds(
        uint32 quest_id) const
    {
        return mSpellAreaForQuestEndMap.equal_range(quest_id);
    }

    SpellAreaForAuraMapBounds GetSpellAreaForAuraMapBounds(
        uint32 spell_id) const
    {
        return mSpellAreaForAuraMap.equal_range(spell_id);
    }

    SpellAreaForAreaMapBounds GetSpellAreaForAreaMapBounds(uint32 area_id) const
    {
        return mSpellAreaForAreaMap.equal_range(area_id);
    }

    // Return: range of spells that spell_id depends on
    SpellRevDepBounds GetSpellDependencies(uint32 spell_id) const
    {
        return mSpellRevDepMap.equal_range(spell_id);
    }

    // Return: range of spells that depend on dependency_id
    SpellDepBounds GetDependentSpells(uint32 dependency_id) const
    {
        return mSpellDepMap.equal_range(dependency_id);
    }

    float spell_level_calc_mana(uint32 level) const
    {
        if (level >= level_calc_.size())
            level = level_calc_.size() - 1;
        return level_calc_[level].first;
    }

    float spell_level_calc_damage(uint32 level) const
    {
        if (level >= level_calc_.size())
            level = level_calc_.size() - 1;
        return level_calc_[level].second;
    }

    // if a spell has spell proc exceptions it can always trigger stuff, even if
    // normal rules disallow it
    bool HasSpellProcExceptions(uint32 spell) const
    {
        return mSpellProcExceptionMap.find(GetFirstSpellInChain(spell)) !=
               mSpellProcExceptionMap.end();
    }

    // returns false if spell_proc_exceptions disallows child spell from
    // proccing from parent
    bool CheckSpellProcException(uint32 parent, uint32 child) const
    {
        auto itr = mSpellProcExceptionMap.find(GetFirstSpellInChain(parent));
        if (itr != mSpellProcExceptionMap.end())
        {
            auto& v = itr->second.ids;
            if (itr->second.whitelist)
                return std::find(v.begin(), v.end(),
                           GetFirstSpellInChain(child)) != v.end();
            else
                return std::find(v.begin(), v.end(),
                           GetFirstSpellInChain(child)) == v.end();
        }
        return true;
    }

    // Multi-part aura means: when @spell_id is applied all connected spells are
    // also applied, and when @spell_id expires they too are automatically
    // removed. It solves the problem of one aura being split up into multiple
    // spells needing to be treated as one.
    const std::vector<uint32>* GetMultipartAura(uint32 spell_id) const
    {
        auto itr = mSpellMultipartAuraMap.find(spell_id);
        if (itr != mSpellMultipartAuraMap.end())
            return &itr->second;
        return nullptr;
    }

    bool IgnoresLineOfSight(uint32 spell_id) const
    {
        if (auto spell = sSpellStore.LookupEntry(spell_id))
            return spell->HasAttribute(SPELL_ATTR_CUSTOM1_IGNORES_LOS);
        return false;
    }

    // Modifiers
public:
    void CheckUsedSpells(char const* table);

    // Loading data at server startup
    void LoadSpellChains();
    void LoadSpellLearnSkills();
    void LoadSpellLearnSpells();
    void LoadSpellScriptTarget();
    void LoadSpellAffects();
    void LoadSpellElixirs();
    void LoadSpellProcEvents();
    void LoadSpellProcItemEnchant();
    void LoadSpellBonuses();
    void LoadSpellTargetPositions();
    void LoadSpellThreats();
    void LoadSkillLineAbilityMap();
    void LoadSkillRaceClassInfoMap();
    void LoadSpellPetAuras();
    void LoadSpellAreas();
    void LoadSpellLevelCalc();
    void LoadSpellDependencies();
    void LoadSpellProcExceptions();
    void LoadSpellMultipartAuras();

private:
    SpellScriptTarget mSpellScriptTarget;
    SpellChainMap mSpellChains;
    SpellLearnSkillMap mSpellLearnSkills;
    SpellLearnSpellMap mSpellLearnSpells;
    SpellTargetPositionMap mSpellTargetPositions;
    SpellAffectMap mSpellAffectMap;
    SpellElixirMap mSpellElixirs;
    SpellThreatMap mSpellThreatMap;
    SpellProcEventMap mSpellProcEventMap;
    SpellProcItemEnchantMap mSpellProcItemEnchantMap;
    SpellBonusMap mSpellBonusMap;
    SkillLineAbilityMap mSkillLineAbilityMap;
    SkillRaceClassInfoMap mSkillRaceClassInfoMap;
    SpellPetAuraMap mSpellPetAuraMap;
    SpellAreaMap mSpellAreaMap;
    SpellAreaForQuestMap mSpellAreaForQuestMap;
    SpellAreaForQuestMap mSpellAreaForActiveQuestMap;
    SpellAreaForQuestMap mSpellAreaForQuestEndMap;
    SpellAreaForAuraMap mSpellAreaForAuraMap;
    SpellAreaForAreaMap mSpellAreaForAreaMap;
    std::vector<std::pair<float, float>>
        level_calc_; // pair<mana factor, damage factor>
    SpellDepMap
        mSpellDepMap; // Indexed on spell that is depended upon by the values
    SpellRevDepMap
        mSpellRevDepMap; // Indexed on spell that depends on the values
    SpellProcExceptionMap mSpellProcExceptionMap;
    SpellMultipartAuraMap mSpellMultipartAuraMap;

    std::unordered_set<uint32> mIgnoreLoSIds;
};

#define sSpellMgr MaNGOS::UnlockedSingleton<SpellMgr>
#endif
