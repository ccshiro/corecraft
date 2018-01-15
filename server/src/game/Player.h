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

#ifndef _PLAYER_H
#define _PLAYER_H

#include "BattleGround.h"
#include "Common.h"
#include "DBCStores.h"
#include "Group.h"
#include "Item.h"
#include "ItemPrototype.h"
#include "MapReference.h"
#include "NPCHandler.h"
#include "Pet.h"
#include "QuestDef.h"
#include "ReputationMgr.h"
#include "SharedDefines.h"
#include "Unit.h"
#include "Util.h"
#include "WorldSession.h"
#include "movement_validator.h"
#include "inventory/personal_storage.h"
#include <string>
#include <unordered_map>
#include <vector>

class Channel;
class Creature;
class DungeonPersistentState;
class DynamicObject;
class Item;
struct Mail;
class PlayerMenu;
class PlayerSocial;
class Spell;
class SpellCastTargets;
class SqlQueryHolder;
class Transport;
class UpdateMask;
class ticket;

namespace inventory
{
class trade;
}

typedef std::deque<Mail*> PlayerMails;

#define PLAYER_MAX_SKILLS 127
#define PLAYER_MAX_DAILY_QUESTS 25
#define PLAYER_EXPLORED_ZONES_SIZE 128

struct RecentDungeon
{
    uint32 map;
    uint32 instance;
    uint32 timestamp;
};

// Note: SPELLMOD_* values is aura types in fact
enum SpellModType
{
    SPELLMOD_FLAT = 107, // SPELL_AURA_ADD_FLAT_MODIFIER
    SPELLMOD_PCT = 108   // SPELL_AURA_ADD_PCT_MODIFIER
};

// 2^n internal values, they are never sent to the client
enum PlayerUnderwaterState
{
    UNDERWATER_NONE = 0x00,
    UNDERWATER_INWATER =
        0x01, // terrain type is water and player is afflicted by it
    UNDERWATER_INLAVA =
        0x02, // terrain type is lava and player is afflicted by it
    UNDERWATER_INSLIME =
        0x04, // terrain type is lava and player is afflicted by it
    UNDERWATER_INDARKWATER =
        0x08, // terrain type is dark water and player is afflicted by it

    UNDERWATER_EXIST_TIMERS = 0x10
};

enum BuyBankSlotResult
{
    ERR_BANKSLOT_FAILED_TOO_MANY = 0,
    ERR_BANKSLOT_INSUFFICIENT_FUNDS = 1,
    ERR_BANKSLOT_NOTBANKER = 2,
    ERR_BANKSLOT_OK = 3
};

enum PlayerSpellState
{
    PLAYERSPELL_UNCHANGED = 0,
    PLAYERSPELL_CHANGED = 1,
    PLAYERSPELL_NEW = 2,
    PLAYERSPELL_REMOVED = 3

};

struct PlayerSpell
{
    PlayerSpellState state : 8;
    bool active : 1;    // show in spellbook
    bool dependent : 1; // learned as result another spell learn, skill grow,
                        // quest reward, etc
    bool disabled : 1; // first rank has been learned in result talent learn but
                       // currently talent unlearned, save max learned ranks
};

typedef std::unordered_map<uint32, PlayerSpell> PlayerSpellMap;

// Spell modifier (used for modify other spells)
struct SpellModifier
{
    SpellModifier() : charges(0), lastAffected(nullptr) {}

    SpellModifier(SpellModOp _op, SpellModType _type, int32 _value,
        uint32 _spellId, uint64 _mask, int16 _charges = 0)
      : op(_op), type(_type), charges(_charges), value(_value), mask(_mask),
        spellId(_spellId), lastAffected(nullptr)
    {
        timestamp = WorldTimer::getMSTime();
    }

    SpellModifier(SpellModOp _op, SpellModType _type, int32 _value,
        uint32 _spellId, ClassFamilyMask _mask, int16 _charges = 0)
      : op(_op), type(_type), charges(_charges), value(_value),
        mask(std::move(_mask)), spellId(_spellId), lastAffected(nullptr)
    {
        timestamp = WorldTimer::getMSTime();
    }

    SpellModifier(SpellModOp _op, SpellModType _type, int32 _value,
        SpellEntry const* spellEntry, SpellEffectIndex eff, int16 _charges = 0);

    SpellModifier(SpellModOp _op, SpellModType _type, int32 _value,
        Aura const* aura, int16 _charges = 0);

    bool isAffectedOnSpell(SpellEntry const* spell) const;

    SpellModOp op : 8;
    SpellModType type : 8;
    int16 charges : 16;
    int32 value;
    ClassFamilyMask mask;
    uint32 spellId;
    uint32 timestamp;
    Spell const* lastAffected; // mark last charge user, used for cleanup
                               // delayed remove spellmods at spell success or
                               // restore charges at cast fail (Is one pointer
                               // only need for cases mixed castes?)
};

typedef std::list<SpellModifier*> SpellModList;

struct SpellCooldown
{
    time_t end;
    uint16 itemid;
};

typedef std::map<uint32, SpellCooldown> SpellCooldowns;

enum TrainerSpellState
{
    TRAINER_SPELL_GREEN = 0,
    TRAINER_SPELL_RED = 1,
    TRAINER_SPELL_GRAY = 2,
    TRAINER_SPELL_GREEN_DISABLED = 10 // custom value, not send to client:
                                      // formally green but learn not allowed
};

enum ActionButtonUpdateState
{
    ACTIONBUTTON_UNCHANGED = 0,
    ACTIONBUTTON_CHANGED = 1,
    ACTIONBUTTON_NEW = 2,
    ACTIONBUTTON_DELETED = 3
};

enum ActionButtonType
{
    ACTION_BUTTON_SPELL = 0x00,
    ACTION_BUTTON_C = 0x01, // click?
    ACTION_BUTTON_MACRO = 0x40,
    ACTION_BUTTON_CMACRO = ACTION_BUTTON_C | ACTION_BUTTON_MACRO,
    ACTION_BUTTON_ITEM = 0x80
};

#define ACTION_BUTTON_ACTION(X) (uint32(X) & 0x00FFFFFF)
#define ACTION_BUTTON_TYPE(X) ((uint32(X) & 0xFF000000) >> 24)
#define MAX_ACTION_BUTTON_ACTION_VALUE (0x00FFFFFF + 1)

struct ActionButton
{
    ActionButton() : packedData(0), uState(ACTIONBUTTON_NEW) {}

    uint32 packedData;
    ActionButtonUpdateState uState;

    // helpers
    ActionButtonType GetType() const
    {
        return ActionButtonType(ACTION_BUTTON_TYPE(packedData));
    }
    uint32 GetAction() const { return ACTION_BUTTON_ACTION(packedData); }
    void SetActionAndType(uint32 action, ActionButtonType type)
    {
        uint32 newData = action | (uint32(type) << 24);
        if (newData != packedData || uState == ACTIONBUTTON_DELETED)
        {
            packedData = newData;
            if (uState != ACTIONBUTTON_NEW)
                uState = ACTIONBUTTON_CHANGED;
        }
    }
};

#define MAX_ACTION_BUTTONS 132 // checked in 2.3.0

typedef std::map<uint8, ActionButton> ActionButtonList;

struct PlayerCreateInfoItem
{
    PlayerCreateInfoItem(uint32 id, uint32 amount)
      : item_id(id), item_amount(amount)
    {
    }

    uint32 item_id;
    uint32 item_amount;
};

typedef std::list<PlayerCreateInfoItem> PlayerCreateInfoItems;

struct PlayerClassLevelInfo
{
    PlayerClassLevelInfo() : basehealth(0), basemana(0) {}
    uint16 basehealth;
    uint16 basemana;
};

struct PlayerClassInfo
{
    PlayerClassInfo() : levelInfo(nullptr) {}

    PlayerClassLevelInfo* levelInfo; //[level-1] 0..MaxPlayerLevel-1
};

struct PlayerLevelInfo
{
    PlayerLevelInfo()
    {
        for (auto& elem : stats)
            elem = 0;
    }

    uint8 stats[MAX_STATS];
};

typedef std::list<uint32> PlayerCreateInfoSpells;

struct PlayerCreateInfoAction
{
    PlayerCreateInfoAction() : button(0), type(0), action(0) {}
    PlayerCreateInfoAction(uint8 _button, uint32 _action, uint8 _type)
      : button(_button), type(_type), action(_action)
    {
    }

    uint8 button;
    uint8 type;
    uint32 action;
};

typedef std::list<PlayerCreateInfoAction> PlayerCreateInfoActions;

struct PlayerInfo
{
    // existence checked by displayId != 0             // existence checked by
    // displayId != 0
    PlayerInfo() : displayId_m(0), displayId_f(0), levelInfo(nullptr) {}

    uint32 mapId;
    uint32 areaId;
    float positionX;
    float positionY;
    float positionZ;
    float orientation;
    uint16 displayId_m;
    uint16 displayId_f;
    PlayerCreateInfoItems item;
    PlayerCreateInfoSpells spell;
    PlayerCreateInfoActions action;

    PlayerLevelInfo* levelInfo; //[level-1] 0..MaxPlayerLevel-1
};

struct PvPInfo
{
    PvPInfo() : inHostileArea(false), endTimer(0) {}

    bool inHostileArea;
    time_t endTimer;
};

struct DuelInfo
{
    DuelInfo()
      : initiator(nullptr), opponent(nullptr), startTimer(0), startTime(0),
        outOfBound(0)
    {
    }

    Player* initiator;
    Player* opponent;
    time_t startTimer;
    time_t startTime;
    time_t outOfBound;
};

struct Areas
{
    uint32 areaID;
    uint32 areaFlag;
    float x1;
    float x2;
    float y1;
    float y2;
};

struct LookingForGroupSlot
{
    LookingForGroupSlot() : entry(0), type(0) {}
    bool Empty() const { return !entry && !type; }
    void Clear()
    {
        entry = 0;
        type = 0;
    }
    void Set(uint32 _entry, uint32 _type)
    {
        entry = _entry;
        type = _type;
    }
    bool Is(uint32 _entry, uint32 _type) const
    {
        return entry == _entry && type == _type;
    }
    bool canAutoJoin() const
    {
        return entry &&
               (type == LFG_TYPE_DUNGEON || type == LFG_TYPE_HEROIC_DUNGEON);
    }

    uint32 entry;
    uint32 type;
};

#define MAX_LOOKING_FOR_GROUP_SLOT 3

struct LookingForGroup
{
    LookingForGroup() {}
    bool HaveInSlot(LookingForGroupSlot const& slot) const
    {
        return HaveInSlot(slot.entry, slot.type);
    }
    bool HaveInSlot(uint32 _entry, uint32 _type) const
    {
        for (auto& elem : slots)
            if (elem.Is(_entry, _type))
                return true;
        return false;
    }

    bool canAutoJoin() const
    {
        for (auto& elem : slots)
            if (elem.canAutoJoin())
                return true;
        return false;
    }

    bool Empty() const
    {
        for (auto& elem : slots)
            if (!elem.Empty())
                return false;
        return more.Empty();
    }

    LookingForGroupSlot slots[MAX_LOOKING_FOR_GROUP_SLOT];
    LookingForGroupSlot more;
    std::string comment;
};

enum RaidGroupError
{
    ERR_RAID_GROUP_NONE = 0,
    ERR_RAID_GROUP_RAIDGRP =
        1, // "You must be in a raid group to enter this instance."
    ERR_RAID_GROUP_FULL = 2, // "The instance is full."
};

enum PlayerMovementType
{
    MOVE_ROOT = 1,
    MOVE_UNROOT = 2,
    MOVE_WATER_WALK = 3,
    MOVE_LAND_WALK = 4
};

enum DrunkenState
{
    DRUNKEN_SOBER = 0,
    DRUNKEN_TIPSY = 1,
    DRUNKEN_DRUNK = 2,
    DRUNKEN_SMASHED = 3
};

#define MAX_DRUNKEN 4

enum PlayerFlags
{
    PLAYER_FLAGS_NONE = 0x00000000,
    PLAYER_FLAGS_GROUP_LEADER = 0x00000001,
    PLAYER_FLAGS_AFK = 0x00000002,
    PLAYER_FLAGS_DND = 0x00000004,
    PLAYER_FLAGS_GM = 0x00000008,
    PLAYER_FLAGS_GHOST = 0x00000010,
    PLAYER_FLAGS_RESTING = 0x00000020,
    PLAYER_FLAGS_UNK7 = 0x00000040, // admin?
    PLAYER_FLAGS_FFA_PVP = 0x00000080,
    PLAYER_FLAGS_CONTESTED_PVP = 0x00000100, // Player has been involved in a
                                             // PvP combat and will be attacked
                                             // by contested guards
    PLAYER_FLAGS_IN_PVP = 0x00000200,
    PLAYER_FLAGS_HIDE_HELM = 0x00000400,
    PLAYER_FLAGS_HIDE_CLOAK = 0x00000800,
    PLAYER_FLAGS_PARTIAL_PLAY_TIME = 0x00001000, // played long time
    PLAYER_FLAGS_NO_PLAY_TIME = 0x00002000,      // played too long time
    PLAYER_FLAGS_UNK15 = 0x00004000,
    PLAYER_FLAGS_UNK16 = 0x00008000,     // strange visual effect (2.0.1), looks
                                         // like PLAYER_FLAGS_GHOST flag
    PLAYER_FLAGS_SANCTUARY = 0x00010000, // player entered sanctuary
    PLAYER_FLAGS_TAXI_BENCHMARK =
        0x00020000, // taxi benchmark mode (on/off) (2.0.1)
    PLAYER_FLAGS_PVP_TIMER =
        0x00040000, // 3.0.2, pvp timer active (after you disable pvp manually)
};

// used for PLAYER__FIELD_KNOWN_TITLES field (uint64), (1<<bit_index) without
// (-1)
// can't use enum for uint64 values
#define PLAYER_TITLE_DISABLED UI64LIT(0x0000000000000000)
#define PLAYER_TITLE_NONE UI64LIT(0x0000000000000001)
#define PLAYER_TITLE_PRIVATE UI64LIT(0x0000000000000002)               // 1
#define PLAYER_TITLE_CORPORAL UI64LIT(0x0000000000000004)              // 2
#define PLAYER_TITLE_SERGEANT_A UI64LIT(0x0000000000000008)            // 3
#define PLAYER_TITLE_MASTER_SERGEANT UI64LIT(0x0000000000000010)       // 4
#define PLAYER_TITLE_SERGEANT_MAJOR UI64LIT(0x0000000000000020)        // 5
#define PLAYER_TITLE_KNIGHT UI64LIT(0x0000000000000040)                // 6
#define PLAYER_TITLE_KNIGHT_LIEUTENANT UI64LIT(0x0000000000000080)     // 7
#define PLAYER_TITLE_KNIGHT_CAPTAIN UI64LIT(0x0000000000000100)        // 8
#define PLAYER_TITLE_KNIGHT_CHAMPION UI64LIT(0x0000000000000200)       // 9
#define PLAYER_TITLE_LIEUTENANT_COMMANDER UI64LIT(0x0000000000000400)  // 10
#define PLAYER_TITLE_COMMANDER UI64LIT(0x0000000000000800)             // 11
#define PLAYER_TITLE_MARSHAL UI64LIT(0x0000000000001000)               // 12
#define PLAYER_TITLE_FIELD_MARSHAL UI64LIT(0x0000000000002000)         // 13
#define PLAYER_TITLE_GRAND_MARSHAL UI64LIT(0x0000000000004000)         // 14
#define PLAYER_TITLE_SCOUT UI64LIT(0x0000000000008000)                 // 15
#define PLAYER_TITLE_GRUNT UI64LIT(0x0000000000010000)                 // 16
#define PLAYER_TITLE_SERGEANT_H UI64LIT(0x0000000000020000)            // 17
#define PLAYER_TITLE_SENIOR_SERGEANT UI64LIT(0x0000000000040000)       // 18
#define PLAYER_TITLE_FIRST_SERGEANT UI64LIT(0x0000000000080000)        // 19
#define PLAYER_TITLE_STONE_GUARD UI64LIT(0x0000000000100000)           // 20
#define PLAYER_TITLE_BLOOD_GUARD UI64LIT(0x0000000000200000)           // 21
#define PLAYER_TITLE_LEGIONNAIRE UI64LIT(0x0000000000400000)           // 22
#define PLAYER_TITLE_CENTURION UI64LIT(0x0000000000800000)             // 23
#define PLAYER_TITLE_CHAMPION UI64LIT(0x0000000001000000)              // 24
#define PLAYER_TITLE_LIEUTENANT_GENERAL UI64LIT(0x0000000002000000)    // 25
#define PLAYER_TITLE_GENERAL UI64LIT(0x0000000004000000)               // 26
#define PLAYER_TITLE_WARLORD UI64LIT(0x0000000008000000)               // 27
#define PLAYER_TITLE_HIGH_WARLORD UI64LIT(0x0000000010000000)          // 28
#define PLAYER_TITLE_GLADIATOR UI64LIT(0x0000000020000000)             // 29
#define PLAYER_TITLE_DUELIST UI64LIT(0x0000000040000000)               // 30
#define PLAYER_TITLE_RIVAL UI64LIT(0x0000000080000000)                 // 31
#define PLAYER_TITLE_CHALLENGER UI64LIT(0x0000000100000000)            // 32
#define PLAYER_TITLE_SCARAB_LORD UI64LIT(0x0000000200000000)           // 33
#define PLAYER_TITLE_CONQUEROR UI64LIT(0x0000000400000000)             // 34
#define PLAYER_TITLE_JUSTICAR UI64LIT(0x0000000800000000)              // 35
#define PLAYER_TITLE_CHAMPION_OF_THE_NAARU UI64LIT(0x0000001000000000) // 36
#define PLAYER_TITLE_MERCILESS_GLADIATOR UI64LIT(0x0000002000000000)   // 37
#define PLAYER_TITLE_OF_THE_SHATTERED_SUN UI64LIT(0x0000004000000000)  // 38
#define PLAYER_TITLE_HAND_OF_ADAL UI64LIT(0x0000008000000000)          // 39
#define PLAYER_TITLE_VENGEFUL_GLADIATOR UI64LIT(0x0000010000000000)    // 40

#define MAX_TITLE_INDEX 64 // 1 uint64 field

// used in (PLAYER_FIELD_BYTES, 0) byte values
enum PlayerFieldByteFlags
{
    PLAYER_FIELD_BYTE_TRACK_STEALTHED = 0x02,
    PLAYER_FIELD_BYTE_RELEASE_TIMER =
        0x08, // Display time till auto release spirit
    PLAYER_FIELD_BYTE_NO_RELEASE_WINDOW =
        0x10 // Display no "release spirit" window at all
};

// used in byte (PLAYER_FIELD_BYTES2,1) values
enum PlayerFieldByte2Flags
{
    PLAYER_FIELD_BYTE2_NONE = 0x00,
    PLAYER_FIELD_BYTE2_DETECT_AMORE_0 =
        0x02, // SPELL_AURA_DETECT_AMORE, not used as value and maybe not
              // relcted to, but used in code as base for mask apply
    PLAYER_FIELD_BYTE2_DETECT_AMORE_1 = 0x04, // SPELL_AURA_DETECT_AMORE value 1
    PLAYER_FIELD_BYTE2_DETECT_AMORE_2 = 0x08, // SPELL_AURA_DETECT_AMORE value 2
    PLAYER_FIELD_BYTE2_DETECT_AMORE_3 = 0x10, // SPELL_AURA_DETECT_AMORE value 3
    PLAYER_FIELD_BYTE2_STEALTH = 0x20,
    PLAYER_FIELD_BYTE2_INVISIBILITY_GLOW = 0x40
};

enum ActivateTaxiReplies
{
    ERR_TAXIOK = 0,
    ERR_TAXIUNSPECIFIEDSERVERERROR = 1,
    ERR_TAXINOSUCHPATH = 2,
    ERR_TAXINOTENOUGHMONEY = 3,
    ERR_TAXITOOFARAWAY = 4,
    ERR_TAXINOVENDORNEARBY = 5,
    ERR_TAXINOTVISITED = 6,
    ERR_TAXIPLAYERBUSY = 7,
    ERR_TAXIPLAYERALREADYMOUNTED = 8,
    ERR_TAXIPLAYERSHAPESHIFTED = 9,
    ERR_TAXIPLAYERMOVING = 10,
    ERR_TAXISAMENODE = 11,
    ERR_TAXINOTSTANDING = 12
};

enum MirrorTimerType
{
    FATIGUE_TIMER = 0,
    BREATH_TIMER = 1,
    FIRE_TIMER = 2
};
#define MAX_TIMERS 3
#define DISABLED_MIRROR_TIMER -1

// 2^n values
enum PlayerExtraFlags
{
    // gm abilities
    PLAYER_EXTRA_GM_ON = 0x0001,
    PLAYER_EXTRA_GM_UNUSED1 = 0x0002, // REUSE THIS ONE
    PLAYER_EXTRA_GM_UNUSED2 = 0x0004, // REUSE THIS ONE
    PLAYER_EXTRA_TAXICHEAT = 0x0008,
    PLAYER_EXTRA_GM_INVISIBLE = 0x0010,
    PLAYER_EXTRA_GM_CHAT = 0x0020, // Show GM badge in chat messages
    PLAYER_EXTRA_AUCTION_NEUTRAL = 0x0040,
    PLAYER_EXTRA_AUCTION_ENEMY =
        0x0080, // overwrite PLAYER_EXTRA_AUCTION_NEUTRAL

    // other states
    PLAYER_EXTRA_PVP_DEATH =
        0x0100 // store PvP death status until corpse creating.
};

// 2^n values
enum AtLoginFlags
{
    AT_LOGIN_NONE = 0x00,
    AT_LOGIN_RENAME = 0x01,
    AT_LOGIN_RESET_SPELLS = 0x02,
    AT_LOGIN_RESET_TALENTS = 0x04,
    // AT_LOGIN_CUSTOMIZE         = 0x08, -- used in post-3.x
    // AT_LOGIN_RESET_PET_TALENTS = 0x10, -- used in post-3.x
    AT_LOGIN_FIRST = 0x20,
};

typedef std::map<uint32, QuestStatusData> QuestStatusMap;

enum QuestSlotOffsets
{
    QUEST_ID_OFFSET = 0,
    QUEST_STATE_OFFSET = 1,
    QUEST_COUNTS_OFFSET = 2,
    QUEST_TIME_OFFSET = 3
};

#define MAX_QUEST_OFFSET 4

enum QuestSlotStateMask
{
    QUEST_STATE_NONE = 0x0000,
    QUEST_STATE_COMPLETE = 0x0001,
    QUEST_STATE_FAIL = 0x0002
};

enum SkillUpdateState
{
    SKILL_UNCHANGED = 0,
    SKILL_CHANGED = 1,
    SKILL_NEW = 2,
    SKILL_DELETED = 3
};

struct SkillStatusData
{
    SkillStatusData(uint8 _pos, SkillUpdateState _uState)
      : pos(_pos), uState(_uState)
    {
    }
    uint8 pos;
    SkillUpdateState uState;
};

typedef std::unordered_map<uint32, SkillStatusData> SkillStatusMap;

enum TransferAbortReason
{
    TRANSFER_ABORT_NONE = 0x00,
    TRANSFER_ABORT_MAX_PLAYERS = 0x01, // Transfer Aborted: instance is full
    TRANSFER_ABORT_NOT_FOUND = 0x02,   // Transfer Aborted: instance not found
    TRANSFER_ABORT_TOO_MANY_INSTANCES =
        0x03, // You have entered too many instances recently.
    TRANSFER_ABORT_ZONE_IN_COMBAT =
        0x05, // Unable to zone in while an encounter is in progress.
    TRANSFER_ABORT_INSUF_EXPAN_LVL =
        0x06, // You must have TBC expansion installed to access this area.
    TRANSFER_ABORT_DIFFICULTY =
        0x07, // <Normal,Heroic,Epic> difficulty mode is not available for %s.
};

enum InstanceResetWarningType
{
    RAID_INSTANCE_WARNING_HOURS =
        1, // WARNING! %s is scheduled to reset in %d hour(s).
    RAID_INSTANCE_WARNING_MIN =
        2, // WARNING! %s is scheduled to reset in %d minute(s)!
    RAID_INSTANCE_WARNING_MIN_SOON =
        3, // WARNING! %s is scheduled to reset in %d minute(s). Please exit the
           // zone or you will be returned to your bind location!
    RAID_INSTANCE_WELCOME =
        4 // Welcome to %s. This raid instance is scheduled to reset in %s.
};

// PLAYER_FIELD_ARENA_TEAM_INFO_1_1 offsets
enum ArenaTeamInfoType
{
    ARENA_TEAM_ID = 0,
    ARENA_TEAM_MEMBER = 1, // 0 - captain, 1 - member
    ARENA_TEAM_GAMES_WEEK = 2,
    ARENA_TEAM_GAMES_SEASON = 3,
    ARENA_TEAM_WINS_SEASON = 4,
    ARENA_TEAM_PERSONAL_RATING = 5,
    ARENA_TEAM_END = 6
};

enum RestType
{
    REST_TYPE_NO = 0,
    REST_TYPE_IN_TAVERN = 1,
    REST_TYPE_IN_CITY = 2
};

enum DuelCompleteType
{
    DUEL_INTERUPTED = 0,
    DUEL_WON = 1,
    DUEL_FLED = 2
};

enum TeleportToOptions
{
    TELE_TO_GM_MODE = 0x01,
    TELE_TO_NOT_LEAVE_TRANSPORT = 0x02,
    TELE_TO_NOT_LEAVE_COMBAT = 0x04,
    TELE_TO_NOT_UNSUMMON_PET = 0x08,
    TELE_TO_SPELL = 0x10,
    TELE_TO_RESURRECT = 0x20,
};

/// Type of environmental damages
enum EnvironmentalDamages
{
    DAMAGE_EXHAUSTED = 0,
    DAMAGE_DROWNING = 1,
    DAMAGE_FALL = 2,
    DAMAGE_LAVA = 3,
    DAMAGE_SLIME = 4,
    DAMAGE_FIRE = 5,
    DAMAGE_FALL_TO_VOID = 6 // custom case for fall without durability loss
};

enum PlayedTimeIndex
{
    PLAYED_TIME_TOTAL = 0,
    PLAYED_TIME_LEVEL = 1
};

#define MAX_PLAYED_TIME_INDEX 2

// used at player loading query list preparing, and later result selection
enum PlayerLoginQueryIndex
{
    PLAYER_LOGIN_QUERY_LOADFROM,
    PLAYER_LOGIN_QUERY_LOADGROUP,
    PLAYER_LOGIN_QUERY_LOADINSTANCEBINDS,
    PLAYER_LOGIN_QUERY_LOADAURAS,
    PLAYER_LOGIN_QUERY_LOADSPELLS,
    PLAYER_LOGIN_QUERY_LOADQUESTSTATUS,
    PLAYER_LOGIN_QUERY_LOADDAILYQUESTSTATUS,
    PLAYER_LOGIN_QUERY_LOADREPUTATION,
    PLAYER_LOGIN_QUERY_LOADINVENTORY,
    PLAYER_LOGIN_QUERY_LOADITEMLOOT,
    PLAYER_LOGIN_QUERY_LOADACTIONS,
    PLAYER_LOGIN_QUERY_LOADSOCIALLIST,
    PLAYER_LOGIN_QUERY_LOADHOMEBIND,
    PLAYER_LOGIN_QUERY_LOADSPELLCOOLDOWNS,
    PLAYER_LOGIN_QUERY_LOADCATEGORYCOOLDOWN,
    PLAYER_LOGIN_QUERY_LOADDECLINEDNAMES,
    PLAYER_LOGIN_QUERY_LOADGUILD,
    PLAYER_LOGIN_QUERY_LOADARENAINFO,
    PLAYER_LOGIN_QUERY_LOADBGDATA,
    PLAYER_LOGIN_QUERY_LOADSKILLS,
    PLAYER_LOGIN_QUERY_LOADMAILS,
    PLAYER_LOGIN_QUERY_LOADMAILEDITEMS,
    PLAYER_LOGIN_QUERY_LOADRECENTDUNGEONS,
    PLAYER_LOGIN_QUERY_PETS,
    PLAYER_LOGIN_QUERY_PET_AURAS,
    PLAYER_LOGIN_QUERY_PET_SPELLS,
    PLAYER_LOGIN_QUERY_PET_SPELL_COOLDOWNS,
    PLAYER_LOGIN_QUERY_PET_DECLINED_NAME,

    MAX_PLAYER_LOGIN_QUERY
};

enum PlayerDelayedOperations
{
    DELAYED_SAVE_PLAYER = 0x01,
    DELAYED_RESURRECT_PLAYER = 0x02,
    DELAYED_SPELL_CAST_DESERTER = 0x04,
    DELAYED_END
};

enum ReputationSource
{
    REPUTATION_SOURCE_KILL,
    REPUTATION_SOURCE_QUEST,
    REPUTATION_SOURCE_SPELL
};

// Player summoning auto-decline time (in secs)
#define MAX_PLAYER_SUMMON_DELAY (2 * MINUTE)
#define MAX_MONEY_AMOUNT (0x7FFFFFFF - 1)

struct InstancePlayerBind
{
    std::weak_ptr<DungeonPersistentState> state;
    bool perm = false;
};

class MANGOS_DLL_SPEC PlayerTaxi
{
public:
    PlayerTaxi();
    ~PlayerTaxi() {}
    // Nodes
    void InitTaxiNodesForLevel(uint32 race, uint32 level);
    void LoadTaxiMask(const char* data);

    bool IsTaximaskNodeKnown(uint32 nodeidx) const
    {
        uint8 field = uint8((nodeidx - 1) / 32);
        uint32 submask = 1 << ((nodeidx - 1) % 32);
        return (m_taximask[field] & submask) == submask;
    }
    bool SetTaximaskNode(uint32 nodeidx)
    {
        uint8 field = uint8((nodeidx - 1) / 32);
        uint32 submask = 1 << ((nodeidx - 1) % 32);
        if ((m_taximask[field] & submask) != submask)
        {
            m_taximask[field] |= submask;
            return true;
        }
        else
            return false;
    }
    void AppendTaximaskTo(ByteBuffer& data, bool all);
    void SetExpress(bool decider) { m_Express = decider; }
    bool IsExpress() { return m_Express; }
    void SetOriginalMountDisplayId(uint32 id) { m_DisplayId = id; }
    uint32 GetOriginalMountDisplayId() { return m_DisplayId; }

    // Destinations
    bool LoadTaxiDestinationsFromString(const std::string& values, Team team);
    std::string SaveTaxiDestinationsToString();

    void ClearTaxiDestinations()
    {
        m_TaxiDestinations.clear();
        m_Express = false;
    }
    void AddTaxiDestination(uint32 dest) { m_TaxiDestinations.push_back(dest); }
    uint32 GetTaxiSource() const
    {
        return m_TaxiDestinations.empty() ? 0 : m_TaxiDestinations.front();
    }
    uint32 GetTaxiDestination() const
    {
        return m_TaxiDestinations.size() < 2 ? 0 : m_TaxiDestinations[1];
    }
    uint32 GetCurrentTaxiPath() const;
    uint32 NextTaxiDestination()
    {
        m_TaxiDestinations.pop_front();
        return GetTaxiDestination();
    }
    bool empty() const { return m_TaxiDestinations.empty(); }

    friend std::ostringstream& operator<<(
        std::ostringstream& ss, PlayerTaxi const& taxi);

private:
    TaxiMask m_taximask;
    std::deque<uint32> m_TaxiDestinations;
    bool m_Express;
    uint32 m_DisplayId;
};

std::ostringstream& operator<<(std::ostringstream& ss, PlayerTaxi const& taxi);

/// Holder for BattleGround data
struct BGData
{
    BGData()
      : bgInstanceID(0), bgTypeID(BATTLEGROUND_TYPE_NONE),
        bgAfkReportedCount(0), bgAfkReportedTimer(0), bgTeam(TEAM_NONE)
    {
    }

    uint32 bgInstanceID; ///< This variable is set to bg->m_InstanceID, saved
    ///  when player is teleported to BG - (it is battleground's GUID)
    BattleGroundTypeId bgTypeID;

    std::set<uint32> bgAfkReporter;
    uint8 bgAfkReportedCount;
    time_t bgAfkReportedTimer;

    Team bgTeam; ///< What side the player will be added to, saved

    WorldLocation joinPos; ///< From where player entered BG, saved
};

class MANGOS_DLL_SPEC Player : public Unit
{
    friend class WorldSession;

public:
    explicit Player(std::shared_ptr<WorldSession> session);
    ~Player();

    void CleanupsBeforeDelete() override;

    static UpdateMask updateVisualBits;
    static void InitVisibleBits();

    bool in_global_transit; // True while changing map
    void AddToWorld() override;
    void RemoveFromWorld() override;

    bool TeleportTo(uint32 mapid, float x, float y, float z, float orientation,
        uint32 options = 0);

    bool TeleportTo(WorldLocation const& loc, uint32 options = 0)
    {
        return TeleportTo(loc.mapid, loc.coord_x, loc.coord_y, loc.coord_z,
            loc.orientation, options);
    }

    bool TeleportToBGEntryPoint();

    void SetSummonPoint(uint32 mapid, float x, float y, float z)
    {
        m_summon_expire =
            WorldTimer::time_no_syscall() + MAX_PLAYER_SUMMON_DELAY;
        m_summon_mapid = mapid;
        m_summon_x = x;
        m_summon_y = y;
        m_summon_z = z;
    }
    void SummonIfPossible(bool agree);

    bool Create(uint32 guidlow, const std::string& name, uint8 race,
        uint8 class_, uint8 gender, uint8 skin, uint8 face, uint8 hairStyle,
        uint8 hairColor, uint8 facialHair, uint8 outfitId);

    void Update(uint32 update_diff, uint32 time);

    static bool BuildEnumData(QueryResult* result, WorldPacket* p_data);

    void SetInWater(bool apply);

    bool IsInWater() const override { return m_isInWater; }
    bool IsUnderWater() const override;

    void SendInitialPacketsBeforeAddToMap();
    void SendInitialPacketsAfterAddToMap();
    void SendTransferAborted(uint32 mapid, uint8 reason, uint8 arg = 0);
    void SendRaidGroupError(RaidGroupError err);
    void SendInstanceResetWarning(uint32 mapid, uint32 time);
    // When character logs in we apply a root hack so he cannot move right away
    void ApplyRootHack();

    Creature* GetNPCIfCanInteractWith(ObjectGuid guid, uint32 npcflagmask);
    GameObject* GetGameObjectIfCanInteractWith(
        ObjectGuid guid, uint32 gameobject_type = MAX_GAMEOBJECT_TYPE) const;

    void ToggleAFK();
    void ToggleDND();
    bool isAFK() const { return HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_AFK); }
    bool isDND() const { return HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_DND); }
    uint8 chatTag() const;
    std::string autoReplyMsg;

    PlayerSocial* GetSocial() { return m_social; }

    PlayerTaxi m_taxi;
    void InitTaxiNodesForLevel()
    {
        m_taxi.InitTaxiNodesForLevel(getRace(), getLevel());
    }
    bool ActivateTaxiPathTo(std::vector<uint32> const& nodes,
        Creature* npc = nullptr, uint32 spellid = 0, bool isExpress = false);
    bool ActivateTaxiPathTo(uint32 taxi_path_id, uint32 spellid = 0);
    // mount_id can be used in scripting calls
    void ContinueTaxiFlight();
    bool isGameMaster() const { return m_ExtraFlags & PLAYER_EXTRA_GM_ON; }
    void SetGameMaster(bool on);
    bool isGMChat() const
    {
        return GetSession()->GetSecurity() >= SEC_TICKET_GM &&
               (m_ExtraFlags & PLAYER_EXTRA_GM_CHAT);
    }
    void SetGMChat(bool on)
    {
        if (on)
            m_ExtraFlags |= PLAYER_EXTRA_GM_CHAT;
        else
            m_ExtraFlags &= ~PLAYER_EXTRA_GM_CHAT;
    }
    bool isTaxiCheater() const { return m_ExtraFlags & PLAYER_EXTRA_TAXICHEAT; }
    void SetTaxiCheater(bool on)
    {
        if (on)
            m_ExtraFlags |= PLAYER_EXTRA_TAXICHEAT;
        else
            m_ExtraFlags &= ~PLAYER_EXTRA_TAXICHEAT;
    }
    bool isGMVisible() const
    {
        return !(m_ExtraFlags & PLAYER_EXTRA_GM_INVISIBLE);
    }
    void SetGMVisible(bool on);
    void SetPvPDeath(bool on)
    {
        if (on)
            m_ExtraFlags |= PLAYER_EXTRA_PVP_DEATH;
        else
            m_ExtraFlags &= ~PLAYER_EXTRA_PVP_DEATH;
    }

    // 0 = own auction, -1 = enemy auction, 1 = goblin auction
    int GetAuctionAccessMode() const
    {
        return m_ExtraFlags & PLAYER_EXTRA_AUCTION_ENEMY ?
                   -1 :
                   (m_ExtraFlags & PLAYER_EXTRA_AUCTION_NEUTRAL ? 1 : 0);
    }
    void SetAuctionAccessMode(int state)
    {
        m_ExtraFlags &=
            ~(PLAYER_EXTRA_AUCTION_ENEMY | PLAYER_EXTRA_AUCTION_NEUTRAL);

        if (state < 0)
            m_ExtraFlags |= PLAYER_EXTRA_AUCTION_ENEMY;
        else if (state > 0)
            m_ExtraFlags |= PLAYER_EXTRA_AUCTION_NEUTRAL;
    }

    void GiveXP(uint32 xp, Unit* victim, float grp_coeff = 1.0f);
    void GiveLevel(uint32 level);

    void InitStatsForLevel(bool reapplyMods = false);

    // Played Time Stuff
    time_t m_logintime;
    time_t m_Last_tick;

    uint32 m_Played_time[MAX_PLAYED_TIME_INDEX];
    uint32 GetTotalPlayedTime() { return m_Played_time[PLAYED_TIME_TOTAL]; }
    uint32 GetLevelPlayedTime() { return m_Played_time[PLAYED_TIME_LEVEL]; }

    void ResetTimeSync();
    void SendTimeSync();

    void SetDeathState(DeathState s) override; // overwrite Unit::SetDeathState

    float GetRestBonus() const { return m_rest_bonus; }
    void SetRestBonus(float rest_bonus_new);

    RestType GetRestType() const { return rest_type; }
    void SetRestType(RestType n_r_type, uint32 areaTriggerId = 0);

    time_t GetTimeInnEnter() const { return time_inn_enter; }
    void UpdateInnerTime(time_t time) { time_inn_enter = time; }

    void RemovePet(PetSaveMode mode);
    void RemoveMiniPet();
    Pet* GetMiniPet() const override;

    void SetBgResummonGuid(ObjectGuid guid) { bg_saved_pet_ = guid; }
    ObjectGuid GetBgResummonGuid() const { return bg_saved_pet_; }

    // use only in Pet::Unsummon/Spell::DoSummon
    void _SetMiniPet(Pet* pet)
    {
        m_miniPetGuid = pet ? pet->GetObjectGuid() : ObjectGuid();
    }

    void Say(const std::string& text, const uint32 language);
    void Yell(const std::string& text, const uint32 language);
    void TextEmote(const std::string& text);
    void Whisper(
        const std::string& text, const uint32 language, Player* receiver);
    void BuildPlayerChat(WorldPacket* data, uint8 msgtype,
        const std::string& text, uint32 language) const;

    // When we join/leave a group we need to resend our hp and max hp to
    // everyone (to adjust from percentage to value, and vice versa)
    void resend_health() override;

    /*********************************************************/
    /***                    STORAGE SYSTEM                 ***/
    /*********************************************************/

    inventory::personal_storage& storage() { return inventory_; }
    const inventory::personal_storage& storage() const { return inventory_; }

    void SetVirtualItemSlot(uint8 i, Item* item);
    void SetSheath(SheathState sheathed) override; // overwrite Unit version
    Item* GetItemByGuid(ObjectGuid guid, bool include_bank = true) const;
    uint32 GetItemDisplayIdInSlot(uint8 bag, uint8 slot) const;
    Item* GetWeaponForAttack(WeaponAttackType attackType) const
    {
        return GetWeaponForAttack(attackType, false, false);
    }
    Item* GetWeaponForAttack(
        WeaponAttackType attackType, bool nonbroken, bool useable) const;
    Item* GetShield(bool useable = false) const;
    uint8 GetBankBagSlotCount() const
    {
        return GetByteValue(PLAYER_BYTES_2, 2);
    }
    void SetBankBagSlotCount(uint8 count)
    {
        SetByteValue(PLAYER_BYTES_2, 2, count);
    }
    bool HasItemCount(uint32 item, uint32 count, bool inBankAlso = false) const;
    bool HasItemFitToSpellReqirements(
        SpellEntry const* spellInfo, Item const* ignoreItem = nullptr);
    bool has_item_equipped(uint32 item_id) const;

    // Checks if this item type is generally usable by this player
    InventoryResult can_use_item(const ItemPrototype* prototype) const;
    // Checks if this player can have this item instance equipped, also checks
    // Player::can_use_item
    InventoryResult can_equip(Item* item) const;
    // Checks if this player is in a state where it can perform
    // the equip operation on this item. Also checks Player::can_equip
    InventoryResult can_perform_equip(Item* item) const;
    InventoryResult can_perform_unequip(Item* item) const;

    bool HasItemTotemCategory(uint32 TotemCategory) const;
    InventoryResult CanUseAmmo(uint32 item) const;

    void SetAmmo(uint32 item);
    void RemoveAmmo();
    float GetAmmoDPS() const { return m_ammoDPS; }
    bool CheckAmmoCompatibility(const ItemPrototype* ammo_proto) const;
    void RemoveItemDependentAurasAndCasts(Item* pItem);

    void TakeExtendedCost(uint32 extendedCostId, uint32 count);

    void SendEquipError(InventoryResult msg, Item* pItem,
        Item* pItem2 = nullptr, uint32 itemid = 0) const;
    void SendBuyError(
        BuyResult msg, Creature* pCreature, uint32 item, uint32 param);
    void SendSellError(
        SellResult msg, Creature* pCreature, ObjectGuid itemGuid, uint32 param);
    void AddWeaponProficiency(uint32 newflag)
    {
        m_WeaponProficiency |= newflag;
    }
    void AddArmorProficiency(uint32 newflag) { m_ArmorProficiency |= newflag; }
    uint32 GetWeaponProficiency() const { return m_WeaponProficiency; }
    uint32 GetArmorProficiency() const { return m_ArmorProficiency; }
    bool BuyItemFromVendor(ObjectGuid vendorGuid, uint32 item, uint8 count,
        inventory::slot dst = inventory::slot());

    float GetReputationPriceDiscount(Creature const* pCreature) const;

    void UpdateItemDurations(time_t diff, bool offline_time = false);
    void UpdateEnchDurations(uint32 update_diff);
    void SendItemDurations();
    void SendEnchDurations();
    void TrackItemDurations(Item* item, bool apply);
    void TrackEnchDurations(Item* item, bool apply);

    void RemoveTempEnchantsOnArenaEntry();
    void ApplyEnchantment(Item* item, EnchantmentSlot slot, bool apply,
        inventory::slot item_slot, bool ignore_meta_gem = false);
    void ApplyEnchantment(Item* item, bool apply, inventory::slot item_slot,
        bool ignore_meta_gem = false);
    // Remove any positive, self-cast, aura that is no longer valid due to our
    // equipment having changed
    void UpdateEquipmentRequiringAuras();
    void RemoveAurasCastedBy(Item* item);

    void LoadCorpse();
    void LoadPet();
    bool HasDeadPet();

    uint32 m_stableSlots;

    void HandleRogueSetupTalent(Unit* pAttacker);

    /*********************************************************/
    /***                    GOSSIP SYSTEM                  ***/
    /*********************************************************/

    void PrepareGossipMenu(WorldObject* pSource, uint32 menuId = 0);
    void SendPreparedGossip(WorldObject* pSource);
    void OnGossipSelect(
        WorldObject* pSource, uint32 gossipListId, uint32 menuId);

    uint32 GetGossipTextId(uint32 menuId, WorldObject* pSource);
    uint32 GetGossipTextId(WorldObject* pSource);
    uint32 GetDefaultGossipMenuForSource(WorldObject* pSource);

    /*********************************************************/
    /***                    QUEST SYSTEM                   ***/
    /*********************************************************/

    // Return player level when QuestLevel is dynamic (-1)
    uint32 GetQuestLevelForPlayer(Quest const* pQuest) const
    {
        return pQuest && (pQuest->GetQuestLevel() > 0) ?
                   (uint32)pQuest->GetQuestLevel() :
                   getLevel();
    }

    void PrepareQuestMenu(ObjectGuid guid);
    void SendPreparedQuest(ObjectGuid guid);
    bool IsActiveQuest(uint32 quest_id) const; // can be taken or taken

    // Quest is taken and not yet rewarded
    // if completed_or_not = 0 (or any other value except 1 or 2) - returns
    // true, if quest is taken and doesn't depend if quest is completed or not
    // if completed_or_not = 1 - returns true, if quest is taken but not
    // completed
    // if completed_or_not = 2 - returns true, if quest is taken and already
    // completed
    bool IsCurrentQuest(uint32 quest_id,
        uint8 completed_or_not = 0) const; // taken and not yet rewarded

    Quest const* GetNextQuest(ObjectGuid guid, Quest const* pQuest);
    // source can be nullptr (e.g. if it's an item, or the quest is shared)
    bool CanSeeStartQuest(Quest const* pQuest, WorldObject* source) const;
    bool CanTakeQuest(Quest const* pQuest, bool msg, WorldObject* source) const;
    bool CanAddQuest(Quest const* pQuest, bool msg) const;
    bool CanCompleteQuest(uint32 quest_id) const;
    bool CanCompleteRepeatableQuest(Quest const* pQuest) const;
    bool CanRewardQuest(Quest const* pQuest, bool msg) const;
    void AddQuest(Quest const* pQuest, Object* questGiver);
    void CompleteQuest(uint32 quest_id);
    void IncompleteQuest(uint32 quest_id);
    bool RewardQuest(Quest const* pQuest, uint32 reward, Object* questGiver,
        bool announce = true);

    void FailQuest(uint32 questId);
    bool SatisfyQuestSkill(Quest const* qInfo, bool msg) const;
    bool SatisfyQuestLevel(Quest const* qInfo, bool msg) const;
    bool SatisfyQuestLog(bool msg) const;
    bool SatisfyQuestPreviousQuest(Quest const* qInfo, bool msg) const;
    bool SatisfyQuestClass(Quest const* qInfo, bool msg) const;
    bool SatisfyQuestRace(Quest const* qInfo, bool msg) const;
    bool SatisfyQuestReputation(Quest const* qInfo, bool msg) const;
    bool SatisfyQuestStatus(Quest const* qInfo, bool msg) const;
    bool SatisfyQuestTimed(Quest const* qInfo, bool msg) const;
    bool SatisfyQuestExclusiveGroup(Quest const* qInfo, bool msg) const;
    bool SatisfyQuestNextChain(Quest const* qInfo, bool msg) const;
    bool SatisfyQuestPrevChain(Quest const* qInfo, bool msg) const;
    bool SatisfyQuestDay(Quest const* qInfo, bool msg) const;
    void GiveQuestSourceItemIfNeed(Quest const* pQuest);
    bool TakeQuestSourceItem(uint32 questId, bool msg);
    bool GetQuestRewardStatus(uint32 questId) const;
    QuestStatus GetQuestStatus(uint32 questId) const;
    void SetQuestStatus(uint32 questId, QuestStatus status);
    void SetDailyQuestStatus(uint32 questId);
    void ResetDailyQuestStatus();
    // Call from the invoker of the group quest to fail or complete for nearby
    // party members as well
    void FailGroupQuest(uint32 questId);
    void CompleteGroupQuest(uint32 questId)
    {
        GroupEventHappens(questId, this);
    } // FIXME: See comment at Player::GroupEventHappens

    uint16 FindQuestSlot(uint32 quest_id) const;
    uint32 GetQuestSlotQuestId(uint16 slot) const
    {
        return GetUInt32Value(
            PLAYER_QUEST_LOG_1_1 + slot * MAX_QUEST_OFFSET + QUEST_ID_OFFSET);
    }
    void SetQuestSlot(uint16 slot, uint32 quest_id, uint32 timer = 0)
    {
        SetUInt32Value(
            PLAYER_QUEST_LOG_1_1 + slot * MAX_QUEST_OFFSET + QUEST_ID_OFFSET,
            quest_id);
        SetUInt32Value(
            PLAYER_QUEST_LOG_1_1 + slot * MAX_QUEST_OFFSET + QUEST_STATE_OFFSET,
            0);
        SetUInt32Value(PLAYER_QUEST_LOG_1_1 + slot * MAX_QUEST_OFFSET +
                           QUEST_COUNTS_OFFSET,
            0);
        SetUInt32Value(
            PLAYER_QUEST_LOG_1_1 + slot * MAX_QUEST_OFFSET + QUEST_TIME_OFFSET,
            timer);
    }
    void SetQuestSlotCounter(uint16 slot, uint8 counter, uint8 count)
    {
        SetByteValue(PLAYER_QUEST_LOG_1_1 + slot * MAX_QUEST_OFFSET +
                         QUEST_COUNTS_OFFSET,
            counter, count);
    }
    void SetQuestSlotState(uint16 slot, uint32 state)
    {
        SetFlag(
            PLAYER_QUEST_LOG_1_1 + slot * MAX_QUEST_OFFSET + QUEST_STATE_OFFSET,
            state);
    }
    void RemoveQuestSlotState(uint16 slot, uint32 state)
    {
        RemoveFlag(
            PLAYER_QUEST_LOG_1_1 + slot * MAX_QUEST_OFFSET + QUEST_STATE_OFFSET,
            state);
    }
    void SetQuestSlotTimer(uint16 slot, uint32 timer)
    {
        SetUInt32Value(
            PLAYER_QUEST_LOG_1_1 + slot * MAX_QUEST_OFFSET + QUEST_TIME_OFFSET,
            timer);
    }
    void SwapQuestSlot(uint16 slot1, uint16 slot2)
    {
        for (int i = 0; i < MAX_QUEST_OFFSET; ++i)
        {
            uint32 temp1 = GetUInt32Value(
                PLAYER_QUEST_LOG_1_1 + MAX_QUEST_OFFSET * slot1 + i);
            uint32 temp2 = GetUInt32Value(
                PLAYER_QUEST_LOG_1_1 + MAX_QUEST_OFFSET * slot2 + i);

            SetUInt32Value(
                PLAYER_QUEST_LOG_1_1 + MAX_QUEST_OFFSET * slot1 + i, temp2);
            SetUInt32Value(
                PLAYER_QUEST_LOG_1_1 + MAX_QUEST_OFFSET * slot2 + i, temp1);
        }
    }
    bool IsQuestObjectiveComplete(uint32 questId, uint32 objectiveIndex) const;
    uint32 GetReqKillOrCastCurrentCount(uint32 quest_id, int32 entry);
    void AreaExploredOrEventHappens(uint32 questId);
    void GroupEventHappens(uint32 questId, WorldObject const* pEventObject);
    void KilledMonster(CreatureInfo const* cInfo, ObjectGuid guid);
    void KilledMonsterCredit(
        uint32 entry, ObjectGuid guid = ObjectGuid(), bool from_script = false);
    void CastedCreatureOrGO(uint32 entry, ObjectGuid guid, uint32 spell_id,
        bool original_caster = true);
    void TalkedToCreature(uint32 entry, ObjectGuid guid);
    void ReputationChanged(FactionEntry const* factionEntry);
    bool HasQuestForItem(uint32 itemid) const;
    bool HasQuestForGO(int32 GOId) const;
    void UpdateForQuestWorldObjects();
    bool CanShareQuest(uint32 quest_id) const;

    void SendQuestCompleteEvent(uint32 quest_id);
    void SendQuestReward(Quest const* pQuest, uint32 XP, Object* questGiver);
    void SendQuestFailed(uint32 quest_id);
    void SendQuestTimerFailed(uint32 quest_id);
    void SendCanTakeQuestResponse(uint32 msg) const;
    void SendQuestConfirmAccept(Quest const* pQuest, Player* pReceiver);
    void SendPushToPartyResponse(Player* pPlayer, uint32 msg);
    void SendQuestUpdateAddCreatureOrGo(Quest const* pQuest, ObjectGuid guid,
        uint32 creatureOrGO_idx, uint32 count);

    ObjectGuid GetDividerGuid() const { return m_dividerGuid; }
    void SetDividerGuid(ObjectGuid guid) { m_dividerGuid = guid; }
    void ClearDividerGuid() { m_dividerGuid.Clear(); }

    uint32 GetInGameTime() { return m_ingametime; }

    void SetInGameTime(uint32 time) { m_ingametime = time; }

    void AddTimedQuest(uint32 quest_id) { m_timedquests.insert(quest_id); }
    void RemoveTimedQuest(uint32 quest_id) { m_timedquests.erase(quest_id); }

    /*********************************************************/
    /***                   LOAD SYSTEM                     ***/
    /*********************************************************/

    bool LoadFromDB(ObjectGuid guid, SqlQueryHolder* holder);

    static uint32 GetZoneIdFromDB(ObjectGuid guid);
    static uint32 GetLevelFromDB(ObjectGuid guid);
    static bool LoadPositionFromDB(ObjectGuid guid, uint32& mapid, float& x,
        float& y, float& z, float& o, bool& in_flight);

    /*********************************************************/
    /***                   SAVE SYSTEM                     ***/
    /*********************************************************/

    void SaveToDB();
    void SaveInventoryAndGoldToDB(); // fast save function for item/money
                                     // cheating preventing
    void SaveGoldToDB();
    static void SetUInt32ValueInArray(Tokens& data, uint16 index, uint32 value);
    static void SetFloatValueInArray(Tokens& data, uint16 index, float value);
    static void SavePositionInDB(ObjectGuid guid, uint32 mapid, float x,
        float y, float z, float o, uint32 zone);

    static void DeleteFromDB(ObjectGuid playerguid, uint32 accountId,
        bool updateRealmChars = true, bool deleteFinally = false);
    static void DeleteOldCharacters();
    static void DeleteOldCharacters(uint32 keepDays);

    bool m_mailsUpdated;

    void SendPetTameFailure(PetTameFailureReason reason);

    void SetBindPoint(ObjectGuid guid);
    void SendTalentWipeConfirm(ObjectGuid guid);
    void RewardRage(
        uint32 damage, uint32 hitResult, uint32 weaponType, bool attacker);
    void SendPetSkillWipeConfirm();
    void CalcRage(uint32 damage, bool attacker);
    void RegenerateAll();
    void Regenerate(Powers power);
    void RegenerateHealth();
    void setRegenTimer(uint32 time) { m_regenTimer = time; }

    QuestStatusMap& getQuestStatusMap() { return mQuestStatus; };

    ObjectGuid const& GetSelectionGuid() const { return m_curSelectionGuid; }
    void SetSelectionGuid(ObjectGuid guid)
    {
        m_curSelectionGuid = guid;
        SetTargetGuid(guid);
    }

    uint8 GetComboPoints() const { return m_comboPoints; }
    ObjectGuid const& GetComboTargetGuid() const { return m_comboTargetGuid; }

    void AddComboPoints(Unit* target, int8 count);
    void ClearComboPoints();
    void SendComboPoints();

    void SendMailResult(uint32 mailId, MailResponseType mailAction,
        MailResponseResult mailError, uint32 equipError = 0,
        uint32 item_guid = 0, uint32 item_count = 0);
    void SendNewMail();
    void UpdateNextMailTimeAndUnreads();
    void AddNewMailDeliverTime(time_t deliver_time);

    void RemoveMail(uint32 id);

    void AddMail(Mail* mail)
    {
        m_mail.push_front(mail);
    } // for call from WorldSession::SendMailTo
    uint32 GetMailSize() { return m_mail.size(); }
    Mail* GetMail(uint32 id);

    PlayerMails::iterator GetMailBegin() { return m_mail.begin(); }
    PlayerMails::iterator GetMailEnd() { return m_mail.end(); }

    /*********************************************************/
    /*** MAILED ITEMS SYSTEM ***/
    /*********************************************************/

    uint8 unReadMails;
    time_t m_nextMailDelivereTime;

    typedef std::unordered_map<uint32, Item*> ItemMap;

    ItemMap mMitems; // template defined in objectmgr.cpp

    Item* GetMItem(uint32 id)
    {
        ItemMap::const_iterator itr = mMitems.find(id);
        return itr != mMitems.end() ? itr->second : nullptr;
    }

    void AddMItem(Item* it)
    {
        assert(it);
        // ASSERT deleted, because items can be added before loading
        mMitems[it->GetGUIDLow()] = it;
    }

    bool RemoveMItem(uint32 id) { return mMitems.erase(id) ? true : false; }

    void PetSpellInitialize();
    void CharmSpellInitialize();
    void PossessSpellInitialize();
    void RemovePetActionBar();

    bool HasSpell(uint32 spell) const override;
    bool HasActiveSpell(uint32 spell) const; // show in spellbook
    TrainerSpellState GetTrainerSpellState(
        TrainerSpell const* trainer_spell, uint32 reqLevel) const;
    bool IsSpellFitByClassAndRace(
        uint32 spell_id, uint32* pReqlevel = nullptr) const;
    bool IsNeedCastPassiveLikeSpellAtLearn(SpellEntry const* spellInfo) const;
    bool IsImmuneToSpellEffect(
        SpellEntry const* spellInfo, SpellEffectIndex index) const override;

    void KnockBack(float angle, float horizontalSpeed, float verticalSpeed);
    void KnockBackFrom(
        Unit* target, float horizontalSpeed, float verticalSpeed);
    bool IsKnockbacked() const { return knockbacked_; }
    void SetKnockbacked(bool state) { knockbacked_ = state; }

    void SendProficiency(ItemClass itemClass, uint32 itemSubclassMask);
    void SendInitialSpells();
    bool addSpell(uint32 spell_id, bool active, bool learning, bool dependent,
        bool disabled);
    void learnSpell(uint32 spell_id, bool dependent);
    void removeSpell(uint32 spell_id, bool disabled = false,
        bool learn_low_rank = true, bool sendUpdate = true);
    void resetSpells();
    void learnDefaultSpells();
    void learnQuestRewardedSpells();
    void learnQuestRewardedSpells(Quest const* quest);
    void learnSpellHighRank(uint32 spellid);

    uint32 GetFreeTalentPoints() const
    {
        return GetUInt32Value(PLAYER_CHARACTER_POINTS1);
    }
    void SetFreeTalentPoints(uint32 points)
    {
        SetUInt32Value(PLAYER_CHARACTER_POINTS1, points);
    }
    void UpdateFreeTalentPoints(bool resetIfNeed = true);
    bool resetTalents(bool no_cost = false);
    uint32 resetTalentsCost() const;
    void InitTalentForLevel();
    void LearnTalent(uint32 talentId, uint32 talentRank);
    uint32 CalculateTalentsPoints() const;

    uint32 GetFreePrimaryProfessionPoints() const
    {
        return GetUInt32Value(PLAYER_CHARACTER_POINTS2);
    }
    void SetFreePrimaryProfessions(uint16 profs)
    {
        SetUInt32Value(PLAYER_CHARACTER_POINTS2, profs);
    }
    void InitPrimaryProfessions();

    PlayerSpellMap const& GetSpellMap() const { return m_spells; }
    PlayerSpellMap& GetSpellMap() { return m_spells; }

    SpellCooldowns const& GetSpellCooldownMap() const
    {
        return m_spellCooldowns;
    }

    void AddSpellMod(SpellModifier** mod, bool apply);
    bool IsAffectedBySpellmod(SpellEntry const* spellInfo, SpellModifier* mod,
        Spell const* spell = nullptr);
    template <class T>
    T ApplySpellMod(uint32 spellId, SpellModOp op, T& basevalue,
        Spell const* spell = nullptr, bool peak = false);
    SpellModifier* GetSpellMod(SpellModOp op, uint32 spellId) const;
    void RemoveSpellMods(Spell const* spell);
    void ResetSpellModsDueToCanceledSpell(Spell const* spell);

    static uint32 const infinityCooldownDelay =
        MONTH; // used for set "infinity cooldowns" for spells and check
    static uint32 const infinityCooldownDelayCheck = MONTH / 2;
    // HasSpellCooldown checks category as well
    bool HasSpellCooldown(uint32 spell_id, uint32 castitem_id = 0) const;
    bool HasSpellCategoryCooldown(uint32 category_id) const;
    time_t GetSpellCooldownDelay(uint32 spell_id) const;
    void AddSpellAndCategoryCooldowns(SpellEntry const* spellInfo,
        uint32 itemId, Spell* spell = nullptr, bool infinityCooldown = false);
    void AddSpellCooldown(uint32 spell_id, uint32 itemid, time_t end_time);
    void SendCooldownEvent(
        SpellEntry const* spellInfo, uint32 itemId = 0, Spell* spell = nullptr);
    void ProhibitSpellSchool(
        SpellSchoolMask idSchoolMask, uint32 unTimeMs) override;
    void RemoveSpellCooldown(uint32 spell_id, bool update = false);
    void RemoveSpellCategoryCooldown(uint32 cat);
    void SendClearCooldown(uint32 spell_id, Unit* target);
    void AddProcEventCooldown(uint32 spell_id, uint32 milliseconds);
    bool HasProcEventCooldown(uint32 spell_id);

    GlobalCooldownMgr& GetGlobalCooldownMgr() { return m_GlobalCooldownMgr; }

    void RemoveArenaSpellCooldowns();
    void RemoveAllSpellCooldown();
    void _LoadSpellCooldowns(
        QueryResult* spell_result, QueryResult* category_result);
    void _SaveSpellCooldowns();

    void setResurrectRequestData(ObjectGuid guid, uint32 mapId, float X,
        float Y, float Z, uint32 health, uint32 mana)
    {
        m_resurrectGuid = guid;
        m_resurrectMap = mapId;
        m_resurrectX = X;
        m_resurrectY = Y;
        m_resurrectZ = Z;
        m_resurrectHealth = health;
        m_resurrectMana = mana;
    }
    void clearResurrectRequestData()
    {
        setResurrectRequestData(ObjectGuid(), 0, 0.0f, 0.0f, 0.0f, 0, 0);
    }
    bool isRessurectRequestedBy(ObjectGuid guid) const
    {
        return m_resurrectGuid == guid;
    }
    bool isRessurectRequested() const { return !m_resurrectGuid.IsEmpty(); }
    void ResurectUsingRequestData();

    uint32 getCinematic() { return m_cinematic; }
    void setCinematic(uint32 cine) { m_cinematic = cine; }

    static bool IsActionButtonDataValid(
        uint8 button, uint32 action, uint8 type, Player* player);
    ActionButton* addActionButton(uint8 button, uint32 action, uint8 type);
    void removeActionButton(uint8 button);
    void SendInitialActionButtons() const;

    PvPInfo pvpInfo;
    void UpdatePvP(bool state, bool ovrride = false);
    bool IsFFAPvP() const
    {
        return HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_FFA_PVP);
    }
    void SetFFAPvP(bool state);

    void UpdateZone(uint32 newZone, uint32 newArea);
    void UpdateArea(uint32 newArea);

    void UpdateZoneDependentAuras();
    void UpdateAreaDependentAuras(); // subzones
    void UpdateZoneDependentPets();

    void UpdateAfkReport(time_t currTime);
    void UpdatePvPFlag(time_t currTime);
    void UpdateContestedPvP(uint32 currTime);
    void SetContestedPvPTimer(uint32 newTime) { m_contestedPvPTimer = newTime; }
    void ResetContestedPvP()
    {
        clearUnitState(UNIT_STAT_ATTACK_PLAYER);
        RemoveFlag(PLAYER_FLAGS, PLAYER_FLAGS_CONTESTED_PVP);
        m_contestedPvPTimer = 0;
    }

    /** todo: -maybe move UpdateDuelFlag+DuelComplete to independent
     * DuelHandler.. **/
    DuelInfo* duel;
    bool IsInDuelWith(Player const* player) const
    {
        return duel && duel->opponent == player && duel->startTime != 0;
    }
    void UpdateDuelFlag(time_t currTime);
    void CheckDuelDistance(time_t currTime);
    void DuelComplete(DuelCompleteType type);
    void SendDuelCountdown(uint32 counter);

    bool IsInGroup() const { return GetGroup() != nullptr; }
    bool IsGroupVisibleFor(Player* p) const;
    bool IsInSameGroupWith(Player const* p) const;
    bool IsInSameRaidWith(Player const* p) const
    {
        return p == this ||
               (GetGroup() != nullptr && GetGroup() == p->GetGroup());
    }
    void UninviteFromGroup();
    static void RemoveFromGroup(Group* group, ObjectGuid guid);
    void RemoveFromGroup() { RemoveFromGroup(GetGroup(), GetObjectGuid()); }
    void SendUpdateToOutOfRangeGroupMembers();

    void SetInGuild(uint32 GuildId) { SetUInt32Value(PLAYER_GUILDID, GuildId); }
    void SetRank(uint32 rankId) { SetUInt32Value(PLAYER_GUILDRANK, rankId); }
    void SetGuildIdInvited(uint32 GuildId) { m_GuildIdInvited = GuildId; }
    uint32 GetGuildId() { return GetUInt32Value(PLAYER_GUILDID); }
    static uint32 GetGuildIdFromDB(ObjectGuid guid);
    uint32 GetRank() { return GetUInt32Value(PLAYER_GUILDRANK); }
    static uint32 GetRankFromDB(ObjectGuid guid);
    int GetGuildIdInvited() { return m_GuildIdInvited; }
    static void RemovePetitionsAndSigns(ObjectGuid guid, uint32 type);

    // Arena Team
    void SetInArenaTeam(uint32 ArenaTeamId, uint8 slot)
    {
        SetArenaTeamInfoField(slot, ARENA_TEAM_ID, ArenaTeamId);
    }
    void SetArenaTeamInfoField(uint8 slot, ArenaTeamInfoType type, uint32 value)
    {
        SetUInt32Value(
            PLAYER_FIELD_ARENA_TEAM_INFO_1_1 + (slot * ARENA_TEAM_END) + type,
            value);
    }
    uint32 GetArenaTeamId(uint8 slot) const
    {
        return GetUInt32Value(PLAYER_FIELD_ARENA_TEAM_INFO_1_1 +
                              (slot * ARENA_TEAM_END) + ARENA_TEAM_ID);
    }
    uint32 GetArenaPersonalRating(uint8 slot) const
    {
        return GetUInt32Value(PLAYER_FIELD_ARENA_TEAM_INFO_1_1 +
                              (slot * ARENA_TEAM_END) +
                              ARENA_TEAM_PERSONAL_RATING);
    }
    static uint32 GetArenaTeamIdFromDB(ObjectGuid guid, ArenaType type);
    void SetArenaTeamIdInvited(uint32 ArenaTeamId)
    {
        m_ArenaTeamIdInvited = ArenaTeamId;
    }
    uint32 GetArenaTeamIdInvited() { return m_ArenaTeamIdInvited; }
    static void LeaveAllArenaTeams(ObjectGuid guid);

    void SetDifficulty(Difficulty dungeon_difficulty)
    {
        m_dungeonDifficulty = dungeon_difficulty;
    }
    Difficulty GetDifficulty() { return m_dungeonDifficulty; }

    bool UpdateSkill(uint32 skill_id, uint32 step);
    bool UpdateSkillPro(uint16 SkillId, int32 Chance, uint32 step);

    bool UpdateCraftSkill(uint32 spellid);
    bool UpdateGatherSkill(uint32 SkillId, uint32 SkillValue, uint32 RedLevel,
        uint32 Multiplicator = 1);
    bool UpdateFishingSkill();

    uint32 GetBaseDefenseSkillValue() const
    {
        return GetBaseSkillValue(SKILL_DEFENSE);
    }
    uint32 GetBaseWeaponSkillValue(WeaponAttackType attType) const;

    uint32 GetSpellByProto(ItemPrototype* proto);

    float GetHealthBonusFromStamina();
    float GetManaBonusFromIntellect();

    bool UpdateStats(Stats stat) override;
    bool UpdateAllStats() override;
    void UpdateResistances(uint32 school) override;
    void UpdateArmor() override;
    void UpdateMaxHealth() override;
    void UpdateMaxPower(Powers power) override;
    void UpdateAttackPowerAndDamage(bool ranged = false) override;
    void UpdateShieldBlockValue();
    void UpdateDamagePhysical(WeaponAttackType attType) override;
    void UpdateSpellDamageAndHealingBonus();
    void ApplyRatingMod(CombatRating cr, int32 value, bool apply);
    void UpdateRating(CombatRating cr);
    void UpdateAllRatings();

    void CalculateMinMaxDamage(WeaponAttackType attType, bool normalized,
        float& min_damage, float& max_damage);

    void UpdateDefenseBonusesMod();
    float GetMeleeCritFromAgility();
    float GetDodgeFromAgility();
    float GetSpellCritFromIntellect();
    float OCTRegenHPPerSpirit();
    float OCTRegenMPPerSpirit();
    float GetRatingMultiplier(CombatRating cr) const;
    float GetRatingBonusValue(CombatRating cr) const;
    uint32 GetMeleeCritDamageReduction(uint32 damage) const;
    uint32 GetRangedCritDamageReduction(uint32 damage) const;
    uint32 GetSpellCritDamageReduction(uint32 damage) const;
    uint32 GetDotDamageReduction(uint32 damage) const;

    float GetExpertiseDodgeOrParryReduction(WeaponAttackType attType) const;
    void UpdateBlockPercentage();
    void UpdateCritPercentage(WeaponAttackType attType);
    void UpdateAllCritPercentages();
    void UpdateParryPercentage();
    void UpdateDodgePercentage();
    void UpdateMeleeHitChances();
    void UpdateRangedHitChances();
    void UpdateSpellHitChances();

    void UpdateAllSpellCritChances();
    void UpdateSpellCritChance(uint32 school);
    void UpdateExpertise(WeaponAttackType attType);
    void UpdateManaRegen();
    // Return: first float is regen OO casting frame in MECHANIC_BANISH, second
    // float is regen inside of casting frame in MECHANIC_BANISH
    std::pair<float, float> CalcCyclonedRegen();

    ObjectGuid const& GetLootGuid() const { return m_lootGuid; }
    void SetLootGuid(ObjectGuid const& guid) { m_lootGuid = guid; }

    void RemovedInsignia(Player* looterPlr);

    WorldSession* GetSession() const { return m_session.get(); }
    // You don't want to use this function casually. Better yet, don't use it!
    void OverwriteSession(std::shared_ptr<WorldSession> session);

    void BuildCreateUpdateBlockForPlayer(
        UpdateData* data, Player* target) const override;
    void DestroyForPlayer(Player* target) const override;
    void SendLogXPGain(
        uint32 GivenXP, Unit* victim, uint32 RestXP, float grp_coeff = 1.0f);

    uint8 LastSwingErrorMsg() const { return m_swingErrorMsg; }
    void SwingErrorMsg(uint8 val) { m_swingErrorMsg = val; }

    // notifiers
    void SendAttackSwingCantAttack();
    void SendAttackSwingCancelAttack();
    void SendAttackSwingDeadTarget();
    void SendAttackSwingNotStanding();
    void SendAttackSwingNotInRange();
    void SendAttackSwingBadFacingAttack();
    void SendAutoRepeatCancel();
    void SendExplorationExperience(uint32 Area, uint32 Experience);

    void SendDungeonDifficulty(bool IsInGroup);
    void ResetInstances(InstanceResetMethod method);
    void SendResetInstanceSuccess(uint32 MapId);
    void SendResetInstanceFailed(InstanceResetFailMsg reason, uint32 MapId);
    void SendResetFailedNotify(uint32 mapid);

    bool SetPosition(
        float x, float y, float z, float orientation, bool teleport = false);
    void UpdateUnderwaterState();

    Corpse* GetCorpse() const;
    void SpawnCorpseBones();
    Corpse* CreateCorpse();
    void KillPlayer();
    uint32 GetResurrectionSpellId();
    void ResurrectPlayer(float restore_percent, bool applySickness = false);
    void BuildPlayerRepop(bool spawn_corpse = true);
    void RepopAtGraveyard();

    // if pct == true: val < 0 => relative loss (-0.1 loses 10% durability); val
    // > 0: absolute (1.0 -> puts durability at 100%)
    void durability(bool pct, double val, bool inventory);
    void durability(Item* item, bool pct, double val);
    inventory::copper repair_cost(float modifier = 1.0f) const;
    inventory::copper repair_cost(Item* item, float modifier = 1.0f) const;
    // Pulls away one durability on a random equipment item
    void rand_equip_dura(bool include_weapons = false);
    void weap_dura_loss(WeaponAttackType attack_type = BASE_ATTACK);

    void UpdateMirrorTimers();
    void StopMirrorTimers()
    {
        StopMirrorTimer(FATIGUE_TIMER);
        StopMirrorTimer(BREATH_TIMER);
        StopMirrorTimer(FIRE_TIMER);
    }

    void SetMovement(PlayerMovementType pType);

    void JoinedChannel(Channel* c);
    void LeftChannel(Channel* c);
    void CleanupChannels();
    void UpdateLocalChannels(uint32 newZone);

    // THREAD UNSAFE: ONLY CALL FROM NON-THREADED SCENARIO
    void JoinLFGChannel();
    // THREAD UNSAFE: ONLY CALL FROM NON-THREADED SCENARIO
    void LeaveLFGChannel();
    bool CanJoinLFGChannel();

    // Returns true if player can join channel (depends on channel flags)
    // NOTE: Does not perform CanJoinLFGChannel() checks
    // zone: Zone id that we're entering/are in
    bool CanJoinConstantChannel(const ChatChannelsEntry* channel, uint32 zone);

    void UpdateDefense();
    void UpdateWeaponSkill(WeaponAttackType attType);
    void UpdateCombatSkills(
        Unit* pVictim, WeaponAttackType attType, bool defence);

    void SetSkill(uint16 id, uint16 currVal, uint16 maxVal, uint16 step = 0);
    uint16 GetMaxSkillValue(
        uint32 skill) const; // max + perm. bonus + temp bonus
    uint16 GetPureMaxSkillValue(uint32 skill) const; // max
    uint16 GetSkillValue(
        uint32 skill) const; // skill value + perm. bonus + temp bonus
    uint16 GetBaseSkillValue(uint32 skill) const; // skill value + perm. bonus
    uint16 GetPureSkillValue(uint32 skill) const; // skill value
    int16 GetSkillPermBonusValue(uint32 skill) const;
    int16 GetSkillTempBonusValue(uint32 skill) const;
    bool HasSkill(uint32 skill) const;
    void learnSkillRewardedSpells(uint32 id, uint32 value);

    WorldLocation& GetTeleportDest() { return m_teleport_dest; }
    bool IsBeingTeleported() const
    {
        return mSemaphoreTeleport_Near || mSemaphoreTeleport_Far;
    }
    bool IsBeingTeleportedNear() const { return mSemaphoreTeleport_Near; }
    bool IsBeingTeleportedFar() const { return mSemaphoreTeleport_Far; }
    void SetSemaphoreTeleportNear(bool semphsetting)
    {
        mSemaphoreTeleport_Near = semphsetting;
    }
    void SetSemaphoreTeleportFar(bool semphsetting)
    {
        mSemaphoreTeleport_Far = semphsetting;
    }
    void ProcessDelayedOperations();
    uint32 GetTeleportOptions() const { return m_teleport_options; }
    void ClearTeleportOptions() { m_teleport_options = 0; }

    static Team TeamForRace(uint8 race);
    Team GetTeam() const { return m_team; }
    static uint32 getFactionForRace(uint8 race);
    void setFactionForRace(uint8 race);

    void InitDisplayIds();

    bool IsAtGroupRewardDistance(WorldObject const* pRewardSource) const;
    void RewardSinglePlayerAtKill(Unit* pVictim);
    void RewardPlayerAndGroupAtEvent(
        uint32 creature_id, WorldObject* pRewardSource);
    void RewardPlayerAndGroupAtCast(
        WorldObject* pRewardSource, uint32 spellid = 0);
    bool isHonorOrXPTarget(Unit* pVictim) const;

    ReputationMgr& GetReputationMgr() { return m_reputationMgr; }
    ReputationMgr const& GetReputationMgr() const { return m_reputationMgr; }
    ReputationRank GetReputationRank(uint32 faction_id) const;
    void RewardReputation(Unit* pVictim, float rate);
    void RewardReputation(Quest const* pQuest);
    int32 CalculateReputationGain(ReputationSource source, int32 rep,
        int32 faction, uint32 creatureOrQuestLevel = 0,
        bool noAuraBonus = false);

    void UpdateSkillsForLevel();
    void UpdateSkillsToMaxSkillsForLevel(); // for .levelup
    void ModifySkillBonus(uint32 skillid, int32 val, bool talent);

    /*********************************************************/
    /***                  PVP SYSTEM                       ***/
    /*********************************************************/
    void UpdateArenaFields();
    void UpdateHonorFields();
    // This gets pvp rank whether it is the active title or not
    int get_pvp_rank() const;
    bool RewardHonor(Unit* pVictim, uint32 groupsize, float honor = -1);
    uint32 GetHonorPoints() const
    {
        return GetUInt32Value(PLAYER_FIELD_HONOR_CURRENCY);
    }
    uint32 GetArenaPoints() const
    {
        return GetUInt32Value(PLAYER_FIELD_ARENA_CURRENCY);
    }
    void SetHonorPoints(uint32 value);
    void SetArenaPoints(uint32 value);
    void ModifyHonorPoints(int32 value);
    void ModifyArenaPoints(int32 value);

    uint32 GetMaxPersonalArenaRatingRequirement();

    // End of PvP System

    void trade(inventory::trade* t) { trade_ = t; }
    inventory::trade* trade() const { return trade_; }
    void cancel_trade();

    void SetDrunkValue(uint16 newDrunkValue, uint32 itemid = 0);
    uint16 GetDrunkValue() const { return m_drunk; }
    static DrunkenState GetDrunkenstateByValue(uint16 value);

    uint32 GetDeathTimer() const { return m_deathTimer; }
    uint32 GetCorpseReclaimDelay(bool pvp) const;
    void UpdateCorpseReclaimDelay();
    void SendCorpseReclaimDelay(bool load = false);

    uint32
    GetShieldBlockValue() const override; // overwrite Unit version (virtual)
    bool CanParry() const { return m_canParry; }
    void SetCanParry(bool value);
    bool CanBlock() const { return m_canBlock; }
    void SetCanBlock(bool value);
    bool CanDualWield() const { return m_canDualWield; }
    void SetCanDualWield(bool value);

    void SetRegularAttackTime();
    void SetBaseModValue(
        BaseModGroup modGroup, BaseModType modType, float value)
    {
        m_auraBaseMod[modGroup][modType] = value;
    }
    void HandleBaseModValue(
        BaseModGroup modGroup, BaseModType modType, float amount, bool apply);
    float GetBaseModValue(BaseModGroup modGroup, BaseModType modType) const;
    float GetTotalBaseModValue(BaseModGroup modGroup) const;
    float GetTotalPercentageModValue(BaseModGroup modGroup) const
    {
        return m_auraBaseMod[modGroup][FLAT_MOD] +
               m_auraBaseMod[modGroup][PCT_MOD];
    }
    void _ApplyAllStatBonuses();
    void _RemoveAllStatBonuses();

    void _ApplyWeaponDependentAuraMods(
        Item* item, WeaponAttackType attackType, bool apply);
    void _ApplyWeaponDependentAuraCritMod(
        Item* item, WeaponAttackType attackType, Aura* aura, bool apply);
    void _ApplyWeaponDependentAuraDamageMod(
        Item* item, WeaponAttackType attackType, Aura* aura, bool apply);

    void _ApplyItemMods(Item* item, bool apply);
    void _RemoveAllItemMods();
    void _ApplyAllItemMods();
    void _ApplyItemBonuses(Item* item, bool apply, inventory::slot item_slot);
    void _ApplyAmmoBonuses();
    void InitDataForForm(bool reapplyMods = false);
    void update_meta_gem();

    void ApplyEquipSpell(SpellEntry const* spellInfo, Item* item, bool apply,
        bool form_change = false);
    void UpdateEquipSpellsAtFormChange();
    void CastItemCombatSpell(Unit* Target, WeaponAttackType attType,
        ExtraAttackType extraAttackType = EXTRA_ATTACK_NONE,
        uint32 extraAttackId = 0, const SpellEntry* causingSpell = nullptr);
    void CastItemUseSpell(
        Item* item, SpellCastTargets const& targets, uint8 cast_count);

    void ApplyItemOnStoreSpell(Item* item, bool apply);

    void SendInitWorldStates(uint32 zone, uint32 area);
    void SendUpdateWorldState(uint32 Field, uint32 Value);
    void SendDirectMessage(WorldPacket&& data);

    void SendAuraDurationsForTarget(Unit* target);

    PlayerMenu* PlayerTalkClass;
    std::vector<ItemSetEffect*> ItemSetEff;

    void SendLoot(ObjectGuid guid, LootType loot_type);
    void SendLootRelease(ObjectGuid guid);
    void SendNotifyLootItemRemoved(uint8 lootSlot);
    void SendNotifyLootMoneyRemoved();

    /*********************************************************/
    /***               BATTLEGROUND SYSTEM                 ***/
    /*********************************************************/

    void ClearBGData();
    bool InBattleGround() const { return m_bgData.bgInstanceID != 0; }
    bool InArena() const;
    uint32 GetBattleGroundId() const { return m_bgData.bgInstanceID; }
    BattleGroundTypeId GetBattleGroundTypeId() const
    {
        return m_bgData.bgTypeID;
    }
    BattleGround* GetBattleGround() const;

    void SetBattleGroundId(uint32 val, BattleGroundTypeId bgTypeId)
    {
        m_bgData.bgInstanceID = val;
        m_bgData.bgTypeID = bgTypeId;
    }
    WorldLocation const& GetBattleGroundEntryPoint() const
    {
        return m_bgData.joinPos;
    }
    void SetBattleGroundEntryPoint(Player* leader = nullptr);

    void SetBGTeam(Team team) { m_bgData.bgTeam = team; }
    Team GetBGTeam() const
    {
        return m_bgData.bgTeam ? m_bgData.bgTeam : GetTeam();
    }

    void LeaveBattleground(bool teleportToEntryPoint = true);
    bool CanReportAfkDueToLimit();
    void ReportedAfkBy(Player* reporter);
    void ClearAfkReports() { m_bgData.bgAfkReporter.clear(); }

    bool GetBGAccessByLevel(BattleGroundTypeId bgTypeId) const;
    bool CanUseBattleGroundObject();
    bool isTotalImmune();
    bool CanCaptureTowerPoint();

    /*********************************************************/
    /***                    REST SYSTEM                    ***/
    /*********************************************************/

    bool isRested() const { return GetRestTime() >= 10 * IN_MILLISECONDS; }
    uint32 GetXPRestBonus(uint32 xp);
    uint32 GetRestTime() const { return m_restTime; }
    void SetRestTime(uint32 v) { m_restTime = v; }

    /*********************************************************/
    /***              ENVIROMENTAL SYSTEM                  ***/
    /*********************************************************/

    uint32 EnvironmentalDamage(EnvironmentalDamages type, uint32 damage);

    /*********************************************************/
    /***               FLOOD FILTER SYSTEM                 ***/
    /*********************************************************/

    void UpdateSpeakTime();
    bool CanSpeak() const;
    void ChangeSpeakTime(int utime);

    /*********************************************************/
    /***                 VARIOUS SYSTEMS                   ***/
    /*********************************************************/
    bool HasMovementFlag(MovementFlags f)
        const; // for script access to m_movementInfo.HasMovementFlag
    void UpdateFallInformationIfNeed(MovementInfo const& minfo, uint16 opcode);
    void SetFallInformation(uint32 time, float z)
    {
        m_lastFallTime = time;
        m_lastFallZ = z;
    }
    void HandleFall(MovementInfo const& movementInfo);

    void BuildTeleportAckMsg(
        WorldPacket& data, float x, float y, float z, float ang) const;
    void send_teleport_msg(float x, float y, float z, float o);

    bool isMoving() const
    {
        return m_movementInfo.HasMovementFlag(movementFlagsMask);
    }
    bool isMovingOrTurning() const
    {
        return m_movementInfo.HasMovementFlag(movementOrTurningFlagsMask);
    }

    bool CanFly() const
    {
        return m_movementInfo.HasMovementFlag(MOVEFLAG_CAN_FLY);
    }
    bool IsFlying() const
    {
        return m_movementInfo.HasMovementFlag(MOVEFLAG_FLYING);
    }
    bool IsFreeFlying() const
    {
        return HasAuraType(SPELL_AURA_MOD_FLIGHT_SPEED_MOUNTED) ||
               HasAuraType(SPELL_AURA_FLY);
    }

    void SetClientControl(Unit* target, uint8 allowMove);
    void SetMovingUnit(Unit* target) { m_mover = target ? target : this; }
    Unit* GetMovingUnit() const { return m_mover; }
    bool IsMovingSelf() const
    {
        return m_mover == this;
    } // normal case for player not controlling other unit
    // ClientMover represents the target the client THINKS it's moving, this
    // does not always reflect the actual mover server-side and is used to
    // determine if we should ignore movement packets.
    ObjectGuid GetClientMovingUnit() const { return client_mover_; }
    void SetClientMovingUnit(ObjectGuid guid) { client_mover_ = guid; }

    void SetRunMode(bool runmode, bool sendToClient = false);
    bool GetRunMode() const { return m_RunModeOn; };

    ObjectGuid const& GetFarSightGuid() const
    {
        return GetGuidValue(PLAYER_FARSIGHT);
    }

    uint32 GetSaveTimer() const { return m_nextSave; }
    void SetSaveTimer(uint32 timer) { m_nextSave = timer; }

    void SetHomebindToLocation(WorldLocation const& loc, uint32 area_id);
    void RelocateToHomebind()
    {
        SetLocationMapId(m_homebindMapId);
        Relocate(m_homebindX, m_homebindY, m_homebindZ);
    }
    bool TeleportToHomebind(uint32 options = 0)
    {
        return TeleportTo(m_homebindMapId, m_homebindX, m_homebindY,
            m_homebindZ, GetO(), options);
    }

    Object* GetObjectByTypeMask(ObjectGuid guid, TypeMask typemask);

    // currently visible objects at player client
    ObjectGuidSet m_clientGUIDs;

    bool HaveAtClient(WorldObject const* u) const
    {
        return u == this ||
               m_clientGUIDs.find(u->GetObjectGuid()) != m_clientGUIDs.end();
    }
    void AddToClient(WorldObject* u);
    void RemoveFromClient(WorldObject* u);

    bool IsVisibleInGridForPlayer(Player* pl) const override;
    bool IsVisibleGloballyFor(Player* pl) const;

    void UpdateVisibilityOf(WorldObject const* viewPoint, WorldObject* target);

    template <class T>
    void UpdateVisibilityOf(WorldObject const* viewPoint, T* target,
        UpdateData& data, std::set<WorldObject*>& visibleNow);

    Camera& GetCamera() { return m_camera; }

    uint8 m_forced_speed_changes[MAX_MOVE_TYPE];

    bool HasAtLoginFlag(AtLoginFlags f) const { return m_atLoginFlags & f; }
    void SetAtLoginFlag(AtLoginFlags f) { m_atLoginFlags |= f; }
    void RemoveAtLoginFlag(AtLoginFlags f, bool in_db_also = false);

    LookingForGroup m_lookingForGroup;

    // Temporarily removed pet cache
    uint32 GetTemporaryUnsummonedPetNumber() const
    {
        return m_temporaryUnsummonedPetNumber;
    }
    void SetTemporaryUnsummonedPetNumber(uint32 petnumber)
    {
        m_temporaryUnsummonedPetNumber = petnumber;
    }
    void UnsummonPetTemporaryIfAny();
    void ResummonPetTemporaryUnSummonedIfAny();
    bool IsPetNeedBeTemporaryUnsummoned() const
    {
        return !IsInWorld() || !isAlive() || IsMounted() /*+in flight*/;
    }

    void SendCinematicStart(uint32 CinematicSequenceId);

    movement_validator* move_validator;
    void set_gm_fly_mode(bool on);
    bool gm_flying() const { return gm_fly_mode_; }

    /*********************************************************/
    /***                 INSTANCE SYSTEM                   ***/
    /*********************************************************/

    typedef std::map<uint32 /*mapId*/, InstancePlayerBind> BoundInstancesMap;

    void UpdateHomebindTime(uint32 time);

    uint32 m_HomebindTimer;
    bool m_InstanceValid;

    // === Instance binds ===
    BoundInstancesMap& GetInstanceBindsMap(Difficulty i)
    {
        return m_instanceBinds[i];
    }
    InstancePlayerBind* GetInstanceBind(uint32 mapid, Difficulty difficulty);
    InstancePlayerBind* BindToInstance(
        std::shared_ptr<DungeonPersistentState> state, bool permanent);
    void UnbindFromInstance(uint32 mapid, Difficulty difficulty);
    // same as UnbindFromInstance but does not affect the database (i.e.
    // permanent state)
    void ClearInstanceBindOnDestruction(DungeonPersistentState* state);
    DungeonPersistentState* GetInstanceBindForZoning(uint32 mapid);
    void UpdateInstanceBindsOnGroupJoinLeave();
    void SendRaidInfo();
    void SendSavedInstances();

    void AddRecentDungeon(uint32 map, uint32 instanceId);
    bool IsDungeonLimitReached(uint32 map, uint32 instanceId);
    void ClearDungeonLimit() { m_recentDungeons.clear(); }

    // The last banker we interacted with. Used for checking if we are indeed at
    // a bank.
    void last_interacted_banker(ObjectGuid guid)
    {
        last_interacted_banker_ = guid;
    }
    ObjectGuid last_interacted_banker() const
    {
        return last_interacted_banker_;
    }

    bool CanUseCapturePoint() const;

    /* Wrappers for Item Handling through Scripts */
    /* All of these could probably be removed if scripts were made a part of the
     * core */
    bool add_item(uint32 id, uint32 count);
    bool can_add_item(uint32 id, uint32 count) const;
    // count == -1 means all of that item
    bool destroy_item(uint32 id, int count = -1);
    bool take_money(uint32 copper);
    bool give_money(uint32 copper);
    // Checks if any equipped item has a required spell that's equal to the
    // spell_id param
    bool match_req_spell(uint32 spell_id) const;
    // Gets the Team that controls the PvP objective that the player is close to
    Team outdoor_pvp_team() const;

    // When a crit is not scored against a player, resilience has a chance to do
    // crit procs anyway
    void DoResilienceCritProc(Unit* attacker, uint32 damage,
        WeaponAttackType att_type, const SpellEntry* spell);

    /*
     * Honorable Kill Distribtuion
    */
    // Honor gain is based on damage done, helper methods to cache this info:
    // The damage an enemy or group does is reset when it leaves combat with the
    // player
    void honor_damage_taken(Player* victim, uint32 dmg);
    void hk_distribute_honor();
    // Clear your pvp dmg entries list, and remove yourself from anyone else's
    void honor_clear_dmg_done();

    std::vector<PetDbData> _pet_store;

private:
    struct pvp_dmg_entry
    {
        pvp_dmg_entry(uint32 g) : low_guid(g), dmg(0), group(0), ignore(false)
        {
        }
        uint32 low_guid;
        uint32 dmg;
        short group; // How many people in the list reference this group
        bool ignore; // Ignore this damage, included as part of a group
    };
    std::pair<std::vector<pvp_dmg_entry>::iterator, bool> honor_get_entry(
        ObjectGuid guid);
    // Remove someone from your pvp dmg entries list
    void honor_remove_dmg_done(Player* victim);
    // For honor distribution, contains sorted player and group GUIDs
    std::vector<pvp_dmg_entry> pvp_dmg_recvd_;
    // List of players you have pvp dmg entries on. When *this leaves combat,
    // he clears himself from all players in this list.
    std::vector<uint32 /* low guid */> pvp_refd_players_;

public:
private:
    void UpdateRecentDungeons();
    std::vector<RecentDungeon> m_recentDungeons;

public:
    /*********************************************************/
    /***                   GROUP SYSTEM                    ***/
    /*********************************************************/

    Group* GetGroupInvite() { return m_groupInvite; }
    void SetGroupInvite(Group* group) { m_groupInvite = group; }
    Group* GetGroup() { return m_group.getTarget(); }
    const Group* GetGroup() const { return (const Group*)m_group.getTarget(); }
    GroupReference& GetGroupRef() { return m_group; }
    void SetGroup(Group* group, int8 subgroup = -1);
    uint8 GetSubGroup() const { return m_group.getSubGroup(); }
    uint32 GetGroupUpdateFlag() const { return m_groupUpdateMask; }
    void SetGroupUpdateFlag(uint32 flag) { m_groupUpdateMask |= flag; }
    const uint64& GetAuraUpdateMask() const { return m_auraUpdateMask; }
    void SetAuraUpdateMask(uint8 slot)
    {
        m_auraUpdateMask |= (uint64(1) << slot);
    }
    Player* GetNextRandomRaidMember(float radius);
    PartyResult CanUninviteFromGroup() const;
    // BattleGround Group System
    void SetBattleGroundRaid(Group* group, int8 subgroup = -1);
    void RemoveFromBattleGroundRaid();
    Group* GetOriginalGroup() { return m_originalGroup.getTarget(); }
    GroupReference& GetOriginalGroupRef() { return m_originalGroup; }
    uint8 GetOriginalSubGroup() const { return m_originalGroup.getSubGroup(); }
    void SetOriginalGroup(Group* group, int8 subgroup = -1);

    MapReference& GetMapRef() { return m_mapRef; }

    bool IsAllowedToLoot(const Creature* creature) const;
    bool HasTapOn(const Creature* creature) const
    {
        return creature->IsTappedBy(this);
    }

    DeclinedName const* GetDeclinedNames() const { return m_declinedname; }
    bool HasTitle(uint32 bitIndex) const;
    bool HasTitle(CharTitlesEntry const* title) const
    {
        return HasTitle(title->bit_index);
    }
    void SetTitle(CharTitlesEntry const* title, bool lost = false);

    void UpdateZoneAreaCache() override;

    G3D::Vector3 GetShadowstepPoint(Unit* target);

    CreatureAI* AI() { return i_AI; }
    bool AIM_Initialize();
    bool AIM_Deinitialize();

    bool PrePossessRunMode;

    bool InControl() const
    {
        return in_control_;
    } // FIXME: This is not a nice way to do it. The whole in
      // control/mover/moved by thing needs a rewrite though

    // Disgusting solutions here we go: This boolean is needed for the spell
    // system
    // when auto shot finishes to start a new GCD-less steady shot (GCD already
    // triggered)
    bool pending_steady_shot;

    // This can only be !nullptr for a GM. This may NOT be accessed in a
    // multi-threaded
    // scenario. See GMTicketMgr.h for information.
    std::shared_ptr<ticket> checked_out_ticket;

    uint32 spying_on_; // Group id that GM is spying on
    uint32 spy_subgroup_;

protected:
    uint32 m_contestedPvPTimer;

    /*********************************************************/
    /***               BATTLEGROUND SYSTEM                 ***/
    /*********************************************************/
    BGData m_bgData;

    /*********************************************************/
    /***                    QUEST SYSTEM                   ***/
    /*********************************************************/

    // We allow only one timed quest active at the same time. Below can then be
    // simple value instead of set.
    typedef std::set<uint32> QuestSet;
    QuestSet m_timedquests;

    ObjectGuid m_dividerGuid;
    uint32 m_ingametime;

    /*********************************************************/
    /***                   LOAD SYSTEM                     ***/
    /*********************************************************/

    void _LoadActions(QueryResult* result);
    void _LoadAuras(QueryResult* result, uint32 timediff);
    void _LoadInstanceBinds(QueryResult* result);
    void _LoadInventory(QueryResult* result, uint32 money, uint32 timediff);
    void _LoadItemLoot(QueryResult* result);
    void _LoadMails(QueryResult* result);
    void _LoadMailedItems(QueryResult* result);
    void _LoadQuestStatus(QueryResult* result);
    void _LoadDailyQuestStatus(QueryResult* result);
    void _LoadGroup(QueryResult* result);
    void _LoadSkills(QueryResult* result);
    void _LoadSpells(QueryResult* result);
    void _LoadFriendList(QueryResult* result);
    bool _LoadHomeBind(QueryResult* result);
    void _LoadDeclinedNames(QueryResult* result);
    void _LoadArenaTeamInfo(QueryResult* result);
    void _LoadBGData(QueryResult* result);
    void _LoadIntoDataField(const char* data, uint32 startOffset, uint32 count);
    void _LoadRecentDungeons(QueryResult* result);
    void _LoadPetStore(SqlQueryHolder* holder);

    /*********************************************************/
    /***                   SAVE SYSTEM                     ***/
    /*********************************************************/

    void _SaveActions();
    void _SaveAuras();
    void _SaveInstanceBinds();
    void _SaveInventory();
    void _SaveMail();
    void _SaveQuestStatus();
    void _SaveDailyQuestStatus();
    void _SaveSkills();
    void _SaveSpells();
    void _SaveStats();
    void _SaveRecentDungeons();
    void _SaveBGData();
    void _SavePetDbDatas();
    static void write_pet_db_data(const PetDbData& data);
    static void drop_pet_db_data(uint32 guid);

    void _SetCreateBits(UpdateMask* updateMask, Player* target) const override;
    void _SetUpdateBits(UpdateMask* updateMask, Player* target) const override;

    /*********************************************************/
    /***              ENVIRONMENTAL SYSTEM                 ***/
    /*********************************************************/
    void HandleSobering();
    void SendMirrorTimer(MirrorTimerType Type, uint32 MaxValue,
        uint32 CurrentValue, int32 Regen);
    void StopMirrorTimer(MirrorTimerType Type);
    void HandleDrowning(uint32 time_diff);
    int32 getMaxTimer(MirrorTimerType timer);

    /*********************************************************/
    /***                  HONOR SYSTEM                     ***/
    /*********************************************************/
    time_t m_lastHonorUpdateTime;

    void outDebugStatsValues() const;
    ObjectGuid m_lootGuid;

    Team m_team;
    uint32 m_nextSave;
    time_t m_speakTime;
    uint32 m_speakCount;
    Difficulty m_dungeonDifficulty;

    uint32 m_atLoginFlags;

    uint32 m_ExtraFlags;
    ObjectGuid m_curSelectionGuid;

    ObjectGuid m_comboTargetGuid;
    int8 m_comboPoints;

    QuestStatusMap mQuestStatus;

    SkillStatusMap mSkillStatus;

    uint32 m_GuildIdInvited;
    uint32 m_ArenaTeamIdInvited;

    PlayerMails m_mail;
    PlayerSpellMap m_spells;
    SpellCooldowns m_spellCooldowns;
    std::map<uint32 /*spell id*/, uint32 /*end timestamp*/>
        proc_event_cooldowns_;

    GlobalCooldownMgr m_GlobalCooldownMgr;

    ActionButtonList m_actionButtons;

    float m_auraBaseMod[BASEMOD_END][MOD_END];
    int16 m_baseRatingValue[MAX_COMBAT_RATING];

    SpellModList m_spellMods[MAX_SPELLMOD];
    int32 m_SpellModRemoveCount;

    std::vector<Item*> enchdur_items_;  // Items with enchantment durations
    std::vector<Item*> duration_items_; // Items with a limited lifetime

    ObjectGuid m_resurrectGuid;
    uint32 m_resurrectMap;
    float m_resurrectX, m_resurrectY, m_resurrectZ;
    uint32 m_resurrectHealth, m_resurrectMana;

    std::shared_ptr<WorldSession> m_session;

    typedef std::list<Channel*> JoinedChannelsList;
    JoinedChannelsList m_channels;

    inventory::trade* trade_;

    uint32 m_cinematic;

    bool m_DailyQuestChanged;

    uint32 m_drunkTimer;
    uint16 m_drunk;
    uint32 combat_dura_timer_; // We lose durability on our gear while being in
                               // combat

    uint32 m_deathTimer;
    time_t m_deathExpireTime;

    uint32 m_restTime;

    uint32 m_WeaponProficiency;
    uint32 m_ArmorProficiency;
    bool m_canParry;
    bool m_canBlock;
    bool m_canDualWield;
    uint8 m_swingErrorMsg;
    float m_ammoDPS;

    ////////////////////Rest System/////////////////////
    time_t time_inn_enter;
    uint32 inn_trigger_id;
    float m_rest_bonus;
    RestType rest_type;
    ////////////////////Rest System/////////////////////

    uint32 m_resetTalentsCost;
    time_t m_resetTalentsTime;
    uint32 m_usedTalentCount;

    // Social
    PlayerSocial* m_social;

    // Groups
    GroupReference m_group;
    GroupReference m_originalGroup;
    Group* m_groupInvite;
    uint32 m_groupUpdateMask;
    uint64 m_auraUpdateMask;

    ObjectGuid m_miniPetGuid;

    // Player summoning
    time_t m_summon_expire;
    uint32 m_summon_mapid;
    float m_summon_x;
    float m_summon_y;
    float m_summon_z;

    DeclinedName* m_declinedname;

private:
    // Internal function to handle Say(), Yell() or TextEmote() while player is
    // in an arena
    void ArenaChat(
        const std::string& text, uint32 channel, uint32 language, float range);

    void UpdateKnownCurrencies(uint32 itemId, bool apply);
    void AdjustQuestReqItemCount(
        Quest const* pQuest, QuestStatusData& questStatusData);

    void SetCanDelayTeleport(bool setting) { m_bCanDelayTeleport = setting; }
    bool ShouldExecuteDelayedTeleport()
    {
        // we should not execute delayed teleports for now dead players but has
        // been alive at teleport
        // because we don't want player's ghost teleported from graveyard
        return m_bHasDelayedTeleport &&
               (isAlive() || !m_bHasBeenAliveAtDelayedTeleport);
    }

    bool SetDelayedTeleportFlagIfCan()
    {
        m_bHasDelayedTeleport = m_bCanDelayTeleport;
        m_bHasBeenAliveAtDelayedTeleport = isAlive();
        return m_bHasDelayedTeleport;
    }

    void ScheduleDelayedOperation(uint32 operation)
    {
        if (operation < DELAYED_END)
            m_DelayedOperations |= operation;
    }

    inventory::personal_storage inventory_;

    Unit* m_mover;
    ObjectGuid client_mover_;
    Camera m_camera;

    MapReference m_mapRef;

    // Homebind coordinates
    uint32 m_homebindMapId;
    uint16 m_homebindAreaId;
    float m_homebindX;
    float m_homebindY;
    float m_homebindZ;

    uint32 m_lastFallTime;
    float m_lastFallZ;

    LiquidTypeEntry const* m_lastLiquid;

    int32 m_MirrorTimer[MAX_TIMERS];
    uint8 m_MirrorTimerFlags;
    uint8 m_MirrorTimerFlagsLast;
    bool m_isInWater;

    // Current teleport data
    WorldLocation m_teleport_dest;
    uint32 m_teleport_options;
    bool mSemaphoreTeleport_Near;
    bool mSemaphoreTeleport_Far;

    uint32 m_DelayedOperations;
    bool m_bHasDelayedTeleport;
    bool m_bCanDelayTeleport;
    bool m_bHasBeenAliveAtDelayedTeleport;
    bool recently_relocated_;

    // Temporary removed pet cache
    uint32 m_temporaryUnsummonedPetNumber;

    ReputationMgr m_reputationMgr;

    uint32 m_timeSyncCounter;
    uint32 m_timeSyncTimer;

    CreatureAI* i_AI;
    bool m_AI_locked;
    bool m_RunModeOn;

    ObjectGuid last_interacted_banker_;

    bool gm_fly_mode_;
    bool in_control_; // FIXME: This is not a nice way to do it. The whole in
                      // control/mover/moved by thing needs a rewrite though

    std::map<uint32 /*category id*/, time_t /*expiry timestamp*/>
        category_cooldowns_; // TODO: All cooldowns should be saved in
                             // milliseconds

    // Ticks after login until we send the unroot packet
    int unroot_hack_ticks_;

    void CheckAreaExploreAndOutdoor();
    int was_outdoors_; // -1 if not set yet

    BoundInstancesMap m_instanceBinds[MAX_DIFFICULTY];

    ObjectGuid bg_saved_pet_;

    bool knockbacked_;
};

void AddItemsSetItem(Player* player, Item* item);
void RemoveItemsSetItem(Player* player, ItemPrototype const* proto);

// "the bodies of template functions must be made available in a header file"
template <class T>
T Player::ApplySpellMod(
    uint32 spellId, SpellModOp op, T& basevalue, Spell const* spell, bool peak)
{
    SpellEntry const* spellInfo = sSpellStore.LookupEntry(spellId);
    if (!spellInfo)
        return 0;
    int32 totalpct = 0;
    int32 totalflat = 0;
    for (auto mod : m_spellMods[op])
    {
        if (!IsAffectedBySpellmod(spellInfo, mod, spell))
            continue;
        if (mod->type == SPELLMOD_FLAT)
            totalflat += mod->value;
        else if (mod->type == SPELLMOD_PCT)
        {
            // skip percent mods for null basevalue (most important for spell
            // mods with charges )
            if (basevalue == T(0))
                continue;

            // special case (skip >10sec spell casts for instant cast setting)
            if (mod->op == SPELLMOD_CASTING_TIME &&
                basevalue >= T(10 * IN_MILLISECONDS) && mod->value <= -100)
                continue;

            totalpct += mod->value;
        }

        // Allow "peaking" spell mods, without using a charge
        if (!peak)
        {
            if (mod->charges > 0)
            {
                if (!spell)
                    spell = FindCurrentSpellBySpellId(spellId);

                // avoid double use spellmod charge by same spell
                if (!mod->lastAffected || mod->lastAffected != spell)
                {
                    --mod->charges;

                    if (mod->charges == 0)
                    {
                        mod->charges = -1;
                        ++m_SpellModRemoveCount;
                    }

                    mod->lastAffected = spell;
                }
            }
        }
    }

    float diff = (float)basevalue * (float)totalpct / 100.0f + (float)totalflat;
    basevalue = T((float)basevalue + diff);
    return T(diff);
}

#endif
