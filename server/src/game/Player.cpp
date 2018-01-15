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

#include "Player.h"
#include "ArenaTeam.h"
#include "BattleGround.h"
#include "BattleGroundAV.h"
#include "BattleGroundMgr.h"
#include "Channel.h"
#include "ChannelMgr.h"
#include "Chat.h"
#include "ConditionMgr.h"
#include "CreatureAI.h"
#include "DynamicObject.h"
#include "Formulas.h"
#include "GossipDef.h"
#include "Group.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "InstanceData.h"
#include "Language.h"
#include "logging.h"
#include "Mail.h"
#include "MapManager.h"
#include "MapPersistentStateMgr.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Opcodes.h"
#include "Pet.h"
#include "PlayerCharmAI.h"
#include "QuestDef.h"
#include "ScriptMgr.h"
#include "SkillDiscovery.h"
#include "SocialMgr.h"
#include "SpecialVisCreature.h"
#include "Totem.h"
#include "TemporarySummon.h"
#include "Spell.h"
#include "SpellMgr.h"
#include "TemporarySummon.h"
#include "Transport.h"
#include "UpdateData.h"
#include "UpdateMask.h"
#include "Util.h"
#include "Weather.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "lfg_tool_container.h"
#include "loot_distributor.h"
#include "Database/DatabaseEnv.h"
#include "Database/DatabaseImpl.h"
#include "maps/visitors.h"
#include "OutdoorPvP/OutdoorPvP.h"
#include "inventory/trade.h"
#include <boost/algorithm/string/replace.hpp>
#include <cmath>
#include <numeric>

static auto& combat_logger = logging.get_logger("combat");

#define PLAYER_SKILL_INDEX(x) (PLAYER_SKILL_INFO_1_1 + ((x)*3))
#define PLAYER_SKILL_VALUE_INDEX(x) (PLAYER_SKILL_INDEX(x) + 1)
#define PLAYER_SKILL_BONUS_INDEX(x) (PLAYER_SKILL_INDEX(x) + 2)

#define SKILL_VALUE(x) PAIR32_LOPART(x)
#define SKILL_MAX(x) PAIR32_HIPART(x)
#define MAKE_SKILL_VALUE(v, m) MAKE_PAIR32(v, m)

#define SKILL_TEMP_BONUS(x) int16(PAIR32_LOPART(x))
#define SKILL_PERM_BONUS(x) int16(PAIR32_HIPART(x))
#define MAKE_SKILL_BONUS(t, p) MAKE_PAIR32(t, p)

#define DUNGEON_LIMIT_NUM 5
#define DUNGEON_LIMIT_TIME 3600

enum CharacterFlags
{
    CHARACTER_FLAG_NONE = 0x00000000,
    CHARACTER_FLAG_UNK1 = 0x00000001,
    CHARACTER_FLAG_UNK2 = 0x00000002,
    CHARACTER_LOCKED_FOR_TRANSFER = 0x00000004,
    CHARACTER_FLAG_UNK4 = 0x00000008,
    CHARACTER_FLAG_UNK5 = 0x00000010,
    CHARACTER_FLAG_UNK6 = 0x00000020,
    CHARACTER_FLAG_UNK7 = 0x00000040,
    CHARACTER_FLAG_UNK8 = 0x00000080,
    CHARACTER_FLAG_UNK9 = 0x00000100,
    CHARACTER_FLAG_UNK10 = 0x00000200,
    CHARACTER_FLAG_HIDE_HELM = 0x00000400,
    CHARACTER_FLAG_HIDE_CLOAK = 0x00000800,
    CHARACTER_FLAG_UNK13 = 0x00001000,
    CHARACTER_FLAG_GHOST = 0x00002000,
    CHARACTER_FLAG_RENAME = 0x00004000,
    CHARACTER_FLAG_UNK16 = 0x00008000,
    CHARACTER_FLAG_UNK17 = 0x00010000,
    CHARACTER_FLAG_UNK18 = 0x00020000,
    CHARACTER_FLAG_UNK19 = 0x00040000,
    CHARACTER_FLAG_UNK20 = 0x00080000,
    CHARACTER_FLAG_UNK21 = 0x00100000,
    CHARACTER_FLAG_UNK22 = 0x00200000,
    CHARACTER_FLAG_UNK23 = 0x00400000,
    CHARACTER_FLAG_UNK24 = 0x00800000,
    CHARACTER_FLAG_LOCKED_BY_BILLING = 0x01000000,
    CHARACTER_FLAG_DECLINED = 0x02000000,
    CHARACTER_FLAG_UNK27 = 0x04000000,
    CHARACTER_FLAG_UNK28 = 0x08000000,
    CHARACTER_FLAG_UNK29 = 0x10000000,
    CHARACTER_FLAG_UNK30 = 0x20000000,
    CHARACTER_FLAG_UNK31 = 0x40000000,
    CHARACTER_FLAG_UNK32 = 0x80000000
};

// corpse reclaim times
#define DEATH_EXPIRE_STEP (5 * MINUTE)
#define MAX_DEATH_COUNT 5

static uint32 copseReclaimDelay[MAX_DEATH_COUNT] = {0, 30, 60, 90, 120};

//== PlayerTaxi ================================================

PlayerTaxi::PlayerTaxi()
{
    // Taxi nodes
    memset(m_taximask, 0, sizeof(m_taximask));
    m_Express = false;
    m_DisplayId = 0;
}

void PlayerTaxi::InitTaxiNodesForLevel(uint32 race, uint32 /*level*/)
{
    // race specific initial known nodes: capital and taxi hub masks
    switch (race)
    {
    case RACE_HUMAN:
        SetTaximaskNode(2);
        break; // Human
    case RACE_ORC:
        SetTaximaskNode(23);
        break; // Orc
    case RACE_DWARF:
        SetTaximaskNode(6);
        break; // Dwarf
    case RACE_NIGHTELF:
        SetTaximaskNode(26);
        SetTaximaskNode(27);
        break; // Night Elf
    case RACE_UNDEAD:
        SetTaximaskNode(11);
        break; // Undead
    case RACE_TAUREN:
        SetTaximaskNode(22);
        break; // Tauren
    case RACE_GNOME:
        SetTaximaskNode(6);
        break; // Gnome
    case RACE_TROLL:
        SetTaximaskNode(23);
        break; // Troll
    case RACE_BLOODELF:
        SetTaximaskNode(82);
        break; // Blood Elf
    case RACE_DRAENEI:
        SetTaximaskNode(94);
        break; // Draenei
    }

    // new continent starting masks (It will be accessible only at new map)
    switch (Player::TeamForRace(race))
    {
    case ALLIANCE:
        SetTaximaskNode(100);
        break;
    case HORDE:
        SetTaximaskNode(99);
        break;
    default:
        break;
    }
    // level dependent taxi hubs
    // if (level >= 68)
    // SetTaximaskNode(213);                               //Shattered Sun
    // Staging Area
}

void PlayerTaxi::LoadTaxiMask(const char* data)
{
    Tokens tokens = StrSplit(data, " ");

    int index;
    Tokens::iterator iter;
    for (iter = tokens.begin(), index = 0;
         (index < TaxiMaskSize) && (iter != tokens.end()); ++iter, ++index)
    {
        // load and set bits only for existing taxi nodes
        m_taximask[index] =
            sTaxiNodesMask[index] & uint32(atol((*iter).c_str()));
    }
}

void PlayerTaxi::AppendTaximaskTo(ByteBuffer& data, bool all)
{
    if (all)
    {
        for (auto& elem : sTaxiNodesMask)
            data << uint32(elem); // all existing nodes
    }
    else
    {
        for (auto& elem : m_taximask)
            data << uint32(elem); // known nodes
    }
}

bool PlayerTaxi::LoadTaxiDestinationsFromString(
    const std::string& values, Team team)
{
    ClearTaxiDestinations();

    Tokens tokens = StrSplit(values, " ");

    for (auto iter = tokens.begin(); iter != tokens.end(); ++iter)
    {
        uint32 node = uint32(atol(iter->c_str()));
        AddTaxiDestination(node);
    }

    if (m_TaxiDestinations.empty())
        return true;

    // Check integrity
    if (m_TaxiDestinations.size() < 2)
        return false;

    for (size_t i = 1; i < m_TaxiDestinations.size(); ++i)
    {
        uint32 cost;
        uint32 path;
        sObjectMgr::Instance()->GetTaxiPath(
            m_TaxiDestinations[i - 1], m_TaxiDestinations[i], path, cost);
        if (!path)
            return false;
    }

    // can't load taxi path without mount set (quest taxi path?)
    if (!sObjectMgr::Instance()->GetTaxiMountDisplayId(
            GetTaxiSource(), team, true))
        return false;

    return true;
}

std::string PlayerTaxi::SaveTaxiDestinationsToString()
{
    if (m_TaxiDestinations.empty())
        return "";

    std::ostringstream ss;
    size_t mSize = m_TaxiDestinations.size();
    // We only store current taxi destination and the current source (allows
    // people to "log out" and cancel paths)
    if (mSize > 2)
        mSize = 2;

    for (size_t i = 0; i < mSize; ++i)
        ss << m_TaxiDestinations[i] << " ";

    return ss.str();
}

uint32 PlayerTaxi::GetCurrentTaxiPath() const
{
    if (m_TaxiDestinations.size() < 2)
        return 0;

    uint32 path;
    uint32 cost;

    sObjectMgr::Instance()->GetTaxiPath(
        m_TaxiDestinations[0], m_TaxiDestinations[1], path, cost);

    return path;
}

std::ostringstream& operator<<(std::ostringstream& ss, PlayerTaxi const& taxi)
{
    for (int i = 0; i < TaxiMaskSize; ++i)
        ss << taxi.m_taximask[i] << " ";
    return ss;
}

SpellModifier::SpellModifier(SpellModOp _op, SpellModType _type, int32 _value,
    SpellEntry const* spellEntry, SpellEffectIndex eff, int16 _charges /*= 0*/)
  : op(_op), type(_type), charges(_charges), value(_value),
    spellId(spellEntry->Id), lastAffected(nullptr)
{
    mask = sSpellMgr::Instance()->GetSpellAffectMask(spellEntry->Id, eff);
    timestamp = WorldTimer::getMSTime();
}

SpellModifier::SpellModifier(SpellModOp _op, SpellModType _type, int32 _value,
    Aura const* aura, int16 _charges /*= 0*/)
  : op(_op), type(_type), charges(_charges), value(_value),
    spellId(aura->GetId()), lastAffected(nullptr)
{
    mask = sSpellMgr::Instance()->GetSpellAffectMask(
        aura->GetId(), aura->GetEffIndex());
    timestamp = WorldTimer::getMSTime();
}

bool SpellModifier::isAffectedOnSpell(SpellEntry const* spell) const
{
    SpellEntry const* affect_spell = sSpellStore.LookupEntry(spellId);
    // False if affect_spell == NULL or spellFamily not equal
    if (!affect_spell ||
        affect_spell->SpellFamilyName != spell->SpellFamilyName)
        return false;
    return spell->IsFitToFamilyMask(mask);
}

//== Player ====================================================

UpdateMask Player::updateVisualBits;

Player::Player(std::shared_ptr<WorldSession> session)
  : Unit(), m_session(std::move(session)), inventory_(this), m_mover(this),
    m_camera(this), m_reputationMgr(this)
{
    in_global_transit = false;

    // Players are always active
    SetActiveObjectState(true);

    m_speakTime = 0;
    m_speakCount = 0;

    m_objectType |= TYPEMASK_PLAYER;
    m_objectTypeId = TYPEID_PLAYER;

    m_valuesCount = PLAYER_END;

    m_ExtraFlags = 0;

    m_comboPoints = 0;

    m_usedTalentCount = 0;

    m_regenTimer = 0;

    m_nextSave = sWorld::Instance()->getConfig(CONFIG_UINT32_INTERVAL_SAVE);

    // randomize first save time in range [CONFIG_UINT32_INTERVAL_SAVE] around
    // [CONFIG_UINT32_INTERVAL_SAVE]
    // this must help in case next save after mass player load after server
    // startup
    m_nextSave = urand(m_nextSave / 2, m_nextSave * 3 / 2);

    clearResurrectRequestData();

    m_SpellModRemoveCount = 0;

    m_social = nullptr;

    // group is initialized in the reference constructor
    SetGroupInvite(nullptr);
    m_groupUpdateMask = 0;
    m_auraUpdateMask = 0;

    duel = nullptr;

    m_GuildIdInvited = 0;
    m_ArenaTeamIdInvited = 0;

    m_atLoginFlags = AT_LOGIN_NONE;

    mSemaphoreTeleport_Near = false;
    mSemaphoreTeleport_Far = false;

    m_DelayedOperations = 0;
    m_bHasDelayedTeleport = false;
    m_bCanDelayTeleport = false;
    m_bHasBeenAliveAtDelayedTeleport =
        true; // overwrite always at setup teleport data, so not used infact
    m_teleport_options = 0;

    trade_ = nullptr;

    m_cinematic = 0;

    PlayerTalkClass = new PlayerMenu(GetSession());

    m_DailyQuestChanged = false;

    m_lastLiquid = nullptr;

    for (auto& elem : m_MirrorTimer)
        elem = DISABLED_MIRROR_TIMER;

    m_MirrorTimerFlags = UNDERWATER_NONE;
    m_MirrorTimerFlagsLast = UNDERWATER_NONE;

    m_isInWater = false;
    m_drunkTimer = 0;
    m_drunk = 0;
    m_restTime = 0;
    m_deathTimer = 0;
    m_deathExpireTime = 0;

    m_swingErrorMsg = 0;

    m_logintime = WorldTimer::time_no_syscall();
    m_Last_tick = m_logintime;
    m_WeaponProficiency = 0;
    m_ArmorProficiency = 0;
    m_canParry = false;
    m_canBlock = false;
    m_canDualWield = false;
    m_ammoDPS = 0.0f;

    m_temporaryUnsummonedPetNumber = 0;

    ////////////////////Rest System/////////////////////
    time_inn_enter = 0;
    inn_trigger_id = 0;
    m_rest_bonus = 0;
    rest_type = REST_TYPE_NO;
    ////////////////////Rest System/////////////////////

    m_mailsUpdated = false;
    unReadMails = 0;
    m_nextMailDelivereTime = 0;

    m_resetTalentsCost = 0;
    m_resetTalentsTime = 0;

    for (auto& elem : m_forced_speed_changes)
        elem = 0;

    m_stableSlots = 0;

    /////////////////// Instance System /////////////////////

    m_HomebindTimer = 0;
    m_InstanceValid = true;
    m_dungeonDifficulty = DUNGEON_DIFFICULTY_NORMAL;

    for (auto& elem : m_auraBaseMod)
    {
        elem[FLAT_MOD] = 0.0f;
        elem[PCT_MOD] = 1.0f;
    }

    for (auto& elem : m_baseRatingValue)
        elem = 0;

    // Honor System
    m_lastHonorUpdateTime = WorldTimer::time_no_syscall();

    // Player summoning
    m_summon_expire = 0;
    m_summon_mapid = 0;
    m_summon_x = 0.0f;
    m_summon_y = 0.0f;
    m_summon_z = 0.0f;

    m_contestedPvPTimer = 0;

    m_declinedname = nullptr;

    m_lastFallTime = 0;
    m_lastFallZ = 0;

    m_timeSyncCounter = 0;
    m_timeSyncTimer = 0;

    // When logging in you seem to always (?) have run mode on
    m_RunModeOn = PrePossessRunMode = true;

    i_AI = nullptr;
    m_AI_locked = false;

    combat_dura_timer_ = urand(sWorld::Instance()->getConfig(
                                   CONFIG_UINT32_DURABILITY_LOSS_COMBAT_MIN) *
                                   1000,
        sWorld::Instance()->getConfig(
            CONFIG_UINT32_DURABILITY_LOSS_COMBAT_MAX) *
            1000);

    move_validator = nullptr;
    gm_fly_mode_ = false;
    in_control_ = true;

    pending_steady_shot = false;

    unroot_hack_ticks_ = 0;

    was_outdoors_ = -1;

    recently_relocated_ = false;

    spying_on_ = 0;

    knockbacked_ = false;
}

Player::~Player()
{
    // it must be unloaded already in PlayerLogout and accessed only for
    // loggined player
    // m_social = NULL;

    CleanupChannels();

    // all mailed items should be deleted, also all mail should be deallocated
    for (PlayerMails::const_iterator itr = m_mail.begin(); itr != m_mail.end();
         ++itr)
        delete *itr;

    for (ItemMap::const_iterator iter = mMitems.begin(); iter != mMitems.end();
         ++iter)
        delete iter->second; // if item is duplicated... then server may crash
                             // ... but that item should be deallocated

    delete PlayerTalkClass;

    for (auto& elem : ItemSetEff)
        if (elem)
            delete elem;

    // clean up player-instance binds, may unload some instance saves
    for (auto& elem : m_instanceBinds)
        for (auto& elem_itr : elem)
            if (auto state = elem_itr.second.state.lock())
                state->UnbindPlayer(this, false);

    delete m_declinedname;

    delete move_validator;

    if (GetSession())
        GetSession()->SetPlayer(nullptr);
}

void Player::CleanupsBeforeDelete()
{
    if (m_uint32Values) // only for fully created Object
    {
        cancel_trade();
        DuelComplete(DUEL_INTERUPTED);
    }

    // notify zone scripts for player logout
    sOutdoorPvPMgr::Instance()->HandlePlayerLeaveZone(this, cached_zone_);

    Unit::CleanupsBeforeDelete();
}

bool Player::Create(uint32 guidlow, const std::string& name, uint8 race,
    uint8 class_, uint8 gender, uint8 skin, uint8 face, uint8 hairStyle,
    uint8 hairColor, uint8 facialHair, uint8 /*outfitId */)
{
    // FIXME: outfitId not used in player creating

    Object::_Create(guidlow, 0, HIGHGUID_PLAYER);

    m_name = name;

    PlayerInfo const* info =
        sObjectMgr::Instance()->GetPlayerInfo(race, class_);
    if (!info)
    {
        logging.error(
            "Player have incorrect race/class pair. Can't be loaded.");
        return false;
    }

    ChrClassesEntry const* cEntry = sChrClassesStore.LookupEntry(class_);
    if (!cEntry)
    {
        logging.error("Class %u not found in DBC (Wrong DBC files?)", class_);
        return false;
    }

    // player store gender in single bit
    if (gender != uint8(GENDER_MALE) && gender != uint8(GENDER_FEMALE))
    {
        logging.error("Invalid gender %u at player creating", uint32(gender));
        return false;
    }

    SetLocationMapId(info->mapId);
    Relocate(info->positionX, info->positionY, info->positionZ);
    SetOrientation(info->orientation);

    SetMap(sMapMgr::Instance()->CreateMap(info->mapId, this));

    uint8 powertype = cEntry->powerType;

    setFactionForRace(race);

    SetByteValue(UNIT_FIELD_BYTES_0, 0, race);
    SetByteValue(UNIT_FIELD_BYTES_0, 1, class_);
    SetByteValue(UNIT_FIELD_BYTES_0, 2, gender);
    SetByteValue(UNIT_FIELD_BYTES_0, 3, powertype);

    InitDisplayIds(); // model, scale and model data

    SetByteValue(UNIT_FIELD_BYTES_2, 1,
        UNIT_BYTE2_FLAG_SANCTUARY | UNIT_BYTE2_FLAG_AURASLOT_SETUP_CHNG);
    SetUInt32Value(UNIT_FIELD_FLAGS, UNIT_FLAG_PVP_ATTACKABLE);
    SetFloatValue(UNIT_MOD_CAST_SPEED,
        1.0f); // fix cast time showed in spell tooltip on client

    SetInt32Value(
        PLAYER_FIELD_WATCHED_FACTION_INDEX, -1); // -1 is default value

    SetByteValue(PLAYER_BYTES, 0, skin);
    SetByteValue(PLAYER_BYTES, 1, face);
    SetByteValue(PLAYER_BYTES, 2, hairStyle);
    SetByteValue(PLAYER_BYTES, 3, hairColor);

    SetByteValue(PLAYER_BYTES_2, 0, facialHair);
    SetByteValue(PLAYER_BYTES_2, 3, 0x02); // rest state = normal

    SetUInt16Value(PLAYER_BYTES_3, 0, gender); // only GENDER_MALE/GENDER_FEMALE
                                               // (1 bit) allowed, drunk state =
                                               // 0
    SetByteValue(PLAYER_BYTES_3, 3, 0); // BattlefieldArenaFaction (0 or 1)

    SetUInt32Value(PLAYER_GUILDID, 0);
    SetUInt32Value(PLAYER_GUILDRANK, 0);
    SetUInt32Value(PLAYER_GUILD_TIMESTAMP, 0);

    SetUInt64Value(PLAYER__FIELD_KNOWN_TITLES, 0); // 0=disabled
    SetUInt32Value(PLAYER_CHOSEN_TITLE, 0);

    SetUInt32Value(PLAYER_FIELD_KILLS, 0);
    SetUInt32Value(PLAYER_FIELD_LIFETIME_HONORBALE_KILLS, 0);
    SetUInt32Value(PLAYER_FIELD_TODAY_CONTRIBUTION, 0);
    SetUInt32Value(PLAYER_FIELD_YESTERDAY_CONTRIBUTION, 0);

    // set starting level
    if (GetSession()->GetSecurity() >= SEC_TICKET_GM)
        SetUInt32Value(UNIT_FIELD_LEVEL,
            sWorld::Instance()->getConfig(CONFIG_UINT32_START_GM_LEVEL));
    else
        SetUInt32Value(UNIT_FIELD_LEVEL,
            sWorld::Instance()->getConfig(CONFIG_UINT32_START_PLAYER_LEVEL));

    SetUInt32Value(PLAYER_FIELD_COINAGE,
        sWorld::Instance()->getConfig(CONFIG_UINT32_START_PLAYER_MONEY));
    SetHonorPoints(
        sWorld::Instance()->getConfig(CONFIG_UINT32_START_HONOR_POINTS));
    SetArenaPoints(
        sWorld::Instance()->getConfig(CONFIG_UINT32_START_ARENA_POINTS));

    // Played time
    m_Last_tick = WorldTimer::time_no_syscall();
    m_Played_time[PLAYED_TIME_TOTAL] = 0;
    m_Played_time[PLAYED_TIME_LEVEL] = 0;

    // base stats and related field values
    InitStatsForLevel();
    InitTaxiNodesForLevel();
    InitTalentForLevel();
    InitPrimaryProfessions(); // to max set before any spell added

    // apply original stats mods before spell loading or item equipment that
    // call before equip _RemoveStatsMods()
    UpdateMaxHealth(); // Update max Health (for add bonus from stamina)
    SetHealth(GetMaxHealth());

    if (getPowerType() == POWER_MANA)
    {
        UpdateMaxPower(
            POWER_MANA); // Update max Mana (for add bonus from intellect)
        SetPower(POWER_MANA, GetMaxPower(POWER_MANA));
    }

    // original spells
    learnDefaultSpells();

    // original action bar
    for (const auto& elem : info->action)
        addActionButton(elem.button, elem.action, elem.type);

    // original items
    uint32 raceClassGender = GetUInt32Value(UNIT_FIELD_BYTES_0) & 0x00FFFFFF;

    CharStartOutfitEntry const* oEntry = nullptr;
    for (uint32 i = 1; i < sCharStartOutfitStore.GetNumRows(); ++i)
    {
        if (CharStartOutfitEntry const* entry =
                sCharStartOutfitStore.LookupEntry(i))
        {
            if (entry->RaceClassGender == raceClassGender)
            {
                oEntry = entry;
                break;
            }
        }
    }

    inventory::transaction trans; // XXX
    std::vector<std::pair<uint32 /*id*/, uint32 /*count*/>>
        delayed_items; // Arrows need to be added after we've equipped the other
                       // items

    if (oEntry)
    {
        for (int j = 0; j < MAX_OUTFIT_ITEMS; ++j)
        {
            if (oEntry->ItemId[j] <= 0)
                continue;

            uint32 item_id = oEntry->ItemId[j];

            // just skip, reported in ObjectMgr::LoadItemPrototypes
            ItemPrototype const* iProto = ObjectMgr::GetItemPrototype(item_id);
            if (!iProto)
                continue;

            // BuyCount by default
            int32 count = iProto->BuyCount;

            // special amount for foor/drink
            if (iProto->Class == ITEM_CLASS_CONSUMABLE &&
                iProto->SubClass == ITEM_SUBCLASS_FOOD)
            {
                switch (iProto->Spells[0].SpellCategory)
                {
                case 11: // food
                    if (iProto->Stackable > 4)
                        count = 4;
                    break;
                case 59: // drink
                    if (iProto->Stackable > 2)
                        count = 2;
                    break;
                }
            }

            if (iProto->InventoryType == INVTYPE_AMMO)
            {
                delayed_items.push_back(std::make_pair(item_id, count));
                continue;
            }

            trans.add(item_id, count);
        }
    }

    for (const auto& elem : info->item)
    {
        const ItemPrototype* proto;
        if ((proto = ObjectMgr::GetItemPrototype(elem.item_id)) != nullptr &&
            proto->InventoryType == INVTYPE_AMMO)
            delayed_items.push_back(
                std::make_pair(elem.item_id, elem.item_amount));
        else
            trans.add(elem.item_id, elem.item_amount);
    }

    // This transaction cannot fail
    storage().finalize(trans);

    // All the items we get when created are put into our inventory, we need to
    // equip them and
    // rearrange the ones we couldn't equip to be in the first available
    // backpack slot
    std::vector<Item*> non_equipped;
    for (int i = inventory::slot_start; i < inventory::slot_end; ++i)
    {
        inventory::slot slot(inventory::personal_slot, inventory::main_bag, i);
        if (Item* item = storage().get(slot))
        {
            inventory::slot target =
                storage().find_auto_equip_slot(item->slot());
            if (!target.valid())
            {
                non_equipped.push_back(item);
                continue;
            }
            storage().swap(target, item->slot());
        }
    }
    for (auto& elem : non_equipped)
    {
        inventory::slot dst = storage().first_empty_slot_for(elem);
        if (dst.valid())
            storage().swap(dst, (elem)->slot());
    }

    inventory::transaction trans_delayed;
    for (auto& delayed_item : delayed_items)
        trans_delayed.add(delayed_item.first, delayed_item.second);
    storage().finalize(trans_delayed); // This transaction cannot fail

    return true;
}

void Player::SendMirrorTimer(
    MirrorTimerType Type, uint32 MaxValue, uint32 CurrentValue, int32 Regen)
{
    if (int(MaxValue) == DISABLED_MIRROR_TIMER)
    {
        if (int(CurrentValue) != DISABLED_MIRROR_TIMER)
            StopMirrorTimer(Type);
        return;
    }
    WorldPacket data(SMSG_START_MIRROR_TIMER, (21));
    data << (uint32)Type;
    data << CurrentValue;
    data << MaxValue;
    data << Regen;
    data << (uint8)0;
    data << (uint32)0; // spell id
    GetSession()->send_packet(std::move(data));
}

void Player::StopMirrorTimer(MirrorTimerType Type)
{
    m_MirrorTimer[Type] = DISABLED_MIRROR_TIMER;
    WorldPacket data(SMSG_STOP_MIRROR_TIMER, 4);
    data << (uint32)Type;
    GetSession()->send_packet(std::move(data));
}

uint32 Player::EnvironmentalDamage(EnvironmentalDamages type, uint32 damage)
{
    if (!isAlive() || isGameMaster())
        return 0;

    // Map type to school mask
    uint32 school_mask;
    switch (type)
    {
    case DAMAGE_LAVA:
    case DAMAGE_FIRE:
        school_mask = SPELL_SCHOOL_MASK_FIRE;
        break;
    case DAMAGE_SLIME:
        school_mask = SPELL_SCHOOL_MASK_NATURE;
        break;
    case DAMAGE_EXHAUSTED:
    case DAMAGE_DROWNING:
    case DAMAGE_FALL:
    case DAMAGE_FALL_TO_VOID:
    default:
        school_mask = SPELL_SCHOOL_MASK_NONE;
        break;
    }

    // Absorb & resist
    uint32 absorb = 0, resist = 0;
    if (school_mask != SPELL_SCHOOL_MASK_NONE)
        damage = do_resist_absorb_helper(
            this, damage, nullptr, false, school_mask, &absorb, &resist);

    DealDamageMods(this, damage, &absorb);

    WorldPacket data(SMSG_ENVIRONMENTALDAMAGELOG, (21));
    data << GetObjectGuid();
    data << uint8(type != DAMAGE_FALL_TO_VOID ? type : DAMAGE_FALL);
    data << uint32(damage);
    data << uint32(absorb);
    data << uint32(resist);
    SendMessageToSet(&data, true);

    uint32 final_damage = DealDamage(this, damage, nullptr, SELF_DAMAGE,
        SPELL_SCHOOL_MASK_NORMAL, nullptr, false, absorb);

    if (type == DAMAGE_FALL &&
        !isAlive()) // DealDamage not apply item durability loss at self damage
    {
        LOG_DEBUG(
            logging, "We are fall to death, loosing 10 percents durability");
        durability(true, -0.1, false);
        // durability lost message
        WorldPacket data2(SMSG_DURABILITY_DAMAGE_DEATH, 0);
        GetSession()->send_packet(std::move(data2));
    }

    return final_damage;
}

int32 Player::getMaxTimer(MirrorTimerType timer)
{
    switch (timer)
    {
    case FATIGUE_TIMER:
        if (GetSession()->GetSecurity() >=
            (AccountTypes)sWorld::Instance()->getConfig(
                CONFIG_UINT32_TIMERBAR_FATIGUE_GMLEVEL))
            return DISABLED_MIRROR_TIMER;
        return sWorld::Instance()->getConfig(
                   CONFIG_UINT32_TIMERBAR_FATIGUE_MAX) *
               IN_MILLISECONDS;
    case BREATH_TIMER:
    {
        if (!isAlive() || HasAuraType(SPELL_AURA_WATER_BREATHING) ||
            GetSession()->GetSecurity() >=
                (AccountTypes)sWorld::Instance()->getConfig(
                    CONFIG_UINT32_TIMERBAR_BREATH_GMLEVEL))
            return DISABLED_MIRROR_TIMER;
        int32 UnderWaterTime =
            sWorld::Instance()->getConfig(CONFIG_UINT32_TIMERBAR_BREATH_MAX) *
            IN_MILLISECONDS;
        const Auras& modWaterBreathing =
            GetAurasByType(SPELL_AURA_MOD_WATER_BREATHING);
        for (const auto& elem : modWaterBreathing)
            UnderWaterTime =
                uint32(UnderWaterTime *
                       (100.0f + (elem)->GetModifier()->m_amount) / 100.0f);
        return UnderWaterTime;
    }
    case FIRE_TIMER:
    {
        if (!isAlive() ||
            GetSession()->GetSecurity() >=
                (AccountTypes)sWorld::Instance()->getConfig(
                    CONFIG_UINT32_TIMERBAR_FIRE_GMLEVEL))
            return DISABLED_MIRROR_TIMER;
        return sWorld::Instance()->getConfig(CONFIG_UINT32_TIMERBAR_FIRE_MAX) *
               IN_MILLISECONDS;
    }
    default:
        return 0;
    }
    return 0;
}

void Player::UpdateMirrorTimers()
{
    // Desync flags for update on next HandleDrowning
    if (m_MirrorTimerFlags)
        m_MirrorTimerFlagsLast = ~m_MirrorTimerFlags;
}

void Player::HandleDrowning(uint32 time_diff)
{
    if (!m_MirrorTimerFlags)
        return;

    // In water
    if (m_MirrorTimerFlags & UNDERWATER_INWATER)
    {
        // Breath timer not activated - activate it
        if (m_MirrorTimer[BREATH_TIMER] == DISABLED_MIRROR_TIMER)
        {
            m_MirrorTimer[BREATH_TIMER] = getMaxTimer(BREATH_TIMER);
            SendMirrorTimer(BREATH_TIMER, m_MirrorTimer[BREATH_TIMER],
                m_MirrorTimer[BREATH_TIMER], -1);
        }
        else
        {
            m_MirrorTimer[BREATH_TIMER] -= time_diff;
            // Timer limit - need deal damage
            if (m_MirrorTimer[BREATH_TIMER] < 0)
            {
                m_MirrorTimer[BREATH_TIMER] += 2 * IN_MILLISECONDS;
                // Calculate and deal damage
                // TODO: Check this formula
                uint32 damage = GetMaxHealth() / 5 + urand(0, getLevel() - 1);
                EnvironmentalDamage(DAMAGE_DROWNING, damage);
            }
            else if (!(m_MirrorTimerFlagsLast &
                         UNDERWATER_INWATER)) // Update time in client if need
                SendMirrorTimer(BREATH_TIMER, getMaxTimer(BREATH_TIMER),
                    m_MirrorTimer[BREATH_TIMER], -1);
        }
    }
    else if (m_MirrorTimer[BREATH_TIMER] !=
             DISABLED_MIRROR_TIMER) // Regen timer
    {
        int32 UnderWaterTime = getMaxTimer(BREATH_TIMER);
        // Need breath regen
        m_MirrorTimer[BREATH_TIMER] += 10 * time_diff;
        if (m_MirrorTimer[BREATH_TIMER] >= UnderWaterTime || !isAlive())
            StopMirrorTimer(BREATH_TIMER);
        else if (m_MirrorTimerFlagsLast & UNDERWATER_INWATER)
            SendMirrorTimer(
                BREATH_TIMER, UnderWaterTime, m_MirrorTimer[BREATH_TIMER], 10);
    }

    // In dark water
    if (m_MirrorTimerFlags & UNDERWATER_INDARKWATER)
    {
        // Fatigue timer not activated - activate it
        if (m_MirrorTimer[FATIGUE_TIMER] == DISABLED_MIRROR_TIMER)
        {
            m_MirrorTimer[FATIGUE_TIMER] = getMaxTimer(FATIGUE_TIMER);
            SendMirrorTimer(FATIGUE_TIMER, m_MirrorTimer[FATIGUE_TIMER],
                m_MirrorTimer[FATIGUE_TIMER], -1);
        }
        else
        {
            m_MirrorTimer[FATIGUE_TIMER] -= time_diff;
            // Timer limit - need deal damage or teleport ghost to graveyard
            if (m_MirrorTimer[FATIGUE_TIMER] < 0)
            {
                m_MirrorTimer[FATIGUE_TIMER] += 2 * IN_MILLISECONDS;
                if (isAlive()) // Calculate and deal damage
                {
                    uint32 damage =
                        GetMaxHealth() / 5 + urand(0, getLevel() - 1);
                    EnvironmentalDamage(DAMAGE_EXHAUSTED, damage);
                }
                else if (HasFlag(PLAYER_FLAGS,
                             PLAYER_FLAGS_GHOST)) // Teleport ghost to graveyard
                    RepopAtGraveyard();
            }
            else if (!(m_MirrorTimerFlagsLast & UNDERWATER_INDARKWATER))
                SendMirrorTimer(FATIGUE_TIMER, getMaxTimer(FATIGUE_TIMER),
                    m_MirrorTimer[FATIGUE_TIMER], -1);
        }
    }
    else if (m_MirrorTimer[FATIGUE_TIMER] !=
             DISABLED_MIRROR_TIMER) // Regen timer
    {
        int32 DarkWaterTime = getMaxTimer(FATIGUE_TIMER);
        m_MirrorTimer[FATIGUE_TIMER] += 10 * time_diff;
        if (m_MirrorTimer[FATIGUE_TIMER] >= DarkWaterTime || !isAlive())
            StopMirrorTimer(FATIGUE_TIMER);
        else if (m_MirrorTimerFlagsLast & UNDERWATER_INDARKWATER)
            SendMirrorTimer(
                FATIGUE_TIMER, DarkWaterTime, m_MirrorTimer[FATIGUE_TIMER], 10);
    }

    if (m_MirrorTimerFlags & (UNDERWATER_INLAVA /*| UNDERWATER_INSLIME*/) &&
        !(m_lastLiquid && m_lastLiquid->SpellId))
    {
        // Breath timer not activated - activate it
        if (m_MirrorTimer[FIRE_TIMER] == DISABLED_MIRROR_TIMER)
            m_MirrorTimer[FIRE_TIMER] = getMaxTimer(FIRE_TIMER);
        else
        {
            m_MirrorTimer[FIRE_TIMER] -= time_diff;
            if (m_MirrorTimer[FIRE_TIMER] < 0)
            {
                m_MirrorTimer[FIRE_TIMER] += 2 * IN_MILLISECONDS;
                // Calculate and deal damage
                // TODO: Check this formula
                uint32 damage = urand(600, 700);
                if (m_MirrorTimerFlags & UNDERWATER_INLAVA)
                    EnvironmentalDamage(DAMAGE_LAVA, damage);
                // else
                // EnvironmentalDamage(DAMAGE_SLIME, damage);
            }
        }
    }
    else
        m_MirrorTimer[FIRE_TIMER] = DISABLED_MIRROR_TIMER;

    // Recheck timers flag
    m_MirrorTimerFlags &= ~UNDERWATER_EXIST_TIMERS;
    for (auto& elem : m_MirrorTimer)
        if (elem != DISABLED_MIRROR_TIMER)
        {
            m_MirrorTimerFlags |= UNDERWATER_EXIST_TIMERS;
            break;
        }
    m_MirrorTimerFlagsLast = m_MirrorTimerFlags;
}

/// The player sobers by 256 every 10 seconds
void Player::HandleSobering()
{
    m_drunkTimer = 0;

    uint32 drunk = (m_drunk <= 256) ? 0 : (m_drunk - 256);
    SetDrunkValue(drunk);
}

DrunkenState Player::GetDrunkenstateByValue(uint16 value)
{
    if (value >= 23000)
        return DRUNKEN_SMASHED;
    if (value >= 12800)
        return DRUNKEN_DRUNK;
    if (value & 0xFFFE)
        return DRUNKEN_TIPSY;
    return DRUNKEN_SOBER;
}

void Player::SetDrunkValue(uint16 newDrunkenValue, uint32 itemId)
{
    uint32 oldDrunkenState = Player::GetDrunkenstateByValue(m_drunk);

    m_drunk = newDrunkenValue;
    SetUInt16Value(PLAYER_BYTES_3, 0, uint16(getGender()) | (m_drunk & 0xFFFE));

    uint32 newDrunkenState = Player::GetDrunkenstateByValue(m_drunk);

    // special drunk invisibility detection
    if (newDrunkenState >= DRUNKEN_DRUNK)
        m_detectInvisibilityMask |= (1 << 6);
    else
        m_detectInvisibilityMask &= ~(1 << 6);

    if (newDrunkenState == oldDrunkenState)
        return;

    WorldPacket data(SMSG_CROSSED_INEBRIATION_THRESHOLD, (8 + 4 + 4));
    data << GetObjectGuid();
    data << uint32(newDrunkenState);
    data << uint32(itemId);

    SendMessageToSet(&data, true);
}

void Player::Update(uint32 update_diff, uint32 p_time)
{
    if (!IsInWorld())
        return;

    if (unlikely(has_queued_actions()))
        update_queued_actions(update_diff);

    // Player::SetPosition needs to do some action after grid relocation:
    if (recently_relocated_)
    {
        if (trade())
            trade()->on_relocation(this);
        UpdateUnderwaterState();
        CheckAreaExploreAndOutdoor();
        recently_relocated_ = false;
    }

    if (unroot_hack_ticks_)
    {
        if (--unroot_hack_ticks_ <= 0)
        {
            WorldPacket data(SMSG_FORCE_MOVE_UNROOT, 8);
            data << GetPackGUID();
            data << uint32(0);
            if (GetSession())
                GetSession()->send_packet(std::move(data));
            unroot_hack_ticks_ = 0;
        }
    }

    // undelivered mail
    if (m_nextMailDelivereTime &&
        m_nextMailDelivereTime <= WorldTimer::time_no_syscall())
    {
        SendNewMail();
        ++unReadMails;

        // It will be recalculate at mailbox open (for unReadMails important
        // non-0 until mailbox open, it also will be recalculated)
        m_nextMailDelivereTime = 0;
    }

    // used to implement delayed far teleports
    SetCanDelayTeleport(true);
    Unit::Update(update_diff, p_time);
    SetCanDelayTeleport(false);

    // update ranged attack timer, main hand and off hand updated in
    // Unit::Update
    if (uint32 ranged_att = getAttackTimer(RANGED_ATTACK))
    {
        setAttackTimer(RANGED_ATTACK,
            (update_diff >= ranged_att ? 0 : ranged_att - update_diff));
    }

    time_t now = WorldTimer::time_no_syscall();

    UpdatePvPFlag(now);

    UpdateContestedPvP(update_diff);

    UpdateDuelFlag(now);

    CheckDuelDistance(now);

    UpdateAfkReport(now);

    // Update items with a limited lifetime
    if (now > m_Last_tick)
        UpdateItemDurations(now - m_Last_tick);

    UpdateEnchDurations(update_diff);

    if (!m_timedquests.empty())
    {
        auto iter = m_timedquests.begin();
        while (iter != m_timedquests.end())
        {
            QuestStatusData& q_status = mQuestStatus[*iter];
            if (q_status.m_timer <= update_diff)
            {
                uint32 quest_id = *iter;
                ++iter; // current iter will be removed in FailQuest
                FailQuest(quest_id);
            }
            else
            {
                q_status.m_timer -= update_diff;
                if (q_status.uState != QUEST_NEW)
                    q_status.uState = QUEST_CHANGED;
                ++iter;
            }
        }
    }

    if (hasUnitState(UNIT_STAT_MELEE_ATTACKING))
    {
        UpdateMeleeAttackingState();

        Unit* pVictim = getVictim();
        if (pVictim && !IsNonMeleeSpellCasted(false))
        {
            Player* vOwner = pVictim->GetCharmerOrOwnerPlayerOrPlayerItself();
            if (vOwner && vOwner->IsPvP() && !IsInDuelWith(vOwner))
            {
                UpdatePvP(true);
                remove_auras_if([](AuraHolder* h)
                    {
                        return h->GetSpellProto()->AuraInterruptFlags &
                               AURA_INTERRUPT_FLAG_ENTER_PVP_COMBAT;
                    });
            }
        }
    }

    if (HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_RESTING))
    {
        if (roll_chance_i(3) && GetTimeInnEnter() > 0) // freeze update
        {
            time_t time_inn = WorldTimer::time_no_syscall() - GetTimeInnEnter();
            if (time_inn >= 10) // freeze update
            {
                float bubble = 0.125f *
                               sWorld::Instance()->getConfig(
                                   CONFIG_FLOAT_RATE_REST_INGAME);
                // speed collect rest bonus (section/in hour)
                SetRestBonus(float(
                    GetRestBonus() +
                    time_inn * (GetUInt32Value(PLAYER_NEXT_LEVEL_XP) / 72000) *
                        bubble));
                UpdateInnerTime(WorldTimer::time_no_syscall());
            }
        }
    }

    if (m_regenTimer)
    {
        if (update_diff >= m_regenTimer)
            m_regenTimer = 0;
        else
            m_regenTimer -= update_diff;
    }

    if (m_timeSyncTimer > 0)
    {
        if (update_diff >= m_timeSyncTimer)
            SendTimeSync();
        else
            m_timeSyncTimer -= update_diff;
    }

    if (isAlive())
    {
        if (isInCombat())
        {
            // In combat our armor deteriorates over time
            if (update_diff >= combat_dura_timer_)
            {
                rand_equip_dura(true);
                combat_dura_timer_ =
                    urand(sWorld::Instance()->getConfig(
                              CONFIG_UINT32_DURABILITY_LOSS_COMBAT_MIN) *
                              1000,
                        sWorld::Instance()->getConfig(
                            CONFIG_UINT32_DURABILITY_LOSS_COMBAT_MAX) *
                            1000);
            }
            else
                combat_dura_timer_ -= update_diff;

            // Keep updating _min_combat_timer; which is reset whenever a PvP
            // action is carried out
            if (_min_combat_timer.GetInterval() != 0 &&
                !_min_combat_timer.Passed())
                _min_combat_timer.Update(update_diff);
        }

        RegenerateAll();
    }

    if (m_deathState == JUST_DIED)
        KillPlayer();

    if (m_nextSave > 0)
    {
        if (update_diff >= m_nextSave)
        {
            // m_nextSave reseted in SaveToDB call
            SaveToDB();
            LOG_DEBUG(logging, "Player '%s' (GUID: %u) saved", GetName(),
                GetGUIDLow());
        }
        else
            m_nextSave -= update_diff;
    }

    // Handle Water/drowning
    HandleDrowning(update_diff);

    // Played time
    if (now > m_Last_tick)
    {
        uint32 elapsed = uint32(now - m_Last_tick);
        m_Played_time[PLAYED_TIME_TOTAL] += elapsed; // Total played time
        m_Played_time[PLAYED_TIME_LEVEL] += elapsed; // Level played time
        m_Last_tick = now;
    }

    if (m_drunk)
    {
        m_drunkTimer += update_diff;

        if (m_drunkTimer > 10 * IN_MILLISECONDS)
            HandleSobering();
    }

    // not auto-free ghost from body in instances
    if (m_deathTimer > 0 && !GetMap()->Instanceable())
    {
        if (p_time >= m_deathTimer)
        {
            m_deathTimer = 0;
            BuildPlayerRepop();
            RepopAtGraveyard();
        }
        else
            m_deathTimer -= p_time;
    }

    UpdateHomebindTime(update_diff);

    // group update
    SendUpdateToOutOfRangeGroupMembers();

    // Get rid off out of range pets and enslaves
    Pet* pet = GetPet();
    if (pet &&
        !pet->IsWithinDistInMap(
            GetCamera().GetBody(), GetMap()->GetVisibilityDistance()))
        pet->Unsummon(pet->getPetType() == HUNTER_PET ? PET_SAVE_AS_CURRENT :
                                                        PET_SAVE_REAGENTS,
            this);
    auto charm = GetCharm();
    if (charm && charm->GetTypeId() == TYPEID_UNIT &&
        !charm->IsWithinDistInMap(this, GetMap()->GetVisibilityDistance()))
        charm->remove_auras(SPELL_AURA_MOD_CHARM);

    if (ShouldExecuteDelayedTeleport())
        TeleportTo(m_teleport_dest, m_teleport_options);

    if (AI() && isAlive() && isCharmed())
    {
        // do not allow the AI to be changed during update
        m_AI_locked = true;
        AI()->UpdateAI(update_diff); // AI not react good at real update delays
                                     // (while freeze in non-active part of map)
        m_AI_locked = false;
    }
}

void Player::SetDeathState(DeathState s)
{
    uint32 ressSpellId = 0;

    bool cur = isAlive();

    if (s == JUST_DIED && cur)
    {
        // drunken state is cleared on death
        SetDrunkValue(0);
        // lost combo points at any target (targeted combo points clear in
        // Unit::SetDeathState)
        ClearComboPoints();

        clearResurrectRequestData();

        if (InBattleGround())
            if (Pet* pet = GetPet())
                SetBgResummonGuid(pet->GetObjectGuid());
        RemovePet(PET_SAVE_NOT_IN_SLOT);

        // remove uncontrolled pets
        RemoveMiniPet();

        // save value before aura remove in Unit::SetDeathState
        ressSpellId = GetUInt32Value(PLAYER_SELF_RES_SPELL);

        // passive spell
        if (!ressSpellId)
            ressSpellId = GetResurrectionSpellId();

        if (InstanceData* mapInstance = GetInstanceData())
            mapInstance->OnPlayerDeath(this);
    }

    Unit::SetDeathState(s);

    // restore resurrection spell id for player after aura remove
    if (s == JUST_DIED && cur && ressSpellId)
        SetUInt32Value(PLAYER_SELF_RES_SPELL, ressSpellId);

    if (isAlive() && !cur)
    {
        // clear aura case after resurrection by another way (spells will be
        // applied before next death)
        SetUInt32Value(PLAYER_SELF_RES_SPELL, 0);
    }
}

bool Player::BuildEnumData(QueryResult* result, WorldPacket* p_data)
{
    //             0               1                2                3
    //             4                  5                       6 7
    //    "SELECT characters.guid, characters.name, characters.race,
    //    characters.class, characters.gender, characters.playerBytes,
    //    characters.playerBytes2, characters.level, "
    //     8                9               10                     11
    //     12                     13                    14
    //    "characters.zone, characters.map, characters.position_x,
    //    characters.position_y, characters.position_z, guild_member.guildid,
    //    characters.playerFlags, "
    //    15                    16                   17                     18
    //    19                         20
    //    "characters.at_login, character_pet.entry, character_pet.modelid,
    //    character_pet.level, characters.equipmentCache,
    //    character_declinedname.genitive "

    Field* fields = result->Fetch();

    uint32 guid = fields[0].GetUInt32();
    uint8 pRace = fields[2].GetUInt8();
    uint8 pClass = fields[3].GetUInt8();

    PlayerInfo const* info =
        sObjectMgr::Instance()->GetPlayerInfo(pRace, pClass);
    if (!info)
    {
        logging.error(
            "Player %u has incorrect race/class pair. Don't build enum.", guid);
        return false;
    }

    *p_data << ObjectGuid(HIGHGUID_PLAYER, guid);
    *p_data << fields[1].GetString();       // name
    *p_data << uint8(pRace);                // race
    *p_data << uint8(pClass);               // class
    *p_data << uint8(fields[4].GetUInt8()); // gender

    uint32 playerBytes = fields[5].GetUInt32();
    *p_data << uint8(playerBytes);       // skin
    *p_data << uint8(playerBytes >> 8);  // face
    *p_data << uint8(playerBytes >> 16); // hair style
    *p_data << uint8(playerBytes >> 24); // hair color

    uint32 playerBytes2 = fields[6].GetUInt32();
    *p_data << uint8(playerBytes2 & 0xFF); // facial hair

    *p_data << uint8(fields[7].GetUInt8());   // level
    *p_data << uint32(fields[8].GetUInt32()); // zone
    *p_data << uint32(fields[9].GetUInt32()); // map

    *p_data << fields[10].GetFloat(); // x
    *p_data << fields[11].GetFloat(); // y
    *p_data << fields[12].GetFloat(); // z

    *p_data << uint32(fields[13].GetUInt32()); // guild id

    uint32 char_flags = 0;
    uint32 playerFlags = fields[14].GetUInt32();
    uint32 atLoginFlags = fields[15].GetUInt32();
    if (playerFlags & PLAYER_FLAGS_HIDE_HELM)
        char_flags |= CHARACTER_FLAG_HIDE_HELM;
    if (playerFlags & PLAYER_FLAGS_HIDE_CLOAK)
        char_flags |= CHARACTER_FLAG_HIDE_CLOAK;
    if (playerFlags & PLAYER_FLAGS_GHOST)
        char_flags |= CHARACTER_FLAG_GHOST;
    if (atLoginFlags & AT_LOGIN_RENAME)
        char_flags |= CHARACTER_FLAG_RENAME;
    if (sWorld::Instance()->getConfig(CONFIG_BOOL_DECLINED_NAMES_USED))
    {
        if (!fields[20].GetCppString().empty())
            char_flags |= CHARACTER_FLAG_DECLINED;
    }
    else
        char_flags |= CHARACTER_FLAG_DECLINED;

    *p_data << uint32(char_flags); // character flags

    // First login
    *p_data << uint8(atLoginFlags & AT_LOGIN_FIRST ? 1 : 0);

    // Pets info
    {
        uint32 petDisplayId = 0;
        uint32 petLevel = 0;
        uint32 petFamily = 0;

        // show pet at selection character in character list only for non-ghost
        // character
        if (result && !(playerFlags & PLAYER_FLAGS_GHOST) &&
            (pClass == CLASS_WARLOCK || pClass == CLASS_HUNTER))
        {
            uint32 entry = fields[16].GetUInt32();
            CreatureInfo const* cInfo =
                sCreatureStorage.LookupEntry<CreatureInfo>(entry);
            if (cInfo)
            {
                petDisplayId = fields[17].GetUInt32();
                petLevel = fields[18].GetUInt32();
                petFamily = cInfo->family;
            }
        }

        *p_data << uint32(petDisplayId);
        *p_data << uint32(petLevel);
        *p_data << uint32(petFamily);
    }

    /*XXX:*/
    Tokens data = StrSplit(fields[19].GetCppString(), " ");
    for (uint8 slot = inventory::equipment_start;
         slot < inventory::equipment_end; slot++)
    {
        uint32 visualbase = slot * 2; // entry, perm ench., temp ench.
        uint32 item_id = GetUInt32ValueFromArray(data, visualbase);
        const ItemPrototype* proto = ObjectMgr::GetItemPrototype(item_id);
        if (!proto)
        {
            *p_data << uint32(0);
            *p_data << uint8(0);
            *p_data << uint32(0);
            continue;
        }

        SpellItemEnchantmentEntry const* enchant = nullptr;

        uint32 enchants = GetUInt32ValueFromArray(data, visualbase + 1);
        for (uint8 enchantSlot = PERM_ENCHANTMENT_SLOT;
             enchantSlot <= TEMP_ENCHANTMENT_SLOT; ++enchantSlot)
        {
            // values stored in 2 uint16
            uint32 enchantId = 0x0000FFFF & (enchants >> enchantSlot * 16);
            if (!enchantId)
                continue;

            if ((enchant = sSpellItemEnchantmentStore.LookupEntry(enchantId)))
                break;
        }

        *p_data << uint32(proto->DisplayInfoID);
        *p_data << uint8(proto->InventoryType);
        *p_data << uint32(enchant ? enchant->aura_id : 0);
    }

    *p_data << uint32(0); // first bag display id
    *p_data << uint8(0);  // first bag inventory type
    *p_data << uint32(0); // enchant?

    return true;
}

void Player::ToggleAFK()
{
    ToggleFlag(PLAYER_FLAGS, PLAYER_FLAGS_AFK);

    // afk player not allowed in battleground
    if (isAFK() && InBattleGround() && !InArena())
        LeaveBattleground();
}

void Player::ToggleDND()
{
    ToggleFlag(PLAYER_FLAGS, PLAYER_FLAGS_DND);
}

uint8 Player::chatTag() const
{
    // it's bitmask
    // 0x1 - afk
    // 0x2 - dnd
    // 0x4 - gm
    // 0x8 - ??

    if (isGMChat()) // Always show GM icons if activated
        return 4;

    if (isAFK())
        return 1;
    if (isDND())
        return 3;

    return 0;
}

bool Player::TeleportTo(
    uint32 mapid, float x, float y, float z, float orientation, uint32 options)
{
    if (!maps::verify_coords(x, y))
    {
        logging.error(
            "TeleportTo: invalid map %d or absent instance template.", mapid);
        return false;
    }

    // preparing unsummon pet if lost (we must get pet before teleportation or
    // will not find it later)
    Pet* pet = GetPet();

    MapEntry const* mEntry = sMapStore.LookupEntry(mapid);

    // don't let enter battlegrounds without assigned battleground id (for
    // example through areatrigger)...
    // don't let gm level > 1 either
    if (!InBattleGround() && mEntry->IsBattleGroundOrArena())
        return false;

    // client without expansion support
    if (GetSession()->Expansion() < mEntry->Expansion())
    {
        LOG_DEBUG(logging,
            "Player %s using client without required expansion tried teleport "
            "to non accessible map %u",
            GetName(), mapid);

        if (Transport* trans = GetTransport())
        {
            trans->RemovePassenger(this);
            RepopAtGraveyard(); // teleport to near graveyard if on transport,
                                // looks blizz like :)
        }

        SendTransferAborted(
            mapid, TRANSFER_ABORT_INSUF_EXPAN_LVL, mEntry->Expansion());

        return false; // normal client can't teleport to this map...
    }
    else
    {
        LOG_DEBUG(logging, "Player %s is being teleported to map %u", GetName(),
            mapid);
    }

    // Unsummon dead pet
    if (pet && pet->isDead())
    {
        pet->Unsummon(PET_SAVE_AS_CURRENT, this);
        SendForcedObjectUpdate(true); // need to update fields before initiate
                                      // telport, or client will bug
    }

    // if we were on a transport, leave
    if (!(options & TELE_TO_NOT_LEAVE_TRANSPORT) && GetTransport())
        GetTransport()->RemovePassenger(this);

    // The player was ported to another map and looses the duel immediately.
    // We have to perform this check before the teleport, otherwise the
    // ObjectAccessor won't find the flag.
    if (duel && GetMapId() != mapid)
        if (GetMap()->GetGameObject(GetGuidValue(PLAYER_DUEL_ARBITER)))
            DuelComplete(DUEL_FLED);

    // reset movement flags at teleport, because player will continue move with
    // these flags after teleport
    m_movementInfo.SetMovementFlags(MOVEFLAG_NONE);
    DisableSpline();

    if (GetMapId() == mapid && !GetTransport())
    {
        // lets reset far teleport flag if it wasn't reset during chained
        // teleports
        SetSemaphoreTeleportFar(false);
        // setup delayed teleport flag
        // if teleport spell is casted in Unit::Update() func
        // then we need to delay it until update process will be finished
        if (SetDelayedTeleportFlagIfCan())
        {
            SetSemaphoreTeleportNear(true);
            // lets save teleport destination for player
            m_teleport_dest = WorldLocation(mapid, x, y, z, orientation);
            m_teleport_options = options;
            // Anti-Cheat: Invalidate next movement packet
            if (move_validator)
                move_validator->ignore_next_packet();
            return true;
        }

        if (!(options & TELE_TO_NOT_UNSUMMON_PET))
        {
            // same map, only remove pet if out of range for new position
            if (pet &&
                !pet->IsWithinDist3d(
                    x, y, z, GetMap()->GetVisibilityDistance()))
                UnsummonPetTemporaryIfAny();
        }

        if (!(options & TELE_TO_NOT_LEAVE_COMBAT))
        {
            CombatStop();
            getHostileRefManager().deleteReferences();
        }

        // this will be used instead of the current location in SaveToDB
        m_teleport_dest = WorldLocation(mapid, x, y, z, orientation);
        SetFallInformation(0, z);

        // code for finish transfer called in
        // WorldSession::HandleMovementOpcodes()
        // at client packet MSG_MOVE_TELEPORT_ACK
        SetSemaphoreTeleportNear(true);
        // near teleport, triggering send MSG_MOVE_TELEPORT_ACK from client at
        // landing
        if (!GetSession()->PlayerLogout())
        {
            WorldPacket data;
            BuildTeleportAckMsg(data, x, y, z, orientation);
            GetSession()->send_packet(std::move(data));
            send_teleport_msg(x, y, z, orientation);
        }
    }
    else
    {
        // far teleport to another map
        Map* oldmap = IsInWorld() ? GetMap() : nullptr;
        // check if we can enter before stopping combat / removing pet / totems
        // / interrupting spells

        // Check enter rights before map getting to avoid creating instance copy
        // for player
        // this check not dependent from map instance copy and same for all
        // instance copies of selected map
        if (!sMapMgr::Instance()->CanPlayerEnter(mapid, this))
            return false;

        // It will be created in the WorldPortAck.
        DungeonPersistentState* state = GetInstanceBindForZoning(mapid);
        Map* map = sMapMgr::Instance()->FindMap(
            mapid, state ? state->GetInstanceId() : 0);

        // If map isn't created yet, check if the dungeon limit has been reached
        // but provide a non existent map
        // that can never be on the recent dungeons list
        if (!map && !isGameMaster() && mEntry->IsDungeon() &&
            IsDungeonLimitReached(0, 0))
        {
            SendTransferAborted(mapid, TRANSFER_ABORT_TOO_MANY_INSTANCES);
            return false;
        }

        // When teleproting to a new map we need to remove loss of control auras
        remove_auras_if([](AuraHolder* holder)
            {
                auto info = holder->GetSpellProto();
                return info->HasApplyAuraName(SPELL_AURA_MOD_FEAR) ||
                       info->HasApplyAuraName(SPELL_AURA_MOD_CONFUSE) ||
                       info->HasApplyAuraName(SPELL_AURA_MOD_POSSESS) ||
                       info->HasApplyAuraName(SPELL_AURA_MOD_CHARM) ||
                       info->HasApplyAuraName(SPELL_AURA_MOD_STUN);
            });

        // If the map is not created, assume it is possible to enter it.
        if (!map || map->CanEnter(this, options & TELE_TO_RESURRECT))
        {
            // remove pet before map change
            auto map_entry = sMapStore.LookupEntry(mapid);
            if (pet)
            {
                // If the target map is an arena it's a permanent unsummon
                if (map_entry && map_entry->IsBattleArena())
                {
                    // Max the pet's health before we desummon it (so it
                    // respawns with full hp)
                    pet->SetHealth(pet->GetMaxHealth());
                    pet->Unsummon(PET_SAVE_NOT_IN_SLOT, this);
                }
                else
                {
                    UnsummonPetTemporaryIfAny();
                }
                SendForcedObjectUpdate(
                    true); // force update to avoid visual bugs in client UI
            }

            // clear temporary unsummoned pets if target map is an arena
            if (map_entry && map_entry->IsBattleArena())
                m_temporaryUnsummonedPetNumber = 0;

            // lets reset near teleport flag if it wasn't reset during chained
            // teleports
            SetSemaphoreTeleportNear(false);

            // setup delayed teleport flag
            // if teleport spell is casted in Unit::Update() func
            // then we need to delay it until update process will be finished
            if (SetDelayedTeleportFlagIfCan() && !GetSession()->PlayerLogout())
            {
                // NOTE: We don't want to do this when logging out, since we
                // wouldn't do oldmap->erase(this, false); then. (Possible
                // crash if we did)
                SetSemaphoreTeleportFar(true);
                // lets save teleport destination for player
                m_teleport_dest = WorldLocation(mapid, x, y, z, orientation);
                m_teleport_options = options;
                return true;
            }

            SetSelectionGuid(ObjectGuid());

            CombatStop();
            getHostileRefManager().deleteReferences();

            ClearComboPoints();

            ResetContestedPvP();

            // remove player from battleground on far teleport (when changing
            // maps)
            if (BattleGround const* bg = GetBattleGround())
            {
                // Note: at battleground join battleground id set before
                // teleport
                // and we already will found "current" battleground
                // just need check that this is targeted map or leave
                if (bg->GetMapId() != mapid)
                    LeaveBattleground(false); // don't teleport to entry point
            }

            // remove all dyn objects
            RemoveAllDynObjects();

            // stop spellcasting
            // not attempt interrupt teleportation spell at caster teleport
            if (!(options & TELE_TO_SPELL))
                if (IsNonMeleeSpellCasted(true))
                    InterruptNonMeleeSpells(true);

            // remove auras before removing from map...
            remove_auras_if([](AuraHolder* h)
                {
                    return h->GetSpellProto()->AuraInterruptFlags &
                           (AURA_INTERRUPT_FLAG_CHANGE_MAP |
                               AURA_INTERRUPT_FLAG_MOVE |
                               AURA_INTERRUPT_FLAG_TURNING);
                });

            if (!GetSession()->PlayerLogout())
            {
                // send transfer packet to display load screen
                WorldPacket data(SMSG_TRANSFER_PENDING, (4 + 4 + 4));
                data << uint32(mapid);
                if (Transport* trans = GetTransport())
                {
                    data << uint32(trans->GetEntry());
                    data << uint32(GetMapId());
                }
                GetSession()->send_packet(std::move(data));
            }

            // remove from old map now
            if (oldmap)
                oldmap->erase(this, false);

            m_teleport_dest = WorldLocation(mapid, x, y, z, orientation);
            SetFallInformation(0, z);
            // if the player is saved before worldport ack (at logout for
            // example)
            // this will be used instead of the current location in SaveToDB

            // move packet sent by client always after far teleport
            // code for finish transfer to new map called in
            // WorldSession::HandleMoveWorldportAckOpcode at client packet
            SetSemaphoreTeleportFar(true);

            if (!GetSession()->PlayerLogout())
            {
                // transfer finished, inform client to start load
                WorldPacket data(SMSG_NEW_WORLD, 20);
                data << uint32(mapid);
                if (GetTransport())
                {
                    data << float(m_movementInfo.transport.pos.x);
                    data << float(m_movementInfo.transport.pos.y);
                    data << float(m_movementInfo.transport.pos.z);
                    data << float(m_movementInfo.transport.pos.o);
                }
                else
                {
                    data << float(m_teleport_dest.coord_x);
                    data << float(m_teleport_dest.coord_y);
                    data << float(m_teleport_dest.coord_z);
                    data << float(m_teleport_dest.orientation);
                }

                GetSession()->send_packet(std::move(data));
                SendSavedInstances();
            }
        }
        else
            return false;
    }
    // Anti-Cheat: Invalidate next movement packet
    if (move_validator)
        move_validator->ignore_next_packet();
    return true;
}

bool Player::TeleportToBGEntryPoint()
{
    bool res = TeleportTo(m_bgData.joinPos);
    return res;
}

void Player::ProcessDelayedOperations()
{
    if (m_DelayedOperations == 0)
        return;

    if (m_DelayedOperations & DELAYED_RESURRECT_PLAYER)
    {
        ResurrectPlayer(0.0f, false);

        if (GetMaxHealth() > m_resurrectHealth)
            SetHealth(m_resurrectHealth);
        else
            SetHealth(GetMaxHealth());

        if (GetMaxPower(POWER_MANA) > m_resurrectMana)
            SetPower(POWER_MANA, m_resurrectMana);
        else
            SetPower(POWER_MANA, GetMaxPower(POWER_MANA));

        SetPower(POWER_RAGE, 0);
        SetPower(POWER_ENERGY, GetMaxPower(POWER_ENERGY));

        SpawnCorpseBones();
    }

    if (m_DelayedOperations & DELAYED_SAVE_PLAYER)
    {
        SaveToDB();
    }

    if (m_DelayedOperations & DELAYED_SPELL_CAST_DESERTER)
    {
        CastSpell(this, 26013, true); // Deserter
    }

    // we have executed ALL delayed ops, so clear the flag
    m_DelayedOperations = 0;
}

void Player::AddToWorld()
{
    bool was_in_world = IsInWorld();

    ///- Do not add/remove the player from the object storage
    ///- It will crash when updating the ObjectAccessor
    ///- The player should only be added when logging in
    Unit::AddToWorld();

    if (!was_in_world)
    {
        was_outdoors_ = -1;
        sObjectAccessor::Instance()->OnPlayerAddToWorld(GetTeam());

        // Cancel stealth/invis when logging in & added to world
        remove_auras(SPELL_AURA_MOD_STEALTH);
        remove_auras(SPELL_AURA_MOD_INVISIBILITY);
    }

    /*XXX:*/
    const int mask = inventory::personal_storage::iterator::all;
    for (inventory::personal_storage::iterator itr = storage().begin(mask);
         itr != storage().end(); ++itr)
        (*itr)->AddToWorld();
}

void Player::RemoveFromWorld()
{
    // cleanup
    if (IsInWorld())
    {
        sObjectAccessor::Instance()->OnPlayerRemoveFromWorld(GetTeam());
        ///- Release charmed creatures, unsummon totems and remove
        /// pets/guardians
        UnsummonAllTotems();
        RemoveMiniPet();
    }

    /*XXX:*/
    const int mask = inventory::personal_storage::iterator::all;
    for (inventory::personal_storage::iterator itr = storage().begin(mask);
         itr != storage().end(); ++itr)
        (*itr)->RemoveFromWorld();

    // remove duel before calling Unit::RemoveFromWorld
    // otherwise there will be an existing duel flag pointer but no entry in
    // m_gameObj
    DuelComplete(DUEL_INTERUPTED);

    ///- Do not add/remove the player from the object storage
    ///- It will crash when updating the ObjectAccessor
    ///- The player should only be removed when logging out
    if (IsInWorld())
        GetCamera().ResetView();

    if (GetSession())
        GetSession()->invalidate_warden_dynamic(WARDEN_DYN_CHECK_PLAYER_BASE);

    Unit::RemoveFromWorld();
}

/* MeleeHitOutcome hitResult & WeaponAttackType weaponType*/
void Player::RewardRage(
    uint32 d, uint32 hitResult, uint32 weaponType, bool attacker)
{
    // The rage calculation formula is devided from a wowwiki history page dated
    // 23:02 june 2, 2008
    /* and confirmed by a bluepost found through webarchive
     * R:  rage generated
     * d:  damage amount
     * c:  rage conversion value
     * s:  weapon speed
     * f:  hit factor
     *
     * Dealing Damage: R = (7.5d / c + fs) / 2
     * Taking Damage: R = 2.5d / c
     *
     * Hit factor:
     * Main Hand, Normal Hit: 3.5
     * Main Hand, Critical Hit: 7.0
     * Off Hand, Normal Hit: 1.75
     * Off Hand, Critical Hit: 3.5
     *
     * Rage Conversion Value:
     * c = 0.0091107836*Level*Level + 3.225598133*Level + 4.2652911
    */

    // Ranged Attacks generate no rage (Source: Testing on retail)
    if (weaponType == RANGED_ATTACK)
        return;

    bool is_crit = (hitResult == MELEE_HIT_CRIT);
    float level = getLevel();

    // Rage conversion value
    float c = 0.0091107836 * level * level + 3.225598133 * level + 4.2652911;

    float R = 0;

    if (attacker)
    {
        // Hit factor
        float f = 0;
        if (weaponType == BASE_ATTACK)
            f = (!is_crit) ? 3.5f : 7.0f;
        else
            f = (!is_crit) ? 1.75f : 3.5f;

        // Weapon Speed
        float s = GetAttackTime((WeaponAttackType)weaponType) / 1000.0f;

        R = (7.5f * d / c + f * s) / 2;

        // Endles Rage (only spell that uses this aura) increases rage
        // generation:
        float mod = 1.0f + (GetTotalAuraModifier(
                                SPELL_AURA_MOD_RAGE_FROM_DAMAGE_DEALT) /
                               100.0f);
        R *= mod;
    }
    else
    {
        R = 2.5f * d / c;

        // ElitistJerks had the value pinpointed at times 3 in WotlK, have not
        // found references
        // for TBC, but it sounds reasonable that it's the same.
        if (has_aura(18499))
            R *= 3;
    }

    R *= sWorld::Instance()->getConfig(CONFIG_FLOAT_RATE_POWER_RAGE_INCOME);

    // Rage is times 10 to allow for decimal precision in an integer
    R *= 10;

    ModifyPower(POWER_RAGE, int32(R));
}

void Player::RegenerateAll()
{
    if (m_regenTimer != 0)
        return;

    // Not in combat or they have regeneration
    if (!isInCombat() || HasAuraType(SPELL_AURA_MOD_REGEN_DURING_COMBAT) ||
        HasAuraType(SPELL_AURA_MOD_HEALTH_REGEN_IN_COMBAT))
    {
        RegenerateHealth();
        if (!isInCombat() && !HasAuraType(SPELL_AURA_INTERRUPT_REGEN))
            Regenerate(POWER_RAGE);
    }

    Regenerate(POWER_ENERGY);

    Regenerate(POWER_MANA);

    // Leave combat if possible
    if (isInCombat())
    {
        if (getHostileRefManager().isEmpty())
        {
            // _min_combat_timer updated in Player::Update
            if (_min_combat_timer.GetInterval() == 0 ||
                _min_combat_timer.Passed())
            {
                ClearInCombat();

                CallForAllControlledUnits(
                    [](auto controlled)
                    {
                        if (!controlled->CanHaveThreatList() &&
                            controlled->getHostileRefManager().isEmpty())
                            controlled->ClearInCombat();
                    },
                    CONTROLLED_PET | CONTROLLED_GUARDIANS | CONTROLLED_CHARM);
            }
        }
    }

    m_regenTimer = REGEN_TIME_FULL;
}

void Player::Regenerate(Powers power)
{
    uint32 curValue = GetPower(power);
    uint32 maxValue = GetMaxPower(power);

    float addvalue = 0.0f;

    switch (power)
    {
    case POWER_MANA:
    {
        bool recentCast = IsUnderLastManaUseEffect();
        float ManaIncreaseRate =
            sWorld::Instance()->getConfig(CONFIG_FLOAT_RATE_POWER_MANA);
        if (recentCast)
        {
            // Mangos Updates Mana in intervals of 2s, which is correct
            if (hasUnitState(UNIT_STAT_STUNNED) &&
                HasAuraWithMechanic(1 << (MECHANIC_BANISH - 1)))
                addvalue = CalcCyclonedRegen().second * ManaIncreaseRate * 2.0f;
            else
                addvalue =
                    GetFloatValue(PLAYER_FIELD_MOD_MANA_REGEN_INTERRUPT) *
                    ManaIncreaseRate * 2.0f;
        }
        else
        {
            if (hasUnitState(UNIT_STAT_STUNNED) &&
                HasAuraWithMechanic(1 << (MECHANIC_BANISH - 1)))
                addvalue = CalcCyclonedRegen().first * ManaIncreaseRate * 2.0f;
            else
                addvalue = GetFloatValue(PLAYER_FIELD_MOD_MANA_REGEN) *
                           ManaIncreaseRate * 2.0f;
        }

        // Aspect of the Viper
        if (getClass() == CLASS_HUNTER &&
            has_aura(34074, SPELL_AURA_PERIODIC_DUMMY))
        {
            // Formula is:
            // viper_mp5 =
            //     intellect * 22/35 * (0.9 - mana / maxmana) +
            //     intellect * 0.11 +
            //     0.35 * level

            float mana = GetPower(POWER_MANA);
            float max_mana = GetMaxPower(POWER_MANA);
            float intellect = GetStat(STAT_INTELLECT);
            float level = getLevel();

            int32 mp5 = intellect * 22.0f / 35.0f * (0.9f - mana / max_mana) +
                        intellect * 0.11f + 0.35f * level;

            // Extra 5% of intellect from tier 6 2piece bonus
            if (has_aura(38390, SPELL_AURA_DUMMY))
                mp5 += intellect * 0.05f;

            addvalue += mp5 * 2.0f / 5.0f;
        }
    }
    break;
    case POWER_RAGE: // Regenerate rage
    {
        float RageDecreaseRate =
            sWorld::Instance()->getConfig(CONFIG_FLOAT_RATE_POWER_RAGE_LOSS);
        addvalue =
            20 * RageDecreaseRate; // 2 rage by tick (= 2 seconds => 1 rage/sec)
    }
    break;
    case POWER_ENERGY: // Regenerate energy (rogue)
    {
        float EnergyRate =
            sWorld::Instance()->getConfig(CONFIG_FLOAT_RATE_POWER_ENERGY);
        addvalue = 20 * EnergyRate;
        break;
    }
    case POWER_FOCUS:
    case POWER_HAPPINESS:
    case POWER_HEALTH:
        break;
    default:
        break;
    }

    // Mana regen calculated in Player::UpdateManaRegen()
    // Exist only for POWER_MANA, POWER_ENERGY, POWER_FOCUS auras
    if (power != POWER_MANA)
    {
        const Auras& modPowerRegenPct =
            GetAurasByType(SPELL_AURA_MOD_POWER_REGEN_PERCENT);
        for (const auto& elem : modPowerRegenPct)
            if ((elem)->GetModifier()->m_miscvalue == int32(power))
                addvalue *= ((elem)->GetModifier()->m_amount + 100) / 100.0f;
    }

    if (power != POWER_RAGE)
    {
        curValue += uint32(addvalue);
        if (curValue > maxValue)
            curValue = maxValue;
    }
    else
    {
        if (curValue <= uint32(addvalue))
            curValue = 0;
        else
            curValue -= uint32(addvalue);
    }
    SetPower(power, curValue);
}

void Player::RegenerateHealth()
{
    uint32 curValue = GetHealth();
    uint32 maxValue = GetMaxHealth();

    if (curValue >= maxValue)
        return;

    float HealthIncreaseRate =
        sWorld::Instance()->getConfig(CONFIG_FLOAT_RATE_HEALTH);

    float addvalue = 0.0f;

    if (!isInCombat() || HasAuraType(SPELL_AURA_MOD_REGEN_DURING_COMBAT))
    {
        addvalue = OCTRegenHPPerSpirit() * HealthIncreaseRate;
        if (!isInCombat())
        {
            const Auras& modHealingPct =
                GetAurasByType(SPELL_AURA_MOD_HEALTH_REGEN_PERCENT);
            for (const auto& elem : modHealingPct)
                addvalue *= (100.0f + (elem)->GetModifier()->m_amount) / 100.0f;
        }
        else if (HasAuraType(SPELL_AURA_MOD_REGEN_DURING_COMBAT))
            addvalue *=
                GetTotalAuraModifier(SPELL_AURA_MOD_REGEN_DURING_COMBAT) /
                100.0f;

        if (!IsStandState())
            addvalue *= 1.33;

        // Regeneration from food buffs (the dbc value is regen per 5 seconds)
        if (!isInCombat())
        {
            auto& al = GetAurasByType(SPELL_AURA_MOD_REGEN);
            for (const auto& elem : al)
                addvalue += (elem)->GetModifier()->m_amount * (2.0f / 5.0f);
        }
    }

    // SPELL_AURA_MOD_HEALTH_REGEN_IN_COMBAT is per 5 second
    addvalue += GetTotalAuraModifier(SPELL_AURA_MOD_HEALTH_REGEN_IN_COMBAT) *
                (2.0f / 5.0f);

    if (addvalue < 0)
        addvalue = 0;

    ModifyHealth(int32(addvalue));
}

Creature* Player::GetNPCIfCanInteractWith(ObjectGuid guid, uint32 npcflagmask)
{
    // some basic checks
    if (!guid || !IsInWorld() || IsTaxiFlying())
        return nullptr;

    // not in interactive state
    if (hasUnitState(UNIT_STAT_CAN_NOT_REACT_OR_LOST_CONTROL))
        return nullptr;

    // exist (we need look pets also for some interaction (quest/etc)
    Creature* unit = GetMap()->GetAnyTypeCreature(guid);
    if (!unit)
        return nullptr;

    // appropriate npc type
    if (npcflagmask && !unit->HasFlag(UNIT_NPC_FLAGS, npcflagmask))
        return nullptr;

    if (npcflagmask == UNIT_NPC_FLAG_STABLEMASTER)
    {
        if (getClass() != CLASS_HUNTER)
            return nullptr;
    }

    // if a dead unit should be able to talk - the creature must be alive and
    // have special flags
    if (!unit->isAlive())
        return nullptr;

    if (isAlive() && unit->isInvisibleForAlive())
        return nullptr;

    // not allow interaction under control, but allow with own pets
    if (unit->GetCharmerGuid())
        return nullptr;

    // not enemy
    if (unit->IsHostileTo(this))
        return nullptr;

    // not too far
    if (!unit->IsWithinDistInMap(this, INTERACTION_DISTANCE))
        return nullptr;

    return unit;
}

GameObject* Player::GetGameObjectIfCanInteractWith(
    ObjectGuid guid, uint32 gameobject_type) const
{
    // some basic checks
    if (!guid || !IsInWorld() || IsTaxiFlying())
        return nullptr;

    // not in interactive state
    if (hasUnitState(UNIT_STAT_CAN_NOT_REACT_OR_LOST_CONTROL))
        return nullptr;

    if (GameObject* go = GetMap()->GetGameObject(guid))
    {
        if (uint32(go->GetGoType()) == gameobject_type ||
            gameobject_type == MAX_GAMEOBJECT_TYPE)
        {
            float maxdist;
            switch (go->GetGoType())
            {
            // TODO: find out how the client calculates the maximal usage
            // distance to spellless working
            // gameobjects like guildbanks and mailboxes - 10.0 is a just an
            // abitrary choosen number
            case GAMEOBJECT_TYPE_GUILD_BANK:
            case GAMEOBJECT_TYPE_MAILBOX:
                maxdist = 10.0f;
                break;
            case GAMEOBJECT_TYPE_FISHINGHOLE:
                maxdist = 20.0f + CONTACT_DISTANCE; // max spell range
                break;
            default:
                maxdist = INTERACTION_DISTANCE + (go->GetGOInfo()->size - 1.0f);
                break;
            }

            if (go->IsWithinDistInMap(this, maxdist) && go->isSpawned())
                return go;

            logging.error(
                "GetGameObjectIfCanInteractWith: GameObject '%s' [GUID: %u] is "
                "too far away from player %s [GUID: %u] to be used by him "
                "(distance=%f, maximal %f is allowed)",
                go->GetGOInfo()->name, go->GetGUIDLow(), GetName(),
                GetGUIDLow(), go->GetDistance(this), maxdist);
        }
    }
    return nullptr;
}

bool Player::IsUnderWater() const
{
    return GetTerrain()->IsUnderWater(GetX(), GetY(), GetZ() + 2);
}

void Player::SetInWater(bool apply)
{
    if (m_isInWater == apply)
        return;

    // define player in water by opcodes
    // move player's guid into HateOfflineList of those mobs
    // which can't swim and move guid back into ThreatList when
    // on surface.
    // TODO: exist also swimming mobs, and function must be symmetric to
    // enter/leave water
    m_isInWater = apply;

    // remove auras that need water/land
    remove_auras_if([apply](AuraHolder* h)
        {
            return h->GetSpellProto()->AuraInterruptFlags &
                   (apply ? AURA_INTERRUPT_FLAG_NOT_ABOVEWATER :
                            AURA_INTERRUPT_FLAG_NOT_UNDERWATER);
        });

    getHostileRefManager().updateThreatTables();
}

struct SetGameMasterOnHelper
{
    explicit SetGameMasterOnHelper() {}
    void operator()(Unit* unit) const
    {
        unit->setFaction(35);
        unit->getHostileRefManager().setOnlineOfflineState(false);
    }
};

struct SetGameMasterOffHelper
{
    explicit SetGameMasterOffHelper(uint32 _faction) : faction(_faction) {}
    void operator()(Unit* unit) const
    {
        unit->setFaction(faction);
        unit->getHostileRefManager().setOnlineOfflineState(true);
    }
    uint32 faction;
};

void Player::SetGameMaster(bool on)
{
    if (on)
    {
        m_ExtraFlags |= PLAYER_EXTRA_GM_ON;
        setFaction(35);
        SetFlag(PLAYER_FLAGS, PLAYER_FLAGS_GM);

        CallForAllControlledUnits(SetGameMasterOnHelper(),
            CONTROLLED_PET | CONTROLLED_TOTEMS | CONTROLLED_GUARDIANS |
                CONTROLLED_CHARM);

        SetFFAPvP(false);
        ResetContestedPvP();

        getHostileRefManager().setOnlineOfflineState(false);
        CombatStopWithPets();
    }
    else
    {
        m_ExtraFlags &= ~PLAYER_EXTRA_GM_ON;
        setFactionForRace(getRace());
        RemoveFlag(PLAYER_FLAGS, PLAYER_FLAGS_GM);

        CallForAllControlledUnits(SetGameMasterOffHelper(getFaction()),
            CONTROLLED_PET | CONTROLLED_TOTEMS | CONTROLLED_GUARDIANS |
                CONTROLLED_CHARM);

        // restore FFA PvP Server state
        if (sWorld::Instance()->IsFFAPvPRealm())
            SetFFAPvP(true);

        // restore FFA PvP area state, remove not allowed for GM mounts
        UpdateArea(cached_area_);

        getHostileRefManager().setOnlineOfflineState(true);
    }

    m_camera.UpdateVisibilityForOwner();
    UpdateObjectVisibility();
    UpdateForQuestWorldObjects();
}

void Player::SetGMVisible(bool on)
{
    if (on)
    {
        m_ExtraFlags &= ~PLAYER_EXTRA_GM_INVISIBLE; // remove flag

        // Reapply stealth/invisibility if active or show if not any
        if (HasAuraType(SPELL_AURA_MOD_STEALTH))
            SetVisibility(VISIBILITY_GROUP_STEALTH);
        else if (HasAuraType(SPELL_AURA_MOD_INVISIBILITY))
            SetVisibility(VISIBILITY_GROUP_INVISIBILITY);
        else
            SetVisibility(VISIBILITY_ON);
    }
    else
    {
        m_ExtraFlags |= PLAYER_EXTRA_GM_INVISIBLE; // add flag

        SetGameMaster(true);

        SetVisibility(VISIBILITY_OFF);
    }
}

bool Player::IsGroupVisibleFor(Player* p) const
{
    switch (sWorld::Instance()->getConfig(CONFIG_UINT32_GROUP_VISIBILITY))
    {
    default:
        return IsInSameGroupWith(p);
    case 1:
        return IsInSameRaidWith(p);
    case 2:
        return GetTeam() == p->GetTeam();
    }
}

bool Player::IsInSameGroupWith(Player const* p) const
{
    return (
        p == this || (GetGroup() != nullptr &&
                         GetGroup()->SameSubGroup((Player*)this, (Player*)p)));
}

///- If the player is invited, remove him. If the group if then only 1 person,
/// disband the group.
/// \todo Shouldn't we also check if there is no other invitees before
/// disbanding the group?
void Player::UninviteFromGroup()
{
    Group* group = GetGroupInvite();
    if (!group)
        return;

    group->RemoveInvite(this);

    if (group->GetMembersCount() <= 1) // group has just 1 member => disband
    {
        if (group->IsCreated())
        {
            group->Disband(true);
            sObjectMgr::Instance()->RemoveGroup(group);
        }
        else
            group->RemoveAllInvites();

        delete group;
    }
}

void Player::RemoveFromGroup(Group* group, ObjectGuid guid)
{
    if (group)
    {
        if (group->RemoveMember(guid, 0) <= 1)
        {
            // group->Disband(); already disbanded in RemoveMember
            sObjectMgr::Instance()->RemoveGroup(group);
            delete group;
            // removemember sets the player's group pointer to NULL
        }
    }
}

void Player::SendLogXPGain(
    uint32 GivenXP, Unit* victim, uint32 RestXP, float grp_coeff)
{
    WorldPacket data(SMSG_LOG_XPGAIN, 21);
    data << (victim ? victim->GetObjectGuid() : ObjectGuid()); // guid
    data << uint32(GivenXP + RestXP); // given experience
    data << uint8(victim ? 0 : 1);    // 00-kill_xp type, 01-non_kill_xp type
    if (victim)
    {
        data << uint32(GivenXP); // experience without rested bonus
        data << float(grp_coeff);
    }
    data << uint8(0); // new 2.4.0
    GetSession()->send_packet(std::move(data));
}

void Player::GiveXP(uint32 xp, Unit* victim, float grp_coeff)
{
    if (xp < 1)
        return;

    if (!isAlive())
        return;

    uint32 level = getLevel();

    // XP to money conversion processed in Player::RewardQuest
    if (level >= sWorld::Instance()->getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL))
        return;

    // XP is reduced if high-levels outside of our group helped kill the mob
    if (victim && victim->GetTypeId() == TYPEID_UNIT)
    {
        auto cv = static_cast<Creature*>(victim);
        auto legit = cv->legit_dmg_taken;
        auto total = cv->total_dmg_taken;

        // Prevent pet classes getting XP from pet doing all the damage
        if (!HasTapOn(cv))
            return;

        if (total > 0)
        {
            auto pct = (float)legit / (float)total;
            if (pct < 0.1f)
                xp *= 0.1f;
            else if (pct < 0.7f)
                xp *= pct;
            if (xp < 1)
                xp = 1;
        }
    }

    // handle SPELL_AURA_MOD_XP_PCT auras
    const Unit::Auras& modXpPct = GetAurasByType(SPELL_AURA_MOD_XP_PCT);
    for (const auto& elem : modXpPct)
        xp = uint32(xp * (1.0f + (elem)->GetModifier()->m_amount / 100.0f));

    // XP resting bonus for kill
    uint32 rested_bonus_xp = victim ? GetXPRestBonus(xp) : 0;

    SendLogXPGain(xp, victim, rested_bonus_xp, grp_coeff);

    uint32 curXP = GetUInt32Value(PLAYER_XP);
    uint32 nextLvlXP = GetUInt32Value(PLAYER_NEXT_LEVEL_XP);
    uint32 newXP = curXP + xp + rested_bonus_xp;

    while (
        newXP >= nextLvlXP &&
        level < sWorld::Instance()->getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL))
    {
        newXP -= nextLvlXP;

        if (level <
            sWorld::Instance()->getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL))
            GiveLevel(level + 1);

        level = getLevel();
        nextLvlXP = GetUInt32Value(PLAYER_NEXT_LEVEL_XP);
    }

    SetUInt32Value(PLAYER_XP, newXP);
}

// Update player to next level
// Current player experience not update (must be update by caller)
void Player::GiveLevel(uint32 level)
{
    if (level == getLevel())
        return;

    PlayerLevelInfo info;
    sObjectMgr::Instance()->GetPlayerLevelInfo(
        getRace(), getClass(), level, &info);

    PlayerClassLevelInfo classInfo;
    sObjectMgr::Instance()->GetPlayerClassLevelInfo(
        getClass(), level, &classInfo);

    // send levelup info to client
    WorldPacket data(
        SMSG_LEVELUP_INFO, (4 + 4 + MAX_POWERS * 4 + MAX_STATS * 4));
    data << uint32(level);
    auto hp_pos = data.wpos();
    data << uint32(0); // health increase
    auto mp_pos = data.wpos();
    data << uint32(0); // mana increase

    // Other powers?
    data << uint32(0);
    data << uint32(0);
    data << uint32(0);
    data << uint32(0);

    int32 hp = int32(classInfo.basehealth) - int32(GetCreateHealth());
    int32 mp = int32(classInfo.basemana) - int32(GetCreateMana());

    for (int i = STAT_STRENGTH; i < MAX_STATS; ++i) // Stats loop (0-4)
    {
        int32 stat = int32(info.stats[i]) - GetCreateStat(Stats(i));

        if (i == STAT_STAMINA)
            hp += stat * 10;
        else if (i == STAT_INTELLECT && mp != 0)
            mp += stat * 15;

        data << uint32(stat);
    }

    // Health and mana increase shown includes stamina and intellect
    data.put(hp_pos, hp);
    data.put(mp_pos, mp);

    GetSession()->send_packet(std::move(data));

    SetUInt32Value(
        PLAYER_NEXT_LEVEL_XP, sObjectMgr::Instance()->GetXPForLevel(level));

    // update level, max level of skills
    if (getLevel() != level)
        m_Played_time[PLAYED_TIME_LEVEL] = 0; // Level Played Time reset
    SetLevel(level);
    UpdateSkillsForLevel();

    // save base values (bonuses already included in stored stats
    for (int i = STAT_STRENGTH; i < MAX_STATS; ++i)
        SetCreateStat(Stats(i), info.stats[i]);

    SetCreateHealth(classInfo.basehealth);
    SetCreateMana(classInfo.basemana);

    InitTalentForLevel();
    InitTaxiNodesForLevel();

    UpdateAllStats();

    // set current level health and mana/energy to maximum after applying all
    // mods.
    if (isAlive())
        SetHealth(GetMaxHealth());
    SetPower(POWER_MANA, GetMaxPower(POWER_MANA));
    SetPower(POWER_ENERGY, GetMaxPower(POWER_ENERGY));
    if (GetPower(POWER_RAGE) > GetMaxPower(POWER_RAGE))
        SetPower(POWER_RAGE, GetMaxPower(POWER_RAGE));
    SetPower(POWER_FOCUS, 0);
    SetPower(POWER_HAPPINESS, 0);

    // update level to hunter/summon pet
    if (Pet* pet = GetPet())
        pet->SynchronizeLevelWithOwner();

    if (MailLevelReward const* mailReward =
            sObjectMgr::Instance()->GetMailLevelReward(level, getRaceMask()))
        MailDraft(mailReward->mailTemplateId)
            .SendMailTo(
                this, MailSender(MAIL_CREATURE, mailReward->senderEntry));
}

void Player::UpdateFreeTalentPoints(bool resetIfNeed)
{
    uint32 level = getLevel();
    // talents base at level diff ( talents = level - 9 but some can be used
    // already)
    if (level < 10)
    {
        // Remove all talent points
        if (m_usedTalentCount > 0) // Free any used talents
        {
            if (resetIfNeed)
                resetTalents(true);
            SetFreeTalentPoints(0);
        }
    }
    else
    {
        uint32 talentPointsForLevel = CalculateTalentsPoints();

        // if used more that have then reset
        if (m_usedTalentCount > talentPointsForLevel)
        {
            if (resetIfNeed && GetSession()->GetSecurity() < SEC_FULL_GM)
                resetTalents(true);
            else
                SetFreeTalentPoints(0);
        }
        // else update amount of free points
        else
            SetFreeTalentPoints(talentPointsForLevel - m_usedTalentCount);
    }
}

void Player::InitTalentForLevel()
{
    UpdateFreeTalentPoints();
}

void Player::InitStatsForLevel(bool reapplyMods)
{
    if (reapplyMods) // reapply stats values only on .reset stats (level)
                     // command
        _RemoveAllStatBonuses();

    PlayerClassLevelInfo classInfo;
    sObjectMgr::Instance()->GetPlayerClassLevelInfo(
        getClass(), getLevel(), &classInfo);

    PlayerLevelInfo info;
    sObjectMgr::Instance()->GetPlayerLevelInfo(
        getRace(), getClass(), getLevel(), &info);

    SetUInt32Value(PLAYER_FIELD_MAX_LEVEL,
        sWorld::Instance()->getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL));
    SetUInt32Value(PLAYER_NEXT_LEVEL_XP,
        sObjectMgr::Instance()->GetXPForLevel(getLevel()));

    // reset before any aura state sources (health set/aura apply)
    SetUInt32Value(UNIT_FIELD_AURASTATE, 0);

    UpdateSkillsForLevel();

    // set default cast time multiplier
    SetFloatValue(UNIT_MOD_CAST_SPEED, 1.0f);

    // save base values (bonuses already included in stored stats
    for (int i = STAT_STRENGTH; i < MAX_STATS; ++i)
        SetCreateStat(Stats(i), info.stats[i]);

    for (int i = STAT_STRENGTH; i < MAX_STATS; ++i)
        SetStat(Stats(i), info.stats[i]);

    SetCreateHealth(classInfo.basehealth);

    // set create powers
    SetCreateMana(classInfo.basemana);

    SetArmor(int32(m_createStats[STAT_AGILITY] * 2));

    InitStatBuffMods();

    // reset rating fields values
    for (uint16 index = PLAYER_FIELD_COMBAT_RATING_1;
         index < PLAYER_FIELD_COMBAT_RATING_1 + MAX_COMBAT_RATING; ++index)
        SetUInt32Value(index, 0);

    SetUInt32Value(PLAYER_FIELD_MOD_HEALING_DONE_POS, 0);
    for (int i = 0; i < MAX_SPELL_SCHOOL; ++i)
    {
        SetUInt32Value(PLAYER_FIELD_MOD_DAMAGE_DONE_NEG + i, 0);
        SetUInt32Value(PLAYER_FIELD_MOD_DAMAGE_DONE_POS + i, 0);
        SetFloatValue(PLAYER_FIELD_MOD_DAMAGE_DONE_PCT + i, 1.00f);
    }

    // reset attack power, damage and attack speed fields
    SetFloatValue(UNIT_FIELD_BASEATTACKTIME, 2000.0f);
    SetFloatValue(
        UNIT_FIELD_BASEATTACKTIME + 1, 2000.0f); // offhand attack time
    SetFloatValue(UNIT_FIELD_RANGEDATTACKTIME, 2000.0f);

    SetFloatValue(UNIT_FIELD_MINDAMAGE, 0.0f);
    SetFloatValue(UNIT_FIELD_MAXDAMAGE, 0.0f);
    SetFloatValue(UNIT_FIELD_MINOFFHANDDAMAGE, 0.0f);
    SetFloatValue(UNIT_FIELD_MAXOFFHANDDAMAGE, 0.0f);
    SetFloatValue(UNIT_FIELD_MINRANGEDDAMAGE, 0.0f);
    SetFloatValue(UNIT_FIELD_MAXRANGEDDAMAGE, 0.0f);

    SetInt32Value(UNIT_FIELD_ATTACK_POWER, 0);
    SetInt32Value(UNIT_FIELD_ATTACK_POWER_MODS, 0);
    SetFloatValue(UNIT_FIELD_ATTACK_POWER_MULTIPLIER, 0.0f);
    SetInt32Value(UNIT_FIELD_RANGED_ATTACK_POWER, 0);
    SetInt32Value(UNIT_FIELD_RANGED_ATTACK_POWER_MODS, 0);
    SetFloatValue(UNIT_FIELD_RANGED_ATTACK_POWER_MULTIPLIER, 0.0f);

    // Base crit values (will be recalculated in UpdateAllStats() at loading and
    // in _ApplyAllStatBonuses() at reset
    SetFloatValue(PLAYER_CRIT_PERCENTAGE, 0.0f);
    SetFloatValue(PLAYER_OFFHAND_CRIT_PERCENTAGE, 0.0f);
    SetFloatValue(PLAYER_RANGED_CRIT_PERCENTAGE, 0.0f);

    // Init spell schools (will be recalculated in UpdateAllStats() at loading
    // and in _ApplyAllStatBonuses() at reset
    for (uint8 i = 0; i < MAX_SPELL_SCHOOL; ++i)
        SetFloatValue(PLAYER_SPELL_CRIT_PERCENTAGE1 + i, 0.0f);

    SetFloatValue(PLAYER_PARRY_PERCENTAGE, 0.0f);
    SetFloatValue(PLAYER_BLOCK_PERCENTAGE, 0.0f);
    SetUInt32Value(PLAYER_SHIELD_BLOCK, 0);

    // Dodge percentage
    SetFloatValue(PLAYER_DODGE_PERCENTAGE, 0.0f);

    // set armor (resistance 0) to original value (create_agility*2)
    SetArmor(int32(m_createStats[STAT_AGILITY] * 2));
    SetResistanceBuffMods(SpellSchools(0), true, 0.0f);
    SetResistanceBuffMods(SpellSchools(0), false, 0.0f);
    // set other resistance to original value (0)
    for (int i = 1; i < MAX_SPELL_SCHOOL; ++i)
    {
        SetResistance(SpellSchools(i), 0);
        SetResistanceBuffMods(SpellSchools(i), true, 0.0f);
        SetResistanceBuffMods(SpellSchools(i), false, 0.0f);
    }

    SetUInt32Value(PLAYER_FIELD_MOD_TARGET_RESISTANCE, 0);
    SetUInt32Value(PLAYER_FIELD_MOD_TARGET_PHYSICAL_RESISTANCE, 0);
    for (int i = 0; i < MAX_SPELL_SCHOOL; ++i)
    {
        SetUInt32Value(UNIT_FIELD_POWER_COST_MODIFIER + i, 0);
        SetFloatValue(UNIT_FIELD_POWER_COST_MULTIPLIER + i, 0.0f);
    }
    // Init data for form but skip reapply item mods for form
    InitDataForForm(reapplyMods);

    // save new stats
    for (int i = POWER_MANA; i < MAX_POWERS; ++i)
        SetMaxPower(Powers(i), GetCreatePowers(Powers(i)));

    SetMaxHealth(classInfo.basehealth); // stamina bonus will applied later

    // cleanup mounted state (it will set correctly at aura loading if player
    // saved at mount.
    SetUInt32Value(UNIT_FIELD_MOUNTDISPLAYID, 0);

    // cleanup unit flags (will be re-applied if need at aura load).
    RemoveFlag(UNIT_FIELD_FLAGS,
        UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_DISABLE_MOVE |
            UNIT_FLAG_NOT_ATTACKABLE_1 | UNIT_FLAG_NOT_PLAYER_ATTACKABLE |
            UNIT_FLAG_PASSIVE | UNIT_FLAG_LOOTING | UNIT_FLAG_PET_IN_COMBAT |
            UNIT_FLAG_SILENCED | UNIT_FLAG_PACIFIED | UNIT_FLAG_STUNNED |
            UNIT_FLAG_IN_COMBAT | UNIT_FLAG_DISARMED | UNIT_FLAG_CONFUSED |
            UNIT_FLAG_FLEEING | UNIT_FLAG_NOT_SELECTABLE | UNIT_FLAG_SKINNABLE |
            UNIT_FLAG_MOUNT | UNIT_FLAG_TAXI_FLIGHT);
    SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PVP_ATTACKABLE); // must be set

    // cleanup player flags (will be re-applied if need at aura load), to avoid
    // have ghost flag without ghost aura, for example.
    RemoveFlag(PLAYER_FLAGS, PLAYER_FLAGS_AFK | PLAYER_FLAGS_DND |
                                 PLAYER_FLAGS_GM | PLAYER_FLAGS_GHOST |
                                 PLAYER_FLAGS_FFA_PVP);

    RemoveStandFlags(UNIT_STAND_FLAGS_ALL); // one form stealth modified bytes

    // restore if need some important flags
    SetUInt32Value(PLAYER_FIELD_BYTES2, 0); // flags empty by default

    if (reapplyMods) // reapply stats values only on .reset stats (level)
                     // command
        _ApplyAllStatBonuses();

    // set current level health and mana/energy to maximum after applying all
    // mods.
    SetHealth(GetMaxHealth());
    SetPower(POWER_MANA, GetMaxPower(POWER_MANA));
    SetPower(POWER_ENERGY, GetMaxPower(POWER_ENERGY));
    if (GetPower(POWER_RAGE) > GetMaxPower(POWER_RAGE))
        SetPower(POWER_RAGE, GetMaxPower(POWER_RAGE));
    SetPower(POWER_FOCUS, 0);
    SetPower(POWER_HAPPINESS, 0);

    // update level to hunter/summon pet
    if (Pet* pet = GetPet())
        pet->SynchronizeLevelWithOwner();
}

void Player::SendInitialSpells()
{
    time_t curTime = WorldTimer::time_no_syscall();
    time_t infTime = curTime + infinityCooldownDelayCheck;

    uint16 spellCount = 0;

    WorldPacket data(SMSG_INITIAL_SPELLS,
        (1 + 2 + 4 * m_spells.size() + 2 +
                         m_spellCooldowns.size() * (2 + 2 + 2 + 4 + 4)));
    data << uint8(0);

    size_t countPos = data.wpos();
    data << uint16(spellCount); // spell count placeholder

    for (PlayerSpellMap::const_iterator itr = m_spells.begin();
         itr != m_spells.end(); ++itr)
    {
        if (itr->second.state == PLAYERSPELL_REMOVED)
            continue;

        if (!itr->second.active || itr->second.disabled)
            continue;

        data << uint16(itr->first);
        data << uint16(0); // it's not slot id

        spellCount += 1;
    }

    data.put<uint16>(countPos, spellCount); // write real count value

    uint16 spellCooldowns = m_spellCooldowns.size();
    data << uint16(spellCooldowns);
    for (SpellCooldowns::const_iterator itr = m_spellCooldowns.begin();
         itr != m_spellCooldowns.end(); ++itr)
    {
        SpellEntry const* sEntry = sSpellStore.LookupEntry(itr->first);
        if (!sEntry)
            continue;

        uint32 category = sEntry->Category;

        // Check if item_template overides the spell's category
        if (const ItemPrototype* item =
                sObjectMgr::Instance()->GetItemPrototype(itr->second.itemid))
        {
            auto f_itr = std::find_if(std::begin(item->Spells),
                std::end(item->Spells), [&itr](const _Spell& s)
                {
                    return s.SpellId == itr->first;
                });
            if (f_itr != std::end(item->Spells) && f_itr->SpellCategory != 0)
                category = f_itr->SpellCategory;
        }

        data << uint16(itr->first);

        data << uint16(itr->second.itemid); // cast item id
        data << uint16(category);           // spell category

        // send infinity cooldown in special format
        if (itr->second.end >= infTime)
        {
            data << uint32(1);          // cooldown
            data << uint32(0x80000000); // category cooldown
            continue;
        }

        time_t cooldown = itr->second.end > curTime ?
                              (itr->second.end - curTime) * IN_MILLISECONDS :
                              0;

        // FIXME: This is still not correct
        if (category && sEntry->RecoveryTime == 0)
        {
            data << uint32(0);        // cooldown
            data << uint32(cooldown); // category cooldown
        }
        else
        {
            data << uint32(cooldown); // cooldown
            data << uint32(0);        // category cooldown
        }
    }

    GetSession()->send_packet(std::move(data));

    LOG_DEBUG(logging, "CHARACTER: Sent Initial Spells");
}

void Player::RemoveMail(uint32 id)
{
    for (auto itr = m_mail.begin(); itr != m_mail.end(); ++itr)
    {
        if ((*itr)->messageID == id)
        {
            // do not delete item, because Player::removeMail() is called when
            // returning mail to sender.
            m_mail.erase(itr);
            return;
        }
    }
}

void Player::SendMailResult(uint32 mailId, MailResponseType mailAction,
    MailResponseResult mailError, uint32 equipError, uint32 item_guid,
    uint32 item_count)
{
    WorldPacket data(SMSG_SEND_MAIL_RESULT,
        (4 + 4 + 4 + (mailError == MAIL_ERR_EQUIP_ERROR ?
                             4 :
                             (mailAction == MAIL_ITEM_TAKEN ? 4 + 4 : 0))));
    data << (uint32)mailId;
    data << (uint32)mailAction;
    data << (uint32)mailError;
    if (mailError == MAIL_ERR_EQUIP_ERROR)
        data << (uint32)equipError;
    else if (mailAction == MAIL_ITEM_TAKEN)
    {
        data << (uint32)item_guid;  // item guid low?
        data << (uint32)item_count; // item count?
    }
    GetSession()->send_packet(std::move(data));
}

void Player::SendNewMail()
{
    // deliver undelivered mail
    WorldPacket data(SMSG_RECEIVED_MAIL, 4);
    data << (uint32)0;
    GetSession()->send_packet(std::move(data));
}

void Player::UpdateNextMailTimeAndUnreads()
{
    // calculate next delivery time (min. from non-delivered mails
    // and recalculate unReadMail
    time_t cTime = WorldTimer::time_no_syscall();
    m_nextMailDelivereTime = 0;
    unReadMails = 0;
    for (auto& elem : m_mail)
    {
        if ((elem)->deliver_time > cTime)
        {
            if (!m_nextMailDelivereTime ||
                m_nextMailDelivereTime > (elem)->deliver_time)
                m_nextMailDelivereTime = (elem)->deliver_time;
        }
        else if (((elem)->checked & MAIL_CHECK_MASK_READ) == 0)
            ++unReadMails;
    }
}

void Player::AddNewMailDeliverTime(time_t deliver_time)
{
    if (deliver_time <= WorldTimer::time_no_syscall()) // ready now
    {
        ++unReadMails;
        SendNewMail();
    }
    else // not ready and no have ready mails
    {
        if (!m_nextMailDelivereTime || m_nextMailDelivereTime > deliver_time)
            m_nextMailDelivereTime = deliver_time;
    }
}

bool Player::addSpell(
    uint32 spell_id, bool active, bool learning, bool dependent, bool disabled)
{
    SpellEntry const* spellInfo = sSpellStore.LookupEntry(spell_id);
    if (!spellInfo)
    {
        // do character spell book cleanup (all characters)
        if (!IsInWorld() && !learning) // spell load case
        {
            logging.error(
                "Player::addSpell: nonexistent in SpellStore spell #%u "
                "request, deleting for all characters in `character_spell`.",
                spell_id);
            CharacterDatabase.PExecute(
                "DELETE FROM character_spell WHERE spell = '%u'", spell_id);
        }
        else
            logging.error(
                "Player::addSpell: nonexistent in SpellStore spell #%u "
                "request.",
                spell_id);

        return false;
    }

    if (!SpellMgr::IsSpellValid(spellInfo, this, false))
    {
        // do character spell book cleanup (all characters)
        if (!IsInWorld() && !learning) // spell load case
        {
            logging.error(
                "Player::addSpell: Broken spell #%u learning not allowed, "
                "deleting for all characters in `character_spell`.",
                spell_id);
            CharacterDatabase.PExecute(
                "DELETE FROM character_spell WHERE spell = '%u'", spell_id);
        }
        else
            logging.error(
                "Player::addSpell: Broken spell #%u learning not allowed.",
                spell_id);

        return false;
    }

    PlayerSpellState state = learning ? PLAYERSPELL_NEW : PLAYERSPELL_UNCHANGED;

    bool dependent_set = false;
    bool disabled_case = false;
    bool superceded_old = false;

    auto itr = m_spells.find(spell_id);
    if (itr != m_spells.end())
    {
        uint32 next_active_spell_id = 0;
        // Spells that don't stack in the spell book has the highest possible
        // rank learned
        // So we go through each rank above the current spell checking if it is
        // available
        // If we run into any spell above this spell's rank, we know we cannot
        // learn this spell
        if (sSpellMgr::Instance()->IsRankedSpellNonStackableInSpellBook(
                spellInfo))
        {
            std::vector<uint32>::const_iterator s_itr, lower, upper;
            if (sSpellMgr::Instance()->GetNextSpellInChainBoundaries(
                    spell_id, lower, upper))
            {
                for (s_itr = lower; s_itr != upper; ++s_itr)
                {
                    if (HasSpell(*s_itr))
                    {
                        // We know a higher rank, so this rank must be inactive
                        active = false;
                        next_active_spell_id = *s_itr;
                        break;
                    }
                }
            }
        }

        // not do anything if already known in expected state
        if (itr->second.state != PLAYERSPELL_REMOVED &&
            itr->second.active == active &&
            itr->second.dependent == dependent &&
            itr->second.disabled == disabled)
        {
            if (!IsInWorld() && !learning) // explicitly load from DB and then
                                           // exist in it already and set
                                           // correctly
                itr->second.state = PLAYERSPELL_UNCHANGED;

            return false;
        }

        // dependent spell known as not dependent, overwrite state
        if (itr->second.state != PLAYERSPELL_REMOVED &&
            !itr->second.dependent && dependent)
        {
            itr->second.dependent = dependent;
            if (itr->second.state != PLAYERSPELL_NEW)
                itr->second.state = PLAYERSPELL_CHANGED;
            dependent_set = true;
        }

        // update active state for known spell
        if (itr->second.active != active &&
            itr->second.state != PLAYERSPELL_REMOVED && !itr->second.disabled)
        {
            itr->second.active = active;

            if (!IsInWorld() && !learning &&
                !dependent_set) // explicitly load from DB and then exist in it
                                // already and set correctly
                itr->second.state = PLAYERSPELL_UNCHANGED;
            else if (itr->second.state != PLAYERSPELL_NEW)
                itr->second.state = PLAYERSPELL_CHANGED;

            if (active)
            {
                if (IsNeedCastPassiveLikeSpellAtLearn(spellInfo))
                    CastSpell(this, spell_id, true);
            }
            else if (IsInWorld())
            {
                if (next_active_spell_id)
                {
                    // update spell ranks in spellbook and action bar
                    WorldPacket data(SMSG_SUPERCEDED_SPELL, (4));
                    data << uint16(spell_id);
                    data << uint16(next_active_spell_id);
                    GetSession()->send_packet(std::move(data));
                }
                else
                {
                    WorldPacket data(SMSG_REMOVED_SPELL, 4);
                    data << uint16(spell_id);
                    GetSession()->send_packet(std::move(data));
                }
            }

            return active; // learn (show in spell book if active now)
        }

        if (itr->second.disabled != disabled &&
            itr->second.state != PLAYERSPELL_REMOVED)
        {
            if (itr->second.state != PLAYERSPELL_NEW)
                itr->second.state = PLAYERSPELL_CHANGED;
            itr->second.disabled = disabled;

            if (disabled)
                return false;

            disabled_case = true;
        }
        else
            switch (itr->second.state)
            {
            case PLAYERSPELL_UNCHANGED: // known saved spell
                return false;
            case PLAYERSPELL_REMOVED: // re-learning removed not saved spell
            {
                m_spells.erase(itr);
                state = PLAYERSPELL_CHANGED;
                break; // need re-add
            }
            default: // known not saved yet spell (new or modified)
            {
                // can be in case spell loading but learned at some previous
                // spell loading
                if (!IsInWorld() && !learning && !dependent_set)
                    itr->second.state = PLAYERSPELL_UNCHANGED;

                return false;
            }
            }
    }

    TalentSpellPos const* talentPos = GetTalentSpellPos(spell_id);

    if (!disabled_case) // skip new spell adding if spell already known
                        // (disabled spells case)
    {
        // talent: unlearn all other talent ranks (high and low)
        if (talentPos)
        {
            if (TalentEntry const* talentInfo =
                    sTalentStore.LookupEntry(talentPos->talent_id))
            {
                for (int i = 0; i < MAX_TALENT_RANK; ++i)
                {
                    // skip learning spell and no rank spell case
                    uint32 rankSpellId = talentInfo->RankID[i];
                    if (!rankSpellId || rankSpellId == spell_id)
                        continue;

                    removeSpell(rankSpellId, false, false);
                }
            }
        }
        // non talent spell: learn low ranks (recursive call)
        else if (uint32 prev_spell =
                     sSpellMgr::Instance()->GetPrevSpellInChain(spell_id))
        {
            if (!IsInWorld() ||
                disabled) // at spells loading, no output, but allow save
                addSpell(prev_spell, active, true, true, disabled);
            else // at normal learning
                learnSpell(prev_spell, true);
        }

        PlayerSpell newspell;
        newspell.state = state;
        newspell.active = active;
        newspell.dependent = dependent;
        newspell.disabled = disabled;

        // replace spells in action bars and spellbook to bigger rank if only
        // one spell rank must be accessible
        if (newspell.active && !newspell.disabled &&
            sSpellMgr::Instance()->IsRankedSpellNonStackableInSpellBook(
                spellInfo))
        {
            for (auto& elem : m_spells)
            {
                if (elem.second.state == PLAYERSPELL_REMOVED)
                    continue;
                SpellEntry const* i_spellInfo =
                    sSpellStore.LookupEntry(elem.first);
                if (!i_spellInfo)
                    continue;

                if (sSpellMgr::Instance()->IsRankSpellDueToSpell(
                        spellInfo, elem.first))
                {
                    if (elem.second.active)
                    {
                        if (sSpellMgr::Instance()->IsHighRankOfSpell(
                                spell_id, elem.first))
                        {
                            if (IsInWorld()) // not send spell (re-/over-)learn
                                             // packets at loading
                            {
                                WorldPacket data(SMSG_SUPERCEDED_SPELL, (4));
                                data << uint16(elem.first);
                                data << uint16(spell_id);
                                GetSession()->send_packet(std::move(data));
                            }

                            // mark old spell as disable (SMSG_SUPERCEDED_SPELL
                            // replace it in client by new)
                            elem.second.active = false;
                            if (elem.second.state != PLAYERSPELL_NEW)
                                elem.second.state = PLAYERSPELL_CHANGED;
                            superceded_old = true; // new spell replace old in
                                                   // action bars and spell
                                                   // book.
                        }
                        else if (sSpellMgr::Instance()->IsHighRankOfSpell(
                                     elem.first, spell_id))
                        {
                            if (IsInWorld()) // not send spell (re-/over-)learn
                                             // packets at loading
                            {
                                WorldPacket data(SMSG_SUPERCEDED_SPELL, (4));
                                data << uint16(spell_id);
                                data << uint16(elem.first);
                                GetSession()->send_packet(std::move(data));
                            }

                            // mark new spell as disable (not learned yet for
                            // client and will not learned)
                            newspell.active = false;
                            if (newspell.state != PLAYERSPELL_NEW)
                                newspell.state = PLAYERSPELL_CHANGED;
                        }
                    }
                }
            }
        }

        m_spells[spell_id] = newspell;

        // return false if spell disabled
        if (newspell.disabled)
            return false;
    }

    if (talentPos)
    {
        // update used talent points count
        m_usedTalentCount += GetTalentSpellCost(talentPos);
        UpdateFreeTalentPoints(false);
    }

    // update free primary prof.points (if any, can be none in case GM .learn
    // prof. learning)
    if (uint32 freeProfs = GetFreePrimaryProfessionPoints())
    {
        if (sSpellMgr::Instance()->IsPrimaryProfessionFirstRankSpell(spell_id))
            SetFreePrimaryProfessions(freeProfs - 1);
    }

    // cast talents with SPELL_EFFECT_LEARN_SPELL (other dependent spells will
    // learned later as not auto-learned)
    // note: all spells with SPELL_EFFECT_LEARN_SPELL isn't passive
    if (talentPos && IsSpellHaveEffect(spellInfo, SPELL_EFFECT_LEARN_SPELL))
    {
        // ignore stance requirement for talent learn spell (stance set for
        // spell only for client spell description show)
        CastSpell(this, spell_id, true);
    }
    // also cast passive (and passive like) spells (including all talents
    // without SPELL_EFFECT_LEARN_SPELL) with additional checks
    else if (IsNeedCastPassiveLikeSpellAtLearn(spellInfo))
    {
        CastSpell(this, spell_id, true);
    }
    else if (IsSpellHaveEffect(spellInfo, SPELL_EFFECT_SKILL_STEP))
    {
        CastSpell(this, spell_id, true);
        return false;
    }

    // add dependent skills
    uint16 maxskill = GetMaxSkillValueForLevel();

    SpellLearnSkillNode const* spellLearnSkill =
        sSpellMgr::Instance()->GetSpellLearnSkill(spell_id);

    if (spellLearnSkill)
    {
        uint32 skill_value = GetPureSkillValue(spellLearnSkill->skill);
        uint32 skill_max_value = GetPureMaxSkillValue(spellLearnSkill->skill);

        if (skill_value < spellLearnSkill->value)
            skill_value = spellLearnSkill->value;

        uint32 new_skill_max_value = spellLearnSkill->maxvalue == 0 ?
                                         maxskill :
                                         spellLearnSkill->maxvalue;

        if (skill_max_value < new_skill_max_value)
            skill_max_value = new_skill_max_value;

        SetSkill(spellLearnSkill->skill, skill_value, skill_max_value,
            spellLearnSkill->step);
    }
    else
    {
        // not ranked skills
        SkillLineAbilityMapBounds skill_bounds =
            sSpellMgr::Instance()->GetSkillLineAbilityMapBounds(spell_id);

        for (auto _spell_idx = skill_bounds.first;
             _spell_idx != skill_bounds.second; ++_spell_idx)
        {
            SkillLineEntry const* pSkill =
                sSkillLineStore.LookupEntry(_spell_idx->second->skillId);
            if (!pSkill)
                continue;

            if (HasSkill(pSkill->id))
                continue;

            if (_spell_idx->second->learnOnGetSkill ==
                    ABILITY_LEARNED_ON_GET_RACE_OR_CLASS_SKILL ||
                // poison special case, not have
                // ABILITY_LEARNED_ON_GET_RACE_OR_CLASS_SKILL
                (pSkill->id == SKILL_POISONS &&
                    _spell_idx->second->max_value == 0) ||
                // lockpicking special case, not have
                // ABILITY_LEARNED_ON_GET_RACE_OR_CLASS_SKILL
                (pSkill->id == SKILL_LOCKPICKING &&
                    _spell_idx->second->max_value == 0))
            {
                switch (GetSkillRangeType(
                    pSkill, _spell_idx->second->racemask != 0))
                {
                case SKILL_RANGE_LANGUAGE:
                    SetSkill(pSkill->id, 300, 300);
                    break;
                case SKILL_RANGE_LEVEL:
                    SetSkill(pSkill->id, 1, GetMaxSkillValueForLevel());
                    break;
                case SKILL_RANGE_MONO:
                    SetSkill(pSkill->id, 1, 1);
                    break;
                default:
                    break;
                }
            }
        }
    }

    // learn dependent spells
    SpellLearnSpellMapBounds spell_bounds =
        sSpellMgr::Instance()->GetSpellLearnSpellMapBounds(spell_id);

    for (auto itr2 = spell_bounds.first; itr2 != spell_bounds.second; ++itr2)
    {
        if (!itr2->second.autoLearned)
        {
            if (!IsInWorld() ||
                !itr2->second
                     .active) // at spells loading, no output, but allow save
                addSpell(
                    itr2->second.spell, itr2->second.active, true, true, false);
            else // at normal learning
                learnSpell(itr2->second.spell, true);
        }
    }

    // return true (for send learn packet) only if spell active (in case ranked
    // spells) and not replace old spell
    return active && !disabled && !superceded_old;
}

bool Player::IsNeedCastPassiveLikeSpellAtLearn(
    SpellEntry const* spellInfo) const
{
    ShapeshiftForm form = GetShapeshiftForm();

    if (IsNeedCastSpellAtFormApply(
            spellInfo, form)) // SPELL_ATTR_PASSIVE | SPELL_ATTR_UNK7 spells
        return true; // all stance req. cases, not have auarastate cases

    if (!spellInfo->HasAttribute(SPELL_ATTR_PASSIVE))
        return false;

    // note: form passives activated with shapeshift spells be implemented by
    // HandleShapeshiftBoosts instead of spell_learn_spell
    // talent dependent passives activated at form apply have proper stance data
    bool need_cast =
        !spellInfo->Stances ||
        (!form && spellInfo->HasAttribute(SPELL_ATTR_EX2_NOT_NEED_SHAPESHIFT));

    // Check CasterAuraStates
    return need_cast &&
           (!spellInfo->CasterAuraState ||
               HasAuraState(AuraState(spellInfo->CasterAuraState)));
}

void Player::learnSpell(uint32 spell_id, bool dependent)
{
    auto itr = m_spells.find(spell_id);

    bool disabled = (itr != m_spells.end()) ? itr->second.disabled : false;
    bool active = disabled ? itr->second.active : true;

    bool learning = addSpell(spell_id, active, true, dependent, false);

    // prevent duplicated entires in spell book, also not send if not in world
    // (loading)
    if (learning && IsInWorld())
    {
        WorldPacket data(SMSG_LEARNED_SPELL, 4);
        data << uint32(spell_id);
        GetSession()->send_packet(std::move(data));
    }

    // Relearn any higher-ranked spell that has been disabled (Recursively calls
    // learnSpell())
    if (disabled)
    {
        std::vector<uint32>::const_iterator s_itr, lower, upper;
        if (sSpellMgr::Instance()->GetNextSpellInChainBoundaries(
                spell_id, lower, upper))
        {
            for (s_itr = lower; s_itr != upper; ++s_itr)
            {
                auto iter = m_spells.find(*s_itr);
                if (iter != m_spells.end() && iter->second.disabled)
                    learnSpell(*s_itr, false);
            }
        }

        // Re-learn spells that depend on this one
        auto bounds = sSpellMgr::Instance()->GetDependentSpells(spell_id);
        for (auto itr = bounds.first; itr != bounds.second; ++itr)
        {
            auto i_itr = m_spells.find(itr->second);
            if (i_itr != m_spells.end() && i_itr->second.disabled)
                learnSpell(itr->second, false);
        }
    }
}

void Player::removeSpell(
    uint32 spell_id, bool disabled, bool learn_low_rank, bool sendUpdate)
{
    auto itr = m_spells.find(spell_id);
    if (itr == m_spells.end())
        return;

    if (itr->second.state == PLAYERSPELL_REMOVED ||
        (disabled && itr->second.disabled))
        return;

    // Remove all spells that come after this spell rank-wise (Recursively calls
    // removeSpell())
    std::vector<uint32>::const_iterator s_itr, lower, upper;
    if (sSpellMgr::Instance()->GetNextSpellInChainBoundaries(
            spell_id, lower, upper))
        for (s_itr = lower; s_itr != upper; ++s_itr)
            if (HasSpell(*s_itr) && !GetTalentSpellPos(*s_itr))
                removeSpell(*s_itr, disabled, false);

    // Remove spells that depend on this one
    auto bounds = sSpellMgr::Instance()->GetDependentSpells(spell_id);
    for (auto itr = bounds.first; itr != bounds.second; ++itr)
        removeSpell(itr->second, disabled);

    // re-search, it can be corrupted in prev loop
    itr = m_spells.find(spell_id);
    if (itr == m_spells.end() || itr->second.state == PLAYERSPELL_REMOVED)
        return; // already unleared

    bool cur_active = itr->second.active;
    bool cur_dependent = itr->second.dependent;

    if (disabled)
    {
        itr->second.disabled = disabled;
        if (itr->second.state != PLAYERSPELL_NEW)
            itr->second.state = PLAYERSPELL_CHANGED;
    }
    else
    {
        if (itr->second.state == PLAYERSPELL_NEW)
            m_spells.erase(itr);
        else
            itr->second.state = PLAYERSPELL_REMOVED;
    }

    remove_auras(spell_id);

    // remove pet auras
    if (PetAura const* petSpell = sSpellMgr::Instance()->GetPetAura(spell_id))
        RemovePetAura(petSpell);

    TalentSpellPos const* talentPos = GetTalentSpellPos(spell_id);
    if (talentPos)
    {
        // free talent points
        uint32 talentCosts = GetTalentSpellCost(talentPos);

        if (talentCosts < m_usedTalentCount)
            m_usedTalentCount -= talentCosts;
        else
            m_usedTalentCount = 0;

        UpdateFreeTalentPoints(false);
    }

    // update free primary prof.points (if not overflow setting, can be in case
    // GM use before .learn prof. learning)
    if (sSpellMgr::Instance()->IsPrimaryProfessionFirstRankSpell(spell_id))
    {
        uint32 freeProfs = GetFreePrimaryProfessionPoints() + 1;
        uint32 maxProfs =
            GetSession()->GetSecurity() <
                    AccountTypes(sWorld::Instance()->getConfig(
                        CONFIG_UINT32_TRADE_SKILL_GMIGNORE_MAX_PRIMARY_COUNT)) ?
                sWorld::Instance()->getConfig(
                    CONFIG_UINT32_MAX_PRIMARY_TRADE_SKILL) :
                10;
        if (freeProfs <= maxProfs)
            SetFreePrimaryProfessions(freeProfs);
    }

    // remove dependent skill
    SpellLearnSkillNode const* spellLearnSkill =
        sSpellMgr::Instance()->GetSpellLearnSkill(spell_id);
    if (spellLearnSkill)
    {
        uint32 prev_spell =
            sSpellMgr::Instance()->GetPrevSpellInChain(spell_id);
        if (!prev_spell) // first rank, remove skill
            SetSkill(spellLearnSkill->skill, 0, 0);
        else
        {
            // search prev. skill setting by spell ranks chain
            SpellLearnSkillNode const* prevSkill =
                sSpellMgr::Instance()->GetSpellLearnSkill(prev_spell);
            while (!prevSkill && prev_spell)
            {
                prev_spell =
                    sSpellMgr::Instance()->GetPrevSpellInChain(prev_spell);
                prevSkill = sSpellMgr::Instance()->GetSpellLearnSkill(
                    sSpellMgr::Instance()->GetFirstSpellInChain(prev_spell));
            }

            if (!prevSkill) // not found prev skill setting, remove skill
                SetSkill(spellLearnSkill->skill, 0, 0);
            else // set to prev. skill setting values
            {
                uint32 skill_value = GetPureSkillValue(prevSkill->skill);
                uint32 skill_max_value = GetPureMaxSkillValue(prevSkill->skill);

                if (skill_value > prevSkill->value)
                    skill_value = prevSkill->value;

                uint32 new_skill_max_value = prevSkill->maxvalue == 0 ?
                                                 GetMaxSkillValueForLevel() :
                                                 prevSkill->maxvalue;

                if (skill_max_value > new_skill_max_value)
                    skill_max_value = new_skill_max_value;

                SetSkill(prevSkill->skill, skill_value, skill_max_value,
                    prevSkill->step);
            }
        }
    }
    else
    {
        // not ranked skills
        SkillLineAbilityMapBounds bounds =
            sSpellMgr::Instance()->GetSkillLineAbilityMapBounds(spell_id);

        for (auto _spell_idx = bounds.first; _spell_idx != bounds.second;
             ++_spell_idx)
        {
            SkillLineEntry const* pSkill =
                sSkillLineStore.LookupEntry(_spell_idx->second->skillId);
            if (!pSkill)
                continue;

            if ((_spell_idx->second->learnOnGetSkill ==
                        ABILITY_LEARNED_ON_GET_RACE_OR_CLASS_SKILL &&
                    pSkill->categoryId !=
                        SKILL_CATEGORY_CLASS) || // not unlearn class skills
                                                 // (spellbook/talent pages)
                // poisons/lockpicking special case, not have
                // ABILITY_LEARNED_ON_GET_RACE_OR_CLASS_SKILL
                ((pSkill->id == SKILL_POISONS ||
                     pSkill->id == SKILL_LOCKPICKING) &&
                    _spell_idx->second->max_value == 0))
            {
                // not reset skills for professions and racial abilities
                if ((pSkill->categoryId == SKILL_CATEGORY_SECONDARY ||
                        pSkill->categoryId == SKILL_CATEGORY_PROFESSION) &&
                    (IsProfessionSkill(pSkill->id) ||
                        _spell_idx->second->racemask != 0))
                    continue;

                SetSkill(pSkill->id, 0, 0);
            }
        }
    }

    // remove dependent spells
    SpellLearnSpellMapBounds spell_bounds =
        sSpellMgr::Instance()->GetSpellLearnSpellMapBounds(spell_id);

    for (auto itr2 = spell_bounds.first; itr2 != spell_bounds.second; ++itr2)
        removeSpell(itr2->second.spell, disabled);

    // activate lesser rank in spellbook/action bar, and cast it if need
    bool prev_activate = false;

    if (uint32 prev_id = sSpellMgr::Instance()->GetPrevSpellInChain(spell_id))
    {
        SpellEntry const* spellInfo = sSpellStore.LookupEntry(spell_id);

        // if talent then lesser rank also talent and need learn
        if (talentPos)
        {
            if (learn_low_rank)
                learnSpell(prev_id, false);
        }
        // if ranked non-stackable spell: need activate lesser rank and update
        // dependence state
        else if (cur_active &&
                 sSpellMgr::Instance()->IsRankedSpellNonStackableInSpellBook(
                     spellInfo))
        {
            // need manually update dependence state (learn spell ignore like
            // attempts)
            auto prev_itr = m_spells.find(prev_id);
            if (prev_itr != m_spells.end())
            {
                if (prev_itr->second.dependent != cur_dependent)
                {
                    prev_itr->second.dependent = cur_dependent;
                    if (prev_itr->second.state != PLAYERSPELL_NEW)
                        prev_itr->second.state = PLAYERSPELL_CHANGED;
                }

                // now re-learn if need re-activate
                if (cur_active && !prev_itr->second.active && learn_low_rank)
                {
                    if (addSpell(prev_id, true, false,
                            prev_itr->second.dependent,
                            prev_itr->second.disabled))
                    {
                        // downgrade spell ranks in spellbook and action bar
                        WorldPacket data(SMSG_SUPERCEDED_SPELL, 4);
                        data << uint16(spell_id);
                        data << uint16(prev_id);
                        GetSession()->send_packet(std::move(data));
                        prev_activate = true;
                    }
                }
            }
        }
    }

    // for shaman Dual-wield
    if (CanDualWield())
    {
        SpellEntry const* spellInfo = sSpellStore.LookupEntry(spell_id);

        if (IsSpellHaveEffect(spellInfo, SPELL_EFFECT_DUAL_WIELD))
            SetCanDualWield(false);
    }

    // remove from spell book if not replaced by lesser rank
    if (!prev_activate && sendUpdate)
    {
        WorldPacket data(SMSG_REMOVED_SPELL, 4);
        data << uint16(spell_id);
        GetSession()->send_packet(std::move(data));
    }
}

void Player::RemoveSpellCooldown(uint32 spell_id, bool update /* = false */)
{
    auto itr = m_spellCooldowns.find(spell_id);
    auto entry = sSpellStore.LookupEntry(spell_id);

    // remove the category cooldown for this spell
    if (itr != m_spellCooldowns.end() && entry)
    {
        uint32 category = entry->Category;
        auto cd = itr->second;

        // check overriden categories from the item template
        if (cd.itemid)
        {
            if (const ItemPrototype* item =
                    sObjectMgr::Instance()->GetItemPrototype(cd.itemid))
            {
                auto f_itr = std::find_if(std::begin(item->Spells),
                    std::end(item->Spells), [&spell_id](const _Spell& s)
                    {
                        return s.SpellId == spell_id;
                    });
                if (f_itr != std::end(item->Spells) &&
                    f_itr->SpellCategory != 0)
                    category = f_itr->SpellCategory;
            }
        }

        if (category)
            RemoveSpellCategoryCooldown(category); // remove category cooldown
    }

    // remove the cooldown from the cooldown map
    if (itr != m_spellCooldowns.end())
        m_spellCooldowns.erase(itr);

    // NOTE: we might wish to send the clear event even if we did not have the
    // cooldown
    if (update)
        SendClearCooldown(spell_id, this);
}

void Player::RemoveSpellCategoryCooldown(uint32 cat)
{
    auto itr = category_cooldowns_.find(cat);
    if (itr != category_cooldowns_.end())
        category_cooldowns_.erase(itr);
}

void Player::RemoveArenaSpellCooldowns()
{
    // remove cooldowns on spells that has < 15 min CD
    SpellCooldowns::iterator itr, next;
    // iterate spell cooldowns
    for (itr = m_spellCooldowns.begin(); itr != m_spellCooldowns.end();
         itr = next)
    {
        next = itr;
        ++next;
        SpellEntry const* entry = sSpellStore.LookupEntry(itr->first);
        // check if spellentry is present and if the cooldown is less than 15
        // mins
        if (entry && entry->RecoveryTime <= 15 * MINUTE * IN_MILLISECONDS &&
            entry->CategoryRecoveryTime <= 15 * MINUTE * IN_MILLISECONDS)
        {
            // remove & notify
            RemoveSpellCooldown(itr->first, true);
        }
    }
}

void Player::RemoveAllSpellCooldown()
{
    if (!m_spellCooldowns.empty())
    {
        for (SpellCooldowns::const_iterator itr = m_spellCooldowns.begin();
             itr != m_spellCooldowns.end(); ++itr)
            SendClearCooldown(itr->first, this);

        m_spellCooldowns.clear();
    }
    category_cooldowns_.clear();
}

void Player::_LoadSpellCooldowns(
    QueryResult* spell_result, QueryResult* category_result)
{
    // some cooldowns can be already set at aura loading...

    // QueryResult* spell_result = CharacterDatabase.PQuery("SELECT
    // spell,item,time FROM character_spell_cooldown WHERE guid =
    // '%u'",GetGUIDLow());
    // QueryResult* category_result = CharacterDatabase.PQuery("SELECT category,
    // time FROM character_category_cooldown WHERE guid = '%u'", GetGUIDLow());

    time_t curTime = WorldTimer::time_no_syscall();

    if (spell_result)
    {
        do
        {
            Field* fields = spell_result->Fetch();

            uint32 spell_id = fields[0].GetUInt32();
            uint32 item_id = fields[1].GetUInt32();
            time_t db_time = (time_t)fields[2].GetUInt64();

            if (!sSpellStore.LookupEntry(spell_id))
            {
                logging.error(
                    "Player %u has unknown spell %u in "
                    "`character_spell_cooldown`, skipping.",
                    GetGUIDLow(), spell_id);
                continue;
            }

            // skip outdated cooldown
            if (db_time <= curTime)
                continue;

            AddSpellCooldown(spell_id, item_id, db_time);

            LOG_DEBUG(logging,
                "Player (GUID: %u) spell %u, item %u cooldown loaded (%u "
                "secs).",
                GetGUIDLow(), spell_id, item_id, uint32(db_time - curTime));
        } while (spell_result->NextRow());

        delete spell_result;
    }

    if (category_result)
    {
        do
        {
            Field* fields = category_result->Fetch();

            uint32 category = fields[0].GetUInt32();
            time_t time = fields[1].GetUInt32();

            if (sSpellCategoryStore.find(category) == sSpellCategoryStore.end())
            {
                logging.error(
                    "Player %u has unknown category cooldown %u in "
                    "`character_category_cooldown`, skipping.",
                    GetGUIDLow(), category);
                return;
            }

            if (time <= curTime)
                continue;

            category_cooldowns_[category] = time;

            // Also add the cooldown for all spells we know with this category
            for (auto entry : m_spells)
            {
                if (entry.second.disabled || !entry.second.active)
                    continue;

                if (auto spell = sSpellStore.LookupEntry(entry.first))
                    if (spell->Category == category)
                    {
                        if (m_spellCooldowns.find(spell->Id) ==
                                m_spellCooldowns.end() ||
                            m_spellCooldowns[spell->Id].end < time)
                            AddSpellCooldown(spell->Id, 0, time);
                    }
            }

        } while (category_result->NextRow());

        delete category_result;
    }
}

void Player::_SaveSpellCooldowns()
{
    static SqlStatementID deleteSpellCooldown;
    static SqlStatementID insertSpellCooldown;
    static SqlStatementID deleteCategoryCooldown;
    static SqlStatementID insertCategoryCooldown;

    SqlStatement stmt = CharacterDatabase.CreateStatement(deleteSpellCooldown,
        "DELETE FROM character_spell_cooldown WHERE guid = ?");
    stmt.PExecute(GetGUIDLow());

    time_t curTime = WorldTimer::time_no_syscall();
    time_t infTime = curTime + infinityCooldownDelayCheck;

    // remove outdated and save active
    for (auto itr = m_spellCooldowns.begin(); itr != m_spellCooldowns.end();)
    {
        if (itr->second.end <= curTime)
            m_spellCooldowns.erase(itr++);
        else if (itr->second.end <= infTime) // not save locked cooldowns, it
                                             // will be reset or set at reload
        {
            stmt = CharacterDatabase.CreateStatement(insertSpellCooldown,
                "INSERT INTO character_spell_cooldown (guid,spell,item,time) "
                "VALUES( ?, ?, ?, ?)");
            stmt.PExecute(GetGUIDLow(), itr->first, itr->second.itemid,
                uint64(itr->second.end));
            ++itr;
        }
        else
            ++itr;
    }

    // save category cooldowns
    stmt = CharacterDatabase.CreateStatement(deleteCategoryCooldown,
        "DELETE FROM character_category_cooldown WHERE guid = ?");
    stmt.PExecute(GetGUIDLow());

    for (auto cd : category_cooldowns_)
    {
        if (cd.second > curTime && cd.second < infTime)
        {
            stmt = CharacterDatabase.CreateStatement(insertCategoryCooldown,
                "INSERT INTO character_category_cooldown (guid, category, "
                "time) VALUES(?, ?, ?)");
            stmt.PExecute(GetGUIDLow(), cd.first, cd.second);
        }
    }
}

uint32 Player::resetTalentsCost() const
{
    // The first time reset costs 1 gold
    if (m_resetTalentsCost < 1 * GOLD)
        return 1 * GOLD;
    // then 5 gold
    else if (m_resetTalentsCost < 5 * GOLD)
        return 5 * GOLD;
    // After that it increases in increments of 5 gold
    else if (m_resetTalentsCost < 10 * GOLD)
        return 10 * GOLD;
    else
    {
        time_t months =
            (WorldTimer::time_no_syscall() - m_resetTalentsTime) / MONTH;
        if (months > 0)
        {
            // This cost will be reduced by a rate of 5 gold per month
            int32 new_cost = int32((m_resetTalentsCost)-5 * GOLD * months);
            // to a minimum of 10 gold.
            return uint32(new_cost < 10 * GOLD ? 10 * GOLD : new_cost);
        }
        else
        {
            // After that it increases in increments of 5 gold
            int32 new_cost = m_resetTalentsCost + 5 * GOLD;
            // until it hits a cap of 50 gold.
            if (new_cost > 50 * GOLD)
                new_cost = 50 * GOLD;
            return new_cost;
        }
    }
}

bool Player::resetTalents(bool no_cost)
{
    // not need after this call
    if (HasAtLoginFlag(AT_LOGIN_RESET_TALENTS))
        RemoveAtLoginFlag(AT_LOGIN_RESET_TALENTS, true);

    if (m_usedTalentCount == 0)
    {
        UpdateFreeTalentPoints(false); // for fix if need counter
        return false;
    }

    uint32 cost = 0;

    if (!no_cost)
    {
        cost = resetTalentsCost();

        // XXX
        inventory::transaction trans;
        trans.remove(cost);
        if (!storage().finalize(trans))
        {
            SendBuyError(BUY_ERR_NOT_ENOUGHT_MONEY, nullptr, 0, 0);
            return false;
        }

        m_resetTalentsCost = cost;
        m_resetTalentsTime = WorldTimer::time_no_syscall();
    }

    for (unsigned int i = 0; i < sTalentStore.GetNumRows(); ++i)
    {
        TalentEntry const* talentInfo = sTalentStore.LookupEntry(i);

        if (!talentInfo)
            continue;

        TalentTabEntry const* talentTabInfo =
            sTalentTabStore.LookupEntry(talentInfo->TalentTab);

        if (!talentTabInfo)
            continue;

        // unlearn only talents for character class
        // some spell learned by one class as normal spells or know at creation
        // but another class learn it as talent,
        // to prevent unexpected lost normal learned spell skip another class
        // talents
        if ((getClassMask() & talentTabInfo->ClassMask) == 0)
            continue;

        for (int j = 0; j < MAX_TALENT_RANK; ++j)
            if (talentInfo->RankID[j])
                removeSpell(talentInfo->RankID[j],
                    !IsPassiveSpell(talentInfo->RankID[j]), false);
    }

    UpdateFreeTalentPoints(false);

    // Get our spell family mask
    SpellFamily spellFamily = SPELLFAMILY_WARRIOR;
    switch (getClass())
    {
    case CLASS_WARRIOR:
        spellFamily = SPELLFAMILY_WARRIOR;
        break;
    case CLASS_PALADIN:
        spellFamily = SPELLFAMILY_PALADIN;
        break;
    case CLASS_SHAMAN:
        spellFamily = SPELLFAMILY_SHAMAN;
        break;
    case CLASS_HUNTER:
        spellFamily = SPELLFAMILY_HUNTER;
        break;
    case CLASS_ROGUE:
        spellFamily = SPELLFAMILY_ROGUE;
        break;
    case CLASS_DRUID:
        spellFamily = SPELLFAMILY_DRUID;
        break;
    case CLASS_MAGE:
        spellFamily = SPELLFAMILY_MAGE;
        break;
    case CLASS_PRIEST:
        spellFamily = SPELLFAMILY_PRIEST;
        break;
    case CLASS_WARLOCK:
        spellFamily = SPELLFAMILY_WARLOCK;
        break;
    }

    // Remove all self buffs with this players SpellFamily to prevent exploits
    remove_auras_if([this, spellFamily](AuraHolder* holder)
        {
            return holder->GetCasterGuid() == GetObjectGuid() &&
                   holder->GetSpellProto()->SpellFamilyName == spellFamily &&
                   !holder->IsPassive();
        });

    // cancel all current casts
    InterruptNonMeleeSpells(false);
    // cancel all queued spell hits
    spell_queue().clear();

    RemovePet(PET_SAVE_REAGENTS);
    // clear temporary unsummoned pet
    m_temporaryUnsummonedPetNumber = 0;

    for (auto& data : _pet_store)
    {
        if (data.slot == PET_SAVE_AS_CURRENT)
        {
            data.slot = PET_SAVE_AS_CURRENT;
            data.needs_save = true;
            break;
        }
    }

    return true;
}

Mail* Player::GetMail(uint32 id)
{
    for (auto& elem : m_mail)
    {
        if ((elem)->messageID == id)
        {
            return (elem);
        }
    }
    return nullptr;
}

void Player::_SetCreateBits(UpdateMask* updateMask, Player* target) const
{
    if (target == this)
    {
        Object::_SetCreateBits(updateMask, target);
    }
    else
    {
        for (uint16 index = 0; index < m_valuesCount; index++)
        {
            if (GetUInt32Value(index) != 0 && updateVisualBits.GetBit(index))
                updateMask->SetBit(index);
        }
    }
}

void Player::_SetUpdateBits(UpdateMask* updateMask, Player* target) const
{
    if (target == this)
    {
        Object::_SetUpdateBits(updateMask, target);
    }
    else
    {
        Object::_SetUpdateBits(updateMask, target);
        *updateMask &= updateVisualBits;
    }
}

void Player::InitVisibleBits()
{
    updateVisualBits.SetCount(PLAYER_END);

    // TODO: really implement OWNER_ONLY and GROUP_ONLY. Flags can be found in
    // UpdateFields.h

    updateVisualBits.SetBit(OBJECT_FIELD_GUID);
    updateVisualBits.SetBit(OBJECT_FIELD_TYPE);
    updateVisualBits.SetBit(OBJECT_FIELD_SCALE_X);
    updateVisualBits.SetBit(UNIT_FIELD_CHARM + 0);
    updateVisualBits.SetBit(UNIT_FIELD_CHARM + 1);
    updateVisualBits.SetBit(UNIT_FIELD_SUMMON + 0);
    updateVisualBits.SetBit(UNIT_FIELD_SUMMON + 1);
    updateVisualBits.SetBit(UNIT_FIELD_CHARMEDBY + 0);
    updateVisualBits.SetBit(UNIT_FIELD_CHARMEDBY + 1);
    updateVisualBits.SetBit(UNIT_FIELD_TARGET + 0);
    updateVisualBits.SetBit(UNIT_FIELD_TARGET + 1);
    updateVisualBits.SetBit(UNIT_FIELD_CHANNEL_OBJECT + 0);
    updateVisualBits.SetBit(UNIT_FIELD_CHANNEL_OBJECT + 1);
    updateVisualBits.SetBit(UNIT_FIELD_HEALTH);
    updateVisualBits.SetBit(UNIT_FIELD_POWER1);
    updateVisualBits.SetBit(UNIT_FIELD_POWER2);
    updateVisualBits.SetBit(UNIT_FIELD_POWER3);
    updateVisualBits.SetBit(UNIT_FIELD_POWER4);
    updateVisualBits.SetBit(UNIT_FIELD_POWER5);
    updateVisualBits.SetBit(UNIT_FIELD_MAXHEALTH);
    updateVisualBits.SetBit(UNIT_FIELD_MAXPOWER1);
    updateVisualBits.SetBit(UNIT_FIELD_MAXPOWER2);
    updateVisualBits.SetBit(UNIT_FIELD_MAXPOWER3);
    updateVisualBits.SetBit(UNIT_FIELD_MAXPOWER4);
    updateVisualBits.SetBit(UNIT_FIELD_MAXPOWER5);
    updateVisualBits.SetBit(UNIT_FIELD_LEVEL);
    updateVisualBits.SetBit(UNIT_FIELD_FACTIONTEMPLATE);
    updateVisualBits.SetBit(UNIT_FIELD_BYTES_0);
    updateVisualBits.SetBit(UNIT_FIELD_FLAGS);
    updateVisualBits.SetBit(UNIT_FIELD_FLAGS_2);
    for (uint16 i = UNIT_FIELD_AURA; i < UNIT_FIELD_AURASTATE; ++i)
        updateVisualBits.SetBit(i);
    updateVisualBits.SetBit(UNIT_FIELD_AURASTATE);
    updateVisualBits.SetBit(UNIT_FIELD_BASEATTACKTIME + 0);
    updateVisualBits.SetBit(UNIT_FIELD_BASEATTACKTIME + 1);
    updateVisualBits.SetBit(UNIT_FIELD_BOUNDINGRADIUS);
    updateVisualBits.SetBit(UNIT_FIELD_COMBATREACH);
    updateVisualBits.SetBit(UNIT_FIELD_DISPLAYID);
    updateVisualBits.SetBit(UNIT_FIELD_NATIVEDISPLAYID);
    updateVisualBits.SetBit(UNIT_FIELD_MOUNTDISPLAYID);
    updateVisualBits.SetBit(UNIT_FIELD_BYTES_1);
    updateVisualBits.SetBit(UNIT_FIELD_PETNUMBER);
    updateVisualBits.SetBit(UNIT_FIELD_PET_NAME_TIMESTAMP);
    updateVisualBits.SetBit(UNIT_DYNAMIC_FLAGS);
    updateVisualBits.SetBit(UNIT_CHANNEL_SPELL);
    updateVisualBits.SetBit(UNIT_MOD_CAST_SPEED);
    updateVisualBits.SetBit(UNIT_FIELD_BYTES_2);

    updateVisualBits.SetBit(PLAYER_DUEL_ARBITER + 0);
    updateVisualBits.SetBit(PLAYER_DUEL_ARBITER + 1);
    updateVisualBits.SetBit(PLAYER_FLAGS);
    updateVisualBits.SetBit(PLAYER_GUILDID);
    updateVisualBits.SetBit(PLAYER_GUILDRANK);
    updateVisualBits.SetBit(PLAYER_BYTES);
    updateVisualBits.SetBit(PLAYER_BYTES_2);
    updateVisualBits.SetBit(PLAYER_BYTES_3);
    updateVisualBits.SetBit(PLAYER_DUEL_TEAM);
    updateVisualBits.SetBit(PLAYER_GUILD_TIMESTAMP);

    // PLAYER_QUEST_LOG_x also visible bit on official (but only on
    // party/raid)...
    for (uint32 i = PLAYER_QUEST_LOG_1_1; i < PLAYER_QUEST_LOG_25_2;
         i += MAX_QUEST_OFFSET)
        updateVisualBits.SetBit(i);

    // Players visible items are not inventory stuff
    // 431) = 884 (0x374) = main weapon
    /*XXX:*/
    for (uint32 i = inventory::equipment_start; i < inventory::equipment_end;
         i++)
    {
        uint32 base = item_field_offset(i);
        for (uint32 i = 0; i < 16; ++i)
            updateVisualBits.SetBit(base + i);
    }

    updateVisualBits.SetBit(PLAYER_CHOSEN_TITLE);
}

void Player::BuildCreateUpdateBlockForPlayer(
    UpdateData* data, Player* target) const
{
    if (target == this)
    {
        /*XXX:*/
        const int mask = inventory::personal_storage::iterator::all_personal;
        for (inventory::personal_storage::iterator itr = storage().begin(mask);
             itr != storage().end(); ++itr)
            (*itr)->BuildCreateUpdateBlockForPlayer(data, target);
    }

    Unit::BuildCreateUpdateBlockForPlayer(data, target);
}

void Player::DestroyForPlayer(Player* target) const
{
    Unit::DestroyForPlayer(target);

    int mask;
    if (target == this)
        mask = inventory::personal_storage::iterator::all;
    else
        mask = inventory::personal_storage::iterator::equipment;

    // XXX: previous code would loop over bags and the backpack (but not bag
    // contents)
    //      for everyone. then they would loop over the inventory (including bag
    //      contents)
    //      if this == target. so some items were looped over twice under some
    //      conditions
    //      this is probably a bug, so we removed it, but we need to verify it.

    for (inventory::personal_storage::iterator itr = storage().begin(mask);
         itr != storage().end(); ++itr)
        (*itr)->DestroyForPlayer(target);
}

bool Player::HasSpell(uint32 spell) const
{
    auto itr = m_spells.find(spell);
    return (itr != m_spells.end() && itr->second.state != PLAYERSPELL_REMOVED &&
            !itr->second.disabled);
}

bool Player::HasActiveSpell(uint32 spell) const
{
    auto itr = m_spells.find(spell);
    return (itr != m_spells.end() && itr->second.state != PLAYERSPELL_REMOVED &&
            itr->second.active && !itr->second.disabled);
}

TrainerSpellState Player::GetTrainerSpellState(
    TrainerSpell const* trainer_spell, uint32 reqLevel) const
{
    if (!trainer_spell)
        return TRAINER_SPELL_RED;

    if (!trainer_spell->spell)
        return TRAINER_SPELL_RED;

    // known spell
    if (HasSpell(trainer_spell->spell))
        return TRAINER_SPELL_GRAY;

    // check race/class requirement
    if (!IsSpellFitByClassAndRace(trainer_spell->spell))
        return TRAINER_SPELL_RED;

    bool prof = SpellMgr::IsProfessionSpell(trainer_spell->spell);

    // check level requirement
    if (!prof ||
        GetSession()->GetSecurity() <
            AccountTypes(sWorld::Instance()->getConfig(
                CONFIG_UINT32_TRADE_SKILL_GMIGNORE_LEVEL)))
        if (getLevel() < reqLevel)
            return TRAINER_SPELL_RED;

    if (SpellChainNode const* spell_chain =
            sSpellMgr::Instance()->GetSpellChainNode(trainer_spell->spell))
    {
        // check prev.rank requirement
        if (spell_chain->prev && !HasSpell(spell_chain->prev))
            return TRAINER_SPELL_RED;

        // check additional spell requirement
        if (spell_chain->req && !HasSpell(spell_chain->req))
            return TRAINER_SPELL_RED;
    }

    // check skill requirement
    if (!prof ||
        GetSession()->GetSecurity() <
            AccountTypes(sWorld::Instance()->getConfig(
                CONFIG_UINT32_TRADE_SKILL_GMIGNORE_SKILL)))
        if (trainer_spell->reqSkill &&
            GetBaseSkillValue(trainer_spell->reqSkill) <
                trainer_spell->reqSkillValue)
            return TRAINER_SPELL_RED;

    // exist, already checked at loading
    SpellEntry const* spell = sSpellStore.LookupEntry(trainer_spell->spell);

    // secondary prof. or not prof. spell
    uint32 skill = spell->EffectMiscValue[1];

    if (spell->Effect[1] != SPELL_EFFECT_SKILL ||
        !IsPrimaryProfessionSkill(skill))
        return TRAINER_SPELL_GREEN;

    // check primary prof. limit
    if (sSpellMgr::Instance()->IsPrimaryProfessionFirstRankSpell(spell->Id) &&
        GetFreePrimaryProfessionPoints() == 0)
        return TRAINER_SPELL_GREEN_DISABLED;

    return TRAINER_SPELL_GREEN;
}

/**
 * Deletes a character from the database
 *
 * The way, how the characters will be deleted is decided based on the config
 *option.
 *
 * @see Player::DeleteOldCharacters
 *
 * @param playerguid       the low-GUID from the player which should be deleted
 * @param accountId        the account id from the player
 * @param updateRealmChars when this flag is set, the amount of characters on
 *that realm will be updated in the realmlist
 * @param deleteFinally    if this flag is set, the config option will be
 *ignored and the character will be permanently removed from the database
 */
void Player::DeleteFromDB(ObjectGuid playerguid, uint32 accountId,
    bool updateRealmChars, bool deleteFinally)
{
    // for nonexistent account avoid update realm
    if (accountId == 0)
        updateRealmChars = false;

    uint32 charDelete_method =
        sWorld::Instance()->getConfig(CONFIG_UINT32_CHARDELETE_METHOD);
    uint32 charDelete_minLvl =
        sWorld::Instance()->getConfig(CONFIG_UINT32_CHARDELETE_MIN_LEVEL);

    // if we want to finally delete the character or the character does not meet
    // the level requirement, we set it to mode 0
    if (deleteFinally || Player::GetLevelFromDB(playerguid) < charDelete_minLvl)
        charDelete_method = 0;

    uint32 lowguid = playerguid.GetCounter();

    // convert corpse to bones if exist (to prevent exiting Corpse in World
    // without DB entry)
    // bones will be deleted by corpse/bones deleting thread shortly
    sObjectAccessor::Instance()->ConvertCorpseForPlayer(playerguid);

    // remove from guild
    if (uint32 guildId = GetGuildIdFromDB(playerguid))
    {
        if (Guild* guild = sGuildMgr::Instance()->GetGuildById(guildId))
        {
            if (guild->DelMember(playerguid))
            {
                guild->Disband();
                delete guild;
            }
        }
    }

    // remove from arena teams
    LeaveAllArenaTeams(playerguid);

    // the player was uninvited already on logout so just remove from group
    std::unique_ptr<QueryResult> result(CharacterDatabase.PQuery(
        "SELECT groupId FROM group_member WHERE memberGuid='%u'", lowguid));
    if (result)
    {
        uint32 groupId = (*result)[0].GetUInt32();
        if (Group* group = sObjectMgr::Instance()->GetGroupById(groupId))
            RemoveFromGroup(group, playerguid);
    }

    // remove signs from petitions (also remove petitions if owner);
    RemovePetitionsAndSigns(playerguid, 10);

    switch (charDelete_method)
    {
    // completely remove from the database
    case 0:
    {
        // return back all mails with COD and Item                 0  1
        // 2              3      4       5          6     7
        std::unique_ptr<QueryResult> result(CharacterDatabase.PQuery(
            "SELECT "
            "id,messageType,mailTemplateId,sender,subject,itemTextId,money,has_"
            "items FROM mail WHERE receiver='%u' AND has_items<>0 AND cod<>0",
            lowguid));
        if (result)
        {
            do
            {
                Field* fields = result->Fetch();

                uint32 mail_id = fields[0].GetUInt32();
                uint16 mailType = fields[1].GetUInt16();
                uint16 mailTemplateId = fields[2].GetUInt16();
                uint32 sender = fields[3].GetUInt32();
                std::string subject = fields[4].GetCppString();
                uint32 itemTextId = fields[5].GetUInt32();
                uint32 money = fields[6].GetUInt32();
                bool has_items = fields[7].GetBool();

                // we can return mail now
                // so firstly delete the old one
                CharacterDatabase.PExecute(
                    "DELETE FROM mail WHERE id = '%u'", mail_id);

                // mail not from player
                if (mailType != MAIL_NORMAL)
                {
                    if (has_items)
                        CharacterDatabase.PExecute(
                            "DELETE FROM mail_items WHERE mail_id = '%u'",
                            mail_id);
                    continue;
                }

                MailDraft draft;
                if (mailTemplateId)
                    draft.SetMailTemplate(
                        mailTemplateId, false); // items already included
                else
                    draft.SetSubjectAndBodyId(subject, itemTextId);

                if (has_items)
                {
                    // data needs to be at first place for Item::LoadFromDB
                    //                                                          0    1         2
                    std::unique_ptr<QueryResult> result(
                        CharacterDatabase.PQuery(
                            "SELECT data,item_guid,item_template FROM "
                            "mail_items JOIN item_instance ON item_guid = guid "
                            "WHERE mail_id='%u'",
                            mail_id));
                    if (result)
                    {
                        do
                        {
                            Field* fields2 = result->Fetch();

                            uint32 item_guidlow = fields2[1].GetUInt32();
                            uint32 item_template = fields2[2].GetUInt32();

                            ItemPrototype const* itemProto =
                                ObjectMgr::GetItemPrototype(item_template);
                            if (!itemProto)
                            {
                                CharacterDatabase.PExecute(
                                    "DELETE FROM item_instance WHERE guid = "
                                    "'%u'",
                                    item_guidlow);
                                continue;
                            }

                            auto pItem = new Item(itemProto);
                            if (!pItem->LoadFromDB(
                                    item_guidlow, fields2, playerguid))
                            {
                                // XXX
                                pItem->db_delete();
                                delete pItem;
                                continue;
                            }

                            draft.AddItem(pItem);
                        } while (result->NextRow());
                    }
                }

                CharacterDatabase.PExecute(
                    "DELETE FROM mail_items WHERE mail_id = '%u'", mail_id);

                uint32 pl_account =
                    sObjectMgr::Instance()->GetPlayerAccountIdByGUID(
                        playerguid);

                draft.SetMoney(money).SendReturnToSender(pl_account, playerguid,
                    ObjectGuid(HIGHGUID_PLAYER, sender));
            } while (result->NextRow());
        }

        // unsummon and delete for pets in world is not required: player deleted
        // from CLI or character list with not loaded pet.
        // Get guids of character's pets, will deleted in transaction
        std::unique_ptr<QueryResult> resultPets(CharacterDatabase.PQuery(
            "SELECT id FROM character_pet WHERE owner = '%u'", lowguid));

        // delete char from friends list when selected chars is online (non
        // existing - error)
        std::unique_ptr<QueryResult> resultFriend(CharacterDatabase.PQuery(
            "SELECT DISTINCT guid FROM character_social WHERE friend = '%u'",
            lowguid));

        // NOW we can finally clear other DB data related to character
        CharacterDatabase.BeginTransaction();
        if (resultPets)
        {
            do
            {
                Field* fields3 = resultPets->Fetch();
                uint32 petguidlow = fields3[0].GetUInt32();
                drop_pet_db_data(petguidlow);
            } while (resultPets->NextRow());
        }

        // cleanup friends for online players, offline case will cleanup later
        // in code
        if (resultFriend)
        {
            do
            {
                Field* fieldsFriend = resultFriend->Fetch();
                if (Player* sFriend =
                        sObjectAccessor::Instance()->FindPlayer(ObjectGuid(
                            HIGHGUID_PLAYER, fieldsFriend[0].GetUInt32())))
                {
                    if (sFriend->IsInWorld())
                    {
                        sFriend->GetSocial()->RemoveFromSocialList(
                            playerguid, false);
                        sSocialMgr::Instance()->SendFriendStatus(
                            sFriend, FRIEND_REMOVED, playerguid, false);
                    }
                }
            } while (resultFriend->NextRow());
        }

        CharacterDatabase.PExecute(
            "DELETE FROM characters WHERE guid = '%u'", lowguid);
        CharacterDatabase.PExecute(
            "DELETE FROM character_declinedname WHERE guid = '%u'", lowguid);
        CharacterDatabase.PExecute(
            "DELETE FROM character_action WHERE guid = '%u'", lowguid);
        CharacterDatabase.PExecute(
            "DELETE FROM character_aura WHERE guid = '%u'", lowguid);
        CharacterDatabase.PExecute(
            "DELETE FROM character_battleground_data WHERE guid = '%u'",
            lowguid);
        CharacterDatabase.PExecute(
            "DELETE FROM character_gifts WHERE guid = '%u'", lowguid);
        CharacterDatabase.PExecute(
            "DELETE FROM character_homebind WHERE guid = '%u'", lowguid);
        CharacterDatabase.PExecute(
            "DELETE FROM character_instance WHERE guid = '%u'", lowguid);
        CharacterDatabase.PExecute(
            "DELETE FROM character_inventory WHERE guid = '%u'", lowguid);
        CharacterDatabase.PExecute(
            "DELETE FROM character_queststatus WHERE guid = '%u'", lowguid);
        CharacterDatabase.PExecute(
            "DELETE FROM character_queststatus_daily WHERE guid = '%u'",
            lowguid);
        CharacterDatabase.PExecute(
            "DELETE FROM character_reputation WHERE guid = '%u'", lowguid);
        CharacterDatabase.PExecute(
            "DELETE FROM character_skills WHERE guid = '%u'", lowguid);
        CharacterDatabase.PExecute(
            "DELETE FROM character_spell WHERE guid = '%u'", lowguid);
        CharacterDatabase.PExecute(
            "DELETE FROM character_spell_cooldown WHERE guid = '%u'", lowguid);
        CharacterDatabase.PExecute(
            "DELETE FROM item_instance WHERE owner_guid = '%u'", lowguid);
        CharacterDatabase.PExecute(
            "DELETE FROM character_social WHERE guid = '%u' OR friend='%u'",
            lowguid, lowguid);
        CharacterDatabase.PExecute(
            "DELETE FROM mail WHERE receiver = '%u'", lowguid);
        CharacterDatabase.PExecute(
            "DELETE FROM mail_items WHERE receiver = '%u'", lowguid);
        CharacterDatabase.PExecute(
            "DELETE FROM character_pet WHERE owner = '%u'", lowguid);
        CharacterDatabase.PExecute(
            "DELETE FROM character_pet_declinedname WHERE owner = '%u'",
            lowguid);
        CharacterDatabase.PExecute(
            "DELETE FROM guild_eventlog WHERE PlayerGuid1 = '%u' OR "
            "PlayerGuid2 = '%u'",
            lowguid, lowguid);
        CharacterDatabase.PExecute(
            "DELETE FROM guild_bank_eventlog WHERE PlayerGuid = '%u'", lowguid);
        CharacterDatabase.CommitTransaction();
        break;
    }
    // The character gets unlinked from the account, the name gets freed up and
    // appears as deleted ingame
    case 1:
        CharacterDatabase.PExecute(
            "UPDATE characters SET deleteInfos_Name=name, "
            "deleteInfos_Account=account, deleteDate='" UI64FMTD
            "', name='', account=0 WHERE guid=%u",
            uint64(WorldTimer::time_no_syscall()), lowguid);
        break;
    default:
        logging.error("Player::DeleteFromDB: Unsupported delete method: %u.",
            charDelete_method);
    }

    if (updateRealmChars)
        sWorld::Instance()->UpdateRealmCharCount(accountId);
}

/**
 * Characters which were kept back in the database after being deleted and are
 *now too old (see config option "CharDelete.KeepDays"), will be completely
 *deleted.
 *
 * @see Player::DeleteFromDB
 */
void Player::DeleteOldCharacters()
{
    uint32 keepDays =
        sWorld::Instance()->getConfig(CONFIG_UINT32_CHARDELETE_KEEP_DAYS);
    if (!keepDays)
        return;

    Player::DeleteOldCharacters(keepDays);
}

/**
 * Characters which were kept back in the database after being deleted and are
 *older than the specified amount of days, will be completely deleted.
 *
 * @see Player::DeleteFromDB
 *
 * @param keepDays overrite the config option by another amount of days
 */
void Player::DeleteOldCharacters(uint32 keepDays)
{
    logging.info(
        "Player::DeleteOldChars: Deleting all characters which have been "
        "deleted %u days before...",
        keepDays);

    std::unique_ptr<QueryResult> result(CharacterDatabase.PQuery(
        "SELECT guid, deleteInfos_Account FROM characters WHERE deleteDate IS "
        "NOT NULL AND deleteDate < '" UI64FMTD "'",
        uint64(WorldTimer::time_no_syscall() - time_t(keepDays * DAY))));
    if (result)
    {
        logging.info("Player::DeleteOldChars: Found %u character(s) to delete",
            uint32(result->GetRowCount()));
        do
        {
            Field* charFields = result->Fetch();
            ObjectGuid guid =
                ObjectGuid(HIGHGUID_PLAYER, charFields[0].GetUInt32());
            Player::DeleteFromDB(guid, charFields[1].GetUInt32(), true, true);
        } while (result->NextRow());
    }
}

void Player::SetMovement(PlayerMovementType pType)
{
    WorldPacket data;
    switch (pType)
    {
    case MOVE_ROOT:
        data.initialize(SMSG_FORCE_MOVE_ROOT, GetPackGUID().size() + 4);
        break;
    case MOVE_UNROOT:
        data.initialize(SMSG_FORCE_MOVE_UNROOT, GetPackGUID().size() + 4);
        break;
    case MOVE_WATER_WALK:
        data.initialize(SMSG_MOVE_WATER_WALK, GetPackGUID().size() + 4);
        break;
    case MOVE_LAND_WALK:
        data.initialize(SMSG_MOVE_LAND_WALK, GetPackGUID().size() + 4);
        break;
    default:
        logging.error(
            "Player::SetMovement: Unsupported move type (%d), data not sent to "
            "client.",
            pType);
        return;
    }
    data << GetPackGUID();
    data << uint32(0);
    GetSession()->send_packet(std::move(data));
}

/* Preconditions:
  - a resurrectable corpse must not be loaded for the player (only bones)
  - the player must be in world
*/
void Player::BuildPlayerRepop(bool spawn_corpse)
{
    if (getRace() == RACE_NIGHTELF)
        CastSpell(this, 20584, true); // auras SPELL_AURA_INCREASE_SPEED(+speed
                                      // in wisp form),
                                      // SPELL_AURA_INCREASE_SWIM_SPEED(+swim
                                      // speed in wisp form),
                                      // SPELL_AURA_TRANSFORM (to wisp form)
    CastSpell(this, 8326, true);      // auras SPELL_AURA_GHOST,
                                      // SPELL_AURA_INCREASE_SPEED(why?),
                                      // SPELL_AURA_INCREASE_SWIM_SPEED(why?)

    // there must be SMSG.FORCE_RUN_SPEED_CHANGE, SMSG.FORCE_SWIM_SPEED_CHANGE,
    // SMSG.MOVE_WATER_WALK
    // there must be SMSG.STOP_MIRROR_TIMER
    // there we must send 888 opcode

    // the player cannot have a corpse already, only bones which are not
    // returned by GetCorpse
    if (GetCorpse())
    {
        logging.error("BuildPlayerRepop: player %s(%d) already has a corpse",
            GetName(), GetGUIDLow());
        assert(false);
    }

    // create a corpse and place it at the player's location
    if (spawn_corpse)
    {
        Corpse* corpse = CreateCorpse();
        if (!corpse)
        {
            logging.error("Error creating corpse for Player %s [%u]", GetName(),
                GetGUIDLow());
            return;
        }
        if (!GetMap()->insert(corpse))
        {
            delete corpse;
            return;
        }

        // to prevent cheating
        corpse->ResetGhostTime();

        SendCorpseReclaimDelay();
    }

    // convert player body to ghost
    SetHealth(1);

    SetMovement(MOVE_WATER_WALK);
    if (!GetSession()->isLogingOut())
        SetMovement(MOVE_UNROOT);

    // BG - remove insignia related
    RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_SKINNABLE);

    StopMirrorTimers(); // disable timers(bars)

    // set and clear other
    SetByteValue(UNIT_FIELD_BYTES_1, 3, UNIT_BYTE1_FLAG_ALWAYS_STAND);
}

void Player::ResurrectPlayer(float restore_percent, bool applySickness)
{
    WorldPacket data(
        SMSG_DEATH_RELEASE_LOC, 4 * 4); // remove spirit healer position
    data << uint32(-1);
    data << float(0);
    data << float(0);
    data << float(0);
    GetSession()->send_packet(std::move(data));

    // speed change, land walk

    // remove death flag + set aura
    SetByteValue(UNIT_FIELD_BYTES_1, 3, 0x00);

    SetDeathState(ALIVE);

    if (getRace() == RACE_NIGHTELF)
        remove_auras(20584); // speed bonuses
    remove_auras(8326);      // SPELL_AURA_GHOST

    SetMovement(MOVE_LAND_WALK);
    SetMovement(MOVE_UNROOT);

    m_deathTimer = 0;

    // set health/powers (0- will be set in caller)
    if (restore_percent > 0.0f)
    {
        SetHealth(uint32(GetMaxHealth() * restore_percent));
        SetPower(POWER_MANA, uint32(GetMaxPower(POWER_MANA) * restore_percent));
        SetPower(POWER_RAGE, 0);
        SetPower(
            POWER_ENERGY, uint32(GetMaxPower(POWER_ENERGY) * restore_percent));
    }

    // trigger update zone for alive state zone updates
    uint32 newzone, newarea;
    GetZoneAndAreaId(newzone, newarea);
    UpdateZone(newzone, newarea);

    // update visibility of world around viewpoint
    m_camera.UpdateVisibilityForOwner();
    // update visibility of player for nearby cameras
    UpdateObjectVisibility();

    if (!applySickness)
        return;

    // Characters from level 1-10 are not affected by resurrection sickness.
    // Characters from level 11-19 will suffer from one minute of sickness
    // for each level they are above 10.
    // Characters level 20 and up suffer from ten minutes of sickness.
    int32 startLevel =
        sWorld::Instance()->getConfig(CONFIG_INT32_DEATH_SICKNESS_LEVEL);

    if (int32(getLevel()) >= startLevel)
    {
        // set resurrection sickness
        CastSpell(this, SPELL_ID_PASSIVE_RESURRECTION_SICKNESS, true);

        // not full duration
        if (int32(getLevel()) < startLevel + 9)
        {
            int32 delta = (int32(getLevel()) - startLevel + 1) * MINUTE;

            if (AuraHolder* holder =
                    get_aura(SPELL_ID_PASSIVE_RESURRECTION_SICKNESS))
            {
                holder->SetAuraDuration(delta * IN_MILLISECONDS);
                holder->UpdateAuraDuration();
            }
        }
    }
}

void Player::KillPlayer()
{
    SetMovement(MOVE_ROOT);

    StopMirrorTimers(); // disable timers(bars)

    SetDeathState(CORPSE);
    // SetFlag( UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_IN_PVP );

    SetUInt32Value(UNIT_DYNAMIC_FLAGS, UNIT_DYNFLAG_NONE);
    ApplyModByteFlag(PLAYER_FIELD_BYTES, 0, PLAYER_FIELD_BYTE_RELEASE_TIMER,
        !sMapStore.LookupEntry(GetMapId())->Instanceable());

    // 6 minutes until repop at graveyard
    m_deathTimer = 6 * MINUTE * IN_MILLISECONDS;

    UpdateCorpseReclaimDelay(); // dependent at use SetDeathPvP() call before
                                // kill

    // don't create corpse at this moment, player might be falling

    // update visibility
    UpdateObjectVisibility();
}

Corpse* Player::CreateCorpse()
{
    // prevent existence 2 corpse for player
    SpawnCorpseBones();

    auto corpse = new Corpse((m_ExtraFlags & PLAYER_EXTRA_PVP_DEATH) ?
                                 CORPSE_RESURRECTABLE_PVP :
                                 CORPSE_RESURRECTABLE_PVE);
    SetPvPDeath(false);

    if (!corpse->Create(sObjectMgr::Instance()->GenerateCorpseLowGuid(), this))
    {
        delete corpse;
        return nullptr;
    }

    uint8 skin = GetByteValue(PLAYER_BYTES, 0);
    uint8 face = GetByteValue(PLAYER_BYTES, 1);
    uint8 hairstyle = GetByteValue(PLAYER_BYTES, 2);
    uint8 haircolor = GetByteValue(PLAYER_BYTES, 3);
    uint8 facialhair = GetByteValue(PLAYER_BYTES_2, 0);

    corpse->SetByteValue(CORPSE_FIELD_BYTES_1, 1, getRace());
    corpse->SetByteValue(CORPSE_FIELD_BYTES_1, 2, getGender());
    corpse->SetByteValue(CORPSE_FIELD_BYTES_1, 3, skin);

    corpse->SetByteValue(CORPSE_FIELD_BYTES_2, 0, face);
    corpse->SetByteValue(CORPSE_FIELD_BYTES_2, 1, hairstyle);
    corpse->SetByteValue(CORPSE_FIELD_BYTES_2, 2, haircolor);
    corpse->SetByteValue(CORPSE_FIELD_BYTES_2, 3, facialhair);

    uint32 flags = CORPSE_FLAG_UNK2;
    if (HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_HIDE_HELM))
        flags |= CORPSE_FLAG_HIDE_HELM;
    if (HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_HIDE_CLOAK))
        flags |= CORPSE_FLAG_HIDE_CLOAK;
    if (InBattleGround() && !InArena())
        flags |= CORPSE_FLAG_LOOTABLE; // to be able to remove insignia
    corpse->SetUInt32Value(CORPSE_FIELD_FLAGS, flags);

    corpse->SetUInt32Value(CORPSE_FIELD_DISPLAY_ID, GetNativeDisplayId());

    corpse->SetUInt32Value(CORPSE_FIELD_GUILD, GetGuildId());

    uint32 iDisplayID;
    uint32 iIventoryType;
    uint32 _cfi;
    /*XXX:*/
    const int mask = inventory::personal_storage::iterator::equipment;
    for (inventory::personal_storage::iterator itr = storage().begin(mask);
         itr != storage().end(); ++itr)
    {
        iDisplayID = (*itr)->GetProto()->DisplayInfoID;
        iIventoryType = (*itr)->GetProto()->InventoryType;

        _cfi = iDisplayID | (iIventoryType << 24);
        corpse->SetUInt32Value(
            CORPSE_FIELD_ITEM + (*itr)->slot().index(), _cfi);
    }

    // we not need saved corpses for BG/arenas
    if (!GetMap()->IsBattleGroundOrArena())
        corpse->SaveToDB();

    // register for player, but not show
    sObjectAccessor::Instance()->AddCorpse(corpse);
    return corpse;
}

void Player::SpawnCorpseBones()
{
    sObjectAccessor::Instance()->ConvertCorpseForPlayer(GetObjectGuid());
}

Corpse* Player::GetCorpse() const
{
    return sObjectAccessor::Instance()->GetCorpseForPlayerGUID(GetObjectGuid());
}

void Player::durability(bool pct, double val, bool inventory)
{
    // XXX
    const int mask = inventory::personal_storage::iterator::equipment |
                     (inventory ?
                             inventory::personal_storage::iterator::inventory |
                                 inventory::personal_storage::iterator::bags :
                             0);
    for (inventory::personal_storage::iterator itr = storage().begin(mask);
         itr != storage().end(); ++itr)
        durability(*itr, pct, val);
}

void Player::durability(Item* item, bool pct, double val)
{
    // XXX
    int32 max_dura = item->GetUInt32Value(ITEM_FIELD_MAXDURABILITY);
    int32 dura = item->GetUInt32Value(ITEM_FIELD_DURABILITY);
    if (max_dura == 0)
        return;

    int32 new_dura;
    if (pct)
    {
        if (val > 0)
            new_dura = static_cast<double>(max_dura) * val;
        else
            new_dura = dura - (static_cast<double>(max_dura) * std::abs(val));
    }
    else
    {
        new_dura = dura + static_cast<int>(val);
    }
    if (new_dura > max_dura)
        new_dura = max_dura;
    else if (new_dura < 0)
        new_dura = 0;

    // Mangos requires item breaking to unapply mods before they break
    if (item->IsEquipped() && new_dura == 0 && dura > 0)
        _ApplyItemMods(item, false);

    item->SetUInt32Value(ITEM_FIELD_DURABILITY, new_dura);

    // Mangos requires item being repaired from broken to apply mods after
    // repair
    if (item->IsEquipped() && new_dura > 0 && dura == 0)
        _ApplyItemMods(item, true);

    LOG_DEBUG(logging,
        "Player::durability: Changing durability of item %u. Before: %u/%u. "
        "After: %u/%u.",
        item->GetEntry(), dura, max_dura, new_dura, max_dura);
    item->mark_for_save();
}

inventory::copper Player::repair_cost(float modifier) const
{
    // XXX
    inventory::copper accumulated(0);

    const int mask = inventory::personal_storage::iterator::equipment |
                     inventory::personal_storage::iterator::inventory |
                     inventory::personal_storage::iterator::bags;
    for (inventory::personal_storage::iterator itr = storage().begin(mask);
         itr != storage().end(); ++itr)
        accumulated += repair_cost(*itr, modifier);

    return accumulated;
}

// The exact_amount is an ugly solution for the fact that we can't truncate
// before we've accumulated everything for the above function
inventory::copper Player::repair_cost(Item* item, float modifier) const
{
    // XXX
    int32 max_dura = item->GetUInt32Value(ITEM_FIELD_MAXDURABILITY);
    int32 dura = item->GetUInt32Value(ITEM_FIELD_DURABILITY);
    if (max_dura == 0 || max_dura == dura)
        return 0;

    const ItemPrototype* prototype = item->GetProto();
    const DurabilityCostsEntry* cost_entry =
        sDurabilityCostsStore.LookupEntry(prototype->ItemLevel);
    const DurabilityQualityEntry* quality_entry =
        sDurabilityQualityStore.LookupEntry((prototype->Quality + 1) * 2);
    if (!cost_entry || !quality_entry)
        return 0;

    uint32 multi = cost_entry->multiplier[ItemSubClassToDurabilityMultiplierId(
        prototype->Class, prototype->SubClass)];
    uint32 cost = (max_dura - dura) * multi * quality_entry->quality_mod;

    cost *= modifier;

    if (cost == 0)
        cost = 1;

    return cost;
}

void Player::rand_equip_dura(bool include_weapons)
{
    // Choose one of player's equipped items that have a durability to substract
    // one point from
    // TODO: This makes it so having small amounts of gear make them deteriate
    // pretty quickly
    // I considered making it only take a random durability-able slot, and if
    // that one didn't have
    // an item ignore the operation. But then I realized that even low level
    // characters have a
    // couple of items equipped, and that it's probably better to keep it like
    // this.

    std::vector<Item*> target_items;
    target_items.reserve(include_weapons ? 16 : 8);

    const int mask = inventory::personal_storage::iterator::equipment;
    for (inventory::personal_storage::iterator itr = storage().begin(mask);
         itr != storage().end(); ++itr)
    {
        if (!include_weapons &&
            ((*itr)->slot().main_hand() || (*itr)->slot().off_hand() ||
                (*itr)->slot().ranged()))
            continue;
        // Skip broken items
        if ((*itr)->IsBroken())
            continue;

        if ((*itr)->GetProto()->MaxDurability > 0)
            target_items.push_back(*itr);
    }

    if (target_items.empty())
        return;

    durability(target_items[urand(0, target_items.size() - 1)], false, -1);
}

void Player::weap_dura_loss(WeaponAttackType attack_type)
{
    inventory::slot s;
    switch (attack_type)
    {
    case BASE_ATTACK:
        s = inventory::slot(inventory::personal_slot, inventory::main_bag,
            inventory::main_hand_e);
        break;
    case OFF_ATTACK:
        s = inventory::slot(inventory::personal_slot, inventory::main_bag,
            inventory::off_hand_e);
        break;
    case RANGED_ATTACK:
        s = inventory::slot(
            inventory::personal_slot, inventory::main_bag, inventory::ranged_e);
        break;
    }

    Item* item = storage().get(s);
    if (!item)
        return;

    // Drop one point of durability
    durability(item, false, -1);
}

void Player::RepopAtGraveyard()
{
    // note: this can be called also when the player is alive
    // for example from WorldSession::HandleMovementOpcodes

    AreaTableEntry const* zone = GetAreaEntryByAreaID(GetAreaId());

    // Such zones are considered unreachable as a ghost and the player must be
    // automatically revived
    if ((!isAlive() && zone && zone->flags & AREA_FLAG_NEED_FLY) ||
        GetTransport())
    {
        // We cannot resurrect the player right away, as he might be in transit
        // to a new map. Instead, add an aura that will resurrect him later on.
        CastSpell(this, 150055, true);
    }

    WorldSafeLocsEntry const* ClosestGrave = nullptr;

    // Special handle for battleground maps
    if (BattleGround* bg = GetBattleGround())
        ClosestGrave = bg->GetClosestGraveYard(this);
    else
    {
        const WorldSafeLocsEntry* corpse_loc =
            sObjectMgr::Instance()->GetCorpseSafeLoc(
                GetX(), GetY(), GetZ(), GetMapId(), GetTeam());
        if (corpse_loc)
            ClosestGrave =
                sObjectMgr::Instance()->GetClosestGraveyard(corpse_loc->x,
                    corpse_loc->y, corpse_loc->z, GetMapId(), GetTeam());
        else
            ClosestGrave = sObjectMgr::Instance()->GetClosestGraveyard(
                GetX(), GetY(), GetZ(), GetMapId(), GetTeam());
    }

    // stop countdown until repop
    m_deathTimer = 0;

    // if no grave found, stay at the current location
    // and don't show spirit healer location
    if (ClosestGrave)
    {
        TeleportTo(ClosestGrave->map_id, ClosestGrave->x, ClosestGrave->y,
            ClosestGrave->z, GetO());
        if (isDead()) // not send if alive, because it used in TeleportTo()
        {
            WorldPacket data(SMSG_DEATH_RELEASE_LOC,
                4 * 4); // show spirit healer position on minimap
            data << ClosestGrave->map_id;
            data << ClosestGrave->x;
            data << ClosestGrave->y;
            data << ClosestGrave->z;
            GetSession()->send_packet(std::move(data));
            // Update visibility, or we might not see the spirit healer if we
            // were just on him when we died
            if (IsInWorld())
                GetCamera().UpdateVisibilityForOwner();
        }
    }
}

void Player::JoinedChannel(Channel* c)
{
    m_channels.push_back(c);
}

void Player::LeftChannel(Channel* c)
{
    m_channels.remove(c);
}

void Player::CleanupChannels()
{
    while (!m_channels.empty())
    {
        Channel* ch = *m_channels.begin();
        m_channels.erase(
            m_channels.begin()); // remove from player's channel list
        ch->Leave(GetObjectGuid(),
            false); // not send to client, not remove from player's channel list
        if (ChannelMgr* cMgr = channelMgr(GetTeam()))
            cMgr->LeftChannel(ch->GetName()); // deleted channel if empty
    }
    LOG_DEBUG(logging, "Player: channels cleaned up!");
}

void Player::UpdateLocalChannels(uint32 newZone)
{
    ChannelMgr* cMgr = channelMgr(GetTeam());
    if (!cMgr)
        return;

    auto zone = GetAreaEntryByAreaID(newZone);
    if (!zone)
        return;

    for (uint32 i = 0; i < sChatChannelsStore.GetNumRows(); ++i)
    {
        auto chan_data = sChatChannelsStore.LookupEntry(i);
        if (!chan_data)
            continue;

        Channel* current_chan = nullptr;
        Channel* leave_chan = nullptr;
        Channel* join_chan = nullptr;
        bool send_leave = true;

        for (auto chan : m_channels)
            if (chan->GetChannelId() == i)
            {
                current_chan = chan;
                break;
            }

        if (!CanJoinConstantChannel(chan_data, newZone))
        {
            leave_chan = current_chan;
            goto update_local_channels;
        }

        // Local Channels, name depend on zone
        if ((chan_data->flags & CHANNEL_DBC_FLAG_GLOBAL) == 0)
        {
            // Skip rejoining Trade, Guild Recruitment; it has the same name no
            // matter the city we're in, as they're all connected
            if (chan_data->flags & CHANNEL_DBC_FLAG_CITY_ONLY && current_chan)
                continue;

            std::string zone_name;

            // Zone %s is "City" for Trade/Guild Recruitment
            if (chan_data->flags & CHANNEL_DBC_FLAG_CITY_ONLY)
                zone_name = GetSession()->GetMangosString(LANG_CHANNEL_CITY);
            // Otherwise, the name of the zone
            else
                zone_name =
                    zone->area_name[GetSession()->GetSessionDbcLocale()];

            std::string chan_name =
                chan_data->pattern[m_session->GetSessionDbcLocale()];

            boost::replace_all(chan_name, "%s", zone_name);

            join_chan = cMgr->GetJoinChannel(chan_name, chan_data->ChannelID);
            if (current_chan)
            {
                if (join_chan != current_chan)
                {
                    leave_chan = current_chan;
                    send_leave = false;
                }
                else
                    join_chan = nullptr;
            }
        }
        // Global Channels, constant name
        else
        {
            join_chan = cMgr->GetJoinChannel(
                chan_data->pattern[m_session->GetSessionDbcLocale()],
                chan_data->ChannelID);
        }

    update_local_channels:

        if (join_chan)
            join_chan->Join(GetObjectGuid(), "");

        if (leave_chan)
        {
            leave_chan->Leave(GetObjectGuid(), send_leave);
            auto name =
                leave_chan->GetName(); // Chan can be erased in LeftChannel
            LeftChannel(leave_chan);
            cMgr->LeftChannel(name);
        }
    }
}

// THREAD UNSAFE: ONLY CALL FROM NON-THREADED SCENARIO
void Player::JoinLFGChannel()
{
    if (!CanJoinLFGChannel())
        return;

    // FIXME: figure out how to make the client join the LFG channel
    /*if (ChannelMgr* mgr = channelMgr(GetTeam()))
        if (Channel* chn = mgr->GetJoinChannel("lookingforgroup", 26))
            chn->Join(GetObjectGuid(), "");*/
}

// THREAD UNSAFE: ONLY CALL FROM NON-THREADED SCENARIO
void Player::LeaveLFGChannel()
{
    for (auto& elem : m_channels)
    {
        if ((elem)->IsLFG())
        {
            (elem)->Leave(GetObjectGuid());
            break;
        }
    }
}

bool Player::CanJoinLFGChannel()
{
    return !sWorld::Instance()->getConfig(CONFIG_BOOL_RESTRICTED_LFG_CHANNEL) ||
           GetSession()->GetSecurity() > SEC_PLAYER ||
           sLfgToolContainer::Instance()->in_tool(GetSession());
}

bool Player::CanJoinConstantChannel(
    const ChatChannelsEntry* channel, uint32 zone_id)
{
    auto flags = channel->flags;
    auto zone = GetAreaEntryByAreaID(zone_id);
    if (!zone)
        return false;

    // Trade & Guild Recruitment can only be joined in Cities
    if (flags & CHANNEL_DBC_FLAG_CITY_ONLY)
    {
        if (!zone || (zone->flags & AREA_FLAG_SLAVE_CAPITAL) == 0)
            return false;
    }

    // Cannot join zone-dependent channels in Arena (like general chat)
    if (flags & CHANNEL_DBC_FLAG_ZONE_DEP &&
        zone->flags & AREA_FLAG_ARENA_INSTANCE)
        return false;

    return true;
}

void Player::UpdateDefense()
{
    uint32 defense_skill_gain =
        sWorld::Instance()->getConfig(CONFIG_UINT32_SKILL_GAIN_DEFENSE);

    if (UpdateSkill(SKILL_DEFENSE, defense_skill_gain))
    {
        // update dependent from defense skill part
        UpdateDefenseBonusesMod();
    }
}

void Player::HandleBaseModValue(
    BaseModGroup modGroup, BaseModType modType, float amount, bool apply)
{
    // TODO: go through all callees and remove this check
    if (modGroup >= BASEMOD_END || modType >= MOD_END)
    {
        logging.error(
            "ERROR in HandleBaseModValue(): nonexistent BaseModGroup of wrong "
            "BaseModType!");
        return;
    }

    float val = 1.0f;

    switch (modType)
    {
    case FLAT_MOD:
        m_auraBaseMod[modGroup][modType] += apply ? amount : -amount;
        break;
    case PCT_MOD:
        if (amount <= -100.0f)
            amount = -200.0f;

        val = (100.0f + amount) / 100.0f;
        m_auraBaseMod[modGroup][modType] *= apply ? val : (1.0f / val);
        break;
    }

    if (!CanModifyStats())
        return;

    switch (modGroup)
    {
    case CRIT_PERCENTAGE:
        UpdateCritPercentage(BASE_ATTACK);
        break;
    case RANGED_CRIT_PERCENTAGE:
        UpdateCritPercentage(RANGED_ATTACK);
        break;
    case OFFHAND_CRIT_PERCENTAGE:
        UpdateCritPercentage(OFF_ATTACK);
        break;
    case SHIELD_BLOCK_VALUE:
        UpdateShieldBlockValue();
        break;
    default:
        break;
    }
}

float Player::GetBaseModValue(BaseModGroup modGroup, BaseModType modType) const
{
    // TODO: go through all callees and remove this check
    if (modGroup >= BASEMOD_END || modType >= MOD_END)
    {
        logging.error(
            "trial to access nonexistent BaseModGroup or wrong BaseModType!");
        return 0.0f;
    }

    if (modType == PCT_MOD && m_auraBaseMod[modGroup][PCT_MOD] <= 0.0f)
        return 0.0f;

    return m_auraBaseMod[modGroup][modType];
}

float Player::GetTotalBaseModValue(BaseModGroup modGroup) const
{
    if (modGroup >= BASEMOD_END)
    {
        logging.error("wrong BaseModGroup in GetTotalBaseModValue()!");
        return 0.0f;
    }

    if (m_auraBaseMod[modGroup][PCT_MOD] <= 0.0f)
        return 0.0f;

    return m_auraBaseMod[modGroup][FLAT_MOD] * m_auraBaseMod[modGroup][PCT_MOD];
}

uint32 Player::GetShieldBlockValue() const
{
    float value = (m_auraBaseMod[SHIELD_BLOCK_VALUE][FLAT_MOD] +
                      GetStat(STAT_STRENGTH) / 20 - 1) *
                  m_auraBaseMod[SHIELD_BLOCK_VALUE][PCT_MOD];

    value = (value < 0) ? 0 : value;

    return uint32(value);
}

float Player::GetMeleeCritFromAgility()
{
    uint32 level = getLevel();
    uint32 pclass = getClass();

    if (level > GT_MAX_LEVEL)
        level = GT_MAX_LEVEL;

    GtChanceToMeleeCritBaseEntry const* critBase =
        sGtChanceToMeleeCritBaseStore.LookupEntry(pclass - 1);
    GtChanceToMeleeCritEntry const* critRatio =
        sGtChanceToMeleeCritStore.LookupEntry(
            (pclass - 1) * GT_MAX_LEVEL + level - 1);
    if (critBase == nullptr || critRatio == nullptr)
        return 0.0f;

    float crit = critBase->base + GetStat(STAT_AGILITY) * critRatio->ratio;
    return crit * 100.0f;
}

float Player::GetDodgeFromAgility()
{
    // Table for base dodge values
    const float dodge_base[MAX_CLASSES] = {
        0.0075f,   // Warrior
        0.00652f,  // Paladin
        -0.0545f,  // Hunter
        -0.0059f,  // Rogue
        0.03183f,  // Priest
        0.0114f,   // DK
        0.0167f,   // Shaman
        0.034575f, // Mage
        0.02011f,  // Warlock
        0.0f,      // ??
        -0.0187f   // Druid
    };
    // Crit/agility to dodge/agility coefficient multipliers
    const float crit_to_dodge[MAX_CLASSES] = {
        1.1f, // Warrior
        1.0f, // Paladin
        1.6f, // Hunter
        2.0f, // Rogue
        1.0f, // Priest
        1.0f, // DK?
        1.0f, // Shaman
        1.0f, // Mage
        1.0f, // Warlock
        0.0f, // ??
        1.7f  // Druid
    };

    uint32 level = getLevel();
    uint32 pclass = getClass();

    if (level > GT_MAX_LEVEL)
        level = GT_MAX_LEVEL;

    // Dodge per agility for most classes equal crit per agility (but for some
    // classes need apply some multiplier)
    GtChanceToMeleeCritEntry const* dodgeRatio =
        sGtChanceToMeleeCritStore.LookupEntry(
            (pclass - 1) * GT_MAX_LEVEL + level - 1);
    if (dodgeRatio == nullptr || pclass > MAX_CLASSES)
        return 0.0f;

    float dodge =
        dodge_base[pclass - 1] +
        GetStat(STAT_AGILITY) * dodgeRatio->ratio * crit_to_dodge[pclass - 1];
    return dodge * 100.0f;
}

float Player::GetSpellCritFromIntellect()
{
    uint32 level = getLevel();
    uint32 pclass = getClass();

    if (level > GT_MAX_LEVEL)
        level = GT_MAX_LEVEL;

    GtChanceToSpellCritBaseEntry const* critBase =
        sGtChanceToSpellCritBaseStore.LookupEntry(pclass - 1);
    GtChanceToSpellCritEntry const* critRatio =
        sGtChanceToSpellCritStore.LookupEntry(
            (pclass - 1) * GT_MAX_LEVEL + level - 1);
    if (critBase == nullptr || critRatio == nullptr)
        return 0.0f;

    float crit = critBase->base + GetStat(STAT_INTELLECT) * critRatio->ratio;
    return crit * 100.0f;
}

float Player::GetRatingMultiplier(CombatRating cr) const
{
    uint32 level = getLevel();

    if (level > GT_MAX_LEVEL)
        level = GT_MAX_LEVEL;

    GtCombatRatingsEntry const* Rating =
        sGtCombatRatingsStore.LookupEntry(cr * GT_MAX_LEVEL + level - 1);
    if (!Rating)
        return 1.0f; // By default use minimum coefficient (not must be called)

    return 1.0f / Rating->ratio;
}

float Player::GetRatingBonusValue(CombatRating cr) const
{
    return float(GetUInt32Value(PLAYER_FIELD_COMBAT_RATING_1 + cr)) *
           GetRatingMultiplier(cr);
}

uint32 Player::GetMeleeCritDamageReduction(uint32 damage) const
{
    float melee = GetRatingBonusValue(CR_CRIT_TAKEN_MELEE) * 2.0f;
    if (melee > 25.0f)
        melee = 25.0f;
    return uint32(melee * damage / 100.0f);
}

uint32 Player::GetRangedCritDamageReduction(uint32 damage) const
{
    float ranged = GetRatingBonusValue(CR_CRIT_TAKEN_RANGED) * 2.0f;
    if (ranged > 25.0f)
        ranged = 25.0f;
    return uint32(ranged * damage / 100.0f);
}

uint32 Player::GetSpellCritDamageReduction(uint32 damage) const
{
    float spell = GetRatingBonusValue(CR_CRIT_TAKEN_SPELL) * 2.0f;
    // In wow script resilience limited to 25%
    if (spell > 25.0f)
        spell = 25.0f;
    return uint32(spell * damage / 100.0f);
}

uint32 Player::GetDotDamageReduction(uint32 damage) const
{
    float spellDot = GetRatingBonusValue(CR_CRIT_TAKEN_SPELL);
    // Dot resilience not limited (limit it by 100%)
    if (spellDot > 100.0f)
        spellDot = 100.0f;
    return uint32(spellDot * damage / 100.0f);
}

float Player::GetExpertiseDodgeOrParryReduction(WeaponAttackType attType) const
{
    switch (attType)
    {
    case BASE_ATTACK:
        return GetUInt32Value(PLAYER_EXPERTISE) / 4.0f;
    case OFF_ATTACK:
        return GetUInt32Value(PLAYER_OFFHAND_EXPERTISE) / 4.0f;
    default:
        break;
    }
    return 0.0f;
}

float Player::OCTRegenHPPerSpirit()
{
    uint32 level = getLevel();
    uint32 pclass = getClass();

    if (level > GT_MAX_LEVEL)
        level = GT_MAX_LEVEL;

    GtOCTRegenHPEntry const* baseRatio =
        sGtOCTRegenHPStore.LookupEntry((pclass - 1) * GT_MAX_LEVEL + level - 1);
    GtRegenHPPerSptEntry const* moreRatio = sGtRegenHPPerSptStore.LookupEntry(
        (pclass - 1) * GT_MAX_LEVEL + level - 1);
    if (baseRatio == nullptr || moreRatio == nullptr)
        return 0.0f;

    // Formula from PaperDollFrame script
    float spirit = GetStat(STAT_SPIRIT);
    float baseSpirit = spirit;
    if (baseSpirit > 50)
        baseSpirit = 50;
    float moreSpirit = spirit - baseSpirit;
    float regen = baseSpirit * baseRatio->ratio + moreSpirit * moreRatio->ratio;
    return regen;
}

float Player::OCTRegenMPPerSpirit()
{
    uint32 level = getLevel();
    uint32 pclass = getClass();

    if (level > GT_MAX_LEVEL)
        level = GT_MAX_LEVEL;

    //    GtOCTRegenMPEntry     const *baseRatio =
    //    sGtOCTRegenMPStore.LookupEntry((pclass-1)*GT_MAX_LEVEL + level-1);
    GtRegenMPPerSptEntry const* moreRatio = sGtRegenMPPerSptStore.LookupEntry(
        (pclass - 1) * GT_MAX_LEVEL + level - 1);
    if (moreRatio == nullptr)
        return 0.0f;

    // Formula get from PaperDollFrame script
    float spirit = GetStat(STAT_SPIRIT);
    float regen = spirit * moreRatio->ratio;
    return regen;
}

void Player::ApplyRatingMod(CombatRating cr, int32 value, bool apply)
{
    m_baseRatingValue[cr] += (apply ? value : -value);

    // explicit affected values
    switch (cr)
    {
    case CR_HASTE_MELEE:
    {
        float RatingChange = value * GetRatingMultiplier(cr);
        ApplyAttackTimePercentMod(BASE_ATTACK, RatingChange, apply);
        ApplyAttackTimePercentMod(OFF_ATTACK, RatingChange, apply);
        break;
    }
    case CR_HASTE_RANGED:
    {
        float RatingChange = value * GetRatingMultiplier(cr);
        ApplyAttackTimePercentMod(RANGED_ATTACK, RatingChange, apply);
        break;
    }
    case CR_HASTE_SPELL:
    {
        float RatingChange = value * GetRatingMultiplier(cr);
        ApplyCastTimePercentMod(RatingChange, apply);
        break;
    }
    default:
        break;
    }

    UpdateRating(cr);
}

void Player::UpdateRating(CombatRating cr)
{
    int32 amount = m_baseRatingValue[cr];
    if (amount < 0)
        amount = 0;
    SetUInt32Value(PLAYER_FIELD_COMBAT_RATING_1 + cr, uint32(amount));

    bool affectStats = CanModifyStats();

    switch (cr)
    {
    case CR_WEAPON_SKILL: // Implemented in Unit::RollMeleeOutcomeAgainst
    case CR_DEFENSE_SKILL:
        UpdateDefenseBonusesMod();
        break;
    case CR_DODGE:
        UpdateDodgePercentage();
        break;
    case CR_PARRY:
        UpdateParryPercentage();
        break;
    case CR_BLOCK:
        UpdateBlockPercentage();
        break;
    case CR_HIT_MELEE:
        UpdateMeleeHitChances();
        break;
    case CR_HIT_RANGED:
        UpdateRangedHitChances();
        break;
    case CR_HIT_SPELL:
        UpdateSpellHitChances();
        break;
    case CR_CRIT_MELEE:
        if (affectStats)
        {
            UpdateCritPercentage(BASE_ATTACK);
            UpdateCritPercentage(OFF_ATTACK);
        }
        break;
    case CR_CRIT_RANGED:
        if (affectStats)
            UpdateCritPercentage(RANGED_ATTACK);
        break;
    case CR_CRIT_SPELL:
        if (affectStats)
            UpdateAllSpellCritChances();
        break;
    case CR_HIT_TAKEN_MELEE: // Implemented in Unit::MeleeMissChanceCalc
    case CR_HIT_TAKEN_RANGED:
        break;
    case CR_HIT_TAKEN_SPELL: // Implemented in Unit::MagicSpellHitResult
        break;
    case CR_CRIT_TAKEN_MELEE: // Implemented in Unit::RollMeleeOutcomeAgainst
                              // (only for chance to crit)
    case CR_CRIT_TAKEN_RANGED:
        break;
    case CR_CRIT_TAKEN_SPELL: // Implemented in Unit::SpellCriticalBonus (only
                              // for chance to crit)
        break;
    case CR_HASTE_MELEE: // Implemented in Player::ApplyRatingMod
    case CR_HASTE_RANGED:
    case CR_HASTE_SPELL:
        break;
    case CR_WEAPON_SKILL_MAINHAND: // Implemented in
                                   // Unit::RollMeleeOutcomeAgainst
    case CR_WEAPON_SKILL_OFFHAND:
    case CR_WEAPON_SKILL_RANGED:
        break;
    case CR_EXPERTISE:
        if (affectStats)
        {
            UpdateExpertise(BASE_ATTACK);
            UpdateExpertise(OFF_ATTACK);
        }
        break;
    }
}

void Player::UpdateAllRatings()
{
    for (int cr = 0; cr < MAX_COMBAT_RATING; ++cr)
        UpdateRating(CombatRating(cr));
}

void Player::SetRegularAttackTime()
{
    for (int i = 0; i < MAX_ATTACK; ++i)
    {
        Item* tmpitem = GetWeaponForAttack(WeaponAttackType(i), true, false);
        if (tmpitem)
        {
            ItemPrototype const* proto = tmpitem->GetProto();
            if (proto->Delay)
                SetAttackTime(WeaponAttackType(i), proto->Delay);
            else
                SetAttackTime(WeaponAttackType(i), BASE_ATTACK_TIME);
        }
    }
}

// skill+step, checking for max value
bool Player::UpdateSkill(uint32 skill_id, uint32 step)
{
    if (!skill_id)
        return false;

    auto itr = mSkillStatus.find(skill_id);
    if (itr == mSkillStatus.end() || itr->second.uState == SKILL_DELETED)
        return false;

    uint32 valueIndex = PLAYER_SKILL_VALUE_INDEX(itr->second.pos);
    uint32 data = GetUInt32Value(valueIndex);
    uint32 value = SKILL_VALUE(data);
    uint32 max = SKILL_MAX(data);

    if (!max || !value || value >= max)
        return false;

    uint32 new_value = value + step;
    if (new_value > max)
        new_value = max;

    SetUInt32Value(valueIndex, MAKE_SKILL_VALUE(new_value, max));
    if (itr->second.uState != SKILL_NEW)
        itr->second.uState = SKILL_CHANGED;

    return true;
}

inline int SkillGainChance(
    uint32 SkillValue, uint32 GrayLevel, uint32 GreenLevel, uint32 YellowLevel)
{
    if (SkillValue >= GrayLevel)
        return sWorld::Instance()->getConfig(CONFIG_UINT32_SKILL_CHANCE_GREY) *
               10;
    if (SkillValue >= GreenLevel)
        return sWorld::Instance()->getConfig(CONFIG_UINT32_SKILL_CHANCE_GREEN) *
               10;
    if (SkillValue >= YellowLevel)
        return sWorld::Instance()->getConfig(
                   CONFIG_UINT32_SKILL_CHANCE_YELLOW) *
               10;
    return sWorld::Instance()->getConfig(CONFIG_UINT32_SKILL_CHANCE_ORANGE) *
           10;
}

bool Player::UpdateCraftSkill(uint32 spellid)
{
    LOG_DEBUG(logging, "UpdateCraftSkill spellid %d", spellid);

    SkillLineAbilityMapBounds bounds =
        sSpellMgr::Instance()->GetSkillLineAbilityMapBounds(spellid);

    for (auto _spell_idx = bounds.first; _spell_idx != bounds.second;
         ++_spell_idx)
    {
        if (_spell_idx->second->skillId)
        {
            uint32 SkillValue = GetPureSkillValue(_spell_idx->second->skillId);

            // Alchemy Discoveries here
            SpellEntry const* spellEntry = sSpellStore.LookupEntry(spellid);
            if (spellEntry && spellEntry->Mechanic == MECHANIC_DISCOVERY)
            {
                if (uint32 discoveredSpell = GetSkillDiscoverySpell(
                        _spell_idx->second->skillId, spellid, this))
                    learnSpell(discoveredSpell, false);
            }

            uint32 craft_skill_gain = sWorld::Instance()->getConfig(
                CONFIG_UINT32_SKILL_GAIN_CRAFTING);

            return UpdateSkillPro(_spell_idx->second->skillId,
                SkillGainChance(SkillValue, _spell_idx->second->max_value,
                                      (_spell_idx->second->max_value +
                                          _spell_idx->second->min_value) /
                                          2,
                                      _spell_idx->second->min_value),
                craft_skill_gain);
        }
    }
    return false;
}

bool Player::UpdateGatherSkill(
    uint32 SkillId, uint32 SkillValue, uint32 RedLevel, uint32 Multiplicator)
{
    LOG_DEBUG(logging,
        "UpdateGatherSkill(SkillId %d SkillLevel %d RedLevel %d)", SkillId,
        SkillValue, RedLevel);

    uint32 gathering_skill_gain =
        sWorld::Instance()->getConfig(CONFIG_UINT32_SKILL_GAIN_GATHERING);

    // For skinning and Mining chance decrease with level. 1-74 - no decrease,
    // 75-149 - 2 times, 225-299 - 8 times
    switch (SkillId)
    {
    case SKILL_HERBALISM:
    case SKILL_LOCKPICKING:
    case SKILL_JEWELCRAFTING:
        return UpdateSkillPro(SkillId,
            SkillGainChance(
                SkillValue, RedLevel + 100, RedLevel + 50, RedLevel + 25) *
                Multiplicator,
            gathering_skill_gain);
    case SKILL_SKINNING:
        if (sWorld::Instance()->getConfig(
                CONFIG_UINT32_SKILL_CHANCE_SKINNING_STEPS) == 0)
            return UpdateSkillPro(SkillId,
                SkillGainChance(
                    SkillValue, RedLevel + 100, RedLevel + 50, RedLevel + 25) *
                    Multiplicator,
                gathering_skill_gain);
        else
            return UpdateSkillPro(SkillId,
                (SkillGainChance(
                     SkillValue, RedLevel + 100, RedLevel + 50, RedLevel + 25) *
                    Multiplicator) >>
                    (SkillValue /
                        sWorld::Instance()->getConfig(
                            CONFIG_UINT32_SKILL_CHANCE_SKINNING_STEPS)),
                gathering_skill_gain);
    case SKILL_MINING:
        if (sWorld::Instance()->getConfig(
                CONFIG_UINT32_SKILL_CHANCE_MINING_STEPS) == 0)
            return UpdateSkillPro(SkillId,
                SkillGainChance(
                    SkillValue, RedLevel + 100, RedLevel + 50, RedLevel + 25) *
                    Multiplicator,
                gathering_skill_gain);
        else
            return UpdateSkillPro(SkillId,
                (SkillGainChance(
                     SkillValue, RedLevel + 100, RedLevel + 50, RedLevel + 25) *
                    Multiplicator) >>
                    (SkillValue /
                        sWorld::Instance()->getConfig(
                            CONFIG_UINT32_SKILL_CHANCE_MINING_STEPS)),
                gathering_skill_gain);
    }
    return false;
}

bool Player::UpdateFishingSkill()
{
    LOG_DEBUG(logging, "UpdateFishingSkill");

    uint32 SkillValue = GetPureSkillValue(SKILL_FISHING);

    int32 chance = SkillValue < 75 ? 100 : 2500 / (SkillValue - 50);

    uint32 gathering_skill_gain =
        sWorld::Instance()->getConfig(CONFIG_UINT32_SKILL_GAIN_GATHERING);

    return UpdateSkillPro(SKILL_FISHING, chance * 10, gathering_skill_gain);
}

bool Player::UpdateSkillPro(uint16 SkillId, int32 Chance, uint32 step)
{
    LOG_DEBUG(logging, "UpdateSkillPro(SkillId %d, Chance %3.1f%%)", SkillId,
        Chance / 10.0);
    if (!SkillId)
        return false;

    if (Chance <= 0) // speedup in 0 chance case
    {
        LOG_DEBUG(logging, "Player::UpdateSkillPro Chance=%3.1f%% missed",
            Chance / 10.0);
        return false;
    }

    auto itr = mSkillStatus.find(SkillId);
    if (itr == mSkillStatus.end() || itr->second.uState == SKILL_DELETED)
        return false;

    uint32 valueIndex = PLAYER_SKILL_VALUE_INDEX(itr->second.pos);

    uint32 data = GetUInt32Value(valueIndex);
    uint16 SkillValue = SKILL_VALUE(data);
    uint16 MaxValue = SKILL_MAX(data);

    if (!MaxValue || !SkillValue || SkillValue >= MaxValue)
        return false;

    int32 Roll = irand(1, 1000);

    if (Roll <= Chance)
    {
        uint32 new_value = SkillValue + step;
        if (new_value > MaxValue)
            new_value = MaxValue;

        SetUInt32Value(valueIndex, MAKE_SKILL_VALUE(new_value, MaxValue));
        if (itr->second.uState != SKILL_NEW)
            itr->second.uState = SKILL_CHANGED;
        LOG_DEBUG(logging, "Player::UpdateSkillPro Chance=%3.1f%% taken",
            Chance / 10.0);
        return true;
    }

    LOG_DEBUG(
        logging, "Player::UpdateSkillPro Chance=%3.1f%% missed", Chance / 10.0);
    return false;
}

void Player::UpdateWeaponSkill(WeaponAttackType attType)
{
    // no skill gain in pvp
    Unit* pVictim = getVictim();
    if (pVictim && pVictim->IsCharmerOrOwnerPlayerOrPlayerItself())
        return;

    if (IsInFeralForm())
        return; // always maximized SKILL_FERAL_COMBAT in fact

    if (GetShapeshiftForm() == FORM_TREE)
        return; // use weapon but not skill up

    uint32 weaponSkillGain =
        sWorld::Instance()->getConfig(CONFIG_UINT32_SKILL_GAIN_WEAPON);

    Item* pWeapon = GetWeaponForAttack(attType, true, true);
    if (pWeapon &&
        pWeapon->GetProto()->SubClass != ITEM_SUBCLASS_WEAPON_FISHING_POLE)
        UpdateSkill(pWeapon->GetSkill(), weaponSkillGain);
    else if (!pWeapon && attType == BASE_ATTACK)
        UpdateSkill(SKILL_UNARMED, weaponSkillGain);

    UpdateAllCritPercentages();
}

void Player::UpdateCombatSkills(
    Unit* /*pVictim*/, WeaponAttackType attType, bool defense)
{
    // TODO: The formula for weapon gain could probably use work.
    // Here are the facts we know:
    // * Chance depends on how far away you are from your current cap & your
    //   intellect.
    // * When really far away from cap (<=50% of max), you always gain a skill.
    // * When about 75% of max, you start missing skill ups in a consistent
    //   manner.
    // * When cloes to max (within 5 points), it goes really slow.
    // * Gaining the last skill point before cap is very slow, many sources
    //   state warriors (low intellect) saying it takes up to an hour.
    // * Getting skill-ups on low level is not that slow. If you're level 1 the
    //   first few points are very quick, despite it being close to your cap.

    int max_skill = 5 * getLevel();
    int skill =
        defense ? GetBaseDefenseSkillValue() : GetBaseWeaponSkillValue(attType);
    int needed_skills = max_skill - skill;

    if (needed_skills <= 0)
        return;

    auto pct = (float)skill / (float)max_skill;
    float chance = 0.0f;

    if (needed_skills > 5 && pct <= 0.5f)
    {
        chance = 1.0f;
    }
    else
    {
        chance = (1.0f - pct) * 0.3f;
        if (needed_skills > 5 && chance < 0.02f)
            chance = 0.02f;
    }

    // Scales with intellect
    // currently: 10% more likely per 20 intellect
    float inte = GetStat(STAT_INTELLECT);
    chance *= 1.0f + (inte * 0.005f);

    float roll = rand_norm_f();
    LOG_DEBUG(combat_logger, "Update %s Skill: chance %f roll %f",
        (defense ? "Defense" : "Weapon"), chance, roll);
    if (roll < chance)
    {
        if (defense)
            UpdateDefense();
        else
            UpdateWeaponSkill(attType);
    }
}

void Player::ModifySkillBonus(uint32 skillid, int32 val, bool talent)
{
    SkillStatusMap::const_iterator itr = mSkillStatus.find(skillid);
    if (itr == mSkillStatus.end() || itr->second.uState == SKILL_DELETED)
        return;

    uint32 bonusIndex = PLAYER_SKILL_BONUS_INDEX(itr->second.pos);

    uint32 bonus_val = GetUInt32Value(bonusIndex);
    int16 temp_bonus = SKILL_TEMP_BONUS(bonus_val);
    int16 perm_bonus = SKILL_PERM_BONUS(bonus_val);

    if (talent) // permanent bonus stored in high part
        SetUInt32Value(
            bonusIndex, MAKE_SKILL_BONUS(temp_bonus, perm_bonus + val));
    else // temporary/item bonus stored in low part
        SetUInt32Value(
            bonusIndex, MAKE_SKILL_BONUS(temp_bonus + val, perm_bonus));
}

void Player::UpdateSkillsForLevel()
{
    uint16 maxconfskill = sWorld::Instance()->GetConfigMaxSkillValue();
    uint32 maxSkill = GetMaxSkillValueForLevel();

    bool alwaysMaxSkill =
        sWorld::Instance()->getConfig(CONFIG_BOOL_ALWAYS_MAX_SKILL_FOR_LEVEL);

    for (auto itr = mSkillStatus.begin(); itr != mSkillStatus.end(); ++itr)
    {
        if (itr->second.uState == SKILL_DELETED)
            continue;

        uint32 pskill = itr->first;

        SkillLineEntry const* pSkill = sSkillLineStore.LookupEntry(pskill);
        if (!pSkill)
            continue;

        if (GetSkillRangeType(pSkill, false) != SKILL_RANGE_LEVEL)
            continue;

        uint32 valueIndex = PLAYER_SKILL_VALUE_INDEX(itr->second.pos);
        uint32 data = GetUInt32Value(valueIndex);
        uint32 max = SKILL_MAX(data);
        uint32 val = SKILL_VALUE(data);

        /// update only level dependent max skill values
        if (max != 1)
        {
            /// maximize skill always
            if (alwaysMaxSkill)
            {
                SetUInt32Value(
                    valueIndex, MAKE_SKILL_VALUE(maxSkill, maxSkill));
                if (itr->second.uState != SKILL_NEW)
                    itr->second.uState = SKILL_CHANGED;
            }
            else if (max != maxconfskill) /// update max skill value if current
                                          /// max skill not maximized
            {
                SetUInt32Value(valueIndex, MAKE_SKILL_VALUE(val, maxSkill));
                if (itr->second.uState != SKILL_NEW)
                    itr->second.uState = SKILL_CHANGED;
            }
        }
    }
}

void Player::UpdateSkillsToMaxSkillsForLevel()
{
    for (auto itr = mSkillStatus.begin(); itr != mSkillStatus.end(); ++itr)
    {
        if (itr->second.uState == SKILL_DELETED)
            continue;

        uint32 skill = itr->first;
        if (IsProfessionOrRidingSkill(skill))
            continue;

        uint32 index = PLAYER_SKILL_VALUE_INDEX(itr->second.pos);
        uint32 value = GetUInt32Value(index);
        uint32 max = SKILL_MAX(value);

        if (max > 1)
        {
            SetUInt32Value(index, MAKE_SKILL_VALUE(max, max));
            if (itr->second.uState != SKILL_NEW)
                itr->second.uState = SKILL_CHANGED;
        }
    }

    // Update everything that scales on skills (we intentionally skip verifying
    // that they have actually changed)
    UpdateDefenseBonusesMod();
    UpdateAllCritPercentages();
}

// This functions sets a skill line value (and adds if doesn't exist yet)
// To "remove" a skill line, set it's values to zero
void Player::SetSkill(
    uint16 id, uint16 currVal, uint16 maxVal, uint16 step /*=0*/)
{
    if (!id)
        return;

    auto itr = mSkillStatus.find(id);

    // has skill
    if (itr != mSkillStatus.end() && itr->second.uState != SKILL_DELETED)
    {
        if (currVal)
        {
            if (step) // need update step
                SetUInt32Value(
                    PLAYER_SKILL_INDEX(itr->second.pos), MAKE_PAIR32(id, step));
            // update value
            SetUInt32Value(PLAYER_SKILL_VALUE_INDEX(itr->second.pos),
                MAKE_SKILL_VALUE(currVal, maxVal));
            if (itr->second.uState != SKILL_NEW)
                itr->second.uState = SKILL_CHANGED;
            // learnSkillRewardedSpells(id, currVal);        // pre-3.x have
            // only 1 skill level req (so at learning only)
        }
        else // remove
        {
            // clear skill fields
            SetUInt32Value(PLAYER_SKILL_INDEX(itr->second.pos), 0);
            SetUInt32Value(PLAYER_SKILL_VALUE_INDEX(itr->second.pos), 0);
            SetUInt32Value(PLAYER_SKILL_BONUS_INDEX(itr->second.pos), 0);

            // mark as deleted or simply remove from map if not saved yet
            if (itr->second.uState != SKILL_NEW)
                itr->second.uState = SKILL_DELETED;
            else
                mSkillStatus.erase(itr);

            // remove all spells that related to this skill
            for (uint32 j = 0; j < sSkillLineAbilityStore.GetNumRows(); ++j)
                if (SkillLineAbilityEntry const* pAbility =
                        sSkillLineAbilityStore.LookupEntry(j))
                    if (pAbility->skillId == id)
                        removeSpell(sSpellMgr::Instance()->GetFirstSpellInChain(
                            pAbility->spellId));
        }
    }
    else if (currVal) // add
    {
        for (int i = 0; i < PLAYER_MAX_SKILLS; ++i)
        {
            if (!GetUInt32Value(PLAYER_SKILL_INDEX(i)))
            {
                SkillLineEntry const* pSkill = sSkillLineStore.LookupEntry(id);
                if (!pSkill)
                {
                    logging.error(
                        "Skill not found in SkillLineStore: skill #%u", id);
                    return;
                }

                SetUInt32Value(PLAYER_SKILL_INDEX(i), MAKE_PAIR32(id, step));
                SetUInt32Value(PLAYER_SKILL_VALUE_INDEX(i),
                    MAKE_SKILL_VALUE(currVal, maxVal));

                // insert new entry or update if not deleted old entry yet
                if (itr != mSkillStatus.end())
                {
                    itr->second.pos = i;
                    itr->second.uState = SKILL_CHANGED;
                }
                else
                    mSkillStatus.insert(SkillStatusMap::value_type(
                        id, SkillStatusData(i, SKILL_NEW)));

                // apply skill bonuses
                SetUInt32Value(PLAYER_SKILL_BONUS_INDEX(i), 0);

                // temporary bonuses
                const Auras& modSkill = GetAurasByType(SPELL_AURA_MOD_SKILL);
                for (const auto& elem : modSkill)
                    if ((elem)->GetModifier()->m_miscvalue == int32(id))
                        (elem)->ApplyModifier(true);

                // permanent bonuses
                const Auras& modSkillTalent =
                    GetAurasByType(SPELL_AURA_MOD_SKILL_TALENT);
                for (const auto& elem : modSkillTalent)
                    if ((elem)->GetModifier()->m_miscvalue == int32(id))
                        (elem)->ApplyModifier(true);

                // Learn all spells for skill
                learnSkillRewardedSpells(id, currVal);
                return;
            }
        }
    }
}

bool Player::HasSkill(uint32 skill) const
{
    if (!skill)
        return false;

    auto itr = mSkillStatus.find(skill);
    return (itr != mSkillStatus.end() && itr->second.uState != SKILL_DELETED);
}

uint16 Player::GetSkillValue(uint32 skill) const
{
    if (!skill)
        return 0;

    auto itr = mSkillStatus.find(skill);
    if (itr == mSkillStatus.end() || itr->second.uState == SKILL_DELETED)
        return 0;

    uint32 bonus = GetUInt32Value(PLAYER_SKILL_BONUS_INDEX(itr->second.pos));

    int32 result = int32(
        SKILL_VALUE(GetUInt32Value(PLAYER_SKILL_VALUE_INDEX(itr->second.pos))));
    result += SKILL_TEMP_BONUS(bonus);
    result += SKILL_PERM_BONUS(bonus);
    return result < 0 ? 0 : result;
}

uint16 Player::GetMaxSkillValue(uint32 skill) const
{
    if (!skill)
        return 0;

    auto itr = mSkillStatus.find(skill);
    if (itr == mSkillStatus.end() || itr->second.uState == SKILL_DELETED)
        return 0;

    uint32 bonus = GetUInt32Value(PLAYER_SKILL_BONUS_INDEX(itr->second.pos));

    int32 result = int32(
        SKILL_MAX(GetUInt32Value(PLAYER_SKILL_VALUE_INDEX(itr->second.pos))));
    result += SKILL_TEMP_BONUS(bonus);
    result += SKILL_PERM_BONUS(bonus);
    return result < 0 ? 0 : result;
}

uint16 Player::GetPureMaxSkillValue(uint32 skill) const
{
    if (!skill)
        return 0;

    auto itr = mSkillStatus.find(skill);
    if (itr == mSkillStatus.end() || itr->second.uState == SKILL_DELETED)
        return 0;

    return SKILL_MAX(GetUInt32Value(PLAYER_SKILL_VALUE_INDEX(itr->second.pos)));
}

uint16 Player::GetBaseSkillValue(uint32 skill) const
{
    if (!skill)
        return 0;

    auto itr = mSkillStatus.find(skill);
    if (itr == mSkillStatus.end() || itr->second.uState == SKILL_DELETED)
        return 0;

    int32 result = int32(
        SKILL_VALUE(GetUInt32Value(PLAYER_SKILL_VALUE_INDEX(itr->second.pos))));
    result += SKILL_PERM_BONUS(
        GetUInt32Value(PLAYER_SKILL_BONUS_INDEX(itr->second.pos)));
    return result < 0 ? 0 : result;
}

uint16 Player::GetPureSkillValue(uint32 skill) const
{
    if (!skill)
        return 0;

    auto itr = mSkillStatus.find(skill);
    if (itr == mSkillStatus.end() || itr->second.uState == SKILL_DELETED)
        return 0;

    return SKILL_VALUE(
        GetUInt32Value(PLAYER_SKILL_VALUE_INDEX(itr->second.pos)));
}

int16 Player::GetSkillPermBonusValue(uint32 skill) const
{
    if (!skill)
        return 0;

    auto itr = mSkillStatus.find(skill);
    if (itr == mSkillStatus.end() || itr->second.uState == SKILL_DELETED)
        return 0;

    return SKILL_PERM_BONUS(
        GetUInt32Value(PLAYER_SKILL_BONUS_INDEX(itr->second.pos)));
}

int16 Player::GetSkillTempBonusValue(uint32 skill) const
{
    if (!skill)
        return 0;

    auto itr = mSkillStatus.find(skill);
    if (itr == mSkillStatus.end() || itr->second.uState == SKILL_DELETED)
        return 0;

    return SKILL_TEMP_BONUS(
        GetUInt32Value(PLAYER_SKILL_BONUS_INDEX(itr->second.pos)));
}

void Player::SendInitialActionButtons() const
{
    LOG_DEBUG(logging, "Initializing Action Buttons for '%u'", GetGUIDLow());

    WorldPacket data(SMSG_ACTION_BUTTONS, (MAX_ACTION_BUTTONS * 4));
    for (uint8 button = 0; button < MAX_ACTION_BUTTONS; ++button)
    {
        auto itr = m_actionButtons.find(button);
        if (itr != m_actionButtons.end() &&
            itr->second.uState != ACTIONBUTTON_DELETED)
            data << uint32(itr->second.packedData);
        else
            data << uint32(0);
    }

    GetSession()->send_packet(std::move(data));
    LOG_DEBUG(logging, "Action Buttons for '%u' Initialized", GetGUIDLow());
}

bool Player::IsActionButtonDataValid(
    uint8 button, uint32 action, uint8 type, Player* player)
{
    if (button >= MAX_ACTION_BUTTONS)
    {
        if (player)
            logging.error(
                "Action %u not added into button %u for player %s: button must "
                "be < %u",
                action, button, player->GetName(), MAX_ACTION_BUTTONS);
        else
            logging.error(
                "Table `playercreateinfo_action` have action %u into button %u "
                ": button must be < %u",
                action, button, MAX_ACTION_BUTTONS);
        return false;
    }

    if (action >= MAX_ACTION_BUTTON_ACTION_VALUE)
    {
        if (player)
            logging.error(
                "Action %u not added into button %u for player %s: action must "
                "be < %u",
                action, button, player->GetName(),
                MAX_ACTION_BUTTON_ACTION_VALUE);
        else
            logging.error(
                "Table `playercreateinfo_action` have action %u into button %u "
                ": action must be < %u",
                action, button, MAX_ACTION_BUTTON_ACTION_VALUE);
        return false;
    }

    switch (type)
    {
    case ACTION_BUTTON_SPELL:
    {
        SpellEntry const* spellProto = sSpellStore.LookupEntry(action);
        if (!spellProto)
        {
            if (player)
                logging.error(
                    "Spell action %u not added into button %u for player %s: "
                    "spell not exist",
                    action, button, player->GetName());
            else
                logging.error(
                    "Table `playercreateinfo_action` have spell action %u into "
                    "button %u: spell not exist",
                    action, button);
            return false;
        }

        if (player)
        {
            if (!player->HasSpell(spellProto->Id))
            {
                logging.error(
                    "Spell action %u not added into button %u for player %s: "
                    "player don't known this spell",
                    action, button, player->GetName());
                return false;
            }
            else if (IsPassiveSpell(spellProto))
            {
                logging.error(
                    "Spell action %u not added into button %u for player %s: "
                    "spell is passive",
                    action, button, player->GetName());
                return false;
            }
        }
        break;
    }
    case ACTION_BUTTON_ITEM:
    {
        if (!ObjectMgr::GetItemPrototype(action))
        {
            if (player)
                logging.error(
                    "Item action %u not added into button %u for player %s: "
                    "item not exist",
                    action, button, player->GetName());
            else
                logging.error(
                    "Table `playercreateinfo_action` have item action %u into "
                    "button %u: item not exist",
                    action, button);
            return false;
        }
        break;
    }
    default:
        break; // other cases not checked at this moment
    }

    return true;
}

ActionButton* Player::addActionButton(uint8 button, uint32 action, uint8 type)
{
    if (!IsActionButtonDataValid(button, action, type, this))
        return nullptr;

    // it create new button (NEW state) if need or return existing
    ActionButton& ab = m_actionButtons[button];

    // set data and update to CHANGED if not NEW
    ab.SetActionAndType(action, ActionButtonType(type));

    LOG_DEBUG(logging, "Player '%u' Added Action '%u' (type %u) to Button '%u'",
        GetGUIDLow(), action, uint32(type), button);
    return &ab;
}

void Player::removeActionButton(uint8 button)
{
    auto buttonItr = m_actionButtons.find(button);
    if (buttonItr == m_actionButtons.end())
        return;

    if (buttonItr->second.uState == ACTIONBUTTON_NEW)
        m_actionButtons.erase(buttonItr); // new and not saved
    else
        buttonItr->second.uState =
            ACTIONBUTTON_DELETED; // saved, will deleted at next save

    LOG_DEBUG(logging, "Action Button '%u' Removed from Player '%u'", button,
        GetGUIDLow());
}

bool Player::SetPosition(
    float x, float y, float z, float orientation, bool teleport)
{
    // prevent crash when a bad coord is sent by the client
    if (!maps::verify_coords(x, y))
    {
        LOG_DEBUG(logging,
            "Player::SetPosition(%f, %f, %f, %f, %d) .. bad coordinates for "
            "player %d!",
            x, y, z, orientation, teleport, GetGUIDLow());
        return false;
    }

    Map* m = GetMap();

    const float old_x = GetX();
    const float old_y = GetY();
    const float old_z = GetZ();
    const float old_r = GetO();

    if (teleport || old_x != x || old_y != y || old_z != z ||
        old_r != orientation)
    {
        if (teleport || old_x != x || old_y != y || old_z != z)
            remove_auras_if([](AuraHolder* h)
                {
                    return h->GetSpellProto()->AuraInterruptFlags &
                           (AURA_INTERRUPT_FLAG_MOVE |
                               AURA_INTERRUPT_FLAG_TURNING);
                });
        else
            remove_auras_if([](AuraHolder* h)
                {
                    return h->GetSpellProto()->AuraInterruptFlags &
                           AURA_INTERRUPT_FLAG_TURNING;
                });

        remove_auras(SPELL_AURA_FEIGN_DEATH);

        m->relocate(this, x, y, z, orientation);

        float x = GetX();
        float y = GetY();

        if (GetGroup() && (old_x != x || old_y != y))
            SetGroupUpdateFlag(GROUP_UPDATE_FLAG_POSITION);

        // Acitons taken after relocate need to be delayed until next gane tick
        recently_relocated_ = true;
    }

    return true;
}

void Player::SendDirectMessage(WorldPacket&& data)
{
    GetSession()->send_packet(std::move(data));
}

void Player::SendCinematicStart(uint32 CinematicSequenceId)
{
    WorldPacket data(SMSG_TRIGGER_CINEMATIC, 4);
    data << uint32(CinematicSequenceId);
    SendDirectMessage(std::move(data));
}

void Player::CheckAreaExploreAndOutdoor()
{
    if (!isAlive() || IsTaxiFlying())
        return;

    bool is_outdoor;
    uint16 areaFlag =
        GetTerrain()->GetAreaFlag(GetX(), GetY(), GetZ(), &is_outdoor);

    if (was_outdoors_ == -1 || (bool)was_outdoors_ != is_outdoor)
    {
        was_outdoors_ = is_outdoor;
        if (is_outdoor)
        {
            if (HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_RESTING) &&
                GetRestType() == REST_TYPE_IN_TAVERN)
            {
                SetRestType(REST_TYPE_NO);
            }

            // Re-apply passive auras when we go outdoor if this spell is
            // outdoor-only.
            for (PlayerSpellMap::const_iterator itr = m_spells.begin();
                 itr != m_spells.end(); ++itr)
            {
                if (itr->second.state != PLAYERSPELL_REMOVED)
                {
                    if (SpellEntry const* spellInfo =
                            sSpellStore.LookupEntry(itr->first))
                    {
                        if (spellInfo->HasAttribute(SPELL_ATTR_PASSIVE) &&
                            spellInfo->HasAttribute(SPELL_ATTR_OUTDOORS_ONLY) &&
                            !has_aura(itr->first))
                            CastSpell(this, itr->first,
                                TRIGGER_TYPE_TRIGGERED |
                                    TRIGGER_TYPE_CHECK_STANCES);
                    }
                }
            }

            // Re-apply set-auras that are outdoor-only when we go outdoor
            for (auto& elem : ItemSetEff)
            {
                ItemSetEffect* eff = elem;
                if (!eff)
                    continue;

                for (int j = 0; j < 8; ++j)
                {
                    const SpellEntry* info = eff->spells[j];
                    if (!info)
                        continue;
                    if (info->HasAttribute(SPELL_ATTR_PASSIVE) &&
                        info->HasAttribute(SPELL_ATTR_OUTDOORS_ONLY) &&
                        !has_aura(info->Id))
                        CastSpell(
                            this, info->Id, TRIGGER_TYPE_TRIGGERED |
                                                TRIGGER_TYPE_CHECK_STANCES);
                }
            }
        }
        else // if (is_outdoor)
        {
            remove_auras_if([](AuraHolder* holder)
                {
                    return holder->GetSpellProto()->HasAttribute(
                        SPELL_ATTR_OUTDOORS_ONLY);
                });
        }
    }

    // Don't allow discovering stuff which riding a transport
    if (GetTransport() != nullptr)
        return;

    if (areaFlag == 0xffff)
        return;
    int offset = areaFlag / 32;

    if (offset >= PLAYER_EXPLORED_ZONES_SIZE)
    {
        logging.error(
            "Wrong area flag %u in map data for (X: %f Y: %f) point to field "
            "PLAYER_EXPLORED_ZONES_1 + %u ( %u must be < %u ).",
            areaFlag, GetX(), GetY(), offset, offset,
            PLAYER_EXPLORED_ZONES_SIZE);
        return;
    }

    uint32 val = (uint32)(1 << (areaFlag % 32));
    uint32 currFields = GetUInt32Value(PLAYER_EXPLORED_ZONES_1 + offset);

    if (!(currFields & val))
    {
        SetUInt32Value(
            PLAYER_EXPLORED_ZONES_1 + offset, (uint32)(currFields | val));

        AreaTableEntry const* p =
            GetAreaEntryByAreaFlagAndMap(areaFlag, GetMapId());
        if (!p)
        {
            logging.error(
                "PLAYER: Player %u discovered unknown area (x: %f y: %f map: "
                "%u",
                GetGUIDLow(), GetX(), GetY(), GetMapId());
        }
        else if (p->area_level > 0)
        {
            uint32 area = p->ID;
            if (getLevel() >=
                sWorld::Instance()->getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL))
            {
                SendExplorationExperience(area, 0);
            }
            else
            {
                int32 diff = int32(getLevel()) - p->area_level;
                uint32 XP = 0;
                if (diff < -5)
                {
                    XP = uint32(
                        sObjectMgr::Instance()->GetBaseXP(getLevel() + 5) *
                        sWorld::Instance()->getConfig(
                            CONFIG_FLOAT_RATE_XP_EXPLORE));
                }
                else if (diff > 5)
                {
                    int32 exploration_percent = (100 - ((diff - 5) * 5));
                    if (exploration_percent > 100)
                        exploration_percent = 100;
                    else if (exploration_percent < 0)
                        exploration_percent = 0;

                    XP = uint32(
                        sObjectMgr::Instance()->GetBaseXP(p->area_level) *
                        exploration_percent / 100 *
                        sWorld::Instance()->getConfig(
                            CONFIG_FLOAT_RATE_XP_EXPLORE));
                }
                else
                {
                    XP = uint32(
                        sObjectMgr::Instance()->GetBaseXP(p->area_level) *
                        sWorld::Instance()->getConfig(
                            CONFIG_FLOAT_RATE_XP_EXPLORE));
                }

                GiveXP(XP, nullptr);
                SendExplorationExperience(area, XP);
            }
            LOG_DEBUG(logging, "PLAYER: Player %u discovered a new area: %u",
                GetGUIDLow(), area);
        }
    }
}

Team Player::TeamForRace(uint8 race)
{
    ChrRacesEntry const* rEntry = sChrRacesStore.LookupEntry(race);
    if (!rEntry)
    {
        logging.error(
            "Race %u not found in DBC: wrong DBC files?", uint32(race));
        return ALLIANCE;
    }

    switch (rEntry->TeamID)
    {
    case 7:
        return ALLIANCE;
    case 1:
        return HORDE;
    }

    logging.error("Race %u have wrong teamid %u in DBC: wrong DBC files?",
        uint32(race), rEntry->TeamID);
    return TEAM_NONE;
}

uint32 Player::getFactionForRace(uint8 race)
{
    ChrRacesEntry const* rEntry = sChrRacesStore.LookupEntry(race);
    if (!rEntry)
    {
        logging.error(
            "Race %u not found in DBC: wrong DBC files?", uint32(race));
        return 0;
    }

    return rEntry->FactionID;
}

void Player::setFactionForRace(uint8 race)
{
    m_team = TeamForRace(race);
    setFaction(getFactionForRace(race));
}

ReputationRank Player::GetReputationRank(uint32 faction) const
{
    FactionEntry const* factionEntry = sFactionStore.LookupEntry(faction);
    return GetReputationMgr().GetRank(factionEntry);
}

// Calculate total reputation percent player gain with quest/creature level
int32 Player::CalculateReputationGain(ReputationSource source, int32 rep,
    int32 faction, uint32 creatureOrQuestLevel, bool noAuraBonus)
{
    float percent = 100.0f;

    float repMod = noAuraBonus ? 0.0f : (float)GetTotalAuraModifier(
                                            SPELL_AURA_MOD_REPUTATION_GAIN);

    // faction specific auras only seem to apply to kills
    if (source == REPUTATION_SOURCE_KILL)
        repMod += GetTotalAuraModifierByMiscValue(
            SPELL_AURA_MOD_FACTION_REPUTATION_GAIN, faction);

    percent += rep > 0 ? repMod : -repMod;

    float rate;
    switch (source)
    {
    case REPUTATION_SOURCE_KILL:
        rate = sWorld::Instance()->getConfig(
            CONFIG_FLOAT_RATE_REPUTATION_LOWLEVEL_KILL);
        break;
    case REPUTATION_SOURCE_QUEST:
        rate = sWorld::Instance()->getConfig(
            CONFIG_FLOAT_RATE_REPUTATION_LOWLEVEL_QUEST);
        break;
    case REPUTATION_SOURCE_SPELL:
    default:
        rate = 1.0f;
        break;
    }

    if (rate != 1.0f &&
        creatureOrQuestLevel <= MaNGOS::XP::GetGrayLevel(getLevel()))
        percent *= rate;

    if (percent <= 0.0f)
        return 0;

    // Multiply result with the faction specific rate
    if (const RepRewardRate* repData =
            sObjectMgr::Instance()->GetRepRewardRate(faction))
    {
        float repRate = 0.0f;
        switch (source)
        {
        case REPUTATION_SOURCE_KILL:
            repRate = repData->creature_rate;
            break;
        case REPUTATION_SOURCE_QUEST:
            repRate = repData->quest_rate;
            break;
        case REPUTATION_SOURCE_SPELL:
            repRate = repData->spell_rate;
            break;
        }

        // for custom, a rate of 0.0 will totally disable reputation gain for
        // this faction/type
        if (repRate <= 0.0f)
            return 0;

        percent *= repRate;
    }

    return int32(
        sWorld::Instance()->getConfig(CONFIG_FLOAT_RATE_REPUTATION_GAIN) * rep *
        percent / 100.0f);
}

// Calculates how many reputation points player gains in victim's enemy factions
void Player::RewardReputation(Unit* pVictim, float rate)
{
    if (!pVictim || pVictim->GetTypeId() == TYPEID_PLAYER)
        return;

    // used current difficulty creature entry instead normal version
    // (GetEntry())
    ReputationOnKillEntry const* Rep =
        sObjectMgr::Instance()->GetReputationOnKillEntry(
            ((Creature*)pVictim)->GetCreatureInfo()->Entry);

    if (!Rep)
        return;

    if (Rep->repfaction1 && (!Rep->team_dependent || GetTeam() == ALLIANCE))
    {
        int32 donerep1 = CalculateReputationGain(REPUTATION_SOURCE_KILL,
            Rep->repvalue1, Rep->repfaction1, pVictim->getLevel());
        donerep1 = int32(donerep1 * rate);
        FactionEntry const* factionEntry1 =
            sFactionStore.LookupEntry(Rep->repfaction1);
        uint32 current_reputation_rank1 =
            GetReputationMgr().GetRank(factionEntry1);
        if (factionEntry1 &&
            current_reputation_rank1 <= Rep->reputation_max_cap1)
            GetReputationMgr().ModifyReputation(factionEntry1, donerep1);

        // Wiki: Team factions value divided by 2
        if (factionEntry1 && Rep->is_teamaward1)
        {
            FactionEntry const* team1_factionEntry =
                sFactionStore.LookupEntry(factionEntry1->team);
            if (team1_factionEntry)
                GetReputationMgr().ModifyReputation(
                    team1_factionEntry, donerep1 / 2);
        }
    }

    if (Rep->repfaction2 && (!Rep->team_dependent || GetTeam() == HORDE))
    {
        int32 donerep2 = CalculateReputationGain(REPUTATION_SOURCE_KILL,
            Rep->repvalue2, Rep->repfaction2, pVictim->getLevel());
        donerep2 = int32(donerep2 * rate);
        FactionEntry const* factionEntry2 =
            sFactionStore.LookupEntry(Rep->repfaction2);
        uint32 current_reputation_rank2 =
            GetReputationMgr().GetRank(factionEntry2);
        if (factionEntry2 &&
            current_reputation_rank2 <= Rep->reputation_max_cap2)
            GetReputationMgr().ModifyReputation(factionEntry2, donerep2);

        // Wiki: Team factions value divided by 2
        if (factionEntry2 && Rep->is_teamaward2)
        {
            FactionEntry const* team2_factionEntry =
                sFactionStore.LookupEntry(factionEntry2->team);
            if (team2_factionEntry)
                GetReputationMgr().ModifyReputation(
                    team2_factionEntry, donerep2 / 2);
        }
    }
}

// Calculate how many reputation points player gain with the quest
void Player::RewardReputation(Quest const* pQuest)
{
    // quest reputation reward/loss
    for (int i = 0; i < QUEST_REPUTATIONS_COUNT; ++i)
    {
        if (!pQuest->RewRepFaction[i])
            continue;

        if (pQuest->RewRepValue[i])
        {
            int32 rep = CalculateReputationGain(REPUTATION_SOURCE_QUEST,
                pQuest->RewRepValue[i], pQuest->RewRepFaction[i],
                GetQuestLevelForPlayer(pQuest));

            if (FactionEntry const* factionEntry =
                    sFactionStore.LookupEntry(pQuest->RewRepFaction[i]))
                GetReputationMgr().ModifyReputation(factionEntry, rep);
        }
    }

    // TODO: implement reputation spillover
}

void Player::UpdateArenaFields(void)
{
    /* arena calcs go here */
}

void Player::UpdateHonorFields()
{
    /// called when rewarding honor and at each save
    time_t now = WorldTimer::time_no_syscall();
    time_t today = (WorldTimer::time_no_syscall() / DAY) * DAY;

    if (m_lastHonorUpdateTime < today)
    {
        time_t yesterday = today - DAY;

        uint16 kills_today = PAIR32_LOPART(GetUInt32Value(PLAYER_FIELD_KILLS));

        // update yesterday's contribution
        if (m_lastHonorUpdateTime >= yesterday)
        {
            SetUInt32Value(PLAYER_FIELD_YESTERDAY_CONTRIBUTION,
                GetUInt32Value(PLAYER_FIELD_TODAY_CONTRIBUTION));

            // this is the first update today, reset today's contribution
            SetUInt32Value(PLAYER_FIELD_TODAY_CONTRIBUTION, 0);
            SetUInt32Value(PLAYER_FIELD_KILLS, MAKE_PAIR32(0, kills_today));
        }
        else
        {
            // no honor/kills yesterday or today, reset
            SetUInt32Value(PLAYER_FIELD_YESTERDAY_CONTRIBUTION, 0);
            SetUInt32Value(PLAYER_FIELD_KILLS, 0);
        }
    }

    m_lastHonorUpdateTime = now;
}

// This gets pvp rank whether it is the active title or not
int Player::get_pvp_rank() const
{
    // Player ranks are [5,18]
    if (GetTeam() == ALLIANCE)
    {
        // Title [1,14]  => Rank [5,18]
        for (int i = 1; i < 15; ++i)
            if (HasTitle(i))
                return i + 4;
    }
    else
    {
        // Title [15,28] => Rank [5,18]
        for (int i = 15; i < 29; ++i)
            if (HasTitle(i))
                return i - 14 + 4;
    }
    return 5; // Players are counted as private/scout if no title
}

// Distribute honor points.
// If honor is <= 0, and victim is an NPC the points are calculated.
bool Player::RewardHonor(Unit* uVictim, uint32 groupsize, float honor)
{
    if (InArena())
        return false;

    // 'Inactive' this aura prevents the player from gaining honor points and
    // battleground tokens
    if (has_aura(SPELL_AURA_PLAYER_INACTIVE, SPELL_AURA_DUMMY))
        return false;

    ObjectGuid victim_guid;
    uint32 victim_rank = 0;

    // need call before fields update to have chance move yesterday data to
    // appropriate fields before today data change.
    UpdateHonorFields();

    if (uVictim && uVictim->GetTypeId() == TYPEID_PLAYER)
    {
        victim_guid = uVictim->GetObjectGuid();
        victim_rank = static_cast<Player*>(uVictim)->get_pvp_rank();
    }

    if (honor <= 0)
    {
        // NOTE: Player kills distributed in Player::hk_distribute_honor()
        if (!uVictim || uVictim->HasAuraType(SPELL_AURA_NO_PVP_CREDIT) ||
            uVictim->GetTypeId() != TYPEID_UNIT)
            return false;

        if (!static_cast<Creature*>(uVictim)->IsRacialLeader())
            return false;

        honor = 100;      // TODO: need more info
        victim_rank = 19; // HK: Leader

        if (groupsize > 1)
            honor /= groupsize;
    }

    honor *= sWorld::Instance()->getConfig(CONFIG_FLOAT_RATE_HONOR);

    // honor - for show honor points in log
    // victim_guid - for show victim name in log
    // victim_rank [1..4]  HK: <dishonored rank>
    // victim_rank [5..19] HK: <alliance\horde rank>
    // victim_rank [0,20+] HK: <>
    WorldPacket data(SMSG_PVP_CREDIT, 4 + 8 + 4);
    data << uint32(honor);
    data << ObjectGuid(victim_guid);
    data << uint32(victim_rank);
    GetSession()->send_packet(std::move(data));

    // add honor points
    ModifyHonorPoints(int32(honor));

    ApplyModUInt32Value(PLAYER_FIELD_TODAY_CONTRIBUTION, uint32(honor), true);
    return true;
}

void Player::SetHonorPoints(uint32 value)
{
    if (value > sWorld::Instance()->getConfig(CONFIG_UINT32_MAX_HONOR_POINTS))
        value = sWorld::Instance()->getConfig(CONFIG_UINT32_MAX_HONOR_POINTS);

    SetUInt32Value(PLAYER_FIELD_HONOR_CURRENCY, value);
}

void Player::SetArenaPoints(uint32 value)
{
    if (value > sWorld::Instance()->getConfig(CONFIG_UINT32_MAX_ARENA_POINTS))
        value = sWorld::Instance()->getConfig(CONFIG_UINT32_MAX_ARENA_POINTS);

    SetUInt32Value(PLAYER_FIELD_ARENA_CURRENCY, value);
}

void Player::ModifyHonorPoints(int32 value)
{
    int32 newValue = (int32)GetHonorPoints() + value;

    if (newValue < 0)
        newValue = 0;

    SetHonorPoints(newValue);
}

void Player::ModifyArenaPoints(int32 value)
{
    int32 newValue = (int32)GetArenaPoints() + value;

    if (newValue < 0)
        newValue = 0;

    SetArenaPoints(newValue);
}

uint32 Player::GetGuildIdFromDB(ObjectGuid guid)
{
    uint32 lowguid = guid.GetCounter();

    std::unique_ptr<QueryResult> result(CharacterDatabase.PQuery(
        "SELECT guildid FROM guild_member WHERE guid='%u'", lowguid));
    if (!result)
        return 0;

    uint32 id = result->Fetch()[0].GetUInt32();
    return id;
}

uint32 Player::GetRankFromDB(ObjectGuid guid)
{
    std::unique_ptr<QueryResult> result(CharacterDatabase.PQuery(
        "SELECT rank FROM guild_member WHERE guid='%u'", guid.GetCounter()));
    if (result)
    {
        uint32 v = result->Fetch()[0].GetUInt32();
        return v;
    }
    else
        return 0;
}

uint32 Player::GetArenaTeamIdFromDB(ObjectGuid guid, ArenaType type)
{
    std::unique_ptr<QueryResult> result(CharacterDatabase.PQuery(
        "SELECT arena_team_member.arenateamid FROM arena_team_member JOIN "
        "arena_team ON arena_team_member.arenateamid = arena_team.arenateamid "
        "WHERE guid='%u' AND type='%u' LIMIT 1",
        guid.GetCounter(), type));
    if (!result)
        return 0;

    uint32 id = (*result)[0].GetUInt32();
    return id;
}

uint32 Player::GetZoneIdFromDB(ObjectGuid guid)
{
    uint32 lowguid = guid.GetCounter();
    std::unique_ptr<QueryResult> result(CharacterDatabase.PQuery(
        "SELECT zone FROM characters WHERE guid='%u'", lowguid));
    if (!result)
        return 0;
    Field* fields = result->Fetch();
    uint32 zone = fields[0].GetUInt32();

    if (!zone)
    {
        // stored zone is zero, use generic and slow zone detection
        result.reset(CharacterDatabase.PQuery(
            "SELECT map,position_x,position_y,position_z FROM characters WHERE "
            "guid='%u'",
            lowguid));
        if (!result)
            return 0;
        fields = result->Fetch();
        uint32 map = fields[0].GetUInt32();
        float posx = fields[1].GetFloat();
        float posy = fields[2].GetFloat();
        float posz = fields[3].GetFloat();

        zone = sTerrainMgr::Instance()->GetZoneId(map, posx, posy, posz);

        if (zone > 0)
            CharacterDatabase.PExecute(
                "UPDATE characters SET zone='%u' WHERE guid='%u'", zone,
                lowguid);
    }

    return zone;
}

uint32 Player::GetLevelFromDB(ObjectGuid guid)
{
    uint32 lowguid = guid.GetCounter();

    std::unique_ptr<QueryResult> result(CharacterDatabase.PQuery(
        "SELECT level FROM characters WHERE guid='%u'", lowguid));
    if (!result)
        return 0;

    Field* fields = result->Fetch();
    uint32 level = fields[0].GetUInt32();

    return level;
}

void Player::UpdateArea(uint32 newArea)
{
    cached_area_ = newArea;

    AreaTableEntry const* area = GetAreaEntryByAreaID(newArea);

    // FFA_PVP flags are area and not zone id dependent
    // so apply them accordingly
    if (area && (area->flags & AREA_FLAG_ARENA))
    {
        if (!isGameMaster())
            SetFFAPvP(true);
    }
    else
    {
        // remove ffa flag only if not ffapvp realm
        // removal in sanctuaries and capitals is handled in zone update
        if (IsFFAPvP() && !sWorld::Instance()->IsFFAPvPRealm())
            SetFFAPvP(false);
    }

    UpdateAreaDependentAuras();
}

void Player::UpdateZone(uint32 newZone, uint32 newArea)
{
    AreaTableEntry const* zone = GetAreaEntryByAreaID(newZone);
    if (!zone)
        return;

    if (cached_zone_ != newZone)
    {
        // handle outdoor pvp zones
        sOutdoorPvPMgr::Instance()->HandlePlayerLeaveZone(this, cached_zone_);
        sOutdoorPvPMgr::Instance()->HandlePlayerEnterZone(this, newZone);

        SendInitWorldStates(newZone, newArea); // only if really enters to new
                                               // zone, not just area change,
                                               // works strange...

        if (sWorld::Instance()->getConfig(CONFIG_BOOL_WEATHER))
        {
            if (Weather* wth = sWorld::Instance()->FindWeather(zone->ID))
                wth->SendWeatherUpdateToPlayer(this);
            else if (!sWorld::Instance()->AddWeather(zone->ID))
            {
                // send fine weather packet to remove old zone's weather
                Weather::SendFineWeatherUpdateToPlayer(this);
            }
        }

        // Client only sends join packets for local channels after login
        if (cached_zone_ != 0)
        {
            // NOTE: Must be delayed on far teleport
            queue_action_ticks(0, [this, newZone]()
                {
                    UpdateLocalChannels(newZone);
                });
        }
    }

    cached_zone_ = newZone;

    // zone changed, so area changed as well, update it
    UpdateArea(newArea);

    // in PvP, any not controlled zone (except zone->team == 6, default case)
    // in PvE, only opposition team capital
    switch (zone->team)
    {
    case AREATEAM_ALLY:
        pvpInfo.inHostileArea =
            GetTeam() != ALLIANCE && (sWorld::Instance()->IsPvPRealm() ||
                                         zone->flags & AREA_FLAG_CAPITAL);
        break;
    case AREATEAM_HORDE:
        pvpInfo.inHostileArea =
            GetTeam() != HORDE && (sWorld::Instance()->IsPvPRealm() ||
                                      zone->flags & AREA_FLAG_CAPITAL);
        break;
    case AREATEAM_NONE:
        // overwrite for battlegrounds, maybe batter some zone flags but current
        // known not 100% fit to this
        pvpInfo.inHostileArea =
            sWorld::Instance()->IsPvPRealm() || InBattleGround();
        break;
    default: // 6 in fact
        pvpInfo.inHostileArea = false;
        break;
    }

    if (pvpInfo.inHostileArea) // in hostile area
    {
        if (!IsPvP() || pvpInfo.endTimer != 0)
            UpdatePvP(true, true);
    }
    else // in friendly area
    {
        if (IsPvP() && !HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_IN_PVP) &&
            pvpInfo.endTimer == 0)
            pvpInfo.endTimer = time(nullptr); // start toggle-off
    }

    if (zone->flags & AREA_FLAG_SANCTUARY) // in sanctuary
    {
        SetFlag(PLAYER_FLAGS, PLAYER_FLAGS_SANCTUARY);
        if (sWorld::Instance()->IsFFAPvPRealm())
            SetFFAPvP(false);
        if (duel)
            DuelComplete(duel->startTime == 0 || duel->startTimer != 0 ?
                             DUEL_INTERUPTED :
                             DUEL_FLED);
    }
    else
    {
        RemoveFlag(PLAYER_FLAGS, PLAYER_FLAGS_SANCTUARY);
    }

    if (zone->flags & AREA_FLAG_CAPITAL) // in capital city
        SetRestType(REST_TYPE_IN_CITY);
    else if (HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_RESTING) &&
             GetRestType() != REST_TYPE_IN_TAVERN)
        // resting and not in tavern (leave city then); tavern leave handled in
        // CheckAreaExploreAndOutdoor
        SetRestType(REST_TYPE_NO);

    // remove items with area/map limitations (delete only for alive player to
    // allow back in ghost mode)
    // if player resurrected at teleport this will be applied in resurrect code
    if (isAlive())
    {
        // XXX
        inventory::transaction trans(false);
        uint32 zone_id = GetZoneId();

        const int mask = inventory::personal_storage::iterator::all_body;
        for (inventory::personal_storage::iterator itr = storage().begin(mask);
             itr != storage().end(); ++itr)
        {
            if ((*itr)->IsLimitedToAnotherMapOrZone(GetMapId(), zone_id))
                trans.destroy(*itr);
        }

        if (!trans.empty())
            storage().finalize(trans);
    }

    // group update
    if (GetGroup())
        SetGroupUpdateFlag(GROUP_UPDATE_FLAG_ZONE);

    UpdateZoneDependentAuras();
    UpdateZoneDependentPets();
}

// If players are too far way of duel flag... then player loose the duel
void Player::CheckDuelDistance(time_t currTime)
{
    if (!duel)
        return;

    auto guid = GetGuidValue(PLAYER_DUEL_ARBITER);

    // Flag not added to map yet
    if (guid.IsEmpty())
        return;

    GameObject* obj = GetMap()->GetGameObject(guid);
    if (!obj)
    {
        // player not at duel start map
        DuelComplete(DUEL_FLED);
        return;
    }

    if (duel->outOfBound == 0)
    {
        if (!IsWithinDistInMap(obj, 80))
        {
            duel->outOfBound = currTime;

            WorldPacket data(SMSG_DUEL_OUTOFBOUNDS, 0);
            GetSession()->send_packet(std::move(data));
        }
    }
    else
    {
        if (IsWithinDistInMap(obj, 78))
        {
            duel->outOfBound = 0;

            WorldPacket data(SMSG_DUEL_INBOUNDS, 0);
            GetSession()->send_packet(std::move(data));
        }
        else if (currTime >= (duel->outOfBound + 10))
        {
            DuelComplete(DUEL_FLED);
        }
    }
}

void Player::DuelComplete(DuelCompleteType type)
{
    // duel not requested
    if (!duel)
        return;

    WorldPacket data(SMSG_DUEL_COMPLETE, (1));
    data << (uint8)((type != DUEL_INTERUPTED) ? 1 : 0);
    GetSession()->send_packet(&data);
    duel->opponent->GetSession()->send_packet(&data);

    if (type != DUEL_INTERUPTED)
    {
        data.initialize(SMSG_DUEL_WINNER, (1 + 20)); // we guess size
        data << (uint8)((type == DUEL_WON) ? 0 : 1); // 0 = just won; 1 = fled
        data << duel->opponent->GetName();
        data << GetName();
        SendMessageToSet(&data, true);
    }

    // Stop attacking
    CombatStopWithPets(true);
    duel->opponent->CombatStopWithPets(true);

    // Remove Duel Flag object
    if (GameObject* obj =
            GetMap()->GetGameObject(GetGuidValue(PLAYER_DUEL_ARBITER)))
        duel->initiator->RemoveGameObject(obj, true);

    /* Remove Auras*/
    // Create a lambda to gather auras we need to remove
    std::vector<AuraHolder*> auras_to_remove;
    auto aura_push_fn = [this, &auras_to_remove](Unit* me, Player* duel_target)
    {
        me->loop_auras([&auras_to_remove, duel_target, this](AuraHolder* holder)
            {
                Unit* caster = holder->GetCaster();
                if (!holder->IsPositive() &&
                    holder->GetAuraApplyTime() >= duel->startTime && caster &&
                    caster->GetCharmerOrOwnerPlayerOrPlayerItself() ==
                        duel_target)
                    auras_to_remove.push_back(holder);
                return true; // continue
            });
    };

    // Call the lambda on all targets we care about
    aura_push_fn(this, duel->opponent); // us
    aura_push_fn(duel->opponent, this); // opponent
    // Our pets
    CallForAllControlledUnits(
        std::bind(aura_push_fn, std::placeholders::_1, duel->opponent),
        CONTROLLED_PET | CONTROLLED_GUARDIANS | CONTROLLED_CHARM);
    // Opponent's pets
    duel->opponent->CallForAllControlledUnits(
        std::bind(aura_push_fn, std::placeholders::_1, this),
        CONTROLLED_PET | CONTROLLED_GUARDIANS | CONTROLLED_CHARM);

    // Do the actual removing of the auras
    for (auto ptr : auras_to_remove)
    {
        // NOTE: AuraHolders we gathered could be disabled already at this point
        if (ptr->IsDisabled())
            continue;

        if (Unit* t = ptr->GetTarget())
            t->RemoveAuraHolder(ptr);
    }

    // HACK: remove SW:Death backlash damage
    if (getClass() == CLASS_PRIEST)
        remove_auras(32409);
    if (duel->opponent->getClass() == CLASS_PRIEST)
        duel->opponent->remove_auras(32409);

    // cleanup combo points
    if (GetComboTargetGuid() == duel->opponent->GetObjectGuid())
        ClearComboPoints();
    else if (GetComboTargetGuid() == duel->opponent->GetPetGuid())
        ClearComboPoints();

    if (duel->opponent->GetComboTargetGuid() == GetObjectGuid())
        duel->opponent->ClearComboPoints();
    else if (duel->opponent->GetComboTargetGuid() == GetPetGuid())
        duel->opponent->ClearComboPoints();

    // Clear queued spells from duel target or duel target's pets
    for (auto itr = spell_queue().begin(); itr != spell_queue().end();)
    {
        if (Unit* caster = GetMap()->GetUnit(itr->spell->GetCasterGUID()))
        {
            if (caster->GetCharmerOrOwnerPlayerOrPlayerItself() ==
                duel->opponent)
                itr = spell_queue().erase(itr);
            else
                ++itr;
        }
    }
    for (auto itr = duel->opponent->spell_queue().begin();
         itr != duel->opponent->spell_queue().end();)
    {
        if (Unit* caster = GetMap()->GetUnit(itr->spell->GetCasterGUID()))
        {
            if (caster->GetCharmerOrOwnerPlayerOrPlayerItself() == this)
                itr = duel->opponent->spell_queue().erase(itr);
            else
                ++itr;
        }
    }

    // cleanups
    SetGuidValue(PLAYER_DUEL_ARBITER, ObjectGuid());
    SetUInt32Value(PLAYER_DUEL_TEAM, 0);
    duel->opponent->SetGuidValue(PLAYER_DUEL_ARBITER, ObjectGuid());
    duel->opponent->SetUInt32Value(PLAYER_DUEL_TEAM, 0);

    delete duel->opponent->duel;
    duel->opponent->duel = nullptr;
    delete duel;
    duel = nullptr;
}

//---------------------------------------------------------//

void Player::_ApplyItemMods(Item* item, bool apply)
{
    // XXX
    inventory::slot slot = item->slot();
    if (!slot.equipment() && !slot.bagslot())
        return;

    const ItemPrototype* proto = item->GetProto();

    int type = MAX_ATTACK;
    if (slot.main_hand())
        type = BASE_ATTACK;
    else if (slot.off_hand())
        type = OFF_ATTACK;
    else if (slot.ranged())
        type = RANGED_ATTACK;

    if (type < MAX_ATTACK)
        _ApplyWeaponDependentAuraMods(
            item, static_cast<WeaponAttackType>(type), apply);

    _ApplyItemBonuses(item, apply, slot);

    if (slot.ranged())
        _ApplyAmmoBonuses();

    ApplyEnchantment(item, apply, slot);

    if (proto->Socket[0].Color)
        update_meta_gem();

    // Equip spells
    for (int i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
    {
        if (!proto->Spells[i].SpellId ||
            !(proto->Spells[i].SpellTrigger & ITEM_SPELLTRIGGER_ON_EQUIP))
            continue;
        if (auto info = sSpellStore.LookupEntry(proto->Spells[i].SpellId))
            ApplyEquipSpell(info, item, apply);
    }
}

void Player::_ApplyItemBonuses(
    Item* item, bool apply, inventory::slot item_slot)
{
    const ItemPrototype* proto = item->GetProto();

    for (uint32 i = 0; i < MAX_ITEM_PROTO_STATS; ++i)
    {
        float val = float(proto->ItemStat[i].ItemStatValue);

        if (val == 0)
            continue;

        switch (proto->ItemStat[i].ItemStatType)
        {
        case ITEM_MOD_MANA:
            HandleStatModifier(UNIT_MOD_MANA, BASE_VALUE, float(val), apply);
            break;
        case ITEM_MOD_HEALTH: // modify HP
            HandleStatModifier(UNIT_MOD_HEALTH, BASE_VALUE, float(val), apply);
            break;
        case ITEM_MOD_AGILITY: // modify agility
            HandleStatModifier(
                UNIT_MOD_STAT_AGILITY, BASE_VALUE, float(val), apply);
            ApplyStatBuffMod(STAT_AGILITY, float(val), apply);
            break;
        case ITEM_MOD_STRENGTH: // modify strength
            HandleStatModifier(
                UNIT_MOD_STAT_STRENGTH, BASE_VALUE, float(val), apply);
            ApplyStatBuffMod(STAT_STRENGTH, float(val), apply);
            break;
        case ITEM_MOD_INTELLECT: // modify intellect
            HandleStatModifier(
                UNIT_MOD_STAT_INTELLECT, BASE_VALUE, float(val), apply);
            ApplyStatBuffMod(STAT_INTELLECT, float(val), apply);
            break;
        case ITEM_MOD_SPIRIT: // modify spirit
            HandleStatModifier(
                UNIT_MOD_STAT_SPIRIT, BASE_VALUE, float(val), apply);
            ApplyStatBuffMod(STAT_SPIRIT, float(val), apply);
            break;
        case ITEM_MOD_STAMINA: // modify stamina
            HandleStatModifier(
                UNIT_MOD_STAT_STAMINA, BASE_VALUE, float(val), apply);
            ApplyStatBuffMod(STAT_STAMINA, float(val), apply);
            break;
        case ITEM_MOD_DEFENSE_SKILL_RATING:
            ApplyRatingMod(CR_DEFENSE_SKILL, int32(val), apply);
            break;
        case ITEM_MOD_DODGE_RATING:
            ApplyRatingMod(CR_DODGE, int32(val), apply);
            break;
        case ITEM_MOD_PARRY_RATING:
            ApplyRatingMod(CR_PARRY, int32(val), apply);
            break;
        case ITEM_MOD_BLOCK_RATING:
            ApplyRatingMod(CR_BLOCK, int32(val), apply);
            break;
        case ITEM_MOD_HIT_MELEE_RATING:
            ApplyRatingMod(CR_HIT_MELEE, int32(val), apply);
            break;
        case ITEM_MOD_HIT_RANGED_RATING:
            ApplyRatingMod(CR_HIT_RANGED, int32(val), apply);
            break;
        case ITEM_MOD_HIT_SPELL_RATING:
            ApplyRatingMod(CR_HIT_SPELL, int32(val), apply);
            break;
        case ITEM_MOD_CRIT_MELEE_RATING:
            ApplyRatingMod(CR_CRIT_MELEE, int32(val), apply);
            break;
        case ITEM_MOD_CRIT_RANGED_RATING:
            ApplyRatingMod(CR_CRIT_RANGED, int32(val), apply);
            break;
        case ITEM_MOD_CRIT_SPELL_RATING:
            ApplyRatingMod(CR_CRIT_SPELL, int32(val), apply);
            break;
        case ITEM_MOD_HIT_TAKEN_MELEE_RATING:
            ApplyRatingMod(CR_HIT_TAKEN_MELEE, int32(val), apply);
            break;
        case ITEM_MOD_HIT_TAKEN_RANGED_RATING:
            ApplyRatingMod(CR_HIT_TAKEN_RANGED, int32(val), apply);
            break;
        case ITEM_MOD_HIT_TAKEN_SPELL_RATING:
            ApplyRatingMod(CR_HIT_TAKEN_SPELL, int32(val), apply);
            break;
        case ITEM_MOD_CRIT_TAKEN_MELEE_RATING:
            ApplyRatingMod(CR_CRIT_TAKEN_MELEE, int32(val), apply);
            break;
        case ITEM_MOD_CRIT_TAKEN_RANGED_RATING:
            ApplyRatingMod(CR_CRIT_TAKEN_RANGED, int32(val), apply);
            break;
        case ITEM_MOD_CRIT_TAKEN_SPELL_RATING:
            ApplyRatingMod(CR_CRIT_TAKEN_SPELL, int32(val), apply);
            break;
        case ITEM_MOD_HASTE_MELEE_RATING:
            ApplyRatingMod(CR_HASTE_MELEE, int32(val), apply);
            break;
        case ITEM_MOD_HASTE_RANGED_RATING:
            ApplyRatingMod(CR_HASTE_RANGED, int32(val), apply);
            break;
        case ITEM_MOD_HASTE_SPELL_RATING:
            ApplyRatingMod(CR_HASTE_SPELL, int32(val), apply);
            break;
        case ITEM_MOD_HIT_RATING:
            ApplyRatingMod(CR_HIT_MELEE, int32(val), apply);
            ApplyRatingMod(CR_HIT_RANGED, int32(val), apply);
            break;
        case ITEM_MOD_CRIT_RATING:
            ApplyRatingMod(CR_CRIT_MELEE, int32(val), apply);
            ApplyRatingMod(CR_CRIT_RANGED, int32(val), apply);
            break;
        case ITEM_MOD_HIT_TAKEN_RATING:
            ApplyRatingMod(CR_HIT_TAKEN_MELEE, int32(val), apply);
            ApplyRatingMod(CR_HIT_TAKEN_RANGED, int32(val), apply);
            ApplyRatingMod(CR_HIT_TAKEN_SPELL, int32(val), apply);
            break;
        case ITEM_MOD_CRIT_TAKEN_RATING:
            ApplyRatingMod(CR_CRIT_TAKEN_MELEE, int32(val), apply);
            ApplyRatingMod(CR_CRIT_TAKEN_RANGED, int32(val), apply);
            ApplyRatingMod(CR_CRIT_TAKEN_SPELL, int32(val), apply);
            break;
        case ITEM_MOD_RESILIENCE_RATING:
            ApplyRatingMod(CR_CRIT_TAKEN_MELEE, int32(val), apply);
            ApplyRatingMod(CR_CRIT_TAKEN_RANGED, int32(val), apply);
            ApplyRatingMod(CR_CRIT_TAKEN_SPELL, int32(val), apply);
            break;
        case ITEM_MOD_HASTE_RATING:
            ApplyRatingMod(CR_HASTE_MELEE, int32(val), apply);
            ApplyRatingMod(CR_HASTE_RANGED, int32(val), apply);
            break;
        case ITEM_MOD_EXPERTISE_RATING:
            ApplyRatingMod(CR_EXPERTISE, int32(val), apply);
            break;
        }
    }

    if (proto->Armor)
        HandleStatModifier(
            UNIT_MOD_ARMOR, BASE_VALUE, float(proto->Armor), apply);

    if (proto->Block)
        HandleBaseModValue(
            SHIELD_BLOCK_VALUE, FLAT_MOD, float(proto->Block), apply);

    if (proto->HolyRes)
        HandleStatModifier(
            UNIT_MOD_RESISTANCE_HOLY, BASE_VALUE, float(proto->HolyRes), apply);

    if (proto->FireRes)
        HandleStatModifier(
            UNIT_MOD_RESISTANCE_FIRE, BASE_VALUE, float(proto->FireRes), apply);

    if (proto->NatureRes)
        HandleStatModifier(UNIT_MOD_RESISTANCE_NATURE, BASE_VALUE,
            float(proto->NatureRes), apply);

    if (proto->FrostRes)
        HandleStatModifier(UNIT_MOD_RESISTANCE_FROST, BASE_VALUE,
            float(proto->FrostRes), apply);

    if (proto->ShadowRes)
        HandleStatModifier(UNIT_MOD_RESISTANCE_SHADOW, BASE_VALUE,
            float(proto->ShadowRes), apply);

    if (proto->ArcaneRes)
        HandleStatModifier(UNIT_MOD_RESISTANCE_ARCANE, BASE_VALUE,
            float(proto->ArcaneRes), apply);

    WeaponAttackType attType = BASE_ATTACK;
    float damage = 0.0f;

    /*XXX:*/
    if (item_slot.ranged() && (proto->InventoryType == INVTYPE_RANGED ||
                                  proto->InventoryType == INVTYPE_THROWN ||
                                  proto->InventoryType == INVTYPE_RANGEDRIGHT))
    {
        attType = RANGED_ATTACK;
    }
    else if (item_slot.off_hand())
    {
        attType = OFF_ATTACK;
    }

    if (proto->Damage[0].DamageMin > 0)
    {
        damage = apply ? proto->Damage[0].DamageMin : BASE_MINDAMAGE;
        SetBaseWeaponDamage(attType, MINDAMAGE, damage);
        // logging.error("applying mindam: assigning %f to weapon
        // mindamage, now is: %f", damage, GetWeaponDamageRange(attType,
        // MINDAMAGE));
    }

    if (proto->Damage[0].DamageMax > 0)
    {
        damage = apply ? proto->Damage[0].DamageMax : BASE_MAXDAMAGE;
        SetBaseWeaponDamage(attType, MAXDAMAGE, damage);
    }

    if (!CanUseEquippedWeapon(attType))
        return;

    /*XXX:*/
    if (proto->Delay)
    {
        if (item_slot.ranged())
            SetAttackTime(
                RANGED_ATTACK, apply ? proto->Delay : BASE_ATTACK_TIME);
        else if (item_slot.main_hand())
            SetAttackTime(BASE_ATTACK, apply ? proto->Delay : BASE_ATTACK_TIME);
        else if (item_slot.off_hand())
            SetAttackTime(OFF_ATTACK, apply ? proto->Delay : BASE_ATTACK_TIME);
    }

    if (CanModifyStats() && (damage || proto->Delay))
        UpdateDamagePhysical(attType);
}

void Player::_ApplyWeaponDependentAuraMods(
    Item* item, WeaponAttackType attackType, bool apply)
{
    const Auras& auraCritList = GetAurasByType(SPELL_AURA_MOD_CRIT_PERCENT);
    for (const auto& elem : auraCritList)
        _ApplyWeaponDependentAuraCritMod(item, attackType, elem, apply);

    const Auras& auraDamageFlatList =
        GetAurasByType(SPELL_AURA_MOD_DAMAGE_DONE);
    for (const auto& elem : auraDamageFlatList)
        _ApplyWeaponDependentAuraDamageMod(item, attackType, elem, apply);

    const Auras& auraDamagePCTList =
        GetAurasByType(SPELL_AURA_MOD_DAMAGE_PERCENT_DONE);
    for (const auto& elem : auraDamagePCTList)
        _ApplyWeaponDependentAuraDamageMod(item, attackType, elem, apply);

    UpdateExpertise(BASE_ATTACK);
    UpdateExpertise(OFF_ATTACK);
}

void Player::_ApplyWeaponDependentAuraCritMod(
    Item* item, WeaponAttackType attackType, Aura* aura, bool apply)
{
    // generic not weapon specific case processes in aura code
    if (aura->GetSpellProto()->EquippedItemClass == -1)
        return;

    BaseModGroup mod = BASEMOD_END;
    switch (attackType)
    {
    case BASE_ATTACK:
        mod = CRIT_PERCENTAGE;
        break;
    case OFF_ATTACK:
        mod = OFFHAND_CRIT_PERCENTAGE;
        break;
    case RANGED_ATTACK:
        mod = RANGED_CRIT_PERCENTAGE;
        break;
    default:
        return;
    }

    if (item->IsFitToSpellRequirements(aura->GetSpellProto()))
    {
        HandleBaseModValue(
            mod, FLAT_MOD, float(aura->GetModifier()->m_amount), apply);
    }
}

void Player::_ApplyWeaponDependentAuraDamageMod(
    Item* item, WeaponAttackType attackType, Aura* aura, bool apply)
{
    // ignore spell mods for not wands
    Modifier const* modifier = aura->GetModifier();
    if ((modifier->m_miscvalue & SPELL_SCHOOL_MASK_NORMAL) == 0 &&
        (getClassMask() & CLASSMASK_WAND_USERS) == 0)
        return;

    // generic not weapon specific case processes in aura code
    if (aura->GetSpellProto()->EquippedItemClass == -1)
        return;

    UnitMods unitMod = UNIT_MOD_END;
    switch (attackType)
    {
    case BASE_ATTACK:
        unitMod = UNIT_MOD_DAMAGE_MAINHAND;
        break;
    case OFF_ATTACK:
        unitMod = UNIT_MOD_DAMAGE_OFFHAND;
        break;
    case RANGED_ATTACK:
        unitMod = UNIT_MOD_DAMAGE_RANGED;
        break;
    default:
        return;
    }

    UnitModifierType unitModType = TOTAL_VALUE;
    switch (modifier->m_auraname)
    {
    case SPELL_AURA_MOD_DAMAGE_DONE:
        unitModType = TOTAL_VALUE;
        break;
    case SPELL_AURA_MOD_DAMAGE_PERCENT_DONE:
        unitModType = TOTAL_PCT;
        break;
    default:
        return;
    }

    if (item->IsFitToSpellRequirements(aura->GetSpellProto()))
    {
        HandleStatModifier(
            unitMod, unitModType, float(modifier->m_amount), apply);
    }
}

void Player::ApplyEquipSpell(
    SpellEntry const* spellInfo, Item* item, bool apply, bool form_change)
{
    if (apply)
    {
        // Cannot be used in this stance/form
        if (GetErrorAtShapeshiftedCast(spellInfo, GetShapeshiftForm()) !=
            SPELL_CAST_OK)
            return;

        if (form_change && item) // check aura active state from other form
        {
            bool found = false;

            // Only loop auras with the same ID as spellInfo->Id
            loop_auras(
                [&found, item](AuraHolder* holder)
                {
                    if (holder->GetCastItemGuid() == item->GetObjectGuid())
                        found = true;
                    return !found; // break when found is true
                },
                spellInfo->Id);

            if (found) // and skip re-cast already active aura at form change
                return;
        }

        // If there's no item, it's a set bonus, and set bonuses are always
        // unique (the PvP sets +35 resi are solved by having 3 different auras)
        if (form_change && !item && has_aura(spellInfo->Id))
            return;

        LOG_DEBUG(logging, "WORLD: cast %s Equip spellId - %i",
            (item ? "item" : "itemset"), spellInfo->Id);

        CastSpell(this, spellInfo, true, item);
    }
    else
    {
        if (form_change) // check aura compatibility
        {
            // Cannot be used in this stance/form
            if (GetErrorAtShapeshiftedCast(spellInfo, GetShapeshiftForm()) ==
                SPELL_CAST_OK)
                return; // and remove only not compatible at form change
        }

        uint32 id = spellInfo->Id;
        if (item) // remove all spells, not only at-equipped
            remove_auras(id, [item](AuraHolder* holder)
                {
                    return holder->GetCastItemGuid() == item->GetObjectGuid();
                });
        else // remove spells (item set case)
            remove_auras(id);
    }
}

void Player::UpdateEquipSpellsAtFormChange()
{
    /*XXX:*/
    const int mask = inventory::personal_storage::iterator::equipment;
    for (inventory::personal_storage::iterator itr = storage().begin(mask);
         itr != storage().end(); ++itr)
    {
        if (!(*itr)->IsBroken())
        {
            for (int i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
            {
                const _Spell& spell_data = (*itr)->GetProto()->Spells[i];
                if (spell_data.SpellId == 0 ||
                    spell_data.SpellTrigger != ITEM_SPELLTRIGGER_ON_EQUIP)
                    continue;
                if (auto info = sSpellStore.LookupEntry(spell_data.SpellId))
                {
                    ApplyEquipSpell(info, *itr, false, true);
                    ApplyEquipSpell(info, *itr, true, true);
                }
            }
        }
    }

    // item set bonuses not dependent from item broken state
    for (auto& elem : ItemSetEff)
    {
        ItemSetEffect* eff = elem;
        if (!eff)
            continue;

        for (uint32 y = 0; y < 8; ++y)
        {
            SpellEntry const* spellInfo = eff->spells[y];
            if (!spellInfo)
                continue;

            ApplyEquipSpell(spellInfo, nullptr, false,
                true); // remove spells that not fit to form
            ApplyEquipSpell(spellInfo, nullptr, true,
                true); // add spells that fit form but not active
        }
    }
}

/**
 * (un-)Apply item spells triggered at adding item to inventory
 *ITEM_SPELLTRIGGER_ON_STORE
 *
 * @param item  added/removed item to/from inventory
 * @param apply (un-)apply spell affects.
 *
 * Note: item moved from slot to slot in 2 steps RemoveItem and
 *StoreItem/EquipItem
 * In result function not called in RemoveItem for prevent unexpected re-apply
 *auras from related spells
 * with duration reset and etc. Instead unapply done in StoreItem/EquipItem and
 *in specialized
 * functions for item final remove/destroy from inventory. If new RemoveItem
 *calls added need be sure that
 * function will call after it in some way if need.
 */

void Player::ApplyItemOnStoreSpell(Item* item, bool apply)
{
    if (!item)
        return;

    ItemPrototype const* proto = item->GetProto();
    if (!proto)
        return;

    for (int i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
    {
        _Spell const& spellData = proto->Spells[i];

        // no spell
        if (!spellData.SpellId)
            continue;

        // apply/unapply only at-store spells
        if (spellData.SpellTrigger != ITEM_SPELLTRIGGER_ON_STORE)
            continue;

        if (apply)
        {
            // can be attempt re-applied at move in inventory slots
            if (!has_aura(spellData.SpellId))
                CastSpell(this, spellData.SpellId, true, item);
        }
        else
            remove_auras(spellData.SpellId, [item](AuraHolder* holder)
                {
                    return holder->GetCastItemGuid() == item->GetObjectGuid();
                });
    }
}

void Player::CastItemCombatSpell(Unit* Target, WeaponAttackType attType,
    ExtraAttackType extraAttackType, uint32 extraAttackId,
    const SpellEntry* causingSpell)
{
    Item* item = GetWeaponForAttack(attType, true, true);
    if (!item)
        return;

    ItemPrototype const* proto = item->GetProto();
    if (!proto)
        return;

    if (!Target || Target == this)
        return;

    for (int i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
    {
        _Spell const& spellData = proto->Spells[i];

        // no spell
        if (!spellData.SpellId)
            continue;

        // wrong triggering type
        if (spellData.SpellTrigger != ITEM_SPELLTRIGGER_CHANCE_ON_HIT)
            continue;

        SpellEntry const* spellInfo =
            sSpellStore.LookupEntry(spellData.SpellId);
        if (!spellInfo)
        {
            logging.error("WORLD: unknown Item spellid %i", spellData.SpellId);
            continue;
        }

        // HACK: Kael's Infinity Blade should only proc on melee ability
        // TODO: Is there a general way to fix this?
        if (spellInfo->Id == 36478 &&
            (!causingSpell || !causingSpell->HasAttribute(SPELL_ATTR_ABILITY)))
            continue;

        float chance = (float)spellInfo->procChance;

        if (spellData.SpellPPMRate)
        {
            chance = GetPPMProcChance(attType, spellData.SpellPPMRate);
        }
        else if (chance > 100.0f)
        {
            chance = GetWeaponProcChance();
        }

        if (roll_chance_f(chance))
            CastSpell(Target, spellInfo->Id, true, item);
    }

    // item combat enchantments
    for (int e_slot = 0; e_slot < MAX_ENCHANTMENT_SLOT; ++e_slot)
    {
        uint32 enchant_id = item->GetEnchantmentId(EnchantmentSlot(e_slot));
        SpellItemEnchantmentEntry const* pEnchant =
            sSpellItemEnchantmentStore.LookupEntry(enchant_id);
        if (!pEnchant)
            continue;
        for (int s = 0; s < 3; ++s)
        {
            uint32 proc_spell_id = pEnchant->spellid[s];

            // Flametongue Weapon (Passive), Ranks (used not existed equip spell
            // id in pre-3.x spell.dbc)
            if (pEnchant->type[s] == ITEM_ENCHANTMENT_TYPE_EQUIP_SPELL)
            {
                switch (proc_spell_id)
                {
                case 10400:
                    proc_spell_id = 8026;
                    break; // Rank 1
                case 15567:
                    proc_spell_id = 8028;
                    break; // Rank 2
                case 15568:
                    proc_spell_id = 8029;
                    break; // Rank 3
                case 15569:
                    proc_spell_id = 10445;
                    break; // Rank 4
                case 16311:
                    proc_spell_id = 16343;
                    break; // Rank 5
                case 16312:
                    proc_spell_id = 16344;
                    break; // Rank 6
                case 16313:
                    proc_spell_id = 25488;
                    break; // Rank 7
                default:
                    continue;
                }
            }
            else if (pEnchant->type[s] != ITEM_ENCHANTMENT_TYPE_COMBAT_SPELL)
                continue;

            SpellEntry const* spellInfo =
                sSpellStore.LookupEntry(proc_spell_id);
            if (!spellInfo)
            {
                logging.error(
                    "Player::CastItemCombatSpell Enchant %i, cast unknown "
                    "spell %i",
                    pEnchant->ID, proc_spell_id);
                continue;
            }

            // If we're currently doing an extra attack we need to check if we
            // can proc more extra attacks (if proccing spell is of that type)
            if (extraAttackType != EXTRA_ATTACK_NONE &&
                IsSpellHaveEffect(spellInfo, SPELL_EFFECT_ADD_EXTRA_ATTACKS))
            {
                // Can proc none
                if (extraAttackType == EXTRA_ATTACK_PROC_NONE)
                    continue;
                // Can proc other but not self
                else if (extraAttackType == EXTRA_ATTACK_PROC_OTHERS &&
                         proc_spell_id == extraAttackId)
                    continue;
                // Can proc any extra attacks
                // else EXTRA_ATTACK_PROC_ALL
            }

            // Use first rank to access spell item enchant procs
            SpellItemProcEntry proc_entry =
                sSpellMgr::Instance()->GetItemEnchantProcChance(spellInfo->Id);

            if (proc_entry.flags != SPELL_ITEM_ENCH_PROC_ALL)
            {
                bool can_proc = false;
                if ((proc_entry.flags & SPELL_ITEM_ENCH_PROC_ON_MELEE) != 0 &&
                    !causingSpell)
                    can_proc = true;
                if ((proc_entry.flags & SPELL_ITEM_ENCH_PROC_ANY_SPELL) != 0 &&
                    causingSpell)
                    can_proc = true;
                if ((proc_entry.flags &
                        SPELL_ITEM_ENCH_PROC_ON_NEXT_MELEE_SPELL) != 0 &&
                    causingSpell &&
                    (causingSpell->HasAttribute(SPELL_ATTR_ON_NEXT_SWING_1) ||
                        causingSpell->HasAttribute(SPELL_ATTR_ON_NEXT_SWING_2)))
                    can_proc = true;

                if (!can_proc)
                    continue;
            }

            float chance = proc_entry.ppm ?
                               GetPPMProcChance(attType, proc_entry.ppm) :
                               pEnchant->amount[s] != 0 ?
                               float(pEnchant->amount[s]) :
                               GetWeaponProcChance();

            ApplySpellMod(spellInfo->Id, SPELLMOD_CHANCE_OF_SUCCESS, chance);

            if (roll_chance_f(chance))
            {
                if (IsPositiveSpell(spellInfo->Id))
                    CastSpell(this, spellInfo->Id, true, item);
                else
                    CastSpell(Target, spellInfo->Id, true, item);
            }
        }
    }
}

void Player::CastItemUseSpell(
    Item* item, SpellCastTargets const& targets, uint8 cast_count)
{
    ItemPrototype const* proto = item->GetProto();
    // special learning case
    if (proto->Spells[0].SpellId == SPELL_ID_GENERIC_LEARN)
    {
        uint32 learn_spell_id = proto->Spells[0].SpellId;
        uint32 learning_spell_id = proto->Spells[1].SpellId;

        SpellEntry const* spellInfo = sSpellStore.LookupEntry(learn_spell_id);
        if (!spellInfo)
        {
            logging.error(
                "Player::CastItemUseSpell: Item (Entry: %u) in have wrong "
                "spell id %u, ignoring ",
                proto->ItemId, learn_spell_id);
            SendEquipError(EQUIP_ERR_NONE, item);
            return;
        }

        auto spell = new Spell(this, spellInfo, false);
        spell->set_cast_item(item);
        spell->m_cast_count = cast_count; // set count of casts
        spell->m_currentBasePoints[EFFECT_INDEX_0] = learning_spell_id;
        spell->prepare(&targets);
        return;
    }

    // use triggered flag only for items with many spell casts and for not first
    // cast
    int count = 0;

    // item spells casted at use
    for (int i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
    {
        _Spell const& spellData = proto->Spells[i];

        // no spell
        if (!spellData.SpellId)
            continue;

        // wrong triggering type
        if (spellData.SpellTrigger != ITEM_SPELLTRIGGER_ON_USE)
            continue;

        SpellEntry const* spellInfo =
            sSpellStore.LookupEntry(spellData.SpellId);
        if (!spellInfo)
        {
            logging.error(
                "Player::CastItemUseSpell: Item (Entry: %u) in have wrong "
                "spell id %u, ignoring",
                proto->ItemId, spellData.SpellId);
            continue;
        }

        auto spell = new Spell(this, spellInfo, (count > 0));
        spell->set_cast_item(item);
        spell->m_cast_count = cast_count; // set count of casts
        spell->prepare(&targets);

        ++count;
    }
}

void Player::_RemoveAllItemMods()
{
    /*XXX:*/

    const int mask = inventory::personal_storage::iterator::equipment |
                     inventory::personal_storage::iterator::bags;
    for (inventory::personal_storage::iterator itr = storage().begin(mask);
         itr != storage().end(); ++itr)
    {
        const ItemPrototype* proto = (*itr)->GetProto();

        // item set bonuses not dependent from item broken state
        if (proto->ItemSet)
            RemoveItemsSetItem(this, proto);

        if ((*itr)->IsBroken())
            continue;

        ApplyEnchantment(*itr, false, (*itr)->slot());
    }

    for (inventory::personal_storage::iterator itr = storage().begin(mask);
         itr != storage().end(); ++itr)
    {
        if ((*itr)->IsBroken())
            continue;
        inventory::slot slot = (*itr)->slot();

        int type = MAX_ATTACK;
        if (slot.main_hand())
            type = BASE_ATTACK;
        else if (slot.off_hand())
            type = OFF_ATTACK;
        else if (slot.ranged())
            type = RANGED_ATTACK;

        if (type < MAX_ATTACK)
            _ApplyWeaponDependentAuraMods(
                *itr, static_cast<WeaponAttackType>(type), false);

        _ApplyItemBonuses(*itr, false, (*itr)->slot());

        if ((*itr)->slot().ranged())
            _ApplyAmmoBonuses();
    }
}

void Player::_ApplyAllItemMods()
{
    /*XXX:*/
    const int mask = inventory::personal_storage::iterator::equipment |
                     inventory::personal_storage::iterator::bags;
    for (inventory::personal_storage::iterator itr = storage().begin(mask);
         itr != storage().end(); ++itr)
    {
        Item* item = *itr;
        if (item->IsBroken())
            continue;

        inventory::slot slot = item->slot();

        int type = MAX_ATTACK;
        if (slot.main_hand())
            type = BASE_ATTACK;
        else if (slot.off_hand())
            type = OFF_ATTACK;
        else if (slot.ranged())
            type = RANGED_ATTACK;

        if (type < MAX_ATTACK)
            _ApplyWeaponDependentAuraMods(
                item, static_cast<WeaponAttackType>(type), true);

        _ApplyItemBonuses(item, true, item->slot());

        if (item->slot().ranged())
            _ApplyAmmoBonuses();
    }

    const int mask2 = inventory::personal_storage::iterator::equipment |
                      inventory::personal_storage::iterator::bags;
    for (inventory::personal_storage::iterator itr = storage().begin(mask2);
         itr != storage().end(); ++itr)
    {
        Item* item = *itr;
        ItemPrototype const* proto = item->GetProto();

        // item set bonuses not dependent from item broken state
        if (proto->ItemSet)
            AddItemsSetItem(this, item);

        if (item->IsBroken())
            continue;

        // XXX ApplyItemEquipSpell(item,true);
        ApplyEnchantment(item, true, item->slot());
    }
}

void Player::_ApplyAmmoBonuses()
{
    // check ammo
    uint32 ammo_id = GetUInt32Value(PLAYER_AMMO_ID);
    if (!ammo_id)
        return;

    float currentAmmoDPS;

    ItemPrototype const* ammo_proto = ObjectMgr::GetItemPrototype(ammo_id);
    if (!ammo_proto || ammo_proto->Class != ITEM_CLASS_PROJECTILE ||
        !CheckAmmoCompatibility(ammo_proto))
        currentAmmoDPS = 0.0f;
    else
        currentAmmoDPS = ammo_proto->Damage[0].DamageMin;

    if (currentAmmoDPS == GetAmmoDPS())
        return;

    m_ammoDPS = currentAmmoDPS;

    if (CanModifyStats())
        UpdateDamagePhysical(RANGED_ATTACK);
}

bool Player::CheckAmmoCompatibility(const ItemPrototype* ammo_proto) const
{
    if (!ammo_proto)
        return false;

    // check ranged weapon
    Item* weapon = GetWeaponForAttack(RANGED_ATTACK, true, false);
    if (!weapon)
        return false;

    ItemPrototype const* weapon_proto = weapon->GetProto();
    if (!weapon_proto || weapon_proto->Class != ITEM_CLASS_WEAPON)
        return false;

    // check ammo ws. weapon compatibility
    switch (weapon_proto->SubClass)
    {
    case ITEM_SUBCLASS_WEAPON_BOW:
    case ITEM_SUBCLASS_WEAPON_CROSSBOW:
        if (ammo_proto->SubClass != ITEM_SUBCLASS_ARROW)
            return false;
        break;
    case ITEM_SUBCLASS_WEAPON_GUN:
        if (ammo_proto->SubClass != ITEM_SUBCLASS_BULLET)
            return false;
        break;
    default:
        return false;
    }

    return true;
}

/*  If in a battleground a player dies, and an enemy removes the insignia, the
   player's bones is lootable
    Called by remove insignia spell effect    */
void Player::RemovedInsignia(Player* /*looterPlr*/)
{
    if (!GetBattleGroundId())
        return;

    // If spirit isn't released, release and spawn bones
    if (m_deathTimer > 0)
    {
        // Spawn bones
        Corpse* bones = new Corpse(CORPSE_BONES);
        if (!bones->Create(
                sObjectMgr::Instance()->GenerateCorpseLowGuid(), this))
        {
            delete bones;
            return;
        }

        bones->SetByteValue(CORPSE_FIELD_BYTES_1, 1, getRace());
        bones->SetByteValue(CORPSE_FIELD_BYTES_1, 2, getGender());

        bones->SetTime(WorldTimer::time_no_syscall());
        bones->SetType(CORPSE_BONES);
        bones->Relocate(GetX(), GetY(), GetZ());
        bones->SetOrientation(GetO());

        bones->SetUInt32Value(
            CORPSE_FIELD_FLAGS, CORPSE_FLAG_UNK2 | CORPSE_FLAG_BONES);
        bones->SetGuidValue(CORPSE_FIELD_OWNER, GetObjectGuid());

        bones->SetFlag(CORPSE_FIELD_DYNAMIC_FLAGS, CORPSE_DYNFLAG_LOOTABLE);

        // We store the level of our player in the gold field
        // We retrieve this information at Player::SendLoot()
        bones->SetOwnerLevel(getLevel());

        // Repop player at graveyard
        m_deathTimer = 0;
        BuildPlayerRepop(false);
        RepopAtGraveyard();

        if (!GetMap()->insert(bones))
        {
            delete bones;
            return;
        }
    }
    // Otherwise, convert corpse to bones
    else
    {
        Corpse* corpse = GetCorpse();
        if (!corpse)
            return;

        sObjectAccessor::Instance()->ConvertCorpseForPlayer(
            GetObjectGuid(), true);
    }

    // TODO: Sending loot doesn't work
    /* auto guid = bones->GetObjectGuid();
    looterPlr->queue_action_ticks(0, [looterPlr, guid]()
        {
            looterPlr->SendLoot(guid, LOOT_INSIGNIA);
        }); */
}

void Player::OverwriteSession(std::shared_ptr<WorldSession> session)
{
    m_session = std::move(session);
    PlayerTalkClass->mGossipMenu.m_session = m_session.get();
}

void Player::SendLootRelease(ObjectGuid guid)
{
    WorldPacket data(SMSG_LOOT_RELEASE_RESPONSE, (8 + 1));
    data << guid;
    data << uint8(1);
    SendDirectMessage(std::move(data));
}

void Player::SendLoot(ObjectGuid guid, LootType loot_type)
{
    LOG_DEBUG(logging, "Player::SendLoot (Looted target: %s, Loot Type: %i)",
        guid.GetString().c_str(), loot_type);

    Object* obj = nullptr;
    if (guid.GetHigh() == HIGHGUID_ITEM)
        obj = GetItemByGuid(guid);
    else
        obj = GetMap()->GetWorldObject(guid);
    if (!obj)
    {
        SendLootRelease(guid);
        return;
    }

    obj->OnLootOpen(loot_type, this);

    if (obj->GetLootDistributor())
    {
        if (obj->GetLootDistributor()->display_loot(this))
            return; // Success; skip release below
    }

    SendLootRelease(guid);
    if (guid.IsGameObject())
        static_cast<GameObject*>(obj)->SetLootState(GO_READY);
}

void Player::SendNotifyLootMoneyRemoved()
{
    WorldPacket data(SMSG_LOOT_CLEAR_MONEY, 0);
    GetSession()->send_packet(std::move(data));
}

void Player::SendNotifyLootItemRemoved(uint8 lootSlot)
{
    WorldPacket data(SMSG_LOOT_REMOVED, 1);
    data << uint8(lootSlot);
    GetSession()->send_packet(std::move(data));
}

void Player::SendUpdateWorldState(uint32 Field, uint32 Value)
{
    WorldPacket data(SMSG_UPDATE_WORLD_STATE, 8);
    data << Field;
    data << Value;
    GetSession()->send_packet(std::move(data));
}

static WorldStatePair AV_world_states[] = {{0x7ae, 0x1}, // 1966  7 snowfall n
    {0x532, 0x1}, // 1330  8 frostwolfhut hc
    {0x531, 0x0}, // 1329  9 frostwolfhut ac
    {0x52e, 0x0}, // 1326 10 stormpike firstaid a_a
    {0x571, 0x0}, // 1393 11 east frostwolf tower horde assaulted -unused
    {0x570, 0x0}, // 1392 12 west frostwolf tower horde assaulted - unused
    {0x567, 0x1}, // 1383 13 frostwolfe c
    {0x566, 0x1}, // 1382 14 frostwolfw c
    {0x550, 0x1}, // 1360 15 irondeep (N) ally
    {0x544, 0x0}, // 1348 16 ice grave a_a
    {0x536, 0x0}, // 1334 17 stormpike grave h_c
    {0x535, 0x1}, // 1333 18 stormpike grave a_c
    {0x518, 0x0}, // 1304 19 stoneheart grave a_a
    {0x517, 0x0}, // 1303 20 stoneheart grave h_a
    {0x574, 0x0}, // 1396 21 unk
    {0x573, 0x0}, // 1395 22 iceblood tower horde assaulted -unused
    {0x572, 0x0}, // 1394 23 towerpoint horde assaulted - unused
    {0x56f, 0x0}, // 1391 24 unk
    {0x56e, 0x0}, // 1390 25 iceblood a
    {0x56d, 0x0}, // 1389 26 towerp a
    {0x56c, 0x0}, // 1388 27 frostwolfe a
    {0x56b, 0x0}, // 1387 28 froswolfw a
    {0x56a, 0x1}, // 1386 29 unk
    {0x569, 0x1}, // 1385 30 iceblood c
    {0x568, 0x1}, // 1384 31 towerp c
    {0x565, 0x0}, // 1381 32 stoneh tower a
    {0x564, 0x0}, // 1380 33 icewing tower a
    {0x563, 0x0}, // 1379 34 dunn a
    {0x562, 0x0}, // 1378 35 duns a
    {0x561, 0x0}, // 1377 36 stoneheart bunker alliance assaulted - unused
    {0x560, 0x0}, // 1376 37 icewing bunker alliance assaulted - unused
    {0x55f, 0x0}, // 1375 38 dunbaldar south alliance assaulted - unused
    {0x55e, 0x0}, // 1374 39 dunbaldar north alliance assaulted - unused
    {0x55d, 0x0}, // 1373 40 stone tower d
    {0x3c6, 0x0}, //  966 41 unk
    {0x3c4, 0x0}, //  964 42 unk
    {0x3c2, 0x0}, //  962 43 unk
    {0x516, 0x1}, // 1302 44 stoneheart grave a_c
    {0x515, 0x0}, // 1301 45 stonheart grave h_c
    {0x3b6, 0x0}, //  950 46 unk
    {0x55c, 0x0}, // 1372 47 icewing tower d
    {0x55b, 0x0}, // 1371 48 dunn d
    {0x55a, 0x0}, // 1370 49 duns d
    {0x559, 0x0}, // 1369 50 unk
    {0x558, 0x0}, // 1368 51 iceblood d
    {0x557, 0x0}, // 1367 52 towerp d
    {0x556, 0x0}, // 1366 53 frostwolfe d
    {0x555, 0x0}, // 1365 54 frostwolfw d
    {0x554, 0x1}, // 1364 55 stoneh tower c
    {0x553, 0x1}, // 1363 56 icewing tower c
    {0x552, 0x1}, // 1362 57 dunn c
    {0x551, 0x1}, // 1361 58 duns c
    {0x54f, 0x0}, // 1359 59 irondeep (N) horde
    {0x54e, 0x0}, // 1358 60 irondeep (N) ally
    {0x54d, 0x1}, // 1357 61 mine (S) neutral
    {0x54c, 0x0}, // 1356 62 mine (S) horde
    {0x54b, 0x0}, // 1355 63 mine (S) ally
    {0x545, 0x0}, // 1349 64 iceblood h_a
    {0x543, 0x1}, // 1347 65 iceblod h_c
    {0x542, 0x0}, // 1346 66 iceblood a_c
    {0x540, 0x0}, // 1344 67 snowfall h_a
    {0x53f, 0x0}, // 1343 68 snowfall a_a
    {0x53e, 0x0}, // 1342 69 snowfall h_c
    {0x53d, 0x0}, // 1341 70 snowfall a_c
    {0x53c, 0x0}, // 1340 71 frostwolf g h_a
    {0x53b, 0x0}, // 1339 72 frostwolf g a_a
    {0x53a, 0x1}, // 1338 73 frostwolf g h_c
    {0x539, 0x0}, // l33t 74 frostwolf g a_c
    {0x538, 0x0}, // 1336 75 stormpike grave h_a
    {0x537, 0x0}, // 1335 76 stormpike grave a_a
    {0x534, 0x0}, // 1332 77 frostwolf hut h_a
    {0x533, 0x0}, // 1331 78 frostwolf hut a_a
    {0x530, 0x0}, // 1328 79 stormpike first aid h_a
    {0x52f, 0x0}, // 1327 80 stormpike first aid h_c
    {0x52d, 0x1}, // 1325 81 stormpike first aid a_c
    {0x0, 0x0}};

static WorldStatePair WS_world_states[] = {
    {0x62d, 0x0}, // 1581  7 alliance flag captures
    {0x62e, 0x0}, // 1582  8 horde flag captures
    {0x609, 0x0}, // 1545  9 unk, set to 1 on alliance flag pickup...
    {0x60a,
     0x0}, // 1546 10 unk, set to 1 on horde flag pickup, after drop it's -1
    {0x60b, 0x2}, // 1547 11 unk
    {0x641, 0x3}, // 1601 12 unk (max flag captures?)
    {0x922, 0x1}, // 2338 13 horde (0 - hide, 1 - flag ok, 2 - flag picked up
                  // (flashing), 3 - flag picked up (not flashing)
    {0x923, 0x1}, // 2339 14 alliance (0 - hide, 1 - flag ok, 2 - flag picked up
                  // (flashing), 3 - flag picked up (not flashing)
    {0x0, 0x0}};

static WorldStatePair AB_world_states[] = {
    {0x6e7, 0x0}, // 1767  7 stables alliance
    {0x6e8, 0x0}, // 1768  8 stables horde
    {0x6e9, 0x0}, // 1769  9 unk, ST?
    {0x6ea, 0x0}, // 1770 10 stables (show/hide)
    {0x6ec,
     0x0}, // 1772 11 farm (0 - horde controlled, 1 - alliance controlled)
    {0x6ed, 0x0},   // 1773 12 farm (show/hide)
    {0x6ee, 0x0},   // 1774 13 farm color
    {0x6ef, 0x0},   // 1775 14 gold mine color, may be FM?
    {0x6f0, 0x0},   // 1776 15 alliance resources
    {0x6f1, 0x0},   // 1777 16 horde resources
    {0x6f2, 0x0},   // 1778 17 horde bases
    {0x6f3, 0x0},   // 1779 18 alliance bases
    {0x6f4, 0x7d0}, // 1780 19 max resources (2000)
    {0x6f6, 0x0},   // 1782 20 blacksmith color
    {0x6f7, 0x0},   // 1783 21 blacksmith (show/hide)
    {0x6f8, 0x0},   // 1784 22 unk, bs?
    {0x6f9, 0x0},   // 1785 23 unk, bs?
    {0x6fb, 0x0},   // 1787 24 gold mine (0 - horde contr, 1 - alliance contr)
    {0x6fc, 0x0},   // 1788 25 gold mine (0 - conflict, 1 - horde)
    {0x6fd, 0x0},   // 1789 26 gold mine (1 - show/0 - hide)
    {0x6fe, 0x0},   // 1790 27 gold mine color
    {0x700, 0x0},   // 1792 28 gold mine color, wtf?, may be LM?
    {0x701, 0x0},   // 1793 29 lumber mill color (0 - conflict, 1 - horde contr)
    {0x702, 0x0},   // 1794 30 lumber mill (show/hide)
    {0x703, 0x0},   // 1795 31 lumber mill color color
    {0x732, 0x1},   // 1842 32 stables (1 - uncontrolled)
    {0x733, 0x1},   // 1843 33 gold mine (1 - uncontrolled)
    {0x734, 0x1},   // 1844 34 lumber mill (1 - uncontrolled)
    {0x735, 0x1},   // 1845 35 farm (1 - uncontrolled)
    {0x736, 0x1},   // 1846 36 blacksmith (1 - uncontrolled)
    {0x745, 0x2},   // 1861 37 unk
    {0x7a3, 0x708}, // 1955 38 warning limit (1800)
    {0x0, 0x0}};

static WorldStatePair EY_world_states[] = {{0xac1, 0x0}, // 2753  7 Horde Bases
    {0xac0, 0x0}, // 2752  8 Alliance Bases
    {0xab6, 0x0}, // 2742  9 Mage Tower - Horde conflict
    {0xab5, 0x0}, // 2741 10 Mage Tower - Alliance conflict
    {0xab4, 0x0}, // 2740 11 Fel Reaver - Horde conflict
    {0xab3, 0x0}, // 2739 12 Fel Reaver - Alliance conflict
    {0xab2, 0x0}, // 2738 13 Draenei - Alliance conflict
    {0xab1, 0x0}, // 2737 14 Draenei - Horde conflict
    {0xab0, 0x0}, // 2736 15 unk // 0 at start
    {0xaaf, 0x0}, // 2735 16 unk // 0 at start
    {0xaad, 0x0}, // 2733 17 Draenei - Horde control
    {0xaac, 0x0}, // 2732 18 Draenei - Alliance control
    {0xaab, 0x1}, // 2731 19 Draenei uncontrolled (1 - yes, 0 - no)
    {0xaaa, 0x0}, // 2730 20 Mage Tower - Alliance control
    {0xaa9, 0x0}, // 2729 21 Mage Tower - Horde control
    {0xaa8, 0x1}, // 2728 22 Mage Tower uncontrolled (1 - yes, 0 - no)
    {0xaa7, 0x0}, // 2727 23 Fel Reaver - Horde control
    {0xaa6, 0x0}, // 2726 24 Fel Reaver - Alliance control
    {0xaa5, 0x1}, // 2725 25 Fel Reaver uncontrolled (1 - yes, 0 - no)
    {0xaa4, 0x0}, // 2724 26 Boold Elf - Horde control
    {0xaa3, 0x0}, // 2723 27 Boold Elf - Alliance control
    {0xaa2, 0x1}, // 2722 28 Boold Elf uncontrolled (1 - yes, 0 - no)
    {0xac5, 0x1}, // 2757 29 Flag (1 - show, 0 - hide) - doesn't work exactly
                  // this way!
    {0xad2, 0x1}, // 2770 30 Horde top-stats (1 - show, 0 - hide) // 02 -> horde
                  // picked up the flag
    {0xad1, 0x1}, // 2769 31 Alliance top-stats (1 - show, 0 - hide) // 02 ->
                  // alliance picked up the flag
    {0xabe, 0x0}, // 2750 32 Horde resources
    {0xabd, 0x0}, // 2749 33 Alliance resources
    {0xa05, 0x8e}, // 2565 34 unk, constant?
    {0xaa0, 0x0}, // 2720 35 Capturing progress-bar (100 -> empty (only grey), 0
                  // -> blue|red (no grey), default 0)
    {0xa9f, 0x0}, // 2719 36 Capturing progress-bar (0 - left, 100 - right)
    {0xa9e, 0x0}, // 2718 37 Capturing progress-bar (1 - show, 0 - hide)
    {0xc0d, 0x17b}, // 3085 38 unk
    // and some more ... unknown
    {0x0, 0x0}};

static WorldStatePair SI_world_states[] = // Silithus
    {
     {2313, 0}, // WORLD_STATE_SI_GATHERED_A
     {2314, 0}, // WORLD_STATE_SI_GATHERED_H
     {2317, 0}  // WORLD_STATE_SI_SILITHYST_MAX
};

static WorldStatePair EP_world_states[] = // Eastern Plaguelands
    {
     {2327, 0},                  // WORLD_STATE_EP_TOWER_COUNT_ALLIANCE
     {2328, 0},                  // WORLD_STATE_EP_TOWER_COUNT_HORDE
     {2355, WORLD_STATE_REMOVE}, // WORLD_STATE_EP_CROWNGUARD_NEUTRAL
     {2374, WORLD_STATE_REMOVE}, // WORLD_STATE_EP_CROWNGUARD_CONTEST_ALLIANCE
     {2375, WORLD_STATE_REMOVE}, // WORLD_STATE_EP_CROWNGUARD_CONTEST_HORDE
     {2376, WORLD_STATE_REMOVE}, // WORLD_STATE_EP_CROWNGUARD_PROGRESS_ALLIANCE
     {2377, WORLD_STATE_REMOVE}, // WORLD_STATE_EP_CROWNGUARD_PROGRESS_HORDE
     {2378, WORLD_STATE_REMOVE}, // WORLD_STATE_EP_CROWNGUARD_ALLIANCE
     {2379, WORLD_STATE_REMOVE}, // WORLD_STATE_EP_CROWNGUARD_HORDE
     {2354, WORLD_STATE_REMOVE}, // WORLD_STATE_EP_EASTWALL_ALLIANCE
     {2356, WORLD_STATE_REMOVE}, // WORLD_STATE_EP_EASTWALL_HORDE
     {2357, WORLD_STATE_REMOVE}, // WORLD_STATE_EP_EASTWALL_PROGRESS_ALLIANCE
     {2358, WORLD_STATE_REMOVE}, // WORLD_STATE_EP_EASTWALL_PROGRESS_HORDE
     {2359, WORLD_STATE_REMOVE}, // WORLD_STATE_EP_EASTWALL_CONTEST_ALLIANCE
     {2360, WORLD_STATE_REMOVE}, // WORLD_STATE_EP_EASTWALL_CONTEST_HORDE
     {2361, WORLD_STATE_REMOVE}, // WORLD_STATE_EP_EASTWALL_NEUTRAL
     {2352, WORLD_STATE_REMOVE}, // WORLD_STATE_EP_NORTHPASS_NEUTRAL
     {2362, WORLD_STATE_REMOVE}, // WORLD_STATE_EP_NORTHPASS_CONTEST_ALLIANCE
     {2363, WORLD_STATE_REMOVE}, // WORLD_STATE_EP_NORTHPASS_CONTEST_HORDE
     {2364, WORLD_STATE_REMOVE}, // WORLD_STATE_EP_NORTHPASS_PROGRESS_ALLIANCE
     {2365, WORLD_STATE_REMOVE}, // WORLD_STATE_EP_NORTHPASS_PROGRESS_HORDE
     {2372, WORLD_STATE_REMOVE}, // WORLD_STATE_EP_NORTHPASS_ALLIANCE
     {2373, WORLD_STATE_REMOVE}, // WORLD_STATE_EP_NORTHPASS_HORDE
     {2353, WORLD_STATE_REMOVE}, // WORLD_STATE_EP_PLAGUEWOOD_NEUTRAL
     {2366, WORLD_STATE_REMOVE}, // WORLD_STATE_EP_PLAGUEWOOD_CONTEST_ALLIANCE
     {2367, WORLD_STATE_REMOVE}, // WORLD_STATE_EP_PLAGUEWOOD_CONTEST_HORDE -
     // not in dbc! sent for consistency's sake, and
     // to match field count
     {2368, WORLD_STATE_REMOVE}, // WORLD_STATE_EP_PLAGUEWOOD_PROGRESS_ALLIANCE
     {2369, WORLD_STATE_REMOVE}, // WORLD_STATE_EP_PLAGUEWOOD_PROGRESS_HORDE
     {2370, WORLD_STATE_REMOVE}, // WORLD_STATE_EP_PLAGUEWOOD_ALLIANCE
     {2371, WORLD_STATE_REMOVE}  // WORLD_STATE_EP_PLAGUEWOOD_HORDE
};

static WorldStatePair HP_world_states[] = // Hellfire Peninsula
    {
     {2490, WORLD_STATE_REMOVE}, // WORLD_STATE_HP_TOWER_DISPLAY_A
     {2489, WORLD_STATE_REMOVE}, // WORLD_STATE_HP_TOWER_DISPLAY_H
     {2485, WORLD_STATE_REMOVE}, // WORLD_STATE_HP_BROKEN_HILL_NEUTRAL
     {2484, WORLD_STATE_REMOVE}, // WORLD_STATE_HP_BROKEN_HILL_HORDE
     {2483, WORLD_STATE_REMOVE}, // WORLD_STATE_HP_BROKEN_HILL_ALLIANCE
     {2482, WORLD_STATE_REMOVE}, // WORLD_STATE_HP_OVERLOOK_NEUTRAL
     {2481, WORLD_STATE_REMOVE}, // WORLD_STATE_HP_OVERLOOK_HORDE
     {2480, WORLD_STATE_REMOVE}, // WORLD_STATE_HP_OVERLOOK_ALLIANCE
     {2478, 0},                  // WORLD_STATE_HP_TOWER_COUNT_HORDE
     {2476, 0},                  // WORLD_STATE_HP_TOWER_COUNT_ALLIANCE
     {2472, WORLD_STATE_REMOVE}, // WORLD_STATE_HP_STADIUM_NEUTRAL
     {2471, WORLD_STATE_REMOVE}, // WORLD_STATE_HP_STADIUM_ALLIANCE
     {2470, WORLD_STATE_REMOVE}  // WORLD_STATE_HP_STADIUM_HORDE
};

static WorldStatePair TF_world_states[] = // Terokkar Forest
    {
     {2622, 0},                  // WORLD_STATE_TF_TOWER_COUNT_H
     {2621, 0},                  // WORLD_STATE_TF_TOWER_COUNT_A
     {2620, WORLD_STATE_REMOVE}, // WORLD_STATE_TF_TOWERS_CONTROLLED
     {2695, WORLD_STATE_REMOVE}, // WORLD_STATE_TF_SOUTH_EAST_TOWER_HORDE
     {2694, WORLD_STATE_REMOVE}, // WORLD_STATE_TF_SOUTH_EAST_TOWER_ALLIANCE
     {2693, WORLD_STATE_REMOVE}, // WORLD_STATE_TF_SOUTH_TOWER_NEUTRAL
     {2692, WORLD_STATE_REMOVE}, // WORLD_STATE_TF_SOUTH_TOWER_HORDE
     {2691, WORLD_STATE_REMOVE}, // WORLD_STATE_TF_SOUTH_TOWER_ALLIANCE
     {2690, WORLD_STATE_REMOVE}, // WORLD_STATE_TF_EAST_TOWER_NEUTRAL
     {2689, WORLD_STATE_REMOVE}, // WORLD_STATE_TF_EAST_TOWER_HORDE
     {2688, WORLD_STATE_REMOVE}, // WORLD_STATE_TF_EAST_TOWER_ALLIANCE
     {2686, WORLD_STATE_REMOVE}, // WORLD_STATE_TF_NORTH_TOWER_NEUTRAL
     {2685, WORLD_STATE_REMOVE}, // WORLD_STATE_TF_NORTH_TOWER_HORDE
     {2684, WORLD_STATE_REMOVE}, // WORLD_STATE_TF_NORTH_TOWER_ALLIANCE
     {2683, WORLD_STATE_REMOVE}, // WORLD_STATE_TF_WEST_TOWER_ALLIANCE
     {2682, WORLD_STATE_REMOVE}, // WORLD_STATE_TF_WEST_TOWER_HORDE
     {2681, WORLD_STATE_REMOVE}, // WORLD_STATE_TF_WEST_TOWER_NEUTRAL
     {2512, 0},                  // WORLD_STATE_TF_TIME_MIN_FIRST_DIGIT
     {2510, 0},                  // WORLD_STATE_TF_TIME_MIN_SECOND_DIGIT
     {2509, 0},                  // WORLD_STATE_TF_TIME_HOURS
     {2508, WORLD_STATE_REMOVE}, // WORLD_STATE_TF_LOCKED_NEUTRAL
     {2696, WORLD_STATE_REMOVE}, // WORLD_STATE_TF_SOUTH_EAST_TOWER_NEUTRAL
     {2768, WORLD_STATE_REMOVE}, // WORLD_STATE_TF_LOCKED_HORDE
     {2767, WORLD_STATE_REMOVE}  // WORLD_STATE_TF_LOCKED_ALLIANCE
};

static WorldStatePair ZM_world_states[] = // Zangarmarsh
    {
     {2653, 0x1},                // WORLD_STATE_ZM_UNK
     {2652, WORLD_STATE_REMOVE}, // WORLD_STATE_ZM_BEACON_EAST_NEUTRAL
     {2651, WORLD_STATE_REMOVE}, // WORLD_STATE_ZM_BEACON_EAST_HORDE
     {2650, WORLD_STATE_REMOVE}, // WORLD_STATE_ZM_BEACON_EAST_ALLIANCE
     {2649, WORLD_STATE_REMOVE}, // WORLD_STATE_ZM_GRAVEYARD_HORDE
     {2648, WORLD_STATE_REMOVE}, // WORLD_STATE_ZM_GRAVEYARD_ALLIANCE
     {2647, WORLD_STATE_REMOVE}, // WORLD_STATE_ZM_GRAVEYARD_NEUTRAL
     {2646, WORLD_STATE_REMOVE}, // WORLD_STATE_ZM_BEACON_WEST_NEUTRAL
     {2645, WORLD_STATE_REMOVE}, // WORLD_STATE_ZM_BEACON_WEST_HORDE
     {2644, WORLD_STATE_REMOVE}, // WORLD_STATE_ZM_BEACON_WEST_ALLIANCE
     {2560, WORLD_STATE_REMOVE}, // WORLD_STATE_ZM_BEACON_EAST_UI_NEUTRAL
     {2559, WORLD_STATE_REMOVE}, // WORLD_STATE_ZM_BEACON_EAST_UI_HORDE
     {2558, WORLD_STATE_REMOVE}, // WORLD_STATE_ZM_BEACON_EAST_UI_ALLIANCE
     {2557, WORLD_STATE_REMOVE}, // WORLD_STATE_ZM_BEACON_WEST_UI_NEUTRAL
     {2556, WORLD_STATE_REMOVE}, // WORLD_STATE_ZM_BEACON_WEST_UI_HORDE
     {2555, WORLD_STATE_REMOVE}, // WORLD_STATE_ZM_BEACON_WEST_UI_ALLIANCE
     {2658, WORLD_STATE_REMOVE}, // WORLD_STATE_ZM_FLAG_READY_HORDE
     {2657, WORLD_STATE_REMOVE}, // WORLD_STATE_ZM_FLAG_NOT_READY_HORDE
     {2656, WORLD_STATE_REMOVE}, // WORLD_STATE_ZM_FLAG_NOT_READY_ALLIANCE
     {2655, WORLD_STATE_REMOVE}  // WORLD_STATE_ZM_FLAG_READY_ALLIANCE
};

static WorldStatePair NA_world_states[] = {
    {2503, 0},                  // WORLD_STATE_NA_GUARDS_HORDE
    {2502, 0},                  // WORLD_STATE_NA_GUARDS_ALLIANCE
    {2493, 0},                  // WORLD_STATE_NA_GUARDS_MAX
    {2491, 0},                  // WORLD_STATE_NA_GUARDS_LEFT
    {2762, WORLD_STATE_REMOVE}, // WORLD_STATE_NA_WYVERN_NORTH_NEUTRAL_H
    {2662, WORLD_STATE_REMOVE}, // WORLD_STATE_NA_WYVERN_NORTH_NEUTRAL_A
    {2663, WORLD_STATE_REMOVE}, // WORLD_STATE_NA_WYVERN_NORTH_H
    {2664, WORLD_STATE_REMOVE}, // WORLD_STATE_NA_WYVERN_NORTH_A
    {2760, WORLD_STATE_REMOVE}, // WORLD_STATE_NA_WYVERN_SOUTH_NEUTRAL_H
    {2670, WORLD_STATE_REMOVE}, // WORLD_STATE_NA_WYVERN_SOUTH_NEUTRAL_A
    {2668, WORLD_STATE_REMOVE}, // WORLD_STATE_NA_WYVERN_SOUTH_H
    {2669, WORLD_STATE_REMOVE}, // WORLD_STATE_NA_WYVERN_SOUTH_A
    {2761, WORLD_STATE_REMOVE}, // WORLD_STATE_NA_WYVERN_WEST_NEUTRAL_H
    {2667, WORLD_STATE_REMOVE}, // WORLD_STATE_NA_WYVERN_WEST_NEUTRAL_A
    {2665, WORLD_STATE_REMOVE}, // WORLD_STATE_NA_WYVERN_WEST_H
    {2666, WORLD_STATE_REMOVE}, // WORLD_STATE_NA_WYVERN_WEST_A
    {2763, WORLD_STATE_REMOVE}, // WORLD_STATE_NA_WYVERN_EAST_NEUTRAL_H
    {2659, WORLD_STATE_REMOVE}, // WORLD_STATE_NA_WYVERN_EAST_NEUTRAL_A
    {2660, WORLD_STATE_REMOVE}, // WORLD_STATE_NA_WYVERN_EAST_H
    {2661, WORLD_STATE_REMOVE}, // WORLD_STATE_NA_WYVERN_EAST_A
    {2671, WORLD_STATE_REMOVE}, // WORLD_STATE_NA_HALAA_NEUTRAL
    {2676, WORLD_STATE_REMOVE}, // WORLD_STATE_NA_HALAA_NEUTRAL_A
    {2677, WORLD_STATE_REMOVE}, // WORLD_STATE_NA_HALAA_NEUTRAL_H
    {2672, WORLD_STATE_REMOVE}, // WORLD_STATE_NA_HALAA_HORDE
    {2673, WORLD_STATE_REMOVE}  // WORLD_STATE_NA_HALAA_ALLIANCE
};

void Player::SendInitWorldStates(uint32 zoneid, uint32 areaid)
{
    // data depends on zoneid/mapid...
    BattleGround* bg = GetBattleGround();
    uint32 mapid = GetMapId();

    uint32 count = 0; // count of world states in packet

    WorldPacket data(SMSG_INIT_WORLD_STATES, (4 + 4 + 4 + 2 + 8 * 8)); // guess
    data << uint32(mapid);                                             // mapid
    data << uint32(zoneid); // zone id
    data << uint32(areaid); // area id, new 2.1.0
    size_t count_pos = data.wpos();
    data << uint16(0); // count of uint64 blocks, placeholder

    // common fields
    FillInitialWorldState(data, count, 0x8d8, 0x0); // 2264 1
    FillInitialWorldState(data, count, 0x8d7, 0x0); // 2263 2
    FillInitialWorldState(data, count, 0x8d6, 0x0); // 2262 3
    FillInitialWorldState(data, count, 0x8d5, 0x0); // 2261 4
    FillInitialWorldState(data, count, 0x8d4, 0x0); // 2260 5
    FillInitialWorldState(data, count, 0x8d3, 0x0); // 2259 6

    if (mapid == 530) // Outland
    {
        FillInitialWorldState(data, count, 0x9bf, 0x0); // 2495
        FillInitialWorldState(data, count, 0x9bd, 0xF); // 2493
        FillInitialWorldState(data, count, 0x9bb, 0xF); // 2491
    }
    switch (zoneid)
    {
    case 1:
    case 11:
    case 12:
    case 38:
    case 40:
    case 51:
    case 1519:
    case 1537:
    case 2257:
        break;
    case 139: // Eastern Plaguelands
        if (OutdoorPvP* outdoorPvP =
                sOutdoorPvPMgr::Instance()->GetScript(zoneid))
            outdoorPvP->FillInitialWorldStates(data, count);
        else
            FillInitialWorldState(data, count, EP_world_states);
        break;
    case 1377: // Silithus
        if (OutdoorPvP* outdoorPvP =
                sOutdoorPvPMgr::Instance()->GetScript(zoneid))
            outdoorPvP->FillInitialWorldStates(data, count);
        else
            FillInitialWorldState(data, count, SI_world_states);
        break;
    case 2597: // AV
        if (bg && bg->GetTypeID() == BATTLEGROUND_AV)
            bg->FillInitialWorldStates(data, count);
        else
            FillInitialWorldState(data, count, AV_world_states);
        break;
    case 3277: // WS
        if (bg && bg->GetTypeID() == BATTLEGROUND_WS)
            bg->FillInitialWorldStates(data, count);
        else
            FillInitialWorldState(data, count, WS_world_states);
        break;
    case 3358: // AB
        if (bg && bg->GetTypeID() == BATTLEGROUND_AB)
            bg->FillInitialWorldStates(data, count);
        else
            FillInitialWorldState(data, count, AB_world_states);
        break;
    case 3820: // EY
        if (bg && bg->GetTypeID() == BATTLEGROUND_EY)
            bg->FillInitialWorldStates(data, count);
        else
            FillInitialWorldState(data, count, EY_world_states);
        break;
    case 3483: // Hellfire Peninsula
        if (OutdoorPvP* outdoorPvP =
                sOutdoorPvPMgr::Instance()->GetScript(zoneid))
            outdoorPvP->FillInitialWorldStates(data, count);
        else
            FillInitialWorldState(data, count, HP_world_states);
        break;
    case 3518: // Nagrand
        if (OutdoorPvP* outdoorPvP =
                sOutdoorPvPMgr::Instance()->GetScript(zoneid))
            outdoorPvP->FillInitialWorldStates(data, count);
        else
            FillInitialWorldState(data, count, NA_world_states);
        break;
    case 3519: // Terokkar Forest
        if (OutdoorPvP* outdoorPvP =
                sOutdoorPvPMgr::Instance()->GetScript(zoneid))
            outdoorPvP->FillInitialWorldStates(data, count);
        else
            FillInitialWorldState(data, count, TF_world_states);
        break;
    case 3521: // Zangarmarsh
        if (OutdoorPvP* outdoorPvP =
                sOutdoorPvPMgr::Instance()->GetScript(zoneid))
            outdoorPvP->FillInitialWorldStates(data, count);
        else
            FillInitialWorldState(data, count, ZM_world_states);
    case 3698: // Nagrand Arena
        if (bg && bg->GetTypeID() == BATTLEGROUND_NA)
            bg->FillInitialWorldStates(data, count);
        else
        {
            FillInitialWorldState(data, count, 0xa0f, 0x0); // 2575 7
            FillInitialWorldState(data, count, 0xa10, 0x0); // 2576 8
            FillInitialWorldState(data, count, 0xa11, 0x0); // 2577 9 show
        }
        break;
    case 3702: // Blade's Edge Arena
        if (bg && bg->GetTypeID() == BATTLEGROUND_BE)
            bg->FillInitialWorldStates(data, count);
        else
        {
            FillInitialWorldState(data, count, 0x9f0, 0x0); // 2544 7 gold
            FillInitialWorldState(data, count, 0x9f1, 0x0); // 2545 8 green
            FillInitialWorldState(data, count, 0x9f3, 0x0); // 2547 9 show
        }
        break;
    case 3968: // Ruins of Lordaeron
        if (bg && bg->GetTypeID() == BATTLEGROUND_RL)
            bg->FillInitialWorldStates(data, count);
        else
        {
            FillInitialWorldState(data, count, 0xbb8, 0x0); // 3000 7 gold
            FillInitialWorldState(data, count, 0xbb9, 0x0); // 3001 8 green
            FillInitialWorldState(data, count, 0xbba, 0x0); // 3002 9 show
        }
        break;
    case 3703: // Shattrath City
        break;
    default:
        FillInitialWorldState(data, count, 0x914, 0x0); // 2324 7
        FillInitialWorldState(data, count, 0x913, 0x0); // 2323 8
        FillInitialWorldState(data, count, 0x912, 0x0); // 2322 9
        FillInitialWorldState(data, count, 0x915, 0x0); // 2325 10
        break;
    }

    data.put<uint16>(count_pos, count); // set actual world state amount

    GetSession()->send_packet(std::move(data));
}

uint32 Player::GetXPRestBonus(uint32 xp)
{
    uint32 rested_bonus = (uint32)GetRestBonus(); // xp for each rested bonus

    if (rested_bonus > xp) // max rested_bonus == xp or (r+x) = 200% xp
        rested_bonus = xp;

    SetRestBonus(GetRestBonus() - rested_bonus);

    LOG_DEBUG(logging,
        "Player gain %u xp (+ %u Rested Bonus). Rested points=%f",
        xp + rested_bonus, rested_bonus, GetRestBonus());
    return rested_bonus;
}

void Player::SetBindPoint(ObjectGuid guid)
{
    WorldPacket data(SMSG_BINDER_CONFIRM, 8);
    data << ObjectGuid(guid);
    GetSession()->send_packet(std::move(data));
}

void Player::SendTalentWipeConfirm(ObjectGuid guid)
{
    WorldPacket data(MSG_TALENT_WIPE_CONFIRM, (8 + 4));
    data << ObjectGuid(guid);
    data << uint32(resetTalentsCost());
    GetSession()->send_packet(std::move(data));
}

void Player::SendPetSkillWipeConfirm()
{
    Pet* pet = GetPet();
    if (!pet)
        return;
    WorldPacket data(SMSG_PET_UNLEARN_CONFIRM, (8 + 4));
    data << ObjectGuid(pet->GetObjectGuid());
    data << uint32(pet->resetTalentsCost());
    GetSession()->send_packet(std::move(data));
}

/*********************************************************/
/***                    STORAGE SYSTEM                 ***/
/*********************************************************/

void Player::SetVirtualItemSlot(uint8 i, Item* item)
{
    assert(i < 3);
    if (i < 2 && item)
    {
        if (!item->GetEnchantmentId(TEMP_ENCHANTMENT_SLOT))
            return;
        uint32 charges = item->GetEnchantmentCharges(TEMP_ENCHANTMENT_SLOT);
        if (charges == 0)
            return;
        if (charges > 1)
            item->SetEnchantmentCharges(TEMP_ENCHANTMENT_SLOT, charges - 1);
        else if (charges <= 1)
        {
            ApplyEnchantment(item, TEMP_ENCHANTMENT_SLOT, false, item->slot());
            item->ClearEnchantment(TEMP_ENCHANTMENT_SLOT);
        }
    }
}

void Player::SetSheath(SheathState sheathed)
{
    switch (sheathed)
    {
    case SHEATH_STATE_UNARMED: // no prepared weapon
        SetVirtualItemSlot(0, nullptr);
        SetVirtualItemSlot(1, nullptr);
        SetVirtualItemSlot(2, nullptr);
        break;
    case SHEATH_STATE_MELEE: // prepared melee weapon
    {
        SetVirtualItemSlot(0, GetWeaponForAttack(BASE_ATTACK, true, true));
        SetVirtualItemSlot(1, GetWeaponForAttack(OFF_ATTACK, true, true));
        SetVirtualItemSlot(2, nullptr);
    };
    break;
    case SHEATH_STATE_RANGED: // prepared ranged weapon
        SetVirtualItemSlot(0, nullptr);
        SetVirtualItemSlot(1, nullptr);
        SetVirtualItemSlot(2, GetWeaponForAttack(RANGED_ATTACK, true, true));
        break;
    default:
        SetVirtualItemSlot(0, nullptr);
        SetVirtualItemSlot(1, nullptr);
        SetVirtualItemSlot(2, nullptr);
        break;
    }
    Unit::SetSheath(
        sheathed); // this must visualize Sheath changing for other players...
}

InventoryResult Player::can_use_item(const ItemPrototype* prototype) const
{
    if ((prototype->AllowableClass & getClassMask()) == 0 ||
        (prototype->AllowableRace & getRaceMask()) == 0)
        return EQUIP_ERR_YOU_CAN_NEVER_USE_THAT_ITEM;

    // This GetSkill() returns a skill based on the type; such as plate or
    // two-handed swords
    if (uint32 item_skill = prototype->GetSkill())
        if (GetSkillValue(item_skill) == 0)
            return EQUIP_ERR_NO_REQUIRED_PROFICIENCY;

    // Check if we have the required SPELL_EFFECT_PROFICIENCY for gear and
    // weapons
    if (prototype->Class == ITEM_CLASS_WEAPON)
        if ((GetWeaponProficiency() & (1 << prototype->SubClass)) == 0)
            return EQUIP_ERR_NO_REQUIRED_PROFICIENCY;
    if (prototype->Class == ITEM_CLASS_ARMOR)
        if ((GetArmorProficiency() & (1 << prototype->SubClass)) == 0)
            return EQUIP_ERR_NO_REQUIRED_PROFICIENCY;

    // This RequirdSkill is a proffession skill, such as fishing, or tailoring
    if (prototype->RequiredSkill != 0)
    {
        if (GetSkillValue(prototype->RequiredSkill) == 0)
            return EQUIP_ERR_NO_REQUIRED_PROFICIENCY;
        else if (GetSkillValue(prototype->RequiredSkill) <
                 prototype->RequiredSkillRank)
            return EQUIP_ERR_CANT_EQUIP_SKILL;
    }

    if (prototype->RequiredSpell != 0 && !HasSpell(prototype->RequiredSpell))
        return EQUIP_ERR_NO_REQUIRED_PROFICIENCY;

    if (getLevel() < prototype->RequiredLevel)
        return EQUIP_ERR_CANT_EQUIP_LEVEL_I;

    if (prototype->RequiredReputationFaction &&
        static_cast<uint32>(
            GetReputationRank(prototype->RequiredReputationFaction)) <
            prototype->RequiredReputationRank)
        return EQUIP_ERR_CANT_EQUIP_REPUTATION;

    return EQUIP_ERR_OK;
}

InventoryResult Player::can_equip(Item* item) const
{
    // TODO: check this out, not sure wtf this error message even means or why
    // it's checked like this but I'm sure it stops some kind of exploit or
    // whatever
    if (item->HasTemporaryLoot())
        return EQUIP_ERR_ALREADY_LOOTED;

    if (item->GetOwnerGuid() !=
        GetObjectGuid()) // XXX: How the fuck could this ever be triggerable?
        return EQUIP_ERR_DONT_OWN_THAT_ITEM;

    const ItemPrototype* prototype = item->GetProto(); // XXX: Unnecessary check
    if (!prototype)
        return EQUIP_ERR_ITEM_NOT_FOUND;

    InventoryResult res = can_use_item(prototype);
    if (res != EQUIP_ERR_OK)
        return res;

    return EQUIP_ERR_OK;
}

InventoryResult Player::can_perform_equip(Item* item) const
{
    InventoryResult res = can_equip(item);
    if (res != EQUIP_ERR_OK)
        return res;

    if (!isAlive())
        return EQUIP_ERR_YOU_ARE_DEAD;

    if (GetShapeshiftForm() == FORM_SPIRITOFREDEMPTION)
        return EQUIP_ERR_CANT_DO_RIGHT_NOW;

    // Should you check for other states? Fleeing, confused, etc?
    if (hasUnitState(UNIT_STAT_STUNNED))
        return EQUIP_ERR_YOU_ARE_STUNNED;

    const ItemPrototype* prototype = item->GetProto(); // XXX: Unnecessary check
    if (!prototype)
        return EQUIP_ERR_ITEM_NOT_FOUND;

    if (!prototype->CanChangeEquipStateInCombat())
    {
        if (isInCombat())
            return EQUIP_ERR_NOT_IN_COMBAT;

        if (BattleGround* bg = GetBattleGround())
            if (bg->isArena() && bg->GetStatus() == STATUS_IN_PROGRESS)
                return EQUIP_ERR_NOT_DURING_ARENA_MATCH;
    }

    // mangos prevents equiping items while we process logout, not sure why
    // it's required but we honor that restriction
    if (GetSession()->isLogingOut())
        return EQUIP_ERR_YOU_ARE_STUNNED;

    if (IsNonMeleeSpellCasted(false))
        return EQUIP_ERR_CANT_DO_RIGHT_NOW;

    return EQUIP_ERR_OK;
}

InventoryResult Player::can_perform_unequip(Item* item) const
{
    if (!isAlive())
        return EQUIP_ERR_YOU_ARE_DEAD;

    if (GetShapeshiftForm() == FORM_SPIRITOFREDEMPTION)
        return EQUIP_ERR_CANT_DO_RIGHT_NOW;

    // Should you check for other states? Fleeing, confused, etc?
    if (hasUnitState(UNIT_STAT_STUNNED))
        return EQUIP_ERR_YOU_ARE_STUNNED;

    const ItemPrototype* prototype = item->GetProto();

    if (!prototype->CanChangeEquipStateInCombat())
    {
        if (isInCombat())
            return EQUIP_ERR_NOT_IN_COMBAT;

        if (BattleGround* bg = GetBattleGround())
            if (bg->isArena() && bg->GetStatus() == STATUS_IN_PROGRESS)
                return EQUIP_ERR_NOT_DURING_ARENA_MATCH;
    }

    // mangos prevents equiping items while we process logout, not sure why
    // it's required but we honor that restriction
    if (GetSession()->isLogingOut())
        return EQUIP_ERR_YOU_ARE_STUNNED;

    if (IsNonMeleeSpellCasted(false))
        return EQUIP_ERR_CANT_DO_RIGHT_NOW;

    return EQUIP_ERR_OK;
}

Item* Player::GetItemByGuid(ObjectGuid guid, bool include_bank) const
{
    if (!guid)
        return nullptr;

    // XXX
    const int mask = include_bank ?
                         inventory::personal_storage::iterator::all_personal :
                         inventory::personal_storage::iterator::all_body;
    for (inventory::personal_storage::iterator itr = storage().begin(mask);
         itr != storage().end(); ++itr)
        if ((*itr)->GetObjectGuid() == guid)
            return *itr;
    return nullptr;
}

uint32 Player::GetItemDisplayIdInSlot(uint8 bag, uint8 slot) const
{
    /*XXX*/
    const Item* item =
        storage().get(inventory::slot(inventory::personal_slot, bag, slot));

    if (!item)
        return 0;

    return item->GetProto()->DisplayInfoID;
}

Item* Player::GetWeaponForAttack(
    WeaponAttackType attackType, bool nonbroken, bool useable) const
{
    // XXX
    uint8 index;
    switch (attackType)
    {
    case BASE_ATTACK:
        index = inventory::main_hand_e;
        break;
    case OFF_ATTACK:
        index = inventory::off_hand_e;
        break;
    case RANGED_ATTACK:
        index = inventory::ranged_e;
        break;
    default:
        return nullptr;
    }

    Item* item = inventory_.get(
        inventory::slot(inventory::personal_slot, inventory::main_bag, index));
    if (!item || item->GetProto()->Class != ITEM_CLASS_WEAPON)
        return nullptr;

    if (useable && !CanUseEquippedWeapon(attackType))
        return nullptr;

    if (nonbroken && item->IsBroken())
        return nullptr;

    return item;
}

Item* Player::GetShield(bool useable) const
{
    // XXX
    Item* item = inventory_.get(inventory::slot(
        inventory::personal_slot, inventory::main_bag, inventory::off_hand_e));
    if (!item || item->GetProto()->Class != ITEM_CLASS_ARMOR)
        return nullptr;

    if ((useable && item->IsBroken()) || !CanUseEquippedWeapon(OFF_ATTACK))
        return nullptr;

    return item;
}

bool Player::HasItemCount(uint32 item, uint32 count, bool inBankAlso) const
{
    // XXX
    return storage().item_count(item, !inBankAlso) >= count;
}

bool Player::has_item_equipped(uint32 item_id) const
{
    const int mask = inventory::personal_storage::iterator::equipment;
    for (inventory::personal_storage::iterator itr = storage().begin(mask);
         itr != storage().end(); ++itr)
        if ((*itr)->GetEntry() == item_id)
            return true;
    return false;
}

bool Player::HasItemTotemCategory(uint32 TotemCategory) const
{
    /*XXX*/
    const int mask = inventory::personal_storage::iterator::all_body;
    for (inventory::personal_storage::iterator itr = storage().begin(mask);
         itr != storage().end(); ++itr)
        if (IsTotemCategoryCompatiableWith(
                (*itr)->GetProto()->TotemCategory, TotemCategory))
            return true;
    return false;
}

InventoryResult Player::CanUseAmmo(uint32 item) const
{
    LOG_DEBUG(logging, "STORAGE: CanUseAmmo item = %u", item);
    if (!isAlive())
        return EQUIP_ERR_YOU_ARE_DEAD;
    // if( isStunned() )
    //    return EQUIP_ERR_YOU_ARE_STUNNED;
    ItemPrototype const* pProto = ObjectMgr::GetItemPrototype(item);
    if (pProto)
    {
        if (pProto->InventoryType != INVTYPE_AMMO)
            return EQUIP_ERR_ONLY_AMMO_CAN_GO_HERE;

        InventoryResult msg = can_use_item(pProto);
        if (msg != EQUIP_ERR_OK)
            return msg;

        /*if ( GetReputationMgr().GetReputation() < pProto->RequiredReputation )
        return EQUIP_ERR_CANT_EQUIP_REPUTATION;
        */

        // Requires No Ammo
        if (has_aura(46699, SPELL_AURA_DUMMY))
            return EQUIP_ERR_BAG_FULL6;

        return EQUIP_ERR_OK;
    }
    return EQUIP_ERR_ITEM_NOT_FOUND;
}

void Player::SetAmmo(uint32 item)
{
    if (!item)
        return;

    // already set
    if (GetUInt32Value(PLAYER_AMMO_ID) == item)
        return;

    // check ammo
    if (item)
    {
        InventoryResult msg = CanUseAmmo(item);
        if (msg != EQUIP_ERR_OK)
        {
            SendEquipError(msg, nullptr, nullptr, item);
            return;
        }
    }

    SetUInt32Value(PLAYER_AMMO_ID, item);

    _ApplyAmmoBonuses();
}

void Player::RemoveAmmo()
{
    SetUInt32Value(PLAYER_AMMO_ID, 0);

    m_ammoDPS = 0.0f;

    if (CanModifyStats())
        UpdateDamagePhysical(RANGED_ATTACK);
}

void Player::cancel_trade()
{
    if (trade_)
        trade_->cancel();
}

void Player::SendEquipError(
    InventoryResult msg, Item* pItem, Item* pItem2, uint32 itemid /*= 0*/) const
{
    WorldPacket data(SMSG_INVENTORY_CHANGE_FAILURE,
        (msg == EQUIP_ERR_CANT_EQUIP_LEVEL_I ? 22 :
                                               (msg == EQUIP_ERR_OK ? 1 : 18)));
    data << uint8(msg);

    if (msg != EQUIP_ERR_OK)
    {
        data << (pItem ? pItem->GetObjectGuid() : ObjectGuid());
        data << (pItem2 ? pItem2->GetObjectGuid() : ObjectGuid());
        data << uint8(0); // bag type subclass, used with
                          // EQUIP_ERR_EVENT_AUTOEQUIP_BIND_CONFIRM and
                          // EQUIP_ERR_ITEM_DOESNT_GO_INTO_BAG2

        if (msg == EQUIP_ERR_CANT_EQUIP_LEVEL_I)
        {
            ItemPrototype const* proto =
                pItem ? pItem->GetProto() : ObjectMgr::GetItemPrototype(itemid);
            data << uint32(proto ? proto->RequiredLevel : 0);
        }
    }
    GetSession()->send_packet(std::move(data));
}

void Player::SendBuyError(
    BuyResult msg, Creature* pCreature, uint32 item, uint32 param)
{
    WorldPacket data(SMSG_BUY_FAILED, (8 + 4 + 4 + 1));
    data << (pCreature ? pCreature->GetObjectGuid() : ObjectGuid());
    data << uint32(item);
    if (param > 0)
        data << uint32(param);
    data << uint8(msg);
    GetSession()->send_packet(std::move(data));
}

void Player::SendSellError(
    SellResult msg, Creature* pCreature, ObjectGuid itemGuid, uint32 param)
{
    WorldPacket data(
        SMSG_SELL_ITEM, (8 + 8 + (param ? 4 : 0) + 1)); // last check 2.0.10
    data << (pCreature ? pCreature->GetObjectGuid() : ObjectGuid());
    data << ObjectGuid(itemGuid);
    if (param > 0)
        data << uint32(param);
    data << uint8(msg);
    GetSession()->send_packet(std::move(data));
}

void Player::UpdateItemDurations(time_t diff, bool offline_time)
{
    for (auto item : duration_items_)
    {
        if (offline_time &&
            !(item->GetProto()->ExtraFlags & ITEM_EXTRA_REAL_TIME_DURATION))
            continue;
        item->UpdateDuration(this, diff);
    }
}

void Player::UpdateEnchDurations(uint32 update_diff)
{
    for (auto item : enchdur_items_)
        item->UpdateEnchDurations(this, update_diff);
}

void Player::SendItemDurations()
{
    for (auto item : duration_items_)
        item->SendDuration(this);
}

void Player::SendEnchDurations()
{
    for (auto item : enchdur_items_)
        item->SendEnchDurations(this);
}

void Player::TrackItemDurations(Item* item, bool apply)
{
    if (apply && item->GetUInt32Value(ITEM_FIELD_DURATION) == 0)
        return;

    auto itr = std::find(duration_items_.begin(), duration_items_.end(), item);

    if (apply)
    {
        if (itr == duration_items_.end())
            duration_items_.push_back(item);

        item->SendDuration(this);
    }
    else
    {
        if (itr != duration_items_.end())
            duration_items_.erase(itr);
    }
}

void Player::TrackEnchDurations(Item* item, bool apply)
{
    if (apply && item->GetEnchantmentDuration(TEMP_ENCHANTMENT_SLOT) == 0)
        return;

    auto itr = std::find(enchdur_items_.begin(), enchdur_items_.end(), item);

    if (apply)
    {
        if (itr == enchdur_items_.end())
            enchdur_items_.push_back(item);

        item->SendEnchDurations(this);
    }
    else
    {
        if (itr != enchdur_items_.end())
            enchdur_items_.erase(itr);
    }
}

void Player::RemoveTempEnchantsOnArenaEntry()
{
    for (auto itr = enchdur_items_.begin(); itr != enchdur_items_.end();)
    {
        // Don't remove poisons
        if (auto enchant = sSpellItemEnchantmentStore.LookupEntry(
                (*itr)->GetEnchantmentId(TEMP_ENCHANTMENT_SLOT)))
        {
            if (enchant->aura_id == ITEM_ENCHANTMENT_AURAID_POISON)
            {
                ++itr;
                continue;
            }
        }

        if ((*itr)->IsEquipped())
            ApplyEnchantment(
                *itr, TEMP_ENCHANTMENT_SLOT, false, (*itr)->slot());

        (*itr)->ClearEnchantment(TEMP_ENCHANTMENT_SLOT);
        itr = enchdur_items_.erase(itr);
    }
}

void Player::ApplyEnchantment(
    Item* item, bool apply, inventory::slot item_slot, bool ignore_meta_gem)
{
    for (uint32 slot = 0; slot < MAX_ENCHANTMENT_SLOT; ++slot)
        ApplyEnchantment(
            item, EnchantmentSlot(slot), apply, item_slot, ignore_meta_gem);
}

void Player::ApplyEnchantment(Item* item, EnchantmentSlot slot, bool apply,
    inventory::slot item_slot, bool ignore_meta_gem)
{
    /*XXX*/
    if (!item)
        return;

    if (slot >= MAX_ENCHANTMENT_SLOT)
        return;

    uint32 enchant_id = item->GetEnchantmentId(slot);
    if (!enchant_id)
        return;

    SpellItemEnchantmentEntry const* pEnchant =
        sSpellItemEnchantmentStore.LookupEntry(enchant_id);
    if (!pEnchant)
        return;

    if (ignore_meta_gem && pEnchant->EnchantmentCondition)
        return;

    if (!item->IsBroken())
    {
        for (int s = 0; s < 3; ++s)
        {
            uint32 enchant_display_type = pEnchant->type[s];
            uint32 enchant_amount = pEnchant->amount[s];
            uint32 enchant_spell_id = pEnchant->spellid[s];

            switch (enchant_display_type)
            {
            case ITEM_ENCHANTMENT_TYPE_NONE:
                break;
            case ITEM_ENCHANTMENT_TYPE_COMBAT_SPELL:
                // processed in Player::CastItemCombatSpell
                break;
            case ITEM_ENCHANTMENT_TYPE_DAMAGE:
                if (item_slot.main_hand())
                    HandleStatModifier(UNIT_MOD_DAMAGE_MAINHAND, TOTAL_VALUE,
                        float(enchant_amount), apply);
                else if (item_slot.off_hand())
                    HandleStatModifier(UNIT_MOD_DAMAGE_OFFHAND, TOTAL_VALUE,
                        float(enchant_amount), apply);
                else if (item_slot.ranged())
                    HandleStatModifier(UNIT_MOD_DAMAGE_RANGED, TOTAL_VALUE,
                        float(enchant_amount), apply);
                break;
            case ITEM_ENCHANTMENT_TYPE_EQUIP_SPELL:
            {
                // Flametongue Weapon (Passive), Ranks (used not existed equip
                // spell id in pre-3.x spell.dbc)
                // See Player::CastItemCombatSpell for workaround implementation
                if (enchant_spell_id && apply)
                {
                    switch (enchant_spell_id)
                    {
                    case 10400: // Rank 1
                    case 15567: // Rank 2
                    case 15568: // Rank 3
                    case 15569: // Rank 4
                    case 16311: // Rank 5
                    case 16312: // Rank 6
                    case 16313: // Rank 7
                        enchant_spell_id = 0;
                        break;
                    default:
                        break;
                    }
                }

                if (enchant_spell_id)
                {
                    if (apply)
                    {
                        int32 basepoints = 0;
                        // Random Property Exist - try found basepoints for
                        // spell (basepoints depends from item suffix factor)
                        if (item->GetItemRandomPropertyId())
                        {
                            ItemRandomSuffixEntry const* item_rand =
                                sItemRandomSuffixStore.LookupEntry(
                                    abs(item->GetItemRandomPropertyId()));
                            if (item_rand)
                            {
                                // Search enchant_amount
                                for (int k = 0; k < 3; ++k)
                                {
                                    if (item_rand->enchant_id[k] == enchant_id)
                                    {
                                        basepoints = int32(
                                            (item_rand->prefix[k] *
                                                item->GetItemSuffixFactor()) /
                                            10000);
                                        break;
                                    }
                                }
                            }
                        }
                        // Cast custom spell vs all equal basepoints getted from
                        // enchant_amount
                        if (basepoints)
                            CastCustomSpell(this, enchant_spell_id, &basepoints,
                                &basepoints, &basepoints, true, item);
                        else
                            CastSpell(this, enchant_spell_id, true, item);
                    }
                    else
                        remove_auras(enchant_spell_id,
                            [item](AuraHolder* holder)
                            {
                                return holder->GetCastItemGuid() ==
                                       item->GetObjectGuid();
                            });
                }
                break;
            }
            case ITEM_ENCHANTMENT_TYPE_RESISTANCE:
                if (!enchant_amount)
                {
                    ItemRandomSuffixEntry const* item_rand =
                        sItemRandomSuffixStore.LookupEntry(
                            abs(item->GetItemRandomPropertyId()));
                    if (item_rand)
                    {
                        for (int k = 0; k < 3; ++k)
                        {
                            if (item_rand->enchant_id[k] == enchant_id)
                            {
                                enchant_amount =
                                    uint32((item_rand->prefix[k] *
                                               item->GetItemSuffixFactor()) /
                                           10000);
                                break;
                            }
                        }
                    }
                }

                HandleStatModifier(
                    UnitMods(UNIT_MOD_RESISTANCE_START + enchant_spell_id),
                    TOTAL_VALUE, float(enchant_amount), apply);
                break;
            case ITEM_ENCHANTMENT_TYPE_STAT:
            {
                if (!enchant_amount)
                {
                    ItemRandomSuffixEntry const* item_rand_suffix =
                        sItemRandomSuffixStore.LookupEntry(
                            abs(item->GetItemRandomPropertyId()));
                    if (item_rand_suffix)
                    {
                        for (int k = 0; k < 3; ++k)
                        {
                            if (item_rand_suffix->enchant_id[k] == enchant_id)
                            {
                                enchant_amount =
                                    uint32((item_rand_suffix->prefix[k] *
                                               item->GetItemSuffixFactor()) /
                                           10000);
                                break;
                            }
                        }
                    }
                }

                LOG_DEBUG(logging, "Adding %u to stat nb %u", enchant_amount,
                    enchant_spell_id);
                switch (enchant_spell_id)
                {
                case ITEM_MOD_MANA:
                    LOG_DEBUG(logging, "+ %u MANA", enchant_amount);
                    HandleStatModifier(UNIT_MOD_MANA, BASE_VALUE,
                        float(enchant_amount), apply);
                    break;
                case ITEM_MOD_HEALTH:
                    LOG_DEBUG(logging, "+ %u HEALTH", enchant_amount);
                    HandleStatModifier(UNIT_MOD_HEALTH, BASE_VALUE,
                        float(enchant_amount), apply);
                    break;
                case ITEM_MOD_AGILITY:
                    LOG_DEBUG(logging, "+ %u AGILITY", enchant_amount);
                    HandleStatModifier(UNIT_MOD_STAT_AGILITY, TOTAL_VALUE,
                        float(enchant_amount), apply);
                    ApplyStatBuffMod(
                        STAT_AGILITY, float(enchant_amount), apply);
                    break;
                case ITEM_MOD_STRENGTH:
                    LOG_DEBUG(logging, "+ %u STRENGTH", enchant_amount);
                    HandleStatModifier(UNIT_MOD_STAT_STRENGTH, TOTAL_VALUE,
                        float(enchant_amount), apply);
                    ApplyStatBuffMod(
                        STAT_STRENGTH, float(enchant_amount), apply);
                    break;
                case ITEM_MOD_INTELLECT:
                    LOG_DEBUG(logging, "+ %u INTELLECT", enchant_amount);
                    HandleStatModifier(UNIT_MOD_STAT_INTELLECT, TOTAL_VALUE,
                        float(enchant_amount), apply);
                    ApplyStatBuffMod(
                        STAT_INTELLECT, float(enchant_amount), apply);
                    break;
                case ITEM_MOD_SPIRIT:
                    LOG_DEBUG(logging, "+ %u SPIRIT", enchant_amount);
                    HandleStatModifier(UNIT_MOD_STAT_SPIRIT, TOTAL_VALUE,
                        float(enchant_amount), apply);
                    ApplyStatBuffMod(STAT_SPIRIT, float(enchant_amount), apply);
                    break;
                case ITEM_MOD_STAMINA:
                    LOG_DEBUG(logging, "+ %u STAMINA", enchant_amount);
                    HandleStatModifier(UNIT_MOD_STAT_STAMINA, TOTAL_VALUE,
                        float(enchant_amount), apply);
                    ApplyStatBuffMod(
                        STAT_STAMINA, float(enchant_amount), apply);
                    break;
                case ITEM_MOD_DEFENSE_SKILL_RATING:
                    ((Player*)this)
                        ->ApplyRatingMod(
                            CR_DEFENSE_SKILL, enchant_amount, apply);
                    LOG_DEBUG(logging, "+ %u DEFENCE", enchant_amount);
                    break;
                case ITEM_MOD_DODGE_RATING:
                    ((Player*)this)
                        ->ApplyRatingMod(CR_DODGE, enchant_amount, apply);
                    LOG_DEBUG(logging, "+ %u DODGE", enchant_amount);
                    break;
                case ITEM_MOD_PARRY_RATING:
                    ((Player*)this)
                        ->ApplyRatingMod(CR_PARRY, enchant_amount, apply);
                    LOG_DEBUG(logging, "+ %u PARRY", enchant_amount);
                    break;
                case ITEM_MOD_BLOCK_RATING:
                    ((Player*)this)
                        ->ApplyRatingMod(CR_BLOCK, enchant_amount, apply);
                    LOG_DEBUG(logging, "+ %u SHIELD_BLOCK", enchant_amount);
                    break;
                case ITEM_MOD_HIT_MELEE_RATING:
                    ((Player*)this)
                        ->ApplyRatingMod(CR_HIT_MELEE, enchant_amount, apply);
                    LOG_DEBUG(logging, "+ %u MELEE_HIT", enchant_amount);
                    break;
                case ITEM_MOD_HIT_RANGED_RATING:
                    ((Player*)this)
                        ->ApplyRatingMod(CR_HIT_RANGED, enchant_amount, apply);
                    LOG_DEBUG(logging, "+ %u RANGED_HIT", enchant_amount);
                    break;
                case ITEM_MOD_HIT_SPELL_RATING:
                    ((Player*)this)
                        ->ApplyRatingMod(CR_HIT_SPELL, enchant_amount, apply);
                    LOG_DEBUG(logging, "+ %u SPELL_HIT", enchant_amount);
                    break;
                case ITEM_MOD_CRIT_MELEE_RATING:
                    ((Player*)this)
                        ->ApplyRatingMod(CR_CRIT_MELEE, enchant_amount, apply);
                    LOG_DEBUG(logging, "+ %u MELEE_CRIT", enchant_amount);
                    break;
                case ITEM_MOD_CRIT_RANGED_RATING:
                    ((Player*)this)
                        ->ApplyRatingMod(CR_CRIT_RANGED, enchant_amount, apply);
                    LOG_DEBUG(logging, "+ %u RANGED_CRIT", enchant_amount);
                    break;
                case ITEM_MOD_CRIT_SPELL_RATING:
                    ((Player*)this)
                        ->ApplyRatingMod(CR_CRIT_SPELL, enchant_amount, apply);
                    LOG_DEBUG(logging, "+ %u SPELL_CRIT", enchant_amount);
                    break;
                //                        Values from ITEM_STAT_MELEE_HA_RATING
                //                        to ITEM_MOD_HASTE_RANGED_RATING are
                //                        never used
                //                        in Enchantments
                //                        case ITEM_MOD_HIT_TAKEN_MELEE_RATING:
                //                            ((Player*)this)->ApplyRatingMod(CR_HIT_TAKEN_MELEE,
                //                            enchant_amount, apply);
                //                            break;
                //                        case ITEM_MOD_HIT_TAKEN_RANGED_RATING:
                //                            ((Player*)this)->ApplyRatingMod(CR_HIT_TAKEN_RANGED,
                //                            enchant_amount, apply);
                //                            break;
                //                        case ITEM_MOD_HIT_TAKEN_SPELL_RATING:
                //                            ((Player*)this)->ApplyRatingMod(CR_HIT_TAKEN_SPELL,
                //                            enchant_amount, apply);
                //                            break;
                //                        case ITEM_MOD_CRIT_TAKEN_MELEE_RATING:
                //                            ((Player*)this)->ApplyRatingMod(CR_CRIT_TAKEN_MELEE,
                //                            enchant_amount, apply);
                //                            break;
                //                        case
                //                        ITEM_MOD_CRIT_TAKEN_RANGED_RATING:
                //                            ((Player*)this)->ApplyRatingMod(CR_CRIT_TAKEN_RANGED,
                //                            enchant_amount, apply);
                //                            break;
                //                        case ITEM_MOD_CRIT_TAKEN_SPELL_RATING:
                //                            ((Player*)this)->ApplyRatingMod(CR_CRIT_TAKEN_SPELL,
                //                            enchant_amount, apply);
                //                            break;
                //                        case ITEM_MOD_HASTE_MELEE_RATING:
                //                            ((Player*)this)->ApplyRatingMod(CR_HASTE_MELEE,
                //                            enchant_amount, apply);
                //                            break;
                //                        case ITEM_MOD_HASTE_RANGED_RATING:
                //                            ((Player*)this)->ApplyRatingMod(CR_HASTE_RANGED,
                //                            enchant_amount, apply);
                //                            break;
                case ITEM_MOD_HASTE_SPELL_RATING:
                    ((Player*)this)
                        ->ApplyRatingMod(CR_HASTE_SPELL, enchant_amount, apply);
                    break;
                case ITEM_MOD_HIT_RATING:
                    ((Player*)this)
                        ->ApplyRatingMod(CR_HIT_MELEE, enchant_amount, apply);
                    ((Player*)this)
                        ->ApplyRatingMod(CR_HIT_RANGED, enchant_amount, apply);
                    LOG_DEBUG(logging, "+ %u HIT", enchant_amount);
                    break;
                case ITEM_MOD_CRIT_RATING:
                    ((Player*)this)
                        ->ApplyRatingMod(CR_CRIT_MELEE, enchant_amount, apply);
                    ((Player*)this)
                        ->ApplyRatingMod(CR_CRIT_RANGED, enchant_amount, apply);
                    LOG_DEBUG(logging, "+ %u CRITICAL", enchant_amount);
                    break;
                //                        Values ITEM_MOD_HIT_TAKEN_RATING and
                //                        ITEM_MOD_CRIT_TAKEN_RATING are never
                //                        used in Enchantment
                //                        case ITEM_MOD_HIT_TAKEN_RATING:
                //                            ((Player*)this)->ApplyRatingMod(CR_HIT_TAKEN_MELEE,
                //                            enchant_amount, apply);
                //                            ((Player*)this)->ApplyRatingMod(CR_HIT_TAKEN_RANGED,
                //                            enchant_amount, apply);
                //                            ((Player*)this)->ApplyRatingMod(CR_HIT_TAKEN_SPELL,
                //                            enchant_amount, apply);
                //                            break;
                //                        case ITEM_MOD_CRIT_TAKEN_RATING:
                //                            ((Player*)this)->ApplyRatingMod(CR_CRIT_TAKEN_MELEE,
                //                            enchant_amount, apply);
                //                            ((Player*)this)->ApplyRatingMod(CR_CRIT_TAKEN_RANGED,
                //                            enchant_amount, apply);
                //                            ((Player*)this)->ApplyRatingMod(CR_CRIT_TAKEN_SPELL,
                //                            enchant_amount, apply);
                //                            break;
                case ITEM_MOD_RESILIENCE_RATING:
                    ((Player*)this)
                        ->ApplyRatingMod(
                            CR_CRIT_TAKEN_MELEE, enchant_amount, apply);
                    ((Player*)this)
                        ->ApplyRatingMod(
                            CR_CRIT_TAKEN_RANGED, enchant_amount, apply);
                    ((Player*)this)
                        ->ApplyRatingMod(
                            CR_CRIT_TAKEN_SPELL, enchant_amount, apply);
                    LOG_DEBUG(logging, "+ %u RESILIENCE", enchant_amount);
                    break;
                case ITEM_MOD_HASTE_RATING:
                    ((Player*)this)
                        ->ApplyRatingMod(CR_HASTE_MELEE, enchant_amount, apply);
                    ((Player*)this)
                        ->ApplyRatingMod(
                            CR_HASTE_RANGED, enchant_amount, apply);
                    LOG_DEBUG(logging, "+ %u HASTE", enchant_amount);
                    break;
                case ITEM_MOD_EXPERTISE_RATING:
                    ((Player*)this)
                        ->ApplyRatingMod(CR_EXPERTISE, enchant_amount, apply);
                    LOG_DEBUG(logging, "+ %u EXPERTISE", enchant_amount);
                    break;
                default:
                    break;
                }
                break;
            }
            case ITEM_ENCHANTMENT_TYPE_TOTEM: // Shaman Rockbiter Weapon
            {
                if (getClass() == CLASS_SHAMAN)
                {
                    float addValue = 0.0f;
                    if (item_slot.main_hand())
                    {
                        addValue = float(
                            enchant_amount * item->GetProto()->Delay / 1000.0f);
                        HandleStatModifier(UNIT_MOD_DAMAGE_MAINHAND,
                            TOTAL_VALUE, addValue, apply);
                    }
                    else if (item_slot.off_hand())
                    {
                        addValue = float(
                            enchant_amount * item->GetProto()->Delay / 1000.0f);
                        HandleStatModifier(UNIT_MOD_DAMAGE_OFFHAND, TOTAL_VALUE,
                            addValue, apply);
                    }
                }
                break;
            }
            default:
                logging.error(
                    "Unknown item enchantment (id = %d) display type: %d",
                    enchant_id, enchant_display_type);
                break;
            } /*switch(enchant_display_type)*/
        }     /*for*/
    }

    /* Visualize enchantments */
    // NOTE: Gems are visualized in Item::visualize_gems (since meta-gems are
    // not always applied)
    // BONUS_ENCHANTMENT_SLOT == bonus for having all gems of correct color
    if ((slot <= TEMP_ENCHANTMENT_SLOT || slot == BONUS_ENCHANTMENT_SLOT) &&
        item_slot.equipment())
        SetUInt32Value(item_field_offset(item_slot.index()) +
                           PLAYER_VISIBLE_ITEM_ENCHANTS + slot,
            (apply ? item->GetEnchantmentId(slot) : 0));
}

// Remove any positive, self-cast, aura that is no longer valid due to our
// equipment having changed
void Player::UpdateEquipmentRequiringAuras()
{
    remove_auras_if([this](AuraHolder* holder)
        {
            const SpellEntry* proto = holder->GetSpellProto();
            if (holder->GetCasterGuid() != GetObjectGuid() ||
                !IsPositiveSpell(proto) ||
                HasItemFitToSpellReqirements(proto) || IsPassiveSpell(proto))
                return false;
            return true;
        });
}

void Player::RemoveAurasCastedBy(Item* item)
{
    remove_auras_if([this, item](AuraHolder* holder)
        {
            return item->GetObjectGuid() == holder->GetCastItemGuid();
        });
}

/*********************************************************/
/***                    GOSSIP SYSTEM                  ***/
/*********************************************************/

void Player::PrepareGossipMenu(WorldObject* pSource, uint32 menuId)
{
    PlayerMenu* pMenu = PlayerTalkClass;
    pMenu->ClearMenus();

    pMenu->GetGossipMenu().SetMenuId(menuId);

    GossipMenuItemsMapBounds pMenuItemBounds =
        sObjectMgr::Instance()->GetGossipMenuItemsMapBounds(menuId);

    // prepares quest menu when true
    bool canSeeQuests = menuId == GetDefaultGossipMenuForSource(pSource);

    // if canSeeQuests (the default, top level menu) and no menu options exist
    // for this, use options from default options
    if (pMenuItemBounds.first == pMenuItemBounds.second && canSeeQuests)
        pMenuItemBounds =
            sObjectMgr::Instance()->GetGossipMenuItemsMapBounds(0);

    bool canTalkToCredit = pSource->GetTypeId() == TYPEID_UNIT;

    for (auto itr = pMenuItemBounds.first; itr != pMenuItemBounds.second; ++itr)
    {
        bool hasMenuItem = true;

        if (!isGameMaster()) // Let GM always see menu items regardless of
                             // conditions
        {
            if (itr->second.conditionId &&
                !sObjectMgr::Instance()->IsPlayerMeetToNEWCondition(
                    this, itr->second.conditionId))
            {
                if (itr->second.option_id == GOSSIP_OPTION_QUESTGIVER)
                    canSeeQuests = false;
                continue;
            }
            else if (!itr->second.conditionId)
            {
                if (itr->second.cond_1 &&
                    !sObjectMgr::Instance()->IsPlayerMeetToCondition(
                        this, itr->second.cond_1))
                {
                    if (itr->second.option_id == GOSSIP_OPTION_QUESTGIVER)
                        canSeeQuests = false;
                    continue;
                }

                if (itr->second.cond_2 &&
                    !sObjectMgr::Instance()->IsPlayerMeetToCondition(
                        this, itr->second.cond_2))
                {
                    if (itr->second.option_id == GOSSIP_OPTION_QUESTGIVER)
                        canSeeQuests = false;
                    continue;
                }

                if (itr->second.cond_3 &&
                    !sObjectMgr::Instance()->IsPlayerMeetToCondition(
                        this, itr->second.cond_3))
                {
                    if (itr->second.option_id == GOSSIP_OPTION_QUESTGIVER)
                        canSeeQuests = false;
                    continue;
                }
            }

            auto conds = sConditionMgr::Instance()->GetGossipOptionConditions(
                itr->second.menu_id, itr->second.id);
            if (conds &&
                !sConditionMgr::Instance()->IsObjectMeetToConditions(
                    this, conds))
            {
                if (itr->second.option_id == GOSSIP_OPTION_QUESTGIVER)
                    canSeeQuests = false;
                continue;
            }
        }

        if (pSource->GetTypeId() == TYPEID_UNIT)
        {
            Creature* pCreature = (Creature*)pSource;

            uint32 npcflags = pCreature->GetUInt32Value(UNIT_NPC_FLAGS);

            if (!(itr->second.npc_option_npcflag & npcflags))
                continue;

            switch (itr->second.option_id)
            {
            case GOSSIP_OPTION_GOSSIP:
                if (itr->second.action_menu_id != 0) // has sub menu (or close
                                                     // gossip), so do not
                                                     // "talk" with this NPC yet
                    canTalkToCredit = false;
                break;
            case GOSSIP_OPTION_QUESTGIVER:
                hasMenuItem = false;
                break;
            case GOSSIP_OPTION_ARMORER:
                hasMenuItem = false; // added in special mode
                break;
            case GOSSIP_OPTION_SPIRITHEALER:
                if (!isDead())
                    hasMenuItem = false;
                break;
            case GOSSIP_OPTION_VENDOR:
            {
                VendorItemData const* vItems = pCreature->GetVendorItems();
                VendorItemData const* tItems =
                    pCreature->GetVendorTemplateItems();
                if ((!vItems || vItems->Empty()) &&
                    (!tItems || tItems->Empty()))
                {
                    logging.error(
                        "Creature %u (Entry: %u) have UNIT_NPC_FLAG_VENDOR but "
                        "have empty trading item list.",
                        pCreature->GetGUIDLow(), pCreature->GetEntry());
                    hasMenuItem = false;
                }
                break;
            }
            case GOSSIP_OPTION_TRAINER:
                if (!pCreature->IsTrainerOf(this, false))
                    hasMenuItem = false;
                break;
            case GOSSIP_OPTION_UNLEARNTALENTS:
                if (!pCreature->CanTrainAndResetTalentsOf(this))
                    hasMenuItem = false;
                break;
            case GOSSIP_OPTION_UNLEARNPETSKILLS:
                if (!GetPet() || GetPet()->getPetType() != HUNTER_PET ||
                    GetPet()->m_spells.size() <= 1 ||
                    pCreature->GetCreatureInfo()->trainer_type !=
                        TRAINER_TYPE_PETS ||
                    pCreature->GetCreatureInfo()->trainer_class != CLASS_HUNTER)
                    hasMenuItem = false;
                break;
            case GOSSIP_OPTION_TAXIVENDOR:
                if (GetSession()->SendLearnNewTaxiNode(pCreature))
                    return;
                break;
            case GOSSIP_OPTION_BATTLEFIELD:
                if (!pCreature->CanInteractWithBattleMaster(this, false))
                    hasMenuItem = false;
                break;
            case GOSSIP_OPTION_STABLEPET:
                if (getClass() != CLASS_HUNTER)
                    hasMenuItem = false;
                break;
            case GOSSIP_OPTION_SPIRITGUIDE:
            case GOSSIP_OPTION_INNKEEPER:
            case GOSSIP_OPTION_BANKER:
            case GOSSIP_OPTION_PETITIONER:
            case GOSSIP_OPTION_TABARDDESIGNER:
            case GOSSIP_OPTION_AUCTIONEER:
                break; // no checks
            default:
                logging.error(
                    "Creature entry %u have unknown gossip option %u for menu "
                    "%u",
                    pCreature->GetEntry(), itr->second.option_id,
                    itr->second.menu_id);
                hasMenuItem = false;
                break;
            }
        }
        else if (pSource->GetTypeId() == TYPEID_GAMEOBJECT)
        {
            GameObject* pGo = (GameObject*)pSource;

            switch (itr->second.option_id)
            {
            case GOSSIP_OPTION_QUESTGIVER:
                hasMenuItem = false;
                break;
            case GOSSIP_OPTION_GOSSIP:
                if (pGo->GetGoType() != GAMEOBJECT_TYPE_QUESTGIVER &&
                    pGo->GetGoType() != GAMEOBJECT_TYPE_GOOBER)
                    hasMenuItem = false;
                break;
            default:
                hasMenuItem = false;
                break;
            }
        }

        if (hasMenuItem)
        {
            std::string strOptionText = itr->second.option_text;
            std::string strBoxText = itr->second.box_text;

            int loc_idx = GetSession()->GetSessionDbLocaleIndex();

            if (loc_idx >= 0)
            {
                uint32 idxEntry = MAKE_PAIR32(menuId, itr->second.id);

                if (GossipMenuItemsLocale const* no =
                        sObjectMgr::Instance()->GetGossipMenuItemsLocale(
                            idxEntry))
                {
                    if (no->OptionText.size() > (size_t)loc_idx &&
                        !no->OptionText[loc_idx].empty())
                        strOptionText = no->OptionText[loc_idx];

                    if (no->BoxText.size() > (size_t)loc_idx &&
                        !no->BoxText[loc_idx].empty())
                        strBoxText = no->BoxText[loc_idx];
                }
            }

            pMenu->GetGossipMenu().AddMenuItem(itr->second.option_icon,
                strOptionText, 0, itr->second.option_id, itr->second.id,
                strBoxText, itr->second.box_money, itr->second.box_coded);
            pMenu->GetGossipMenu().AddGossipMenuItemData(
                itr->second.action_menu_id, itr->second.action_poi_id,
                itr->second.action_script_id);
        }
    }

    if (canSeeQuests)
        PrepareQuestMenu(pSource->GetObjectGuid());

    if (canTalkToCredit)
    {
        if (pSource->HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP) &&
            !(((Creature*)pSource)->GetCreatureInfo()->flags_extra &
                CREATURE_FLAG_EXTRA_NO_TALKTO_CREDIT))
            TalkedToCreature(pSource->GetEntry(), pSource->GetObjectGuid());
    }

    // some gossips aren't handled in normal way ... so we need to do it this
    // way .. TODO: handle it in normal way ;-)
    /*if (pMenu->Empty())
    {
        if (pCreature->HasFlag(UNIT_NPC_FLAGS,UNIT_NPC_FLAG_TRAINER))
        {
            // output error message if need
            pCreature->IsTrainerOf(this, true);
        }

        if (pCreature->HasFlag(UNIT_NPC_FLAGS,UNIT_NPC_FLAG_BATTLEMASTER))
        {
            // output error message if need
            pCreature->CanInteractWithBattleMaster(this, true);
        }
    }*/
}

void Player::SendPreparedGossip(WorldObject* pSource)
{
    if (!pSource)
        return;

    if (pSource->GetTypeId() == TYPEID_UNIT)
    {
        // in case no gossip flag and quest menu not empty, open quest menu
        // (client expect gossip menu with this flag)
        if (!((Creature*)pSource)
                 ->HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP) &&
            !PlayerTalkClass->GetQuestMenu().Empty())
        {
            SendPreparedQuest(pSource->GetObjectGuid());
            return;
        }
    }
    else if (pSource->GetTypeId() == TYPEID_GAMEOBJECT)
    {
        // probably need to find a better way here
        if (!PlayerTalkClass->GetGossipMenu().GetMenuId() &&
            !PlayerTalkClass->GetQuestMenu().Empty())
        {
            SendPreparedQuest(pSource->GetObjectGuid());
            return;
        }
    }

    // in case non empty gossip menu (that not included quests list size) show
    // it
    // (quest entries from quest menu will be included in list)

    uint32 textId = GetGossipTextId(pSource);

    if (uint32 menuId = PlayerTalkClass->GetGossipMenu().GetMenuId())
        textId = GetGossipTextId(menuId, pSource);

    PlayerTalkClass->SendGossipMenu(textId, pSource->GetObjectGuid());
}

void Player::OnGossipSelect(
    WorldObject* pSource, uint32 gossipListId, uint32 menuId)
{
    GossipMenu& gossipmenu = PlayerTalkClass->GetGossipMenu();

    if (gossipListId >= gossipmenu.MenuItemCount())
        return;

    // if not same, then something funky is going on
    if (menuId != gossipmenu.GetMenuId())
        return;

    GossipMenuItem const& menu_item = gossipmenu.GetItem(gossipListId);

    uint32 gossipOptionId = menu_item.m_gOptionId;
    ObjectGuid guid = pSource->GetObjectGuid();
    uint32 moneyTake = menu_item.m_gBoxMoney;

    // XXX
    if (moneyTake > 0)
    {
        inventory::transaction trans;
        trans.remove(moneyTake);
        if (!storage().finalize(trans))
            return;
    }

    if (pSource->GetTypeId() == TYPEID_GAMEOBJECT)
    {
        if (gossipOptionId > GOSSIP_OPTION_QUESTGIVER)
        {
            logging.error(
                "Player guid %u request invalid gossip option for GameObject "
                "entry %u",
                GetGUIDLow(), pSource->GetEntry());
            return;
        }
    }

    GossipMenuItemData pMenuData = gossipmenu.GetItemData(gossipListId);

    switch (gossipOptionId)
    {
    case GOSSIP_OPTION_GOSSIP:
    {
        if (pMenuData.m_gAction_poi)
            PlayerTalkClass->SendPointOfInterest(pMenuData.m_gAction_poi);

        // send new menu || close gossip || stay at current menu
        if (pMenuData.m_gAction_menu > 0)
        {
            PrepareGossipMenu(pSource, uint32(pMenuData.m_gAction_menu));
            SendPreparedGossip(pSource);
        }
        else if (pMenuData.m_gAction_menu < 0)
        {
            PlayerTalkClass->CloseGossip();
            TalkedToCreature(pSource->GetEntry(), pSource->GetObjectGuid());
        }

        break;
    }
    case GOSSIP_OPTION_SPIRITHEALER:
        if (isDead())
            ((Creature*)pSource)
                ->CastSpell(((Creature*)pSource), 17251, true, nullptr, nullptr,
                    GetObjectGuid());
        break;
    case GOSSIP_OPTION_QUESTGIVER:
        PrepareQuestMenu(guid);
        SendPreparedQuest(guid);
        break;
    case GOSSIP_OPTION_VENDOR:
    case GOSSIP_OPTION_ARMORER:
        GetSession()->SendListInventory(guid);
        break;
    case GOSSIP_OPTION_STABLEPET:
        GetSession()->SendStablePet(guid);
        break;
    case GOSSIP_OPTION_TRAINER:
        GetSession()->SendTrainerList(guid);
        break;
    case GOSSIP_OPTION_UNLEARNTALENTS:
        PlayerTalkClass->CloseGossip();
        SendTalentWipeConfirm(guid);
        break;
    case GOSSIP_OPTION_UNLEARNPETSKILLS:
        PlayerTalkClass->CloseGossip();
        SendPetSkillWipeConfirm();
        break;
    case GOSSIP_OPTION_TAXIVENDOR:
        GetSession()->SendTaxiMenu(((Creature*)pSource));
        break;
    case GOSSIP_OPTION_INNKEEPER:
        PlayerTalkClass->CloseGossip();
        SetBindPoint(guid);
        break;
    case GOSSIP_OPTION_BANKER:
        GetSession()->SendShowBank(guid);
        break;
    case GOSSIP_OPTION_PETITIONER:
        PlayerTalkClass->CloseGossip();
        GetSession()->SendPetitionShowList(guid);
        break;
    case GOSSIP_OPTION_TABARDDESIGNER:
        PlayerTalkClass->CloseGossip();
        GetSession()->SendTabardVendorActivate(guid);
        break;
    case GOSSIP_OPTION_AUCTIONEER:
        GetSession()->SendAuctionHello(((Creature*)pSource));
        break;
    case GOSSIP_OPTION_SPIRITGUIDE:
        PrepareGossipMenu(pSource);
        SendPreparedGossip(pSource);
        break;
    case GOSSIP_OPTION_BATTLEFIELD:
    {
        BattleGroundTypeId bgTypeId =
            sBattleGroundMgr::Instance()->GetBattleMasterBG(
                pSource->GetEntry());

        if (bgTypeId == BATTLEGROUND_TYPE_NONE)
        {
            logging.error(
                "a user (guid %u) requested battlegroundlist from a npc who is "
                "no battlemaster",
                GetGUIDLow());
            return;
        }

        GetSession()->SendBattlegGroundList(guid, bgTypeId);
        break;
    }
    }

    if (pMenuData.m_gAction_script)
    {
        if (pSource->GetTypeId() == TYPEID_UNIT)
            GetMap()->ScriptsStart(
                sGossipScripts, pMenuData.m_gAction_script, pSource, this);
        else if (pSource->GetTypeId() == TYPEID_GAMEOBJECT)
            GetMap()->ScriptsStart(
                sGossipScripts, pMenuData.m_gAction_script, this, pSource);
    }
}

uint32 Player::GetGossipTextId(WorldObject* pSource)
{
    if (!pSource || pSource->GetTypeId() != TYPEID_UNIT)
        return DEFAULT_GOSSIP_MESSAGE;

    if (uint32 pos = sObjectMgr::Instance()->GetNpcGossip(
            ((Creature*)pSource)->GetGUIDLow()))
        return pos;

    return DEFAULT_GOSSIP_MESSAGE;
}

uint32 Player::GetGossipTextId(uint32 menuId, WorldObject* pSource)
{
    uint32 textId = DEFAULT_GOSSIP_MESSAGE;

    if (!menuId)
        return textId;

    GossipMenusMapBounds pMenuBounds =
        sObjectMgr::Instance()->GetGossipMenusMapBounds(menuId);

    std::vector<GossipMenusMap::const_iterator> possible_texts;
    bool has_order = false;

    for (auto itr = pMenuBounds.first; itr != pMenuBounds.second; ++itr)
    {
        auto conds = sConditionMgr::Instance()->GetGossipMenuConditions(
            menuId, itr->second.text_id);
        if (conds &&
            !sConditionMgr::Instance()->IsObjectMeetToConditions(this, conds))
            continue;

        if (itr->second.conditionId &&
            sObjectMgr::Instance()->IsPlayerMeetToNEWCondition(
                this, itr->second.conditionId))
        {
            possible_texts.push_back(itr);
            if (itr->second.ordering)
                has_order = true;
        }
        else if (!itr->second.conditionId)
        {
            if (sObjectMgr::Instance()->IsPlayerMeetToCondition(
                    this, itr->second.cond_1) &&
                sObjectMgr::Instance()->IsPlayerMeetToCondition(
                    this, itr->second.cond_2))
            {
                possible_texts.push_back(itr);
                if (itr->second.ordering)
                    has_order = true;
            }
        }
    }

    if (!possible_texts.empty())
    {
        if (has_order)
        {
            // find smallest available order
            uint32 order = 0;
            for (auto itr : possible_texts)
                if (itr->second.ordering &&
                    (itr->second.ordering < order || order == 0))
                    order = itr->second.ordering;
            // drop any text above order
            auto drop_fn = [order](GossipMenusMap::const_iterator itr)
            {
                return itr->second.ordering > order;
            };
            possible_texts.erase(std::remove_if(possible_texts.begin(),
                                     possible_texts.end(), drop_fn),
                possible_texts.end());
        }

        auto itr = possible_texts[possible_texts.size() == 1 ?
                                      0 :
                                      urand(0, possible_texts.size() - 1)];
        textId = itr->second.text_id;
        if (itr->second.script_id)
            GetMap()->ScriptsStart(
                sGossipScripts, itr->second.script_id, this, pSource);
    }

    return textId;
}

uint32 Player::GetDefaultGossipMenuForSource(WorldObject* pSource)
{
    if (pSource->GetTypeId() == TYPEID_UNIT)
        return ((Creature*)pSource)->GetCreatureInfo()->GossipMenuId;
    else if (pSource->GetTypeId() == TYPEID_GAMEOBJECT)
        return ((GameObject*)pSource)->GetGOInfo()->GetGossipMenuId();

    return 0;
}

/*********************************************************/
/***                    QUEST SYSTEM                   ***/
/*********************************************************/

void Player::PrepareQuestMenu(ObjectGuid guid)
{
    QuestRelationsMapBounds rbounds;
    QuestRelationsMapBounds irbounds;

    WorldObject* target = nullptr;

    // pets also can have quests
    if (Creature* pCreature = GetMap()->GetAnyTypeCreature(guid))
    {
        rbounds = sObjectMgr::Instance()->GetCreatureQuestRelationsMapBounds(
            pCreature->GetEntry());
        irbounds =
            sObjectMgr::Instance()->GetCreatureQuestInvolvedRelationsMapBounds(
                pCreature->GetEntry());
        target = pCreature;
    }
    else
    {
        // we should obtain map pointer from GetMap() in 99% of cases. Special
        // case
        // only for quests which cast teleport spells on player
        Map* _map = IsInWorld() ? GetMap() : sMapMgr::Instance()->FindMap(
                                                 GetMapId(), GetInstanceId());
        assert(_map);

        if (GameObject* pGameObject = _map->GetGameObject(guid))
        {
            rbounds = sObjectMgr::Instance()->GetGOQuestRelationsMapBounds(
                pGameObject->GetEntry());
            irbounds =
                sObjectMgr::Instance()->GetGOQuestInvolvedRelationsMapBounds(
                    pGameObject->GetEntry());
            target = pGameObject;
        }
        else
            return;
    }

    QuestMenu& qm = PlayerTalkClass->GetQuestMenu();
    qm.ClearMenu();

    for (auto itr = irbounds.first; itr != irbounds.second; ++itr)
    {
        uint32 quest_id = itr->second;

        Quest const* pQuest =
            sObjectMgr::Instance()->GetQuestTemplate(quest_id);

        if (!pQuest || !pQuest->IsActive())
            continue;

        QuestStatus status = GetQuestStatus(quest_id);

        if (status == QUEST_STATUS_COMPLETE && !GetQuestRewardStatus(quest_id))
            qm.AddMenuItem(quest_id, DIALOG_STATUS_REWARD_REP);
        else if (status == QUEST_STATUS_INCOMPLETE)
            qm.AddMenuItem(quest_id, DIALOG_STATUS_INCOMPLETE);
        else if (status == QUEST_STATUS_AVAILABLE)
            qm.AddMenuItem(quest_id, DIALOG_STATUS_CHAT);
    }

    for (auto itr = rbounds.first; itr != rbounds.second; ++itr)
    {
        uint32 quest_id = itr->second;

        Quest const* pQuest =
            sObjectMgr::Instance()->GetQuestTemplate(quest_id);

        if (!pQuest || !pQuest->IsActive())
            continue;

        QuestStatus status = GetQuestStatus(quest_id);

        if (pQuest->IsAutoComplete() && CanTakeQuest(pQuest, false, target))
            qm.AddMenuItem(quest_id, DIALOG_STATUS_REWARD_REP);
        else if (status == QUEST_STATUS_NONE &&
                 CanTakeQuest(pQuest, false, target))
            qm.AddMenuItem(quest_id, DIALOG_STATUS_AVAILABLE);
    }
}

void Player::SendPreparedQuest(ObjectGuid guid)
{
    QuestMenu& questMenu = PlayerTalkClass->GetQuestMenu();

    if (questMenu.Empty())
        return;

    QuestMenuItem const& qmi0 = questMenu.GetItem(0);

    uint32 status = qmi0.m_qIcon;

    // single element case
    if (questMenu.MenuItemCount() == 1)
    {
        // Auto open -- maybe also should verify there is no greeting
        uint32 quest_id = qmi0.m_qId;
        Quest const* pQuest =
            sObjectMgr::Instance()->GetQuestTemplate(quest_id);

        if (pQuest)
        {
            if (status == DIALOG_STATUS_REWARD_REP &&
                !GetQuestRewardStatus(quest_id))
                PlayerTalkClass->SendQuestGiverRequestItems(
                    pQuest, guid, CanRewardQuest(pQuest, false), true);
            else if (status == DIALOG_STATUS_INCOMPLETE)
                PlayerTalkClass->SendQuestGiverRequestItems(
                    pQuest, guid, false, true);
            // Send completable on repeatable and autoCompletable quest if
            // player don't have quest
            // TODO: verify if check for !pQuest->IsDaily() is really correct
            // (possibly not)
            else if (pQuest->IsAutoComplete() && pQuest->IsRepeatable() &&
                     !pQuest->IsDaily())
                PlayerTalkClass->SendQuestGiverRequestItems(
                    pQuest, guid, CanCompleteRepeatableQuest(pQuest), true);
            else
                PlayerTalkClass->SendQuestGiverQuestDetails(pQuest, guid, true);
        }
    }
    // multiply entries
    else
    {
        QEmote qe;
        qe._Delay = 0;
        qe._Emote = 0;
        std::string title = "";

        // need pet case for some quests
        if (Creature* pCreature = GetMap()->GetAnyTypeCreature(guid))
        {
            uint32 textid = GetGossipTextId(pCreature);

            GossipText const* gossiptext =
                sObjectMgr::Instance()->GetGossipText(textid);
            if (!gossiptext)
            {
                qe._Delay =
                    0; // TEXTEMOTE_MESSAGE;              //zyg: player emote
                qe._Emote =
                    0; // TEXTEMOTE_HELLO;                //zyg: NPC emote
                title = "";
            }
            else
            {
                qe = gossiptext->Options[0].Emotes[0];

                int loc_idx = GetSession()->GetSessionDbLocaleIndex();

                std::string title0 = gossiptext->Options[0].Text_0;
                std::string title1 = gossiptext->Options[0].Text_1;
                sObjectMgr::Instance()->GetNpcTextLocaleStrings0(
                    textid, loc_idx, &title0, &title1);

                title = !title0.empty() ? title0 : title1;
            }
        }
        PlayerTalkClass->SendQuestGiverQuestList(qe, title, guid);
    }
}

bool Player::IsActiveQuest(uint32 quest_id) const
{
    auto itr = mQuestStatus.find(quest_id);

    return itr != mQuestStatus.end() &&
           itr->second.m_status != QUEST_STATUS_NONE;
}

bool Player::IsCurrentQuest(uint32 quest_id, uint8 completed_or_not) const
{
    auto itr = mQuestStatus.find(quest_id);
    if (itr == mQuestStatus.end())
        return false;

    switch (completed_or_not)
    {
    case 1:
        return itr->second.m_status == QUEST_STATUS_INCOMPLETE;
    case 2:
        return itr->second.m_status == QUEST_STATUS_COMPLETE &&
               !itr->second.m_rewarded;
    default:
        return itr->second.m_status == QUEST_STATUS_INCOMPLETE ||
               (itr->second.m_status == QUEST_STATUS_COMPLETE &&
                   !itr->second.m_rewarded);
    }
}

Quest const* Player::GetNextQuest(ObjectGuid guid, Quest const* pQuest)
{
    QuestRelationsMapBounds rbounds;

    if (Creature* pCreature = GetMap()->GetAnyTypeCreature(guid))
    {
        rbounds = sObjectMgr::Instance()->GetCreatureQuestRelationsMapBounds(
            pCreature->GetEntry());
    }
    else
    {
        // we should obtain map pointer from GetMap() in 99% of cases. Special
        // case
        // only for quests which cast teleport spells on player
        Map* _map = IsInWorld() ? GetMap() : sMapMgr::Instance()->FindMap(
                                                 GetMapId(), GetInstanceId());
        assert(_map);

        if (GameObject* pGameObject = _map->GetGameObject(guid))
        {
            rbounds = sObjectMgr::Instance()->GetGOQuestRelationsMapBounds(
                pGameObject->GetEntry());
        }
        else
            return nullptr;
    }

    uint32 nextQuestID = pQuest->GetNextQuestInChain();
    for (auto itr = rbounds.first; itr != rbounds.second; ++itr)
    {
        if (itr->second == nextQuestID)
            return sObjectMgr::Instance()->GetQuestTemplate(nextQuestID);
    }

    return nullptr;
}

bool Player::CanSeeStartQuest(Quest const* pQuest, WorldObject* source) const
{
    if (SatisfyQuestClass(pQuest, false) && SatisfyQuestRace(pQuest, false) &&
        SatisfyQuestSkill(pQuest, false) &&
        SatisfyQuestExclusiveGroup(pQuest, false) &&
        SatisfyQuestReputation(pQuest, false) &&
        SatisfyQuestPreviousQuest(pQuest, false) &&
        SatisfyQuestNextChain(pQuest, false) &&
        SatisfyQuestPrevChain(pQuest, false) &&
        SatisfyQuestDay(pQuest, false) && pQuest->IsActive())
    {
        // check conditions
        auto conds = sConditionMgr::Instance()->GetQuestAvailableConditions(
            pQuest->GetQuestId());
        if (conds)
        {
            // FIXME: const_cast should not be needed
            ConditionSourceInfo sources(const_cast<Player*>(this), source);
            if (!sConditionMgr::Instance()->IsObjectMeetToConditions(
                    sources, conds))
                return false;
        }

        return int32(getLevel()) +
                   sWorld::Instance()->getConfig(
                       CONFIG_INT32_QUEST_HIGH_LEVEL_HIDE_DIFF) >=
               int32(pQuest->GetMinLevel());
    }

    return false;
}

bool Player::CanTakeQuest(
    Quest const* pQuest, bool msg, WorldObject* source) const
{
    if (!SatisfyQuestStatus(pQuest, msg) ||
        !SatisfyQuestExclusiveGroup(pQuest, msg) ||
        !SatisfyQuestClass(pQuest, msg) || !SatisfyQuestRace(pQuest, msg) ||
        !SatisfyQuestLevel(pQuest, msg) || !SatisfyQuestSkill(pQuest, msg) ||
        !SatisfyQuestReputation(pQuest, msg) ||
        !SatisfyQuestPreviousQuest(pQuest, msg) ||
        !SatisfyQuestTimed(pQuest, msg) ||
        !SatisfyQuestNextChain(pQuest, msg) ||
        !SatisfyQuestPrevChain(pQuest, msg) || !SatisfyQuestDay(pQuest, msg) ||
        !pQuest->IsActive())
        return false;

    // check conditions
    auto conds = sConditionMgr::Instance()->GetQuestAvailableConditions(
        pQuest->GetQuestId());
    if (conds)
    {
        // FIXME: const_cast should not be needed
        ConditionSourceInfo sources(const_cast<Player*>(this), source);
        if (!sConditionMgr::Instance()->IsObjectMeetToConditions(
                sources, conds))
            return false;
    }

    return true;
}

bool Player::CanAddQuest(Quest const* pQuest, bool msg) const
{
    if (!SatisfyQuestLog(msg))
        return false;

    if (uint32 srcitem = pQuest->GetSrcItemId())
    {
        // Subtract any items we already have
        int count = static_cast<int>(pQuest->GetSrcItemCount()) -
                    static_cast<int>(storage().item_count(srcitem));
        if (count > 0)
        {
            inventory::transaction trans;
            trans.add(srcitem, count);
            if (!storage().verify(trans)) // We only verify, we don't actually
                                          // finalize the transaction
            {
                if (msg)
                    SendEquipError(
                        static_cast<InventoryResult>(trans.error()), nullptr);
                return false;
            }
        }
    }

    return true;
}

bool Player::CanCompleteQuest(uint32 quest_id) const
{
    if (!quest_id)
        return false;

    auto q_itr = mQuestStatus.find(quest_id);

    // some quests can be auto taken and auto completed in one step
    QuestStatus status = q_itr != mQuestStatus.end() ? q_itr->second.m_status :
                                                       QUEST_STATUS_NONE;

    if (status == QUEST_STATUS_COMPLETE)
        return false; // not allow re-complete quest

    Quest const* qInfo = sObjectMgr::Instance()->GetQuestTemplate(quest_id);

    if (!qInfo)
        return false;

    // only used for "flag" quests and not real in-game quests
    if (qInfo->HasQuestFlag(QUEST_FLAGS_AUTO_REWARDED))
    {
        // a few checks, not all "satisfy" is needed
        if (SatisfyQuestPreviousQuest(qInfo, false) &&
            SatisfyQuestLevel(qInfo, false) &&
            SatisfyQuestSkill(qInfo, false) && SatisfyQuestRace(qInfo, false) &&
            SatisfyQuestClass(qInfo, false))
            return true;

        return false;
    }

    // auto complete quest
    if (qInfo->IsAutoComplete() && CanTakeQuest(qInfo, false, nullptr))
        return true;

    if (status != QUEST_STATUS_INCOMPLETE)
        return false;

    // incomplete quest have status data
    QuestStatusData const& q_status = q_itr->second;

    if (qInfo->HasSpecialFlag(QUEST_SPECIAL_FLAG_DELIVER))
    {
        for (int i = 0; i < QUEST_OBJECTIVES_COUNT; ++i)
        {
            if (qInfo->ReqItemCount[i] != 0 &&
                q_status.m_itemcount[i] < qInfo->ReqItemCount[i])
                return false;
        }
    }

    if (qInfo->HasSpecialFlag(QuestSpecialFlags(
            QUEST_SPECIAL_FLAG_KILL_OR_CAST | QUEST_SPECIAL_FLAG_SPEAKTO)))
    {
        for (int i = 0; i < QUEST_OBJECTIVES_COUNT; ++i)
        {
            if (qInfo->ReqCreatureOrGOId[i] == 0)
                continue;

            if (qInfo->ReqCreatureOrGOCount[i] != 0 &&
                q_status.m_creatureOrGOcount[i] <
                    qInfo->ReqCreatureOrGOCount[i])
                return false;
        }
    }

    if (qInfo->HasSpecialFlag(QUEST_SPECIAL_FLAG_EXPLORATION_OR_EVENT) &&
        !q_status.m_explored)
        return false;

    if (qInfo->HasSpecialFlag(QUEST_SPECIAL_FLAG_TIMED) &&
        q_status.m_timer == 0)
        return false;

    if (qInfo->GetRewOrReqMoney() < 0)
    {
        if (storage().money().get() < uint32(-qInfo->GetRewOrReqMoney()))
            return false;
    }

    uint32 repFacId = qInfo->GetRepObjectiveFaction();
    if (repFacId &&
        GetReputationMgr().GetReputation(repFacId) <
            qInfo->GetRepObjectiveValue())
        return false;

    return true;
}

bool Player::CanCompleteRepeatableQuest(Quest const* pQuest) const
{
    // Solve problem that player don't have the quest and try complete it.
    // if repeatable she must be able to complete event if player don't have it.
    // Seem that all repeatable quest are DELIVER Flag so, no need to add more.
    if (!CanTakeQuest(pQuest, false, nullptr))
        return false;

    if (pQuest->HasSpecialFlag(QUEST_SPECIAL_FLAG_DELIVER))
        for (int i = 0; i < QUEST_ITEM_OBJECTIVES_COUNT; ++i)
            if (pQuest->ReqItemId[i] && pQuest->ReqItemCount[i] &&
                !HasItemCount(pQuest->ReqItemId[i], pQuest->ReqItemCount[i]))
                return false;

    if (!CanRewardQuest(pQuest, false))
        return false;

    return true;
}

bool Player::CanRewardQuest(Quest const* pQuest, bool msg) const
{
    // not auto complete quest and not completed quest (only cheating case, then
    // ignore without message)
    if (!pQuest->IsAutoComplete() &&
        GetQuestStatus(pQuest->GetQuestId()) != QUEST_STATUS_COMPLETE)
        return false;

    // daily quest can't be rewarded (25 daily quest already completed)
    if (!SatisfyQuestDay(pQuest, true))
        return false;

    // rewarded and not repeatable quest (only cheating case, then ignore
    // without message)
    if (GetQuestRewardStatus(pQuest->GetQuestId()))
        return false;

    // prevent receive reward with quest items in bank
    if (pQuest->HasSpecialFlag(QUEST_SPECIAL_FLAG_DELIVER))
    {
        for (int i = 0; i < QUEST_ITEM_OBJECTIVES_COUNT; ++i)
        {
            if (pQuest->ReqItemCount[i] != 0 &&
                storage().item_count(pQuest->ReqItemId[i], true) <
                    pQuest->ReqItemCount[i])
            {
                if (msg)
                    SendEquipError(EQUIP_ERR_ITEM_NOT_FOUND, nullptr, nullptr,
                        pQuest->ReqItemId[i]);

                return false;
            }
        }
    }

    // prevent receive reward with low money and GetRewOrReqMoney() < 0
    if (pQuest->GetRewOrReqMoney() < 0 &&
        storage().money().get() < uint32(-pQuest->GetRewOrReqMoney()))
        return false;

    return true;
}

void Player::SendPetTameFailure(PetTameFailureReason reason)
{
    WorldPacket data(SMSG_PET_TAME_FAILURE, 1);
    data << uint8(reason);
    GetSession()->send_packet(std::move(data));
}

void Player::AddQuest(Quest const* pQuest, Object* questGiver)
{
    uint16 log_slot = FindQuestSlot(0);
    assert(log_slot < MAX_QUEST_LOG_SIZE);

    uint32 quest_id = pQuest->GetQuestId();

    // if not exist then created with set uState==NEW and rewarded=false
    QuestStatusData& questStatusData = mQuestStatus[quest_id];

    // check for repeatable quests status reset
    questStatusData.m_status = QUEST_STATUS_INCOMPLETE;
    questStatusData.m_explored = false;

    if (pQuest->HasSpecialFlag(QUEST_SPECIAL_FLAG_DELIVER))
    {
        for (int i = 0; i < QUEST_OBJECTIVES_COUNT; ++i)
            questStatusData.m_itemcount[i] = 0;
    }

    if (pQuest->HasSpecialFlag(QuestSpecialFlags(
            QUEST_SPECIAL_FLAG_KILL_OR_CAST | QUEST_SPECIAL_FLAG_SPEAKTO)))
    {
        for (int i = 0; i < QUEST_OBJECTIVES_COUNT; ++i)
            questStatusData.m_creatureOrGOcount[i] = 0;
    }

    if (pQuest->GetRepObjectiveFaction())
        if (FactionEntry const* factionEntry =
                sFactionStore.LookupEntry(pQuest->GetRepObjectiveFaction()))
            GetReputationMgr().SetVisible(factionEntry);

    uint32 qtime = 0;
    if (pQuest->HasSpecialFlag(QUEST_SPECIAL_FLAG_TIMED))
    {
        uint32 limittime = pQuest->GetLimitTime();

        // shared timed quest
        if (questGiver && questGiver->GetTypeId() == TYPEID_PLAYER)
            limittime =
                ((Player*)questGiver)->getQuestStatusMap()[quest_id].m_timer /
                IN_MILLISECONDS;

        AddTimedQuest(quest_id);
        questStatusData.m_timer = limittime * IN_MILLISECONDS;
        qtime = static_cast<uint32>(WorldTimer::time_no_syscall()) + limittime;
    }
    else
        questStatusData.m_timer = 0;

    SetQuestSlot(log_slot, quest_id, qtime);

    if (questStatusData.uState != QUEST_NEW)
        questStatusData.uState = QUEST_CHANGED;

    // quest accept scripts
    if (questGiver)
    {
        switch (questGiver->GetTypeId())
        {
        case TYPEID_UNIT:
            sScriptMgr::Instance()->OnQuestAccept(
                this, (Creature*)questGiver, pQuest);
            break;
        case TYPEID_ITEM:
        case TYPEID_CONTAINER:
            sScriptMgr::Instance()->OnQuestAccept(
                this, (Item*)questGiver, pQuest);
            break;
        case TYPEID_GAMEOBJECT:
            sScriptMgr::Instance()->OnQuestAccept(
                this, (GameObject*)questGiver, pQuest);
            break;
        }

        // starting initial DB quest script
        if (pQuest->GetQuestStartScript() != 0)
            GetMap()->ScriptsStart(sQuestStartScripts,
                pQuest->GetQuestStartScript(), questGiver, this);
    }

    // remove start item if not need
    if (questGiver && questGiver->isType(TYPEMASK_ITEM))
    {
        // destroy not required for quest finish quest starting item
        bool notRequiredItem = true;
        for (int i = 0; i < QUEST_ITEM_OBJECTIVES_COUNT; ++i)
        {
            if (pQuest->ReqItemId[i] == questGiver->GetEntry())
            {
                notRequiredItem = false;
                break;
            }
        }

        if (pQuest->GetSrcItemId() == questGiver->GetEntry())
            notRequiredItem = false;

        if (notRequiredItem)
        {
            // XXX
            inventory::transaction trans(false);
            trans.destroy(static_cast<Item*>(questGiver));
            storage().finalize(trans);
        }
    }

    GiveQuestSourceItemIfNeed(pQuest);

    AdjustQuestReqItemCount(pQuest, questStatusData);

    // Some spells applied at quest activation
    SpellAreaForQuestMapBounds saBounds =
        sSpellMgr::Instance()->GetSpellAreaForQuestMapBounds(quest_id, true);
    if (saBounds.first != saBounds.second)
    {
        uint32 zone, area;
        GetZoneAndAreaId(zone, area);

        for (auto itr = saBounds.first; itr != saBounds.second; ++itr)
            if (itr->second->autocast &&
                itr->second->IsFitToRequirements(this, zone, area))
                if (!has_aura(itr->second->spellId))
                    CastSpell(this, itr->second->spellId, true);
    }

    UpdateForQuestWorldObjects();
}

void Player::CompleteQuest(uint32 quest_id)
{
    if (quest_id)
    {
        SetQuestStatus(quest_id, QUEST_STATUS_COMPLETE);

        uint16 log_slot = FindQuestSlot(quest_id);
        if (log_slot < MAX_QUEST_LOG_SIZE)
            SetQuestSlotState(log_slot, QUEST_STATE_COMPLETE);

        if (Quest const* qInfo =
                sObjectMgr::Instance()->GetQuestTemplate(quest_id))
        {
            if (qInfo->HasQuestFlag(QUEST_FLAGS_AUTO_REWARDED))
                RewardQuest(qInfo, 0, this, false);
        }
    }
}

void Player::IncompleteQuest(uint32 quest_id)
{
    if (quest_id)
    {
        SetQuestStatus(quest_id, QUEST_STATUS_INCOMPLETE);

        uint16 log_slot = FindQuestSlot(quest_id);
        if (log_slot < MAX_QUEST_LOG_SIZE)
            RemoveQuestSlotState(log_slot, QUEST_STATE_COMPLETE);
    }
}

bool Player::RewardQuest(
    Quest const* pQuest, uint32 reward, Object* questGiver, bool announce)
{
    if (!CanRewardQuest(pQuest, false))
        return false;

    uint32 quest_id = pQuest->GetQuestId();
    bool max_level = getLevel() >= sWorld::Instance()->getConfig(
                                       CONFIG_UINT32_MAX_PLAYER_LEVEL);
    inventory::transaction trans; // One big transaction, to allow rewards
                                  // taking the place of now gone quest items

    /* We must process all inventory changes before we do anything else,
       as they might cause our completion of the quest to fail. */

    for (int i = 0; i < QUEST_ITEM_OBJECTIVES_COUNT; ++i)
        if (pQuest->ReqItemId[i])
            trans.destroy(pQuest->ReqItemId[i], pQuest->ReqItemCount[i]);

    for (int i = 0; i < QUEST_SOURCE_ITEM_IDS_COUNT; ++i)
    {
        if (pQuest->ReqSourceId[i])
        {
            ItemPrototype const* iProto =
                ObjectMgr::GetItemPrototype(pQuest->ReqSourceId[i]);
            if (iProto && iProto->Bonding == BIND_QUEST_ITEM)
            {
                uint32 count = storage().item_count(pQuest->ReqSourceId[i]);
                if (count > 0)
                {
                    if (count > pQuest->ReqSourceCount[i])
                        count = pQuest->ReqSourceCount[i];
                    trans.destroy(pQuest->ReqSourceId[i], count);
                }
            }
        }
    }

    if (pQuest->GetRewChoiceItemsCount() > 0)
    {
        if (uint32 itemId = pQuest->RewChoiceItemId[reward])
            trans.add(itemId, pQuest->RewChoiceItemCount[reward]);
    }

    if (pQuest->GetRewItemsCount() > 0)
    {
        for (uint32 i = 0; i < pQuest->GetRewItemsCount(); ++i)
            if (uint32 itemId = pQuest->RewItemId[i])
                trans.add(itemId, pQuest->RewItemCount[i]);
    }

    int32 total_gold = 0;
    if (max_level)
        total_gold +=
            int32(pQuest->GetRewMoneyMaxLevel() *
                  sWorld::Instance()->getConfig(CONFIG_FLOAT_RATE_DROP_MONEY));

    total_gold += pQuest->GetRewOrReqMoney();

    // Reward money if total > 0, and required money if total < 0, and do
    // nothing if total == 0
    if (total_gold > 0)
        trans.add(total_gold);
    else if (total_gold < 0)
        trans.remove(-total_gold);

    // Attempt to finalize the transaction
    if (!storage().finalize(trans))
    {
        SendEquipError(static_cast<InventoryResult>(trans.error()), nullptr);
        return false;
    }

    // Used for client inform but rewarded only in case not max level
    uint32 xp =
        uint32(pQuest->XPValue(this) *
               sWorld::Instance()->getConfig(CONFIG_FLOAT_RATE_XP_QUEST));

    if (!max_level)
        GiveXP(xp, nullptr);

    RemoveTimedQuest(quest_id);

    if (BattleGround* bg = GetBattleGround())
        if (bg->GetTypeID() == BATTLEGROUND_AV)
            ((BattleGroundAV*)bg)
                ->HandleQuestComplete(pQuest->GetQuestId(), this);

    RewardReputation(pQuest);

    uint16 log_slot = FindQuestSlot(quest_id);
    if (log_slot < MAX_QUEST_LOG_SIZE)
        SetQuestSlot(log_slot, 0);

    QuestStatusData& q_status = mQuestStatus[quest_id];

    // honor reward
    if (pQuest->GetRewHonorableKills())
        RewardHonor(nullptr, 0, MaNGOS::Honor::hk_honor_at_level(getLevel(),
                                    pQuest->GetRewHonorableKills()));

    // title reward
    if (pQuest->GetCharTitleId())
    {
        if (CharTitlesEntry const* titleEntry =
                sCharTitlesStore.LookupEntry(pQuest->GetCharTitleId()))
            SetTitle(titleEntry);
    }

    // Send reward mail
    if (uint32 mail_template_id = pQuest->GetRewMailTemplateId())
        MailDraft(mail_template_id)
            .SendMailTo(this, questGiver, MAIL_CHECK_MASK_HAS_BODY,
                pQuest->GetRewMailDelaySecs());

    if (pQuest->IsDaily())
        SetDailyQuestStatus(quest_id);

    if (!pQuest->IsRepeatable())
        SetQuestStatus(quest_id, QUEST_STATUS_COMPLETE);
    else
        SetQuestStatus(quest_id, QUEST_STATUS_NONE);

    q_status.m_rewarded = true;
    if (q_status.uState != QUEST_NEW)
        q_status.uState = QUEST_CHANGED;

    if (announce)
        SendQuestReward(pQuest, xp, questGiver);

    bool handled = false;

    switch (questGiver->GetTypeId())
    {
    case TYPEID_UNIT:
        handled = sScriptMgr::Instance()->OnQuestRewarded(
            this, (Creature*)questGiver, pQuest);
        break;
    case TYPEID_GAMEOBJECT:
        handled = sScriptMgr::Instance()->OnQuestRewarded(
            this, (GameObject*)questGiver, pQuest);
        break;
    }

    if (!handled && pQuest->GetQuestCompleteScript() != 0)
        GetMap()->ScriptsStart(sQuestEndScripts,
            pQuest->GetQuestCompleteScript(), questGiver, this);

    // cast spells after mark quest complete (some spells have quest completed
    // state reqyurements in spell_area data)
    if (pQuest->GetRewSpellCast() > 0)
        CastSpell(this, pQuest->GetRewSpellCast(), true);
    else if (pQuest->GetRewSpell() > 0)
        CastSpell(this, pQuest->GetRewSpell(), true);

    uint32 zone = 0;
    uint32 area = 0;

    // remove auras from spells with quest reward state limitations
    SpellAreaForQuestMapBounds saEndBounds =
        sSpellMgr::Instance()->GetSpellAreaForQuestEndMapBounds(quest_id);
    if (saEndBounds.first != saEndBounds.second)
    {
        GetZoneAndAreaId(zone, area);

        for (auto itr = saEndBounds.first; itr != saEndBounds.second; ++itr)
            if (!itr->second->IsFitToRequirements(this, zone, area))
                remove_auras(itr->second->spellId);
    }

    // Some spells applied at quest reward
    SpellAreaForQuestMapBounds saBounds =
        sSpellMgr::Instance()->GetSpellAreaForQuestMapBounds(quest_id, false);
    if (saBounds.first != saBounds.second)
    {
        if (!zone || !area)
            GetZoneAndAreaId(zone, area);

        for (auto itr = saBounds.first; itr != saBounds.second; ++itr)
            if (itr->second->autocast &&
                itr->second->IsFitToRequirements(this, zone, area))
                if (!has_aura(itr->second->spellId))
                    CastSpell(this, itr->second->spellId, true);
    }

    // For some reason the client stopped querying for quest status updates
    // between Vanilla and TBC (given the same packet flow, minus the status
    // query packet which became status query multiple) when turning in a quest
    // you do not have in your quest log. Should probably investigate why, but
    // this solves the immediate issue.
    if (log_slot == MAX_QUEST_LOG_SIZE)
        GetSession()->SendQuestgiverStatusMultiple();

    return true;
}

void Player::FailQuest(uint32 questId)
{
    if (Quest const* pQuest = sObjectMgr::Instance()->GetQuestTemplate(questId))
    {
        SetQuestStatus(questId, QUEST_STATUS_FAILED);

        uint16 log_slot = FindQuestSlot(questId);

        if (log_slot < MAX_QUEST_LOG_SIZE)
        {
            SetQuestSlotTimer(log_slot, 1);
            SetQuestSlotState(log_slot, QUEST_STATE_FAIL);
        }

        if (pQuest->HasSpecialFlag(QUEST_SPECIAL_FLAG_TIMED))
        {
            QuestStatusData& q_status = mQuestStatus[questId];

            RemoveTimedQuest(questId);
            q_status.m_timer = 0;

            SendQuestTimerFailed(questId);
        }
        else
            SendQuestFailed(questId);
    }
}

void Player::FailGroupQuest(uint32 questId)
{
    FailQuest(questId);

    if (IsInGroup())
    {
        // Grep for nearby players instead of looping through the group to avoid
        // locking
        maps::visitors::simple<Player>{}(this,
            sWorld::Instance()->getConfig(CONFIG_FLOAT_GROUP_XP_DISTANCE),
            [this, questId](Player* elem)
            {
                if (elem != this && (elem)->IsInSameGroupWith(this) &&
                    elem->HasQuest(questId))
                    elem->FailQuest(questId);
            });
    }
}

bool Player::SatisfyQuestSkill(Quest const* qInfo, bool msg) const
{
    uint32 skill = qInfo->GetRequiredSkill();

    // skip 0 case RequiredSkill
    if (skill == 0)
        return true;

    // check skill value
    if (GetSkillValue(skill) < qInfo->GetRequiredSkillValue())
    {
        if (msg)
            SendCanTakeQuestResponse(INVALIDREASON_DONT_HAVE_REQ);

        return false;
    }

    return true;
}

bool Player::SatisfyQuestLevel(Quest const* qInfo, bool msg) const
{
    if (getLevel() < qInfo->GetMinLevel())
    {
        if (msg)
            SendCanTakeQuestResponse(INVALIDREASON_DONT_HAVE_REQ);

        return false;
    }

    return true;
}

bool Player::SatisfyQuestLog(bool msg) const
{
    // exist free slot
    if (FindQuestSlot(0) < MAX_QUEST_LOG_SIZE)
        return true;

    if (msg)
    {
        WorldPacket data(SMSG_QUESTLOG_FULL, 0);
        GetSession()->send_packet(std::move(data));
    }
    return false;
}

bool Player::SatisfyQuestPreviousQuest(Quest const* qInfo, bool msg) const
{
    // No previous quest (might be first quest in a series)
    if (qInfo->prevQuests.empty())
        return true;

    for (const auto& elem : qInfo->prevQuests)
    {
        uint32 prevId = abs(elem);

        auto i_prevstatus = mQuestStatus.find(prevId);
        Quest const* qPrevInfo =
            sObjectMgr::Instance()->GetQuestTemplate(prevId);

        if (qPrevInfo && i_prevstatus != mQuestStatus.end())
        {
            // If any of the positive previous quests completed, return true
            if (elem > 0 && i_prevstatus->second.m_rewarded)
            {
                // skip one-from-all exclusive group
                if (qPrevInfo->GetExclusiveGroup() >= 0)
                    return true;

                // each-from-all exclusive group ( < 0)
                // can be start if only all quests in prev quest exclusive group
                // completed and rewarded
                ExclusiveQuestGroupsMapBounds bounds =
                    sObjectMgr::Instance()->GetExclusiveQuestGroupsMapBounds(
                        qPrevInfo->GetExclusiveGroup());

                assert(
                    bounds.first != bounds.second); // always must be found if
                // qPrevInfo->ExclusiveGroup != 0

                for (auto iter2 = bounds.first; iter2 != bounds.second; ++iter2)
                {
                    uint32 exclude_Id = iter2->second;

                    // skip checked quest id, only state of other quests in
                    // group is interesting
                    if (exclude_Id == prevId)
                        continue;

                    auto i_exstatus = mQuestStatus.find(exclude_Id);

                    // alternative quest from group also must be completed and
                    // rewarded(reported)
                    if (i_exstatus == mQuestStatus.end() ||
                        !i_exstatus->second.m_rewarded)
                    {
                        if (msg)
                            SendCanTakeQuestResponse(
                                INVALIDREASON_DONT_HAVE_REQ);

                        return false;
                    }
                }
                return true;
            }
            // If any of the negative previous quests active, return true
            if (elem < 0 && IsCurrentQuest(prevId))
            {
                // skip one-from-all exclusive group
                if (qPrevInfo->GetExclusiveGroup() >= 0)
                    return true;

                // each-from-all exclusive group ( < 0)
                // can be start if only all quests in prev quest exclusive group
                // active
                ExclusiveQuestGroupsMapBounds bounds =
                    sObjectMgr::Instance()->GetExclusiveQuestGroupsMapBounds(
                        qPrevInfo->GetExclusiveGroup());

                assert(
                    bounds.first != bounds.second); // always must be found if
                // qPrevInfo->ExclusiveGroup != 0

                for (auto iter2 = bounds.first; iter2 != bounds.second; ++iter2)
                {
                    uint32 exclude_Id = iter2->second;

                    // skip checked quest id, only state of other quests in
                    // group is interesting
                    if (exclude_Id == prevId)
                        continue;

                    // alternative quest from group also must be active
                    if (!IsCurrentQuest(exclude_Id))
                    {
                        if (msg)
                            SendCanTakeQuestResponse(
                                INVALIDREASON_DONT_HAVE_REQ);

                        return false;
                    }
                }
                return true;
            }
        }
    }

    // Has only positive prev. quests in non-rewarded state
    // and negative prev. quests in non-active state
    if (msg)
        SendCanTakeQuestResponse(INVALIDREASON_DONT_HAVE_REQ);

    return false;
}

bool Player::SatisfyQuestClass(Quest const* qInfo, bool msg) const
{
    uint32 reqClass = qInfo->GetRequiredClasses();

    if (reqClass == 0)
        return true;

    if ((reqClass & getClassMask()) == 0)
    {
        if (msg)
            SendCanTakeQuestResponse(INVALIDREASON_DONT_HAVE_REQ);

        return false;
    }

    return true;
}

bool Player::SatisfyQuestRace(Quest const* qInfo, bool msg) const
{
    uint32 reqraces = qInfo->GetRequiredRaces();

    if (reqraces == 0)
        return true;

    if ((reqraces & getRaceMask()) == 0)
    {
        if (msg)
            SendCanTakeQuestResponse(INVALIDREASON_QUEST_FAILED_WRONG_RACE);

        return false;
    }

    return true;
}

bool Player::SatisfyQuestReputation(Quest const* qInfo, bool msg) const
{
    uint32 fIdMin = qInfo->GetRequiredMinRepFaction(); // Min required rep
    if (fIdMin &&
        GetReputationMgr().GetReputation(fIdMin) <
            qInfo->GetRequiredMinRepValue())
    {
        if (msg)
            SendCanTakeQuestResponse(INVALIDREASON_DONT_HAVE_REQ);

        return false;
    }

    uint32 fIdMax = qInfo->GetRequiredMaxRepFaction(); // Max required rep
    if (fIdMax &&
        GetReputationMgr().GetReputation(fIdMax) >=
            qInfo->GetRequiredMaxRepValue())
    {
        if (msg)
            SendCanTakeQuestResponse(INVALIDREASON_DONT_HAVE_REQ);

        return false;
    }

    return true;
}

bool Player::SatisfyQuestStatus(Quest const* qInfo, bool msg) const
{
    auto itr = mQuestStatus.find(qInfo->GetQuestId());

    if (itr != mQuestStatus.end() && itr->second.m_status != QUEST_STATUS_NONE)
    {
        if (msg)
            SendCanTakeQuestResponse(INVALIDREASON_QUEST_ALREADY_ON);

        return false;
    }

    return true;
}

bool Player::SatisfyQuestTimed(Quest const* qInfo, bool msg) const
{
    if (!m_timedquests.empty() &&
        qInfo->HasSpecialFlag(QUEST_SPECIAL_FLAG_TIMED))
    {
        if (msg)
            SendCanTakeQuestResponse(INVALIDREASON_QUEST_ONLY_ONE_TIMED);

        return false;
    }

    return true;
}

bool Player::SatisfyQuestExclusiveGroup(Quest const* qInfo, bool msg) const
{
    // non positive exclusive group, if > 0 then can be start if any other quest
    // in exclusive group already started/completed
    if (qInfo->GetExclusiveGroup() <= 0)
        return true;

    ExclusiveQuestGroupsMapBounds bounds =
        sObjectMgr::Instance()->GetExclusiveQuestGroupsMapBounds(
            qInfo->GetExclusiveGroup());

    assert(bounds.first !=
           bounds.second); // must always be found if qInfo->ExclusiveGroup != 0

    for (auto iter = bounds.first; iter != bounds.second; ++iter)
    {
        uint32 exclude_Id = iter->second;

        // skip checked quest id, only state of other quests in group is
        // interesting
        if (exclude_Id == qInfo->GetQuestId())
            continue;

        // not allow have daily quest if daily quest from exclusive group
        // already recently completed
        Quest const* Nquest =
            sObjectMgr::Instance()->GetQuestTemplate(exclude_Id);
        if (!SatisfyQuestDay(Nquest, false))
        {
            if (msg)
                SendCanTakeQuestResponse(INVALIDREASON_DONT_HAVE_REQ);

            return false;
        }

        auto i_exstatus = mQuestStatus.find(exclude_Id);

        // alternative quest already started or completed
        if (i_exstatus != mQuestStatus.end() &&
            (i_exstatus->second.m_status == QUEST_STATUS_COMPLETE ||
                i_exstatus->second.m_status == QUEST_STATUS_INCOMPLETE))
        {
            if (msg)
                SendCanTakeQuestResponse(INVALIDREASON_DONT_HAVE_REQ);

            return false;
        }
    }

    return true;
}

bool Player::SatisfyQuestNextChain(Quest const* qInfo, bool msg) const
{
    if (!qInfo->GetNextQuestInChain())
        return true;

    // next quest in chain already started or completed
    auto itr = mQuestStatus.find(qInfo->GetNextQuestInChain());
    if (itr != mQuestStatus.end() &&
        (itr->second.m_status == QUEST_STATUS_COMPLETE ||
            itr->second.m_status == QUEST_STATUS_INCOMPLETE))
    {
        if (msg)
            SendCanTakeQuestResponse(INVALIDREASON_DONT_HAVE_REQ);

        return false;
    }

    // check for all quests further up the chain
    // only necessary if there are quest chains with more than one quest that
    // can be skipped
    // return SatisfyQuestNextChain( qInfo->GetNextQuestInChain(), msg );
    return true;
}

bool Player::SatisfyQuestPrevChain(Quest const* qInfo, bool msg) const
{
    // No previous quest in chain
    if (qInfo->prevChainQuests.empty())
        return true;

    for (auto prevId : qInfo->prevChainQuests)
    {
        // If any of the previous quests in chain active, return false
        if (IsCurrentQuest(prevId))
        {
            if (msg)
                SendCanTakeQuestResponse(INVALIDREASON_DONT_HAVE_REQ);

            return false;
        }

        // check for all quests further down the chain
        // only necessary if there are quest chains with more than one quest
        // that can be skipped
        // if( !SatisfyQuestPrevChain( prevId, msg ) )
        //    return false;
    }

    // No previous quest in chain active
    return true;
}

bool Player::SatisfyQuestDay(Quest const* qInfo, bool msg) const
{
    if (!qInfo->IsDaily())
        return true;

    bool have_slot = false;
    for (uint32 quest_daily_idx = 0; quest_daily_idx < PLAYER_MAX_DAILY_QUESTS;
         ++quest_daily_idx)
    {
        uint32 id =
            GetUInt32Value(PLAYER_FIELD_DAILY_QUESTS_1 + quest_daily_idx);
        if (qInfo->GetQuestId() == id)
            return false;

        if (!id)
            have_slot = true;
    }

    if (!have_slot)
    {
        if (msg)
            SendCanTakeQuestResponse(INVALIDREASON_DAILY_QUESTS_REMAINING);

        return false;
    }

    return true;
}

void Player::GiveQuestSourceItemIfNeed(Quest const* pQuest)
{
    if (uint32 srcitem = pQuest->GetSrcItemId())
    {
        // Subtract any items we already have
        int count = static_cast<int>(pQuest->GetSrcItemCount()) -
                    static_cast<int>(storage().item_count(srcitem));
        if (count > 0)
        {
            inventory::transaction trans;
            trans.add(srcitem, count);
            if (!storage().finalize(trans))
                SendEquipError(
                    static_cast<InventoryResult>(trans.error()), nullptr);
        }
    }
}

bool Player::TakeQuestSourceItem(uint32 quest_id, bool msg)
{
    const Quest* quest_info =
        sObjectMgr::Instance()->GetQuestTemplate(quest_id);
    if (quest_info)
    {
        uint32 src_item = quest_info->GetSrcItemId();
        uint32 count = quest_info->GetSrcItemCount() > 0 ?
                           quest_info->GetSrcItemCount() :
                           1;

        // The item might be gone already, or its quantity lowered
        uint32 has_count =
            storage().item_count(src_item); // includes bank and buyback
        if (has_count < count)
            count = has_count;

        // If count is 0, it means the item was already deleted
        if (src_item > 0 && count > 0)
        {
            inventory::transaction trans;
            trans.destroy(src_item, count);
            // It's possible some items cannot be deleted, for example an
            // equipped non-empty bag
            if (!storage().finalize(trans))
            {
                if (msg)
                    SendEquipError(
                        static_cast<InventoryResult>(trans.error()), nullptr);
                return false;
            }
        }
    }
    return true;
}

bool Player::GetQuestRewardStatus(uint32 quest_id) const
{
    Quest const* qInfo = sObjectMgr::Instance()->GetQuestTemplate(quest_id);
    if (qInfo)
    {
        // for repeatable quests: rewarded field is set after first reward only
        // to prevent getting XP more than once
        auto itr = mQuestStatus.find(quest_id);
        if (itr != mQuestStatus.end() &&
            itr->second.m_status != QUEST_STATUS_NONE && !qInfo->IsRepeatable())
            return itr->second.m_rewarded;

        return false;
    }
    return false;
}

QuestStatus Player::GetQuestStatus(uint32 quest_id) const
{
    if (quest_id)
    {
        auto itr = mQuestStatus.find(quest_id);
        if (itr != mQuestStatus.end())
            return itr->second.m_status;
    }
    return QUEST_STATUS_NONE;
}

bool Player::CanShareQuest(uint32 quest_id) const
{
    if (Quest const* qInfo = sObjectMgr::Instance()->GetQuestTemplate(quest_id))
        if (qInfo->HasQuestFlag(QUEST_FLAGS_SHARABLE))
            return IsCurrentQuest(quest_id);

    return false;
}

void Player::SetQuestStatus(uint32 quest_id, QuestStatus status)
{
    if (sObjectMgr::Instance()->GetQuestTemplate(quest_id))
    {
        QuestStatusData& q_status = mQuestStatus[quest_id];

        q_status.m_status = status;

        if (q_status.uState != QUEST_NEW)
            q_status.uState = QUEST_CHANGED;
    }

    UpdateForQuestWorldObjects();
}

bool Player::IsQuestObjectiveComplete(
    uint32 questId, uint32 objectiveIndex) const
{
    const Quest* q = sObjectMgr::Instance()->GetQuestTemplate(questId);
    auto itr = mQuestStatus.find(questId);
    if (!q || itr == mQuestStatus.end())
        return false;

    const QuestStatusData& data = itr->second;
    if (data.m_status != QUEST_STATUS_INCOMPLETE)
        return true;

    if (q->ReqCreatureOrGOCount[objectiveIndex])
        if (data.m_creatureOrGOcount[objectiveIndex] <
            q->ReqCreatureOrGOCount[objectiveIndex])
            return false;
    if (q->ReqItemCount[objectiveIndex])
        if (data.m_itemcount[objectiveIndex] < q->ReqItemCount[objectiveIndex])
            return false;

    return true;
}

// not used in MaNGOS, but used in scripting code
uint32 Player::GetReqKillOrCastCurrentCount(uint32 quest_id, int32 entry)
{
    Quest const* qInfo = sObjectMgr::Instance()->GetQuestTemplate(quest_id);
    if (!qInfo)
        return 0;

    for (int j = 0; j < QUEST_OBJECTIVES_COUNT; ++j)
        if (qInfo->ReqCreatureOrGOId[j] == entry)
            return mQuestStatus[quest_id].m_creatureOrGOcount[j];

    return 0;
}

void Player::AdjustQuestReqItemCount(
    Quest const* pQuest, QuestStatusData& questStatusData)
{
    if (pQuest->HasSpecialFlag(QUEST_SPECIAL_FLAG_DELIVER))
    {
        for (int i = 0; i < QUEST_ITEM_OBJECTIVES_COUNT; ++i)
        {
            uint32 reqitemcount = pQuest->ReqItemCount[i];
            if (reqitemcount != 0)
            {
                uint32 curitemcount =
                    storage().item_count(pQuest->ReqItemId[i]);

                questStatusData.m_itemcount[i] =
                    std::min(curitemcount, reqitemcount);
                if (questStatusData.uState != QUEST_NEW)
                    questStatusData.uState = QUEST_CHANGED;
            }
        }
    }
}

uint16 Player::FindQuestSlot(uint32 quest_id) const
{
    for (uint16 i = 0; i < MAX_QUEST_LOG_SIZE; ++i)
        if (GetQuestSlotQuestId(i) == quest_id)
            return i;

    return MAX_QUEST_LOG_SIZE;
}

// FIXME: Rename to a name indicating the function deals with quests
void Player::AreaExploredOrEventHappens(uint32 questId)
{
    if (questId)
    {
        uint16 log_slot = FindQuestSlot(questId);
        if (log_slot < MAX_QUEST_LOG_SIZE)
        {
            QuestStatusData& q_status = mQuestStatus[questId];

            if (!q_status.m_explored)
            {
                SetQuestSlotState(log_slot, QUEST_STATE_COMPLETE);
                SendQuestCompleteEvent(questId);
                q_status.m_explored = true;

                if (q_status.uState != QUEST_NEW)
                    q_status.uState = QUEST_CHANGED;
            }
        }
        if (CanCompleteQuest(questId))
            CompleteQuest(questId);
    }
}

// FIXME: Move functionalit to CompleteGroupQuest (as the name is more
// appropriate) -- right now that function simply calls this one
void Player::GroupEventHappens(uint32 questId, WorldObject const* pEventObject)
{
    if (Group* group = GetGroup())
    {
        for (auto member : group->members(true))
        {
            // for any leave or dead (with not released body) group member at
            // appropriate distance
            if (member->IsAtGroupRewardDistance(pEventObject) &&
                !member->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_GHOST))
                member->AreaExploredOrEventHappens(questId);
        }
    }
    else
        AreaExploredOrEventHappens(questId);
}

void Player::KilledMonster(CreatureInfo const* cInfo, ObjectGuid guid)
{
    if (cInfo->Entry)
        KilledMonsterCredit(cInfo->Entry, guid);

    for (int i = 0; i < MAX_KILL_CREDIT; ++i)
        if (cInfo->KillCredit[i])
            KilledMonsterCredit(cInfo->KillCredit[i], guid);
}

void Player::KilledMonsterCredit(
    uint32 entry, ObjectGuid guid, bool from_script)
{
    uint32 addkillcount = 1;

    for (int i = 0; i < MAX_QUEST_LOG_SIZE; ++i)
    {
        uint32 questid = GetQuestSlotQuestId(i);
        if (!questid)
            continue;

        Quest const* qInfo = sObjectMgr::Instance()->GetQuestTemplate(questid);
        if (!qInfo)
            continue;
        // just if !ingroup || !noraidgroup || raidgroup
        QuestStatusData& q_status = mQuestStatus[questid];
        if (q_status.m_status == QUEST_STATUS_INCOMPLETE &&
            (!GetGroup() || !GetGroup()->isRaidGroup() ||
                qInfo->IsAllowedInRaid()))
        {
            if (qInfo->HasSpecialFlag(QUEST_SPECIAL_FLAG_SCRIPT_COMPLETED) &&
                !from_script)
                continue;

            if (qInfo->HasSpecialFlag(QUEST_SPECIAL_FLAG_KILL_OR_CAST))
            {
                for (int j = 0; j < QUEST_OBJECTIVES_COUNT; ++j)
                {
                    // skip GO activate objective or none
                    if (qInfo->ReqCreatureOrGOId[j] <= 0)
                        continue;

                    // skip Cast at creature objective
                    if (qInfo->ReqSpell[j] != 0)
                        continue;

                    uint32 reqkill = qInfo->ReqCreatureOrGOId[j];

                    if (reqkill == entry)
                    {
                        uint32 reqkillcount = qInfo->ReqCreatureOrGOCount[j];
                        uint32 curkillcount = q_status.m_creatureOrGOcount[j];
                        if (curkillcount < reqkillcount)
                        {
                            q_status.m_creatureOrGOcount[j] =
                                curkillcount + addkillcount;
                            if (q_status.uState != QUEST_NEW)
                                q_status.uState = QUEST_CHANGED;

                            SendQuestUpdateAddCreatureOrGo(qInfo, guid, j,
                                q_status.m_creatureOrGOcount[j]);
                        }

                        if (CanCompleteQuest(questid))
                            CompleteQuest(questid);

                        // same objective target can be in many active quests,
                        // but not in 2 objectives for single quest (code
                        // optimization).
                        continue;
                    }
                }
            }
        }
    }
}

void Player::CastedCreatureOrGO(
    uint32 entry, ObjectGuid guid, uint32 spell_id, bool original_caster)
{
    bool isCreature = guid.IsCreature();

    uint32 addCastCount = 1;
    for (int i = 0; i < MAX_QUEST_LOG_SIZE; ++i)
    {
        uint32 questid = GetQuestSlotQuestId(i);
        if (!questid)
            continue;

        Quest const* qInfo = sObjectMgr::Instance()->GetQuestTemplate(questid);
        if (!qInfo)
            continue;

        if (!original_caster && !qInfo->HasQuestFlag(QUEST_FLAGS_SHARABLE))
            continue;

        if (!qInfo->HasSpecialFlag(QUEST_SPECIAL_FLAG_KILL_OR_CAST))
            continue;

        QuestStatusData& q_status = mQuestStatus[questid];

        if (q_status.m_status != QUEST_STATUS_INCOMPLETE)
            continue;

        for (int j = 0; j < QUEST_OBJECTIVES_COUNT; ++j)
        {
            // skip kill creature objective (0) or wrong spell casts
            if (qInfo->ReqSpell[j] != spell_id)
                continue;

            uint32 reqTarget = 0;

            if (isCreature)
            {
                // creature activate objectives
                if (qInfo->ReqCreatureOrGOId[j] > 0)
                    // checked at quest_template loading
                    reqTarget = qInfo->ReqCreatureOrGOId[j];
            }
            else
            {
                // GO activate objective
                if (qInfo->ReqCreatureOrGOId[j] < 0)
                    // checked at quest_template loading
                    reqTarget = -qInfo->ReqCreatureOrGOId[j];
            }

            // other not this creature/GO related objectives
            if (reqTarget != entry)
                continue;

            uint32 reqCastCount = qInfo->ReqCreatureOrGOCount[j];
            uint32 curCastCount = q_status.m_creatureOrGOcount[j];
            if (curCastCount < reqCastCount)
            {
                q_status.m_creatureOrGOcount[j] = curCastCount + addCastCount;
                if (q_status.uState != QUEST_NEW)
                    q_status.uState = QUEST_CHANGED;

                SendQuestUpdateAddCreatureOrGo(
                    qInfo, guid, j, q_status.m_creatureOrGOcount[j]);
            }

            if (CanCompleteQuest(questid))
                CompleteQuest(questid);

            // same objective target can be in many active quests, but not in 2
            // objectives for single quest (code optimization).
            break;
        }
    }
}

void Player::TalkedToCreature(uint32 entry, ObjectGuid guid)
{
    uint32 addTalkCount = 1;
    for (int i = 0; i < MAX_QUEST_LOG_SIZE; ++i)
    {
        uint32 questid = GetQuestSlotQuestId(i);
        if (!questid)
            continue;

        Quest const* qInfo = sObjectMgr::Instance()->GetQuestTemplate(questid);
        if (!qInfo)
            continue;

        QuestStatusData& q_status = mQuestStatus[questid];

        if (q_status.m_status == QUEST_STATUS_INCOMPLETE)
        {
            if (qInfo->HasSpecialFlag(
                    QuestSpecialFlags(QUEST_SPECIAL_FLAG_KILL_OR_CAST |
                                      QUEST_SPECIAL_FLAG_SPEAKTO)) &&
                !qInfo->HasSpecialFlag(QUEST_SPECIAL_FLAG_NO_TALK_TO_CREDIT))
            {
                for (int j = 0; j < QUEST_OBJECTIVES_COUNT; ++j)
                {
                    // skip spell casts and Gameobject objectives
                    if (qInfo->ReqSpell[j] > 0 ||
                        qInfo->ReqCreatureOrGOId[j] < 0)
                        continue;

                    uint32 reqTarget = 0;

                    if (qInfo->ReqCreatureOrGOId[j] >
                        0) // creature activate objectives
                           // checked at quest_template loading
                        reqTarget = qInfo->ReqCreatureOrGOId[j];
                    else
                        continue;

                    if (reqTarget == entry)
                    {
                        uint32 reqTalkCount = qInfo->ReqCreatureOrGOCount[j];
                        uint32 curTalkCount = q_status.m_creatureOrGOcount[j];
                        if (curTalkCount < reqTalkCount)
                        {
                            q_status.m_creatureOrGOcount[j] =
                                curTalkCount + addTalkCount;
                            if (q_status.uState != QUEST_NEW)
                                q_status.uState = QUEST_CHANGED;

                            SendQuestUpdateAddCreatureOrGo(qInfo, guid, j,
                                q_status.m_creatureOrGOcount[j]);
                        }
                        if (CanCompleteQuest(questid))
                            CompleteQuest(questid);

                        // same objective target can be in many active quests,
                        // but not in 2 objectives for single quest (code
                        // optimization).
                        continue;
                    }
                }
            }
        }
    }
}

void Player::ReputationChanged(FactionEntry const* factionEntry)
{
    for (int i = 0; i < MAX_QUEST_LOG_SIZE; ++i)
    {
        if (uint32 questid = GetQuestSlotQuestId(i))
        {
            if (Quest const* qInfo =
                    sObjectMgr::Instance()->GetQuestTemplate(questid))
            {
                if (qInfo->GetRepObjectiveFaction() == factionEntry->ID)
                {
                    QuestStatusData& q_status = mQuestStatus[questid];
                    if (q_status.m_status == QUEST_STATUS_INCOMPLETE)
                    {
                        if (GetReputationMgr().GetReputation(factionEntry) >=
                            qInfo->GetRepObjectiveValue())
                            if (CanCompleteQuest(questid))
                                CompleteQuest(questid);
                    }
                    else if (q_status.m_status == QUEST_STATUS_COMPLETE)
                    {
                        if (GetReputationMgr().GetReputation(factionEntry) <
                            qInfo->GetRepObjectiveValue())
                            IncompleteQuest(questid);
                    }
                }
            }
        }
    }
}

bool Player::HasQuestForItem(uint32 itemid) const
{
    for (int i = 0; i < MAX_QUEST_LOG_SIZE; ++i)
    {
        uint32 questid = GetQuestSlotQuestId(i);
        if (questid == 0)
            continue;

        auto qs_itr = mQuestStatus.find(questid);
        if (qs_itr == mQuestStatus.end())
            continue;

        QuestStatusData const& q_status = qs_itr->second;

        if (q_status.m_status == QUEST_STATUS_INCOMPLETE)
        {
            Quest const* qinfo =
                sObjectMgr::Instance()->GetQuestTemplate(questid);
            if (!qinfo)
                continue;

            // hide quest if player is in raid-group and quest is no raid quest
            if (GetGroup() && GetGroup()->isRaidGroup() &&
                !qinfo->IsAllowedInRaid() && !InBattleGround())
                continue;

            // There should be no mixed ReqItem/ReqSource drop
            // This part for ReqItem drop
            for (int j = 0; j < QUEST_ITEM_OBJECTIVES_COUNT; ++j)
            {
                if (itemid == qinfo->ReqItemId[j] &&
                    q_status.m_itemcount[j] < qinfo->ReqItemCount[j])
                    return true;
            }
            // This part - for ReqSource
            for (int j = 0; j < QUEST_SOURCE_ITEM_IDS_COUNT; ++j)
            {
                // examined item is a source item
                if (qinfo->ReqSourceId[j] == itemid)
                {
                    ItemPrototype const* pProto =
                        ObjectMgr::GetItemPrototype(itemid);

                    // 'unique' item
                    if (pProto->MaxCount &&
                        storage().item_count(itemid) < pProto->MaxCount)
                        return true;

                    // allows custom amount drop when not 0
                    if (qinfo->ReqSourceCount[j])
                    {
                        if (storage().item_count(itemid) <
                            qinfo->ReqSourceCount[j])
                            return true;
                    }
                    else if (storage().item_count(itemid) < pProto->Stackable)
                        return true;
                }
            }
        }
    }
    return false;
}

// Used for quests having some event (explore, escort, "external event") as
// quest objective.
void Player::SendQuestCompleteEvent(uint32 quest_id)
{
    if (quest_id)
    {
        WorldPacket data(SMSG_QUESTUPDATE_COMPLETE, 4);
        data << uint32(quest_id);
        GetSession()->send_packet(std::move(data));
    }
}

void Player::SendQuestReward(
    Quest const* pQuest, uint32 XP, Object* /*questGiver*/)
{
    uint32 questid = pQuest->GetQuestId();
    WorldPacket data(SMSG_QUESTGIVER_QUEST_COMPLETE,
        (4 + 4 + 4 + 4 + 4 + 4 + pQuest->GetRewItemsCount() * 8));
    data << uint32(questid);
    data << uint32(0x03);

    if (getLevel() <
        sWorld::Instance()->getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL))
    {
        data << uint32(XP);
        data << uint32(pQuest->GetRewOrReqMoney());
    }
    else
    {
        data << uint32(0);
        data << uint32(
            pQuest->GetRewOrReqMoney() +
            int32(pQuest->GetRewMoneyMaxLevel() *
                  sWorld::Instance()->getConfig(CONFIG_FLOAT_RATE_DROP_MONEY)));
    }
    data << uint32(0);                          // new 2.3.0, HonorPoints?
    data << uint32(pQuest->GetRewItemsCount()); // max is 5

    for (uint32 i = 0; i < pQuest->GetRewItemsCount(); ++i)
    {
        if (pQuest->RewItemId[i] > 0)
            data << pQuest->RewItemId[i] << pQuest->RewItemCount[i];
        else
            data << uint32(0) << uint32(0);
    }
    GetSession()->send_packet(std::move(data));
}

void Player::SendQuestFailed(uint32 quest_id)
{
    if (quest_id)
    {
        WorldPacket data(SMSG_QUESTGIVER_QUEST_FAILED, 4);
        data << uint32(quest_id);
        GetSession()->send_packet(std::move(data));
    }
}

void Player::SendQuestTimerFailed(uint32 quest_id)
{
    if (quest_id)
    {
        WorldPacket data(SMSG_QUESTUPDATE_FAILEDTIMER, 4);
        data << uint32(quest_id);
        GetSession()->send_packet(std::move(data));
    }
}

void Player::SendCanTakeQuestResponse(uint32 msg) const
{
    WorldPacket data(SMSG_QUESTGIVER_QUEST_INVALID, 4);
    data << uint32(msg);
    GetSession()->send_packet(std::move(data));
}

void Player::SendQuestConfirmAccept(const Quest* pQuest, Player* pReceiver)
{
    if (pReceiver)
    {
        int loc_idx = pReceiver->GetSession()->GetSessionDbLocaleIndex();
        std::string title = pQuest->GetTitle();
        sObjectMgr::Instance()->GetQuestLocaleStrings(
            pQuest->GetQuestId(), loc_idx, &title);

        WorldPacket data(SMSG_QUEST_CONFIRM_ACCEPT, (4 + title.size() + 8));
        data << uint32(pQuest->GetQuestId());
        data << title;
        data << GetObjectGuid();
        pReceiver->GetSession()->send_packet(std::move(data));
    }
}

void Player::SendPushToPartyResponse(Player* pPlayer, uint32 msg)
{
    if (pPlayer)
    {
        WorldPacket data(MSG_QUEST_PUSH_RESULT, (8 + 1));
        data << pPlayer->GetObjectGuid();
        data << uint8(msg); // valid values: 0-8
        GetSession()->send_packet(std::move(data));
    }
}

void Player::SendQuestUpdateAddCreatureOrGo(
    Quest const* pQuest, ObjectGuid guid, uint32 creatureOrGO_idx, uint32 count)
{
    assert(count < 256 && "mob/GO count store in 8 bits 2^8 = 256 (0..256)");

    int32 entry = pQuest->ReqCreatureOrGOId[creatureOrGO_idx];
    if (entry < 0)
        // client expected gameobject template id in form (id|0x80000000)
        entry = (-entry) | 0x80000000;

    WorldPacket data(SMSG_QUESTUPDATE_ADD_KILL, (4 * 4 + 8));
    data << uint32(pQuest->GetQuestId());
    data << uint32(entry);
    data << uint32(count);
    data << uint32(pQuest->ReqCreatureOrGOCount[creatureOrGO_idx]);
    data << guid;
    GetSession()->send_packet(std::move(data));

    uint16 log_slot = FindQuestSlot(pQuest->GetQuestId());
    if (log_slot < MAX_QUEST_LOG_SIZE)
        SetQuestSlotCounter(log_slot, creatureOrGO_idx, count);
}

/*********************************************************/
/***                   LOAD SYSTEM                     ***/
/*********************************************************/

void Player::_LoadDeclinedNames(QueryResult* result)
{
    if (!result)
        return;

    if (m_declinedname)
        delete m_declinedname;

    m_declinedname = new DeclinedName;
    Field* fields = result->Fetch();
    for (int i = 0; i < MAX_DECLINED_NAME_CASES; ++i)
        m_declinedname->name[i] = fields[i].GetCppString();

    delete result;
}

void Player::_LoadArenaTeamInfo(QueryResult* result)
{
    // arenateamid, played_week, played_season, personal_rating
    memset((void*)&m_uint32Values[PLAYER_FIELD_ARENA_TEAM_INFO_1_1], 0,
        sizeof(uint32) * MAX_ARENA_SLOT * ARENA_TEAM_END);
    if (!result)
        return;

    do
    {
        Field* fields = result->Fetch();

        uint32 arenateamid = fields[0].GetUInt32();
        uint32 played_week = fields[1].GetUInt32();
        uint32 played_season = fields[2].GetUInt32();
        uint32 personal_rating = fields[3].GetUInt32();

        ArenaTeam* aTeam =
            sObjectMgr::Instance()->GetArenaTeamById(arenateamid);
        if (!aTeam)
        {
            logging.error(
                "Player::_LoadArenaTeamInfo: couldn't load arenateam %u, week "
                "%u, season %u, rating %u",
                arenateamid, played_week, played_season, personal_rating);
            continue;
        }
        uint8 arenaSlot = aTeam->GetSlot();

        SetArenaTeamInfoField(arenaSlot, ARENA_TEAM_ID, arenateamid);
        SetArenaTeamInfoField(arenaSlot, ARENA_TEAM_MEMBER,
            (aTeam->GetCaptainGuid() == GetObjectGuid()) ? 0 : 1);
        SetArenaTeamInfoField(arenaSlot, ARENA_TEAM_GAMES_WEEK, played_week);
        SetArenaTeamInfoField(
            arenaSlot, ARENA_TEAM_GAMES_SEASON, played_season);
        SetArenaTeamInfoField(arenaSlot, ARENA_TEAM_WINS_SEASON, 0);
        SetArenaTeamInfoField(
            arenaSlot, ARENA_TEAM_PERSONAL_RATING, personal_rating);

    } while (result->NextRow());
    delete result;
}

void Player::_LoadBGData(QueryResult* result)
{
    if (!result)
        return;

    // Expecting only one row
    Field* fields = result->Fetch();
    /* bgInstanceID, bgTeam, x, y, z, o, map */
    m_bgData.bgInstanceID = fields[0].GetUInt32();
    m_bgData.bgTeam = Team(fields[1].GetUInt32());
    m_bgData.joinPos = WorldLocation(fields[6].GetUInt32(), // Map
        fields[2].GetFloat(),                               // X
        fields[3].GetFloat(),                               // Y
        fields[4].GetFloat(),                               // Z
        fields[5].GetFloat());                              // Orientation

    delete result;
}

bool Player::LoadPositionFromDB(ObjectGuid guid, uint32& mapid, float& x,
    float& y, float& z, float& o, bool& in_flight)
{
    std::unique_ptr<QueryResult> result(CharacterDatabase.PQuery(
        "SELECT position_x,position_y,position_z,orientation,map,taxi_path "
        "FROM characters WHERE guid = '%u'",
        guid.GetCounter()));
    if (!result)
        return false;

    Field* fields = result->Fetch();

    x = fields[0].GetFloat();
    y = fields[1].GetFloat();
    z = fields[2].GetFloat();
    o = fields[3].GetFloat();
    mapid = fields[4].GetUInt32();
    in_flight = !fields[5].GetCppString().empty();

    return true;
}

void Player::_LoadIntoDataField(
    const char* data, uint32 startOffset, uint32 count)
{
    if (!data)
        return;

    Tokens tokens = StrSplit(data, " ");

    if (tokens.size() != count)
        return;

    Tokens::iterator iter;
    uint32 index;
    for (iter = tokens.begin(), index = 0; index < count; ++iter, ++index)
    {
        m_uint32Values[startOffset + index] = atol((*iter).c_str());
        _changedFields[startOffset + index] = true;
    }
}

bool Player::LoadFromDB(ObjectGuid guid, SqlQueryHolder* holder)
{
    // SELECT
    // guid, account, name, race, class, gender, level, xp, money, playerBytes,
    // playerBytes2, playerFlags," position_x, position_y, position_z, map,
    // orientation, taximask, cinematic, totaltime, leveltime, rest_bonus,
    // logout_time, is_logout_resting, resettalents_cost, resettalents_time,
    // trans_x, trans_y, trans_z, trans_o, transguid, extra_flags, stable_slots,
    // at_login, zone, online, death_expire_time, taxi_path, dungeon_difficulty,
    // arenaPoints, totalHonorPoints, todayHonorPoints, yesterdayHonorPoints,
    // totalKills, todayKills, yesterdayKills, chosenTitle, watchedFaction,
    // drunk, health, power1, power2, power3, power4, power5, exploredZones,
    // equipmentCache, ammoId, knownTitles, actionBars, pvp_flagged, fall_z
    // FROM characters WHERE guid = '%u'", GUID_LOPART(m_guid));
    QueryResult* result = holder->GetResult(PLAYER_LOGIN_QUERY_LOADFROM);

    if (!result)
    {
        logging.error("%s not found in table `characters`, can't load. ",
            guid.GetString().c_str());
        return false;
    }

    Field* fields = result->Fetch();

    uint32 dbAccountId = fields[1].GetUInt32();

    // check if the character's account in the db and the logged in account
    // match.
    // player should be able to load/delete character only with correct account!
    if (dbAccountId != GetSession()->GetAccountId())
    {
        logging.error("%s loading from wrong account (is: %u, should be: %u)",
            guid.GetString().c_str(), GetSession()->GetAccountId(),
            dbAccountId);
        delete result;
        return false;
    }

    Object::_Create(guid.GetCounter(), 0, HIGHGUID_PLAYER);

    // Set initial state of movement generators
    movement_gens.reset();

    m_name = fields[2].GetCppString();

    // check name limitations
    if (ObjectMgr::CheckPlayerName(m_name) != CHAR_NAME_SUCCESS ||
        (GetSession()->GetSecurity() == SEC_PLAYER &&
            sObjectMgr::Instance()->IsReservedName(m_name)))
    {
        delete result;
        CharacterDatabase.PExecute(
            "UPDATE characters SET at_login = at_login | '%u' WHERE guid ='%u'",
            uint32(AT_LOGIN_RENAME), guid.GetCounter());
        return false;
    }

    // overwrite possible wrong/corrupted guid
    SetGuidValue(OBJECT_FIELD_GUID, guid);

    // overwrite some data fields
    SetByteValue(UNIT_FIELD_BYTES_0, 0, fields[3].GetUInt8()); // race
    SetByteValue(UNIT_FIELD_BYTES_0, 1, fields[4].GetUInt8()); // class

    uint8 gender = fields[5].GetUInt8() & 0x01;  // allowed only 1 bit values
                                                 // male/female cases (for fit
                                                 // drunk gender part)
    SetByteValue(UNIT_FIELD_BYTES_0, 2, gender); // gender

    SetByteValue(UNIT_FIELD_BYTES_2, 1,
        UNIT_BYTE2_FLAG_SANCTUARY | UNIT_BYTE2_FLAG_AURASLOT_SETUP_CHNG);

    SetUInt32Value(UNIT_FIELD_LEVEL, fields[6].GetUInt8());
    SetUInt32Value(PLAYER_XP, fields[7].GetUInt32());

    _LoadIntoDataField(fields[55].GetString(), PLAYER_EXPLORED_ZONES_1,
        PLAYER_EXPLORED_ZONES_SIZE);
    _LoadIntoDataField(fields[58].GetString(), PLAYER__FIELD_KNOWN_TITLES, 2);

    InitDisplayIds(); // model, scale and model data

    uint32 money = fields[8].GetUInt32();

    SetUInt32Value(PLAYER_BYTES, fields[9].GetUInt32());
    SetUInt32Value(PLAYER_BYTES_2, fields[10].GetUInt32());

    m_drunk = fields[48].GetUInt16();

    SetUInt16Value(PLAYER_BYTES_3, 0, (m_drunk & 0xFFFE) | gender);

    SetUInt32Value(PLAYER_FLAGS, fields[11].GetUInt32());
    SetInt32Value(PLAYER_FIELD_WATCHED_FACTION_INDEX, fields[47].GetInt32());

    SetUInt32Value(PLAYER_AMMO_ID, fields[57].GetUInt32());

    // Action bars state
    SetByteValue(PLAYER_FIELD_BYTES, 2, fields[59].GetUInt8());

    LOG_DEBUG(logging, "Load Basic value of player %s is: ", m_name.c_str());
    outDebugStatsValues();

    // Need to call it to initialize m_team (m_team can be calculated from race)
    // Other way is to saves m_team into characters table.
    setFactionForRace(getRace());
    SetCharm(nullptr);

    // load home bind and check in same time class/race pair, it used later for
    // restore broken positions
    if (!_LoadHomeBind(holder->GetResult(PLAYER_LOGIN_QUERY_LOADHOMEBIND)))
    {
        delete result;
        return false;
    }

    InitPrimaryProfessions(); // to max set before any spell loaded

    // init saved position, and fix it later if problematic
    uint32 transGUID = fields[30].GetUInt32();
    Relocate(
        fields[12].GetFloat(), fields[13].GetFloat(), fields[14].GetFloat());
    SetOrientation(fields[16].GetFloat());
    SetLocationMapId(fields[15].GetUInt32());

    uint32 difficulty = fields[38].GetUInt32();
    if (difficulty >= MAX_DIFFICULTY)
        difficulty = DUNGEON_DIFFICULTY_NORMAL;
    SetDifficulty(Difficulty(difficulty)); // may be changed in _LoadGroup

    _LoadGroup(holder->GetResult(PLAYER_LOGIN_QUERY_LOADGROUP));

    _LoadArenaTeamInfo(holder->GetResult(PLAYER_LOGIN_QUERY_LOADARENAINFO));

    SetArenaPoints(fields[39].GetUInt32());

    // check arena teams integrity
    for (uint32 arena_slot = 0; arena_slot < MAX_ARENA_SLOT; ++arena_slot)
    {
        uint32 arena_team_id = GetArenaTeamId(arena_slot);
        if (!arena_team_id)
            continue;

        if (ArenaTeam* at =
                sObjectMgr::Instance()->GetArenaTeamById(arena_team_id))
            if (at->HaveMember(GetObjectGuid()))
                continue;

        // arena team not exist or not member, cleanup fields
        for (int j = 0; j < ARENA_TEAM_END; ++j)
            SetArenaTeamInfoField(arena_slot, ArenaTeamInfoType(j), 0);
    }

    SetHonorPoints(fields[40].GetUInt32());

    SetUInt32Value(PLAYER_FIELD_TODAY_CONTRIBUTION, fields[41].GetUInt32());
    SetUInt32Value(PLAYER_FIELD_YESTERDAY_CONTRIBUTION, fields[42].GetUInt32());
    SetUInt32Value(
        PLAYER_FIELD_LIFETIME_HONORBALE_KILLS, fields[43].GetUInt32());
    SetUInt16Value(PLAYER_FIELD_KILLS, 0, fields[44].GetUInt16());
    SetUInt16Value(PLAYER_FIELD_KILLS, 1, fields[45].GetUInt16());

    _LoadInstanceBinds(holder->GetResult(PLAYER_LOGIN_QUERY_LOADINSTANCEBINDS));

    bool force_relocated = false;

    if (!IsPositionValid())
    {
        logging.error(
            "%s have invalid coordinates (X: %f Y: %f Z: %f O: %f). Teleport "
            "to default race/class locations.",
            guid.GetString().c_str(), GetX(), GetY(), GetZ(), GetO());
        RelocateToHomebind();
        force_relocated = true;

        transGUID = 0;

        m_movementInfo.transport.Reset();
    }

    _LoadBGData(holder->GetResult(PLAYER_LOGIN_QUERY_LOADBGDATA));

    if (m_bgData.bgInstanceID) // saved in BattleGround
    {
        BattleGround* currentBg = sBattleGroundMgr::Instance()->GetBattleGround(
            m_bgData.bgInstanceID, BATTLEGROUND_TYPE_NONE);

        bool player_at_bg =
            currentBg && currentBg->IsPlayerInBattleGround(GetObjectGuid());

        if (player_at_bg && currentBg->GetStatus() != STATUS_WAIT_LEAVE)
        {
            m_bgData.bgTypeID =
                currentBg->GetTypeID(); // bg data not marked as modified

            // join player to battleground group
            currentBg->EventPlayerLoggedIn(this, GetObjectGuid());
            currentBg->AddOrSetPlayerToCorrectBgGroup(
                this, GetObjectGuid(), m_bgData.bgTeam);
        }
        else
        {
            // leave bg
            if (player_at_bg)
                currentBg->RemovePlayerAtLeave(GetObjectGuid(), false);

            // move to bg enter point
            const WorldLocation& _loc = GetBattleGroundEntryPoint();
            SetLocationMapId(_loc.mapid);
            Relocate(_loc.coord_x, _loc.coord_y, _loc.coord_z);
            SetOrientation(_loc.orientation);
            force_relocated = true;

            // We are not in BG anymore
            SetBattleGroundId(0, BATTLEGROUND_TYPE_NONE);
            ClearBGData();
            // DB data will be removed on next save
        }
    }
    else
    {
        MapEntry const* mapEntry = sMapStore.LookupEntry(GetMapId());
        // if server restart after player save in BG or area
        // player can have current coordinates in to BG/Arena map, fix this
        if (!mapEntry || mapEntry->IsBattleGroundOrArena())
        {
            const WorldLocation& _loc = GetBattleGroundEntryPoint();
            SetLocationMapId(_loc.mapid);
            Relocate(_loc.coord_x, _loc.coord_y, _loc.coord_z);
            SetOrientation(_loc.orientation);
            force_relocated = true;

            // We are not in BG anymore
            SetBattleGroundId(0, BATTLEGROUND_TYPE_NONE);
            ClearBGData();
            // DB data will be removed on next save
        }
    }

    if (transGUID != 0)
    {
        m_movementInfo.transport.guid =
            ObjectGuid(HIGHGUID_MO_TRANSPORT, transGUID);
        m_movementInfo.transport.pos.Set(fields[26].GetFloat(),
            fields[27].GetFloat(), fields[28].GetFloat(),
            fields[29].GetFloat());

        if (!maps::verify_coords(GetX() + m_movementInfo.transport.pos.x,
                GetY() + m_movementInfo.transport.pos.y) ||
            // transport size limited
            m_movementInfo.transport.pos.x > 50 ||
            m_movementInfo.transport.pos.y > 50 ||
            m_movementInfo.transport.pos.z > 50)
        {
            logging.error(
                "%s have invalid transport coordinates (X: %f Y: %f Z: %f O: "
                "%f). Teleport to default race/class locations.",
                guid.GetString().c_str(),
                GetX() + m_movementInfo.transport.pos.x,
                GetY() + m_movementInfo.transport.pos.y,
                GetZ() + m_movementInfo.transport.pos.z,
                GetO() + m_movementInfo.transport.pos.o);

            RelocateToHomebind();
            force_relocated = true;

            m_movementInfo.transport.Reset();

            transGUID = 0;
        }
    }

    if (transGUID != 0)
    {
        if (Transport* trans = sTransportMgr::Instance()->GetContinentTransport(
                m_movementInfo.transport.guid))
        {
            MapEntry const* transMapEntry =
                sMapStore.LookupEntry(trans->GetMapId());
            // client without expansion support
            if (GetSession()->Expansion() < transMapEntry->Expansion())
            {
                LOG_DEBUG(logging,
                    "Player %s using client without required expansion tried "
                    "login at transport at non accessible map %u",
                    GetName(), trans->GetMapId());
            }
            else
            {
                trans->AddPassenger(this);
                SetLocationMapId(trans->GetMapId());
            }
        }

        if (!GetTransport())
        {
            logging.error(
                "%s have problems with transport guid (%u). Teleport to "
                "default race/class locations.",
                guid.GetString().c_str(), transGUID);

            RelocateToHomebind();
            force_relocated = true;

            m_movementInfo.transport.Reset();

            transGUID = 0;
        }
    }
    else // not transport case
    {
        MapEntry const* mapEntry = sMapStore.LookupEntry(GetMapId());
        // client without expansion support
        if (GetSession()->Expansion() < mapEntry->Expansion())
        {
            LOG_DEBUG(logging,
                "Player %s using client without required expansion tried login "
                "at non accessible map %u",
                GetName(), GetMapId());
            RelocateToHomebind();
            force_relocated = true;
        }
    }

    // player bounded instance saves loaded in _LoadBoundInstances, group
    // versions at group loading
    DungeonPersistentState* state = GetInstanceBindForZoning(GetMapId());

    // load the player's map here if it's not already loaded
    SetMap(sMapMgr::Instance()->CreateMap(GetMapId(), this));

    // if the player is in an instance and it has been reset in the meantime
    // teleport him to the entrance
    if (GetInstanceId() && !state)
    {
        AreaTrigger const* at =
            sObjectMgr::Instance()->GetMapEntranceTrigger(GetMapId());
        if (at)
        {
            Relocate(at->target_X, at->target_Y, at->target_Z);
            SetOrientation(at->target_Orientation);
            force_relocated = true;
        }
        else
            logging.error(
                "Player %s(GUID: %u) logged in to a reset instance (map: %u) "
                "and there is no area-trigger leading to this map. Thus he "
                "can't be ported back to the entrance. This _might_ be an "
                "exploit attempt.",
                GetName(), GetGUIDLow(), GetMapId());
    }

    time_t now = WorldTimer::time_no_syscall();
    time_t logoutTime = time_t(fields[22].GetUInt64());

    // since last logout (in seconds)
    uint32 time_diff = uint32(now - logoutTime);

    // set value, including drunk invisibility detection
    // calculate sobering. after 15 minutes logged out, the player will be sober
    // again
    float soberFactor;
    if (time_diff > 15 * MINUTE)
        soberFactor = 0;
    else
        soberFactor = 1 - time_diff / (15.0f * MINUTE);
    uint16 newDrunkenValue = uint16(soberFactor * m_drunk);
    SetDrunkValue(newDrunkenValue);

    m_cinematic = fields[18].GetUInt32();
    m_Played_time[PLAYED_TIME_TOTAL] = fields[19].GetUInt32();
    m_Played_time[PLAYED_TIME_LEVEL] = fields[20].GetUInt32();

    m_resetTalentsCost = fields[24].GetUInt32();
    m_resetTalentsTime = time_t(fields[25].GetUInt64());

    // reserve some flags
    uint32 old_safe_flags = GetUInt32Value(PLAYER_FLAGS) &
                            (PLAYER_FLAGS_HIDE_CLOAK | PLAYER_FLAGS_HIDE_HELM);

    if (HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_GM))
        SetUInt32Value(PLAYER_FLAGS, 0 | old_safe_flags);

    m_taxi.LoadTaxiMask(
        fields[17].GetString()); // must be before InitTaxiNodesForLevel

    uint32 extraflags = fields[31].GetUInt32();

    m_stableSlots = fields[32].GetUInt32();
    if (m_stableSlots > MAX_PET_STABLES)
    {
        logging.error(
            "Player can have not more %u stable slots, but have in DB %u",
            MAX_PET_STABLES, uint32(m_stableSlots));
        m_stableSlots = MAX_PET_STABLES;
    }

    m_atLoginFlags = fields[33].GetUInt32();

    // Honor system
    // Update Honor kills data
    m_lastHonorUpdateTime = logoutTime;
    UpdateHonorFields();

    m_deathExpireTime = (time_t)fields[36].GetUInt64();
    if (m_deathExpireTime > now + MAX_DEATH_COUNT * DEATH_EXPIRE_STEP)
        m_deathExpireTime = now + MAX_DEATH_COUNT * DEATH_EXPIRE_STEP - 1;

    std::string taxi_nodes = fields[37].GetCppString();

    // clear channel spell data (if saved at channel spell casting)
    SetChannelObjectGuid(ObjectGuid());
    SetUInt32Value(UNIT_CHANNEL_SPELL, 0);

    // clear charm/summon related fields
    SetCharm(nullptr);
    SetPet(nullptr);
    SetTargetGuid(ObjectGuid());
    SetCharmerGuid(ObjectGuid());
    SetOwnerGuid(ObjectGuid());
    SetCreatorGuid(ObjectGuid());
    client_mover_ = GetObjectGuid();

    // reset some aura modifiers before aura apply

    SetGuidValue(PLAYER_FARSIGHT, ObjectGuid());
    SetUInt32Value(PLAYER_TRACK_CREATURES, 0);
    SetUInt32Value(PLAYER_TRACK_RESOURCES, 0);

    // cleanup aura list explicitly before skill load where some spells can be
    // applied
    remove_auras();

    // make sure the unit is considered out of combat for proper loading
    ClearInCombat();

    // make sure the unit is considered not in duel for proper loading
    SetGuidValue(PLAYER_DUEL_ARBITER, ObjectGuid());
    SetUInt32Value(PLAYER_DUEL_TEAM, 0);

    // reset stats before loading any modifiers
    InitStatsForLevel();
    InitTaxiNodesForLevel();

    // rest bonus can only be calculated after InitStatsForLevel()
    m_rest_bonus = fields[21].GetFloat();

    if (time_diff > 0)
    {
        // speed collect rest bonus in offline, in logout, far from tavern, city
        // (section/in hour)
        float bubble0 = 0.031f;
        // speed collect rest bonus in offline, in logout, in tavern, city
        // (section/in hour)
        float bubble1 = 0.125f;
        float bubble =
            fields[23].GetUInt32() > 0 ?
                bubble1 *
                    sWorld::Instance()->getConfig(
                        CONFIG_FLOAT_RATE_REST_OFFLINE_IN_TAVERN_OR_CITY) :
                bubble0 *
                    sWorld::Instance()->getConfig(
                        CONFIG_FLOAT_RATE_REST_OFFLINE_IN_WILDERNESS);

        SetRestBonus(GetRestBonus() +
                     time_diff *
                         ((float)GetUInt32Value(PLAYER_NEXT_LEVEL_XP) / 72000) *
                         bubble);
    }

    // load skills after InitStatsForLevel because it triggering aura apply also
    _LoadSkills(holder->GetResult(PLAYER_LOGIN_QUERY_LOADSKILLS));

    // apply original stats mods before spell loading or item equipment that
    // call before equip _RemoveStatsMods()

    // Mail
    _LoadMails(holder->GetResult(PLAYER_LOGIN_QUERY_LOADMAILS));
    _LoadMailedItems(holder->GetResult(PLAYER_LOGIN_QUERY_LOADMAILEDITEMS));
    UpdateNextMailTimeAndUnreads();

    _LoadAuras(holder->GetResult(PLAYER_LOGIN_QUERY_LOADAURAS), time_diff);

    // add ghost flag (must be after aura load: PLAYER_FLAGS_GHOST set in aura)
    if (HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_GHOST))
        m_deathState = DEAD;

    _LoadSpells(holder->GetResult(PLAYER_LOGIN_QUERY_LOADSPELLS));

    // after spell load
    InitTalentForLevel();
    learnDefaultSpells();

    // after spell load, learn rewarded spell if need also
    _LoadQuestStatus(holder->GetResult(PLAYER_LOGIN_QUERY_LOADQUESTSTATUS));
    _LoadDailyQuestStatus(
        holder->GetResult(PLAYER_LOGIN_QUERY_LOADDAILYQUESTSTATUS));

    // must be before inventory (some items required reputation check)
    m_reputationMgr.LoadFromDB(
        holder->GetResult(PLAYER_LOGIN_QUERY_LOADREPUTATION));

    _LoadInventory(
        holder->GetResult(PLAYER_LOGIN_QUERY_LOADINVENTORY), money, time_diff);
    _LoadItemLoot(holder->GetResult(PLAYER_LOGIN_QUERY_LOADITEMLOOT));

    // update item durations that keep ticking while offline
    UpdateItemDurations(time_diff, true);

    _LoadActions(holder->GetResult(PLAYER_LOGIN_QUERY_LOADACTIONS));

    m_social = sSocialMgr::Instance()->LoadFromDB(
        holder->GetResult(PLAYER_LOGIN_QUERY_LOADSOCIALLIST), GetObjectGuid());

    // check PLAYER_CHOSEN_TITLE compatibility with PLAYER__FIELD_KNOWN_TITLES
    // note: PLAYER__FIELD_KNOWN_TITLES updated at quest status loaded
    uint32 curTitle = fields[46].GetUInt32();
    if (curTitle && !HasTitle(curTitle))
        curTitle = 0;

    SetUInt32Value(PLAYER_CHOSEN_TITLE, curTitle);

    if (!m_taxi.LoadTaxiDestinationsFromString(taxi_nodes, GetTeam()))
    {
        // problems with taxi path loading
        TaxiNodesEntry const* nodeEntry = nullptr;
        if (uint32 node_id = m_taxi.GetTaxiSource())
            nodeEntry = sTaxiNodesStore.LookupEntry(node_id);

        if (!nodeEntry) // don't know taxi start node, to homebind
        {
            logging.error(
                "Character %u have wrong data in taxi destination list, "
                "teleport to homebind.",
                GetGUIDLow());
            RelocateToHomebind();
        }
        else // have start node, to it
        {
            logging.error(
                "Character %u have too short taxi destination list, teleport "
                "to original node.",
                GetGUIDLow());
            SetLocationMapId(nodeEntry->map_id);
            Relocate(nodeEntry->x, nodeEntry->y, nodeEntry->z);
        }

        // we can be relocated from taxi and still have an outdated Map pointer!
        // so we need to get a new Map pointer!
        SetMap(sMapMgr::Instance()->CreateMap(GetMapId(), this));

        m_taxi.ClearTaxiDestinations();
        force_relocated = true;
    }

    // checked in m_taxi.LoadTaxiDestinationsFromString
    assert(m_taxi.GetTaxiSource() == 0 ||
           sTaxiNodesStore.LookupEntry(m_taxi.GetTaxiSource()));

    // has to be called after last Relocate() in Player::LoadFromDB
    float fallz = force_relocated ? GetZ() : fields[61].GetFloat();
    SetFallInformation(0, fallz);

    _LoadSpellCooldowns(
        holder->GetResult(PLAYER_LOGIN_QUERY_LOADSPELLCOOLDOWNS),
        holder->GetResult(PLAYER_LOGIN_QUERY_LOADCATEGORYCOOLDOWN));

    // Spell code allow apply any auras to dead character in load time in
    // aura/spell/item loading
    // Do now before stats re-calculation cleanup for ghost state unexpected
    // auras
    if (!isAlive())
        remove_auras_if([this](AuraHolder* h)
            {
                return !(h->IsPassive() && h->GetCaster() == this &&
                           player_or_pet()) &&
                       !h->IsDeathPersistent();
            });

    // apply all stat bonuses from items and auras
    SetCanModifyStats(true);
    UpdateAllStats();

    // restore remembered power/health values (but not more max values)
    uint32 savedhealth = fields[49].GetUInt32();
    SetHealth(savedhealth > GetMaxHealth() ? GetMaxHealth() : savedhealth);
    for (uint32 i = 0; i < MAX_POWERS; ++i)
    {
        uint32 savedpower = fields[50 + i].GetUInt32();
        SetPower(Powers(i), savedpower > GetMaxPower(Powers(i)) ?
                                GetMaxPower(Powers(i)) :
                                savedpower);
    }

    LOG_DEBUG(logging, "The value of player %s after load item and aura is: ",
        m_name.c_str());
    outDebugStatsValues();

    bool pvp_flagged = fields[60].GetUInt8();

    // all fields read
    delete result;

    // GM state
    if (GetSession()->GetSecurity() > SEC_PLAYER)
    {
        switch (sWorld::Instance()->getConfig(CONFIG_UINT32_GM_LOGIN_STATE))
        {
        default:
        case 0:
            break; // disable
        case 1:
            SetGameMaster(true);
            break; // enable
        case 2:    // save state
            if (extraflags & PLAYER_EXTRA_GM_ON)
                SetGameMaster(true);
            break;
        }

        switch (sWorld::Instance()->getConfig(CONFIG_UINT32_GM_VISIBLE_STATE))
        {
        default:
        case 0:
            SetGMVisible(false);
            break; // invisible
        case 1:
            break; // visible
        case 2:    // save state
            if (extraflags & PLAYER_EXTRA_GM_INVISIBLE)
                SetGMVisible(false);
            break;
        }

        switch (sWorld::Instance()->getConfig(CONFIG_UINT32_GM_CHAT))
        {
        default:
        case 0:
            break; // disable
        case 1:
            SetGMChat(true);
            break; // enable
        case 2:    // save state
            if (extraflags & PLAYER_EXTRA_GM_CHAT)
                SetGMChat(true);
            break;
        }
    }

    _LoadDeclinedNames(holder->GetResult(PLAYER_LOGIN_QUERY_LOADDECLINEDNAMES));
    _LoadRecentDungeons(
        holder->GetResult(PLAYER_LOGIN_QUERY_LOADRECENTDUNGEONS));

    _LoadPetStore(holder);

    // If you log off with pvp flag, your 5 minute PvP timer resets
    if (pvp_flagged)
    {
        SetPvP(true);
        pvpInfo.endTimer = WorldTimer::time_no_syscall();
    }

    return true;
}

bool Player::IsAllowedToLoot(const Creature* creature) const
{
    if (!creature->GetLootDistributor())
        return false;

    return creature->GetLootDistributor()->can_view_loot(this);
}

void Player::_LoadActions(QueryResult* result)
{
    m_actionButtons.clear();

    // QueryResult *result = CharacterDatabase.PQuery("SELECT button,action,type
    // FROM character_action WHERE guid = '%u' ORDER BY button",GetGUIDLow());

    if (result)
    {
        do
        {
            Field* fields = result->Fetch();

            uint8 button = fields[0].GetUInt8();
            uint32 action = fields[1].GetUInt32();
            uint8 type = fields[2].GetUInt8();

            if (ActionButton* ab = addActionButton(button, action, type))
                ab->uState = ACTIONBUTTON_UNCHANGED;
            else
            {
                logging.error("  ...at loading, and will deleted in DB also");

                // Will deleted in DB at next save (it can create data until
                // save but marked as deleted)
                m_actionButtons[button].uState = ACTIONBUTTON_DELETED;
            }
        } while (result->NextRow());

        delete result;
    }
}

void Player::_LoadAuras(QueryResult* result, uint32 timediff)
{
    // remove_auras(); -- some spells casted before aura load, for example in
    // LoadSkills, aura list explicitly cleaned early

    // all aura related fields
    for (int i = UNIT_FIELD_AURA; i <= UNIT_FIELD_AURASTATE; ++i)
        SetUInt32Value(i, 0);

    // QueryResult *result = CharacterDatabase.PQuery("SELECT
    // caster_guid,item_guid,spell,stackcount,remaincharges,basepoints0,basepoints1,basepoints2,periodictime0,periodictime1,periodictime2,maxduration,remaintime,effIndexMask
    // FROM character_aura WHERE guid = '%u'",GetGUIDLow());

    if (result)
    {
        do
        {
            Field* fields = result->Fetch();
            ObjectGuid caster_guid = ObjectGuid(fields[0].GetUInt64());
            uint32 item_lowguid = fields[1].GetUInt32();
            uint32 spellid = fields[2].GetUInt32();
            uint32 stackcount = fields[3].GetUInt32();
            uint32 remaincharges = fields[4].GetUInt32();
            int32 damage[MAX_EFFECT_INDEX];
            uint32 periodicTime[MAX_EFFECT_INDEX];

            for (int32 i = 0; i < MAX_EFFECT_INDEX; ++i)
            {
                damage[i] = fields[i + 5].GetInt32();
                periodicTime[i] = fields[i + 8].GetUInt32();
            }

            int32 maxduration = fields[11].GetInt32();
            int32 remaintime = fields[12].GetInt32();
            uint32 effIndexMask = fields[13].GetUInt32();

            SpellEntry const* spellproto = sSpellStore.LookupEntry(spellid);
            if (!spellproto)
            {
                logging.error("Unknown spell (spellid %u), ignore.", spellid);
                continue;
            }

            if (remaintime != -1 && !IsPositiveSpell(spellproto))
            {
                if (remaintime / IN_MILLISECONDS <= int32(timediff))
                    continue;

                remaintime -= timediff * IN_MILLISECONDS;
            }

            // prevent wrong values of remaincharges
            if (spellproto->procCharges == 0)
                remaincharges = 0;

            if (!spellproto->StackAmount)
                stackcount = 1;
            else if (spellproto->StackAmount < stackcount)
                stackcount = spellproto->StackAmount;
            else if (!stackcount)
                stackcount = 1;

            AuraHolder* holder = CreateAuraHolder(spellproto, this, nullptr);
            holder->SetLoadedState(caster_guid,
                ObjectGuid(HIGHGUID_ITEM, item_lowguid), stackcount,
                remaincharges, maxduration, remaintime);

            for (int32 i = 0; i < MAX_EFFECT_INDEX; ++i)
            {
                if ((effIndexMask & (1 << i)) == 0)
                    continue;

                Aura* aura = CreateAura(
                    spellproto, SpellEffectIndex(i), nullptr, holder, this);
                if (!damage[i])
                    damage[i] = aura->GetModifier()->m_amount;

                aura->SetLoadedState(damage[i], periodicTime[i]);
                holder->AddAura(aura, SpellEffectIndex(i));
            }

            if (!holder->IsEmptyHolder())
            {
                AddAuraHolder(holder);
                LOG_DEBUG(
                    logging, "Added auras from spellid %u", spellproto->Id);
            }
            else
                delete holder;
        } while (result->NextRow());
        delete result;
    }

    if (getClass() == CLASS_WARRIOR && !HasAuraType(SPELL_AURA_MOD_SHAPESHIFT))
        CastSpell(this, SPELL_ID_PASSIVE_BATTLE_STANCE, true);
}

void Player::LoadCorpse()
{
    if (isAlive())
    {
        sObjectAccessor::Instance()->ConvertCorpseForPlayer(GetObjectGuid());
    }
    else
    {
        if (Corpse* corpse = GetCorpse())
        {
            ApplyModByteFlag(PLAYER_FIELD_BYTES, 0,
                PLAYER_FIELD_BYTE_RELEASE_TIMER,
                corpse &&
                    !sMapStore.LookupEntry(corpse->GetMapId())->Instanceable());
        }
        else
        {
            // Prevent Dead Player login without corpse
            ResurrectPlayer(0.5f);
        }
    }
}

void Player::_LoadInventory(QueryResult* result, uint32 money, uint32 timediff)
{
    std::vector<Item*> mail_items;
    std::string mail_subject;

    mail_items = inventory_.load(result, money, timediff);

    if (!mail_items.empty())
        mail_subject = GetSession()->GetMangosString(LANG_NOT_EQUIPPED_ITEM);

    auto itr = mail_items.begin();
    while (itr != mail_items.end())
    {
        MailDraft draft(mail_subject);

        for (int count = 0; itr != mail_items.end() && count < 16;
             ++itr, ++count)
            draft.AddItem(*itr);

        draft.SendMailTo(
            this, MailSender(this, MAIL_STATIONERY_GM), MAIL_CHECK_MASK_COPIED);
    }
}

void Player::_LoadItemLoot(QueryResult* result)
{
    // QueryResult *result = CharacterDatabase.PQuery("SELECT
    // guid,itemid,amount,suffix,property FROM item_loot WHERE guid = '%u'",
    // GetGUIDLow());

    if (result)
    {
        do
        {
            Field* fields = result->Fetch();
            uint32 item_guid = fields[0].GetUInt32();

            Item* item = GetItemByGuid(ObjectGuid(HIGHGUID_ITEM, item_guid));

            if (!item)
            {
                CharacterDatabase.PExecute(
                    "DELETE FROM item_loot WHERE guid = '%u'", item_guid);
                logging.error(
                    "Player::_LoadItemLoot: Player %s has loot for nonexistent "
                    "item (GUID: %u) in `item_loot`, deleted.",
                    GetName(), item_guid);
                continue;
            }

            item->LoadLootFromDB(fields);

        } while (result->NextRow());

        delete result;
    }
}

// load mailed item which should receive current player
void Player::_LoadMailedItems(QueryResult* result)
{
    // data needs to be at first place for Item::LoadFromDB
    //         0     1        2          3
    // "SELECT data, mail_id, item_guid, item_template FROM mail_items JOIN
    // item_instance ON item_guid = guid WHERE receiver = '%u'",
    // GUID_LOPART(m_guid)
    if (!result)
        return;

    do
    {
        Field* fields = result->Fetch();
        uint32 mail_id = fields[1].GetUInt32();
        uint32 item_guid_low = fields[2].GetUInt32();
        uint32 item_template = fields[3].GetUInt32();

        Mail* mail = GetMail(mail_id);
        if (!mail)
            continue;
        mail->AddItem(item_guid_low, item_template);

        ItemPrototype const* proto = ObjectMgr::GetItemPrototype(item_template);

        if (!proto)
        {
            logging.error(
                "Player %u has unknown item_template (ProtoType) in mailed "
                "items(GUID: %u template: %u) in mail (%u), deleted.",
                GetGUIDLow(), item_guid_low, item_template, mail->messageID);
            CharacterDatabase.PExecute(
                "DELETE FROM mail_items WHERE item_guid = '%u'", item_guid_low);
            CharacterDatabase.PExecute(
                "DELETE FROM item_instance WHERE guid = '%u'", item_guid_low);
            continue;
        }

        /*XXX:*/
        auto item = new Item(proto);

        if (!item->LoadFromDB(item_guid_low, fields, GetObjectGuid()))
        {
            logging.error(
                "Player::_LoadMailedItems - Item in mail (%u) doesn't exist "
                "!!!! - item guid: %u, deleted from mail",
                mail->messageID, item_guid_low);
            CharacterDatabase.PExecute(
                "DELETE FROM mail_items WHERE item_guid = '%u'", item_guid_low);

            item->db_delete();
            delete item;
            continue;
        }

        AddMItem(item);
    } while (result->NextRow());

    delete result;
}

void Player::_LoadMails(QueryResult* result)
{
    m_mail.clear();
    //        0  1           2      3        4       5          6           7
    //        8     9   10      11         12             13
    //"SELECT
    // id,messageType,sender,receiver,subject,itemTextId,expire_time,deliver_time,money,cod,checked,stationery,mailTemplateId,has_items
    // FROM mail WHERE receiver = '%u' ORDER BY id DESC",GetGUIDLow()
    if (!result)
        return;

    do
    {
        Field* fields = result->Fetch();
        auto m = new Mail;
        m->messageID = fields[0].GetUInt32();
        m->messageType = fields[1].GetUInt8();
        m->sender = fields[2].GetUInt32();
        m->receiverGuid = ObjectGuid(HIGHGUID_PLAYER, fields[3].GetUInt32());
        m->subject = fields[4].GetCppString();
        m->itemTextId = fields[5].GetUInt32();
        m->expire_time = (time_t)fields[6].GetUInt64();
        m->deliver_time = (time_t)fields[7].GetUInt64();
        m->money = fields[8].GetUInt32();
        m->COD = fields[9].GetUInt32();
        m->checked = fields[10].GetUInt32();
        m->stationery = fields[11].GetUInt8();
        m->mailTemplateId = fields[12].GetInt16();
        m->has_items = fields[13].GetBool(); // true, if mail have items or mail
                                             // have template and items
                                             // generated (maybe none)

        if (m->mailTemplateId &&
            !sMailTemplateStore.LookupEntry(m->mailTemplateId))
        {
            logging.error(
                "Player::_LoadMail - Mail (%u) have nonexistent MailTemplateId "
                "(%u), remove at load",
                m->messageID, m->mailTemplateId);
            m->mailTemplateId = 0;
        }

        m->state = MAIL_STATE_UNCHANGED;

        m_mail.push_back(m);

        if (m->mailTemplateId && !m->has_items)
            m->prepareTemplateItems(this);

    } while (result->NextRow());
    delete result;
}

void Player::LoadPet()
{
    // fixme: the pet should still be loaded if the player is not in world
    // just not added to the map
    if (IsInWorld())
    {
        auto pet = new Pet;
        if (!pet->LoadPetFromDB(this, 0, 0, true))
            delete pet;
    }
}

void Player::_LoadQuestStatus(QueryResult* result)
{
    mQuestStatus.clear();

    uint32 slot = 0;

    ////                                                     0      1       2
    /// 3         4      5          6          7          8          9
    /// 10          11          12
    // QueryResult *result = CharacterDatabase.PQuery("SELECT quest, status,
    // rewarded, explored, timer, mobcount1, mobcount2, mobcount3, mobcount4,
    // itemcount1, itemcount2, itemcount3, itemcount4 FROM character_queststatus
    // WHERE guid = '%u'", GetGUIDLow());

    if (result)
    {
        do
        {
            Field* fields = result->Fetch();

            uint32 quest_id = fields[0].GetUInt32();
            // used to be new, no delete?
            Quest const* pQuest =
                sObjectMgr::Instance()->GetQuestTemplate(quest_id);
            if (pQuest)
            {
                // find or create
                QuestStatusData& questStatusData = mQuestStatus[quest_id];

                uint32 qstatus = fields[1].GetUInt32();
                if (qstatus < MAX_QUEST_STATUS)
                    questStatusData.m_status = QuestStatus(qstatus);
                else
                {
                    questStatusData.m_status = QUEST_STATUS_NONE;
                    logging.error(
                        "Player %s have invalid quest %d status (%d), replaced "
                        "by QUEST_STATUS_NONE(0).",
                        GetName(), quest_id, qstatus);
                }

                questStatusData.m_rewarded = (fields[2].GetUInt8() > 0);
                questStatusData.m_explored = (fields[3].GetUInt8() > 0);

                time_t quest_time = time_t(fields[4].GetUInt64());

                if (pQuest->HasSpecialFlag(QUEST_SPECIAL_FLAG_TIMED) &&
                    !GetQuestRewardStatus(quest_id) &&
                    questStatusData.m_status != QUEST_STATUS_NONE)
                {
                    AddTimedQuest(quest_id);

                    if (quest_time <= WorldTimer::time_no_syscall())
                        questStatusData.m_timer = 1;
                    else
                        questStatusData.m_timer =
                            uint32(quest_time - WorldTimer::time_no_syscall()) *
                            IN_MILLISECONDS;
                }
                else
                    quest_time = 0;

                questStatusData.m_creatureOrGOcount[0] = fields[5].GetUInt32();
                questStatusData.m_creatureOrGOcount[1] = fields[6].GetUInt32();
                questStatusData.m_creatureOrGOcount[2] = fields[7].GetUInt32();
                questStatusData.m_creatureOrGOcount[3] = fields[8].GetUInt32();
                questStatusData.m_itemcount[0] = fields[9].GetUInt32();
                questStatusData.m_itemcount[1] = fields[10].GetUInt32();
                questStatusData.m_itemcount[2] = fields[11].GetUInt32();
                questStatusData.m_itemcount[3] = fields[12].GetUInt32();

                questStatusData.uState = QUEST_UNCHANGED;

                // add to quest log
                if (slot < MAX_QUEST_LOG_SIZE &&
                    ((questStatusData.m_status == QUEST_STATUS_INCOMPLETE ||
                         questStatusData.m_status == QUEST_STATUS_COMPLETE ||
                         questStatusData.m_status == QUEST_STATUS_FAILED) &&
                        (!questStatusData.m_rewarded ||
                            pQuest->IsRepeatable())))
                {
                    SetQuestSlot(slot, quest_id, uint32(quest_time));

                    if (questStatusData.m_explored)
                        SetQuestSlotState(slot, QUEST_STATE_COMPLETE);

                    if (questStatusData.m_status == QUEST_STATUS_COMPLETE)
                        SetQuestSlotState(slot, QUEST_STATE_COMPLETE);

                    if (questStatusData.m_status == QUEST_STATUS_FAILED)
                        SetQuestSlotState(slot, QUEST_STATE_FAIL);

                    for (uint8 idx = 0; idx < QUEST_OBJECTIVES_COUNT; ++idx)
                        if (questStatusData.m_creatureOrGOcount[idx])
                            SetQuestSlotCounter(slot, idx,
                                questStatusData.m_creatureOrGOcount[idx]);

                    ++slot;
                }

                if (questStatusData.m_rewarded)
                {
                    // learn rewarded spell if unknown
                    learnQuestRewardedSpells(pQuest);

                    // set rewarded title if any
                    if (pQuest->GetCharTitleId())
                    {
                        if (CharTitlesEntry const* titleEntry =
                                sCharTitlesStore.LookupEntry(
                                    pQuest->GetCharTitleId()))
                            SetTitle(titleEntry);
                    }
                }

                LOG_DEBUG(logging,
                    "Quest status is {%u} for quest {%u} for player (GUID: %u)",
                    questStatusData.m_status, quest_id, GetGUIDLow());
            }
        } while (result->NextRow());

        delete result;
    }

    // clear quest log tail
    for (uint16 i = slot; i < MAX_QUEST_LOG_SIZE; ++i)
        SetQuestSlot(i, 0);
}

void Player::_LoadDailyQuestStatus(QueryResult* result)
{
    for (uint32 quest_daily_idx = 0; quest_daily_idx < PLAYER_MAX_DAILY_QUESTS;
         ++quest_daily_idx)
        SetUInt32Value(PLAYER_FIELD_DAILY_QUESTS_1 + quest_daily_idx, 0);

    // QueryResult *result = CharacterDatabase.PQuery("SELECT quest FROM
    // character_queststatus_daily WHERE guid = '%u'", GetGUIDLow());

    if (result)
    {
        uint32 quest_daily_idx = 0;

        do
        {
            if (quest_daily_idx >=
                PLAYER_MAX_DAILY_QUESTS) // max amount with exist data in query
            {
                logging.error(
                    "Player (GUID: %u) have more 25 daily quest records in "
                    "`charcter_queststatus_daily`",
                    GetGUIDLow());
                break;
            }

            Field* fields = result->Fetch();

            uint32 quest_id = fields[0].GetUInt32();

            Quest const* pQuest =
                sObjectMgr::Instance()->GetQuestTemplate(quest_id);
            if (!pQuest)
                continue;

            SetUInt32Value(
                PLAYER_FIELD_DAILY_QUESTS_1 + quest_daily_idx, quest_id);
            ++quest_daily_idx;

            LOG_DEBUG(logging,
                "Daily quest {%u} cooldown for player (GUID: %u)", quest_id,
                GetGUIDLow());
        } while (result->NextRow());

        delete result;
    }

    m_DailyQuestChanged = false;
}

void Player::_LoadSpells(QueryResult* result)
{
    // QueryResult *result = CharacterDatabase.PQuery("SELECT
    // spell,active,disabled FROM character_spell WHERE guid =
    // '%u'",GetGUIDLow());

    if (result)
    {
        do
        {
            Field* fields = result->Fetch();

            uint32 spell_id = fields[0].GetUInt32();

            addSpell(spell_id, fields[1].GetBool(), false, false,
                fields[2].GetBool());
        } while (result->NextRow());

        delete result;
    }
}

void Player::_LoadGroup(QueryResult* result)
{
    // QueryResult *result = CharacterDatabase.PQuery("SELECT groupId FROM
    // group_member WHERE memberGuid='%u'", GetGUIDLow());
    if (result)
    {
        uint32 groupId = (*result)[0].GetUInt32();
        delete result;

        if (Group* group = sObjectMgr::Instance()->GetGroupById(groupId))
        {
            uint8 subgroup = group->GetMemberGroup(GetObjectGuid());
            SetGroup(group, subgroup);
            if (getLevel() >= LEVELREQUIREMENT_HEROIC)
            {
                // the group leader may change the instance difficulty while the
                // player is offline
                SetDifficulty(group->GetDifficulty());
            }
        }
    }
}

void Player::_LoadInstanceBinds(QueryResult* result)
{
    for (auto& elem : m_instanceBinds)
        elem.clear();

    // QueryResult *result = CharacterDatabase.PQuery("SELECT id, permanent,
    // map, difficulty, resettime FROM character_instance LEFT JOIN instance ON
    // instance = id WHERE guid = '%u'", GUID_LOPART(m_guid));
    if (!result)
        return;

    do
    {
        Field* fields = result->Fetch();
        uint32 instanceId = fields[0].GetUInt32();
        bool perm = fields[1].GetBool();
        uint32 mapId = fields[2].GetUInt32();
        uint8 difficulty = fields[3].GetUInt8();
        time_t resetTime = (time_t)fields[4].GetUInt64();

        auto mapEntry = sMapStore.LookupEntry(mapId);
        if (!mapEntry || !mapEntry->IsDungeon())
        {
            logging.error(
                "_LoadBoundInstances: player %s(%d) has bind to nonexistent or "
                "not dungeon map %d",
                GetName(), GetGUIDLow(), mapId);
            CharacterDatabase.PExecute(
                "DELETE FROM character_instance WHERE guid = '%u' AND instance "
                "= '%u'",
                GetGUIDLow(), instanceId);
            continue;
        }

        if (difficulty >= MAX_DIFFICULTY)
        {
            logging.error(
                "_LoadBoundInstances: player %s(%d) has bind to nonexistent "
                "difficulty %d instance for map %u",
                GetName(), GetGUIDLow(), difficulty, mapId);
            CharacterDatabase.PExecute(
                "DELETE FROM character_instance WHERE guid = '%u' AND instance "
                "= '%u'",
                GetGUIDLow(), instanceId);
            continue;
        }

        sMapPersistentStateMgr::Instance()->AddPersistentState(mapEntry,
            instanceId, Difficulty(difficulty), resetTime, !perm, true);
        auto state =
            sMapPersistentStateMgr::Instance()->GetDungeonPersistentState(
                instanceId);

        if (state)
            BindToInstance(std::move(state), perm);

    } while (result->NextRow());

    delete result;
}

InstancePlayerBind* Player::GetInstanceBind(uint32 mapid, Difficulty difficulty)
{
    // some instances only have one difficulty
    const MapEntry* entry = sMapStore.LookupEntry(mapid);
    if (!entry || !entry->SupportsHeroicMode())
        difficulty = DUNGEON_DIFFICULTY_NORMAL;

    auto itr = m_instanceBinds[difficulty].find(mapid);
    if (itr != m_instanceBinds[difficulty].end())
        return &itr->second;
    else
        return nullptr;
}

InstancePlayerBind* Player::BindToInstance(
    std::shared_ptr<DungeonPersistentState> state, bool permanent)
{
    InstancePlayerBind& bind =
        m_instanceBinds[state->GetDifficulty()][state->GetMapId()];

    // Don't update anything if this is already a permanent bind
    auto prev_state = bind.state.lock();
    if (prev_state && prev_state == state && bind.perm)
        return &bind;

    if (prev_state != state)
    {
        if (prev_state)
            prev_state->UnbindPlayer(this, true); // state can become invalid
        state->BindPlayer(this);
    }

    if (permanent)
        state->SetCanReset(false);

    // "You are now saved to this instance"
    if (permanent && !bind.perm)
    {
        WorldPacket data(SMSG_INSTANCE_SAVE_CREATED, 4);
        data << uint32(0);
        GetSession()->send_packet(std::move(data));
    }

    bind.state = state;
    bind.perm = permanent;

    return &bind;
}

void Player::UnbindFromInstance(uint32 mapid, Difficulty difficulty)
{
    auto itr = m_instanceBinds[difficulty].find(mapid);
    if (itr != m_instanceBinds[difficulty].end())
    {
        if (auto state = itr->second.state.lock())
            state->UnbindPlayer(this, true); // state can become invalid
        m_instanceBinds[difficulty].erase(itr);
    }
}

void Player::ClearInstanceBindOnDestruction(DungeonPersistentState* state)
{
    auto itr = m_instanceBinds[state->GetDifficulty()].find(state->GetMapId());
    if (itr != m_instanceBinds[state->GetDifficulty()].end())
        m_instanceBinds[state->GetDifficulty()].erase(itr);
}

DungeonPersistentState* Player::GetInstanceBindForZoning(uint32 mapid)
{
    // The order of precedence when creating a new instance is:
    // Permanent binds
    // Group binds
    // Temporary binds

    auto bind = GetInstanceBind(mapid, GetDifficulty());

    if (bind && bind->perm)
    {
        if (auto state = bind->state.lock())
            return state.get();
    }

    if (Group* group = GetGroup())
        if (auto grpbind = group->GetInstanceBind(mapid, GetDifficulty()))
        {
            if (auto state = grpbind->state.lock())
                return state.get();
        }

    if (bind)
    {
        if (auto state = bind->state.lock())
            return state.get();
    }

    return nullptr;
}

void Player::UpdateInstanceBindsOnGroupJoinLeave()
{
    for (auto& elem : m_instanceBinds)
    {
        for (auto itr = elem.begin(); itr != elem.end();)
        {
            // Only remove temporary binds
            if (itr->second.perm)
            {
                ++itr;
                continue;
            }

            auto state = itr->second.state.lock();
            if (state && state->GetInstanceId() != GetInstanceId())
            {
                // Binds to instances we're not in are dropped completely
                state->UnbindPlayer(this, true);
                itr = elem.erase(itr);
            }
            else
            {
                // Bind to an instance we're in is kept, but co-owner status is
                // dropped
                if (state)
                    state->RemoveCoOwner(this);
                ++itr;
            }
        }
    }
}

void Player::SendRaidInfo()
{
    uint32 counter = 0;

    WorldPacket data(SMSG_RAID_INSTANCE_INFO, 4);

    size_t p_counter = data.wpos();
    data << uint32(counter); // placeholder

    for (auto& elem : m_instanceBinds)
    {
        for (auto& elem_itr : elem)
        {
            if (elem_itr.second.perm)
            {
                auto state = elem_itr.second.state.lock();
                if (!state)
                    continue;
                data << uint32(state->GetMapId()); // map id
                data << uint32(
                    state->GetResetTime() - WorldTimer::time_no_syscall());
                data << uint32(state->GetInstanceId()); // instance id
                data << uint32(counter);
                ++counter;
            }
        }
    }
    data.put<uint32>(p_counter, counter);
    GetSession()->send_packet(std::move(data));
}

/*
- called on every successful teleportation to a map
*/
void Player::SendSavedInstances()
{
    bool hasBeenSaved = false;
    WorldPacket data;

    for (auto& elem : m_instanceBinds)
    {
        for (auto& elem_itr : elem)
        {
            if (elem_itr.second.perm) // only permanent binds are sent
            {
                hasBeenSaved = true;
                break;
            }
        }
    }

    // Send opcode 811. true or false means, whether you have current
    // raid/heroic instances
    data.initialize(SMSG_UPDATE_INSTANCE_OWNERSHIP);
    data << uint32(hasBeenSaved);
    GetSession()->send_packet(std::move(data));

    if (!hasBeenSaved)
        return;

    for (auto& elem : m_instanceBinds)
    {
        for (auto& elem_itr : elem)
        {
            if (elem_itr.second.perm)
            {
                auto state = elem_itr.second.state.lock();
                if (!state)
                    continue;
                data.initialize(SMSG_UPDATE_LAST_INSTANCE);
                data << uint32(state->GetMapId());
                GetSession()->send_packet(std::move(data));
            }
        }
    }
}

bool Player::_LoadHomeBind(QueryResult* result)
{
    PlayerInfo const* info =
        sObjectMgr::Instance()->GetPlayerInfo(getRace(), getClass());
    if (!info)
    {
        logging.error(
            "Player have incorrect race/class pair. Can't be loaded.");
        return false;
    }

    bool ok = false;
    // QueryResult *result = CharacterDatabase.PQuery("SELECT
    // map,zone,position_x,position_y,position_z FROM character_homebind WHERE
    // guid = '%u'", GUID_LOPART(playerGuid));
    if (result)
    {
        Field* fields = result->Fetch();
        m_homebindMapId = fields[0].GetUInt32();
        m_homebindAreaId = fields[1].GetUInt16();
        m_homebindX = fields[2].GetFloat();
        m_homebindY = fields[3].GetFloat();
        m_homebindZ = fields[4].GetFloat();
        delete result;

        MapEntry const* bindMapEntry = sMapStore.LookupEntry(m_homebindMapId);

        // accept saved data only for valid position (and non instanceable), and
        // accessable
        if (maps::verify_coords(m_homebindX, m_homebindY) &&
            !bindMapEntry->Instanceable() &&
            GetSession()->Expansion() >= bindMapEntry->Expansion())
        {
            ok = true;
        }
        else
            CharacterDatabase.PExecute(
                "DELETE FROM character_homebind WHERE guid = '%u'",
                GetGUIDLow());
    }

    if (!ok)
    {
        m_homebindMapId = info->mapId;
        m_homebindAreaId = info->areaId;
        m_homebindX = info->positionX;
        m_homebindY = info->positionY;
        m_homebindZ = info->positionZ;

        CharacterDatabase.PExecute(
            "INSERT INTO character_homebind "
            "(guid,map,zone,position_x,position_y,position_z) VALUES ('%u', "
            "'%u', '%u', '%f', '%f', '%f')",
            GetGUIDLow(), m_homebindMapId, (uint32)m_homebindAreaId,
            m_homebindX, m_homebindY, m_homebindZ);
    }

    LOG_DEBUG(logging,
        "Setting player home position: mapid is: %u, zoneid is %u, X is %f, Y "
        "is %f, Z is %f",
        m_homebindMapId, m_homebindAreaId, m_homebindX, m_homebindY,
        m_homebindZ);

    return true;
}

/*********************************************************/
/***                   SAVE SYSTEM                     ***/
/*********************************************************/

void Player::SaveToDB()
{
    // we should assure this: ASSERT((m_nextSave !=
    // sWorld::Instance()->getConfig(CONFIG_UINT32_INTERVAL_SAVE)));
    // delay auto save at any saves (manual, in code, or autosave)
    m_nextSave = sWorld::Instance()->getConfig(CONFIG_UINT32_INTERVAL_SAVE);

    // lets allow only players in world to be saved
    if (IsBeingTeleportedFar())
    {
        ScheduleDelayedOperation(DELAYED_SAVE_PLAYER);
        return;
    }

    // first save/honor gain after midnight will also update the player's honor
    // fields
    UpdateHonorFields();

    LOG_DEBUG(logging, "The value of player %s at save: ", m_name.c_str());
    outDebugStatsValues();

    CharacterDatabase.BeginTransaction();

    static SqlStatementID delChar;
    static SqlStatementID insChar;

    SqlStatement stmt = CharacterDatabase.CreateStatement(
        delChar, "DELETE FROM characters WHERE guid = ?");
    stmt.PExecute(GetGUIDLow());

    SqlStatement uberInsert = CharacterDatabase.CreateStatement(insChar,
        "INSERT INTO characters "
        "(guid,account,name,race,class,gender,level,xp,money,playerBytes,"
        "playerBytes2,playerFlags,"
        "map, dungeon_difficulty, position_x, position_y, position_z, "
        "orientation, "
        "taximask, online, cinematic, "
        "totaltime, leveltime, rest_bonus, logout_time, is_logout_resting, "
        "resettalents_cost, resettalents_time, "
        "trans_x, trans_y, trans_z, trans_o, transguid, extra_flags, "
        "stable_slots, at_login, zone, "
        "death_expire_time, taxi_path, arenaPoints, totalHonorPoints, "
        "todayHonorPoints, yesterdayHonorPoints, totalKills, "
        "todayKills, yesterdayKills, chosenTitle, watchedFaction, drunk, "
        "health, power1, power2, power3, "
        "power4, power5, exploredZones, equipmentCache, ammoId, knownTitles, "
        "actionBars, pvp_flagged, fall_z) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, "
        "?, ?, ?, ?, ?, ?, "
        "?, ?, ?, "
        "?, ?, ?, ?, ?, ?, ?, "
        "?, ?, ?, ?, ?, ?, ?, ?, ?, "
        "?, ?, ?, ?, ?, ?, ?, "
        "?, ?, ?, ?, ?, ?, ?, ?, ?, "
        "?, ?, ?, ?, ?, ?, ?, ?, ?) ");

    uberInsert.addUInt32(GetGUIDLow());
    uberInsert.addUInt32(GetSession()->GetAccountId());
    uberInsert.addString(m_name);
    uberInsert.addUInt8(getRace());
    uberInsert.addUInt8(getClass());
    uberInsert.addUInt8(getGender());
    uberInsert.addUInt32(getLevel());
    uberInsert.addUInt32(GetUInt32Value(PLAYER_XP));
    uberInsert.addUInt32(storage().money().get());
    uberInsert.addUInt32(GetUInt32Value(PLAYER_BYTES));
    uberInsert.addUInt32(GetUInt32Value(PLAYER_BYTES_2));
    uberInsert.addUInt32(GetUInt32Value(PLAYER_FLAGS));

    if (!IsBeingTeleported())
    {
        uberInsert.addUInt32(GetMapId());
        uberInsert.addUInt32(uint32(GetDifficulty()));
        uberInsert.addFloat(finiteAlways(GetX()));
        uberInsert.addFloat(finiteAlways(GetY()));
        uberInsert.addFloat(finiteAlways(GetZ()));
        uberInsert.addFloat(finiteAlways(GetO()));
    }
    else
    {
        uberInsert.addUInt32(GetTeleportDest().mapid);
        uberInsert.addUInt32(uint32(GetDifficulty()));
        uberInsert.addFloat(finiteAlways(GetTeleportDest().coord_x));
        uberInsert.addFloat(finiteAlways(GetTeleportDest().coord_y));
        uberInsert.addFloat(finiteAlways(GetTeleportDest().coord_z));
        uberInsert.addFloat(finiteAlways(GetTeleportDest().orientation));
    }

    std::ostringstream ss;
    ss << m_taxi; // string with TaxiMaskSize numbers
    uberInsert.addString(ss);

    uberInsert.addUInt32(IsInWorld() ? 1 : 0);

    uberInsert.addUInt32(m_cinematic);

    uberInsert.addUInt32(m_Played_time[PLAYED_TIME_TOTAL]);
    uberInsert.addUInt32(m_Played_time[PLAYED_TIME_LEVEL]);

    uberInsert.addFloat(finiteAlways(m_rest_bonus));
    uberInsert.addUInt64(uint64(WorldTimer::time_no_syscall()));
    uberInsert.addUInt32(HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_RESTING) ? 1 : 0);
    // save, far from tavern/city
    // save, but in tavern/city
    uberInsert.addUInt32(m_resetTalentsCost);
    uberInsert.addUInt64(uint64(m_resetTalentsTime));

    uberInsert.addFloat(finiteAlways(m_movementInfo.transport.pos.x));
    uberInsert.addFloat(finiteAlways(m_movementInfo.transport.pos.y));
    uberInsert.addFloat(finiteAlways(m_movementInfo.transport.pos.z));
    uberInsert.addFloat(finiteAlways(m_movementInfo.transport.pos.o));
    if (Transport* trans = GetTransport())
        uberInsert.addUInt32(trans->GetGUIDLow());
    else
        uberInsert.addUInt32(0);

    uberInsert.addUInt32(m_ExtraFlags);

    uberInsert.addUInt32(
        uint32(m_stableSlots)); // to prevent save uint8 as char

    uberInsert.addUInt32(uint32(m_atLoginFlags));

    uberInsert.addUInt32(IsInWorld() ? GetZoneId() : GetZoneId());

    uberInsert.addUInt64(uint64(m_deathExpireTime));

    ss << m_taxi.SaveTaxiDestinationsToString(); // string
    uberInsert.addString(ss);

    uberInsert.addUInt32(GetArenaPoints());

    uberInsert.addUInt32(GetHonorPoints());

    uberInsert.addUInt32(GetUInt32Value(PLAYER_FIELD_TODAY_CONTRIBUTION));

    uberInsert.addUInt32(GetUInt32Value(PLAYER_FIELD_YESTERDAY_CONTRIBUTION));

    uberInsert.addUInt32(GetUInt32Value(PLAYER_FIELD_LIFETIME_HONORBALE_KILLS));

    uberInsert.addUInt16(GetUInt16Value(PLAYER_FIELD_KILLS, 0));

    uberInsert.addUInt16(GetUInt16Value(PLAYER_FIELD_KILLS, 1));

    uberInsert.addUInt32(GetUInt32Value(PLAYER_CHOSEN_TITLE));

    // FIXME: at this moment send to DB as unsigned, including unit32(-1)
    uberInsert.addUInt32(GetUInt32Value(PLAYER_FIELD_WATCHED_FACTION_INDEX));

    uberInsert.addUInt16(uint16(GetUInt32Value(PLAYER_BYTES_3) & 0xFFFE));

    uberInsert.addUInt32(GetHealth());

    for (uint32 i = 0; i < MAX_POWERS; ++i)
        uberInsert.addUInt32(GetPower(Powers(i)));

    for (uint32 i = 0; i < PLAYER_EXPLORED_ZONES_SIZE; ++i) // string
    {
        ss << GetUInt32Value(PLAYER_EXPLORED_ZONES_1 + i) << " ";
    }
    uberInsert.addString(ss);

    // Need to save enchants to the database so they're rendered properly in the
    // character selection screen
    for (uint32 i = inventory::equipment_start; i < inventory::equipment_end;
         ++i) // string: item id, ench (perm/temp)
    {
        ss << GetUInt32Value(item_field_offset(i) + PLAYER_VISIBLE_ITEM_ENTRY)
           << " ";

        uint32 ench1 =
            GetUInt32Value(item_field_offset(i) + PLAYER_VISIBLE_ITEM_ENCHANTS +
                           PERM_ENCHANTMENT_SLOT);
        uint32 ench2 =
            GetUInt32Value(item_field_offset(i) + PLAYER_VISIBLE_ITEM_ENCHANTS +
                           TEMP_ENCHANTMENT_SLOT);
        ss << uint32(MAKE_PAIR32(ench1, ench2)) << " ";
    }
    uberInsert.addString(ss);

    uberInsert.addUInt32(GetUInt32Value(PLAYER_AMMO_ID));

    for (uint32 i = 0; i < 2; ++i)
    {
        ss << GetUInt32Value(PLAYER__FIELD_KNOWN_TITLES + i) << " ";
    }
    uberInsert.addString(ss);

    uberInsert.addUInt32(uint32(GetByteValue(PLAYER_FIELD_BYTES, 2)));

    uberInsert.addUInt8(HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PVP) ? 1 : 0);

    uberInsert.addFloat(m_lastFallZ);

    uberInsert.Execute();

    if (m_mailsUpdated) // save mails only when needed
        _SaveMail();

    _SaveBGData();
    _SaveInventory();
    _SaveInstanceBinds();
    _SaveQuestStatus();
    _SaveDailyQuestStatus();
    _SaveSpells();
    _SaveSpellCooldowns();
    _SaveActions();
    _SaveAuras();
    _SaveSkills();
    m_reputationMgr.SaveToDB();
    GetSession()->SaveTutorialsData(); // changed only while character in game

    // save pet (hunter pet level and experience and all type pets health/mana).
    if (Pet* pet = GetPet())
        pet->SavePetToDB(PET_SAVE_AS_CURRENT);

    // Must be after Pet::SavePetToDB(), which can update the pet store
    _SavePetDbDatas();

    _SaveRecentDungeons();

    CharacterDatabase.CommitTransaction();

    // check if stats should only be saved on logout
    // save stats can be out of transaction
    if (m_session->isLogingOut() ||
        !sWorld::Instance()->getConfig(CONFIG_BOOL_STATS_SAVE_ONLY_ON_LOGOUT))
        _SaveStats();
}

// fast save function for item/money cheating preventing - save only inventory
// and money state
void Player::SaveInventoryAndGoldToDB()
{
    _SaveInventory();
    SaveGoldToDB();
}

void Player::SaveGoldToDB()
{
    static SqlStatementID updateGold;

    SqlStatement stmt = CharacterDatabase.CreateStatement(
        updateGold, "UPDATE characters SET money = ? WHERE guid = ?");
    stmt.PExecute(storage().money().get(), GetGUIDLow());
}

void Player::_SaveActions()
{
    static SqlStatementID insertAction;
    static SqlStatementID updateAction;
    static SqlStatementID deleteAction;

    for (auto itr = m_actionButtons.begin(); itr != m_actionButtons.end();)
    {
        switch (itr->second.uState)
        {
        case ACTIONBUTTON_NEW:
        {
            SqlStatement stmt = CharacterDatabase.CreateStatement(insertAction,
                "INSERT INTO character_action (guid,button,action,type) VALUES "
                "(?, ?, ?, ?)");
            stmt.addUInt32(GetGUIDLow());
            stmt.addUInt32(uint32(itr->first));
            stmt.addUInt32(itr->second.GetAction());
            stmt.addUInt32(uint32(itr->second.GetType()));
            stmt.Execute();
            itr->second.uState = ACTIONBUTTON_UNCHANGED;
            ++itr;
        }
        break;
        case ACTIONBUTTON_CHANGED:
        {
            SqlStatement stmt = CharacterDatabase.CreateStatement(updateAction,
                "UPDATE character_action  SET action = ?, type = ? WHERE guid "
                "= ? AND button = ?");
            stmt.addUInt32(itr->second.GetAction());
            stmt.addUInt32(uint32(itr->second.GetType()));
            stmt.addUInt32(GetGUIDLow());
            stmt.addUInt32(uint32(itr->first));
            stmt.Execute();
            itr->second.uState = ACTIONBUTTON_UNCHANGED;
            ++itr;
        }
        break;
        case ACTIONBUTTON_DELETED:
        {
            SqlStatement stmt = CharacterDatabase.CreateStatement(deleteAction,
                "DELETE FROM character_action WHERE guid = ? AND button = ?");
            stmt.addUInt32(GetGUIDLow());
            stmt.addUInt32(uint32(itr->first));
            stmt.Execute();
            m_actionButtons.erase(itr++);
        }
        break;
        default:
            ++itr;
            break;
        }
    }
}

void Player::_SaveAuras()
{
    static SqlStatementID deleteAuras;
    static SqlStatementID insertAuras;

    SqlStatement stmt = CharacterDatabase.CreateStatement(
        deleteAuras, "DELETE FROM character_aura WHERE guid = ?");
    stmt.PExecute(GetGUIDLow());

    std::vector<AuraHolder*> holders;
    loop_auras([&holders](AuraHolder* holder)
        {
            holders.push_back(holder);
            return true;
        });

    if (holders.empty())
        return;

    stmt = CharacterDatabase.CreateStatement(insertAuras,
        "INSERT INTO character_aura (guid, caster_guid, item_guid, spell, "
        "stackcount, remaincharges, "
        "basepoints0, basepoints1, basepoints2, periodictime0, periodictime1, "
        "periodictime2, maxduration, remaintime, effIndexMask) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");

    for (auto holder : holders)
    {
        // skip all holders from spells that are passive or channeled
        if (!holder->IsPassive() &&
            !IsChanneledSpell(holder->GetSpellProto()) &&
            !holder->GetSpellProto()->HasAttribute(
                SPELL_ATTR_CUSTOM_DONT_SAVE_AURA))
        {
            int32 damage[MAX_EFFECT_INDEX];
            uint32 periodicTime[MAX_EFFECT_INDEX];
            uint32 effIndexMask = 0;

            for (uint32 i = 0; i < MAX_EFFECT_INDEX; ++i)
            {
                damage[i] = 0;
                periodicTime[i] = 0;

                if (Aura* aur = holder->GetAura(SpellEffectIndex(i)))
                {
                    // don't save not own area auras
                    if (aur->IsAreaAura() &&
                        holder->GetCasterGuid() != GetObjectGuid())
                        continue;

                    damage[i] = aur->GetModifier()->m_amount;
                    periodicTime[i] = aur->GetModifier()->periodictime;
                    effIndexMask |= (1 << i);
                }
            }

            if (!effIndexMask)
                continue;

            stmt.addUInt32(GetGUIDLow());
            stmt.addUInt64(holder->GetCasterGuid().GetRawValue());
            stmt.addUInt32(holder->GetCastItemGuid().GetCounter());
            stmt.addUInt32(holder->GetId());
            stmt.addUInt32(holder->GetStackAmount());
            stmt.addUInt8(holder->GetAuraCharges());

            for (auto& elem : damage)
                stmt.addInt32(elem);

            for (auto& elem : periodicTime)
                stmt.addUInt32(elem);

            stmt.addInt32(holder->GetAuraMaxDuration());
            stmt.addInt32(holder->GetAuraDuration());
            stmt.addUInt32(effIndexMask);
            stmt.Execute();
        }
    }
}

void Player::_SaveInstanceBinds()
{
    // NOTE: All permanent binds are saved to the database.
    //       A temporary bind is only saved if we're currently inside that
    //       instance.

    static SqlStatementID delete_binds;
    static SqlStatementID save_binds;

    SqlStatement stmt = CharacterDatabase.CreateStatement(
        delete_binds, "DELETE FROM character_instance WHERE guid = ?");
    stmt.PExecute(GetGUIDLow());

    uint32 saved_tmp_id = 0;
    for (auto& elem : m_instanceBinds)
    {
        for (auto& elem_itr : elem)
        {
            auto state = elem_itr.second.state.lock();
            if (!state)
                continue;

            if (!elem_itr.second.perm &&
                state->GetInstanceId() != GetInstanceId())
                continue;

            // Make sure the instance is not deleted from the database; our bind
            // is preserved
            if (!elem_itr.second.perm)
                saved_tmp_id = GetInstanceId();

            stmt = CharacterDatabase.CreateStatement(save_binds,
                "INSERT INTO character_instance (guid, instance, permanent) "
                "VALUES(?, ?, ?)");
            stmt.PExecute(
                GetGUIDLow(), state->GetInstanceId(), elem_itr.second.perm);
        }
    }

    // If we're logging out, unbind us from all instances after we've saved
    // persistent binds
    if (GetSession()->PlayerLogout())
    {
        for (auto& elem : m_instanceBinds)
        {
            for (auto& elem_itr : elem)
            {
                auto state = elem_itr.second.state.lock();
                if (!state)
                    continue;
                // Reset dungeon map if it's empty
                if (Map* map = sMapMgr::Instance()->FindMap(
                        state->GetMapId(), state->GetInstanceId()))
                    if (map->IsDungeon() && !map->HavePlayers())
                        ((DungeonMap*)map)->Reset(INSTANCE_RESET_ALL);
                // Unbind player
                bool db_del = !elem_itr.second.perm &&
                              saved_tmp_id != state->GetInstanceId();
                state->UnbindPlayer(this, db_del); // state can become invalid
            }
            elem.clear();
        }
    }
}

void Player::_SaveInventory()
{
    inventory_.save();
}

void Player::_SaveMail()
{
    static SqlStatementID updateMail;
    static SqlStatementID deleteMailItems;

    static SqlStatementID deleteItem;
    static SqlStatementID deleteItemText;
    static SqlStatementID deleteMain;
    static SqlStatementID deleteItems;

    for (auto m : m_mail)
    {
        if (m->state == MAIL_STATE_CHANGED)
        {
            SqlStatement stmt = CharacterDatabase.CreateStatement(updateMail,
                "UPDATE mail SET itemTextId = ?,has_items = ?, expire_time = "
                "?, deliver_time = ?, money = ?, cod = ?, checked = ? WHERE id "
                "= ?");
            stmt.addUInt32(m->itemTextId);
            stmt.addUInt32(m->HasItems() ? 1 : 0);
            stmt.addUInt64(uint64(m->expire_time));
            stmt.addUInt64(uint64(m->deliver_time));
            stmt.addUInt32(m->money);
            stmt.addUInt32(m->COD);
            stmt.addUInt32(m->checked);
            stmt.addUInt32(m->messageID);
            stmt.Execute();

            if (m->removedItems.size())
            {
                stmt = CharacterDatabase.CreateStatement(deleteMailItems,
                    "DELETE FROM mail_items WHERE item_guid = ?");

                for (std::vector<uint32>::const_iterator itr2 =
                         m->removedItems.begin();
                     itr2 != m->removedItems.end(); ++itr2)
                    stmt.PExecute(*itr2);

                m->removedItems.clear();
            }
            m->state = MAIL_STATE_UNCHANGED;
        }
        else if (m->state == MAIL_STATE_DELETED)
        {
            if (m->HasItems())
            {
                SqlStatement stmt = CharacterDatabase.CreateStatement(
                    deleteItem, "DELETE FROM item_instance WHERE guid = ?");
                for (MailItemInfoVec::const_iterator itr2 = m->items.begin();
                     itr2 != m->items.end(); ++itr2)
                    stmt.PExecute(itr2->item_guid);
            }

            if (m->itemTextId)
            {
                SqlStatement stmt = CharacterDatabase.CreateStatement(
                    deleteItemText, "DELETE FROM item_text WHERE id = ?");
                stmt.PExecute(m->itemTextId);
            }

            SqlStatement stmt = CharacterDatabase.CreateStatement(
                deleteMain, "DELETE FROM mail WHERE id = ?");
            stmt.PExecute(m->messageID);

            stmt = CharacterDatabase.CreateStatement(
                deleteItems, "DELETE FROM mail_items WHERE mail_id = ?");
            stmt.PExecute(m->messageID);
        }
    }

    // deallocate deleted mails...
    for (auto itr = m_mail.begin(); itr != m_mail.end();)
    {
        if ((*itr)->state == MAIL_STATE_DELETED)
        {
            Mail* m = *itr;
            m_mail.erase(itr);
            delete m;
            itr = m_mail.begin();
        }
        else
            ++itr;
    }

    m_mailsUpdated = false;
}

void Player::_SaveQuestStatus()
{
    static SqlStatementID insertQuestStatus;

    static SqlStatementID updateQuestStatus;

    // we don't need transactions here.
    for (auto& elem : mQuestStatus)
    {
        switch (elem.second.uState)
        {
        case QUEST_NEW:
        {
            SqlStatement stmt =
                CharacterDatabase.CreateStatement(insertQuestStatus,
                    "INSERT INTO character_queststatus "
                    "(guid,quest,status,rewarded,explored,timer,mobcount1,"
                    "mobcount2,mobcount3,mobcount4,itemcount1,itemcount2,"
                    "itemcount3,itemcount4) "
                    "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");

            stmt.addUInt32(GetGUIDLow());
            stmt.addUInt32(elem.first);
            stmt.addUInt8(elem.second.m_status);
            stmt.addUInt8(elem.second.m_rewarded);
            stmt.addUInt8(elem.second.m_explored);
            stmt.addUInt64(uint64(elem.second.m_timer / IN_MILLISECONDS +
                                  WorldTimer::time_no_syscall()));
            for (int k = 0; k < QUEST_OBJECTIVES_COUNT; ++k)
                stmt.addUInt32(elem.second.m_creatureOrGOcount[k]);
            for (int k = 0; k < QUEST_OBJECTIVES_COUNT; ++k)
                stmt.addUInt32(elem.second.m_itemcount[k]);
            stmt.Execute();
        }
        break;
        case QUEST_CHANGED:
        {
            SqlStatement stmt =
                CharacterDatabase.CreateStatement(updateQuestStatus,
                    "UPDATE character_queststatus SET status = ?,rewarded = "
                    "?,explored = ?,timer = ?,"
                    "mobcount1 = ?,mobcount2 = ?,mobcount3 = ?,mobcount4 = "
                    "?,itemcount1 = ?,itemcount2 = ?,itemcount3 = ?,itemcount4 "
                    "= ?  WHERE guid = ? AND quest = ?");

            stmt.addUInt8(elem.second.m_status);
            stmt.addUInt8(elem.second.m_rewarded);
            stmt.addUInt8(elem.second.m_explored);
            stmt.addUInt64(uint64(elem.second.m_timer / IN_MILLISECONDS +
                                  WorldTimer::time_no_syscall()));
            for (int k = 0; k < QUEST_OBJECTIVES_COUNT; ++k)
                stmt.addUInt32(elem.second.m_creatureOrGOcount[k]);
            for (int k = 0; k < QUEST_OBJECTIVES_COUNT; ++k)
                stmt.addUInt32(elem.second.m_itemcount[k]);
            stmt.addUInt32(GetGUIDLow());
            stmt.addUInt32(elem.first);
            stmt.Execute();
        }
        break;
        case QUEST_UNCHANGED:
            break;
        };
        elem.second.uState = QUEST_UNCHANGED;
    }
}

void Player::_SaveDailyQuestStatus()
{
    if (!m_DailyQuestChanged)
        return;

    // we don't need transactions here.
    static SqlStatementID delQuestStatus;
    static SqlStatementID insQuestStatus;

    SqlStatement stmtDel = CharacterDatabase.CreateStatement(delQuestStatus,
        "DELETE FROM character_queststatus_daily WHERE guid = ?");
    SqlStatement stmtIns = CharacterDatabase.CreateStatement(insQuestStatus,
        "INSERT INTO character_queststatus_daily (guid,quest) VALUES (?, ?)");

    stmtDel.PExecute(GetGUIDLow());

    for (uint32 quest_daily_idx = 0; quest_daily_idx < PLAYER_MAX_DAILY_QUESTS;
         ++quest_daily_idx)
        if (GetUInt32Value(PLAYER_FIELD_DAILY_QUESTS_1 + quest_daily_idx))
            stmtIns.PExecute(GetGUIDLow(),
                GetUInt32Value(PLAYER_FIELD_DAILY_QUESTS_1 + quest_daily_idx));

    m_DailyQuestChanged = false;
}

void Player::_LoadRecentDungeons(QueryResult* result)
{
    if (result)
    {
        do
        {
            Field* fields = result->Fetch();
            uint32 map = fields[0].GetUInt32();
            uint32 instance = fields[1].GetUInt32();
            uint32 timestamp = fields[2].GetUInt32();

            RecentDungeon rd = {map, instance, timestamp};
            m_recentDungeons.push_back(rd);
        } while (result->NextRow());
        delete result;
    }
}

void Player::_LoadPetStore(SqlQueryHolder* holder)
{
    auto res = holder->GetResult(PLAYER_LOGIN_QUERY_PETS);
    if (!res)
        return;

    // Read all auras
    std::map<uint32, std::vector<PetDbData::Aura>> aura_map;
    auto aura_res = holder->GetResult(PLAYER_LOGIN_QUERY_PET_AURAS);
    if (aura_res)
    {
        do
        {
            Field* fields = aura_res->Fetch();
            uint32 pet_guid = fields[0].GetUInt32();

            PetDbData::Aura aura;
            aura.caster_guid = fields[1].GetUInt64();
            aura.item_guid = fields[2].GetUInt32();
            aura.spell_id = fields[3].GetUInt32();
            aura.stacks = fields[4].GetUInt32();
            aura.charges = fields[5].GetUInt32();
            aura.bp[0] = fields[6].GetUInt32();
            aura.bp[1] = fields[7].GetUInt32();
            aura.bp[2] = fields[8].GetUInt32();
            aura.periodic_time[0] = fields[9].GetUInt32();
            aura.periodic_time[1] = fields[10].GetUInt32();
            aura.periodic_time[2] = fields[11].GetUInt32();
            aura.max_duration = fields[12].GetUInt32();
            aura.duration = fields[13].GetUInt32();
            aura.eff_mask = fields[14].GetUInt32();

            aura_map[pet_guid].push_back(std::move(aura));
        } while (aura_res->NextRow());
        delete aura_res;
    }

    // Read all spells
    std::map<uint32, std::vector<PetDbData::Spell>> spell_map;
    auto spell_res = holder->GetResult(PLAYER_LOGIN_QUERY_PET_SPELLS);
    if (spell_res)
    {
        do
        {
            Field* fields = spell_res->Fetch();
            uint32 pet_guid = fields[0].GetUInt32();

            PetDbData::Spell spell;
            spell.id = fields[1].GetUInt32();
            spell.active = ActiveStates(fields[2].GetUInt32());

            spell_map[pet_guid].push_back(std::move(spell));
        } while (spell_res->NextRow());
        delete spell_res;
    }

    // Read all spell cooldowns
    std::map<uint32, std::vector<PetDbData::SpellCooldown>> spell_cd_map;
    auto cd_res = holder->GetResult(PLAYER_LOGIN_QUERY_PET_SPELL_COOLDOWNS);
    if (cd_res)
    {
        do
        {
            Field* fields = cd_res->Fetch();
            uint32 pet_guid = fields[0].GetUInt32();

            PetDbData::SpellCooldown cd;
            cd.id = fields[1].GetUInt32();
            cd.time = fields[2].GetUInt32();

            spell_cd_map[pet_guid].push_back(std::move(cd));
        } while (cd_res->NextRow());
        delete cd_res;
    }

    // Read all declined pet names
    std::map<uint32, DeclinedName> declined_map;
    auto declined_res = holder->GetResult(PLAYER_LOGIN_QUERY_PET_DECLINED_NAME);
    if (declined_res)
    {
        do
        {
            Field* fields = declined_res->Fetch();
            uint32 pet_guid = fields[0].GetUInt32();

            DeclinedName name;
            for (int i = 0; i < 5; ++i)
                name.name[i] = fields[i + 1].GetCppString();

            declined_map[pet_guid] = std::move(name);
        } while (declined_res->NextRow());
        delete declined_res;
    }

    // Read all pets
    do
    {
        Field* fields = res->Fetch();

        PetDbData data;
        data.guid = fields[0].GetUInt32();
        data.id = fields[1].GetUInt32();
        data.owner_guid = fields[2].GetUInt32();
        data.model_id = fields[3].GetUInt32();
        data.create_spell = fields[4].GetUInt32();
        data.pet_type = fields[5].GetUInt8();
        data.level = fields[6].GetUInt32();
        data.exp = fields[7].GetUInt32();
        data.react_state = fields[8].GetUInt32();
        data.loyalty_points = fields[9].GetInt32();
        data.loyalty = fields[10].GetUInt32();
        data.training_points = fields[11].GetInt32();
        data.name = fields[12].GetCppString();
        data.renamed = fields[13].GetBool();
        data.slot = fields[14].GetUInt32();
        data.health = fields[15].GetUInt32();
        data.mana = fields[16].GetUInt32();
        data.happiness = fields[17].GetUInt32();
        data.save_time = fields[18].GetUInt64();
        data.reset_talents_cost = fields[19].GetUInt32();
        data.reset_talents_time = fields[20].GetUInt32();
        data.action_bar_raw = fields[21].GetCppString();
        data.teach_spells_raw = fields[22].GetCppString();
        data.dead = fields[23].GetBool();

        data.auras = aura_map[data.guid];
        data.spells = spell_map[data.guid];
        data.spell_cooldowns = spell_cd_map[data.guid];
        data.declined_name = declined_map[data.guid];

        _pet_store.push_back(std::move(data));
    } while (res->NextRow());

    delete res;
}

void Player::_SaveRecentDungeons()
{
    static SqlStatementID delDungeons;
    static SqlStatementID insDungeons;

    // Delete the existing dungeons
    SqlStatement stmtDel = CharacterDatabase.CreateStatement(
        delDungeons, "DELETE FROM character_recent_dungeons WHERE guid = ?");
    SqlStatement stmtIns = CharacterDatabase.CreateStatement(insDungeons,
        "INSERT INTO character_recent_dungeons (guid, map, instance,timestamp) "
        "VALUES (?, ?, ?, ?)");

    stmtDel.PExecute(GetGUIDLow());
    for (auto& elem : m_recentDungeons)
        stmtIns.PExecute(GetGUIDLow(), elem.map, elem.instance, elem.timestamp);
}

void Player::_SaveSkills()
{
    static SqlStatementID delSkills;
    static SqlStatementID insSkills;
    static SqlStatementID updSkills;

    // we don't need transactions here.
    for (auto itr = mSkillStatus.begin(); itr != mSkillStatus.end();)
    {
        if (itr->second.uState == SKILL_UNCHANGED)
        {
            ++itr;
            continue;
        }

        if (itr->second.uState == SKILL_DELETED)
        {
            SqlStatement stmt = CharacterDatabase.CreateStatement(delSkills,
                "DELETE FROM character_skills WHERE guid = ? AND skill = ?");
            stmt.PExecute(GetGUIDLow(), itr->first);
            mSkillStatus.erase(itr++);
            continue;
        }

        uint32 valueData =
            GetUInt32Value(PLAYER_SKILL_VALUE_INDEX(itr->second.pos));
        uint16 value = SKILL_VALUE(valueData);
        uint16 max = SKILL_MAX(valueData);

        switch (itr->second.uState)
        {
        case SKILL_NEW:
        {
            SqlStatement stmt = CharacterDatabase.CreateStatement(insSkills,
                "INSERT INTO character_skills (guid, skill, value, max) VALUES "
                "(?, ?, ?, ?)");
            stmt.PExecute(GetGUIDLow(), itr->first, value, max);
        }
        break;
        case SKILL_CHANGED:
        {
            SqlStatement stmt = CharacterDatabase.CreateStatement(updSkills,
                "UPDATE character_skills SET value = ?, max = ? WHERE guid = ? "
                "AND skill = ?");
            stmt.PExecute(value, max, GetGUIDLow(), itr->first);
        }
        break;
        case SKILL_UNCHANGED:
        case SKILL_DELETED:
            assert(false);
            break;
        };
        itr->second.uState = SKILL_UNCHANGED;

        ++itr;
    }
}

void Player::_SaveSpells()
{
    static SqlStatementID delSpells;
    static SqlStatementID insSpells;

    SqlStatement stmtDel = CharacterDatabase.CreateStatement(
        delSpells, "DELETE FROM character_spell WHERE guid = ? and spell = ?");
    SqlStatement stmtIns = CharacterDatabase.CreateStatement(insSpells,
        "INSERT INTO character_spell (guid,spell,active,disabled) VALUES (?, "
        "?, ?, ?)");

    for (auto itr = m_spells.begin(); itr != m_spells.end();)
    {
        if (itr->second.state == PLAYERSPELL_REMOVED ||
            itr->second.state == PLAYERSPELL_CHANGED)
            stmtDel.PExecute(GetGUIDLow(), itr->first);

        // add only changed/new not dependent spells
        if (!itr->second.dependent &&
            (itr->second.state == PLAYERSPELL_NEW ||
                itr->second.state == PLAYERSPELL_CHANGED))
            stmtIns.PExecute(GetGUIDLow(), itr->first,
                uint8(itr->second.active ? 1 : 0),
                uint8(itr->second.disabled ? 1 : 0));

        if (itr->second.state == PLAYERSPELL_REMOVED)
            itr = m_spells.erase(itr);
        else
        {
            itr->second.state = PLAYERSPELL_UNCHANGED;
            ++itr;
        }
    }
}

// save player stats -- only for external usage
// real stats will be recalculated on player login
void Player::_SaveStats()
{
    // check if stat saving is enabled and if char level is high enough
    if (!sWorld::Instance()->getConfig(CONFIG_UINT32_MIN_LEVEL_STAT_SAVE) ||
        getLevel() <
            sWorld::Instance()->getConfig(CONFIG_UINT32_MIN_LEVEL_STAT_SAVE))
        return;

    static SqlStatementID delStats;
    static SqlStatementID insertStats;

    SqlStatement stmt = CharacterDatabase.CreateStatement(
        delStats, "DELETE FROM character_stats WHERE guid = ?");
    stmt.PExecute(GetGUIDLow());

    stmt = CharacterDatabase.CreateStatement(insertStats,
        "INSERT INTO character_stats (guid, maxhealth, maxpower1, maxpower2, "
        "maxpower3, maxpower4, maxpower5, "
        "strength, agility, stamina, intellect, spirit, armor, resHoly, "
        "resFire, resNature, resFrost, resShadow, resArcane, "
        "blockPct, dodgePct, parryPct, critPct, rangedCritPct, spellCritPct, "
        "attackPower, rangedAttackPower, spellPower) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, "
        "?, ?, ?, ?, ?, ?, ?, ?)");

    stmt.addUInt32(GetGUIDLow());
    stmt.addUInt32(GetMaxHealth());
    for (int i = 0; i < MAX_POWERS; ++i)
        stmt.addUInt32(GetMaxPower(Powers(i)));
    for (int i = 0; i < MAX_STATS; ++i)
        stmt.addFloat(GetStat(Stats(i)));
    // armor + school resistances
    for (int i = 0; i < MAX_SPELL_SCHOOL; ++i)
        stmt.addUInt32(GetResistance(SpellSchools(i)));
    stmt.addFloat(GetFloatValue(PLAYER_BLOCK_PERCENTAGE));
    stmt.addFloat(GetFloatValue(PLAYER_DODGE_PERCENTAGE));
    stmt.addFloat(GetFloatValue(PLAYER_PARRY_PERCENTAGE));
    stmt.addFloat(GetFloatValue(PLAYER_CRIT_PERCENTAGE));
    stmt.addFloat(GetFloatValue(PLAYER_RANGED_CRIT_PERCENTAGE));
    stmt.addFloat(GetFloatValue(PLAYER_SPELL_CRIT_PERCENTAGE1));
    stmt.addUInt32(GetUInt32Value(UNIT_FIELD_ATTACK_POWER));
    stmt.addUInt32(GetUInt32Value(UNIT_FIELD_RANGED_ATTACK_POWER));
    stmt.addUInt32(GetUInt32Value(PLAYER_FIELD_MOD_HEALING_DONE_POS));

    stmt.Execute();
}

void Player::outDebugStatsValues() const
{
    // optimize disabled debug output
    if (logging.get_logger().get_level() == LogLevel::debug)
        return;

    LOG_DEBUG(logging, "HP is: \t\t\t%u\t\tMP is: \t\t\t%u", GetMaxHealth(),
        GetMaxPower(POWER_MANA));
    LOG_DEBUG(logging, "AGILITY is: \t\t%f\t\tSTRENGTH is: \t\t%f",
        GetStat(STAT_AGILITY), GetStat(STAT_STRENGTH));
    LOG_DEBUG(logging, "INTELLECT is: \t\t%f\t\tSPIRIT is: \t\t%f",
        GetStat(STAT_INTELLECT), GetStat(STAT_SPIRIT));
    LOG_DEBUG(logging, "STAMINA is: \t\t%f", GetStat(STAT_STAMINA));
    LOG_DEBUG(logging, "Armor is: \t\t%u\t\tBlock is: \t\t%f", GetArmor(),
        GetFloatValue(PLAYER_BLOCK_PERCENTAGE));
    LOG_DEBUG(logging, "HolyRes is: \t\t%u\t\tFireRes is: \t\t%u",
        GetResistance(SPELL_SCHOOL_HOLY), GetResistance(SPELL_SCHOOL_FIRE));
    LOG_DEBUG(logging, "NatureRes is: \t\t%u\t\tFrostRes is: \t\t%u",
        GetResistance(SPELL_SCHOOL_NATURE), GetResistance(SPELL_SCHOOL_FROST));
    LOG_DEBUG(logging, "ShadowRes is: \t\t%u\t\tArcaneRes is: \t\t%u",
        GetResistance(SPELL_SCHOOL_SHADOW), GetResistance(SPELL_SCHOOL_ARCANE));
    LOG_DEBUG(logging, "MIN_DAMAGE is: \t\t%f\tMAX_DAMAGE is: \t\t%f",
        GetFloatValue(UNIT_FIELD_MINDAMAGE),
        GetFloatValue(UNIT_FIELD_MAXDAMAGE));
    LOG_DEBUG(logging,
        "MIN_OFFHAND_DAMAGE is: \t%f\tMAX_OFFHAND_DAMAGE is: \t%f",
        GetFloatValue(UNIT_FIELD_MINOFFHANDDAMAGE),
        GetFloatValue(UNIT_FIELD_MAXOFFHANDDAMAGE));
    LOG_DEBUG(logging, "MIN_RANGED_DAMAGE is: \t%f\tMAX_RANGED_DAMAGE is: \t%f",
        GetFloatValue(UNIT_FIELD_MINRANGEDDAMAGE),
        GetFloatValue(UNIT_FIELD_MAXRANGEDDAMAGE));
    LOG_DEBUG(logging, "ATTACK_TIME is: \t%u\t\tRANGE_ATTACK_TIME is: \t%u",
        GetAttackTime(BASE_ATTACK), GetAttackTime(RANGED_ATTACK));
}

/*********************************************************/
/***               FLOOD FILTER SYSTEM                 ***/
/*********************************************************/

void Player::UpdateSpeakTime()
{
    // ignore chat spam protection for GMs in any mode
    if (GetSession()->GetSecurity() > SEC_PLAYER)
        return;

    time_t current = time(nullptr);
    if (m_speakTime > current)
    {
        uint32 max_count = sWorld::Instance()->getConfig(
            CONFIG_UINT32_CHATFLOOD_MESSAGE_COUNT);
        if (!max_count)
            return;

        ++m_speakCount;
        if (m_speakCount >= max_count)
        {
            // prevent overwrite mute time, if message send just before mutes
            // set, for example.
            time_t new_mute = current +
                              sWorld::Instance()->getConfig(
                                  CONFIG_UINT32_CHATFLOOD_MUTE_TIME);
            if (GetSession()->m_muteTime < new_mute)
                GetSession()->m_muteTime = new_mute;

            m_speakCount = 0;
        }
    }
    else
        m_speakCount = 0;

    m_speakTime =
        current +
        sWorld::Instance()->getConfig(CONFIG_UINT32_CHATFLOOD_MESSAGE_DELAY);
}

bool Player::CanSpeak() const
{
    return GetSession()->m_muteTime <= time(nullptr);
}

/*********************************************************/
/***              LOW LEVEL FUNCTIONS:Notifiers        ***/
/*********************************************************/

void Player::SendAttackSwingNotInRange()
{
    WorldPacket data(SMSG_ATTACKSWING_NOTINRANGE, 0);
    GetSession()->send_packet(std::move(data));
}

void Player::SavePositionInDB(ObjectGuid guid, uint32 mapid, float x, float y,
    float z, float o, uint32 zone)
{
    std::ostringstream ss;
    ss << "UPDATE characters SET position_x='" << x << "',position_y='" << y
       << "',position_z='" << z << "',orientation='" << o << "',map='" << mapid
       << "',zone='" << zone << "',trans_x='0',trans_y='0',trans_z='0',"
       << "transguid='0',taxi_path='' WHERE guid='" << guid.GetCounter() << "'";
    LOG_DEBUG(logging, "%s", ss.str().c_str());
    CharacterDatabase.Execute(ss.str().c_str());
}

void Player::SetUInt32ValueInArray(Tokens& tokens, uint16 index, uint32 value)
{
    char buf[11];
    snprintf(buf, 11, "%u", value);

    if (index >= tokens.size())
        return;

    tokens[index] = buf;
}

void Player::SendAttackSwingNotStanding()
{
    WorldPacket data(SMSG_ATTACKSWING_NOTSTANDING, 0);
    GetSession()->send_packet(std::move(data));
}

void Player::SendAttackSwingDeadTarget()
{
    WorldPacket data(SMSG_ATTACKSWING_DEADTARGET, 0);
    GetSession()->send_packet(std::move(data));
}

void Player::SendAttackSwingCantAttack()
{
    WorldPacket data(SMSG_ATTACKSWING_CANT_ATTACK, 0);
    GetSession()->send_packet(std::move(data));
}

void Player::SendAttackSwingCancelAttack()
{
    WorldPacket data(SMSG_CANCEL_COMBAT, 0);
    GetSession()->send_packet(std::move(data));
}

void Player::SendAttackSwingBadFacingAttack()
{
    WorldPacket data(SMSG_ATTACKSWING_BADFACING, 0);
    GetSession()->send_packet(std::move(data));
}

void Player::SendAutoRepeatCancel()
{
    WorldPacket data(SMSG_CANCEL_AUTO_REPEAT, 0);
    GetSession()->send_packet(std::move(data));
}

void Player::SendExplorationExperience(uint32 Area, uint32 Experience)
{
    WorldPacket data(SMSG_EXPLORATION_EXPERIENCE, 8);
    data << uint32(Area);
    data << uint32(Experience);
    GetSession()->send_packet(std::move(data));
}

void Player::SendDungeonDifficulty(bool IsInGroup)
{
    uint8 val = 0x00000001;
    WorldPacket data(MSG_SET_DUNGEON_DIFFICULTY, 12);
    data << (uint32)GetDifficulty();
    data << uint32(val);
    data << uint32(IsInGroup);
    GetSession()->send_packet(std::move(data));
}

void Player::SendResetFailedNotify(uint32 mapid)
{
    WorldPacket data(SMSG_RESET_FAILED_NOTIFY, 4);
    data << uint32(mapid);
    GetSession()->send_packet(std::move(data));
}

/// Reset all solo instances and optionally send a message on success for each
void Player::ResetInstances(InstanceResetMethod method)
{
    // method can be INSTANCE_RESET_ALL, INSTANCE_RESET_CHANGE_DIFFICULTY

    for (int d = DUNGEON_DIFFICULTY_NORMAL; d <= DUNGEON_DIFFICULTY_HEROIC; d++)
    {
        Difficulty diff = static_cast<Difficulty>(d);

        for (auto itr = m_instanceBinds[diff].begin();
             itr != m_instanceBinds[diff].end();)
        {
            auto state = itr->second.state.lock();
            if (!state)
            {
                itr = m_instanceBinds[diff].erase(itr);
                continue;
            }

            const MapEntry* entry = sMapStore.LookupEntry(itr->first);
            if (!entry || !state->CanReset())
            {
                ++itr;
                continue;
            }

            if (method == INSTANCE_RESET_ALL)
            {
                // the "reset all instances" method can only reset normal maps
                if (entry->map_type == MAP_RAID ||
                    diff == DUNGEON_DIFFICULTY_HEROIC)
                {
                    ++itr;
                    continue;
                }
            }

            // if the map is loaded, reset it
            bool isReset = false;
            if (Map* map = sMapMgr::Instance()->FindMap(
                    state->GetMapId(), state->GetInstanceId()))
                if (map->IsDungeon())
                    isReset = ((DungeonMap*)map)->Reset(method);

            if (method == INSTANCE_RESET_ALL ||
                method == INSTANCE_RESET_CHANGE_DIFFICULTY)
            {
                if (isReset)
                    SendResetInstanceSuccess(state->GetMapId());
                else
                    SendResetInstanceFailed(
                        INSTANCE_RESET_FAIL_INSIDE, state->GetMapId());
            }

            state->UnbindPlayer(this, true); // state can be deleted by this
            itr = m_instanceBinds[diff].erase(itr);
        }
    }
}

void Player::SendResetInstanceSuccess(uint32 MapId)
{
    WorldPacket data(SMSG_INSTANCE_RESET, 4);
    data << uint32(MapId);
    GetSession()->send_packet(std::move(data));
}

void Player::SendResetInstanceFailed(InstanceResetFailMsg reason, uint32 MapId)
{
    // TODO: find what other fail reasons there are besides players in the
    // instance
    WorldPacket data(SMSG_INSTANCE_RESET_FAILED, 4);
    data << uint32(reason);
    data << uint32(MapId);
    GetSession()->send_packet(std::move(data));
}

/*********************************************************/
/***              Update timers                        ***/
/*********************************************************/

/// checks the 15 afk reports per 5 minutes limit
void Player::UpdateAfkReport(time_t currTime)
{
    if (m_bgData.bgAfkReportedTimer <= currTime)
    {
        m_bgData.bgAfkReportedCount = 0;
        m_bgData.bgAfkReportedTimer = currTime + 5 * MINUTE;
    }
}

void Player::UpdateContestedPvP(uint32 diff)
{
    if (!m_contestedPvPTimer || isInCombat())
        return;
    if (m_contestedPvPTimer <= diff)
    {
        ResetContestedPvP();
    }
    else
        m_contestedPvPTimer -= diff;
}

void Player::UpdatePvPFlag(time_t currTime)
{
    if (!IsPvP())
        return;
    if (pvpInfo.endTimer == 0 || currTime < (pvpInfo.endTimer + 300))
        return;

    UpdatePvP(false);
}

void Player::SetFFAPvP(bool state)
{
    if (state)
        SetFlag(PLAYER_FLAGS, PLAYER_FLAGS_FFA_PVP);
    else
        RemoveFlag(PLAYER_FLAGS, PLAYER_FLAGS_FFA_PVP);
}

void Player::UpdateDuelFlag(time_t currTime)
{
    if (!duel || duel->startTimer == 0 || currTime < duel->startTimer + 3)
        return;

    SetUInt32Value(PLAYER_DUEL_TEAM, 1);
    duel->opponent->SetUInt32Value(PLAYER_DUEL_TEAM, 2);

    duel->startTimer = 0;
    duel->startTime = currTime;
    duel->opponent->duel->startTimer = 0;
    duel->opponent->duel->startTime = currTime;
}

void Player::RemovePet(PetSaveMode mode)
{
    if (Pet* pet = GetPet())
        pet->Unsummon(mode, this);
}

void Player::RemoveMiniPet()
{
    if (Pet* pet = GetMiniPet())
        pet->Unsummon(PET_SAVE_AS_DELETED);
}

Pet* Player::GetMiniPet() const
{
    if (m_miniPetGuid.IsEmpty())
        return nullptr;

    return GetMap()->GetPet(m_miniPetGuid);
}

void Player::BuildPlayerChat(WorldPacket* data, uint8 msgtype,
    const std::string& text, uint32 language) const
{
    *data << uint8(msgtype);
    *data << uint32(language);
    *data << ObjectGuid(GetObjectGuid());
    *data << uint32(language); // language 2.1.0 ?
    *data << ObjectGuid(GetObjectGuid());
    *data << uint32(text.length() + 1);
    *data << text;
    *data << uint8(chatTag());
}

void Player::Say(const std::string& text, const uint32 language)
{
    if (!GetMap()->IsBattleArena())
    {
        // Normal handling
        WorldPacket data(SMSG_MESSAGECHAT, 200);
        BuildPlayerChat(&data, CHAT_MSG_SAY, text, language);
        SendMessageToSetInRange(&data,
            sWorld::Instance()->getConfig(CONFIG_FLOAT_LISTEN_RANGE_SAY), true);
    }
    else
    {
        // Arena handling
        ArenaChat(text, CHAT_MSG_SAY, language,
            sWorld::Instance()->getConfig(CONFIG_FLOAT_LISTEN_RANGE_SAY));
    }
}

void Player::Yell(const std::string& text, const uint32 language)
{
    if (!GetMap()->IsBattleArena())
    {
        // Normal handling
        WorldPacket data(SMSG_MESSAGECHAT, 200);
        BuildPlayerChat(&data, CHAT_MSG_YELL, text, language);
        SendMessageToSetInRange(&data,
            sWorld::Instance()->getConfig(CONFIG_FLOAT_LISTEN_RANGE_YELL),
            true);
    }
    else
    {
        // Arena handling
        ArenaChat(text, CHAT_MSG_YELL, language,
            sWorld::Instance()->getConfig(CONFIG_FLOAT_LISTEN_RANGE_YELL));
    }
}

void Player::TextEmote(const std::string& text)
{
    if (!GetMap()->IsBattleArena())
    {
        // Normal handling
        WorldPacket data(SMSG_MESSAGECHAT, 200);
        BuildPlayerChat(&data, CHAT_MSG_EMOTE, text, LANG_UNIVERSAL);
        GetMap()->broadcast_message(this, &data, true, false);
    }
    else
    {
        // Arena handling
        ArenaChat(text, CHAT_MSG_EMOTE, LANG_UNIVERSAL,
            sWorld::Instance()->getConfig(CONFIG_FLOAT_LISTEN_RANGE_TEXTEMOTE));
    }
}

// Internal function to handle Say(), Yell() or TextEmote() while player is in
// an arena
void Player::ArenaChat(
    const std::string& text, uint32 channel, uint32 language, float range)
{
    if (!IsInWorld())
        return;

    auto all = maps::visitors::yield_set<Player>{}(this, range, [](Player* p)
        {
            return p->isAlive();
        });

    std::vector<Player*> same_team, opposing_team;
    for (auto p : all)
    {
        if (GetBGTeam() == p->GetBGTeam())
            same_team.push_back(p);
        else
            opposing_team.push_back(p);
    }

    // send to teammates
    WorldPacket data_one(SMSG_MESSAGECHAT, 200);
    BuildPlayerChat(&data_one, channel, text, language);
    for (auto p : same_team)
        if (WorldSession* session = p->GetSession())
            session->send_packet(&data_one);

    // text emotes are not sent to enemies
    if (channel == CHAT_MSG_EMOTE)
        return;

    // send say and yell as generic emotes to enemies
    std::stringstream ss;
    if (channel == CHAT_MSG_SAY)
        ss << "says something unintelligible.";
    else
        ss << "yells at " << (getGender() == GENDER_MALE ? "his" : "her")
           << " teammates.";

    WorldPacket data_two(SMSG_MESSAGECHAT, 200);
    BuildPlayerChat(&data_two, CHAT_MSG_EMOTE, ss.str(), LANG_UNIVERSAL);
    for (auto p : opposing_team)
    {
        if (!HaveAtClient(p))
            continue;
        if (WorldSession* session = p->GetSession())
            session->send_packet(&data_two);
    }
}

void Player::Whisper(const std::string& text, uint32 language, Player* receiver)
{
    if (language != LANG_ADDON)    // if not addon data
        language = LANG_UNIVERSAL; // whispers should always be readable

    WorldPacket data(SMSG_MESSAGECHAT, 200);
    BuildPlayerChat(&data, CHAT_MSG_WHISPER, text, language);
    receiver->GetSession()->send_packet(std::move(data));

    // not send confirmation for addon messages
    if (language != LANG_ADDON)
    {
        data.initialize(SMSG_MESSAGECHAT, 200);
        receiver->BuildPlayerChat(&data, CHAT_MSG_REPLY, text, language);
        GetSession()->send_packet(std::move(data));
    }

    // announce afk or dnd message
    if (receiver->isAFK())
        ChatHandler(this).PSendSysMessage(LANG_PLAYER_AFK, receiver->GetName(),
            receiver->autoReplyMsg.c_str());
    else if (receiver->isDND())
        ChatHandler(this).PSendSysMessage(LANG_PLAYER_DND, receiver->GetName(),
            receiver->autoReplyMsg.c_str());
}

void Player::PetSpellInitialize()
{
    Creature* pet = GetPet();
    bool is_pet = true;

    if (!pet)
    {
        Unit* tmp = GetCharm();
        if (!tmp || tmp->GetTypeId() != TYPEID_UNIT)
            return;
        pet = static_cast<Creature*>(tmp);
        is_pet = false;
    }

    if (!pet || !pet->GetCharmInfo())
        return;

    LOG_DEBUG(logging, "Pet Spells Groups");

    CharmInfo* charmInfo = pet->GetCharmInfo();
    if (!charmInfo)
        return;

    WorldPacket data(SMSG_PET_SPELLS,
        8 + 4 + 1 + 1 + 2 + 4 * MAX_UNIT_ACTION_BAR_INDEX + 1 + 1);
    data << pet->GetObjectGuid();
    data << uint32(0);
    data << uint8(charmInfo->GetReactState())
         << uint8(charmInfo->GetCommandState()) << uint16(0);

    // action bar loop
    charmInfo->BuildActionBar(&data);

    size_t spellsCountPos = data.wpos();

    // spells count
    uint8 addlist = 0;
    data << uint8(addlist); // placeholder

    if (is_pet && (static_cast<Pet*>(pet)->IsPermanentPetFor(this) ||
                      pet->GetEntry() == 510)) // include water elemental
    {
        // spells loop
        for (PetSpellMap::const_iterator itr =
                 static_cast<Pet*>(pet)->m_spells.begin();
             itr != static_cast<Pet*>(pet)->m_spells.end(); ++itr)
        {
            if (itr->second.state == PETSPELL_REMOVED)
                continue;

            data << uint32(
                MAKE_UNIT_ACTION_BUTTON(itr->first, itr->second.active));
            ++addlist;
        }
    }

    data.put<uint8>(spellsCountPos, addlist);

    uint8 cooldownsCount = pet->m_CreatureSpellCooldowns.size() +
                           pet->m_CreatureCategoryCooldowns.size();
    data << uint8(cooldownsCount);

    time_t curTime = WorldTimer::time_no_syscall();

    for (CreatureSpellCooldowns::const_iterator itr =
             pet->m_CreatureSpellCooldowns.begin();
         itr != pet->m_CreatureSpellCooldowns.end(); ++itr)
    {
        time_t cooldown = (itr->second > curTime) ?
                              (itr->second - curTime) * IN_MILLISECONDS :
                              0;

        data << uint16(itr->first); // spellid
        data << uint16(0);          // spell category?
        data << uint32(cooldown);   // cooldown
        data << uint32(0);          // category cooldown
    }

    for (CreatureSpellCooldowns::const_iterator itr =
             pet->m_CreatureCategoryCooldowns.begin();
         itr != pet->m_CreatureCategoryCooldowns.end(); ++itr)
    {
        time_t cooldown = (itr->second > curTime) ?
                              (itr->second - curTime) * IN_MILLISECONDS :
                              0;

        data << uint16(itr->first); // spellid
        data << uint16(0);          // spell category?
        data << uint32(0);          // cooldown
        data << uint32(cooldown);   // category cooldown
    }

    GetSession()->send_packet(std::move(data));
}

void Player::PossessSpellInitialize()
{
    Unit* charm = GetCharm();

    if (!charm)
        return;

    CharmInfo* charmInfo = charm->GetCharmInfo();

    if (!charmInfo)
    {
        logging.error(
            "Player::PossessSpellInitialize(): charm (GUID: %u TypeId: %u) has "
            "no charminfo!",
            charm->GetGUIDLow(), charm->GetTypeId());
        return;
    }

    WorldPacket data(
        SMSG_PET_SPELLS, 8 + 4 + 4 + 4 * MAX_UNIT_ACTION_BAR_INDEX + 1 + 1);
    data << charm->GetObjectGuid();
    data << uint32(0);
    data << uint32(0);

    charmInfo->BuildActionBar(&data);

    data << uint8(0); // spells count
    data << uint8(0); // cooldowns count

    GetSession()->send_packet(std::move(data));
}

void Player::CharmSpellInitialize()
{
    Unit* charm = GetCharm();

    if (!charm)
        return;

    CharmInfo* charmInfo = charm->GetCharmInfo();
    if (!charmInfo)
    {
        logging.error(
            "Player::CharmSpellInitialize(): the player's charm (GUID: %u "
            "TypeId: %u) has no charminfo!",
            charm->GetGUIDLow(), charm->GetTypeId());
        return;
    }

    uint8 addlist = 0;

    if (charm->GetTypeId() != TYPEID_PLAYER)
    {
        CreatureInfo const* cinfo = ((Creature*)charm)->GetCreatureInfo();

        if (cinfo && cinfo->type == CREATURE_TYPE_DEMON &&
            getClass() == CLASS_WARLOCK)
        {
            for (uint32 i = 0; i < CREATURE_MAX_SPELLS; ++i)
            {
                if (charmInfo->GetCharmSpell(i)->GetAction())
                    ++addlist;
            }
        }
    }

    WorldPacket data(SMSG_PET_SPELLS, 8 + 4 + 1 + 1 + 2 +
                                          4 * MAX_UNIT_ACTION_BAR_INDEX + 1 +
                                          4 * addlist + 1);
    data << charm->GetObjectGuid();
    data << uint32(0x00000000);

    if (charm->GetTypeId() != TYPEID_PLAYER)
        data << uint8(charmInfo->GetReactState())
             << uint8(charmInfo->GetCommandState()) << uint16(0);
    else
        data << uint8(0) << uint8(0) << uint16(0);

    charmInfo->BuildActionBar(&data);

    data << uint8(addlist);

    if (addlist)
    {
        for (uint32 i = 0; i < CREATURE_MAX_SPELLS; ++i)
        {
            CharmSpellEntry* cspell = charmInfo->GetCharmSpell(i);
            if (cspell->GetAction())
                data << uint32(cspell->packedData);
        }
    }

    data << uint8(0); // cooldowns count

    GetSession()->send_packet(std::move(data));
}

void Player::RemovePetActionBar()
{
    WorldPacket data(SMSG_PET_SPELLS, 8);
    data << ObjectGuid();
    SendDirectMessage(std::move(data));
}

bool Player::IsAffectedBySpellmod(
    SpellEntry const* spellInfo, SpellModifier* mod, Spell const* spell)
{
    if (!mod || !spellInfo)
        return false;

    // Modifiers applied after spell was casted do not affect spell
    if (spell && spell->casted_timestamp() > 0 &&
        spell->casted_timestamp() < mod->timestamp)
        return false;

    if (mod->charges == -1 && mod->lastAffected) // marked as expired but locked
                                                 // until spell casting finish
    {
        // prevent apply to any spell except spell that trigger expire
        if (spell)
        {
            if (mod->lastAffected != spell)
                return false;
        }
        else if (mod->lastAffected != FindCurrentSpellBySpellId(spellInfo->Id))
            return false;
    }

    return mod->isAffectedOnSpell(spellInfo);
}

void Player::AddSpellMod(SpellModifier** mod, bool apply)
{
    uint16 Opcode = ((*mod)->type == SPELLMOD_FLAT) ?
                        SMSG_SET_FLAT_SPELL_MODIFIER :
                        SMSG_SET_PCT_SPELL_MODIFIER;

    for (int eff = 0; eff < 64; ++eff)
    {
        uint64 _mask = uint64(1) << eff;
        if ((*mod)->mask.IsFitToFamilyMask(_mask))
        {
            int32 val = 0;
            for (SpellModList::const_iterator itr =
                     m_spellMods[(*mod)->op].begin();
                 itr != m_spellMods[(*mod)->op].end(); ++itr)
            {
                if ((*itr)->type == (*mod)->type &&
                    ((*itr)->mask.IsFitToFamilyMask(_mask)))
                    val += (*itr)->value;
            }
            val += apply ? (*mod)->value : -((*mod)->value);
            WorldPacket data(Opcode, (1 + 1 + 4));
            data << uint8(eff);
            data << uint8((*mod)->op);
            data << int32(val);
            SendDirectMessage(std::move(data));
        }
    }

    if (apply)
        m_spellMods[(*mod)->op].push_back(*mod);
    else
    {
        if ((*mod)->charges == -1)
            --m_SpellModRemoveCount;
        m_spellMods[(*mod)->op].remove(*mod);
        delete *mod;
        *mod = nullptr;
    }
}

SpellModifier* Player::GetSpellMod(SpellModOp op, uint32 spellId) const
{
    for (const auto& elem : m_spellMods[op])
        if ((elem)->spellId == spellId)
            return elem;

    return nullptr;
}

void Player::RemoveSpellMods(Spell const* spell)
{
    if (!spell || (m_SpellModRemoveCount == 0))
        return;

    for (auto& elem : m_spellMods)
    {
        for (SpellModList::const_iterator itr = elem.begin();
             itr != elem.end();)
        {
            SpellModifier* mod = *itr;
            ++itr;

            if (mod && mod->charges == -1 &&
                (mod->lastAffected == spell || mod->lastAffected == nullptr))
            {
                remove_auras(mod->spellId);
                if (elem.empty())
                    break;
                else
                    itr = elem.begin();
            }
        }
    }
}

void Player::ResetSpellModsDueToCanceledSpell(Spell const* spell)
{
    for (auto& elem : m_spellMods)
    {
        for (SpellModList::const_iterator itr = elem.begin(); itr != elem.end();
             ++itr)
        {
            SpellModifier* mod = *itr;

            if (mod->lastAffected != spell)
                continue;

            mod->lastAffected = nullptr;

            if (mod->charges == -1)
            {
                mod->charges = 1;
                if (m_SpellModRemoveCount > 0)
                    --m_SpellModRemoveCount;
            }
            else if (mod->charges > 0)
                ++mod->charges;
        }
    }
}

// send Proficiency
void Player::SendProficiency(ItemClass itemClass, uint32 itemSubclassMask)
{
    WorldPacket data(SMSG_SET_PROFICIENCY, 1 + 4);
    data << uint8(itemClass) << uint32(itemSubclassMask);
    GetSession()->send_packet(std::move(data));
}

void Player::RemovePetitionsAndSigns(ObjectGuid guid, uint32 type)
{
    uint32 lowguid = guid.GetCounter();

    QueryResult* result = nullptr;
    if (type == 10)
        result = CharacterDatabase.PQuery(
            "SELECT ownerguid,petitionguid FROM petition_sign WHERE playerguid "
            "= '%u'",
            lowguid);
    else
        result = CharacterDatabase.PQuery(
            "SELECT ownerguid,petitionguid FROM petition_sign WHERE playerguid "
            "= '%u' AND type = '%u'",
            lowguid, type);
    if (result)
    {
        do // this part effectively does nothing, since the deletion /
           // modification only takes place _after_ the PetitionQuery. Though I
           // don't know if the result remains intact if I execute the delete
           // query beforehand.
        {  // and SendPetitionQueryOpcode reads data from the DB
            Field* fields = result->Fetch();
            ObjectGuid ownerguid =
                ObjectGuid(HIGHGUID_PLAYER, fields[0].GetUInt32());
            ObjectGuid petitionguid =
                ObjectGuid(HIGHGUID_ITEM, fields[1].GetUInt32());

            // send update if charter owner in game
            Player* owner = sObjectMgr::Instance()->GetPlayer(ownerguid);
            if (owner)
                owner->GetSession()->SendPetitionQueryOpcode(petitionguid);

        } while (result->NextRow());

        delete result;

        if (type == 10)
            CharacterDatabase.PExecute(
                "DELETE FROM petition_sign WHERE playerguid = '%u'", lowguid);
        else
            CharacterDatabase.PExecute(
                "DELETE FROM petition_sign WHERE playerguid = '%u' AND type = "
                "'%u'",
                lowguid, type);
    }

    CharacterDatabase.BeginTransaction();
    if (type == 10)
    {
        CharacterDatabase.PExecute(
            "DELETE FROM petition WHERE ownerguid = '%u'", lowguid);
        CharacterDatabase.PExecute(
            "DELETE FROM petition_sign WHERE ownerguid = '%u'", lowguid);
    }
    else
    {
        CharacterDatabase.PExecute(
            "DELETE FROM petition WHERE ownerguid = '%u' AND type = '%u'",
            lowguid, type);
        CharacterDatabase.PExecute(
            "DELETE FROM petition_sign WHERE ownerguid = '%u' AND type = '%u'",
            lowguid, type);
    }
    CharacterDatabase.CommitTransaction();
}

void Player::LeaveAllArenaTeams(ObjectGuid guid)
{
    uint32 lowguid = guid.GetCounter();
    std::unique_ptr<QueryResult> result(CharacterDatabase.PQuery(
        "SELECT arena_team_member.arenateamid FROM arena_team_member JOIN "
        "arena_team ON arena_team_member.arenateamid = arena_team.arenateamid "
        "WHERE guid='%u'",
        lowguid));
    if (!result)
        return;

    do
    {
        Field* fields = result->Fetch();
        if (uint32 at_id = fields[0].GetUInt32())
            if (ArenaTeam* at = sObjectMgr::Instance()->GetArenaTeamById(at_id))
                at->DelMember(guid, true);

    } while (result->NextRow());
}

void Player::SetRestBonus(float rest_bonus_new)
{
    // Prevent resting on max level
    if (getLevel() >=
        sWorld::Instance()->getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL))
        rest_bonus_new = 0;

    if (rest_bonus_new < 0)
        rest_bonus_new = 0;

    float rest_bonus_max =
        (float)GetUInt32Value(PLAYER_NEXT_LEVEL_XP) * 1.5f / 2.0f;

    if (rest_bonus_new > rest_bonus_max)
        m_rest_bonus = rest_bonus_max;
    else
        m_rest_bonus = rest_bonus_new;

    // update data for client
    if (m_rest_bonus > 10)
        SetByteValue(PLAYER_BYTES_2, 3, 0x01); // Set Reststate = Rested
    else if (m_rest_bonus <= 1)
        SetByteValue(PLAYER_BYTES_2, 3, 0x02); // Set Reststate = Normal

    // RestTickUpdate
    SetUInt32Value(PLAYER_REST_STATE_EXPERIENCE, uint32(m_rest_bonus));
}

bool Player::ActivateTaxiPathTo(std::vector<uint32> const& nodes,
    Creature* npc /*= NULL*/, uint32 spellid /*= 0*/, bool isExpress /*=false*/)
{
    if (nodes.size() < 2)
        return false;

    // not let cheating with start flight in time of logout process || if
    // casting not finished || while in combat || if not use Spell's with
    // EffectSendTaxi
    if (GetSession()->isLogingOut() || isInCombat())
    {
        WorldPacket data(SMSG_ACTIVATETAXIREPLY, 4);
        data << uint32(ERR_TAXIPLAYERBUSY);
        GetSession()->send_packet(std::move(data));
        return false;
    }

    if (HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_DISABLE_MOVE))
        return false;

    // taximaster case
    if (npc)
    {
        // not let cheating with start flight mounted
        if (IsMounted())
        {
            WorldPacket data(SMSG_ACTIVATETAXIREPLY, 4);
            data << uint32(ERR_TAXIPLAYERALREADYMOUNTED);
            GetSession()->send_packet(std::move(data));
            return false;
        }

        if (IsInDisallowedMountForm())
        {
            WorldPacket data(SMSG_ACTIVATETAXIREPLY, 4);
            data << uint32(ERR_TAXIPLAYERSHAPESHIFTED);
            GetSession()->send_packet(std::move(data));
            return false;
        }

        // not let cheating with start flight in time of logout process || if
        // casting not finished || while in combat || if not use Spell's with
        // EffectSendTaxi
        if (IsNonMeleeSpellCasted(false))
        {
            WorldPacket data(SMSG_ACTIVATETAXIREPLY, 4);
            data << uint32(ERR_TAXIPLAYERBUSY);
            GetSession()->send_packet(std::move(data));
            return false;
        }
    }
    // cast case or scripted call case
    else
    {
        remove_auras(SPELL_AURA_MOUNTED);

        if (IsInDisallowedMountForm())
            remove_auras(SPELL_AURA_MOD_SHAPESHIFT);

        if (Spell* spell = GetCurrentSpell(CURRENT_GENERIC_SPELL))
            if (spell->m_spellInfo->Id != spellid)
                InterruptSpell(CURRENT_GENERIC_SPELL, false);

        InterruptSpell(CURRENT_AUTOREPEAT_SPELL, false);

        if (Spell* spell = GetCurrentSpell(CURRENT_CHANNELED_SPELL))
            if (spell->m_spellInfo->Id != spellid)
                InterruptSpell(CURRENT_CHANNELED_SPELL, true);
    }

    uint32 sourcenode = nodes[0];

    // starting node too far away (cheat?)
    TaxiNodesEntry const* node = sTaxiNodesStore.LookupEntry(sourcenode);
    if (!node)
    {
        WorldPacket data(SMSG_ACTIVATETAXIREPLY, 4);
        data << uint32(ERR_TAXINOSUCHPATH);
        GetSession()->send_packet(std::move(data));
        return false;
    }

    // check node starting pos data set case if provided
    if (node->x != 0.0f || node->y != 0.0f || node->z != 0.0f)
    {
        if (node->map_id != GetMapId() ||
            (node->x - GetX()) * (node->x - GetX()) +
                    (node->y - GetY()) * (node->y - GetY()) +
                    (node->z - GetZ()) * (node->z - GetZ()) >
                (2 * INTERACTION_DISTANCE) * (2 * INTERACTION_DISTANCE) *
                    (2 * INTERACTION_DISTANCE))
        {
            WorldPacket data(SMSG_ACTIVATETAXIREPLY, 4);
            data << uint32(ERR_TAXITOOFARAWAY);
            GetSession()->send_packet(std::move(data));
            return false;
        }
    }
    // node must have pos if taxi master case (npc != NULL)
    else if (npc)
    {
        WorldPacket data(SMSG_ACTIVATETAXIREPLY, 4);
        data << uint32(ERR_TAXIUNSPECIFIEDSERVERERROR);
        GetSession()->send_packet(std::move(data));
        return false;
    }

    // Prepare to flight start now

    // stop combat at start taxi flight if any
    CombatStop();

    // Cancel trade at flight
    cancel_trade();

    // clean not finished taxi path if any
    m_taxi.ClearTaxiDestinations();

    // 0 element current node
    m_taxi.AddTaxiDestination(sourcenode);

    // fill destinations path tail
    uint32 sourcepath = 0;
    uint32 totalcost = 0;

    uint32 prevnode = sourcenode;
    uint32 lastnode = 0;

    for (uint32 i = 1; i < nodes.size(); ++i)
    {
        uint32 path, cost;

        lastnode = nodes[i];
        sObjectMgr::Instance()->GetTaxiPath(prevnode, lastnode, path, cost);

        if (!path)
        {
            m_taxi.ClearTaxiDestinations();
            return false;
        }

        totalcost += cost;

        if (prevnode == sourcenode)
        {
            sourcepath = path;
        }

        m_taxi.AddTaxiDestination(lastnode);

        prevnode = lastnode;
    }

    // get mount model (in case non taximaster (npc==NULL) allow more wide
    // lookup)
    uint32 mount_display_id = sObjectMgr::Instance()->GetTaxiMountDisplayId(
        sourcenode, GetTeam(), npc == nullptr);

    // in spell case allow 0 model
    if ((mount_display_id == 0 && spellid == 0) || sourcepath == 0)
    {
        WorldPacket data(SMSG_ACTIVATETAXIREPLY, 4);
        data << uint32(ERR_TAXIUNSPECIFIEDSERVERERROR);
        GetSession()->send_packet(std::move(data));
        m_taxi.ClearTaxiDestinations();
        return false;
    }

    if (npc)
        totalcost = (uint32)ceil(totalcost * GetReputationPriceDiscount(npc));

    inventory::transaction trans;
    trans.remove(totalcost);
    if (!storage().finalize(trans))
    {
        WorldPacket data(SMSG_ACTIVATETAXIREPLY, 4);
        data << uint32(ERR_TAXINOTENOUGHMONEY);
        GetSession()->send_packet(std::move(data));
        m_taxi.ClearTaxiDestinations();
        return false;
    }

    // prevent stealth flight
    remove_auras(SPELL_AURA_MOD_STEALTH);

    WorldPacket data(SMSG_ACTIVATETAXIREPLY, 4);
    data << uint32(ERR_TAXIOK);
    GetSession()->send_packet(std::move(data));

    m_taxi.SetExpress(isExpress);
    m_taxi.SetOriginalMountDisplayId(mount_display_id);

    GetSession()->SendDoFlight(mount_display_id, sourcepath);

    return true;
}

bool Player::ActivateTaxiPathTo(uint32 taxi_path_id, uint32 spellid /*= 0*/)
{
    TaxiPathEntry const* entry = sTaxiPathStore.LookupEntry(taxi_path_id);
    if (!entry)
        return false;

    std::vector<uint32> nodes;

    nodes.resize(2);
    nodes[0] = entry->from;
    nodes[1] = entry->to;

    return ActivateTaxiPathTo(nodes, nullptr, spellid);
}

void Player::ContinueTaxiFlight()
{
    uint32 sourceNode = m_taxi.GetTaxiSource();
    if (!sourceNode)
        return;

    LOG_DEBUG(logging, "WORLD: Restart character %u taxi flight", GetGUIDLow());

    uint32 mountDisplayId = sObjectMgr::Instance()->GetTaxiMountDisplayId(
        sourceNode, GetTeam(), true);
    uint32 path = m_taxi.GetCurrentTaxiPath();

    // search appropriate start path node
    uint32 startNode = 0;

    TaxiPathNodeList const& nodeList = sTaxiPathNodesByPath[path];

    float distNext = (nodeList[0].x - GetX()) * (nodeList[0].x - GetX()) +
                     (nodeList[0].y - GetY()) * (nodeList[0].y - GetY()) +
                     (nodeList[0].z - GetZ()) * (nodeList[0].z - GetZ());

    for (uint32 i = 1; i < nodeList.size(); ++i)
    {
        TaxiPathNodeEntry const& node = nodeList[i];
        TaxiPathNodeEntry const& prevNode = nodeList[i - 1];

        // skip nodes at another map
        if (node.mapid != GetMapId())
            continue;

        float distPrev = distNext;

        distNext = (node.x - GetX()) * (node.x - GetX()) +
                   (node.y - GetY()) * (node.y - GetY()) +
                   (node.z - GetZ()) * (node.z - GetZ());

        float distNodes = (node.x - prevNode.x) * (node.x - prevNode.x) +
                          (node.y - prevNode.y) * (node.y - prevNode.y) +
                          (node.z - prevNode.z) * (node.z - prevNode.z);

        if (distNext + distPrev < distNodes)
        {
            startNode = i;
            break;
        }
    }

    GetSession()->SendDoFlight(mountDisplayId, path, startNode);
}

void Player::ProhibitSpellSchool(SpellSchoolMask idSchoolMask, uint32 unTimeMs)
{
    // last check 2.0.10
    WorldPacket data(SMSG_SPELL_COOLDOWN, 8 + 1 + m_spells.size() * 8);
    data << GetObjectGuid();
    data << uint8(0x0); // flags (0x1, 0x2)
    time_t curTime = WorldTimer::time_no_syscall();
    for (PlayerSpellMap::const_iterator itr = m_spells.begin();
         itr != m_spells.end(); ++itr)
    {
        if (itr->second.state == PLAYERSPELL_REMOVED)
            continue;
        uint32 unSpellId = itr->first;
        SpellEntry const* spellInfo = sSpellStore.LookupEntry(unSpellId);
        if (!spellInfo)
        {
            assert(spellInfo);
            continue;
        }

        // Not send cooldown for this spells
        if (spellInfo->HasAttribute(SPELL_ATTR_DISABLED_WHILE_ACTIVE))
            continue;

        if ((idSchoolMask & GetSpellSchoolMask(spellInfo)) &&
            GetSpellCooldownDelay(unSpellId) < unTimeMs)
        {
            data << uint32(unSpellId);
            data << uint32(unTimeMs); // in m.secs
            AddSpellCooldown(
                unSpellId, 0, curTime + unTimeMs / IN_MILLISECONDS);
        }
    }
    GetSession()->send_packet(std::move(data));
}

void Player::InitDataForForm(bool reapplyMods)
{
    ShapeshiftForm form = GetShapeshiftForm();

    SpellShapeshiftFormEntry const* ssEntry =
        sSpellShapeshiftFormStore.LookupEntry(form);
    if (ssEntry && ssEntry->attackSpeed)
    {
        SetAttackTime(BASE_ATTACK, ssEntry->attackSpeed);
        SetAttackTime(OFF_ATTACK, ssEntry->attackSpeed);
        SetAttackTime(RANGED_ATTACK, BASE_ATTACK_TIME);
    }
    else
        SetRegularAttackTime();

    switch (form)
    {
    case FORM_CAT:
    {
        if (getPowerType() != POWER_ENERGY)
            setPowerType(POWER_ENERGY);
        break;
    }
    case FORM_BEAR:
    case FORM_DIREBEAR:
    {
        if (getPowerType() != POWER_RAGE)
            setPowerType(POWER_RAGE);
        break;
    }
    default: // 0, for example
    {
        ChrClassesEntry const* cEntry =
            sChrClassesStore.LookupEntry(getClass());
        if (cEntry && cEntry->powerType < MAX_POWERS &&
            uint32(getPowerType()) != cEntry->powerType)
            setPowerType(Powers(cEntry->powerType));
        break;
    }
    }

    // update auras at form change, ignore this at mods reapply (.reset
    // stats/etc) when form not change.
    if (!reapplyMods)
        UpdateEquipSpellsAtFormChange();

    UpdateAttackPowerAndDamage();
    UpdateAttackPowerAndDamage(true);
    UpdateAllCritPercentages();
}

void Player::InitDisplayIds()
{
    PlayerInfo const* info =
        sObjectMgr::Instance()->GetPlayerInfo(getRace(), getClass());
    if (!info)
    {
        logging.error(
            "Player %u has incorrect race/class pair. Can't init display ids.",
            GetGUIDLow());
        return;
    }

    // reset scale before reapply auras
    SetObjectScale(DEFAULT_OBJECT_SCALE);

    uint8 gender = getGender();
    switch (gender)
    {
    case GENDER_FEMALE:
        SetDisplayId(info->displayId_f);
        SetNativeDisplayId(info->displayId_f);
        break;
    case GENDER_MALE:
        SetDisplayId(info->displayId_m);
        SetNativeDisplayId(info->displayId_m);
        break;
    default:
        logging.error("Invalid gender %u for player", gender);
        return;
    }
}

void Player::TakeExtendedCost(uint32 extendedCostId, uint32 count)
{
    const ItemExtendedCostEntry* extendedCost =
        sObjectMgr::Instance()->GetExtendedCostOverride(extendedCostId);

    if (!extendedCost)
        extendedCost = sItemExtendedCostStore.LookupEntry(extendedCostId);

    if (extendedCost->reqhonorpoints)
        ModifyHonorPoints(-int32(extendedCost->reqhonorpoints * count));
    if (extendedCost->reqarenapoints)
        ModifyArenaPoints(-int32(extendedCost->reqarenapoints * count));

    for (uint8 i = 0; i < MAX_EXTENDED_COST_ITEMS; ++i)
    {
        if (extendedCost->reqitem[i])
        {
            inventory::transaction trans;
            trans.destroy(extendedCost->reqitem[i],
                extendedCost->reqitemcount[i] * count);
            storage().finalize(trans);
        }
    }
}

namespace
{
void send_ext_cost_err(Player* player, const ItemExtendedCostEntry* cost)
{
    // This is a hack to get around that we cannot modify client-side visual
    // representation of extended cost.

    std::stringstream err;
    err << "That item costs";
    bool first = true;

    if (cost->reqhonorpoints)
    {
        err << " " << cost->reqhonorpoints << " honor";
        first = false;
    }

    if (cost->reqarenapoints)
    {
        err << (!first ? ", " : " ") << cost->reqarenapoints << " arena points";
        first = false;
    }

    for (int i = 0; i < MAX_EXTENDED_COST_ITEMS; ++i)
    {
        if (cost->reqitem[i])
        {
            if (auto itemproto =
                    sItemStorage.LookupEntry<ItemPrototype>(cost->reqitem[i]))
            {
                std::string name = itemproto->Name1;
                sObjectMgr::Instance()->GetItemLocaleStrings(itemproto->ItemId,
                    player->GetSession()->GetSessionDbLocaleIndex(), &name);
                err << (!first ? ", " : " ") << cost->reqitemcount[i] << "x"
                    << "|cffffffff|Hitem:" << cost->reqitem[i]
                    << ":0:0:0:0:0:0:0|h[" << name << "]|h|r";
                first = false;
            }
        }
    }

    err << ".";

    if (cost->reqpersonalarenarating)
        err << " And, you must have a personal rating of at least "
            << cost->reqpersonalarenarating << ".";

    ChatHandler(player).SendSysMessage(err.str().c_str());
}
}

// Return true is the bought item has a max count to force refresh of window by
// caller
bool Player::BuyItemFromVendor(
    ObjectGuid vendorGuid, uint32 item, uint8 count, inventory::slot dst)
{
    if (count < 1)
        return false;

    if (!isAlive())
        return false;

    ItemPrototype const* pProto = ObjectMgr::GetItemPrototype(item);
    if (!pProto)
    {
        SendBuyError(BUY_ERR_CANT_FIND_ITEM, nullptr, item, 0);
        return false;
    }

    Creature* pCreature =
        GetNPCIfCanInteractWith(vendorGuid, UNIT_NPC_FLAG_VENDOR);
    if (!pCreature)
    {
        LOG_DEBUG(logging,
            "WORLD: BuyItemFromVendor - %s not found or you can't interact "
            "with him.",
            vendorGuid.GetString().c_str());
        SendBuyError(BUY_ERR_DISTANCE_TOO_FAR, nullptr, item, 0);
        return false;
    }

    VendorItemData const* vItems = pCreature->GetVendorItems();
    VendorItemData const* tItems = pCreature->GetVendorTemplateItems();
    if ((!vItems || vItems->Empty()) && (!tItems || tItems->Empty()))
    {
        SendBuyError(BUY_ERR_CANT_FIND_ITEM, pCreature, item, 0);
        return false;
    }

    uint32 vCount = vItems ? vItems->GetItemCount() : 0;
    uint32 tCount = tItems ? tItems->GetItemCount() : 0;

    size_t vendorslot = vItems ? vItems->FindItemSlot(item) : vCount;
    if (vendorslot >= vCount)
        vendorslot = vCount + (tItems ? tItems->FindItemSlot(item) : tCount);

    if (vendorslot >= vCount + tCount)
    {
        SendBuyError(BUY_ERR_CANT_FIND_ITEM, pCreature, item, 0);
        return false;
    }

    VendorItem const* crItem = vendorslot < vCount ?
                                   vItems->GetItem(vendorslot) :
                                   tItems->GetItem(vendorslot - vCount);
    if (!crItem || crItem->item != item) // store diff item (cheating)
    {
        SendBuyError(BUY_ERR_CANT_FIND_ITEM, pCreature, item, 0);
        return false;
    }

    uint32 totalCount = pProto->BuyCount * count;

    // check current item amount if it limited
    if (crItem->maxcount != 0)
    {
        if (pCreature->GetVendorItemCurrentCount(crItem) < totalCount)
        {
            SendBuyError(BUY_ERR_ITEM_ALREADY_SOLD, pCreature, item, 0);
            return false;
        }
    }

    if (uint32(GetReputationRank(pProto->RequiredReputationFaction)) <
        pProto->RequiredReputationRank)
    {
        SendBuyError(BUY_ERR_REPUTATION_REQUIRE, pCreature, item, 0);
        return false;
    }

    if (uint32 extendedCostId = crItem->ExtendedCost)
    {
        const ItemExtendedCostEntry* iece =
            sObjectMgr::Instance()->GetExtendedCostOverride(
                crItem->ExtendedCost);
        if (!iece)
            iece = sItemExtendedCostStore.LookupEntry(extendedCostId);
        if (!iece)
        {
            logging.error("Item %u have wrong ExtendedCost field value %u",
                pProto->ItemId, extendedCostId);
            return false;
        }

        // honor points price
        if (GetHonorPoints() < (iece->reqhonorpoints * count))
        {
            send_ext_cost_err(this, iece);
            SendEquipError(EQUIP_ERR_NOT_ENOUGH_HONOR_POINTS, nullptr, nullptr);
            return false;
        }

        // arena points price
        if (GetArenaPoints() < (iece->reqarenapoints * count))
        {
            send_ext_cost_err(this, iece);
            SendEquipError(EQUIP_ERR_NOT_ENOUGH_ARENA_POINTS, nullptr, nullptr);
            return false;
        }

        // item base price
        for (uint8 i = 0; i < MAX_EXTENDED_COST_ITEMS; ++i)
        {
            if (iece->reqitem[i] &&
                !HasItemCount(iece->reqitem[i], iece->reqitemcount[i] * count))
            {
                send_ext_cost_err(this, iece);
                SendEquipError(
                    EQUIP_ERR_VENDOR_MISSING_TURNINS, nullptr, nullptr);
                return false;
            }
        }

        // check for personal arena rating requirement
        if (GetMaxPersonalArenaRatingRequirement() <
            iece->reqpersonalarenarating)
        {
            // probably not the proper equip err
            send_ext_cost_err(this, iece);
            SendEquipError(EQUIP_ERR_CANT_EQUIP_RANK, nullptr, nullptr);
            return false;
        }
    }

    // Calculate cost, including reputation-based discounts
    uint32 tmp = pProto->BuyPrice * count;
    inventory::copper price =
        uint32(floor(tmp * GetReputationPriceDiscount(pCreature)));

    if (storage().money().get() < price.get())
    {
        SendBuyError(BUY_ERR_NOT_ENOUGHT_MONEY, pCreature, item, 0);
        return false;
    }

    if (dst == inventory::slot())
    {
        inventory::transaction trans(true, inventory::transaction::send_self,
            inventory::transaction::add_vendor);
        trans.remove(price);
        trans.add(item, totalCount);
        if (!storage().finalize(trans))
        {
            SendEquipError(
                static_cast<InventoryResult>(trans.error()), nullptr);
            return false;
        }
    }
    else
    {
        InventoryResult err = storage().add_to_slot(item, count, dst);
        if (err != EQUIP_ERR_OK)
        {
            SendEquipError(err, nullptr);
            return false;
        }
        inventory::transaction trans;
        trans.remove(price);
        storage().finalize(trans); // Cannot fail, money checked above
    }

    // Extended cost includes things such as arena & honor points, as well as
    // actual items
    if (crItem->ExtendedCost)
        TakeExtendedCost(crItem->ExtendedCost, count);

    uint32 new_count =
        pCreature->UpdateVendorItemCurrentCount(crItem, totalCount);

    WorldPacket data(SMSG_BUY_ITEM, 8 + 4 + 4 + 4);
    data << pCreature->GetObjectGuid();
    data << uint32(vendorslot + 1); // numbered from 1 at client
    data << uint32(crItem->maxcount > 0 ? new_count : 0xFFFFFFFF);
    data << uint32(count);
    GetSession()->send_packet(std::move(data));

    return crItem->maxcount != 0;
}

uint32 Player::GetMaxPersonalArenaRatingRequirement()
{
    // returns the maximal personal arena rating that can be used to purchase
    // items requiring this condition
    // the personal rating of the arena team must match the required limit as
    // well
    // so return max[in arenateams](min(personalrating[teamtype],
    // teamrating[teamtype]))
    uint32 max_personal_rating = 0;
    for (int i = 0; i < MAX_ARENA_SLOT; ++i)
    {
        if (ArenaTeam* at =
                sObjectMgr::Instance()->GetArenaTeamById(GetArenaTeamId(i)))
        {
            uint32 p_rating = GetArenaPersonalRating(i);
            uint32 t_rating = at->GetRating();
            p_rating = p_rating < t_rating ? p_rating : t_rating;
            if (max_personal_rating < p_rating)
                max_personal_rating = p_rating;
        }
    }
    return max_personal_rating;
}

void Player::UpdateHomebindTime(uint32 time)
{
    // GMs never get homebind timer online
    if (m_InstanceValid || isGameMaster())
    {
        if (m_HomebindTimer) // instance valid, but timer not reset
        {
            // hide reminder
            WorldPacket data(SMSG_RAID_GROUP_ONLY, 4 + 4);
            data << uint32(0);
            data << uint32(
                ERR_RAID_GROUP_NONE); // error used only when timer = 0
            GetSession()->send_packet(std::move(data));
        }
        // instance is valid, reset homebind timer
        m_HomebindTimer = 0;
    }
    else if (m_HomebindTimer > 0)
    {
        if (time >= m_HomebindTimer)
        {
            // teleport to homebind location
            TeleportTo(
                m_homebindMapId, m_homebindX, m_homebindY, m_homebindZ, GetO());
        }
        else
            m_HomebindTimer -= time;
    }
    else
    {
        // instance is invalid, start homebind timer
        m_HomebindTimer = 60000;
        // send message to player
        WorldPacket data(SMSG_RAID_GROUP_ONLY, 4 + 4);
        data << uint32(m_HomebindTimer);
        data << uint32(ERR_RAID_GROUP_NONE); // error used only when timer = 0
        GetSession()->send_packet(std::move(data));
        LOG_DEBUG(logging,
            "PLAYER: Player '%s' (GUID: %u) will be teleported to homebind in "
            "60 seconds",
            GetName(), GetGUIDLow());
    }
}

void Player::UpdatePvP(bool state, bool ovrride)
{
    if (!state || ovrride)
    {
        SetPvP(state);
        pvpInfo.endTimer = 0;
    }
    else
    {
        if (pvpInfo.endTimer != 0)
            pvpInfo.endTimer = WorldTimer::time_no_syscall();
        else
            SetPvP(state);
    }
}

bool Player::HasSpellCooldown(uint32 spell_id, uint32 castitem_id) const
{
    // If we have a cast item, we need to check spell categories specified in
    // item_template
    if (castitem_id)
    {
        if (const ItemPrototype* proto =
                ObjectMgr::GetItemPrototype(castitem_id))
        {
            for (int i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
            {
                if (proto->Spells[i].SpellId == spell_id)
                {
                    if (proto->Spells[i].SpellCategory &&
                        HasSpellCategoryCooldown(
                            proto->Spells[i].SpellCategory))
                        return true;

                    break;
                }
            }
        }
    }

    // Check category cooldowns
    if (const SpellEntry* spell = sSpellStore.LookupEntry(spell_id))
        if (spell->Category && HasSpellCategoryCooldown(spell->Category))
            return true;

    auto itr = m_spellCooldowns.find(spell_id);
    return itr != m_spellCooldowns.end() &&
           itr->second.end > WorldTimer::time_no_syscall();
}

bool Player::HasSpellCategoryCooldown(uint32 category_id) const
{
    auto itr = category_cooldowns_.find(category_id);
    if (itr != category_cooldowns_.end())
        if (itr->second > WorldTimer::time_no_syscall())
            return true;
    return false;
}

time_t Player::GetSpellCooldownDelay(uint32 spell_id) const
{
    // Figure out category delay
    time_t category_end = 0;
    if (const SpellEntry* spell = sSpellStore.LookupEntry(spell_id))
        if (spell->Category)
        {
            auto itr = category_cooldowns_.find(spell->Category);
            category_end = itr != category_cooldowns_.end() ? itr->second : 0;
        }

    // Figure out invididual spell delay
    auto itr = m_spellCooldowns.find(spell_id);
    time_t t = WorldTimer::time_no_syscall();
    time_t delay = itr != m_spellCooldowns.end() && itr->second.end > t ?
                       itr->second.end :
                       0;

    // Pick the delay that's the biggest
    if (delay < category_end)
        delay = category_end;

    return delay > t ? delay - t : 0;
}

void Player::AddSpellAndCategoryCooldowns(SpellEntry const* spellInfo,
    uint32 itemId, Spell* spell, bool infinityCooldown)
{
    // init cooldown values
    uint32 cat = 0;
    int32 rec = -1;
    int32 catrec = -1;

    // some special item spells without correct cooldown in SpellInfo
    // cooldown information stored in item prototype
    // This used in same way in WorldSession::HandleItemQuerySingleOpcode data
    // sending to client.

    if (itemId)
    {
        if (ItemPrototype const* proto = ObjectMgr::GetItemPrototype(itemId))
        {
            for (int idx = 0; idx < MAX_ITEM_PROTO_SPELLS; ++idx)
            {
                if (proto->Spells[idx].SpellId == spellInfo->Id)
                {
                    cat = proto->Spells[idx].SpellCategory;
                    rec = proto->Spells[idx].SpellCooldown;
                    catrec = proto->Spells[idx].SpellCategoryCooldown;
                    break;
                }
            }
        }
    }

    // if no cooldown found above then base at DBC data
    if (rec < 0 && catrec < 0)
    {
        cat = spellInfo->Category;
        rec = spellInfo->RecoveryTime;
        catrec = spellInfo->CategoryRecoveryTime;
    }

    time_t curTime = WorldTimer::time_no_syscall();

    time_t catrecTime;
    time_t recTime;

    // overwrite time for selected category
    if (infinityCooldown)
    {
        // use +MONTH as infinity mark for spell cooldown (will checked as
        // MONTH/2 at save ans skipped)
        // but not allow ignore until reset or re-login
        catrecTime = catrec > 0 ? curTime + infinityCooldownDelay : 0;
        recTime = rec > 0 ? curTime + infinityCooldownDelay : catrecTime;
    }
    else
    {
        // shoot spells used equipped item cooldown values already assigned in
        // GetAttackTime(RANGED_ATTACK)
        // prevent 0 cooldowns set by another way
        if (rec <= 0 && catrec <= 0 && (cat == 76 || cat == 351))
            rec = GetAttackTime(RANGED_ATTACK);

        // Now we have cooldown data (if found any), time to apply mods
        if (rec > 0)
            ApplySpellMod(spellInfo->Id, SPELLMOD_COOLDOWN, rec, spell);

        if (catrec > 0)
            ApplySpellMod(spellInfo->Id, SPELLMOD_COOLDOWN, catrec, spell);

        // replace negative cooldowns by 0
        if (rec < 0)
            rec = 0;
        if (catrec < 0)
            catrec = 0;

        // no cooldown after applying spell mods
        if (rec == 0 && catrec == 0)
            return;

        catrecTime = catrec ? curTime + catrec / IN_MILLISECONDS : 0;
        recTime = rec ? curTime + rec / IN_MILLISECONDS : catrecTime;
    }

    // self spell cooldown
    if (recTime > 0)
        AddSpellCooldown(spellInfo->Id, itemId, recTime);

    // category cooldown
    if (cat && catrec > 0)
    {
        category_cooldowns_[cat] = catrecTime;

        // Recursively add all items as unique cooldowns that share this
        // category
        // We need to do this to properly show the cooldown after
        // zoning/relogging
        for (auto itr = storage().begin(
                 inventory::personal_storage::iterator::all_body);
             itr != storage().end(); ++itr)
        {
            auto proto = (*itr)->GetProto();
            for (int i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
            {
                if (!proto->Spells[i].SpellId)
                    continue;
                if (proto->Spells[i].SpellCategory != cat)
                    continue;
                if ((*itr)->GetEntry() == itemId)
                    continue;
                if (sSpellStore.LookupEntry(proto->Spells[i].SpellId) ==
                    nullptr)
                    continue;
                AddSpellCooldown(
                    proto->Spells[i].SpellId, (*itr)->GetEntry(), catrecTime);
            }
        }
    }
}

void Player::AddSpellCooldown(uint32 spellid, uint32 itemid, time_t end_time)
{
    SpellCooldown sc;
    sc.end = end_time;
    sc.itemid = itemid;
    m_spellCooldowns[spellid] = sc;
}

void Player::SendCooldownEvent(
    SpellEntry const* spellInfo, uint32 itemId, Spell* spell)
{
    // start cooldowns at server side, if any
    AddSpellAndCategoryCooldowns(spellInfo, itemId, spell);

    // Send activate cooldown timer (possible 0) at client side
    WorldPacket data(SMSG_COOLDOWN_EVENT, (4 + 8));
    data << uint32(spellInfo->Id);
    data << GetObjectGuid();
    SendDirectMessage(std::move(data));
}

void Player::AddProcEventCooldown(uint32 spell_id, uint32 milliseconds)
{
    proc_event_cooldowns_[spell_id] = WorldTimer::getMSTime() + milliseconds;
}

bool Player::HasProcEventCooldown(uint32 spell_id)
{
    auto itr = proc_event_cooldowns_.find(spell_id);
    if (itr != proc_event_cooldowns_.end())
    {
        if (itr->second <= WorldTimer::getMSTime())
        {
            proc_event_cooldowns_.erase(itr);
            return false;
        }
        return true;
    }
    return false;
}

void Player::update_meta_gem()
{
    // Only your head can have a meta-gem, and there is at max one possible
    Item* head = storage().get(inventory::slot(
        inventory::personal_slot, inventory::main_bag, inventory::head));
    if (!head)
        return;

    // Find spell enchantment entry of equipped meta-gem
    const SpellItemEnchantmentEntry* meta_gem = nullptr;
    EnchantmentSlot meta_ench_slot;
    for (int i = 0; i < MAX_GEM_SOCKETS; ++i)
    {
        if (head->GetProto()->Socket[i].Color != SOCKET_COLOR_META)
            continue;
        uint32 ench_id = head->GetEnchantmentId(
            static_cast<EnchantmentSlot>(SOCK_ENCHANTMENT_SLOT + i));
        if (!ench_id)
            continue;
        const SpellItemEnchantmentEntry* entry =
            sSpellItemEnchantmentStore.LookupEntry(ench_id);
        if (entry->EnchantmentCondition ==
            0) // Meta gems always have conditions
            continue;
        meta_gem = entry;
        meta_ench_slot =
            static_cast<EnchantmentSlot>(SOCK_ENCHANTMENT_SLOT + i);
        break;
    }

    const SpellItemEnchantmentConditionEntry* meta_cond;
    if (!meta_gem ||
        (meta_cond = sSpellItemEnchantmentConditionStore.LookupEntry(
             meta_gem->EnchantmentCondition)) == nullptr)
        return; // No equipped meta-gem

    // Go through all equipped items and count how many of each gem color we
    // have equipped in matching socket colors
    int gem_colors[] = {0, 0, 0}; // red, yellow, blue
    const int mask = inventory::personal_storage::iterator::equipment;
    for (inventory::personal_storage::iterator itr = storage().begin(mask);
         itr != storage().end(); ++itr)
    {
        Item* item = *itr;
        for (int i = 0; i < MAX_GEM_SOCKETS; ++i)
        {
            uint32 ench_id = item->GetEnchantmentId(
                static_cast<EnchantmentSlot>(SOCK_ENCHANTMENT_SLOT + i));
            if (!ench_id)
                continue;

            const SpellItemEnchantmentEntry* entry =
                sSpellItemEnchantmentStore.LookupEntry(ench_id);
            const GemPropertiesEntry* gem_properties =
                sGemPropertiesStore.LookupEntry(
                    ObjectMgr::GetItemPrototype(entry->GemID)->GemProperties);

            if (gem_properties->color & SOCKET_COLOR_RED)
                gem_colors[0] += 1;
            if (gem_properties->color & SOCKET_COLOR_YELLOW)
                gem_colors[1] += 1;
            if (gem_properties->color & SOCKET_COLOR_BLUE)
                gem_colors[2] += 1;
        }
    }

    bool meta_gem_active = true;
    for (int i = 0; i < 5 && meta_gem_active != false; ++i)
    {
        if (meta_cond->Color[i] < 2 ||
            meta_cond->Color[i] >
                4) // Meta == 1, Red == 2, Yellow == 3, Blue == 4
            continue;

        uint32 gem_count = gem_colors[meta_cond->Color[i] - 2];
        uint32 cmp_count;

        // If CompareColor is not 0, we're comparing a relationship against
        // another gem color (e.g.: less Green than Blue)
        if (meta_cond->CompareColor[i] != 0)
            cmp_count = gem_colors[meta_cond->CompareColor[i] - 2];
        // Otherwise we're comparing against a set value (e.g. Must have more
        // than 4 Blue gems)
        else
            cmp_count = meta_cond->Value[i];

        // "Comparator" Indicates the type of comparison we do
        switch (meta_cond->Comparator[i])
        {
        case 2: // less than
            meta_gem_active = gem_count < cmp_count;
            break;
        case 3: // bigger than
            meta_gem_active = gem_count > cmp_count;
            break;
        case 5: // bigger than or equal to
            meta_gem_active = gem_count >= cmp_count;
            break;
        }
    }

    LOG_DEBUG(logging,
        "Player::update_meta_gems(): Red Gems: %u Yellow Gems: %u Blue Gems: "
        "%u. Meta-gem active: %s (was on before: %s)",
        gem_colors[0], gem_colors[1], gem_colors[2],
        (meta_gem_active ? "Yes" : "No"),
        (head->meta_toggled_on ? "Yes" : "No"));

    if (head->meta_toggled_on)
    {
        ApplyEnchantment(head, meta_ench_slot, false, head->slot());
        head->meta_toggled_on = false;
    }
    if (meta_gem_active)
    {
        ApplyEnchantment(head, meta_ench_slot, true, head->slot());
        head->meta_toggled_on = true;
    }
}

void Player::SetBattleGroundEntryPoint(Player* leader /*= NULL*/)
{
    // chat command use case, or non-group join
    if (!leader || !leader->IsInWorld() || leader->IsTaxiFlying() ||
        leader->GetMap()->IsDungeon() ||
        leader->GetMap()->IsBattleGroundOrArena())
        leader = this;

    if (leader->IsInWorld() && !leader->IsTaxiFlying())
    {
        // If map is dungeon find linked graveyard
        if (leader->GetMap()->IsDungeon())
        {
            if (const WorldSafeLocsEntry* entry =
                    sObjectMgr::Instance()->GetClosestGraveyard(leader->GetX(),
                        leader->GetY(), leader->GetZ(), leader->GetMapId(),
                        leader->GetTeam()))
            {
                m_bgData.joinPos = WorldLocation(
                    entry->map_id, entry->x, entry->y, entry->z, 0.0f);
                return;
            }
            else
                logging.error(
                    "SetBattleGroundEntryPoint: Dungeon map %u has no linked "
                    "graveyard, setting home location as entry point.",
                    leader->GetMapId());
        }
        // If new entry point is not BG or arena set it
        else if (!leader->GetMap()->IsBattleGroundOrArena())
        {
            m_bgData.joinPos = WorldLocation(leader->GetMapId(), leader->GetX(),
                leader->GetY(), leader->GetZ(), leader->GetO());
            return;
        }
    }

    // In error cases use homebind position
    m_bgData.joinPos = WorldLocation(
        m_homebindMapId, m_homebindX, m_homebindY, m_homebindZ, 0.0f);
}

void Player::LeaveBattleground(bool teleportToEntryPoint)
{
    if (BattleGround* bg = GetBattleGround())
    {
        bg->RemovePlayerAtLeave(GetObjectGuid(), teleportToEntryPoint);

        // call after remove to be sure that player resurrected for correct cast
        if (bg->isBattleGround() && !isGameMaster() &&
            sWorld::Instance()->getConfig(
                CONFIG_BOOL_BATTLEGROUND_CAST_DESERTER))
        {
            if (bg->GetStatus() == STATUS_IN_PROGRESS ||
                bg->GetStatus() == STATUS_WAIT_JOIN)
            {
                // lets check if player was teleported from BG and schedule
                // delayed Deserter spell cast
                if (IsBeingTeleportedFar())
                {
                    ScheduleDelayedOperation(DELAYED_SPELL_CAST_DESERTER);
                    return;
                }

                CastSpell(this, 26013, true); // Deserter
            }
        }
    }
}

bool Player::CanReportAfkDueToLimit()
{
    // a player can complain about 15 people per 5 minutes
    if (m_bgData.bgAfkReportedCount++ >= 15)
        return false;

    return true;
}

/// This player has been blamed to be inactive in a battleground
void Player::ReportedAfkBy(Player* reporter)
{
    BattleGround* bg = GetBattleGround();
    if (!bg || bg != reporter->GetBattleGround() ||
        GetTeam() != reporter->GetTeam())
        return;

    // check if player has 'Idle' or 'Inactive' debuff
    if (m_bgData.bgAfkReporter.find(reporter->GetGUIDLow()) ==
            m_bgData.bgAfkReporter.end() &&
        !has_aura(43680) && !has_aura(43681) &&
        reporter->CanReportAfkDueToLimit())
    {
        m_bgData.bgAfkReporter.insert(reporter->GetGUIDLow());
        // 3 players have to complain to apply debuff
        if (m_bgData.bgAfkReporter.size() >= 3)
        {
            // cast 'Idle' spell
            CastSpell(this, 43680, true);
            m_bgData.bgAfkReporter.clear();
        }
    }
}

bool Player::IsVisibleInGridForPlayer(Player* pl) const
{
    // gamemaster in GM mode see all, including ghosts
    if (pl->isGameMaster() &&
        GetSession()->GetSecurity() <= pl->GetSession()->GetSecurity())
        return true;

    // player see dead player/ghost from own group/raid
    if (IsInSameRaidWith(pl))
        return true;

    // Live player see live player or dead player with not realized corpse
    if (pl->isAlive() || pl->m_deathTimer > 0)
        return isAlive() || m_deathTimer > 0;

    // Ghost see other friendly ghosts, that's for sure
    if (!(isAlive() || m_deathTimer > 0) && IsFriendlyTo(pl))
        return true;

    // Dead player see live players near own corpse
    if (isAlive())
    {
        if (Corpse* corpse = pl->GetCorpse())
        {
            // 20 - aggro distance for same level, 25 - max additional distance
            // if player level less that creature level
            if (corpse->IsWithinDistInMap(this, (20 + 25)))
                return true;
        }
    }

    // and not see any other
    return false;
}

bool Player::IsVisibleGloballyFor(Player* u) const
{
    if (!u)
        return false;

    // Always can see self
    if (u == this)
        return true;

    // Visible units, always are visible for all players
    if (GetVisibility() == VISIBILITY_ON)
        return true;

    // GMs are visible for higher gms (or players are visible for gms)
    if (u->GetSession()->GetSecurity() > SEC_PLAYER)
        return GetSession()->GetSecurity() <= u->GetSession()->GetSecurity();

    // non faction visibility non-breakable for non-GMs
    if (GetVisibility() == VISIBILITY_OFF)
        return false;

    // non-gm stealth/invisibility not hide from global player lists
    return true;
}

template <class T>
inline void BeforeVisibilityDestroy(T* /*t*/, Player* /*p*/)
{
}

template <>
inline void BeforeVisibilityDestroy<Creature>(Creature* t, Player* p)
{
    if (p->GetPetGuid() == t->GetObjectGuid() && ((Creature*)t)->IsPet())
        ((Pet*)t)->Unsummon(PET_SAVE_REAGENTS);
}

void Player::UpdateVisibilityOf(
    WorldObject const* viewPoint, WorldObject* target)
{
    if (HaveAtClient(target))
    {
        if (!target->isVisibleForInState(this, viewPoint, true))
        {
            ObjectGuid t_guid = target->GetObjectGuid();

            if (target->GetTypeId() == TYPEID_UNIT)
                BeforeVisibilityDestroy<Creature>((Creature*)target, this);

            target->DestroyForPlayer(this);
            m_clientGUIDs.erase(t_guid);

            LOG_DEBUG(logging, "%s out of range for player %u. Distance = %f",
                t_guid.GetString().c_str(), GetGUIDLow(), GetDistance(target));
        }
    }
    else
    {
        if (target->isVisibleForInState(this, viewPoint, false))
        {
            target->SendCreateUpdateToPlayer(this);
            if (target->GetTypeId() != TYPEID_GAMEOBJECT ||
                !((GameObject*)target)->IsTransport())
                m_clientGUIDs.insert(target->GetObjectGuid());

            LOG_DEBUG(logging,
                "Object %u (Type: %u) is visible now for player %u. Distance = "
                "%f",
                target->GetGUIDLow(), target->GetTypeId(), GetGUIDLow(),
                GetDistance(target));

            // target aura duration for caster show only if target exist at
            // caster client
            // send data at target visibility change (adding to client)
            if (target != this && target->isType(TYPEMASK_UNIT))
                SendAuraDurationsForTarget((Unit*)target);
        }
    }
}

template <class T>
inline void UpdateVisibilityOf_helper(ObjectGuidSet& s64, T* target)
{
    s64.insert(target->GetObjectGuid());
}

template <>
inline void UpdateVisibilityOf_helper(ObjectGuidSet& s64, GameObject* target)
{
    if (!target->IsTransport())
        s64.insert(target->GetObjectGuid());
}

template <class T>
void Player::UpdateVisibilityOf(WorldObject const* viewPoint, T* target,
    UpdateData& data, std::set<WorldObject*>& visibleNow)
{
    if (HaveAtClient(target))
    {
        if (!target->isVisibleForInState(this, viewPoint, true))
        {
            BeforeVisibilityDestroy<T>(target, this);

            ObjectGuid t_guid = target->GetObjectGuid();

            target->BuildOutOfRangeUpdateBlock(&data);
            m_clientGUIDs.erase(t_guid);

            LOG_DEBUG(logging, "%s is out of range for %s. Distance = %f",
                t_guid.GetString().c_str(), GetGuidStr().c_str(),
                GetDistance(target));
        }
    }
    else
    {
        if (target->isVisibleForInState(this, viewPoint, false))
        {
            visibleNow.insert(target);
            target->BuildCreateUpdateBlockForPlayer(&data, this);
            UpdateVisibilityOf_helper(m_clientGUIDs, target);

            LOG_DEBUG(logging, "%s is visible now for %s. Distance = %f",
                target->GetGuidStr().c_str(), GetGuidStr().c_str(),
                GetDistance(target));
        }
    }
}

template void Player::UpdateVisibilityOf(WorldObject const* viewPoint,
    Player* target, UpdateData& data, std::set<WorldObject*>& visibleNow);
template void Player::UpdateVisibilityOf(WorldObject const* viewPoint,
    Creature* target, UpdateData& data, std::set<WorldObject*>& visibleNow);
template void Player::UpdateVisibilityOf(WorldObject const* viewPoint,
    Pet* target, UpdateData& data, std::set<WorldObject*>& visibleNow);
template void Player::UpdateVisibilityOf(WorldObject const* viewPoint,
    Corpse* target, UpdateData& data, std::set<WorldObject*>& visibleNow);
template void Player::UpdateVisibilityOf(WorldObject const* viewPoint,
    GameObject* target, UpdateData& data, std::set<WorldObject*>& visibleNow);
template void Player::UpdateVisibilityOf(WorldObject const* viewPoint,
    DynamicObject* target, UpdateData& data,
    std::set<WorldObject*>& visibleNow);
template void Player::UpdateVisibilityOf(WorldObject const* viewPoint,
    SpecialVisCreature* target, UpdateData& data,
    std::set<WorldObject*>& visibleNow);
template void Player::UpdateVisibilityOf(WorldObject const* viewPoint,
    TemporarySummon* target, UpdateData& data,
    std::set<WorldObject*>& visibleNow);
template void Player::UpdateVisibilityOf(WorldObject const* viewPoint,
    Totem* target, UpdateData& data, std::set<WorldObject*>& visibleNow);

void Player::AddToClient(WorldObject* u)
{
    // This code was copied from Player::HandleSingleStealthedUnitDetection and
    // might not work "universally" for adding to the client

    u->SendCreateUpdateToPlayer(this);
    m_clientGUIDs.insert(u->GetObjectGuid());

    // target aura duration for caster show only if target exist at caster
    // client
    // send data at target visibility change (adding to client)
    if (u->isType(TYPEMASK_UNIT))
        SendAuraDurationsForTarget(static_cast<Unit*>(u));
}

void Player::RemoveFromClient(WorldObject* u)
{
    // See comment in Player::AddToClient, same thing applies here
    u->DestroyForPlayer(this);
    m_clientGUIDs.erase(u->GetObjectGuid());
}

void Player::InitPrimaryProfessions()
{
    uint32 maxProfs =
        GetSession()->GetSecurity() <
                AccountTypes(sWorld::Instance()->getConfig(
                    CONFIG_UINT32_TRADE_SKILL_GMIGNORE_MAX_PRIMARY_COUNT)) ?
            sWorld::Instance()->getConfig(
                CONFIG_UINT32_MAX_PRIMARY_TRADE_SKILL) :
            10;
    SetFreePrimaryProfessions(maxProfs);
}

void Player::SendComboPoints()
{
    Unit* combotarget = ObjectAccessor::GetUnit(*this, m_comboTargetGuid);
    if (combotarget)
    {
        WorldPacket data(
            SMSG_UPDATE_COMBO_POINTS, combotarget->GetPackGUID().size() + 1);
        data << combotarget->GetPackGUID();
        data << uint8(m_comboPoints);
        GetSession()->send_packet(std::move(data));
    }
}

void Player::AddComboPoints(Unit* target, int8 count)
{
    if (!count)
        return;

    // without combo points lost (duration checked in aura)
    remove_auras(SPELL_AURA_RETAIN_COMBO_POINTS);

    if (target->GetObjectGuid() == m_comboTargetGuid)
    {
        m_comboPoints += count;
    }
    else
    {
        if (m_comboTargetGuid)
            if (Unit* target2 =
                    ObjectAccessor::GetUnit(*this, m_comboTargetGuid))
                target2->RemoveComboPointHolder(GetGUIDLow());

        m_comboTargetGuid = target->GetObjectGuid();
        m_comboPoints = count;

        target->AddComboPointHolder(GetGUIDLow());
    }

    if (m_comboPoints > 5)
        m_comboPoints = 5;
    if (m_comboPoints < 0)
        m_comboPoints = 0;

    SendComboPoints();
}

void Player::ClearComboPoints()
{
    if (!m_comboTargetGuid)
        return;

    // without combopoints lost (duration checked in aura)
    remove_auras(SPELL_AURA_RETAIN_COMBO_POINTS);

    m_comboPoints = 0;

    SendComboPoints();

    if (Unit* target = ObjectAccessor::GetUnit(*this, m_comboTargetGuid))
        target->RemoveComboPointHolder(GetGUIDLow());

    m_comboTargetGuid.Clear();
}

void Player::SetGroup(Group* group, int8 subgroup)
{
    if (group == nullptr)
        m_group.unlink();
    else
    {
        // never use SetGroup without a subgroup unless you specify NULL for
        // group
        assert(subgroup >= 0);
        m_group.link(group, this);
        m_group.setSubGroup((uint8)subgroup);
    }
}

void Player::SendInitialPacketsBeforeAddToMap()
{
    WorldPacket data(SMSG_SET_REST_START, 4);
    data << uint32(0); // unknown, may be rest state time or experience
    GetSession()->send_packet(std::move(data));

    // Homebind
    data.initialize(SMSG_BINDPOINTUPDATE, 5 * 4);
    data << m_homebindX << m_homebindY << m_homebindZ;
    data << (uint32)m_homebindMapId;
    data << (uint32)m_homebindAreaId;
    GetSession()->send_packet(std::move(data));

    // SMSG_SET_PROFICIENCY
    // SMSG_UPDATE_AURA_DURATION

    // tutorial stuff
    GetSession()->SendTutorialsData();

    SendInitialSpells();

    data.initialize(SMSG_SEND_UNLEARN_SPELLS, 4);
    data << uint32(0); // count, for(count) uint32;
    GetSession()->send_packet(std::move(data));

    SendInitialActionButtons();
    m_reputationMgr.SendInitialReputations();

    // SMSG_SET_AURA_SINGLE

    data.initialize(SMSG_LOGIN_SETTIMESPEED, 4 + 4);
    data << uint32(secsToTimeBitFields(WorldTimer::time_no_syscall()));
    data << (float)0.01666667f; // game speed
    GetSession()->send_packet(std::move(data));

    // set fly flag if in fly form or taxi flight to prevent visually drop at
    // ground in showup moment
    if (IsFreeFlying() || IsTaxiFlying())
        m_movementInfo.AddMovementFlag(MOVEFLAG_FLYING);

    SetMovingUnit(this);
}

void Player::SendInitialPacketsAfterAddToMap()
{
    WorldPacket data(SMSG_INSTANCE_DIFFICULTY, 8);
    data << uint32(GetMap()->GetDifficulty());
    data << uint32(0);
    GetSession()->send_packet(std::move(data));

    // update zone
    uint32 newzone, newarea;
    GetZoneAndAreaId(newzone, newarea);
    UpdateZone(newzone, newarea); // also call SendInitWorldStates();

    ResetTimeSync();
    SendTimeSync();

    CastSpell(this, 836, true); // LOGINEFFECT

    // set some aura effects that send packet to player client after add player
    // to map
    // SendMessageToSet not send it to player not it map, only for aura that not
    // changed anything at re-apply
    // same auras state lost at far teleport, send it one more time in this case
    // also
    static const AuraType auratypes[] = {SPELL_AURA_MOD_FEAR,
        SPELL_AURA_TRANSFORM, SPELL_AURA_WATER_WALK, SPELL_AURA_FEATHER_FALL,
        SPELL_AURA_HOVER, SPELL_AURA_SAFE_FALL, SPELL_AURA_FLY,
        SPELL_AURA_MOD_FLIGHT_SPEED_MOUNTED, SPELL_AURA_NONE};
    for (AuraType const* itr = &auratypes[0]; itr && itr[0] != SPELL_AURA_NONE;
         ++itr)
    {
        const Auras& auraList = GetAurasByType(*itr);
        if (!auraList.empty())
            auraList.front()->ApplyModifier(true, true);
    }

    if (HasAuraType(SPELL_AURA_MOD_STUN))
        SetMovement(MOVE_ROOT);

    // manual send package (have code in ApplyModifier(true,true); that don't
    // must be re-applied.
    if (HasAuraType(SPELL_AURA_MOD_ROOT))
    {
        WorldPacket data2(SMSG_FORCE_MOVE_ROOT, 10);
        data2 << GetPackGUID();
        data2 << (uint32)2;
        SendMessageToSet(&data2, true);
    }

    SendEnchDurations(); // must be after add to map
    SendItemDurations(); // must be after add to map
}

void Player::SendUpdateToOutOfRangeGroupMembers()
{
    if (m_groupUpdateMask == GROUP_UPDATE_FLAG_NONE)
        return;
    if (Group* group = GetGroup())
        group->UpdatePlayerOutOfRange(this);

    m_groupUpdateMask = GROUP_UPDATE_FLAG_NONE;
    m_auraUpdateMask = 0;
    if (Pet* pet = GetPet())
        pet->ResetAuraUpdateMask();
}

void Player::SendTransferAborted(uint32 mapid, uint8 reason, uint8 arg)
{
    WorldPacket data(SMSG_TRANSFER_ABORTED, 4 + 2);
    data << uint32(mapid);
    data << uint8(reason); // transfer abort reason
    switch (reason)
    {
    case TRANSFER_ABORT_INSUF_EXPAN_LVL:
    case TRANSFER_ABORT_DIFFICULTY:
        data << uint8(arg);
        break;
    default
        : // possible not neaded (absent in 0.13, but add at backport for safe)
        data << uint8(0);
        break;
    }
    GetSession()->send_packet(std::move(data));
}

void Player::SendRaidGroupError(RaidGroupError err)
{
    WorldPacket data(SMSG_RAID_GROUP_ONLY, 8);
    data << uint32(0);
    data << uint32(err);
    GetSession()->send_packet(std::move(data));
}

void Player::SendInstanceResetWarning(uint32 mapid, uint32 time)
{
    // type of warning, based on the time remaining until reset
    uint32 type;
    if (time > 3600)
        type = RAID_INSTANCE_WELCOME;
    else if (time > 900 && time <= 3600)
        type = RAID_INSTANCE_WARNING_HOURS;
    else if (time > 300 && time <= 900)
        type = RAID_INSTANCE_WARNING_MIN;
    else
        type = RAID_INSTANCE_WARNING_MIN_SOON;

    WorldPacket data(SMSG_RAID_INSTANCE_MESSAGE, 4 + 4 + 4);
    data << uint32(type);
    data << uint32(mapid);
    data << uint32(time);
    GetSession()->send_packet(std::move(data));
}

void Player::ApplyRootHack()
{
    if (hasUnitState(UNIT_STAT_STUNNED | UNIT_STAT_ROOT | UNIT_STAT_CONFUSED |
                     UNIT_STAT_FLEEING))
    {
        unroot_hack_ticks_ = 0;
        return;
    }

    WorldPacket data(SMSG_FORCE_MOVE_ROOT, 8);
    data << GetPackGUID();
    data << uint32(0);
    GetSession()->send_packet(std::move(data));
    unroot_hack_ticks_ = 2;
}

void Player::resetSpells()
{
    // not need after this call
    if (HasAtLoginFlag(AT_LOGIN_RESET_SPELLS))
        RemoveAtLoginFlag(AT_LOGIN_RESET_SPELLS, true);

    // make full copy of map (spells removed and marked as deleted at another
    // spell remove
    // and we can't use original map for safe iterative with visit each spell at
    // loop end
    PlayerSpellMap smap = GetSpellMap();

    for (PlayerSpellMap::const_iterator iter = smap.begin(); iter != smap.end();
         ++iter)
        removeSpell(iter->first, false, false); // only iter->first can be
                                                // accessed, object by
                                                // iter->second can be deleted
                                                // already

    learnDefaultSpells();
    learnQuestRewardedSpells();
}

void Player::learnDefaultSpells()
{
    // learn default race/class spells
    PlayerInfo const* info =
        sObjectMgr::Instance()->GetPlayerInfo(getRace(), getClass());
    for (auto tspell : info->spell)
    {
        LOG_DEBUG(logging,
            "PLAYER (Class: %u Race: %u): Adding initial spell, id = %u",
            uint32(getClass()), uint32(getRace()), tspell);
        if (!IsInWorld()) // will send in INITIAL_SPELLS in list anyway at map
                          // add
            addSpell(tspell, true, true, true, false);
        else // but send in normal spell in game learn case
            learnSpell(tspell, true);
    }
}

void Player::learnQuestRewardedSpells(Quest const* quest)
{
    uint32 spell_id = quest->GetRewSpellCast();

    // skip quests without rewarded spell
    if (!spell_id)
        return;

    SpellEntry const* spellInfo = sSpellStore.LookupEntry(spell_id);
    if (!spellInfo)
        return;

    // check learned spells state
    bool found = false;
    for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        if (spellInfo->Effect[i] == SPELL_EFFECT_LEARN_SPELL &&
            !HasSpell(spellInfo->EffectTriggerSpell[i]))
        {
            found = true;
            break;
        }
    }

    // skip quests with not teaching spell or already known spell
    if (!found)
        return;

    // prevent learn non first rank unknown profession and second specialization
    // for same profession)
    uint32 learned_0 = spellInfo->EffectTriggerSpell[EFFECT_INDEX_0];
    if (sSpellMgr::Instance()->GetSpellRank(learned_0) > 1 &&
        !HasSpell(learned_0))
    {
        // not have first rank learned (unlearned prof?)
        uint32 first_spell =
            sSpellMgr::Instance()->GetFirstSpellInChain(learned_0);
        if (!HasSpell(first_spell))
            return;

        SpellEntry const* learnedInfo = sSpellStore.LookupEntry(learned_0);
        if (!learnedInfo)
            return;

        // specialization
        if (learnedInfo->Effect[EFFECT_INDEX_0] == SPELL_EFFECT_TRADE_SKILL &&
            learnedInfo->Effect[EFFECT_INDEX_1] == 0)
        {
            // search other specialization for same prof
            for (PlayerSpellMap::const_iterator itr = m_spells.begin();
                 itr != m_spells.end(); ++itr)
            {
                if (itr->second.state == PLAYERSPELL_REMOVED ||
                    itr->first == learned_0)
                    continue;

                SpellEntry const* itrInfo = sSpellStore.LookupEntry(itr->first);
                if (!itrInfo)
                    return;

                // compare only specializations
                if (itrInfo->Effect[EFFECT_INDEX_0] !=
                        SPELL_EFFECT_TRADE_SKILL ||
                    itrInfo->Effect[EFFECT_INDEX_1] != 0)
                    continue;

                // compare same chain spells
                if (sSpellMgr::Instance()->GetFirstSpellInChain(itr->first) !=
                    first_spell)
                    continue;

                // now we have 2 specialization, learn possible only if found is
                // lesser specialization rank
                if (!sSpellMgr::Instance()->IsHighRankOfSpell(
                        learned_0, itr->first))
                    return;
            }
        }
    }

    CastSpell(this, spell_id, true);
}

void Player::learnQuestRewardedSpells()
{
    // learn spells received from quest completing
    for (QuestStatusMap::const_iterator itr = mQuestStatus.begin();
         itr != mQuestStatus.end(); ++itr)
    {
        // skip no rewarded quests
        if (!itr->second.m_rewarded)
            continue;

        Quest const* quest =
            sObjectMgr::Instance()->GetQuestTemplate(itr->first);
        if (!quest)
            continue;

        learnQuestRewardedSpells(quest);
    }
}

void Player::learnSkillRewardedSpells(uint32 skill_id, uint32 skill_value)
{
    uint32 raceMask = getRaceMask();
    uint32 classMask = getClassMask();
    for (uint32 j = 0; j < sSkillLineAbilityStore.GetNumRows(); ++j)
    {
        SkillLineAbilityEntry const* pAbility =
            sSkillLineAbilityStore.LookupEntry(j);
        if (!pAbility || pAbility->skillId != skill_id ||
            pAbility->learnOnGetSkill !=
                ABILITY_LEARNED_ON_GET_PROFESSION_SKILL)
            continue;
        // Check race if set
        if (pAbility->racemask && !(pAbility->racemask & raceMask))
            continue;
        // Check class if set
        if (pAbility->classmask && !(pAbility->classmask & classMask))
            continue;

        if (sSpellStore.LookupEntry(pAbility->spellId))
        {
            // need unlearn spell
            if (skill_value < pAbility->req_skill_value)
                removeSpell(pAbility->spellId);
            // need learn
            else if (!IsInWorld())
                addSpell(pAbility->spellId, true, true, true, false);
            else
                learnSpell(pAbility->spellId, true);
        }
    }
}

void Player::SendAuraDurationsForTarget(Unit* target)
{
    target->loop_auras([this](AuraHolder* holder)
        {
            if (holder->GetAuraSlot() >= MAX_AURAS || holder->IsPassive() ||
                holder->GetCasterGuid() != GetObjectGuid())
                return true; // continue

            holder->SendAuraDurationForCaster(this);
            return true; // continue
        });
}

void Player::SetDailyQuestStatus(uint32 quest_id)
{
    for (uint32 quest_daily_idx = 0; quest_daily_idx < PLAYER_MAX_DAILY_QUESTS;
         ++quest_daily_idx)
    {
        if (!GetUInt32Value(PLAYER_FIELD_DAILY_QUESTS_1 + quest_daily_idx))
        {
            SetUInt32Value(
                PLAYER_FIELD_DAILY_QUESTS_1 + quest_daily_idx, quest_id);
            m_DailyQuestChanged = true;
            break;
        }
    }
}

void Player::ResetDailyQuestStatus()
{
    for (uint32 quest_daily_idx = 0; quest_daily_idx < PLAYER_MAX_DAILY_QUESTS;
         ++quest_daily_idx)
        SetUInt32Value(PLAYER_FIELD_DAILY_QUESTS_1 + quest_daily_idx, 0);

    // DB data deleted in caller
    m_DailyQuestChanged = false;
}

BattleGround* Player::GetBattleGround() const
{
    if (GetBattleGroundId() == 0)
        return nullptr;

    return sBattleGroundMgr::Instance()->GetBattleGround(
        GetBattleGroundId(), m_bgData.bgTypeID);
}

void Player::ClearBGData()
{
    // Clear everything but last join pos; that is still relevant
    auto fresh = BGData();
    fresh.joinPos = m_bgData.joinPos;
    m_bgData = fresh;
}

bool Player::InArena() const
{
    BattleGround* bg = GetBattleGround();
    if (!bg || !bg->isArena())
        return false;

    return true;
}

bool Player::GetBGAccessByLevel(BattleGroundTypeId bgTypeId) const
{
    // FIXME: This is not the way to do it, nor the place where this logic
    // should reside
    switch (bgTypeId)
    {
    case BATTLEGROUND_AV:
        return getLevel() >= 51;
    case BATTLEGROUND_WS:
        return getLevel() >= 10;
    case BATTLEGROUND_AB:
        return getLevel() >= 10;
    case BATTLEGROUND_EY:
        return getLevel() >= 61;
    default:
        return getLevel() >= 20; // Arenas
    }
    return false;
}

float Player::GetReputationPriceDiscount(Creature const* pCreature) const
{
    FactionTemplateEntry const* vendor_faction =
        pCreature->getFactionTemplateEntry();
    if (!vendor_faction || !vendor_faction->faction)
        return 1.0f;

    ReputationRank rank = GetReputationRank(vendor_faction->faction);
    if (rank <= REP_NEUTRAL)
        return 1.0f;

    return 1.0f - 0.05f * (rank - REP_NEUTRAL);
}

/**
 * Check spell availability for training base at
 SkillLineAbility/SkillRaceClassInfo data.
 * Checked allowed race/class and dependent from race/class allowed min level
 *
 * @param spell_id  checked spell id
 * @param pReqlevel if arg provided then function work in view mode (level check
 not applied but detected minlevel returned to var by arg pointer.
                    if arg not provided then considered train action mode and
 level checked
 * @return          true if spell available for show in trainer list (with skip
 level check) or training.
 */
bool Player::IsSpellFitByClassAndRace(
    uint32 spell_id, uint32* pReqlevel /*= NULL*/) const
{
    uint32 racemask = getRaceMask();
    uint32 classmask = getClassMask();

    SkillLineAbilityMapBounds bounds =
        sSpellMgr::Instance()->GetSkillLineAbilityMapBounds(spell_id);
    if (bounds.first == bounds.second)
        return true;

    for (auto _spell_idx = bounds.first; _spell_idx != bounds.second;
         ++_spell_idx)
    {
        SkillLineAbilityEntry const* abilityEntry = _spell_idx->second;
        // skip wrong race skills
        if (abilityEntry->racemask && (abilityEntry->racemask & racemask) == 0)
            continue;

        // skip wrong class skills
        if (abilityEntry->classmask &&
            (abilityEntry->classmask & classmask) == 0)
            continue;

        SkillRaceClassInfoMapBounds bounds =
            sSpellMgr::Instance()->GetSkillRaceClassInfoMapBounds(
                abilityEntry->skillId);
        for (auto itr = bounds.first; itr != bounds.second; ++itr)
        {
            SkillRaceClassInfoEntry const* skillRCEntry = itr->second;
            if ((skillRCEntry->raceMask & racemask) &&
                (skillRCEntry->classMask & classmask))
            {
                if (skillRCEntry->flags & ABILITY_SKILL_NONTRAINABLE)
                    return false;

                if (pReqlevel) // show trainers list case
                {
                    if (skillRCEntry->reqLevel)
                    {
                        *pReqlevel = skillRCEntry->reqLevel;
                        return true;
                    }
                }
                else // check availble case at train
                {
                    if (skillRCEntry->reqLevel &&
                        getLevel() < skillRCEntry->reqLevel)
                        return false;
                }
            }
        }

        return true;
    }

    return false;
}

bool Player::HasQuestForGO(int32 GOId) const
{
    for (int i = 0; i < MAX_QUEST_LOG_SIZE; ++i)
    {
        uint32 questid = GetQuestSlotQuestId(i);
        if (questid == 0)
            continue;

        auto qs_itr = mQuestStatus.find(questid);
        if (qs_itr == mQuestStatus.end())
            continue;

        QuestStatusData const& qs = qs_itr->second;

        if (qs.m_status == QUEST_STATUS_INCOMPLETE)
        {
            Quest const* qinfo =
                sObjectMgr::Instance()->GetQuestTemplate(questid);
            if (!qinfo)
                continue;

            if (GetGroup() && GetGroup()->isRaidGroup() &&
                !qinfo->IsAllowedInRaid())
                continue;

            for (int j = 0; j < QUEST_OBJECTIVES_COUNT; ++j)
            {
                if (qinfo->ReqCreatureOrGOId[j] >= 0) // skip non GO case
                    continue;

                if ((-1) * GOId == qinfo->ReqCreatureOrGOId[j] &&
                    qs.m_creatureOrGOcount[j] < qinfo->ReqCreatureOrGOCount[j])
                    return true;
            }
        }
    }
    return false;
}

void Player::UpdateForQuestWorldObjects()
{
    if (m_clientGUIDs.empty())
        return;

    UpdateData udata;
    for (const auto& elem : m_clientGUIDs)
    {
        if (elem.IsGameObject())
        {
            if (GameObject* obj = GetMap()->GetGameObject(elem))
                obj->BuildValuesUpdateBlockForPlayer(&udata, this);
        }
    }
    udata.SendPacket(GetSession());
}

void Player::SummonIfPossible(bool agree)
{
    if (!agree)
    {
        m_summon_expire = 0;
        return;
    }

    // We can not be summoned in spirit of redemption
    if (GetShapeshiftForm() == FORM_SPIRITOFREDEMPTION)
    {
        m_summon_expire = 0;
        return;
    }

    // expire and auto declined
    if (m_summon_expire < WorldTimer::time_no_syscall())
        return;

    // stop taxi flight at summon
    if (IsTaxiFlying())
    {
        movement_gens.remove_if([](auto* gen)
            {
                return gen->id() == movement::gen::flight;
            });
        m_taxi.ClearTaxiDestinations();
    }

    // drop flag at summon
    // this code can be reached only when GM is summoning player who carries
    // flag, because player should be immune to summoning spells when he carries
    // flag
    if (BattleGround* bg = GetBattleGround())
        bg->EventPlayerDroppedFlag(this);

    m_summon_expire = 0;

    TeleportTo(m_summon_mapid, m_summon_x, m_summon_y, m_summon_z, GetO());
}

bool Player::HasItemFitToSpellReqirements(
    SpellEntry const* spellInfo, Item const* ignoreItem)
{
    /*XXX*/
    if (spellInfo->EquippedItemClass < 0)
        return true;

    // scan other equipped items for same requirements (mostly 2 daggers/etc)
    // for optimize check 2 used cases only
    switch (spellInfo->EquippedItemClass)
    {
    case ITEM_CLASS_WEAPON:
    {
        for (int i = inventory::main_hand_e; i <= inventory::ranged_e; ++i)
            if (Item* item = storage().get(inventory::slot(
                    inventory::personal_slot, inventory::main_bag, i)))
                if (item != ignoreItem &&
                    item->IsFitToSpellRequirements(spellInfo))
                {
                    WeaponAttackType at =
                        item->slot().main_hand() ?
                            BASE_ATTACK :
                            (item->slot().off_hand() ? OFF_ATTACK :
                                                       RANGED_ATTACK);
                    if (CanUseEquippedWeapon(at))
                        return true;
                }
        break;
    }
    case ITEM_CLASS_ARMOR:
    {
        // tabard not have dependent spells
        for (int i = inventory::equipment_start; i <= inventory::back; ++i)
            if (Item* item = storage().get(inventory::slot(
                    inventory::personal_slot, inventory::main_bag, i)))
                if (item != ignoreItem &&
                    item->IsFitToSpellRequirements(spellInfo))
                    return true;

        // shields can be equipped to offhand slot
        if (Item* item = storage().get(inventory::slot(inventory::personal_slot,
                inventory::main_bag, inventory::off_hand_e)))
            if (item != ignoreItem && item->IsFitToSpellRequirements(spellInfo))
                return true;

        // ranged slot can have some armor subclasses
        if (Item* item = storage().get(inventory::slot(inventory::personal_slot,
                inventory::main_bag, inventory::ranged_e)))
            if (item != ignoreItem && item->IsFitToSpellRequirements(spellInfo))
                return true;

        break;
    }
    default:
        logging.error(
            "HasItemFitToSpellReqirements: Not handled spell requirement for "
            "item class %u",
            spellInfo->EquippedItemClass);
        break;
    }

    return false;
}

void Player::RemoveItemDependentAurasAndCasts(Item* item)
{
    loop_auras([this, item](AuraHolder* holder)
        {
            // skip passive (passive item-dependent spells work in another way)
            // and not self-applied auras
            const SpellEntry* spellInfo = holder->GetSpellProto();
            if (holder->IsPassive() ||
                holder->GetCasterGuid() != GetObjectGuid())
                return true; // continue

            // skip if not item dependent or have alternative item
            if (HasItemFitToSpellReqirements(spellInfo, item))
                return true; // continue

            // no alt item, remove aura
            RemoveAuraHolder(holder);

            return true; // continue
        });

    // currently casted spells can be dependent from item
    for (uint32 i = 0; i < CURRENT_MAX_SPELL; ++i)
        if (Spell* spell = GetCurrentSpell(CurrentSpellTypes(i)))
            if (spell->getState() != SPELL_STATE_DELAYED &&
                !HasItemFitToSpellReqirements(spell->m_spellInfo, item))
                InterruptSpell(CurrentSpellTypes(i));
}

uint32 Player::GetResurrectionSpellId()
{
    // search priceless resurrection possibilities
    uint32 prio = 0;
    uint32 spell_id = 0;
    const Auras& dummyAuras = GetAurasByType(SPELL_AURA_DUMMY);
    for (const auto& dummyAura : dummyAuras)
    {
        // Soulstone Resurrection                           // prio: 3 (max, non
        // death persistent)
        if (prio < 2 && (dummyAura)->GetSpellProto()->SpellVisual == 99 &&
            (dummyAura)->GetSpellProto()->SpellIconID == 92)
        {
            switch ((dummyAura)->GetId())
            {
            case 20707:
                spell_id = 3026;
                break; // rank 1
            case 20762:
                spell_id = 20758;
                break; // rank 2
            case 20763:
                spell_id = 20759;
                break; // rank 3
            case 20764:
                spell_id = 20760;
                break; // rank 4
            case 20765:
                spell_id = 20761;
                break; // rank 5
            case 27239:
                spell_id = 27240;
                break; // rank 6
            default:
                logging.error(
                    "Unhandled spell %u: S.Resurrection", (dummyAura)->GetId());
                continue;
            }

            prio = 3;
        }
        // Twisting Nether                                  // prio: 2 (max)
        else if ((dummyAura)->GetId() == 23701 && roll_chance_i(10))
        {
            prio = 2;
            spell_id = 23700;
        }
    }

    // Reincarnation (passive spell)                        // prio: 1
    if (prio < 1 && HasSpell(20608) && !HasSpellCooldown(21169) &&
        HasItemCount(17030, EFFECT_INDEX_1))
        spell_id = 21169;

    return spell_id;
}

// Used in triggers for check "Only to targets that grant experience or honor"
// req
bool Player::isHonorOrXPTarget(Unit* pVictim) const
{
    uint32 v_level = pVictim->getLevel();
    uint32 k_grey = MaNGOS::XP::GetGrayLevel(getLevel());

    // Victim level less gray level
    if (v_level <= k_grey)
        return false;

    if (pVictim->GetTypeId() == TYPEID_UNIT)
    {
        if (((Creature*)pVictim)->IsTotem() || ((Creature*)pVictim)->IsPet() ||
            ((Creature*)pVictim)->GetCreatureInfo()->flags_extra &
                CREATURE_FLAG_EXTRA_NO_XP_AT_KILL)
            return false;
    }
    return true;
}

void Player::RewardSinglePlayerAtKill(Unit* pVictim)
{
    if (unlikely(pVictim->GetTypeId() == TYPEID_UNIT &&
                 static_cast<Creature*>(pVictim)->GetCreatureType() ==
                     CREATURE_TYPE_CRITTER))
        return;

    bool PvP = pVictim->isCharmedOwnedByPlayerOrPlayer();
    uint32 xp = PvP ? 0 : MaNGOS::XP::Gain(this, pVictim);

    // honor from player tagets handled in Player::hk_distribute_honor()
    if (!PvP)
        RewardHonor(pVictim, 1);

    // xp and reputation only in !PvP case
    if (!PvP)
    {
        RewardReputation(pVictim, 1);
        GiveXP(xp, pVictim);

        if (Pet* pet = GetPet())
            pet->GivePetXP(xp);

        // normal creature (not pet/etc) can be only in !PvP case
        if (pVictim->GetTypeId() == TYPEID_UNIT)
            if (CreatureInfo const* normalInfo =
                    ObjectMgr::GetCreatureTemplate(pVictim->GetEntry()))
                KilledMonster(normalInfo, pVictim->GetObjectGuid());
    }
}

void Player::RewardPlayerAndGroupAtEvent(
    uint32 creature_id, WorldObject* pRewardSource)
{
    ObjectGuid creature_guid = pRewardSource->GetTypeId() == TYPEID_UNIT ?
                                   pRewardSource->GetObjectGuid() :
                                   ObjectGuid();

    // prepare data for near group iteration
    if (Group* group = GetGroup())
    {
        for (auto member : group->members(true))
        {
            if (!member->IsAtGroupRewardDistance(pRewardSource))
                continue; // member (alive or dead) or his corpse at req.
                          // distance

            // quest objectives updated only for alive group member or dead but
            // with not released body
            if (member->isAlive() ||
                !member->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_GHOST))
                member->KilledMonsterCredit(creature_id, creature_guid, true);
        }
    }
    else // if (!pGroup)
        KilledMonsterCredit(creature_id, creature_guid, true);
}

void Player::RewardPlayerAndGroupAtCast(
    WorldObject* pRewardSource, uint32 spellid)
{
    // prepare data for near group iteration
    if (Group* group = GetGroup())
    {
        for (auto member : group->members(true))
        {
            if (!member->IsAtGroupRewardDistance(pRewardSource))
                continue; // member (alive or dead) or his corpse at req.
                          // distance

            // quest objectives updated only for alive group member or dead but
            // with not released body
            if (member->isAlive() ||
                !member->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_GHOST))
                member->CastedCreatureOrGO(pRewardSource->GetEntry(),
                    pRewardSource->GetObjectGuid(), spellid, member == this);
        }
    }
    else // if (!pGroup)
        CastedCreatureOrGO(
            pRewardSource->GetEntry(), pRewardSource->GetObjectGuid(), spellid);
}

bool Player::IsAtGroupRewardDistance(WorldObject const* pRewardSource) const
{
    if (pRewardSource->IsWithinDistInMap(this,
            sWorld::Instance()->getConfig(CONFIG_FLOAT_GROUP_XP_DISTANCE)))
        return true;

    if (isAlive())
        return false;

    Corpse* corpse = GetCorpse();
    if (!corpse)
        return false;

    return pRewardSource->IsWithinDistInMap(
        corpse, sWorld::Instance()->getConfig(CONFIG_FLOAT_GROUP_XP_DISTANCE));
}

uint32 Player::GetBaseWeaponSkillValue(WeaponAttackType attType) const
{
    Item* item = GetWeaponForAttack(attType, true, true);

    // unarmed only with base attack
    if (attType != BASE_ATTACK && !item)
        return 0;

    // weapon skill or (unarmed for base attack)
    uint32 skill = item ? item->GetSkill() : uint32(SKILL_UNARMED);
    return GetBaseSkillValue(skill);
}

void Player::ResurectUsingRequestData()
{
    // Teleport before resurrecting, otherwise the player might get attacked
    // from creatures near his corpse
    TeleportTo(m_resurrectMap, m_resurrectX, m_resurrectY, m_resurrectZ, GetO(),
        TELE_TO_RESURRECT);

    // Resurrect after the teleport has finished
    ScheduleDelayedOperation(DELAYED_RESURRECT_PLAYER);
}

void Player::SetClientControl(Unit* target, uint8 allowMove)
{
    WorldPacket data(
        SMSG_CLIENT_CONTROL_UPDATE, target->GetPackGUID().size() + 1);
    data << target->GetPackGUID();
    data << uint8(allowMove);
    GetSession()->send_packet(std::move(data));

    // Resend speeds & movement data of controlled unit
    if (target != this)
    {
        // TODO: Can we use Object::BuildMovementUpdateBlock?

        WorldPacket data(MSG_MOVE_SET_WALK_SPEED, 64);
        data << target->GetPackGUID();
        data << target->m_movementInfo;
        data << float(target->GetSpeed(MOVE_WALK));
        SendMessageToSet(&data, true);

        data.initialize(MSG_MOVE_SET_RUN_SPEED, 64);
        data << target->GetPackGUID();
        data << target->m_movementInfo;
        data << float(target->GetSpeed(MOVE_RUN));
        SendMessageToSet(&data, true);
    }

    in_control_ = allowMove;
}

void Player::UpdateZoneDependentAuras()
{
    // Some spells applied at enter into zone (with subzones), aura removed in
    // UpdateAreaDependentAuras that called always at zone->area update
    SpellAreaForAreaMapBounds saBounds =
        sSpellMgr::Instance()->GetSpellAreaForAreaMapBounds(cached_zone_);
    for (auto itr = saBounds.first; itr != saBounds.second; ++itr)
        if (itr->second->autocast &&
            itr->second->IsFitToRequirements(this, cached_zone_, 0))
            if (!has_aura(itr->second->spellId))
                CastSpell(this, itr->second->spellId, true);
}

void Player::UpdateAreaDependentAuras()
{
    // remove auras from spells with area limitations
    for (auto& elem : m_auraHolders)
    {
        if (elem.second->IsDisabled())
            continue;
        // use cached_zone_ for speed: UpdateArea called from UpdateZone or
        // instead UpdateZone in both cases cached_zone_ up-to-date
        if (sSpellMgr::Instance()->GetSpellAllowedInLocationError(
                elem.second->GetSpellProto(), GetMapId(), cached_zone_,
                cached_area_, this) != SPELL_CAST_OK)
            RemoveAuraHolder(elem.second);
    }

    // some auras applied at subzone enter
    SpellAreaForAreaMapBounds saBounds =
        sSpellMgr::Instance()->GetSpellAreaForAreaMapBounds(cached_area_);
    for (auto itr = saBounds.first; itr != saBounds.second; ++itr)
        if (itr->second->autocast &&
            itr->second->IsFitToRequirements(this, cached_zone_, cached_area_))
            if (!has_aura(itr->second->spellId))
                CastSpell(this, itr->second->spellId, true);

    // Apply item equip-auras that are zone or area restricted
    for (auto itr =
             storage().begin(inventory::personal_storage::iterator::all_body);
         itr != storage().end(); ++itr)
    {
        auto proto = (*itr)->GetProto();
        for (int i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
        {
            // we only care about equip or store spells
            if (!proto->Spells[i].SpellId ||
                (proto->Spells[i].SpellTrigger != ITEM_SPELLTRIGGER_ON_EQUIP &&
                    proto->Spells[i].SpellTrigger !=
                        ITEM_SPELLTRIGGER_ON_STORE))
                continue;

            // for equip spells, item needs to be equipped
            if (proto->Spells[i].SpellTrigger == ITEM_SPELLTRIGGER_ON_EQUIP &&
                !(*itr)->IsEquipped())
                continue;

            // only check spells with area restrictions
            auto info = sSpellStore.LookupEntry(proto->Spells[i].SpellId);
            if (!info || info->AreaId == 0)
                continue;

            // check so it can be applied
            if (sSpellMgr::Instance()->GetSpellAllowedInLocationError(info,
                    GetMapId(), cached_zone_, cached_area_,
                    this) != SPELL_CAST_OK)
                continue;

            // check so we don't already have it
            if (has_aura(info->Id))
                continue;

            // apply aura
            CastSpell(this, info->Id, true, *itr);
        }
    }
}

struct UpdateZoneDependentPetsHelper
{
    explicit UpdateZoneDependentPetsHelper(
        Player* _owner, uint32 zone, uint32 area)
      : owner(_owner), zone_id(zone), area_id(area)
    {
    }
    void operator()(Unit* unit) const
    {
        if (unit->GetTypeId() == TYPEID_UNIT && ((Creature*)unit)->IsPet() &&
            !((Pet*)unit)->IsPermanentPetFor(owner))
            if (uint32 spell_id = unit->GetUInt32Value(UNIT_CREATED_BY_SPELL))
                if (SpellEntry const* spellEntry =
                        sSpellStore.LookupEntry(spell_id))
                    if (sSpellMgr::Instance()->GetSpellAllowedInLocationError(
                            spellEntry, owner->GetMapId(), zone_id, area_id,
                            owner) != SPELL_CAST_OK)
                        ((Pet*)unit)->Unsummon(PET_SAVE_AS_DELETED, owner);
    }
    Player* owner;
    uint32 zone_id;
    uint32 area_id;
};

void Player::UpdateZoneDependentPets()
{
    // check pet (permanent pets ignored), minipet, guardians (including
    // protector)
    CallForAllControlledUnits(
        UpdateZoneDependentPetsHelper(this, cached_zone_, cached_area_),
        CONTROLLED_PET | CONTROLLED_GUARDIANS | CONTROLLED_MINIPET);
}

uint32 Player::GetCorpseReclaimDelay(bool pvp) const
{
    if ((pvp &&
            !sWorld::Instance()->getConfig(
                CONFIG_BOOL_DEATH_CORPSE_RECLAIM_DELAY_PVP)) ||
        (!pvp &&
            !sWorld::Instance()->getConfig(
                CONFIG_BOOL_DEATH_CORPSE_RECLAIM_DELAY_PVE)))
    {
        return copseReclaimDelay[0];
    }

    time_t now = WorldTimer::time_no_syscall();
    // 0..2 full period
    uint32 count = (now < m_deathExpireTime) ?
                       uint32((m_deathExpireTime - now) / DEATH_EXPIRE_STEP) :
                       0;
    return copseReclaimDelay[count];
}

void Player::UpdateCorpseReclaimDelay()
{
    bool pvp = m_ExtraFlags & PLAYER_EXTRA_PVP_DEATH;

    if ((pvp &&
            !sWorld::Instance()->getConfig(
                CONFIG_BOOL_DEATH_CORPSE_RECLAIM_DELAY_PVP)) ||
        (!pvp &&
            !sWorld::Instance()->getConfig(
                CONFIG_BOOL_DEATH_CORPSE_RECLAIM_DELAY_PVE)))
        return;

    time_t now = WorldTimer::time_no_syscall();
    if (now < m_deathExpireTime)
    {
        // full and partly periods 1..3
        uint32 count =
            uint32((m_deathExpireTime - now) / DEATH_EXPIRE_STEP + 1);
        if (count < MAX_DEATH_COUNT)
            m_deathExpireTime = now + (count + 1) * DEATH_EXPIRE_STEP;
        else
            m_deathExpireTime = now + MAX_DEATH_COUNT * DEATH_EXPIRE_STEP;
    }
    else
        m_deathExpireTime = now + DEATH_EXPIRE_STEP;
}

void Player::SendCorpseReclaimDelay(bool load)
{
    Corpse* corpse = GetCorpse();
    if (!corpse)
        return;

    uint32 delay;
    if (load)
    {
        if (corpse->GetGhostTime() > m_deathExpireTime)
            return;

        bool pvp = corpse->GetType() == CORPSE_RESURRECTABLE_PVP;

        uint32 count;
        if ((pvp &&
                sWorld::Instance()->getConfig(
                    CONFIG_BOOL_DEATH_CORPSE_RECLAIM_DELAY_PVP)) ||
            (!pvp &&
                sWorld::Instance()->getConfig(
                    CONFIG_BOOL_DEATH_CORPSE_RECLAIM_DELAY_PVE)))
        {
            count = uint32(m_deathExpireTime - corpse->GetGhostTime()) /
                    DEATH_EXPIRE_STEP;
            if (count >= MAX_DEATH_COUNT)
                count = MAX_DEATH_COUNT - 1;
        }
        else
            count = 0;

        time_t expected_time =
            corpse->GetGhostTime() + copseReclaimDelay[count];

        time_t now = WorldTimer::time_no_syscall();
        if (now >= expected_time)
            return;

        delay = uint32(expected_time - now);
    }
    else
        delay = GetCorpseReclaimDelay(
            corpse->GetType() == CORPSE_RESURRECTABLE_PVP);

    //! corpse reclaim delay 30 * 1000ms or longer at often deaths
    WorldPacket data(SMSG_CORPSE_RECLAIM_DELAY, 4);
    data << uint32(delay * IN_MILLISECONDS);
    GetSession()->send_packet(std::move(data));
}

Player* Player::GetNextRandomRaidMember(float radius)
{
    Group* group = GetGroup();
    if (!group)
        return nullptr;

    std::vector<Player*> nearMembers;
    nearMembers.reserve(group->GetMembersCount());

    for (auto member : group->members(true))
    {
        // IsHostileTo check duel and controlled by enemy
        if (member != this && IsWithinDistInMap(member, radius) &&
            !member->HasInvisibilityAura() && !IsHostileTo(member))
            nearMembers.push_back(member);
    }

    if (nearMembers.empty())
        return nullptr;

    uint32 randTarget = urand(0, nearMembers.size() - 1);
    return nearMembers[randTarget];
}

PartyResult Player::CanUninviteFromGroup() const
{
    const Group* grp = GetGroup();
    if (!grp)
        return ERR_NOT_IN_GROUP;

    if (!grp->IsLeader(GetObjectGuid()) && !grp->IsAssistant(GetObjectGuid()))
        return ERR_NOT_LEADER;

    if (InBattleGround())
        return ERR_INVITE_RESTRICTED;

    return ERR_PARTY_RESULT_OK;
}

void Player::SetBattleGroundRaid(Group* group, int8 subgroup)
{
    // we must move references from m_group to m_originalGroup
    SetOriginalGroup(GetGroup(), GetSubGroup());

    m_group.unlink();
    m_group.link(group, this);
    m_group.setSubGroup((uint8)subgroup);
}

void Player::RemoveFromBattleGroundRaid()
{
    // remove existing reference
    m_group.unlink();
    if (Group* group = GetOriginalGroup())
    {
        m_group.link(group, this);
        m_group.setSubGroup(GetOriginalSubGroup());
    }
    SetOriginalGroup(nullptr);
}

void Player::SetOriginalGroup(Group* group, int8 subgroup)
{
    if (group == nullptr)
        m_originalGroup.unlink();
    else
    {
        // never use SetOriginalGroup without a subgroup unless you specify NULL
        // for group
        assert(subgroup >= 0);
        m_originalGroup.link(group, this);
        m_originalGroup.setSubGroup((uint8)subgroup);
    }
}

void Player::UpdateUnderwaterState()
{
    GridMapLiquidData liquid_status;
    GridMapLiquidStatus res = GetLiquidStatus(MAP_ALL_LIQUIDS, &liquid_status);
    if (!res)
    {
        m_MirrorTimerFlags &= ~(UNDERWATER_INWATER | UNDERWATER_INLAVA |
                                  UNDERWATER_INSLIME | UNDERWATER_INDARKWATER);
        if (m_lastLiquid && m_lastLiquid->SpellId)
            remove_auras(m_lastLiquid->SpellId);
        m_lastLiquid = nullptr;
        return;
    }

    if (uint32 liqEntry = liquid_status.entry)
    {
        LiquidTypeEntry const* liquid = sLiquidTypeStore.LookupEntry(liqEntry);
        if (m_lastLiquid && m_lastLiquid->SpellId &&
            m_lastLiquid->Id != liqEntry)
            remove_auras(m_lastLiquid->SpellId);

        if (liquid && liquid->SpellId)
        {
            if (res & (LIQUID_MAP_UNDER_WATER | LIQUID_MAP_IN_WATER))
            {
                if (!has_aura(liquid->SpellId))
                    CastSpell(this, liquid->SpellId, true);
            }
            else
                remove_auras(liquid->SpellId);
        }

        m_lastLiquid = liquid;
    }
    else if (m_lastLiquid && m_lastLiquid->SpellId)
    {
        remove_auras(m_lastLiquid->SpellId);
        m_lastLiquid = nullptr;
    }

    // All liquids type - check under water position
    if (liquid_status.type_flags &
        (MAP_LIQUID_TYPE_WATER | MAP_LIQUID_TYPE_OCEAN | MAP_LIQUID_TYPE_MAGMA |
            MAP_LIQUID_TYPE_SLIME))
    {
        if (res & LIQUID_MAP_UNDER_WATER)
            m_MirrorTimerFlags |= UNDERWATER_INWATER;
        else
            m_MirrorTimerFlags &= ~UNDERWATER_INWATER;
    }

    // Allow travel in dark water on taxi or transport
    if ((liquid_status.type_flags & MAP_LIQUID_TYPE_DARK_WATER) &&
        !IsTaxiFlying() && !GetTransport())
        m_MirrorTimerFlags |= UNDERWATER_INDARKWATER;
    else
        m_MirrorTimerFlags &= ~UNDERWATER_INDARKWATER;

    // in lava check, anywhere in lava level
    if (liquid_status.type_flags & MAP_LIQUID_TYPE_MAGMA)
    {
        if (res & (LIQUID_MAP_UNDER_WATER | LIQUID_MAP_IN_WATER |
                      LIQUID_MAP_WATER_WALK))
            m_MirrorTimerFlags |= UNDERWATER_INLAVA;
        else
            m_MirrorTimerFlags &= ~UNDERWATER_INLAVA;
    }
    // in slime check, anywhere in slime level
    if (liquid_status.type_flags & MAP_LIQUID_TYPE_SLIME)
    {
        if (res & (LIQUID_MAP_UNDER_WATER | LIQUID_MAP_IN_WATER |
                      LIQUID_MAP_WATER_WALK))
            m_MirrorTimerFlags |= UNDERWATER_INSLIME;
        else
            m_MirrorTimerFlags &= ~UNDERWATER_INSLIME;
    }
}

void Player::SetCanParry(bool value)
{
    if (m_canParry == value)
        return;

    m_canParry = value;
    UpdateParryPercentage();
}

void Player::SetCanBlock(bool value)
{
    if (m_canBlock == value)
        return;

    m_canBlock = value;
    UpdateBlockPercentage();
}

void Player::SetCanDualWield(bool value)
{
    m_canDualWield = value;

    // Nothing changes if value becomes true
    if (value == true)
        return;

    // If it becomes false we either need to unequip the off-hand item, or mail
    // it if we can't unequip it
    inventory::slot oh_slot = inventory::slot(
        inventory::personal_slot, inventory::main_bag, inventory::off_hand_e);
    if (Item* off_hand = storage().get(oh_slot))
    {
        uint32 it = off_hand->GetProto()->InventoryType;
        if (it != INVTYPE_WEAPON && it != INVTYPE_WEAPONOFFHAND)
            return;
        inventory::slot dst = storage().first_empty_slot_for(off_hand);
        if (!dst.valid() || storage().swap(dst, oh_slot) != EQUIP_ERR_OK)
        {
            inventory::transaction trans(false);
            trans.remove(off_hand);
            if (storage().finalize(trans)) // Should never be able to fail
            {
                MailDraft d(
                    GetSession()->GetMangosString(LANG_NOT_EQUIPPED_ITEM));
                d.AddItem(off_hand);
                d.SendMailTo(this, MailSender(this, MAIL_STATIONERY_GM),
                    MAIL_CHECK_MASK_COPIED);
            }
        }
    }
}

bool Player::CanUseBattleGroundObject()
{
    // TODO : some spells gives player ForceReaction to one faction
    // (ReputationMgr::ApplyForceReaction)
    // maybe gameobject code should handle that ForceReaction usage
    return ( // InBattleGround() &&                          // in battleground
             // - not need, check in other cases
        //!IsMounted() && - not correct, player is dismounted when he clicks on
        // flag
        // player cannot use object when he is invulnerable (immune)
        !isTotalImmune() &&                       // not totally immune
        !has_aura(SPELL_RECENTLY_DROPPED_FLAG) && // can't pickup
        isAlive()                                 // live player
        );
}

bool Player::CanCaptureTowerPoint()
{
    return (!HasStealthAura() &&      // not stealthed
            !HasInvisibilityAura() && // not invisible
            isAlive()                 // live player
        );
}

bool Player::isTotalImmune()
{
    const Auras& immune = GetAurasByType(SPELL_AURA_SCHOOL_IMMUNITY);

    uint32 immuneMask = 0;
    for (const auto& elem : immune)
    {
        immuneMask |= (elem)->GetModifier()->m_miscvalue;
        if (immuneMask & SPELL_SCHOOL_MASK_ALL) // total immunity
            return true;
    }
    return false;
}

bool Player::HasTitle(uint32 bitIndex) const
{
    if (bitIndex > MAX_TITLE_INDEX)
        return false;

    uint32 fieldIndexOffset = bitIndex / 32;
    uint32 flag = 1 << (bitIndex % 32);
    return HasFlag(PLAYER__FIELD_KNOWN_TITLES + fieldIndexOffset, flag);
}

void Player::SetTitle(CharTitlesEntry const* title, bool lost)
{
    uint32 fieldIndexOffset = title->bit_index / 32;
    uint32 flag = 1 << (title->bit_index % 32);

    if (lost)
    {
        if (!HasFlag(PLAYER__FIELD_KNOWN_TITLES + fieldIndexOffset, flag))
            return;

        RemoveFlag(PLAYER__FIELD_KNOWN_TITLES + fieldIndexOffset, flag);
    }
    else
    {
        if (HasFlag(PLAYER__FIELD_KNOWN_TITLES + fieldIndexOffset, flag))
            return;

        SetFlag(PLAYER__FIELD_KNOWN_TITLES + fieldIndexOffset, flag);
    }

    WorldPacket data(SMSG_TITLE_EARNED, 4 + 4);
    data << uint32(title->bit_index);
    data << uint32(lost ? 0 : 1); // 1 - earned, 0 - lost
    GetSession()->send_packet(std::move(data));
}

void Player::UpdateZoneAreaCache()
{
    float x, y, z;
    GetPosition(x, y, z);

    uint32 new_zone, new_area;
    GetTerrain()->GetZoneAndAreaId(new_zone, new_area, x, y, z);

    // UpdateZone() and UpdateArea() will changed cahce values
    if (new_zone != cached_zone_)
        UpdateZone(new_zone, new_area); // will also invoke UpdateArea()
    else if (new_area != cached_area_)
        UpdateArea(new_area);
}

G3D::Vector3 Player::GetShadowstepPoint(Unit* target)
{
    auto pos = target->GetPoint(M_PI_F, 2.0f, true);

    // Case: Target is standing with back against the end of a cliff, bridge or
    //        wall
    if (target->GetDistance(pos.x, pos.y, pos.z) < 1.5f)
    {
        auto far_away2d = target->GetPoint2d(M_PI_F, 5.0f);
        auto far_away = G3D::Vector3(far_away2d.x, far_away2d.y, GetZ());
        auto to = target->GetO();
        // If LOS is not blocked and ADT is below target Z: we can use 2 yards
        // away exactly
        if (IsWithinLOS(far_away.x, far_away.y, far_away.z + 2.0f))
        {
            auto new_pos =
                G3D::Vector3(target->GetX() + 2.0f * cos(to + M_PI_F),
                    target->GetY() + 2.0f * sin(to + M_PI_F), target->GetZ());
            if (GetTerrain()->GetHeightStatic(
                    new_pos.x, new_pos.y, new_pos.z, false) -
                    0.5f <
                new_pos.z)
                return new_pos;
        }
    }

    // Good ol' pos works fine
    return pos;
}

uint32 Player::CalculateTalentsPoints() const
{
    uint32 talentPointsForLevel = getLevel() < 10 ? 0 : getLevel() - 9;
    return uint32(talentPointsForLevel *
                  sWorld::Instance()->getConfig(CONFIG_FLOAT_RATE_TALENT));
}

struct DoPlayerLearnSpell
{
    DoPlayerLearnSpell(Player& _player) : player(_player) {}
    void operator()(uint32 spell_id) { player.learnSpell(spell_id, false); }
    Player& player;
};

void Player::learnSpellHighRank(uint32 spellid)
{
    learnSpell(spellid, false);

    DoPlayerLearnSpell worker(*this);
    sSpellMgr::Instance()->doForHighRanks(spellid, worker);
}

void Player::_LoadSkills(QueryResult* result)
{
    //                                                           0      1      2
    // SetPQuery(PLAYER_LOGIN_QUERY_LOADSKILLS,          "SELECT skill, value,
    // max FROM character_skills WHERE guid = '%u'", GUID_LOPART(m_guid));

    uint32 count = 0;
    if (result)
    {
        do
        {
            Field* fields = result->Fetch();

            uint16 skill = fields[0].GetUInt16();
            uint16 value = fields[1].GetUInt16();
            uint16 max = fields[2].GetUInt16();

            SkillLineEntry const* pSkill = sSkillLineStore.LookupEntry(skill);
            if (!pSkill)
            {
                logging.error("Character %u has skill %u that does not exist.",
                    GetGUIDLow(), skill);
                continue;
            }

            // set fixed skill ranges
            switch (GetSkillRangeType(pSkill, false))
            {
            case SKILL_RANGE_LANGUAGE: // 300..300
                value = max = 300;
                break;
            case SKILL_RANGE_MONO: // 1..1, grey monolite bar
                value = max = 1;
                break;
            default:
                break;
            }

            if (value == 0)
            {
                logging.error(
                    "Character %u has skill %u with value 0. Will be deleted.",
                    GetGUIDLow(), skill);
                CharacterDatabase.PExecute(
                    "DELETE FROM character_skills WHERE guid = '%u' AND skill "
                    "= '%u' ",
                    GetGUIDLow(), skill);
                continue;
            }

            SetUInt32Value(PLAYER_SKILL_INDEX(count), MAKE_PAIR32(skill, 0));
            SetUInt32Value(
                PLAYER_SKILL_VALUE_INDEX(count), MAKE_SKILL_VALUE(value, max));
            SetUInt32Value(PLAYER_SKILL_BONUS_INDEX(count), 0);

            mSkillStatus.insert(SkillStatusMap::value_type(
                skill, SkillStatusData(count, SKILL_UNCHANGED)));

            learnSkillRewardedSpells(skill, value);

            ++count;

            if (count >= PLAYER_MAX_SKILLS) // client limit
            {
                logging.error("Character %u has more than %u skills.",
                    GetGUIDLow(), PLAYER_MAX_SKILLS);
                break;
            }
        } while (result->NextRow());
        delete result;
    }

    for (; count < PLAYER_MAX_SKILLS; ++count)
    {
        SetUInt32Value(PLAYER_SKILL_INDEX(count), 0);
        SetUInt32Value(PLAYER_SKILL_VALUE_INDEX(count), 0);
        SetUInt32Value(PLAYER_SKILL_BONUS_INDEX(count), 0);
    }
}

void Player::HandleFall(MovementInfo const& movementInfo)
{
    // calculate total z distance of the fall
    float z_diff = m_lastFallZ - movementInfo.pos.z;
    LOG_DEBUG(logging, "zDiff = %f", z_diff);

    // Players with low fall distance, Feather Fall or physical immunity
    // (charges used) are ignored
    // 14.57 can be calculated by resolving damageperc formula below to 0
    if (z_diff >= 14.57f && !isDead() && !isGameMaster() &&
        !HasMovementFlag(MOVEFLAG_ONTRANSPORT) &&
        !HasAuraType(SPELL_AURA_HOVER) &&
        !HasAuraType(SPELL_AURA_FEATHER_FALL) && !HasAuraType(SPELL_AURA_FLY) &&
        !IsImmunedToDamage(SPELL_SCHOOL_MASK_NORMAL))
    {
        // Safe fall, fall height reduction
        int32 safe_fall = GetTotalAuraModifier(SPELL_AURA_SAFE_FALL);

        float damageperc = 0.018f * (z_diff - safe_fall) - 0.2426f;

        if (damageperc > 0)
        {
            uint32 damage = (uint32)(
                damageperc * GetMaxHealth() *
                sWorld::Instance()->getConfig(CONFIG_FLOAT_RATE_DAMAGE_FALL));

            float height = movementInfo.pos.z;
            UpdateAllowedPositionZ(
                movementInfo.pos.x, movementInfo.pos.y, height);

            if (damage > 0)
            {
                // Prevent fall damage from being more than the player maximum
                // health
                if (damage > GetMaxHealth())
                    damage = GetMaxHealth();

                // Gust of Wind
                if (has_aura(43621, SPELL_AURA_DUMMY))
                    damage = GetMaxHealth() / 2;

                EnvironmentalDamage(DAMAGE_FALL, damage);
            }

            // Z given by moveinfo, LastZ, FallTime, WaterZ, MapZ, Damage,
            // Safefall reduction
            LOG_DEBUG(logging,
                "FALLDAMAGE z=%f sz=%f pZ=%f FallTime=%d mZ=%f damage=%d SF=%d",
                movementInfo.pos.z, height, GetZ(), movementInfo.fallTime,
                height, damage, safe_fall);
        }
    }
}

void Player::LearnTalent(uint32 talentId, uint32 talentRank)
{
    uint32 CurTalentPoints = GetFreeTalentPoints();

    if (CurTalentPoints == 0)
        return;

    if (talentRank >= MAX_TALENT_RANK)
        return;

    TalentEntry const* talentInfo = sTalentStore.LookupEntry(talentId);

    if (!talentInfo)
        return;

    TalentTabEntry const* talentTabInfo =
        sTalentTabStore.LookupEntry(talentInfo->TalentTab);

    if (!talentTabInfo)
        return;

    // prevent learn talent for different class (cheating)
    if ((getClassMask() & talentTabInfo->ClassMask) == 0)
        return;

    // find current max talent rank
    uint32 curtalent_maxrank = 0;
    for (int32 k = MAX_TALENT_RANK - 1; k > -1; --k)
    {
        if (talentInfo->RankID[k] && HasSpell(talentInfo->RankID[k]))
        {
            curtalent_maxrank = k + 1;
            break;
        }
    }

    // we already have same or higher talent rank learned
    if (curtalent_maxrank >= (talentRank + 1))
        return;

    // check if we have enough talent points
    if (CurTalentPoints < (talentRank - curtalent_maxrank + 1))
        return;

    // Check if it requires another talent
    if (talentInfo->DependsOn > 0)
    {
        if (TalentEntry const* depTalentInfo =
                sTalentStore.LookupEntry(talentInfo->DependsOn))
        {
            bool hasEnoughRank = false;
            for (int i = talentInfo->DependsOnRank; i < MAX_TALENT_RANK; ++i)
            {
                if (depTalentInfo->RankID[i] != 0)
                    if (HasSpell(depTalentInfo->RankID[i]))
                        hasEnoughRank = true;
            }

            if (!hasEnoughRank)
                return;
        }
    }

    // Check if it requires spell
    if (talentInfo->DependsOnSpell && !HasSpell(talentInfo->DependsOnSpell))
        return;

    // Find out how many points we have in this field
    uint32 spentPoints = 0;

    uint32 tTab = talentInfo->TalentTab;
    if (talentInfo->Row > 0)
    {
        unsigned int numRows = sTalentStore.GetNumRows();
        for (unsigned int i = 0; i < numRows; ++i) // Loop through all talents.
        {
            // Someday, someone needs to revamp
            const TalentEntry* tmpTalent = sTalentStore.LookupEntry(i);
            if (tmpTalent) // the way talents are tracked
            {
                if (tmpTalent->TalentTab == tTab)
                {
                    for (int j = 0; j < MAX_TALENT_RANK; ++j)
                    {
                        if (tmpTalent->RankID[j] != 0)
                        {
                            if (HasSpell(tmpTalent->RankID[j]))
                            {
                                spentPoints += j + 1;
                            }
                        }
                    }
                }
            }
        }
    }

    // not have required min points spent in talent tree
    if (spentPoints < (talentInfo->Row * MAX_TALENT_RANK))
        return;

    // spell not set in talent.dbc
    uint32 spellid = talentInfo->RankID[talentRank];
    if (spellid == 0)
    {
        logging.error("Talent.dbc have for talent: %u Rank: %u spell id = 0",
            talentId, talentRank);
        return;
    }

    // already known
    if (HasSpell(spellid))
        return;

    // learn! (other talent ranks will unlearned at learning)
    learnSpell(spellid, false);
    LOG_DEBUG(logging, "TalentID: %u Rank: %u Spell: %u\n", talentId,
        talentRank, spellid);
}

void Player::UpdateFallInformationIfNeed(
    MovementInfo const& minfo, uint16 opcode)
{
    if (m_lastFallTime >= minfo.fallTime || m_lastFallZ <= minfo.pos.z ||
        opcode == MSG_MOVE_FALL_LAND)
        SetFallInformation(minfo.fallTime, minfo.pos.z);
}

void Player::UnsummonPetTemporaryIfAny()
{
    Pet* pet = GetPet();
    if (!pet)
        return;

    if (!pet->isAlive())
    {
        pet->Unsummon(PET_SAVE_NOT_IN_SLOT, this);
        return;
    }

    if (!m_temporaryUnsummonedPetNumber && pet->isControlled() &&
        !pet->isTemporarySummoned())
        m_temporaryUnsummonedPetNumber = pet->GetCharmInfo()->GetPetNumber();

    pet->Unsummon(PET_SAVE_AS_CURRENT, this);
}

void Player::ResummonPetTemporaryUnSummonedIfAny()
{
    if (!m_temporaryUnsummonedPetNumber)
        return;

    // not resummon in not appropriate state
    if (IsPetNeedBeTemporaryUnsummoned())
        return;

    if (GetPetGuid())
        return;

    auto NewPet = new Pet;
    if (!NewPet->LoadPetFromDB(this, 0, m_temporaryUnsummonedPetNumber, true))
        delete NewPet;

    m_temporaryUnsummonedPetNumber = 0;
}

void Player::_SaveBGData()
{
    static SqlStatementID delBGData;
    static SqlStatementID insBGData;

    // delete even if we have nothing to save, in case we commited join pos
    // because the player relogged while in queue
    SqlStatement stmt = CharacterDatabase.CreateStatement(
        delBGData, "DELETE FROM character_battleground_data WHERE guid = ?");
    stmt.PExecute(GetGUIDLow());

    // If we're not in a created battleground instance, then we have to be in
    // queue or have a pending invite for a write to take place
    if (m_bgData.bgInstanceID == 0 &&
        sBattlefieldQueue::Instance()
            ->current_queues(GetObjectGuid())
            .empty() &&
        !sBattleGroundMgr::Instance()->has_pending_invite(GetObjectGuid()))
        return;

    // When in queue we only write the battleground data of the latest
    // battleground we queued to
    stmt = CharacterDatabase.CreateStatement(insBGData,
        "INSERT INTO character_battleground_data (guid, instance_id, team, "
        "join_x, join_y, join_z, join_o, join_map) VALUES (?, ?, ?, ?, ?, ?, "
        "?, ?)");
    stmt.addUInt32(GetGUIDLow());
    stmt.addUInt32(m_bgData.bgInstanceID);
    stmt.addUInt32(uint32(m_bgData.bgTeam));
    stmt.addFloat(m_bgData.joinPos.coord_x);
    stmt.addFloat(m_bgData.joinPos.coord_y);
    stmt.addFloat(m_bgData.joinPos.coord_z);
    stmt.addFloat(m_bgData.joinPos.orientation);
    stmt.addUInt32(m_bgData.joinPos.mapid);

    stmt.Execute();
}

void Player::_SavePetDbDatas()
{
    for (auto itr = _pet_store.begin(); itr != _pet_store.end();)
    {
        auto& data = *itr;
        if (data.deleted)
        {
            drop_pet_db_data(data.guid);
            itr = _pet_store.erase(itr);
            continue;
        }
        else if (data.needs_save)
        {
            write_pet_db_data(data);
            data.needs_save = false;
        }
        ++itr;
    }
}

void Player::write_pet_db_data(const PetDbData& data)
{
    static SqlStatementID del;
    static SqlStatementID ins;

    SqlStatement del_stmt = CharacterDatabase.CreateStatement(
        del, "DELETE FROM character_pet WHERE id = ?");
    del_stmt.PExecute(data.guid);

    // character_pet
    SqlStatement ins_stmt = CharacterDatabase.CreateStatement(ins,
        "INSERT INTO character_pet (id, entry, owner, modelid, CreatedBySpell, "
        "PetType, level, exp, Reactstate, loyaltypoints, loyalty, trainpoint, "
        "name, renamed, slot, curhealth, curmana, curhappiness, savetime, "
        "resettalents_cost, resettalents_time, abdata, teachspelldata, dead) "
        "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, "
        "?, ?, ?)");
    ins_stmt.PExecute(data.guid, data.id, data.owner_guid, data.model_id,
        data.create_spell, data.pet_type, data.level, data.exp,
        data.react_state, data.loyalty_points, data.loyalty,
        data.training_points, data.name, data.renamed, data.slot, data.health,
        data.mana, data.happiness, data.save_time, data.reset_talents_cost,
        data.reset_talents_time, data.action_bar_raw, data.teach_spells_raw,
        data.dead);

    // pet_aura
    static SqlStatementID del_auras;
    SqlStatement delauras_stmt = CharacterDatabase.CreateStatement(
        del_auras, "DELETE FROM pet_aura WHERE guid = ?");
    delauras_stmt.PExecute(data.guid);
    for (auto& aura : data.auras)
    {
        static SqlStatementID ins_aura;
        SqlStatement stmt = CharacterDatabase.CreateStatement(ins_aura,
            "INSERT INTO pet_aura (guid, caster_guid, item_guid, spell, "
            "stackcount, remaincharges, basepoints0, basepoints1, basepoints2, "
            "periodictime0, periodictime1, periodictime2, maxduration, "
            "remaintime, effIndexMask) VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, "
            "?, ?, ?, ?)");
        stmt.PExecute(data.guid, aura.caster_guid, aura.item_guid,
            aura.spell_id, aura.stacks, aura.charges, aura.bp[0], aura.bp[1],
            aura.bp[2], aura.periodic_time[0], aura.periodic_time[1],
            aura.periodic_time[2], aura.max_duration, aura.duration,
            aura.eff_mask);
    }

    // pet_spell
    static SqlStatementID del_spells;
    SqlStatement delspells_stmt = CharacterDatabase.CreateStatement(
        del_spells, "DELETE FROM pet_spell WHERE guid = ?");
    delspells_stmt.PExecute(data.guid);
    for (auto& spell : data.spells)
    {
        static SqlStatementID ins_spell;
        SqlStatement stmt = CharacterDatabase.CreateStatement(ins_spell,
            "INSERT INTO pet_spell (guid, spell, active) VALUES(?, ?, ?)");
        stmt.PExecute(data.guid, spell.id, uint32(spell.active));
    }

    // pet_spell_cooldown
    static SqlStatementID del_cds;
    SqlStatement delcds_stmt = CharacterDatabase.CreateStatement(
        del_cds, "DELETE FROM pet_spell_cooldown WHERE guid = ?");
    delcds_stmt.PExecute(data.guid);
    for (auto& cd : data.spell_cooldowns)
    {
        static SqlStatementID ins_cd;
        SqlStatement stmt = CharacterDatabase.CreateStatement(ins_cd,
            "INSERT INTO pet_spell_cooldown (guid, spell, time) VALUES(?, ?, "
            "?)");
        stmt.PExecute(data.guid, cd.id, cd.time);
    }

    // character_pet_declinedname
    static SqlStatementID del_declined;
    SqlStatement deldeclined_stmt = CharacterDatabase.CreateStatement(
        del_declined, "DELETE FROM character_pet_declinedname WHERE id = ?");
    deldeclined_stmt.PExecute(data.guid);
    bool has_declined = false;
    for (auto& str : data.declined_name.name)
        if (!str.empty())
            has_declined = true;
    if (has_declined)
    {
        static SqlStatementID ins_declined;
        SqlStatement stmt = CharacterDatabase.CreateStatement(ins_declined,
            "INSERT INTO character_pet_declinedname (id, owner, genitive, "
            "dative, accusative, instrumental, prepositional) VALUES(?, ?, ?, "
            "?, ?, ?, ?)");
        stmt.PExecute(data.guid, data.owner_guid, data.declined_name.name[0],
            data.declined_name.name[1], data.declined_name.name[2],
            data.declined_name.name[3], data.declined_name.name[4]);
    }
}

void Player::drop_pet_db_data(uint32 guid)
{
    static SqlStatementID del;
    SqlStatement del_stmt = CharacterDatabase.CreateStatement(
        del, "DELETE FROM character_pet WHERE id = ?");
    del_stmt.PExecute(guid);

    static SqlStatementID del_auras;
    SqlStatement delauras_stmt = CharacterDatabase.CreateStatement(
        del_auras, "DELETE FROM pet_aura WHERE guid = ?");
    delauras_stmt.PExecute(guid);

    static SqlStatementID del_spells;
    SqlStatement delspells_stmt = CharacterDatabase.CreateStatement(
        del_spells, "DELETE FROM pet_spell WHERE guid = ?");
    delspells_stmt.PExecute(guid);

    static SqlStatementID del_cds;
    SqlStatement delcds_stmt = CharacterDatabase.CreateStatement(
        del_cds, "DELETE FROM pet_spell_cooldown WHERE guid = ?");
    delcds_stmt.PExecute(guid);

    static SqlStatementID del_declined;
    SqlStatement deldeclined_stmt = CharacterDatabase.CreateStatement(
        del_declined, "DELETE FROM character_pet_declinedname WHERE id = ?");
    deldeclined_stmt.PExecute(guid);
}

void Player::RemoveAtLoginFlag(AtLoginFlags f, bool in_db_also /*= false*/)
{
    m_atLoginFlags &= ~f;

    if (in_db_also)
        CharacterDatabase.PExecute(
            "UPDATE characters set at_login = at_login & ~ %u WHERE guid ='%u'",
            uint32(f), GetGUIDLow());
}

void Player::SendClearCooldown(uint32 spell_id, Unit* target)
{
    WorldPacket data(SMSG_CLEAR_COOLDOWN, 4 + 8);
    data << uint32(spell_id);
    data << target->GetObjectGuid();
    SendDirectMessage(std::move(data));
}

void Player::BuildTeleportAckMsg(
    WorldPacket& data, float x, float y, float z, float ang) const
{
    MovementInfo mi = m_movementInfo;
    mi.pos.Set(x, y, z, ang);

    data.initialize(MSG_MOVE_TELEPORT_ACK, 41);
    data << GetPackGUID();
    data << uint32(0); // this value increments every time
    data << mi;
}

void Player::send_teleport_msg(float x, float y, float z, float o)
{
    MovementInfo mi = m_movementInfo;
    mi.pos.Set(x, y, z, o);
    mi.time = WorldTimer::getMSTime();

    WorldPacket data(MSG_MOVE_TELEPORT, 37);
    data << GetPackGUID();
    data << mi;

    SendMessageToSet(&data, false);
}

bool Player::HasMovementFlag(MovementFlags f) const
{
    return m_movementInfo.HasMovementFlag(f);
}

void Player::ResetTimeSync()
{
    m_timeSyncCounter = 0;
    m_timeSyncTimer = 0;
}

void Player::SendTimeSync()
{
    WorldPacket data(SMSG_TIME_SYNC_REQ, 4);
    data << uint32(m_timeSyncCounter++);
    GetSession()->send_packet(std::move(data));

    m_timeSyncTimer = 10000;
}

void Player::SetHomebindToLocation(WorldLocation const& loc, uint32 area_id)
{
    m_homebindMapId = loc.mapid;
    m_homebindAreaId = area_id;
    m_homebindX = loc.coord_x;
    m_homebindY = loc.coord_y;
    m_homebindZ = loc.coord_z;

    // update sql homebind
    CharacterDatabase.PExecute(
        "UPDATE character_homebind SET map = '%u', zone = '%u', position_x = "
        "'%f', position_y = '%f', position_z = '%f' WHERE guid = '%u'",
        m_homebindMapId, m_homebindAreaId, m_homebindX, m_homebindY,
        m_homebindZ, GetGUIDLow());
}

Object* Player::GetObjectByTypeMask(ObjectGuid guid, TypeMask typemask)
{
    switch (guid.GetHigh())
    {
    case HIGHGUID_ITEM:
        if (typemask & TYPEMASK_ITEM)
            return GetItemByGuid(guid);
        break;
    case HIGHGUID_PLAYER:
        if (GetObjectGuid() == guid)
            return this;
        if ((typemask & TYPEMASK_PLAYER) && IsInWorld())
            return ObjectAccessor::FindPlayer(guid);
        break;
    case HIGHGUID_GAMEOBJECT:
        if ((typemask & TYPEMASK_GAMEOBJECT) && IsInWorld())
            return GetMap()->GetGameObject(guid);
        break;
    case HIGHGUID_UNIT:
        if ((typemask & TYPEMASK_UNIT) && IsInWorld())
            return GetMap()->GetCreature(guid);
        break;
    case HIGHGUID_PET:
        if ((typemask & TYPEMASK_UNIT) && IsInWorld())
            return GetMap()->GetPet(guid);
        break;
    case HIGHGUID_DYNAMICOBJECT:
        if ((typemask & TYPEMASK_DYNAMICOBJECT) && IsInWorld())
            return GetMap()->GetDynamicObject(guid);
        break;
    case HIGHGUID_TRANSPORT:
    case HIGHGUID_CORPSE:
    case HIGHGUID_MO_TRANSPORT:
    case HIGHGUID_GROUP:
    default:
        break;
    }

    return nullptr;
}

void Player::SetRestType(RestType n_r_type, uint32 areaTriggerId /*= 0*/)
{
    rest_type = n_r_type;

    if (rest_type == REST_TYPE_NO)
    {
        RemoveFlag(PLAYER_FLAGS, PLAYER_FLAGS_RESTING);

        // Set player to FFA PVP when not in rested environment.
        if (sWorld::Instance()->IsFFAPvPRealm())
            SetFFAPvP(true);
    }
    else
    {
        SetFlag(PLAYER_FLAGS, PLAYER_FLAGS_RESTING);

        inn_trigger_id = areaTriggerId;
        time_inn_enter = WorldTimer::time_no_syscall();

        if (sWorld::Instance()->IsFFAPvPRealm())
            SetFFAPvP(false);
    }
}

void Player::SendDuelCountdown(uint32 counter)
{
    WorldPacket data(SMSG_DUEL_COUNTDOWN, 4);
    data << uint32(counter); // seconds
    GetSession()->send_packet(std::move(data));
}

bool Player::IsImmuneToSpellEffect(
    SpellEntry const* spellInfo, SpellEffectIndex index) const
{
    switch (spellInfo->Effect[index])
    {
    case SPELL_EFFECT_ATTACK_ME:
        return true;
    default:
        break;
    }
    switch (spellInfo->EffectApplyAuraName[index])
    {
    case SPELL_AURA_MOD_TAUNT:
        return true;
    default:
        break;
    }
    return Unit::IsImmuneToSpellEffect(spellInfo, index);
}

void Player::KnockBack(float angle, float horizontalSpeed, float verticalSpeed)
{
    float vsin = sin(angle);
    float vcos = cos(angle);

    WorldPacket data(SMSG_MOVE_KNOCK_BACK, 9 + 4 + 4 + 4 + 4 + 4);
    data << GetPackGUID();
    data << uint32(0);              // Sequence
    data << float(vcos);            // x direction
    data << float(vsin);            // y direction
    data << float(horizontalSpeed); // Horizontal speed
    data << float(-verticalSpeed);  // Z Movement speed (vertical)
    GetSession()->send_packet(std::move(data));

    if (move_validator)
        move_validator->knock_back(verticalSpeed);

    remove_auras(SPELL_AURA_MOUNTED);
    remove_auras(SPELL_AURA_FLY);
}

void Player::KnockBackFrom(
    Unit* target, float horizontalSpeed, float verticalSpeed)
{
    float angle = this == target ? GetO() + M_PI_F : target->GetAngle(this);
    KnockBack(angle, horizontalSpeed, verticalSpeed);
}

bool Player::AIM_Initialize()
{
    // make sure nothing can change the AI during AI update
    if (m_AI_locked)
    {
        LOG_DEBUG(logging, "AIM_Initialize: failed to init, locked.");
        return false;
    }

    if (i_AI)
    {
        if (!AIM_Deinitialize())
            return false;
    }

    i_AI = new PlayerCharmAI(this);

    return true;
}

bool Player::AIM_Deinitialize()
{
    if (m_AI_locked)
        return false;

    // Remove movegens used by player charm AI
    int p = movement::get_default_priority(movement::gen::controlled);
    movement_gens.remove_if([p](const movement::Generator* gen)
        {
            return p < gen->priority() && gen->priority() < p + 10;
        });

    delete i_AI;
    i_AI = nullptr;

    return true;
}

void Player::SetRunMode(bool runmode, bool sendToClient)
{
    m_RunModeOn = runmode;

    if (sendToClient)
    {
        if (runmode)
            m_movementInfo.RemoveMovementFlag(MOVEFLAG_WALK_MODE);
        else
            m_movementInfo.AddMovementFlag(MOVEFLAG_WALK_MODE);

        // Note: This is just a guestimate to how the package should look
        uint16 opcode =
            runmode ? MSG_MOVE_SET_RUN_MODE : MSG_MOVE_SET_WALK_MODE;
        WorldPacket data(opcode, 40);
        data << GetPackGUID();                             // Packet GUID
        data << uint32(m_movementInfo.GetMovementFlags()); // flags
        data << uint8(0);                                  // Unk
        data << WorldTimer::getMSTime();                   // Time
        data << m_movementInfo.pos.x;                      // X, Y, Z, O
        data << m_movementInfo.pos.y;
        data << m_movementInfo.pos.z;
        data << m_movementInfo.pos.o;
        data << uint32(0); // FallTime ?
        SendMessageToSet(&data, true);
    }
}

void Player::HandleRogueSetupTalent(Unit* pAttacker)
{
    // Setup can only trigger off of main target since early TBC
    if (GetTargetGuid() != pAttacker->GetObjectGuid())
        return;

    // Only setup has that spell icon and aura type combination
    const Auras& al = GetAurasByType(SPELL_AURA_PROC_TRIGGER_SPELL);
    for (const auto& elem : al)
    {
        if ((elem)->GetSpellProto()->SpellIconID == 229)
        {
            if (roll_chance_i((elem)->GetSpellProto()->procChance))
                CastSpell(pAttacker, 15250, true); // Add one combo points
            break;
        }
    }
}

bool Player::HasDeadPet()
{
    Pet* pet = GetPet();
    if (pet && pet->isAlive())
        return false;

    PetDbData* data = nullptr;

    // Check current and not in slot for hunter
    if (getClass() == CLASS_HUNTER)
    {
        for (auto& d : _pet_store)
        {
            if (d.slot == PET_SAVE_AS_CURRENT ||
                d.slot > PET_SAVE_LAST_STABLE_SLOT)
            {
                data = &d;
                break;
            }
        }
    }
    // Check current only for any other class
    else
    {
        for (auto& d : _pet_store)
        {
            if (d.slot == PET_SAVE_AS_CURRENT)
            {
                data = &d;
                break;
            }
        }
    }

    if (!data)
        return false;

    return !data->deleted && data->dead;
}

void Player::AddRecentDungeon(uint32 map, uint32 instanceId)
{
    UpdateRecentDungeons();
    for (auto& elem : m_recentDungeons)
        if (elem.map == map && elem.instance == instanceId)
            return;

    RecentDungeon rd = {
        map, instanceId, static_cast<uint32>(WorldTimer::time_no_syscall())};
    m_recentDungeons.push_back(rd);
}

bool Player::IsDungeonLimitReached(uint32 map, uint32 instanceId)
{
    UpdateRecentDungeons();
    if (m_recentDungeons.size() < DUNGEON_LIMIT_NUM)
        return false;

    for (auto& elem : m_recentDungeons)
        if (elem.map == map && elem.instance == instanceId)
            return false;

    return true;
}

void Player::UpdateRecentDungeons()
{
    for (auto itr = m_recentDungeons.begin(); itr != m_recentDungeons.end();
        /* nothing */)
        if ((itr->timestamp + DUNGEON_LIMIT_TIME) <=
            WorldTimer::time_no_syscall())
            itr = m_recentDungeons.erase(itr);
        else
            ++itr;
}

void Player::set_gm_fly_mode(bool on)
{
    WorldPacket data(SMSG_MOVE_SET_CAN_FLY, 12);
    if (!on)
        data.opcode(SMSG_MOVE_UNSET_CAN_FLY);
    data << GetPackGUID();
    data << uint32(0); // unknown
    SendDirectMessage(std::move(data));

    gm_fly_mode_ = on;
}

bool Player::CanUseCapturePoint() const
{
    return isAlive() &&              // living
           !HasStealthAura() &&      // not stealthed
           !HasInvisibilityAura() && // visible
           (IsPvP() || sWorld::Instance()->IsPvPRealm()) &&
           !HasMovementFlag(MOVEFLAG_FLYING) && !IsTaxiFlying() &&
           !isGameMaster();
}

bool Player::add_item(uint32 id, uint32 count)
{
    inventory::transaction trans;
    trans.add(id, count);
    if (!storage().finalize(trans))
    {
        SendEquipError(static_cast<InventoryResult>(trans.error()), nullptr);
        return false;
    }

    return true;
}

bool Player::can_add_item(uint32 id, uint32 count) const
{
    inventory::transaction trans;
    trans.add(id, count);
    return storage().verify(trans);
}

// Destroy item used by scripts: do not use for user initiated actions!
bool Player::destroy_item(uint32 id, int count)
{
    uint32 cnt;
    if (count == -1)
        cnt = storage().item_count(id);
    else
        cnt = static_cast<uint32>(count);
    inventory::transaction trans(false);
    trans.destroy(id, cnt);
    return storage().finalize(trans);
}

bool Player::take_money(uint32 copper)
{
    inventory::copper c(copper);
    inventory::transaction trans;
    trans.remove(c);
    return storage().finalize(trans);
}

bool Player::give_money(uint32 copper)
{
    inventory::copper c(copper);
    inventory::transaction trans;
    trans.add(c);
    return storage().finalize(trans);
}

bool Player::match_req_spell(uint32 spell_id) const
{
    for (auto itr =
             storage().begin(inventory::personal_storage::iterator::equipment);
         itr != storage().end(); ++itr)
    {
        if ((*itr)->GetProto()->RequiredSpell == spell_id)
            return true;
    }
    return false;
}

Team Player::outdoor_pvp_team() const
{
    if (OutdoorPvP* outdoorPvP =
            sOutdoorPvPMgr::Instance()->GetScript(GetZoneId()))
        return outdoorPvP->GetOwningTeam();
    return TEAM_NONE;
}

// When we join/leave a group we need to resend our hp and max hp to everyone
// (to adjust from percentage to value, and vice versa)
void Player::resend_health()
{
    Object::resend_health();
    if (Pet* pet = GetPet())
        pet->resend_health();
    for (auto guid : GetGuardians())
        if (Unit* u = GetMap()->GetUnit(guid))
            u->resend_health();
}

void Player::DoResilienceCritProc(Unit* attacker, uint32 damage,
    WeaponAttackType att_type, const SpellEntry* spell)
{
    if (GetRatingBonusValue(CR_CRIT_TAKEN_MELEE) <= 0)
        return; // does not apply if player has no resilience

    for (auto& elem : m_auraHolders)
    {
        if (elem.second->IsDisabled())
            continue;

        const SpellEntry* info = elem.second->GetSpellProto();
        if (!info)
            continue;

        if (!info->HasAttribute(SPELL_ATTR_CUSTOM_RESILIENCE_CRIT_PROC))
            continue;

        // TODO: We should use overriden proc flags and cooldown from
        // spell_proc_event if any auras need that,
        // but at the time of writing this no auras that we added
        // SPELL_ATTR_CUSTOM_RESILIENCE_CRIT_PROC to has
        // an entry in spell_proc_event that matters to this code. Fix if that
        // changes.

        float chance = 0; // [0,100]
        uint32 proc_flags = 0;

        if (spell)
        {
            // Check if aura can proc from the type of spell we were hit with
            if (spell->HasAttribute(SPELL_ATTR_RANGED) &&
                info->procFlags & PROC_FLAG_TAKEN_RANGED_SPELL_HIT)
            {
                chance = GetRatingBonusValue(CR_CRIT_TAKEN_RANGED);
                proc_flags |= PROC_FLAG_TAKEN_RANGED_SPELL_HIT;
            }
            else if (spell->HasAttribute(SPELL_ATTR_ABILITY) &&
                     info->procFlags & PROC_FLAG_TAKEN_MELEE_SPELL_HIT)
            {
                chance = GetRatingBonusValue(CR_CRIT_TAKEN_MELEE);
                proc_flags |= PROC_FLAG_TAKEN_MELEE_SPELL_HIT;
            }
            else if (info->procFlags & PROC_FLAG_TAKEN_NEGATIVE_SPELL_HIT)
            {
                chance = GetRatingBonusValue(CR_CRIT_TAKEN_SPELL);
                proc_flags |= PROC_FLAG_TAKEN_NEGATIVE_SPELL_HIT;
            }
        }
        else
        {
            // Check if aura can proc from the type of attack we were hit with
            if (att_type == RANGED_ATTACK)
            {
                if (info->procFlags & PROC_FLAG_TAKEN_RANGED_HIT)
                {
                    chance = GetRatingBonusValue(CR_CRIT_TAKEN_RANGED);
                    proc_flags |= PROC_FLAG_TAKEN_RANGED_HIT;
                }
            }
            else
            {
                if (info->procFlags & PROC_FLAG_TAKEN_MELEE_HIT)
                {
                    chance = GetRatingBonusValue(CR_CRIT_TAKEN_MELEE);
                    proc_flags |= PROC_FLAG_TAKEN_MELEE_HIT;
                }
            }
        }

        if (chance == 0)
            continue;

        float roll = frand(0, 100);
        if (roll < chance)
        {
            // Proc aura
            for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
            {
                // All affected talents are Aura Id 42 or 4
                if (info->EffectApplyAuraName[i] !=
                        SPELL_AURA_PROC_TRIGGER_SPELL &&
                    info->EffectApplyAuraName[i] != SPELL_AURA_DUMMY)
                    continue;
                Aura* aura =
                    elem.second->GetAura(static_cast<SpellEffectIndex>(i));
                if (!aura)
                    continue;
                (*this.*AuraProcHandler[info->EffectApplyAuraName[i]])(attacker,
                    proc_amount(true, damage, this), aura, spell, proc_flags,
                    PROC_EX_CRITICAL_HIT, 0, EXTRA_ATTACK_NONE, 0);
            }
        }
    }
}

// Honor gain is based on damage done, helper methods to cache this info:
void Player::honor_damage_taken(Player* attacker, uint32 dmg)
{
    // Damage done falls to your group if you're in a group. It actually works
    // in a way that makes it so if your group does 90% damage on a target and
    // disbands, 90% of the honor of that kill is gone and goes to no-one
    // (tested on retail).

    if (GetTeam() == attacker->GetTeam())
        return;

    auto group = attacker->GetGroup();

    auto att_entry = honor_get_entry(attacker->GetObjectGuid());
    att_entry.first->dmg += dmg;

    if (att_entry.second)
        attacker->pvp_refd_players_.push_back(GetGUIDLow());

    if (!group && att_entry.first->ignore)
    {
        att_entry.first->ignore = false;
    }
    else if (group && !InBattleGround())
    {
        auto group_entry = honor_get_entry(group->GetObjectGuid());
        // If we just added *this, increase counter for group
        if (att_entry.second)
            group_entry.first->group += 1;
        group_entry.first->dmg += dmg;
        att_entry.first->ignore = true;
    }
}

namespace
{
void hk_distribute_helper(Player* victim, Player* receiver, float honor)
{
    auto vlevel = static_cast<int>(victim->getLevel());
    auto rlevel = static_cast<int>(receiver->getLevel());
    auto level_diff = vlevel - rlevel;
    auto gray_level = static_cast<int>(MaNGOS::XP::GetGrayLevel(rlevel));

    // Higher level: you get 5% more honor for each point, up to 20%
    if (level_diff > 0)
    {
        honor *= (level_diff >= 4) ? 1.2f : (1.0f + 0.05f * level_diff);
    }
    // Lower level: if gray, no honor is given
    else if (level_diff < 0 && vlevel <= gray_level)
    {
        return;
    }
    // Lower level: linear formula for all levels down to the one before gray,
    //              which gets 40% of the base honor
    else if (level_diff < 0 && gray_level + 1 < rlevel)
    {
        int span = rlevel - (gray_level + 1);
        honor *= 0.4f + 0.6f / span * (span + level_diff);
    }
    // Equal level: You get the exact honor

    honor = ceil(honor);
    if (honor <= 0.1f)
        honor = 1;

    receiver->ApplyModUInt32Value(PLAYER_FIELD_KILLS, 1, true);
    receiver->ApplyModUInt32Value(
        PLAYER_FIELD_LIFETIME_HONORBALE_KILLS, 1, true);
    receiver->RewardHonor(victim, 0, honor);
}
}

void Player::hk_distribute_honor()
{
    if (has_aura(SPELL_AURA_NO_PVP_CREDIT) || InArena())
        return;

    // Battleground groups have no unique identifier, but I think treating them
    // differently is fine: pick all on tap list, and as long as the list is >
    // 0, give to anyone in range too
    if (InBattleGround())
    {
        if (pvp_dmg_recvd_.empty())
            return;
        auto players =
            maps::visitors::yield_set<Player>{}(this, 120.0f, [this](Player* p)
                {
                    return p->GetTeam() != GetTeam() && p->isAlive();
                });
        for (auto& entry : pvp_dmg_recvd_)
        {
            if (!entry.group &&
                std::find_if(players.begin(), players.end(), [&entry](Player* p)
                    {
                        return p->GetGUIDLow() == entry.low_guid;
                    }) == players.end())
            {
                if (auto player = ObjectMgr::GetPlayer(
                        ObjectGuid(HIGHGUID_PLAYER, entry.low_guid)))
                    players.push_back(player);
            }
        }
        if (players.empty())
            return; // not possible
        float honor = MaNGOS::Honor::hk_honor_at_level(getLevel()) /
                      static_cast<float>(players.size());
        for (auto player : players)
            hk_distribute_helper(this, player, honor);
        return;
    }

    // Make sure no non-ignored player is grouped, this can happen if a player
    // made some damage on a target and was then invited by a group and did no
    // further action.
    for (auto itr = std::begin(pvp_dmg_recvd_);
         itr != std::end(pvp_dmg_recvd_);)
    {
        if (itr->group == 0 && !itr->ignore)
        {
            if (auto player = ObjectMgr::GetPlayer(
                    ObjectGuid(HIGHGUID_PLAYER, itr->low_guid)))
                if (player->GetGroup() != nullptr)
                {
                    itr = pvp_dmg_recvd_.erase(itr);
                    continue;
                }
        }
        ++itr;
    }

    auto total_damage = std::accumulate(std::begin(pvp_dmg_recvd_),
        std::end(pvp_dmg_recvd_), 0, [](auto&& curr, auto&& e)
        {
            return curr + (e.ignore ? 0 : e.dmg);
        });

    if (total_damage == 0)
        return;

    float total_honor = MaNGOS::Honor::hk_honor_at_level(getLevel());

    // Honor percentage
    for (auto& entry : pvp_dmg_recvd_)
    {
        // The group gets a percentage of the honor, that's then divided between
        // nearby members
        if (entry.group > 0)
        {
            std::vector<Player*> members;
            if (auto group =
                    sObjectMgr::Instance()->GetGroupById(entry.low_guid))
            {
                members.reserve(group->GetMembersCount());
                for (auto member : group->members(true))
                    if (IsWithinDistInMap(
                            member, GetMap()->GetVisibilityDistance()))
                        members.push_back(member);
            }
            if (members.empty())
                continue;
            float group_honor =
                total_honor * (static_cast<float>(entry.dmg) / total_damage);
            float member_honor = group_honor / members.size();
            if (member_honor > 0)
            {
                for (auto member : members)
                    hk_distribute_helper(this, member, member_honor);
            }
        }
        // A single player gets his percentage of the honor
        else if (!entry.ignore)
        {
            if (auto player = ObjectMgr::GetPlayer(
                    ObjectGuid(HIGHGUID_PLAYER, entry.low_guid)))
            {
                float honor = total_honor *
                              (static_cast<float>(entry.dmg) / total_damage);
                if (honor > 0)
                    hk_distribute_helper(this, player, honor);
            }
        }
    }
}

std::pair<std::vector<Player::pvp_dmg_entry>::iterator, bool>
Player::honor_get_entry(ObjectGuid guid)
{
    auto itr = std::lower_bound(std::begin(pvp_dmg_recvd_),
        std::end(pvp_dmg_recvd_), guid, [](auto&& f, auto&& v)
        {
            auto e = ObjectGuid(
                f.group > 0 ? HIGHGUID_GROUP : HIGHGUID_PLAYER, f.low_guid);
            return e < v;
        });

    bool inserted = false;
    if (itr == std::end(pvp_dmg_recvd_) ||
        !(itr->low_guid == guid.GetCounter() &&
            bool(itr->group) == guid.IsGroup()))
    {
        itr = pvp_dmg_recvd_.emplace(itr, guid.GetCounter());
        inserted = true;
    }

    return std::make_pair(itr, inserted);
}

void Player::honor_remove_dmg_done(Player* attacker)
{
    auto itr = std::lower_bound(std::begin(pvp_dmg_recvd_),
        std::end(pvp_dmg_recvd_), attacker->GetGUIDLow(), [](auto&& f, auto&& v)
        {
            // Group guid > Player guid
            if (f.group > 0)
                return false;
            return f.low_guid < v;
        });

    if (itr == std::end(pvp_dmg_recvd_) ||
        !(itr->low_guid == attacker->GetGUIDLow() && bool(itr->group) == false))
        return;

    pvp_dmg_recvd_.erase(itr);

    if (InBattleGround())
        return;

    // Lower group count, remove if it reaches 0
    if (auto group = attacker->GetGroup())
    {
        auto itr = std::lower_bound(std::begin(pvp_dmg_recvd_),
            std::end(pvp_dmg_recvd_), group->GetId(), [](auto&& f, auto&& v)
            {
                // Player guid < Group guid
                if (f.group == 0)
                    return true;
                return f.low_guid < v;
            });

        if (itr != std::end(pvp_dmg_recvd_) && itr->group > 0 &&
            itr->low_guid == group->GetId())
            if (--itr->group == 0)
                pvp_dmg_recvd_.erase(itr);
    }
}

void Player::honor_clear_dmg_done()
{
    pvp_dmg_recvd_.clear();

    for (auto low_guid : pvp_refd_players_)
    {
        auto guid = ObjectGuid(HIGHGUID_PLAYER, low_guid);
        if (auto player = sObjectMgr::Instance()->GetPlayer(guid))
            player->honor_remove_dmg_done(this);
    }

    pvp_refd_players_.clear();
}
