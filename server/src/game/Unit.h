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

#ifndef __UNIT_H
#define __UNIT_H

#include "Common.h"
#include "DBCStructure.h"
#include "GridMap.h"
#include "HostileRefManager.h"
#include "movement/generators.h"
#include "Object.h"
#include "Opcodes.h"
#include "Path.h"
#include "SharedDefines.h"
#include "SpellAuraDefines.h"
#include "SpellAuras.h"
#include "ThreatManager.h"
#include "Timer.h"
#include "UpdateFields.h"
#include "WorldPacket.h"
#include "Utilities/EventProcessor.h"
#include "Utilities/ref_counter.h"
#include <list>
#include <unordered_map>

class Aura;
class AuraHolder;
class Creature;
class DynamicObject;
struct FactionTemplateEntry;
class GameObject;
class Item;
struct Modifier;
class Pet;
class PetAura;
class Spell;
struct SpellEntry;
struct SpellEntryExt;
class Totem;
// Note: This is somewhat related to Unit, but it's the closest match imo
// DynamiWaypoint allows for on-the-fly generated waypoint paths through
// MotionMaster's MoveDynamicWaypoint(const std::vector<DynamicWaypoint> &)
struct DynamicWaypoint
{
    DynamicWaypoint(float x, float y, float z, float o = 100, uint32 _delay = 0,
        bool _run = false)
      : X(x), Y(y), Z(z), O(o), delay(_delay), run(_run)
    {
    }
    float X, Y, Z, O;
    uint32 delay;
    bool run;
};

enum SpellInterruptFlags
{
    SPELL_INTERRUPT_FLAG_MOVEMENT = 0x01,
    SPELL_INTERRUPT_FLAG_DAMAGE = 0x02,
    SPELL_INTERRUPT_FLAG_INTERRUPT = 0x04,
    SPELL_INTERRUPT_FLAG_AUTOATTACK = 0x08,
    SPELL_INTERRUPT_FLAG_ABORT_ON_DMG =
        0x10, // _complete_ interrupt on direct damage
    // SPELL_INTERRUPT_UNK             = 0x20                // unk, 564 of 727
    // spells having this spell start with "Glyph"
};

enum SpellChannelInterruptFlags
{
    CHANNEL_FLAG_ENTER_COMBAT = 0x0001, // Not 100% confirmed
    CHANNEL_FLAG_DAMAGE = 0x0002,
    CHANNEL_FLAG_MOVEMENT = 0x0008,
    CHANNEL_FLAG_TURNING = 0x0010,
    CHANNEL_FLAG_DAMAGE2 = 0x0080,
    CHANNEL_FLAG_DELAY = 0x4000
};

enum SpellAuraInterruptFlags
{
    AURA_INTERRUPT_FLAG_HITBYSPELL =
        0x00000001, // 0    removed when getting hit by a negative spell?
    AURA_INTERRUPT_FLAG_DAMAGE = 0x00000002,  // 1 removed by any damage
    AURA_INTERRUPT_FLAG_CAST = 0x00000004,    // 2
    AURA_INTERRUPT_FLAG_MOVE = 0x00000008,    // 3 removed by any movement
    AURA_INTERRUPT_FLAG_TURNING = 0x00000010, // 4 removed by any turning
    AURA_INTERRUPT_FLAG_ENTER_COMBAT =
        0x00000020, // 5    removed by entering combat
    AURA_INTERRUPT_FLAG_NOT_MOUNTED = 0x00000040, // 6 removed by unmounting
    AURA_INTERRUPT_FLAG_NOT_ABOVEWATER =
        0x00000080, // 7    removed by entering water
    AURA_INTERRUPT_FLAG_NOT_UNDERWATER =
        0x00000100, // 8    removed by leaving water
    AURA_INTERRUPT_FLAG_NOT_SHEATHED =
        0x00000200, // 9    removed by unsheathing
    AURA_INTERRUPT_FLAG_INTERACT =
        0x00000400, // 10 interacting with npc (talk/loot)
    AURA_INTERRUPT_FLAG_USE =
        0x00000800, // 11 mine/use/open action on gameobject
    AURA_INTERRUPT_FLAG_MELEE_ATTACK = 0x00001000, // 12 removed by attacking
    AURA_INTERRUPT_FLAG_SPELL_ATTACK = 0x00002000, // 13 what's diff between 2?
    AURA_INTERRUPT_FLAG_UNK14 = 0x00004000,        // 14
    AURA_INTERRUPT_FLAG_TRANSFORM = 0x00008000,    // 15 removed by transform?
    AURA_INTERRUPT_FLAG_CAST_FINISH = 0x00010000,  // 16
    AURA_INTERRUPT_FLAG_MOUNTING = 0x00020000,     // 17   removed by mounting
    AURA_INTERRUPT_FLAG_NOT_SEATED =
        0x00040000, // 18   removed by standing up (used by food and drink
                    // mostly and sleep/Fake Death like)
    AURA_INTERRUPT_FLAG_CHANGE_MAP =
        0x00080000, // 19   leaving map/getting teleported
    AURA_INTERRUPT_FLAG_IMMUNE_OR_LOST_SELECTION =
        0x00100000, // 20   removed by auras that make you invulnerable, or make
                    // other to loose selection on you
    AURA_INTERRUPT_FLAG_UNK21 = 0x00200000,      // 21
    AURA_INTERRUPT_FLAG_TELEPORTED = 0x00400000, // 22
    AURA_INTERRUPT_FLAG_ENTER_PVP_COMBAT =
        0x00800000, // 23   removed by entering pvp combat
    AURA_INTERRUPT_FLAG_DIRECT_DAMAGE =
        0x01000000 // 24   removed by any direct damage
};

enum SpellModOp
{
    SPELLMOD_DAMAGE = 0,
    SPELLMOD_DURATION = 1,
    SPELLMOD_THREAT = 2,
    SPELLMOD_EFFECT1 = 3,
    SPELLMOD_CHARGES = 4,
    SPELLMOD_RANGE = 5,
    SPELLMOD_RADIUS = 6,
    SPELLMOD_CRITICAL_CHANCE = 7,
    SPELLMOD_ALL_EFFECTS = 8,
    SPELLMOD_NOT_LOSE_CASTING_TIME = 9,
    SPELLMOD_CASTING_TIME = 10,
    SPELLMOD_COOLDOWN = 11,
    SPELLMOD_EFFECT2 = 12,
    // spellmod 13 unused
    SPELLMOD_COST = 14,
    SPELLMOD_CRIT_DAMAGE_BONUS = 15,
    SPELLMOD_RESIST_MISS_CHANCE = 16,
    SPELLMOD_JUMP_TARGETS = 17,
    SPELLMOD_CHANCE_OF_SUCCESS = 18, // Only used with
                                     // SPELL_AURA_ADD_FLAT_MODIFIER and affects
                                     // proc spells
    SPELLMOD_ACTIVATION_TIME = 19,
    SPELLMOD_EFFECT_PAST_FIRST = 20,
    SPELLMOD_CASTING_TIME_OLD = 21,
    SPELLMOD_DOT = 22,
    SPELLMOD_EFFECT3 = 23,
    SPELLMOD_SPELL_BONUS_DAMAGE = 24,
    SPELLMOD_SPELLPOWER = 25,
    // SPELLMOD_FREQUENCY_OF_SUCCESS   = 26,                 // not used in
    // 2.4.3
    SPELLMOD_MULTIPLE_VALUE = 27,
    SPELLMOD_RESIST_DISPEL_CHANCE = 28
};

#define MAX_SPELLMOD 32

enum SpellFacingFlags
{
    SPELL_FACING_FLAG_INFRONT = 0x0001
};

#define BASE_MELEERANGE_OFFSET 1.33f
#define BASE_MINDAMAGE 1.0f
#define BASE_MAXDAMAGE 2.0f
#define BASE_ATTACK_TIME 2000
#define STEALTH_UPDATE_TIME 3000

// byte value (UNIT_FIELD_BYTES_1,0)
enum UnitStandStateType
{
    UNIT_STAND_STATE_STAND = 0,
    UNIT_STAND_STATE_SIT = 1,
    UNIT_STAND_STATE_SIT_CHAIR = 2,
    UNIT_STAND_STATE_SLEEP = 3,
    UNIT_STAND_STATE_SIT_LOW_CHAIR = 4,
    UNIT_STAND_STATE_SIT_MEDIUM_CHAIR = 5,
    UNIT_STAND_STATE_SIT_HIGH_CHAIR = 6,
    UNIT_STAND_STATE_DEAD = 7,
    UNIT_STAND_STATE_KNEEL = 8,
    UNIT_STAND_STATE_CUSTOM =
        9 // Depends on model animation. Submerge, freeze, hide, hibernate, rest
};

#define MAX_UNIT_STAND_STATE 10

// byte flags value (UNIT_FIELD_BYTES_1,2)
enum UnitStandFlags
{
    UNIT_STAND_FLAGS_UNK1 = 0x01,
    UNIT_STAND_FLAGS_CREEP = 0x02,
    UNIT_STAND_FLAGS_UNK3 = 0x04,
    UNIT_STAND_FLAGS_UNK4 = 0x08,
    UNIT_STAND_FLAGS_UNK5 = 0x10,
    UNIT_STAND_FLAGS_ALL = 0xFF
};

// byte flags value (UNIT_FIELD_BYTES_1,2)
// This corresponds to free talent points (pet case)

// byte flags value (UNIT_FIELD_BYTES_1,3)
enum UnitBytes1_Flags
{
    UNIT_BYTE1_FLAG_ALWAYS_STAND = 0x01,
    UNIT_BYTE1_FLAG_UNK_2 = 0x02,
    UNIT_BYTE1_FLAG_UNTRACKABLE = 0x04,
    UNIT_BYTE1_FLAG_ALL = 0xFF
};

// byte value (UNIT_FIELD_BYTES_2,0)
enum SheathState
{
    SHEATH_STATE_UNARMED = 0, // non prepared weapon
    SHEATH_STATE_MELEE = 1,   // prepared melee weapon
    SHEATH_STATE_RANGED = 2   // prepared ranged weapon
};

#define MAX_SHEATH_STATE 3

// byte flags value (UNIT_FIELD_BYTES_2,1)
enum UnitBytes2_Flags
{
    UNIT_BYTE2_FLAG_PVP =
        0x01, // TODO: Ported from TC, needs confirmation for 2.4.3
    UNIT_BYTE2_FLAG_UNK1 = 0x02,
    UNIT_BYTE2_FLAG_FFA_PVP =
        0x04, // TODO: Ported from TC, needs confirmation for 2.4.3
    UNIT_BYTE2_FLAG_SANCTUARY =
        0x08, // TODO: Ported from TC, needs confirmation for 2.4.3
              // (UNIT_BYTE2_FLAG_UNK3 previously, changed in code; used for
              // pets it seems, is this correct?)
    UNIT_BYTE2_FLAG_POSITIVE_AURAS =
        0x10, // This unit can have positive auras (if missing all auras are
              // shown as debuffs). For players this is different: if a player
              // has only this then 0-15 is pos. and 16-55 neg. Not having this
              // flag but the following flag yields 0-39 pos., and 44-55 neg.
    UNIT_BYTE2_FLAG_AURASLOT_SETUP_CHNG =
        0x20, // Defines which slots are negative and positive. The rules are
              // (1=on,0=0ff): Player: 1 (0-39 pos., 44-55 neg.) / 0 (0-7 pos.,
              // 8-55 neg.) || Creature: 1 (0-39 pos., 44-55 neg.) / 0 (0-15
              // pos., 16-55 neg.) || Pet: 1 (0-55 pos.) / 0 (0-23 pos., 24-55
              // neg.)
    UNIT_BYTE2_FLAG_UNK6 = 0x40,
    UNIT_BYTE2_FLAG_UNK7 = 0x80
};

// byte flags value (UNIT_FIELD_BYTES_2,2)
enum UnitRename
{
    UNIT_CAN_BE_RENAMED = 0x01,
    UNIT_CAN_BE_ABANDONED = 0x02,
};

#define CREATURE_MAX_SPELLS 4

enum Swing
{
    NOSWING = 0,
    SINGLEHANDEDSWING = 1,
    TWOHANDEDSWING = 2
};

enum VictimState
{
    VICTIMSTATE_UNAFFECTED = 0, // seen in relation with HITINFO_MISS
    VICTIMSTATE_NORMAL = 1,
    VICTIMSTATE_DODGE = 2,
    VICTIMSTATE_PARRY = 3,
    VICTIMSTATE_INTERRUPT = 4,
    VICTIMSTATE_BLOCKS = 5,
    VICTIMSTATE_EVADES = 6,
    VICTIMSTATE_IS_IMMUNE = 7,
    VICTIMSTATE_DEFLECTS = 8
};

enum HitInfo
{
    HITINFO_NORMALSWING = 0x00000000,
    HITINFO_UNK0 = 0x00000001, // req correct packet structure
    HITINFO_NORMALSWING2 = 0x00000002,
    HITINFO_LEFTSWING = 0x00000004,
    HITINFO_UNK3 = 0x00000008,
    HITINFO_MISS = 0x00000010,
    HITINFO_ABSORB = 0x00000020, // plays absorb sound
    HITINFO_RESIST = 0x00000040, // resisted atleast some damage
    HITINFO_CRITICALHIT = 0x00000080,
    HITINFO_UNK8 = 0x00000100, // wotlk?
    HITINFO_UNK9 = 0x00002000, // wotlk?
    HITINFO_GLANCING = 0x00004000,
    HITINFO_CRUSHING = 0x00008000,
    HITINFO_NOACTION = 0x00010000,
    HITINFO_SWINGNOHITSOUND = 0x00080000
};

// i would like to remove this: (it is defined in item.h
enum InventorySlot
{
    NULL_BAG = 0,
    NULL_SLOT = 255
};

struct SpellImmune
{
    uint32 type;
    uint32 spell_id;
    const Aura* owner; // Do not dereference
};

typedef std::list<SpellImmune> SpellImmuneList;

enum UnitModifierType
{
    BASE_VALUE = 0,
    BASE_PCT = 1,
    TOTAL_VALUE = 2,
    TOTAL_PCT = 3,
    MODIFIER_TYPE_END = 4
};

enum WeaponDamageRange
{
    MINDAMAGE,
    MAXDAMAGE
};

enum DamageTypeToSchool
{
    RESISTANCE,
    DAMAGE_DEALT,
    DAMAGE_TAKEN
};

enum UnitMods
{
    UNIT_MOD_STAT_STRENGTH, // UNIT_MOD_STAT_STRENGTH..UNIT_MOD_STAT_SPIRIT must
                            // be in existing order, it's accessed by index
                            // values of Stats enum.
    UNIT_MOD_STAT_AGILITY,
    UNIT_MOD_STAT_STAMINA,
    UNIT_MOD_STAT_INTELLECT,
    UNIT_MOD_STAT_SPIRIT,
    UNIT_MOD_HEALTH,
    UNIT_MOD_MANA, // UNIT_MOD_MANA..UNIT_MOD_HAPPINESS must be in existing
                   // order, it's accessed by index values of Powers enum.
    UNIT_MOD_RAGE,
    UNIT_MOD_FOCUS,
    UNIT_MOD_ENERGY,
    UNIT_MOD_HAPPINESS,
    UNIT_MOD_ARMOR, // UNIT_MOD_ARMOR..UNIT_MOD_RESISTANCE_ARCANE must be in
                    // existing order, it's accessed by index values of
                    // SpellSchools enum.
    UNIT_MOD_RESISTANCE_HOLY,
    UNIT_MOD_RESISTANCE_FIRE,
    UNIT_MOD_RESISTANCE_NATURE,
    UNIT_MOD_RESISTANCE_FROST,
    UNIT_MOD_RESISTANCE_SHADOW,
    UNIT_MOD_RESISTANCE_ARCANE,
    UNIT_MOD_ATTACK_POWER,
    UNIT_MOD_ATTACK_POWER_RANGED,
    UNIT_MOD_DAMAGE_MAINHAND,
    UNIT_MOD_DAMAGE_OFFHAND,
    UNIT_MOD_DAMAGE_RANGED,
    UNIT_MOD_END,
    // synonyms
    UNIT_MOD_STAT_START = UNIT_MOD_STAT_STRENGTH,
    UNIT_MOD_STAT_END = UNIT_MOD_STAT_SPIRIT + 1,
    UNIT_MOD_RESISTANCE_START = UNIT_MOD_ARMOR,
    UNIT_MOD_RESISTANCE_END = UNIT_MOD_RESISTANCE_ARCANE + 1,
    UNIT_MOD_POWER_START = UNIT_MOD_MANA,
    UNIT_MOD_POWER_END = UNIT_MOD_HAPPINESS + 1
};

enum AttackPowerBuffs
{
    // See: http://www.wowwiki.com/API_UnitAttackPower
    //  and http://www.wowwiki.com/API_UnitRangedAttackPower
    UNIT_AP_BUFF_POS,
    UNIT_AP_BUFF_NEG,
    UNIT_RAP_BUFF_POS,
    UNIT_RAP_BUFF_NEG,
    UNIT_AP_BUFF_END
};

enum BaseModGroup
{
    CRIT_PERCENTAGE,
    RANGED_CRIT_PERCENTAGE,
    OFFHAND_CRIT_PERCENTAGE,
    SHIELD_BLOCK_VALUE,
    BASEMOD_END
};

enum BaseModType
{
    FLAT_MOD,
    PCT_MOD
};

#define MOD_END (PCT_MOD + 1)

enum DeathState
{
    ALIVE = 0,     // show as alive
    JUST_DIED = 1, // temporary state at die, for creature auto converted to
                   // CORPSE, for player at next update call
    CORPSE = 2,    // corpse state, for player this also meaning that player not
                   // leave corpse
    DEAD = 3, // for creature despawned state (corpse despawned), for player
              // CORPSE/DEAD not clear way switches (FIXME), and use
              // m_deathtimer > 0 check for real corpse state
    JUST_ALIVED = 4, // temporary state at resurrection, for creature auto
                     // converted to ALIVE, for player at next update call
};

// internal state flags for some auras and movement generators, other.
enum UnitState
{
    // persistent state (applied by aura/etc until expire)
    UNIT_STAT_MELEE_ATTACKING =
        0x00000001, // unit is melee attacking someone Unit::Attack
    UNIT_STAT_ATTACK_PLAYER = 0x00000002, // unit attack player or player's
                                          // controlled unit and have contested
                                          // pvpv timer setup, until timer
                                          // expire, combat end and etc
    UNIT_STAT_DIED = 0x00000004,          // Unit::SetFeignDeath
    UNIT_STAT_STUNNED = 0x00000008,       // Aura::HandleAuraModStun
    UNIT_STAT_ROOT = 0x00000010,          // Aura::HandleAuraModRoot
    UNIT_STAT_ISOLATED = 0x00000020, // area auras do not affect other players,
                                     // Aura::HandleAuraModSchoolImmunity
    UNIT_STAT_CONTROLLED = 0x00000040, // Aura::HandleAuraModPossess

    // persistent movement generator state (all time while movement generator
    // applied to unit (independent from top state of movegen)
    UNIT_STAT_TAXI_FLIGHT = 0x00000080, // player is in flight mode (in fact
                                        // interrupted at far teleport until
                                        // next map telport landing)
    UNIT_STAT_DISTRACTED = 0x00000100,  // DistractedMovementGenerator active

    // persistent movement generator state with non-persistent mirror states for
    // stop support
    // (can be removed temporary by stop command or another movement generator
    // apply)
    // not use _MOVE versions for generic movegen state, it can be removed
    // temporary for unit stop and etc
    UNIT_STAT_CONFUSED = 0x00000200, // ConfusedMovementGenerator active/onstack
    UNIT_STAT_CONFUSED_MOVE = 0x00000400,
    UNIT_STAT_ROAMING =
        0x00000800, // RandomMovementGenerator/PointMovementGenerator/WaypointMovementGenerator
                    // active (now always set)
    UNIT_STAT_ROAMING_MOVE = 0x00001000,
    UNIT_STAT_CHASE = 0x00002000, // ChaseMovementGenerator active
    UNIT_STAT_CHASE_MOVE = 0x00004000,
    UNIT_STAT_FOLLOW = 0x00008000, // FollowMovementGenerator active
    UNIT_STAT_FOLLOW_MOVE = 0x00010000,
    UNIT_STAT_FLEEING =
        0x00020000, // FleeMovementGenerator/TimedFleeingMovementGenerator
                    // active/onstack
    UNIT_STAT_FLEEING_MOVE = 0x00040000,
    UNIT_STAT_IGNORE_PATHFINDING =
        0x00080000, // do not use pathfinding in any MovementGenerator
    UNIT_STAT_CANNOT_ROTATE = 0x00100000,
    UNIT_STAT_SAPPED = 0x00200000,

    // masks (only for check)

    // can't move currently
    UNIT_STAT_CAN_NOT_MOVE =
        UNIT_STAT_ROOT | UNIT_STAT_STUNNED | UNIT_STAT_DIED,

    // stay by different reasons
    UNIT_STAT_NOT_MOVE = UNIT_STAT_ROOT | UNIT_STAT_STUNNED | UNIT_STAT_DIED |
                         UNIT_STAT_DISTRACTED,

    // not react at move in sight or other
    UNIT_STAT_CAN_NOT_REACT = UNIT_STAT_STUNNED | UNIT_STAT_DIED |
                              UNIT_STAT_CONFUSED |
                              UNIT_STAT_FLEEING,

    // AI disabled by some reason
    UNIT_STAT_LOST_CONTROL = UNIT_STAT_FLEEING | UNIT_STAT_CONTROLLED,

    // above 2 state cases
    UNIT_STAT_CAN_NOT_REACT_OR_LOST_CONTROL =
        UNIT_STAT_CAN_NOT_REACT | UNIT_STAT_LOST_CONTROL,

    // masks (for check or reset)

    // for real move using movegen check and stop (except unstoppable flight)
    UNIT_STAT_MOVING = UNIT_STAT_ROAMING_MOVE | UNIT_STAT_CHASE_MOVE |
                       UNIT_STAT_FOLLOW_MOVE |
                       UNIT_STAT_FLEEING_MOVE,

    UNIT_STAT_ALL_STATE = 0xFFFFFFFF
};

enum UnitMoveType
{
    MOVE_WALK = 0,
    MOVE_RUN = 1,
    MOVE_RUN_BACK = 2,
    MOVE_SWIM = 3,
    MOVE_SWIM_BACK = 4,
    MOVE_TURN_RATE = 5,
    MOVE_FLIGHT = 6,
    MOVE_FLIGHT_BACK = 7,
};

#define MAX_MOVE_TYPE 8

extern float baseMoveSpeed[MAX_MOVE_TYPE];

enum CombatRating
{
    CR_WEAPON_SKILL = 0,
    CR_DEFENSE_SKILL = 1,
    CR_DODGE = 2,
    CR_PARRY = 3,
    CR_BLOCK = 4,
    CR_HIT_MELEE = 5,
    CR_HIT_RANGED = 6,
    CR_HIT_SPELL = 7,
    CR_CRIT_MELEE = 8,
    CR_CRIT_RANGED = 9,
    CR_CRIT_SPELL = 10,
    CR_HIT_TAKEN_MELEE = 11,
    CR_HIT_TAKEN_RANGED = 12,
    CR_HIT_TAKEN_SPELL = 13,
    CR_CRIT_TAKEN_MELEE = 14,
    CR_CRIT_TAKEN_RANGED = 15,
    CR_CRIT_TAKEN_SPELL = 16,
    CR_HASTE_MELEE = 17,
    CR_HASTE_RANGED = 18,
    CR_HASTE_SPELL = 19,
    CR_WEAPON_SKILL_MAINHAND = 20,
    CR_WEAPON_SKILL_OFFHAND = 21,
    CR_WEAPON_SKILL_RANGED = 22,
    CR_EXPERTISE = 23
};

#define MAX_COMBAT_RATING 24

/// internal used flags for marking special auras - for example some dummy-auras
enum UnitAuraFlags
{
    UNIT_AURAFLAG_ALIVE_INVISIBLE =
        0x1, // aura which makes unit invisible for alive
};

enum UnitVisibility
{
    VISIBILITY_OFF = 0, // absolute, not detectable, GM-like, can see all other
    VISIBILITY_ON = 1,
    VISIBILITY_GROUP_STEALTH =
        2, // detect chance, seen and can see group members
    VISIBILITY_GROUP_INVISIBILITY =
        3, // invisibility, can see and can be seen only another invisible unit
           // or invisible detection unit, set only if not stealthed, and in
           // checks not used (mask used instead)
    VISIBILITY_GROUP_NO_DETECT = 4, // state just at stealth apply for update
                                    // Grid state. Don't remove, otherwise
                                    // stealth spells will break
    VISIBILITY_REMOVE_CORPSE = 5    // special totally not detectable visibility
                                    // for force delete object while removing a
                                    // corpse
};

// Value masks for UNIT_FIELD_FLAGS
enum UnitFlags
{
    UNIT_FLAG_UNK_0 = 0x00000001,
    UNIT_FLAG_NON_ATTACKABLE = 0x00000002, // not attackable
    UNIT_FLAG_DISABLE_MOVE = 0x00000004,
    UNIT_FLAG_PVP_ATTACKABLE = 0x00000008, // allow apply pvp rules to
                                           // attackable state in addition to
                                           // faction dependent state
    UNIT_FLAG_RENAME = 0x00000010,
    UNIT_FLAG_PREPARATION = 0x00000020, // don't take reagents for spells with
                                        // SPELL_ATTR_EX5_NO_REAGENT_WHILE_PREP
    UNIT_FLAG_UNK_6 = 0x00000040,
    UNIT_FLAG_NOT_ATTACKABLE_1 = 0x00000080, // ?? (UNIT_FLAG_PVP_ATTACKABLE |
                                             // UNIT_FLAG_NOT_ATTACKABLE_1) is
                                             // NON_PVP_ATTACKABLE
    UNIT_FLAG_NOT_PLAYER_ATTACKABLE =
        0x00000100, // Unit does not fight with players; cannot be attacked by
                    // players, does not attack players
    UNIT_FLAG_PASSIVE = 0x00000200, // makes you unable to attack everything.
                                    // Almost identical to our "civilian"-term.
                                    // Will ignore it's surroundings and not
                                    // engage in combat unless "called upon" or
                                    // engaged by another unit.
    UNIT_FLAG_LOOTING = 0x00000400, // loot animation
    UNIT_FLAG_PET_IN_COMBAT = 0x00000800, // in combat?, 2.0.8
    UNIT_FLAG_PVP = 0x00001000,
    UNIT_FLAG_SILENCED = 0x00002000, // silenced, 2.1.1
    UNIT_FLAG_UNK_14 = 0x00004000,   // 2.0.8
    UNIT_FLAG_UNK_15 = 0x00008000,
    UNIT_FLAG_UNK_16 = 0x00010000, // removes attackable icon
    UNIT_FLAG_PACIFIED = 0x00020000,
    UNIT_FLAG_STUNNED = 0x00040000, // stunned, 2.1.1
    UNIT_FLAG_IN_COMBAT = 0x00080000,
    UNIT_FLAG_TAXI_FLIGHT =
        0x00100000, // disable casting at client side spell not allowed by taxi
                    // flight (mounted?), probably used with 0x4 flag
    UNIT_FLAG_DISARMED = 0x00200000, // disable melee spells casting...,
                                     // "Required melee weapon" added to melee
                                     // spells tooltip.
    UNIT_FLAG_CONFUSED = 0x00400000,
    UNIT_FLAG_FLEEING = 0x00800000,
    UNIT_FLAG_PLAYER_CONTROLLED = 0x01000000, // used in spell Eyes of the Beast
                                              // for pet... let attack by
                                              // controlled creature
    UNIT_FLAG_NOT_SELECTABLE = 0x02000000,
    UNIT_FLAG_SKINNABLE = 0x04000000,
    UNIT_FLAG_MOUNT = 0x08000000,
    UNIT_FLAG_UNK_28 = 0x10000000,
    UNIT_FLAG_UNK_29 = 0x20000000, // used in Feing Death spell
    UNIT_FLAG_SHEATHE = 0x40000000
    // UNIT_FLAG_UNK_31              = 0x80000000            // no affect in
    // 2.4.3
};

// Value masks for UNIT_FIELD_FLAGS_2
enum UnitFlags2
{
    UNIT_FLAG2_FEIGN_DEATH = 0x00000001,
    UNIT_FLAG2_UNK1 = 0x00000002, // Hides body and body armor. Weapons and
                                  // shoulder and head armor still visible
    UNIT_FLAG2_IGNORE_REPUTATION =
        0x00000004, // From TC that is 3.3.3, is this flag the same in 2.4.3?
    UNIT_FLAG2_COMPREHEND_LANG = 0x00000008,
    UNIT_FLAG2_CLONED = 0x00000010, // Used in SPELL_AURA_MIRROR_IMAGE
    UNIT_FLAG2_UNK5 = 0x00000020,
    UNIT_FLAG2_FORCE_MOVE = 0x00000040,
    // UNIT_FLAG2_DISARM_OFFHAND       = 0x00000080,         // also shield case
    // - added in 3.x, possible all later not used in pre-3.x
    // UNIT_FLAG2_UNK8                 = 0x00000100,
    // UNIT_FLAG2_UNK9                 = 0x00000200,
    // UNIT_FLAG2_DISARM_RANGED        = 0x00000400,         // added in 3.x
    // UNIT_FLAG2_REGENERATE_POWER     = 0x00000800,         // added in 3.x
};

/// Non Player Character flags
enum NPCFlags
{
    UNIT_NPC_FLAG_NONE = 0x00000000,
    UNIT_NPC_FLAG_GOSSIP = 0x00000001,     // 100%
    UNIT_NPC_FLAG_QUESTGIVER = 0x00000002, // guessed, probably ok
    UNIT_NPC_FLAG_UNK1 = 0x00000004,
    UNIT_NPC_FLAG_UNK2 = 0x00000008,
    UNIT_NPC_FLAG_TRAINER = 0x00000010,            // 100%
    UNIT_NPC_FLAG_TRAINER_CLASS = 0x00000020,      // 100%
    UNIT_NPC_FLAG_TRAINER_PROFESSION = 0x00000040, // 100%
    UNIT_NPC_FLAG_VENDOR = 0x00000080,             // 100%
    UNIT_NPC_FLAG_VENDOR_AMMO = 0x00000100,        // 100%, general goods vendor
    UNIT_NPC_FLAG_VENDOR_FOOD = 0x00000200,        // 100%
    UNIT_NPC_FLAG_VENDOR_POISON = 0x00000400,      // guessed
    UNIT_NPC_FLAG_VENDOR_REAGENT = 0x00000800,     // 100%
    UNIT_NPC_FLAG_REPAIR = 0x00001000,             // 100%
    UNIT_NPC_FLAG_FLIGHTMASTER = 0x00002000,       // 100%
    UNIT_NPC_FLAG_SPIRITHEALER = 0x00004000,       // guessed
    UNIT_NPC_FLAG_SPIRITGUIDE = 0x00008000,        // guessed
    UNIT_NPC_FLAG_INNKEEPER = 0x00010000,          // 100%
    UNIT_NPC_FLAG_BANKER = 0x00020000,             // 100%
    UNIT_NPC_FLAG_PETITIONER = 0x00040000, // 100% 0xC0000 = guild petitions,
                                           // 0x40000 = arena team petitions
    UNIT_NPC_FLAG_TABARDDESIGNER = 0x00080000, // 100%
    UNIT_NPC_FLAG_BATTLEMASTER = 0x00100000,   // 100%
    UNIT_NPC_FLAG_AUCTIONEER = 0x00200000,     // 100%
    UNIT_NPC_FLAG_STABLEMASTER = 0x00400000,   // 100%
    UNIT_NPC_FLAG_GUILD_BANKER = 0x00800000, // cause client to send 997 opcode
    UNIT_NPC_FLAG_SPELLCLICK =
        0x01000000, // cause client to send 1015 opcode (spell click), dynamic,
                    // set at loading and don't must be set in DB
};

// used in most movement packets (send and received)
enum MovementFlags
{
    MOVEFLAG_NONE = 0x00000000,
    MOVEFLAG_FORWARD = 0x00000001,
    MOVEFLAG_BACKWARD = 0x00000002,
    MOVEFLAG_STRAFE_LEFT = 0x00000004,
    MOVEFLAG_STRAFE_RIGHT = 0x00000008,
    MOVEFLAG_TURN_LEFT = 0x00000010,
    MOVEFLAG_TURN_RIGHT = 0x00000020,
    MOVEFLAG_PITCH_UP = 0x00000040,
    MOVEFLAG_PITCH_DOWN = 0x00000080,
    MOVEFLAG_WALK_MODE = 0x00000100,   // Walking
    MOVEFLAG_ONTRANSPORT = 0x00000200, // Used for flying on some creatures
    MOVEFLAG_LEVITATING = 0x00000400,
    MOVEFLAG_ROOT = 0x00000800,
    MOVEFLAG_GRAVITY = 0x00001000, // gravity is currently affecting unit (added
                                   // as soon as you leave ground)
    MOVEFLAG_FALLING_FAR =
        0x00002000, // unit has been falling for more than a second
    MOVEFLAG_FALLING_UNK1 = 0x00004000, // reason unknown (added if you jump
                                        // with forward/backward key pressed and
                                        // then release it)
    MOVEFLAG_FALLING_UNK2 = 0x00008000, // reason unknown (added if you jump
                                        // with strafe_left/strafe_right key
                                        // pressed and then release it)
    MOVEFLAG_SWIMMING = 0x00200000,     // appears with fly flag also
    MOVEFLAG_ASCENDING = 0x00400000,    // swim up also
    MOVEFLAG_CAN_FLY = 0x00800000,
    MOVEFLAG_FLYING = 0x01000000,
    MOVEFLAG_FLYING2 = 0x02000000,          // Actual flying mode
    MOVEFLAG_SPLINE_ELEVATION = 0x04000000, // used for flight paths
    MOVEFLAG_SPLINE_ENABLED = 0x08000000,   // used for flight paths
    MOVEFLAG_WATERWALKING =
        0x10000000,                  // prevent unit from falling through water
    MOVEFLAG_SAFE_FALL = 0x20000000, // active rogue safe fall spell (passive)
    MOVEFLAG_HOVER = 0x40000000,

    MOVEFLAG_MASK_MOVING = // Flags that need to be removed on MOVEFLAG_ROOT
    MOVEFLAG_FORWARD | MOVEFLAG_BACKWARD | MOVEFLAG_STRAFE_LEFT |
    MOVEFLAG_STRAFE_RIGHT |
    MOVEFLAG_PITCH_UP |
    MOVEFLAG_PITCH_DOWN |
    MOVEFLAG_GRAVITY |
    MOVEFLAG_ASCENDING |
    MOVEFLAG_SPLINE_ELEVATION,

    MOVEFLAG_MASK_FLYING =
        MOVEFLAG_FLYING2 | MOVEFLAG_ASCENDING | MOVEFLAG_CAN_FLY,
};

// flags that use in movement check for example at spell casting
MovementFlags const movementFlagsMask = MovementFlags(
    MOVEFLAG_FORWARD | MOVEFLAG_BACKWARD | MOVEFLAG_STRAFE_LEFT |
    MOVEFLAG_STRAFE_RIGHT | MOVEFLAG_PITCH_UP | MOVEFLAG_PITCH_DOWN |
    MOVEFLAG_ROOT | MOVEFLAG_GRAVITY | MOVEFLAG_FALLING_UNK1 |
    MOVEFLAG_ASCENDING | MOVEFLAG_SPLINE_ELEVATION);

MovementFlags const movementOrTurningFlagsMask =
    MovementFlags(movementFlagsMask | MOVEFLAG_TURN_LEFT | MOVEFLAG_TURN_RIGHT);

namespace movement
{
class MoveSpline;
}

enum DiminishingLevels
{
    DIMINISHING_LEVEL_1 = 0,
    DIMINISHING_LEVEL_2 = 1,
    DIMINISHING_LEVEL_3 = 2,
    DIMINISHING_LEVEL_IMMUNE = 3
};

struct DiminishingReturn
{
    DiminishingReturn(DiminishingGroup group, uint32 t, uint32 count)
      : DRGroup(group), hitTime(t), hitCount(count)
    {
    }

    DiminishingGroup DRGroup;
    uint32 hitTime;
    uint32 hitCount;
};

// At least some values expected fixed and used in auras field, other custom
enum MeleeHitOutcome
{
    MELEE_HIT_EVADE = 0,
    MELEE_HIT_MISS = 1,
    MELEE_HIT_DODGE = 2, // used as misc in SPELL_AURA_IGNORE_COMBAT_RESULT
    MELEE_HIT_BLOCK = 3, // used as misc in SPELL_AURA_IGNORE_COMBAT_RESULT
    MELEE_HIT_PARRY = 4, // used as misc in SPELL_AURA_IGNORE_COMBAT_RESULT
    MELEE_HIT_GLANCING = 5,
    MELEE_HIT_CRIT = 6,
    MELEE_HIT_CRUSHING = 7,
    MELEE_HIT_NORMAL = 8,
};

struct CleanDamage
{
    CleanDamage(uint32 _damage, WeaponAttackType _attackType,
        MeleeHitOutcome _hitOutCome)
      : damage(_damage), attackType(_attackType), hitOutCome(_hitOutCome)
    {
    }

    uint32 damage;
    WeaponAttackType attackType;
    MeleeHitOutcome hitOutCome;
};

// Struct for use in Unit::CalculateMeleeDamage
// Need create structure like in SMSG_ATTACKERSTATEUPDATE opcode
struct CalcDamageInfo
{
    Unit* attacker; // Attacker
    Unit* target;   // Target for damage
    SpellSchoolMask damageSchoolMask;
    uint32 damage;
    uint32 absorb;
    uint32 resist;
    uint32 blocked_amount;
    uint32 HitInfo;
    uint32 TargetState;
    // Helper
    WeaponAttackType attackType; //
    uint32 procAttacker;
    uint32 procVictim;
    uint32 procEx;
    uint32 cleanDamage; // Used only for rage calculation
    MeleeHitOutcome
        hitOutCome; // TODO: remove this field (need use TargetState)
};

// Spell damage info structure based on structure sending in
// SMSG_SPELLNONMELEEDAMAGELOG opcode
struct SpellNonMeleeDamage
{
    SpellNonMeleeDamage(Unit* _attacker, Unit* _target, uint32 _SpellID,
        SpellSchoolMask _schoolMask)
      : target(_target), attacker(_attacker), SpellID(_SpellID), damage(0),
        schoolMask(_schoolMask), absorb(0), resist(0), physicalLog(false),
        unused(false), blocked(0), HitInfo(0)
    {
    }

    Unit* target;
    Unit* attacker;
    uint32 SpellID;
    uint32 damage;
    SpellSchoolMask schoolMask;
    uint32 absorb;
    uint32 resist;
    bool physicalLog;
    bool unused;
    uint32 blocked;
    uint32 HitInfo;
};

struct SpellPeriodicAuraLogInfo
{
    SpellPeriodicAuraLogInfo(Aura* _aura, uint32 _damage, uint32 _absorb,
        uint32 _resist, float _multiplier)
      : aura(_aura), damage(_damage), absorb(_absorb), resist(_resist),
        multiplier(_multiplier)
    {
    }

    Aura* aura;
    uint32 damage;
    uint32 absorb;
    uint32 resist;
    float multiplier;
};

uint32 createProcExtendMask(
    SpellNonMeleeDamage* damageInfo, SpellMissInfo missCondition);

enum SpellAuraProcResult
{
    SPELL_AURA_PROC_OK = 0,     // proc was processed, will remove charges
    SPELL_AURA_PROC_FAILED = 1, // proc failed - if at least one aura failed the
                                // proc, charges won't be taken
    SPELL_AURA_PROC_CANT_TRIGGER = 2 // aura can't trigger - skip charges
                                     // taking, move to next aura if exists
};

// Controls what Extra Attack it is, to indicate if more extra attacks can proc
// off it
enum ExtraAttackType
{
    EXTRA_ATTACK_NONE = 0,
    EXTRA_ATTACK_PROC_NONE,
    EXTRA_ATTACK_PROC_OTHERS,
    EXTRA_ATTACK_PROC_ALL
};

// healing/damage amount for procs
class proc_amount
{
public:
    proc_amount(bool forced = false)
      : damage_(true), forced_(forced), total_(0), over_(0), absorb_(0)
    {
    }
    // damage == false => healing
    proc_amount(bool damage, uint32 amount, Unit* target, uint32 absorb = 0);

    bool is_damage() const { return damage_; }
    bool is_healing() const { return !is_damage(); }
    bool empty() const { return total_ == 0; }
    bool forced() const { return forced_; }

    uint32 healing() const { return is_healing() ? total_ : 0; }
    uint32 overhealing() const { return is_healing() ? over_ : 0; }
    uint32 damage() const { return is_damage() ? total_ : 0; }
    uint32 overdamage() const { return is_damage() ? over_ : 0; }
    uint32 absorb() const { return is_damage() ? absorb_ : 0; }
    uint32 total() const { return total_; }

private:
    bool damage_;
    bool forced_; // still procs stuff, but has no amount
    uint32 total_;
    uint32 over_;
    uint32 absorb_;
};

typedef SpellAuraProcResult (Unit::*pAuraProcHandler)(Unit* pVictim,
    proc_amount damage, Aura* triggeredByAura, SpellEntry const* procSpell,
    uint32 procFlag, uint32 procEx, uint32 cooldown,
    ExtraAttackType extraAttackType, uint32 extraAttackId);
extern pAuraProcHandler AuraProcHandler[TOTAL_AURAS];

enum CurrentSpellTypes
{
    CURRENT_MELEE_SPELL = 0,
    CURRENT_GENERIC_SPELL = 1,
    CURRENT_AUTOREPEAT_SPELL = 2,
    CURRENT_CHANNELED_SPELL = 3
};

#define CURRENT_FIRST_NON_MELEE_SPELL 1
#define CURRENT_MAX_SPELL 4

struct GlobalCooldown
{
    explicit GlobalCooldown(uint32 _dur = 0, uint32 _time = 0)
      : duration(_dur), cast_time(_time)
    {
    }

    uint32 duration;
    uint32 cast_time;
};

typedef std::unordered_map<uint32 /*category*/, GlobalCooldown>
    GlobalCooldownList;

class GlobalCooldownMgr // Shared by Player and CharmInfo
{
public:
    GlobalCooldownMgr() {}

public:
    bool HasGlobalCooldown(SpellEntry const* spellInfo) const;
    void AddGlobalCooldown(SpellEntry const* spellInfo, uint32 gcd);
    void CancelGlobalCooldown(SpellEntry const* spellInfo);

private:
    GlobalCooldownList m_GlobalCooldowns;
};

enum ActiveStates
{
    ACT_PASSIVE = 0x01,  // 0x01 - passive
    ACT_DISABLED = 0x81, // 0x80 - castable
    ACT_ENABLED = 0xC1,  // 0x40 | 0x80 - auto cast + castable
    ACT_COMMAND = 0x07,  // 0x01 | 0x02 | 0x04
    ACT_REACTION = 0x06, // 0x02 | 0x04
    ACT_DECIDE = 0x00    // custom
};

enum ReactStates
{
    REACT_PASSIVE = 0,
    REACT_DEFENSIVE = 1,
    REACT_AGGRESSIVE = 2
};

enum CommandStates
{
    COMMAND_STAY = 0,
    COMMAND_FOLLOW = 1,
    COMMAND_ATTACK = 2,
    COMMAND_ABANDON = 3
};

#define UNIT_ACTION_BUTTON_ACTION(X) (uint32(X) & 0x00FFFFFF)
#define UNIT_ACTION_BUTTON_TYPE(X) ((uint32(X) & 0xFF000000) >> 24)
#define MAX_UNIT_ACTION_BUTTON_ACTION_VALUE (0x00FFFFFF + 1)
#define MAKE_UNIT_ACTION_BUTTON(A, T) (uint32(A) | (uint32(T) << 24))

struct UnitActionBarEntry
{
    UnitActionBarEntry() : packedData(uint32(ACT_DISABLED) << 24) {}

    uint32 packedData;

    // helper
    ActiveStates GetType() const
    {
        return ActiveStates(UNIT_ACTION_BUTTON_TYPE(packedData));
    }
    uint32 GetAction() const { return UNIT_ACTION_BUTTON_ACTION(packedData); }
    bool IsActionBarForSpell() const
    {
        ActiveStates Type = GetType();
        return Type == ACT_DISABLED || Type == ACT_ENABLED ||
               Type == ACT_PASSIVE;
    }

    void SetActionAndType(uint32 action, ActiveStates type)
    {
        packedData = MAKE_UNIT_ACTION_BUTTON(action, type);
    }

    void SetType(ActiveStates type)
    {
        packedData = MAKE_UNIT_ACTION_BUTTON(
            UNIT_ACTION_BUTTON_ACTION(packedData), type);
    }

    void SetAction(uint32 action)
    {
        packedData =
            (packedData & 0xFF000000) | UNIT_ACTION_BUTTON_ACTION(action);
    }
};

typedef UnitActionBarEntry CharmSpellEntry;

enum ActionBarIndex
{
    ACTION_BAR_INDEX_START = 0,
    ACTION_BAR_INDEX_PET_SPELL_START = 3,
    ACTION_BAR_INDEX_PET_SPELL_END = 7,
    ACTION_BAR_INDEX_END = 10,
};

#define MAX_UNIT_ACTION_BAR_INDEX \
    (ACTION_BAR_INDEX_END - ACTION_BAR_INDEX_START)

struct CharmInfo
{
public:
    explicit CharmInfo(Unit* unit);
    uint32 GetPetNumber() const { return m_petnumber; }
    void SetPetNumber(uint32 petnumber, bool statwindow);

    void SetCommandState(CommandStates st) { m_CommandState = st; }
    CommandStates GetCommandState() { return m_CommandState; }
    bool HasCommandState(CommandStates state)
    {
        return (m_CommandState == state);
    }
    void SetReactState(ReactStates st) { m_reactState = st; }
    ReactStates GetReactState() { return m_reactState; }
    bool HasReactState(ReactStates state) { return (m_reactState == state); }

    void InitPossessCreateSpells();
    void InitCharmCreateSpells();
    void InitPetActionBar();
    void InitEmptyActionBar();

    // return true if successful
    bool AddSpellToActionBar(
        uint32 spellid, ActiveStates newstate = ACT_DECIDE);
    bool RemoveSpellFromActionBar(uint32 spell_id);
    void LoadPetActionBar(const std::string& data);
    void BuildActionBar(WorldPacket* data);
    void SetSpellAutocast(uint32 spell_id, bool state);
    void SetActionBar(uint8 index, uint32 spellOrAction, ActiveStates type)
    {
        PetActionBar[index].SetActionAndType(spellOrAction, type);
    }
    UnitActionBarEntry const* GetActionBarEntry(uint8 index) const
    {
        return &(PetActionBar[index]);
    }

    void ToggleCreatureAutocast(uint32 spellid, bool apply);

    CharmSpellEntry* GetCharmSpell(uint8 index)
    {
        return &(m_charmspells[index]);
    }

    GlobalCooldownMgr& GetGlobalCooldownMgr() { return m_GlobalCooldownMgr; }

    void SaveStayPosition(float Posx, float Posy, float Posz);
    float GetStayPosX() { return m_stayX; }
    float GetStayPosY() { return m_stayY; }
    float GetStayPosZ() { return m_stayZ; }
    void SetIsReturning(bool val) { m_isReturning = val; }
    bool IsReturning() { return m_isReturning; }

private:
    Unit* m_unit;
    UnitActionBarEntry PetActionBar[MAX_UNIT_ACTION_BAR_INDEX];
    CharmSpellEntry m_charmspells[CREATURE_MAX_SPELLS];
    CommandStates m_CommandState;
    ReactStates m_reactState;
    uint32 m_petnumber;
    GlobalCooldownMgr m_GlobalCooldownMgr;

    bool m_isReturning;
    float m_stayX;
    float m_stayY;
    float m_stayZ;
};

// used in CallForAllControlledUnits/CheckAllControlledUnits
enum ControlledUnitMask
{
    CONTROLLED_PET = 0x01,
    CONTROLLED_MINIPET = 0x02,
    CONTROLLED_GUARDIANS = 0x04, // including PROTECTOR_PET
    CONTROLLED_CHARM = 0x08,
    CONTROLLED_TOTEMS = 0x10,
};

// for clearing special attacks
#define REACTIVE_TIMER_START 4000

enum ReactiveType
{
    REACTIVE_DEFENSE = 1,
    REACTIVE_HUNTER_PARRY = 2,
    REACTIVE_CRIT = 3,
    REACTIVE_HUNTER_CRIT = 4,
    REACTIVE_OVERPOWER = 5
};

#define MAX_REACTIVE 6

typedef std::set<ObjectGuid> GuardianPetList;

// delay time next attack to prevent client attack animation problems
#define ATTACK_DISPLAY_DELAY 200
#define MAX_PLAYER_STEALTH_DETECT_RANGE \
    45.0f // max distance for detection targets by player
#define MAX_CREATURE_ATTACK_RADIUS 45.0f // max distance for creature aggro

// Regeneration defines
#define REGEN_TIME_FULL 2000 // For this time difference is computed regen value
#define REGEN_POLY_TIME \
    1000 // Mage's polymorph ticks every second (handled in Aura::Update)
#define REGEN_POLY_PCT 0.1f // 10% of health is restored each tick

struct SpellProcEventEntry; // used only privately

// Used for queued white attacks
struct WhiteAttack
{
    WhiteAttack() { extraAttackSpellId = 0; }

    ExtraAttackType extraAttackType;
    WeaponAttackType weaponAttackType;
    bool onlyTriggerOnNormalSwing;
    uint32 extraAttackSpellId;
};

// To allow more options for casting triggered spells
// Keeps true/false notation intact, but also allows you to go further
enum TriggerType
{
    TRIGGER_TYPE_TRIGGERED =
        0x01, // Needed unless otherwise mentioned in comment
    TRIGGER_TYPE_SHOW_IN_COMBAT_LOG = 0x02,
    TRIGGER_TYPE_CHECK_STANCES = 0x04, // Spell is triggered, but we still want
                                       // to verify Stance requirements
    TRIGGER_TYPE_IGNORE_GCD =
        0x08, // Not triggered, but ignores & doesn't activate global cooldown
    TRIGGER_TYPE_IGNORE_CD =
        0x10, // Spell is triggered, and ignores cooldown & category cooldown
    TRIGGER_TYPE_NO_CASTING_TIME =
        0x20, // Spell is triggered, and ignores its casting time
    TRIGGER_TYPE_TRIGGER_PROCS =
        0x40, // Spell is triggered, but can trigger procs
    TRIGGER_TYPE_BYPASS_SPELL_QUEUE =
        0x80, // Bypasses spell queue, gets executed instantly
};
class spell_trigger_type
{
public:
    spell_trigger_type() : type_(0) {}
    explicit spell_trigger_type(bool b)
    {
        type_ = b ? TRIGGER_TYPE_TRIGGERED : 0;
    }
    spell_trigger_type(uint32 flags) { type_ = flags; }

    bool triggered() const { return type_ & TRIGGER_TYPE_TRIGGERED; }
    bool combat_log() const
    {
        return triggered() && type_ & TRIGGER_TYPE_SHOW_IN_COMBAT_LOG;
    }
    bool check_stances() const
    {
        return !triggered() || type_ & TRIGGER_TYPE_CHECK_STANCES;
    }
    bool ignore_gcd() const { return type_ & TRIGGER_TYPE_IGNORE_GCD; }
    bool ignore_cd() const
    {
        return triggered() && type_ & TRIGGER_TYPE_IGNORE_CD;
    }
    bool ignore_cast_time() const
    {
        return triggered() && type_ & TRIGGER_TYPE_NO_CASTING_TIME;
    }
    bool can_trigger_procs() const
    {
        return !triggered() || type_ & TRIGGER_TYPE_TRIGGER_PROCS;
    }
    bool bypasses_spell_queue() const
    {
        return triggered() && type_ & TRIGGER_TYPE_BYPASS_SPELL_QUEUE;
    }

private:
    uint32 type_;
};

class MANGOS_DLL_SPEC Unit : public WorldObject
{
public:
    typedef std::set<Unit*> AttackerSet;
    typedef std::multimap<uint32, AuraHolder*> AuraHolderMap;
    typedef std::vector<Aura*> Auras;
    typedef std::vector<DiminishingReturn> Diminishing;
    typedef std::set<uint32> ComboPointHolderSet;

    struct spell_ref
    {
        spell_ref(MaNGOS::ref_counter c, Spell* s)
          : counter(std::move(c)), spell(s)
        {
        }

        // the counter exists solely to make the spell know it's still
        // referenced,
        // so that it won't be deleted
        MaNGOS::ref_counter counter;
        Spell* spell;
    };

    virtual ~Unit();

    void AddToWorld() override;
    void RemoveFromWorld() override;

    void CleanupsBeforeDelete()
        override; // used in ~Creature/~Player (or before mass
                  // creature delete to remove cross-references
                  // to already deleted units)

    float
    GetObjectBoundingRadius() const override // overwrite WorldObject version
    {
        return m_floatValues[UNIT_FIELD_BOUNDINGRADIUS];
    }

    DiminishingLevels GetDiminishing(DiminishingGroup group);
    void IncrDiminishing(DiminishingGroup group);
    void RefreshDiminishing(DiminishingGroup group);
    void UpdateDiminishing();
    void ApplyDiminishingToDuration(DiminishingGroup group, int32& duration,
        Unit* caster, DiminishingLevels Level, bool isReflected);
    void ClearDiminishings() { m_Diminishing.clear(); }

    void Update(uint32 update_diff, uint32 time);

    void setAttackTimer(WeaponAttackType type, uint32 time)
    {
        m_attackTimer[type] = time;
    }
    void resetAttackTimer(WeaponAttackType type = BASE_ATTACK);
    uint32 getAttackTimer(WeaponAttackType type) const
    {
        return m_attackTimer[type];
    }
    bool isAttackReady(WeaponAttackType type = BASE_ATTACK) const
    {
        return m_attackTimer[type] == 0;
    }
    bool haveOffhandWeapon() const;
    bool UpdateMeleeAttackingState();
    bool CanUseEquippedWeapon(WeaponAttackType attackType) const;
    float GetMeleeReach(const Unit* victim, float flat_mod = 0.0f) const;
    bool CanReachWithMeleeAttack(
        const Unit* pVictim, float flat_mod = 0.0f) const;

    void _addAttacker(
        Unit* pAttacker) // must be called only from Unit::Attack(Unit*)
    {
        auto itr = m_attackers.find(pAttacker);
        if (itr == m_attackers.end())
            m_attackers.insert(pAttacker);
    }
    void _removeAttacker(
        Unit* pAttacker) // must be called only from Unit::AttackStop()
    {
        m_attackers.erase(pAttacker);
    }
    Unit* getAttackerForHelper() // If someone wants to help, who to give them
    {
        if (getVictim() != nullptr)
            return getVictim();

        if (!m_attackers.empty())
            return *(m_attackers.begin());

        return nullptr;
    }
    bool Attack(Unit* victim, bool meleeAttack);
    void AttackedBy(Unit* attacker);
    void CastStop(uint32 except_spellid = 0);
    bool AttackStop(bool targetSwitch = false, bool client_initiated = false,
        bool pet_evade = false, bool on_death = false);
    void RemoveAllAttackers(bool on_death = false);
    AttackerSet const& getAttackers() const { return m_attackers; }
    bool isAttackingPlayer() const;
    Unit* getVictim() const { return m_attacking; }
    ObjectGuid GetVictimGuid() const { return m_attackingGuid; }
    void CombatStop(bool includingCast = false, bool keep_combat = false,
        bool on_death = false);
    void CombatStopWithPets(bool includingCast = false);
    void StopAttackFaction(uint32 faction_id);
    void clear_attacking()
    {
        m_attacking = nullptr;
        m_attackingGuid.Clear();
    } // pet_behavior needs this, as it does not use SelectHostileTarget(),
      // therefore m_attacking could become invalid for it

    Unit* SelectRandomUnfriendlyTarget(
        Unit* except = nullptr, float radius = ATTACK_DISTANCE) const;
    Unit* SelectRandomFriendlyTarget(
        Unit* except = nullptr, float radius = ATTACK_DISTANCE) const;
    Unit* SelectNearestPetTarget(
        Unit* except = nullptr, float radius = ATTACK_DISTANCE) const;
    void SelectUnfriendlyInRange(std::vector<Unit*>& targets,
        float radius = ATTACK_DISTANCE, bool ignoreLos = false) const;
    uint32 NearbyEnemyCount(float radius, bool ignoreLos = false) const
    {
        std::vector<Unit*> temp;
        SelectUnfriendlyInRange(temp, radius, ignoreLos);
        return temp.size();
    }

    void SendMeleeAttackStop(Unit* victim);
    void SendMeleeAttackStart(Unit* pVictim);

    void addUnitState(uint32 f) { m_state |= f; }
    bool hasUnitState(uint32 f) const { return (m_state & f); }
    void clearUnitState(uint32 f) { m_state &= ~f; }
    bool CanFreeMove() const;

    float GetAverageLevelHp(uint32 pLevel);
    uint32 getLevel() const { return GetUInt32Value(UNIT_FIELD_LEVEL); }
    virtual uint32 GetLevelForTarget(Unit const* /*target*/) const
    {
        return getLevel();
    }
    void SetLevel(uint32 lvl);
    uint8 getRace() const { return GetByteValue(UNIT_FIELD_BYTES_0, 0); }
    uint32 getRaceMask() const { return 1 << (getRace() - 1); }
    uint8 getClass() const { return GetByteValue(UNIT_FIELD_BYTES_0, 1); }
    uint32 getClassMask() const { return 1 << (getClass() - 1); }
    uint8 getGender() const { return GetByteValue(UNIT_FIELD_BYTES_0, 2); }

    float GetStat(Stats stat) const
    {
        return float(GetUInt32Value(UNIT_FIELD_STAT0 + stat));
    }
    void SetStat(Stats stat, int32 val)
    {
        SetStatInt32Value(UNIT_FIELD_STAT0 + stat, val);
    }
    uint32 GetArmor() const { return GetResistance(SPELL_SCHOOL_NORMAL); }
    void SetArmor(int32 val) { SetResistance(SPELL_SCHOOL_NORMAL, val); }

    uint32 GetResistance(SpellSchools school) const
    {
        return GetUInt32Value(UNIT_FIELD_RESISTANCES + school);
    }
    void SetResistance(SpellSchools school, int32 val)
    {
        SetStatInt32Value(UNIT_FIELD_RESISTANCES + school, val);
    }

    uint32 GetHealth() const { return GetUInt32Value(UNIT_FIELD_HEALTH); }
    uint32 GetMaxHealth() const { return GetUInt32Value(UNIT_FIELD_MAXHEALTH); }
    float GetHealthPercent() const
    {
        return (GetHealth() * 100.0f) / GetMaxHealth();
    }
    void SetHealth(uint32 val);
    void SetMaxHealth(uint32 val);
    void SetHealthPercent(float percent);
    int32 ModifyHealth(int32 val);

    Powers getPowerType() const
    {
        return Powers(GetByteValue(UNIT_FIELD_BYTES_0, 3));
    }
    void setPowerType(Powers power);
    uint32 GetPower(Powers power) const
    {
        return GetUInt32Value(UNIT_FIELD_POWER1 + power);
    }
    uint32 GetMaxPower(Powers power) const
    {
        return GetUInt32Value(UNIT_FIELD_MAXPOWER1 + power);
    }
    void SetPower(Powers power, uint32 val);
    void SetMaxPower(Powers power, uint32 val);
    int32 ModifyPower(Powers power, int32 val);
    void ApplyPowerMod(Powers power, uint32 val, bool apply);
    void ApplyMaxPowerMod(Powers power, uint32 val, bool apply);

    uint32 GetAttackTime(WeaponAttackType att) const
    {
        return (uint32)(GetFloatValue(UNIT_FIELD_BASEATTACKTIME + att) /
                        m_modAttackSpeedPct[att]);
    }
    void SetAttackTime(WeaponAttackType att, uint32 val)
    {
        SetFloatValue(
            UNIT_FIELD_BASEATTACKTIME + att, val * m_modAttackSpeedPct[att]);
    }
    void ApplyAttackTimePercentMod(WeaponAttackType att, float val, bool apply);
    void ApplyCastTimePercentMod(float val, bool apply);

    SheathState GetSheath() const
    {
        return SheathState(GetByteValue(UNIT_FIELD_BYTES_2, 0));
    }
    virtual void SetSheath(SheathState sheathed)
    {
        SetByteValue(UNIT_FIELD_BYTES_2, 0, sheathed);
    }

    ReputationRank GetReactionTo(Unit* target);
    static ReputationRank GetFactionReactionTo(
        const FactionTemplateEntry* factionTemplateEntry, Unit* target);

    // faction template id
    uint32 getFaction() const
    {
        return GetUInt32Value(UNIT_FIELD_FACTIONTEMPLATE);
    }
    void setFaction(uint32 faction)
    {
        SetUInt32Value(UNIT_FIELD_FACTIONTEMPLATE, faction);
    }
    FactionTemplateEntry const* getFactionTemplateEntry() const;
    bool IsHostileTo(Unit const* unit) const override;
    bool IsHostileToPlayers() const;
    bool IsFriendlyTo(Unit const* unit) const override;
    bool IsNeutralToAll() const;
    bool IsInPartyWith(Unit* unit);
    bool IsInRaidWith(Unit* unit);
    void GetPartyMembers(std::list<Unit*>& units);
    bool IsContestedGuard() const
    {
        if (FactionTemplateEntry const* entry = getFactionTemplateEntry())
            return entry->IsContestedGuardFaction();

        return false;
    }
    bool IsPvP() const { return HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PVP); }
    void SetPvP(bool state);
    uint32 GetCreatureType() const;
    uint32 GetCreatureTypeMask() const
    {
        uint32 creatureType = GetCreatureType();
        return (creatureType >= 1) ? (1 << (creatureType - 1)) : 0;
    }

    uint8 getStandState() const { return GetByteValue(UNIT_FIELD_BYTES_1, 0); }
    bool IsSitState() const;
    bool IsStandState() const;
    void SetStandState(uint8 state);

    void SetStandFlags(uint8 flags)
    {
        SetByteFlag(UNIT_FIELD_BYTES_1, 2, flags);
    }
    void RemoveStandFlags(uint8 flags)
    {
        RemoveByteFlag(UNIT_FIELD_BYTES_1, 2, flags);
    }

    bool IsMounted() const
    {
        return HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_MOUNT);
    }
    uint32 GetMountID() const
    {
        return GetUInt32Value(UNIT_FIELD_MOUNTDISPLAYID);
    }
    void Mount(uint32 mount, uint32 spellId = 0);
    void Unmount(bool from_aura = false);

    uint16 GetMaxSkillValueForLevel(Unit const* target = nullptr) const
    {
        return (target ? GetLevelForTarget(target) : getLevel()) * 5;
    }
    void DealDamageMods(Unit* pVictim, uint32& damage, uint32* absorb);
    uint32 DealDamage(Unit* pVictim, uint32 damage,
        CleanDamage const* cleanDamage, DamageEffectType damagetype,
        SpellSchoolMask damageSchoolMask, SpellEntry const* spellProto,
        bool durabilityLoss, bool absorb);
    void Kill(Unit* victim, bool durabilityLoss = true,
        const SpellEntry* spellProto = nullptr);
    int32 DealHeal(Unit* pVictim, uint32 addhealth,
        SpellEntry const* spellProto, bool critical = false);

    void PetOwnerKilledUnit(Unit* pVictim);

    void ProcDamageAndSpell(Unit* pVictim, uint32 procAttacker,
        uint32 procVictim, uint32 procEx, proc_amount amount,
        WeaponAttackType attType = BASE_ATTACK,
        SpellEntry const* procSpell = nullptr,
        ExtraAttackType extraAttackType = EXTRA_ATTACK_NONE,
        uint32 extraAttackId = 0);
    void ProcDamageAndSpellFor(bool isVictim, Unit* pTarget, uint32 procFlag,
        uint32 procExtra, WeaponAttackType attType, SpellEntry const* procSpell,
        proc_amount amount, ExtraAttackType extraAttackType = EXTRA_ATTACK_NONE,
        uint32 extraAttackId = 0);
    /* This is called when the spell leaves the caster's hands. */
    void ProcSpellsOnCast(Spell* spell, Unit* pVictim, uint32 procAttacker,
        uint32 procVictim, uint32 procExtra, proc_amount amount,
        WeaponAttackType attType, SpellEntry const* procSpell,
        ExtraAttackType extraAttackType = EXTRA_ATTACK_NONE,
        uint32 extraAttackId = 0, int32 cast_time = 0);

    void HandleEmote(uint32 emote_id); // auto-select command/state
    void HandleEmoteCommand(uint32 emote_id);
    void HandleEmoteState(uint32 emote_id);

    // Set attackerStateLock to true before AttackerStateUpdate call and false
    // after,
    // as well as do not call AttackerStateUpdate when attackerStateLock is
    // true.
    bool attackerStateLock;
    void AttackerStateUpdate(Unit* pVictim, bool isNormalSwing);
    void DoWhiteAttack(Unit* pVictim, const WhiteAttack& whiteAttack);
    void QueueWhiteAttack(WhiteAttack whiteAttack);
    void ClearQueuedWhiteAttacks() { m_whiteAttacksQueue.clear(); }

    float MeleeMissChanceCalc(
        const Unit* pVictim, WeaponAttackType attType) const;
    float SpellMissChanceCalc(
        const Unit* pVictim, SpellEntry const* spell) const;

    void CalculateMeleeDamage(Unit* pVictim, uint32 damage,
        CalcDamageInfo* damageInfo, WeaponAttackType attackType = BASE_ATTACK);
    void DealMeleeDamage(CalcDamageInfo* damageInfo, bool durabilityLoss,
        ExtraAttackType extraAttackType = EXTRA_ATTACK_NONE,
        uint32 extraAttackId = 0);

    bool IsAllowedDamageInArea(Unit* pVictim) const;

    void CalculateSpellDamage(SpellNonMeleeDamage* damageInfo, int32 damage,
        SpellEntry const* spellInfo, WeaponAttackType attackType = BASE_ATTACK,
        uint32 TargetCount = 0, float crit_mod = 0.0f,
        const Spell* spell = nullptr);
    void DealSpellDamage(SpellNonMeleeDamage* damageInfo, bool durabilityLoss);

    float MeleeSpellMissChance(Unit* pVictim, WeaponAttackType attType,
        int32 skillDiff, SpellEntry const* spell);
    SpellMissInfo MeleeSpellHitResult(Unit* pVictim, SpellEntry const* spell);
    SpellMissInfo MagicSpellHitResult(Unit* pVictim, SpellEntry const* spell);
    SpellMissInfo SpellHitResult(Unit* pVictim, SpellEntry const* spell,
        bool canReflect = false, bool was_reflected = false);
    // returns: mask of effects that unit is still not immune to
    uint32 SpellImmunityCheck(const SpellEntry* info, uint32 effectMask);

    float GetUnitDodgeChance() const;
    float GetUnitParryChance() const;
    float GetUnitBlockChance() const;
    float GetUnitCriticalChance(
        WeaponAttackType attackType, const Unit* pVictim) const;

    virtual uint32 GetShieldBlockValue() const = 0;
    uint32 GetUnitMeleeSkill(Unit const* target = nullptr) const
    {
        return (target ? GetLevelForTarget(target) : getLevel()) * 5;
    }
    uint32 GetDefenseSkillValue(Unit const* target = nullptr) const;
    uint32 GetWeaponSkillValue(
        WeaponAttackType attType, Unit const* target = nullptr) const;
    float GetWeaponProcChance() const;
    float GetPPMProcChance(WeaponAttackType type, float PPM) const;

    MeleeHitOutcome RollMeleeOutcomeAgainst(
        const Unit* pVictim, WeaponAttackType attType) const;
    MeleeHitOutcome RollMeleeOutcomeAgainst(const Unit* pVictim,
        WeaponAttackType attType, int32 crit_chance, int32 miss_chance,
        int32 dodge_chance, int32 parry_chance, int32 block_chance) const;

    bool isVendor() const
    {
        return HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_VENDOR);
    }
    bool isTrainer() const
    {
        return HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_TRAINER);
    }
    bool isQuestGiver() const
    {
        return HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_QUESTGIVER);
    }
    bool isGossip() const
    {
        return HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP);
    }
    bool isTaxi() const
    {
        return HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_FLIGHTMASTER);
    }
    bool isGuildMaster() const
    {
        return HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_PETITIONER);
    }
    bool isBattleMaster() const
    {
        return HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_BATTLEMASTER);
    }
    bool isBanker() const
    {
        return HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_BANKER);
    }
    bool isInnkeeper() const
    {
        return HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_INNKEEPER);
    }
    bool isSpiritHealer() const
    {
        return HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_SPIRITHEALER);
    }
    bool isSpiritGuide() const
    {
        return HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_SPIRITGUIDE);
    }
    bool isTabardDesigner() const
    {
        return HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_TABARDDESIGNER);
    }
    bool isAuctioner() const
    {
        return HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_AUCTIONEER);
    }
    bool isArmorer() const
    {
        return HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_REPAIR);
    }
    bool isServiceProvider() const
    {
        return HasFlag(UNIT_NPC_FLAGS,
            UNIT_NPC_FLAG_VENDOR | UNIT_NPC_FLAG_TRAINER |
                UNIT_NPC_FLAG_FLIGHTMASTER | UNIT_NPC_FLAG_PETITIONER |
                UNIT_NPC_FLAG_BATTLEMASTER | UNIT_NPC_FLAG_BANKER |
                UNIT_NPC_FLAG_INNKEEPER | UNIT_NPC_FLAG_SPIRITHEALER |
                UNIT_NPC_FLAG_SPIRITGUIDE | UNIT_NPC_FLAG_TABARDDESIGNER |
                UNIT_NPC_FLAG_AUCTIONEER);
    }
    bool isSpiritService() const
    {
        return HasFlag(UNIT_NPC_FLAGS,
            UNIT_NPC_FLAG_SPIRITHEALER | UNIT_NPC_FLAG_SPIRITGUIDE);
    }

    bool IsTaxiFlying() const { return hasUnitState(UNIT_STAT_TAXI_FLIGHT); }

    bool isInCombat() const
    {
        return HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_IN_COMBAT);
    }
    void SetInCombatState(bool PvP, Unit* enemy = nullptr);
    void SetInCombatWith(Unit* enemy);
    void AdoptUnitCombatState(Unit* target);
    void ClearInCombat();
    ShortIntervalTimer _min_combat_timer;
    uint32 InPvPCombat() const { return _min_combat_timer.GetInterval() != 0; }

    bool has_aura(uint32 spell_id, AuraType type = SPELL_AURA_NONE) const;
    bool HasAuraType(AuraType auraType) const;
    uint32 GetAuraCount(uint32 spellId) const;
    bool HasAuraWithMechanic(uint32 mechanic_mask) const;

    bool virtual HasSpell(uint32 /*spellID*/) const { return false; }

    void InformPetsAboutCC(const SpellEntry* spellInfo = nullptr);
    bool HasBreakableByDamageAuraType(AuraType type) const;
    bool HasBreakOnDamageCCAura() const;
    bool HasStealthAura() const { return HasAuraType(SPELL_AURA_MOD_STEALTH); }
    bool HasInvisibilityAura() const
    {
        return HasAuraType(SPELL_AURA_MOD_INVISIBILITY);
    }
    bool isFeared() const { return HasAuraType(SPELL_AURA_MOD_FEAR); }
    bool isInRoots() const { return HasAuraType(SPELL_AURA_MOD_ROOT); }
    bool IsPolymorphed() const { return m_polyRegenTimer != -1; }
    bool IsAffectedByThreatIgnoringCC() const;

    bool isFrozen() const;

    void RemoveSpellbyDamageTaken(
        AuraType auraType, uint32 damage, bool dot_damage);

    bool isTargetableForAttack(bool inversAlive = false) const;
    bool isPassiveToHostile() const
    {
        return HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PASSIVE);
    }

    // Get liquid status at current position; general case quicker than
    // Terrain's function
    GridMapLiquidStatus GetLiquidStatus(
        uint8 ReqType = 0, GridMapLiquidData* out_data = nullptr) const;
    // NOTE: only use if you're confident Unit is in same area as x,y,z
    GridMapLiquidStatus GetLiquidStatus(float x, float y, float z,
        uint8 ReqType = 0, GridMapLiquidData* out_data = nullptr) const;
    virtual bool IsInWater() const;
    virtual bool IsUnderWater() const;
    bool isInAccessablePlaceFor(Creature const* c) const;

    void SendHealSpellLog(
        Unit* pVictim, uint32 SpellID, uint32 Damage, bool critical = false);
    void SendEnergizeSpellLog(
        Unit* pVictim, uint32 SpellID, uint32 Damage, Powers powertype);
    void EnergizeBySpell(
        Unit* pVictim, uint32 SpellID, uint32 Damage, Powers powertype);
    void CastSpell(Unit* Victim, uint32 spellId, spell_trigger_type triggered,
        Item* castItem = nullptr, Aura* triggeredByAura = nullptr,
        ObjectGuid originalCaster = ObjectGuid(),
        SpellEntry const* triggeredBy = nullptr);
    void CastSpell(Unit* Victim, SpellEntry const* spellInfo,
        spell_trigger_type triggered, Item* castItem = nullptr,
        Aura* triggeredByAura = nullptr,
        ObjectGuid originalCaster = ObjectGuid(),
        SpellEntry const* triggeredBy = nullptr);
    void CastCustomSpell(Unit* Victim, uint32 spellId, int32 const* bp0,
        int32 const* bp1, int32 const* bp2, spell_trigger_type triggered,
        Item* castItem = nullptr, Aura* triggeredByAura = nullptr,
        ObjectGuid originalCaster = ObjectGuid(),
        SpellEntry const* triggeredBy = nullptr);
    void CastCustomSpell(Unit* Victim, SpellEntry const* spellInfo,
        int32 const* bp0, int32 const* bp1, int32 const* bp2,
        spell_trigger_type triggered, Item* castItem = nullptr,
        Aura* triggeredByAura = nullptr,
        ObjectGuid originalCaster = ObjectGuid(),
        SpellEntry const* triggeredBy = nullptr);
    void CastSpell(float x, float y, float z, uint32 spellId,
        spell_trigger_type triggered, Item* castItem = nullptr,
        Aura* triggeredByAura = nullptr,
        ObjectGuid originalCaster = ObjectGuid(),
        SpellEntry const* triggeredBy = nullptr);
    void CastSpell(float x, float y, float z, SpellEntry const* spellInfo,
        spell_trigger_type triggered, Item* castItem = nullptr,
        Aura* triggeredByAura = nullptr,
        ObjectGuid originalCaster = ObjectGuid(),
        SpellEntry const* triggeredBy = nullptr);

    void DeMorph();

    void SendAttackStateUpdate(CalcDamageInfo* damageInfo);
    void SendAttackStateUpdate(uint32 HitInfo, Unit* target, uint8 SwingType,
        SpellSchoolMask damageSchoolMask, uint32 Damage, uint32 AbsorbDamage,
        uint32 Resist, VictimState TargetState, uint32 BlockedAmount);
    void SendSpellNonMeleeDamageLog(SpellNonMeleeDamage* log);
    void SendSpellNonMeleeDamageLog(Unit* target, uint32 SpellID, uint32 Damage,
        SpellSchoolMask damageSchoolMask, uint32 AbsorbedDamage, uint32 Resist,
        bool PhysicalDamage, uint32 Blocked, bool CriticalHit = false);
    void SendPeriodicAuraLog(SpellPeriodicAuraLogInfo* pInfo);
    void SendSpellMiss(Unit* target, uint32 spellID, SpellMissInfo missInfo);

    void NearTeleportTo(
        float x, float y, float z, float orientation, bool casting = false);
    void SendHeartBeat();
    bool IsLevitating() const
    {
        return m_movementInfo.HasMovementFlag(MOVEFLAG_LEVITATING);
    }
    bool IsWalking() const
    {
        return m_movementInfo.HasMovementFlag(MOVEFLAG_WALK_MODE);
    }

    void SetInFront(Unit const* target);
    void SetFacingTo(float ori);
    void SetFacingToObject(WorldObject* pObject);

    bool isAlive() const { return (m_deathState == ALIVE); };
    bool isDead() const
    {
        return (m_deathState == DEAD || m_deathState == CORPSE);
    };
    DeathState getDeathState() const { return m_deathState; };
    virtual void SetDeathState(
        DeathState s); // overwritten in Creature/Player/Pet

    ObjectGuid const& GetOwnerGuid() const
    {
        return GetGuidValue(UNIT_FIELD_SUMMONEDBY);
    }
    void SetOwnerGuid(ObjectGuid owner)
    {
        SetGuidValue(UNIT_FIELD_SUMMONEDBY, owner);
    }
    ObjectGuid const& GetCreatorGuid() const
    {
        return GetGuidValue(UNIT_FIELD_CREATEDBY);
    }
    void SetCreatorGuid(ObjectGuid creator)
    {
        SetGuidValue(UNIT_FIELD_CREATEDBY, creator);
    }
    ObjectGuid const& GetPetGuid() const
    {
        return GetGuidValue(UNIT_FIELD_SUMMON);
    }
    void SetPetGuid(ObjectGuid pet) { SetGuidValue(UNIT_FIELD_SUMMON, pet); }
    ObjectGuid const& GetCharmerGuid() const
    {
        return GetGuidValue(UNIT_FIELD_CHARMEDBY);
    }
    void SetCharmerGuid(ObjectGuid owner)
    {
        SetGuidValue(UNIT_FIELD_CHARMEDBY, owner);
    }
    ObjectGuid const& GetCharmGuid() const
    {
        return GetGuidValue(UNIT_FIELD_CHARM);
    }
    void SetCharmGuid(ObjectGuid charm)
    {
        SetGuidValue(UNIT_FIELD_CHARM, charm);
    }
    ObjectGuid const& GetTargetGuid() const
    {
        return GetGuidValue(UNIT_FIELD_TARGET);
    }
    void SetTargetGuid(ObjectGuid targetGuid)
    {
        SetGuidValue(UNIT_FIELD_TARGET, targetGuid);
    }
    ObjectGuid const& GetChannelObjectGuid() const
    {
        return GetGuidValue(UNIT_FIELD_CHANNEL_OBJECT);
    }
    void SetChannelObjectGuid(ObjectGuid targetGuid)
    {
        SetGuidValue(UNIT_FIELD_CHANNEL_OBJECT, targetGuid);
    }

    virtual Pet* GetMiniPet() const { return nullptr; } // overwrited in Player

    ObjectGuid const& GetCharmerOrOwnerGuid() const
    {
        return GetCharmerGuid() ? GetCharmerGuid() : GetOwnerGuid();
    }
    ObjectGuid const& GetCharmerOrOwnerOrOwnGuid() const
    {
        if (ObjectGuid const& guid = GetCharmerOrOwnerGuid())
            return guid;
        return GetObjectGuid();
    }
    bool isCharmedOwnedByPlayerOrPlayer() const
    {
        return GetCharmerOrOwnerOrOwnGuid().IsPlayer();
    }

    Player* GetSpellModOwner() const;

    Unit* GetOwner() const;
    Pet* GetPet() const;
    Unit* GetCharmer() const;
    Unit* GetCharm() const;
    void Uncharm();
    Unit* GetCharmerOrOwner() const
    {
        return GetCharmerGuid() ? GetCharmer() : GetOwner();
    }
    Unit* GetCharmerOrOwnerOrTotemOwner() const;
    Unit* GetCharmerOrOwnerOrSelf()
    {
        if (Unit* u = GetCharmerOrOwner())
            return u;

        return this;
    }
    bool IsCharmerOrOwnerPlayerOrPlayerItself() const;
    Player* GetAffectingPlayer() const;
    Player* GetCharmerOrOwnerPlayerOrPlayerItself();
    Player const* GetCharmerOrOwnerPlayerOrPlayerItself() const;
    // Returns true if unit is a player, or if owned or controlled by a player
    bool player_controlled() const;
    float GetCombatDistance(const Unit* target) const;

    // returns: true if spells etc cannot hit this target because of pvp flags
    bool ShouldIgnoreTargetBecauseOfPvpFlag(Unit* target) const;

    void SetPet(Pet* pet);
    void SetCharm(Unit* pet);
    void AddCharm(Unit* target)
    {
        charm_targets_.insert(target->GetObjectGuid());
    }
    void RemoveCharm(Unit* target)
    {
        auto itr = charm_targets_.find(target->GetObjectGuid());
        if (itr != charm_targets_.end())
            charm_targets_.erase(itr);
    }
    void InterruptCharms();

    void AddGuardian(Pet* pet);
    void RemoveGuardian(Pet* pet);
    void RemoveGuardians();
    Pet* FindGuardianWithEntry(uint32 entry);

    bool isCharmed() const { return !GetCharmerGuid().IsEmpty(); }

    CharmInfo* GetCharmInfo() { return m_charmInfo; }
    CharmInfo* InitCharmInfo(Unit* charm);
    void DeleteCharmInfo();

    ObjectGuid const& GetTotemGuid(TotemSlot slot) const
    {
        return m_TotemSlot[slot];
    }
    Totem* GetTotem(TotemSlot slot) const;
    bool IsAllTotemSlotsUsed() const;

    void _AddTotem(
        TotemSlot slot, Totem* totem); // only for call from Totem summon code
    void _RemoveTotem(Totem* totem);   // only for call from Totem class

    template <typename Func>
    void CallForAllControlledUnits(Func const& func, uint32 controlledMask);
    template <typename Func>
    bool CheckAllControlledUnits(Func const& func, uint32 controlledMask) const;

    // Called after we applied a single-target aura on someone
    void AddSingleTargetAura(AuraHolder* holder, ObjectGuid target);

    bool AddAuraHolder(AuraHolder* holder);
    void RemoveAuraHolder(
        AuraHolder* holder, AuraRemoveMode mode = AURA_REMOVE_BY_DEFAULT);
    // NOTE: prefer CastSpell over this function; it's meant for the cases when
    // you intend to bypass all Spell checks, etc
    // duration > 0: custom max duration
    void AddAuraThroughNewHolder(
        uint32 spell_id, Unit* caster, int duration = 0);

    static bool aura_no_op_true(AuraHolder*) { return true; }

    inline void remove_auras(AuraRemoveMode mode = AURA_REMOVE_BY_DEFAULT);
    template <typename F>
    void remove_auras_if(F func, AuraRemoveMode mode = AURA_REMOVE_BY_DEFAULT);
    template <typename F = decltype(aura_no_op_true)>
    void remove_auras(uint32 id, F func = aura_no_op_true,
        AuraRemoveMode mode = AURA_REMOVE_BY_DEFAULT);
    template <typename F = decltype(aura_no_op_true)>
    void remove_auras(AuraType type, F func = aura_no_op_true,
        AuraRemoveMode mode = AURA_REMOVE_BY_DEFAULT);
    template <typename F = decltype(aura_no_op_true)>
    AuraHolder* get_aura(uint32 spellId, ObjectGuid caster = ObjectGuid(),
        F func = aura_no_op_true) const;
    template <typename F = decltype(aura_no_op_true)>
    AuraHolder* get_aura(AuraType type, ObjectGuid caster = ObjectGuid(),
        F func = aura_no_op_true) const;
    // func: return true to continue, false to break
    template <typename F>
    void loop_auras(F func, uint32 id = 0) const;

    // flags: SpellAuraInterruptFlags
    void remove_auras_on_event(uint32 flags, uint32 ignore = 0);

    void AddAuraToModList(Aura* aura);
    void RemoveAuraFromModList(Aura* aura);
    const Auras& GetAurasByType(AuraType type) const
    {
        return m_modAuras[type];
    }

    // Specialized aura removal functions
    void RemoveArenaAuras(bool onleave = false);

    void DelaySpellAuraHolder(
        uint32 spellId, int32 delaytime, ObjectGuid casterGuid);

    float GetResistanceBuffMods(SpellSchools school, bool positive) const
    {
        return GetFloatValue(
            positive ? UNIT_FIELD_RESISTANCEBUFFMODSPOSITIVE + school :
                       UNIT_FIELD_RESISTANCEBUFFMODSNEGATIVE + school);
    }
    void SetResistanceBuffMods(SpellSchools school, bool positive, float val)
    {
        SetFloatValue(positive ?
                          UNIT_FIELD_RESISTANCEBUFFMODSPOSITIVE + school :
                          UNIT_FIELD_RESISTANCEBUFFMODSNEGATIVE + school,
            val);
    }
    void ApplyResistanceBuffModsMod(
        SpellSchools school, bool positive, float val, bool apply)
    {
        ApplyModSignedFloatValue(
            positive ? UNIT_FIELD_RESISTANCEBUFFMODSPOSITIVE + school :
                       UNIT_FIELD_RESISTANCEBUFFMODSNEGATIVE + school,
            val, apply);
    }
    void ApplyResistanceBuffModsPercentMod(
        SpellSchools school, bool positive, float val, bool apply)
    {
        ApplyPercentModFloatValue(
            positive ? UNIT_FIELD_RESISTANCEBUFFMODSPOSITIVE + school :
                       UNIT_FIELD_RESISTANCEBUFFMODSNEGATIVE + school,
            val, apply);
    }
    void InitStatBuffMods()
    {
        for (int i = STAT_STRENGTH; i < MAX_STATS; ++i)
            SetFloatValue(UNIT_FIELD_POSSTAT0 + i, 0);
        for (int i = STAT_STRENGTH; i < MAX_STATS; ++i)
            SetFloatValue(UNIT_FIELD_NEGSTAT0 + i, 0);
    }
    void ApplyStatBuffMod(Stats stat, float val, bool apply)
    {
        ApplyModSignedFloatValue(
            (val > 0 ? UNIT_FIELD_POSSTAT0 + stat : UNIT_FIELD_NEGSTAT0 + stat),
            val, apply);
    }
    void ApplyStatPercentBuffMod(Stats stat, float val, bool apply)
    {
        ApplyPercentModFloatValue(UNIT_FIELD_POSSTAT0 + stat, val, apply);
        ApplyPercentModFloatValue(UNIT_FIELD_NEGSTAT0 + stat, val, apply);
    }
    void SetCreateStat(Stats stat, float val) { m_createStats[stat] = val; }
    void SetCreateHealth(uint32 val)
    {
        SetUInt32Value(UNIT_FIELD_BASE_HEALTH, val);
    }
    uint32 GetCreateHealth() const
    {
        return GetUInt32Value(UNIT_FIELD_BASE_HEALTH);
    }
    void SetCreateMana(uint32 val)
    {
        SetUInt32Value(UNIT_FIELD_BASE_MANA, val);
    }
    uint32 GetCreateMana() const
    {
        return GetUInt32Value(UNIT_FIELD_BASE_MANA);
    }
    uint32 GetCreatePowers(Powers power) const;
    float GetPosStat(Stats stat) const
    {
        return GetFloatValue(UNIT_FIELD_POSSTAT0 + stat);
    }
    float GetNegStat(Stats stat) const
    {
        return GetFloatValue(UNIT_FIELD_NEGSTAT0 + stat);
    }
    float GetCreateStat(Stats stat) const { return m_createStats[stat]; }

    void SetCurrentCastedSpell(Spell* pSpell);
    virtual void ProhibitSpellSchool(
        SpellSchoolMask /*idSchoolMask*/, uint32 /*unTimeMs*/)
    {
    }
    void InterruptSpell(CurrentSpellTypes spellType, bool withDelayed = true,
        Spell* replacedBy = nullptr, bool send_autorepeat = true);
    void FinishSpell(CurrentSpellTypes spellType, bool ok = true);
    // Interrupts all spells casted on the target
    void InterruptSpellOn(Unit* target);

    // set withDelayed to true to account delayed spells as casted
    // delayed+channeled spells are always accounted as casted
    // we can skip channeled or delayed checks using flags
    bool IsNonMeleeSpellCasted(bool withDelayed, bool skipChanneled = false,
        bool skipAutorepeat = false) const;

    // returns true if currently casted/channeled spells are preventing the
    // unit to move or auto-attack without breaking said spell
    bool IsCastedSpellPreventingMovementOrAttack() const;

    // set withDelayed to true to interrupt delayed spells too
    // delayed+channeled spells are always interrupted
    void InterruptNonMeleeSpells(bool withDelayed, uint32 spellid = 0);

    Spell* GetCurrentSpell(CurrentSpellTypes spellType) const
    {
        return m_currentSpells[spellType];
    }
    Spell* FindCurrentSpellBySpellId(uint32 spell_id) const;

    bool CheckAndIncreaseCastCounter();
    void DecreaseCastCounter()
    {
        if (m_castCounter)
            --m_castCounter;
    }

    ObjectGuid m_ObjectSlotGuid[4];
    uint32 m_detectInvisibilityMask;
    uint32 m_invisibilityMask;

    ShapeshiftForm GetShapeshiftForm() const
    {
        return ShapeshiftForm(GetByteValue(UNIT_FIELD_BYTES_2, 3));
    }
    void SetShapeshiftForm(ShapeshiftForm form)
    {
        SetByteValue(UNIT_FIELD_BYTES_2, 3, form);
    }

    bool IsInFeralForm() const
    {
        ShapeshiftForm form = GetShapeshiftForm();
        return form == FORM_CAT || form == FORM_BEAR || form == FORM_DIREBEAR;
    }

    bool IsInDisallowedMountForm() const
    {
        ShapeshiftForm form = GetShapeshiftForm();
        return form != FORM_NONE && form != FORM_BATTLESTANCE &&
               form != FORM_BERSERKERSTANCE && form != FORM_DEFENSIVESTANCE &&
               form != FORM_SHADOW;
    }

    float m_modMeleeHitChance;
    float m_modRangedHitChance;
    float m_modSpellHitChance;
    int32 m_baseSpellCritChance;

    float m_threatModifier[MAX_SPELL_SCHOOL];
    float m_modAttackSpeedPct[3];

    // Event handler
    EventProcessor m_Events;

    // stat system
    bool HandleStatModifier(UnitMods unitMod, UnitModifierType modifierType,
        float amount, bool apply);
    void SetModifierValue(
        UnitMods unitMod, UnitModifierType modifierType, float value)
    {
        m_auraModifiersGroup[unitMod][modifierType] = value;
    }
    float GetModifierValue(
        UnitMods unitMod, UnitModifierType modifierType) const;
    float GetTotalStatValue(Stats stat, float flat_incr = 0.0f) const;
    float GetTotalAuraModValue(UnitMods unitMod) const;
    SpellSchools GetSpellSchoolByAuraGroup(UnitMods unitMod) const;
    Stats GetStatByAuraGroup(UnitMods unitMod) const;
    Powers GetPowerTypeByAuraGroup(UnitMods unitMod) const;
    bool CanModifyStats() const { return m_canModifyStats; }
    void SetCanModifyStats(bool modifyStats) { m_canModifyStats = modifyStats; }
    virtual bool UpdateStats(Stats stat) = 0;
    virtual bool UpdateAllStats() = 0;
    virtual void UpdateResistances(uint32 school) = 0;
    virtual void UpdateArmor() = 0;
    virtual void UpdateMaxHealth() = 0;
    virtual void UpdateMaxPower(Powers power) = 0;
    virtual void UpdateAttackPowerAndDamage(bool ranged = false) = 0;
    virtual void UpdateDamagePhysical(WeaponAttackType attType) = 0;
    float GetTotalAttackPowerValue(WeaponAttackType attType) const;
    float GetWeaponDamageRange(
        WeaponAttackType attType, WeaponDamageRange type) const;
    void SetBaseWeaponDamage(
        WeaponAttackType attType, WeaponDamageRange damageRange, float value)
    {
        m_weaponDamage[attType][damageRange] = value;
    }
    // The stat system is not cleanly extensible to account for
    // positive/negative AP modifiers,

    // Visibility system
    UnitVisibility GetVisibility() const { return m_Visibility; }
    void SetVisibility(UnitVisibility x);
    void UpdateVisibilityAndView() override; // overwrite
    // WorldObject::UpdateVisibilityAndView()

    // common function for visibility checks for player/creatures
    // u: the unit that is looking at *this, can he see us?
    // viewPoint: where u's camer is located
    bool can_be_seen_by(const Unit* u, const WorldObject* viewPoint,
        bool inVisibleList = false, bool is3dDistance = true,
        bool isThreatCheck = false) const;
    // u: the unit shooting a delayed spell at us
    bool can_be_hit_by_delayed_spell_stealth_check(const Unit* u) const;
    bool canDetectInvisibilityOf(Unit const* u) const;
    float stealth_detect_dist(const Unit* rhs) const;

    // virtual functions for all world objects types
    bool isVisibleForInState(Player const* u, WorldObject const* viewPoint,
        bool inVisibleList) const override;
    // function for low level grid visibility checks in player/creature cases
    virtual bool IsVisibleInGridForPlayer(Player* pl) const = 0;
    bool isInvisibleForAlive() const;

    SpellImmuneList m_spellImmune[MAX_SPELL_IMMUNITY];

    // Threat related methods
    bool CanHaveThreatList() const;
    void AddThreat(Unit* pVictim, float threat = 0.0f, bool crit = false,
        SpellSchoolMask schoolMask = SPELL_SCHOOL_MASK_NONE,
        SpellEntry const* threatSpell = nullptr, bool byGroup = false,
        bool untauntable_threat = false);
    float ApplyTotalThreatModifier(
        float threat, SpellSchoolMask schoolMask = SPELL_SCHOOL_MASK_NORMAL);
    void DeleteThreatList();
    bool IsSecondChoiceTarget(Unit* pTarget, bool checkThreatArea);
    bool SelectHostileTarget(bool force = false);
    void TauntApply(Unit* pVictim);
    void TauntFadeOut(Unit* taunter);
    ThreatManager& getThreatManager() { return m_ThreatManager; }
    ThreatManager const& getThreatManager() const { return m_ThreatManager; }
    void addHatedBy(HostileReference* pHostileReference)
    {
        m_HostileRefManager.insertFirst(pHostileReference);
    };
    void removeHatedBy(HostileReference* /*pHostileReference*/)
    { /* nothing to do yet */}
    HostileRefManager& getHostileRefManager() { return m_HostileRefManager; }
    void SaveThreatList();
    void RestoreThreatList();

    int32 GetTotalAuraModifier(AuraType auratype) const;
    float GetTotalAuraMultiplier(AuraType auratype) const;
    int32 GetMaxPositiveAuraModifier(AuraType auratype) const;
    int32 GetMaxNegativeAuraModifier(AuraType auratype) const;

    int32 GetTotalAuraModifierByMiscMask(
        AuraType auratype, uint32 misc_mask) const;
    float GetTotalAuraMultiplierByMiscMask(
        AuraType auratype, uint32 misc_mask) const;
    int32 GetMaxPositiveAuraModifierByMiscMask(
        AuraType auratype, uint32 misc_mask) const;
    int32 GetMaxNegativeAuraModifierByMiscMask(
        AuraType auratype, uint32 misc_mask) const;

    int32 GetTotalAuraModifierByMiscValue(
        AuraType auratype, int32 misc_value) const;
    float GetTotalAuraMultiplierByMiscValue(
        AuraType auratype, int32 misc_value) const;
    int32 GetMaxPositiveAuraModifierByMiscValue(
        AuraType auratype, int32 misc_value) const;
    int32 GetMaxNegativeAuraModifierByMiscValue(
        AuraType auratype, int32 misc_value) const;

    uint32 m_AuraFlags;

    uint32 GetDisplayId() const { return GetUInt32Value(UNIT_FIELD_DISPLAYID); }
    void SetDisplayId(uint32 modelId);
    uint32 GetNativeDisplayId() const
    {
        return GetUInt32Value(UNIT_FIELD_NATIVEDISPLAYID);
    }
    void SetNativeDisplayId(uint32 modelId)
    {
        SetUInt32Value(UNIT_FIELD_NATIVEDISPLAYID, modelId);
    }
    void setTransForm(uint32 spellid) { m_transform = spellid; }
    uint32 getTransForm() const { return m_transform; }

    // at any changes to scale and/or displayId
    void UpdateModelData();

    DynamicObject* GetDynObject(uint32 spellId, SpellEffectIndex effIndex);
    std::list<DynamicObject*> GetAllDynObjects(
        uint32 spellId, SpellEffectIndex effIndex);
    DynamicObject* GetDynObject(uint32 spellId);
    void AddDynObject(DynamicObject* dynObj);
    void RemoveDynObject(uint32 spellid);
    void RemoveDynObjectWithGUID(ObjectGuid guid)
    {
        m_dynObjGUIDs.remove(guid);
    }
    void RemoveAllDynObjects();

    GameObject* GetGameObject(uint32 spellId) const;
    void AddGameObject(GameObject* gameObj);
    void RemoveGameObject(GameObject* gameObj, bool del);
    void RemoveGameObject(uint32 spellid, bool del);
    void RemoveAllGameObjects();

    uint32 CalculateDamage(WeaponAttackType attType, bool normalized);
    float GetAPMultiplier(WeaponAttackType attType, bool normalized);
    void ModifyAuraState(AuraState flag, bool apply);
    bool HasAuraState(AuraState flag) const
    {
        return HasFlag(UNIT_FIELD_AURASTATE, 1 << (flag - 1));
    }
    bool HasAuraStateForCaster(AuraState flag, ObjectGuid casterGuid) const;
    void UnsummonAllTotems();
    Unit* SelectMagnetTarget(Unit* victim, Spell* spell = nullptr,
        SpellEffectIndex eff = EFFECT_INDEX_0);

    int32 SpellBonusWithCoeffs(SpellEntry const* spellProto, int32 total,
        int32 benefit, int32 ap_benefit, DamageEffectType damagetype,
        bool donePart, Unit* pCaster = nullptr);
    int32 SpellBaseDamageBonusDone(
        SpellSchoolMask schoolMask, uint32 Id, bool exact_mask = false);
    int32 SpellBaseDamageBonusTaken(
        SpellSchoolMask schoolMask, bool exact_mask = false);
    uint32 SpellDamageBonusDone(Unit* pVictim, SpellEntry const* spellProto,
        uint32 pdamage, DamageEffectType damagetype, uint32 stack = 1);
    uint32 SpellDamageBonusTaken(Unit* pCaster, SpellEntry const* spellProto,
        uint32 pdamage, DamageEffectType damagetype, uint32 stack = 1);
    int32 SpellBaseHealingBonusDone(SpellSchoolMask schoolMask, uint32 Id);
    int32 SpellBaseHealingBonusTaken(SpellSchoolMask schoolMask);
    uint32 SpellHealingBonusDone(Unit* pVictim, SpellEntry const* spellProto,
        int32 healamount, DamageEffectType damagetype, uint32 stack = 1);
    uint32 SpellHealingBonusTaken(Unit* pCaster, SpellEntry const* spellProto,
        int32 healamount, DamageEffectType damagetype, uint32 stack = 1);
    uint32 MeleeDamageBonusDone(Unit* pVictim, uint32 damage,
        WeaponAttackType attType, SpellEntry const* spellProto = nullptr,
        DamageEffectType damagetype = DIRECT_DAMAGE, uint32 stack = 1);
    uint32 MeleeDamageBonusTaken(Unit* pCaster, uint32 pdamage,
        WeaponAttackType attType, SpellEntry const* spellProto = nullptr,
        DamageEffectType damagetype = DIRECT_DAMAGE, uint32 stack = 1);

    bool IsSpellBlocked(Unit* pCaster, SpellEntry const* spellProto,
        WeaponAttackType attackType = BASE_ATTACK);
    bool IsSpellCrit(Unit* pVictim, SpellEntry const* spellProto,
        SpellSchoolMask schoolMask, WeaponAttackType attackType = BASE_ATTACK,
        float crit_mod = 0.0f, const Spell* spell = nullptr);
    uint32 SpellCriticalDamageBonus(
        SpellEntry const* spellProto, uint32 damage, Unit* pVictim);
    uint32 SpellCriticalHealingBonus(
        SpellEntry const* spellProto, uint32 damage, Unit* pVictim);

    bool IsTriggeredAtSpellProcEvent(Unit* pVictim, AuraHolder* holder,
        SpellEntry const* procSpell, uint32 procFlag, uint32 procExtra,
        WeaponAttackType attType, bool isVictim,
        SpellProcEventEntry const*& spellProcEvent);
    // Aura proc handlers
    SpellAuraProcResult HandleDummyAuraProc(Unit* pVictim, proc_amount amount,
        Aura* triggeredByAura, SpellEntry const* procSpell, uint32 procFlag,
        uint32 procEx, uint32 cooldown, ExtraAttackType extraAttackType,
        uint32 extraAttackId);
    SpellAuraProcResult HandleHasteAuraProc(Unit* pVictim, proc_amount amount,
        Aura* triggeredByAura, SpellEntry const* procSpell, uint32 procFlag,
        uint32 procEx, uint32 cooldown, ExtraAttackType extraAttackType,
        uint32 extraAttackId);
    SpellAuraProcResult HandleProcTriggerSpellAuraProc(Unit* pVictim,
        proc_amount amount, Aura* triggeredByAura, SpellEntry const* procSpell,
        uint32 procFlag, uint32 procEx, uint32 cooldown,
        ExtraAttackType extraAttackType, uint32 extraAttackId);
    SpellAuraProcResult HandleProcTriggerDamageAuraProc(Unit* pVictim,
        proc_amount amount, Aura* triggeredByAura, SpellEntry const* procSpell,
        uint32 procFlag, uint32 procEx, uint32 cooldown,
        ExtraAttackType extraAttackType, uint32 extraAttackId);
    SpellAuraProcResult HandleOverrideClassScriptAuraProc(Unit* pVictim,
        proc_amount amount, Aura* triggeredByAura, SpellEntry const* procSpell,
        uint32 procFlag, uint32 procEx, uint32 cooldown,
        ExtraAttackType extraAttackType, uint32 extraAttackId);
    SpellAuraProcResult HandleMendingAuraProc(Unit* pVictim, proc_amount amount,
        Aura* triggeredByAura, SpellEntry const* procSpell, uint32 procFlag,
        uint32 procEx, uint32 cooldown, ExtraAttackType extraAttackType,
        uint32 extraAttackId);
    SpellAuraProcResult HandleModCastingSpeedNotStackAuraProc(Unit* pVictim,
        proc_amount amount, Aura* triggeredByAura, SpellEntry const* procSpell,
        uint32 procFlag, uint32 procEx, uint32 cooldown,
        ExtraAttackType extraAttackType, uint32 extraAttackId);
    SpellAuraProcResult HandleReflectSpellsSchoolAuraProc(Unit* pVictim,
        proc_amount amount, Aura* triggeredByAura, SpellEntry const* procSpell,
        uint32 procFlag, uint32 procEx, uint32 cooldown,
        ExtraAttackType extraAttackType, uint32 extraAttackId);
    SpellAuraProcResult HandleModPowerCostSchoolAuraProc(Unit* pVictim,
        proc_amount amount, Aura* triggeredByAura, SpellEntry const* procSpell,
        uint32 procFlag, uint32 procEx, uint32 cooldown,
        ExtraAttackType extraAttackType, uint32 extraAttackId);
    SpellAuraProcResult HandleMechanicImmuneResistanceAuraProc(Unit* pVictim,
        proc_amount amount, Aura* triggeredByAura, SpellEntry const* procSpell,
        uint32 procFlag, uint32 procEx, uint32 cooldown,
        ExtraAttackType extraAttackType, uint32 extraAttackId);
    SpellAuraProcResult HandleManaShieldAuraProc(Unit* pVictim,
        proc_amount amount, Aura* triggeredByAura, SpellEntry const* procSpell,
        uint32 procFlag, uint32 procEx, uint32 cooldown,
        ExtraAttackType extraAttackType, uint32 extraAttackId);
    SpellAuraProcResult HandleAttackPowerAttackerBonusAuraProc(Unit* pVictim,
        proc_amount amount, Aura* triggeredByAura, SpellEntry const* procSpell,
        uint32 procFlag, uint32 procEx, uint32 cooldown,
        ExtraAttackType extraAttackType, uint32 extraAttackId);
    SpellAuraProcResult HandleModResistanceAuraProc(Unit* pVictim,
        proc_amount amount, Aura* triggeredByAura, SpellEntry const* procSpell,
        uint32 procFlag, uint32 procEx, uint32 cooldown,
        ExtraAttackType extraAttackType, uint32 extraAttackId);
    SpellAuraProcResult HandleNULLProc(Unit* /*pVictim*/,
        proc_amount /*amount*/, Aura* /*triggeredByAura*/,
        SpellEntry const* /*procSpell*/, uint32 /*procFlag*/, uint32 /*procEx*/,
        uint32 /*cooldown*/, ExtraAttackType /*extraAttackType*/,
        uint32 /*extraAttackId*/)
    {
        // no proc handler for this aura type
        return SPELL_AURA_PROC_OK;
    }
    SpellAuraProcResult HandleCantTrigger(Unit* /*pVictim*/,
        proc_amount /*damage*/, Aura* /*triggeredByAura*/,
        SpellEntry const* /*procSpell*/, uint32 /*procFlag*/, uint32 /*procEx*/,
        uint32 /*cooldown*/, ExtraAttackType /*extraAttackType*/,
        uint32 /*extraAttackId*/)
    {
        // this aura type can't proc
        return SPELL_AURA_PROC_CANT_TRIGGER;
    }

    void SetLastManaUse() { m_lastManaUseTimer = 5000; }
    bool IsUnderLastManaUseEffect() const { return m_lastManaUseTimer; }

    void SetContestedPvP(Player* attackedPlayer = nullptr);

    void ApplySpellImmune(
        const Aura* owner, uint32 op, uint32 type, bool apply);
    void ApplySpellDispelImmunity(
        const Aura* owner, DispelType type, bool apply);
    // redefined in Creature
    virtual bool IsImmuneToSpell(SpellEntry const* spellInfo);
    bool IsImmunedToDamage(SpellSchoolMask meleeSchoolMask);
    // redefined in Creature
    virtual bool IsImmuneToSpellEffect(
        SpellEntry const* spellInfo, SpellEffectIndex index) const;
    bool IsImmuneToHealing() const;
    bool IsImmuneToMechanic(Mechanics mechanic) const;

    // ===================================================
    // Damage Calculations
    // do_* functions
    //
    // Below are functions that apply some component to the damage calculation
    // process. If multiple do_* functions are required, use them in the order
    // they appear in the header unless you have reasons to rearrange the order
    // of calculations.
    // ===================================================

    // Apply spell block rules to spell damage
    // Returns: how many damage points were blocked
    uint32 do_spell_block(Unit* attacker, uint32 damage,
        const SpellEntry* spell_info,
        WeaponAttackType attack_type = BASE_ATTACK);

    // Apply resistance rules to school damage
    // Returns: how many damage points were resisted
    uint32 do_resistance(Unit* attacker, uint32 damage, uint32 school_mask);

    // Apply resistance rules to spell damage
    // Returns: how many damage points were resisted
    uint32 do_resistance(
        Unit* caster, uint32 damage, const SpellEntry* spell_info);

    // Apply absorb to damage
    // Note: This will consume remaining damage on active absorb auras
    //       And cause spells like reflective shield to trigger
    // Params:
    //     @can_reflect_damage: If auras to reflect some damage on absorb
    //     exists, and this is true, the reflect damage will be applied to
    //     attacker.
    // Returns: how many damage points were absorbed
    uint32 do_absorb(Unit* attacker, uint32 damage, uint32 school_mask,
        bool can_reflect_damage);

    // Apply sharing of damage (where some of it is absorbed by another unit)
    // Note: This will potentially damage other units
    // Returns: damage points absorbed by damage sharing
    uint32 do_damage_sharing(Unit* attacker, uint32 damage, uint32 school_mask);

    // Applies death prevention if damage is enough to kill target
    // Note: Only handles Rogue's Cheat Death at the moment
    // Returns: how many damage points were absorbed
    uint32 do_death_prevention(uint32 damage);

    // Helper function to apply multiple dos that are commonly used together
    // Applies: resist, absorb, damage sharing and death prevention
    // Params:
    //     @spell_info & @school_mask: Only need one or the other
    // Returns: remaining damage
    uint32 do_resist_absorb_helper(Unit* attacker, uint32 damage,
        const SpellEntry* spell_info, bool can_reflect_damage,
        uint32 school_mask = 0, uint32* absorb_out = nullptr,
        uint32* resist_out = nullptr);

    // Reduce damage by armor
    uint32 calculate_armor_reduced_damage(Unit* victim, uint32 damage);

    // Calculate partial resistance percentage of given spell
    float calculate_partial_resistance(Unit* caster, uint32 schol_mask);

    // Roll for binary resistance
    bool roll_binary_spell(Unit* caster, const SpellEntry* spell);

    void UpdateSpeed(UnitMoveType mtype, bool forced, float ratio = 1.0f);
    float GetSpeed(UnitMoveType mtype) const;
    float GetSpeedRate(UnitMoveType mtype) const { return m_speed_rate[mtype]; }
    void SetSpeedRate(UnitMoveType mtype, float rate, bool forced = false);

    bool isHover() const { return HasAuraType(SPELL_AURA_HOVER); }

    void _RemoveAllAuraMods();
    void _ApplyAllAuraMods();

    int32 CalculateSpellDamage(Unit const* target, SpellEntry const* spellProto,
        SpellEffectIndex effect_index, int32 const* basePoints = nullptr);

    int32 CalculateAuraDuration(SpellEntry const* spellProto, uint32 effectMask,
        int32 duration, Unit const* caster);

    // Returns scaling factor for spell downranking
    float calculate_spell_downrank_factor(const SpellEntry* spell_info) const;

    // only_directional: if true does not count jumping, etc as moving
    bool is_moving(bool only_directional = false) const;
    // estimate: if true, the position is 'unsafe' (i.e., don't tp/run to)
    G3D::Vector3 predict_dest(int milliseconds, bool estimate = true);

    bool IsStopped() const { return !(hasUnitState(UNIT_STAT_MOVING)); }
    void StopMoving();

    void CharmUnit(bool apply, bool removeByDelete, Unit* target,
        AuraHolder* aura = nullptr);
    void SetFeignDeath(
        bool apply, ObjectGuid casterGuid = ObjectGuid(), uint32 spellID = 0);

    // returns true if unit is a player, or a player owned pet
    bool player_or_pet() const;

    // returns true if charmed/owned by unit
    bool controlled_by(Unit* unit) const
    {
        return GetOwnerGuid() == unit->GetObjectGuid() ||
               GetCharmerGuid() == unit->GetObjectGuid();
    }

    // Invoked when a concurrent path finished generating for the unit
    void finished_path(std::vector<G3D::Vector3> path, uint32 id);
    // For spells waiting on pregenerated path
    void spell_pathgen_callback(std::vector<G3D::Vector3> path, uint32 id);

    // returns whether unit can pathing cheat _right now_ (does not necessarily
    // hold true prev/later)
    bool can_pathing_cheat() const;

    void Root(bool enable);

private:
    // counters for SetFeared and SetConfused
    unsigned int m_confuseCount;
    unsigned int m_fleeCount;

public:
    void AddComboPointHolder(uint32 lowguid)
    {
        m_ComboPointHolders.insert(lowguid);
    }
    void RemoveComboPointHolder(uint32 lowguid)
    {
        m_ComboPointHolders.erase(lowguid);
    }
    void ClearComboPointHolders();

    ///----------Pet responses methods-----------------
    void SendPetCastFail(uint32 spellid, SpellCastResult msg);
    void SendPetActionFeedback(uint8 msg);
    void SendPetTalk(uint32 pettalk);
    void SendPetAIReaction();
    ///----------End of Pet responses methods----------

    // reactive attacks
    void ClearAllReactives();
    void StartReactiveTimer(ReactiveType reactive)
    {
        m_reactiveTimer[reactive] = REACTIVE_TIMER_START;
    }
    void UpdateReactives(uint32 p_time);

    // group updates
    void UpdateAuraForGroup(uint8 slot);

    // pet auras
    typedef std::set<PetAura const*> PetAuraSet;
    PetAuraSet m_petAuras;
    void AddPetAura(PetAura const* petSpell);
    void RemovePetAura(PetAura const* petSpell);

    // Spline
    movement::MoveSpline* movespline;

    void ScheduleAINotify(uint32 delay);
    bool IsAINotifyScheduled() const { return m_AINotifyScheduled; }
    void _SetAINotifyScheduled(bool on)
    {
        m_AINotifyScheduled = on;
    } // only for call from RelocationNotifyEvent code
    void OnRelocated();

    bool HasDispellableBuff(DispelType);
    bool HasDispellableDebuff(DispelType);

    bool IsCastingRitual();
    GameObject* GetCastedRitual();

    float GetInterruptAndSilenceModifier(
        const SpellEntry* info, uint32 eff_mask) const;

    const GuardianPetList& GetGuardians() const { return m_guardianPets; }

    // dry_run: returns spells we could _potentially_ dispel, does not roll
    //          against them, etc
    std::vector<std::pair<AuraHolder*, uint32 /*stack amount*/>>
    get_dispel_buffs(int count, uint32 dispel_mask, bool steal_buff,
        Unit* caster, const Spell* spell = nullptr,
        std::vector<uint32>* fail_list = nullptr, bool reflected = false,
        bool dry_run = false) const;

    bool AutoRepeatFirstCast() const { return m_AutoRepeatFirstCast; }

    void queue_spell_hit(Spell* spell);
    std::vector<spell_ref>& spell_queue() { return spell_queue_; }

    void force_stealth_update_timer(uint32 update_in);

    void SetMagePolymorphed(bool apply)
    {
        m_polyRegenTimer = apply ? REGEN_POLY_TIME : -1;
    }

    movement::GeneratorQueue movement_gens;
    ObjectGuid overpower_target;

protected:
    explicit Unit();

    void _UpdateSpells(uint32 time);
    void _UpdateAutoRepeatSpell();
    bool m_AutoRepeatFirstCast;

    uint32 m_attackTimer[MAX_ATTACK];

    float m_createStats[MAX_STATS];

    AttackerSet m_attackers;
    Unit* m_attacking;
    ObjectGuid m_attackingGuid;

    DeathState m_deathState;

    // While iterating m_auraHolders, create one of these, when it's destroyed
    // any queued aura holder will be added to the unit
    struct aura_holder_guard
    {
        aura_holder_guard(Unit* owner);
        ~aura_holder_guard();
        Unit* owner_;
    };
    int guarding_holders_;
    std::vector<AuraHolder*> m_queuedHolderAdds;
    AuraHolderMap m_auraHolders;
    std::map<uint32, ObjectGuid>
        m_singleTargetAuras; // targets we have affected by single-target auras

    typedef std::list<ObjectGuid> DynObjectGUIDs;
    DynObjectGUIDs m_dynObjGUIDs;

    typedef std::list<GameObject*> GameObjectList;
    GameObjectList m_gameObj;
    bool m_isSorted;
    uint32 m_transform;

    Auras m_modAuras[TOTAL_AURAS];
    float m_auraModifiersGroup[UNIT_MOD_END][MODIFIER_TYPE_END];
    uint32 ap_buffs_[UNIT_AP_BUFF_END];
    float m_weaponDamage[MAX_ATTACK][2];
    bool m_canModifyStats;

    float m_speed_rate[MAX_MOVE_TYPE];

    CharmInfo* m_charmInfo;

    virtual SpellSchoolMask GetMeleeDamageSchoolMask() const;

    uint32 m_reactiveTimer[MAX_REACTIVE];
    uint32 m_regenTimer;
    int32 m_polyRegenTimer; // -1 when polymorph isn't on target
    uint32 m_lastManaUseTimer;

    void DisableSpline();

    std::set<ObjectGuid> charm_targets_;

    ShortIntervalTimer stealth_update_timer_; // Timer to next stealth update
    std::vector<ObjectGuid> stealth_detected_by_; // This is a a vector of units
                                                  // that can currently see us
                                                  // despite us being stealthed,
                                                  // always empty if not
                                                  // stealthed

private:
    void FinalAddAuraHolder(AuraHolder* holder, bool was_delayed);
    void CleanDisabledAuras();

    void UpdateSplineMovement(uint32 t_diff);
    void UpdateSplinePosition();
    void update_stealth();
    bool can_stealth_against(const Unit* target) const;
    bool can_stealth_detect(Unit* target);

    Unit* _GetTotem(
        TotemSlot slot) const; // for templated function without include need
    Pet* _GetPet(
        ObjectGuid guid) const; // for templated function without include need

    uint32 m_state; // Even derived shouldn't modify

    Spell* m_currentSpells[CURRENT_MAX_SPELL];
    uint32 m_castCounter; // count casts chain of triggered spells for prevent
                          // infinity cast crashes

    UnitVisibility m_Visibility;
    Position m_last_notified_position;
    bool m_AINotifyScheduled;
    ShortTimeTracker m_movesplineTimer;

    Diminishing m_Diminishing;
    ShortIntervalTimer m_diminishingTimer;

    // Manage all Units threatening us
    ThreatManager m_ThreatManager;
    // Manage all Units that are threatened by us
    HostileRefManager m_HostileRefManager;
    // pair<guid, threat>: saved on charm, reapplied on expire
    std::vector<std::pair<ObjectGuid, uint32>> saved_threat;

    ComboPointHolderSet m_ComboPointHolders;

    GuardianPetList m_guardianPets;

    ObjectGuid m_TotemSlot[MAX_TOTEM_SLOT];

    std::vector<WhiteAttack> m_whiteAttacksQueue;

    std::vector<spell_ref> spell_queue_; // spells that have hit us are in this
                                         // queue, and are batch updated each
                                         // 400 ms
    void update_spell_queue();

    uint32 interrupt_mask_;
    std::vector<AuraHolder*> interruptible_auras_;
    void update_interrupt_mask();
};

template <typename Func>
void Unit::CallForAllControlledUnits(Func const& func, uint32 controlledMask)
{
    if (controlledMask & CONTROLLED_PET)
        if (Pet* pet = GetPet())
            func(pet);

    if (controlledMask & CONTROLLED_MINIPET)
        if (Pet* mini = GetMiniPet())
            func(mini);

    if (controlledMask & CONTROLLED_GUARDIANS)
    {
        for (auto itr = m_guardianPets.begin(); itr != m_guardianPets.end();)
            if (Pet* guardian = _GetPet(*(itr++)))
                func(guardian);
    }

    if (controlledMask & CONTROLLED_TOTEMS)
    {
        for (int i = 0; i < MAX_TOTEM_SLOT; ++i)
            if (Unit* totem = _GetTotem(TotemSlot(i)))
                func(totem);
    }

    if (controlledMask & CONTROLLED_CHARM)
        if (Unit* charm = GetCharm())
            func(charm);
}

template <typename Func>
bool Unit::CheckAllControlledUnits(
    Func const& func, uint32 controlledMask) const
{
    if (controlledMask & CONTROLLED_PET)
        if (Pet const* pet = GetPet())
            if (func(pet))
                return true;

    if (controlledMask & CONTROLLED_MINIPET)
        if (Pet const* mini = GetMiniPet())
            if (func(mini))
                return true;

    if (controlledMask & CONTROLLED_GUARDIANS)
    {
        for (auto itr = m_guardianPets.begin(); itr != m_guardianPets.end();)
            if (Pet const* guardian = _GetPet(*(itr++)))
                if (func(guardian))
                    return true;
    }

    if (controlledMask & CONTROLLED_TOTEMS)
    {
        for (int i = 0; i < MAX_TOTEM_SLOT; ++i)
            if (Unit const* totem = _GetTotem(TotemSlot(i)))
                if (func(totem))
                    return true;
    }

    if (controlledMask & CONTROLLED_CHARM)
        if (Unit const* charm = GetCharm())
            if (func(charm))
                return true;

    return false;
}

inline void Unit::remove_auras(AuraRemoveMode mode)
{
    aura_holder_guard guard(this);

    for (auto& elem : m_auraHolders)
    {
        if (elem.second->IsDisabled())
            continue;
        RemoveAuraHolder(elem.second, mode);
    }
}

template <typename F>
void Unit::remove_auras_if(F func, AuraRemoveMode mode)
{
    aura_holder_guard guard(this);

    for (auto& elem : m_auraHolders)
    {
        if (elem.second->IsDisabled())
            continue;
        if (func(elem.second))
            RemoveAuraHolder(elem.second, mode);
    }
}

template <typename F>
void Unit::remove_auras(uint32 id, F func, AuraRemoveMode mode)
{
    aura_holder_guard guard(this);

    for (auto& elem : m_auraHolders)
    {
        auto holder = elem.second;
        if (holder->IsDisabled())
            continue;
        if (likely(holder->GetId() != id))
            continue;
        if (func(holder))
            RemoveAuraHolder(holder, mode);
    }
}

template <typename F>
void Unit::remove_auras(AuraType type, F func, AuraRemoveMode mode)
{
    if (m_modAuras[type].empty())
        return;

    aura_holder_guard guard(this);

    // RemoveAuraHolder will modify m_modAuras[type]; make a copy
    Auras tmp = m_modAuras[type];

    for (auto aura : tmp)
    {
        auto holder = aura->GetHolder();
        if (holder->IsDisabled())
            continue;
        if (func(holder))
            RemoveAuraHolder(holder, mode);
    }
}

template <typename F>
AuraHolder* Unit::get_aura(uint32 spellId, ObjectGuid caster, F func) const
{
    auto bounds = m_auraHolders.equal_range(spellId);
    for (auto itr = bounds.first; itr != bounds.second; ++itr)
    {
        if (itr->second->IsDisabled())
            continue;
        if (caster && itr->second->GetCasterGuid() != caster)
            continue;
        if (!func(itr->second))
            continue;
        return itr->second;
    }
    return nullptr;
}

template <typename F>
AuraHolder* Unit::get_aura(AuraType type, ObjectGuid caster, F func) const
{
    auto& al = m_modAuras[type];
    for (auto& elem : al)
    {
        if ((elem)->GetHolder()->IsDisabled())
            continue;
        if (caster && (elem)->GetCasterGuid() != caster)
            continue;
        if (!func((elem)->GetHolder()))
            continue;
        return (elem)->GetHolder();
    }
    return nullptr;
}

template <typename F>
void Unit::loop_auras(F func, uint32 id) const
{
    if (id)
    {
        auto bounds = m_auraHolders.equal_range(id);
        for (auto itr = bounds.first; itr != bounds.second; ++itr)
        {
            if (!itr->second->IsDisabled())
            {
                bool cont = func(itr->second);
                if (!cont)
                    break;
            }
        }
    }
    else
    {
        for (const auto& elem : m_auraHolders)
        {
            if (!elem.second->IsDisabled())
            {
                bool cont = func(elem.second);
                if (!cont)
                    break;
            }
        }
    }
}

#endif
