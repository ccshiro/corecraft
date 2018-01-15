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
 * along with this program; if not, write to the Free Softwared
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "BattleGround.h"
#include "Common.h"
#include "Creature.h"
#include "CreatureAI.h"
#include "DynamicObject.h"
#include "Formulas.h"
#include "Group.h"
#include "logging.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Opcodes.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SpecialVisCreature.h"
#include "Totem.h"
#include "TemporarySummon.h"
#include "Spell.h"
#include "SpellMgr.h"
#include "Totem.h"
#include "Unit.h"
#include "UpdateData.h"
#include "UpdateMask.h"
#include "Util.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "buff_stacking.h"
#include "loot_distributor.h"
#include "pet_behavior.h"
#include "Database/DatabaseEnv.h"
#include "Policies/Singleton.h"
#include "maps/checks.h"
#include "maps/visitors.h"
#include "movement/ConfusedMovementGenerator.h"
#include "movement/FleeingMovementGenerator.h"
#include "movement/IdleMovementGenerator.h"
#include "movement/PointMovementGenerator.h"
#include "movement/TargetedMovementGenerator.h"

#define NULL_AURA_SLOT 0xFF

pAuraHandler AuraHandler[TOTAL_AURAS] = {
    &Aura::HandleNULL,               //  0 SPELL_AURA_NONE
    &Aura::HandleBindSight,          //  1 SPELL_AURA_BIND_SIGHT
    &Aura::HandleModPossess,         //  2 SPELL_AURA_MOD_POSSESS
    &Aura::HandlePeriodicDamage,     //  3 SPELL_AURA_PERIODIC_DAMAGE
    &Aura::HandleAuraDummy,          //  4 SPELL_AURA_DUMMY
    &Aura::HandleModConfuse,         //  5 SPELL_AURA_MOD_CONFUSE
    &Aura::HandleModCharm,           //  6 SPELL_AURA_MOD_CHARM
    &Aura::HandleModFear,            //  7 SPELL_AURA_MOD_FEAR
    &Aura::HandlePeriodicHeal,       //  8 SPELL_AURA_PERIODIC_HEAL
    &Aura::HandleModAttackSpeed,     //  9 SPELL_AURA_MOD_ATTACKSPEED
    &Aura::HandleModThreat,          // 10 SPELL_AURA_MOD_THREAT
    &Aura::HandleModTaunt,           // 11 SPELL_AURA_MOD_TAUNT
    &Aura::HandleAuraModStun,        // 12 SPELL_AURA_MOD_STUN
    &Aura::HandleModDamageDone,      // 13 SPELL_AURA_MOD_DAMAGE_DONE
    &Aura::HandleNoImmediateEffect,  // 14 SPELL_AURA_MOD_DAMAGE_TAKEN
                                     // implemented in
                                     // Unit::MeleeDamageBonusTaken and
                                     // Unit::SpellBaseDamageBonusTaken
    &Aura::HandleNoImmediateEffect,  // 15 SPELL_AURA_DAMAGE_SHIELD
                                     // implemented in Unit::DealMeleeDamage
    &Aura::HandleModStealth,         // 16 SPELL_AURA_MOD_STEALTH
    &Aura::HandleNoImmediateEffect,  // 17 SPELL_AURA_MOD_STEALTH_DETECT
                                     // implemented in Unit::update_stealth
    &Aura::HandleInvisibility,       // 18 SPELL_AURA_MOD_INVISIBILITY
    &Aura::HandleInvisibilityDetect, // 19 SPELL_AURA_MOD_INVISIBILITY_DETECTION
    &Aura::HandleAuraModTotalHealthPercentRegen, // 20 SPELL_AURA_OBS_MOD_HEALTH
    &Aura::HandleAuraModTotalManaPercentRegen,   // 21 SPELL_AURA_OBS_MOD_MANA
    &Aura::HandleAuraModResistance,              // 22 SPELL_AURA_MOD_RESISTANCE
    &Aura::HandlePeriodicTriggerSpell, // 23 SPELL_AURA_PERIODIC_TRIGGER_SPELL
    &Aura::HandlePeriodicEnergize,     // 24 SPELL_AURA_PERIODIC_ENERGIZE
    &Aura::HandleAuraModPacify,        // 25 SPELL_AURA_MOD_PACIFY
    &Aura::HandleAuraModRoot,          // 26 SPELL_AURA_MOD_ROOT
    &Aura::HandleAuraModSilence,       // 27 SPELL_AURA_MOD_SILENCE
    &Aura::HandleNoImmediateEffect,    // 28 SPELL_AURA_REFLECT_SPELLS
                                       // implement in Unit::SpellHitResult
    &Aura::HandleAuraModStat,          // 29 SPELL_AURA_MOD_STAT
    &Aura::HandleAuraModSkill,         // 30 SPELL_AURA_MOD_SKILL
    &Aura::HandleAuraModIncreaseSpeed, // 31 SPELL_AURA_MOD_INCREASE_SPEED
    &Aura::HandleAuraModIncreaseMountedSpeed, // 32
    // SPELL_AURA_MOD_INCREASE_MOUNTED_SPEED
    &Aura::HandleAuraModDecreaseSpeed,  // 33 SPELL_AURA_MOD_DECREASE_SPEED
    &Aura::HandleAuraModIncreaseHealth, // 34 SPELL_AURA_MOD_INCREASE_HEALTH
    &Aura::HandleAuraModIncreaseEnergy, // 35 SPELL_AURA_MOD_INCREASE_ENERGY
    &Aura::HandleAuraModShapeshift,     // 36 SPELL_AURA_MOD_SHAPESHIFT
    &Aura::HandleAuraModEffectImmunity, // 37 SPELL_AURA_EFFECT_IMMUNITY
    &Aura::HandleAuraModStateImmunity,  // 38 SPELL_AURA_STATE_IMMUNITY
    &Aura::HandleAuraModSchoolImmunity, // 39 SPELL_AURA_SCHOOL_IMMUNITY
    &Aura::HandleAuraModDmgImmunity,    // 40 SPELL_AURA_DAMAGE_IMMUNITY
    &Aura::HandleAuraModDispelImmunity, // 41 SPELL_AURA_DISPEL_IMMUNITY
    &Aura::HandleAuraProcTriggerSpell,  // 42 SPELL_AURA_PROC_TRIGGER_SPELL
                                        // implemented in
                                        // Unit::ProcDamageAndSpellFor and
                                        // Unit::HandleProcTriggerSpell
    &Aura::HandleNoImmediateEffect,     // 43 SPELL_AURA_PROC_TRIGGER_DAMAGE
                                        // implemented in
                                        // Unit::ProcDamageAndSpellFor
    &Aura::HandleAuraTrackCreatures,    // 44 SPELL_AURA_TRACK_CREATURES
    &Aura::HandleAuraTrackResources,    // 45 SPELL_AURA_TRACK_RESOURCES
    &Aura::HandleUnused,                // 46 SPELL_AURA_46
    &Aura::HandleAuraModParryPercent,   // 47 SPELL_AURA_MOD_PARRY_PERCENT
    &Aura::HandleUnused,                // 48 SPELL_AURA_48
    &Aura::HandleAuraModDodgePercent,   // 49 SPELL_AURA_MOD_DODGE_PERCENT
    &Aura::HandleUnused, // 50 SPELL_AURA_MOD_BLOCK_SKILL    obsolete?
    &Aura::HandleAuraModBlockPercent, // 51 SPELL_AURA_MOD_BLOCK_PERCENT
    &Aura::HandleAuraModCritPercent,  // 52 SPELL_AURA_MOD_CRIT_PERCENT
    &Aura::HandlePeriodicLeech,       // 53 SPELL_AURA_PERIODIC_LEECH
    &Aura::HandleModHitChance,        // 54 SPELL_AURA_MOD_HIT_CHANCE
    &Aura::HandleModSpellHitChance,   // 55 SPELL_AURA_MOD_SPELL_HIT_CHANCE
    &Aura::HandleAuraTransform,       // 56 SPELL_AURA_TRANSFORM
    &Aura::HandleModSpellCritChance,  // 57 SPELL_AURA_MOD_SPELL_CRIT_CHANCE
    &Aura::HandleAuraModIncreaseSwimSpeed, // 58
                                           // SPELL_AURA_MOD_INCREASE_SWIM_SPEED
    &Aura::HandleNoImmediateEffect, // 59 SPELL_AURA_MOD_DAMAGE_DONE_CREATURE
                                    // implemented in Unit::MeleeDamageBonusDone
                                    // and Unit::SpellDamageBonusDone
    &Aura::HandleAuraModPacifyAndSilence, // 60 SPELL_AURA_MOD_PACIFY_SILENCE
    &Aura::HandleAuraModScale,            // 61 SPELL_AURA_MOD_SCALE
    &Aura::HandlePeriodicHealthFunnel, // 62 SPELL_AURA_PERIODIC_HEALTH_FUNNEL
    &Aura::HandleUnused, // 63 SPELL_AURA_PERIODIC_MANA_FUNNEL obsolete?
    &Aura::HandlePeriodicManaLeech, // 64 SPELL_AURA_PERIODIC_MANA_LEECH
    &Aura::HandleModCastingSpeed,   // 65 SPELL_AURA_MOD_CASTING_SPEED_NOT_STACK
    &Aura::HandleFeignDeath,        // 66 SPELL_AURA_FEIGN_DEATH
    &Aura::HandleAuraModDisarm,     // 67 SPELL_AURA_MOD_DISARM
    &Aura::HandleAuraModStalked,    // 68 SPELL_AURA_MOD_STALKED
    &Aura::HandleSchoolAbsorb, // 69 SPELL_AURA_SCHOOL_ABSORB implemented in
                               // Unit::CalculateAbsorbAndResist
    &Aura::HandleUnused, // 70 SPELL_AURA_EXTRA_ATTACKS      Useless, used by
                         // only one spell that has only visual effect
    &Aura::HandleModSpellCritChanceShool, // 71
    // SPELL_AURA_MOD_SPELL_CRIT_CHANCE_SCHOOL
    &Aura::HandleModPowerCostPCT,     // 72 SPELL_AURA_MOD_POWER_COST_SCHOOL_PCT
    &Aura::HandleModPowerCost,        // 73 SPELL_AURA_MOD_POWER_COST_SCHOOL
    &Aura::HandleNoImmediateEffect,   // 74 SPELL_AURA_REFLECT_SPELLS_SCHOOL
                                      // implemented in Unit::SpellHitResult
    &Aura::HandleNoImmediateEffect,   // 75 SPELL_AURA_MOD_LANGUAGE
                                      // implemented in
                                      // WorldSession::HandleMessagechatOpcode
    &Aura::HandleFarSight,            // 76 SPELL_AURA_FAR_SIGHT
    &Aura::HandleModMechanicImmunity, // 77 SPELL_AURA_MECHANIC_IMMUNITY
    &Aura::HandleAuraMounted,         // 78 SPELL_AURA_MOUNTED
    &Aura::HandleModDamagePercentDone, // 79 SPELL_AURA_MOD_DAMAGE_PERCENT_DONE
    &Aura::HandleModPercentStat,       // 80 SPELL_AURA_MOD_PERCENT_STAT
    &Aura::HandleNoImmediateEffect,    // 81 SPELL_AURA_SPLIT_DAMAGE_PCT
                                       // implemented in
                                       // Unit::CalculateAbsorbAndResist
    &Aura::HandleWaterBreathing,       // 82 SPELL_AURA_WATER_BREATHING
    &Aura::HandleModBaseResistance,    // 83 SPELL_AURA_MOD_BASE_RESISTANCE
    &Aura::HandleModRegen,             // 84 SPELL_AURA_MOD_REGEN
    &Aura::HandleModPowerRegen,        // 85 SPELL_AURA_MOD_POWER_REGEN
    &Aura::HandleChannelDeathItem,     // 86 SPELL_AURA_CHANNEL_DEATH_ITEM
    &Aura::HandleNoImmediateEffect,    // 87 SPELL_AURA_MOD_DAMAGE_PERCENT_TAKEN
                                       // implemented in
                                       // Unit::MeleeDamageBonusTaken and
                                       // Unit::SpellDamageBonusTaken
    &Aura::HandleNoImmediateEffect,    // 88 SPELL_AURA_MOD_HEALTH_REGEN_PERCENT
    // implemented in Player::RegenerateHealth
    &Aura::HandlePeriodicDamagePCT, // 89 SPELL_AURA_PERIODIC_DAMAGE_PERCENT
    &Aura::HandleUnused,            // 90 SPELL_AURA_MOD_RESIST_CHANCE  Useless
    &Aura::HandleNoImmediateEffect, // 91 SPELL_AURA_MOD_DETECT_RANGE
                                    // implemented in
                                    // Creature::GetAggroDistance
    &Aura::HandlePreventFleeing,    // 92 SPELL_AURA_PREVENTS_FLEEING
    &Aura::HandleModUnattackable,   // 93 SPELL_AURA_MOD_UNATTACKABLE
    &Aura::HandleNoImmediateEffect, // 94 SPELL_AURA_INTERRUPT_REGEN implemented
                                    // in Player::RegenerateAll
    &Aura::HandleAuraGhost,         // 95 SPELL_AURA_GHOST
    &Aura::HandleNoImmediateEffect, // 96 SPELL_AURA_SPELL_MAGNET implemented in
                                    // Unit::SelectMagnetTarget
    &Aura::HandleManaShield,        // 97 SPELL_AURA_MANA_SHIELD implemented in
                                    // Unit::CalculateAbsorbAndResist
    &Aura::HandleAuraModSkill,      // 98 SPELL_AURA_MOD_SKILL_TALENT
    &Aura::HandleAuraModAttackPower, // 99 SPELL_AURA_MOD_ATTACK_POWER
    &Aura::HandleUnused, // 100 SPELL_AURA_AURAS_VISIBLE obsolete? all player
                         // can see all auras now
    &Aura::HandleModResistancePercent, // 101 SPELL_AURA_MOD_RESISTANCE_PCT
    &Aura::HandleNoImmediateEffect,    // 102
    // SPELL_AURA_MOD_MELEE_ATTACK_POWER_VERSUS
    // implemented in Unit::MeleeDamageBonusDone
    &Aura::HandleAuraModTotalThreat, // 103 SPELL_AURA_MOD_TOTAL_THREAT
    &Aura::HandleAuraWaterWalk,      // 104 SPELL_AURA_WATER_WALK
    &Aura::HandleAuraFeatherFall,    // 105 SPELL_AURA_FEATHER_FALL
    &Aura::HandleAuraHover,          // 106 SPELL_AURA_HOVER
    &Aura::HandleAddModifier,        // 107 SPELL_AURA_ADD_FLAT_MODIFIER
    &Aura::HandleAddModifier,        // 108 SPELL_AURA_ADD_PCT_MODIFIER
    &Aura::HandleNoImmediateEffect,  // 109 SPELL_AURA_ADD_TARGET_TRIGGER
    &Aura::HandleModPowerRegenPCT,   // 110 SPELL_AURA_MOD_POWER_REGEN_PERCENT
    &Aura::HandleNoImmediateEffect,  // 111 SPELL_AURA_ADD_CASTER_HIT_TRIGGER
                                     // implemented in Unit::SelectMagnetTarget
    &Aura::HandleNoImmediateEffect,  // 112 SPELL_AURA_OVERRIDE_CLASS_SCRIPTS
                                     // implemented in diff functions.
    &Aura::HandleNoImmediateEffect,  // 113 SPELL_AURA_MOD_RANGED_DAMAGE_TAKEN
                                     // implemented in
                                     // Unit::MeleeDamageBonusTaken
    &Aura::HandleNoImmediateEffect,  // 114
                                     // SPELL_AURA_MOD_RANGED_DAMAGE_TAKEN_PCT
                                     // implemented in
                                     // Unit::MeleeDamageBonusTaken
    &Aura::HandleNoImmediateEffect,  // 115 SPELL_AURA_MOD_HEALING
                                     // implemented in
                                     // Unit::SpellBaseHealingBonusTaken
    &Aura::HandleNoImmediateEffect,  // 116 SPELL_AURA_MOD_REGEN_DURING_COMBAT
                                     // imppemented in Player::RegenerateAll and
                                     // Player::RegenerateHealth
    &Aura::HandleNoImmediateEffect,  // 117 SPELL_AURA_MOD_MECHANIC_RESISTANCE
                                     // implemented in Spell::HandleEffects
    &Aura::HandleNoImmediateEffect,  // 118 SPELL_AURA_MOD_HEALING_PCT
                                     // implemented in
                                     // Unit::SpellHealingBonusTaken
    &Aura::HandleUnused,          // 119 SPELL_AURA_SHARE_PET_TRACKING useless
    &Aura::HandleAuraUntrackable, // 120 SPELL_AURA_UNTRACKABLE
    &Aura::HandleAuraEmpathy,     // 121 SPELL_AURA_EMPATHY
    &Aura::HandleModOffhandDamagePercent, // 122
                                          // SPELL_AURA_MOD_OFFHAND_DAMAGE_PCT
    &Aura::HandleModTargetResistance, // 123 SPELL_AURA_MOD_TARGET_RESISTANCE
    &Aura::HandleAuraModRangedAttackPower, // 124
                                           // SPELL_AURA_MOD_RANGED_ATTACK_POWER
    &Aura::HandleNoImmediateEffect, // 125 SPELL_AURA_MOD_MELEE_DAMAGE_TAKEN
                                    // implemented in
                                    // Unit::MeleeDamageBonusTaken
    &Aura::HandleNoImmediateEffect, // 126 SPELL_AURA_MOD_MELEE_DAMAGE_TAKEN_PCT
                                    // implemented in
                                    // Unit::MeleeDamageBonusTaken
    &Aura::HandleNoImmediateEffect, // 127
    // SPELL_AURA_RANGED_ATTACK_POWER_ATTACKER_BONUS
    // implemented in Unit::MeleeDamageBonusDone
    &Aura::HandleModPossessPet,               // 128 SPELL_AURA_MOD_POSSESS_PET
    &Aura::HandleAuraModIncreaseSpeed,        // 129 SPELL_AURA_MOD_SPEED_ALWAYS
    &Aura::HandleAuraModIncreaseMountedSpeed, // 130
    // SPELL_AURA_MOD_MOUNTED_SPEED_ALWAYS
    &Aura::HandleNoImmediateEffect, // 131
                                    // SPELL_AURA_MOD_RANGED_ATTACK_POWER_VERSUS
                                    // implemented in Unit::MeleeDamageBonusDone
    &Aura::HandleAuraModIncreaseEnergyPercent, // 132
    // SPELL_AURA_MOD_INCREASE_ENERGY_PERCENT
    &Aura::HandleAuraModIncreaseHealthPercent, // 133
    // SPELL_AURA_MOD_INCREASE_HEALTH_PERCENT
    &Aura::HandleAuraModRegenInterrupt, // 134
                                        // SPELL_AURA_MOD_MANA_REGEN_INTERRUPT
    &Aura::HandleModHealingDone,        // 135 SPELL_AURA_MOD_HEALING_DONE
    &Aura::HandleNoImmediateEffect,   // 136 SPELL_AURA_MOD_HEALING_DONE_PERCENT
                                      // implemented in
                                      // Unit::SpellHealingBonusDone
    &Aura::HandleModTotalPercentStat, // 137
                                      // SPELL_AURA_MOD_TOTAL_STAT_PERCENTAGE
    &Aura::HandleModMeleeSpeedPct,    // 138 SPELL_AURA_MOD_MELEE_HASTE
    &Aura::HandleForceReaction,       // 139 SPELL_AURA_FORCE_REACTION
    &Aura::HandleAuraModRangedHaste,  // 140 SPELL_AURA_MOD_RANGED_HASTE
    &Aura::HandleRangedAmmoHaste,     // 141 SPELL_AURA_MOD_RANGED_AMMO_HASTE
    &Aura::HandleAuraModBaseResistancePCT, // 142
                                           // SPELL_AURA_MOD_BASE_RESISTANCE_PCT
    &Aura::HandleAuraModResistanceExclusive, // 143
    // SPELL_AURA_MOD_RESISTANCE_EXCLUSIVE
    &Aura::HandleAuraSafeFall,            // 144 SPELL_AURA_SAFE_FALL
                                          // implemented in
                                          // WorldSession::HandleMovementOpcodes
    &Aura::HandleUnused,                  // 145 SPELL_AURA_CHARISMA obsolete?
    &Aura::HandleUnused,                  // 146 SPELL_AURA_PERSUADED obsolete?
    &Aura::HandleModMechanicImmunityMask, // 147
                                          // SPELL_AURA_MECHANIC_IMMUNITY_MASK
                                          // implemented in
                                          // Unit::IsImmuneToSpell and
                                          // Unit::IsImmuneToSpellEffect (check
                                          // part)
    &Aura::HandleAuraRetainComboPoints,   // 148 SPELL_AURA_RETAIN_COMBO_POINTS
    &Aura::HandleNoImmediateEffect,       // 149 SPELL_AURA_RESIST_PUSHBACK
                                          // implemented in Spell::Delayed and
                                          // Spell::DelayedChannel
    &Aura::HandleShieldBlockValue,   // 150 SPELL_AURA_MOD_SHIELD_BLOCKVALUE_PCT
    &Aura::HandleAuraTrackStealthed, // 151 SPELL_AURA_TRACK_STEALTHED
    &Aura::HandleNoImmediateEffect,  // 152 SPELL_AURA_MOD_DETECTED_RANGE
                                     // implemented in
                                     // Creature::GetAggroDistance
    &Aura::HandleNoImmediateEffect,  // 153 SPELL_AURA_SPLIT_DAMAGE_FLAT
                                     // implemented in
                                     // Unit::CalculateAbsorbAndResist
    &Aura::HandleNoImmediateEffect,  // 154 SPELL_AURA_MOD_STEALTH_LEVEL
                                     // implemented in Unit::update_stealth
    &Aura::HandleNoImmediateEffect,  // 155 SPELL_AURA_MOD_WATER_BREATHING
                                     // implemented in Player::getMaxTimer
    &Aura::HandleNoImmediateEffect,  // 156 SPELL_AURA_MOD_REPUTATION_GAIN
                                     // implemented in
                                     // Player::CalculateReputationGain
    &Aura::HandleUnused, // 157 SPELL_AURA_PET_DAMAGE_MULTI (single test like
                         // spell 20782, also single for 214 aura)
    &Aura::HandleShieldBlockValue,  // 158 SPELL_AURA_MOD_SHIELD_BLOCKVALUE
    &Aura::HandleNoImmediateEffect, // 159 SPELL_AURA_NO_PVP_CREDIT
                                    // implemented in Player::RewardHonor
    &Aura::HandleNoImmediateEffect, // 160 SPELL_AURA_MOD_AOE_AVOIDANCE
                                    // implemented in Unit::MagicSpellHitResult
    &Aura::HandleNoImmediateEffect, // 161 SPELL_AURA_MOD_HEALTH_REGEN_IN_COMBAT
                                    // implemented in Player::RegenerateAll and
                                    // Player::RegenerateHealth
    &Aura::HandleAuraPowerBurn,     // 162 SPELL_AURA_POWER_BURN_MANA
    &Aura::HandleNoImmediateEffect, // 163 SPELL_AURA_MOD_CRIT_DAMAGE_BONUS
                                    // implemented in Unit::CalculateMeleeDamage
                                    // and Unit::SpellCriticalDamageBonus
    &Aura::HandleUnused,            // 164 useless, only one test spell
    &Aura::HandleNoImmediateEffect, // 165
    // SPELL_AURA_MELEE_ATTACK_POWER_ATTACKER_BONUS
    // implemented in Unit::MeleeDamageBonusDone
    &Aura::HandleAuraModAttackPowerPercent, // 166
                                            // SPELL_AURA_MOD_ATTACK_POWER_PCT
    &Aura::HandleAuraModRangedAttackPowerPercent, // 167
    // SPELL_AURA_MOD_RANGED_ATTACK_POWER_PCT
    &Aura::HandleNoImmediateEffect, // 168 SPELL_AURA_MOD_DAMAGE_DONE_VERSUS
                                    // implemented in
                                    // Unit::SpellDamageBonusDone,
                                    // Unit::MeleeDamageBonusDone
    &Aura::HandleNoImmediateEffect, // 169 SPELL_AURA_MOD_CRIT_PERCENT_VERSUS
                                    // implemented in Unit::DealDamageBySchool,
                                    // Unit::DoAttackDamage,
                                    // Unit::SpellCriticalBonus
    &Aura::HandleDetectAmore, // 170 SPELL_AURA_DETECT_AMORE       only for
                              // Detect Amore spell
    &Aura::HandleAuraModIncreaseSpeed, // 171 SPELL_AURA_MOD_SPEED_NOT_STACK
    &Aura::HandleAuraModIncreaseMountedSpeed, // 172
    // SPELL_AURA_MOD_MOUNTED_SPEED_NOT_STACK
    &Aura::HandleUnused, // 173 SPELL_AURA_ALLOW_CHAMPION_SPELLS  only for
                         // Proclaim Champion spell
    &Aura::HandleModSpellDamagePercentFromStat, // 174
    // SPELL_AURA_MOD_SPELL_DAMAGE_OF_STAT_PERCENT
    // implemented in
    // Unit::SpellBaseDamageBonusDone
    &Aura::HandleModSpellHealingPercentFromStat, // 175
    // SPELL_AURA_MOD_SPELL_HEALING_OF_STAT_PERCENT
    // implemented in
    // Unit::SpellBaseHealingBonusDone
    &Aura::HandleSpiritOfRedemption, // 176 SPELL_AURA_SPIRIT_OF_REDEMPTION
                                     // only for Spirit of Redemption spell, die
                                     // at aura end
    &Aura::HandleModAoeCharm,        // 177 SPELL_AURA_AOE_CHARM
    &Aura::HandleNoImmediateEffect,  // 178 SPELL_AURA_MOD_DEBUFF_RESISTANCE
                                     // implemented in Unit::MagicSpellHitResult
    &Aura::HandleNoImmediateEffect,  // 179
    // SPELL_AURA_MOD_ATTACKER_SPELL_CRIT_CHANCE
    // implemented in Unit::SpellCriticalBonus
    &Aura::HandleNoImmediateEffect, // 180
                                    // SPELL_AURA_MOD_FLAT_SPELL_DAMAGE_VERSUS
                                    // implemented in Unit::SpellDamageBonusDone
    &Aura::HandleUnused, // 181 SPELL_AURA_MOD_FLAT_SPELL_CRIT_DAMAGE_VERSUS
                         // unused
    &Aura::HandleAuraModResistenceOfStatPercent, // 182
    // SPELL_AURA_MOD_RESISTANCE_OF_STAT_PERCENT
    &Aura::HandleNoImmediateEffect, // 183 SPELL_AURA_MOD_CRITICAL_THREAT only
                                    // used in 28746, implemented in
                                    // ThreatCalcHelper::CalcThreat
    &Aura::HandleNoImmediateEffect, // 184
                                    // SPELL_AURA_MOD_ATTACKER_MELEE_HIT_CHANCE
                                    // implemented in
                                    // Unit::RollMeleeOutcomeAgainst
    &Aura::HandleNoImmediateEffect, // 185
                                    // SPELL_AURA_MOD_ATTACKER_RANGED_HIT_CHANCE
                                    // implemented in
                                    // Unit::RollMeleeOutcomeAgainst
    &Aura::HandleNoImmediateEffect, // 186
                                    // SPELL_AURA_MOD_ATTACKER_SPELL_HIT_CHANCE
                                    // implemented in Unit::MagicSpellHitResult
    &Aura::HandleNoImmediateEffect, // 187
                                    // SPELL_AURA_MOD_ATTACKER_MELEE_CRIT_CHANCE
                                    // implemented in
                                    // Unit::GetUnitCriticalChance
    &Aura::HandleNoImmediateEffect, // 188
    // SPELL_AURA_MOD_ATTACKER_RANGED_CRIT_CHANCE
    // implemented in Unit::GetUnitCriticalChance
    &Aura::HandleModRating,         // 189 SPELL_AURA_MOD_RATING
    &Aura::HandleNoImmediateEffect, // 190
                                    // SPELL_AURA_MOD_FACTION_REPUTATION_GAIN
                                    // implemented in
                                    // Player::CalculateReputationGain
    &Aura::HandleAuraModUseNormalSpeed,  // 191
                                         // SPELL_AURA_USE_NORMAL_MOVEMENT_SPEED
    &Aura::HandleModMeleeRangedSpeedPct, // 192
                                         // SPELL_AURA_MOD_MELEE_RANGED_HASTE
    &Aura::HandleModCombatSpeedPct, // 193 SPELL_AURA_HASTE_ALL (in fact combat
                                    // (any type attack) speed pct)
    &Aura::HandleUnused, // 194 SPELL_AURA_MOD_DEPRICATED_1 not used now (old
                         // SPELL_AURA_MOD_SPELL_DAMAGE_OF_INTELLECT)
    &Aura::HandleUnused, // 195 SPELL_AURA_MOD_DEPRICATED_2 not used now (old
                         // SPELL_AURA_MOD_SPELL_HEALING_OF_INTELLECT)
    &Aura::HandleNULL,   // 196 SPELL_AURA_MOD_COOLDOWN
    &Aura::HandleNoImmediateEffect, // 197
    // SPELL_AURA_MOD_ATTACKER_SPELL_AND_WEAPON_CRIT_CHANCE
    // implemented in Unit::SpellCriticalBonus
    // Unit::GetUnitCriticalChance
    &Aura::HandleUnused,            // 198 SPELL_AURA_MOD_ALL_WEAPON_SKILLS
    &Aura::HandleNoImmediateEffect, // 199
                                    // SPELL_AURA_MOD_INCREASES_SPELL_PCT_TO_HIT
                                    // implemented in Unit::MagicSpellHitResult
    &Aura::HandleNoImmediateEffect, // 200 SPELL_AURA_MOD_XP_PCT
                                    // implemented in Player::GiveXP
    &Aura::HandleAuraAllowFlight,   // 201 SPELL_AURA_FLY
                                    // this aura enable flight mode...
    &Aura::HandleNoImmediateEffect, // 202 SPELL_AURA_IGNORE_COMBAT_RESULT
                                    // implemented in Unit::MeleeSpellHitResult
    &Aura::HandleNoImmediateEffect, // 203
                                    // SPELL_AURA_MOD_ATTACKER_MELEE_CRIT_DAMAGE
                                    // implemented in Unit::CalculateMeleeDamage
                                    // and Unit::SpellCriticalDamageBonus
    &Aura::HandleNoImmediateEffect, // 204
    // SPELL_AURA_MOD_ATTACKER_RANGED_CRIT_DAMAGE
    // implemented in Unit::CalculateMeleeDamage
    // and Unit::SpellCriticalDamageBonus
    &Aura::HandleNoImmediateEffect, // 205
                                    // SPELL_AURA_MOD_ATTACKER_SPELL_CRIT_DAMAGE
                                    // implemented in
                                    // Unit::SpellCriticalDamageBonus
    &Aura::HandleAuraModIncreaseFlightSpeed, // 206 SPELL_AURA_MOD_FLIGHT_SPEED
    &Aura::HandleAuraModIncreaseFlightSpeed, // 207
    // SPELL_AURA_MOD_FLIGHT_SPEED_MOUNTED
    &Aura::HandleAuraModIncreaseFlightSpeed, // 208
    // SPELL_AURA_MOD_FLIGHT_SPEED_STACKING
    &Aura::HandleAuraModIncreaseFlightSpeed, // 209
    // SPELL_AURA_MOD_FLIGHT_SPEED_MOUNTED_STACKING
    &Aura::HandleAuraModIncreaseFlightSpeed, // 210
    // SPELL_AURA_MOD_FLIGHT_SPEED_NOT_STACKING
    &Aura::HandleAuraModIncreaseFlightSpeed, // 211
    // SPELL_AURA_MOD_FLIGHT_SPEED_MOUNTED_NOT_STACKING
    &Aura::HandleAuraModRangedAttackPowerOfStatPercent, // 212
    // SPELL_AURA_MOD_RANGED_ATTACK_POWER_OF_STAT_PERCENT
    &Aura::HandleNoImmediateEffect, // 213 SPELL_AURA_MOD_RAGE_FROM_DAMAGE_DEALT
                                    // implemented in Player::RewardRage
    &Aura::HandleNULL,              // 214 Tamed Pet Passive
    &Aura::HandleArenaPreparation,  // 215 SPELL_AURA_ARENA_PREPARATION
    &Aura::HandleModCastingSpeed,   // 216 SPELL_AURA_HASTE_SPELLS
    &Aura::HandleUnused, // 217                                   unused
    &Aura::HandleAuraModRangedHaste, // 218 SPELL_AURA_HASTE_RANGED
    &Aura::HandleModManaRegen,       // 219 SPELL_AURA_MOD_MANA_REGEN_FROM_STAT
    &Aura::HandleUnused,             // 220 SPELL_AURA_MOD_RATING_FROM_STAT
    &Aura::HandleNULL,               // 221 ignored
    &Aura::HandleUnused,             // 222 unused
    &Aura::HandleNULL,               // 223 Cold Stare
    &Aura::HandleUnused,             // 224 unused
    &Aura::HandleNoImmediateEffect,  // 225 SPELL_AURA_PRAYER_OF_MENDING
    &Aura::HandleAuraPeriodicDummy,  // 226 SPELL_AURA_PERIODIC_DUMMY
    &Aura::HandlePeriodicTriggerSpellWithValue, // 227
    // SPELL_AURA_PERIODIC_TRIGGER_SPELL_WITH_VALUE
    &Aura::HandleNoImmediateEffect, // 228 SPELL_AURA_DETECT_STEALTH
    &Aura::HandleNoImmediateEffect, // 229 SPELL_AURA_MOD_AOE_DAMAGE_AVOIDANCE
                                    // implemented in
                                    // Unit::SpellDamageBonusTaken
    &Aura::HandleAuraModIncreaseMaxHealth, // 230 Commanding Shout
    &Aura::HandleNoImmediateEffect,        // 231
    // SPELL_AURA_PROC_TRIGGER_SPELL_WITH_VALUE
    &Aura::HandleNoImmediateEffect, // 232 SPELL_AURA_MECHANIC_DURATION_MOD
                                    // implement in Unit::CalculateAuraDuration
    &Aura::HandleNULL, // 233 set model id to the one of the creature with id
                       // m_modifier.m_miscvalue
    &Aura::HandleNoImmediateEffect, // 234
    // SPELL_AURA_MECHANIC_DURATION_MOD_NOT_STACK
    // implement in Unit::CalculateAuraDuration
    &Aura::HandleAuraModDispelResist, // 235 SPELL_AURA_MOD_DISPEL_RESIST
                                      // implement in Unit::MagicSpellHitResult
    &Aura::HandleUnused,              // 236 unused
    &Aura::HandleModSpellDamagePercentFromAttackPower, // 237
    // SPELL_AURA_MOD_SPELL_DAMAGE_OF_ATTACK_POWER
    // implemented in
    // Unit::SpellBaseDamageBonusDone
    &Aura::HandleModSpellHealingPercentFromAttackPower, // 238
    // SPELL_AURA_MOD_SPELL_HEALING_OF_ATTACK_POWER
    // implemented in
    // Unit::SpellBaseHealingBonusDone
    &Aura::HandleAuraModScale, // 239 SPELL_AURA_MOD_SCALE_2 only in
                               // Noggenfogger Elixir (16595) before 2.3.0 aura
                               // 61
    &Aura::HandleAuraModExpertise,   // 240 SPELL_AURA_MOD_EXPERTISE
    &Aura::HandleForceMoveForward,   // 241 Forces the caster to move forward
    &Aura::HandleUnused,             // 242 unused
    &Aura::HandleUnused,             // 243 used by two test spells
    &Aura::HandleComprehendLanguage, // 244 SPELL_AURA_COMPREHEND_LANGUAGE
    &Aura::HandleUnused,             // 245 unused
    &Aura::HandleUnused,             // 246 unused
    &Aura::HandleAuraMirrorImage,    // 247 SPELL_AURA_MIRROR_IMAGE
                                     // target to become a clone of the caster
    &Aura::HandleNoImmediateEffect,  // 248 SPELL_AURA_MOD_COMBAT_RESULT_CHANCE
                                     // implemented in
                                     // Unit::RollMeleeOutcomeAgainst
    &Aura::HandleNULL,               // 249
    &Aura::HandleAuraModIncreaseHealth, // 250 SPELL_AURA_MOD_INCREASE_HEALTH_2
    &Aura::HandleNULL,                  // 251 SPELL_AURA_MOD_ENEMY_DODGE
    &Aura::HandleUnused,                // 252 unused
    &Aura::HandleUnused,                // 253 unused
    &Aura::HandleUnused,                // 254 unused
    &Aura::HandleUnused,                // 255 unused
    &Aura::HandleUnused,                // 256 unused
    &Aura::HandleUnused,                // 257 unused
    &Aura::HandleUnused,                // 258 unused
    &Aura::HandleUnused,                // 259 unused
    &Aura::HandleUnused,                // 260 unused
    &Aura::HandleNULL // 261 SPELL_AURA_261 some phased state (44856 spell)
};

static AuraType const frozenAuraTypes[] = {
    SPELL_AURA_MOD_ROOT, SPELL_AURA_MOD_STUN, SPELL_AURA_NONE};

Aura::Aura(SpellEntry const* spellproto, SpellEffectIndex eff,
    int32* currentBasePoints, AuraHolder* holder, Unit* target, Unit* caster,
    Item* castItem)
  : m_spellmod(nullptr), m_periodicTimer(0), m_periodicTick(0),
    m_removeMode(AURA_REMOVE_BY_DEFAULT), m_effIndex(eff), m_positive(false),
    m_isPeriodic(false), m_isAreaAura(false), m_isPersistent(false),
    m_disabled(false), m_spellAuraHolder(holder)
{
    assert(target);
    assert(spellproto &&
           spellproto == sSpellStore.LookupEntry(spellproto->Id) &&
           "`info` must be pointer to sSpellStore element");

    m_currentBasePoints = currentBasePoints ?
                              *currentBasePoints :
                              spellproto->CalculateSimpleValue(eff);

    m_positive = IsPositiveEffect(spellproto, m_effIndex);
    m_applyTime = WorldTimer::time_no_syscall();

    int32 damage;
    if (!caster)
        damage = m_currentBasePoints;
    else
    {
        damage = caster->CalculateSpellDamage(
            target, spellproto, m_effIndex, &m_currentBasePoints);

        if (!damage && castItem && castItem->GetItemSuffixFactor())
        {
            ItemRandomSuffixEntry const* item_rand_suffix =
                sItemRandomSuffixStore.LookupEntry(
                    abs(castItem->GetItemRandomPropertyId()));
            if (item_rand_suffix)
            {
                for (int k = 0; k < 3; ++k)
                {
                    SpellItemEnchantmentEntry const* pEnchant =
                        sSpellItemEnchantmentStore.LookupEntry(
                            item_rand_suffix->enchant_id[k]);
                    if (pEnchant)
                    {
                        for (int t = 0; t < 3; ++t)
                        {
                            if (pEnchant->spellid[t] != spellproto->Id)
                                continue;

                            damage =
                                uint32((item_rand_suffix->prefix[k] *
                                           castItem->GetItemSuffixFactor()) /
                                       10000);
                            break;
                        }
                    }

                    if (damage)
                        break;
                }
            }
        }
    }

    // Some spells start out stacked
    damage *= holder->GetStackAmount();

    LOG_DEBUG(logging,
        "Aura: construct Spellid : %u, Aura : %u Target : %d Damage : %d",
        spellproto->Id, spellproto->EffectApplyAuraName[eff],
        spellproto->EffectImplicitTargetA[eff], damage);

    SetModifier(AuraType(spellproto->EffectApplyAuraName[eff]), damage,
        spellproto->EffectAmplitude[eff], spellproto->EffectMiscValue[eff]);

    Player* modOwner = caster ? caster->GetSpellModOwner() : nullptr;

    // Increase tick speed based on haste
    if (modOwner &&
        spellproto->HasAttribute(SPELL_ATTR_EX5_APPLIES_HASTE_BONUS))
        m_modifier.periodictime *= modOwner->GetFloatValue(UNIT_MOD_CAST_SPEED);

    // Apply periodic time mod
    if (modOwner && m_modifier.periodictime)
        modOwner->ApplySpellMod(
            spellproto->Id, SPELLMOD_ACTIVATION_TIME, m_modifier.periodictime);

    // Start periodic on next tick or at aura apply
    if (!spellproto->HasAttribute(SPELL_ATTR_EX5_START_PERIODIC_AT_APPLY))
        m_periodicTimer = m_modifier.periodictime;
}

Aura::~Aura()
{
}

AreaAura::AreaAura(SpellEntry const* spellproto, SpellEffectIndex eff,
    int32* currentBasePoints, AuraHolder* holder, Unit* target, Unit* caster,
    Item* castItem)
  : Aura(spellproto, eff, currentBasePoints, holder, target, caster, castItem)
{
    m_isAreaAura = true;

    // caster==NULL in constructor args if target==caster in fact
    Unit* caster_ptr = caster ? caster : target;

    m_radius = GetSpellRadius(sSpellRadiusStore.LookupEntry(
        spellproto->EffectRadiusIndex[m_effIndex]));
    if (Player* modOwner = caster_ptr->GetSpellModOwner())
        modOwner->ApplySpellMod(spellproto->Id, SPELLMOD_RADIUS, m_radius);

    switch (spellproto->Effect[eff])
    {
    case SPELL_EFFECT_APPLY_AREA_AURA_PARTY:
        m_areaAuraType = AREA_AURA_PARTY;
        break;
    case SPELL_EFFECT_APPLY_AREA_AURA_FRIEND:
        m_areaAuraType = AREA_AURA_FRIEND;
        break;
    case SPELL_EFFECT_APPLY_AREA_AURA_ENEMY:
        m_areaAuraType = AREA_AURA_ENEMY;
        if (target == caster_ptr)
            m_modifier.m_auraname =
                SPELL_AURA_NONE; // Do not do any effect on self
        break;
    case SPELL_EFFECT_APPLY_AREA_AURA_PET:
        m_areaAuraType = AREA_AURA_PET;
        break;
    case SPELL_EFFECT_APPLY_AREA_AURA_OWNER:
        m_areaAuraType = AREA_AURA_OWNER;
        if (target == caster_ptr)
            m_modifier.m_auraname = SPELL_AURA_NONE;
        break;
    default:
        logging.error("Wrong spell effect in AreaAura constructor");
        assert(false);
        break;
    }

    // totems are immune to any kind of area auras
    if (target->GetTypeId() == TYPEID_UNIT && ((Creature*)target)->IsTotem())
        m_modifier.m_auraname = SPELL_AURA_NONE;

    // do first target search right away
    new_targets_search_timer_.SetInterval(0);
}

AreaAura::~AreaAura()
{
}

PersistentAreaAura::PersistentAreaAura(SpellEntry const* spellproto,
    SpellEffectIndex eff, int32* currentBasePoints, AuraHolder* holder,
    Unit* target, Unit* caster, Item* castItem)
  : Aura(spellproto, eff, currentBasePoints, holder, target, caster, castItem)
{
    m_isPersistent = true;
}

PersistentAreaAura::~PersistentAreaAura()
{
}

SingleEnemyTargetAura::SingleEnemyTargetAura(SpellEntry const* spellproto,
    SpellEffectIndex eff, int32* currentBasePoints, AuraHolder* holder,
    Unit* target, Unit* caster, Item* castItem)
  : Aura(spellproto, eff, currentBasePoints, holder, target, caster, castItem)
{
    if (caster)
        m_castersTargetGuid = caster->GetTypeId() == TYPEID_PLAYER ?
                                  ((Player*)caster)->GetSelectionGuid() :
                                  caster->GetTargetGuid();
}

SingleEnemyTargetAura::~SingleEnemyTargetAura()
{
}

Unit* SingleEnemyTargetAura::GetTriggerTarget() const
{
    return ObjectAccessor::GetUnit(
        *(m_spellAuraHolder->GetTarget()), m_castersTargetGuid);
}

Aura* CreateAura(SpellEntry const* spellproto, SpellEffectIndex eff,
    int32* currentBasePoints, AuraHolder* holder, Unit* target, Unit* caster,
    Item* castItem)
{
    if (IsAreaAuraEffect(spellproto->Effect[eff]))
        return new AreaAura(spellproto, eff, currentBasePoints, holder, target,
            caster, castItem);

    uint32 triggeredSpellId = spellproto->EffectTriggerSpell[eff];

    if (SpellEntry const* triggeredSpellInfo =
            sSpellStore.LookupEntry(triggeredSpellId))
        for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
            if (triggeredSpellInfo->EffectImplicitTargetA[i] ==
                TARGET_SINGLE_ENEMY)
                return new SingleEnemyTargetAura(spellproto, eff,
                    currentBasePoints, holder, target, caster, castItem);

    return new Aura(
        spellproto, eff, currentBasePoints, holder, target, caster, castItem);
}

AuraHolder* CreateAuraHolder(const SpellEntry* spellproto, Unit* target,
    WorldObject* caster, Item* castItem, const SpellEntry* triggeredByProto)
{
    return new AuraHolder(
        spellproto, target, caster, castItem, triggeredByProto);
}

void Aura::SetModifier(AuraType t, int32 a, uint32 pt, int32 miscValue)
{
    m_modifier.m_auraname = t;
    m_modifier.m_amount = a;
    m_modifier.m_miscvalue = miscValue;
    m_modifier.periodictime = pt;
}

void Aura::Update(uint32 diff)
{
    assert(!m_disabled && "Aura::Update called for a deleted aura");

    if (m_isPeriodic)
    {
        // NPCs with auras that are kept inactive can have huge update diffs; we
        // don't want an insane amount of ticks to happen just because an NPC
        // got reactivated.
        if (diff > 5000)
        {
            auto caster = GetCaster();
            if (caster && caster->GetTypeId() == TYPEID_UNIT)
                diff = 0; // Treat reactivation as no time expired
        }

        m_periodicTimer -= diff;

        // tick also at m_periodicTimer==0 to prevent lost last tick in case max
        // m_duration == (max m_periodicTimer)*N
        auto max_tick = GetAuraMaxTicks();
        while (m_periodicTimer <= 0 && (!max_tick || m_periodicTick < max_tick))
        {
            // update before applying (aura can be removed in TriggerSpell or
            // PeriodicTick calls)
            m_periodicTimer += m_modifier.periodictime;
            ++m_periodicTick; // for some infinity auras in some cases can
                              // overflow and reset
            PeriodicTick();

            // TODO: Verify having a periodic without an amplitude isn't
            //       possible in the code, and remove safety check
            if (m_modifier.periodictime <= 0)
                break;
        }
    }
}

void AreaAura::Update(uint32 diff)
{
    assert(!m_disabled && "AreaAura::Update called for a deleted aura");

    // update for the caster of the aura
    if (GetCasterGuid() == GetTarget()->GetObjectGuid())
    {
        Unit* caster = GetTarget();

        bool search_new_targets = false;
        new_targets_search_timer_.Update(diff);
        if (new_targets_search_timer_.Passed())
        {
            search_new_targets = true;

            if (new_targets_search_timer_.GetInterval() == 0)
                new_targets_search_timer_.SetInterval(2000);
            new_targets_search_timer_.Reset();
        }

        if (search_new_targets && !caster->hasUnitState(UNIT_STAT_ISOLATED) &&
            !(caster->isDead() &&
                !GetSpellProto()->HasAttribute(
                    SPELL_ATTR_EX3_DEATH_PERSISTENT)))
        {
            Unit* owner = caster->GetCharmerOrOwner();
            if (!owner)
                owner = caster;
            std::vector<Unit*> targets;

            switch (m_areaAuraType)
            {
            case AREA_AURA_PARTY:
            {
                Group* group = nullptr;

                if (owner->GetTypeId() == TYPEID_PLAYER &&
                    !static_cast<Player*>(owner)->duel)
                    group = ((Player*)owner)->GetGroup();

                // Grouped player owned unit
                if (group)
                {
                    uint8 subgroup = ((Player*)owner)->GetSubGroup();
                    for (auto member : group->members(true))
                    {
                        if (member->isAlive() &&
                            member->GetSubGroup() == subgroup &&
                            caster->IsFriendlyTo(member) && !member->duel)
                        {
                            if (caster->IsWithinDistInMap(member, m_radius))
                                targets.push_back(member);
                            Pet* pet = member->GetPet();
                            if (pet && pet->isAlive() &&
                                caster->IsWithinDistInMap(pet, m_radius))
                                targets.push_back(pet);
                        }
                    }
                }
                // Non-grouped player owned unit
                else if (owner->GetTypeId() == TYPEID_PLAYER ||
                         static_cast<Creature*>(owner)->IsPlayerPet())
                {
                    // Add owner
                    if (owner != caster &&
                        caster->IsWithinDistInMap(owner, m_radius))
                        targets.push_back(owner);
                    // Add caster's pet
                    Unit* pet = caster->GetPet();
                    if (pet && caster->IsWithinDistInMap(pet, m_radius))
                        targets.push_back(pet);
                }
                // Regular NPC
                else if (caster->GetTypeId() == TYPEID_UNIT &&
                         !static_cast<Creature*>(caster)->IsPlayerPet())
                {
                    auto ccaster = static_cast<Creature*>(caster);
                    if (auto cgrp = ccaster->GetGroup())
                    {
                        for (auto& member : *cgrp)
                        {
                            if (!caster->isAlive())
                                continue;
                            if (caster->IsWithinDistInMap(member, m_radius))
                                targets.push_back(member);
                            Pet* pet = member->GetPet();
                            if (pet && pet->isAlive() &&
                                caster->IsWithinDistInMap(pet, m_radius))
                                targets.push_back(pet);
                        }
                    }
                    else
                    {
                        targets = maps::visitors::yield_set<Unit, Creature, Pet,
                            SpecialVisCreature, TemporarySummon>{}(caster,
                            m_radius,
                            maps::checks::friendly_status{caster,
                                maps::checks::friendly_status::not_hostile});
                    }
                }
                break;
            }
            case AREA_AURA_FRIEND:
            {
                targets = maps::visitors::yield_set<Unit, Player, Creature, Pet,
                    SpecialVisCreature, TemporarySummon>{}(caster, m_radius,
                    maps::checks::friendly_status{
                        caster, maps::checks::friendly_status::friendly});
                break;
            }
            case AREA_AURA_ENEMY:
            {
                bool targetForPlayer = caster->IsControlledByPlayer();
                targets = maps::visitors::yield_set<Unit, Player, Creature, Pet,
                    SpecialVisCreature, TemporarySummon>{}(caster, m_radius,
                    [targetForPlayer, caster](Unit* u)
                    {
                        // Check contains checks for: live, non-selectable,
                        // non-attackable
                        // flags, flight check and GM check, ignore totems
                        if (!u->isTargetableForAttack())
                            return false;

                        if (u->GetTypeId() == TYPEID_UNIT &&
                            ((Creature*)u)->IsTotem())
                            return false;

                        if ((targetForPlayer ? !caster->IsFriendlyTo(u) :
                                               caster->IsHostileTo(u)))
                            return true;

                        return false;
                    });
                break;
            }
            case AREA_AURA_OWNER:
            case AREA_AURA_PET:
            {
                if (owner != caster &&
                    caster->IsWithinDistInMap(owner, m_radius))
                    targets.push_back(owner);
                break;
            }
            }

            for (auto& target : targets)
            {
                if (GetSpellProto()->HasAttribute(
                        SPELL_ATTR_EX3_ONLY_TARGET_PLAYERS) &&
                    target->GetTypeId() != TYPEID_PLAYER)
                    continue;

                // flag for seelction is need apply aura to current iteration
                // target
                bool apply = true;

                // If we have an AreaAura with the same Id, we do some basic
                // checks to see if it stacks, otherwise we skip attempting to
                // apply it
                (target)->loop_auras([&apply, this, target](AuraHolder* holder)
                    {
                        Aura* aur = holder->GetAura(m_effIndex);

                        if (!aur)
                            return true; // continue

                        // we only care about auras in the same chain
                        if (sSpellMgr::Instance()->GetFirstSpellInChain(
                                aur->GetId()) !=
                            sSpellMgr::Instance()->GetFirstSpellInChain(
                                GetId()))
                            return true; // continue

                        // Heroic & Inspiring Presence can stack to 2% (one for
                        // caster & one from another draenei)
                        if (holder->GetId() == 6562 || holder->GetId() == 28878)
                        {
                            // If holder is casted by target, we ignore this
                            // aura
                            if (holder->GetCasterGuid() ==
                                (target)->GetObjectGuid())
                                return true; // continue
                        }

                        switch (m_areaAuraType)
                        {
                        case AREA_AURA_ENEMY:
                            // non caster self-casted auras (non stacked)
                            if (aur->GetModifier()->m_auraname !=
                                SPELL_AURA_NONE)
                                apply = false;
                            break;
                        default:
                            // in generic case not allow stacking area auras
                            apply = false;
                            break;
                        }

                        // We can't apply if this caster already has aura on
                        if (holder->GetCasterGuid() == GetCasterGuid())
                            apply = false;
                        // DB can override these stacking rules
                        else if (buff_stacks::area_aura_stacks(holder->GetId()))
                            apply = true;

                        return apply; // break when apply is false
                    });

                if (!apply)
                    continue;

                if (SpellEntry const* actualSpellInfo =
                        sSpellMgr::Instance()->SelectAuraRankForLevel(
                            GetSpellProto(), (target)->getLevel()))
                {
                    int32 actualBasePoints = m_currentBasePoints;
                    // recalculate basepoints for lower rank (all AreaAura spell
                    // not use custom basepoints?)
                    if (actualSpellInfo != GetSpellProto())
                        actualBasePoints =
                            actualSpellInfo->CalculateSimpleValue(m_effIndex);

                    AuraHolder* holder = (target)->get_aura(
                        actualSpellInfo->Id, GetCasterGuid());

                    bool addedToExisting = true;
                    if (!holder)
                    {
                        holder =
                            CreateAuraHolder(actualSpellInfo, (target), caster);
                        addedToExisting = false;
                    }

                    holder->SetAuraDuration(GetAuraDuration());

                    auto aur = new AreaAura(actualSpellInfo, m_effIndex,
                        &actualBasePoints, holder, (target), caster, nullptr);
                    holder->AddAura(aur, m_effIndex);

                    if (addedToExisting)
                    {
                        (target)->AddAuraToModList(aur);
                        aur->ApplyModifier(true, true);
                    }
                    else
                        (target)->AddAuraHolder(holder);
                }
            }
        }

        Aura::Update(diff);
    }
    else // aura at non-caster
    {
        Unit* caster = GetCaster();
        Unit* target = GetTarget();

        Aura::Update(diff);
        if (m_disabled)
            return;

        // remove aura if out-of-range from caster (after teleport for example)
        // or caster is isolated or caster no longer has the aura
        // or caster is (no longer) friendly
        // or the caster is indeed dead (and the spell is not death persistent)
        // or if the aura affects a duelling target that isn't our pet/charm or
        // owner
        bool needFriendly = (m_areaAuraType == AREA_AURA_ENEMY ? false : true);
        if (!caster || caster->hasUnitState(UNIT_STAT_ISOLATED) ||
            !caster->IsWithinDistInMap(target, m_radius) ||
            !caster->has_aura(GetId()) ||
            caster->IsFriendlyTo(target) != needFriendly ||
            (caster->isDead() &&
                !GetSpellProto()->HasAttribute(
                    SPELL_ATTR_EX3_DEATH_PERSISTENT)) ||
            (caster->GetTypeId() == TYPEID_PLAYER &&
                static_cast<Player*>(caster)->duel &&
                !target->controlled_by(caster)) ||
            (target->GetTypeId() == TYPEID_PLAYER &&
                static_cast<Player*>(target)->duel &&
                !caster->controlled_by(target)))
        {
            Disable();
        }
        else if (m_areaAuraType == AREA_AURA_PARTY)
        {
            // Regular NPCs
            if (caster->GetTypeId() == TYPEID_UNIT &&
                !static_cast<Creature*>(caster)->IsPlayerPet())
            {
                // No cancel scenario
            }
            // Player or player-owned NPCs,
            // in same sub-group (unless target == owner or target == pet)
            else if (caster->GetCharmerOrOwnerGuid() !=
                         target->GetObjectGuid() &&
                     caster->GetObjectGuid() != target->GetCharmerOrOwnerGuid())
            {
                Player* check = caster->GetCharmerOrOwnerPlayerOrPlayerItself();

                Group* pGroup = check ? check->GetGroup() : nullptr;
                if (pGroup)
                {
                    Player* checkTarget =
                        target->GetCharmerOrOwnerPlayerOrPlayerItself();
                    if (!checkTarget ||
                        !pGroup->SameSubGroup(check, checkTarget))
                        Disable();
                }
                else
                    Disable();
            }
        }
        else if (m_areaAuraType == AREA_AURA_PET ||
                 m_areaAuraType == AREA_AURA_OWNER)
        {
            if (target->GetObjectGuid() != caster->GetCharmerOrOwnerGuid())
                Disable();
        }
    }
}

void PersistentAreaAura::Update(uint32 diff)
{
    assert(
        !m_disabled && "PersistentAreaAura::Update called for a deleted aura");

    bool remove = true;

    // remove the aura if its caster or the dynamic object causing it was
    // removed or if the target moves too far from the dynamic object
    if (Unit* caster = GetCaster())
    {
        auto dynObjs = caster->GetAllDynObjects(GetId(), GetEffIndex());
        for (auto dynObj : dynObjs)
        {
            auto target = GetTarget();
            if ((GetSpellProto()->HasAttribute(
                     SPELL_ATTR_CUSTOM1_AOE_FORCE_LOS) &&
                    !dynObj->IsWithinWmoLOSInMap(target)) ||
                !target->IsWithinDistInMap(dynObj, dynObj->GetRadius()))
            {
                dynObj->RemoveAffected(target);
            }
            else
            {
                remove = false;
            }
        }
    }

    Aura::Update(diff);

    if (remove && !m_disabled)
        Disable();
}

void Aura::Disable(AuraRemoveMode mode, bool removeauraholder)
{
    Unit* target = GetHolder()->GetTarget();
    AuraHolder* holder = GetHolder();

    // remove from list before mods removing (prevent cyclic calls, mods added
    // before including to aura list - use reverse order)
    target->RemoveAuraFromModList(this);

    // Set remove mode
    SetRemoveMode(mode);

    // aura _MUST_ be removed from holder before unapplied
    // unapply code expects the aura to not be on the holder
    // it could also happen, that if the target dies in the unapplication
    // process, it'd be invoked twice
    holder->DisableAura(m_effIndex);

    // some auras also need to apply modifier (on caster) on remove
    if (mode == AURA_REMOVE_BY_DELETE)
    {
        switch (GetModifier()->m_auraname)
        {
        // need properly undo any auras with player-caster mover set (or will
        // crash at next caster move packet)
        case SPELL_AURA_MOD_POSSESS:
        case SPELL_AURA_MOD_POSSESS_PET:
            ApplyModifier(false, true);
            break;
        default:
            break;
        }
    }
    else
        ApplyModifier(false, true);

    // Remove target from persistent area aura's dyn objects
    if (IsPersistent())
    {
        if (Unit* caster = GetCaster())
        {
            auto dyn_objs = caster->GetAllDynObjects(GetId(), GetEffIndex());
            for (auto& dyn_obj : dyn_objs)
                if (dyn_obj)
                    (dyn_obj)->RemoveAffected(target);
        }
    }

    if (removeauraholder)
        return; // Skip removal below if we're called from inside of
                // Unit::RemoveAuraHolder

    if (holder->IsDisabled())
        return; // Could be disabled by ApplyModifier

    // Remove aura holder if all auras on the holder are now disabled
    bool has_aura = false;
    for (int i = 0; i < MAX_EFFECT_INDEX && !has_aura; ++i)
        if (holder->GetAura(SpellEffectIndex(i)) != nullptr)
            has_aura = true;
    if (!has_aura)
        target->RemoveAuraHolder(holder, mode);
}

void Aura::ApplyModifier(bool apply, bool Real)
{
    AuraType aura = m_modifier.m_auraname;

    if (aura < TOTAL_AURAS)
        (*this.*AuraHandler[aura])(apply, Real);
}

bool Aura::isAffectedOnSpell(SpellEntry const* spell) const
{
    if (m_spellmod)
        return m_spellmod->isAffectedOnSpell(spell);

    // Check family name
    if (spell->SpellFamilyName != GetSpellProto()->SpellFamilyName)
        return false;

    ClassFamilyMask mask =
        sSpellMgr::Instance()->GetSpellAffectMask(GetId(), GetEffIndex());
    return spell->IsFitToFamilyMask(mask);
}

bool Aura::CanProcFrom(SpellEntry const* spell, uint32 EventProcEx,
    uint32 procEx, bool active, bool useClassMask) const
{
    // Check EffectClassMask (in pre-3.x stored in spell_affect in fact)
    ClassFamilyMask mask =
        sSpellMgr::Instance()->GetSpellAffectMask(GetId(), GetEffIndex());

    // if no class mask defined, or spell_proc_event has SpellFamilyName=0 -
    // allow proc
    if (!useClassMask || !mask)
    {
        if (!(EventProcEx & PROC_EX_EX_TRIGGER_ALWAYS))
        {
            // Check for extra req (if none) and hit/crit
            if (EventProcEx == PROC_EX_NONE)
            {
                // No extra req, so can trigger only for active (damage/healing
                // present) and hit/crit
                if ((procEx & (PROC_EX_NORMAL_HIT | PROC_EX_CRITICAL_HIT)) &&
                    active)
                    return true;
                else
                    return false;
            }
            else // Passive spells hits here only if resist/reflect/immune/evade
            {
                // Passive spells can`t trigger if need hit (exclude cases when
                // procExtra include non-active flags)
                if ((EventProcEx & (PROC_EX_NORMAL_HIT | PROC_EX_CRITICAL_HIT) &
                        procEx) &&
                    !active)
                    return false;
            }
        }
        return true;
    }
    else
    {
        // SpellFamilyName check is performed in
        // SpellMgr::IsSpellProcEventCanTriggeredBy and it is done once for
        // whole holder
        // note: SpellFamilyName is not checked if no spell_proc_event is
        // defined
        return mask.IsFitToFamilyMask(spell->SpellFamilyFlags);
    }
}

void Aura::ReapplyAffectedPassiveAuras(Unit* target, bool owner_mode)
{
    // we need store cast item guids for self casted spells
    // expected that not exist permanent auras from stackable auras from
    // different items
    std::map<uint32, ObjectGuid> affectedSelf;

    std::set<uint32> affectedAuraCaster;

    target->loop_auras([&](AuraHolder* holder)
        {
            // permanent passive or permanent area aura
            // passive spells can be affected only by own or owner spell mods)
            if ((holder->IsPermanent() &&
                    ((owner_mode && holder->IsPassive()) ||
                        holder->IsAreaAura())) &&
                // non deleted and not same aura (any with same spell id)
                !holder->IsDisabled() && holder->GetId() != GetId() &&
                // and affected by aura
                isAffectedOnSpell(holder->GetSpellProto()))
            {
                // only applied by self or aura caster
                if (holder->GetCasterGuid() == target->GetObjectGuid())
                    affectedSelf[holder->GetId()] = holder->GetCastItemGuid();
                else if (holder->GetCasterGuid() == GetCasterGuid())
                    affectedAuraCaster.insert(holder->GetId());
            }
            return true; // continue
        });

    if (!affectedSelf.empty())
    {
        Player* pTarget =
            target->GetTypeId() == TYPEID_PLAYER ? (Player*)target : nullptr;

        for (std::map<uint32, ObjectGuid>::const_iterator map_itr =
                 affectedSelf.begin();
             map_itr != affectedSelf.end(); ++map_itr)
        {
            Item* item = pTarget && map_itr->second ?
                             pTarget->GetItemByGuid(map_itr->second) :
                             nullptr;
            target->remove_auras(map_itr->first);
            target->CastSpell(target, map_itr->first, true, item);
        }
    }

    if (!affectedAuraCaster.empty())
    {
        Unit* caster = GetCaster();
        for (const auto& elem : affectedAuraCaster)
        {
            target->remove_auras(elem);
            if (caster)
                caster->CastSpell(GetTarget(), elem, true);
        }
    }
}

struct ReapplyAffectedPassiveAurasHelper
{
    explicit ReapplyAffectedPassiveAurasHelper(Aura* _aura) : aura(_aura) {}
    void operator()(Unit* unit) const
    {
        aura->ReapplyAffectedPassiveAuras(unit, true);
    }
    Aura* aura;
};

void Aura::ReapplyAffectedPassiveAuras()
{
    // not reapply spell mods with charges (use original value because processed
    // and at remove)
    if (GetSpellProto()->procCharges)
        return;

    // not reapply some spell mods ops (mostly speedup case)
    switch (m_modifier.m_miscvalue)
    {
    case SPELLMOD_DURATION:
    case SPELLMOD_CHARGES:
    case SPELLMOD_NOT_LOSE_CASTING_TIME:
    case SPELLMOD_CASTING_TIME:
    case SPELLMOD_COOLDOWN:
    case SPELLMOD_COST:
    case SPELLMOD_ACTIVATION_TIME:
    case SPELLMOD_CASTING_TIME_OLD:
        return;
    }

    // reapply talents to own passive persistent auras
    ReapplyAffectedPassiveAuras(GetTarget(), true);

    // re-apply talents/passives/area auras applied to pet/totems (it affected
    // by player spellmods)
    GetTarget()->CallForAllControlledUnits(
        ReapplyAffectedPassiveAurasHelper(this),
        CONTROLLED_PET | CONTROLLED_TOTEMS);

    // re-apply talents/passives/area auras applied to group members (it
    // affected by player spellmods)
    if (Group* group = ((Player*)GetTarget())->GetGroup())
        for (auto member : group->members(true))
            if (member != GetTarget() && member->IsInMap(GetTarget()))
                ReapplyAffectedPassiveAuras(member, false);
}

/*********************************************************/
/***               BASIC AURA FUNCTION                 ***/
/*********************************************************/
void Aura::HandleAddModifier(bool apply, bool Real)
{
    Unit* target = GetTarget();
    if (target->GetTypeId() != TYPEID_PLAYER || !Real)
        return;

    if (m_modifier.m_miscvalue >= MAX_SPELLMOD)
        return;

    if (apply)
    {
        SpellEntry const* spellProto = GetSpellProto();

        // Add custom charges for some mod aura
        switch (spellProto->Id)
        {
        case 17941: // Shadow Trance
        case 22008: // Netherwind Focus
        case 34936: // Backlash
            GetHolder()->SetAuraCharges(1);
            break;
        }

        m_spellmod = new SpellModifier(SpellModOp(m_modifier.m_miscvalue),
            SpellModType(m_modifier.m_auraname), // SpellModType value == spell
                                                 // aura types
            m_modifier.m_amount, this,
            // prevent expire spell mods with (charges > 0 && m_stackAmount > 1)
            // all this spell expected expire not at use but at spell proc event
            // check
            spellProto->StackAmount > 1 ? 0 : GetHolder()->GetAuraCharges());
    }

    static_cast<Player*>(target)->AddSpellMod(&m_spellmod, apply);

    ReapplyAffectedPassiveAuras();
}

void Aura::TriggerSpell()
{
    ObjectGuid casterGUID = GetCasterGuid();
    Unit* triggerTarget = GetTriggerTarget();

    if (!casterGUID || !triggerTarget)
        return;

    // generic casting code with custom spells and target/caster customs
    uint32 trigger_spell_id = GetSpellProto()->EffectTriggerSpell[m_effIndex];

    SpellEntry const* triggeredSpellInfo =
        sSpellStore.LookupEntry(trigger_spell_id);
    SpellEntry const* auraSpellInfo = GetSpellProto();
    uint32 auraId = auraSpellInfo->Id;
    Unit* target = GetTarget();
    Unit* triggerCaster = triggerTarget;
    WorldObject* triggerTargetObject = nullptr;

    // specific code for cases with no trigger spell provided in field
    if (triggeredSpellInfo == nullptr)
    {
        switch (auraSpellInfo->SpellFamilyName)
        {
        case SPELLFAMILY_GENERIC:
        {
            switch (auraId)
            {
            // Firestone Passive (1-5 ranks)
            case 758:
            case 17945:
            case 17947:
            case 17949:
            case 27252:
            {
                if (triggerTarget->GetTypeId() != TYPEID_PLAYER)
                    return;
                Item* item =
                    ((Player*)triggerTarget)->GetWeaponForAttack(BASE_ATTACK);
                if (!item)
                    return;
                uint32 enchant_id = 0;
                switch (GetId())
                {
                case 758:
                    enchant_id = 1803;
                    break; // Rank 1
                case 17945:
                    enchant_id = 1823;
                    break; // Rank 2
                case 17947:
                    enchant_id = 1824;
                    break; // Rank 3
                case 17949:
                    enchant_id = 1825;
                    break; // Rank 4
                case 27252:
                    enchant_id = 2645;
                    break; // Rank 5
                default:
                    return;
                }
                // remove old enchanting before applying new
                ((Player*)triggerTarget)
                    ->ApplyEnchantment(
                        item, TEMP_ENCHANTMENT_SLOT, false, item->slot());
                item->SetEnchantment(TEMP_ENCHANTMENT_SLOT, enchant_id,
                    m_modifier.periodictime + 1000, 0);
                // add new enchanting
                ((Player*)triggerTarget)
                    ->ApplyEnchantment(
                        item, TEMP_ENCHANTMENT_SLOT, true, item->slot());
                return;
            }
            case 812: // Periodic Mana Burn
            {
                trigger_spell_id = 25779; // Mana Burn

                if (GetTarget()->GetTypeId() != TYPEID_UNIT)
                    return;

                triggerTarget =
                    ((Creature*)GetTarget())
                        ->SelectAttackingTarget(ATTACKING_TARGET_TOPAGGRO, 0,
                            trigger_spell_id, SELECT_FLAG_POWER_MANA);
                if (!triggerTarget)
                    return;

                break;
            }
            //                    // Polymorphic Ray
            //                    case 6965: break;
            //                    // Fire Nova (1-7 ranks)
            //                    case 8350:
            //                    case 8508:
            //                    case 8509:
            //                    case 11312:
            //                    case 11313:
            //                    case 25540:
            //                    case 25544:
            //                        break;
            case 9712: // Thaumaturgy Channel
                trigger_spell_id = 21029;
                break;
            //                    // Egan's Blaster
            //                    case 17368: break;
            //                    // Haunted
            //                    case 18347: break;
            //                    // Ranshalla Waiting
            //                    case 18953: break;
            case 19695: // Inferno (Baron Geddon)
            {
                int32 bp0 = 500 * GetAuraTicks();
                target->CastCustomSpell(target, 19698, &bp0, nullptr, nullptr,
                    TRIGGER_TYPE_TRIGGERED | TRIGGER_TYPE_BYPASS_SPELL_QUEUE);
                return;
            }
            //                    // Frostwolf Muzzle DND
            //                    case 21794: break;
            //                    // Alterac Ram Collar DND
            //                    case 21866: break;
            //                    // Celebras Waiting
            //                    case 21916: break;
            case 23170: // Brood Affliction: Bronze
            {
                if (roll_chance_i(20))
                    target->CastSpell(target, 23171, true, nullptr, this);
                return;
            }
            case 23173: // Brood Affliction
            {
                static const int AFFLICTIONS_NUM = 5;
                static const uint32 spells[AFFLICTIONS_NUM] = {
                    23153, 23154, 23155, 23169, 23170};

                auto caster = GetCaster();
                if (!caster)
                    return;

                for (int i = 0; i < AFFLICTIONS_NUM; ++i)
                {
                    if (!target->has_aura(spells[i]))
                        return;
                }

                for (int i = 0; i < AFFLICTIONS_NUM; ++i)
                    target->remove_auras(spells[i]);

                caster->CastSpell(target, 23174, true);
                return;
            }
            case 23184: // Mark of Frost
            case 25041: // Mark of Nature
            case 37125: // Mark of Death
            {
                // spells existed in 1.x.x; 23183 - mark of frost; 25042 - mark
                // of nature; both had radius of 100.0 yards in 1.x.x DBC
                // spells are used by Azuregos and the Emerald dragons in order
                // to put a stun debuff on the players which resurrect during
                // the encounter
                // in order to implement the missing spells we need to make a
                // grid search for hostile players and check their auras; if
                // they are marked apply debuff
                // spell 37127 used for the Mark of Death, is used server side,
                // so it needs to be implemented here

                uint32 markSpellId = 0;
                uint32 debuffSpellId = 0;

                switch (auraId)
                {
                case 23184:
                    markSpellId = 23182;
                    debuffSpellId = 23186;
                    break;
                case 25041:
                    markSpellId = 25040;
                    debuffSpellId = 25043;
                    break;
                case 37125:
                    markSpellId = 37128;
                    debuffSpellId = 37131;
                    break;
                }

                auto target = GetTarget();
                auto targets = maps::visitors::yield_set<Player>{}(target,
                    target->GetMap()->GetVisibilityDistance(),
                    [markSpellId](Player* p)
                    {
                        return p->isAlive() && p->has_aura(markSpellId);
                    });

                for (auto& target : targets)
                    (target)->CastSpell((target), debuffSpellId, true, nullptr,
                        nullptr, casterGUID);

                return;
            }
            case 24379: // Restoration
            {
                uint32 heal = triggerTarget->GetMaxHealth() / 10;
                triggerTarget->DealHeal(triggerTarget, heal, auraSpellInfo);

                if (int32 mana = triggerTarget->GetMaxPower(POWER_MANA))
                {
                    mana /= 10;
                    triggerTarget->EnergizeBySpell(
                        triggerTarget, GetId(), mana, POWER_MANA);
                }

                Pet* pet = triggerTarget->GetPet();
                if (pet && pet->getPetType() == HUNTER_PET)
                {
                    if (uint32 happy = pet->GetMaxPower(POWER_HAPPINESS))
                    {
                        happy /= 10;
                        triggerTarget->EnergizeBySpell(
                            pet, GetId(), happy, POWER_HAPPINESS);
                    }
                }
                return;
            }
            //                    // Stoneclaw Totem Passive TEST
            //                    case 23792: break;
            case 24210: // Mark of Arlokk
            {
                // Replacement for (classic) spell 24211 (doesn't exist anymore)
                // Search for all Zulian Prowler in range

                auto list = maps::visitors::yield_set<Creature>{}(
                    triggerTarget, 15.0f, maps::checks::entry_guid{15101, 0});

                for (auto& elem : list)
                    if (elem->isAlive())
                        elem->AddThreat(triggerTarget, float(5000));

                return;
            }
            //                    // Restoration
            //                    case 24379: break;
            //                    // Happy Pet
            //                    case 24716: break;
            case 24780: // Dream Fog
            {
                // Note: In 1.12 triggered spell 24781 still exists, need to
                // script dummy effect for this spell then
                // Select an unfriendly enemy in 100y range and attack it
                if (target->GetTypeId() != TYPEID_UNIT)
                    return;

                ThreatList const& tList =
                    target->getThreatManager().getThreatList();
                for (const auto& elem : tList)
                {
                    Unit* pUnit =
                        target->GetMap()->GetUnit((elem)->getUnitGuid());

                    if (pUnit && target->getThreatManager().getThreat(pUnit))
                        target->getThreatManager().modifyThreatPercent(
                            pUnit, -100);
                }

                if (Unit* pEnemy = target->SelectRandomUnfriendlyTarget(
                        target->getVictim(), 100.0f))
                    ((Creature*)target)->AI()->AttackStart(pEnemy);

                return;
            }
            //                    // Cannon Prep
            //                    case 24832: break;
            case 24834: // Shadow Bolt Whirl
            {
                uint32 spellForTick[8] = {
                    24820, 24821, 24822, 24823, 24835, 24836, 24837, 24838};
                uint32 tick = GetAuraTicks();
                if (tick < 8)
                {
                    trigger_spell_id = spellForTick[tick];

                    // casted in left/right (but triggered spell have wide
                    // forward cone)
                    float forward = target->GetO();
                    float angle = target->GetO() +
                                  (tick % 2 == 0 ? M_PI_F / 2 : -M_PI_F / 2);
                    target->SetOrientation(angle);
                    triggerTarget->CastSpell(triggerTarget, trigger_spell_id,
                        true, nullptr, this, casterGUID);
                    target->SetOrientation(forward);
                }
                return;
            }
            //                    // Stink Trap
            //                    case 24918: break;
            //                    // Agro Drones
            //                    case 25152: break;
            case 25371: // Consume
            {
                int32 bpDamage = triggerTarget->GetMaxHealth() * 10 / 100;
                triggerTarget->CastCustomSpell(triggerTarget, 25373, &bpDamage,
                    nullptr, nullptr, true, nullptr, this, casterGUID);
                return;
            }
            //                    // Pain Spike
            //                    case 25572: break;
            //                    // Consume
            //                    case 26196: break;
            //                    // Berserk
            //                    case 26615: break;
            //                    // Defile
            //                    case 27177: break;
            //                    // Teleport: IF/UC
            //                    case 27601: break;
            //                    // Five Fat Finger Exploding Heart Technique
            //                    case 27673: break;
            //                    // Nitrous Boost
            //                    case 27746: break;
            //                    // Steam Tank Passive
            //                    case 27747: break;
            case 26009: // Rotate 360
            {
                // One PI rotation takes 35 seconds; 1 trigger per sec
                float o = triggerTarget->GetO() + M_PI_F / 35;
                triggerTarget->SetFacingTo(o);
                triggerTarget->SetOrientation(o);
                triggerTarget->CastSpell(triggerTarget, 26029, true);
                return;
            }
            case 26136: // Rotate -360
            {
                // One PI rotation takes 35 seconds; 1 trigger per sec
                float o = triggerTarget->GetO() - M_PI_F / 35;
                triggerTarget->SetFacingTo(o);
                triggerTarget->SetOrientation(o);
                triggerTarget->CastSpell(triggerTarget, 26029, true);
                return;
            }
            case 27808: // Frost Blast
            {
                int32 bpDamage = triggerTarget->GetMaxHealth() * 26 / 100;
                triggerTarget->CastCustomSpell(triggerTarget, 29879, &bpDamage,
                    nullptr, nullptr, true, nullptr, this, casterGUID);
                return;
            }
            // Detonate Mana
            case 27819:
            {
                // 33% Mana Burn on normal mode, 50% on heroic mode
                int32 bpDamage =
                    (int32)triggerTarget->GetPower(POWER_MANA) /
                    (triggerTarget->GetMap()->GetDifficulty() ? 2 : 3);
                triggerTarget->ModifyPower(POWER_MANA, -bpDamage);
                triggerTarget->CastCustomSpell(triggerTarget, 27820, &bpDamage,
                    nullptr, nullptr, true, nullptr, this,
                    triggerTarget->GetObjectGuid());
                return;
            }
            //                    // Controller Timer
            //                    case 28095: break;
            // Stalagg Chain and Feugen Chain
            case 28096:
            case 28111:
            {
                // X-Chain is casted by Tesla to X, so: caster == Tesla, target
                // = X
                Unit* pCaster = GetCaster();
                if (pCaster && pCaster->GetTypeId() == TYPEID_UNIT &&
                    !pCaster->IsWithinDistInMap(target, 60.0f))
                {
                    pCaster->InterruptNonMeleeSpells(true);
                    ((Creature*)pCaster)->SetInCombatWithZone();
                    // Stalagg Tesla Passive or Feugen Tesla Passive
                    pCaster->CastSpell(pCaster, auraId == 28096 ? 28097 : 28109,
                        true, nullptr, nullptr, target->GetObjectGuid());
                }
                return;
            }
            // Stalagg Tesla Passive and Feugen Tesla Passive
            case 28097:
            case 28109:
            {
                // X-Tesla-Passive is casted by Tesla on Tesla with original
                // caster X, so: caster = X, target = Tesla
                Unit* pCaster = GetCaster();
                if (pCaster && pCaster->GetTypeId() == TYPEID_UNIT)
                {
                    if (pCaster->getVictim() &&
                        !pCaster->IsWithinDistInMap(target, 60.0f))
                    {
                        if (Unit* pTarget = ((Creature*)pCaster)
                                                ->SelectAttackingTarget(
                                                    ATTACKING_TARGET_RANDOM, 0))
                            target->CastSpell(pTarget, 28099, false); // Shock
                    }
                    else
                    {
                        // "Evade"
                        target->remove_auras(auraId);
                        target->DeleteThreatList();
                        target->CombatStop(true);
                        // Recast chain (Stalagg Chain or Feugen Chain
                        target->CastSpell(
                            pCaster, auraId == 28097 ? 28096 : 28111, false);
                    }
                }
                return;
            }
            //                    // Mark of Didier
            //                    case 28114: break;
            //                    // Communique Timer, camp
            //                    case 28346: break;
            //                    // Icebolt
            //                    case 28522: break;
            //                    // Silithyst
            //                    case 29519: break;
            case 29528: // Inoculate Nestlewood Owlkin
                // prevent error reports in case ignored player target
                if (triggerTarget->GetTypeId() != TYPEID_UNIT)
                    return;
                break;
            //                    // Overload
            //                    case 29768: break;
            //                    // Return Fire
            //                    case 29788: break;
            //                    // Return Fire
            //                    case 29793: break;
            //                    // Return Fire
            //                    case 29794: break;
            //                    // Guardian of Icecrown Passive
            //                    case 29897: break;
            case 29917: // Feed Captured Animal
                trigger_spell_id = 29916;
                break;
            //                    // Flame Wreath
            //                    case 29946: break;
            //                    // Flame Wreath
            //                    case 29947: break;
            //                    // Mind Exhaustion Passive
            //                    case 30025: break;
            //                    // Nether Beam - Serenity
            //                    case 30401: break;
            case 30427: // Extract Gas
            {
                Unit* caster = GetCaster();
                if (!caster)
                    return;
                // move loot to player inventory and despawn target
                if (caster->GetTypeId() == TYPEID_PLAYER &&
                    triggerTarget->GetTypeId() == TYPEID_UNIT &&
                    static_cast<Creature*>(triggerTarget)->GetCreatureInfo() &&
                    static_cast<Creature*>(triggerTarget)
                            ->GetCreatureInfo()
                            ->type == CREATURE_TYPE_GAS_CLOUD)
                {
                    Player* player = (Player*)caster;
                    Creature* creature = (Creature*)triggerTarget;
                    // missing lootid has been reported on startup - just return
                    if (!creature->GetCreatureInfo()->SkinLootId)
                        return;

                    {
                        loot_distributor ld(creature, LOOT_SKINNING);
                        ld.recipient_mgr()->add_solo_tap(player);
                        ld.generate_loot(player);
                        ld.auto_store_all_loot(player);
                    } // Destroy loot_distributor

                    creature->ForcedDespawn();
                }
                return;
            }
            case 30576: // Quake
                trigger_spell_id = 30571;
                break;
            //                    // Burning Maul
            //                    case 30598: break;
            //                    // Regeneration
            //                    case 30799:
            //                    case 30800:
            //                    case 30801:
            //                        break;
            //                    // Despawn Self - Smoke cloud
            //                    case 31269: break;
            //                    // Time Rift Periodic
            //                    case 31320: break;
            //                    // Corrupt Medivh
            //                    case 31326: break;
            case 31347: // Doom
            {
                target->CastSpell(target, 31350, true);
                target->DealDamage(target, target->GetHealth(), nullptr,
                    DIRECT_DAMAGE, SPELL_SCHOOL_MASK_NORMAL, nullptr, false,
                    false);
                return;
            }
            case 31373: // Spellcloth
            {
                // Summon Elemental after create item
                triggerTarget->SummonCreature(17870, 0.0f, 0.0f, 0.0f,
                    triggerTarget->GetO(), TEMPSUMMON_DEAD_DESPAWN, 0);
                return;
            }
            //                    // Bloodmyst Tesla
            //                    case 31611: break;
            case 31944: // Doomfire
            {
                int32 damage = m_modifier.m_amount *
                               ((GetAuraDuration() + m_modifier.periodictime) /
                                   GetAuraMaxDuration());
                triggerTarget->CastCustomSpell(triggerTarget, 31969, &damage,
                    nullptr, nullptr, true, nullptr, this, casterGUID);
                return;
            }
            //                    // Teleport Test
            //                    case 32236: break;
            //                    // Earthquake
            //                    case 32686: break;
            //                    // Possess
            //                    case 33401: break;
            //                    // Draw Shadows
            //                    case 33563: break;
            case 34229: // Flame Quills
            {
                // cast 24 spells 34269-34289, 34314-34316
                for (uint32 spell_id = 34269; spell_id != 34290; ++spell_id)
                    triggerTarget->CastSpell(triggerTarget, spell_id, true,
                        nullptr, this, casterGUID);
                for (uint32 spell_id = 34314; spell_id != 34317; ++spell_id)
                    triggerTarget->CastSpell(triggerTarget, spell_id, true,
                        nullptr, this, casterGUID);
                return;
            }
            case 34480: // Gravity Lapse
            {
                if (triggerTarget->GetZ() < 50.0f &&
                    triggerTarget->GetTypeId() == TYPEID_PLAYER)
                {
                    uint32 bp =
                        GetSpellProto()->EffectBasePoints[EFFECT_INDEX_0] +
                        1.0f;
                    uint32 bds =
                        GetSpellProto()->EffectDieSides[EFFECT_INDEX_0] - 1.0f;
                    float dmg = frand(bp, bp + bds);
                    ((Player*)triggerTarget)
                        ->KnockBack(0.0f, 0.0f, dmg / 10.0f);
                    // Reapply gravity lapse fly aura
                    triggerTarget->AddAuraThroughNewHolder(
                        39432, triggerTarget, GetAuraDuration());
                }
                return;
            }
            //                    // Tornado
            //                    case 34683: break;
            //                    // Frostbite Rotate
            //                    case 34748: break;
            //                    // Arcane Flurry
            //                    case 34821: break;
            //                    // Interrupt Shutdown
            //                    case 35016: break;
            //                    // Interrupt Shutdown
            //                    case 35176: break;
            //                    // Inferno
            //                    case 35268: break;
            //                    // Salaadin's Tesla
            //                    case 35515: break;
            //                    // Ethereal Channel (Red)
            //                    case 35518: break;
            //                    // Nether Vapor
            //                    case 35879: break;
            //                    // Dark Portal Storm
            //                    case 36018: break;
            //                    // Burning Maul
            //                    case 36056: break;
            //                    // Living Grove Defender Lifespan
            //                    case 36061: break;
            //                    // Professor Dabiri Talks
            //                    case 36064: break;
            //                    // Kael Gaining Power
            //                    case 36091: break;
            //                    // They Must Burn Bomb Aura
            //                    case 36344: break;
            //                    // They Must Burn Bomb Aura (self)
            //                    case 36350: break;
            //                    // Stolen Ravenous Ravager Egg
            //                    case 36401: break;
            //                    // Activated Cannon
            //                    case 36410: break;
            //                    // Stolen Ravenous Ravager Egg
            //                    case 36418: break;
            //                    // Enchanted Weapons
            //                    case 36510: break;
            //                    // Cursed Scarab Periodic
            //                    case 36556: break;
            //                    // Cursed Scarab Despawn Periodic
            //                    case 36561: break;
            //                    // Vision Guide
            //                    case 36573: break;
            //                    // Cannon Charging (platform)
            //                    case 36785: break;
            //                    // Cannon Charging (self)
            //                    case 36860: break;
            case 37027:                  // Remote Toy
                if (urand(0, 100) >= 70) // ~30% chance to fail
                    return;
                trigger_spell_id = 37029;
                break;
            case 37098: // Rain of Bones
            {
                auto caster = GetCaster();
                int count = 1;
                if (!caster)
                    return;
                // 2/3 chance to get an extra spell cast
                if (rand_norm_f() < 2.0f / 3.0f)
                    count = 2;
                for (int i = 0; i < count; ++i)
                    caster->CastSpell(triggerTarget, 37091, true);
                return;
            }
            //                    // Mark of Death
            //                    case 37125: break;
            //                    // Arcane Flurry
            //                    case 37268: break;
            //                    // Spout
            //                    case 37429: break;
            //                    // Spout
            //                    case 37430: break;
            //                    // Karazhan - Chess NPC AI, Snapshot timer
            //                    case 37440: break;
            //                    // Karazhan - Chess NPC AI, action timer
            //                    case 37504: break;
            //                    // Karazhan - Chess: Is Square OCCUPIED aura
            //                    (DND)
            //                    case 39400: break;
            //                    // Banish
            //                    case 37546: break;
            //                    // Shriveling Gaze
            //                    case 37589: break;
            //                    // Fake Aggro Radius (2 yd)
            //                    case 37815: break;
            //                    // Corrupt Medivh
            //                    case 37853: break;
            case 38495: // Eye of Grillok
            {
                // Aggro Bleeding Hollow
                target->CastSpell(target, 38508, true);
                // Quest Credit for Eye of Grillok
                target->CastSpell(target, 38530, true);
                return;
            }
            case 38554: // Absorb Eye of Grillok (Zezzak's Shard)
            {
                if (target->GetTypeId() != TYPEID_UNIT)
                    return;

                if (Unit* caster = GetCaster())
                    caster->CastSpell(caster, 38495, true, nullptr, this);
                else
                    return;

                Creature* creatureTarget = (Creature*)target;

                creatureTarget->ForcedDespawn();
                return;
            }
            //                    // Magic Sucker Device timer
            //                    case 38672: break;
            //                    // Tomb Guarding Charging
            //                    case 38751: break;
            case 39105: // Activate Nether-wraith Beacon (31742 Nether-wraith
                        // Beacon item)
                {
                    auto pos = triggerTarget->GetPoint(0.0f, 20.0f);
                    triggerTarget->SummonCreature(22408, pos.x, pos.y, pos.z,
                        triggerTarget->GetO(), TEMPSUMMON_DEAD_DESPAWN, 0);
                    return;
                }
            //                    // Drain World Tree Visual
            //                    case 39140: break;
            //                    // Quest - Dustin's Undead Dragon Visual aura
            //                    case 39259: break;
            //                    // Hellfire - The Exorcism, Jules releases
            //                    darkness, aura
            //                    case 39306: break;
            //                    // Inferno
            //                    case 39346: break;
            //                    // Enchanted Weapons
            //                    case 39489: break;
            //                    // Shadow Bolt Whirl
            //                    case 39630: break;
            //                    // Shadow Bolt Whirl
            //                    case 39634: break;
            //                    // Shadow Inferno
            //                    case 39645: break;
            case 39857: // Tear of Azzinoth Summon Channel - it's not really
                        // supposed to do anything,and this only prevents the
                        // console spam
                trigger_spell_id = 39856;
                break;
            //                    // Soulgrinder Ritual Visual (Smashed)
            //                    case 39974: break;
            //                    // Simon Game Pre-game timer
            //                    case 40041: break;
            //                    // Knockdown Fel Cannon: The Aggro Check Aura
            //                    case 40113: break;
            //                    // Spirit Lance
            //                    case 40157: break;
            //                    // Demon Transform 2
            //                    case 40398: break;
            //                    // Demon Transform 1
            //                    case 40511: break;
            //                    // Ancient Flames
            //                    case 40657: break;
            //                    // Ethereal Ring Cannon: Cannon Aura
            //                    case 40734: break;
            //                    // Cage Trap
            //                    case 40760: break;
            //                    // Random Periodic
            //                    case 40867: break;
            //                    // Prismatic Shield
            //                    case 40879: break;
            //                    // Aura of Desire
            //                    case 41350: break;
            //                    // Dementia
            //                    case 41404: break;
            //                    // Chaos Form
            //                    case 41629: break;
            //                    // Alert Drums
            //                    case 42177: break;
            //                    // Spout
            //                    case 42581: break;
            //                    // Spout
            //                    case 42582: break;
            //                    // Return to the Spirit Realm
            //                    case 44035: break;
            //                    // Curse of Boundless Agony
            //                    case 45050: break;
            //                    // Earthquake
            //                    case 46240: break;
            case 46736: // Personalized Weather
                trigger_spell_id = 46737;
                break;
            //                    // Stay Submerged
            //                    case 46981: break;
            //                    // Dragonblight Ram
            //                    case 47015: break;
            //                    // Party G.R.E.N.A.D.E.
            //                    case 51510: break;
            default:
                break;
            }
            break;
        }
        case SPELLFAMILY_MAGE:
        {
            switch (auraId)
            {
            case 66: // Invisibility
                if (Unit* caster = GetCaster())
                    caster->getHostileRefManager().addThreatPercent(-10);
                return;
            default:
                break;
            }
            break;
        }
        //            case SPELLFAMILY_PRIEST:
        //            {
        //                switch(auraId)
        //                {
        //                    // Blue Beam
        //                    case 32930: break;
        //                    // Fury of the Dreghood Elders
        //                    case 35460: break;
        //                    default:
        //                        break;
        //                }
        //                break;
        //            }
        case SPELLFAMILY_DRUID:
        {
            switch (auraId)
            {
            case 768: // Cat Form
                // trigger_spell_id not set and unknown effect triggered in this
                // case, ignoring for while
                return;
            case 22842: // Frenzied Regeneration
            case 22895:
            case 22896:
            case 26999:
            {
                int32 LifePerRage = GetModifier()->m_amount;

                int32 lRage = target->GetPower(POWER_RAGE);
                if (lRage > 100) // rage stored as rage*10
                    lRage = 100;
                target->ModifyPower(POWER_RAGE, -lRage);
                int32 FRTriggerBasePoints = int32(lRage * LifePerRage / 10);
                target->CastCustomSpell(target, 22845, &FRTriggerBasePoints,
                    nullptr, nullptr, true, nullptr, this);
                return;
            }
            default:
                break;
            }
            break;
        }
        case SPELLFAMILY_HUNTER:
        {
            switch (auraId)
            {
            // Frost Trap Aura
            case 13810:
            {
                if (Unit* caster = GetCaster())
                {
                    if (caster->GetTypeId() == TYPEID_PLAYER)
                    {
                        float procChance = 0;
                        auto& auraTriggerSpell = caster->GetAurasByType(
                            SPELL_AURA_PROC_TRIGGER_SPELL);
                        for (const auto& elem : auraTriggerSpell)
                        {
                            switch ((elem)->GetSpellProto()->Id)
                            {
                            case 19384:
                            case 19387:
                            case 19388:
                                procChance =
                                    (elem)->GetSpellProto()->procChance;
                                break;
                            }
                        }

                        if (roll_chance_f(procChance))
                        {
                            trigger_spell_id = 19185;
                        }
                        else
                            return;
                    }
                }
                break;
            }
            default:
                break;
            }
            break;
        }
        case SPELLFAMILY_SHAMAN:
        {
            switch (auraId)
            {
            case 28820: // Lightning Shield (The Earthshatterer set trigger
                        // after cast Lighting Shield)
                {
                    // Need remove self if Lightning Shield not active
                    bool remove = true;
                    triggerTarget->loop_auras([&remove](AuraHolder* holder)
                        {
                            const SpellEntry* proto = holder->GetSpellProto();
                            if (proto->SpellFamilyName == SPELLFAMILY_SHAMAN &&
                                proto->SpellFamilyFlags & 0x400)
                                remove = false;
                            return remove; // break when remove is false
                        });
                    if (remove)
                        triggerTarget->remove_auras(28820);
                    return;
                }
            case 38443: // Totemic Mastery (Skyshatter Regalia (Shaman Tier 6) -
                        // bonus)
                {
                    if (triggerTarget->IsAllTotemSlotsUsed())
                        triggerTarget->CastSpell(
                            triggerTarget, 38437, true, nullptr, this);
                    else
                        triggerTarget->remove_auras(38437);
                    return;
                }
            default:
                break;
            }
            break;
        }
        default:
            break;
        }

        // Reget trigger spell proto
        triggeredSpellInfo = sSpellStore.LookupEntry(trigger_spell_id);
    }
    else // initial triggeredSpellInfo != NULL
    {
        // for channeled spell cast applied from aura owner to channel target
        // (persistent aura affects already applied to true target)
        // come periodic casts applied to targets, so need seelct proper caster
        // (ex. 15790)
        if (IsChanneledSpell(GetSpellProto()) &&
            GetSpellProto()->Effect[GetEffIndex()] !=
                SPELL_EFFECT_PERSISTENT_AREA_AURA)
        {
            // the triggered spell is in front of caster, in which case it
            // should not be casted on the channeled on target
            if (!triggeredSpellInfo->HasTargetType(TARGET_CHAIN_DAMAGE) &&
                !triggeredSpellInfo->HasTargetType(TARGET_DUELVSPLAYER) &&
                (triggeredSpellInfo->HasTargetType(TARGET_LARGE_FRONTAL_CONE) ||
                    triggeredSpellInfo->HasTargetType(
                        TARGET_NARROW_FRONTAL_CONE) ||
                    triggeredSpellInfo->HasTargetType(
                        TARGET_IN_FRONT_OF_CASTER)))
            {
                triggerTarget = triggerCaster;
            }
            // periodic aura at caster of channeled spell
            else if (target->GetObjectGuid() == casterGUID)
            {
                triggerCaster = target;

                if (WorldObject* channelTarget =
                        target->GetMap()->GetWorldObject(
                            target->GetChannelObjectGuid()))
                {
                    if (channelTarget->isType(TYPEMASK_UNIT))
                        triggerTarget = (Unit*)channelTarget;
                    else
                        triggerTargetObject = channelTarget;
                }
            }
            // or periodic aura at caster channel target
            else if (Unit* caster = GetCaster())
            {
                if (target->GetObjectGuid() == caster->GetChannelObjectGuid())
                {
                    triggerCaster = caster;
                    triggerTarget = target;
                }
            }
        }

        // Spell exist but require custom code
        switch (auraId)
        {
        case 9347: // Mortal Strike
        {
            if (target->GetTypeId() != TYPEID_UNIT)
                return;
            // expected selection current fight target
            triggerTarget =
                ((Creature*)target)
                    ->SelectAttackingTarget(
                        ATTACKING_TARGET_TOPAGGRO, 0, triggeredSpellInfo);
            if (!triggerTarget)
                return;

            break;
        }
        case 1010: // Curse of Idiocy
        {
            // TODO: spell casted by result in correct way mostly
            // BUT:
            // 1) target show casting at each triggered cast: target don't must
            // show casting animation for any triggered spell
            //      but must show affect apply like item casting
            // 2) maybe aura must be replace by new with accumulative stat mods
            // instead stacking

            // prevent cast by triggered auras
            if (casterGUID == triggerTarget->GetObjectGuid())
                return;

            // stop triggering after each affected stats lost > 90
            int32 intelectLoss = 0;
            int32 spiritLoss = 0;

            auto& mModStat = triggerTarget->GetAurasByType(SPELL_AURA_MOD_STAT);
            for (const auto& elem : mModStat)
            {
                if ((elem)->GetId() == 1010)
                {
                    switch ((elem)->GetModifier()->m_miscvalue)
                    {
                    case STAT_INTELLECT:
                        intelectLoss += (elem)->GetModifier()->m_amount;
                        break;
                    case STAT_SPIRIT:
                        spiritLoss += (elem)->GetModifier()->m_amount;
                        break;
                    default:
                        break;
                    }
                }
            }

            if (intelectLoss <= -90 && spiritLoss <= -90)
                return;

            break;
        }
        case 16191: // Mana Tide
        {
            triggerTarget->CastCustomSpell(triggerTarget, trigger_spell_id,
                &m_modifier.m_amount, nullptr, nullptr, true, nullptr, this);
            return;
        }
        case 19636: // Fire Blossom
        case 21737: // Periodic Knock Away
        {
            // TODO: Maybe there exists some general rule for this. The spell is
            // an aura that's put on self, and every tick it shoots a fire ball
            // at target's victim.
            if (auto victim = target->getVictim())
                triggerTarget = victim;
            else
                return;
            break;
        }
        case 33525: // Ground Slam
            triggerTarget->CastSpell(triggerTarget, trigger_spell_id, true,
                nullptr, this, casterGUID);
            return;
        case 37716: // Demon Link
            if (auto victim = target->getVictim())
                triggerTarget = victim;
            break;
        case 38736: // Rod of Purification - for quest 10839 (Veil Skith:
                    // Darkstone of Terokk)
            {
                if (Unit* caster = GetCaster())
                    caster->CastSpell(
                        triggerTarget, trigger_spell_id, true, nullptr, this);
                return;
            }
        case 44883: // Encapsulate
        {
            // Self cast spell, hence overwrite caster (only channeled spell
            // where the triggered spell deals dmg to SELF)
            triggerCaster = triggerTarget;
            break;
        }
        }
    }

    // All ok cast by default case
    if (triggeredSpellInfo)
    {
        if (triggerTargetObject)
            triggerCaster->CastSpell(triggerTargetObject->GetX(),
                triggerTargetObject->GetY(), triggerTargetObject->GetZ(),
                triggeredSpellInfo, true, nullptr, this, casterGUID);
        else
            triggerCaster->CastSpell(triggerTarget, triggeredSpellInfo, true,
                nullptr, this, casterGUID);
    }
    else
    {
        if (Unit* caster = GetCaster())
        {
            if (triggerTarget->GetTypeId() != TYPEID_UNIT ||
                !sScriptMgr::Instance()->OnEffectDummy(
                    caster, GetId(), GetEffIndex(), (Creature*)triggerTarget))
                logging.error(
                    "Aura::TriggerSpell: Spell %u have 0 in "
                    "EffectTriggered[%d], not handled custom case?",
                    GetId(), GetEffIndex());
        }
    }
}

void Aura::TriggerSpellWithValue()
{
    ObjectGuid casterGuid = GetCasterGuid();
    Unit* target = GetTriggerTarget();

    if (!casterGuid || !target)
        return;

    // generic casting code with custom spells and target/caster customs
    uint32 trigger_spell_id = GetSpellProto()->EffectTriggerSpell[m_effIndex];
    int32 basepoints0 = GetModifier()->m_amount;

    target->CastCustomSpell(target, trigger_spell_id, &basepoints0, nullptr,
        nullptr, true, nullptr, this, casterGuid);
}

/*********************************************************/
/***                  AURA EFFECTS                     ***/
/*********************************************************/

void Aura::HandleAuraDummy(bool apply, bool Real)
{
    // spells required only Real aura add/remove
    if (!Real)
        return;

    Unit* target = GetTarget();

    // AT APPLY
    if (apply)
    {
        switch (GetSpellProto()->SpellFamilyName)
        {
        case SPELLFAMILY_GENERIC:
        {
            switch (GetId())
            {
            case 126: // Eye of Kilrogg
                if (target->GetTypeId() == TYPEID_PLAYER)
                {
                    // Cancel enslave demon
                    if (auto charm = target->GetCharm())
                        charm->remove_auras(SPELL_AURA_MOD_CHARM);
                    static_cast<Player*>(target)->UnsummonPetTemporaryIfAny();
                }
                return;
            case 1515: // Tame beast
                // FIX_ME: this is 2.0.12 threat effect replaced in 2.1.x by
                // dummy aura, must be checked for correctness
                if (target->CanHaveThreatList())
                    if (Unit* caster = GetCaster())
                        target->AddThreat(caster, 10.0f, false,
                            GetSpellSchoolMask(GetSpellProto()),
                            GetSpellProto());
                return;
            case 7057: // Haunting Spirits
                // expected to tick with 30 sec period (tick part see in
                // Aura::PeriodicTick)
                m_isPeriodic = true;
                m_modifier.periodictime = 30 * IN_MILLISECONDS;
                m_periodicTimer = m_modifier.periodictime;
                return;
            case 13278: // Gnomish Death Ray
            {
                // If you change this formula also ee SpellEffects.cpp,
                // EffectDummy, case 13278
                int32 hp = 150 + target->GetHealth() / 16;
                // seems to be capped out, to not do insane damage in TBC
                if (hp > 400)
                    hp = 400;
                target->CastCustomSpell(
                    target, 13493, &hp, nullptr, nullptr, true);
                return;
            }
            case 10255: // Stoned
            {
                if (Unit* caster = GetCaster())
                {
                    if (caster->GetTypeId() != TYPEID_UNIT)
                        return;

                    caster->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
                    caster->addUnitState(UNIT_STAT_ROOT);
                }
                return;
            }
            case 13183: // Goblin Dragon Gun
            {
                // 10% chance to backfire
                if (urand(0, 10) < 1)
                {
                    target->remove_auras(13183);
                    target->CastSpell(target, 13466, true);
                }
                return;
            }
            case 29384: // Light Beacon
            {
                // Alert nearby Stoneschyte Whelp when aura is applied
                if (Unit* caster = GetCaster())
                {
                    auto cl = maps::visitors::yield_set<Creature>{}(
                        caster, 40.0f, maps::checks::entry_guid{16927, 0});
                    for (auto& elem : cl)
                    {
                        auto pos = caster->GetPoint(elem, 2.5f);
                        elem->movement_gens.push(
                            new movement::PointMovementGenerator(
                                0, pos.x, pos.y, pos.z, true, true));
                    }
                }
                return;
            }
            case 31427: // Allergies
            {
                // Proces sneeze between 1 second from now and 2:59 minutes
                target->queue_action(urand(1000, 3 * 60 * 1000 - 1000),
                    [target]()
                    {
                        if (!target->isAlive() || !target->has_aura(31427))
                            return;

                        // Sneeze
                        target->CastSpell(target, 31428, true);
                        target->remove_auras(31427);
                    });
                return;
            }
            case 31606: // Stormcrow Amulet
            {
                CreatureInfo const* cInfo =
                    ObjectMgr::GetCreatureTemplate(17970);

                // we must assume db or script set display id to native at
                // ending flight (if not, target is stuck with this model)
                if (cInfo)
                    target->SetDisplayId(Creature::ChooseDisplayId(cInfo));

                return;
            }
            case 32045: // Soul Charge
            case 32051:
            case 32052:
            {
                // max duration is 2 minutes, but expected to be random duration
                // real time randomness is unclear, using max 30 seconds here
                // see further down for expire of this aura
                GetHolder()->SetAuraDuration(urand(1, 30) * IN_MILLISECONDS);
                return;
            }
            case 32830: // Auchenai Crypts Possess
            {
                target->CastSpell(target, 32831, true, nullptr, this);
                target->CastSpell(target, 150007, true, nullptr, this);
                return;
            }
            case 33711: // Murmur's Touch Normal
            case 38794: // Murmur's Touch Heroic
            {
                // Trigger player pull on aura application
                target->CastSpell(target, 33689, true);
                return;
            }
            case 12021: // Fixate
            case 34719:
            {
                if (target && GetCaster() &&
                    GetCaster()->GetTypeId() == TYPEID_UNIT)
                {
                    ((Creature*)GetCaster())->SetFocusTarget(target);
                }
                return;
            }
            case 37676: // Insidious Whisper
            {
                if (Unit* caster = GetCaster())
                {
                    auto pos = target->GetPoint(frand(0, 2 * M_PI_F), 5.0f);
                    // Script despawns creature
                    if (Creature* c = caster->SummonCreature(21857, pos.x,
                            pos.y, pos.z, 0.0f, TEMPSUMMON_MANUAL_DESPAWN, 0))
                        if (c->AI())
                            c->AI()->AttackStart(target); // Let script know the
                                                          // target we spawned
                                                          // from
                }
                return;
            }
            // Gender spells
            case 38224: // Illidari Agent Illusion
            case 37096: // Blood Elf Illusion
            case 46354: // Blood Elf Illusion
            {
                uint8 gender = target->getGender();
                uint32 spellId;
                switch (GetId())
                {
                case 38224:
                    spellId = (gender == GENDER_MALE ? 38225 : 38227);
                    break;
                case 37096:
                    spellId = (gender == GENDER_MALE ? 37092 : 37094);
                    break;
                case 46354:
                    spellId = (gender == GENDER_MALE ? 46355 : 46356);
                    break;
                default:
                    return;
                }
                target->CastSpell(target, spellId, true, nullptr, this);
                return;
            }
            case 38544: // Coax Marmot
            {
                if (target->GetTypeId() != TYPEID_PLAYER)
                    return;
                float x, y, z, o = target->GetO();
                target->GetPosition(x, y, z);
                if (Creature* c = target->SummonCreature(
                        GetSpellProto()->EffectMiscValue[EFFECT_INDEX_0], x, y,
                        z, o, TEMPSUMMON_CORPSE_DESPAWN, 0))
                    target->CharmUnit(true, true, c, GetHolder());
                return;
            }
            case 39053: // Serpentshrine Parasite Single
            {
                // Despawn Parasite
                if (Unit* caster = GetCaster())
                    if (caster->GetTypeId() == TYPEID_UNIT &&
                        caster->GetEntry() == 22379)
                        ((Creature*)caster)->ForcedDespawn();
                return;
            }
            case 39850:                // Rocket Blast
                if (roll_chance_i(20)) // backfire stun
                    target->CastSpell(target, 51581, true, nullptr, this);
                return;
            case 43873: // Headless Horseman Laugh
                target->PlayDistanceSound(11965);
                return;
            case 46699: // Requires No Ammo
                if (target->GetTypeId() == TYPEID_PLAYER)
                    // not use ammo and not allow use
                    ((Player*)target)->RemoveAmmo();
                return;
            case 48025: // Headless Horseman's Mount
                Spell::SelectMountByAreaAndSkill(
                    target, GetSpellProto(), 51621, 48024, 51617, 48023, 0);
                return;
            }
            break;
        }
        case SPELLFAMILY_WARRIOR:
        {
            switch (GetId())
            {
            case 41099: // Battle Stance
            {
                if (target->GetTypeId() != TYPEID_UNIT)
                    return;

                // Stance Cooldown
                target->CastSpell(target, 41102, true, nullptr, this);

                // Battle Aura
                target->CastSpell(target, 41106, true, nullptr, this);

                // equipment
                ((Creature*)target)->SetVirtualItem(VIRTUAL_ITEM_SLOT_0, 32614);
                ((Creature*)target)->SetVirtualItem(VIRTUAL_ITEM_SLOT_1, 0);
                ((Creature*)target)->SetVirtualItem(VIRTUAL_ITEM_SLOT_2, 0);
                return;
            }
            case 41100: // Berserker Stance
            {
                if (target->GetTypeId() != TYPEID_UNIT)
                    return;

                // Stance Cooldown
                target->CastSpell(target, 41102, true, nullptr, this);

                // Berserker Aura
                target->CastSpell(target, 41107, true, nullptr, this);

                // equipment
                ((Creature*)target)->SetVirtualItem(VIRTUAL_ITEM_SLOT_0, 32614);
                ((Creature*)target)->SetVirtualItem(VIRTUAL_ITEM_SLOT_1, 0);
                ((Creature*)target)->SetVirtualItem(VIRTUAL_ITEM_SLOT_2, 0);
                return;
            }
            case 41101: // Defensive Stance
            {
                if (target->GetTypeId() != TYPEID_UNIT)
                    return;

                // Stance Cooldown
                target->CastSpell(target, 41102, true, nullptr, this);

                // Defensive Aura
                target->CastSpell(target, 41105, true, nullptr, this);

                // equipment
                ((Creature*)target)->SetVirtualItem(VIRTUAL_ITEM_SLOT_0, 32604);
                ((Creature*)target)->SetVirtualItem(VIRTUAL_ITEM_SLOT_1, 31467);
                ((Creature*)target)->SetVirtualItem(VIRTUAL_ITEM_SLOT_2, 0);
                return;
            }
            }
            break;
        }
        case SPELLFAMILY_WARLOCK:
        {
            // Voidstar Talisman
            if (GetId() == 37386)
            {
                target->CastSpell(target, 150045, true);
            }
            break;
        }
        }
    }
    // AT REMOVE
    else
    {
        if (IsQuestTameSpell(GetId()) && target->isAlive())
        {
            Unit* caster = GetCaster();
            if (!caster || !caster->isAlive())
                return;

            uint32 finalSpellId = 0;
            switch (GetId())
            {
            case 19548:
                finalSpellId = 19597;
                break;
            case 19674:
                finalSpellId = 19677;
                break;
            case 19687:
                finalSpellId = 19676;
                break;
            case 19688:
                finalSpellId = 19678;
                break;
            case 19689:
                finalSpellId = 19679;
                break;
            case 19692:
                finalSpellId = 19680;
                break;
            case 19693:
                finalSpellId = 19684;
                break;
            case 19694:
                finalSpellId = 19681;
                break;
            case 19696:
                finalSpellId = 19682;
                break;
            case 19697:
                finalSpellId = 19683;
                break;
            case 19699:
                finalSpellId = 19685;
                break;
            case 19700:
                finalSpellId = 19686;
                break;
            case 30646:
                finalSpellId = 30647;
                break;
            case 30653:
                finalSpellId = 30648;
                break;
            case 30654:
                finalSpellId = 30652;
                break;
            case 30099:
                finalSpellId = 30100;
                break;
            case 30102:
                finalSpellId = 30103;
                break;
            case 30105:
                finalSpellId = 30104;
                break;
            }

            if (finalSpellId)
                caster->CastSpell(target, finalSpellId, true, nullptr, this);

            return;
        }

        switch (GetId())
        {
        case 126: // Eye of Kilrogg
            if (target->GetTypeId() == TYPEID_PLAYER)
            {
                // Need to wait so Kilrogg's AI gets time to realize the Aura is
                // out, so player's pet is not desummoned in Unit::Charm
                target->queue_action_ticks(2, [target]()
                    {
                        static_cast<Player*>(target)
                            ->ResummonPetTemporaryUnSummonedIfAny();
                    });
            }
            return;
        case 10255: // Stoned
        {
            if (Unit* caster = GetCaster())
            {
                if (caster->GetTypeId() != TYPEID_UNIT)
                    return;

                // see dummy effect of spell 10254 for removal of flags etc
                caster->CastSpell(caster, 10254, true);
            }
            return;
        }
        case 12479: // Hex of Jammal'an
            target->CastSpell(target, 12480, true, nullptr, this);
            return;
        case 12774: // (DND) Belnistrasz Idol Shutdown Visual
        {
            if (m_removeMode == AURA_REMOVE_BY_DEATH)
                return;

            // Idom Rool Camera Shake <- wtf, don't drink while making
            // spellnames?
            if (Unit* caster = GetCaster())
                caster->CastSpell(caster, 12816, true);

            return;
        }
        case 13278: // Gnomish Death Ray
        {
            // Remove self-damaging spell if the aura was cancelled
            if (m_removeMode == AURA_REMOVE_BY_CANCEL)
                if (Unit* caster = GetCaster())
                    caster->remove_auras(13493);
            return;
        }
        case 22682: // Shadow flame
        {
            GetTarget()->CastSpell(
                GetTarget(), 22993, true, nullptr, nullptr, GetCasterGuid());
            return;
        }
        case 23155: // Brood Affliction: Red
        {
            if (m_removeMode == AURA_REMOVE_BY_DEATH)
                if (auto caster = GetCaster())
                    GetTarget()->CastSpell(caster, 23168, true);
            return;
        }
        case 28169: // Mutating Injection
        {
            // Mutagen Explosion
            target->CastSpell(target, 28206, true, nullptr, this);
            // Poison Cloud
            target->CastSpell(target, 28240, true, nullptr, this);
            return;
        }
        case 28682: // Mage's Combustion
        {
            target->remove_auras(11129, Unit::aura_no_op_true, m_removeMode);
            return;
        }
        case 30410: // Shadow Grasp
        {
            target->CastSpell(target, 44032, true);
            return;
        }
        case 32045: // Soul Charge
        {
            if (m_removeMode == AURA_REMOVE_BY_EXPIRE)
                target->CastSpell(target, 32054, true, nullptr, this);

            return;
        }
        case 32051: // Soul Charge
        {
            if (m_removeMode == AURA_REMOVE_BY_EXPIRE)
                target->CastSpell(target, 32057, true, nullptr, this);

            return;
        }
        case 32052: // Soul Charge
        {
            if (m_removeMode == AURA_REMOVE_BY_EXPIRE)
                target->CastSpell(target, 32053, true, nullptr, this);

            return;
        }
        case 32286: // Focus Target Visual
        {
            if (m_removeMode == AURA_REMOVE_BY_EXPIRE)
                target->CastSpell(target, 32301, true, nullptr, this);

            return;
        }
        case 32756: // Shadowy Disguise
        {
            if (target->getGender() == GENDER_MALE)
                target->remove_auras(38080);
            else
                target->remove_auras(38081);
            return;
        }
        case 32830: // Auchenai Crypts Possess
        {
            target->remove_auras(32831);
            target->remove_auras(150007);
            return;
        }
        case 33045: // Wrath of the Astromancer
        {
            if (m_removeMode != AURA_REMOVE_BY_DEATH &&
                m_removeMode != AURA_REMOVE_BY_EXPIRE)
                return;

            // Cast twice if target dies
            int max = m_removeMode == AURA_REMOVE_BY_DEATH ? 2 : 1;

            for (int i = 0; i < max; ++i)
                target->CastSpell(
                    target, 33048, true, nullptr, this, GetCasterGuid());

            return;
        }
        case 33711: // Murmur's Touch Normal
        case 38794: // Murmur's Touch Heroic
        {
            // Trigger shockwave on aura expiration
            target->CastSpell(target, 33686, true);
            return;
        }
        case 12021: // Fixate
        case 34719:
        {
            if (target && GetCaster() &&
                GetCaster()->GetTypeId() == TYPEID_UNIT)
            {
                ((Creature*)GetCaster())->SetFocusTarget(nullptr);
            }
            return;
        }
        case 34799: // Arcane Devastation
            target->remove_auras(34794);
            return;
        case 34946: // Golem Repair
            target->remove_auras(34937);
            return;
        case 35079: // Misdirection, triggered buff
        {
            if (Unit* pCaster = GetCaster())
                pCaster->remove_auras(34477);
            return;
        }
        case 36620: // Spirit Hunter
        {
            if (Pet* p = target->FindGuardianWithEntry(21332))
            {
                p->ForcedDespawn();
                target->RemoveGuardian(p);
            }
            return;
        }
        case 36642: // Banished from Shattrath City
        case 36671:
        {
            target->CombatStopWithPets(true);
            target->getHostileRefManager().deleteReferences();
            target->NearTeleportTo(-1500, 5210, 34.34, 2);
            target->CastSpell(target, 39533, true);
            break;
        }
        case 36730: // Flame Strike
        {
            target->CastSpell(target, 36731, true, nullptr, this);
            return;
        }
        case 36731: // Flame Strike
        {
            if (target->GetEntry() == 21369 &&
                target->GetTypeId() == TYPEID_UNIT)
                ((Creature*)target)->ForcedDespawn();
            return;
        }
        case 36904: // Lashh'an Channeling
        {
            while (Pet* p = target->FindGuardianWithEntry(21468))
                p->Unsummon(PET_SAVE_AS_DELETED); // Not saved
            return;
        }
        case 37096: // Blood Elf Illusion
        case 38224: // Illidari Agent Illusion
        case 46354: // Blood Elf Illusion
        {
            uint8 gender = target->getGender();
            uint32 spellId;
            switch (GetId())
            {
            case 38224:
                spellId = (gender == GENDER_MALE ? 38225 : 38227);
                break;
            case 37096:
                spellId = (gender == GENDER_MALE ? 37092 : 37094);
                break;
            case 46354:
                spellId = (gender == GENDER_MALE ? 46355 : 46356);
                break;
            default:
                return;
            }
            target->remove_auras(spellId);
            return;
        }
        case 37386: // Voidstar Talisman
        {
            if (Pet* pet = target->GetPet())
                pet->remove_auras(150045);
            break;
        }
        case 37408: // Oscillation Field
        {
            // If you have 5 buffs on you, you receive kill credit
            if (target->GetTypeId() == TYPEID_PLAYER)
            {
                Player* plr = (Player*)target;
                if (plr->GetAuraCount(37408) >= 5)
                    plr->AreaExploredOrEventHappens(
                        10594); // Gauging the Resonant Frequency
            }
            return;
        }
        case 37850: // Watery Grave
        case 38023:
        case 38024:
        case 38025:
        {
            target->CastSpell(target, 37852, true);
            return;
        }
        case 38544: // Coax Marmot
        {
            if (Creature* c = dynamic_cast<Creature*>(target->GetCharm()))
            {
                target->CharmUnit(false, true, c, GetHolder());

                // we should not get attacked by the creatures that killed the
                // marmot
                c->CombatStop();
                c->getHostileRefManager().deleteReferences();
                target->CombatStop();
                target->getHostileRefManager().deleteReferences();

                c->ForcedDespawn();
            }
            return;
        }
        case 38708: // Demonaic Visitation
        {
            // Cast "Summon Demonaic Visitation". His text is handled in SmartAI
            target->CastSpell(target, 38991, true);
            return;
        }
        case 39032: // Initial Infection (SSC)
        {
            if (Unit* caster = GetCaster())
            {
                int32 bp0, bp1;
                bp0 = bp1 =
                    GetSpellProto()->EffectBasePoints[EFFECT_INDEX_1] * 1.1f;
                target->CastCustomSpell(target, 39042, &bp0, &bp1, nullptr,
                    true, nullptr, nullptr, caster->GetObjectGuid());
            }
            return;
        }
        case 39042: // Rampant Infection (SSC)
        {
            if (Unit* caster = GetCaster())
            {
                int32 bp0, bp1;
                bp0 = bp1 = GetBasePoints() * 1.1f;
                target->CastCustomSpell(target, 39042, &bp0, &bp1, nullptr,
                    true, nullptr, nullptr, caster->GetObjectGuid());
            }
            return;
        }
        case 39044: // Serpentshrine Parasite AoE
        case 39053: // Serpentshrine Parasite Single
        {
            // Summon a parasite (or 4 if host died):
            for (int i = 0; i < (m_removeMode == AURA_REMOVE_BY_DEATH ? 4 : 1);
                 ++i)
                target->CastSpell(target, 39045, true);
            return;
        }
        case 39219: // Death's Door Fel Cannon
        {
            GetTarget()->Kill(GetTarget(), false);
            return;
        }
        case 39238: // Fumping
        {
            Unit* caster = GetCaster();
            if (caster && m_removeMode == AURA_REMOVE_BY_EXPIRE)
            {
                float x, y, z, o = caster->GetO();
                caster->GetPosition(x, y, z);
                if (roll_chance_i(50))
                {
                    // Mature Bone Sifter
                    caster->SummonCreature(
                        22482, x, y, z, o, TEMPSUMMON_TIMED_DESPAWN, 120000);
                }
                else
                {
                    // Sand Gnome
                    caster->SummonCreature(
                        22483, x, y, z, o, TEMPSUMMON_TIMED_DESPAWN, 120000);
                }
            }
            return;
        }
        case 39246: // Fumping
        {
            Unit* caster = GetCaster();
            if (caster && m_removeMode == AURA_REMOVE_BY_EXPIRE)
            {
                float o = caster->GetO();
                // Wowhead comments list it to ~5% chance to get Hai'shulud
                float rn = rand_norm_f();
                if (rn < 0.05f)
                {
                    // Hai'shulud <The Bone Emperor>
                    float x, y, z;
                    caster->GetPosition(x, y, z);
                    caster->SummonCreature(
                        22038, x, y, z, o, TEMPSUMMON_TIMED_DESPAWN, 180000);
                }
                else if (rn < 0.525)
                {
                    // 3 x Mature Bone Sifter
                    for (int i = 0; i < 3; ++i)
                    {
                        auto pos =
                            caster->GetPoint(frand(0.0f, 2 * M_PI_F), 4.0f);
                        o = caster->GetAngle(pos.x, pos.y) + M_PI_F;
                        if (o > 2 * M_PI_F)
                            o -= 2 * M_PI_F;
                        caster->SummonCreature(22482, pos.x, pos.y, pos.z, o,
                            TEMPSUMMON_TIMED_DESPAWN, 120000);
                    }
                }
                else
                {
                    // 3 x Sand Gnome
                    for (int i = 0; i < 3; ++i)
                    {
                        auto pos =
                            caster->GetPoint(frand(0.0f, 2 * M_PI_F), 4.0f);
                        o = caster->GetAngle(pos.x, pos.y) + M_PI_F;
                        if (o > 2 * M_PI_F)
                            o -= 2 * M_PI_F;
                        caster->SummonCreature(22483, pos.x, pos.y, pos.z, o,
                            TEMPSUMMON_TIMED_DESPAWN, 120000);
                    }
                }
            }
            return;
        }
        case 41099: // Battle Stance
        {
            // Battle Aura
            target->remove_auras(41106);
            return;
        }
        case 41100: // Berserker Stance
        {
            // Berserker Aura
            target->remove_auras(41107);
            return;
        }
        case 41101: // Defensive Stance
        {
            // Defensive Aura
            target->remove_auras(41105);
            return;
        }
        case 42454: // Captured Totem
        {
            if (m_removeMode == AURA_REMOVE_BY_DEFAULT)
            {
                if (target->getDeathState() != CORPSE)
                    return;

                Unit* pCaster = GetCaster();

                if (!pCaster)
                    return;

                // Captured Totem Test Credit
                if (Player* pPlayer =
                        pCaster->GetCharmerOrOwnerPlayerOrPlayerItself())
                    pPlayer->CastSpell(pPlayer, 42455, true);
            }

            return;
        }
        case 42517: // Beam to Zelfrax
        {
            // expecting target to be a dummy creature
            Creature* pSummon = target->SummonCreature(23864, 0.0f, 0.0f, 0.0f,
                target->GetO(), TEMPSUMMON_DEAD_DESPAWN, 0);

            Unit* pCaster = GetCaster();

            if (pSummon && pCaster)
                pSummon->movement_gens.push(
                    new movement::PointMovementGenerator(0, pCaster->GetX(),
                        pCaster->GetY(), pCaster->GetZ(), true, true));

            return;
        }
        case 44191: // Flame Strike
        {
            if (target->GetMap()->IsDungeon())
            {
                uint32 spellId =
                    target->GetMap()->IsRegularDifficulty() ? 44190 : 46163;

                target->CastSpell(target, spellId, true, nullptr, this);
            }
            return;
        }
        case 45934: // Dark Fiend
        {
            // Kill target if dispelled
            if (m_removeMode == AURA_REMOVE_BY_DISPEL)
                target->DealDamage(target, target->GetHealth(), nullptr,
                    DIRECT_DAMAGE, SPELL_SCHOOL_MASK_NORMAL, nullptr, false,
                    false);
            return;
        }
        case 46308: // Burning Winds
        {
            // casted only at creatures at spawn
            target->CastSpell(target, 47287, true, nullptr, this);
            return;
        }
        case 150018: // Mark of Death (Doomwalker)
        {
            if (m_removeMode == AURA_REMOVE_BY_DEATH)
                target->AddAuraThroughNewHolder(37128, target);
            return;
        }
        case 150055: // Delayed Graveyard Resurrection
        {
            if (m_removeMode == AURA_REMOVE_BY_EXPIRE && target->isDead() &&
                target->GetTypeId() == TYPEID_PLAYER && target->IsInWorld())
            {
                static_cast<Player*>(target)->ResurrectPlayer(0.5f);
                static_cast<Player*>(target)->SpawnCorpseBones();
            }
            return;
        }
        case 150070: // Clear Inebriate On Expiry Dummy
        {
            if (target->GetTypeId() == TYPEID_PLAYER)
                static_cast<Player*>(target)->SetDrunkValue(0, 0);
            return;
        }
        }
    }

    // AT APPLY & REMOVE

    switch (GetSpellProto()->SpellFamilyName)
    {
    case SPELLFAMILY_GENERIC:
    {
        switch (GetId())
        {
        case 6606: // Self Visual - Sleep Until Cancelled (DND)
        {
            if (apply)
            {
                target->SetStandState(UNIT_STAND_STATE_SLEEP);
                target->addUnitState(UNIT_STAT_ROOT);
            }
            else
            {
                target->clearUnitState(UNIT_STAT_ROOT);
                target->SetStandState(UNIT_STAND_STATE_STAND);
            }

            return;
        }
        case 24658: // Unstable Power
        {
            if (apply)
            {
                Unit* caster = GetCaster();
                if (!caster)
                    return;

                caster->CastSpell(
                    target, 24659, true, nullptr, nullptr, GetCasterGuid());
            }
            else
                target->remove_auras(24659);
            return;
        }
        case 24661: // Restless Strength
        {
            if (apply)
            {
                Unit* caster = GetCaster();
                if (!caster)
                    return;

                caster->CastSpell(
                    target, 24662, true, nullptr, nullptr, GetCasterGuid());
            }
            else
                target->remove_auras(24662);
            return;
        }
        case 29266: // Permanent Feign Death
        case 31261: // Permanent Feign Death (Root)
        case 37493: // Feign Death
        {
            // Unclear what the difference really is between them.
            // Some has effect1 that makes the difference, however not all.
            // Some appear to be used depending on creature location, in water,
            // at solid ground, in air/suspended, etc
            // For now, just handle all the same way
            if (target->GetTypeId() == TYPEID_UNIT)
                target->SetFeignDeath(apply);

            return;
        }
        case 32216: // Victorious
            if (target->getClass() == CLASS_WARRIOR)
                target->ModifyAuraState(AURA_STATE_WARRIOR_VICTORY_RUSH, apply);
            return;
        case 35356: // Spawn Feign Death
        case 35357: // Spawn Feign Death
        {
            if (target->GetTypeId() == TYPEID_UNIT)
            {
                // Flags not set like it's done in SetFeignDeath()
                // UNIT_DYNFLAG_DEAD does not appear with these spells.
                // All of the spells appear to be present at spawn and not used
                // to feign in combat or similar.
                if (apply)
                {
                    target->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_UNK_29);
                    target->SetFlag(UNIT_FIELD_FLAGS_2, UNIT_FLAG2_FEIGN_DEATH);

                    target->addUnitState(UNIT_STAT_DIED);
                }
                else
                {
                    target->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_UNK_29);
                    target->RemoveFlag(
                        UNIT_FIELD_FLAGS_2, UNIT_FLAG2_FEIGN_DEATH);

                    target->clearUnitState(UNIT_STAT_DIED);
                }
            }
            return;
        }
        case 40133: // Summon Fire Elemental
        {
            Unit* caster = GetCaster();
            if (!caster)
                return;

            Unit* owner = caster->GetOwner();
            if (owner && owner->GetTypeId() == TYPEID_PLAYER)
            {
                if (apply)
                    owner->CastSpell(owner, 8985, true);
                else
                    ((Player*)owner)->RemovePet(PET_SAVE_REAGENTS);
            }
            return;
        }
        case 40132: // Summon Earth Elemental
        {
            Unit* caster = GetCaster();
            if (!caster)
                return;

            Unit* owner = caster->GetOwner();
            if (owner && owner->GetTypeId() == TYPEID_PLAYER)
            {
                if (apply)
                    owner->CastSpell(owner, 19704, true);
                else
                    ((Player*)owner)->RemovePet(PET_SAVE_REAGENTS);
            }
            return;
        }
        case 40214: // Dragonmaw Illusion
        {
            if (apply)
            {
                target->CastSpell(target, 40216, true);
                target->CastSpell(target, 42016, true);
            }
            else
            {
                target->remove_auras(40216);
                target->remove_auras(42016);
            }
            return;
        }
        case 42515: // Jarl Beam
        {
            // aura animate dead (fainted) state for the duration, but we need
            // to animate the death itself (correct way below?)
            if (Unit* pCaster = GetCaster())
                pCaster->ApplyModFlag(
                    UNIT_FIELD_FLAGS_2, UNIT_FLAG2_FEIGN_DEATH, apply);

            // Beam to Zelfrax at remove
            if (!apply)
                target->CastSpell(target, 42517, true);
            return;
        }
        case 27978:
        case 40131:
            if (apply)
                target->m_AuraFlags |= UNIT_AURAFLAG_ALIVE_INVISIBLE;
            else
                target->m_AuraFlags |= ~UNIT_AURAFLAG_ALIVE_INVISIBLE;
            return;
        case 150061: // Loss of Control ZG Hakkar Blood Siphon
        {
            if (target->GetTypeId() == TYPEID_PLAYER)
                static_cast<Player*>(target)->SetClientControl(target, !apply);
            return;
        }
        }
        break;
    }
    case SPELLFAMILY_PALADIN:
    {
        // Seal of the Crusader
        if (GetSpellProto()->SpellFamilyFlags & UI64LIT(0x0000000000000200))
        {
            Unit* caster = GetCaster();
            if (!caster || caster->GetTypeId() != TYPEID_PLAYER)
                return;

            if (apply)
                m_spellmod = new SpellModifier(SPELLMOD_EFFECT2, SPELLMOD_FLAT,
                    40, GetId(), UI64LIT(0x0000800000000000));

            ((Player*)caster)->AddSpellMod(&m_spellmod, apply);
            // Damage multiplier = 1/1.4
            caster->HandleStatModifier(
                UNIT_MOD_DAMAGE_MAINHAND, TOTAL_PCT, -28.57f, apply);
        }

        switch (GetId())
        {
        // Increased Flash of Light
        case 28851:
        case 28853:
        case 32403:
        {
            if (target->GetTypeId() != TYPEID_PLAYER)
                return;
            if (apply)
                m_spellmod = new SpellModifier(SPELLMOD_SPELLPOWER,
                    SPELLMOD_FLAT, m_modifier.m_amount, GetId(),
                    UI64LIT(0x0000000040000000));
            static_cast<Player*>(target)->AddSpellMod(&m_spellmod, apply);
            return;
        }
        // Libram of Wacking
        case 33695:
        {
            if (target->GetTypeId() != TYPEID_PLAYER)
                return;
            if (apply)
                m_spellmod = new SpellModifier(SPELLMOD_SPELLPOWER,
                    SPELLMOD_FLAT, m_modifier.m_amount, GetId(),
                    UI64LIT(0x0000000200000000));
            static_cast<Player*>(target)->AddSpellMod(&m_spellmod, apply);
            return;
        }
        // Libram of the Lightbringer
        case 34231:
        {
            if (target->GetTypeId() != TYPEID_PLAYER)
                return;
            if (apply)
                m_spellmod = new SpellModifier(SPELLMOD_SPELLPOWER,
                    SPELLMOD_FLAT, m_modifier.m_amount, GetId(),
                    UI64LIT(0x0000000080000000));
            static_cast<Player*>(target)->AddSpellMod(&m_spellmod, apply);
            return;
        }
        // T4 healing set bonus: Increased Judgement of Light
        case 37182:
        {
            if (target->GetTypeId() != TYPEID_PLAYER)
                return;
            if (apply)
                m_spellmod = new SpellModifier(SPELLMOD_SPELLPOWER,
                    SPELLMOD_FLAT, m_modifier.m_amount, GetId(), 0x80000000);
            static_cast<Player*>(target)->AddSpellMod(&m_spellmod, apply);
            return;
        }
        // Libram of Divine Purpose
        case 43743:
        {
            if (target->GetTypeId() != TYPEID_PLAYER)
                return;
            if (apply)
                m_spellmod = new SpellModifier(SPELLMOD_SPELLPOWER,
                    SPELLMOD_FLAT, m_modifier.m_amount, GetId(),
                    UI64LIT(0x0000000000000400));
            static_cast<Player*>(target)->AddSpellMod(&m_spellmod, apply);
            return;
        }
        // Righteous Fury
        case 25780:
        {
            Unit* caster = GetCaster();

            if (caster && target->GetTypeId() == TYPEID_PLAYER)
            {
                auto& auras = caster->GetAurasByType(SPELL_AURA_MOD_THREAT);
                for (const auto& aura : auras)
                {
                    // Remove Fanaticism threat reduction while Righteous Fury
                    // is active
                    if ((aura)->GetId() >= 31879 && (aura)->GetId() <= 31883)
                    {
                        (aura)->HandleModThreat(!apply, true);
                        break; // for-loop
                    }
                }
            }
            break;
        }
        // Fanaticism
        case 31879:
        case 31880:
        case 31881:
        case 31882:
        case 31883:
        {
            Unit* caster = GetCaster();

            // Negate Threat modifier if Righteous Fury is active
            if (caster && caster->has_aura(25780) && apply)
            {
                m_modifier.m_miscvalue = 127;
                m_modifier.m_amount = GetSpellProto()->EffectBasePoints[1] + 1;
                HandleModThreat(false, true);
            }
            break;
        }
        }
        break;
    }
    case SPELLFAMILY_MAGE:
    {
        // Hypothermia
        if (GetId() == 41425)
        {
            target->ModifyAuraState(AURA_STATE_HYPOTHERMIA, apply);
            return;
        }
        break;
    }
    case SPELLFAMILY_DRUID:
    {
        switch (GetId())
        {
        case 34246: // Idol of the Emerald Queen
        {
            if (target->GetTypeId() != TYPEID_PLAYER)
                return;

            if (apply)
                // dummy not have proper effectclassmask
                m_spellmod = new SpellModifier(SPELLMOD_SPELLPOWER,
                    SPELLMOD_FLAT, m_modifier.m_amount, GetId(), 0x1000000000);

            ((Player*)target)->AddSpellMod(&m_spellmod, apply);
            return;
        }
        }

        // Lifebloom
        if (GetSpellProto()->SpellFamilyFlags & 0x1000000000)
        {
            if (apply)
            {
                // prevent double apply bonuses
                if (target->GetTypeId() != TYPEID_PLAYER ||
                    !((Player*)target)->GetSession()->PlayerLoading())
                {
                    // save final heal amount
                    // final heal ignore stack amount
                    m_modifier.m_amount /= GetStackAmount();
                    // HACK: Idol of the Emerald Queen is currently affecting
                    // the final heal
                    if (target->has_aura(34246, SPELL_AURA_DUMMY))
                        m_modifier.m_amount -= 30;
                }
            }
            else
            {
                // Final heal on duration end
                if (m_removeMode != AURA_REMOVE_BY_EXPIRE &&
                    m_removeMode != AURA_REMOVE_BY_DISPEL)
                    return;

                // final heal
                if (target->IsInWorld() &&
                    !target->HasAuraWithMechanic(1 << (MECHANIC_BANISH - 1)))
                {
                    // Lifebloom dummy store single stack amount always
                    int32 amount = m_modifier.m_amount;
                    target->CastCustomSpell(target, 33778, &amount, nullptr,
                        nullptr, true, nullptr, this, GetCasterGuid());
                }
            }
            return;
        }

        // Predatory Strikes
        if (target->GetTypeId() == TYPEID_PLAYER &&
            GetSpellProto()->SpellIconID == 1563)
        {
            ((Player*)target)->UpdateAttackPowerAndDamage();
            return;
        }
        break;
    }
    case SPELLFAMILY_ROGUE:
        break;
    case SPELLFAMILY_SHAMAN:
    {
        switch (GetId())
        {
        case 6495: // Sentry Totem
        {
            target->queue_action_ticks(2, [target, apply]()
                {
                    if (target->GetTypeId() != TYPEID_PLAYER)
                        return;

                    Totem* totem = target->GetTotem(TOTEM_SLOT_AIR);

                    if (totem && apply)
                        ((Player*)target)->GetCamera().SetView(totem);
                    else
                        ((Player*)target)->GetCamera().ResetView();
                });

            return;
        }
        case 34294: // Increased Healing Wave (Totem of Spontaneous Regrowth)
        {
            if (target->GetTypeId() != TYPEID_PLAYER)
                return;
            if (apply)
                m_spellmod = new SpellModifier(SPELLMOD_SPELLPOWER,
                    SPELLMOD_FLAT, m_modifier.m_amount, GetId(), 0x40);
            static_cast<Player*>(target)->AddSpellMod(&m_spellmod, apply);
            return;
        }
        }
        // Improved Weapon Totems
        if (GetSpellProto()->SpellIconID == 57 &&
            target->GetTypeId() == TYPEID_PLAYER)
        {
            if (apply)
            {
                switch (m_effIndex)
                {
                case 0:
                    // Windfury Totem
                    m_spellmod = new SpellModifier(SPELLMOD_EFFECT1,
                        SPELLMOD_PCT, m_modifier.m_amount, GetId(),
                        UI64LIT(0x00200000000));
                    break;
                case 1:
                    // Flametongue Totem
                    m_spellmod = new SpellModifier(SPELLMOD_EFFECT1,
                        SPELLMOD_PCT, m_modifier.m_amount, GetId(),
                        UI64LIT(0x00400000000));
                    break;
                default:
                    return;
                }
            }

            ((Player*)target)->AddSpellMod(&m_spellmod, apply);
            return;
        }
        break;
    }
    }

    // pet auras
    if (PetAura const* petSpell = sSpellMgr::Instance()->GetPetAura(GetId()))
    {
        if (apply)
            target->AddPetAura(petSpell);
        else
            target->RemovePetAura(petSpell);
        return;
    }

    if (GetEffIndex() == EFFECT_INDEX_0 && target->GetTypeId() == TYPEID_PLAYER)
    {
        SpellAreaForAreaMapBounds saBounds =
            sSpellMgr::Instance()->GetSpellAreaForAuraMapBounds(GetId());
        if (saBounds.first != saBounds.second)
        {
            uint32 zone, area;
            target->GetZoneAndAreaId(zone, area);

            for (auto itr = saBounds.first; itr != saBounds.second; ++itr)
            {
                // some auras remove at aura remove
                if (!itr->second->IsFitToRequirements(
                        (Player*)target, zone, area))
                    target->remove_auras(itr->second->spellId);
                // some auras applied at aura apply
                else if (itr->second->autocast)
                {
                    if (!target->has_aura(itr->second->spellId))
                        target->CastSpell(target, itr->second->spellId, true);
                }
            }
        }
    }

    // script has to "handle with care", only use where data are not ok to use
    // in the above code.
    if (target->GetTypeId() == TYPEID_UNIT)
        sScriptMgr::Instance()->OnAuraDummy(this, apply);
}

void Aura::HandleAuraMounted(bool apply, bool Real)
{
    // only at real add/remove aura
    if (!Real)
        return;

    Unit* target = GetTarget();

    if (apply)
    {
        CreatureInfo const* ci =
            ObjectMgr::GetCreatureTemplate(m_modifier.m_miscvalue);
        if (!ci)
        {
            logging.error(
                "AuraMounted: `creature_template`='%u' not found in database "
                "(only need it modelid)",
                m_modifier.m_miscvalue);
            return;
        }

        uint32 display_id = Creature::ChooseDisplayId(ci);
        CreatureModelInfo const* minfo =
            sObjectMgr::Instance()->GetCreatureModelRandomGender(display_id);
        if (minfo)
            display_id = minfo->modelid;

        target->Mount(display_id, GetId());
    }
    else
    {
        target->Unmount(true);
    }
}

void Aura::HandleAuraWaterWalk(bool apply, bool Real)
{
    // only at real add/remove aura
    if (!Real)
        return;

    WorldPacket data;
    if (apply)
        data.initialize(SMSG_MOVE_WATER_WALK, 8 + 4);
    else
        data.initialize(SMSG_MOVE_LAND_WALK, 8 + 4);
    data << GetTarget()->GetPackGUID();
    data << uint32(0);
    GetTarget()->SendMessageToSet(&data, true);
}

void Aura::HandleAuraFeatherFall(bool apply, bool Real)
{
    // only at real add/remove aura
    if (!Real)
        return;
    Unit* target = GetTarget();
    WorldPacket data;
    if (apply)
        data.initialize(SMSG_MOVE_FEATHER_FALL, 8 + 4);
    else
        data.initialize(SMSG_MOVE_NORMAL_FALL, 8 + 4);
    data << target->GetPackGUID();
    data << uint32(0);
    target->SendMessageToSet(&data, true);

    // start fall from current height
    if (!apply && target->GetTypeId() == TYPEID_PLAYER)
        ((Player*)target)->SetFallInformation(0, target->GetZ());
}

void Aura::HandleAuraHover(bool apply, bool Real)
{
    // only at real add/remove aura
    if (!Real)
        return;

    WorldPacket data;
    if (apply)
        data.initialize(SMSG_MOVE_SET_HOVER, 8 + 4);
    else
        data.initialize(SMSG_MOVE_UNSET_HOVER, 8 + 4);
    data << GetTarget()->GetPackGUID();
    data << uint32(0);
    GetTarget()->SendMessageToSet(&data, true);
}

void Aura::HandleWaterBreathing(bool /*apply*/, bool /*Real*/)
{
    // update timers in client
    if (GetTarget()->GetTypeId() == TYPEID_PLAYER)
        ((Player*)GetTarget())->UpdateMirrorTimers();
}

void Aura::HandleAuraModShapeshift(bool apply, bool Real)
{
    if (!Real)
        return;

    ShapeshiftForm form = ShapeshiftForm(m_modifier.m_miscvalue);

    SpellShapeshiftFormEntry const* ssEntry =
        sSpellShapeshiftFormStore.LookupEntry(form);
    if (!ssEntry)
    {
        logging.error("Unknown shapeshift form %u in spell %u", form, GetId());
        return;
    }

    uint32 modelid = 0;
    Powers PowerType = POWER_MANA;
    Unit* target = GetTarget();

    if (ssEntry->modelID_A)
    {
        // i will asume that creatures will always take the defined model from
        // the dbc
        // since no field in creature_templates describes wether an alliance or
        // horde modelid should be used at shapeshifting
        if (target->GetTypeId() != TYPEID_PLAYER)
            modelid = ssEntry->modelID_A;
        else
        {
            // players are a bit different since the dbc has seldomly an horde
            // modelid
            if (Player::TeamForRace(target->getRace()) == HORDE)
            {
                // get model for race ( in 2.2.4 no horde models in dbc field,
                // only 0 in it
                modelid = sObjectMgr::Instance()->GetModelForRace(
                    ssEntry->modelID_A, target->getRaceMask());
            }

            // nothing found in above, so use default
            if (!modelid)
                modelid = ssEntry->modelID_A;
        }
    }

    // remove polymorph before changing display id to keep new display id
    switch (form)
    {
    case FORM_CAT:
    case FORM_TREE:
    case FORM_TRAVEL:
    case FORM_AQUA:
    case FORM_BEAR:
    case FORM_DIREBEAR:
    case FORM_FLIGHT_EPIC:
    case FORM_FLIGHT:
    case FORM_MOONKIN:
    {
        // Remove mechanic root and snare auras
        target->remove_auras_if(
            [](AuraHolder* holder)
            {
                uint32 mechanics =
                    GetAllSpellMechanicMask(holder->GetSpellProto());
                return mechanics & ((1 << (MECHANIC_SNARE - 1)) |
                                       (1 << (MECHANIC_ROOT - 1)));
            },
            AURA_REMOVE_BY_CANCEL);

        // Remove polymorph
        if (target->IsPolymorphed())
            target->remove_auras(target->getTransForm());

        break;
    }
    default:
        break;
    }

    if (apply)
    {
        // remove other shapeshift before applying a new one
        target->remove_auras(SPELL_AURA_MOD_SHAPESHIFT,
            [this](AuraHolder* holder)
            {
                return holder != GetHolder();
            });

        // need send to client not form active state, or at re-apply form client
        // go crazy
        // target->SendForcedObjectUpdate();                -- not need in
        // pre-3.x

        if (modelid > 0)
            target->SetDisplayId(modelid);

        // now only powertype must be set
        switch (form)
        {
        case FORM_CAT:
            PowerType = POWER_ENERGY;
            break;
        case FORM_BEAR:
        case FORM_DIREBEAR:
        case FORM_BATTLESTANCE:
        case FORM_BERSERKERSTANCE:
        case FORM_DEFENSIVESTANCE:
            PowerType = POWER_RAGE;
            break;
        default:
            break;
        }

        if (PowerType != POWER_MANA)
        {
            // reset power to default values only at power change
            if (target->getPowerType() != PowerType)
                target->setPowerType(PowerType);

            switch (form)
            {
            case FORM_CAT:
            case FORM_BEAR:
            case FORM_DIREBEAR:
            {
                // get furor proc chance
                int32 furorChance = 0;
                auto& dummy = target->GetAurasByType(SPELL_AURA_DUMMY);
                for (const auto& elem : dummy)
                {
                    if ((elem)->GetSpellProto()->SpellIconID == 238)
                    {
                        furorChance = (elem)->GetModifier()->m_amount;
                        break;
                    }
                }

                if (m_modifier.m_miscvalue == FORM_CAT)
                {
                    target->SetPower(POWER_ENERGY, 0);
                    if (irand(1, 100) <= furorChance)
                        target->CastSpell(target, 17099, true, nullptr, this);
                }
                else
                {
                    target->SetPower(POWER_RAGE, 0);
                    if (irand(1, 100) <= furorChance)
                        target->CastSpell(target, 17057, true, nullptr, this);
                }
                break;
            }
            case FORM_BATTLESTANCE:
            case FORM_DEFENSIVESTANCE:
            case FORM_BERSERKERSTANCE:
            {
                uint32 Rage_val = 0;
                // Stance mastery + Tactical mastery (both passive, and last
                // have aura only in defense stance, but need apply at any
                // stance switch)
                if (target->GetTypeId() == TYPEID_PLAYER)
                {
                    PlayerSpellMap const& sp_list =
                        ((Player*)target)->GetSpellMap();
                    for (const auto& elem : sp_list)
                    {
                        if (elem.second.state == PLAYERSPELL_REMOVED)
                            continue;

                        SpellEntry const* spellInfo =
                            sSpellStore.LookupEntry(elem.first);
                        if (spellInfo &&
                            spellInfo->SpellFamilyName == SPELLFAMILY_WARRIOR &&
                            spellInfo->SpellIconID == 139)
                            Rage_val += target->CalculateSpellDamage(
                                            target, spellInfo, EFFECT_INDEX_0) *
                                        10;
                    }
                }

                if (target->GetPower(POWER_RAGE) > Rage_val)
                    target->SetPower(POWER_RAGE, Rage_val);
                break;
            }
            default:
                break;
            }
        }

        target->SetShapeshiftForm(form);

        // a form can give the player a new castbar with some spells.. this is a
        // clientside process..
        // serverside just needs to register the new spells so that player isn't
        // kicked as cheater
        if (target->GetTypeId() == TYPEID_PLAYER)
            for (uint32 i = 0; i < 8; ++i)
                if (ssEntry->spellId[i])
                    ((Player*)target)
                        ->addSpell(
                            ssEntry->spellId[i], true, false, false, false);

        // On cat form apply cast Track Humanoids (only if we have the spell
        // learned, so check if we do first)
        if (target->GetTypeId() == TYPEID_PLAYER && form == FORM_CAT &&
            !target->HasAuraType(SPELL_AURA_TRACK_RESOURCES))
            if (((Player*)target)->HasSpell(5225))
                target->CastSpell(target, 5225, true);
    }
    else
    {
        if (modelid > 0)
            target->SetDisplayId(target->GetNativeDisplayId());

        if (target->getClass() == CLASS_DRUID)
            target->setPowerType(POWER_MANA);

        target->SetShapeshiftForm(FORM_NONE);

        switch (form)
        {
        // Nordrassil Harness - bonus
        case FORM_BEAR:
        case FORM_DIREBEAR:
        case FORM_CAT:
            if (AuraHolder* holder = target->get_aura(37315))
                if (Aura* aura = holder->GetAura(EFFECT_INDEX_0))
                    target->CastSpell(target, 37316, true, nullptr, aura);
            break;
        // Nordrassil Regalia - bonus
        case FORM_MOONKIN:
            if (AuraHolder* holder = target->get_aura(37324))
                if (Aura* aura = holder->GetAura(EFFECT_INDEX_0))
                    target->CastSpell(target, 37325, true, nullptr, aura);
            break;
        default:
            break;
        }

        // look at the comment in apply-part
        if (target->GetTypeId() == TYPEID_PLAYER)
            for (uint32 i = 0; i < 8; ++i)
                if (ssEntry->spellId[i])
                    ((Player*)target)
                        ->removeSpell(ssEntry->spellId[i], false, false, false);

        // re-apply any transform
        auto& al = target->GetAurasByType(SPELL_AURA_TRANSFORM);
        Aura* transform = nullptr;
        // reverse lookup to take latest cast transform first
        for (auto itr = al.rbegin(); itr != al.rend(); ++itr)
        {
            // prefer negative transforms
            if (transform == nullptr || !(*itr)->IsPositive())
                transform = *itr;
        }
        if (transform)
            transform->ApplyModifier(true);
    }

    // adding/removing linked auras
    // add/remove the shapeshift aura's boosts
    HandleShapeshiftBoosts(apply);

    if (target->GetTypeId() == TYPEID_PLAYER)
        ((Player*)target)->InitDataForForm();
}

void Aura::HandleAuraTransform(bool apply, bool Real)
{
    Unit* target = GetTarget();
    if (apply)
    {
        // NPC: remove mount on transform
        if (target->GetTypeId() == TYPEID_UNIT)
            target->Unmount();

        // special case (spell specific functionality)
        if (m_modifier.m_miscvalue == 0)
        {
            switch (GetId())
            {
            case 16739: // Orb of Deception
            {
                uint32 orb_model = target->GetNativeDisplayId();
                switch (orb_model)
                {
                // Troll Female
                case 1479:
                    target->SetDisplayId(10134);
                    break;
                // Troll Male
                case 1478:
                    target->SetDisplayId(10135);
                    break;
                // Tauren Male
                case 59:
                    target->SetDisplayId(10136);
                    break;
                // Human Male
                case 49:
                    target->SetDisplayId(10137);
                    break;
                // Human Female
                case 50:
                    target->SetDisplayId(10138);
                    break;
                // Orc Male
                case 51:
                    target->SetDisplayId(10139);
                    break;
                // Orc Female
                case 52:
                    target->SetDisplayId(10140);
                    break;
                // Dwarf Male
                case 53:
                    target->SetDisplayId(10141);
                    break;
                // Dwarf Female
                case 54:
                    target->SetDisplayId(10142);
                    break;
                // NightElf Male
                case 55:
                    target->SetDisplayId(10143);
                    break;
                // NightElf Female
                case 56:
                    target->SetDisplayId(10144);
                    break;
                // Undead Female
                case 58:
                    target->SetDisplayId(10145);
                    break;
                // Undead Male
                case 57:
                    target->SetDisplayId(10146);
                    break;
                // Tauren Female
                case 60:
                    target->SetDisplayId(10147);
                    break;
                // Gnome Male
                case 1563:
                    target->SetDisplayId(10148);
                    break;
                // Gnome Female
                case 1564:
                    target->SetDisplayId(10149);
                    break;
                // BloodElf Female
                case 15475:
                    target->SetDisplayId(17830);
                    break;
                // BloodElf Male
                case 15476:
                    target->SetDisplayId(17829);
                    break;
                // Dranei Female
                case 16126:
                    target->SetDisplayId(17828);
                    break;
                // Dranei Male
                case 16125:
                    target->SetDisplayId(17827);
                    break;
                default:
                    break;
                }
                break;
            }
            case 42365: // Murloc costume
                target->SetDisplayId(21723);
                break;
            // case 44186:                               // Gossip NPC
            // Appearance - All, Brewfest
            // break;
            // case 48305:                               // Gossip NPC
            // Appearance - All, Spirit of Competition
            // break;
            case 50517: // Dread Corsair
            case 51926: // Corsair Costume
            {
                // expected for players
                uint32 race = target->getRace();

                switch (race)
                {
                case RACE_HUMAN:
                    target->SetDisplayId(
                        target->getGender() == GENDER_MALE ? 25037 : 25048);
                    break;
                case RACE_ORC:
                    target->SetDisplayId(
                        target->getGender() == GENDER_MALE ? 25039 : 25050);
                    break;
                case RACE_DWARF:
                    target->SetDisplayId(
                        target->getGender() == GENDER_MALE ? 25034 : 25045);
                    break;
                case RACE_NIGHTELF:
                    target->SetDisplayId(
                        target->getGender() == GENDER_MALE ? 25038 : 25049);
                    break;
                case RACE_UNDEAD:
                    target->SetDisplayId(
                        target->getGender() == GENDER_MALE ? 25042 : 25053);
                    break;
                case RACE_TAUREN:
                    target->SetDisplayId(
                        target->getGender() == GENDER_MALE ? 25040 : 25051);
                    break;
                case RACE_GNOME:
                    target->SetDisplayId(
                        target->getGender() == GENDER_MALE ? 25035 : 25046);
                    break;
                case RACE_TROLL:
                    target->SetDisplayId(
                        target->getGender() == GENDER_MALE ? 25041 : 25052);
                    break;
                case RACE_GOBLIN: // not really player race (3.x), but model
                                  // exist
                    target->SetDisplayId(
                        target->getGender() == GENDER_MALE ? 25036 : 25047);
                    break;
                case RACE_BLOODELF:
                    target->SetDisplayId(
                        target->getGender() == GENDER_MALE ? 25032 : 25043);
                    break;
                case RACE_DRAENEI:
                    target->SetDisplayId(
                        target->getGender() == GENDER_MALE ? 25033 : 25044);
                    break;
                }

                break;
            }
            // case 50531:                               // Gossip NPC
            // Appearance - All, Pirate Day
            // break;
            // case 51010:                               // Dire Brew
            // break;
            default:
                logging.error(
                    "Aura::HandleAuraTransform, spell %u does not have "
                    "creature entry defined, need custom defined model.",
                    GetId());
                break;
            }
        }
        else
        {
            uint32 model_id;

            CreatureInfo const* ci =
                ObjectMgr::GetCreatureTemplate(m_modifier.m_miscvalue);
            if (!ci)
            {
                model_id = 16358; // pig pink ^_^
                logging.error(
                    "Auras: unknown creature id = %d (only need its modelid) "
                    "Form Spell Aura Transform in Spell ID = %d",
                    m_modifier.m_miscvalue, GetId());
            }
            else
                model_id = Creature::ChooseDisplayId(
                    ci); // Will use the default model here

            target->SetDisplayId(model_id);

            // creature case, need to update equipment
            if (ci && target->GetTypeId() == TYPEID_UNIT)
                ((Creature*)target)->LoadEquipment(ci->equipmentId, true);

            // Dragonmaw Illusion (set mount model also)
            if (GetId() == 42016 && target->GetMountID() &&
                !target->GetAurasByType(SPELL_AURA_MOD_FLIGHT_SPEED_MOUNTED)
                     .empty())
                target->SetUInt32Value(UNIT_FIELD_MOUNTDISPLAYID, 16314);

            // Shape of the Beast (apply hidden aura)
            if (GetId() == 33499)
                target->AddAuraThroughNewHolder(33949, target);
        }

        // update active transform spell only not set or not overwriting
        // negative by positive case
        if (!target->getTransForm() || !IsPositiveSpell(GetId()) ||
            IsPositiveSpell(target->getTransForm()))
            target->setTransForm(GetId());

        // polymorph case
        if (Real && target->GetTypeId() == TYPEID_PLAYER &&
            (GetSpellProto()->SpellFamilyName == SPELLFAMILY_MAGE &&
                GetSpellProto()->SpellFamilyFlags & 0x1000000))
        {
            // for players, start regeneration after 1s (in polymorph fast
            // regeneration case)
            // only if caster is Player (after patch 2.4.2)
            if (GetCasterGuid().IsPlayer())
                ((Player*)target)->setRegenTimer(1 * IN_MILLISECONDS);

            // dismount polymorphed target (after patch 2.4.2)
            if (target->IsMounted())
                target->remove_auras(SPELL_AURA_MOUNTED,
                    [this](AuraHolder* holder)
                    {
                        return holder != GetHolder();
                    });
        }
    }
    else
    {
        // ApplyModifier(true) will reapply it if need
        target->setTransForm(0);
        target->SetDisplayId(target->GetNativeDisplayId());

        // apply default equipment for creature case
        if (target->GetTypeId() == TYPEID_UNIT)
            ((Creature*)target)
                ->LoadEquipment(
                    ((Creature*)target)->GetCreatureInfo()->equipmentId, true);

        // re-apply some from still active with preference negative cases
        auto& otherTransforms = target->GetAurasByType(SPELL_AURA_TRANSFORM);
        if (!otherTransforms.empty())
        {
            // look for other transform auras
            Aura* handledAura = *otherTransforms.begin();
            for (const auto& otherTransform : otherTransforms)
            {
                // negative auras are preferred
                if (!IsPositiveSpell((otherTransform)->GetSpellProto()->Id))
                {
                    handledAura = otherTransform;
                    break;
                }
            }
            handledAura->ApplyModifier(true);
        }

        // Dragonmaw Illusion (restore mount model)
        if (GetId() == 42016 && target->GetMountID() == 16314)
        {
            if (!target->GetAurasByType(SPELL_AURA_MOUNTED).empty())
            {
                uint32 cr_id = target->GetAurasByType(SPELL_AURA_MOUNTED)
                                   .front()
                                   ->GetModifier()
                                   ->m_miscvalue;
                if (CreatureInfo const* ci =
                        ObjectMgr::GetCreatureTemplate(cr_id))
                {
                    uint32 display_id = Creature::ChooseDisplayId(ci);
                    CreatureModelInfo const* minfo =
                        sObjectMgr::Instance()->GetCreatureModelRandomGender(
                            display_id);
                    if (minfo)
                        display_id = minfo->modelid;

                    target->SetUInt32Value(
                        UNIT_FIELD_MOUNTDISPLAYID, display_id);
                }
            }
        }

        // Shape of the Beast (remove hidden aura)
        if (GetId() == 33499)
            target->remove_auras(33949);
    }

    // Set mage's polymorph status
    if (Real && GetSpellProto()->SpellFamilyName == SPELLFAMILY_MAGE &&
        GetSpellProto()->SpellFamilyFlags & 0x1000000)
        target->SetMagePolymorphed(apply);
}

void Aura::HandleForceReaction(bool apply, bool Real)
{
    if (GetTarget()->GetTypeId() != TYPEID_PLAYER)
        return;

    if (!Real)
        return;

    Player* player = (Player*)GetTarget();

    uint32 faction_id = m_modifier.m_miscvalue;
    ReputationRank faction_rank = ReputationRank(m_modifier.m_amount);

    player->GetReputationMgr().ApplyForceReaction(
        faction_id, faction_rank, apply);
    player->GetReputationMgr().SendForceReactions();

    // stop fighting if at apply forced rank friendly or at remove real rank
    // friendly
    if ((apply && faction_rank >= REP_FRIENDLY) ||
        (!apply && player->GetReputationRank(faction_id) >= REP_FRIENDLY))
        player->StopAttackFaction(faction_id);
}

void Aura::HandleAuraModSkill(bool apply, bool /*Real*/)
{
    if (GetTarget()->GetTypeId() != TYPEID_PLAYER)
        return;

    uint32 prot = GetSpellProto()->EffectMiscValue[m_effIndex];
    int32 points = GetModifier()->m_amount;

    ((Player*)GetTarget())
        ->ModifySkillBonus(prot, (apply ? points : -points),
            m_modifier.m_auraname == SPELL_AURA_MOD_SKILL_TALENT);
    if (prot == SKILL_DEFENSE)
        ((Player*)GetTarget())->UpdateDefenseBonusesMod();
}

void Aura::HandleChannelDeathItem(bool apply, bool Real)
{
    if (Real && !apply)
    {
        if (m_removeMode != AURA_REMOVE_BY_DEATH)
            return;
        // Item amount
        if (m_modifier.m_amount <= 0)
            return;

        SpellEntry const* spellInfo = GetSpellProto();
        if (spellInfo->EffectItemType[m_effIndex] == 0)
            return;

        Unit* victim = GetTarget();
        Unit* caster = GetCaster();
        if (!caster || caster->GetTypeId() != TYPEID_PLAYER)
            return;

        Player* p_caster = static_cast<Player*>(caster);

        // Soul Shard (target req.)
        if (spellInfo->EffectItemType[m_effIndex] == 6265)
        {
            // Only from non-grey units
            if (!p_caster->isHonorOrXPTarget(victim) ||
                (victim->GetTypeId() == TYPEID_UNIT &&
                    !p_caster->HasTapOn(static_cast<Creature*>(victim))))
                return;
        }

        // Add as many of the item as we can add
        uint32 item_id = spellInfo->EffectItemType[m_effIndex];
        uint32 count = m_modifier.m_amount;
        inventory::transaction trans(true, inventory::transaction::send_party,
            inventory::transaction::add_craft);
        trans.add(item_id, count);
        if (!p_caster->storage().finalize(trans))
        {
            p_caster->SendEquipError(
                EQUIP_ERR_INVENTORY_FULL, nullptr, nullptr, item_id);
            uint32 failed = trans.add_failures()[0];
            if (failed == count)
                return;
            trans = inventory::transaction();
            trans.add(item_id, count - failed);
            if (!p_caster->storage().finalize(trans))
                return;
        }
    }
}

void Aura::HandleBindSight(bool apply, bool /*Real*/)
{
    Unit* caster = GetCaster();
    if (!caster || caster->GetTypeId() != TYPEID_PLAYER)
        return;

    Camera& camera = ((Player*)caster)->GetCamera();
    if (apply)
        camera.SetView(GetTarget());
    else
        camera.ResetView();
}

void Aura::HandleFarSight(bool apply, bool /*Real*/)
{
    Unit* caster = GetCaster();
    if (!caster || caster->GetTypeId() != TYPEID_PLAYER)
        return;

    Camera& camera = ((Player*)caster)->GetCamera();
    if (apply)
        camera.SetView(GetTarget());
    else
        camera.ResetView();
}

void Aura::HandleAuraTrackCreatures(bool apply, bool /*Real*/)
{
    if (GetTarget()->GetTypeId() != TYPEID_PLAYER)
        return;

    /*if (apply)
        GetTarget()->RemoveNoStackAurasDueToAuraHolder(GetHolder());*/

    // Invalidate tracking checking for warden (if one such check is currently
    // pending)
    Unit* caster;
    if ((caster = GetCaster()) && caster->GetTypeId() == TYPEID_PLAYER)
    {
        if (WorldSession* session = static_cast<Player*>(caster)->GetSession())
            session->invalidate_warden_dynamic(WARDEN_DYN_CHECK_TRACKING);
    }

    if (apply)
        GetTarget()->SetFlag(
            PLAYER_TRACK_CREATURES, uint32(1) << (m_modifier.m_miscvalue - 1));
    else
        GetTarget()->RemoveFlag(
            PLAYER_TRACK_CREATURES, uint32(1) << (m_modifier.m_miscvalue - 1));
}

void Aura::HandleAuraTrackResources(bool apply, bool /*Real*/)
{
    if (GetTarget()->GetTypeId() != TYPEID_PLAYER)
        return;

    /*if (apply)
        GetTarget()->RemoveNoStackAurasDueToAuraHolder(GetHolder());*/

    // Invalidate tracking checking for warden (if one such check is currently
    // pending)
    Unit* caster;
    if ((caster = GetCaster()) && caster->GetTypeId() == TYPEID_PLAYER)
    {
        if (WorldSession* session = static_cast<Player*>(caster)->GetSession())
            session->invalidate_warden_dynamic(WARDEN_DYN_CHECK_TRACKING);
    }

    if (apply)
        GetTarget()->SetFlag(
            PLAYER_TRACK_RESOURCES, uint32(1) << (m_modifier.m_miscvalue - 1));
    else
        GetTarget()->RemoveFlag(
            PLAYER_TRACK_RESOURCES, uint32(1) << (m_modifier.m_miscvalue - 1));
}

void Aura::HandleAuraTrackStealthed(bool apply, bool /*Real*/)
{
    if (GetTarget()->GetTypeId() != TYPEID_PLAYER)
        return;

    /*if(apply)
        GetTarget()->RemoveNoStackAurasDueToAuraHolder(GetHolder());*/

    GetTarget()->ApplyModByteFlag(
        PLAYER_FIELD_BYTES, 0, PLAYER_FIELD_BYTE_TRACK_STEALTHED, apply);
}

void Aura::HandleAuraModScale(bool apply, bool Real)
{
    if (!Real)
        return;
    GetTarget()->ApplyPercentModFloatValue(
        OBJECT_FIELD_SCALE_X, float(m_modifier.m_amount), apply);
    GetTarget()->UpdateModelData();
}

void Aura::HandleModPossess(bool apply, bool Real)
{
    if (!Real)
        return;

    Unit* target = GetTarget();

    // not possess yourself
    if (GetCasterGuid() == target->GetObjectGuid())
        return;

    Unit* caster = GetCaster();
    if (!caster || caster->GetTypeId() != TYPEID_PLAYER)
        return;

    if (Pet* pet = caster->GetPet())
        pet->Unsummon(PET_SAVE_AS_CURRENT, caster);

    caster->CharmUnit(
        apply, m_removeMode == AURA_REMOVE_BY_DELETE, target, GetHolder());

    // Priest's mind control: add a lot of threat to caster on expiry NOTE:
    // sources are very vague and inconsistent, the most repeated theory (with
    // most test data) was that it's based on the mob's health
    // This threat is applied "above" your normal threat, as in if a warrior
    // taunts, this threat will be ignored for the threat transfer.
    // NOTE2: This seems to be all such spells, another example is the
    // controller on Razorgore more or less har permanent threat after
    // controlling him.
    if (!apply && target->isAlive())
    {
        target->AddThreat(caster, target->GetHealth(), false,
            SPELL_SCHOOL_MASK_NONE, nullptr, false, true);
    }
}

void Aura::HandleModPossessPet(bool apply, bool Real)
{
    if (!Real)
        return;

    Unit* caster = GetCaster();
    if (!caster || caster->GetTypeId() != TYPEID_PLAYER)
        return;

    Unit* target = GetTarget();
    if (target->GetTypeId() != TYPEID_UNIT || !((Creature*)target)->IsPet())
        return;

    Pet* pet = (Pet*)target;

    Player* p_caster = (Player*)caster;
    Camera& camera = p_caster->GetCamera();

    if (apply)
    {
        pet->addUnitState(UNIT_STAT_CONTROLLED);

        // target should became visible at SetView call(if not visible before):
        // otherwise client\p_caster will ignore packets from the
        // target(SetClientControl for example)
        camera.SetView(pet);

        p_caster->SetCharm(pet);
        p_caster->SetMovingUnit(pet);

        pet->SetCharmerGuid(p_caster->GetObjectGuid());
        pet->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PLAYER_CONTROLLED);
        pet->AttackStop();

        pet->movement_gens.push(
            new movement::ControlledMovementGenerator(p_caster, GetHolder()));
    }
    else
    {
        p_caster->SetCharm(nullptr);
        p_caster->SetMovingUnit(p_caster);

        // there is a possibility that target became invisible for
        // client\p_caster at ResetView call:
        // it must be called after movement control unapplying, not before! the
        // reason is same as at aura applying
        camera.ResetView();

        // on delete only do caster related effects
        if (m_removeMode == AURA_REMOVE_BY_DELETE)
            return;

        pet->SetCharmerGuid(ObjectGuid());
        pet->clearUnitState(UNIT_STAT_CONTROLLED);
        pet->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PLAYER_CONTROLLED);
        pet->AttackStop();

        pet->movement_gens.remove_if([this](const movement::Generator* gen)
            {
                if (auto controlled = dynamic_cast<
                        const movement::ControlledMovementGenerator*>(gen))
                    return controlled->holder() == GetHolder();
                return false;
            });
    }
}

void Aura::HandleModCharm(bool apply, bool Real)
{
    if (!Real)
        return;

    Unit* target = GetTarget();

    // not charm yourself
    if (GetCasterGuid() == target->GetObjectGuid())
        return;

    Unit* caster = GetCaster();
    if (!caster)
        return;

    bool isKarazhanChessPiece = false;
    if (target->GetMapId() == 532)
    {
        uint32 e = target->GetEntry();
        if (e == 21684 || e == 21683 || e == 21682 || e == 21664 ||
            e == 21160 || e == 17211 || e == 21752 || e == 21750 ||
            e == 21747 || e == 21748 || e == 21726 || e == 17469)
            isKarazhanChessPiece = true;
    }

    if (apply)
    {
        // is it really need after spell check checks?
        target->remove_auras(SPELL_AURA_MOD_CHARM, [this](AuraHolder* holder)
            {
                return holder != GetHolder();
            });
        target->remove_auras(SPELL_AURA_MOD_POSSESS, [this](AuraHolder* holder)
            {
                return holder != GetHolder();
            });

        // Remove casters current pet (charm + pet not possible)
        if (Pet* pet = caster->GetPet())
            pet->Unsummon(PET_SAVE_AS_CURRENT, caster);

        target->SetCharmerGuid(GetCasterGuid());
        if (!isKarazhanChessPiece)
            target->setFaction(caster->getFaction());
        target->CastStop(target == caster ? GetId() : 0);
        caster->SetCharm(target);

        if (caster->GetTypeId() == TYPEID_PLAYER)
            target->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PVP_ATTACKABLE);

        if (!isKarazhanChessPiece)
        {
            target->CombatStop(true, true);
            target->getHostileRefManager().setOnlineOfflineState(false);
        }

        if (target->GetTypeId() == TYPEID_UNIT)
        {
            CharmInfo* charmInfo = target->InitCharmInfo(target);
            charmInfo->InitCharmCreateSpells();
            charmInfo->SetReactState(REACT_DEFENSIVE);

            if (caster->GetTypeId() == TYPEID_PLAYER &&
                caster->getClass() == CLASS_WARLOCK &&
                GetSpellProto()->SpellIconID == 1500)
            {
                CreatureInfo const* cinfo =
                    ((Creature*)target)->GetCreatureInfo();
                if (cinfo && cinfo->type == CREATURE_TYPE_DEMON)
                {
                    // creature with pet number expected have class set
                    if (target->GetByteValue(UNIT_FIELD_BYTES_0, 1) == 0)
                    {
                        if (cinfo->unit_class == 0)
                            logging.error(
                                "Creature (Entry: %u) have unit_class = 0 but "
                                "used in charmed spell, that will be result "
                                "client crash.",
                                cinfo->Entry);
                        else
                            logging.error(
                                "Creature (Entry: %u) have unit_class = %u but "
                                "at charming have class 0!!! that will be "
                                "result client crash.",
                                cinfo->Entry, cinfo->unit_class);

                        target->SetByteValue(UNIT_FIELD_BYTES_0, 1, CLASS_MAGE);
                    }

                    // just to enable stat window
                    charmInfo->SetPetNumber(
                        sObjectMgr::Instance()->GeneratePetNumber(), true);
                    // if charmed two demons the same session, the 2nd gets the
                    // 1st one's name
                    target->SetUInt32Value(UNIT_FIELD_PET_NAME_TIMESTAMP,
                        uint32(WorldTimer::time_no_syscall()));
                }
            }

            target->CombatStop(true);
            target->DeleteThreatList();
            static_cast<Creature*>(target)->install_pet_behavior();
        }

        if (target->GetTypeId() == TYPEID_PLAYER)
        {
            // Only use controlled move gen for player targets
            target->movement_gens.push(
                new movement::ControlledMovementGenerator(caster, GetHolder()));

            Player* pPlayer = (Player*)target;
            pPlayer->AIM_Initialize();

            target->CallForAllControlledUnits(
                [caster](Unit* pet)
                {
                    pet->CombatStop(true);
                    pet->getHostileRefManager().deleteReferences();
                    pet->setFaction(caster->getFaction());
                },
                CONTROLLED_PET | CONTROLLED_GUARDIANS | CONTROLLED_CHARM |
                    CONTROLLED_TOTEMS);
        }

        if (caster->GetTypeId() == TYPEID_PLAYER)
            ((Player*)caster)->CharmSpellInitialize();

        caster->AddCharm(target);
    }
    else
    {
        target->SetCharmerGuid(ObjectGuid());

        if (target->GetTypeId() == TYPEID_PLAYER)
            ((Player*)target)->setFactionForRace(target->getRace());
        else
        {
            CreatureInfo const* cinfo = ((Creature*)target)->GetCreatureInfo();

            // restore faction
            if (((Creature*)target)->IsPet())
            {
                if (Unit* owner = target->GetOwner())
                    target->setFaction(owner->getFaction());
                else if (cinfo)
                    target->setFaction(cinfo->faction_A);
            }
            else if (cinfo) // normal creature
                if (!isKarazhanChessPiece)
                    target->setFaction(cinfo->faction_A);

            // restore UNIT_FIELD_BYTES_0
            if (cinfo && caster->GetTypeId() == TYPEID_PLAYER &&
                caster->getClass() == CLASS_WARLOCK &&
                cinfo->type == CREATURE_TYPE_DEMON &&
                GetSpellProto()->SpellIconID == 1500)
            {
                // DB must have proper class set in field at loading, not req.
                // restore, including workaround case at apply
                // m_target->SetByteValue(UNIT_FIELD_BYTES_0, 1,
                // cinfo->unit_class);

                if (target->GetCharmInfo())
                    target->GetCharmInfo()->SetPetNumber(0, true);
                else
                    logging.error(
                        "Aura::HandleModCharm: target (GUID: %u TypeId: %u) "
                        "has a charm aura but no charm info!",
                        target->GetGUIDLow(), target->GetTypeId());
            }
        }

        if (target->GetTypeId() == TYPEID_PLAYER)
        {
            // Only use controlled move gen for player targets
            target->movement_gens.remove_if(
                [this](const movement::Generator* gen)
                {
                    if (auto controlled = dynamic_cast<
                            const movement::ControlledMovementGenerator*>(gen))
                        return controlled->holder() == GetHolder();
                    return false;
                });

            Player* pPlayer = (Player*)target;
            pPlayer->AIM_Deinitialize();

            uint32 fac = pPlayer->getFactionForRace(pPlayer->getRace());
            target->CallForAllControlledUnits(
                [fac](Unit* pet)
                {
                    pet->CombatStop(true);
                    pet->getHostileRefManager().deleteReferences();
                    pet->setFaction(fac);
                },
                CONTROLLED_PET | CONTROLLED_GUARDIANS | CONTROLLED_CHARM |
                    CONTROLLED_TOTEMS);
        }

        if (caster->GetTypeId() == TYPEID_PLAYER)
            target->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PVP_ATTACKABLE);

        caster->SetCharm(nullptr);

        if (caster->GetTypeId() == TYPEID_PLAYER)
            ((Player*)caster)->RemovePetActionBar();

        // HACK: Blackheart's Incite Chaos: drop all hostile references but
        // Blackheart (this isn't fully how it should work!)
        if (GetSpellProto()->Id == 33684)
        {
            target->InterruptNonMeleeSpells(false);
            target->AttackStop();
            std::vector<Unit*> remove_for;
            for (auto& elem : target->getHostileRefManager())
            {
                auto unit = elem.getSource()->getOwner();
                if (unit->GetTypeId() != TYPEID_UNIT ||
                    unit->GetEntry() != 18667)
                    remove_for.push_back(unit);
            }
            for (auto unit : remove_for)
            {
                if (target->getAttackers().find(unit) !=
                    target->getAttackers().end())
                    unit->AttackStop();
                target->getHostileRefManager().deleteReference(unit);
            }
        }
        else if (!isKarazhanChessPiece)
        {
            target->CombatStop(true, true);
            target->getHostileRefManager().setOnlineOfflineState(false);

            // delete all friendly targets on charm over
            if (target->GetTypeId() ==
                TYPEID_PLAYER) // use friendly check for players
            {
                target->getHostileRefManager().deleteReferencesCallback(
                    [target](Unit* other)
                    {
                        return target->IsFriendlyTo(other);
                    });
            }
            else // and not-hostile for NPCs
            {
                target->getHostileRefManager().deleteReferencesCallback(
                    [target](Unit* other)
                    {
                        return !target->IsHostileTo(other);
                    });
            }
        }

        if (target->GetTypeId() == TYPEID_UNIT)
        {
            if (!isKarazhanChessPiece)
            {
                // Reset pet behaviors next tick, this could be invoked from
                // within the AI we might destroy. TODO: restructure & clean up
                auto guid = caster->GetObjectGuid();
                target->queue_action_ticks(1, [target, guid]()
                    {
                        static_cast<Creature*>(target)
                            ->uninstall_pet_behavior();
                        if (auto caster = target->GetMap()->GetUnit(guid))
                            target->AttackedBy(caster);

                        // Restore home gen that was made inactive in
                        // install_pet_behavior()
                        if (auto home =
                                target->movement_gens.get(movement::gen::home))
                        {
                            if (home->priority() == -10)
                                target->movement_gens.mod_priority(
                                    home, movement::get_default_priority(
                                              movement::gen::home));
                        }
                    });
            }

            // Remove Soul Link (works on enslaved demons since Patch 2.1)
            target->remove_auras(25228);
        }

        caster->RemoveCharm(target);

        target->DeleteCharmInfo();
    }
}

void Aura::HandleModConfuse(bool apply, bool Real)
{
    if (!Real)
        return;

    Unit* target = GetTarget();

    if (apply && GetSpellProto())
        target->InformPetsAboutCC(GetSpellProto());

    if (target->GetTypeId() == TYPEID_UNIT)
        ((Creature*)target)->SetKeepTargetEmptyDueToCC(apply);

    if (apply)
    {
        target->movement_gens.push(
            new movement::ConfusedMovementGenerator(GetHolder()));

        target->CastStop(target->GetObjectGuid() == GetCasterGuid() ?
                             GetSpellProto()->Id :
                             0);
    }
    else
    {
        target->movement_gens.remove_if([this](const movement::Generator* gen)
            {
                if (auto confused = dynamic_cast<
                        const movement::ConfusedMovementGenerator*>(gen))
                    return confused->holder() == GetHolder();
                return false;
            });
    }
}

void Aura::HandleModFear(bool apply, bool Real)
{
    if (!Real)
        return;

    Unit* target = GetTarget();

    // Prevent fear movement generator to start if a SPELL_AURA_PREVENTS_FLEEING
    // is active
    if (apply)
    {
        if (target->HasAuraType(SPELL_AURA_PREVENTS_FLEEING))
            return;

        target->CastStop(target->GetObjectGuid() == GetCasterGuid() ?
                             GetSpellProto()->Id :
                             0);
    }

    if (apply)
    {
        target->movement_gens.push(new movement::FleeingMovementGenerator(
            GetSpellProto()->AttributesCustom & SPELL_ATTR_CUSTOM_PANIC_FEAR ?
                ObjectGuid() :
                GetCasterGuid(),
            GetHolder()));
    }
    else
    {
        target->movement_gens.remove_if([this](const movement::Generator* gen)
            {
                if (auto fleeing =
                        dynamic_cast<const movement::FleeingMovementGenerator*>(
                            gen))
                    return fleeing->holder() == GetHolder();
                return false;
            });
    }
}

static bool feign_death_resist(Unit* me, Unit* him)
{
    // Chance to resist Feign Death
    // (http://wowpedia.org/Feign_Death#Chance_to_resist)
    // TODO: It's probably the case that spell hit rating should affect this,
    // needs more research

    int his_level = him->getLevel();
    int level_diff = his_level - static_cast<int>(me->getLevel());

    float resist_chance = 0;
    switch (level_diff)
    {
    case 0:
        resist_chance = 4;
        break;
    case 1:
        resist_chance = 5;
        break;
    case 2:
        resist_chance = 6;
        break;
    case 3:
        resist_chance = 17;
        break;
    case 4:
        resist_chance = 28;
        break;
    case 5:
        resist_chance = 39;
        break;
    case 6:
        resist_chance = 50;
        break;
    default:
        if (level_diff < 0)
            resist_chance = 1;
        else
            resist_chance = 50;
    }

    // Survival talent Improved Feign Death
    if (me->has_aura(19287))
        resist_chance -= 4;
    else if (me->has_aura(19286))
        resist_chance -= 2;
    // Demon Stalker
    if (me->has_aura(37484))
        resist_chance -= 5;

    if (resist_chance > 0)
    {
        resist_chance /= 100;
        if (rand_norm_f() < resist_chance)
            return true;
    }

    return false;
}

void Aura::HandleFeignDeath(bool apply, bool Real)
{
    if (!Real)
        return;

    Unit* target = GetTarget();

    /* How feign death works (based on retail testing):
       - Solo:      If you're solo, mobs evade (including bosses)
       - Group:     In group you leave combat, except for raid bosses, where you
       remain in combat with the boss
       - Bosses:    If everyone else in your group dies, even if you're in
                    combat with them, the remaining mobs will evade if you're
       still in FD
       - Threat:    All threat is fully lost when you evade
       - Resist:    Mobs (not players or player pets) can resist your FD, this
       means they will not drop threat or
                    aggro on you. Other mobs that did not resist it will evade
       from you, but will most likely
                    be reaggrod by combat pulsing --- you will still have
       dropped threat against them, though.
                    Reistance is based on a flat table, and besides the survival
       talent and demon stalker not
                    affected by hit. (TODO: This one is the only one not based
       on retail testing, and sources
                    are not 100% in agreeance, it's possible resistance should
       make the spell never go off, or
                    that it should affect all mobs, WTB better sources.)
    */

    if (apply)
    {
        bool clear_combat = true;
        std::vector<Unit*> succeeded;

        target->AttackStop();

        // Make pets stop attacking us
        target->InformPetsAboutCC();

        // Test resistance against all mobs
        for (auto itr = target->getHostileRefManager().begin();
             itr != target->getHostileRefManager().end(); ++itr)
        {
            Unit* hostile = itr->getSource()->getOwner();
            if (!hostile)
                continue;

            // Players cannot resist
            if (hostile->player_or_pet())
            {
                if (target->getAttackers().find(hostile) !=
                    target->getAttackers().end())
                    succeeded.push_back(target);
                continue;
            }

            if (feign_death_resist(target, hostile))
            {
                WorldPacket data(SMSG_FEIGN_DEATH_RESISTED, 16);
                data << hostile->GetObjectGuid();
                data << uint8(1);
                target->SendMessageToSet(&data, true);
                clear_combat = false;
                continue;
            }

            succeeded.push_back(hostile);
        }

        // === begin: Interrupt all spells on us ===
        // Select nearby units
        float dist = target->GetMap()->GetVisibilityDistance();
        auto vec = maps::visitors::yield_set<Unit, Player, Creature, Pet,
            SpecialVisCreature, TemporarySummon>{}(target, dist, [](auto&& elem)
            {
                return elem->isAlive();
            });
        // Interrupt spells they're casting on us if they're hostile
        for (auto u : vec)
        {
            if (u->IsHostileTo(target))
            {
                // Don't interrupt on creature that resisted our feign death
                if (!u->player_or_pet() &&
                    std::find(succeeded.begin(), succeeded.end(), u) ==
                        succeeded.end())
                    continue;
                u->InterruptSpellOn(target);
            }
        }
        // === end: Interrupt all spells on us ===

        // Clear combat with all targets that didn't resist
        for (auto u : succeeded)
        {
            if (target->getAttackers().find(u) != target->getAttackers().end())
                u->AttackStop();

            // Don't clear hostile reference against bosses, instead mark them
            // as ignored (Patch 2.3.0 undocumented changes @ wowwiki)
            if (u->GetTypeId() == TYPEID_UNIT &&
                static_cast<Creature*>(u)->GetCreatureInfo()->rank ==
                    CREATURE_ELITE_WORLDBOSS)
            {
                // We need to reset threat to 0 for bosses
                u->getThreatManager().modifyThreatPercent(target, -100);
                target->getHostileRefManager().setIgnoredState(u, true);
                clear_combat = false;
            }
            else
            {
                target->getHostileRefManager().deleteReference(u);
            }
        }

        if (clear_combat)
            target->ClearInCombat();
    }
    else
    {
        // Clear any ignored states we set on application
        for (auto itr = target->getHostileRefManager().begin();
             itr != target->getHostileRefManager().end(); ++itr)
            if (Unit* hostile = itr->getSource()->getOwner())
                target->getHostileRefManager().setIgnoredState(hostile, false);
    }

    target->SetFeignDeath(apply, GetCasterGuid(), GetId());
}

void Aura::HandleAuraModDisarm(bool apply, bool Real)
{
    if (!Real)
        return;

    Unit* target = GetTarget();

    if (!apply && target->HasAuraType(GetModifier()->m_auraname))
        return;

    target->ApplyModFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_DISARMED, apply);

    if (target->GetTypeId() != TYPEID_PLAYER)
        return;

    // main-hand attack speed already set to special value for feral form
    // already and don't must change and reset at remove.
    if (target->IsInFeralForm())
        return;

    if (apply)
        target->SetAttackTime(BASE_ATTACK, BASE_ATTACK_TIME);
    else
        ((Player*)target)->SetRegularAttackTime();

    target->UpdateDamagePhysical(BASE_ATTACK);
}

void Aura::HandleAuraModStun(bool apply, bool Real)
{
    if (!Real)
        return;

    Unit* target = GetTarget();
    const SpellEntry* spellInfo = GetSpellProto();
    if (!spellInfo)
        return;

    if (apply)
    {
        target->InformPetsAboutCC(spellInfo);

        // Frost stun aura -> freeze/unfreeze target
        if (GetSpellSchoolMask(spellInfo) & SPELL_SCHOOL_MASK_FROST)
            target->ModifyAuraState(AURA_STATE_FROZEN, apply);

        if (target->GetTypeId() != TYPEID_PLAYER)
            target->SetTargetGuid(ObjectGuid());

        target->CastStop(
            target->GetObjectGuid() == GetCasterGuid() ? GetId() : 0);

        // Creature specific
        if (target->GetTypeId() != TYPEID_PLAYER)
        {
            if (auto grp = static_cast<Creature*>(target)->GetGroup())
            {
                if (!target->isInCombat())
                    target->GetMap()->GetCreatureGroupMgr().ProcessGroupEvent(
                        grp->GetId(), CREATURE_GROUP_EVENT_MOVEMENT_PAUSE);
            }
        }
        else
        {
            target->SetStandState(UNIT_STAND_STATE_STAND); // in 1.5 client

            // Second Wind (stun)
            if (target->getClass() == CLASS_WARRIOR &&
                GetAllSpellMechanicMask(spellInfo) & (1 << (MECHANIC_STUN - 1)))
            {
                if (target->has_aura(29838))
                    target->CastSpell(target, 29842, true);
                else if (target->has_aura(29834))
                    target->CastSpell(target, 29841, true);
            }
        }

        if (spellInfo->Mechanic == MECHANIC_SAPPED)
            target->addUnitState(UNIT_STAT_SAPPED);

        target->movement_gens.push(
            new movement::StunMovementGenerator(GetHolder()));

        // Do a facing spline once rooted, to make sure the client knows our
        // current orientation
        if (target->GetTypeId() == TYPEID_UNIT)
            target->SetFacingTo(GetHolder()->GetBeforeStunFacing());

        // Summon the Naj'entus Spine GameObject on target if spell is Impaling
        // Spine
        if (GetId() == 39837)
        {
            auto pObj = new GameObject;
            if (pObj->Create(
                    target->GetMap()->GenerateLocalLowGuid(HIGHGUID_GAMEOBJECT),
                    185584, target->GetMap(), target->GetX(), target->GetY(),
                    target->GetZ(), target->GetO()))
            {
                pObj->SetRespawnTime(GetAuraDuration() / IN_MILLISECONDS);
                pObj->SetSpellId(GetId());
                target->AddGameObject(pObj);
                target->GetMap()->insert(pObj);
            }
            else
                delete pObj;
        }
    }
    else
    {
        // Frost stun aura -> freeze/unfreeze target
        if (GetSpellSchoolMask(spellInfo) & SPELL_SCHOOL_MASK_FROST)
        {
            bool found_another = false;
            for (AuraType const* itr = &frozenAuraTypes[0];
                 *itr != SPELL_AURA_NONE; ++itr)
            {
                auto& auras = target->GetAurasByType(*itr);
                for (const auto& aura : auras)
                {
                    if (GetSpellSchoolMask((aura)->GetSpellProto()) &
                        SPELL_SCHOOL_MASK_FROST)
                    {
                        found_another = true;
                        break;
                    }
                }
                if (found_another)
                    break;
            }

            if (!found_another)
                target->ModifyAuraState(AURA_STATE_FROZEN, apply);
        }

        if (spellInfo->Mechanic == MECHANIC_SAPPED)
            target->clearUnitState(UNIT_STAT_SAPPED);

        target->movement_gens.remove_if([this](const movement::Generator* gen)
            {
                if (auto stun =
                        dynamic_cast<const movement::StunMovementGenerator*>(
                            gen))
                    return stun->holder() == GetHolder();
                return false;
            });

        // Real remove called after current aura remove from lists, check if
        // other similar auras active
        if (target->HasAuraType(SPELL_AURA_MOD_STUN))
            return;

        if (target->GetTypeId() != TYPEID_PLAYER)
        {
            if (auto grp = static_cast<Creature*>(target)->GetGroup())
            {
                if (!target->isInCombat())
                    target->GetMap()->GetCreatureGroupMgr().ProcessGroupEvent(
                        grp->GetId(), CREATURE_GROUP_EVENT_MOVEMENT_RESUME);
            }
        }

        // Wyvern Sting
        if (spellInfo->SpellFamilyName == SPELLFAMILY_HUNTER &&
            GetSpellProto()->SpellFamilyFlags & UI64LIT(0x0000100000000000))
        {
            Unit* caster = GetCaster();
            if (!caster || caster->GetTypeId() != TYPEID_PLAYER)
                return;

            uint32 spell_id = 0;

            switch (GetId())
            {
            case 19386:
                spell_id = 24131;
                break;
            case 24132:
                spell_id = 24134;
                break;
            case 24133:
                spell_id = 24135;
                break;
            case 27068:
                spell_id = 27069;
                break;
            default:
                logging.error(
                    "Spell selection called for unexpected original spell %u, "
                    "new spell for this spell family?",
                    GetId());
                return;
            }

            SpellEntry const* spellInfo = sSpellStore.LookupEntry(spell_id);

            if (!spellInfo)
                return;

            caster->CastSpell(target, spellInfo, true, nullptr, this);
            return;
        }

        // NPC Wyvern Stings
        switch (GetId())
        {
        case 24335:
            if (Unit* caster = GetCaster())
                caster->CastSpell(target, 24336, true, nullptr, this);
            break;
        }
    }
}

void Aura::HandleModStealth(bool apply, bool Real)
{
    Unit* target = GetTarget();

    if (apply)
    {
        // drop flag at stealth in bg
        target->remove_auras_if([](AuraHolder* h)
            {
                return h->GetSpellProto()->AuraInterruptFlags &
                       AURA_INTERRUPT_FLAG_IMMUNE_OR_LOST_SELECTION;
            });

        // only at real aura add
        if (Real)
        {
            target->SetStandFlags(UNIT_STAND_FLAGS_CREEP);

            if (target->GetTypeId() == TYPEID_PLAYER)
                target->SetByteFlag(
                    PLAYER_FIELD_BYTES2, 1, PLAYER_FIELD_BYTE2_STEALTH);

            // apply only if not in GM invisibility (and overwrite invisibility
            // state)
            if (target->GetVisibility() != VISIBILITY_OFF)
            {
                target->SetVisibility(VISIBILITY_GROUP_STEALTH);
                target->force_stealth_update_timer(urand(400, 600));
            }

            // for RACE_NIGHTELF stealth
            if (target->GetTypeId() == TYPEID_PLAYER && GetId() == 20580)
                target->CastSpell(target, 21009, true, nullptr, this);

            // apply full stealth period bonuses only at first stealth aura in
            // stack
            if (target->GetAurasByType(SPELL_AURA_MOD_STEALTH).size() <= 1)
            {
                auto& dummyAuras = target->GetAurasByType(SPELL_AURA_DUMMY);
                for (const auto& dummyAura : dummyAuras)
                {
                    // Master of Subtlety
                    if ((dummyAura)->GetSpellProto()->SpellIconID == 2114)
                    {
                        target->remove_auras(31666);
                        int32 bp = (dummyAura)->GetModifier()->m_amount;
                        target->CastCustomSpell(
                            target, 31665, &bp, nullptr, nullptr, true);
                        break;
                    }
                }
            }
        }
    }
    else
    {
        // for RACE_NIGHTELF stealth
        if (Real && target->GetTypeId() == TYPEID_PLAYER && GetId() == 20580)
            target->remove_auras(21009);

        // Patch 1.11: "Canceling your Stealth aura while Vanish is running will
        // now cause Vanish to be canceled as well."
        if (Real && GetSpellProto()->SpellFamilyName == SPELLFAMILY_ROGUE &&
            GetSpellProto()->SpellFamilyFlags & 0x400000)
            target->remove_auras_if([](AuraHolder* h)
                {
                    return h->GetSpellProto()->SpellFamilyName ==
                               SPELLFAMILY_ROGUE &&
                           h->GetSpellProto()->SpellFamilyFlags & 0x800;
                });

        // only at real aura remove of _last_ SPELL_AURA_MOD_STEALTH
        if (Real && !target->HasAuraType(SPELL_AURA_MOD_STEALTH))
        {
            // if no GM invisibility
            if (target->GetVisibility() != VISIBILITY_OFF)
            {
                target->RemoveStandFlags(UNIT_STAND_FLAGS_CREEP);

                if (target->GetTypeId() == TYPEID_PLAYER)
                    target->RemoveByteFlag(
                        PLAYER_FIELD_BYTES2, 1, PLAYER_FIELD_BYTE2_STEALTH);

                // restore invisibility if any
                if (target->HasAuraType(SPELL_AURA_MOD_INVISIBILITY))
                {
                    target->SetVisibility(VISIBILITY_GROUP_NO_DETECT);
                    target->SetVisibility(VISIBILITY_GROUP_INVISIBILITY);
                }
                else
                    target->SetVisibility(VISIBILITY_ON);

                target->force_stealth_update_timer(urand(400, 600));
            }

            // apply delayed talent bonus remover at last stealth aura remove
            auto& dummyAuras = target->GetAurasByType(SPELL_AURA_DUMMY);
            for (const auto& dummyAura : dummyAuras)
            {
                // Master of Subtlety
                if ((dummyAura)->GetSpellProto()->SpellIconID == 2114)
                {
                    target->CastSpell(target, 31666, true);
                    break;
                }
            }
        }
    }
}

void Aura::HandleInvisibility(bool apply, bool Real)
{
    Unit* target = GetTarget();

    if (apply)
    {
        target->m_invisibilityMask |= (1 << m_modifier.m_miscvalue);

        target->remove_auras_if([this](AuraHolder* h)
            {
                return (h->GetSpellProto()->AuraInterruptFlags &
                           AURA_INTERRUPT_FLAG_IMMUNE_OR_LOST_SELECTION) &&
                       GetHolder() != h;
            });

        if (Real && target->GetTypeId() == TYPEID_PLAYER)
        {
            // apply glow vision
            target->SetByteFlag(
                PLAYER_FIELD_BYTES2, 1, PLAYER_FIELD_BYTE2_INVISIBILITY_GLOW);
        }

        // apply only if not in GM invisibility and not stealth
        if (target->GetVisibility() == VISIBILITY_ON)
        {
            // Aura not added yet but visibility code expect temporary add aura
            target->SetVisibility(VISIBILITY_GROUP_NO_DETECT);
            target->SetVisibility(VISIBILITY_GROUP_INVISIBILITY);
        }
    }
    else
    {
        // recalculate value at modifier remove (current aura already removed)
        target->m_invisibilityMask = 0;
        auto& auras = target->GetAurasByType(SPELL_AURA_MOD_INVISIBILITY);
        for (const auto& aura : auras)
            target->m_invisibilityMask |=
                (1 << (aura)->GetModifier()->m_miscvalue);

        // only at real aura remove and if not have different invisibility
        // auras.
        if (Real && target->m_invisibilityMask == 0)
        {
            // remove glow vision
            if (target->GetTypeId() == TYPEID_PLAYER)
                target->RemoveByteFlag(PLAYER_FIELD_BYTES2, 1,
                    PLAYER_FIELD_BYTE2_INVISIBILITY_GLOW);

            // apply only if not in GM invisibility & not stealthed while
            // invisible
            if (target->GetVisibility() != VISIBILITY_OFF)
            {
                // if have stealth aura then already have stealth visibility
                if (!target->HasAuraType(SPELL_AURA_MOD_STEALTH))
                    target->SetVisibility(VISIBILITY_ON);
            }
        }
    }
}

void Aura::HandleInvisibilityDetect(bool apply, bool Real)
{
    Unit* target = GetTarget();

    if (apply)
    {
        target->m_detectInvisibilityMask |= (1 << m_modifier.m_miscvalue);
    }
    else
    {
        // recalculate value at modifier remove (current aura already removed)
        target->m_detectInvisibilityMask = 0;
        auto& auras =
            target->GetAurasByType(SPELL_AURA_MOD_INVISIBILITY_DETECTION);
        for (const auto& aura : auras)
            target->m_detectInvisibilityMask |=
                (1 << (aura)->GetModifier()->m_miscvalue);
    }

    if (Real && target->GetTypeId() == TYPEID_PLAYER && target->IsInWorld())
    {
        ((Player*)target)->GetCamera().UpdateVisibilityForOwner();

        // Select any invisible players nearby and update their visibility too
        maps::visitors::simple<Player>()(target,
            target->GetMap()->GetVisibilityDistance(), [target](Player* p)
            {
                if (p != target && p->HasAuraType(SPELL_AURA_MOD_INVISIBILITY))
                    p->GetCamera().UpdateVisibilityForOwner();
            });
    }
}

void Aura::HandleDetectAmore(bool apply, bool /*real*/)
{
    GetTarget()->ApplyModByteFlag(PLAYER_FIELD_BYTES2, 1,
        (PLAYER_FIELD_BYTE2_DETECT_AMORE_0 << m_modifier.m_amount), apply);
}

void Aura::HandleAuraModRoot(bool apply, bool Real)
{
    // only at real add/remove aura
    if (!Real)
        return;

    auto spellInfo = GetSpellProto();

    Unit* target = GetTarget();

    if (apply)
    {
        // Frost root aura -> freeze/unfreeze target
        if (GetSpellSchoolMask(spellInfo) & SPELL_SCHOOL_MASK_FROST)
            target->ModifyAuraState(AURA_STATE_FROZEN, apply);

        if (target->GetTypeId() == TYPEID_UNIT)
        {
            target->StopMoving();
        }
        else
        {
            // Second Wind (root)
            if (target->getClass() == CLASS_WARRIOR &&
                GetAllSpellMechanicMask(spellInfo) & (1 << (MECHANIC_ROOT - 1)))
            {
                if (target->has_aura(29838))
                    target->CastSpell(target, 29842, true);
                else if (target->has_aura(29834))
                    target->CastSpell(target, 29841, true);
            }
        }

        target->movement_gens.push(
            new movement::RootMovementGenerator(GetHolder()));
    }
    else
    {
        // Frost root aura -> freeze/unfreeze target
        if (GetSpellSchoolMask(spellInfo) & SPELL_SCHOOL_MASK_FROST)
        {
            bool found_another = false;
            for (AuraType const* itr = &frozenAuraTypes[0];
                 *itr != SPELL_AURA_NONE; ++itr)
            {
                auto& auras = target->GetAurasByType(*itr);
                for (const auto& aura : auras)
                {
                    if (GetSpellSchoolMask((aura)->GetSpellProto()) &
                        SPELL_SCHOOL_MASK_FROST)
                    {
                        found_another = true;
                        break;
                    }
                }
                if (found_another)
                    break;
            }

            if (!found_another)
                target->ModifyAuraState(AURA_STATE_FROZEN, apply);
        }

        target->movement_gens.remove_if([this](const movement::Generator* gen)
            {
                if (auto root =
                        dynamic_cast<const movement::RootMovementGenerator*>(
                            gen))
                    return root->holder() == GetHolder();
                return false;
            });
    }
}

void Aura::HandleAuraModSilence(bool apply, bool Real)
{
    // only at real add/remove aura
    if (!Real)
        return;

    Unit* target = GetTarget();

    if (apply)
    {
        target->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_SILENCED);
        // Stop cast only spells vs PreventionType ==
        // SPELL_PREVENTION_TYPE_SILENCE
        for (uint32 i = CURRENT_MELEE_SPELL; i < CURRENT_MAX_SPELL; ++i)
            if (Spell* spell = target->GetCurrentSpell(CurrentSpellTypes(i)))
                if (spell->m_spellInfo->PreventionType ==
                    SPELL_PREVENTION_TYPE_SILENCE)
                    // Stop spells on prepare or casting state
                    target->InterruptSpell(CurrentSpellTypes(i), false);

        switch (GetId())
        {
        // Arcane Torrent (Energy)
        case 25046:
        {
            Unit* caster = GetCaster();
            if (!caster)
                return;

            // Search Mana Tap auras on caster
            if (AuraHolder* holder = caster->get_aura(28734))
            {
                int32 bp = holder->GetStackAmount() * 10;
                caster->CastCustomSpell(
                    caster, 25048, &bp, nullptr, nullptr, true);
                caster->remove_auras(28734);
            }
        }
        }
    }
    else
    {
        // Real remove called after current aura remove from lists, check if
        // other similar auras active
        if (target->HasAuraType(SPELL_AURA_MOD_SILENCE))
            return;

        target->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_SILENCED);
    }
}

void Aura::HandleModThreat(bool apply, bool Real)
{
    // only at real add/remove aura
    if (!Real)
        return;

    Unit* target = GetTarget();

    if (!target->isAlive())
        return;

    int level_diff = 0;
    int multiplier = 0;
    switch (GetId())
    {
    // Arcane Shroud
    case 26400:
        level_diff = target->getLevel() - 60;
        multiplier = 2;
        break;
    // The Eye of Diminution
    case 28862:
        level_diff = target->getLevel() - 60;
        multiplier = 1;
        break;
    }

    if (level_diff > 0)
        m_modifier.m_amount += multiplier * level_diff;

    if (target->GetTypeId() == TYPEID_PLAYER)
        for (int8 x = 0; x < MAX_SPELL_SCHOOL; x++)
            if (m_modifier.m_miscvalue & int32(1 << x))
                ApplyPercentModFloatVar(target->m_threatModifier[x],
                    float(m_modifier.m_amount), apply);
}

void Aura::HandleAuraModTotalThreat(bool apply, bool Real)
{
    // only at real add/remove aura
    if (!Real)
        return;

    Unit* target = GetTarget();

    if (!target->isAlive() || target->GetTypeId() != TYPEID_PLAYER)
        return;

    Unit* caster = GetCaster();

    if (!caster || !caster->isAlive())
        return;

    float threatMod =
        apply ? float(m_modifier.m_amount) : float(-m_modifier.m_amount);

    target->getHostileRefManager().modThreat(caster, threatMod, apply);
}

void Aura::HandleModTaunt(bool apply, bool Real)
{
    // only at real add/remove aura
    if (!Real)
        return;

    Unit* target = GetTarget();
    bool is_pet = target->GetTypeId() != TYPEID_PLAYER &&
                  static_cast<Creature*>(target)->IsPlayerPet();

    if (!target->isAlive() || (!target->CanHaveThreatList() && !is_pet))
        return;

    Unit* caster = GetCaster();

    if (!caster || !caster->isAlive())
        return;

    if (is_pet)
    {
        if (!target->IsAffectedByThreatIgnoringCC() && apply &&
            static_cast<Creature*>(target)->behavior())
            static_cast<Creature*>(target)->behavior()->attempt_attack(caster);
    }
    else
    {
        // HandleModTaunt just forces the target to attack us for the duration
        // of the spell
        if (apply)
        {
            // Force the target to attack us
            target->TauntApply(caster);
        }
        else
        {
            // When the aura fades out we will no longer force the target to
            // attack us
            target->TauntFadeOut(caster);
        }
    }
}

/*********************************************************/
/***                  MODIFY SPEED                     ***/
/*********************************************************/
void Aura::HandleAuraModIncreaseSpeed(bool /*apply*/, bool Real)
{
    // all applied/removed only at real aura add/remove
    if (!Real)
        return;

    GetTarget()->UpdateSpeed(MOVE_RUN, true);
}

void Aura::HandleAuraModIncreaseMountedSpeed(bool /*apply*/, bool Real)
{
    // all applied/removed only at real aura add/remove
    if (!Real)
        return;

    GetTarget()->UpdateSpeed(MOVE_RUN, true);
}

void Aura::HandleAuraModIncreaseFlightSpeed(bool apply, bool Real)
{
    // all applied/removed only at real aura add/remove
    if (!Real)
        return;

    Unit* target = GetTarget();

    // Enable Fly mode for flying mounts
    if (m_modifier.m_auraname == SPELL_AURA_MOD_FLIGHT_SPEED_MOUNTED)
    {
        if (target->GetTypeId() == TYPEID_PLAYER)
        {
            WorldPacket data;
            if (apply)
                data.initialize(SMSG_MOVE_SET_CAN_FLY, 12);
            else
                data.initialize(SMSG_MOVE_UNSET_CAN_FLY, 12);
            data << target->GetPackGUID();
            data << uint32(0); // unknown
            static_cast<Player*>(target)->SendDirectMessage(std::move(data));
        }

        // Dragonmaw Illusion (overwrite mount model, mounted aura already
        // applied)
        if (apply && target->has_aura(42016) && target->GetMountID())
            target->SetUInt32Value(UNIT_FIELD_MOUNTDISPLAYID, 16314);
    }

    target->UpdateSpeed(MOVE_FLIGHT, true);
}

void Aura::HandleAuraModIncreaseSwimSpeed(bool /*apply*/, bool Real)
{
    // all applied/removed only at real aura add/remove
    if (!Real)
        return;

    GetTarget()->UpdateSpeed(MOVE_SWIM, true);
}

void Aura::HandleAuraModDecreaseSpeed(bool apply, bool Real)
{
    // all applied/removed only at real aura add/remove
    if (!Real)
        return;

    Unit* target = GetTarget();

    if (apply)
    {
        // Gronn Lord's Grasp, becomes stoned
        if (GetId() == 33572)
        {
            if (GetStackAmount() >= 5 && !target->has_aura(33652))
                target->CastSpell(target, 33652, true);
        }
    }
    else
    {
        // Impending Coma
        if (GetId() == 34800)
        {
            // Apply Sleep
            if (GetAuraDuration() ==
                0) // Only if faded due to time, and not dispel or the likes
                target->CastSpell(target, 34801, true);
        }
    }

    target->UpdateSpeed(MOVE_RUN, true);
    target->UpdateSpeed(MOVE_SWIM, true);
    target->UpdateSpeed(MOVE_FLIGHT, true);
}

void Aura::HandleAuraModUseNormalSpeed(bool /*apply*/, bool Real)
{
    // all applied/removed only at real aura add/remove
    if (!Real)
        return;

    Unit* target = GetTarget();

    target->UpdateSpeed(MOVE_RUN, true);
    target->UpdateSpeed(MOVE_SWIM, true);
    target->UpdateSpeed(MOVE_FLIGHT, true);
}

/*********************************************************/
/***                     IMMUNITY                      ***/
/*********************************************************/

void Aura::HandleModMechanicImmunity(bool apply, bool Real)
{
    uint32 misc = m_modifier.m_miscvalue;
    Unit* target = GetTarget();

    if (apply &&
        GetSpellProto()->HasAttribute(SPELL_ATTR_EX_DISPEL_AURAS_ON_IMMUNITY))
    {
        uint32 mechanic = 1 << (misc - 1);

        // immune movement impairment and loss of control (spell data have
        // special structure for mark this case)
        bool pvp_trinket = false;
        if (IsSpellRemoveAllMovementAndControlLossEffects(GetSpellProto()))
        {
            mechanic = IMMUNE_TO_MOVEMENT_IMPAIRMENT_AND_LOSS_CONTROL_MASK;
            pvp_trinket = true;
        }

        target->remove_auras_if([mechanic, pvp_trinket](AuraHolder* holder)
            {
                auto info = holder->GetSpellProto();
                if (!info->HasAttribute(
                        SPELL_ATTR_UNAFFECTED_BY_INVULNERABILITY) &&
                    holder->HasMechanicMask(mechanic))
                    return true;
                // Judgement of justice is removed by PvP trinket since 2.3
                if (pvp_trinket &&
                    info->SpellFamilyName == SPELLFAMILY_PALADIN &&
                    (info->SpellFamilyFlags & 0x100000) != 0)
                    return true;
                return false;
            });
    }

    target->ApplySpellImmune(this, IMMUNITY_MECHANIC, misc, apply);

    // special cases
    switch (misc)
    {
    case MECHANIC_INVULNERABILITY:
        target->ModifyAuraState(AURA_STATE_FORBEARANCE, apply);
        break;
    case MECHANIC_SHIELD:
        target->ModifyAuraState(AURA_STATE_WEAKENED_SOUL, apply);
        break;
    }
}

void Aura::HandleModMechanicImmunityMask(bool apply, bool /*Real*/)
{
    uint32 mechanic = m_modifier.m_miscvalue;

    if (apply &&
        GetSpellProto()->HasAttribute(SPELL_ATTR_EX_DISPEL_AURAS_ON_IMMUNITY))
        GetTarget()->remove_auras_if([mechanic](AuraHolder* holder)
            {
                return !holder->GetSpellProto()->HasAttribute(
                           SPELL_ATTR_UNAFFECTED_BY_INVULNERABILITY) &&
                       holder->HasMechanicMask(mechanic);
            });

    // check implemented in Unit::IsImmuneToSpell and
    // Unit::IsImmuneToSpellEffect
}

// this method is called whenever we add / remove aura which gives m_target some
// imunity to some spell effect
void Aura::HandleAuraModEffectImmunity(bool apply, bool /*Real*/)
{
    Unit* target = GetTarget();

    // when removing flag aura, handle flag drop
    if (!apply && target->GetTypeId() == TYPEID_PLAYER &&
        (GetSpellProto()->AuraInterruptFlags &
            AURA_INTERRUPT_FLAG_IMMUNE_OR_LOST_SELECTION))
    {
        if (BattleGround* bg = ((Player*)target)->GetBattleGround())
            bg->EventPlayerDroppedFlag(((Player*)target));
    }

    target->ApplySpellImmune(
        this, IMMUNITY_EFFECT, m_modifier.m_miscvalue, apply);
}

void Aura::HandleAuraModStateImmunity(bool apply, bool Real)
{
    if (apply && Real &&
        GetSpellProto()->HasAttribute(SPELL_ATTR_EX_DISPEL_AURAS_ON_IMMUNITY))
    {
        GetTarget()->remove_auras(AuraType(m_modifier.m_miscvalue),
            [this](AuraHolder* holder)
            {
                return holder != GetHolder(); // skip self (already added)
            });
    }

    GetTarget()->ApplySpellImmune(
        this, IMMUNITY_STATE, m_modifier.m_miscvalue, apply);
}

void Aura::HandleAuraModSchoolImmunity(bool apply, bool Real)
{
    Unit* target = GetTarget();
    target->ApplySpellImmune(
        this, IMMUNITY_SCHOOL, m_modifier.m_miscvalue, apply);

    // remove all flag auras (they are positive, but they must be removed when
    // you are immune)
    if (GetSpellProto()->HasAttribute(SPELL_ATTR_EX_DISPEL_AURAS_ON_IMMUNITY) &&
        GetSpellProto()->HasAttribute(SPELL_ATTR_EX2_DAMAGE_REDUCED_SHIELD))
        target->remove_auras_if([](AuraHolder* h)
            {
                return h->GetSpellProto()->AuraInterruptFlags &
                       AURA_INTERRUPT_FLAG_IMMUNE_OR_LOST_SELECTION;
            });

    // TODO: optimalize this cycle
    if (Real && apply &&
        GetSpellProto()->HasAttribute(SPELL_ATTR_EX_DISPEL_AURAS_ON_IMMUNITY))
    {
        uint32 school_mask = m_modifier.m_miscvalue;
        target->remove_auras_if([school_mask, this](AuraHolder* holder)
            {
                const SpellEntry* info = holder->GetSpellProto();
                if ((GetSpellSchoolMask(info) &
                        school_mask) // Check for school mask
                    &&
                    !info->HasAttribute(
                        SPELL_ATTR_UNAFFECTED_BY_INVULNERABILITY) &&
                    !info->HasAttribute(SPELL_ATTR_PASSIVE) &&
                    // Only remove positive spells if aura is negative
                    (!holder->IsPositive() || !IsPositive()) &&
                    info->Id != GetId()) // Don't remove self
                {
                    return true;
                }
                return false;
            });
    }
    if (Real && GetSpellProto()->Mechanic == MECHANIC_BANISH)
    {
        if (apply)
            target->addUnitState(UNIT_STAT_ISOLATED);
        else
            target->clearUnitState(UNIT_STAT_ISOLATED);
    }
}

void Aura::HandleAuraModDmgImmunity(bool apply, bool /*Real*/)
{
    GetTarget()->ApplySpellImmune(
        this, IMMUNITY_DAMAGE, m_modifier.m_miscvalue, apply);
}

void Aura::HandleAuraModDispelImmunity(bool apply, bool Real)
{
    // all applied/removed only at real aura add/remove
    if (!Real)
        return;

    auto target = GetTarget();
    auto type = DispelType(m_modifier.m_miscvalue);

    // World bosses cannot be made immune to stealth and invisibility
    if ((type == DISPEL_STEALTH || type == DISPEL_INVISIBILITY) &&
        target->GetTypeId() == TYPEID_UNIT &&
        static_cast<Creature*>(target)->IsWorldBoss())
        return;

    target->ApplySpellDispelImmunity(this, type, apply);

    // Stoneform (Dwarf Racial) uses more immunity effects than what the dbc can
    // store,
    // which is probably the reason why some are missing. Here we handle those.
    if (GetSpellProto()->Id == 20594)
    {
        target->ApplySpellDispelImmunity(this, DISPEL_DISEASE, apply);

        // ApplySpellImmune does not remove current effects
        if (apply)
            target->remove_auras_if([](AuraHolder* holder)
                {
                    return !holder->GetSpellProto()->HasAttribute(
                               SPELL_ATTR_UNAFFECTED_BY_INVULNERABILITY) &&
                           holder->HasMechanicMask(1 << (MECHANIC_BLEED - 1));
                });
        target->ApplySpellImmune(
            this, IMMUNITY_MECHANIC, MECHANIC_BLEED, apply);
    }
}

void Aura::HandleAuraProcTriggerSpell(bool apply, bool Real)
{
    if (!Real)
        return;

    switch (GetId())
    {
    // some spell have charges by functionality not have its in spell data
    case 28200: // Ascendance (Talisman of Ascendance trinket)
        if (apply)
            GetHolder()->SetAuraCharges(6);
        break;
    default:
        break;
    }
}

void Aura::HandleAuraModStalked(bool apply, bool Real)
{
    auto target = GetTarget();

    // used by spells: Hunter's Mark, Mind Vision, Syndicate Tracker (MURP) DND
    if (apply)
        target->SetFlag(UNIT_DYNAMIC_FLAGS, UNIT_DYNFLAG_TRACK_UNIT);
    else
        target->RemoveFlag(UNIT_DYNAMIC_FLAGS, UNIT_DYNFLAG_TRACK_UNIT);

    // Hunter's mark reveals invisible & stealthed units; do a visibility update
    if (Real && target->GetTypeId() == TYPEID_PLAYER && target->IsInWorld() &&
        GetSpellProto()->SpellFamilyName == SPELLFAMILY_HUNTER &&
        GetSpellProto()->SpellFamilyFlags & 0x400)
    {
        static_cast<Player*>(target)->GetCamera().UpdateVisibilityForOwner();

        // Select nearby players that can detect us, and update for them too
        maps::visitors::simple<Player>()(target,
            target->GetMap()->GetVisibilityDistance(), [target](Player* p)
            {
                if (p != target && p->canDetectInvisibilityOf(target))
                    p->GetCamera().UpdateVisibilityForOwner();
            });
    }
}

/*********************************************************/
/***                   PERIODIC                        ***/
/*********************************************************/

void Aura::HandlePeriodicTriggerSpell(bool apply, bool /*Real*/)
{
    m_isPeriodic = apply;

    Unit* target = GetTarget();

    if (!apply)
    {
        switch (GetId())
        {
        case 66: // Invisibility
            if (m_removeMode == AURA_REMOVE_BY_EXPIRE)
                target->CastSpell(target, 32612, true, nullptr, this);

            return;
        case 29213: // Curse of the Plaguebringer
            if (m_removeMode != AURA_REMOVE_BY_DISPEL)
                // Cast Wrath of the Plaguebringer if not dispelled
                target->CastSpell(target, 29214, true, nullptr, this);
            return;
        case 42783: // Wrath of the Astrom...
            if (m_removeMode == AURA_REMOVE_BY_EXPIRE &&
                GetEffIndex() + 1 < MAX_EFFECT_INDEX)
                target->CastSpell(target,
                    GetSpellProto()->CalculateSimpleValue(
                        SpellEffectIndex(GetEffIndex() + 1)),
                    true);

            return;
        default:
            break;
        }
    }
}

void Aura::HandlePeriodicTriggerSpellWithValue(bool apply, bool /*Real*/)
{
    m_isPeriodic = apply;
}

void Aura::HandlePeriodicEnergize(bool apply, bool /*Real*/)
{
    m_isPeriodic = apply;
}

void Aura::HandleAuraPowerBurn(bool apply, bool /*Real*/)
{
    m_isPeriodic = apply;
}

void Aura::HandleAuraPeriodicDummy(bool apply, bool Real)
{
    // spells required only Real aura add/remove
    if (!Real)
        return;

    Unit* target = GetTarget();

    SpellEntry const* spell = GetSpellProto();
    switch (spell->SpellFamilyName)
    {
    case SPELLFAMILY_ROGUE:
    {
        // Master of Subtlety
        if (spell->Id == 31666 && !apply)
        {
            target->remove_auras(31665);
            break;
        }
        break;
    }
    }

    m_isPeriodic = apply;
}

void Aura::HandlePeriodicHeal(bool apply, bool /*Real*/)
{
    m_isPeriodic = apply;

    Unit* target = GetTarget();

    // For prevent double apply bonuses
    bool loading = (target->GetTypeId() == TYPEID_PLAYER &&
                    ((Player*)target)->GetSession()->PlayerLoading());

    // Custom damage calculation after
    if (apply)
    {
        if (loading)
            return;

        Unit* caster = GetCaster();
        if (!caster)
            return;

        // Restart with base value if a second or higher stack is applied,
        // otherwise we'll scale based on an already scaled value
        if (GetStackAmount() > 1)
            m_modifier.m_amount =
                GetSpellProto()->CalculateSimpleValue(m_effIndex) *
                GetStackAmount();

        m_modifier.m_amount = caster->SpellHealingBonusDone(target,
            GetSpellProto(), m_modifier.m_amount, DOT, GetStackAmount());
    }
}

void Aura::HandlePeriodicDamage(bool apply, bool Real)
{
    // spells required only Real aura add/remove
    if (!Real)
        return;

    m_isPeriodic = apply;

    Unit* target = GetTarget();
    SpellEntry const* spellProto = GetSpellProto();

    // For prevent double apply bonuses
    bool loading = (target->GetTypeId() == TYPEID_PLAYER &&
                    ((Player*)target)->GetSession()->PlayerLoading());

    // Custom damage calculation after
    if (apply)
    {
        if (loading)
            return;

        Unit* caster = GetCaster();
        if (!caster)
            return;

        switch (spellProto->SpellFamilyName)
        {
        case SPELLFAMILY_WARRIOR:
        {
            // Rend
            if (spellProto->SpellFamilyFlags & UI64LIT(0x0000000000000020))
            {
                // 0.00743*(($MWB+$mwb)/2+$AP/14*$MWS) bonus per tick
                float ap = caster->GetTotalAttackPowerValue(BASE_ATTACK);
                int32 mws = caster->GetAttackTime(BASE_ATTACK);
                float mwb_min =
                    caster->GetWeaponDamageRange(BASE_ATTACK, MINDAMAGE);
                float mwb_max =
                    caster->GetWeaponDamageRange(BASE_ATTACK, MAXDAMAGE);
                m_modifier.m_amount += int32(
                    ((mwb_min + mwb_max) / 2 + ap * mws / 14000) * 0.00743f);
            }
            break;
        }
        case SPELLFAMILY_DRUID:
        {
            // Rip
            if (spellProto->SpellFamilyFlags & UI64LIT(0x000000000000800000))
            {
                if (caster->GetTypeId() != TYPEID_PLAYER)
                    break;

                // $AP * min(0.06*$cp, 0.24)/6 [Yes, there is no difference,
                // whether 4 or 5 CPs are being used]

                uint8 cp = ((Player*)caster)->GetComboPoints();

                // Idol of Feral Shadows. Cant be handled as SpellMod in
                // SpellAura:Dummy due its dependency from CPs
                auto& dummyAuras = caster->GetAurasByType(SPELL_AURA_DUMMY);
                for (const auto& dummyAura : dummyAuras)
                {
                    if ((dummyAura)->GetId() == 34241)
                    {
                        m_modifier.m_amount +=
                            cp * (dummyAura)->GetModifier()->m_amount;
                        break;
                    }
                }

                if (cp > 4)
                    cp = 4;
                // 6% ap scaling per cp divided to 6 ticks
                float ap_coeff = cp * 0.06f / 6.0f;
                m_modifier.m_amount += int32(
                    caster->GetTotalAttackPowerValue(BASE_ATTACK) * ap_coeff);
            }
            break;
        }
        case SPELLFAMILY_ROGUE:
        {
            // Rupture
            if (spellProto->SpellFamilyFlags & UI64LIT(0x000000000000100000))
            {
                if (caster->GetTypeId() != TYPEID_PLAYER)
                    break;

                uint8 cp = ((Player*)caster)->GetComboPoints();

                int ticks = 3 + cp;
                float ap_coeff = 0.0f;

                switch (cp)
                {
                case 1:
                {
                    ap_coeff = 0.04f / ticks;
                    break;
                }
                case 2:
                {
                    ap_coeff = 0.10f / ticks;
                    break;
                }
                case 3:
                {
                    ap_coeff = 0.18f / ticks;
                    break;
                }
                case 4:
                {
                    ap_coeff = 0.21f / ticks;
                    break;
                }
                case 5:
                {
                    ap_coeff = 0.24f / ticks;
                    break;
                }
                }
                m_modifier.m_amount += int32(
                    caster->GetTotalAttackPowerValue(BASE_ATTACK) * ap_coeff);
            }
            break;
        }
        default:
            break;
        }

        if (m_modifier.m_auraname == SPELL_AURA_PERIODIC_DAMAGE)
        {
            // SpellDamageBonusDone for magic spells
            if (spellProto->DmgClass == SPELL_DAMAGE_CLASS_NONE ||
                spellProto->DmgClass == SPELL_DAMAGE_CLASS_MAGIC)
                m_modifier.m_amount =
                    caster->SpellDamageBonusDone(target, GetSpellProto(),
                        m_modifier.m_amount, DOT, GetStackAmount());
            // MeleeDamagebonusDone for weapon based spells
            else
            {
                WeaponAttackType attackType =
                    GetWeaponAttackType(GetSpellProto());
                m_modifier.m_amount =
                    caster->MeleeDamageBonusDone(target, m_modifier.m_amount,
                        attackType, GetSpellProto(), DOT, GetStackAmount());
            }
        }
    }
    // remove time effects
    else
    {
        // Parasitic Shadowfiend - handle summoning of two Shadowfiends on DoT
        // expire
        if (spellProto->Id == 41917)
            target->CastSpell(target, 41915, true);
    }
}

void Aura::HandlePeriodicDamagePCT(bool apply, bool /*Real*/)
{
    m_isPeriodic = apply;
}

void Aura::HandlePeriodicLeech(bool apply, bool /*Real*/)
{
    m_isPeriodic = apply;

    // For prevent double apply bonuses
    bool loading = (GetTarget()->GetTypeId() == TYPEID_PLAYER &&
                    ((Player*)GetTarget())->GetSession()->PlayerLoading());

    // Custom damage calculation after
    if (apply)
    {
        if (loading)
            return;

        Unit* caster = GetCaster();
        if (!caster)
            return;

        m_modifier.m_amount = caster->SpellDamageBonusDone(GetTarget(),
            GetSpellProto(), m_modifier.m_amount, DOT, GetStackAmount());
    }
}

void Aura::HandlePeriodicManaLeech(bool apply, bool /*Real*/)
{
    m_isPeriodic = apply;
}

void Aura::HandlePeriodicHealthFunnel(bool apply, bool /*Real*/)
{
    m_isPeriodic = apply;

    // For prevent double apply bonuses
    bool loading = (GetTarget()->GetTypeId() == TYPEID_PLAYER &&
                    ((Player*)GetTarget())->GetSession()->PlayerLoading());

    // Custom damage calculation after
    if (apply)
    {
        if (loading)
            return;

        Unit* caster = GetCaster();
        if (!caster)
            return;

        m_modifier.m_amount = caster->SpellDamageBonusDone(GetTarget(),
            GetSpellProto(), m_modifier.m_amount, DOT, GetStackAmount());
    }
}

/*********************************************************/
/***                  MODIFY STATS                     ***/
/*********************************************************/

/********************************/
/***        RESISTANCE        ***/
/********************************/

void Aura::HandleAuraModResistanceExclusive(bool apply, bool /*Real*/)
{
    float auras[MAX_SPELL_SCHOOL] = {0.0f};

    // Find the most potent resistance aura of each field (that way we only
    // search through the aura pool once)
    auto& al = GetTarget()->GetAurasByType(SPELL_AURA_MOD_RESISTANCE_EXCLUSIVE);
    for (const auto& elem : al)
    {
        if (elem == this)
            continue; // Skip ourself
        if (m_positive != (elem)->IsPositive())
            continue; // Skip auras not the same "sign" as us
        for (int32 i = SPELL_SCHOOL_NORMAL; i < MAX_SPELL_SCHOOL; ++i)
        {
            // It seems holy resistance cannot be applied from buffs
            if (i == SPELL_SCHOOL_HOLY)
                continue;
            if ((elem)->GetModifier()->m_miscvalue & (1 << i))
            {
                if (auras[i] < (elem)->GetModifier()->m_amount)
                    auras[i] = (elem)->GetModifier()->m_amount;
            }
        }
    }

    // Modify the categories we affect
    for (int32 i = SPELL_SCHOOL_NORMAL; i < MAX_SPELL_SCHOOL; ++i)
    {
        // It seems holy resistance cannot be applied from buffs
        if (i == SPELL_SCHOOL_HOLY)
            continue;
        if (!(m_modifier.m_miscvalue & (1 << i)))
            continue;
        // If we're stronger than the other auras, we need to modify the
        // resistance, else we don't care
        if (m_modifier.m_amount > auras[i])
        {
            // We either apply or remove the difference between us and the
            // second strongest
            float diff = m_modifier.m_amount - auras[i];
            GetTarget()->HandleStatModifier(
                static_cast<UnitMods>(UNIT_MOD_RESISTANCE_START + i),
                TOTAL_VALUE, diff, apply);

            // This is what makes it appear as green ("extra") resistance in the
            // character pane (might have some other effect too)
            if (GetTarget()->GetTypeId() == TYPEID_PLAYER ||
                static_cast<Creature*>(GetTarget())->IsPet())
                GetTarget()->ApplyResistanceBuffModsMod(
                    static_cast<SpellSchools>(i), m_positive, diff, apply);
        }
    }
}

void Aura::HandleAuraModResistance(bool apply, bool /*Real*/)
{
    for (int8 x = SPELL_SCHOOL_NORMAL; x < MAX_SPELL_SCHOOL; x++)
    {
        // It seems holy resistance cannot be applied from buffs
        if (x == SPELL_SCHOOL_HOLY)
            continue;
        if (m_modifier.m_miscvalue & int32(1 << x))
        {
            GetTarget()->HandleStatModifier(
                UnitMods(UNIT_MOD_RESISTANCE_START + x), TOTAL_VALUE,
                float(m_modifier.m_amount), apply);
            if (GetTarget()->GetTypeId() == TYPEID_PLAYER ||
                ((Creature*)GetTarget())->IsPet())
                GetTarget()->ApplyResistanceBuffModsMod(SpellSchools(x),
                    m_positive, float(m_modifier.m_amount), apply);
        }
    }
}

void Aura::HandleAuraModBaseResistancePCT(bool apply, bool /*Real*/)
{
    auto target = GetTarget();

    // only players have base stats
    if (target->GetTypeId() != TYPEID_PLAYER)
    {
        // pets only have base armor
        if (static_cast<Creature*>(target)->IsPet() &&
            (m_modifier.m_miscvalue & SPELL_SCHOOL_MASK_NORMAL))
            target->HandleStatModifier(
                UNIT_MOD_ARMOR, BASE_PCT, float(m_modifier.m_amount), apply);
    }
    else
    {
        // Druid's enrage
        if (apply && GetId() == 5229)
            m_modifier.m_amount =
                target->GetShapeshiftForm() == FORM_DIREBEAR ? -16 : -27;

        for (int8 x = SPELL_SCHOOL_NORMAL; x < MAX_SPELL_SCHOOL; x++)
        {
            if (m_modifier.m_miscvalue & int32(1 << x))
                target->HandleStatModifier(
                    UnitMods(UNIT_MOD_RESISTANCE_START + x), BASE_PCT,
                    float(m_modifier.m_amount), apply);
        }
    }
}

void Aura::HandleModResistancePercent(bool apply, bool /*Real*/)
{
    Unit* target = GetTarget();

    for (int8 i = SPELL_SCHOOL_NORMAL; i < MAX_SPELL_SCHOOL; i++)
    {
        if (m_modifier.m_miscvalue & int32(1 << i))
        {
            target->HandleStatModifier(UnitMods(UNIT_MOD_RESISTANCE_START + i),
                TOTAL_PCT, float(m_modifier.m_amount), apply);
            if (target->GetTypeId() == TYPEID_PLAYER ||
                static_cast<Creature*>(target)->IsPet())
                target->ApplyResistanceBuffModsPercentMod(SpellSchools(i),
                    m_positive, float(m_modifier.m_amount), apply);
        }
    }
}

void Aura::HandleModBaseResistance(bool apply, bool /*Real*/)
{
    // only players have base stats
    if (GetTarget()->GetTypeId() != TYPEID_PLAYER)
    {
        // only pets have base stats
        if (((Creature*)GetTarget())->IsPet() &&
            (m_modifier.m_miscvalue & SPELL_SCHOOL_MASK_NORMAL))
            GetTarget()->HandleStatModifier(
                UNIT_MOD_ARMOR, TOTAL_VALUE, float(m_modifier.m_amount), apply);
    }
    else
    {
        for (int i = SPELL_SCHOOL_NORMAL; i < MAX_SPELL_SCHOOL; i++)
            if (m_modifier.m_miscvalue & (1 << i))
                GetTarget()->HandleStatModifier(
                    UnitMods(UNIT_MOD_RESISTANCE_START + i), TOTAL_VALUE,
                    float(m_modifier.m_amount), apply);
    }
}

/********************************/
/***           STAT           ***/
/********************************/

void Aura::HandleAuraModStat(bool apply, bool Real)
{
    if (m_modifier.m_miscvalue < -2 || m_modifier.m_miscvalue > 4)
    {
        logging.error(
            "WARNING: Spell %u effect %u have unsupported misc value (%i) for "
            "SPELL_AURA_MOD_STAT ",
            GetId(), GetEffIndex(), m_modifier.m_miscvalue);
        return;
    }

    Unit* target = GetTarget();

    // Enchant Weapon Crusader - Reduced effect for level 60+
    if (apply && Real && GetId() == 20007)
    {
        uint32 level = target->getLevel();
        if (level > 60)
            m_modifier.m_amount *= (1 - 0.04 * (level - 60));
    }

    for (int32 i = STAT_STRENGTH; i < MAX_STATS; i++)
    {
        // -1 or -2 is all stats ( misc < -2 checked in function beginning )
        if (m_modifier.m_miscvalue < 0 || m_modifier.m_miscvalue == i)
        {
            // m_target->ApplyStatMod(Stats(i), m_modifier.m_amount,apply);
            target->HandleStatModifier(UnitMods(UNIT_MOD_STAT_START + i),
                TOTAL_VALUE, float(m_modifier.m_amount), apply);
            if (target->GetTypeId() == TYPEID_PLAYER ||
                ((Creature*)target)->IsPet())
                target->ApplyStatBuffMod(
                    Stats(i), float(m_modifier.m_amount), apply);

            // If a stamina/intellect buff is applied and one was previously
            // overwritten, we need to adjust
            // our current health/mana, to not cause droppage when reapplying
            // stamina/intellect effects
            if (i == STAT_STAMINA || i == STAT_INTELLECT)
            {
                auto holder = GetHolder();
                auto p = holder->GetSavedHpMp();
                uint32 prev = i == STAT_STAMINA ? p.first : p.second,
                       curr = i == STAT_STAMINA ? target->GetHealth() :
                                                  target->GetPower(POWER_MANA);
                if (prev > curr)
                {
                    if (i == STAT_STAMINA)
                    {
                        target->SetHealth(p.first);
                        holder->SetSavedHpMp(std::make_pair(0, p.second));
                    }
                    else
                    {
                        target->SetPower(POWER_MANA, p.second);
                        holder->SetSavedHpMp(std::make_pair(p.first, 0));
                    }
                }
            }
        }
    }
}

void Aura::HandleModPercentStat(bool apply, bool /*Real*/)
{
    if (m_modifier.m_miscvalue < -1 || m_modifier.m_miscvalue > 4)
    {
        logging.error(
            "WARNING: Misc Value for SPELL_AURA_MOD_PERCENT_STAT not valid");
        return;
    }

    // only players have base stats
    if (GetTarget()->GetTypeId() != TYPEID_PLAYER)
        return;

    for (int32 i = STAT_STRENGTH; i < MAX_STATS; ++i)
    {
        if (m_modifier.m_miscvalue == i || m_modifier.m_miscvalue == -1)
            GetTarget()->HandleStatModifier(UnitMods(UNIT_MOD_STAT_START + i),
                BASE_PCT, float(m_modifier.m_amount), apply);
    }
}

void Aura::HandleModSpellDamagePercentFromStat(bool /*apply*/, bool /*Real*/)
{
    if (GetTarget()->GetTypeId() != TYPEID_PLAYER)
        return;

    // Magic damage modifiers implemented in Unit::SpellDamageBonusDone
    // This information for client side use only
    // Recalculate bonus
    ((Player*)GetTarget())->UpdateSpellDamageAndHealingBonus();
}

void Aura::HandleModSpellHealingPercentFromStat(bool /*apply*/, bool /*Real*/)
{
    if (GetTarget()->GetTypeId() != TYPEID_PLAYER)
        return;

    // Recalculate bonus
    ((Player*)GetTarget())->UpdateSpellDamageAndHealingBonus();
}

void Aura::HandleAuraModDispelResist(bool apply, bool Real)
{
    if (!Real || !apply)
        return;

    if (GetId() == 33206)
        GetTarget()->CastSpell(
            GetTarget(), 44416, true, nullptr, this, GetCasterGuid());
}

void Aura::HandleModSpellDamagePercentFromAttackPower(
    bool /*apply*/, bool /*Real*/)
{
    if (GetTarget()->GetTypeId() != TYPEID_PLAYER)
        return;

    // Magic damage modifiers implemented in Unit::SpellDamageBonusDone
    // This information for client side use only
    // Recalculate bonus
    ((Player*)GetTarget())->UpdateSpellDamageAndHealingBonus();
}

void Aura::HandleModSpellHealingPercentFromAttackPower(
    bool /*apply*/, bool /*Real*/)
{
    if (GetTarget()->GetTypeId() != TYPEID_PLAYER)
        return;

    // Recalculate bonus
    ((Player*)GetTarget())->UpdateSpellDamageAndHealingBonus();
}

void Aura::HandleModHealingDone(bool /*apply*/, bool /*Real*/)
{
    if (GetTarget()->GetTypeId() != TYPEID_PLAYER)
        return;
    // implemented in Unit::SpellHealingBonusDone
    // this information is for client side only
    ((Player*)GetTarget())->UpdateSpellDamageAndHealingBonus();
}

void Aura::HandleModTotalPercentStat(bool apply, bool /*Real*/)
{
    if (m_modifier.m_miscvalue < -1 || m_modifier.m_miscvalue > 4)
    {
        logging.error(
            "WARNING: Misc Value for SPELL_AURA_MOD_PERCENT_STAT not valid");
        return;
    }

    Unit* target = GetTarget();

    // save current and max HP before applying aura
    uint32 curHPValue = target->GetHealth();
    uint32 maxHPValue = target->GetMaxHealth();

    for (int32 i = STAT_STRENGTH; i < MAX_STATS; i++)
    {
        if (m_modifier.m_miscvalue == i || m_modifier.m_miscvalue == -1)
        {
            target->HandleStatModifier(UnitMods(UNIT_MOD_STAT_START + i),
                TOTAL_PCT, float(m_modifier.m_amount), apply);
            if (target->GetTypeId() == TYPEID_PLAYER ||
                ((Creature*)target)->IsPet())
                target->ApplyStatPercentBuffMod(
                    Stats(i), float(m_modifier.m_amount), apply);
        }
    }

    // recalculate current HP/MP after applying aura modifications (only for
    // spells with 0x10 flag)
    if (m_modifier.m_miscvalue == STAT_STAMINA && maxHPValue > 0 &&
        GetSpellProto()->HasAttribute(SPELL_ATTR_ABILITY))
    {
        // newHP = (curHP / maxHP) * newMaxHP = (newMaxHP * curHP) / maxHP ->
        // which is better because no int -> double -> int conversion is needed
        uint32 newHPValue = (target->GetMaxHealth() * curHPValue) / maxHPValue;
        target->SetHealth(newHPValue);
    }
}

void Aura::HandleAuraModResistenceOfStatPercent(bool /*apply*/, bool /*Real*/)
{
    if (GetTarget()->GetTypeId() != TYPEID_PLAYER)
        return;

    if (m_modifier.m_miscvalue != SPELL_SCHOOL_MASK_NORMAL)
    {
        // support required adding replace UpdateArmor by loop by
        // UpdateResistence at intellect update
        // and include in UpdateResistence same code as in UpdateArmor for aura
        // mod apply.
        logging.error(
            "Aura SPELL_AURA_MOD_RESISTANCE_OF_STAT_PERCENT(182) need adding "
            "support for non-armor resistances!");
        return;
    }

    // Recalculate Armor
    GetTarget()->UpdateArmor();
}

/********************************/
/***      HEAL & ENERGIZE     ***/
/********************************/
void Aura::HandleAuraModTotalHealthPercentRegen(bool apply, bool /*Real*/)
{
    m_isPeriodic = apply;
}

void Aura::HandleAuraModTotalManaPercentRegen(bool apply, bool /*Real*/)
{
    if (m_modifier.periodictime == 0)
        m_modifier.periodictime = 1000;

    m_periodicTimer = m_modifier.periodictime;
    m_isPeriodic = apply;
}

void Aura::HandleModRegen(bool apply, bool /*Real*/) // eating
{
    if (m_modifier.periodictime == 0)
        m_modifier.periodictime = 5000;

    m_periodicTimer = 5000;
    m_isPeriodic = apply;
}

void Aura::HandleModPowerRegen(bool apply, bool Real)
{
    if (!Real)
        return;

    Unit* target = GetTarget();
    const SpellEntry* info = GetSpellProto();

    Powers pt = target->getPowerType();

    // Anger Management (only spell use this aura for rage)
    if (m_modifier.periodictime == 0 && pt == POWER_RAGE)
        m_modifier.periodictime = 3000;

    // Drinking (all drink spells have effect value 0 in post 2.4.0)
    if (pt == POWER_MANA && GetBasePoints() == 0)
    {
        // Calculate drink value right away if we're not in an Arena
        if (!target->GetMap()->IsBattleArena() && GetEffIndex() < 2 &&
            info->EffectApplyAuraName[GetEffIndex() + 1] ==
                SPELL_AURA_PERIODIC_DUMMY)
        {
            // The drink value is specified in the periodic dummy
            int points = info->EffectBasePoints[GetEffIndex() + 1] - 1;
            if (points > 0)
                m_modifier.m_amount = points; // TODO: This does not respect
                                              // base point overrides on the
                                              // periodic dummy aura
        }
    }

    m_periodicTimer = 5000;

    if (GetTarget()->GetTypeId() == TYPEID_PLAYER &&
        m_modifier.m_miscvalue == POWER_MANA)
        ((Player*)GetTarget())->UpdateManaRegen();

    m_isPeriodic = apply;
}

void Aura::HandleModPowerRegenPCT(bool /*apply*/, bool Real)
{
    // spells required only Real aura add/remove
    if (!Real)
        return;

    if (GetTarget()->GetTypeId() != TYPEID_PLAYER)
        return;

    // Update manaregen value
    if (m_modifier.m_miscvalue == POWER_MANA)
        ((Player*)GetTarget())->UpdateManaRegen();
}

void Aura::HandleModManaRegen(bool /*apply*/, bool Real)
{
    // spells required only Real aura add/remove
    if (!Real)
        return;

    if (GetTarget()->GetTypeId() != TYPEID_PLAYER)
        return;

    // Note: an increase in regen does NOT cause threat.
    ((Player*)GetTarget())->UpdateManaRegen();
}

void Aura::HandleComprehendLanguage(bool apply, bool /*Real*/)
{
    if (apply)
        GetTarget()->SetFlag(UNIT_FIELD_FLAGS_2, UNIT_FLAG2_COMPREHEND_LANG);
    else
        GetTarget()->RemoveFlag(UNIT_FIELD_FLAGS_2, UNIT_FLAG2_COMPREHEND_LANG);
}

void Aura::HandleAuraModIncreaseHealth(bool apply, bool Real)
{
    Unit* target = GetTarget();

    // Special case with temporary increase max/current health
    switch (GetId())
    {
    case 12976: // Warrior Last Stand triggered spell
    case 28726: // Nightmare Seed ( Nightmare Seed )
    case 34511: // Valor (Bulwark of Kings, Bulwark of the Ancient Kings)
    case 44055: // Tremendous Fortitude (Battlemaster's Alacrity)
    {
        if (Real)
        {
            if (apply)
            {
                target->HandleStatModifier(UNIT_MOD_HEALTH, TOTAL_VALUE,
                    float(m_modifier.m_amount), apply);
                target->ModifyHealth(m_modifier.m_amount);
            }
            else
            {
                if (int32(target->GetHealth()) > m_modifier.m_amount)
                    target->ModifyHealth(-m_modifier.m_amount);
                else
                    target->SetHealth(1);
                target->HandleStatModifier(UNIT_MOD_HEALTH, TOTAL_VALUE,
                    float(m_modifier.m_amount), apply);
            }
        }
        return;
    }
    }

    // generic case
    target->HandleStatModifier(
        UNIT_MOD_HEALTH, TOTAL_VALUE, float(m_modifier.m_amount), apply);
}

void Aura::HandleAuraModIncreaseMaxHealth(bool apply, bool /*Real*/)
{
    if (GetId() == 30421)
    {
        // Netherspite's Red Player Buff. Works completely different from normal
        // ModIncreaseMaxHealth
        Unit* target = GetTarget();
        target->HandleStatModifier(UNIT_MOD_HEALTH, TOTAL_VALUE,
            float(31000 + m_modifier.m_amount), apply);
        target->SetHealth(target->GetMaxHealth());
        return;
    }

    Unit* target = GetTarget();
    uint32 oldhealth = target->GetHealth();
    double healthPercentage =
        (double)oldhealth / (double)target->GetMaxHealth();

    target->HandleStatModifier(
        UNIT_MOD_HEALTH, TOTAL_VALUE, float(m_modifier.m_amount), apply);

    // refresh percentage
    if (oldhealth > 0)
    {
        uint32 newhealth =
            uint32(ceil((double)target->GetMaxHealth() * healthPercentage));
        if (newhealth == 0)
            newhealth = 1;

        target->SetHealth(newhealth);
    }
}

void Aura::HandleAuraModIncreaseEnergy(bool apply, bool /*Real*/)
{
    Unit* target = GetTarget();
    Powers powerType = target->getPowerType();
    if (int32(powerType) != m_modifier.m_miscvalue)
        return;

    UnitMods unitMod = UnitMods(UNIT_MOD_POWER_START + powerType);

    target->HandleStatModifier(
        unitMod, TOTAL_VALUE, float(m_modifier.m_amount), apply);
}

void Aura::HandleAuraModIncreaseEnergyPercent(bool apply, bool /*Real*/)
{
    Powers powerType = GetTarget()->getPowerType();
    if (int32(powerType) != m_modifier.m_miscvalue)
        return;

    UnitMods unitMod = UnitMods(UNIT_MOD_POWER_START + powerType);

    GetTarget()->HandleStatModifier(
        unitMod, TOTAL_PCT, float(m_modifier.m_amount), apply);
}

void Aura::HandleAuraModIncreaseHealthPercent(bool apply, bool Real)
{
    auto target = GetTarget();

    uint32 prev_maxhp = target->GetMaxHealth();

    target->HandleStatModifier(
        UNIT_MOD_HEALTH, TOTAL_PCT, float(m_modifier.m_amount), apply);

    uint32 new_maxhp = target->GetMaxHealth();

    // Increase current health with the difference between new and previous max
    if (apply && Real && new_maxhp > prev_maxhp)
    {
        auto diff = new_maxhp - prev_maxhp;
        target->SetHealth(target->GetHealth() + diff);
    }

    // Burning Adrenaline
    if (GetId() == 23619 && Real && apply && GetStackAmount() >= 20)
    {
        target->SetMaxHealth(1);
        target->SetHealth(1);
        target->CastSpell(target, 23478, true);
    }
}

/********************************/
/***          FIGHT           ***/
/********************************/

void Aura::HandleAuraModParryPercent(bool /*apply*/, bool /*Real*/)
{
    if (GetTarget()->GetTypeId() != TYPEID_PLAYER)
        return;

    ((Player*)GetTarget())->UpdateParryPercentage();
}

void Aura::HandleAuraModDodgePercent(bool /*apply*/, bool /*Real*/)
{
    if (GetTarget()->GetTypeId() != TYPEID_PLAYER)
        return;

    ((Player*)GetTarget())->UpdateDodgePercentage();
    // logging.error("BONUS DODGE CHANCE: + %f",
    // float(m_modifier.m_amount));
}

void Aura::HandleAuraModBlockPercent(bool /*apply*/, bool /*Real*/)
{
    if (GetTarget()->GetTypeId() != TYPEID_PLAYER)
        return;

    ((Player*)GetTarget())->UpdateBlockPercentage();
    // logging.error("BONUS BLOCK CHANCE: + %f",
    // float(m_modifier.m_amount));
}

void Aura::HandleAuraModRegenInterrupt(bool /*apply*/, bool Real)
{
    // spells required only Real aura add/remove
    if (!Real)
        return;

    if (GetTarget()->GetTypeId() != TYPEID_PLAYER)
        return;

    ((Player*)GetTarget())->UpdateManaRegen();
}

void Aura::HandleAuraModCritPercent(bool apply, bool Real)
{
    Unit* target = GetTarget();

    if (target->GetTypeId() != TYPEID_PLAYER)
        return;

    // apply item specific bonuses for already equipped weapon
    if (Real)
    {
        for (int i = 0; i < MAX_ATTACK; ++i)
            if (Item* pItem =
                    ((Player*)target)
                        ->GetWeaponForAttack(WeaponAttackType(i), true, false))
                ((Player*)target)
                    ->_ApplyWeaponDependentAuraCritMod(
                        pItem, WeaponAttackType(i), this, apply);
    }

    // mods must be applied base at equipped weapon class and subclass
    // comparison
    // with spell->EquippedItemClass and  EquippedItemSubClassMask and
    // EquippedItemInventoryTypeMask
    // m_modifier.m_miscvalue comparison with item generated damage types

    if (GetSpellProto()->EquippedItemClass == -1)
    {
        ((Player*)target)
            ->HandleBaseModValue(
                CRIT_PERCENTAGE, FLAT_MOD, float(m_modifier.m_amount), apply);
        ((Player*)target)
            ->HandleBaseModValue(OFFHAND_CRIT_PERCENTAGE, FLAT_MOD,
                float(m_modifier.m_amount), apply);
        ((Player*)target)
            ->HandleBaseModValue(RANGED_CRIT_PERCENTAGE, FLAT_MOD,
                float(m_modifier.m_amount), apply);
    }
    else
    {
        // done in Player::_ApplyWeaponDependentAuraMods
    }
}

void Aura::HandleModHitChance(bool apply, bool /*Real*/)
{
    Unit* target = GetTarget();

    if (target->GetTypeId() == TYPEID_PLAYER)
    {
        ((Player*)target)->UpdateMeleeHitChances();
        ((Player*)target)->UpdateRangedHitChances();
    }
    else
    {
        target->m_modMeleeHitChance +=
            apply ? m_modifier.m_amount : (-m_modifier.m_amount);
        target->m_modRangedHitChance +=
            apply ? m_modifier.m_amount : (-m_modifier.m_amount);
    }
}

void Aura::HandleModSpellHitChance(bool apply, bool /*Real*/)
{
    if (GetTarget()->GetTypeId() == TYPEID_PLAYER)
    {
        ((Player*)GetTarget())->UpdateSpellHitChances();
    }
    else
    {
        GetTarget()->m_modSpellHitChance +=
            apply ? m_modifier.m_amount : (-m_modifier.m_amount);
    }
}

void Aura::HandleModSpellCritChance(bool apply, bool Real)
{
    // spells required only Real aura add/remove
    if (!Real)
        return;

    if (GetTarget()->GetTypeId() == TYPEID_PLAYER)
    {
        ((Player*)GetTarget())->UpdateAllSpellCritChances();
    }
    else
    {
        GetTarget()->m_baseSpellCritChance +=
            apply ? m_modifier.m_amount : (-m_modifier.m_amount);
    }
}

void Aura::HandleModSpellCritChanceShool(bool /*apply*/, bool Real)
{
    // spells required only Real aura add/remove
    if (!Real)
        return;

    if (GetTarget()->GetTypeId() != TYPEID_PLAYER)
        return;

    for (int school = SPELL_SCHOOL_NORMAL; school < MAX_SPELL_SCHOOL; ++school)
        if (m_modifier.m_miscvalue & (1 << school))
            ((Player*)GetTarget())->UpdateSpellCritChance(school);
}

/********************************/
/***         ATTACK SPEED     ***/
/********************************/

void Aura::HandleModCastingSpeed(bool apply, bool /*Real*/)
{
    GetTarget()->ApplyCastTimePercentMod(float(m_modifier.m_amount), apply);
}

void Aura::HandleModMeleeRangedSpeedPct(bool apply, bool /*Real*/)
{
    Unit* target = GetTarget();
    target->ApplyAttackTimePercentMod(
        BASE_ATTACK, float(m_modifier.m_amount), apply);
    target->ApplyAttackTimePercentMod(
        OFF_ATTACK, float(m_modifier.m_amount), apply);
    target->ApplyAttackTimePercentMod(
        RANGED_ATTACK, float(m_modifier.m_amount), apply);
}

void Aura::HandleModCombatSpeedPct(bool apply, bool /*Real*/)
{
    Unit* target = GetTarget();
    target->ApplyCastTimePercentMod(float(m_modifier.m_amount), apply);
    target->ApplyAttackTimePercentMod(
        BASE_ATTACK, float(m_modifier.m_amount), apply);
    target->ApplyAttackTimePercentMod(
        OFF_ATTACK, float(m_modifier.m_amount), apply);
    target->ApplyAttackTimePercentMod(
        RANGED_ATTACK, float(m_modifier.m_amount), apply);
}

void Aura::HandleModAttackSpeed(bool apply, bool /*Real*/)
{
    Unit* t = GetTarget();

    t->ApplyAttackTimePercentMod(
        BASE_ATTACK, float(m_modifier.m_amount), apply);

    // Cobra Reflexes
    if (GetId() == 25076 && t->GetTypeId() == TYPEID_UNIT &&
        static_cast<Creature*>(t)->IsPet())
        static_cast<Pet*>(t)->UpdateDamagePhysical(BASE_ATTACK);
}

void Aura::HandleModMeleeSpeedPct(bool apply, bool /*Real*/)
{
    Unit* target = GetTarget();
    target->ApplyAttackTimePercentMod(
        BASE_ATTACK, float(m_modifier.m_amount), apply);
    target->ApplyAttackTimePercentMod(
        OFF_ATTACK, float(m_modifier.m_amount), apply);
}

void Aura::HandleAuraModRangedHaste(bool apply, bool /*Real*/)
{
    GetTarget()->ApplyAttackTimePercentMod(
        RANGED_ATTACK, float(m_modifier.m_amount), apply);
}

void Aura::HandleRangedAmmoHaste(bool apply, bool /*Real*/)
{
    if (GetTarget()->GetTypeId() != TYPEID_PLAYER)
        return;
    GetTarget()->ApplyAttackTimePercentMod(
        RANGED_ATTACK, float(m_modifier.m_amount), apply);
}

/********************************/
/***        ATTACK POWER      ***/
/********************************/

void Aura::HandleAuraModAttackPower(bool apply, bool /*Real*/)
{
    GetTarget()->HandleStatModifier(
        UNIT_MOD_ATTACK_POWER, TOTAL_VALUE, float(m_modifier.m_amount), apply);
}

void Aura::HandleAuraModRangedAttackPower(bool apply, bool /*Real*/)
{
    if ((GetTarget()->getClassMask() & CLASSMASK_WAND_USERS) != 0)
        return;

    GetTarget()->HandleStatModifier(UNIT_MOD_ATTACK_POWER_RANGED, TOTAL_VALUE,
        float(m_modifier.m_amount), apply);
}

void Aura::HandleAuraModAttackPowerPercent(bool apply, bool /*Real*/)
{
    // UNIT_FIELD_ATTACK_POWER_MULTIPLIER = multiplier - 1
    GetTarget()->HandleStatModifier(
        UNIT_MOD_ATTACK_POWER, TOTAL_PCT, float(m_modifier.m_amount), apply);
}

void Aura::HandleAuraModRangedAttackPowerPercent(bool apply, bool /*Real*/)
{
    if ((GetTarget()->getClassMask() & CLASSMASK_WAND_USERS) != 0)
        return;

    // UNIT_FIELD_RANGED_ATTACK_POWER_MULTIPLIER = multiplier - 1
    GetTarget()->HandleStatModifier(UNIT_MOD_ATTACK_POWER_RANGED, TOTAL_PCT,
        float(m_modifier.m_amount), apply);
}

void Aura::HandleAuraModRangedAttackPowerOfStatPercent(
    bool /*apply*/, bool Real)
{
    // spells required only Real aura add/remove
    if (!Real)
        return;

    // Recalculate bonus
    if (GetTarget()->GetTypeId() == TYPEID_PLAYER &&
        !(GetTarget()->getClassMask() & CLASSMASK_WAND_USERS))
        ((Player*)GetTarget())->UpdateAttackPowerAndDamage(true);
}

/********************************/
/***        DAMAGE BONUS      ***/
/********************************/
void Aura::HandleModDamageDone(bool apply, bool Real)
{
    Unit* target = GetTarget();

    // apply item specific bonuses for already equipped weapon
    if (Real && target->GetTypeId() == TYPEID_PLAYER)
    {
        for (int i = 0; i < MAX_ATTACK; ++i)
            if (Item* pItem =
                    ((Player*)target)
                        ->GetWeaponForAttack(WeaponAttackType(i), true, false))
                ((Player*)target)
                    ->_ApplyWeaponDependentAuraDamageMod(
                        pItem, WeaponAttackType(i), this, apply);
    }

    // m_modifier.m_miscvalue is bitmask of spell schools
    // 1 ( 0-bit ) - normal school damage (SPELL_SCHOOL_MASK_NORMAL)
    // 126 - full bitmask all magic damages (SPELL_SCHOOL_MASK_MAGIC) including
    // wands
    // 127 - full bitmask any damages
    //
    // mods must be applied base at equipped weapon class and subclass
    // comparison
    // with spell->EquippedItemClass and  EquippedItemSubClassMask and
    // EquippedItemInventoryTypeMask
    // m_modifier.m_miscvalue comparison with item generated damage types

    if ((m_modifier.m_miscvalue & SPELL_SCHOOL_MASK_NORMAL) != 0)
    {
        // apply generic physical damage bonuses including wand case
        if (GetSpellProto()->EquippedItemClass == -1 ||
            target->GetTypeId() != TYPEID_PLAYER)
        {
            target->HandleStatModifier(UNIT_MOD_DAMAGE_MAINHAND, TOTAL_VALUE,
                float(m_modifier.m_amount), apply);
            target->HandleStatModifier(UNIT_MOD_DAMAGE_OFFHAND, TOTAL_VALUE,
                float(m_modifier.m_amount), apply);
            target->HandleStatModifier(UNIT_MOD_DAMAGE_RANGED, TOTAL_VALUE,
                float(m_modifier.m_amount), apply);
        }
        else
        {
            // done in Player::_ApplyWeaponDependentAuraMods
        }

        if (target->GetTypeId() == TYPEID_PLAYER)
        {
            if (m_positive)
                target->ApplyModUInt32Value(PLAYER_FIELD_MOD_DAMAGE_DONE_POS,
                    m_modifier.m_amount, apply);
            else
                target->ApplyModUInt32Value(PLAYER_FIELD_MOD_DAMAGE_DONE_NEG,
                    m_modifier.m_amount, apply);
        }
    }

    // Skip non magic case for speedup
    if ((m_modifier.m_miscvalue & SPELL_SCHOOL_MASK_MAGIC) == 0)
        return;

    if (GetSpellProto()->EquippedItemClass != -1 ||
        GetSpellProto()->EquippedItemInventoryTypeMask != 0)
    {
        // wand magic case (skip generic to all item spell bonuses)
        // done in Player::_ApplyWeaponDependentAuraMods

        // Skip item specific requirements for not wand magic damage
        return;
    }

    // Magic damage modifiers implemented in Unit::SpellDamageBonusDone
    // This information for client side use only
    if (target->GetTypeId() == TYPEID_PLAYER)
    {
        static_cast<Player*>(target)->UpdateSpellDamageAndHealingBonus();
        Pet* pet = target->GetPet();
        if (pet)
            pet->UpdateAttackPowerAndDamage();
    }
}

void Aura::HandleModDamagePercentDone(bool apply, bool Real)
{
    LOG_DEBUG(logging, "AURA MOD DAMAGE type:%u negative:%u",
        m_modifier.m_miscvalue, m_positive ? 0 : 1);
    Unit* target = GetTarget();

    // Patch 2.1: Death wish and Enrage effects no longer stack
    // Meaning: the buffs stack, but only one can affect the player at any one
    // point in time
    // XXX: THIS CODE IS REALLY MESSY WITH INTERVENED CALLS AND WHAT NOT, PLEASE
    // MAKE IT LESS MESSY
    auto info = GetSpellProto();
    if (Real &&
        (info->Id == 12292 || info->Id == 12880 || info->Id == 14201 ||
            info->Id == 14202 || info->Id == 14203 || info->Id == 14204))
    {
        auto& auras =
            target->GetAurasByType(SPELL_AURA_MOD_DAMAGE_PERCENT_DONE);
        for (auto other : auras)
        {
            if (other == this)
                continue;
            auto other_info = other->GetSpellProto();
            if (!(other_info->Id == 12292 || other_info->Id == 12880 ||
                    other_info->Id == 14201 || other_info->Id == 14202 ||
                    other_info->Id == 14203 || other_info->Id == 14204))
                continue;
            // We've located both enrage and death wish
            // APPLY case:
            if (apply)
            {
                // We're weaker, we become disabled
                if (other->GetModifier()->m_amount >= m_modifier.m_amount)
                {
                    m_modifier.m_amount = 0;
                }
                // We're stronger, we disable the other aura
                else
                {
                    // Remove other aura, note that inside of the removal
                    // code of other (the else below), *this cannot be m_amount
                    // == 0
                    // so there's no chance of ending up recalling this function
                    other->HandleModDamagePercentDone(false, true);
                    other->GetModifier()->m_amount = 0;
                }
            }
            // REMOVE case:
            else
            {
                // We're being removed; if other's modifier amount is 0,
                // we need to restore it and apply it.

                // Save amount for restoration
                int32 amount = m_modifier.m_amount;

                if (other->GetModifier()->m_amount == 0)
                {
                    // We're about to potentially re-enter this code from the
                    // other auras PoV,
                    // so we need to set this one to 0 first so that the other
                    // one is applied.
                    m_modifier.m_amount = 0; // Modified here, restored later
                    other->GetModifier()->m_amount =
                        other_info
                            ->EffectBasePoints[other_info->GetIndexForAura(
                                SPELL_AURA_MOD_DAMAGE_PERCENT_DONE)] +
                        1;
                    other->HandleModDamagePercentDone(true, true);
                }

                // Restore m_amount so it's properly removed below
                m_modifier.m_amount = amount;
            }
        }
    }

    // apply item specific bonuses for already equipped weapon
    if (Real && target->GetTypeId() == TYPEID_PLAYER)
    {
        for (int i = 0; i < MAX_ATTACK; ++i)
            if (Item* pItem =
                    ((Player*)target)
                        ->GetWeaponForAttack(WeaponAttackType(i), true, false))
                ((Player*)target)
                    ->_ApplyWeaponDependentAuraDamageMod(
                        pItem, WeaponAttackType(i), this, apply);
    }

    // Tamed Pet Passive (DND): add pet happiness damage increase to this aura
    if (apply && Real && GetId() == 8875 &&
        target->GetTypeId() == TYPEID_UNIT &&
        static_cast<Creature*>(target)->IsPet() &&
        static_cast<Pet*>(target)->getPetType() == HUNTER_PET)
    {
        int mod = 0;
        switch (static_cast<Pet*>(target)->GetHappinessState())
        {
        case UNHAPPY:
            mod = -25;
            break;
        case CONTENT:
            break;
        case HAPPY:
            mod = 25;
            break;
        }

        static_cast<Pet*>(target)->SetHappiness =
            static_cast<Pet*>(target)->GetHappinessState();
        m_modifier.m_amount += mod;
    }

    // m_modifier.m_miscvalue is bitmask of spell schools
    // 1 ( 0-bit ) - normal school damage (SPELL_SCHOOL_MASK_NORMAL)
    // 126 - full bitmask all magic damages (SPELL_SCHOOL_MASK_MAGIC) including
    // wand
    // 127 - full bitmask any damages
    //
    // mods must be applied base at equipped weapon class and subclass
    // comparison
    // with spell->EquippedItemClass and  EquippedItemSubClassMask and
    // EquippedItemInventoryTypeMask
    // m_modifier.m_miscvalue comparison with item generated damage types

    if ((m_modifier.m_miscvalue & SPELL_SCHOOL_MASK_NORMAL) != 0)
    {
        // apply generic physical damage bonuses including wand case
        if (GetSpellProto()->EquippedItemClass == -1 ||
            target->GetTypeId() != TYPEID_PLAYER)
        {
            target->HandleStatModifier(UNIT_MOD_DAMAGE_MAINHAND, TOTAL_PCT,
                float(m_modifier.m_amount), apply);
            target->HandleStatModifier(UNIT_MOD_DAMAGE_OFFHAND, TOTAL_PCT,
                float(m_modifier.m_amount), apply);
            target->HandleStatModifier(UNIT_MOD_DAMAGE_RANGED, TOTAL_PCT,
                float(m_modifier.m_amount), apply);
        }
        else
        {
            // done in Player::_ApplyWeaponDependentAuraMods
        }
        // For show in client
        if (target->GetTypeId() == TYPEID_PLAYER &&
            !GetSpellProto()->HasAttribute(SPELL_ATTR_UNK7))
            target->ApplyModSignedFloatValue(PLAYER_FIELD_MOD_DAMAGE_DONE_PCT,
                m_modifier.m_amount / 100.0f, apply);
    }

    // Skip non magic case for speedup
    if ((m_modifier.m_miscvalue & SPELL_SCHOOL_MASK_MAGIC) == 0)
        return;

    if (GetSpellProto()->EquippedItemClass != -1 ||
        GetSpellProto()->EquippedItemInventoryTypeMask != 0)
    {
        // wand magic case (skip generic to all item spell bonuses)
        // done in Player::_ApplyWeaponDependentAuraMods

        // Skip item specific requirements for not wand magic damage
        return;
    }

    // Magic damage percent modifiers implemented in Unit::SpellDamageBonusDone
    // Send info to client
    if (target->GetTypeId() == TYPEID_PLAYER)
        for (int i = SPELL_SCHOOL_HOLY; i < MAX_SPELL_SCHOOL; ++i)
            target->ApplyModSignedFloatValue(
                PLAYER_FIELD_MOD_DAMAGE_DONE_PCT + i,
                m_modifier.m_amount / 100.0f, apply);
}

void Aura::HandleModOffhandDamagePercent(bool apply, bool Real)
{
    // spells required only Real aura add/remove
    if (!Real)
        return;

    LOG_DEBUG(logging, "AURA MOD OFFHAND DAMAGE");

    GetTarget()->HandleStatModifier(
        UNIT_MOD_DAMAGE_OFFHAND, TOTAL_PCT, float(m_modifier.m_amount), apply);
}

/********************************/
/***        POWER COST        ***/
/********************************/

void Aura::HandleModPowerCostPCT(bool apply, bool Real)
{
    // spells required only Real aura add/remove
    if (!Real)
        return;

    float amount = m_modifier.m_amount / 100.0f;
    for (int i = 0; i < MAX_SPELL_SCHOOL; ++i)
        if (m_modifier.m_miscvalue & (1 << i))
            GetTarget()->ApplyModSignedFloatValue(
                UNIT_FIELD_POWER_COST_MULTIPLIER + i, amount, apply);
}

void Aura::HandleModPowerCost(bool apply, bool Real)
{
    // spells required only Real aura add/remove
    if (!Real)
        return;

    for (int i = 0; i < MAX_SPELL_SCHOOL; ++i)
        if (m_modifier.m_miscvalue & (1 << i))
            GetTarget()->ApplyModInt32Value(
                UNIT_FIELD_POWER_COST_MODIFIER + i, m_modifier.m_amount, apply);
}

/*********************************************************/
/***                    OTHERS                         ***/
/*********************************************************/

void Aura::HandleShapeshiftBoosts(bool apply)
{
    uint32 spellId1 = 0;
    uint32 spellId2 = 0;
    uint32 HotWSpellId = 0;

    ShapeshiftForm form = ShapeshiftForm(GetModifier()->m_miscvalue);

    Unit* target = GetTarget();

    switch (form)
    {
    case FORM_CAT:
        spellId1 = 3025;
        HotWSpellId = 24900;
        break;
    case FORM_TREE:
        spellId1 = 5420;
        break;
    case FORM_TRAVEL:
        spellId1 = 5419;
        break;
    case FORM_AQUA:
        spellId1 = 5421;
        break;
    case FORM_BEAR:
        spellId1 = 1178;
        spellId2 = 21178;
        HotWSpellId = 24899;
        break;
    case FORM_DIREBEAR:
        spellId1 = 9635;
        spellId2 = 21178;
        HotWSpellId = 24899;
        break;
    case FORM_BATTLESTANCE:
        spellId1 = 21156;
        break;
    case FORM_DEFENSIVESTANCE:
        spellId1 = 7376;
        break;
    case FORM_BERSERKERSTANCE:
        spellId1 = 7381;
        break;
    case FORM_MOONKIN:
        spellId1 = 24905;
        break;
    case FORM_FLIGHT:
        spellId1 = 33948;
        spellId2 = 34764;
        break;
    case FORM_FLIGHT_EPIC:
        spellId1 = 40122;
        spellId2 = 40121;
        break;
    case FORM_SPIRITOFREDEMPTION:
        spellId1 = 27792;
        spellId2 = 27795; // must be second, this important at aura remove to
                          // prevent to early iterator invalidation.
        break;
    case FORM_GHOSTWOLF:
    case FORM_AMBIENT:
    case FORM_GHOUL:
    case FORM_SHADOW:
    case FORM_STEALTH:
    case FORM_CREATURECAT:
    case FORM_CREATUREBEAR:
        break;
    default:
        break;
    }

    if (apply)
    {
        if (spellId1)
            target->CastSpell(target, spellId1, true, nullptr, this);
        if (spellId2)
            target->CastSpell(target, spellId2, true, nullptr, this);

        if (target->GetTypeId() == TYPEID_PLAYER)
        {
            const PlayerSpellMap& sp_list = ((Player*)target)->GetSpellMap();
            for (const auto& elem : sp_list)
            {
                if (elem.second.state == PLAYERSPELL_REMOVED)
                    continue;
                if (elem.first == spellId1 || elem.first == spellId2)
                    continue;
                SpellEntry const* spellInfo =
                    sSpellStore.LookupEntry(elem.first);
                if (!spellInfo || !IsNeedCastSpellAtFormApply(spellInfo, form))
                    continue;
                // Check so dependencies are met
                bool has_dep = true;
                auto dep_bounds =
                    sSpellMgr::Instance()->GetSpellDependencies(spellInfo->Id);
                for (auto itr = dep_bounds.first;
                     itr != dep_bounds.second && has_dep; ++itr)
                    if (!target->HasSpell(itr->second))
                        has_dep = false;
                if (!has_dep)
                    continue;
                target->CastSpell(target, elem.first, true, nullptr, this);
            }

            // Leader of the Pack
            if (((Player*)target)->HasSpell(17007))
            {
                SpellEntry const* spellInfo = sSpellStore.LookupEntry(24932);
                if (spellInfo && spellInfo->Stances & (1 << (form - 1)))
                    target->CastSpell(target, 24932, true, nullptr, this);
            }

            // Heart of the Wild
            if (HotWSpellId)
            {
                auto& modTotalStatPct = target->GetAurasByType(
                    SPELL_AURA_MOD_TOTAL_STAT_PERCENTAGE);
                for (const auto& elem : modTotalStatPct)
                {
                    if ((elem)->GetSpellProto()->SpellIconID == 240 &&
                        (elem)->GetModifier()->m_miscvalue == 3)
                    {
                        int32 HotWMod = (elem)->GetModifier()->m_amount;
                        if (GetModifier()->m_miscvalue == FORM_CAT)
                            HotWMod /= 2;

                        target->CastCustomSpell(target, HotWSpellId, &HotWMod,
                            nullptr, nullptr, true, nullptr, this);
                        break;
                    }
                }
            }
        }
    }
    else
    {
        if (spellId1)
            target->remove_auras(spellId1);
        if (spellId2)
            target->remove_auras(spellId2);

        uint32 new_shapemask = 0;
        AuraHolder* new_aura = GetHolder()->overwriting_aura();
        if (new_aura)
        {
            int index = new_aura->GetSpellProto()->GetIndexForAura(
                SPELL_AURA_MOD_SHAPESHIFT);
            if (index != MAX_EFFECT_INDEX)
                new_shapemask =
                    1 << (new_aura->GetSpellProto()->EffectMiscValue[index] -
                          1);
        }

        target->remove_auras_if([new_shapemask](AuraHolder* holder)
            {
                if (holder->IsRemovedOnShapeLost() &&
                    (holder->GetSpellProto()->Stances & new_shapemask) == 0)
                    return true;
                return false;
            });
    }
}

void Aura::HandleAuraEmpathy(bool apply, bool /*Real*/)
{
    auto target = GetTarget();
    if (target->GetTypeId() != TYPEID_UNIT)
        return;

    // NOTE: The dynamic flags will be filtered out for everyone but the hunter
    // in Object::BuildValuesUpdate()
    CreatureInfo const* ci = ObjectMgr::GetCreatureTemplate(target->GetEntry());
    if (ci && ci->type == CREATURE_TYPE_BEAST)
    {
        target->ApplyModUInt32Value(
            UNIT_DYNAMIC_FLAGS, UNIT_DYNFLAG_SPECIALINFO, apply);
        // Force health to be updated
        target->UpdateValueIndex(UNIT_FIELD_MAXHEALTH);
        target->UpdateValueIndex(UNIT_FIELD_HEALTH);
    }
}

void Aura::HandleAuraUntrackable(bool apply, bool /*Real*/)
{
    if (apply)
        GetTarget()->SetByteFlag(
            UNIT_FIELD_BYTES_1, 2, UNIT_BYTE1_FLAG_UNTRACKABLE);
    else
        GetTarget()->RemoveByteFlag(
            UNIT_FIELD_BYTES_1, 2, UNIT_BYTE1_FLAG_UNTRACKABLE);
}

void Aura::HandleAuraModPacify(bool apply, bool /*Real*/)
{
    if (apply)
        GetTarget()->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PACIFIED);
    else
        GetTarget()->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PACIFIED);
}

void Aura::HandleAuraModPacifyAndSilence(bool apply, bool Real)
{
    HandleAuraModPacify(apply, Real);
    HandleAuraModSilence(apply, Real);
}

void Aura::HandleAuraGhost(bool apply, bool /*Real*/)
{
    if (GetTarget()->GetTypeId() != TYPEID_PLAYER)
        return;

    if (apply)
    {
        GetTarget()->SetFlag(PLAYER_FLAGS, PLAYER_FLAGS_GHOST);
    }
    else
    {
        GetTarget()->RemoveFlag(PLAYER_FLAGS, PLAYER_FLAGS_GHOST);
    }
}

void Aura::HandleAuraAllowFlight(bool apply, bool Real)
{
    // all applied/removed only at real aura add/remove
    if (!Real)
        return;

    Unit* target = GetTarget();

    if (target->GetTypeId() == TYPEID_PLAYER)
    {
        // allow fly
        WorldPacket data;
        if (apply)
            data.initialize(SMSG_MOVE_SET_CAN_FLY, 12);
        else
            data.initialize(SMSG_MOVE_UNSET_CAN_FLY, 12);
        data << target->GetPackGUID();
        data << uint32(0); // unk
        static_cast<Player*>(target)->SendDirectMessage(std::move(data));

        if (!apply)
            target->m_movementInfo.RemoveMovementFlag(MOVEFLAG_MASK_FLYING);
    }
    else
    {
        if (apply)
            target->m_movementInfo.SetMovementFlags(MOVEFLAG_FLYING);
        else
            target->m_movementInfo.RemoveMovementFlag(MOVEFLAG_FLYING);
    }
}

void Aura::HandleModRating(bool apply, bool Real)
{
    // spells required only Real aura add/remove
    if (!Real)
        return;

    if (GetTarget()->GetTypeId() != TYPEID_PLAYER)
        return;

    for (uint32 rating = 0; rating < MAX_COMBAT_RATING; ++rating)
        if (m_modifier.m_miscvalue & (1 << rating))
            ((Player*)GetTarget())
                ->ApplyRatingMod(
                    CombatRating(rating), m_modifier.m_amount, apply);
}

void Aura::HandleForceMoveForward(bool apply, bool Real)
{
    if (!Real)
        return;

    if (apply)
        GetTarget()->SetFlag(UNIT_FIELD_FLAGS_2, UNIT_FLAG2_FORCE_MOVE);
    else
        GetTarget()->RemoveFlag(UNIT_FIELD_FLAGS_2, UNIT_FLAG2_FORCE_MOVE);
}

void Aura::HandleAuraModExpertise(bool /*apply*/, bool /*Real*/)
{
    if (GetTarget()->GetTypeId() != TYPEID_PLAYER)
        return;

    ((Player*)GetTarget())->UpdateExpertise(BASE_ATTACK);
    ((Player*)GetTarget())->UpdateExpertise(OFF_ATTACK);
}

void Aura::HandleModTargetResistance(bool apply, bool Real)
{
    // spells required only Real aura add/remove
    if (!Real)
        return;
    Unit* target = GetTarget();

    if (target->GetTypeId() == TYPEID_PLAYER &&
        (m_modifier.m_miscvalue & SPELL_SCHOOL_MASK_NORMAL))
        target->ApplyModInt32Value(PLAYER_FIELD_MOD_TARGET_PHYSICAL_RESISTANCE,
            m_modifier.m_amount, apply);

    // show as spell penetration only full spell penetration bonuses (all
    // resistances except armor and holy
    if (target->GetTypeId() == TYPEID_PLAYER &&
        (m_modifier.m_miscvalue & SPELL_SCHOOL_MASK_SPELL) ==
            SPELL_SCHOOL_MASK_SPELL)
        target->ApplyModInt32Value(
            PLAYER_FIELD_MOD_TARGET_RESISTANCE, m_modifier.m_amount, apply);
}

void Aura::HandleShieldBlockValue(bool apply, bool /*Real*/)
{
    BaseModType modType = FLAT_MOD;
    if (m_modifier.m_auraname == SPELL_AURA_MOD_SHIELD_BLOCKVALUE_PCT)
        modType = PCT_MOD;

    if (GetTarget()->GetTypeId() == TYPEID_PLAYER)
        ((Player*)GetTarget())
            ->HandleBaseModValue(
                SHIELD_BLOCK_VALUE, modType, float(m_modifier.m_amount), apply);
}

void Aura::HandleAuraRetainComboPoints(bool apply, bool Real)
{
    // spells required only Real aura add/remove
    if (!Real)
        return;

    if (GetTarget()->GetTypeId() != TYPEID_PLAYER)
        return;

    Player* target = (Player*)GetTarget();

    // combo points was added in SPELL_EFFECT_ADD_COMBO_POINTS handler
    // remove only if aura expire by time (in case combo points amount change
    // aura removed without combo points lost)
    if (!apply && m_removeMode == AURA_REMOVE_BY_EXPIRE &&
        target->GetComboTargetGuid())
        if (Unit* unit = ObjectAccessor::GetUnit(
                *GetTarget(), target->GetComboTargetGuid()))
            target->AddComboPoints(unit, -m_modifier.m_amount);
}

void Aura::HandleModUnattackable(bool Apply, bool Real)
{
    if (Real && Apply)
    {
        if (!(GetSpellProto()->Id == 27792 &&
                GetTarget()->GetTypeId() == TYPEID_UNIT))
            GetTarget()->CombatStop();

        GetTarget()->remove_auras_if([](AuraHolder* h)
            {
                return h->GetSpellProto()->AuraInterruptFlags &
                       AURA_INTERRUPT_FLAG_IMMUNE_OR_LOST_SELECTION;
            });
    }
    GetTarget()->ApplyModFlag(
        UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE, Apply);
}

void Aura::HandleSpiritOfRedemption(bool apply, bool Real)
{
    // spells required only Real aura add/remove
    if (!Real)
        return;

    Unit* target = GetTarget();

    // prepare spirit state
    if (apply)
    {
        if (target->GetTypeId() == TYPEID_PLAYER)
        {
            // disable breath/etc timers
            ((Player*)target)->StopMirrorTimers();

            // set stand state (expected in this form)
            if (!target->IsStandState())
                target->SetStandState(UNIT_STAND_STATE_STAND);
        }

        target->SetHealth(target->GetMaxHealth());
        if (target->getPowerType() == POWER_MANA)
            target->SetPower(POWER_MANA, target->GetMaxPower(POWER_MANA));

        if (target->IsNonMeleeSpellCasted(false))
            target->InterruptNonMeleeSpells(false);
    }
    // die at aura end
    else
    {
        target->Kill(target, false, GetSpellProto());
    }
}

void Aura::HandleModAoeCharm(bool apply, bool Real)
{
    if (!Real)
        return;

    Unit* caster = GetCaster();
    Unit* utar = GetTarget();
    if (!caster || !utar || utar == caster ||
        caster->GetTypeId() != TYPEID_UNIT ||
        utar->GetTypeId() != TYPEID_PLAYER)
        return;

    Player* target = (Player*)utar;

    if (apply)
    {
        target->SetCharmerGuid(caster->GetObjectGuid());

        target->CombatStop(true, true);
        target->getHostileRefManager().setOnlineOfflineState(false);

        target->setFaction(caster->getFaction());
        target->CallForAllControlledUnits(
            [caster](Unit* pet)
            {
                pet->CombatStop(true);
                pet->getHostileRefManager().deleteReferences();
                pet->setFaction(caster->getFaction());
            },
            CONTROLLED_PET | CONTROLLED_GUARDIANS | CONTROLLED_CHARM |
                CONTROLLED_TOTEMS);

        target->movement_gens.push(
            new movement::ControlledMovementGenerator(caster, GetHolder()));

        target->AIM_Initialize();

        caster->AddCharm(target);
    }
    else
    {
        target->SetCharmerGuid(ObjectGuid());

        uint32 fac = target->getFactionForRace(target->getRace());
        target->setFaction(fac);
        target->CallForAllControlledUnits(
            [fac](Unit* pet)
            {
                pet->CombatStop(true);
                pet->getHostileRefManager().deleteReferences();
                pet->setFaction(fac);
            },
            CONTROLLED_PET | CONTROLLED_GUARDIANS | CONTROLLED_CHARM |
                CONTROLLED_TOTEMS);

        target->CombatStop(true, true);
        target->getHostileRefManager().setOnlineOfflineState(false);
        // delete all friendly targets on charm over
        if (target->GetTypeId() ==
            TYPEID_PLAYER) // use friendly check for players
        {
            target->getHostileRefManager().deleteReferencesCallback(
                [target](Unit* other)
                {
                    return target->IsFriendlyTo(other);
                });
        }
        else // and not-hostile for NPCs
        {
            target->getHostileRefManager().deleteReferencesCallback(
                [target](Unit* other)
                {
                    return !target->IsHostileTo(other);
                });
        }

        target->movement_gens.remove_if([this](const movement::Generator* gen)
            {
                if (auto controlled = dynamic_cast<
                        const movement::ControlledMovementGenerator*>(gen))
                    return controlled->holder() == GetHolder();
                return false;
            });

        target->AIM_Deinitialize();

        caster->RemoveCharm(target);
    }
}

void Aura::HandleSchoolAbsorb(bool apply, bool Real)
{
    if (!Real)
        return;

    Unit* caster = GetCaster();
    if (!caster)
        return;

    Unit* target = GetTarget();
    SpellEntry const* spellProto = GetSpellProto();
    if (apply)
    {
        // prevent double apply bonuses
        if (target->GetTypeId() != TYPEID_PLAYER ||
            !((Player*)target)->GetSession()->PlayerLoading())
        {
            float DoneActualBenefit = 0.0f;
            switch (spellProto->SpellFamilyName)
            {
            case SPELLFAMILY_PRIEST:
                // Power Word: Shield
                if (spellProto->SpellFamilyFlags & UI64LIT(0x0000000000000001))
                {
                    //+30% from +healing bonus
                    DoneActualBenefit =
                        caster->SpellBaseHealingBonusDone(
                            GetSpellSchoolMask(spellProto), spellProto->Id) *
                        0.3f;
                    break;
                }
                break;
            case SPELLFAMILY_MAGE:
                // Frost Ward, Fire Ward
                if (spellProto->IsFitToFamilyMask(UI64LIT(0x0000000100080108)))
                    //+10% from +spell bonus
                    DoneActualBenefit =
                        caster->SpellBaseDamageBonusDone(
                            GetSpellSchoolMask(spellProto), spellProto->Id) *
                        0.1f;
                break;
            case SPELLFAMILY_WARLOCK:
                // Shadow Ward
                if (spellProto->SpellFamilyFlags == UI64LIT(0x00))
                    //+10% from +spell bonus
                    DoneActualBenefit =
                        caster->SpellBaseDamageBonusDone(
                            GetSpellSchoolMask(spellProto), spellProto->Id) *
                        0.1f;
                break;
            default:
                break;
            }

            DoneActualBenefit *=
                caster->calculate_spell_downrank_factor(GetSpellProto());

            m_modifier.m_amount += (int32)DoneActualBenefit;
        }
    }
}

void Aura::PeriodicTick()
{
    Unit* target = GetTarget();
    SpellEntry const* spellProto = GetSpellProto();

    switch (m_modifier.m_auraname)
    {
    case SPELL_AURA_PERIODIC_DAMAGE:
    case SPELL_AURA_PERIODIC_DAMAGE_PERCENT:
    {
        // don't damage target if not alive, possible death persistent effects
        if (!target->isAlive())
            return;

        Unit* pCaster = GetCaster();
        if (!pCaster)
            return;

        if (spellProto->Effect[GetEffIndex()] ==
                SPELL_EFFECT_PERSISTENT_AREA_AURA &&
            pCaster->SpellHitResult(target, spellProto, false) !=
                SPELL_MISS_NONE)
            return;

        // Check for immune (not use charges)
        if (target->IsImmunedToDamage(GetSpellSchoolMask(spellProto)))
            return;

        // some auras remove at specific health level or more
        if (m_modifier.m_auraname == SPELL_AURA_PERIODIC_DAMAGE)
        {
            switch (GetId())
            {
            // 100% hp
            case 31956: // Grievous Wound
            case 38801: // Grievous Wound
            case 43093: // Grievous Throw
            {
                if (target->GetHealth() == target->GetMaxHealth())
                {
                    target->remove_auras(GetId());
                    return;
                }
                break;
            }
            // 50% hp
            case 38048: // Curse of Pain
            {
                if (target->GetHealthPercent() < 50.0f)
                {
                    target->remove_auras(GetId());
                    return;
                }
                break;
            }
            case 38772:
            {
                uint32 percent =
                    GetEffIndex() < EFFECT_INDEX_2 &&
                            spellProto->Effect[GetEffIndex()] ==
                                SPELL_EFFECT_DUMMY ?
                        pCaster->CalculateSpellDamage(target, spellProto,
                            SpellEffectIndex(GetEffIndex() + 1)) :
                        100;
                if (target->GetHealth() * 100 >=
                    target->GetMaxHealth() * percent)
                {
                    target->remove_auras(GetId());
                    return;
                }
                break;
            }
            default:
                break;
            }
        }

        uint32 absorb = 0;
        uint32 resist = 0;
        CleanDamage cleanDamage = CleanDamage(0, BASE_ATTACK, MELEE_HIT_NORMAL);

        // ignore non positive values (can be result apply spellmods to aura
        // damage
        uint32 amount = m_modifier.m_amount > 0 ? m_modifier.m_amount : 0;

        uint32 pdamage;

        if (m_modifier.m_auraname == SPELL_AURA_PERIODIC_DAMAGE)
            pdamage = amount;
        else
            pdamage = uint32(target->GetMaxHealth() * amount / 100);

        // SpellDamageBonus for magic spells
        if (spellProto->DmgClass == SPELL_DAMAGE_CLASS_NONE ||
            spellProto->DmgClass == SPELL_DAMAGE_CLASS_MAGIC)
            pdamage = target->SpellDamageBonusTaken(
                pCaster, spellProto, pdamage, DOT, GetStackAmount());
        // MeleeDamagebonus for weapon based spells
        else
        {
            WeaponAttackType attackType = GetWeaponAttackType(spellProto);
            pdamage = target->MeleeDamageBonusTaken(pCaster, pdamage,
                attackType, spellProto, DOT, GetStackAmount());
        }

        // Calculate armor mitigation if it is a physical spell
        // But not for bleed mechanic spells
        if (GetSpellSchoolMask(spellProto) & SPELL_SCHOOL_MASK_NORMAL &&
            spellProto->EffectMechanic[m_effIndex] != MECHANIC_BLEED &&
            spellProto->Mechanic != MECHANIC_BLEED &&
            !spellProto->HasAttribute(SPELL_ATTR_CUSTOM_IGNORES_ARMOR))
        {
            uint32 pdamageReductedArmor =
                pCaster->calculate_armor_reduced_damage(target, pdamage);
            cleanDamage.damage += pdamage - pdamageReductedArmor;
            pdamage = pdamageReductedArmor;
        }

        // Curse of Agony damage-per-tick calculation
        if (spellProto->SpellFamilyName == SPELLFAMILY_WARLOCK &&
            (spellProto->SpellFamilyFlags & UI64LIT(0x0000000000000400)) &&
            spellProto->SpellIconID == 544)
        {
            // 1..4 ticks, 1/2 from normal tick damage
            if (GetAuraTicks() <= 4)
                pdamage = pdamage / 2;
            // 9..12 ticks, 3/2 from normal tick damage
            else if (GetAuraTicks() >= 9)
                pdamage +=
                    (pdamage + 1) /
                    2; // +1 prevent 0.5 damage possible lost at 1..4 ticks
                       // 5..8 ticks have normal tick damage
        }

        // As of 2.2 resilience reduces damage from DoT ticks as much as the
        // chance to not be critically hit
        // Reduce dot damage from resilience for players
        if (target->GetTypeId() == TYPEID_PLAYER &&
            !spellProto->HasAttribute(SPELL_ATTR_EX5_DOT_IGNORE_RESILIENCE))
            pdamage -= ((Player*)target)->GetDotDamageReduction(pdamage);

        pdamage = target->do_resist_absorb_helper(
            pCaster, pdamage, spellProto, true, 0, &absorb, &resist);

        // HACK: FIXME:The damage from Warlock's Hellfire should not be
        // resistable by the caster
        if (resist && pCaster == target &&
            spellProto->SpellFamilyName == SPELLFAMILY_WARLOCK &&
            spellProto->SpellFamilyFlags & 0x40)
        {
            pdamage += resist;
            resist = 0;
        }

        LOG_DEBUG(logging,
            "PeriodicTick: %s attacked %s for %u dmg inflicted by %u",
            GetCasterGuid().GetString().c_str(), target->GetGuidStr().c_str(),
            pdamage, GetId());

        pCaster->DealDamageMods(target, pdamage, &absorb);

        // Set trigger flag
        uint32 procAttacker = PROC_FLAG_ON_DO_PERIODIC; //  |
        //  PROC_FLAG_SUCCESSFUL_HARMFUL_SPELL_HIT;
        uint32 procVictim =
            PROC_FLAG_ON_TAKE_PERIODIC; // | PROC_FLAG_TAKEN_HARMFUL_SPELL_HIT;

        SpellPeriodicAuraLogInfo pInfo(this, pdamage, absorb, resist, 0.0f);
        target->SendPeriodicAuraLog(&pInfo);

        if (pdamage)
            procVictim |= PROC_FLAG_TAKEN_ANY_DAMAGE;

        proc_amount proc_amnt(true, pdamage, target, absorb);

        bool dura_loss = true;
        // Hellfire: No durability loss (TODO: Find general rule)
        if (spellProto->SpellFamilyName == SPELLFAMILY_WARLOCK &&
            spellProto->SpellFamilyFlags & 0x40)
            dura_loss = false;

        pCaster->DealDamage(target, pdamage, &cleanDamage, DOT,
            GetSpellSchoolMask(spellProto), spellProto, dura_loss, absorb);

        pCaster->ProcDamageAndSpell(target, procAttacker, procVictim,
            PROC_EX_NORMAL_HIT, proc_amnt, BASE_ATTACK, spellProto);
        break;
    }
    case SPELL_AURA_PERIODIC_LEECH:
    case SPELL_AURA_PERIODIC_HEALTH_FUNNEL:
    {
        // don't damage target if not alive, possible death persistent effects
        if (!target->isAlive())
            return;

        Unit* pCaster = GetCaster();
        if (!pCaster)
            return;

        // For health funnel the caster is drained and target healed (opposite
        // of leech)
        if (m_modifier.m_auraname == SPELL_AURA_PERIODIC_HEALTH_FUNNEL)
            std::swap(pCaster, target);

        if (spellProto->Effect[GetEffIndex()] ==
                SPELL_EFFECT_PERSISTENT_AREA_AURA &&
            pCaster->SpellHitResult(target, spellProto, false) !=
                SPELL_MISS_NONE)
            return;

        // Check for immune
        if (target->IsImmunedToDamage(GetSpellSchoolMask(spellProto)))
            return;

        uint32 absorb = 0;
        uint32 resist = 0;
        CleanDamage cleanDamage = CleanDamage(0, BASE_ATTACK, MELEE_HIT_NORMAL);

        uint32 pdamage = m_modifier.m_amount > 0 ? m_modifier.m_amount : 0;

        // Calculate armor mitigation if it is a physical spell
        if (GetSpellSchoolMask(spellProto) & SPELL_SCHOOL_MASK_NORMAL &&
            !spellProto->HasAttribute(SPELL_ATTR_CUSTOM_IGNORES_ARMOR))
        {
            uint32 pdamageReductedArmor =
                pCaster->calculate_armor_reduced_damage(target, pdamage);
            cleanDamage.damage += pdamage - pdamageReductedArmor;
            pdamage = pdamageReductedArmor;
        }

        pdamage = target->SpellDamageBonusTaken(
            pCaster, spellProto, pdamage, DOT, GetStackAmount());

        // As of 2.2 resilience reduces damage from DoT ticks as much as the
        // chance to not be critically hit
        // Reduce dot damage from resilience for players
        if (target->GetTypeId() == TYPEID_PLAYER)
            pdamage -= ((Player*)target)->GetDotDamageReduction(pdamage);

        pdamage = target->do_resist_absorb_helper(
            pCaster, pdamage, spellProto, true, 0, &absorb, &resist);

        pCaster->DealDamageMods(target, pdamage, &absorb);

        LOG_DEBUG(logging,
            "PeriodicTick: %s health leech of %s for %u dmg inflicted by %u "
            "abs is %u",
            GetCasterGuid().GetString().c_str(), target->GetGuidStr().c_str(),
            pdamage, GetId(), absorb);

        SpellPeriodicAuraLogInfo combat_log_entry(
            this, pdamage, absorb, resist, 0.0f);
        target->SendPeriodicAuraLog(&combat_log_entry);

        float multiplier = spellProto->EffectMultipleValue[GetEffIndex()] > 0 ?
                               spellProto->EffectMultipleValue[GetEffIndex()] :
                               1;

        // Set trigger flag
        uint32 procAttacker = PROC_FLAG_ON_DO_PERIODIC; // |
        // PROC_FLAG_SUCCESSFUL_HARMFUL_SPELL_HIT;
        uint32 procVictim =
            PROC_FLAG_ON_TAKE_PERIODIC; // | PROC_FLAG_TAKEN_HARMFUL_SPELL_HIT;

        if (pdamage)
            procVictim |= PROC_FLAG_TAKEN_ANY_DAMAGE;

        proc_amount procamnt(true, pdamage, target, absorb);

        int32 new_damage = pCaster->DealDamage(target, pdamage, &cleanDamage,
            DOT, GetSpellSchoolMask(spellProto), spellProto, true, absorb);

        pCaster->ProcDamageAndSpell(target, procAttacker, procVictim,
            PROC_EX_NORMAL_HIT, procamnt, BASE_ATTACK, spellProto);

        if (!target->isAlive() && pCaster->IsNonMeleeSpellCasted(false))
            for (uint32 i = CURRENT_FIRST_NON_MELEE_SPELL;
                 i < CURRENT_MAX_SPELL; ++i)
                if (Spell* spell =
                        pCaster->GetCurrentSpell(CurrentSpellTypes(i)))
                    if (spell->m_spellInfo->Id == GetId())
                        spell->cancel();

        if (Player* modOwner = pCaster->GetSpellModOwner())
            modOwner->ApplySpellMod(
                GetId(), SPELLMOD_MULTIPLE_VALUE, multiplier);

        if (pCaster->isAlive())
        {
            uint32 heal = pCaster->SpellHealingBonusTaken(pCaster, spellProto,
                int32(new_damage * multiplier), DOT, GetStackAmount());
            pCaster->DealHeal(pCaster, heal, spellProto);
        }

        break;
    }
    case SPELL_AURA_PERIODIC_HEAL:
    case SPELL_AURA_OBS_MOD_HEALTH:
    {
        // don't heal target if not alive, mostly death persistent effects from
        // items
        if (!target->isAlive())
            return;

        Unit* pCaster = GetCaster();
        if (!pCaster)
            return;

        if (target->IsImmuneToHealing())
            return;

        // heal for caster damage (must be alive)
        if (target != pCaster && spellProto->SpellVisual == 163 &&
            !pCaster->isAlive())
            return;

        // ignore non positive values (can be result apply spellmods to aura
        // damage
        uint32 amount = m_modifier.m_amount > 0 ? m_modifier.m_amount : 0;

        uint32 pdamage;

        if (m_modifier.m_auraname == SPELL_AURA_OBS_MOD_HEALTH)
            pdamage = uint32(target->GetMaxHealth() * amount / 100);
        else
            pdamage = amount;

        pdamage = target->SpellHealingBonusTaken(
            pCaster, spellProto, pdamage, DOT, GetStackAmount());

        LOG_DEBUG(logging,
            "PeriodicTick: %s heal of %s for %u health inflicted by %u",
            GetCasterGuid().GetString().c_str(), target->GetGuidStr().c_str(),
            pdamage, GetId());

        proc_amount procamnt = proc_amount(false, pdamage, target);

        int32 gain = target->ModifyHealth(pdamage);
        SpellPeriodicAuraLogInfo pInfo(this, pdamage, 0, 0, 0.0f);
        target->SendPeriodicAuraLog(&pInfo);

        // Set trigger flag
        uint32 procAttacker = PROC_FLAG_ON_DO_PERIODIC;
        uint32 procVictim = PROC_FLAG_ON_TAKE_PERIODIC;
        uint32 procEx = PROC_EX_NORMAL_HIT | PROC_EX_PERIODIC_POSITIVE;
        pCaster->ProcDamageAndSpell(target, procAttacker, procVictim, procEx,
            procamnt, BASE_ATTACK, spellProto);

        // Improved mend pet effect
        if (spellProto->SpellFamilyName == SPELLFAMILY_HUNTER &&
            (spellProto->SpellFamilyFlags & UI64LIT(0x0000000000800000)) &&
            spellProto->SpellIconID == 267)
        {
            if (GetCaster() && GetCaster()->has_aura(19573))
            {
                if (roll_chance_i(50))
                    GetCaster()->CastSpell(GetTarget(), 24406, true);
            }
            else if (GetCaster() && GetCaster()->has_aura(19572))
            {
                if (roll_chance_i(25))
                    GetCaster()->CastSpell(GetTarget(), 24406, true);
            }
        }

        // add HoTs to amount healed in bgs
        if (pCaster->GetTypeId() == TYPEID_PLAYER)
            if (BattleGround* bg = ((Player*)pCaster)->GetBattleGround())
                bg->UpdatePlayerScore(
                    ((Player*)pCaster), SCORE_HEALING_DONE, gain);

        target->getHostileRefManager().threatAssist(pCaster,
            float(gain) * 0.5f *
                sSpellMgr::Instance()->GetSpellThreatMultiplier(spellProto),
            spellProto);
        break;
    }
    case SPELL_AURA_PERIODIC_MANA_LEECH:
    {
        // don't damage target if not alive, possible death persistent effects
        if (!target->isAlive())
            return;

        if (m_modifier.m_miscvalue < 0 || m_modifier.m_miscvalue >= MAX_POWERS)
            return;

        Powers power = Powers(m_modifier.m_miscvalue);

        // power type might have changed between aura applying and tick (druid's
        // shapeshift)
        if (target->getPowerType() != power)
            return;

        Unit* pCaster = GetCaster();
        if (!pCaster)
            return;

        if (!pCaster->isAlive())
            return;

        if (GetSpellProto()->Effect[GetEffIndex()] ==
                SPELL_EFFECT_PERSISTENT_AREA_AURA &&
            pCaster->SpellHitResult(target, spellProto, false) !=
                SPELL_MISS_NONE)
            return;

        // Check for immune (not use charges)
        if (target->IsImmunedToDamage(GetSpellSchoolMask(spellProto)))
            return;

        // ignore non positive values (can be result apply spellmods to aura
        // damage
        uint32 pdamage = m_modifier.m_amount > 0 ? m_modifier.m_amount : 0;

        LOG_DEBUG(logging,
            "PeriodicTick: %s power leech of %s for %u dmg inflicted by %u",
            GetCasterGuid().GetString().c_str(), target->GetGuidStr().c_str(),
            pdamage, GetId());

        int32 drain_amount = target->GetPower(power) > pdamage ?
                                 pdamage :
                                 target->GetPower(power);

        // resilience reduce mana draining effect at spell crit damage reduction
        // (added in 2.4)
        if (power == POWER_MANA && target->GetTypeId() == TYPEID_PLAYER)
            drain_amount -=
                ((Player*)target)->GetSpellCritDamageReduction(drain_amount);

        target->ModifyPower(power, -drain_amount);

        float gain_multiplier = 0.0f;

        // Mana drain breaks CC the same way direct damage does
        if (drain_amount > 0 &&
            !(GetSpellProto()->AttributesEx4 &
                SPELL_ATTR_EX4_NO_PUSHBACK_OR_CC_BREAKAGE))
        {
            target->remove_auras_on_event(AURA_INTERRUPT_FLAG_DAMAGE, GetId());
            target->RemoveSpellbyDamageTaken(
                SPELL_AURA_MOD_ROOT, drain_amount, true);
            target->RemoveSpellbyDamageTaken(
                SPELL_AURA_MOD_FEAR, drain_amount, true);
        }

        if (pCaster->GetMaxPower(power) > 0)
        {
            gain_multiplier = spellProto->EffectMultipleValue[GetEffIndex()];

            if (Player* modOwner = pCaster->GetSpellModOwner())
                modOwner->ApplySpellMod(
                    GetId(), SPELLMOD_MULTIPLE_VALUE, gain_multiplier);
        }

        SpellPeriodicAuraLogInfo pInfo(
            this, drain_amount, 0, 0, gain_multiplier);
        target->SendPeriodicAuraLog(&pInfo);

        if (int32 gain_amount = int32(drain_amount * gain_multiplier))
        {
            int32 gain = pCaster->ModifyPower(power, gain_amount);

            if (GetId() == 5138) // Drain Mana
                if (Aura* petPart = GetHolder()->GetAura(EFFECT_INDEX_1))
                    if (int pet_gain = gain_amount *
                                       petPart->GetModifier()->m_amount / 100)
                        pCaster->CastCustomSpell(
                            pCaster, 32554, &pet_gain, nullptr, nullptr, true);

            target->AddThreat(pCaster, float(gain) * 0.5f, false,
                GetSpellSchoolMask(spellProto), spellProto);
        }

        // Some special cases
        switch (GetId())
        {
        case 32960: // Mark of Kazzak
        {
            if (target->GetTypeId() == TYPEID_PLAYER &&
                target->getPowerType() == POWER_MANA)
            {
                // Drain 5% of target's mana
                pdamage = target->GetMaxPower(POWER_MANA) * 5 / 100;
                drain_amount = target->GetPower(POWER_MANA) > pdamage ?
                                   pdamage :
                                   target->GetPower(POWER_MANA);
                target->ModifyPower(POWER_MANA, -drain_amount);

                SpellPeriodicAuraLogInfo pInfo(this, drain_amount, 0, 0, 0.0f);
                target->SendPeriodicAuraLog(&pInfo);
            }
            // no break here
        }
        case 21056: // Mark of Kazzak
        case 31447: // Mark of Kaz'rogal
        {
            uint32 triggerSpell = 0;
            switch (GetId())
            {
            case 21056:
                triggerSpell = 21058;
                break;
            case 31447:
                triggerSpell = 31463;
                break;
            case 32960:
                triggerSpell = 32961;
                break;
            }
            if (target->GetTypeId() == TYPEID_PLAYER &&
                target->GetPower(power) == 0)
            {
                target->CastSpell(target, triggerSpell, true, nullptr, this);
                target->remove_auras(GetId());
            }
            break;
        }
        }
        break;
    }
    case SPELL_AURA_PERIODIC_ENERGIZE:
    {
        // don't energize target if not alive, possible death persistent effects
        if (!target->isAlive())
            return;

        // ignore non positive values (can be result apply spellmods to aura
        // damage
        uint32 pdamage = m_modifier.m_amount > 0 ? m_modifier.m_amount : 0;

        LOG_DEBUG(logging,
            "PeriodicTick: %s energize %s for %u dmg inflicted by %u",
            GetCasterGuid().GetString().c_str(), target->GetGuidStr().c_str(),
            pdamage, GetId());

        if (m_modifier.m_miscvalue < 0 || m_modifier.m_miscvalue >= MAX_POWERS)
            break;

        Powers power = Powers(m_modifier.m_miscvalue);

        if (target->GetMaxPower(power) == 0)
            break;

        SpellPeriodicAuraLogInfo pInfo(this, pdamage, 0, 0, 0.0f);
        target->SendPeriodicAuraLog(&pInfo);

        target->ModifyPower(power, pdamage);
        break;
    }
    case SPELL_AURA_OBS_MOD_MANA:
    {
        // don't energize target if not alive, possible death persistent effects
        if (!target->isAlive())
            return;

        // ignore non positive values (can be result apply spellmods to aura
        // damage
        uint32 amount = m_modifier.m_amount > 0 ? m_modifier.m_amount : 0;

        uint32 pdamage = uint32(target->GetMaxPower(POWER_MANA) * amount / 100);

        LOG_DEBUG(logging,
            "PeriodicTick: %s energize %s for %u mana inflicted by %u",
            GetCasterGuid().GetString().c_str(), target->GetGuidStr().c_str(),
            pdamage, GetId());

        if (target->GetMaxPower(POWER_MANA) == 0)
            break;

        SpellPeriodicAuraLogInfo pInfo(this, pdamage, 0, 0, 0.0f);
        target->SendPeriodicAuraLog(&pInfo);

        int32 gain = target->ModifyPower(POWER_MANA, pdamage);

        if (Unit* pCaster = GetCaster())
            target->getHostileRefManager().threatAssist(pCaster,
                float(gain) * 0.5f *
                    sSpellMgr::Instance()->GetSpellThreatMultiplier(spellProto),
                spellProto);
        break;
    }
    case SPELL_AURA_POWER_BURN_MANA:
    {
        // don't mana burn target if not alive, possible death persistent
        // effects
        if (!target->isAlive())
            return;

        Unit* pCaster = GetCaster();
        if (!pCaster)
            return;

        // Check for immune (not use charges)
        if (target->IsImmunedToDamage(GetSpellSchoolMask(spellProto)))
            return;

        int32 pdamage = m_modifier.m_amount > 0 ? m_modifier.m_amount : 0;

        Powers powerType = Powers(m_modifier.m_miscvalue);

        if (!target->isAlive() || target->getPowerType() != powerType)
            return;

        // resilience reduce mana draining effect at spell crit damage reduction
        // (added in 2.4)
        if (powerType == POWER_MANA && target->GetTypeId() == TYPEID_PLAYER)
            pdamage -= ((Player*)target)->GetSpellCritDamageReduction(pdamage);

        uint32 gain = uint32(-target->ModifyPower(powerType, -pdamage));

        gain = uint32(gain * spellProto->EffectMultipleValue[GetEffIndex()]);

        // maybe has to be sent different to client, but not by
        // SMSG_PERIODICAURALOG
        SpellNonMeleeDamage damageInfo(pCaster, target, spellProto->Id,
            SpellSchoolMask(spellProto->SchoolMask));
        pCaster->CalculateSpellDamage(&damageInfo, gain, spellProto);

        damageInfo.damage = damageInfo.target->do_resist_absorb_helper(pCaster,
            damageInfo.damage, spellProto, true, 0, &damageInfo.absorb,
            &damageInfo.resist);

        pCaster->DealDamageMods(
            damageInfo.target, damageInfo.damage, &damageInfo.absorb);

        pCaster->SendSpellNonMeleeDamageLog(&damageInfo);

        // Set trigger flag
        uint32 procAttacker = PROC_FLAG_ON_DO_PERIODIC; //  |
        //  PROC_FLAG_SUCCESSFUL_HARMFUL_SPELL_HIT;
        uint32 procVictim =
            PROC_FLAG_ON_TAKE_PERIODIC; // | PROC_FLAG_TAKEN_HARMFUL_SPELL_HIT;
        uint32 procEx = createProcExtendMask(&damageInfo, SPELL_MISS_NONE);
        if (damageInfo.damage)
            procVictim |= PROC_FLAG_TAKEN_ANY_DAMAGE;

        proc_amount procamnt(
            true, damageInfo.damage, damageInfo.target, damageInfo.absorb);

        pCaster->DealSpellDamage(&damageInfo, true);

        pCaster->ProcDamageAndSpell(damageInfo.target, procAttacker, procVictim,
            procEx, procamnt, BASE_ATTACK, spellProto);
        break;
    }
    case SPELL_AURA_MOD_REGEN:
    {
        // don't heal target if not alive, possible death persistent effects
        if (!target->isAlive())
            return;

        // Eating animation
        if (spellProto->AuraInterruptFlags & AURA_INTERRUPT_FLAG_NOT_SEATED)
            target->HandleEmoteCommand(EMOTE_ONESHOT_EAT);
        break;
    }
    case SPELL_AURA_MOD_POWER_REGEN:
    {
        // don't energize target if not alive, possible death persistent effects
        if (!target->isAlive())
            return;

        if (spellProto->AuraInterruptFlags & AURA_INTERRUPT_FLAG_NOT_SEATED)
        {
            // eating anim
            target->HandleEmoteCommand(EMOTE_ONESHOT_EAT);
        }
        else if (GetId() == 20577)
        {
            // cannibalize anim
            target->HandleEmoteCommand(EMOTE_STATE_CANNIBALIZE);
        }

        Powers pt = target->getPowerType();
        if (int32(pt) != m_modifier.m_miscvalue)
            return;

        // Anger Management (Only works if rage > 0; the in combat is a lie)
        // amount = 1+ 16 = 17 = 3,4*5 = 10,2*5/3
        // so 17 is rounded amount for 5 sec tick grow ~ 1 range grow in 3 sec
        if (pt == POWER_RAGE && target->GetPower(POWER_RAGE) > 0)
            target->ModifyPower(pt, m_modifier.m_amount * 3 / 5);
        break;
    }
    // Here tick dummy auras
    case SPELL_AURA_DUMMY: // some spells have dummy aura
    case SPELL_AURA_PERIODIC_DUMMY:
    {
        PeriodicDummyTick();
        break;
    }
    case SPELL_AURA_PERIODIC_TRIGGER_SPELL:
    {
        // Skip periodic trigger spell when banished
        // TODO: This rule might be much more generic
        if (target->hasUnitState(UNIT_STAT_ISOLATED) &&
            target->HasAuraWithMechanic(1 << (MECHANIC_BANISH - 1)))
            break;

        TriggerSpell();
        break;
    }
    case SPELL_AURA_PERIODIC_TRIGGER_SPELL_WITH_VALUE:
    {
        TriggerSpellWithValue();
        break;
    }
    default:
        break;
    }
}

void Aura::PeriodicDummyTick()
{
    SpellEntry const* spell = GetSpellProto();
    Unit* target = GetTarget();
    switch (spell->SpellFamilyName)
    {
    case SPELLFAMILY_GENERIC:
    {
        switch (spell->Id)
        {
        // Explosive Sheep
        case 4074:
            if (!target->FindGuardianWithEntry(2675))
                target->remove_auras(4074);
            return;
        // Forsaken Skills
        case 7054:
        {
            // Possibly need cast one of them (but
            // 7038 Forsaken Skill: Swords
            // 7039 Forsaken Skill: Axes
            // 7040 Forsaken Skill: Daggers
            // 7041 Forsaken Skill: Maces
            // 7042 Forsaken Skill: Staves
            // 7043 Forsaken Skill: Bows
            // 7044 Forsaken Skill: Guns
            // 7045 Forsaken Skill: 2H Axes
            // 7046 Forsaken Skill: 2H Maces
            // 7047 Forsaken Skill: 2H Swords
            // 7048 Forsaken Skill: Defense
            // 7049 Forsaken Skill: Fire
            // 7050 Forsaken Skill: Frost
            // 7051 Forsaken Skill: Holy
            // 7053 Forsaken Skill: Shadow
            return;
        }
        case 7057: // Haunting Spirits
            if (roll_chance_i(33))
                target->CastSpell(
                    target, m_modifier.m_amount, true, nullptr, this);
            return;
        //              // Panda
        //              case 19230: break;
        //              // Gossip NPC Periodic - Talk
        case 32441: // Brittle Bones (Karazhan, Skeletal Waiter)
            if (target)
            {
                if (!target->has_aura(32437))
                {
                    // ~5% chance:
                    if (urand(1, 100) <= 5)
                        target->CastSpell(target, 32437, true);
                }
            }
            return;
        case 8892: // Goblin Rocket Boots
        {
            if (target->GetTypeId() != TYPEID_PLAYER)
                return;

            Player* player = static_cast<Player*>(target);

            // Sources:
            // "The boots don't always explode as soon as you activate them,
            // sometimes it's mid-sprint."
            // http://www.wowhead.com/item=7189#comments
            // "The boots do indeed explode, and on average, after only two or
            // three goes." http://www.wowwiki.com/Goblin_Rocket_Boots

            // With a 20 second duration and a tick every second we take a 1/50
            // chance to explode
            if (urand(0, 49) < 1)
            {
                // The Effect of Destroy Rocket Boots (8897):
                if (Item* item =
                        player->GetItemByGuid(GetCastItemGuid(), false))
                {
                    inventory::transaction trans;
                    trans.destroy(item);
                    if (player->storage().finalize(trans))
                    {
                        // Cast Rocket Boots Malfunction, which explodes and
                        // creates a new pair of boots
                        player->CastSpell(player, 8893, true);
                    }
                }
                player->remove_auras(8892);
            }
            return;
        }
        case 13141: // Gnomish Rocket Boots
        {
            if (target->GetTypeId() != TYPEID_PLAYER)
                return;

            Player* player = static_cast<Player*>(target);

            // Sources for how often these malfunction is missing.
            // We'll assume that their most common malfunction is a reduced
            // duration
            // and that it should happen on average about ~10 seconds in (1/10
            // chance to malfunction)

            if (urand(0, 9) < 1)
            {
                uint32 rand = urand(0, 99);
                if (rand < 75) // 75% chance
                {
                    player->remove_auras(13141);
                }
                else // 25% chance
                {
                    // Rocket Boots Malfunction (confuse)
                    player->CastSpell(player, 13158, true);
                }
            }
            return;
        }
        //              case 33208: break;
        //              // Gossip NPC Periodic - Despawn
        //              case 33209: break;
        //              // Steal Weapon
        //              case 36207: break;
        //              // Simon Game START timer, (DND)
        //              case 39993: break;
        //              // Harpooner's Mark
        //              case 40084: break;
        //              // Old Mount Spell
        //              case 40154: break;
        //              // Magnetic Pull
        //              case 40581: break;
        //              // Ethereal Ring: break; The Bolt Burst
        //              case 40801: break;
        //              // Crystal Prison
        //              case 40846: break;
        //              // Copy Weapon
        //              case 41054: break;
        //              // Ethereal Ring Visual, Lightning Aura
        //              case 41477: break;
        //              // Ethereal Ring Visual, Lightning Aura (Fork)
        //              case 41525: break;
        //              // Ethereal Ring Visual, Lightning Jumper Aura
        //              case 41567: break;
        //              // No Man's Land
        //              case 41955: break;
        //              // Headless Horseman - Fire
        //              case 42074: break;
        //              // Headless Horseman - Visual - Large Fire
        //              case 42075: break;
        //              // Headless Horseman - Start Fire, Periodic Aura
        //              case 42140: break;
        //              // Ram Speed Boost
        //              case 42152: break;
        //              // Headless Horseman - Fires Out Victory Aura
        //              case 42235: break;
        //              // Pumpkin Life Cycle
        //              case 42280: break;
        //              // Brewfest Request Chick Chuck Mug Aura
        //              case 42537: break;
        //              // Squashling
        //              case 42596: break;
        //              // Headless Horseman Climax, Head: Periodic
        //              case 42603: break;
        //              // Fire Bomb
        //              case 42621: break;
        //              // Headless Horseman - Conflagrate, Periodic Aura
        //              case 42637: break;
        //              // Headless Horseman - Create Pumpkin Treats Aura
        //              case 42774: break;
        //              // Headless Horseman Climax - Summoning Rhyme Aura
        //              case 42879: break;
        //              // Tricky Treat
        //              case 42919: break;
        //              // Giddyup!
        //              case 42924: break;
        //              // Ram - Trot
        //              case 42992: break;
        //              // Ram - Canter
        //              case 42993: break;
        //              // Ram - Gallop
        //              case 42994: break;
        //              // Ram Level - Neutral
        //              case 43310: break;
        //              // Headless Horseman - Maniacal Laugh, Maniacal, Delayed
        //              17
        //              case 43884: break;
        //              // Headless Horseman - Maniacal Laugh, Maniacal, other,
        //              Delayed 17
        //              case 44000: break;
        //              // Energy Feedback
        //              case 44328: break;
        //              // Romantic Picnic
        //              case 45102: break;
        //              // Romantic Picnic
        //              case 45123: break;
        //              // Looking for Love
        //              case 45124: break;
        //              // Kite - Lightning Strike Kite Aura
        //              case 45197: break;
        //              // Rocket Chicken
        //              case 45202: break;
        //              // Copy Offhand Weapon
        //              case 45205: break;
        //              // Upper Deck - Kite - Lightning Periodic Aura
        //              case 45207: break;
        //              // Kite -Sky  Lightning Strike Kite Aura
        //              case 45251: break;
        //              // Ribbon Pole Dancer Check Aura
        //              case 45390: break;
        //              // Holiday - Midsummer, Ribbon Pole Periodic Visual
        //              case 45406: break;
        //              // Alliance Flag, Extra Damage Debuff
        //              case 45898: break;
        //              // Horde Flag, Extra Damage Debuff
        //              case 45899: break;
        //              // Ahune - Summoning Rhyme Aura
        //              case 45926: break;
        //              // Ahune - Slippery Floor
        //              case 45945: break;
        //              // Ahune's Shield
        //              case 45954: break;
        //              // Nether Vapor Lightning
        //              case 45960: break;
        //              // Darkness
        //              case 45996: break;
        case 46041: // Summon Blood Elves Periodic
            target->CastSpell(target, 46037, true, nullptr, this);
            target->CastSpell(
                target, roll_chance_i(50) ? 46038 : 46039, true, nullptr, this);
            target->CastSpell(target, 46040, true, nullptr, this);
            return;
        //              // Transform Visual Missile Periodic
        //              case 46205: break;
        //              // Find Opening Beam End
        //              case 46333: break;
        //              // Ice Spear Control Aura
        //              case 46371: break;
        //              // Hailstone Chill
        //              case 46458: break;
        //              // Hailstone Chill, Internal
        //              case 46465: break;
        //              // Chill, Internal Shifter
        //              case 46549: break;
        //              // Summon Ice Spear Knockback Delayer
        //              case 46878: break;
        //              // Send Mug Control Aura
        //              case 47369: break;
        //              // Direbrew's Disarm (precast)
        //              case 47407: break;
        //              // Mole Machine Port Schedule
        //              case 47489: break;
        //              // Mole Machine Portal Schedule
        //              case 49466: break;
        //              // Drink Coffee
        //              case 49472: break;
        //              // Listening to Music
        //              case 50493: break;
        //              // Love Rocket Barrage
        //              case 50530: break;
        // Exist more after, need add later
        default:
            break;
        }

        // Drink (item drink spells)
        if (GetEffIndex() > EFFECT_INDEX_0 &&
            spell->EffectApplyAuraName[GetEffIndex() - 1] ==
                SPELL_AURA_MOD_POWER_REGEN)
        {
            if (target->GetTypeId() != TYPEID_PLAYER)
                return;
            // Search SPELL_AURA_MOD_POWER_REGEN aura for this spell and add
            // bonus
            if (Aura* aura =
                    GetHolder()->GetAura(SpellEffectIndex(GetEffIndex() - 1)))
            {
                // Patch 2.4.0: In arena the first three ticks are modified to *
                // 0, * 1.67 and * 1.33
                if (target->GetMap()->IsBattleArena())
                {
                    // Note: The * 0 tick has already happened at this point

                    int32 mod;
                    if (GetAuraTicks() == 1)
                        mod = m_modifier.m_amount * 1.67;
                    else if (GetAuraTicks() == 2)
                        mod = m_modifier.m_amount * 1.33;
                    else
                    {
                        mod = m_modifier.m_amount;
                        // Disable continue
                        m_isPeriodic = false;
                    }

                    aura->GetModifier()->m_amount = mod;
                    static_cast<Player*>(target)->UpdateManaRegen();
                }
                return;
            }
            return;
        }
        break;
    }
    default:
        break;
    }
}

void Aura::HandlePreventFleeing(bool apply, bool Real)
{
    if (!Real)
        return;

    Unit* target = GetTarget();

    // Cancel the running away in fear movement generator
    if (apply)
    {
        if (target->GetTypeId() == TYPEID_UNIT)
            target->movement_gens.remove_all(movement::gen::run_in_fear);
    }

    // Cancel or restart fear movement generators
    if (apply)
    {
        target->movement_gens.remove_all(movement::gen::fleeing);
    }
    else
    {
        const auto& al = target->GetAurasByType(SPELL_AURA_MOD_FEAR);
        for (auto& aura : al)
        {
            target->movement_gens.push(new movement::FleeingMovementGenerator(
                GetSpellProto()->AttributesCustom &
                        SPELL_ATTR_CUSTOM_PANIC_FEAR ?
                    ObjectGuid() :
                    GetCasterGuid(),
                aura->GetHolder()));
        }
    }
}

void Aura::HandleManaShield(bool apply, bool Real)
{
    if (!Real)
        return;

    // prevent double apply bonuses
    if (apply && (GetTarget()->GetTypeId() != TYPEID_PLAYER ||
                     !((Player*)GetTarget())->GetSession()->PlayerLoading()))
    {
        if (Unit* caster = GetCaster())
        {
            float DoneActualBenefit = 0.0f;
            switch (GetSpellProto()->SpellFamilyName)
            {
            case SPELLFAMILY_MAGE:
                if (GetSpellProto()->SpellFamilyFlags &
                    UI64LIT(0x0000000000008000))
                {
                    // Mana Shield
                    // +50% from +spd bonus
                    DoneActualBenefit = caster->SpellBaseDamageBonusDone(
                                            GetSpellSchoolMask(GetSpellProto()),
                                            GetSpellProto()->Id) *
                                        0.5f;
                    break;
                }
                break;
            default:
                break;
            }

            DoneActualBenefit *=
                caster->calculate_spell_downrank_factor(GetSpellProto());

            m_modifier.m_amount += (int32)DoneActualBenefit;
        }
    }
}

void Aura::HandleArenaPreparation(bool apply, bool Real)
{
    if (!Real)
        return;

    Unit* target = GetTarget();

    target->ApplyModFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PREPARATION, apply);

    if (apply)
    {
        // max regen powers at start preparation
        target->SetHealth(target->GetMaxHealth());
        target->SetPower(POWER_MANA, target->GetMaxPower(POWER_MANA));
        target->SetPower(POWER_ENERGY, target->GetMaxPower(POWER_ENERGY));
    }
    else
    {
        // reset originally 0 powers at start/leave
        target->SetPower(POWER_RAGE, 0);
    }
}

void Aura::HandleAuraMirrorImage(bool /*apply*/, bool Real)
{
    if (!Real)
        return;

    /* FIXME: Not used by any legit spells in 2.4.3 */
}

void Aura::HandleAuraSafeFall(bool Apply, bool Real)
{
    // implemented in WorldSession::HandleMovementOpcodes

    // only special case
    if (Apply && Real && GetId() == 32474 &&
        GetTarget()->GetTypeId() == TYPEID_PLAYER)
        ((Player*)GetTarget())->ActivateTaxiPathTo(506, GetId());
}

bool Aura::HasMechanic(uint32 mechanic) const
{
    return GetSpellProto()->Mechanic == mechanic ||
           GetSpellProto()->EffectMechanic[m_effIndex] == mechanic;
}

AuraHolder::AuraHolder(const SpellEntry* spellproto, Unit* target,
    WorldObject* caster, Item* castItem, const SpellEntry* triggeredByProto)
  : m_enabled_auras{nullptr}, m_target(target),
    m_castItemGuid(castItem ? castItem->GetObjectGuid() : ObjectGuid()),
    m_spellProto(spellproto), m_triggeredByProto(triggeredByProto),
    m_auraSlot(MAX_AURAS), m_prefAuraSlot(0xFF), m_auraLevel(1),
    m_procCharges(0), m_stackAmount(1), m_timeCla(1000),
    m_removeMode(AURA_REMOVE_BY_DEFAULT), m_permanent(false),
    m_isRemovedOnShapeLost(true), m_disabled(false), saved_hp_(0), saved_mp_(0),
    overwritten_by_(nullptr), m_stunFace(true),
    m_diminishingGroup(DIMINISHING_NONE)
{
    assert(target);
    assert(spellproto &&
           spellproto == sSpellStore.LookupEntry(spellproto->Id) &&
           "`info` must be pointer to sSpellStore element");

    if (!caster)
        m_casterGuid = target->GetObjectGuid();
    else
    {
        // remove this assert when not unit casters will be supported
        assert(caster->isType(TYPEMASK_UNIT));
        m_casterGuid = caster->GetObjectGuid();
    }

    m_applyTime = WorldTimer::time_no_syscall();
    m_isPassive = IsPassiveSpell(spellproto);
    m_isDeathPersist = IsDeathPersistentSpell(spellproto);
    m_procCharges = spellproto->procCharges;

    m_isRemovedOnShapeLost =
        (GetCasterGuid() == m_target->GetObjectGuid() &&
            m_spellProto->Stances &&
            !m_spellProto->HasAttribute(SPELL_ATTR_EX2_NOT_NEED_SHAPESHIFT) &&
            !m_spellProto->HasAttribute(SPELL_ATTR_NOT_SHAPESHIFT));

    Unit* unitCaster =
        caster && caster->isType(TYPEMASK_UNIT) ? (Unit*)caster : nullptr;

    m_duration = CalculateSpellDuration(spellproto, unitCaster);

    // Curse of Shadow/the Elements has reduced duration against PvP targets
    // since 2.1.0
    if (spellproto->SpellFamilyName == SPELLFAMILY_WARLOCK &&
        spellproto->SpellFamilyFlags & 0x20000000000 &&
        (target->IsPvP() || target->GetCharmerOrOwnerPlayerOrPlayerItself()))
        m_duration = (2 * MINUTE * IN_MILLISECONDS) < m_duration ?
                         (2 * MINUTE * IN_MILLISECONDS) :
                         m_duration;

    m_maxDuration = m_duration;

    if (HasMechanic(MECHANIC_SILENCE))
    {
        m_duration = m_maxDuration =
            m_duration *
            target->GetInterruptAndSilenceModifier(spellproto, 0x7);
    }

    if (m_maxDuration == -1 || (m_isPassive && spellproto->DurationIndex == 0))
        m_permanent = true;

    if (unitCaster)
    {
        if (Player* modOwner = unitCaster->GetSpellModOwner())
            modOwner->ApplySpellMod(GetId(), SPELLMOD_CHARGES, m_procCharges);
    }

    // some custom stack values at aura holder create
    switch (m_spellProto->Id)
    {
    // some auras applied with max stack
    case 24575: // Brittle Armor
    case 24659: // Unstable Power
    case 24662: // Restless Strength
    case 26464: // Mercurial Shield
    case 30554: // Wrath of the Titans
    case 32065: // Fungal Decay
        m_stackAmount = m_spellProto->StackAmount;
        break;
    // Custom stack amount
    case 23155:
        m_stackAmount = 15;
        break;
    }

    for (auto& elem : m_enabled_auras)
        elem = nullptr;

    m_heartbeatTimer = 0;
    m_breakChance = 0;

    // Heartbeat Resist Check. (If m_breakChance > 0 the heartbeat resist check
    // will be executed once per second)
    Unit* owner = target->GetCharmerOrOwner();
    if (owner)
    {
        if (Unit* iowner = owner->GetCharmerOrOwner())
            owner = iowner;
    }
    else
        owner = target;
    bool subject_to_player_dr = owner->GetTypeId() == TYPEID_PLAYER;
    if (caster && caster->GetTypeId() == TYPEID_PLAYER &&
        !subject_to_player_dr &&
        spellproto->Attributes & SPELL_ATTR_AFFECTED_BY_HEARTBEAT)
    {
        if (m_maxDuration > 1000)
        {
            m_heartbeatTimer = 1000;

            // Formula is: chance/ticks * 13
            // (FIXME: This formula is just an estimated guess, and not based on
            // actual data)
            float missChance =
                ((Player*)caster)->SpellMissChanceCalc(target, spellproto) /
                100.0f; // Returns 1-100.0f
            uint32 ticks = m_maxDuration / 1000;
            m_breakChance = (missChance / ticks * 13) * 10000;
        }
    }
}

// If false, aura needs to be dropped/cannot be applied to target
bool AuraHolder::valid_in_current_state(Unit* caster)
{
    auto target = GetTarget();
    auto interrupt = GetSpellProto()->AuraInterruptFlags;

    // FIXME: This flag does not actually mean ENTER_COMBAT!
    /*if (interrupt & AURA_INTERRUPT_FLAG_ENTER_COMBAT && target->isInCombat())
        return false;*/

    if (interrupt & AURA_INTERRUPT_FLAG_NOT_MOUNTED && !target->IsMounted())
        return false;

    if (interrupt & AURA_INTERRUPT_FLAG_NOT_ABOVEWATER && target->IsInWater())
        return false;

    if (interrupt & AURA_INTERRUPT_FLAG_NOT_UNDERWATER && !target->IsInWater())
        return false;

    if (interrupt & AURA_INTERRUPT_FLAG_NOT_SHEATHED &&
        target->GetSheath() != SHEATH_STATE_UNARMED)
        return false;

    if (interrupt & AURA_INTERRUPT_FLAG_NOT_SEATED && target->IsStandState())
        return false;

    if (interrupt & AURA_INTERRUPT_FLAG_IMMUNE_OR_LOST_SELECTION)
    {
        // Fail if we have a mechanic immune shield aura on us
        // TODO: Is this correct?
        bool found = false;
        target->loop_auras([&found](auto* holder)
            {
                if (holder->GetSpellProto()->Mechanic == MECHANIC_IMMUNE_SHIELD)
                    found = true;
                return !found; // break when found == true
            });
        if (found)
            return false;
    }

    if (interrupt & AURA_INTERRUPT_FLAG_MOUNTING && target->IsMounted())
        return false;

    // TODO: We can't check AURA_INTERRUPT_FLAG_ENTER_PVP_COMBAT atm

    return true;
}

void AuraHolder::AddAura(Aura* aura, SpellEffectIndex index)
{
    m_enabled_auras[index] = aura;
    m_total_auras.push_back(aura);

    // XXX: If an aura is added after the holder is added to m_target
    // AddAuraToModList is not invoked.
    //      Not sure how I wanna deal with that.
}

void AuraHolder::DisableAura(SpellEffectIndex index)
{
    m_enabled_auras[index] = nullptr;
}

void AuraHolder::ApplyAuraModifiers(bool apply, bool real)
{
    for (int32 i = 0; i < MAX_EFFECT_INDEX && !IsDisabled(); ++i)
        if (Aura* aur = GetAura(SpellEffectIndex(i)))
            aur->ApplyModifier(apply, real);
}

uint8 AuraHolder::first_free_slot() const
{
    assert(m_target);

    // NOTE: Positive range always comes before negative

    uint16 positive_end; // one past-the-end

    // Flags defines which slots are positive and negative
    uint8 flags = m_target->GetByteValue(UNIT_FIELD_BYTES_2, 1);

    if ((flags & UNIT_BYTE2_FLAG_POSITIVE_AURAS) == 0)
    {
        // These retarded rules are described as a comment to
        // UNIT_BYTE2_FLAG_AURASLOT_SETUP_CHNG
        if (m_target->GetTypeId() == TYPEID_PLAYER)
        {
            if ((flags & UNIT_BYTE2_FLAG_AURASLOT_SETUP_CHNG) == 0)
                positive_end = 16;
            else
                positive_end = 40; // This is the correct case for players
        }
        else
            positive_end = 0; // all auras are negative
    }
    else
    {
        // These retarded rules are described as a comment to
        // UNIT_BYTE2_FLAG_AURASLOT_SETUP_CHNG
        if (m_target->GetTypeId() == TYPEID_PLAYER)
        {
            if ((flags & UNIT_BYTE2_FLAG_AURASLOT_SETUP_CHNG) == 0)
                positive_end = 8;
            else
                positive_end = 0; // all auras are positive
        }
        else
        {
            if (static_cast<Creature*>(m_target)->IsPet())
            {
                if ((flags & UNIT_BYTE2_FLAG_AURASLOT_SETUP_CHNG) == 0)
                    positive_end = 24; // This is the correct case for pets
                else
                    positive_end = MAX_AURAS;
            }
            else
            {
                if ((flags & UNIT_BYTE2_FLAG_AURASLOT_SETUP_CHNG) == 0)
                    positive_end = 16; // This is the correct case for creatures
                else
                    positive_end = 40;
            }
        }
    }

    if (IsPositive())
    {
        if (m_prefAuraSlot < positive_end &&
            m_target->GetUInt32Value(UNIT_FIELD_AURA + m_prefAuraSlot) == 0)
            return m_prefAuraSlot;
        for (uint8 i = 0; i < positive_end; ++i)
        {
            if (m_target->GetUInt32Value(UNIT_FIELD_AURA + i) == 0)
                return i;
        }
    }
    else
    {
        if (positive_end <= m_prefAuraSlot && m_prefAuraSlot < MAX_AURAS &&
            m_target->GetUInt32Value(UNIT_FIELD_AURA + m_prefAuraSlot) == 0)
            return m_prefAuraSlot;
        for (uint8 i = positive_end; i < MAX_AURAS; ++i)
        {
            if (m_target->GetUInt32Value(UNIT_FIELD_AURA + i) == 0)
                return i;
        }
    }

    return NULL_AURA_SLOT; // no slot found
}

void AuraHolder::_AddAuraHolder()
{
    if (!GetId())
        return;
    if (!m_target)
        return;

    // Try find slot for aura
    uint8 slot = NULL_AURA_SLOT;
    Unit* caster = GetCaster();

    if (IsNeedVisibleSlot(caster))
        slot = first_free_slot();

    // set infinity cooldown state for spells
    if (caster && caster->GetTypeId() == TYPEID_PLAYER)
    {
        if (m_spellProto->HasAttribute(SPELL_ATTR_DISABLED_WHILE_ACTIVE))
        {
            Item* castItem =
                m_castItemGuid ?
                    ((Player*)caster)->GetItemByGuid(m_castItemGuid) :
                    nullptr;
            ((Player*)caster)
                ->AddSpellAndCategoryCooldowns(m_spellProto,
                    castItem ? castItem->GetEntry() : 0, nullptr, true);
        }
    }

    SetAuraSlot(slot);

    // Not update fields for not first spell's aura, all data already in fields
    if (slot < MAX_AURAS) // slot found
    {
        SetAura(slot, false);
        SetAuraFlag(slot, true);
        SetAuraLevel(slot,
            caster ? caster->getLevel() : sWorld::Instance()->getConfig(
                                              CONFIG_UINT32_MAX_PLAYER_LEVEL));
        UpdateAuraApplication();

        // update for out of range group members
        m_target->UpdateAuraForGroup(slot);

        UpdateAuraDuration();
    }

    //*****************************************************
    // Update target aura state flag (at 1 aura apply)
    // TODO: Make it easer
    //*****************************************************
    // Sitdown on apply aura req seated
    if (m_spellProto->AuraInterruptFlags & AURA_INTERRUPT_FLAG_NOT_SEATED &&
        !m_target->IsSitState())
        m_target->SetStandState(UNIT_STAND_STATE_SIT);

    // Update Seals information
    if (IsSealSpell(GetSpellProto()))
        m_target->ModifyAuraState(AURA_STATE_JUDGEMENT, true);

    // Conflagrate aura state
    if (GetSpellProto()->IsFitToFamily(
            SPELLFAMILY_WARLOCK, UI64LIT(0x0000000000000004)))
        m_target->ModifyAuraState(AURA_STATE_CONFLAGRATE, true);

    // Faerie Fire (druid versions)
    if (m_spellProto->IsFitToFamily(
            SPELLFAMILY_DRUID, UI64LIT(0x0000000000000400)))
        m_target->ModifyAuraState(AURA_STATE_FAERIE_FIRE, true);

    // Swiftmend state on Regrowth & Rejuvenation
    if (m_spellProto->IsFitToFamily(
            SPELLFAMILY_DRUID, UI64LIT(0x0000000000000050)))
        m_target->ModifyAuraState(AURA_STATE_SWIFTMEND, true);

    // Deadly poison aura state
    if (m_spellProto->IsFitToFamily(
            SPELLFAMILY_ROGUE, UI64LIT(0x0000000000010000)))
        m_target->ModifyAuraState(AURA_STATE_DEADLY_POISON, true);
}

void AuraHolder::_RemoveAuraHolder()
{
    // Remove all triggered by aura spells vs unlimited duration
    // except same aura replace case
    if (m_removeMode != AURA_REMOVE_BY_STACK)
        CleanupTriggeredSpells();

    Unit* caster = GetCaster();

    if (caster && IsPersistent())
        if (DynamicObject* dynObj = caster->GetDynObject(GetId()))
            dynObj->RemoveAffected(m_target);

    // remove at-store spell cast items (for all remove modes?)
    if (m_target->GetTypeId() == TYPEID_PLAYER &&
        m_removeMode != AURA_REMOVE_BY_DEFAULT &&
        m_removeMode != AURA_REMOVE_BY_DELETE)
        if (ObjectGuid castItemGuid = GetCastItemGuid())
            if (Item* castItem =
                    ((Player*)m_target)->GetItemByGuid(castItemGuid))
            {
                // XXX
                for (int i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
                {
                    const ItemPrototype* prototype = castItem->GetProto();
                    const _Spell& data = prototype->Spells[i];
                    if (data.SpellId != GetId() ||
                        data.SpellTrigger != ITEM_SPELLTRIGGER_ON_STORE)
                        continue;
                    inventory::transaction trans(false);
                    trans.destroy(castItem);
                    break;
                }
            }

    // reset cooldown state for spells
    if (caster && caster->GetTypeId() == TYPEID_PLAYER &&
        m_spellProto->HasAttribute(SPELL_ATTR_DISABLED_WHILE_ACTIVE))
        static_cast<Player*>(caster)->SendCooldownEvent(GetSpellProto());

    uint8 slot = GetAuraSlot();

    if (slot >= MAX_AURAS) // slot not set
        return;

    if (m_target->GetUInt32Value((uint16)(UNIT_FIELD_AURA + slot)) == 0)
        return;

    SetAura(slot, true);
    SetAuraFlag(slot, false);
    SetAuraLevel(slot,
        caster ? caster->getLevel() :
                 sWorld::Instance()->getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL));

    m_procCharges = 0;
    m_stackAmount = 1;
    UpdateAuraApplication();

    if (m_removeMode != AURA_REMOVE_BY_DELETE)
    {
        // Refresh duration of Diminishing Returns, if this aura belongs to a DR
        // group
        if (m_diminishingGroup != DIMINISHING_NONE)
            m_target->RefreshDiminishing(m_diminishingGroup);

        // update for out of range group members
        m_target->UpdateAuraForGroup(slot);

        //*****************************************************
        // Update target aura state flag (at last aura remove)
        //*****************************************************
        uint32 removeState = 0;
        ClassFamilyMask removeFamilyFlag = m_spellProto->SpellFamilyFlags;
        switch (m_spellProto->SpellFamilyName)
        {
        case SPELLFAMILY_PALADIN:
            if (IsSealSpell(m_spellProto))
                removeState = AURA_STATE_JUDGEMENT; // Update Seals information
            break;
        case SPELLFAMILY_WARLOCK:
            if (m_spellProto->IsFitToFamilyMask(UI64LIT(0x0000000000000004)))
                removeState = AURA_STATE_CONFLAGRATE; // Conflagrate aura state
            break;
        case SPELLFAMILY_DRUID:
            if (m_spellProto->IsFitToFamilyMask(UI64LIT(0x0000000000000400)))
                removeState =
                    AURA_STATE_FAERIE_FIRE; // Faerie Fire (druid versions)
            else if (m_spellProto->IsFitToFamilyMask(
                         UI64LIT(0x0000000000000050)))
            {
                removeFamilyFlag = ClassFamilyMask(UI64LIT(0x00000000000050));
                removeState = AURA_STATE_SWIFTMEND; // Swiftmend aura state
            }
            break;
        case SPELLFAMILY_ROGUE:
            if (m_spellProto->IsFitToFamilyMask(UI64LIT(0x0000000000010000)))
                removeState =
                    AURA_STATE_DEADLY_POISON; // Deadly poison aura state
            break;
        }

        // Remove state (but need check other auras for it)
        if (removeState)
        {
            bool found = false;
            m_target->loop_auras(
                [&found, this, removeFamilyFlag](AuraHolder* holder)
                {
                    if (holder->GetSpellProto()->IsFitToFamily(
                            SpellFamily(m_spellProto->SpellFamilyName),
                            removeFamilyFlag))
                        found = true;
                    return !found; // break when found is true
                });

            // this has been last aura
            if (!found)
                m_target->ModifyAuraState(AuraState(removeState), false);
        }
    }
}

void AuraHolder::CleanupTriggeredSpells()
{
    for (int32 i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        if (!m_spellProto->EffectApplyAuraName[i])
            continue;

        uint32 tSpellId = m_spellProto->EffectTriggerSpell[i];
        if (!tSpellId)
            continue;

        SpellEntry const* tProto = sSpellStore.LookupEntry(tSpellId);
        if (!tProto)
            continue;

        if (GetSpellDuration(tProto) != -1)
            continue;

        // needed for spell 43680, maybe others
        // TODO: is there a spell flag, which can solve this in a more
        // sophisticated way?
        if (m_spellProto->EffectApplyAuraName[i] ==
                SPELL_AURA_PERIODIC_TRIGGER_SPELL &&
            GetSpellDuration(m_spellProto) ==
                int32(m_spellProto->EffectAmplitude[i]))
            continue;

        m_target->remove_auras(tSpellId);
    }
}

bool AuraHolder::ModStackAmount(int32 num, bool transfer)
{
    uint32 protoStackAmount = m_spellProto->StackAmount;

    // Can`t mod
    if (!protoStackAmount)
        return true;

    // Modify stack but limit it
    int32 stackAmount = static_cast<int>(m_stackAmount) + num;
    if (stackAmount > (int32)protoStackAmount)
        stackAmount = protoStackAmount;
    else if (stackAmount <= 0) // Last aura from stack removed
    {
        m_stackAmount = 0;
        return true; // need remove aura
    }

    // Update stack amount
    SetStackAmount(stackAmount, transfer);
    return false;
}

void AuraHolder::SetStackAmount(uint32 stackAmount, bool transfer)
{
    Unit* target = GetTarget();
    Unit* caster = GetCaster();
    if (!target || !caster)
        return;

    bool refresh = stackAmount >= m_stackAmount;
    if (stackAmount != m_stackAmount)
    {
        m_stackAmount = stackAmount;
        UpdateAuraApplication();

        for (int32 i = 0; i < MAX_EFFECT_INDEX; ++i)
        {
            if (Aura* aur = GetAura(SpellEffectIndex(i)))
            {
                int32 bp = aur->GetBasePoints();
                int32 amount = m_stackAmount *
                               caster->CalculateSpellDamage(target,
                                   m_spellProto, SpellEffectIndex(i), &bp);
                // Update amount and reapply modifier
                if (amount != aur->GetModifier()->m_amount)
                {
                    // NOTE: If we're transferring stacks to a new holder, we
                    // delay updating the modifiers (see Unit::AddAuraHolder)
                    if (!transfer)
                        aur->ApplyModifier(false, true);
                    aur->GetModifier()->m_amount = amount;
                    if (!transfer)
                        aur->ApplyModifier(true, true);
                }
            }
        }
    }

    if (refresh)
        // Stack increased refresh duration
        RefreshHolder();
}

Unit* AuraHolder::GetCaster() const
{
    if (GetCasterGuid() == m_target->GetObjectGuid())
        return m_target;

    return ObjectAccessor::GetUnit(
        *m_target, m_casterGuid); // player will search at any maps
}

bool AuraHolder::IsWeaponBuffCoexistableWith(AuraHolder const* ref) const
{
    // only item casted spells
    if (!GetCastItemGuid())
        return false;

    // Exclude Debuffs
    if (!IsPositive())
        return false;

    // Exclude Non-generic Buffs and Executioner-Enchant
    if (GetSpellProto()->SpellFamilyName != SPELLFAMILY_GENERIC ||
        GetId() == 42976)
        return false;

    // Exclude Stackable Buffs [ie: Blood Reserve]
    if (GetSpellProto()->StackAmount)
        return false;

    // only self applied player buffs
    if (m_target->GetTypeId() != TYPEID_PLAYER ||
        m_target->GetObjectGuid() != GetCasterGuid())
        return false;

    Item* castItem = ((Player*)m_target)->GetItemByGuid(GetCastItemGuid());
    if (!castItem)
        return false;

    // Limit to Weapon-Slots
    if (!castItem->slot().main_hand() && !castItem->slot().off_hand()) // XXX
        return false;

    // form different weapons
    return ref->GetCastItemGuid() &&
           ref->GetCastItemGuid() != GetCastItemGuid();
}

bool AuraHolder::IsNeedVisibleSlot(Unit const* caster) const
{
    if (m_spellProto->HasAttribute(
            SPELL_ATTR_CUSTOM_DONT_USE_VISIBLE_AURA_SLOT))
        return false;

    bool totemAura = caster && caster->GetTypeId() == TYPEID_UNIT &&
                     ((Creature*)caster)->IsTotem();

    for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        Aura* aura = GetAura(SpellEffectIndex(i));
        if (!aura)
            continue;

        // special area auras cases
        switch (m_spellProto->Effect[i])
        {
        case SPELL_EFFECT_APPLY_AREA_AURA_ENEMY:
            return m_target != caster;
        case SPELL_EFFECT_APPLY_AREA_AURA_OWNER:
            return true;
        case SPELL_EFFECT_APPLY_AREA_AURA_PET:
        case SPELL_EFFECT_APPLY_AREA_AURA_FRIEND:
        case SPELL_EFFECT_APPLY_AREA_AURA_PARTY:
            // passive auras (except totem auras) do not get placed in caster
            // slot
            return (m_target != caster || totemAura || !m_isPassive) &&
                   aura->GetModifier()->m_auraname != SPELL_AURA_NONE;
        default:
            break;
        }
    }

    // passive auras (except totem auras) do not get placed in the slots
    // also, self buffs for AoE
    return !m_isPassive || totemAura;
}

void AuraHolder::HandleSpellSpecificBoosts(bool apply)
{
    uint32 spellId1 = 0;
    uint32 spellId2 = 0;
    uint32 spellId3 = 0;
    uint32 spellId4 = 0;
    uint32 spellId5 = 0;

    switch (GetSpellProto()->SpellFamilyName)
    {
    case SPELLFAMILY_MAGE:
    {
        switch (GetId())
        {
        case 11189: // Frost Warding
        case 28332:
        {
            if (m_target->GetTypeId() == TYPEID_PLAYER && !apply)
            {
                // reflection chance (effect 1) of Frost Ward, applied in dummy
                // effect
                if (SpellModifier* mod =
                        ((Player*)m_target)
                            ->GetSpellMod(SPELLMOD_EFFECT2, GetId()))
                    ((Player*)m_target)->AddSpellMod(&mod, false);
            }
            return;
        }
        case 11094: // Molten Shields
        case 13043:
        {
            if (m_target->GetTypeId() == TYPEID_PLAYER && !apply)
            {
                // reflection chance (effect 1) of Fire Ward, applied in dummy
                // effect
                if (SpellModifier* mod =
                        ((Player*)m_target)
                            ->GetSpellMod(SPELLMOD_EFFECT2, GetId()))
                    ((Player*)m_target)->AddSpellMod(&mod, false);
            }
            return;
        }
        default:
            return;
        }
        break;
    }
    case SPELLFAMILY_WARRIOR:
    {
        if (!apply)
        {
            // Remove Blood Frenzy only if target no longer has any Deep Wound
            // or Rend (applying is handled by procs)
            if (GetSpellProto()->Mechanic != MECHANIC_BLEED)
                return;

            // If target still has one of Warrior's bleeds, do nothing
            auto& PeriodicDamage =
                m_target->GetAurasByType(SPELL_AURA_PERIODIC_DAMAGE);
            for (const auto& elem : PeriodicDamage)
                if ((elem)->GetCasterGuid() == GetCasterGuid() &&
                    (elem)->GetSpellProto()->SpellFamilyName ==
                        SPELLFAMILY_WARRIOR &&
                    (elem)->GetSpellProto()->Mechanic == MECHANIC_BLEED)
                    return;

            spellId1 = 30069; // Blood Frenzy (Rank 1)
            spellId2 = 30070; // Blood Frenzy (Rank 2)
        }
        else
            return;
        break;
    }
    case SPELLFAMILY_WARLOCK:
    {
        // Shadow Embrace Removal
        if (!apply)
        {
            // Ignore any spell that is not Corruption, Curse of Agnoy, Seed of
            // Corruption, or Siphon Life
            if (GetSpellProto()->SpellIconID != 313 &&
                GetSpellProto()->SpellIconID != 544 &&
                GetSpellProto()->SpellIconID != 1932 &&
                GetSpellProto()->SpellIconID != 152)
                return;

            bool found = false;
            // If target still has one of Corruption, Curse of Agony, or Seed of
            // Corruption do nothing
            auto& PeriodicDamage =
                m_target->GetAurasByType(SPELL_AURA_PERIODIC_DAMAGE);
            for (const auto& elem : PeriodicDamage)
            {
                if ((elem)->GetCasterGuid() == GetCasterGuid() &&
                    (elem)->GetSpellProto()->SpellFamilyName ==
                        SPELLFAMILY_WARLOCK)
                    if ((elem)->GetSpellProto()->SpellIconID == 313 ||
                        (elem)->GetSpellProto()->SpellIconID == 544 ||
                        (elem)->GetSpellProto()->SpellIconID == 1932)
                    {
                        found = true;
                        break;
                    }
            }
            // Now check for siphon life, no need to check if already found
            if (!found)
            {
                auto& PeriodicLeech =
                    m_target->GetAurasByType(SPELL_AURA_PERIODIC_LEECH);
                for (const auto& elem : PeriodicLeech)
                {
                    if ((elem)->GetCasterGuid() == GetCasterGuid() &&
                        (elem)->GetSpellProto()->SpellFamilyName ==
                            SPELLFAMILY_WARLOCK &&
                        (elem)->GetSpellProto()->SpellIconID == 152)
                    {
                        found = true;
                        break;
                    }
                }
            }

            if (found)
                return;

            spellId1 = 32386;
            spellId2 = 32388;
            spellId3 = 32389;
            spellId4 = 32390;
            spellId5 = 32391;
        }
        else
            return;
        break;
    }
    case SPELLFAMILY_HUNTER:
    {
        switch (GetId())
        {
        // Misdirection, main spell
        case 34477:
        {
            if (!apply)
                m_target->getHostileRefManager().ResetThreatRedirection();
            return;
        }
        // Improved Hunter's Mark
        case 19421:
        case 19422:
        case 19423:
        case 19424:
        case 19425:
        {
            Unit* caster = GetCaster();
            if (!caster || caster->GetTypeId() != TYPEID_PLAYER)
                return;

            if (apply)
            {
                // Calculate Basepoints and add Modifier
                int rap = 0;
                if (caster->HasSpell(14325))
                    rap = 110;
                else if (caster->HasSpell(14324))
                    rap = 75;
                else if (caster->HasSpell(14323))
                    rap = 45;
                else if (caster->HasSpell(1130))
                    rap = 20;
                int32 basepoints =
                    float(GetSpellProto()->EffectBasePoints[0] + 1) / 100 * rap;
                auto spellmod = new SpellModifier(SPELLMOD_EFFECT3,
                    SPELLMOD_FLAT, basepoints, GetId(), UI64LIT(0x400));
                caster->GetSpellModOwner()->AddSpellMod(&spellmod, true);
            }
            else
            {
                // Get Modifier and Remove it
                if (SpellModifier* mod =
                        caster->GetSpellModOwner()->GetSpellMod(
                            SPELLMOD_EFFECT3, GetId()))
                    caster->GetSpellModOwner()->AddSpellMod(&mod, false);
            }
            return;
        }
        default:
            return;
        }
        break;
    }
    default:
        return;
    }

    if (apply)
    {
        if (spellId1)
            m_target->CastSpell(
                m_target, spellId1, true, nullptr, nullptr, GetCasterGuid());
        if (spellId2 && !IsDisabled())
            m_target->CastSpell(
                m_target, spellId2, true, nullptr, nullptr, GetCasterGuid());
        if (spellId3 && !IsDisabled())
            m_target->CastSpell(
                m_target, spellId3, true, nullptr, nullptr, GetCasterGuid());
        if (spellId4 && !IsDisabled())
            m_target->CastSpell(
                m_target, spellId4, true, nullptr, nullptr, GetCasterGuid());
        if (spellId5 && !IsDisabled())
            m_target->CastSpell(
                m_target, spellId5, true, nullptr, nullptr, GetCasterGuid());
    }
    else
    {
        if (spellId1)
            m_target->remove_auras(spellId1, [this](AuraHolder* holder)
                {
                    return holder->GetCasterGuid() == GetCasterGuid();
                });
        if (spellId2)
            m_target->remove_auras(spellId2, [this](AuraHolder* holder)
                {
                    return holder->GetCasterGuid() == GetCasterGuid();
                });
        if (spellId3)
            m_target->remove_auras(spellId3, [this](AuraHolder* holder)
                {
                    return holder->GetCasterGuid() == GetCasterGuid();
                });
        if (spellId4)
            m_target->remove_auras(spellId4, [this](AuraHolder* holder)
                {
                    return holder->GetCasterGuid() == GetCasterGuid();
                });
        if (spellId5)
            m_target->remove_auras(spellId5, [this](AuraHolder* holder)
                {
                    return holder->GetCasterGuid() == GetCasterGuid();
                });
    }
}

AuraHolder::~AuraHolder()
{
    for (auto& elem : m_enabled_auras)
        elem = nullptr;
    for (auto aura : m_total_auras)
        delete aura;
    m_total_auras.clear();
}

void AuraHolder::Update(uint32 diff)
{
    assert(!m_disabled && "Tried updating deleted AuraHolder");

    if (m_duration > 0)
    {
        m_duration -= diff;
        if (m_duration < 0)
            m_duration = 0;

        m_timeCla -= diff;

        if (m_timeCla <= 0)
        {
            if (Unit* caster = GetCaster())
            {
                Powers powertype = Powers(GetSpellProto()->powerType);
                int32 manaPerSecond =
                    GetSpellProto()->manaPerSecond +
                    GetSpellProto()->manaPerSecondPerLevel * caster->getLevel();
                m_timeCla = 1 * IN_MILLISECONDS;

                if (manaPerSecond && GetTarget() != GetCaster())
                {
                    if (powertype == POWER_HEALTH)
                    {
                        if (caster->GetHealth() <=
                            static_cast<uint32>(manaPerSecond))
                            caster->Kill(caster, true, GetSpellProto());
                        else
                            caster->ModifyHealth(-manaPerSecond);
                    }
                    else
                        caster->ModifyPower(powertype, -manaPerSecond);
                }
            }
        }
    }

    for (int32 i = 0; i < MAX_EFFECT_INDEX; ++i)
        if (Aura* aura = GetAura(SpellEffectIndex(i)))
            aura->Update(diff);

    if (m_disabled)
        return; // Could be disabled by Aura::Update

    // Some aura types require the caster to be alive
    for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        uint32 a = m_spellProto->EffectApplyAuraName[i];
        if (a != SPELL_AURA_MOD_CHARM && a != SPELL_AURA_MOD_POSSESS &&
            a != SPELL_AURA_MOD_POSSESS_PET && a != SPELL_AURA_AOE_CHARM)
            continue;
        Unit* caster = GetCaster();
        if (!caster || !caster->isAlive())
        {
            m_target->RemoveAuraHolder(this);
            return;
        }
    }

    // FIXME: This is a temporary fix for the running while eating/drinking or
    // auto shooting while eating/drinking bug
    if (GetSpellProto()->AuraInterruptFlags & AURA_INTERRUPT_FLAG_NOT_SEATED)
    {
        Unit* caster = GetCaster();
        if (caster && caster->GetTypeId() == TYPEID_PLAYER &&
            (!caster->IsSitState() ||
                caster->GetCurrentSpell(CURRENT_AUTOREPEAT_SPELL) ||
                static_cast<Player*>(caster)->isMoving()))
        {
            m_target->RemoveAuraHolder(this);
            return;
        }
    }

    // Channeled aura required check distance from caster
    if (IsChanneledSpell(m_spellProto) &&
        GetCasterGuid() != m_target->GetObjectGuid())
    {
        Unit* caster = GetCaster();
        if (!caster)
        {
            m_target->RemoveAuraHolder(this);
            return;
        }

        // need check distance for channeled target only
        if (caster->GetChannelObjectGuid() == m_target->GetObjectGuid())
        {
            // Get spell range
            float max_range = GetSpellMaxRange(
                sSpellRangeStore.LookupEntry(m_spellProto->rangeIndex));

            if (Player* modOwner = caster->GetSpellModOwner())
                modOwner->ApplySpellMod(
                    GetId(), SPELLMOD_RANGE, max_range, nullptr);

            float buffered_range = max_range + 10.0;
            // 10 yard buffer for channeled spells
            if (!caster->IsWithinDistInMap(m_target, buffered_range))
            {
                caster->InterruptSpell(CURRENT_CHANNELED_SPELL);
                return;
            }
        }
    }

    // Heartbeat Resist
    if (m_breakChance > 0)
    {
        m_heartbeatTimer -= diff;

        if (m_heartbeatTimer <= 0)
        {
            m_heartbeatTimer += 1000;

            float chance = m_breakChance;

            // Reduce break chance by 50% if we're at the beginning or the end
            // of the spell
            // (Not exactly how blizzard does it, I'd imagine, but it gets
            // somewhat close)
            float durationRatio = m_duration / m_maxDuration;
            if (durationRatio < 1 / 4 || durationRatio > 3 / 4)
                chance *= 0.5f;

            uint32 rand = urand(0, 9999);
            if (rand < chance)
            {
                m_target->RemoveAuraHolder(this);
                return;
            }
        }
    }
}

void AuraHolder::RefreshHolder()
{
    for (auto aura : m_enabled_auras)
        if (aura)
            aura->ResetAuraTicks();
    SetAuraDuration(GetAuraMaxDuration());
    UpdateAuraDuration();
}

void AuraHolder::SetAuraMaxDuration(int32 duration)
{
    m_maxDuration = duration;
    for (auto aura : m_enabled_auras)
        if (aura)
            aura->ResetAuraTicks();

    // possible overwrite persistent state
    if (duration > 0)
    {
        if (!(IsPassive() && GetSpellProto()->DurationIndex == 0))
            SetPermanent(false);
    }
}

bool AuraHolder::HasMechanic(uint32 mechanic) const
{
    if (mechanic == m_spellProto->Mechanic)
        return true;

    for (int32 i = 0; i < MAX_EFFECT_INDEX; ++i)
        if (GetAura(SpellEffectIndex(i)) &&
            m_spellProto->EffectMechanic[i] == mechanic)
            return true;
    return false;
}

bool AuraHolder::HasMechanicMask(uint32 mechanicMask) const
{
    if (mechanicMask & (1 << (m_spellProto->Mechanic - 1)))
        return true;

    for (int32 i = 0; i < MAX_EFFECT_INDEX; ++i)
        if (GetAura(SpellEffectIndex(i)) && m_spellProto->EffectMechanic[i] &&
            ((1 << (m_spellProto->EffectMechanic[i] - 1)) & mechanicMask))
            return true;
    return false;
}

bool AuraHolder::IsPersistent() const
{
    for (int32 i = 0; i < MAX_EFFECT_INDEX; ++i)
        if (Aura* aur = GetAura(SpellEffectIndex(i)))
            if (aur->IsPersistent())
                return true;
    return false;
}

bool AuraHolder::IsAreaAura() const
{
    for (int32 i = 0; i < MAX_EFFECT_INDEX; ++i)
        if (Aura* aur = GetAura(SpellEffectIndex(i)))
            if (aur->IsAreaAura())
                return true;
    return false;
}

bool AuraHolder::IsPositive() const
{
    for (int32 i = 0; i < MAX_EFFECT_INDEX; ++i)
        if (Aura* aur = GetAura(SpellEffectIndex(i)))
            if (!aur->IsPositive())
                return false;
    return true;
}

bool AuraHolder::IsEmptyHolder() const
{
    for (int32 i = 0; i < MAX_EFFECT_INDEX; ++i)
        if (GetAura(SpellEffectIndex(i)) != nullptr)
            return false;
    return true;
}

void AuraHolder::SetAura(uint32 slot, bool remove)
{
    m_target->SetUInt32Value(UNIT_FIELD_AURA + slot, remove ? 0 : GetId());
}

void AuraHolder::SetAuraFlag(uint32 slot, bool add)
{
    uint32 index = slot / 4;
    uint32 byte = (slot % 4) * 8;
    uint32 val = m_target->GetUInt32Value(UNIT_FIELD_AURAFLAGS + index);
    val &= ~((uint32)AFLAG_MASK << byte);
    if (add)
    {
        // FIXME: The flags are NOT correct at the moment
        uint32 flag = AFLAG_UNK1 | AFLAG_UNK2 | AFLAG_UNK3 | AFLAG_UNK4;

        // The only auras that are not active & removable are area auras or
        // channeled spells
        // casted by someone other than the affected target. Add this flag for
        // every other case.
        if (!((IsAreaAura() || IsChanneledSpell(GetSpellProto())) &&
                GetTarget() != GetCaster()))
            flag |= AFLAG_ACTIVE_AND_REMOVABLE;

        // HACK: Mind Vision is channeled, but still cannot have
        // AFLAG_ACTIVE_AND_REMOVABLE
        //       There's probably some general rule here that I'm missin; adding
        //       this hack for now
        if (m_spellProto->Id == 2096 || m_spellProto->Id == 10909)
            flag &= ~AFLAG_ACTIVE_AND_REMOVABLE;

        val |= flag << byte;
    }
    m_target->SetUInt32Value(UNIT_FIELD_AURAFLAGS + index, val);
}

void AuraHolder::SetAuraLevel(uint32 slot, uint32 level)
{
    uint32 index = slot / 4;
    uint32 byte = (slot % 4) * 8;
    uint32 val = m_target->GetUInt32Value(UNIT_FIELD_AURALEVELS + index);
    val &= ~(0xFF << byte);
    val |= (level << byte);
    m_target->SetUInt32Value(UNIT_FIELD_AURALEVELS + index, val);
}

void AuraHolder::UpdateAuraApplication()
{
    if (m_auraSlot >= MAX_AURAS)
        return;

    uint32 stackCount =
        m_procCharges > 0 ? m_procCharges * m_stackAmount : m_stackAmount;

    uint32 index = m_auraSlot / 4;
    uint32 byte = (m_auraSlot % 4) * 8;
    uint32 val = m_target->GetUInt32Value(UNIT_FIELD_AURAAPPLICATIONS + index);
    val &= ~(0xFF << byte);
    // field expect count-1 for proper amount show, also prevent overflow at
    // client side
    val |= ((uint8(stackCount <= 255 ? stackCount - 1 : 255 - 1)) << byte);
    m_target->SetUInt32Value(UNIT_FIELD_AURAAPPLICATIONS + index, val);
}

void AuraHolder::UpdateAuraDuration()
{
    if (GetAuraSlot() >= MAX_AURAS || m_isPassive)
        return;

    if (m_target->GetTypeId() == TYPEID_PLAYER)
    {
        WorldPacket data(SMSG_UPDATE_AURA_DURATION, 5);
        data << uint8(GetAuraSlot());
        data << uint32(GetAuraDuration());
        static_cast<Player*>(m_target)->SendDirectMessage(std::move(data));

        data.initialize(SMSG_SET_EXTRA_AURA_INFO, (8 + 1 + 4 + 4 + 4));
        data << m_target->GetPackGUID();
        data << uint8(GetAuraSlot());
        data << uint32(GetId());
        data << uint32(GetAuraMaxDuration());
        data << uint32(GetAuraDuration());
        static_cast<Player*>(m_target)->SendDirectMessage(std::move(data));
    }

    // not send in case player loading (will not work anyway until player not
    // added to map), sent in visibility change code
    if (m_target->GetTypeId() == TYPEID_PLAYER &&
        static_cast<Player*>(m_target)->GetSession()->PlayerLoading())
        return;

    Unit* caster = GetCaster();
    Unit* owner;

    if (caster && caster->GetTypeId() == TYPEID_PLAYER && caster != m_target)
        SendAuraDurationForCaster(static_cast<Player*>(caster));
    // Send aura duration to owner/charmer
    if (caster && caster->GetTypeId() == TYPEID_UNIT &&
        (owner = caster->GetCharmerOrOwner()) != nullptr &&
        owner->GetTypeId() == TYPEID_PLAYER)
        SendAuraDurationForCaster(static_cast<Player*>(owner));
}

void AuraHolder::SendAuraDurationForCaster(Player* caster)
{
    WorldPacket data(SMSG_SET_EXTRA_AURA_INFO_NEED_UPDATE, (8 + 1 + 4 + 4 + 4));
    data << m_target->GetPackGUID();
    data << uint8(GetAuraSlot());
    data << uint32(GetId());
    data << uint32(GetAuraMaxDuration()); // full
    data << uint32(GetAuraDuration());    // remain
    caster->GetSession()->send_packet(std::move(data));
}
