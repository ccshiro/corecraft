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

#include "ArenaTeam.h"
#include "Common.h"
#include "Group.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "Language.h"
#include "logging.h"
#include "MapManager.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Opcodes.h"
#include "Player.h"
#include "SharedDefines.h"
#include "SocialMgr.h"
#include "UpdateMask.h"
#include "Util.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "action_limit.h"
#include "Auth/md5.h"
#include "Database/DatabaseEnv.h"
#include "Database/DatabaseImpl.h"

// config option SkipCinematics supported values
enum CinematicsSkipMode
{
    CINEMATICS_SKIP_NONE = 0,
    CINEMATICS_SKIP_SAME_RACE = 1,
    CINEMATICS_SKIP_ALL = 2,
};

class LoginQueryHolder : public SqlQueryHolder
{
private:
    uint32 m_accountId;
    ObjectGuid m_guid;

public:
    LoginQueryHolder(uint32 accountId, ObjectGuid guid)
      : m_accountId(accountId), m_guid(std::move(guid))
    {
    }
    ObjectGuid GetGuid() const { return m_guid; }
    uint32 GetAccountId() const { return m_accountId; }
    bool Initialize();
};

bool LoginQueryHolder::Initialize()
{
    SetSize(MAX_PLAYER_LOGIN_QUERY);

    bool res = true;

    // NOTE: all fields in `characters` must be read to prevent lost character
    // data at next save in case wrong DB structure.
    // !!! NOTE: including unused `zone`,`online`
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADFROM,
        "SELECT guid, account, name, race, class, gender, level, xp, money, "
        "playerBytes, playerBytes2, playerFlags,"
        "position_x, position_y, position_z, map, orientation, taximask, "
        "cinematic, totaltime, leveltime, rest_bonus, logout_time, "
        "is_logout_resting, resettalents_cost,"
        "resettalents_time, trans_x, trans_y, trans_z, trans_o, transguid, "
        "extra_flags, stable_slots, at_login, zone, online, death_expire_time, "
        "taxi_path, dungeon_difficulty,"
        "arenaPoints, totalHonorPoints, todayHonorPoints, "
        "yesterdayHonorPoints, totalKills, todayKills, yesterdayKills, "
        "chosenTitle, watchedFaction, drunk,"
        "health, power1, power2, power3, power4, power5, exploredZones, "
        "equipmentCache, ammoId, knownTitles, actionBars, pvp_flagged, "
        "fall_z FROM "
        "characters WHERE guid = '%u'",
        m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADGROUP,
        "SELECT groupId FROM group_member WHERE memberGuid ='%u'",
        m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADINSTANCEBINDS,
        "SELECT id, permanent, map, difficulty, resettime FROM "
        "character_instance LEFT JOIN instance ON instance = id WHERE guid = "
        "'%u'",
        m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADAURAS,
        "SELECT "
        "caster_guid,item_guid,spell,stackcount,remaincharges,basepoints0,"
        "basepoints1,basepoints2,periodictime0,periodictime1,periodictime2,"
        "maxduration,remaintime,effIndexMask FROM character_aura WHERE guid = "
        "'%u'",
        m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADSPELLS,
        "SELECT spell,active,disabled FROM character_spell WHERE guid = '%u'",
        m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADQUESTSTATUS,
        "SELECT "
        "quest,status,rewarded,explored,timer,mobcount1,mobcount2,mobcount3,"
        "mobcount4,itemcount1,itemcount2,itemcount3,itemcount4 FROM "
        "character_queststatus WHERE guid = '%u'",
        m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADDAILYQUESTSTATUS,
        "SELECT quest FROM character_queststatus_daily WHERE guid = '%u'",
        m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADREPUTATION,
        "SELECT faction,standing,flags FROM character_reputation WHERE guid = "
        "'%u'",
        m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADINVENTORY,
        "SELECT data,bag,`index`,item,item_template FROM character_inventory "
        "JOIN item_instance ON character_inventory.item = item_instance.guid "
        "WHERE character_inventory.guid = '%u' ORDER BY bag DESC",
        m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADITEMLOOT,
        "SELECT guid,itemid,amount,suffix,property FROM item_loot WHERE "
        "owner_guid = '%u'",
        m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADACTIONS,
        "SELECT button,action,type FROM character_action WHERE guid = '%u' "
        "ORDER BY button",
        m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADSOCIALLIST,
        "SELECT friend,flags,note FROM character_social WHERE guid = '%u' "
        "LIMIT 255",
        m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADHOMEBIND,
        "SELECT map,zone,position_x,position_y,position_z FROM "
        "character_homebind WHERE guid = '%u'",
        m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADSPELLCOOLDOWNS,
        "SELECT spell,item,time FROM character_spell_cooldown WHERE guid = "
        "'%u'",
        m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADCATEGORYCOOLDOWN,
        "SELECT category, time FROM character_category_cooldown WHERE guid = "
        "'%u'",
        m_guid.GetCounter());
    if (sWorld::Instance()->getConfig(CONFIG_BOOL_DECLINED_NAMES_USED))
        res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADDECLINEDNAMES,
            "SELECT genitive, dative, accusative, instrumental, prepositional "
            "FROM character_declinedname WHERE guid = '%u'",
            m_guid.GetCounter());
    // in other case still be dummy query
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADGUILD,
        "SELECT guildid,rank FROM guild_member WHERE guid = '%u'",
        m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADARENAINFO,
        "SELECT arenateamid, played_week, played_season, personal_rating FROM "
        "arena_team_member WHERE guid='%u'",
        m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADBGDATA,
        "SELECT instance_id, team, join_x, join_y, join_z, join_o, join_map "
        "FROM character_battleground_data WHERE guid = '%u'",
        m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADSKILLS,
        "SELECT skill, value, max FROM character_skills WHERE guid = '%u'",
        m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADMAILS,
        "SELECT "
        "id,messageType,sender,receiver,subject,itemTextId,expire_time,deliver_"
        "time,money,cod,checked,stationery,mailTemplateId,has_items FROM mail "
        "WHERE receiver = '%u' ORDER BY id DESC",
        m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADMAILEDITEMS,
        "SELECT data, mail_id, item_guid, item_template FROM mail_items JOIN "
        "item_instance ON item_guid = guid WHERE receiver = '%u'",
        m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADRECENTDUNGEONS,
        "SELECT map,instance,timestamp FROM character_recent_dungeons WHERE "
        "guid = '%u'",
        m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_PETS,
        "SELECT id, entry, owner, modelid, CreatedBySpell, "
        "PetType, level, exp, Reactstate, loyaltypoints, loyalty, trainpoint, "
        "name, renamed, slot, curhealth, curmana, curhappiness, savetime, "
        "resettalents_cost, resettalents_time, abdata, teachspelldata, dead "
        "FROM character_pet WHERE owner = %u",
        m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_PET_AURAS,
        "SELECT pa.guid, pa.caster_guid, pa.item_guid, pa.spell, "
        "pa.stackcount, pa.remaincharges, pa.basepoints0, pa.basepoints1, "
        "pa.basepoints2, pa.periodictime0, pa.periodictime1, pa.periodictime2, "
        "pa.maxduration, pa.remaintime, pa.effIndexMask FROM pet_aura AS pa "
        "INNER JOIN character_pet as cp ON pa.guid = cp.id WHERE cp.owner = %u",
        m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_PET_SPELLS,
        "SELECT ps.guid, ps.spell, ps.active FROM pet_spell AS ps INNER JOIN "
        "character_pet AS cp ON ps.guid = cp.id WHERE cp.owner = %u",
        m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_PET_SPELL_COOLDOWNS,
        "SELECT psc.guid, psc.spell, psc.time FROM pet_spell_cooldown AS psc "
        "INNER JOIN character_pet AS cp ON psc.guid = cp.id WHERE cp.owner = "
        "%u",
        m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_PET_DECLINED_NAME,
        "SELECT cpdn.id, cpdn.genitive, cpdn.dative, cpdn.accusative, "
        "cpdn.instrumental, cpdn.prepositional "
        "FROM character_pet_declinedname AS cpdn INNER JOIN character_pet "
        "AS cp ON cpdn.id = cp.id WHERE cp.owner = %u",
        m_guid.GetCounter());

    return res;
}

// don't call WorldSession directly
// it may get deleted before the query callbacks get executed
// instead pass an account id to this handler
class CharacterHandler
{
public:
    void HandleCharEnumCallback(QueryResult* result, uint32 account)
    {
        WorldSession* session = sWorld::Instance()->FindSession(account);
        if (!session)
        {
            delete result;
            return;
        }
        session->HandleCharEnum(result);
    }
    void HandlePlayerLoginCallback(
        QueryResult* /*dummy*/, SqlQueryHolder* holder)
    {
        if (!holder)
            return;
        WorldSession* session = sWorld::Instance()->FindSession(
            ((LoginQueryHolder*)holder)->GetAccountId());
        if (!session)
        {
            delete holder;
            return;
        }
        session->HandlePlayerLogin((LoginQueryHolder*)holder);
    }
} chrHandler;

void WorldSession::HandleCharEnum(QueryResult* result)
{
    if (!sActionLimit::Instance()->attempt(
            "char_enum", action_limit::ip, this, 10, 0))
    {
        // Place a temporary ban on the IP address
        sWorld::Instance()->BanAccount(BAN_IP, this->GetRemoteAddress(), 300,
            "Repeated Char Enumeration", "Core");
        KickPlayer();
        return;
    }

    WorldPacket data(SMSG_CHAR_ENUM, 100); // we guess size

    uint8 num = 0;

    size_t num_pos = data.wpos();
    data << num;

    if (result)
    {
        do
        {
            LOG_DEBUG(logging,
                "Build enum data for char guid %u from account %u.",
                (*result)[0].GetUInt32(), GetAccountId());
            if (Player::BuildEnumData(result, &data))
                ++num;
        } while (result->NextRow());

        delete result;
    }

    data.put<uint8>(num_pos, num);

    send_packet(std::move(data));

    // Not in the world, so only option is that the user is at the character
    // selection screen. Initiate timeout
    if (!_player)
        char_select_start_t_ = WorldTimer::time_no_syscall();
}

void WorldSession::HandleCharEnumOpcode(WorldPacket& /*recv_data*/)
{
    /// get all the data necessary for loading all characters (along with their
    /// pets) on the account
    CharacterDatabase.AsyncPQuery(&chrHandler,
        &CharacterHandler::HandleCharEnumCallback, GetAccountId(),
        !sWorld::Instance()->getConfig(CONFIG_BOOL_DECLINED_NAMES_USED) ?
            //   ------- Query Without Declined Names --------
            "SELECT characters.guid, characters.name, characters.race, "
            "characters.class, characters.gender, characters.playerBytes, "
            "characters.playerBytes2, characters.level, "
            "characters.zone, characters.map, characters.position_x, "
            "characters.position_y, characters.position_z, "
            "guild_member.guildid, characters.playerFlags, "
            "characters.at_login, character_pet.entry, character_pet.modelid, "
            "character_pet.level, characters.equipmentCache "
            "FROM characters LEFT JOIN character_pet ON "
            "characters.guid=character_pet.owner AND character_pet.slot='%u' "
            "AND character_pet.dead = 0 LEFT JOIN guild_member ON "
            "characters.guid = guild_member.guid WHERE characters.account = "
            "'%u' ORDER BY characters.guid" :
            //   --------- Query With Declined Names ---------
            "SELECT characters.guid, characters.name, characters.race, "
            "characters.class, characters.gender, characters.playerBytes, "
            "characters.playerBytes2, characters.level, "
            "characters.zone, characters.map, characters.position_x, "
            "characters.position_y, characters.position_z, "
            "guild_member.guildid, characters.playerFlags, "
            "characters.at_login, character_pet.entry, character_pet.modelid, "
            "character_pet.level, characters.equipmentCache, "
            "character_declinedname.genitive "
            "FROM characters LEFT JOIN character_pet ON characters.guid = "
            "character_pet.owner AND character_pet.slot='%u' AND "
            "character_pet.dead = 0 LEFT JOIN character_declinedname ON "
            "characters.guid = character_declinedname.guid "
            "LEFT JOIN guild_member ON characters.guid = guild_member.guid "
            "WHERE characters.account = '%u' ORDER BY characters.guid",
        PET_SAVE_AS_CURRENT, GetAccountId());
}

void WorldSession::HandleCharCreateOpcode(WorldPacket& recv_data)
{
    std::string name;
    uint8 race_, class_;

    recv_data >> name;

    recv_data >> race_;
    recv_data >> class_;

    // extract other data required for player creating
    uint8 gender, skin, face, hairStyle, hairColor, facialHair, outfitId;
    recv_data >> gender >> skin >> face;
    recv_data >> hairStyle >> hairColor >> facialHair >> outfitId;

    WorldPacket data(
        SMSG_CHAR_CREATE, 1); // returned with diff.values in all cases

    if (GetSecurity() == SEC_PLAYER)
    {
        if (uint32 mask = sWorld::Instance()->getConfig(
                CONFIG_UINT32_CHARACTERS_CREATING_DISABLED))
        {
            bool disabled = false;

            Team team = Player::TeamForRace(race_);
            switch (team)
            {
            case ALLIANCE:
                disabled = mask & (1 << 0);
                break;
            case HORDE:
                disabled = mask & (1 << 1);
                break;
            default:
                disabled = true;
                break;
            }

            if (disabled)
            {
                data << (uint8)CHAR_CREATE_DISABLED;
                send_packet(std::move(data));
                return;
            }
        }
    }

    ChrClassesEntry const* classEntry = sChrClassesStore.LookupEntry(class_);
    ChrRacesEntry const* raceEntry = sChrRacesStore.LookupEntry(race_);

    if (!classEntry || !raceEntry)
    {
        data << (uint8)CHAR_CREATE_FAILED;
        send_packet(std::move(data));
        logging.error(
            "Class: %u or Race %u not found in DBC (Wrong DBC files?) or "
            "Cheater?",
            class_, race_);
        return;
    }

    // prevent character creating Expansion race without Expansion account
    if (raceEntry->expansion > Expansion())
    {
        data << (uint8)CHAR_CREATE_EXPANSION;
        logging.error(
            "Expansion %u account:[%d] tried to Create character with "
            "expansion %u race (%u)",
            Expansion(), GetAccountId(), raceEntry->expansion, race_);
        send_packet(std::move(data));
        return;
    }

    // prevent character creating with invalid name
    if (!normalizePlayerName(name))
    {
        data << (uint8)CHAR_NAME_NO_NAME;
        send_packet(std::move(data));
        logging.error(
            "Account:[%d] but tried to Create character with empty [name]",
            GetAccountId());
        return;
    }

    // check name limitations
    uint8 res = ObjectMgr::CheckPlayerName(name, true);
    if (res != CHAR_NAME_SUCCESS)
    {
        data << uint8(res);
        send_packet(std::move(data));
        return;
    }

    if (GetSecurity() == SEC_PLAYER &&
        sObjectMgr::Instance()->IsReservedName(name))
    {
        data << (uint8)CHAR_NAME_RESERVED;
        send_packet(std::move(data));
        return;
    }

    if (sObjectMgr::Instance()->GetPlayerGuidByName(name))
    {
        data << (uint8)CHAR_CREATE_NAME_IN_USE;
        send_packet(std::move(data));
        return;
    }

    std::unique_ptr<QueryResult> result(LoginDatabase.PQuery(
        "SELECT SUM(numchars) FROM realmcharacters WHERE acctid = '%u'",
        GetAccountId()));
    if (result)
    {
        Field* fields = result->Fetch();
        uint32 acctcharcount = fields[0].GetUInt32();

        if (acctcharcount >=
            sWorld::Instance()->getConfig(CONFIG_UINT32_CHARACTERS_PER_ACCOUNT))
        {
            data << (uint8)CHAR_CREATE_ACCOUNT_LIMIT;
            send_packet(std::move(data));
            return;
        }
    }

    result.reset(CharacterDatabase.PQuery(
        "SELECT COUNT(guid) FROM characters WHERE account = '%u'",
        GetAccountId()));
    uint8 charcount = 0;
    if (result)
    {
        Field* fields = result->Fetch();
        charcount = fields[0].GetUInt8();

        if (charcount >=
            sWorld::Instance()->getConfig(CONFIG_UINT32_CHARACTERS_PER_REALM))
        {
            data << (uint8)CHAR_CREATE_SERVER_LIMIT;
            send_packet(std::move(data));
            return;
        }
    }

    bool AllowTwoSideAccounts =
        !sWorld::Instance()->IsPvPRealm() ||
        sWorld::Instance()->getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_ACCOUNTS) ||
        GetSecurity() > SEC_PLAYER;
    CinematicsSkipMode skipCinematics = CinematicsSkipMode(
        sWorld::Instance()->getConfig(CONFIG_UINT32_SKIP_CINEMATICS));

    bool have_same_race = false;
    if (!AllowTwoSideAccounts || skipCinematics == CINEMATICS_SKIP_SAME_RACE)
    {
        std::unique_ptr<QueryResult> result2(CharacterDatabase.PQuery(
            "SELECT race FROM characters WHERE account = '%u' %s",
            GetAccountId(),
            (skipCinematics == CINEMATICS_SKIP_SAME_RACE) ? "" : "LIMIT 1"));
        if (result2)
        {
            Team team_ = Player::TeamForRace(race_);

            Field* field = result2->Fetch();
            uint8 acc_race = field[0].GetUInt32();

            // need to check team only for first character
            // TODO: what to if account already has characters of both races?
            if (!AllowTwoSideAccounts)
            {
                if (acc_race == 0 || Player::TeamForRace(acc_race) != team_)
                {
                    data << (uint8)CHAR_CREATE_PVP_TEAMS_VIOLATION;
                    send_packet(std::move(data));
                    return;
                }
            }

            // search same race for cinematic or same class if need
            // TODO: check if cinematic already shown? (already logged in?;
            // cinematic field)
            while (
                skipCinematics == CINEMATICS_SKIP_SAME_RACE && !have_same_race)
            {
                if (!result2->NextRow())
                    break;

                field = result2->Fetch();
                acc_race = field[0].GetUInt32();

                have_same_race = race_ == acc_race;
            }
        }
    }

    auto pNewChar = new Player(shared_from_this());
    if (!pNewChar->Create(sObjectMgr::Instance()->GeneratePlayerLowGuid(), name,
            race_, class_, gender, skin, face, hairStyle, hairColor, facialHair,
            outfitId))
    {
        // Player not create (race/class problem?)
        delete pNewChar;

        data << (uint8)CHAR_CREATE_ERROR;
        send_packet(std::move(data));

        return;
    }

    if ((have_same_race && skipCinematics == CINEMATICS_SKIP_SAME_RACE) ||
        skipCinematics == CINEMATICS_SKIP_ALL)
        pNewChar->setCinematic(1); // not show intro

    pNewChar->SetAtLoginFlag(AT_LOGIN_FIRST); // First login

    // Player created, save it now
    pNewChar->SaveToDB();
    charcount += 1;

    LoginDatabase.PExecute(
        "DELETE FROM realmcharacters WHERE acctid= '%u' AND realmid = '%u'",
        GetAccountId(), realmID);
    LoginDatabase.PExecute(
        "INSERT INTO realmcharacters (numchars, acctid, realmid) VALUES (%u, "
        "%u, %u)",
        charcount, GetAccountId(), realmID);

    data << (uint8)CHAR_CREATE_SUCCESS;
    send_packet(std::move(data));

    std::string IP_str = GetRemoteAddress();
    LOG_DEBUG(logging, "Account: %d (IP: %s) Create Character:[%s] (guid: %u)",
        GetAccountId(), IP_str.c_str(), name.c_str(), pNewChar->GetGUIDLow());

    delete pNewChar; // created only to call SaveToDB()
}

void WorldSession::HandleCharDeleteOpcode(WorldPacket& recv_data)
{
    ObjectGuid guid;
    recv_data >> guid;

    // can't delete loaded character
    if (sObjectMgr::Instance()->GetPlayer(guid))
    {
        WorldPacket data(SMSG_CHAR_DELETE, 1);
        data << (uint8)CHAR_DELETE_FAILED;
        send_packet(std::move(data));
        return;
    }

    uint32 accountId = 0;
    std::string name;

    // is guild leader
    if (sGuildMgr::Instance()->GetGuildByLeader(guid))
    {
        WorldPacket data(SMSG_CHAR_DELETE, 1);
        data << (uint8)CHAR_DELETE_FAILED_GUILD_LEADER;
        send_packet(std::move(data));
        return;
    }

    // is arena team captain
    if (sObjectMgr::Instance()->GetArenaTeamByCaptain(guid))
    {
        WorldPacket data(SMSG_CHAR_DELETE, 1);
        data << (uint8)CHAR_DELETE_FAILED_ARENA_CAPTAIN;
        send_packet(std::move(data));
        return;
    }

    uint32 lowguid = guid.GetCounter();

    std::unique_ptr<QueryResult> result(CharacterDatabase.PQuery(
        "SELECT account,name FROM characters WHERE guid='%u'", lowguid));
    if (result)
    {
        Field* fields = result->Fetch();
        accountId = fields[0].GetUInt32();
        name = fields[1].GetCppString();
    }

    // prevent deleting other players' characters using cheating tools
    if (accountId != GetAccountId())
        return;

    std::string IP_str = GetRemoteAddress();
    LOG_DEBUG(logging, "Account: %d (IP: %s) Delete Character:[%s] (guid: %u)",
        GetAccountId(), IP_str.c_str(), name.c_str(), lowguid);

    Player::DeleteFromDB(guid, GetAccountId());

    WorldPacket data(SMSG_CHAR_DELETE, 1);
    data << (uint8)CHAR_DELETE_SUCCESS;
    send_packet(std::move(data));
}

void WorldSession::HandlePlayerLoginOpcode(WorldPacket& recv_data)
{
    ObjectGuid playerGuid;
    recv_data >> playerGuid;

    if (PlayerLoading() || GetPlayer() != nullptr)
    {
        logging.error(
            "Player tryes to login again, AccountId = %d", GetAccountId());
        return;
    }

    m_playerLoading = true;

    // Reset the character select screen idle counter so we don't log the player
    // out during logi
    char_select_start_t_ = static_cast<time_t>(0);

    // If player is already in world we must reuse that entity
    if (auto player = sObjectAccessor::Instance()->FindPlayer(playerGuid, true))
    {
        HandlePlayerRelog(player);
        return;
    }

    auto holder = new LoginQueryHolder(GetAccountId(), playerGuid);
    if (!holder->Initialize())
    {
        delete holder; // delete all unprocessed queries
        m_playerLoading = false;
        return;
    }

    CharacterDatabase.DelayQueryHolder(
        &chrHandler, &CharacterHandler::HandlePlayerLoginCallback, holder);
}

void WorldSession::HandlePlayerLogin(LoginQueryHolder* holder)
{
    ObjectGuid playerGuid = holder->GetGuid();

    auto pCurrChar = new Player(shared_from_this());

    // "GetAccountId()==db stored account id" checked in LoadFromDB (prevent
    // login not own character using cheating tools)
    if (!pCurrChar->LoadFromDB(playerGuid, holder))
    {
        KickPlayer(); // disconnect client, player no set to session and it will
                      // not deleted or saved at kick
        delete pCurrChar; // delete it manually
        delete holder;    // delete all unprocessed queries
        m_playerLoading = false;
        return;
    }

    pCurrChar->move_validator = new movement_validator(pCurrChar);

    SetPlayer(pCurrChar);

    pCurrChar->SendDungeonDifficulty(false);

    WorldPacket data(SMSG_LOGIN_VERIFY_WORLD, 20);
    data << pCurrChar->GetMapId();
    data << pCurrChar->GetX();
    data << pCurrChar->GetY();
    data << pCurrChar->GetZ();
    data << pCurrChar->GetO();
    send_packet(std::move(data));

    data.initialize(SMSG_ACCOUNT_DATA_TIMES, 128);
    for (int i = 0; i < 32; ++i)
        data << uint32(0);
    send_packet(std::move(data));

    data.initialize(SMSG_FEATURE_SYSTEM_STATUS, 2); // added in 2.2.0
    data << uint8(2);                               // unknown value
    data << uint8(0); // enable(1)/disable(0) voice chat interface in client
    send_packet(std::move(data));

    // Send MOTD
    {
        data.initialize(SMSG_MOTD, 50); // new in 2.0.1

        uint32 linecount = 0;
        auto linecount_pos = data.wpos();
        data << (uint32)0; // placeholder

        std::string str_motd = sWorld::Instance()->GetMotd();
        std::string::size_type pos, nextpos;

        pos = 0;
        while ((nextpos = str_motd.find('@', pos)) != std::string::npos)
        {
            if (nextpos != pos)
            {
                data << str_motd.substr(pos, nextpos - pos);
                ++linecount;
            }
            pos = nextpos + 1;
        }

        if (pos < str_motd.length())
        {
            data << str_motd.substr(pos);
            ++linecount;
        }

        data.put(linecount_pos, linecount);

        send_packet(std::move(data));
    }

    // QueryResult *result = CharacterDatabase.PQuery("SELECT guildid,rank FROM
    // guild_member WHERE guid = '%u'",pCurrChar->GetGUIDLow());
    QueryResult* resultGuild = holder->GetResult(PLAYER_LOGIN_QUERY_LOADGUILD);

    if (resultGuild)
    {
        Field* fields = resultGuild->Fetch();
        pCurrChar->SetInGuild(fields[0].GetUInt32());
        pCurrChar->SetRank(fields[1].GetUInt32());
        delete resultGuild;
    }
    else if (pCurrChar->GetGuildId()) // clear guild related fields in case
                                      // wrong data about nonexistent membership
    {
        pCurrChar->SetInGuild(0);
        pCurrChar->SetRank(0);
    }

    if (pCurrChar->GetGuildId() != 0)
    {
        Guild* guild =
            sGuildMgr::Instance()->GetGuildById(pCurrChar->GetGuildId());
        if (guild)
        {
            guild->SendMotd(this);
            guild->BroadcastEvent(
                GE_SIGNED_ON, pCurrChar->GetObjectGuid(), pCurrChar->GetName());
        }
        else
        {
            // remove wrong guild data
            logging.error(
                "Player %s (GUID: %u) marked as member of nonexistent guild "
                "(id: %u), removing guild membership for player.",
                pCurrChar->GetName(), pCurrChar->GetGUIDLow(),
                pCurrChar->GetGuildId());
            pCurrChar->SetInGuild(0);
        }
    }

    if (!pCurrChar->isAlive())
        pCurrChar->SendCorpseReclaimDelay(true);

    pCurrChar->SendInitialPacketsBeforeAddToMap();

    // Show cinematic at the first time that player login
    if (!pCurrChar->getCinematic())
    {
        pCurrChar->setCinematic(1);

        if (ChrRacesEntry const* rEntry =
                sChrRacesStore.LookupEntry(pCurrChar->getRace()))
            pCurrChar->SendCinematicStart(rEntry->CinematicSequence);
    }

    if (!sMapMgr::Instance()->CanPlayerEnter(
            pCurrChar->GetMapId(), pCurrChar) ||
        !pCurrChar->GetMap()->insert(pCurrChar))
    {
        // normal delayed teleport protection not applied (and this correct) for
        // this case (Player object just created)
        AreaTrigger const* at =
            sObjectMgr::Instance()->GetGoBackTrigger(pCurrChar->GetMapId());
        if (at)
            pCurrChar->TeleportTo(at->target_mapId, at->target_X, at->target_Y,
                at->target_Z, pCurrChar->GetO());
        else
            pCurrChar->TeleportToHomebind();
    }

    sObjectAccessor::Instance()->AddObject(pCurrChar);
    // LOG_DEBUG(logging,"Player %s added to Map.",pCurrChar->GetName());
    pCurrChar->GetSocial()->SendSocialList();

    pCurrChar->SendInitialPacketsAfterAddToMap();

    static SqlStatementID updChars;
    static SqlStatementID updAccount;

    SqlStatement stmt = CharacterDatabase.CreateStatement(
        updChars, "UPDATE characters SET online = 1 WHERE guid = ?");
    stmt.PExecute(pCurrChar->GetGUIDLow());

    stmt = LoginDatabase.CreateStatement(
        updAccount, "UPDATE account SET active_realm_id = ? WHERE id = ?");
    stmt.PExecute(realmID, GetAccountId());

    pCurrChar->SetInGameTime(WorldTimer::getMSTime());

    // announce group about member online (must be after add to player list to
    // receive announce to self)
    if (Group* group = pCurrChar->GetGroup())
    {
        // Pass leader to curr char if group has no one online, and not a
        // recently logged offline leader
        bool alone_in_group = true;
        for (auto member : group->members(false))
        {
            if (member == pCurrChar)
                continue;
            alone_in_group = false;
        }
        if (alone_in_group &&
            !sObjectMgr::Instance()->IsOfflineLeaderGroup(group))
            group->ChangeLeader(pCurrChar->GetObjectGuid());

        // Send updates
        group->SendUpdate();
        pCurrChar->SetGroupUpdateFlag(GROUP_UPDATE_FULL);
        group->UpdatePlayerOutOfRange(pCurrChar);
    }

    // friend status
    sSocialMgr::Instance()->SendFriendStatus(
        pCurrChar, FRIEND_ONLINE, pCurrChar->GetObjectGuid(), true);

    // Place character in world (and load zone) before some object loading
    pCurrChar->LoadCorpse();

    // setting Ghost+speed if dead
    if (pCurrChar->m_deathState != ALIVE)
    {
        // not blizz like, we must correctly save and load player instead...
        if (pCurrChar->getRace() == RACE_NIGHTELF)
            pCurrChar->CastSpell(pCurrChar, 20584,
                true); // auras SPELL_AURA_INCREASE_SPEED(+speed in wisp form),
                       // SPELL_AURA_INCREASE_SWIM_SPEED(+swim speed in wisp
                       // form), SPELL_AURA_TRANSFORM (to wisp form)
        pCurrChar->CastSpell(pCurrChar, 8326,
            true); // auras SPELL_AURA_GHOST, SPELL_AURA_INCREASE_SPEED(why?),
                   // SPELL_AURA_INCREASE_SWIM_SPEED(why?)

        pCurrChar->SetMovement(MOVE_WATER_WALK);
    }

    // Need a valid map pointer before we start flight path again
    pCurrChar->queue_action(0, [pCurrChar]()
        {
            pCurrChar->ContinueTaxiFlight();
        });

    // Load pet if any (if player not alive and in taxi flight or another then
    // pet will remember as temporary unsummoned)
    if (!pCurrChar->HasDeadPet())
        pCurrChar->LoadPet();

    // Set FFA PvP for non GM in non-rest mode
    if (sWorld::Instance()->IsFFAPvPRealm() && !pCurrChar->isGameMaster() &&
        !pCurrChar->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_RESTING))
        pCurrChar->SetFFAPvP(true);

    if (pCurrChar->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_CONTESTED_PVP))
        pCurrChar->SetContestedPvP();

    // Apply at_login requests
    if (pCurrChar->HasAtLoginFlag(AT_LOGIN_RESET_SPELLS))
    {
        pCurrChar->resetSpells();
        SendNotification(LANG_RESET_SPELLS);
    }

    if (pCurrChar->HasAtLoginFlag(AT_LOGIN_RESET_TALENTS))
    {
        pCurrChar->resetTalents(true);
        SendNotification(
            LANG_RESET_TALENTS); // we can use SMSG_TALENTS_INVOLUNTARILY_RESET
                                 // here
    }

    if (pCurrChar->HasAtLoginFlag(AT_LOGIN_FIRST))
        pCurrChar->RemoveAtLoginFlag(AT_LOGIN_FIRST);

    // show time before shutdown if shutdown planned.
    if (sWorld::Instance()->IsShutdowning())
        sWorld::Instance()->ShutdownMsg(true, pCurrChar);

    if (sWorld::Instance()->getConfig(CONFIG_BOOL_ALL_TAXI_PATHS))
        pCurrChar->SetTaxiCheater(true);

    if (pCurrChar->isGameMaster())
        SendNotification(LANG_GM_ON);

    if (!pCurrChar->isGMVisible())
        SendNotification(LANG_INVISIBLE_INVISIBLE);

    std::string IP_str = GetRemoteAddress();

    if (!pCurrChar->IsStandState() &&
        !pCurrChar->hasUnitState(UNIT_STAT_STUNNED))
        pCurrChar->SetStandState(UNIT_STAND_STATE_STAND);

    // Root player when he logs in, will be unrooted in Player::Update
    if (!pCurrChar->m_taxi.GetTaxiSource())
        pCurrChar->ApplyRootHack();

    m_playerLoading = false;
    delete holder;
}

void WorldSession::HandlePlayerRelog(Player* player)
{
    auto prev_session = player->GetSession();

    if (!prev_session || prev_session->GetAccountId() != GetAccountId())
    {
        m_playerLoading = false;
        KickPlayer();
        return;
    }

    prev_session->SetPlayer(nullptr);

    SetPlayer(player);
    player->OverwriteSession(shared_from_this());

    player->move_validator = new movement_validator(player);

    player->SendDungeonDifficulty(false);

    WorldPacket data(SMSG_LOGIN_VERIFY_WORLD, 20);
    data << player->GetMapId();
    data << player->GetX();
    data << player->GetY();
    data << player->GetZ();
    data << player->GetO();
    send_packet(std::move(data));

    data.initialize(SMSG_ACCOUNT_DATA_TIMES, 128);
    for (int i = 0; i < 32; ++i)
        data << uint32(0);
    send_packet(std::move(data));

    data.initialize(SMSG_FEATURE_SYSTEM_STATUS, 2); // added in 2.2.0
    data << uint8(2);                               // unknown value
    data << uint8(0); // enable(1)/disable(0) voice chat interface in client
    send_packet(std::move(data));

    // Send MOTD
    {
        data.initialize(SMSG_MOTD, 50); // new in 2.0.1

        uint32 linecount = 0;
        auto linecount_pos = data.wpos();
        data << (uint32)0; // placeholder

        std::string str_motd = sWorld::Instance()->GetMotd();
        std::string::size_type pos, nextpos;

        pos = 0;
        while ((nextpos = str_motd.find('@', pos)) != std::string::npos)
        {
            if (nextpos != pos)
            {
                data << str_motd.substr(pos, nextpos - pos);
                ++linecount;
            }
            pos = nextpos + 1;
        }

        if (pos < str_motd.length())
        {
            data << str_motd.substr(pos);
            ++linecount;
        }

        data.put(linecount_pos, linecount);

        send_packet(std::move(data));
    }

    if (player->GetGuildId() != 0)
    {
        Guild* guild =
            sGuildMgr::Instance()->GetGuildById(player->GetGuildId());
        if (guild)
        {
            guild->SendMotd(this);

            /* XXX guild->DisplayGuildBankTabsInfo(this);

            guild->BroadcastEvent(GE_SIGNED_ON, player->GetObjectGuid(),
            player->GetName());
            */
        }
    }

    // XXX
    if (!player->isAlive())
        player->SendCorpseReclaimDelay(true);

    player->SendInitialPacketsBeforeAddToMap();

    player->GetMap()->relog(player);
    player->GetSocial()->SendSocialList();

    player->SendInitialPacketsAfterAddToMap();

    if (Group* group = player->GetGroup())
    {
        group->SendUpdate();
        player->SetGroupUpdateFlag(GROUP_UPDATE_FULL);
        group->UpdatePlayerOutOfRange(player);
    }

    sSocialMgr::Instance()->SendFriendStatus(
        player, FRIEND_ONLINE, player->GetObjectGuid(), true);

    player->ContinueTaxiFlight();

    if (player->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_CONTESTED_PVP))
        player->SetContestedPvP();

    // Apply at_login requests
    if (player->HasAtLoginFlag(AT_LOGIN_RESET_SPELLS))
    {
        player->resetSpells();
        SendNotification(LANG_RESET_SPELLS);
    }

    if (player->HasAtLoginFlag(AT_LOGIN_RESET_TALENTS))
    {
        player->resetTalents(true);
        SendNotification(
            LANG_RESET_TALENTS); // we can use SMSG_TALENTS_INVOLUNTARILY_RESET
                                 // here
    }

    if (sWorld::Instance()->IsShutdowning())
        sWorld::Instance()->ShutdownMsg(true, player);

    if (player->isGameMaster())
        SendNotification(LANG_GM_ON);

    if (!player->isGMVisible())
        SendNotification(LANG_INVISIBLE_INVISIBLE);

    if (player->isDead() && player->GetCorpse() == nullptr)
    {
        player->BuildPlayerRepop();
        player->RepopAtGraveyard();
    }

    m_playerLoading = false;

    // Refresh aura durations
    player->loop_auras([](AuraHolder* holder)
        {
            if (holder->GetAuraSlot() < MAX_AURAS)
                holder->UpdateAuraDuration();
            return true; // continue
        });

    // Refresh control states
    bool rooted = false;

    // Root/stun
    if (player->HasAuraType(SPELL_AURA_MOD_ROOT) ||
        player->HasAuraType(SPELL_AURA_MOD_STUN))
    {
        player->Root(true);
        rooted = true;
    }

    // Charming someone
    if (auto charm = player->GetCharm())
    {
        player->SetClientControl(player, 0);
        player->SetClientControl(charm, 1);
        player->SetMovingUnit(charm);
    }

    // Unable to control self
    if (player->HasAuraType(SPELL_AURA_MOD_CONFUSE) ||
        player->HasAuraType(SPELL_AURA_MOD_FEAR) ||
        player->GetCharmer() != nullptr)
    {
        player->SetClientControl(player, 0);
    }

    // Root player when he logs in, will be unrooted in Player::Update
    if (!rooted && !player->IsTaxiFlying())
        player->ApplyRootHack();
}

void WorldSession::HandleSetFactionAtWarOpcode(WorldPacket& recv_data)
{
    uint32 repListID;
    uint8 flag;

    recv_data >> repListID;
    recv_data >> flag;

    GetPlayer()->GetReputationMgr().SetAtWar(repListID, flag);
}

void WorldSession::HandleMeetingStoneInfoOpcode(WorldPacket& /*recv_data*/)
{
    WorldPacket data(SMSG_MEETINGSTONE_SETQUEUE, 5);
    data << uint32(0) << uint8(6);
    send_packet(std::move(data));
}

void WorldSession::HandleTutorialFlagOpcode(WorldPacket& recv_data)
{
    uint32 iFlag;
    recv_data >> iFlag;

    uint32 wInt = (iFlag / 32);
    if (wInt >= 8)
    {
        // logging.error("CHEATER? Account:[%d] Guid[%u] tried to
        // send wrong CMSG_TUTORIAL_FLAG", GetAccountId(),GetGUID());
        return;
    }
    uint32 rInt = (iFlag % 32);

    uint32 tutflag = GetTutorialInt(wInt);
    tutflag |= (1 << rInt);
    SetTutorialInt(wInt, tutflag);

    // LOG_DEBUG(logging,"Received Tutorial Flag Set {%u}.", iFlag);
}

void WorldSession::HandleTutorialClearOpcode(WorldPacket& /*recv_data*/)
{
    for (int i = 0; i < 8; ++i)
        SetTutorialInt(i, 0xFFFFFFFF);
}

void WorldSession::HandleTutorialResetOpcode(WorldPacket& /*recv_data*/)
{
    for (int i = 0; i < 8; ++i)
        SetTutorialInt(i, 0x00000000);
}

void WorldSession::HandleSetWatchedFactionOpcode(WorldPacket& recv_data)
{
    int32 repId;
    recv_data >> repId;
    GetPlayer()->SetInt32Value(PLAYER_FIELD_WATCHED_FACTION_INDEX, repId);
}

void WorldSession::HandleSetFactionInactiveOpcode(WorldPacket& recv_data)
{
    uint32 replistid;
    uint8 inactive;
    recv_data >> replistid >> inactive;

    _player->GetReputationMgr().SetInactive(replistid, inactive);
}

void WorldSession::HandleShowingHelmOpcode(WorldPacket& /*recv_data*/)
{
    _player->ToggleFlag(PLAYER_FLAGS, PLAYER_FLAGS_HIDE_HELM);
}

void WorldSession::HandleShowingCloakOpcode(WorldPacket& /*recv_data*/)
{
    _player->ToggleFlag(PLAYER_FLAGS, PLAYER_FLAGS_HIDE_CLOAK);
}

void WorldSession::HandleCharRenameOpcode(WorldPacket& recv_data)
{
    ObjectGuid guid;
    std::string newname;

    recv_data >> guid;
    recv_data >> newname;

    // prevent character rename to invalid name
    if (!normalizePlayerName(newname))
    {
        WorldPacket data(SMSG_CHAR_RENAME, 1);
        data << uint8(CHAR_NAME_NO_NAME);
        send_packet(std::move(data));
        return;
    }

    uint8 res = ObjectMgr::CheckPlayerName(newname, true);
    if (res != CHAR_NAME_SUCCESS)
    {
        WorldPacket data(SMSG_CHAR_RENAME, 1);
        data << uint8(res);
        send_packet(std::move(data));
        return;
    }

    // check name limitations
    if (GetSecurity() == SEC_PLAYER &&
        sObjectMgr::Instance()->IsReservedName(newname))
    {
        WorldPacket data(SMSG_CHAR_RENAME, 1);
        data << uint8(CHAR_NAME_RESERVED);
        send_packet(std::move(data));
        return;
    }

    std::string escaped_newname = newname;
    CharacterDatabase.escape_string(escaped_newname);

    // make sure that the character belongs to the current account, that rename
    // at login is enabled
    // and that there is no character with the desired new name
    CharacterDatabase.AsyncPQuery(
        &WorldSession::HandleChangePlayerNameOpcodeCallBack, GetAccountId(),
        newname,
        "SELECT guid, name FROM characters WHERE guid = %u AND account = %u "
        "AND (at_login & %u) = %u AND NOT EXISTS (SELECT NULL FROM characters "
        "WHERE name = '%s')",
        guid.GetCounter(), GetAccountId(), AT_LOGIN_RENAME, AT_LOGIN_RENAME,
        escaped_newname.c_str());
}

void WorldSession::HandleChangePlayerNameOpcodeCallBack(
    QueryResult* result, uint32 accountId, std::string newname)
{
    WorldSession* session = sWorld::Instance()->FindSession(accountId);
    if (!session)
    {
        if (result)
            delete result;
        return;
    }

    if (!result)
    {
        WorldPacket data(SMSG_CHAR_RENAME, 1);
        data << uint8(CHAR_CREATE_ERROR);
        session->send_packet(std::move(data));
        return;
    }

    uint32 guidLow = result->Fetch()[0].GetUInt32();
    ObjectGuid guid = ObjectGuid(HIGHGUID_PLAYER, guidLow);
    std::string oldname = result->Fetch()[1].GetCppString();

    delete result;

    CharacterDatabase.BeginTransaction();
    CharacterDatabase.PExecute(
        "UPDATE characters set name = '%s', at_login = at_login & ~ %u WHERE "
        "guid ='%u'",
        newname.c_str(), uint32(AT_LOGIN_RENAME), guidLow);
    CharacterDatabase.PExecute(
        "DELETE FROM character_declinedname WHERE guid ='%u'", guidLow);
    CharacterDatabase.CommitTransaction();

    WorldPacket data(SMSG_CHAR_RENAME, 1 + 8 + (newname.size() + 1));
    data << uint8(RESPONSE_SUCCESS);
    data << guid;
    data << newname;
    session->send_packet(std::move(data));
}

void WorldSession::HandleSetPlayerDeclinedNamesOpcode(WorldPacket& recv_data)
{
    ObjectGuid guid;

    recv_data >> guid;

    // not accept declined names for unsupported languages
    std::string name;
    if (!sObjectMgr::Instance()->GetPlayerNameByGUID(guid, name))
    {
        WorldPacket data(SMSG_SET_PLAYER_DECLINED_NAMES_RESULT, 4 + 8);
        data << uint32(1);
        data << ObjectGuid(guid);
        send_packet(std::move(data));
        return;
    }

    std::wstring wname;
    if (!Utf8toWStr(name, wname))
    {
        WorldPacket data(SMSG_SET_PLAYER_DECLINED_NAMES_RESULT, 4 + 8);
        data << uint32(1);
        data << ObjectGuid(guid);
        send_packet(std::move(data));
        return;
    }

    if (!isCyrillicCharacter(
            wname[0])) // name already stored as only single alphabet using
    {
        WorldPacket data(SMSG_SET_PLAYER_DECLINED_NAMES_RESULT, 4 + 8);
        data << uint32(1);
        data << ObjectGuid(guid);
        send_packet(std::move(data));
        return;
    }

    std::string name2;
    DeclinedName declinedname;

    recv_data >> name2;

    if (name2 != name) // character have different name
    {
        WorldPacket data(SMSG_SET_PLAYER_DECLINED_NAMES_RESULT, 4 + 8);
        data << uint32(1);
        data << ObjectGuid(guid);
        send_packet(std::move(data));
        return;
    }

    for (int i = 0; i < MAX_DECLINED_NAME_CASES; ++i)
    {
        recv_data >> declinedname.name[i];
        if (!normalizePlayerName(declinedname.name[i]))
        {
            WorldPacket data(SMSG_SET_PLAYER_DECLINED_NAMES_RESULT, 4 + 8);
            data << uint32(1);
            data << ObjectGuid(guid);
            send_packet(std::move(data));
            return;
        }
    }

    if (!ObjectMgr::CheckDeclinedNames(
            GetMainPartOfName(wname, 0), declinedname))
    {
        WorldPacket data(SMSG_SET_PLAYER_DECLINED_NAMES_RESULT, 4 + 8);
        data << uint32(1);
        data << ObjectGuid(guid);
        send_packet(std::move(data));
        return;
    }

    for (int i = 0; i < MAX_DECLINED_NAME_CASES; ++i)
        CharacterDatabase.escape_string(declinedname.name[i]);

    CharacterDatabase.BeginTransaction();
    CharacterDatabase.PExecute(
        "DELETE FROM character_declinedname WHERE guid = '%u'",
        guid.GetCounter());
    CharacterDatabase.PExecute(
        "INSERT INTO character_declinedname (guid, genitive, dative, "
        "accusative, instrumental, prepositional) VALUES "
        "('%u','%s','%s','%s','%s','%s')",
        guid.GetCounter(), declinedname.name[0].c_str(),
        declinedname.name[1].c_str(), declinedname.name[2].c_str(),
        declinedname.name[3].c_str(), declinedname.name[4].c_str());
    CharacterDatabase.CommitTransaction();

    WorldPacket data(SMSG_SET_PLAYER_DECLINED_NAMES_RESULT, 4 + 8);
    data << uint32(0); // OK
    data << ObjectGuid(guid);
    send_packet(std::move(data));
}
