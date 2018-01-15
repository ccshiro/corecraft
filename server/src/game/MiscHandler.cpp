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

#include "BattleGround.h"
#include "Chat.h"
#include "Common.h"
#include "GuildMgr.h"
#include "Language.h"
#include "logging.h"
#include "LootMgr.h"
#include "MapManager.h"
#include "Object.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Opcodes.h"
#include "Pet.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SocialMgr.h"
#include "UpdateData.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "action_limit.h"
#include "Auth/BigNumber.h"
#include "Auth/Sha1.h"
#include "Database/DatabaseEnv.h"
#include "Database/DatabaseImpl.h"
#include <zlib.h>

void WorldSession::HandleRepopRequestOpcode(WorldPacket& recv_data)
{
    recv_data.read_skip<uint8>();

    if (_player->isAlive() ||
        _player->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_GHOST))
        return;

    // the world update order is sessions, players, creatures
    // the netcode runs in parallel with all of these
    // creatures can kill players
    // so if the server is lagging enough the player can
    // release spirit after he's killed but before he is updated
    if (GetPlayer()->getDeathState() == JUST_DIED)
    {
        LOG_DEBUG(logging,
            "HandleRepopRequestOpcode: got request after player %s(%d) was "
            "killed and before he was updated",
            GetPlayer()->GetName(), GetPlayer()->GetGUIDLow());
        GetPlayer()->KillPlayer();
    }

    // this is spirit release confirm?
    _player->RemovePet(PET_SAVE_REAGENTS);
    _player->BuildPlayerRepop();
    _player->RepopAtGraveyard();
    _player->UpdateObjectVisibility();
}

void WorldSession::HandleWhoOpcode(WorldPacket& recv_data)
{
    if (!sActionLimit::Instance()->attempt(
            "who", action_limit::local, this, 0, 4))
        return;

    if (!sActionLimit::Instance()->attempt(
            "who", action_limit::global, this, 500, 0))
        return;

    /* See: http://www.wowwiki.com/API_GetNumWhoResults
     * Note: This was actually bugged on retail. The retail server would never
     * send more than 49 num_whos, and never more than 50 total_count. This
     * means that you never actually saw the total count in the client, because
     * the default UI checks bigger than 50. One can speculate this could've
     * been the result of a miscommunication between the UI and server
     * developers.
     */
    uint32 total_count = 0, num_whos = 0;

    uint32 level_min, level_max, racemask, classmask, zones_count, str_count;
    uint32 zoneids[10]; // 10 is client limit
    std::string player_name, guild_name;

    recv_data >> level_min;   // maximal player level, default 0
    recv_data >> level_max;   // minimal player level, default 100 (MAX_LEVEL)
    recv_data >> player_name; // player name, case sensitive...

    recv_data >> guild_name; // guild name, case sensitive...

    recv_data >> racemask;  // race mask
    recv_data >> classmask; // class mask
    recv_data >> zones_count;
    // zones count, client limit=10 (2.0.10)

    if (zones_count > 10)
        return; // can't be received from real client or broken packet

    for (uint32 i = 0; i < zones_count; ++i)
    {
        uint32 temp;
        recv_data >> temp; // zone id, 0 if zone is unknown...
        zoneids[i] = temp;
        LOG_DEBUG(logging, "Zone %u: %u", i, zoneids[i]);
    }

    recv_data >> str_count; // user entered strings count, client limit=4
                            // (checked on 2.0.10)

    if (str_count > 4)
        return; // can't be received from real client or broken packet

    LOG_DEBUG(logging,
        "Minlvl %u, maxlvl %u, name %s, guild %s, racemask %u, classmask %u, "
        "zones %u, strings %u",
        level_min, level_max, player_name.c_str(), guild_name.c_str(), racemask,
        classmask, zones_count, str_count);

    std::wstring str[4]; // 4 is client limit
    for (uint32 i = 0; i < str_count; ++i)
    {
        std::string temp;
        recv_data >> temp; // user entered string, it's used as universal search
                           // pattern (this equals names, guilds, zones, race
                           // and classes.)

        if (!Utf8toWStr(temp, str[i]))
            continue;

        wstrToLower(str[i]);

        LOG_DEBUG(logging, "String %u: %s", i, temp.c_str());
    }

    std::wstring wplayer_name;
    std::wstring wguild_name;
    if (!(Utf8toWStr(player_name, wplayer_name) &&
            Utf8toWStr(guild_name, wguild_name)))
        return;
    wstrToLower(wplayer_name);
    wstrToLower(wguild_name);

    // client send in case not set max level value 100 but mangos support 255
    // max level,
    // update it to show GMs with characters after 100 level
    if (level_max >= MAX_LEVEL)
        level_max = STRONG_MAX_LEVEL;

    Team team = _player->GetTeam();
    AccountTypes security = GetSecurity();
    bool allowTwoSideWhoList =
        sWorld::Instance()->getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_WHO_LIST);
    AccountTypes gmLevelInWhoList = (AccountTypes)sWorld::Instance()->getConfig(
        CONFIG_UINT32_GM_LEVEL_IN_WHO_LIST);

    WorldPacket data(SMSG_WHO, 50); // maximum packet size

    // See NOTE above for num_whos and total_count comment
    auto num_whos_pos = data.wpos();
    data << uint32(num_whos);
    auto total_count_pos = data.wpos();
    data << uint32(total_count);

    HashMapHolder<Player>::LockedContainer m =
        sObjectAccessor::Instance()->GetPlayers();
    for (HashMapHolder<Player>::MapType::const_iterator itr = m.get().begin();
         itr != m.get().end(); ++itr)
    {
        Player* pl = itr->second;

        if (security == SEC_PLAYER)
        {
            // player can see member of other team only if
            // CONFIG_BOOL_ALLOW_TWO_SIDE_WHO_LIST
            if (pl->GetTeam() != team && !allowTwoSideWhoList)
                continue;

            // player can see MODERATOR, GAME MASTER, ADMINISTRATOR only if
            // CONFIG_GM_IN_WHO_LIST
            if (pl->GetSession()->GetSecurity() > gmLevelInWhoList)
                continue;
        }

        // do not process players which are not in world
        if (!pl->IsInWorld())
            continue;

        // check if target is globally visible for player
        if (!pl->IsVisibleGloballyFor(_player))
            continue;

        // check if target's level is in level range
        uint32 lvl = pl->getLevel();
        if (lvl < level_min || lvl > level_max)
            continue;

        // check if class matches classmask
        uint32 class_ = pl->getClass();
        if (!(classmask & (1 << class_)))
            continue;

        // check if race matches racemask
        uint32 race = pl->getRace();
        if (!(racemask & (1 << race)))
            continue;

        uint32 pzoneid = pl->GetZoneId();
        uint8 gender = pl->getGender();

        bool z_show = true;
        for (uint32 i = 0; i < zones_count; ++i)
        {
            if (zoneids[i] == pzoneid)
            {
                z_show = true;
                break;
            }

            z_show = false;
        }
        if (!z_show)
            continue;

        std::string pname = pl->GetName();
        std::wstring wpname;
        if (!Utf8toWStr(pname, wpname))
            continue;
        wstrToLower(wpname);

        if (!(wplayer_name.empty() ||
                wpname.find(wplayer_name) != std::wstring::npos))
            continue;

        std::string gname =
            sGuildMgr::Instance()->GetGuildNameById(pl->GetGuildId());
        std::wstring wgname;
        if (!Utf8toWStr(gname, wgname))
            continue;
        wstrToLower(wgname);

        if (!(wguild_name.empty() ||
                wgname.find(wguild_name) != std::wstring::npos))
            continue;

        std::string aname;
        if (AreaTableEntry const* areaEntry = GetAreaEntryByAreaID(pzoneid))
        {
            aname = areaEntry->area_name[GetSessionDbcLocale()];
            // If it's a sub-zone use the name of the parent zone instead
            if (areaEntry->zone != 0)
            {
                if (const AreaTableEntry* parent_area =
                        GetAreaEntryByAreaID(pzoneid))
                    aname = parent_area->area_name[GetSessionDbcLocale()];
            }
        }

        std::string race_name;
        if (const ChrRacesEntry* entry = sChrRacesStore.LookupEntry(race))
            race_name = entry->name[GetSessionDbcLocale()];

        std::string class_name;
        if (const ChrClassesEntry* entry = sChrClassesStore.LookupEntry(class_))
            class_name = entry->name[GetSessionDbcLocale()];

        bool s_show = true;
        for (uint32 i = 0; i < str_count; ++i)
        {
            if (!str[i].empty())
            {
                // strings match names, guilds, zones, races and classes
                if (wgname.find(str[i]) != std::wstring::npos ||
                    wpname.find(str[i]) != std::wstring::npos ||
                    Utf8FitTo(aname, str[i]) || Utf8FitTo(race_name, str[i]) ||
                    Utf8FitTo(class_name, str[i]))
                {
                    s_show = true;
                    break;
                }
                s_show = false;
            }
        }
        if (!s_show)
            continue;

        ++total_count;
        // If total_count is 50 at this point, num_whos
        // is 49. Skip sending this guy.
        if (total_count == 50)
        {
            total_count = 49;
            break;
        }

        data << pname;           // player name
        data << gname;           // guild name
        data << uint32(lvl);     // player level
        data << uint32(class_);  // player class
        data << uint32(race);    // player race
        data << uint8(gender);   // player gender
        data << uint32(pzoneid); // player zone id

        ++num_whos;
    }

    data.put(num_whos_pos, num_whos);
    data.put(total_count_pos, total_count);

    send_packet(std::move(data));
}

void WorldSession::HandleLogoutRequestOpcode(WorldPacket& /*recv_data*/)
{
    if (ObjectGuid lootGuid = GetPlayer()->GetLootGuid())
        DoLootRelease(lootGuid);

    // Can not logout if...
    if (GetPlayer()->isInCombat() || //...is in combat
        GetPlayer()->duel ||         //...is in Duel
                                     //...is jumping ...is falling
        GetPlayer()->m_movementInfo.HasMovementFlag(
            MovementFlags(MOVEFLAG_GRAVITY | MOVEFLAG_FALLING_UNK1)))
    {
        WorldPacket data(SMSG_LOGOUT_RESPONSE, (2 + 4));
        data << (uint8)0xC;
        data << uint32(0);
        data << uint8(0);
        send_packet(std::move(data));
        LogoutRequest(0);
        return;
    }

    // instant logout in taverns/cities or on taxi or for admins, gm's, mod's if
    // its enabled in mangosd.conf
    if (GetPlayer()->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_RESTING) ||
        GetPlayer()->IsTaxiFlying() ||
        GetSecurity() >= (AccountTypes)sWorld::Instance()->getConfig(
                             CONFIG_UINT32_INSTANT_LOGOUT))
    {
        LogoutPlayer(true);
        return;
    }

    // not set flags if player can't free move to prevent lost state at logout
    // cancel
    if (GetPlayer()->CanFreeMove())
    {
        float height = GetPlayer()->GetTerrain()->GetHeightStatic(
            GetPlayer()->GetX(), GetPlayer()->GetY(), GetPlayer()->GetZ());
        if ((GetPlayer()->GetZ() < height + 0.1f) &&
            !(GetPlayer()->IsInWater()))
            GetPlayer()->SetStandState(UNIT_STAND_STATE_SIT);

        WorldPacket data(SMSG_FORCE_MOVE_ROOT, (8 + 4)); // guess size
        data << GetPlayer()->GetPackGUID();
        data << (uint32)2;
        send_packet(std::move(data));
        GetPlayer()->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_STUNNED);
    }

    WorldPacket data(SMSG_LOGOUT_RESPONSE, 5);
    data << uint32(0);
    data << uint8(0);
    send_packet(std::move(data));
    LogoutRequest(WorldTimer::time_no_syscall());
}

void WorldSession::HandlePlayerLogoutOpcode(WorldPacket& /*recv_data*/)
{
}

void WorldSession::HandleLogoutCancelOpcode(WorldPacket& /*recv_data*/)
{
    LogoutRequest(0);

    WorldPacket data(SMSG_LOGOUT_CANCEL_ACK, 0);
    send_packet(std::move(data));

    // not remove flags if can't free move - its not set in Logout request code.
    if (GetPlayer()->CanFreeMove())
    {
        //!we can move again
        data.initialize(SMSG_FORCE_MOVE_UNROOT, 16); // guess size
        data << GetPlayer()->GetPackGUID();
        data << uint32(0);
        send_packet(std::move(data));

        //! Stand Up
        GetPlayer()->SetStandState(UNIT_STAND_STATE_STAND);

        //! DISABLE_ROTATE
        GetPlayer()->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_STUNNED);

        // For some bizarre reason when reloading ui you should lose stealth,
        // this opcode is sent by the client on reload ui
        _player->remove_auras(SPELL_AURA_MOD_STEALTH);
        _player->remove_auras(SPELL_AURA_MOD_INVISIBILITY);
    }
}

void WorldSession::HandleTogglePvP(WorldPacket& recv_data)
{
    // this opcode can be used in two ways: Either set explicit new status or
    // toggle old status
    if (recv_data.size() == 1)
    {
        bool newPvPStatus;
        recv_data >> newPvPStatus;
        GetPlayer()->ApplyModFlag(
            PLAYER_FLAGS, PLAYER_FLAGS_IN_PVP, newPvPStatus);
    }
    else
    {
        GetPlayer()->ToggleFlag(PLAYER_FLAGS, PLAYER_FLAGS_IN_PVP);
    }

    if (GetPlayer()->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_IN_PVP))
    {
        if (!GetPlayer()->IsPvP() || GetPlayer()->pvpInfo.endTimer != 0)
            GetPlayer()->UpdatePvP(true, true);
    }
    else
    {
        if (!GetPlayer()->pvpInfo.inHostileArea && GetPlayer()->IsPvP())
            GetPlayer()->pvpInfo.endTimer =
                WorldTimer::time_no_syscall(); // start toggle-off
    }
}

void WorldSession::HandleZoneUpdateOpcode(WorldPacket& recv_data)
{
    uint32 newZone;
    recv_data >> newZone;

    // use server size data
    uint32 newzone, newarea;
    GetPlayer()->GetZoneAndAreaId(newzone, newarea);
    GetPlayer()->UpdateZone(newzone, newarea);
}

void WorldSession::HandleSetTargetOpcode(WorldPacket& recv_data)
{
    // When this packet send?
    ObjectGuid guid;
    recv_data >> guid;

    _player->SetTargetGuid(guid);

    // update reputation list if need
    Unit* unit = ObjectAccessor::GetUnit(
        *_player, guid); // can select group members at diff maps
    if (!unit)
        return;

    if (FactionTemplateEntry const* factionTemplateEntry =
            sFactionTemplateStore.LookupEntry(unit->getFaction()))
        _player->GetReputationMgr().SetVisible(factionTemplateEntry);
}

void WorldSession::HandleSetSelectionOpcode(WorldPacket& recv_data)
{
    ObjectGuid guid;
    recv_data >> guid;

    _player->SetSelectionGuid(guid);
}

void WorldSession::HandleStandStateChangeOpcode(WorldPacket& recv_data)
{
    uint32 animstate;
    recv_data >> animstate;

    _player->SetStandState(animstate);
}

void WorldSession::HandleContactListOpcode(WorldPacket& recv_data)
{
    uint32 unk;
    recv_data >> unk;
    LOG_DEBUG(logging, "unk value is %u", unk);
    _player->GetSocial()->SendSocialList();
}

void WorldSession::HandleAddFriendOpcode(WorldPacket& recv_data)
{
    std::string friendName = GetMangosString(LANG_FRIEND_IGNORE_UNKNOWN);
    std::string friendNote;

    recv_data >> friendName;

    recv_data >> friendNote;

    if (!normalizePlayerName(friendName))
        return;

    CharacterDatabase.escape_string(friendName); // prevent SQL injection -
                                                 // normal name don't must
                                                 // changed by this call

    LOG_DEBUG(logging, "WORLD: %s asked to add friend : '%s'",
        GetPlayer()->GetName(), friendName.c_str());

    CharacterDatabase.AsyncPQuery(&WorldSession::HandleAddFriendOpcodeCallBack,
        GetAccountId(), friendNote,
        "SELECT guid, race FROM characters WHERE name = '%s'",
        friendName.c_str());
}

void WorldSession::HandleAddFriendOpcodeCallBack(
    QueryResult* result, uint32 accountId, std::string friendNote)
{
    if (!result)
        return;

    uint32 friendLowGuid = (*result)[0].GetUInt32();
    ObjectGuid friendGuid = ObjectGuid(HIGHGUID_PLAYER, friendLowGuid);
    Team team = Player::TeamForRace((*result)[1].GetUInt8());

    delete result;

    WorldSession* session = sWorld::Instance()->FindSession(accountId);
    if (!session || !session->GetPlayer())
        return;

    FriendsResult friendResult = FRIEND_NOT_FOUND;
    if (friendGuid)
    {
        if (friendGuid == session->GetPlayer()->GetObjectGuid())
            friendResult = FRIEND_SELF;
        else if (session->GetPlayer()->GetTeam() != team &&
                 !sWorld::Instance()->getConfig(
                     CONFIG_BOOL_ALLOW_TWO_SIDE_ADD_FRIEND) &&
                 session->GetSecurity() < SEC_TICKET_GM)
            friendResult = FRIEND_ENEMY;
        else if (session->GetPlayer()->GetSocial()->HasFriend(friendGuid))
            friendResult = FRIEND_ALREADY;
        else
        {
            Player* pFriend = ObjectAccessor::FindPlayer(friendGuid);
            if (pFriend && pFriend->IsInWorld() &&
                pFriend->IsVisibleGloballyFor(session->GetPlayer()))
                friendResult = FRIEND_ADDED_ONLINE;
            else
                friendResult = FRIEND_ADDED_OFFLINE;

            if (!session->GetPlayer()->GetSocial()->AddToSocialList(
                    friendGuid, false))
            {
                friendResult = FRIEND_LIST_FULL;
                LOG_DEBUG(logging, "WORLD: %s's friend list is full.",
                    session->GetPlayer()->GetName());
            }

            session->GetPlayer()->GetSocial()->SetFriendNote(
                friendGuid, friendNote);
        }
    }

    sSocialMgr::Instance()->SendFriendStatus(
        session->GetPlayer(), friendResult, friendGuid, false);
}

void WorldSession::HandleDelFriendOpcode(WorldPacket& recv_data)
{
    ObjectGuid friendGuid;

    recv_data >> friendGuid;

    _player->GetSocial()->RemoveFromSocialList(friendGuid, false);

    sSocialMgr::Instance()->SendFriendStatus(
        GetPlayer(), FRIEND_REMOVED, friendGuid, false);
}

void WorldSession::HandleAddIgnoreOpcode(WorldPacket& recv_data)
{
    std::string IgnoreName = GetMangosString(LANG_FRIEND_IGNORE_UNKNOWN);

    recv_data >> IgnoreName;

    if (!normalizePlayerName(IgnoreName))
        return;

    CharacterDatabase.escape_string(IgnoreName); // prevent SQL injection -
                                                 // normal name don't must
                                                 // changed by this call

    LOG_DEBUG(logging, "WORLD: %s asked to Ignore: '%s'",
        GetPlayer()->GetName(), IgnoreName.c_str());

    CharacterDatabase.AsyncPQuery(&WorldSession::HandleAddIgnoreOpcodeCallBack,
        GetAccountId(), "SELECT guid FROM characters WHERE name = '%s'",
        IgnoreName.c_str());
}

void WorldSession::HandleAddIgnoreOpcodeCallBack(
    QueryResult* result, uint32 accountId)
{
    if (!result)
        return;

    uint32 ignoreLowGuid = (*result)[0].GetUInt32();
    ObjectGuid ignoreGuid = ObjectGuid(HIGHGUID_PLAYER, ignoreLowGuid);

    delete result;

    WorldSession* session = sWorld::Instance()->FindSession(accountId);
    if (!session || !session->GetPlayer())
        return;

    FriendsResult ignoreResult = FRIEND_IGNORE_NOT_FOUND;
    if (ignoreGuid)
    {
        if (ignoreGuid == session->GetPlayer()->GetObjectGuid())
            ignoreResult = FRIEND_IGNORE_SELF;
        else if (session->GetPlayer()->GetSocial()->HasIgnore(ignoreGuid))
            ignoreResult = FRIEND_IGNORE_ALREADY;
        else
        {
            ignoreResult = FRIEND_IGNORE_ADDED;

            // ignore list full
            if (!session->GetPlayer()->GetSocial()->AddToSocialList(
                    ignoreGuid, true))
                ignoreResult = FRIEND_IGNORE_FULL;
        }
    }

    sSocialMgr::Instance()->SendFriendStatus(
        session->GetPlayer(), ignoreResult, ignoreGuid, false);
}

void WorldSession::HandleDelIgnoreOpcode(WorldPacket& recv_data)
{
    ObjectGuid ignoreGuid;

    recv_data >> ignoreGuid;

    _player->GetSocial()->RemoveFromSocialList(ignoreGuid, true);

    sSocialMgr::Instance()->SendFriendStatus(
        GetPlayer(), FRIEND_IGNORE_REMOVED, ignoreGuid, false);
}

void WorldSession::HandleSetContactNotesOpcode(WorldPacket& recv_data)
{
    ObjectGuid guid;
    std::string note;
    recv_data >> guid >> note;
    _player->GetSocial()->SetFriendNote(guid, note);
}

void WorldSession::HandleBugOpcode(WorldPacket& recv_data)
{
    uint32 suggestion, contentlen, typelen;
    std::string content, type;

    recv_data >> suggestion >> contentlen >> content;

    recv_data >> typelen >> type;

#ifndef OPTIMIZED_BUILD
    if (suggestion == 0)
        LOG_DEBUG(logging, "WORLD: Received CMSG_BUG [Bug Report]");
    else
        LOG_DEBUG(logging, "%s", type.c_str());
    LOG_DEBUG(logging, "%s", content.c_str());
#endif

    CharacterDatabase.escape_string(type);
    CharacterDatabase.escape_string(content);
    CharacterDatabase.PExecute(
        "INSERT INTO bugreport (type,content) VALUES('%s', '%s')", type.c_str(),
        content.c_str());
}

void WorldSession::HandleReclaimCorpseOpcode(WorldPacket& recv_data)
{
    ObjectGuid guid;
    recv_data >> guid;

    if (GetPlayer()->isAlive())
        return;

    // do not allow corpse reclaim in arena
    if (GetPlayer()->InArena())
        return;

    // body not released yet
    if (!GetPlayer()->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_GHOST))
        return;

    Corpse* corpse = GetPlayer()->GetCorpse();

    if (!corpse)
        return;

    // prevent resurrect before 30-sec delay after body release not finished
    if (corpse->GetGhostTime() +
            GetPlayer()->GetCorpseReclaimDelay(
                corpse->GetType() == CORPSE_RESURRECTABLE_PVP) >
        WorldTimer::time_no_syscall())
        return;

    if (!corpse->IsWithinDistInMap(
            GetPlayer(), CORPSE_RECLAIM_RADIUS + 3.0f, true))
        return;

    // resurrect
    GetPlayer()->ResurrectPlayer(0.5f);

    // spawn bones
    GetPlayer()->SpawnCorpseBones();
}

void WorldSession::HandleResurrectResponseOpcode(WorldPacket& recv_data)
{
    ObjectGuid guid;
    uint8 status;
    recv_data >> guid;
    recv_data >> status;

    if (GetPlayer()->isAlive())
        return;

    if (status == 0)
    {
        GetPlayer()->clearResurrectRequestData(); // reject
        return;
    }

    if (!GetPlayer()->isRessurectRequestedBy(guid))
        return;

    GetPlayer()->ResurectUsingRequestData(); // will call spawncorpsebones
}

void WorldSession::HandleAreaTriggerOpcode(WorldPacket& recv_data)
{
    uint32 Trigger_ID;

    recv_data >> Trigger_ID;
    LOG_DEBUG(logging, "Trigger ID: %u", Trigger_ID);

    if (GetPlayer()->IsTaxiFlying())
    {
        LOG_DEBUG(logging,
            "Player '%s' (GUID: %u) in flight, ignore Area Trigger ID: %u",
            GetPlayer()->GetName(), GetPlayer()->GetGUIDLow(), Trigger_ID);
        return;
    }

    AreaTriggerEntry const* atEntry = sAreaTriggerStore.LookupEntry(Trigger_ID);
    if (!atEntry)
    {
        LOG_DEBUG(logging,
            "Player '%s' (GUID: %u) send unknown (by DBC) Area Trigger ID: %u",
            GetPlayer()->GetName(), GetPlayer()->GetGUIDLow(), Trigger_ID);
        return;
    }

    // delta is safe radius
    const float delta = 5.0f;
    // check if player in the range of areatrigger
    Player* pl = GetPlayer();

    if (!IsPointInAreaTriggerZone(
            atEntry, pl->GetMapId(), pl->GetX(), pl->GetY(), pl->GetZ(), delta))
    {
        LOG_DEBUG(logging,
            "Player '%s' (GUID: %u) too far, ignore Area Trigger ID: %u",
            pl->GetName(), pl->GetGUIDLow(), Trigger_ID);
        return;
    }

    if (sScriptMgr::Instance()->OnAreaTrigger(pl, atEntry))
        return;

    uint32 quest_id =
        sObjectMgr::Instance()->GetQuestForAreaTrigger(Trigger_ID);
    if (quest_id && pl->isAlive() && pl->IsActiveQuest(quest_id))
    {
        Quest const* pQuest =
            sObjectMgr::Instance()->GetQuestTemplate(quest_id);
        if (pQuest)
        {
            if (pl->GetQuestStatus(quest_id) == QUEST_STATUS_INCOMPLETE)
                pl->AreaExploredOrEventHappens(quest_id);
        }
    }

    // enter to tavern, not overwrite city rest
    if (sObjectMgr::Instance()->IsTavernAreaTrigger(Trigger_ID))
    {
        // set resting flag we are in the inn
        if (pl->GetRestType() != REST_TYPE_IN_CITY)
            pl->SetRestType(REST_TYPE_IN_TAVERN, Trigger_ID);
        return;
    }

    if (pl->InBattleGround())
    {
        if (BattleGround* bg = pl->GetBattleGround())
            bg->HandleAreaTrigger(pl, Trigger_ID);
        return;
    }

    // NULL if all values default (non teleport trigger)
    AreaTrigger const* at = sObjectMgr::Instance()->GetAreaTrigger(Trigger_ID);
    if (!at)
        return;

    // Check area-trigger specific requirements (requirements that don't apply
    // to the target map, but just this area trigger)
    if (!_player->isGameMaster())
    {
        if (_player->getLevel() < at->requiredLevel)
        {
            SendAreaTriggerMessage(
                "You need to be level %u to enter this portal.",
                at->requiredLevel);
            return;
        }

        if (at->requiredQuest &&
            !_player->GetQuestRewardStatus(at->requiredQuest))
            return;
    }

    if (!sMapMgr::Instance()->CanPlayerEnter(at->target_mapId, _player))
        return;

    GetPlayer()->TeleportTo(at->target_mapId, at->target_X, at->target_Y,
        at->target_Z, at->target_Orientation, TELE_TO_NOT_LEAVE_TRANSPORT);
}

void WorldSession::HandleUpdateAccountData(WorldPacket& recv_data)
{
    recv_data.rpos(recv_data.wpos()); // prevent spam at unimplemented packet
}

void WorldSession::HandleRequestAccountData(WorldPacket& /*recv_data*/)
{
}

void WorldSession::HandleSetActionButtonOpcode(WorldPacket& recv_data)
{
    uint8 button;
    uint32 packetData;
    recv_data >> button >> packetData;

    uint32 action = ACTION_BUTTON_ACTION(packetData);
    uint8 type = ACTION_BUTTON_TYPE(packetData);

    LOG_DEBUG(logging, "BUTTON: %u ACTION: %u TYPE: %u", button, action, type);
    if (!packetData)
    {
        LOG_DEBUG(logging, "MISC: Remove action from button %u", button);
        GetPlayer()->removeActionButton(button);
    }
    else
    {
        switch (type)
        {
        case ACTION_BUTTON_MACRO:
        case ACTION_BUTTON_CMACRO:
            LOG_DEBUG(
                logging, "MISC: Added Macro %u into button %u", action, button);
            break;
        case ACTION_BUTTON_SPELL:
            LOG_DEBUG(
                logging, "MISC: Added Spell %u into button %u", action, button);
            break;
        case ACTION_BUTTON_ITEM:
            LOG_DEBUG(
                logging, "MISC: Added Item %u into button %u", action, button);
            break;
        default:
            logging.error(
                "MISC: Unknown action button type %u for action %u into button "
                "%u",
                type, action, button);
            return;
        }
        GetPlayer()->addActionButton(button, action, type);
    }
}

void WorldSession::HandleCompleteCinematic(WorldPacket& /*recv_data*/)
{
    LOG_DEBUG(logging, "WORLD: Player is watching cinema");
}

void WorldSession::HandleNextCinematicCamera(WorldPacket& /*recv_data*/)
{
    LOG_DEBUG(logging, "WORLD: Which movie to play");
}

void WorldSession::HandleFeatherFallAck(WorldPacket& recv_data)
{
    // no used
    recv_data.rpos(recv_data.wpos()); // prevent warnings spam
}

void WorldSession::HandleMoveUnRootAck(WorldPacket& recv_data)
{
    // no used
    recv_data.rpos(recv_data.wpos()); // prevent warnings spam
                                      /*
                                          ObjectGuid guid;
                                          recv_data >> guid;
                                  
                                          // now can skip not our packet
                                          if(_player->GetGUID() != guid)
                                          {
                                              recv_data.rpos(recv_data.wpos());                   // prevent
                                         warnings spam
                                              return;
                                          }
                                  
                                          recv_data.read_skip<uint32>();                          // unk
                                  
                                          MovementInfo movementInfo;
                                          ReadMovementInfo(recv_data, &movementInfo);
                                      */
}

void WorldSession::HandleMoveRootAck(WorldPacket& recv_data)
{
    // no used
    recv_data.rpos(recv_data.wpos()); // prevent warnings spam
                                      /*
                                          ObjectGuid guid;
                                          recv_data >> guid;
                                  
                                          // now can skip not our packet
                                          if(_player->GetObjectGuid() != guid)
                                          {
                                              recv_data.rpos(recv_data.wpos());                   // prevent
                                         warnings spam
                                              return;
                                          }
                                  
                                          recv_data.read_skip<uint32>();                          // unk
                                  
                                          MovementInfo movementInfo;
                                          ReadMovementInfo(recv_data, &movementInfo);
                                      */
}

void WorldSession::HandleSetActionBarTogglesOpcode(WorldPacket& recv_data)
{
    uint8 ActionBar;

    recv_data >> ActionBar;

    if (!GetPlayer()) // ignore until not logged (check needed because
                      // STATUS_AUTHED)
    {
        if (ActionBar != 0)
            logging.error(
                "WorldSession::HandleSetActionBarToggles in not logged state "
                "with value: %u, ignored",
                uint32(ActionBar));
        return;
    }

    GetPlayer()->SetByteValue(PLAYER_FIELD_BYTES, 2, ActionBar);
}

void WorldSession::HandlePlayedTime(WorldPacket& /*recv_data*/)
{
    WorldPacket data(SMSG_PLAYED_TIME, 4 + 4);
    data << uint32(_player->GetTotalPlayedTime());
    data << uint32(_player->GetLevelPlayedTime());
    send_packet(std::move(data));
}

void WorldSession::HandleInspectOpcode(WorldPacket& recv_data)
{
    ObjectGuid guid;
    recv_data >> guid;
    LOG_DEBUG(logging, "Inspected guid is %s", guid.GetString().c_str());

    _player->SetSelectionGuid(guid);

    Player* plr = sObjectMgr::Instance()->GetPlayer(guid);
    if (!plr) // wrong player
        return;

    static const uint32 talent_points = 0x3D;
    WorldPacket data(SMSG_INSPECT_TALENT, 4 + talent_points);
    data << plr->GetPackGUID();
    data << uint32(talent_points);

    size_t start_talent_pos = data.wpos();

    // fill by 0 talents array
    for (uint32 i = 0; i < talent_points; ++i)
        data << uint8(0);

    if (sWorld::Instance()->getConfig(CONFIG_BOOL_TALENTS_INSPECTING) ||
        _player->isGameMaster())
    {
        // find class talent tabs (all players have 3 talent tabs)
        uint32 const* talentTabIds = GetTalentTabPages(plr->getClass());

        uint32 talentTabPos =
            0; // pos of first talent rank in tab including all prev tabs
        for (uint32 i = 0; i < 3; ++i)
        {
            uint32 talentTabId = talentTabIds[i];

            // fill by real data
            for (uint32 talentId = 0; talentId < sTalentStore.GetNumRows();
                 ++talentId)
            {
                TalentEntry const* talentInfo =
                    sTalentStore.LookupEntry(talentId);
                if (!talentInfo)
                    continue;

                // skip another tab talents
                if (talentInfo->TalentTab != talentTabId)
                    continue;

                // find talent rank
                uint32 curtalent_maxrank = 0;
                for (uint32 k = MAX_TALENT_RANK; k > 0; --k)
                {
                    if (talentInfo->RankID[k - 1] &&
                        plr->HasSpell(talentInfo->RankID[k - 1]))
                    {
                        curtalent_maxrank = k;
                        break;
                    }
                }

                // not learned talent
                if (!curtalent_maxrank)
                    continue;

                // 1 rank talent bit index
                uint32 curtalent_index =
                    talentTabPos + GetTalentInspectBitPosInTab(talentId);

                uint32 curtalent_rank_index =
                    curtalent_index + curtalent_maxrank - 1;

                // slot/offset in 7-bit bytes
                uint32 curtalent_rank_slot7 = curtalent_rank_index / 7;
                uint32 curtalent_rank_offset7 = curtalent_rank_index % 7;

                // rank pos with skipped 8 bit
                uint32 curtalent_rank_index2 =
                    curtalent_rank_slot7 * 8 + curtalent_rank_offset7;

                // slot/offset in 8-bit bytes with skipped high bit
                uint32 curtalent_rank_slot = curtalent_rank_index2 / 8;
                uint32 curtalent_rank_offset = curtalent_rank_index2 % 8;

                // apply mask
                uint32 val =
                    data.read<uint8>(start_talent_pos + curtalent_rank_slot);
                val |= (1 << curtalent_rank_offset);
                data.put<uint8>(
                    start_talent_pos + curtalent_rank_slot, val & 0xFF);
            }

            talentTabPos += GetTalentTabInspectBitSize(talentTabId);
        }
    }

    send_packet(std::move(data));
}

void WorldSession::HandleInspectHonorStatsOpcode(WorldPacket& recv_data)
{
    ObjectGuid guid;
    recv_data >> guid;

    Player* player = sObjectMgr::Instance()->GetPlayer(guid);

    if (!player)
    {
        logging.error("InspectHonorStats: WTF, player not found...");
        return;
    }

    WorldPacket data(MSG_INSPECT_HONOR_STATS, 8 + 1 + 4 * 4);
    data << player->GetObjectGuid();
    data << uint8(player->GetUInt32Value(PLAYER_FIELD_HONOR_CURRENCY));
    data << uint32(player->GetUInt32Value(PLAYER_FIELD_KILLS));
    data << uint32(player->GetUInt32Value(PLAYER_FIELD_TODAY_CONTRIBUTION));
    data << uint32(player->GetUInt32Value(PLAYER_FIELD_YESTERDAY_CONTRIBUTION));
    data << uint32(
        player->GetUInt32Value(PLAYER_FIELD_LIFETIME_HONORBALE_KILLS));
    send_packet(std::move(data));
}

void WorldSession::HandleWorldTeleportOpcode(WorldPacket& recv_data)
{
    // write in client console: worldport 469 452 6454 2536 180 or /console
    // worldport 469 452 6454 2536 180
    // Received opcode CMSG_WORLD_TELEPORT
    // Time is ***, map=469, x=452.000000, y=6454.000000, z=2536.000000,
    // orient=3.141593

    uint32 time;
    uint32 mapid;
    float PositionX;
    float PositionY;
    float PositionZ;
    float Orientation;

    recv_data >> time; // time in m.sec.
    recv_data >> mapid;
    recv_data >> PositionX;
    recv_data >> PositionY;
    recv_data >> PositionZ;
    recv_data >> Orientation; // o (3.141593 = 180 degrees)

    // if(GetPlayer()->IsTaxiFlying())
    {
        LOG_DEBUG(logging,
            "Player '%s' (GUID: %u) in flight, ignore worldport command.",
            GetPlayer()->GetName(), GetPlayer()->GetGUIDLow());
        return;
    }

    LOG_DEBUG(logging, "Time %u sec, map=%u, x=%f, y=%f, z=%f, orient=%f",
        time / 1000, mapid, PositionX, PositionY, PositionZ, Orientation);

    if (GetSecurity() >= SEC_FULL_GM)
        GetPlayer()->TeleportTo(
            mapid, PositionX, PositionY, PositionZ, Orientation);
    else
        SendNotification(LANG_YOU_NOT_HAVE_PERMISSION);
    LOG_DEBUG(logging, "Received worldport command from player %s",
        GetPlayer()->GetName());
}

void WorldSession::HandleWhoisOpcode(WorldPacket& recv_data)
{
    std::string charname;
    recv_data >> charname;

    if (GetSecurity() < SEC_FULL_GM)
    {
        SendNotification(LANG_YOU_NOT_HAVE_PERMISSION);
        return;
    }

    if (charname.empty() || !normalizePlayerName(charname))
    {
        SendNotification(LANG_NEED_CHARACTER_NAME);
        return;
    }

    Player* plr = sObjectMgr::Instance()->GetPlayer(charname.c_str());

    if (!plr)
    {
        SendNotification(LANG_PLAYER_NOT_EXIST_OR_OFFLINE, charname.c_str());
        return;
    }

    uint32 accid = plr->GetSession()->GetAccountId();

    std::unique_ptr<QueryResult> result(LoginDatabase.PQuery(
        "SELECT username,email,last_ip FROM account WHERE id=%u", accid));
    if (!result)
    {
        SendNotification(LANG_ACCOUNT_FOR_PLAYER_NOT_FOUND, charname.c_str());
        return;
    }

    Field* fields = result->Fetch();
    std::string acc = fields[0].GetCppString();
    if (acc.empty())
        acc = "Unknown";
    std::string email = fields[1].GetCppString();
    if (email.empty())
        email = "Unknown";
    std::string lastip = fields[2].GetCppString();
    if (lastip.empty())
        lastip = "Unknown";

    std::string msg = charname + "'s " + "account is " + acc + ", e-mail: " +
                      email + ", last ip: " + lastip;

    WorldPacket data(SMSG_WHOIS, msg.size() + 1);
    data << msg;
    _player->GetSession()->send_packet(std::move(data));

    LOG_DEBUG(logging, "Received whois command from player %s for character %s",
        GetPlayer()->GetName(), charname.c_str());
}

void WorldSession::HandleComplainOpcode(WorldPacket& recv_data)
{
    uint8 spam_type; // 0 - mail, 1 - chat
    ObjectGuid spammerGuid;
    uint32 unk1 = 0;
    uint32 unk2 = 0;
    uint32 unk3 = 0;
    uint32 unk4 = 0;
    std::string description = "";
    recv_data >> spam_type;   // unk 0x01 const, may be spam type (mail/chat)
    recv_data >> spammerGuid; // player guid
    switch (spam_type)
    {
    case 0:
        recv_data >> unk1; // const 0
        recv_data >> unk2; // probably mail id
        recv_data >> unk3; // const 0
        break;
    case 1:
        recv_data >> unk1;        // probably language
        recv_data >> unk2;        // message type?
        recv_data >> unk3;        // probably channel id
        recv_data >> unk4;        // unk random value
        recv_data >> description; // spam description string (messagetype,
                                  // channel name, player name, message)
        break;
    }

    // NOTE: all chat messages from this spammer automatically ignored by spam
    // reporter until logout in case chat spam.
    // if it's mail spam - ALL mails from this spammer automatically removed by
    // client

    // Complaint Received message
    WorldPacket data(SMSG_COMPLAIN_RESULT, 1);
    data << uint8(0);
    send_packet(std::move(data));

    LOG_DEBUG(logging,
        "REPORT SPAM: type %u, spammer %s, unk1 %u, unk2 %u, unk3 %u, unk4 %u, "
        "message %s",
        spam_type, spammerGuid.GetString().c_str(), unk1, unk2, unk3, unk4,
        description.c_str());
}

void WorldSession::HandleRealmSplitOpcode(WorldPacket& recv_data)
{
    uint32 unk;
    std::string split_date = "01/01/01";
    recv_data >> unk;

    WorldPacket data(SMSG_REALM_SPLIT, 4 + 4 + split_date.size() + 1);
    data << unk;
    data << uint32(0x00000000); // realm split state
    // split states:
    // 0x0 realm normal
    // 0x1 realm split
    // 0x2 realm split pending
    data << split_date;
    send_packet(std::move(data));
    // LOG_DEBUG(logging,"response sent %u", unk);
}

void WorldSession::HandleFarSightOpcode(WorldPacket& recv_data)
{
    uint8 op;
    recv_data >> op;

    WorldObject* obj =
        _player->GetMap()->GetWorldObject(_player->GetFarSightGuid());
    if (!obj)
        return;

    switch (op)
    {
    case 0:
        LOG_DEBUG(
            logging, "Removed FarSight from %s", _player->GetGuidStr().c_str());
        _player->GetCamera().ResetView(false);
        break;
    case 1:
        LOG_DEBUG(logging, "Added FarSight %s to %s",
            _player->GetFarSightGuid().GetString().c_str(),
            _player->GetGuidStr().c_str());
        _player->GetCamera().SetView(obj, false);
        break;
    }
}

void WorldSession::HandleSetTitleOpcode(WorldPacket& recv_data)
{
    int32 title;
    recv_data >> title;

    // -1 at none
    if (title > 0 && title < MAX_TITLE_INDEX)
    {
        if (!GetPlayer()->HasTitle(title))
            return;
    }
    else
        title = 0;

    GetPlayer()->SetUInt32Value(PLAYER_CHOSEN_TITLE, title);
}

void WorldSession::HandleTimeSyncResp(WorldPacket& recv_data)
{
    uint32 counter, clientTicks;
    recv_data >> counter >> clientTicks;

#ifndef OPTIMIZED_BUILD
    if (counter != _player->m_timeSyncCounter - 1)
        LOG_DEBUG(logging, "Wrong time sync counter from player %s (cheater?)",
            _player->GetName());
#endif

    client_tick_count = clientTicks;
    server_tick_count = WorldTimer::getMSTime();
}

void WorldSession::HandleResetInstancesOpcode(WorldPacket& /*recv_data*/)
{
    if (Group* pGroup = _player->GetGroup())
    {
        if (pGroup->IsLeader(_player->GetObjectGuid()))
            pGroup->ResetInstances(INSTANCE_RESET_ALL, _player);
    }
    else
        _player->ResetInstances(INSTANCE_RESET_ALL);
}

void WorldSession::HandleSetDungeonDifficultyOpcode(WorldPacket& recv_data)
{
    LOG_DEBUG(logging, "MSG_SET_DUNGEON_DIFFICULTY");

    uint32 mode;
    recv_data >> mode;

    if (mode >= MAX_DIFFICULTY)
    {
        logging.error(
            "WorldSession::HandleSetDungeonDifficultyOpcode: player %d sent an "
            "invalid instance mode %d!",
            _player->GetGUIDLow(), mode);
        return;
    }

    if (Difficulty(mode) == _player->GetDifficulty())
        return;

    // cannot reset while in an instance
    Map* map = _player->GetMap();
    if (map && map->IsDungeon())
    {
        logging.error(
            "WorldSession::HandleSetDungeonDifficultyOpcode: player %d tried "
            "to reset the instance while inside!",
            _player->GetGUIDLow());
        return;
    }

    if (_player->getLevel() < LEVELREQUIREMENT_HEROIC)
        return;

    if (Group* pGroup = _player->GetGroup())
    {
        if (pGroup->IsLeader(_player->GetObjectGuid()))
        {
            pGroup->ResetInstances(INSTANCE_RESET_CHANGE_DIFFICULTY, _player);
            pGroup->SetDifficulty(Difficulty(mode));
        }
    }
    else
    {
        _player->ResetInstances(INSTANCE_RESET_CHANGE_DIFFICULTY);
        _player->SetDifficulty(Difficulty(mode));
    }
}

void WorldSession::HandleCancelMountAuraOpcode(WorldPacket& /*recv_data*/)
{
    // If player is not mounted, so go out :)
    if (!_player->IsMounted()) // not blizz like; no any messages on blizz
    {
        ChatHandler(this).SendSysMessage(LANG_CHAR_NON_MOUNTED);
        return;
    }

    if (_player->IsTaxiFlying()) // not blizz like; no any messages on blizz
    {
        ChatHandler(this).SendSysMessage(LANG_YOU_IN_FLIGHT);
        return;
    }

    _player->Unmount(_player->HasAuraType(SPELL_AURA_MOUNTED));
    _player->remove_auras(SPELL_AURA_MOUNTED);
}

void WorldSession::HandleMoveSetCanFlyAckOpcode(WorldPacket& recv_data)
{
    MovementInfo movementInfo;

    recv_data >> Unused<uint64>(); // guid
    recv_data >> Unused<uint32>(); // unk
    recv_data >> movementInfo;
    recv_data >> Unused<uint32>(); // unk2

    _player->m_movementInfo.SetMovementFlags(movementInfo.GetMovementFlags());
}

void WorldSession::HandleRequestPetInfoOpcode(WorldPacket& /*recv_data */)
{
    // Sent by client on "/console ReloadUI" to get the pet's action bar again
    _player->PetSpellInitialize();
}

void WorldSession::HandleSetTaxiBenchmarkOpcode(WorldPacket& recv_data)
{
    uint8 mode;
    recv_data >> mode;

    LOG_DEBUG(logging, "Client used \"/timetest %d\" command", mode);
}
