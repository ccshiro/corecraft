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

#include "Creature.h"
#include "ObjectMgr.h"
#include "Pet.h"
#include "Player.h"
#include "SharedDefines.h"
#include "SpellAuras.h"
#include "Unit.h"

/*#######################################
########                         ########
########   PLAYERS STAT SYSTEM   ########
########                         ########
#######################################*/

bool Player::UpdateStats(Stats stat)
{
    if (stat > STAT_SPIRIT)
        return false;

    // value = ((base_value * base_pct) + total_value) * total_pct
    float value = GetTotalStatValue(stat);

    SetStat(stat, int32(value));

    if (stat == STAT_STAMINA || stat == STAT_INTELLECT)
    {
        Pet* pet = GetPet();
        if (pet)
            pet->UpdateStats(stat);
    }

    switch (stat)
    {
    case STAT_STRENGTH:
        UpdateShieldBlockValue();
        break;
    case STAT_AGILITY:
        UpdateArmor();
        UpdateAllCritPercentages();
        UpdateDodgePercentage();
        break;
    case STAT_STAMINA:
        UpdateMaxHealth();
        break;
    case STAT_INTELLECT:
        UpdateMaxPower(POWER_MANA);
        UpdateAllSpellCritChances();
        UpdateArmor(); // SPELL_AURA_MOD_RESISTANCE_OF_INTELLECT_PERCENT, only
                       // armor currently
        break;

    case STAT_SPIRIT:
        break;

    default:
        break;
    }
    // Need update (exist AP from stat auras)
    UpdateAttackPowerAndDamage();
    UpdateAttackPowerAndDamage(true);

    UpdateSpellDamageAndHealingBonus();
    UpdateManaRegen();

    return true;
}

void Player::UpdateSpellDamageAndHealingBonus()
{
    // Magic damage modifiers implemented in Unit::SpellDamageBonusDone
    // This information for client side use only
    // Get healing bonus for all schools
    SetStatInt32Value(PLAYER_FIELD_MOD_HEALING_DONE_POS,
        SpellBaseHealingBonusDone(SPELL_SCHOOL_MASK_ALL, 0));
    // Get damage bonus for all schools
    for (int i = SPELL_SCHOOL_HOLY; i < MAX_SPELL_SCHOOL; ++i)
        SetStatInt32Value(PLAYER_FIELD_MOD_DAMAGE_DONE_POS + i,
            SpellBaseDamageBonusDone(SpellSchoolMask(1 << i), 0));
}

bool Player::UpdateAllStats()
{
    for (int i = STAT_STRENGTH; i < MAX_STATS; ++i)
    {
        float value = GetTotalStatValue(Stats(i));
        SetStat(Stats(i), (int32)value);
    }

    UpdateAttackPowerAndDamage();
    UpdateAttackPowerAndDamage(true);
    UpdateArmor();
    UpdateMaxHealth();

    for (int i = POWER_MANA; i < MAX_POWERS; ++i)
        UpdateMaxPower(Powers(i));

    UpdateAllRatings();
    UpdateAllCritPercentages();
    UpdateAllSpellCritChances();
    UpdateDefenseBonusesMod();
    UpdateShieldBlockValue();
    UpdateSpellDamageAndHealingBonus();
    UpdateManaRegen();
    UpdateExpertise(BASE_ATTACK);
    UpdateExpertise(OFF_ATTACK);
    for (int i = SPELL_SCHOOL_NORMAL; i < MAX_SPELL_SCHOOL; ++i)
        UpdateResistances(i);

    return true;
}

void Player::UpdateResistances(uint32 school)
{
    if (school > SPELL_SCHOOL_NORMAL)
    {
        float value =
            GetTotalAuraModValue(UnitMods(UNIT_MOD_RESISTANCE_START + school));
        SetResistance(SpellSchools(school), int32(value));

        Pet* pet = GetPet();
        if (pet)
            pet->UpdateResistances(school);
    }
    else
        UpdateArmor();
}

void Player::UpdateArmor()
{
    float value = 0.0f;
    UnitMods unitMod = UNIT_MOD_ARMOR;

    value = GetModifierValue(unitMod, BASE_VALUE); // base armor (from items)
    value *= GetModifierValue(unitMod, BASE_PCT);  // armor percent from items

    value += GetStat(STAT_AGILITY) * 2.0f; // armor bonus from stats
    value += GetModifierValue(unitMod,
        TOTAL_VALUE); // Direct values (such as the Druid's T4 set piece bonus)

    // add dynamic flat mods
    auto& resbyIntellect =
        GetAurasByType(SPELL_AURA_MOD_RESISTANCE_OF_STAT_PERCENT);
    for (const auto& elem : resbyIntellect)
    {
        Modifier* mod = (elem)->GetModifier();
        if (mod->m_miscvalue & SPELL_SCHOOL_MASK_NORMAL)
            value += int32(GetStat(Stats((elem)->GetMiscBValue())) *
                           mod->m_amount / 100.0f);
    }

    value *= GetModifierValue(unitMod, TOTAL_PCT);

    SetArmor(int32(value));

    Pet* pet = GetPet();
    if (pet)
        pet->UpdateArmor();
}

float Player::GetHealthBonusFromStamina()
{
    float stamina = GetStat(STAT_STAMINA);

    float baseStam = stamina < 20 ? stamina : 20;
    float moreStam = stamina - baseStam;

    return baseStam + (moreStam * 10.0f);
}

float Player::GetManaBonusFromIntellect()
{
    float intellect = GetStat(STAT_INTELLECT);

    float baseInt = intellect < 20 ? intellect : 20;
    float moreInt = intellect - baseInt;

    return baseInt + (moreInt * 15.0f);
}

void Player::UpdateMaxHealth()
{
    UnitMods unitMod = UNIT_MOD_HEALTH;

    float value = GetModifierValue(unitMod, BASE_VALUE) + GetCreateHealth();
    value *= GetModifierValue(unitMod, BASE_PCT);
    value +=
        GetModifierValue(unitMod, TOTAL_VALUE) + GetHealthBonusFromStamina();
    value *= GetModifierValue(unitMod, TOTAL_PCT);

    SetMaxHealth((uint32)value);
}

void Player::UpdateMaxPower(Powers power)
{
    UnitMods unitMod = UnitMods(UNIT_MOD_POWER_START + power);

    uint32 create_power = GetCreatePowers(power);

    // ignore classes without mana
    float bonusPower = (power == POWER_MANA && create_power > 0) ?
                           GetManaBonusFromIntellect() :
                           0;

    float value = GetModifierValue(unitMod, BASE_VALUE) + create_power;
    value *= GetModifierValue(unitMod, BASE_PCT);
    value += GetModifierValue(unitMod, TOTAL_VALUE) + bonusPower;
    value *= GetModifierValue(unitMod, TOTAL_PCT);

    SetMaxPower(power, uint32(value));
}

void Player::UpdateAttackPowerAndDamage(bool ranged)
{
    float val2 = 0.0f;
    float level = float(getLevel());

    UnitMods unitMod =
        ranged ? UNIT_MOD_ATTACK_POWER_RANGED : UNIT_MOD_ATTACK_POWER;

    uint16 index = UNIT_FIELD_ATTACK_POWER;
    uint16 index_mod = UNIT_FIELD_ATTACK_POWER_MODS;
    uint16 index_mult = UNIT_FIELD_ATTACK_POWER_MULTIPLIER;

    if (ranged)
    {
        index = UNIT_FIELD_RANGED_ATTACK_POWER;
        index_mod = UNIT_FIELD_RANGED_ATTACK_POWER_MODS;
        index_mult = UNIT_FIELD_RANGED_ATTACK_POWER_MULTIPLIER;

        switch (getClass())
        {
        case CLASS_HUNTER:
            val2 = level * 2.0f + GetStat(STAT_AGILITY) - 10.0f;
            break;
        case CLASS_ROGUE:
            val2 = level + GetStat(STAT_AGILITY) - 10.0f;
            break;
        case CLASS_WARRIOR:
            val2 = level + GetStat(STAT_AGILITY) - 10.0f;
            break;
        case CLASS_DRUID:
            switch (GetShapeshiftForm())
            {
            case FORM_CAT:
            case FORM_BEAR:
            case FORM_DIREBEAR:
                val2 = 0.0f;
                break;
            default:
                val2 = GetStat(STAT_AGILITY) - 10.0f;
                break;
            }
            break;
        default:
            val2 = GetStat(STAT_AGILITY) - 10.0f;
            break;
        }
    }
    else
    {
        switch (getClass())
        {
        case CLASS_WARRIOR:
            val2 = level * 3.0f + GetStat(STAT_STRENGTH) * 2.0f - 20.0f;
            break;
        case CLASS_PALADIN:
            val2 = level * 3.0f + GetStat(STAT_STRENGTH) * 2.0f - 20.0f;
            break;
        case CLASS_ROGUE:
            val2 = level * 2.0f + GetStat(STAT_STRENGTH) +
                   GetStat(STAT_AGILITY) - 20.0f;
            break;
        case CLASS_HUNTER:
            val2 = level * 2.0f + GetStat(STAT_STRENGTH) +
                   GetStat(STAT_AGILITY) - 20.0f;
            break;
        case CLASS_SHAMAN:
            val2 = level * 2.0f + GetStat(STAT_STRENGTH) * 2.0f - 20.0f;
            break;
        case CLASS_DRUID:
        {
            ShapeshiftForm form = GetShapeshiftForm();
            // Check if Predatory Strikes is skilled
            float mLevelMult = 0.0;
            switch (form)
            {
            case FORM_CAT:
            case FORM_BEAR:
            case FORM_DIREBEAR:
            case FORM_MOONKIN:
            {
                auto& dummy = GetAurasByType(SPELL_AURA_DUMMY);
                for (const auto& elem : dummy)
                {
                    // Predatory Strikes
                    if ((elem)->GetSpellProto()->SpellIconID == 1563)
                    {
                        mLevelMult = (elem)->GetModifier()->m_amount / 100.0f;
                        break;
                    }
                }
                break;
            }
            default:
                break;
            }

            switch (form)
            {
            case FORM_CAT:
                val2 = getLevel() * (mLevelMult + 2.0f) +
                       GetStat(STAT_STRENGTH) * 2.0f + GetStat(STAT_AGILITY) -
                       20.0f;
                break;
            case FORM_BEAR:
            case FORM_DIREBEAR:
                val2 = getLevel() * (mLevelMult + 3.0f) +
                       GetStat(STAT_STRENGTH) * 2.0f - 20.0f;
                break;
            case FORM_MOONKIN:
                val2 = getLevel() * (mLevelMult + 1.5f) +
                       GetStat(STAT_STRENGTH) * 2.0f - 20.0f;
                break;
            default:
                val2 = GetStat(STAT_STRENGTH) * 2.0f - 20.0f;
                break;
            }
            break;
        }
        case CLASS_MAGE:
            val2 = GetStat(STAT_STRENGTH) - 10.0f;
            break;
        case CLASS_PRIEST:
            val2 = GetStat(STAT_STRENGTH) - 10.0f;
            break;
        case CLASS_WARLOCK:
            val2 = GetStat(STAT_STRENGTH) - 10.0f;
            break;
        }
    }

    SetModifierValue(unitMod, BASE_VALUE, val2);

    float base_ap = GetModifierValue(unitMod, BASE_VALUE) *
                    GetModifierValue(unitMod, BASE_PCT);
    uint32 buff_pos = ap_buffs_[ranged ? UNIT_RAP_BUFF_POS : UNIT_AP_BUFF_POS];
    uint32 buff_neg = ap_buffs_[ranged ? UNIT_RAP_BUFF_NEG : UNIT_AP_BUFF_NEG];
    float ap_coeff = GetModifierValue(unitMod, TOTAL_PCT) - 1.0f;

    // Add ranged AP that's based of stat percentage (i.e. its value is
    // "dynamic")
    if (ranged)
    {
        if ((getClassMask() & CLASSMASK_WAND_USERS) == 0)
        {
            auto& al = GetAurasByType(
                SPELL_AURA_MOD_RANGED_ATTACK_POWER_OF_STAT_PERCENT);
            for (auto aura : al)
            {
                int32 mod =
                    int32(GetStat(Stats(aura->GetModifier()->m_miscvalue)) *
                          aura->GetModifier()->m_amount / 100.0f);
                if (mod > 0)
                    buff_pos += mod;
                else if (mod < 0)
                    buff_neg += (uint32)std::abs(mod);
            }
        }
    }

    SetInt32Value(
        index, (uint32)base_ap); // UNIT_FIELD_(RANGED)_ATTACK_POWER field

    // This field is split up into 2 16-bit integers, namely: neg ap | pos ap
    SetUInt32Value(index_mod, 0); // UNIT_FIELD_(RANGED)_ATTACK_POWER_MODS field
    SetInt16Value(index_mod, 0, (int16)buff_pos);
    SetInt16Value(index_mod, 1, -((int16)buff_neg));

    SetFloatValue(index_mult,
        ap_coeff); // UNIT_FIELD_(RANGED)_ATTACK_POWER_MULTIPLIER field

    // automatically update weapon damage after attack power modification
    if (ranged)
    {
        UpdateDamagePhysical(RANGED_ATTACK);

        Pet* pet = GetPet(); // update pet's AP
        if (pet)
            pet->UpdateAttackPowerAndDamage();
    }
    else
    {
        UpdateDamagePhysical(BASE_ATTACK);
        if (CanDualWield() && haveOffhandWeapon()) // allow update offhand
                                                   // damage only if player
                                                   // knows DualWield Spec and
                                                   // has equipped offhand
                                                   // weapon
            UpdateDamagePhysical(OFF_ATTACK);
    }
}

void Player::UpdateShieldBlockValue()
{
    SetUInt32Value(PLAYER_SHIELD_BLOCK, GetShieldBlockValue());
}

void Player::CalculateMinMaxDamage(WeaponAttackType attType, bool normalized,
    float& min_damage, float& max_damage)
{
    UnitMods unitMod;
    // UnitMods attPower; FIXME: Why is this unused? Is that really correct?

    switch (attType)
    {
    case BASE_ATTACK:
    default:
        unitMod = UNIT_MOD_DAMAGE_MAINHAND;
        // attPower = UNIT_MOD_ATTACK_POWER;
        break;
    case OFF_ATTACK:
        unitMod = UNIT_MOD_DAMAGE_OFFHAND;
        // attPower = UNIT_MOD_ATTACK_POWER;
        break;
    case RANGED_ATTACK:
        unitMod = UNIT_MOD_DAMAGE_RANGED;
        // attPower = UNIT_MOD_ATTACK_POWER_RANGED;
        break;
    }

    float att_speed = GetAPMultiplier(attType, normalized);

    float base_value = GetModifierValue(unitMod, BASE_VALUE) +
                       GetTotalAttackPowerValue(attType) / 14.0f * att_speed;
    float base_pct = GetModifierValue(unitMod, BASE_PCT);
    float total_value = GetModifierValue(unitMod, TOTAL_VALUE);
    float total_pct = GetModifierValue(unitMod, TOTAL_PCT);

    float weapon_mindamage = GetWeaponDamageRange(attType, MINDAMAGE);
    float weapon_maxdamage = GetWeaponDamageRange(attType, MAXDAMAGE);

    if (IsInFeralForm()) // check if player is druid and in cat or bear forms,
                         // non main hand attacks not allowed for this mode so
                         // not check attack type
    {
        uint32 lvl = getLevel();
        if (lvl > 60)
            lvl = 60;

        weapon_mindamage = lvl * 0.85f * att_speed;
        weapon_maxdamage = lvl * 1.25f * att_speed;
    }
    else if (!CanUseEquippedWeapon(attType)) // check if player not in form but
                                             // still can't use weapon
                                             // (broken/etc)
    {
        weapon_mindamage = BASE_MINDAMAGE;
        weapon_maxdamage = BASE_MAXDAMAGE;
    }
    else if (attType == RANGED_ATTACK) // add ammo DPS to ranged damage
    {
        weapon_mindamage += GetAmmoDPS() * att_speed;
        weapon_maxdamage += GetAmmoDPS() * att_speed;
    }

    min_damage =
        ((base_value + weapon_mindamage) * base_pct + total_value) * total_pct;
    max_damage =
        ((base_value + weapon_maxdamage) * base_pct + total_value) * total_pct;
}

void Player::UpdateDamagePhysical(WeaponAttackType attType)
{
    float mindamage;
    float maxdamage;

    CalculateMinMaxDamage(attType, false, mindamage, maxdamage);

    switch (attType)
    {
    case BASE_ATTACK:
    default:
        SetStatFloatValue(UNIT_FIELD_MINDAMAGE, mindamage);
        SetStatFloatValue(UNIT_FIELD_MAXDAMAGE, maxdamage);
        break;
    case OFF_ATTACK:
        SetStatFloatValue(UNIT_FIELD_MINOFFHANDDAMAGE, mindamage);
        SetStatFloatValue(UNIT_FIELD_MAXOFFHANDDAMAGE, maxdamage);
        break;
    case RANGED_ATTACK:
        SetStatFloatValue(UNIT_FIELD_MINRANGEDDAMAGE, mindamage);
        SetStatFloatValue(UNIT_FIELD_MAXRANGEDDAMAGE, maxdamage);
        break;
    }
}

void Player::UpdateDefenseBonusesMod()
{
    UpdateBlockPercentage();
    UpdateParryPercentage();
    UpdateDodgePercentage();
}

void Player::UpdateBlockPercentage()
{
    // No block
    float value = 0.0f;
    if (CanBlock())
    {
        // Base value
        value = 5.0f;
        // Modify value from defense skill
        value += (int32(GetDefenseSkillValue()) -
                     int32(GetMaxSkillValueForLevel())) *
                 0.04f;
        // Increase from SPELL_AURA_MOD_BLOCK_PERCENT aura
        value += GetTotalAuraModifier(SPELL_AURA_MOD_BLOCK_PERCENT);
        // Increase from rating
        value += GetRatingBonusValue(CR_BLOCK);
        value = value < 0.0f ? 0.0f : value;
    }
    SetStatFloatValue(PLAYER_BLOCK_PERCENTAGE, value);
}

void Player::UpdateCritPercentage(WeaponAttackType attType)
{
    BaseModGroup modGroup;
    uint16 index;
    CombatRating cr;

    switch (attType)
    {
    case OFF_ATTACK:
        modGroup = OFFHAND_CRIT_PERCENTAGE;
        index = PLAYER_OFFHAND_CRIT_PERCENTAGE;
        cr = CR_CRIT_MELEE;
        break;
    case RANGED_ATTACK:
        modGroup = RANGED_CRIT_PERCENTAGE;
        index = PLAYER_RANGED_CRIT_PERCENTAGE;
        cr = CR_CRIT_RANGED;
        break;
    case BASE_ATTACK:
    default:
        modGroup = CRIT_PERCENTAGE;
        index = PLAYER_CRIT_PERCENTAGE;
        cr = CR_CRIT_MELEE;
        break;
    }

    float value =
        GetTotalPercentageModValue(modGroup) + GetRatingBonusValue(cr);
    // Modify crit from weapon skill and maximized defense skill of same level
    // victim difference
    value += (int32(GetWeaponSkillValue(attType)) -
                 int32(GetMaxSkillValueForLevel())) *
             0.04f;
    value = value < 0.0f ? 0.0f : value;
    SetStatFloatValue(index, value);
}

void Player::UpdateAllCritPercentages()
{
    float value = GetMeleeCritFromAgility();

    SetBaseModValue(CRIT_PERCENTAGE, PCT_MOD, value);
    SetBaseModValue(OFFHAND_CRIT_PERCENTAGE, PCT_MOD, value);
    SetBaseModValue(RANGED_CRIT_PERCENTAGE, PCT_MOD, value);

    UpdateCritPercentage(BASE_ATTACK);
    UpdateCritPercentage(OFF_ATTACK);
    UpdateCritPercentage(RANGED_ATTACK);
}

void Player::UpdateParryPercentage()
{
    // No parry
    float value = 0.0f;
    if (CanParry())
    {
        // Base parry
        value = 5.0f;
        // Modify value from defense skill
        value += (int32(GetDefenseSkillValue()) -
                     int32(GetMaxSkillValueForLevel())) *
                 0.04f;
        // Parry from SPELL_AURA_MOD_PARRY_PERCENT aura
        value += GetTotalAuraModifier(SPELL_AURA_MOD_PARRY_PERCENT);
        // Parry from rating
        value += GetRatingBonusValue(CR_PARRY);
        value = value < 0.0f ? 0.0f : value;
    }
    SetStatFloatValue(PLAYER_PARRY_PERCENTAGE, value);
}

void Player::UpdateDodgePercentage()
{
    // Dodge from agility
    float value = GetDodgeFromAgility();
    // Modify value from defense skill
    value +=
        (int32(GetDefenseSkillValue()) - int32(GetMaxSkillValueForLevel())) *
        0.04f;
    // Dodge from SPELL_AURA_MOD_DODGE_PERCENT aura
    value += GetTotalAuraModifier(SPELL_AURA_MOD_DODGE_PERCENT);
    // Dodge from rating
    value += GetRatingBonusValue(CR_DODGE);
    value = value < 0.0f ? 0.0f : value;
    SetStatFloatValue(PLAYER_DODGE_PERCENTAGE, value);
}

void Player::UpdateSpellCritChance(uint32 school)
{
    // For normal school set zero crit chance
    if (school == SPELL_SCHOOL_NORMAL)
    {
        SetFloatValue(PLAYER_SPELL_CRIT_PERCENTAGE1, 0.0f);
        return;
    }
    // For others recalculate it from:
    float crit = 0.0f;
    // Crit from Intellect
    crit += GetSpellCritFromIntellect();
    // Increase crit from SPELL_AURA_MOD_SPELL_CRIT_CHANCE
    crit += GetTotalAuraModifier(SPELL_AURA_MOD_SPELL_CRIT_CHANCE);
    // Increase crit by school from SPELL_AURA_MOD_SPELL_CRIT_CHANCE_SCHOOL
    crit += GetTotalAuraModifierByMiscMask(
        SPELL_AURA_MOD_SPELL_CRIT_CHANCE_SCHOOL, 1 << school);
    // Increase crit from spell crit ratings
    crit += GetRatingBonusValue(CR_CRIT_SPELL);

    // Store crit value
    SetFloatValue(PLAYER_SPELL_CRIT_PERCENTAGE1 + school, crit);
}

void Player::UpdateMeleeHitChances()
{
    m_modMeleeHitChance = GetTotalAuraModifier(SPELL_AURA_MOD_HIT_CHANCE);
    m_modMeleeHitChance += GetRatingBonusValue(CR_HIT_MELEE);
}

void Player::UpdateRangedHitChances()
{
    m_modRangedHitChance = GetTotalAuraModifier(SPELL_AURA_MOD_HIT_CHANCE);
    m_modRangedHitChance += GetRatingBonusValue(CR_HIT_RANGED);
}

void Player::UpdateSpellHitChances()
{
    m_modSpellHitChance = GetTotalAuraModifier(SPELL_AURA_MOD_SPELL_HIT_CHANCE);
    m_modSpellHitChance += GetRatingBonusValue(CR_HIT_SPELL);
}

void Player::UpdateAllSpellCritChances()
{
    for (int i = SPELL_SCHOOL_NORMAL; i < MAX_SPELL_SCHOOL; ++i)
        UpdateSpellCritChance(i);
}

void Player::UpdateExpertise(WeaponAttackType attack)
{
    if (attack == RANGED_ATTACK)
        return;

    int32 expertise = int32(GetRatingBonusValue(CR_EXPERTISE));

    Item* weapon = GetWeaponForAttack(attack);

    auto& expAuras = GetAurasByType(SPELL_AURA_MOD_EXPERTISE);
    for (const auto& expAura : expAuras)
    {
        // item neutral spell
        if ((expAura)->GetSpellProto()->EquippedItemClass == -1)
            expertise += (expAura)->GetModifier()->m_amount;
        // item dependent spell
        else if (weapon &&
                 weapon->IsFitToSpellRequirements((expAura)->GetSpellProto()))
            expertise += (expAura)->GetModifier()->m_amount;
    }

    if (expertise < 0)
        expertise = 0;

    switch (attack)
    {
    case BASE_ATTACK:
        SetUInt32Value(PLAYER_EXPERTISE, expertise);
        break;
    case OFF_ATTACK:
        SetUInt32Value(PLAYER_OFFHAND_EXPERTISE, expertise);
        break;
    default:
        break;
    }
}

void Player::UpdateManaRegen()
{
    float Intellect = GetStat(STAT_INTELLECT);
    // Mana regen from spirit and intellect
    float power_regen = sqrt(Intellect) * OCTRegenMPPerSpirit();
    // Apply PCT bonus from SPELL_AURA_MOD_POWER_REGEN_PERCENT aura on spirit
    // base regen
    power_regen *= GetTotalAuraMultiplierByMiscValue(
        SPELL_AURA_MOD_POWER_REGEN_PERCENT, POWER_MANA);

    // Mana regen from SPELL_AURA_MOD_POWER_REGEN aura
    float power_regen_mp5 = GetTotalAuraModifierByMiscValue(
                                SPELL_AURA_MOD_POWER_REGEN, POWER_MANA) /
                            5.0f;

    // Get bonus from SPELL_AURA_MOD_MANA_REGEN_FROM_STAT aura
    auto& regenAura = GetAurasByType(SPELL_AURA_MOD_MANA_REGEN_FROM_STAT);
    for (const auto& elem : regenAura)
    {
        Modifier* mod = (elem)->GetModifier();
        power_regen_mp5 +=
            GetStat(Stats(mod->m_miscvalue)) * mod->m_amount / 500.0f;
    }

    // Set regen rate in cast state apply only on spirit based regen
    int32 modManaRegenInterrupt =
        GetTotalAuraModifier(SPELL_AURA_MOD_MANA_REGEN_INTERRUPT);
    if (modManaRegenInterrupt > 100)
        modManaRegenInterrupt = 100;
    SetStatFloatValue(PLAYER_FIELD_MOD_MANA_REGEN_INTERRUPT,
        power_regen_mp5 + power_regen * modManaRegenInterrupt / 100.0f);

    SetStatFloatValue(
        PLAYER_FIELD_MOD_MANA_REGEN, power_regen_mp5 + power_regen);
}

std::pair<float, float> Player::CalcCyclonedRegen()
{
    float intellect = GetStat(STAT_INTELLECT);
    // Mana regen from spirit and intellect
    float regen = sqrt(intellect) * OCTRegenMPPerSpirit();

    float mp5_regen = 0;

    // Get bonus from SPELL_AURA_MOD_MANA_REGEN_FROM_STAT aura
    auto& auras = GetAurasByType(SPELL_AURA_MOD_MANA_REGEN_FROM_STAT);
    for (auto aura : auras)
    {
        Modifier* mod = aura->GetModifier();
        mp5_regen += GetStat(Stats(mod->m_miscvalue)) * mod->m_amount / 500.0f;
    }

    int32 interrupt = GetTotalAuraModifier(SPELL_AURA_MOD_MANA_REGEN_INTERRUPT);
    if (interrupt > 100)
        interrupt = 100;

    return std::make_pair(
        mp5_regen + regen, mp5_regen + regen * interrupt / 100.0f);
}

void Player::_ApplyAllStatBonuses()
{
    SetCanModifyStats(false);

    _ApplyAllAuraMods();
    _ApplyAllItemMods();

    SetCanModifyStats(true);

    UpdateAllStats();
}

void Player::_RemoveAllStatBonuses()
{
    SetCanModifyStats(false);

    _RemoveAllItemMods();
    _RemoveAllAuraMods();

    SetCanModifyStats(true);

    UpdateAllStats();
}

/*#######################################
########                         ########
########    MOBS STAT SYSTEM     ########
########                         ########
#######################################*/

bool Creature::UpdateStats(Stats /*stat*/)
{
    return true;
}

bool Creature::UpdateAllStats()
{
    UpdateMaxHealth();
    UpdateAttackPowerAndDamage();

    for (int i = POWER_MANA; i < MAX_POWERS; ++i)
        UpdateMaxPower(Powers(i));

    for (int i = SPELL_SCHOOL_NORMAL; i < MAX_SPELL_SCHOOL; ++i)
        UpdateResistances(i);

    return true;
}

void Creature::UpdateResistances(uint32 school)
{
    if (school > SPELL_SCHOOL_NORMAL)
    {
        float value =
            GetTotalAuraModValue(UnitMods(UNIT_MOD_RESISTANCE_START + school));
        SetResistance(SpellSchools(school), int32(value));
    }
    else
        UpdateArmor();
}

void Creature::UpdateArmor()
{
    float value = GetTotalAuraModValue(UNIT_MOD_ARMOR);
    SetArmor(int32(value));
}

void Creature::UpdateMaxHealth()
{
    float value = GetTotalAuraModValue(UNIT_MOD_HEALTH);
    SetMaxHealth((uint32)value);
}

void Creature::UpdateMaxPower(Powers power)
{
    UnitMods unitMod = UnitMods(UNIT_MOD_POWER_START + power);

    float value = GetTotalAuraModValue(unitMod);
    SetMaxPower(power, uint32(value));
}

void Creature::UpdateAttackPowerAndDamage(bool ranged)
{
    UnitMods unitMod =
        ranged ? UNIT_MOD_ATTACK_POWER_RANGED : UNIT_MOD_ATTACK_POWER;

    uint16 index = UNIT_FIELD_ATTACK_POWER;
    uint16 index_mod = UNIT_FIELD_ATTACK_POWER_MODS;
    uint16 index_mult = UNIT_FIELD_ATTACK_POWER_MULTIPLIER;

    if (ranged)
    {
        index = UNIT_FIELD_RANGED_ATTACK_POWER;
        index_mod = UNIT_FIELD_RANGED_ATTACK_POWER_MODS;
        index_mult = UNIT_FIELD_RANGED_ATTACK_POWER_MULTIPLIER;
    }

    float base_ap = GetModifierValue(unitMod, BASE_VALUE) *
                    GetModifierValue(unitMod, BASE_PCT);
    uint32 buff_pos = ap_buffs_[ranged ? UNIT_RAP_BUFF_POS : UNIT_AP_BUFF_POS];
    uint32 buff_neg = ap_buffs_[ranged ? UNIT_RAP_BUFF_NEG : UNIT_AP_BUFF_NEG];
    float ap_coeff = GetModifierValue(unitMod, TOTAL_PCT) - 1.0f;

    SetInt32Value(
        index, (uint32)base_ap); // UNIT_FIELD_(RANGED)_ATTACK_POWER field

    // This field is split up into 2 16-bit integers, namely: neg ap | pos ap
    SetUInt32Value(index_mod, 0); // UNIT_FIELD_(RANGED)_ATTACK_POWER_MODS field
    SetInt16Value(index_mod, 0, (int16)buff_pos);
    SetInt16Value(index_mod, 1, -((int16)buff_neg));

    SetFloatValue(index_mult,
        ap_coeff); // UNIT_FIELD_(RANGED)_ATTACK_POWER_MULTIPLIER field

    if (ranged)
        return;

    // automatically update weapon damage after attack power modification
    UpdateDamagePhysical(BASE_ATTACK);
    UpdateDamagePhysical(OFF_ATTACK);
}

void Creature::UpdateDamagePhysical(WeaponAttackType attType)
{
    if (attType > OFF_ATTACK)
        return;

    UnitMods unitMod = (attType == BASE_ATTACK ? UNIT_MOD_DAMAGE_MAINHAND :
                                                 UNIT_MOD_DAMAGE_OFFHAND);

    /* difference in AP between current attack power and base value from DB */

    uint32 base_attackpower;
    if (GetCreatureInfo()->attackpower > 0)
        base_attackpower = GetCreatureInfo()->attackpower;
    else
        base_attackpower = this->getLevel() * 4 + 48;

    float attackpowerfactor =
        1 -
        ((1 - GetTotalAttackPowerValue(attType) / base_attackpower) * 0.1425);
    float base_value = GetModifierValue(unitMod, BASE_VALUE);
    float base_pct = GetModifierValue(unitMod, BASE_PCT);
    float total_value = GetModifierValue(unitMod, TOTAL_VALUE);
    float total_pct = GetModifierValue(unitMod, TOTAL_PCT);
    float dmg_multiplier = GetCreatureInfo()->dmg_multiplier;

    float weapon_mindamage = GetWeaponDamageRange(attType, MINDAMAGE);
    float weapon_maxdamage = GetWeaponDamageRange(attType, MAXDAMAGE);

    float mindamage =
        ((base_value + weapon_mindamage) * dmg_multiplier * base_pct +
            total_value) *
        total_pct * attackpowerfactor;
    float maxdamage =
        ((base_value + weapon_maxdamage) * dmg_multiplier * base_pct +
            total_value) *
        total_pct * attackpowerfactor;

    SetStatFloatValue(attType == BASE_ATTACK ? UNIT_FIELD_MINDAMAGE :
                                               UNIT_FIELD_MINOFFHANDDAMAGE,
        mindamage);
    SetStatFloatValue(attType == BASE_ATTACK ? UNIT_FIELD_MAXDAMAGE :
                                               UNIT_FIELD_MAXOFFHANDDAMAGE,
        maxdamage);
}

/*#######################################
########                         ########
########    PETS STAT SYSTEM     ########
########                         ########
#######################################*/

bool Pet::UpdateStats(Stats stat)
{
    if (stat > STAT_SPIRIT)
        return false;

    // Guardians get their hp, mana, damage, etc from the database and do not
    // scale (except for some special cases handled in InitStatsForLevel)
    if (getPetType() == GUARDIAN_PET)
    {
        if (stat != STAT_SPIRIT)
        {
            SetStat(stat, 0);
        }
        else
        {
            // We do however give them some base spirit to allow hp regen.
            // (FIXME: Made up scaling factor)
            SetStat(stat, getLevel() * 2.0f);
        }
    }
    else
    {
        // Other pets
        float value = 0;
        Unit* owner = GetOwner();

        if (stat == STAT_STAMINA)
        {
            if (owner)
                value += owner->GetStat(stat) * 0.3f;
        }
        else if (stat == STAT_INTELLECT && getPetType() == SUMMON_PET)
        {
            if (owner && (owner->getClass() == CLASS_WARLOCK ||
                             owner->getClass() == CLASS_MAGE))
                value += owner->GetStat(stat) * 0.3f;
        }

        value = GetTotalStatValue(stat, value);
        SetStat(stat, int32(value));
    }

    switch (stat)
    {
    case STAT_STRENGTH:
        UpdateAttackPowerAndDamage();
        break;
    case STAT_AGILITY:
        UpdateArmor();
        break;
    case STAT_STAMINA:
        UpdateMaxHealth();
        break;
    case STAT_INTELLECT:
        UpdateMaxPower(POWER_MANA);
        break;
    case STAT_SPIRIT:
    default:
        break;
    }

    return true;
}

bool Pet::UpdateAllStats()
{
    for (int i = STAT_STRENGTH; i < MAX_STATS; ++i)
        UpdateStats(Stats(i));

    for (int i = POWER_MANA; i < MAX_POWERS; ++i)
        UpdateMaxPower(Powers(i));

    for (int i = SPELL_SCHOOL_NORMAL; i < MAX_SPELL_SCHOOL; ++i)
        UpdateResistances(i);

    return true;
}

void Pet::UpdateResistances(uint32 school)
{
    if (school > SPELL_SCHOOL_NORMAL)
    {
        float value =
            GetTotalAuraModValue(UnitMods(UNIT_MOD_RESISTANCE_START + school));

        Unit* owner = GetOwner();
        // hunter and warlock pets gain 40% of owner's resistance
        if (owner && (getPetType() == HUNTER_PET ||
                         (getPetType() == SUMMON_PET &&
                             owner->getClass() == CLASS_WARLOCK)))
            value += float(owner->GetResistance(SpellSchools(school))) * 0.4f;

        SetResistance(SpellSchools(school), int32(value));
    }
    else
        UpdateArmor();
}

void Pet::UpdateArmor()
{
    float value = 0.0f;
    float bonus_armor = 0.0f;
    UnitMods unitMod = UNIT_MOD_ARMOR;

    Unit* owner = GetOwner();
    // hunter and warlock pets gain 35% of owner's armor value
    if (owner &&
        (getPetType() == HUNTER_PET ||
            (getPetType() == SUMMON_PET && owner->getClass() == CLASS_WARLOCK)))
        bonus_armor = 0.35f * float(owner->GetArmor());

    value = GetModifierValue(unitMod, BASE_VALUE);
    value *= GetModifierValue(unitMod, BASE_PCT);
    value += GetStat(STAT_AGILITY) * 2.0f;
    value += GetModifierValue(unitMod, TOTAL_VALUE) + bonus_armor;
    value *= GetModifierValue(unitMod, TOTAL_PCT);

    SetArmor(int32(value));
}

static void warlock_demonic_knowledge_hack(Pet* pet)
{
    // update demonic knowledge on stam/int change
    // sort of hackish approach, but that's blizzard's fault!

    if (pet->getPetType() != SUMMON_PET)
        return;

    Unit* owner = pet->GetOwner();
    if (!owner)
        return;

    if (owner->getClass() != CLASS_WARLOCK)
        return;

    AuraHolder* holder = owner->get_aura(35696);
    if (!holder)
        return;

    Aura* aura = holder->GetAura(EFFECT_INDEX_0);
    if (!aura)
        return;

    auto mod = aura->GetModifier();
    if (!mod)
        return;

    AuraHolder* talent_holder =
        owner->get_aura(SPELL_AURA_DUMMY, ObjectGuid(), [](AuraHolder* holder)
            {
                return holder->GetSpellProto()->SpellFamilyName ==
                           SPELLFAMILY_WARLOCK &&
                       holder->GetSpellProto()->SpellIconID == 1876;
            });
    if (!talent_holder)
        return;

    Aura* talent_aura = talent_holder->GetAura(EFFECT_INDEX_0);
    if (!talent_aura)
        return;

    aura->HandleModDamageDone(false, true);
    mod->m_amount = int32(
        talent_aura->GetBasePoints() *
        (pet->GetStat(STAT_STAMINA) + pet->GetStat(STAT_INTELLECT)) / 100);
    aura->HandleModDamageDone(true, true);
}

void Pet::UpdateMaxHealth()
{
    float value =
        GetModifierValue(UNIT_MOD_HEALTH, BASE_VALUE) + GetCreateHealth();
    value *= GetModifierValue(UNIT_MOD_HEALTH, BASE_PCT);
    value += GetModifierValue(UNIT_MOD_HEALTH, TOTAL_VALUE);

    // The first 20 stamina points are 1 health per stamina
    float stamina = GetStat(STAT_STAMINA);
    float first_points = (stamina > 20) ? 20 : stamina;
    stamina -= first_points;

    value += first_points;
    value += stamina * sObjectMgr::Instance()->GetPetStaminaScaling(GetEntry());

    value *= GetModifierValue(UNIT_MOD_HEALTH, TOTAL_PCT);

    SetMaxHealth((uint32)value);

    warlock_demonic_knowledge_hack(this);
}

void Pet::UpdateMaxPower(Powers power)
{
    UnitMods unitMod = UnitMods(UNIT_MOD_POWER_START + power);

    float value =
        GetModifierValue(unitMod, BASE_VALUE) + GetCreatePowers(power);
    value *= GetModifierValue(unitMod, BASE_PCT);
    value += GetModifierValue(unitMod, TOTAL_VALUE);

    // The first 20 intellect points are 1 mana per intellect
    float intellect = (power == POWER_MANA) ? GetStat(STAT_INTELLECT) : 0.0f;
    float first_points = (intellect > 20) ? 20 : intellect;
    intellect -= first_points;

    value += first_points;
    if (intellect > 0.0f)
        value += intellect *
                 sObjectMgr::Instance()->GetPetIntellectScaling(GetEntry());

    value *= GetModifierValue(unitMod, TOTAL_PCT);

    SetMaxPower(power, uint32(value));

    if (power == POWER_MANA)
        warlock_demonic_knowledge_hack(this);
}

void Pet::UpdateAttackPowerAndDamage(bool ranged)
{
    if (ranged)
        return;

    float val = 0.0f;
    float bonusAP = 0.0f;
    UnitMods unitMod = UNIT_MOD_ATTACK_POWER;

    if (GetEntry() == 416) // imp's attack power
        val = GetStat(STAT_STRENGTH) - 10.0f;
    else
        val = 2 * GetStat(STAT_STRENGTH) - 20.0f;

    init_spell_bonus();

    SetModifierValue(UNIT_MOD_ATTACK_POWER, BASE_VALUE, val + bonusAP);

    // in BASE_VALUE of UNIT_MOD_ATTACK_POWER for creatures we store data of
    // meleeattackpower field in DB
    float base_ap = (GetModifierValue(unitMod, BASE_VALUE) + owner_bonus_ap()) *
                    GetModifierValue(unitMod, BASE_PCT);
    uint32 buff_pos = ap_buffs_[ranged ? UNIT_RAP_BUFF_POS : UNIT_AP_BUFF_POS];
    uint32 buff_neg = ap_buffs_[ranged ? UNIT_RAP_BUFF_NEG : UNIT_AP_BUFF_NEG];
    float ap_coeff = GetModifierValue(unitMod, TOTAL_PCT) - 1.0f;

    SetInt32Value(UNIT_FIELD_ATTACK_POWER, (uint32)base_ap);

    // This field is split up into 2 16-bit integers, namely: neg ap | pos ap
    SetUInt32Value(UNIT_FIELD_ATTACK_POWER_MODS, 0);
    SetInt16Value(UNIT_FIELD_ATTACK_POWER_MODS, 0, (int16)buff_pos);
    SetInt16Value(UNIT_FIELD_ATTACK_POWER_MODS, 1, -((int16)buff_neg));

    SetFloatValue(UNIT_FIELD_ATTACK_POWER_MULTIPLIER, ap_coeff);

    // automatically update weapon damage after attack power modification
    UpdateDamagePhysical(BASE_ATTACK);
    if (getPetType() == GUARDIAN_PET)
        UpdateDamagePhysical(OFF_ATTACK);
}

void Pet::UpdateDamagePhysical(WeaponAttackType attType)
{
    if (attType > BASE_ATTACK && getPetType() != GUARDIAN_PET)
        return;

    float bonusDamage = 0.0f;
    if (Unit* owner = GetOwner())
    {
        // force of nature
        if (GetEntry() == 1964)
        {
            int32 spellDmg =
                int32(owner->GetUInt32Value(
                    PLAYER_FIELD_MOD_DAMAGE_DONE_POS + SPELL_SCHOOL_NATURE)) -
                owner->GetUInt32Value(
                    PLAYER_FIELD_MOD_DAMAGE_DONE_NEG + SPELL_SCHOOL_NATURE);
            if (spellDmg > 0)
                bonusDamage = spellDmg * 0.09f;
        }
    }

    UnitMods unitMod = attType == BASE_ATTACK ? UNIT_MOD_DAMAGE_MAINHAND :
                                                UNIT_MOD_DAMAGE_OFFHAND;

    float att_speed = float(GetAttackTime(attType)) / 1000.0f;

    float base_value = GetModifierValue(unitMod, BASE_VALUE) +
                       GetTotalAttackPowerValue(attType) / 14.0f * att_speed +
                       bonusDamage;
    float base_pct = GetModifierValue(unitMod, BASE_PCT);
    float total_value = GetModifierValue(unitMod, TOTAL_VALUE);
    float total_pct = GetModifierValue(unitMod, TOTAL_PCT);

    float weapon_mindamage = GetWeaponDamageRange(attType, MINDAMAGE);
    float weapon_maxdamage = GetWeaponDamageRange(attType, MAXDAMAGE);

    float mindamage =
        ((base_value + weapon_mindamage) * base_pct + total_value) * total_pct;
    float maxdamage =
        ((base_value + weapon_maxdamage) * base_pct + total_value) * total_pct;

    // Cobra Reflexes
    if (has_aura(25076))
    {
        mindamage *= 0.86;
        maxdamage *= 0.86;
    }

    SetStatFloatValue(attType == BASE_ATTACK ? UNIT_FIELD_MINDAMAGE :
                                               UNIT_FIELD_MINOFFHANDDAMAGE,
        mindamage);
    SetStatFloatValue(attType == BASE_ATTACK ? UNIT_FIELD_MAXDAMAGE :
                                               UNIT_FIELD_MAXOFFHANDDAMAGE,
        maxdamage);
}

void Pet::init_spell_bonus()
{
    Unit* owner = GetOwner();
    if (!owner || owner->GetTypeId() != TYPEID_PLAYER)
        return;

    switch (getPetType())
    {
    case HUNTER_PET:
        spell_bonus(owner->GetTotalAttackPowerValue(RANGED_ATTACK) * 0.129f);
        break;
    case SUMMON_PET:
        if (owner->getClass() == CLASS_MAGE)
        {
            // Mage pet scales with frost
            spell_bonus(owner->GetUInt32Value(PLAYER_FIELD_MOD_DAMAGE_DONE_POS +
                                              SPELL_SCHOOL_FROST) *
                        0.4);
        }
        else if (owner->getClass() == CLASS_WARLOCK)
        {
            // Warlock pets scales with fire and shadow (whichever is highest)
            float sp = std::max(
                owner->GetUInt32Value(
                    PLAYER_FIELD_MOD_DAMAGE_DONE_POS + SPELL_SCHOOL_FIRE),
                owner->GetUInt32Value(
                    PLAYER_FIELD_MOD_DAMAGE_DONE_POS + SPELL_SCHOOL_SHADOW));
            spell_bonus(sp * 0.15f);
        }
        break;
    default:
        break;
    }
}

int32 Pet::owner_bonus_ap()
{
    Unit* owner = GetOwner();
    if (!owner || owner->GetTypeId() != TYPEID_PLAYER)
        return 0;

    switch (getPetType())
    {
    case HUNTER_PET:
        return owner->GetTotalAttackPowerValue(RANGED_ATTACK) * 0.22f;
    case SUMMON_PET:
        if (owner->getClass() == CLASS_PRIEST)
        {
            return owner->GetUInt32Value(
                       PLAYER_FIELD_MOD_DAMAGE_DONE_POS + SPELL_SCHOOL_SHADOW) *
                   0.57f;
        }
        else if (owner->getClass() == CLASS_WARLOCK)
        {
            float sp = std::max(
                owner->GetUInt32Value(
                    PLAYER_FIELD_MOD_DAMAGE_DONE_POS + SPELL_SCHOOL_FIRE),
                owner->GetUInt32Value(
                    PLAYER_FIELD_MOD_DAMAGE_DONE_POS + SPELL_SCHOOL_SHADOW));
            return sp * 0.57f;
        }
        break;
    default:
        break;
    }

    return 0;
}
