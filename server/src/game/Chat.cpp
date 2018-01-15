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

#include "Chat.h"
#include "AccountMgr.h"
#include "GameEventMgr.h"
#include "Language.h"
#include "logging.h"
#include "ObjectGuid.h"
#include "ObjectMgr.h"
#include "Opcodes.h"
#include "Player.h"
#include "PoolManager.h"
#include "SpellMgr.h"
#include "UpdateMask.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "Database/DatabaseEnv.h"

static auto& gm_logger = logging.get_logger("gm.command");

// Supported shift-links (client generated and server side)
// |color|Harea:area_id|h[name]|h|r
// |color|Hareatrigger:id|h[name]|h|r
// |color|Hareatrigger_target:id|h[name]|h|r
// |color|Hcreature:creature_guid|h[name]|h|r
// |color|Hcreature_entry:creature_id|h[name]|h|r
// |color|Henchant:recipe_spell_id|h[prof_name: recipe_name]|h|r          -
// client, at shift click in recipes list dialog
// |color|Hgameevent:id|h[name]|h|r
// |color|Hgameobject:go_guid|h[name]|h|r
// |color|Hgameobject_entry:go_id|h[name]|h|r
// |color|Hitem:item_id:perm_ench_id:gem1:gem2:gem3:0:0:0:0|h[name]|h|r   -
// client, item icon shift click
// |color|Hitemset:itemset_id|h[name]|h|r
// |color|Hplayer:name|h[name]|h|r                                        -
// client, in some messages, at click copy only name instead link, so no way
// generate it in client string send to server
// |color|Hpool:pool_id|h[name]|h|r
// |color|Hquest:quest_id:quest_level|h[name]|h|r                         -
// client, quest list name shift-click
// |color|Hskill:skill_id|h[name]|h|r
// |color|Hspell:spell_id|h[name]|h|r                                     -
// client, spellbook spell icon shift-click
// |color|Htalent:talent_id,rank|h[name]|h|r                              -
// client, talent icon shift-click rank==-1 if shift-copy unlearned talent
// |color|Htaxinode:id|h[name]|h|r
// |color|Htele:id|h[name]|h|r
// |color|Htitle:id|h[name]|h|r

bool ChatHandler::load_command_table = true;

ChatCommand* ChatHandler::getCommandTable()
{
    static ChatCommand accountSetCommandTable[] = {
        {"addon", SEC_FULL_GM, true, &ChatHandler::HandleAccountSetAddonCommand,
         "", nullptr},
        {"gmlevel", SEC_CONSOLE, true,
         &ChatHandler::HandleAccountSetGmLevelCommand, "", nullptr},
        {"password", SEC_CONSOLE, true,
         &ChatHandler::HandleAccountSetPasswordCommand, "", nullptr},
        {nullptr, 0, false, nullptr, "", nullptr}};

    static ChatCommand accountCommandTable[] = {
        {"characters", SEC_FULL_GM, true,
         &ChatHandler::HandleAccountCharactersCommand, "", nullptr},
        {"create", SEC_CONSOLE, true, &ChatHandler::HandleAccountCreateCommand,
         "", nullptr},
        {"delete", SEC_CONSOLE, true, &ChatHandler::HandleAccountDeleteCommand,
         "", nullptr},
        {"onlinelist", SEC_CONSOLE, true,
         &ChatHandler::HandleAccountOnlineListCommand, "", nullptr},
        {"lock", SEC_FULL_GM, true, &ChatHandler::HandleAccountLockCommand, "",
         nullptr},
        {"set", SEC_FULL_GM, true, nullptr, "", accountSetCommandTable},
        {"password", SEC_PLAYER, true,
         &ChatHandler::HandleAccountPasswordCommand, "", nullptr},
        {"", SEC_FULL_GM, true, &ChatHandler::HandleAccountCommand, "",
         nullptr},
        {nullptr, 0, false, nullptr, "", nullptr}};

    static ChatCommand arenaTeamCommandTable[] = {
        {"create", SEC_FULL_GM, false,
         &ChatHandler::HandleArenaTeamCreateCommand, "", nullptr},
        {"list", SEC_FULL_GM, false, &ChatHandler::HandleArenaTeamListCommand,
         "", nullptr},
        {"rating", SEC_FULL_GM, true,
         &ChatHandler::HandleArenaTeamRatingCommand, "", nullptr},
        {nullptr, 0, false, nullptr, "", nullptr}};

    static ChatCommand arenaCommandTable[] = {
        {"team", SEC_FULL_GM, true, nullptr, "", arenaTeamCommandTable},
        {nullptr, 0, false, nullptr, "", nullptr}};

    static ChatCommand auctionCommandTable[] = {
        {"alliance", SEC_FULL_GM, false,
         &ChatHandler::HandleAuctionAllianceCommand, "", nullptr},
        {"goblin", SEC_FULL_GM, false, &ChatHandler::HandleAuctionGoblinCommand,
         "", nullptr},
        {"horde", SEC_FULL_GM, false, &ChatHandler::HandleAuctionHordeCommand,
         "", nullptr},
        {"item", SEC_FULL_GM, true, &ChatHandler::HandleAuctionItemCommand, "",
         nullptr},
        {"", SEC_FULL_GM, false, &ChatHandler::HandleAuctionCommand, "",
         nullptr},
        {nullptr, 0, false, nullptr, "", nullptr}};

    static ChatCommand banCommandTable[] = {
        {"account", SEC_FULL_GM, true, &ChatHandler::HandleBanAccountCommand,
         "", nullptr},
        {"character", SEC_FULL_GM, true,
         &ChatHandler::HandleBanCharacterCommand, "", nullptr},
        {"ip", SEC_FULL_GM, true, &ChatHandler::HandleBanIPCommand, "",
         nullptr},
        {nullptr, 0, false, nullptr, "", nullptr}};

    static ChatCommand baninfoCommandTable[] = {
        {"account", SEC_FULL_GM, true,
         &ChatHandler::HandleBanInfoAccountCommand, "", nullptr},
        {"character", SEC_FULL_GM, true,
         &ChatHandler::HandleBanInfoCharacterCommand, "", nullptr},
        {"ip", SEC_FULL_GM, true, &ChatHandler::HandleBanInfoIPCommand, "",
         nullptr},
        {nullptr, 0, false, nullptr, "", nullptr}};

    static ChatCommand banlistCommandTable[] = {
        {"account", SEC_FULL_GM, true,
         &ChatHandler::HandleBanListAccountCommand, "", nullptr},
        {"character", SEC_FULL_GM, true,
         &ChatHandler::HandleBanListCharacterCommand, "", nullptr},
        {"ip", SEC_FULL_GM, true, &ChatHandler::HandleBanListIPCommand, "",
         nullptr},
        {nullptr, 0, false, nullptr, "", nullptr}};

    static ChatCommand banWaveCommandTable[] = {
        {"add", SEC_FULL_GM, false, &ChatHandler::HandleBanWaveAddCommand, "",
         nullptr},
        {"print", SEC_FULL_GM, false, &ChatHandler::HandleBanWavePrintCommand,
         "", nullptr},
        {"remove", SEC_FULL_GM, false, &ChatHandler::HandleBanWaveRemoveCommand,
         "", nullptr},
        {"run", SEC_FULL_GM, false, &ChatHandler::HandleBanWaveRunCommand, "",
         nullptr},
        {"search", SEC_FULL_GM, false, &ChatHandler::HandleBanWaveSearchCommand,
         "", nullptr},
        {nullptr, 0, false, nullptr, "", nullptr}};

    static ChatCommand castCommandTable[] = {
        {"back", SEC_FULL_GM, false, &ChatHandler::HandleCastBackCommand, "",
         nullptr},
        {"dist", SEC_FULL_GM, false, &ChatHandler::HandleCastDistCommand, "",
         nullptr},
        {"self", SEC_FULL_GM, false, &ChatHandler::HandleCastSelfCommand, "",
         nullptr},
        {"target", SEC_FULL_GM, false, &ChatHandler::HandleCastTargetCommand,
         "", nullptr},
        {"", SEC_FULL_GM, false, &ChatHandler::HandleCastCommand, "", nullptr},
        {nullptr, 0, false, nullptr, "", nullptr}};

    static ChatCommand characterDeletedCommandTable[] = {
        {"delete", SEC_CONSOLE, true,
         &ChatHandler::HandleCharacterDeletedDeleteCommand, "", nullptr},
        {"list", SEC_FULL_GM, true,
         &ChatHandler::HandleCharacterDeletedListCommand, "", nullptr},
        {"restore", SEC_FULL_GM, true,
         &ChatHandler::HandleCharacterDeletedRestoreCommand, "", nullptr},
        {"old", SEC_CONSOLE, true,
         &ChatHandler::HandleCharacterDeletedOldCommand, "", nullptr},
        {nullptr, 0, false, nullptr, "", nullptr}};

    static ChatCommand characterCommandTable[] = {
        {"deleted", SEC_POWER_GM, true, nullptr, "",
         characterDeletedCommandTable},
        {"erase", SEC_CONSOLE, true, &ChatHandler::HandleCharacterEraseCommand,
         "", nullptr},
        {"level", SEC_FULL_GM, true, &ChatHandler::HandleCharacterLevelCommand,
         "", nullptr},
        {"rename", SEC_POWER_GM, true,
         &ChatHandler::HandleCharacterRenameCommand, "", nullptr},
        {"reputation", SEC_POWER_GM, true,
         &ChatHandler::HandleCharacterReputationCommand, "", nullptr},
        {"titles", SEC_POWER_GM, true,
         &ChatHandler::HandleCharacterTitlesCommand, "", nullptr},
        {nullptr, 0, false, nullptr, "", nullptr}};

    static ChatCommand debugPathingCommandTable[] = {
        {"adt", SEC_FULL_GM, false, &ChatHandler::HandleDebugPathingAdtCommand,
         "", nullptr},
        {"path", SEC_FULL_GM, false,
         &ChatHandler::HandleDebugPathingPathCommand, "", nullptr},
        {"position", SEC_FULL_GM, false,
         &ChatHandler::HandleDebugPathingPositionCommand, "", nullptr},
        {"wmo", SEC_FULL_GM, false, &ChatHandler::HandleDebugPathingWmoCommand,
         "", nullptr},
        {nullptr, 0, false, nullptr, "", nullptr}};

    static ChatCommand debugPlayCommandTable[] = {
        {"cinematic", SEC_TICKET_GM, false,
         &ChatHandler::HandleDebugPlayCinematicCommand, "", nullptr},
        {"sound", SEC_TICKET_GM, false,
         &ChatHandler::HandleDebugPlaySoundCommand, "", nullptr},
        {nullptr, 0, false, nullptr, "", nullptr}};

    static ChatCommand debugSamplingCommandTable[] = {
        {"map", SEC_FULL_GM, true, &ChatHandler::HandleDebugSamplingMapCommand,
         "", nullptr},
        {nullptr, 0, false, nullptr, "", nullptr}};

    static ChatCommand debugSendCommandTable[] = {
        {"auctionerror", SEC_FULL_GM, false,
         &ChatHandler::HandleDebugSendAuctionHouseError, "", nullptr},
        {"buyerror", SEC_FULL_GM, false,
         &ChatHandler::HandleDebugSendBuyErrorCommand, "", nullptr},
        {"channelnotify", SEC_FULL_GM, false,
         &ChatHandler::HandleDebugSendChannelNotifyCommand, "", nullptr},
        {"chatmmessage", SEC_FULL_GM, false,
         &ChatHandler::HandleDebugSendChatMsgCommand, "", nullptr},
        {"equiperror", SEC_FULL_GM, false,
         &ChatHandler::HandleDebugSendEquipErrorCommand, "", nullptr},
        {"opcode", SEC_FULL_GM, false,
         &ChatHandler::HandleDebugSendOpcodeCommand, "", nullptr},
        {"poi", SEC_FULL_GM, false, &ChatHandler::HandleDebugSendPoiCommand, "",
         nullptr},
        {"qpartymsg", SEC_FULL_GM, false,
         &ChatHandler::HandleDebugSendQuestPartyMsgCommand, "", nullptr},
        {"qinvalidmsg", SEC_FULL_GM, false,
         &ChatHandler::HandleDebugSendQuestInvalidMsgCommand, "", nullptr},
        {"sellerror", SEC_FULL_GM, false,
         &ChatHandler::HandleDebugSendSellErrorCommand, "", nullptr},
        {"spellfail", SEC_FULL_GM, false,
         &ChatHandler::HandleDebugSendSpellFailCommand, "", nullptr},
        {"resetfail", SEC_FULL_GM, false,
         &ChatHandler::HandleDebugSendResetFailCommand, "", nullptr},
        {"difficulty", SEC_FULL_GM, false,
         &ChatHandler::HandleDebugSendDifficultyCommand, "", nullptr},
        {nullptr, 0, false, nullptr, "", nullptr}};

    static ChatCommand debugCommandTable[] = {
        {"anim", SEC_POWER_GM, false, &ChatHandler::HandleDebugAnimCommand, "",
         nullptr},
        {"battlefieldqueue", SEC_FULL_GM, false,
         &ChatHandler::HandleDebugBattlefieldQueueCommand, "", nullptr},
        {"lootrecipient", SEC_POWER_GM, false,
         &ChatHandler::HandleDebugGetLootRecipientCommand, "", nullptr},
        {"los", SEC_TICKET_GM, false, &ChatHandler::HandleDebugLoSCommand, "",
         nullptr},
        {"getitemvalue", SEC_FULL_GM, false,
         &ChatHandler::HandleDebugGetItemValueCommand, "", nullptr},
        {"getvalue", SEC_FULL_GM, false,
         &ChatHandler::HandleDebugGetValueCommand, "", nullptr},
        {"itemdisplay", SEC_FULL_GM, false,
         &ChatHandler::HandleDebugItemDisplay, "", nullptr},
        {"itemdurability", SEC_FULL_GM, false,
         &ChatHandler::HandleDebugItemDurabilityCommand, "", nullptr},
        {"moditemvalue", SEC_FULL_GM, false,
         &ChatHandler::HandleDebugModItemValueCommand, "", nullptr},
        {"modvalue", SEC_FULL_GM, false,
         &ChatHandler::HandleDebugModValueCommand, "", nullptr},
        {"pathing", SEC_FULL_GM, false, nullptr, "", debugPathingCommandTable},
        {"play", SEC_TICKET_GM, false, nullptr, "", debugPlayCommandTable},
        {"pvp", SEC_FULL_GM, false, &ChatHandler::HandleDebugPvPCommand, "",
         nullptr},
        {"sampling", SEC_FULL_GM, true, nullptr, "", debugSamplingCommandTable},
        {"send", SEC_FULL_GM, false, nullptr, "", debugSendCommandTable},
        {"setaurastate", SEC_FULL_GM, false,
         &ChatHandler::HandleDebugSetAuraStateCommand, "", nullptr},
        {"setitemvalue", SEC_FULL_GM, false,
         &ChatHandler::HandleDebugSetItemValueCommand, "", nullptr},
        {"setvalue", SEC_FULL_GM, false,
         &ChatHandler::HandleDebugSetValueCommand, "", nullptr},
        {"spellcheck", SEC_CONSOLE, true,
         &ChatHandler::HandleDebugSpellCheckCommand, "", nullptr},
        {"spellcoefs", SEC_FULL_GM, true,
         &ChatHandler::HandleDebugSpellCoefsCommand, "", nullptr},
        {"spellmods", SEC_FULL_GM, false,
         &ChatHandler::HandleDebugSpellModsCommand, "", nullptr},
        {"threadedmaps", SEC_FULL_GM, true,
         &ChatHandler::HandleDebugThreadedMaps, "", nullptr},
        {"threatlist", SEC_FULL_GM, true, &ChatHandler::HandleDebugThreatlist,
         "", nullptr},
        {"uws", SEC_FULL_GM, false,
         &ChatHandler::HandleDebugUpdateWorldStateCommand, "", nullptr},
        {"warden", SEC_FULL_GM, true, &ChatHandler::HandleDebugWardenCommand,
         "", nullptr},
        {"wmolist", SEC_FULL_GM, false, &ChatHandler::HandleDebugWmolistCommand,
         "", nullptr},
        {nullptr, 0, false, nullptr, "", nullptr}};

    static ChatCommand eventCommandTable[] = {
        {"list", SEC_POWER_GM, true, &ChatHandler::HandleEventListCommand, "",
         nullptr},
        {"start", SEC_POWER_GM, true, &ChatHandler::HandleEventStartCommand, "",
         nullptr},
        {"stop", SEC_POWER_GM, true, &ChatHandler::HandleEventStopCommand, "",
         nullptr},
        {"", SEC_POWER_GM, true, &ChatHandler::HandleEventInfoCommand, "",
         nullptr},
        {nullptr, 0, false, nullptr, "", nullptr}};

    static ChatCommand gmCommandTable[] = {
        {"chat", SEC_TICKET_GM, false, &ChatHandler::HandleGMChatCommand, "",
         nullptr},
        {"fly", SEC_FULL_GM, false, &ChatHandler::HandleGMFlyCommand, "",
         nullptr},
        {"ingame", SEC_PLAYER, true, &ChatHandler::HandleGMListIngameCommand,
         "", nullptr},
        {"list", SEC_FULL_GM, true, &ChatHandler::HandleGMListFullCommand, "",
         nullptr},
        {"visible", SEC_TICKET_GM, false, &ChatHandler::HandleGMVisibleCommand,
         "", nullptr},
        {"setview", SEC_TICKET_GM, false, &ChatHandler::HandleSetViewCommand,
         "", nullptr},
        {"", SEC_TICKET_GM, false, &ChatHandler::HandleGMCommand, "", nullptr},
        {nullptr, 0, false, nullptr, "", nullptr}};

    static ChatCommand gobjectCommandTable[] = {
        {"add", SEC_POWER_GM, false, &ChatHandler::HandleGameObjectAddCommand,
         "", nullptr},
        {"delete", SEC_POWER_GM, false,
         &ChatHandler::HandleGameObjectDeleteCommand, "", nullptr},
        {"move", SEC_POWER_GM, false, &ChatHandler::HandleGameObjectMoveCommand,
         "", nullptr},
        {"near", SEC_POWER_GM, false, &ChatHandler::HandleGameObjectNearCommand,
         "", nullptr},
        {"target", SEC_POWER_GM, false,
         &ChatHandler::HandleGameObjectTargetCommand, "", nullptr},
        {"turn", SEC_POWER_GM, false, &ChatHandler::HandleGameObjectTurnCommand,
         "", nullptr},
        {nullptr, 0, false, nullptr, "", nullptr}};

    static ChatCommand guildCommandTable[] = {
        {"create", SEC_POWER_GM, true, &ChatHandler::HandleGuildCreateCommand,
         "", nullptr},
        {"delete", SEC_POWER_GM, true, &ChatHandler::HandleGuildDeleteCommand,
         "", nullptr},
        {"invite", SEC_POWER_GM, true, &ChatHandler::HandleGuildInviteCommand,
         "", nullptr},
        {"uninvite", SEC_POWER_GM, true,
         &ChatHandler::HandleGuildUninviteCommand, "", nullptr},
        {"rank", SEC_POWER_GM, true, &ChatHandler::HandleGuildRankCommand, "",
         nullptr},
        {nullptr, 0, false, nullptr, "", nullptr}};

    static ChatCommand honorCommandTable[] = {
        {"add", SEC_POWER_GM, false, &ChatHandler::HandleHonorAddCommand, "",
         nullptr},
        {"addkill", SEC_POWER_GM, false,
         &ChatHandler::HandleHonorAddKillCommand, "", nullptr},
        {"update", SEC_POWER_GM, false, &ChatHandler::HandleHonorUpdateCommand,
         "", nullptr},
        {nullptr, 0, false, nullptr, "", nullptr}};

    static ChatCommand instanceCommandTable[] = {
        {"listbinds", SEC_FULL_GM, false,
         &ChatHandler::HandleInstanceListBindsCommand, "", nullptr},
        {"unbind", SEC_FULL_GM, false,
         &ChatHandler::HandleInstanceUnbindCommand, "", nullptr},
        {"resetlimit", SEC_FULL_GM, false,
         &ChatHandler::HandleInstanceResetLimitCommand, "", nullptr},
        {"stats", SEC_FULL_GM, true, &ChatHandler::HandleInstanceStatsCommand,
         "", nullptr},
        {"savedata", SEC_FULL_GM, false,
         &ChatHandler::HandleInstanceSaveDataCommand, "", nullptr},
        {nullptr, 0, false, nullptr, "", nullptr}};

    static ChatCommand learnCommandTable[] = {
        {"all", SEC_FULL_GM, false, &ChatHandler::HandleLearnAllCommand, "",
         nullptr},
        {"all_gm", SEC_POWER_GM, false, &ChatHandler::HandleLearnAllGMCommand,
         "", nullptr},
        {"all_crafts", SEC_POWER_GM, false,
         &ChatHandler::HandleLearnAllCraftsCommand, "", nullptr},
        {"all_default", SEC_TICKET_GM, false,
         &ChatHandler::HandleLearnAllDefaultCommand, "", nullptr},
        {"all_lang", SEC_TICKET_GM, false,
         &ChatHandler::HandleLearnAllLangCommand, "", nullptr},
        {"all_myclass", SEC_FULL_GM, false,
         &ChatHandler::HandleLearnAllMyClassCommand, "", nullptr},
        {"all_myspells", SEC_FULL_GM, false,
         &ChatHandler::HandleLearnAllMySpellsCommand, "", nullptr},
        {"all_mytalents", SEC_FULL_GM, false,
         &ChatHandler::HandleLearnAllMyTalentsCommand, "", nullptr},
        {"all_recipes", SEC_POWER_GM, false,
         &ChatHandler::HandleLearnAllRecipesCommand, "", nullptr},
        {"", SEC_FULL_GM, false, &ChatHandler::HandleLearnCommand, "", nullptr},
        {nullptr, 0, false, nullptr, "", nullptr}};

    static ChatCommand listCommandTable[] = {
        {"creature", SEC_FULL_GM, true, &ChatHandler::HandleListCreatureCommand,
         "", nullptr},
        {"item", SEC_FULL_GM, true, &ChatHandler::HandleListItemCommand, "",
         nullptr},
        {"object", SEC_FULL_GM, true, &ChatHandler::HandleListObjectCommand, "",
         nullptr},
        {"talents", SEC_FULL_GM, false, &ChatHandler::HandleListTalentsCommand,
         "", nullptr},
        {nullptr, 0, false, nullptr, "", nullptr}};

    static ChatCommand lookupAccountCommandTable[] = {
        {"email", SEC_POWER_GM, true,
         &ChatHandler::HandleLookupAccountEmailCommand, "", nullptr},
        {"ip", SEC_POWER_GM, true, &ChatHandler::HandleLookupAccountIpCommand,
         "", nullptr},
        {"name", SEC_POWER_GM, true,
         &ChatHandler::HandleLookupAccountNameCommand, "", nullptr},
        {nullptr, 0, false, nullptr, "", nullptr}};

    static ChatCommand lookupPlayerCommandTable[] = {
        {"account", SEC_POWER_GM, true,
         &ChatHandler::HandleLookupPlayerAccountCommand, "", nullptr},
        {"email", SEC_POWER_GM, true,
         &ChatHandler::HandleLookupPlayerEmailCommand, "", nullptr},
        {"ip", SEC_POWER_GM, true, &ChatHandler::HandleLookupPlayerIpCommand,
         "", nullptr},
        {nullptr, 0, false, nullptr, "", nullptr}};

    static ChatCommand lookupCommandTable[] = {
        {"account", SEC_POWER_GM, true, nullptr, "", lookupAccountCommandTable},
        {"area", SEC_TICKET_GM, true, &ChatHandler::HandleLookupAreaCommand, "",
         nullptr},
        {"creature", SEC_FULL_GM, true,
         &ChatHandler::HandleLookupCreatureCommand, "", nullptr},
        {"event", SEC_POWER_GM, true, &ChatHandler::HandleLookupEventCommand,
         "", nullptr},
        {"faction", SEC_FULL_GM, true, &ChatHandler::HandleLookupFactionCommand,
         "", nullptr},
        {"item", SEC_FULL_GM, true, &ChatHandler::HandleLookupItemCommand, "",
         nullptr},
        {"itemset", SEC_FULL_GM, true, &ChatHandler::HandleLookupItemSetCommand,
         "", nullptr},
        {"object", SEC_FULL_GM, true, &ChatHandler::HandleLookupObjectCommand,
         "", nullptr},
        {"quest", SEC_FULL_GM, true, &ChatHandler::HandleLookupQuestCommand, "",
         nullptr},
        {"player", SEC_POWER_GM, true, nullptr, "", lookupPlayerCommandTable},
        {"pool", SEC_POWER_GM, true, &ChatHandler::HandleLookupPoolCommand, "",
         nullptr},
        {"skill", SEC_FULL_GM, true, &ChatHandler::HandleLookupSkillCommand, "",
         nullptr},
        {"spell", SEC_FULL_GM, true, &ChatHandler::HandleLookupSpellCommand, "",
         nullptr},
        {"taxinode", SEC_FULL_GM, true,
         &ChatHandler::HandleLookupTaxiNodeCommand, "", nullptr},
        {"tploc", SEC_TICKET_GM, true, &ChatHandler::HandleLookupTplocCommand,
         "", nullptr},
        {"title", SEC_POWER_GM, true, &ChatHandler::HandleLookupTitleCommand,
         "", nullptr},
        {nullptr, 0, false, nullptr, "", nullptr}};

    static ChatCommand mmapCommandTable[] = {
        {"path", SEC_POWER_GM, false, &ChatHandler::HandleMmapPathCommand, "",
         nullptr},
        {"loc", SEC_POWER_GM, false, &ChatHandler::HandleMmapLocCommand, "",
         nullptr},
        {"loadedtiles", SEC_POWER_GM, false,
         &ChatHandler::HandleMmapLoadedTilesCommand, "", nullptr},
        {"stats", SEC_POWER_GM, false, &ChatHandler::HandleMmapStatsCommand, "",
         nullptr},
        {"testarea", SEC_POWER_GM, false, &ChatHandler::HandleMmapTestArea, "",
         nullptr},
        {"", SEC_FULL_GM, false, &ChatHandler::HandleMmap, "", nullptr},
        {nullptr, 0, false, nullptr, "", nullptr}};

    static ChatCommand modifyCommandTable[] = {
        {"hp", SEC_TICKET_GM, false, &ChatHandler::HandleModifyHPCommand, "",
         nullptr},
        {"mana", SEC_TICKET_GM, false, &ChatHandler::HandleModifyManaCommand,
         "", nullptr},
        {"rage", SEC_TICKET_GM, false, &ChatHandler::HandleModifyRageCommand,
         "", nullptr},
        {"energy", SEC_TICKET_GM, false,
         &ChatHandler::HandleModifyEnergyCommand, "", nullptr},
        {"money", SEC_TICKET_GM, false, &ChatHandler::HandleModifyMoneyCommand,
         "", nullptr},
        {"speed", SEC_TICKET_GM, false, &ChatHandler::HandleModifySpeedCommand,
         "", nullptr},
        {"swim", SEC_TICKET_GM, false, &ChatHandler::HandleModifySwimCommand,
         "", nullptr},
        {"scale", SEC_TICKET_GM, false, &ChatHandler::HandleModifyScaleCommand,
         "", nullptr},
        {"bwalk", SEC_TICKET_GM, false, &ChatHandler::HandleModifyBWalkCommand,
         "", nullptr},
        {"fly", SEC_TICKET_GM, false, &ChatHandler::HandleModifyFlyCommand, "",
         nullptr},
        {"aspeed", SEC_TICKET_GM, false,
         &ChatHandler::HandleModifyASpeedCommand, "", nullptr},
        {"faction", SEC_TICKET_GM, false,
         &ChatHandler::HandleModifyFactionCommand, "", nullptr},
        {"tp", SEC_TICKET_GM, false, &ChatHandler::HandleModifyTalentCommand,
         "", nullptr},
        {"mount", SEC_TICKET_GM, false, &ChatHandler::HandleModifyMountCommand,
         "", nullptr},
        {"honor", SEC_TICKET_GM, false, &ChatHandler::HandleModifyHonorCommand,
         "", nullptr},
        {"rep", SEC_POWER_GM, false, &ChatHandler::HandleModifyRepCommand, "",
         nullptr},
        {"arena", SEC_TICKET_GM, false, &ChatHandler::HandleModifyArenaCommand,
         "", nullptr},
        {"drunk", SEC_TICKET_GM, false, &ChatHandler::HandleModifyDrunkCommand,
         "", nullptr},
        {"standstate", SEC_POWER_GM, false,
         &ChatHandler::HandleModifyStandStateCommand, "", nullptr},
        {"morph", SEC_POWER_GM, false, &ChatHandler::HandleModifyMorphCommand,
         "", nullptr},
        {"gender", SEC_POWER_GM, false, &ChatHandler::HandleModifyGenderCommand,
         "", nullptr},
        {nullptr, 0, false, nullptr, "", nullptr}};

    static ChatCommand npcGroupFlagCommandTable[] = {
        {"add", SEC_POWER_GM, false, &ChatHandler::HandleNpcGroupFlagAddCommand,
         "", nullptr},
        {"list", SEC_POWER_GM, false,
         &ChatHandler::HandleNpcGroupFlagListCommand, "", nullptr},
        {"remove", SEC_POWER_GM, false,
         &ChatHandler::HandleNpcGroupFlagRemoveCommand, "", nullptr},
        {nullptr, 0, false, nullptr, "", nullptr}};

    static ChatCommand npcGroupWpCommandTable[] = {
        {"add", SEC_POWER_GM, false, &ChatHandler::HandleNpcGroupWpAddCommand,
         "", nullptr},
        {"edit", SEC_POWER_GM, false, &ChatHandler::HandleNpcGroupWpEditCommand,
         "", nullptr},
        {"move", SEC_POWER_GM, false, &ChatHandler::HandleNpcGroupWpMoveCommand,
         "", nullptr},
        {"remove", SEC_POWER_GM, false,
         &ChatHandler::HandleNpcGroupWpRemoveCommand, "", nullptr},
        {"show", SEC_POWER_GM, false, &ChatHandler::HandleNpcGroupWpShowCommand,
         "", nullptr},
        {"unshow", SEC_POWER_GM, false,
         &ChatHandler::HandleNpcGroupWpUnshowCommand, "", nullptr},
        {nullptr, 0, false, nullptr, "", nullptr}};

    static ChatCommand npcGroupCommandTable[] = {
        {"add", SEC_POWER_GM, false, &ChatHandler::HandleNpcGroupAddCommand, "",
         nullptr},
        {"create", SEC_POWER_GM, false,
         &ChatHandler::HandleNpcGroupCreateCommand, "", nullptr},
        {"delete", SEC_POWER_GM, false,
         &ChatHandler::HandleNpcGroupDeleteCommand, "", nullptr},
        {"flag", SEC_POWER_GM, false, nullptr, "", npcGroupFlagCommandTable},
        {"go", SEC_POWER_GM, false, &ChatHandler::HandleNpcGroupGoCommand, "",
         nullptr},
        {"gwp", SEC_POWER_GM, false, nullptr, "", npcGroupWpCommandTable},
        {"info", SEC_POWER_GM, false, &ChatHandler::HandleNpcGroupInfoCommand,
         "", nullptr},
        {"leader", SEC_POWER_GM, false,
         &ChatHandler::HandleNpcGroupLeaderCommand, "", nullptr},
        {"list", SEC_POWER_GM, false, &ChatHandler::HandleNpcGroupListCommand,
         "", nullptr},
        {"remove", SEC_POWER_GM, false,
         &ChatHandler::HandleNpcGroupRemoveCommand, "", nullptr},
        {"rename", SEC_POWER_GM, false,
         &ChatHandler::HandleNpcGroupRenameCommand, "", nullptr},
        {"show", SEC_POWER_GM, false, &ChatHandler::HandleNpcGroupShowCommand,
         "", nullptr},
        {"unshow", SEC_POWER_GM, false,
         &ChatHandler::HandleNpcGroupUnshowCommand, "", nullptr},
        {nullptr, 0, false, nullptr, "", nullptr}};

    static ChatCommand npcCommandTable[] = {
        {"add", SEC_POWER_GM, false, &ChatHandler::HandleNpcAddCommand, "",
         nullptr},
        {"additem", SEC_POWER_GM, false,
         &ChatHandler::HandleNpcAddVendorItemCommand, "", nullptr},
        {"aggrolink", SEC_FULL_GM, false,
         &ChatHandler::HandleNpcAggroLinkCommand, "", nullptr},
        {"addmove", SEC_POWER_GM, false, &ChatHandler::HandleNpcAddMoveCommand,
         "", nullptr},
        {"aiinfo", SEC_POWER_GM, false, &ChatHandler::HandleNpcAIInfoCommand,
         "", nullptr},
        {"allowmove", SEC_FULL_GM, false,
         &ChatHandler::HandleNpcAllowMovementCommand, "", nullptr},
        {"bosslink", SEC_POWER_GM, false,
         &ChatHandler::HandleNpcBossLinkCommand, "", nullptr},
        {"changeentry", SEC_FULL_GM, false,
         &ChatHandler::HandleNpcChangeEntryCommand, "", nullptr},
        {"changelevel", SEC_POWER_GM, false,
         &ChatHandler::HandleNpcChangeLevelCommand, "", nullptr},
        {"delete", SEC_POWER_GM, false, &ChatHandler::HandleNpcDeleteCommand,
         "", nullptr},
        {"delitem", SEC_POWER_GM, false,
         &ChatHandler::HandleNpcDelVendorItemCommand, "", nullptr},
        {"factionid", SEC_POWER_GM, false,
         &ChatHandler::HandleNpcFactionIdCommand, "", nullptr},
        {"flag", SEC_POWER_GM, false, &ChatHandler::HandleNpcFlagCommand, "",
         nullptr},
        {"follow", SEC_POWER_GM, false, &ChatHandler::HandleNpcFollowCommand,
         "", nullptr},
        {"group", SEC_POWER_GM, false, nullptr, "", npcGroupCommandTable},
        {"info", SEC_FULL_GM, false, &ChatHandler::HandleNpcInfoCommand, "",
         nullptr},
        {"leash", SEC_FULL_GM, false, &ChatHandler::HandleNpcLeashCommand, "",
         nullptr},
        {"move", SEC_POWER_GM, false, &ChatHandler::HandleNpcMoveCommand, "",
         nullptr},
        {"playemote", SEC_FULL_GM, false,
         &ChatHandler::HandleNpcPlayEmoteCommand, "", nullptr},
        {"setmodel", SEC_POWER_GM, false,
         &ChatHandler::HandleNpcSetModelCommand, "", nullptr},
        {"setmovetype", SEC_POWER_GM, false,
         &ChatHandler::HandleNpcSetMoveTypeCommand, "", nullptr},
        {"spawndist", SEC_POWER_GM, false,
         &ChatHandler::HandleNpcSpawnDistCommand, "", nullptr},
        {"spawntime", SEC_POWER_GM, false,
         &ChatHandler::HandleNpcSpawnTimeCommand, "", nullptr},
        {"say", SEC_TICKET_GM, false, &ChatHandler::HandleNpcSayCommand, "",
         nullptr},
        {"textemote", SEC_TICKET_GM, false,
         &ChatHandler::HandleNpcTextEmoteCommand, "", nullptr},
        {"unfollow", SEC_POWER_GM, false,
         &ChatHandler::HandleNpcUnFollowCommand, "", nullptr},
        {"whisper", SEC_TICKET_GM, false, &ChatHandler::HandleNpcWhisperCommand,
         "", nullptr},
        {"yell", SEC_TICKET_GM, false, &ChatHandler::HandleNpcYellCommand, "",
         nullptr},
        {"tame", SEC_POWER_GM, false, &ChatHandler::HandleNpcTameCommand, "",
         nullptr},
        {"setdeathstate", SEC_POWER_GM, false,
         &ChatHandler::HandleNpcSetDeathStateCommand, "", nullptr},

        //{ TODO: fix or remove this commands
        {"addweapon", SEC_FULL_GM, false,
         &ChatHandler::HandleNpcAddWeaponCommand, "", nullptr},
        {"name", SEC_POWER_GM, false, &ChatHandler::HandleNpcNameCommand, "",
         nullptr},
        {"subname", SEC_POWER_GM, false, &ChatHandler::HandleNpcSubNameCommand,
         "", nullptr},
        //}

        {nullptr, 0, false, nullptr, "", nullptr}};

    static ChatCommand poolCommandTable[] = {
        {"list", SEC_POWER_GM, false, &ChatHandler::HandlePoolListCommand, "",
         nullptr},
        {"spawns", SEC_POWER_GM, false, &ChatHandler::HandlePoolSpawnsCommand,
         "", nullptr},
        {"", SEC_POWER_GM, true, &ChatHandler::HandlePoolInfoCommand, "",
         nullptr},
        {nullptr, 0, false, nullptr, "", nullptr}};

    static ChatCommand questCommandTable[] = {
        {"add", SEC_FULL_GM, false, &ChatHandler::HandleQuestAddCommand, "",
         nullptr},
        {"complete", SEC_FULL_GM, false,
         &ChatHandler::HandleQuestCompleteCommand, "", nullptr},
        {"remove", SEC_FULL_GM, false, &ChatHandler::HandleQuestRemoveCommand,
         "", nullptr},
        {nullptr, 0, false, nullptr, "", nullptr}};

    static ChatCommand reloadCommandTable[] = {
        {"all", SEC_FULL_GM, true, &ChatHandler::HandleReloadAllCommand, "",
         nullptr},
        {"all_area", SEC_FULL_GM, true,
         &ChatHandler::HandleReloadAllAreaCommand, "", nullptr},
        {"all_eventai", SEC_FULL_GM, true,
         &ChatHandler::HandleReloadAllEventAICommand, "", nullptr},
        {"all_gossips", SEC_FULL_GM, true,
         &ChatHandler::HandleReloadAllGossipsCommand, "", nullptr},
        {"all_item", SEC_FULL_GM, true,
         &ChatHandler::HandleReloadAllItemCommand, "", nullptr},
        {"all_locales", SEC_FULL_GM, true,
         &ChatHandler::HandleReloadAllLocalesCommand, "", nullptr},
        {"all_loot", SEC_FULL_GM, true,
         &ChatHandler::HandleReloadAllLootCommand, "", nullptr},
        {"all_npc", SEC_FULL_GM, true, &ChatHandler::HandleReloadAllNpcCommand,
         "", nullptr},
        {"all_quest", SEC_FULL_GM, true,
         &ChatHandler::HandleReloadAllQuestCommand, "", nullptr},
        {"all_scripts", SEC_FULL_GM, true,
         &ChatHandler::HandleReloadAllScriptsCommand, "", nullptr},
        {"all_spell", SEC_FULL_GM, true,
         &ChatHandler::HandleReloadAllSpellCommand, "", nullptr},

        {"config", SEC_FULL_GM, true, &ChatHandler::HandleReloadConfigCommand,
         "", nullptr},

        {"areatrigger_involvedrelation", SEC_FULL_GM, true,
         &ChatHandler::HandleReloadQuestAreaTriggersCommand, "", nullptr},
        {"areatrigger_tavern", SEC_FULL_GM, true,
         &ChatHandler::HandleReloadAreaTriggerTavernCommand, "", nullptr},
        {"areatrigger_teleport", SEC_FULL_GM, true,
         &ChatHandler::HandleReloadAreaTriggerTeleportCommand, "", nullptr},
        {"command", SEC_FULL_GM, true, &ChatHandler::HandleReloadCommandCommand,
         "", nullptr},
        {"conditions", SEC_FULL_GM, true,
         &ChatHandler::HandleReloadConditionsCommand, "", nullptr},
        {"creature_ai_scripts", SEC_FULL_GM, true,
         &ChatHandler::HandleReloadEventAIScriptsCommand, "", nullptr},
        {"creature_ai_summons", SEC_FULL_GM, true,
         &ChatHandler::HandleReloadEventAISummonsCommand, "", nullptr},
        {"creature_ai_texts", SEC_FULL_GM, true,
         &ChatHandler::HandleReloadEventAITextsCommand, "", nullptr},
        {"creature_battleground", SEC_FULL_GM, true,
         &ChatHandler::HandleReloadBattleEventCommand, "", nullptr},
        {"creature_involvedrelation", SEC_FULL_GM, true,
         &ChatHandler::HandleReloadCreatureQuestInvRelationsCommand, "",
         nullptr},
        {"creature_loot_template", SEC_FULL_GM, true,
         &ChatHandler::HandleReloadLootTemplatesCreatureCommand, "", nullptr},
        {"creature_questrelation", SEC_FULL_GM, true,
         &ChatHandler::HandleReloadCreatureQuestRelationsCommand, "", nullptr},
        {"db_script_string", SEC_FULL_GM, true,
         &ChatHandler::HandleReloadDbScriptStringCommand, "", nullptr},
        {"disenchant_loot_template", SEC_FULL_GM, true,
         &ChatHandler::HandleReloadLootTemplatesDisenchantCommand, "", nullptr},
        {"event_scripts", SEC_FULL_GM, true,
         &ChatHandler::HandleReloadEventScriptsCommand, "", nullptr},
        {"fishing_loot_template", SEC_FULL_GM, true,
         &ChatHandler::HandleReloadLootTemplatesFishingCommand, "", nullptr},
        {"game_graveyard_zone", SEC_FULL_GM, true,
         &ChatHandler::HandleReloadGameGraveyardZoneCommand, "", nullptr},
        {"game_tele", SEC_FULL_GM, true,
         &ChatHandler::HandleReloadGameTeleCommand, "", nullptr},
        {"gameobject_involvedrelation", SEC_FULL_GM, true,
         &ChatHandler::HandleReloadGOQuestInvRelationsCommand, "", nullptr},
        {"gameobject_loot_template", SEC_FULL_GM, true,
         &ChatHandler::HandleReloadLootTemplatesGameobjectCommand, "", nullptr},
        {"gameobject_questrelation", SEC_FULL_GM, true,
         &ChatHandler::HandleReloadGOQuestRelationsCommand, "", nullptr},
        {"gameobject_scripts", SEC_FULL_GM, true,
         &ChatHandler::HandleReloadGameObjectScriptsCommand, "", nullptr},
        {"gameobject_battleground", SEC_FULL_GM, true,
         &ChatHandler::HandleReloadBattleEventCommand, "", nullptr},
        {"gossip_menu", SEC_FULL_GM, true,
         &ChatHandler::HandleReloadGossipMenuCommand, "", nullptr},
        {"gossip_menu_option", SEC_FULL_GM, true,
         &ChatHandler::HandleReloadGossipMenuCommand, "", nullptr},
        {"gossip_scripts", SEC_FULL_GM, true,
         &ChatHandler::HandleReloadGossipScriptsCommand, "", nullptr},
        {"item_enchantment_template", SEC_FULL_GM, true,
         &ChatHandler::HandleReloadItemEnchantementsCommand, "", nullptr},
        {"item_loot_template", SEC_FULL_GM, true,
         &ChatHandler::HandleReloadLootTemplatesItemCommand, "", nullptr},
        {"item_required_target", SEC_FULL_GM, true,
         &ChatHandler::HandleReloadItemRequiredTragetCommand, "", nullptr},
        {"locales_creature", SEC_FULL_GM, true,
         &ChatHandler::HandleReloadLocalesCreatureCommand, "", nullptr},
        {"locales_gameobject", SEC_FULL_GM, true,
         &ChatHandler::HandleReloadLocalesGameobjectCommand, "", nullptr},
        {"locales_gossip_menu_option", SEC_FULL_GM, true,
         &ChatHandler::HandleReloadLocalesGossipMenuOptionCommand, "", nullptr},
        {"locales_item", SEC_FULL_GM, true,
         &ChatHandler::HandleReloadLocalesItemCommand, "", nullptr},
        {"locales_npc_text", SEC_FULL_GM, true,
         &ChatHandler::HandleReloadLocalesNpcTextCommand, "", nullptr},
        {"locales_page_text", SEC_FULL_GM, true,
         &ChatHandler::HandleReloadLocalesPageTextCommand, "", nullptr},
        {"locales_points_of_interest", SEC_FULL_GM, true,
         &ChatHandler::HandleReloadLocalesPointsOfInterestCommand, "", nullptr},
        {"locales_quest", SEC_FULL_GM, true,
         &ChatHandler::HandleReloadLocalesQuestCommand, "", nullptr},
        {"mail_level_reward", SEC_FULL_GM, true,
         &ChatHandler::HandleReloadMailLevelRewardCommand, "", nullptr},
        {"mail_loot_template", SEC_FULL_GM, true,
         &ChatHandler::HandleReloadLootTemplatesMailCommand, "", nullptr},
        {"mangos_string", SEC_FULL_GM, true,
         &ChatHandler::HandleReloadMangosStringCommand, "", nullptr},
        {"npc_gossip", SEC_FULL_GM, true,
         &ChatHandler::HandleReloadNpcGossipCommand, "", nullptr},
        {"npc_text", SEC_FULL_GM, true,
         &ChatHandler::HandleReloadNpcTextCommand, "", nullptr},
        {"npc_trainer", SEC_FULL_GM, true,
         &ChatHandler::HandleReloadNpcTrainerCommand, "", nullptr},
        {"npc_vendor", SEC_FULL_GM, true,
         &ChatHandler::HandleReloadNpcVendorCommand, "", nullptr},
        {"page_text", SEC_FULL_GM, true,
         &ChatHandler::HandleReloadPageTextsCommand, "", nullptr},
        {"pickpocketing_loot_template", SEC_FULL_GM, true,
         &ChatHandler::HandleReloadLootTemplatesPickpocketingCommand, "",
         nullptr},
        {"points_of_interest", SEC_FULL_GM, true,
         &ChatHandler::HandleReloadPointsOfInterestCommand, "", nullptr},
        {"prospecting_loot_template", SEC_FULL_GM, true,
         &ChatHandler::HandleReloadLootTemplatesProspectingCommand, "",
         nullptr},
        {"quest_end_scripts", SEC_FULL_GM, true,
         &ChatHandler::HandleReloadQuestEndScriptsCommand, "", nullptr},
        {"quest_start_scripts", SEC_FULL_GM, true,
         &ChatHandler::HandleReloadQuestStartScriptsCommand, "", nullptr},
        {"quest_template", SEC_FULL_GM, true,
         &ChatHandler::HandleReloadQuestTemplateCommand, "", nullptr},
        {"reference_loot_template", SEC_FULL_GM, true,
         &ChatHandler::HandleReloadLootTemplatesReferenceCommand, "", nullptr},
        {"reserved_name", SEC_FULL_GM, true,
         &ChatHandler::HandleReloadReservedNameCommand, "", nullptr},
        {"reputation_reward_rate", SEC_FULL_GM, true,
         &ChatHandler::HandleReloadReputationRewardRateCommand, "", nullptr},
        {"reputation_spillover_template", SEC_FULL_GM, true,
         &ChatHandler::HandleReloadReputationSpilloverTemplateCommand, "",
         nullptr},
        {"skill_discovery_template", SEC_FULL_GM, true,
         &ChatHandler::HandleReloadSkillDiscoveryTemplateCommand, "", nullptr},
        {"skill_extra_item_template", SEC_FULL_GM, true,
         &ChatHandler::HandleReloadSkillExtraItemTemplateCommand, "", nullptr},
        {"skill_fishing_base_level", SEC_FULL_GM, true,
         &ChatHandler::HandleReloadSkillFishingBaseLevelCommand, "", nullptr},
        {"skinning_loot_template", SEC_FULL_GM, true,
         &ChatHandler::HandleReloadLootTemplatesSkinningCommand, "", nullptr},
        {"spell_affect", SEC_FULL_GM, true,
         &ChatHandler::HandleReloadSpellAffectCommand, "", nullptr},
        {"spell_area", SEC_FULL_GM, true,
         &ChatHandler::HandleReloadSpellAreaCommand, "", nullptr},
        {"spell_bonus_data", SEC_FULL_GM, true,
         &ChatHandler::HandleReloadSpellBonusesCommand, "", nullptr},
        {"spell_chain", SEC_FULL_GM, true,
         &ChatHandler::HandleReloadSpellChainCommand, "", nullptr},
        {"spell_elixir", SEC_FULL_GM, true,
         &ChatHandler::HandleReloadSpellElixirCommand, "", nullptr},
        {"spell_learn_spell", SEC_FULL_GM, true,
         &ChatHandler::HandleReloadSpellLearnSpellCommand, "", nullptr},
        {"spell_pet_auras", SEC_FULL_GM, true,
         &ChatHandler::HandleReloadSpellPetAurasCommand, "", nullptr},
        {"spell_proc_event", SEC_FULL_GM, true,
         &ChatHandler::HandleReloadSpellProcEventCommand, "", nullptr},
        {"spell_proc_item_enchant", SEC_FULL_GM, true,
         &ChatHandler::HandleReloadSpellProcItemEnchantCommand, "", nullptr},
        {"spell_script_target", SEC_FULL_GM, true,
         &ChatHandler::HandleReloadSpellScriptTargetCommand, "", nullptr},
        {"spell_scripts", SEC_FULL_GM, true,
         &ChatHandler::HandleReloadSpellScriptsCommand, "", nullptr},
        {"spell_target_position", SEC_FULL_GM, true,
         &ChatHandler::HandleReloadSpellTargetPositionCommand, "", nullptr},
        {"spell_threats", SEC_FULL_GM, true,
         &ChatHandler::HandleReloadSpellThreatsCommand, "", nullptr},

        {nullptr, 0, false, nullptr, "", nullptr}};

    static ChatCommand resetCommandTable[] = {
        {"honor", SEC_FULL_GM, true, &ChatHandler::HandleResetHonorCommand, "",
         nullptr},
        {"level", SEC_FULL_GM, true, &ChatHandler::HandleResetLevelCommand, "",
         nullptr},
        {"spells", SEC_FULL_GM, true, &ChatHandler::HandleResetSpellsCommand,
         "", nullptr},
        {"stats", SEC_FULL_GM, true, &ChatHandler::HandleResetStatsCommand, "",
         nullptr},
        {"talents", SEC_FULL_GM, true, &ChatHandler::HandleResetTalentsCommand,
         "", nullptr},
        {"all", SEC_FULL_GM, true, &ChatHandler::HandleResetAllCommand, "",
         nullptr},
        {nullptr, 0, false, nullptr, "", nullptr}};

    static ChatCommand sendMassCommandTable[] = {
        {"items", SEC_FULL_GM, true, &ChatHandler::HandleSendMassItemsCommand,
         "", nullptr},
        {"mail", SEC_FULL_GM, true, &ChatHandler::HandleSendMassMailCommand, "",
         nullptr},
        {"money", SEC_FULL_GM, true, &ChatHandler::HandleSendMassMoneyCommand,
         "", nullptr},
        {nullptr, 0, false, nullptr, "", nullptr}};

    static ChatCommand sendCommandTable[] = {
        {"mass", SEC_FULL_GM, true, nullptr, "", sendMassCommandTable},

        {"items", SEC_FULL_GM, true, &ChatHandler::HandleSendItemsCommand, "",
         nullptr},
        {"mail", SEC_TICKET_GM, true, &ChatHandler::HandleSendMailCommand, "",
         nullptr},
        {"message", SEC_FULL_GM, true, &ChatHandler::HandleSendMessageCommand,
         "", nullptr},
        {"money", SEC_FULL_GM, true, &ChatHandler::HandleSendMoneyCommand, "",
         nullptr},
        {nullptr, 0, false, nullptr, "", nullptr}};

    static ChatCommand serverIdleRestartCommandTable[] = {
        {"cancel", SEC_FULL_GM, true,
         &ChatHandler::HandleServerShutDownCancelCommand, "", nullptr},
        {"", SEC_FULL_GM, true, &ChatHandler::HandleServerIdleRestartCommand,
         "", nullptr},
        {nullptr, 0, false, nullptr, "", nullptr}};

    static ChatCommand serverRestartCommandTable[] = {
        {"cancel", SEC_FULL_GM, true,
         &ChatHandler::HandleServerShutDownCancelCommand, "", nullptr},
        {"", SEC_FULL_GM, true, &ChatHandler::HandleServerRestartCommand, "",
         nullptr},
        {nullptr, 0, false, nullptr, "", nullptr}};

    static ChatCommand serverShutdownCommandTable[] = {
        {"cancel", SEC_FULL_GM, true,
         &ChatHandler::HandleServerShutDownCancelCommand, "", nullptr},
        {"", SEC_FULL_GM, true, &ChatHandler::HandleServerShutDownCommand, "",
         nullptr},
        {nullptr, 0, false, nullptr, "", nullptr}};

    static ChatCommand serverSetCommandTable[] = {
        {"motd", SEC_FULL_GM, true, &ChatHandler::HandleServerSetMotdCommand,
         "", nullptr},
        {nullptr, 0, false, nullptr, "", nullptr}};

    static ChatCommand serverCommandTable[] = {
        {"corpses", SEC_POWER_GM, true,
         &ChatHandler::HandleServerCorpsesCommand, "", nullptr},
        {"exit", SEC_CONSOLE, true, &ChatHandler::HandleServerExitCommand, "",
         nullptr},
        {"idlerestart", SEC_FULL_GM, true, nullptr, "",
         serverIdleRestartCommandTable},
        {"idleshutdown", SEC_FULL_GM, true, nullptr, "",
         serverShutdownCommandTable},
        {"info", SEC_PLAYER, true, &ChatHandler::HandleServerInfoCommand, "",
         nullptr},
        {"motd", SEC_PLAYER, true, &ChatHandler::HandleServerMotdCommand, "",
         nullptr},
        {"plimit", SEC_FULL_GM, true, &ChatHandler::HandleServerPLimitCommand,
         "", nullptr},
        {"restart", SEC_FULL_GM, true, nullptr, "", serverRestartCommandTable},
        {"shutdown", SEC_FULL_GM, true, nullptr, "",
         serverShutdownCommandTable},
        {"set", SEC_FULL_GM, true, nullptr, "", serverSetCommandTable},
        {nullptr, 0, false, nullptr, "", nullptr}};

    static ChatCommand smartAICommandTable[] = {
        {"addwp", SEC_FULL_GM, false, &ChatHandler::HandleSmartAIAddWp, "",
         nullptr},
        {"addgwp", SEC_FULL_GM, false, &ChatHandler::HandleSmartAIAddGwp, "",
         nullptr},
        {nullptr, 0, false, nullptr, "", nullptr}};

    static ChatCommand ticketCommandTable[] = {
        {"checkin", SEC_TICKET_GM, false, &ChatHandler::HandleTicketCheckin, "",
         nullptr},
        {"checkout", SEC_TICKET_GM, false, &ChatHandler::HandleTicketCheckout,
         "", nullptr},
        {"comment", SEC_TICKET_GM, false, &ChatHandler::HandleTicketComment, "",
         nullptr},
        {"pnote", SEC_TICKET_GM, false, &ChatHandler::HandleTicketPnote, "",
         nullptr},
        {"pong", SEC_TICKET_GM, false, &ChatHandler::HandleTicketPong, "",
         nullptr},
        {"resolve", SEC_TICKET_GM, false, &ChatHandler::HandleTicketResolve, "",
         nullptr},
        {"say", SEC_TICKET_GM, false, &ChatHandler::HandleTicketSay, "",
         nullptr},
        {nullptr, 0, false, nullptr, "", nullptr}};

    static ChatCommand titlesCommandTable[] = {
        {"add", SEC_POWER_GM, false, &ChatHandler::HandleTitlesAddCommand, "",
         nullptr},
        {"current", SEC_POWER_GM, false,
         &ChatHandler::HandleTitlesCurrentCommand, "", nullptr},
        {"remove", SEC_POWER_GM, false, &ChatHandler::HandleTitlesRemoveCommand,
         "", nullptr},
        {"setmask", SEC_POWER_GM, false,
         &ChatHandler::HandleTitlesSetMaskCommand, "", nullptr},
        {nullptr, 0, false, nullptr, "", nullptr}};

    static ChatCommand tplocCommandTable[] = {
        {"add", SEC_FULL_GM, false, &ChatHandler::HandleTplocAddCommand, "",
         nullptr},
        {"del", SEC_FULL_GM, false, &ChatHandler::HandleTplocDelCommand, "",
         nullptr},
        {nullptr, 0, false, nullptr, "", nullptr}};

    static ChatCommand triggerCommandTable[] = {
        {"active", SEC_POWER_GM, false,
         &ChatHandler::HandleTriggerActiveCommand, "", nullptr},
        {"near", SEC_POWER_GM, false, &ChatHandler::HandleTriggerNearCommand,
         "", nullptr},
        {"", SEC_POWER_GM, true, &ChatHandler::HandleTriggerCommand, "",
         nullptr},
        {nullptr, 0, false, nullptr, "", nullptr}};

    static ChatCommand unbanCommandTable[] = {
        {"account", SEC_FULL_GM, true, &ChatHandler::HandleUnBanAccountCommand,
         "", nullptr},
        {"character", SEC_FULL_GM, true,
         &ChatHandler::HandleUnBanCharacterCommand, "", nullptr},
        {"ip", SEC_FULL_GM, true, &ChatHandler::HandleUnBanIPCommand, "",
         nullptr},
        {nullptr, 0, false, nullptr, "", nullptr}};

    static ChatCommand wpCommandTable[] = {
        {"show", SEC_POWER_GM, false, &ChatHandler::HandleWpShowCommand, "",
         nullptr},
        {"add", SEC_POWER_GM, false, &ChatHandler::HandleWpAddCommand, "",
         nullptr},
        {"modify", SEC_POWER_GM, false, &ChatHandler::HandleWpModifyCommand, "",
         nullptr},
        {"export", SEC_FULL_GM, false, &ChatHandler::HandleWpExportCommand, "",
         nullptr},
        {"import", SEC_FULL_GM, false, &ChatHandler::HandleWpImportCommand, "",
         nullptr},
        {nullptr, 0, false, nullptr, "", nullptr}};

    static ChatCommand commandTable[] = {
        {"account", SEC_PLAYER, true, nullptr, "", accountCommandTable},
        {"arena", SEC_PLAYER, true, nullptr, "", arenaCommandTable},
        {"auction", SEC_FULL_GM, false, nullptr, "", auctionCommandTable},
        {"ban", SEC_FULL_GM, true, nullptr, "", banCommandTable},
        {"baninfo", SEC_FULL_GM, false, nullptr, "", baninfoCommandTable},
        {"banlist", SEC_FULL_GM, true, nullptr, "", banlistCommandTable},
        {"banwave", SEC_FULL_GM, false, nullptr, "", banWaveCommandTable},
        {"cast", SEC_FULL_GM, false, nullptr, "", castCommandTable},
        {"character", SEC_POWER_GM, true, nullptr, "", characterCommandTable},
        {"debug", SEC_TICKET_GM, true, nullptr, "", debugCommandTable},
        {"event", SEC_POWER_GM, false, nullptr, "", eventCommandTable},
        {"gm", SEC_PLAYER, true, nullptr, "", gmCommandTable},
        {"honor", SEC_POWER_GM, false, nullptr, "", honorCommandTable},
        {"gobject", SEC_POWER_GM, false, nullptr, "", gobjectCommandTable},
        {"guild", SEC_POWER_GM, true, nullptr, "", guildCommandTable},
        {"instance", SEC_FULL_GM, true, nullptr, "", instanceCommandTable},
        {"learn", SEC_TICKET_GM, false, nullptr, "", learnCommandTable},
        {"list", SEC_FULL_GM, true, nullptr, "", listCommandTable},
        {"lookup", SEC_TICKET_GM, true, nullptr, "", lookupCommandTable},
        {"modify", SEC_TICKET_GM, false, nullptr, "", modifyCommandTable},
        {"mmap", SEC_POWER_GM, false, nullptr, "", mmapCommandTable},
        {"npc", SEC_TICKET_GM, false, nullptr, "", npcCommandTable},
        {"pool", SEC_POWER_GM, true, nullptr, "", poolCommandTable},
        {"quest", SEC_FULL_GM, false, nullptr, "", questCommandTable},
        {"reload", SEC_FULL_GM, true, nullptr, "", reloadCommandTable},
        {"reset", SEC_FULL_GM, true, nullptr, "", resetCommandTable},
        {"send", SEC_TICKET_GM, true, nullptr, "", sendCommandTable},
        {"server", SEC_PLAYER, true, nullptr, "", serverCommandTable},
        {"smartai", SEC_FULL_GM, false, nullptr, "", smartAICommandTable},
        {"ticket", SEC_TICKET_GM, false, nullptr, "", ticketCommandTable},
        {"titles", SEC_POWER_GM, false, nullptr, "", titlesCommandTable},
        {"tploc", SEC_FULL_GM, false, nullptr, "", tplocCommandTable},
        {"trigger", SEC_POWER_GM, false, nullptr, "", triggerCommandTable},
        {"unban", SEC_FULL_GM, true, nullptr, "", unbanCommandTable},
        {"wp", SEC_POWER_GM, false, nullptr, "", wpCommandTable},

        {"additem", SEC_FULL_GM, false, &ChatHandler::HandleAddItemCommand, "",
         nullptr},
        {"additemset", SEC_FULL_GM, false,
         &ChatHandler::HandleAddItemSetCommand, "", nullptr},
        {"announce", SEC_TICKET_GM, true, &ChatHandler::HandleAnnounceCommand,
         "", nullptr},
        {"aura", SEC_FULL_GM, false, &ChatHandler::HandleAuraCommand, "",
         nullptr},
        {"bank", SEC_FULL_GM, false, &ChatHandler::HandleBankCommand, "",
         nullptr},
        {"summon", SEC_POWER_GM, false, &ChatHandler::HandleSummonCommand, "",
         nullptr},
        {"combatstop", SEC_POWER_GM, false,
         &ChatHandler::HandleCombatStopCommand, "", nullptr},
        {"cometome", SEC_FULL_GM, false, &ChatHandler::HandleComeToMeCommand,
         "", nullptr},
        {"commands", SEC_PLAYER, true, &ChatHandler::HandleCommandsCommand, "",
         nullptr},
        {"cooldown", SEC_FULL_GM, false, &ChatHandler::HandleCooldownCommand,
         "", nullptr},
        {"damage", SEC_FULL_GM, false, &ChatHandler::HandleDamageCommand, "",
         nullptr},
        {"defmode", SEC_FULL_GM, true, &ChatHandler::HandleDefmodeCommand, "",
         nullptr},
        {"demorph", SEC_POWER_GM, false, &ChatHandler::HandleDeMorphCommand, "",
         nullptr},
        {"die", SEC_FULL_GM, false, &ChatHandler::HandleDieCommand, "",
         nullptr},
        {"dismount", SEC_PLAYER, false, &ChatHandler::HandleDismountCommand, "",
         nullptr},
        {"distance", SEC_FULL_GM, false, &ChatHandler::HandleGetDistanceCommand,
         "", nullptr},
        {"explorecheat", SEC_FULL_GM, false,
         &ChatHandler::HandleExploreCheatCommand, "", nullptr},
        {"flusharenapoints", SEC_FULL_GM, false,
         &ChatHandler::HandleFlushArenaPointsCommand, "", nullptr},
        {"gps", SEC_TICKET_GM, false, &ChatHandler::HandleGPSCommand, "",
         nullptr},
        {"guid", SEC_POWER_GM, false, &ChatHandler::HandleGUIDCommand, "",
         nullptr},
        {"help", SEC_PLAYER, true, &ChatHandler::HandleHelpCommand, "",
         nullptr},
        {"hidearea", SEC_FULL_GM, false, &ChatHandler::HandleHideAreaCommand,
         "", nullptr},
        {"kick", SEC_POWER_GM, true, &ChatHandler::HandleKickPlayerCommand, "",
         nullptr},
        {"levelup", SEC_FULL_GM, false, &ChatHandler::HandleLevelUpCommand, "",
         nullptr},
        {"linkgrave", SEC_FULL_GM, false, &ChatHandler::HandleLinkGraveCommand,
         "", nullptr},
        {"loadscripts", SEC_FULL_GM, true,
         &ChatHandler::HandleLoadScriptsCommand, "", nullptr},
        {"maxskill", SEC_FULL_GM, false, &ChatHandler::HandleMaxSkillCommand,
         "", nullptr},
        {"maxprof", SEC_FULL_GM, false, &ChatHandler::HandleMaxProfCommand, "",
         nullptr},
        {"movegens", SEC_FULL_GM, false, &ChatHandler::HandleMovegensCommand,
         "", nullptr},
        {"splinemake", SEC_FULL_GM, false,
         &ChatHandler::HandleSplinemakeCommand, "", nullptr},
        {"splineplay", SEC_FULL_GM, false,
         &ChatHandler::HandleSplineplayCommand, "", nullptr},
        {"mute", SEC_TICKET_GM, true, &ChatHandler::HandleMuteCommand, "",
         nullptr},
        {"mobconvert", SEC_TICKET_GM, true,
         &ChatHandler::HandleMobConvertCommand, "", nullptr},
        {"neargrave", SEC_FULL_GM, false, &ChatHandler::HandleNearGraveCommand,
         "", nullptr},
        {"notify", SEC_TICKET_GM, true, &ChatHandler::HandleNotifyCommand, "",
         nullptr},
        {"pet", SEC_POWER_GM, false, &ChatHandler::HandlePetCommand, "",
         nullptr},
        {"pinfo", SEC_POWER_GM, true, &ChatHandler::HandlePInfoCommand, "",
         nullptr},
        {"quit", SEC_CONSOLE, true, &ChatHandler::HandleQuitCommand, "",
         nullptr},
        {"repairitems", SEC_POWER_GM, true,
         &ChatHandler::HandleRepairitemsCommand, "", nullptr},
        {"respawn", SEC_FULL_GM, false, &ChatHandler::HandleRespawnCommand, "",
         nullptr},
        {"revive", SEC_FULL_GM, true, &ChatHandler::HandleReviveCommand, "",
         nullptr},
        {"save", SEC_TICKET_GM, false, &ChatHandler::HandleSaveCommand, "",
         nullptr},
        {"saveall", SEC_TICKET_GM, true, &ChatHandler::HandleSaveAllCommand, "",
         nullptr},
        {"setskill", SEC_FULL_GM, false, &ChatHandler::HandleSetSkillCommand,
         "", nullptr},
        {"showarea", SEC_FULL_GM, false, &ChatHandler::HandleShowAreaCommand,
         "", nullptr},
        {"spy", SEC_FULL_GM, false, &ChatHandler::HandleSpyCommand, "",
         nullptr},
        {"stable", SEC_FULL_GM, false, &ChatHandler::HandleStableCommand, "",
         nullptr},
        {"start", SEC_PLAYER, false, &ChatHandler::HandleStartCommand, "",
         nullptr},
        {"taxicheat", SEC_TICKET_GM, false,
         &ChatHandler::HandleTaxiCheatCommand, "", nullptr},
        {"tempsummon", SEC_TICKET_GM, false,
         &ChatHandler::HandleTempSummonCommand, "", nullptr},
        {"tp", SEC_POWER_GM, false, &ChatHandler::HandleTpCommand, "", nullptr},
        {"unaura", SEC_FULL_GM, false, &ChatHandler::HandleUnAuraCommand, "",
         nullptr},
        {"unlearn", SEC_FULL_GM, false, &ChatHandler::HandleUnLearnCommand, "",
         nullptr},
        {"unmute", SEC_TICKET_GM, true, &ChatHandler::HandleUnmuteCommand, "",
         nullptr},
        {"waterwalk", SEC_POWER_GM, false, &ChatHandler::HandleWaterwalkCommand,
         "", nullptr},
        {"wchange", SEC_FULL_GM, false,
         &ChatHandler::HandleChangeWeatherCommand, "", nullptr},
        {nullptr, 0, false, nullptr, "", nullptr}};

    if (load_command_table)
    {
        load_command_table = false;

        // check hardcoded part integrity
        CheckIntegrity(commandTable, nullptr);

        QueryResult* result =
            WorldDatabase.Query("SELECT name,security,help FROM command");
        if (result)
        {
            do
            {
                Field* fields = result->Fetch();
                std::string name = fields[0].GetCppString();

                SetDataForCommandInTable(commandTable, name.c_str(),
                    fields[1].GetUInt16(), fields[2].GetCppString());

            } while (result->NextRow());
            delete result;
        }
    }

    return commandTable;
}

ChatHandler::ChatHandler(WorldSession* session) : m_session(session)
{
}

ChatHandler::ChatHandler(Player* player) : m_session(player->GetSession())
{
}

ChatHandler::~ChatHandler()
{
}

const char* ChatHandler::GetMangosString(int32 entry) const
{
    return m_session->GetMangosString(entry);
}

const char* ChatHandler::GetOnOffStr(bool value) const
{
    return value ? GetMangosString(LANG_ON) : GetMangosString(LANG_OFF);
}

uint32 ChatHandler::GetAccountId() const
{
    return m_session->GetAccountId();
}

AccountTypes ChatHandler::GetAccessLevel() const
{
    return m_session->GetSecurity();
}

bool ChatHandler::isAvailable(ChatCommand const& cmd) const
{
    // check security level only for simple  command (without child commands)
    return GetAccessLevel() >= (AccountTypes)cmd.SecurityLevel;
}

std::string ChatHandler::GetNameLink() const
{
    return GetNameLink(m_session->GetPlayer());
}

bool ChatHandler::HasLowerSecurity(Player* target, ObjectGuid guid, bool strong)
{
    WorldSession* target_session = nullptr;
    uint32 target_account = 0;

    if (target)
        target_session = target->GetSession();
    else
        target_account = sObjectMgr::Instance()->GetPlayerAccountIdByGUID(guid);

    if (!target_session && !target_account)
    {
        SendSysMessage(LANG_PLAYER_NOT_FOUND);
        SetSentErrorMessage(true);
        return true;
    }

    return HasLowerSecurityAccount(target_session, target_account, strong);
}

bool ChatHandler::HasLowerSecurityAccount(
    WorldSession* target, uint32 target_account, bool strong)
{
    AccountTypes target_sec;

    // ignore only for non-players for non strong checks (when allow apply
    // command at least to same sec level)
    if (GetAccessLevel() > SEC_PLAYER && !strong &&
        !sWorld::Instance()->getConfig(CONFIG_BOOL_GM_LOWER_SECURITY))
        return false;

    if (target)
        target_sec = target->GetSecurity();
    else if (target_account)
        target_sec = sAccountMgr::Instance()->GetSecurity(target_account);
    else
        return true; // caller must report error for (target==NULL &&
                     // target_account==0)

    if (GetAccessLevel() < target_sec ||
        (strong && GetAccessLevel() <= target_sec))
    {
        SendSysMessage(LANG_YOURS_SECURITY_IS_LOW);
        SetSentErrorMessage(true);
        return true;
    }

    return false;
}

bool ChatHandler::hasStringAbbr(const char* name, const char* part)
{
    // non "" command
    if (*name)
    {
        // "" part from non-"" command
        if (!*part)
            return false;

        for (;;)
        {
            if (!*part)
                return true;
            else if (!*name)
                return false;
            else if (tolower(*name) != tolower(*part))
                return false;
            ++name;
            ++part;
        }
    }
    // allow with any for ""

    return true;
}

void ChatHandler::SendSysMessage(const char* str)
{
    // need copy to prevent corruption by strtok call in LineFromMessage
    // original string
    char* buf = mangos_strdup(str);
    char* pos = buf;

    while (char* line = LineFromMessage(pos))
    {
        WorldPacket data;
        FillSystemMessageData(&data, line);
        m_session->send_packet(std::move(data));
    }

    delete[] buf;
}

void ChatHandler::SendGlobalSysMessage(const char* str)
{
    // Chat output
    WorldPacket data;

    // need copy to prevent corruption by strtok call in LineFromMessage
    // original string
    char* buf = mangos_strdup(str);
    char* pos = buf;

    while (char* line = LineFromMessage(pos))
    {
        FillSystemMessageData(&data, line);
        sWorld::Instance()->SendGlobalMessage(&data);
    }

    delete[] buf;
}

void ChatHandler::SendSysMessage(int32 entry)
{
    SendSysMessage(GetMangosString(entry));
}

void ChatHandler::PSendSysMessage(int32 entry, ...)
{
    const char* format = GetMangosString(entry);
    va_list ap;
    char str[2048];
    va_start(ap, entry);
    vsnprintf(str, 2048, format, ap);
    va_end(ap);
    SendSysMessage(str);
}

void ChatHandler::PSendSysMessage(const char* format, ...)
{
    va_list ap;
    char str[2048];
    va_start(ap, format);
    vsnprintf(str, 2048, format, ap);
    va_end(ap);
    SendSysMessage(str);
}

void ChatHandler::CheckIntegrity(ChatCommand* table, ChatCommand* parentCommand)
{
    for (uint32 i = 0; table[i].Name != nullptr; ++i)
    {
        ChatCommand* command = &table[i];

        if (parentCommand &&
            command->SecurityLevel < parentCommand->SecurityLevel)
            logging.error(
                "Subcommand '%s' of command '%s' have less access level (%u) "
                "that parent (%u)",
                command->Name, parentCommand->Name, command->SecurityLevel,
                parentCommand->SecurityLevel);

        if (!parentCommand && strlen(command->Name) == 0)
            logging.error("Subcommand '' at top level");

        if (command->ChildCommands)
        {
            if (command->Handler)
            {
                if (parentCommand)
                    logging.error(
                        "Subcommand '%s' of command '%s' have handler and "
                        "subcommands in same time, must be used '' subcommand "
                        "for handler instead.",
                        command->Name, parentCommand->Name);
                else
                    logging.error(
                        "First level command '%s' have handler and subcommands "
                        "in same time, must be used '' subcommand for handler "
                        "instead.",
                        command->Name);
            }

            if (parentCommand && strlen(command->Name) == 0)
                logging.error("Subcommand '' of command '%s' have subcommands",
                    parentCommand->Name);

            CheckIntegrity(command->ChildCommands, command);
        }
        else if (!command->Handler)
        {
            if (parentCommand)
                logging.error(
                    "Subcommand '%s' of command '%s' not have handler and "
                    "subcommands in same time. Must have some from its!",
                    command->Name, parentCommand->Name);
            else
                logging.error(
                    "First level command '%s' not have handler and subcommands "
                    "in same time. Must have some from its!",
                    command->Name);
        }
    }
}

/**
 * Search (sub)command for command line available for chat handler access level
 *
 * @param text  Command line string that will parsed for (sub)command search
 *
 * @return Pointer to found command structure or NULL if appropriate command not
 *found
 */
ChatCommand const* ChatHandler::FindCommand(char const* text)
{
    ChatCommand* command = nullptr;
    char const* textPtr = text;

    return FindCommand(getCommandTable(), textPtr, command) == CHAT_COMMAND_OK ?
               command :
               nullptr;
}

/**
 * Search (sub)command for command line available for chat handler access level
 *with options and fail case additional info
 *
 * @param table         Pointer to command C-style array first level command
 *where will be searched
 * @param text          Command line string that will parsed for (sub)command
 *search,
 *                      it modified at return from function and pointed to not
 *parsed tail
 * @param command       At success this is found command, at other cases this is
 *last found parent command
 *                      before subcommand search fail
 * @param parentCommand Output arg for optional return parent command for
 *command arg.
 * @param cmdNamePtr    Output arg for optional return last parsed command name.
 * @param allAvailable  Optional arg (with false default value) control use
 *command access level checks while command search.
 * @param exactlyName   Optional arg (with false default value) control use
 *exactly name in checks while command search.
 *
 * @return one from enum value of ChatCommandSearchResult. Output args return
 *values highly dependent from this return result:
 *
 *      CHAT_COMMAND_OK       - Command found!
 *                              text point to non parsed tail with possible
 *command specific data, command store found command pointer,
 *                              parentCommand have parent of found command or
 *NULL if command found in table array directly
 *                              cmdNamePtr store found command name in original
 *form from command line
 *      CHAT_COMMAND_UNKNOWN  - Command not found in table directly
 *                              text only skip possible whitespaces,
 *                              command is NULL
 *                              parentCommand is NULL
 *                              cmdNamePtr store command name that not found as
 *it extracted from command line
 *      CHAT_COMMAND_UNKNOWN_SUBCOMMAND - Subcommand not found in some deed
 *subcomand lists
 *                              text point to non parsed tail including not
 *found command name in command line,
 *                              command store last found parent command if any
 *                              parentCommand have parent of command in command
 *arg or NULL
 *                              cmdNamePtr store command name that not found as
 *it extracted from command line
 */
ChatCommandSearchResult ChatHandler::FindCommand(ChatCommand* table,
    char const*& text, ChatCommand*& command,
    ChatCommand** parentCommand /*= NULL*/, std::string* cmdNamePtr /*= NULL*/,
    bool allAvailable /*= false*/, bool exactlyName /*= false*/)
{
    std::string cmd = "";

    // skip whitespaces
    while (*text != ' ' && *text != '\0')
    {
        cmd += *text;
        ++text;
    }

    while (*text == ' ')
        ++text;

    // If any exact match exists in the command table, we match the first
    // command like exactlyName was true rather than accepting abbreviations
    bool fake_exactly = false;
    if (!exactlyName)
    {
        for (uint32 i = 0; table[i].Name != nullptr; ++i)
        {
            size_t len = strlen(table[i].Name);
            if (strncmp(table[i].Name, cmd.c_str(), len + 1) == 0)
            {
                fake_exactly = true;
                break;
            }
        }
    }

    // search first level command in table
    for (uint32 i = 0; table[i].Name != nullptr; ++i)
    {
        if (exactlyName || fake_exactly)
        {
            size_t len = strlen(table[i].Name);
            if (strncmp(table[i].Name, cmd.c_str(), len + 1) != 0)
                continue;
        }
        else
        {
            if (!hasStringAbbr(table[i].Name, cmd.c_str()))
                continue;
        }
        // select subcommand from child commands list
        if (table[i].ChildCommands != nullptr)
        {
            char const* oldchildtext = text;
            ChatCommand* parentSubcommand = nullptr;
            ChatCommandSearchResult res =
                FindCommand(table[i].ChildCommands, text, command,
                    &parentSubcommand, cmdNamePtr, allAvailable, exactlyName);

            switch (res)
            {
            case CHAT_COMMAND_OK:
            {
                // if subcommand success search not return parent command, then
                // this parent command is owner of child commands
                if (parentCommand)
                    *parentCommand =
                        parentSubcommand ? parentSubcommand : &table[i];

                // Name == "" is special case: restore original command text for
                // next level "" (where parentSubcommand==NULL)
                if (strlen(command->Name) == 0 && !parentSubcommand)
                    text = oldchildtext;

                return CHAT_COMMAND_OK;
            }
            case CHAT_COMMAND_UNKNOWN:
            {
                // command not found directly in child command list, return
                // child command list owner
                command = &table[i];
                if (parentCommand)
                    *parentCommand = nullptr; // we don't known parent of table
                                              // list at this point

                text = oldchildtext; // restore text to stated just after parse
                                     // found parent command
                return CHAT_COMMAND_UNKNOWN_SUBCOMMAND; // we not found
                                                        // subcommand for
                                                        // table[i]
            }
            case CHAT_COMMAND_UNKNOWN_SUBCOMMAND:
            default:
            {
                // some deep subcommand not found, if this second level
                // subcommand then parentCommand can be NULL, use known value
                // for it
                if (parentCommand)
                    *parentCommand =
                        parentSubcommand ? parentSubcommand : &table[i];
                return res;
            }
            }
        }

        // must be available (not checked for subcommands case because parent
        // command expected have most low access that all subcommands always
        if (!allAvailable && !isAvailable(table[i]))
            continue;

        // must be have handler is explicitly selected
        if (!table[i].Handler)
            continue;

        // command found directly in to table
        command = &table[i];

        // unknown table owner at this point
        if (parentCommand)
            *parentCommand = nullptr;

        if (cmdNamePtr)
            *cmdNamePtr = cmd;

        return CHAT_COMMAND_OK;
    }

    // command not found in table directly
    command = nullptr;

    // unknown table owner at this point
    if (parentCommand)
        *parentCommand = nullptr;

    if (cmdNamePtr)
        *cmdNamePtr = cmd;

    return CHAT_COMMAND_UNKNOWN;
}

/**
 * Execute (sub)command available for chat handler access level with options in
 *command line string
 *
 * @param text  Command line string that will parsed for (sub)command search and
 *command specific data
 *
 * Command output and errors in command execution will send to chat handler.
 */
void ChatHandler::ExecuteCommand(const char* text)
{
    std::string fullcmd = text; // original `text` can't be used. It content
                                // destroyed in command code processing.

    ChatCommand* command = nullptr;
    ChatCommand* parentCommand = nullptr;

    ChatCommandSearchResult res =
        FindCommand(getCommandTable(), text, command, &parentCommand);

    switch (res)
    {
    case CHAT_COMMAND_OK:
    {
        SetSentErrorMessage(false);
        if ((this->*(command->Handler))(
                (char*)text)) // text content destroyed at call
        {
            if (command->SecurityLevel > SEC_PLAYER)
                LogCommand(fullcmd.c_str());
        }
        // some commands have custom error messages. Don't send the default one
        // in these cases.
        else if (!HasSentErrorMessage())
        {
            if (!command->Help.empty())
                SendSysMessage(command->Help.c_str());
            else
                SendSysMessage(LANG_CMD_SYNTAX);

            if (ChatCommand* showCommand =
                    (strlen(command->Name) == 0 && parentCommand ?
                            parentCommand :
                            command))
                if (ChatCommand* childs = showCommand->ChildCommands)
                    ShowHelpForSubCommands(childs, showCommand->Name);

            SetSentErrorMessage(true);
        }
        break;
    }
    case CHAT_COMMAND_UNKNOWN_SUBCOMMAND:
    {
        SendSysMessage(LANG_NO_SUBCMD);
        ShowHelpForCommand(command->ChildCommands, text);
        SetSentErrorMessage(true);
        break;
    }
    case CHAT_COMMAND_UNKNOWN:
    {
        SendSysMessage(LANG_NO_CMD);
        SetSentErrorMessage(true);
        break;
    }
    }
}

/**
 * Function find appropriate command and update command security level and help
 *text
 *
 * @param commandTable  Table for first level command search
 * @param text          Command line string that will parsed for (sub)command
 *search
 * @param security      New security level for command
 * @param help          New help text  for command
 *
 * @return true if command has been found, and false in other case
 *
 * All problems found while command search and updated output as to DB errors
 *log
 */
bool ChatHandler::SetDataForCommandInTable(ChatCommand* commandTable,
    const char* text, uint32 security, std::string const& help)
{
    std::string fullcommand = text; // original `text` can't be used. It content
                                    // destroyed in command code processing.

    ChatCommand* command = nullptr;
    std::string cmdName;

    ChatCommandSearchResult res =
        FindCommand(commandTable, text, command, nullptr, &cmdName, true, true);

    switch (res)
    {
    case CHAT_COMMAND_OK:
    {
#ifndef OPTIMIZED_BUILD
        if (command->SecurityLevel != security)
            LOG_DEBUG(logging,
                "Table `command` overwrite for command '%s' default security "
                "(%u) by %u",
                fullcommand.c_str(), command->SecurityLevel, security);
#endif

        command->SecurityLevel = security;
        command->Help = help;
        return true;
    }
    case CHAT_COMMAND_UNKNOWN_SUBCOMMAND:
    {
        // command have subcommands, but not '' subcommand and then any data in
        // `command` useless for it.
        if (cmdName.empty())
            logging.error(
                "Table `command` have command '%s' that only used with some "
                "subcommand selection, it can't have help or overwritten "
                "access level, skip.",
                cmdName.c_str());
        else
            logging.error(
                "Table `command` have unexpected subcommand '%s' in command "
                "'%s', skip.",
                cmdName.c_str(), fullcommand.c_str());
        return false;
    }
    case CHAT_COMMAND_UNKNOWN:
    {
        logging.error("Table `command` have nonexistent command '%s', skip.",
            cmdName.c_str());
        return false;
    }
    }

    return false;
}

bool ChatHandler::ParseCommands(const char* text)
{
    assert(text);
    assert(*text);

    // if(m_session->GetSecurity() == SEC_PLAYER)
    //    return false;

    /// chat case (.command or !command format)
    if (m_session)
    {
        if (text[0] != '!' && text[0] != '.')
            return false;

        /// ignore single . and ! in line
        if (strlen(text) < 2)
            return false;
    }

    /// ignore messages staring from many dots.
    if ((text[0] == '.' && text[1] == '.') ||
        (text[0] == '!' && text[1] == '!'))
        return false;

    /// skip first . or ! (in console allowed use command with . and ! and
    /// without its)
    if (text[0] == '!' || text[0] == '.')
        ++text;

    ExecuteCommand(text);

    return true;
}

bool ChatHandler::ShowHelpForSubCommands(ChatCommand* table, char const* cmd)
{
    std::string list;
    for (uint32 i = 0; table[i].Name != nullptr; ++i)
    {
        // must be available (ignore handler existence for show command with
        // possible available subcommands
        if (!isAvailable(table[i]))
            continue;

        if (m_session)
            list += "\n    ";
        else
            list += "\n\r    ";

        list += table[i].Name;

        if (table[i].ChildCommands)
            list += " ...";
    }

    if (list.empty())
        return false;

    if (table == getCommandTable())
    {
        SendSysMessage(LANG_AVIABLE_CMD);
        SendSysMessage(list.c_str());
    }
    else
    {
        PSendSysMessage(LANG_SUBCMDS_LIST, cmd);
        SendSysMessage(list.c_str());
    }
    return true;
}

bool ChatHandler::ShowHelpForCommand(ChatCommand* table, const char* cmd)
{
    char const* oldCmd = cmd;
    ChatCommand* command = nullptr;
    ChatCommand* parentCommand = nullptr;

    ChatCommand* showCommand = nullptr;
    ChatCommand* childCommands = nullptr;

    ChatCommandSearchResult res =
        FindCommand(table, cmd, command, &parentCommand);

    switch (res)
    {
    case CHAT_COMMAND_OK:
    {
        // for "" subcommand use parent command if any for subcommands list
        // output
        if (strlen(command->Name) == 0 && parentCommand)
        {
            showCommand = parentCommand;
            cmd = "";
        }
        else
            showCommand = command;

        childCommands = showCommand->ChildCommands;
        break;
    }
    case CHAT_COMMAND_UNKNOWN_SUBCOMMAND:
        showCommand = command;
        childCommands = showCommand->ChildCommands;
        break;
    case CHAT_COMMAND_UNKNOWN:
        // not show command list at error in first level command find fail
        childCommands =
            table != getCommandTable() || strlen(oldCmd) == 0 ? table : nullptr;
        command = nullptr;
        break;
    }

    if (command && !command->Help.empty())
        SendSysMessage(command->Help.c_str());

    if (childCommands)
        if (ShowHelpForSubCommands(
                childCommands, showCommand ? showCommand->Name : ""))
            return true;

    if (command && command->Help.empty())
        SendSysMessage(LANG_NO_HELP_CMD);

    return command || childCommands;
}

bool ChatHandler::isValidChatMessage(const char* message)
{
    /*

    valid examples:
    |cffa335ee|Hitem:812:0:0:0:0:0:0:0:70|h[Glowing Brightwood Staff]|h|r
    |cff808080|Hquest:2278:47|h[The Platinum Discs]|h|r
    |cff4e96f7|Htalent:2232:-1|h[Taste for Blood]|h|r
    |cff71d5ff|Hspell:21563|h[Command]|h|r
    |cffffd000|Henchant:3919|h[Engineering: Rough Dynamite]|h|r

    | will be escaped to ||
    */

    if (strlen(message) > 255)
        return false;

    const char validSequence[6] = "cHhhr";
    const char* validSequenceIterator = validSequence;

    // more simple checks
    if (sWorld::Instance()->getConfig(
            CONFIG_UINT32_CHAT_STRICT_LINK_CHECKING_SEVERITY) < 3)
    {
        const std::string validCommands = "cHhr|";

        while (*message)
        {
            // find next pipe command
            message = strchr(message, '|');

            if (!message)
                return true;

            ++message;
            char commandChar = *message;
            if (validCommands.find(commandChar) == std::string::npos)
                return false;

            ++message;
            // validate sequence
            if (sWorld::Instance()->getConfig(
                    CONFIG_UINT32_CHAT_STRICT_LINK_CHECKING_SEVERITY) == 2)
            {
                if (commandChar == *validSequenceIterator)
                {
                    if (validSequenceIterator == validSequence + 4)
                        validSequenceIterator = validSequence;
                    else
                        ++validSequenceIterator;
                }
                else if (commandChar != '|')
                    return false;
            }
        }
        return true;
    }

    std::istringstream reader(message);
    char buffer[256];

    uint32 color = 0;

    ItemPrototype const* linkedItem = nullptr;
    Quest const* linkedQuest = nullptr;
    SpellEntry const* linkedSpell = nullptr;
    ItemRandomPropertiesEntry const* itemProperty = nullptr;
    ItemRandomSuffixEntry const* itemSuffix = nullptr;

    while (!reader.eof())
    {
        if (validSequence == validSequenceIterator)
        {
            linkedItem = nullptr;
            linkedQuest = nullptr;
            linkedSpell = nullptr;
            itemProperty = nullptr;
            itemSuffix = nullptr;

            reader.ignore(255, '|');
        }
        else if (reader.get() != '|')
        {
            LOG_DEBUG(logging,
                "ChatHandler::isValidChatMessage sequence aborted "
                "unexpectedly");
            return false;
        }

        // pipe has always to be followed by at least one char
        if (reader.peek() == '\0')
        {
            LOG_DEBUG(logging,
                "ChatHandler::isValidChatMessage pipe followed by \\0");
            return false;
        }

        // no further pipe commands
        if (reader.eof())
            break;

        char commandChar;
        reader >> commandChar;

        // | in normal messages is escaped by ||
        if (commandChar != '|')
        {
            if (commandChar == *validSequenceIterator)
            {
                if (validSequenceIterator == validSequence + 4)
                    validSequenceIterator = validSequence;
                else
                    ++validSequenceIterator;
            }
            else
            {
                LOG_DEBUG(logging,
                    "ChatHandler::isValidChatMessage invalid sequence, "
                    "expected %c but got %c",
                    *validSequenceIterator, commandChar);
                return false;
            }
        }
        else if (validSequence != validSequenceIterator)
        {
            // no escaped pipes in sequences
            LOG_DEBUG(logging,
                "ChatHandler::isValidChatMessage got escaped pipe in sequence");
            return false;
        }

        switch (commandChar)
        {
        case 'c':
            color = 0;
            // validate color, expect 8 hex chars
            for (int i = 0; i < 8; i++)
            {
                char c;
                reader >> c;
                if (!c)
                {
                    LOG_DEBUG(logging,
                        "ChatHandler::isValidChatMessage got \\0 while reading "
                        "color in |c command");
                    return false;
                }

                color <<= 4;
                // check for hex char
                if (c >= '0' && c <= '9')
                {
                    color |= c - '0';
                    continue;
                }
                if (c >= 'a' && c <= 'f')
                {
                    color |= 10 + c - 'a';
                    continue;
                }
                LOG_DEBUG(logging,
                    "ChatHandler::isValidChatMessage got non hex char '%c' "
                    "while reading color",
                    c);
                return false;
            }
            break;
        case 'H':
            // read chars up to colon  = link type
            reader.getline(buffer, 256, ':');
            if (reader.eof()) // : must be
                return false;

            if (strcmp(buffer, "item") == 0)
            {
                // read item entry
                reader.getline(buffer, 256, ':');
                if (reader.eof()) // : must be
                    return false;

                linkedItem = ObjectMgr::GetItemPrototype(atoi(buffer));
                if (!linkedItem)
                {
                    LOG_DEBUG(logging,
                        "ChatHandler::isValidChatMessage got invalid itemID %u "
                        "in |item command",
                        atoi(buffer));
                    return false;
                }

                if (color != ItemQualityColors[linkedItem->Quality])
                {
                    LOG_DEBUG(logging,
                        "ChatHandler::isValidChatMessage linked item has color "
                        "%u, but user claims %u",
                        ItemQualityColors[linkedItem->Quality], color);
                    return false;
                }

                // the itementry is followed by several integers which describe
                // an instance of this item

                // position relative after itemEntry
                const uint8 randomPropertyPosition = 6;

                int32 propertyId = 0;
                bool negativeNumber = false;
                char c;
                for (uint8 i = 0; i < randomPropertyPosition; ++i)
                {
                    propertyId = 0;
                    negativeNumber = false;
                    while ((c = reader.get()) != ':')
                    {
                        if (c >= '0' && c <= '9')
                        {
                            propertyId *= 10;
                            propertyId += c - '0';
                        }
                        else if (c == '-')
                            negativeNumber = true;
                        else
                            return false;
                    }
                }
                if (negativeNumber)
                    propertyId *= -1;

                if (propertyId > 0)
                {
                    itemProperty =
                        sItemRandomPropertiesStore.LookupEntry(propertyId);
                    if (!itemProperty)
                        return false;
                }
                else if (propertyId < 0)
                {
                    itemSuffix =
                        sItemRandomSuffixStore.LookupEntry(-propertyId);
                    if (!itemSuffix)
                        return false;
                }

                // ignore other integers
                while ((c >= '0' && c <= '9') || c == ':')
                {
                    reader.ignore(1);
                    c = reader.peek();
                }
            }
            else if (strcmp(buffer, "quest") == 0)
            {
                // no color check for questlinks, each client will adapt it
                // anyway
                uint32 questid = 0;
                // read questid
                char c = reader.peek();
                while (c >= '0' && c <= '9')
                {
                    reader.ignore(1);
                    questid *= 10;
                    questid += c - '0';
                    c = reader.peek();
                }

                linkedQuest = sObjectMgr::Instance()->GetQuestTemplate(questid);

                if (!linkedQuest)
                {
                    LOG_DEBUG(logging,
                        "ChatHandler::isValidChatMessage Questtemplate %u not "
                        "found",
                        questid);
                    return false;
                }

                if (c != ':')
                {
                    LOG_DEBUG(logging,
                        "ChatHandler::isValidChatMessage Invalid quest link "
                        "structure");
                    return false;
                }

                reader.ignore(1);
                c = reader.peek();
                // level
                uint32 questlevel = 0;
                while (c >= '0' && c <= '9')
                {
                    reader.ignore(1);
                    questlevel *= 10;
                    questlevel += c - '0';
                    c = reader.peek();
                }

                if (questlevel >= STRONG_MAX_LEVEL)
                {
                    LOG_DEBUG(logging,
                        "ChatHandler::isValidChatMessage Quest level %u too "
                        "big",
                        questlevel);
                    return false;
                }

                if (c != '|')
                {
                    LOG_DEBUG(logging,
                        "ChatHandler::isValidChatMessage Invalid quest link "
                        "structure");
                    return false;
                }
            }
            else if (strcmp(buffer, "talent") == 0)
            {
                // talent links are always supposed to be blue
                if (color != CHAT_LINK_COLOR_TALENT)
                    return false;

                // read talent entry
                reader.getline(buffer, 256, ':');
                if (reader.eof()) // : must be
                    return false;

                TalentEntry const* talentInfo =
                    sTalentStore.LookupEntry(atoi(buffer));
                if (!talentInfo)
                    return false;

                linkedSpell = sSpellStore.LookupEntry(talentInfo->RankID[0]);
                if (!linkedSpell)
                    return false;

                char c = reader.peek();
                // skillpoints? whatever, drop it
                while (c != '|' && c != '\0')
                {
                    reader.ignore(1);
                    c = reader.peek();
                }
            }
            else if (strcmp(buffer, "spell") == 0)
            {
                if (color != CHAT_LINK_COLOR_SPELL)
                    return false;

                uint32 spellid = 0;
                // read spell entry
                char c = reader.peek();
                while (c >= '0' && c <= '9')
                {
                    reader.ignore(1);
                    spellid *= 10;
                    spellid += c - '0';
                    c = reader.peek();
                }
                linkedSpell = sSpellStore.LookupEntry(spellid);
                if (!linkedSpell)
                    return false;
            }
            else if (strcmp(buffer, "enchant") == 0)
            {
                if (color != CHAT_LINK_COLOR_ENCHANT)
                    return false;

                uint32 spellid = 0;
                // read spell entry
                char c = reader.peek();
                while (c >= '0' && c <= '9')
                {
                    reader.ignore(1);
                    spellid *= 10;
                    spellid += c - '0';
                    c = reader.peek();
                }
                linkedSpell = sSpellStore.LookupEntry(spellid);
                if (!linkedSpell)
                    return false;
            }
            else
            {
                LOG_DEBUG(logging,
                    "ChatHandler::isValidChatMessage user sent unsupported "
                    "link type '%s'",
                    buffer);
                return false;
            }
            break;
        case 'h':
            // if h is next element in sequence, this one must contain the
            // linked text :)
            if (*validSequenceIterator == 'h')
            {
                // links start with '['
                if (reader.get() != '[')
                {
                    LOG_DEBUG(logging,
                        "ChatHandler::isValidChatMessage link caption doesn't "
                        "start with '['");
                    return false;
                }
                reader.getline(buffer, 256, ']');
                if (reader.eof()) // ] must be
                    return false;

                // verify the link name
                if (linkedSpell)
                {
                    // spells with that flag have a prefix of "$PROFESSION: "
                    if (linkedSpell->HasAttribute(SPELL_ATTR_TRADESPELL))
                    {
                        // lookup skillid
                        SkillLineAbilityMapBounds bounds =
                            sSpellMgr::Instance()->GetSkillLineAbilityMapBounds(
                                linkedSpell->Id);
                        if (bounds.first == bounds.second)
                        {
                            return false;
                        }

                        SkillLineAbilityEntry const* skillInfo =
                            bounds.first->second;

                        if (!skillInfo)
                        {
                            return false;
                        }

                        SkillLineEntry const* skillLine =
                            sSkillLineStore.LookupEntry(skillInfo->skillId);
                        if (!skillLine)
                        {
                            return false;
                        }

                        for (uint8 i = 0; i < MAX_LOCALE; ++i)
                        {
                            uint32 skillLineNameLength =
                                strlen(skillLine->name[i]);
                            if (skillLineNameLength > 0 &&
                                strncmp(skillLine->name[i], buffer,
                                    skillLineNameLength) == 0)
                            {
                                // found the prefix, remove it to perform
                                // spellname validation below
                                // -2 = strlen(": ")
                                uint32 spellNameLength =
                                    strlen(buffer) - skillLineNameLength - 2;
                                memmove(buffer,
                                    buffer + skillLineNameLength + 2,
                                    spellNameLength + 1);
                            }
                        }
                    }
                    bool foundName = false;
                    for (uint8 i = 0; i < MAX_LOCALE; ++i)
                    {
                        if (*linkedSpell->SpellName[i] &&
                            strcmp(linkedSpell->SpellName[i], buffer) == 0)
                        {
                            foundName = true;
                            break;
                        }
                    }
                    if (!foundName)
                        return false;
                }
                else if (linkedQuest)
                {
                    if (linkedQuest->GetTitle() != buffer)
                    {
                        QuestLocale const* ql =
                            sObjectMgr::Instance()->GetQuestLocale(
                                linkedQuest->GetQuestId());

                        if (!ql)
                        {
                            LOG_DEBUG(logging,
                                "ChatHandler::isValidChatMessage default "
                                "questname didn't match and there is no "
                                "locale");
                            return false;
                        }

                        bool foundName = false;
                        for (auto& elem : ql->Title)
                        {
                            if (elem == buffer)
                            {
                                foundName = true;
                                break;
                            }
                        }
                        if (!foundName)
                        {
                            LOG_DEBUG(logging,
                                "ChatHandler::isValidChatMessage no quest "
                                "locale title matched");
                            return false;
                        }
                    }
                }
                else if (linkedItem)
                {
                    char* const* suffix =
                        itemSuffix ?
                            itemSuffix->nameSuffix :
                            (itemProperty ? itemProperty->nameSuffix : nullptr);

                    std::string expectedName = std::string(linkedItem->Name1);
                    if (suffix)
                    {
                        expectedName += " ";
                        expectedName += suffix[LOCALE_enUS];
                    }

                    if (expectedName != buffer)
                    {
                        ItemLocale const* il =
                            sObjectMgr::Instance()->GetItemLocale(
                                linkedItem->ItemId);

                        bool foundName = false;
                        for (uint8 i = LOCALE_koKR; i < MAX_LOCALE; ++i)
                        {
                            int8 dbIndex =
                                sObjectMgr::Instance()->GetIndexForLocale(
                                    LocaleConstant(i));
                            if (dbIndex == -1 || il == nullptr ||
                                (size_t)dbIndex >= il->Name.size())
                                // using strange database/client combinations
                                // can lead to this case
                                expectedName = linkedItem->Name1;
                            else
                                expectedName = il->Name[dbIndex];
                            if (suffix)
                            {
                                expectedName += " ";
                                expectedName += suffix[i];
                            }
                            if (expectedName == buffer)
                            {
                                foundName = true;
                                break;
                            }
                        }
                        if (!foundName)
                        {
                            LOG_DEBUG(logging,
                                "ChatHandler::isValidChatMessage linked item "
                                "name wasn't found in any localization");
                            return false;
                        }
                    }
                }
                // that place should never be reached - if nothing linked has
                // been set in |H
                // it will return false before
                else
                    return false;
            }
            break;
        case 'r':
        case '|':
            // no further payload
            break;
        default:
            LOG_DEBUG(logging,
                "ChatHandler::isValidChatMessage got invalid command |%c",
                commandChar);
            return false;
        }
    }

// check if every opened sequence was also closed properly
#ifndef OPTIMIZED_BUILD
    if (validSequence != validSequenceIterator)
        LOG_DEBUG(
            logging, "ChatHandler::isValidChatMessage EOF in active sequence");
#endif
    return validSequence == validSequenceIterator;
}

// Note: target_guid used only in CHAT_MSG_WHISPER_INFORM mode (in this case
// channelName ignored)
void ChatHandler::FillMessageData(WorldPacket* data, WorldSession* session,
    uint8 type, uint32 language, const char* channelName, ObjectGuid targetGuid,
    const char* message, Unit* speaker)
{
    uint32 messageLength = (message ? strlen(message) : 0) + 1;

    data->initialize(SMSG_MESSAGECHAT, 100); // guess size
    *data << uint8(type);
    if ((type != CHAT_MSG_CHANNEL && type != CHAT_MSG_WHISPER) ||
        language == LANG_ADDON)
        *data << uint32(language);
    else
        *data << uint32(LANG_UNIVERSAL);

    switch (type)
    {
    case CHAT_MSG_SAY:
    case CHAT_MSG_PARTY:
    case CHAT_MSG_RAID:
    case CHAT_MSG_GUILD:
    case CHAT_MSG_OFFICER:
    case CHAT_MSG_YELL:
    case CHAT_MSG_WHISPER:
    case CHAT_MSG_CHANNEL:
    case CHAT_MSG_RAID_LEADER:
    case CHAT_MSG_RAID_WARNING:
    case CHAT_MSG_BG_SYSTEM_NEUTRAL:
    case CHAT_MSG_BG_SYSTEM_ALLIANCE:
    case CHAT_MSG_BG_SYSTEM_HORDE:
    case CHAT_MSG_BATTLEGROUND:
    case CHAT_MSG_BATTLEGROUND_LEADER:
        targetGuid =
            session ? session->GetPlayer()->GetObjectGuid() : ObjectGuid();
        break;
    case CHAT_MSG_MONSTER_SAY:
    case CHAT_MSG_MONSTER_PARTY:
    case CHAT_MSG_MONSTER_YELL:
    case CHAT_MSG_MONSTER_WHISPER:
    case CHAT_MSG_MONSTER_EMOTE:
    case CHAT_MSG_RAID_BOSS_WHISPER:
    case CHAT_MSG_RAID_BOSS_EMOTE:
    {
        *data << ObjectGuid(speaker->GetObjectGuid());
        *data << uint32(0); // 2.1.0
        *data << uint32(strlen(speaker->GetName()) + 1);
        *data << speaker->GetName();
        ObjectGuid listener_guid;
        *data << listener_guid;
        if (listener_guid && !listener_guid.IsPlayer())
        {
            *data << uint32(1); // string listener_name_length
            *data << uint8(0);  // string listener_name
        }
        *data << uint32(messageLength);
        *data << message;
        *data << uint8(0);
        return;
    }
    default:
        if (type != CHAT_MSG_REPLY && type != CHAT_MSG_IGNORED &&
            type != CHAT_MSG_DND && type != CHAT_MSG_AFK)
            targetGuid.Clear(); // only for CHAT_MSG_WHISPER_INFORM used
                                // original value target_guid
        break;
    }

    *data << ObjectGuid(targetGuid); // there 0 for BG messages
    *data << uint32(0);              // can be chat msg group or something

    if (type == CHAT_MSG_CHANNEL)
    {
        assert(channelName);
        *data << channelName;
    }

    *data << ObjectGuid(targetGuid);
    *data << uint32(messageLength);
    *data << message;
    if (session != nullptr && type != CHAT_MSG_REPLY && type != CHAT_MSG_DND &&
        type != CHAT_MSG_AFK)
        *data << uint8(session->GetPlayer()->chatTag());
    else
        *data << uint8(0);
}

Player* ChatHandler::getSelectedPlayer()
{
    if (!m_session)
        return nullptr;

    ObjectGuid guid = m_session->GetPlayer()->GetSelectionGuid();

    if (!guid)
        return m_session->GetPlayer();

    return sObjectMgr::Instance()->GetPlayer(guid);
}

Unit* ChatHandler::getSelectedUnit()
{
    if (!m_session)
        return nullptr;

    ObjectGuid guid = m_session->GetPlayer()->GetSelectionGuid();

    if (!guid)
        return m_session->GetPlayer();

    // can be selected player at another map
    return ObjectAccessor::GetUnit(*m_session->GetPlayer(), guid);
}

Creature* ChatHandler::getSelectedCreature()
{
    if (!m_session)
        return nullptr;

    return m_session->GetPlayer()->GetMap()->GetAnyTypeCreature(
        m_session->GetPlayer()->GetSelectionGuid());
}

/**
 * Function skip all whitespaces in args string
 *
 * @param args variable pointer to non parsed args string, updated at function
 *call to new position (with skipped white spaces)
 *             allowed NULL string pointer stored in *args
 */
void ChatHandler::SkipWhiteSpaces(char** args)
{
    if (!*args)
        return;

    while (isWhiteSpace(**args))
        ++(*args);
}

/**
 * Function extract to val arg signed integer value or fail
 *
 * @param args variable pointer to non parsed args string, updated at function
 *call to new position (with skipped white spaces)
 * @param val  return extracted value if function success, in fail case original
 *value unmodified
 * @return     true if value extraction successful
 */
bool ChatHandler::ExtractInt32(char** args, int32& val)
{
    if (!*args || !**args)
        return false;

    char* tail = *args;

    long valRaw = strtol(*args, &tail, 10);

    if (tail != *args && isWhiteSpace(*tail))
        *(tail++) = '\0';
    else if (tail && *tail) // some not whitespace symbol
        return false;       // args not modified and can be re-parsed

    if (valRaw < std::numeric_limits<int32>::min() ||
        valRaw > std::numeric_limits<int32>::max())
        return false;

    // value successfully extracted
    val = int32(valRaw);
    *args = tail;
    return true;
}

/**
 * Function extract to val arg optional signed integer value or use default
 *value. Fail if extracted not signed integer.
 *
 * @param args    variable pointer to non parsed args string, updated at
 *function call to new position (with skipped white spaces)
 * @param val     return extracted value if function success, in fail case
 *original value unmodified
 * @param defVal  default value used if no data for extraction in args
 * @return        true if value extraction successful
 */
bool ChatHandler::ExtractOptInt32(char** args, int32& val, int32 defVal)
{
    if (!*args || !**args)
    {
        val = defVal;
        return true;
    }

    return ExtractInt32(args, val);
}

/**
 * Function extract to val arg unsigned integer value or fail
 *
 * @param args variable pointer to non parsed args string, updated at function
 *call to new position (with skipped white spaces)
 * @param val  return extracted value if function success, in fail case original
 *value unmodified
 * @param base set used base for extracted value format (10 for decimal, 16 for
 *hex, etc), 0 let auto select by system internal function
 * @return     true if value extraction successful
 */
bool ChatHandler::ExtractUInt32Base(char** args, uint32& val, uint32 base)
{
    if (!*args || !**args)
        return false;

    char* tail = *args;

    unsigned long valRaw = strtoul(*args, &tail, base);

    if (tail != *args && isWhiteSpace(*tail))
        *(tail++) = '\0';
    else if (tail && *tail) // some not whitespace symbol
        return false;       // args not modified and can be re-parsed

    if (valRaw > std::numeric_limits<uint32>::max())
        return false;

    // value successfully extracted
    val = uint32(valRaw);
    *args = tail;

    SkipWhiteSpaces(args);
    return true;
}

/**
 * Function extract to val arg optional unsigned integer value or use default
 *value. Fail if extracted not unsigned integer.
 *
 * @param args    variable pointer to non parsed args string, updated at
 *function call to new position (with skipped white spaces)
 * @param val     return extracted value if function success, in fail case
 *original value unmodified
 * @param defVal  default value used if no data for extraction in args
 * @return        true if value extraction successful
 */
bool ChatHandler::ExtractOptUInt32(char** args, uint32& val, uint32 defVal)
{
    if (!*args || !**args)
    {
        val = defVal;
        return true;
    }

    return ExtractUInt32(args, val);
}

/**
 * Function extract to val arg float value or fail
 *
 * @param args variable pointer to non parsed args string, updated at function
 *call to new position (with skipped white spaces)
 * @param val  return extracted value if function success, in fail case original
 *value unmodified
 * @return     true if value extraction successful
 */
bool ChatHandler::ExtractFloat(char** args, float& val)
{
    if (!*args || !**args)
        return false;

    char* tail = *args;

    double valRaw = strtod(*args, &tail);

    if (tail != *args && isWhiteSpace(*tail))
        *(tail++) = '\0';
    else if (tail && *tail) // some not whitespace symbol
        return false;       // args not modified and can be re-parsed

    // value successfully extracted
    val = float(valRaw);
    *args = tail;

    SkipWhiteSpaces(args);
    return true;
}

/**
 * Function extract to val arg optional float value or use default value. Fail
 *if extracted not float.
 *
 * @param args    variable pointer to non parsed args string, updated at
 *function call to new position (with skipped white spaces)
 * @param val     return extracted value if function success, in fail case
 *original value unmodified
 * @param defVal  default value used if no data for extraction in args
 * @return        true if value extraction successful
 */
bool ChatHandler::ExtractOptFloat(char** args, float& val, float defVal)
{
    if (!*args || !**args)
    {
        val = defVal;
        return true;
    }

    return ExtractFloat(args, val);
}

/**
 * Function extract name-like string (from non-numeric or special symbol until
 *whitespace)
 *
 * @param args variable pointer to non parsed args string, updated at function
 *call to new position (with skipped white spaces)
 * @param lit  optional explicit literal requirement. function fail if literal
 *is not starting substring of lit.
 *             Note: function in same way fail if no any literal or literal not
 *fit in this case. Need additional check for select specific fail case
 * @return     name/number-like string without whitespaces, or NULL if args
 *empty or not appropriate content.
 */
char* ChatHandler::ExtractLiteralArg(char** args, char const* lit /*= NULL*/)
{
    if (!*args || !**args)
        return nullptr;

    char* head = *args;

    // reject quoted string or link (|-started text)
    switch (head[0])
    {
    // reject quoted string
    case '[':
    case '\'':
    case '"':
        return nullptr;
    // reject link (|-started text)
    case '|':
        // client replace all | by || in raw text
        if (head[1] != '|')
            return nullptr;
        ++head; // skip one |
        break;
    default:
        break;
    }

    if (lit)
    {
        int l = strlen(lit);

        int largs = 0;
        while (head[largs] && !isWhiteSpace(head[largs]))
            ++largs;

        if (largs < l)
            l = largs;

        int diff = strncmp(head, lit, l);

        if (diff != 0)
            return nullptr;

        if (head[l] && !isWhiteSpace(head[l]))
            return nullptr;

        char* arg = head;

        if (head[l])
        {
            head[l] = '\0';

            head += l + 1;

            *args = head;
        }
        else
            *args = head + l;

        SkipWhiteSpaces(args);
        return arg;
    }

    char* name = strtok(head, " ");

    char* tail = strtok(nullptr, "");

    *args = tail ? tail : (char*)""; // *args don't must be NULL

    SkipWhiteSpaces(args);

    return name;
}

/**
 * Function extract quote-like string (any characters guarded by some special
 *character, in our cases ['")
 *
 * @param args variable pointer to non parsed args string, updated at function
 *call to new position (with skipped white spaces)
 * @param asis control save quote string wrappers
 * @return     quote-like string, or NULL if args empty or not appropriate
 *content.
 */
char* ChatHandler::ExtractQuotedArg(char** args, bool asis /*= false*/)
{
    if (!*args || !**args)
        return nullptr;

    if (**args != '\'' && **args != '"' && **args != '[')
        return nullptr;

    char guard = (*args)[0];

    if (guard == '[')
        guard = ']';

    char* tail = (*args) + 1;         // start scan after first quote symbol
    char* head = asis ? *args : tail; // start arg

    while (*tail && *tail != guard)
        ++tail;

    if (!*tail || (tail[1] && !isWhiteSpace(tail[1]))) // fail
        return nullptr;

    if (!tail[1]) // quote is last char in string
    {
        if (!asis)
            *tail = '\0';
    }
    else // quote isn't last char
    {
        if (asis)
            ++tail;

        *tail = '\0';
    }

    *args = tail + 1;

    SkipWhiteSpaces(args);

    return head;
}

/**
 * Function extract quote-like string or literal if quote not detected
 *
 * @param args variable pointer to non parsed args string, updated at function
 *call to new position (with skipped white spaces)
 * @param asis control save quote string wrappers
 * @return     quote/literal string, or NULL if args empty or not appropriate
 *content.
 */
char* ChatHandler::ExtractQuotedOrLiteralArg(char** args, bool asis /*= false*/)
{
    char* arg = ExtractQuotedArg(args, asis);
    if (!arg)
        arg = ExtractLiteralArg(args);
    return arg;
}

/**
 * Function extract on/off literals as boolean values
 *
 * @param args variable pointer to non parsed args string, updated at function
 *call to new position (with skipped white spaces)
 * @param val  return extracted value if function success, in fail case original
 *value unmodified
 * @return     true at success
 */
bool ChatHandler::ExtractOnOff(char** args, bool& value)
{
    char* arg = ExtractLiteralArg(args);
    if (!arg)
        return false;

    if (strncmp(arg, "on", 3) == 0)
        value = true;
    else if (strncmp(arg, "off", 4) == 0)
        value = false;
    else
        return false;

    return true;
}

/**
 * Function extract shift-link-like string (any characters guarded by | and |h|r
 *with some additional internal structure check)
 *
 * @param args variable pointer to non parsed args string, updated at function
 *call to new position (with skipped white spaces)
 *
 * @param linkTypes  optional NULL-terminated array of link types, shift-link
 *must fit one from link type from array if provided or extraction fail
 *
 * @param found_idx  if not NULL then at return index in linkTypes that fit
 *shift-link type, if extraction fail then non modified
 *
 * @param keyPair    if not NULL then pointer to 2-elements array for return
 *start and end pointer for found key
 *                   if extraction fail then non modified
 *
 * @param somethingPair then pointer to 2-elements array for return start and
 *end pointer if found.
 *                   if not NULL then shift-link must have data field, if
 *extraction fail then non modified
 *
 * @return     shift-link-like string, or NULL if args empty or not appropriate
 *content.
 */
char* ChatHandler::ExtractLinkArg(char** args,
    char const* const* linkTypes /*= NULL*/, int* foundIdx /*= NULL*/,
    char** keyPair /*= NULL*/, char** somethingPair /*= NULL*/)
{
    if (!*args || !**args)
        return nullptr;

    // skip if not linked started or encoded single | (doubled by client)
    if ((*args)[0] != '|' || (*args)[1] == '|')
        return nullptr;

    // |color|Hlinktype:key:data...|h[name]|h|r

    char* head = *args;

    // [name] Shift-click form |color|linkType:key|h[name]|h|r
    // or
    // [name] Shift-click form
    // |color|linkType:key:something1:...:somethingN|h[name]|h|r
    // or
    // [name] Shift-click form |linkType:key|h[name]|h|r

    // |color|Hlinktype:key:data...|h[name]|h|r

    char* tail = (*args) + 1; // skip |

    if (*tail != 'H') // skip color part, some links can not have color part
    {
        while (*tail && *tail != '|')
            ++tail;

        if (!*tail)
            return nullptr;

        // |Hlinktype:key:data...|h[name]|h|r

        ++tail; // skip |
    }

    // Hlinktype:key:data...|h[name]|h|r

    if (*tail != 'H')
        return nullptr;

    int linktype_idx = 0;

    if (linkTypes) // check link type if provided
    {
        // check linktypes (its include H in name)
        for (; linkTypes[linktype_idx]; ++linktype_idx)
        {
            // exactly string with follow : or |
            int l = strlen(linkTypes[linktype_idx]);
            if (strncmp(tail, linkTypes[linktype_idx], l) == 0 &&
                (tail[l] == ':' || tail[l] == '|'))
                break;
        }

        // is search fail?
        if (!linkTypes[linktype_idx]) // NULL terminator in last element
            return nullptr;

        tail += strlen(linkTypes[linktype_idx]); // skip linktype string

        // :key:data...|h[name]|h|r

        if (*tail != ':')
            return nullptr;
    }
    else
    {
        while (*tail && *tail != ':') // skip linktype string
            ++tail;

        if (!*tail)
            return nullptr;
    }

    ++tail;

    // key:data...|h[name]|h|r
    char* keyStart = tail; // remember key start for return
    char* keyEnd = tail;   // key end for truncate, will updated

    while (*tail && *tail != '|' && *tail != ':')
        ++tail;

    if (!*tail)
        return nullptr;

    keyEnd = tail; // remember key end for truncate

    // |h[name]|h|r or :something...|h[name]|h|r

    char* somethingStart = tail + 1;
    char* somethingEnd = tail + 1; // will updated later if need

    if (*tail == ':' && somethingPair) // optional data extraction
    {
        // :something...|h[name]|h|r

        if (*tail == ':')
            ++tail;

        // something|h[name]|h|r or something:something2...|h[name]|h|r

        while (*tail && *tail != '|' && *tail != ':')
            ++tail;

        if (!*tail)
            return nullptr;

        somethingEnd = tail; // remember data end for truncate
    }

    // |h[name]|h|r or :something2...|h[name]|h|r

    while (
        *tail && (*tail != '|' || *(tail + 1) != 'h')) // skip ... part if exist
        ++tail;

    if (!*tail)
        return nullptr;

    // |h[name]|h|r

    tail += 2; // skip |h

    // [name]|h|r
    if (!*tail || *tail != '[')
        return nullptr;

    while (*tail && (*tail != ']' || *(tail + 1) != '|')) // skip name part
        ++tail;

    tail += 2; // skip ]|

    // h|r
    if (!*tail || *tail != 'h' || *(tail + 1) != '|')
        return nullptr;

    tail += 2; // skip h|

    // r
    if (!*tail || *tail != 'r' || (*(tail + 1) && !isWhiteSpace(*(tail + 1))))
        return nullptr;

    ++tail; // skip r

    // success

    if (*tail) // truncate all link string
        *(tail++) = '\0';

    if (foundIdx)
        *foundIdx = linktype_idx;

    if (keyPair)
    {
        keyPair[0] = keyStart;
        keyPair[1] = keyEnd;
    }

    if (somethingPair)
    {
        somethingPair[0] = somethingStart;
        somethingPair[1] = somethingEnd;
    }

    *args = tail;

    SkipWhiteSpaces(args);

    return head;
}

/**
 * Function extract name/number/quote/shift-link-like string
 *
 * @param args variable pointer to non parsed args string, updated at function
 *call to new position (with skipped white spaces)
 * @param asis control save quote string wrappers
 * @return     extracted arg string, or NULL if args empty or not appropriate
 *content.
 */
char* ChatHandler::ExtractArg(char** args, bool asis /*= false*/)
{
    if (!*args || !**args)
        return nullptr;

    char* arg = ExtractQuotedOrLiteralArg(args, asis);
    if (!arg)
        arg = ExtractLinkArg(args);

    return arg;
}

/**
 * Function extract name/quote/number/shift-link-like string, and return it if
 *args have more non-whitespace data
 *
 * @param args variable pointer to non parsed args string, updated at function
 *call to new position (with skipped white spaces)
 *             if args have only single arg then args still pointing to this arg
 *(unmodified pointer)
 * @return     extracted string, or NULL if args empty or not appropriate
 *content or have single arg totally.
 */
char* ChatHandler::ExtractOptNotLastArg(char** args)
{
    char* arg = ExtractArg(args, true);

    // have more data
    if (*args && **args)
        return arg;

    // optional name not found
    *args = arg ? arg : (char*)""; // *args don't must be NULL

    return nullptr;
}

/**
 * Function extract data from shift-link
 *"|color|LINKTYPE:RETURN:SOMETHING1|h[name]|h|r if linkType == LINKTYPE
 * It also extract literal/quote if not shift-link in args
 *
 * @param args       variable pointer to non parsed args string, updated at
 *function call to new position (with skipped white spaces)
 *                   if args have sift link with linkType != LINKTYPE then args
 *still pointing to this arg (unmodified pointer)
 *
 * @param linkType   shift-link must fit by link type to this arg value or
 *extraction fail
 *
 * @param something1 if not NULL then shift-link must have data field and it
 *returned into this arg
 *                   if extraction fail then non modified
 *
 * @return           extracted key, or NULL if args empty or not appropriate
 *content or not fit to linkType.
 */
char* ChatHandler::ExtractKeyFromLink(
    char** text, char const* linkType, char** something1 /*= NULL*/)
{
    char const* linkTypes[2];
    linkTypes[0] = linkType;
    linkTypes[1] = nullptr;

    int foundIdx;

    return ExtractKeyFromLink(text, linkTypes, &foundIdx, something1);
}

/**
 * Function extract data from shift-link
 *"|color|LINKTYPE:RETURN:SOMETHING1|h[name]|h|r if LINKTYPE in linkTypes array
 * It also extract literal/quote if not shift-link in args
 *
 * @param args       variable pointer to non parsed args string, updated at
 *function call to new position (with skipped white spaces)
 *                   if args have sift link with linkType != LINKTYPE then args
 *still pointing to this arg (unmodified pointer)
 *
 * @param linkTypes  NULL-terminated array of link types, shift-link must fit
 *one from link type from array or extraction fail
 *
 * @param found_idx  if not NULL then at return index in linkTypes that fit
 *shift-link type, for non-link case return -1
 *                   if extraction fail then non modified
 *
 * @param something1 if not NULL then shift-link must have data field and it
 *returned into this arg
 *                   if extraction fail then non modified
 *
 * @return           extracted key, or NULL if args empty or not appropriate
 *content or not fit to linkType.
 */
char* ChatHandler::ExtractKeyFromLink(char** text, char const* const* linkTypes,
    int* found_idx, char** something1 /*= NULL*/)
{
    // skip empty
    if (!*text || !**text)
        return nullptr;

    // return non link case
    char* arg = ExtractQuotedOrLiteralArg(text);
    if (arg)
    {
        if (found_idx)
            *found_idx = -1; // special index case

        return arg;
    }

    char* keyPair[2];
    char* somethingPair[2];

    arg = ExtractLinkArg(text, linkTypes, found_idx, keyPair,
        something1 ? somethingPair : nullptr);
    if (!arg)
        return nullptr;

    *keyPair[1] = '\0'; // truncate key string

    if (something1)
    {
        *somethingPair[1] = '\0'; // truncate data string
        *something1 = somethingPair[0];
    }

    return keyPair[0];
}

/**
 * Function extract uint32 key from shift-link
 *"|color|LINKTYPE:RETURN|h[name]|h|r if linkType == LINKTYPE
 * It also extract direct number if not shift-link in args
 *
 * @param args       variable pointer to non parsed args string, updated at
 *function call to new position (with skipped white spaces)
 *                   if args have sift link with linkType != LINKTYPE then args
 *still pointing to this arg (unmodified pointer)
 *
 * @param linkType   shift-link must fit by link type to this arg value or
 *extraction fail
 *
 * @param value      store result value at success return, not modified at fail
 *
 * @return           true if extraction succesful
 */
bool ChatHandler::ExtractUint32KeyFromLink(
    char** text, char const* linkType, uint32& value)
{
    char* arg = ExtractKeyFromLink(text, linkType);
    if (!arg)
        return false;

    return ExtractUInt32(&arg, value);
}

GameObject* ChatHandler::GetGameObjectWithGuid(uint32 lowguid, uint32 entry)
{
    if (!m_session)
        return nullptr;

    Player* pl = m_session->GetPlayer();

    return pl->GetMap()->GetGameObject(
        ObjectGuid(HIGHGUID_GAMEOBJECT, entry, lowguid));
}

enum SpellLinkType
{
    SPELL_LINK_RAW = -1, // non-link case
    SPELL_LINK_SPELL = 0,
    SPELL_LINK_TALENT = 1,
    SPELL_LINK_ENCHANT = 2,
};

static char const* const spellKeys[] = {"Hspell", // normal spell
    "Htalent",                                    // talent spell
    "Henchant",                                   // enchanting recipe spell
    nullptr};

uint32 ChatHandler::ExtractSpellIdFromLink(char** text)
{
    // number or [name] Shift-click form
    // |color|Henchant:recipe_spell_id|h[prof_name: recipe_name]|h|r
    // number or [name] Shift-click form |color|Hspell:spell_id|h[name]|h|r
    // number or [name] Shift-click form
    // |color|Htalent:talent_id,rank|h[name]|h|r
    int type;
    char* param1_str = nullptr;
    char* idS = ExtractKeyFromLink(text, spellKeys, &type, &param1_str);
    if (!idS)
        return 0;

    uint32 id;
    if (!ExtractUInt32(&idS, id))
        return 0;

    switch (type)
    {
    case SPELL_LINK_RAW:
    case SPELL_LINK_SPELL:
    case SPELL_LINK_ENCHANT:
        return id;
    case SPELL_LINK_TALENT:
    {
        // talent
        TalentEntry const* talentEntry = sTalentStore.LookupEntry(id);
        if (!talentEntry)
            return 0;

        int32 rank;
        if (!ExtractInt32(&param1_str, rank))
            return 0;

        if (rank < 0) // unlearned talent have in shift-link field -1 as rank
            rank = 0;

        return rank < MAX_TALENT_RANK ? talentEntry->RankID[rank] : 0;
    }
    }

    // unknown type?
    return 0;
}

GameTele const* ChatHandler::ExtractGameTeleFromLink(char** text)
{
    // id, or string, or [name] Shift-click form |color|Htele:id|h[name]|h|r
    char* cId = ExtractKeyFromLink(text, "Htele");
    if (!cId)
        return nullptr;

    // id case (explicit or from shift link)
    uint32 id;
    if (ExtractUInt32(&cId, id))
        return sObjectMgr::Instance()->GetGameTele(id);
    else
        return sObjectMgr::Instance()->GetGameTele(cId, true);
}

enum GuidLinkType
{
    GUID_LINK_RAW = -1, // non-link case
    GUID_LINK_PLAYER = 0,
    GUID_LINK_CREATURE = 1,
    GUID_LINK_GAMEOBJECT = 2,
};

static char const* const guidKeys[] = {
    "Hplayer", "Hcreature", "Hgameobject", nullptr};

ObjectGuid ChatHandler::ExtractGuidFromLink(char** text)
{
    int type = 0;

    // |color|Hcreature:creature_guid|h[name]|h|r
    // |color|Hgameobject:go_guid|h[name]|h|r
    // |color|Hplayer:name|h[name]|h|r
    char* idS = ExtractKeyFromLink(text, guidKeys, &type);
    if (!idS)
        return ObjectGuid();

    switch (type)
    {
    case GUID_LINK_RAW:
    case GUID_LINK_PLAYER:
    {
        std::string name = idS;
        if (!normalizePlayerName(name))
            return ObjectGuid();

        if (Player* player = sObjectMgr::Instance()->GetPlayer(name.c_str()))
            return player->GetObjectGuid();

        return sObjectMgr::Instance()->GetPlayerGuidByName(name);
    }
    case GUID_LINK_CREATURE:
    {
        uint32 lowguid;
        if (!ExtractUInt32(&idS, lowguid))
            return ObjectGuid();

        if (CreatureData const* data =
                sObjectMgr::Instance()->GetCreatureData(lowguid))
            return data->GetObjectGuid(lowguid);
        else
            return ObjectGuid();
    }
    case GUID_LINK_GAMEOBJECT:
    {
        uint32 lowguid;
        if (!ExtractUInt32(&idS, lowguid))
            return ObjectGuid();

        if (GameObjectData const* data =
                sObjectMgr::Instance()->GetGOData(lowguid))
            return ObjectGuid(HIGHGUID_GAMEOBJECT, data->id, lowguid);
        else
            return ObjectGuid();
    }
    }

    // unknown type?
    return ObjectGuid();
}

enum LocationLinkType
{
    LOCATION_LINK_RAW = -1, // non-link case
    LOCATION_LINK_PLAYER = 0,
    LOCATION_LINK_TELE = 1,
    LOCATION_LINK_TAXINODE = 2,
    LOCATION_LINK_CREATURE = 3,
    LOCATION_LINK_GAMEOBJECT = 4,
    LOCATION_LINK_CREATURE_ENTRY = 5,
    LOCATION_LINK_GAMEOBJECT_ENTRY = 6,
    LOCATION_LINK_AREATRIGGER = 7,
    LOCATION_LINK_AREATRIGGER_TARGET = 8,
};

static char const* const locationKeys[] = {"Htele", "Htaxinode", "Hplayer",
    "Hcreature", "Hgameobject", "Hcreature_entry", "Hgameobject_entry",
    "Hareatrigger", "Hareatrigger_target", nullptr};

bool ChatHandler::ExtractLocationFromLink(
    char** text, uint32& mapid, float& x, float& y, float& z)
{
    int type = 0;

    // |color|Hplayer:name|h[name]|h|r
    // |color|Htele:id|h[name]|h|r
    // |color|Htaxinode:id|h[name]|h|r
    // |color|Hcreature:creature_guid|h[name]|h|r
    // |color|Hgameobject:go_guid|h[name]|h|r
    // |color|Hcreature_entry:creature_id|h[name]|h|r
    // |color|Hgameobject_entry:go_id|h[name]|h|r
    // |color|Hareatrigger:id|h[name]|h|r
    // |color|Hareatrigger_target:id|h[name]|h|r
    char* idS = ExtractKeyFromLink(text, locationKeys, &type);
    if (!idS)
        return false;

    switch (type)
    {
    case LOCATION_LINK_RAW:
    case LOCATION_LINK_PLAYER:
    {
        std::string name = idS;
        if (!normalizePlayerName(name))
            return false;

        if (Player* player = sObjectMgr::Instance()->GetPlayer(name.c_str()))
        {
            mapid = player->GetMapId();
            x = player->GetX();
            y = player->GetY();
            z = player->GetZ();
            return true;
        }

        if (ObjectGuid guid = sObjectMgr::Instance()->GetPlayerGuidByName(name))
        {
            // to point where player stay (if loaded)
            float o;
            bool in_flight;
            return Player::LoadPositionFromDB(
                guid, mapid, x, y, z, o, in_flight);
        }

        return false;
    }
    case LOCATION_LINK_TELE:
    {
        uint32 id;
        if (!ExtractUInt32(&idS, id))
            return false;

        GameTele const* tele = sObjectMgr::Instance()->GetGameTele(id);
        if (!tele)
            return false;
        mapid = tele->mapId;
        x = tele->position_x;
        y = tele->position_y;
        z = tele->position_z;
        return true;
    }
    case LOCATION_LINK_TAXINODE:
    {
        uint32 id;
        if (!ExtractUInt32(&idS, id))
            return false;

        TaxiNodesEntry const* node = sTaxiNodesStore.LookupEntry(id);
        if (!node)
            return false;
        mapid = node->map_id;
        x = node->x;
        y = node->y;
        z = node->z;
        return true;
    }
    case LOCATION_LINK_CREATURE:
    {
        uint32 lowguid;
        if (!ExtractUInt32(&idS, lowguid))
            return false;

        if (CreatureData const* data =
                sObjectMgr::Instance()->GetCreatureData(lowguid))
        {
            mapid = data->mapid;
            x = data->posX;
            y = data->posY;
            z = data->posZ;
            return true;
        }
        else
            return false;
    }
    case LOCATION_LINK_GAMEOBJECT:
    {
        uint32 lowguid;
        if (!ExtractUInt32(&idS, lowguid))
            return false;

        if (GameObjectData const* data =
                sObjectMgr::Instance()->GetGOData(lowguid))
        {
            mapid = data->mapid;
            x = data->posX;
            y = data->posY;
            z = data->posZ;
            return true;
        }
        else
            return false;
    }
    case LOCATION_LINK_CREATURE_ENTRY:
    {
        uint32 id;
        if (!ExtractUInt32(&idS, id))
            return false;

        if (ObjectMgr::GetCreatureTemplate(id))
        {
            FindCreatureData worker(
                id, m_session ? m_session->GetPlayer() : nullptr);

            sObjectMgr::Instance()->DoCreatureData(worker);

            if (CreatureDataPair const* dataPair = worker.GetResult())
            {
                mapid = dataPair->second.mapid;
                x = dataPair->second.posX;
                y = dataPair->second.posY;
                z = dataPair->second.posZ;
                return true;
            }
            else
                return false;
        }
        else
            return false;
    }
    case LOCATION_LINK_GAMEOBJECT_ENTRY:
    {
        uint32 id;
        if (!ExtractUInt32(&idS, id))
            return false;

        if (ObjectMgr::GetGameObjectInfo(id))
        {
            FindGOData worker(id, m_session ? m_session->GetPlayer() : nullptr);

            sObjectMgr::Instance()->DoGOData(worker);

            if (GameObjectDataPair const* dataPair = worker.GetResult())
            {
                mapid = dataPair->second.mapid;
                x = dataPair->second.posX;
                y = dataPair->second.posY;
                z = dataPair->second.posZ;
                return true;
            }
            else
                return false;
        }
        else
            return false;
    }
    case LOCATION_LINK_AREATRIGGER:
    {
        uint32 id;
        if (!ExtractUInt32(&idS, id))
            return false;

        AreaTriggerEntry const* atEntry = sAreaTriggerStore.LookupEntry(id);
        if (!atEntry)
        {
            PSendSysMessage(LANG_COMMAND_GOAREATRNOTFOUND, id);
            SetSentErrorMessage(true);
            return false;
        }

        mapid = atEntry->mapid;
        x = atEntry->x;
        y = atEntry->y;
        z = atEntry->z;
        return true;
    }
    case LOCATION_LINK_AREATRIGGER_TARGET:
    {
        uint32 id;
        if (!ExtractUInt32(&idS, id))
            return false;

        if (!sAreaTriggerStore.LookupEntry(id))
        {
            PSendSysMessage(LANG_COMMAND_GOAREATRNOTFOUND, id);
            SetSentErrorMessage(true);
            return false;
        }

        AreaTrigger const* at = sObjectMgr::Instance()->GetAreaTrigger(id);
        if (!at)
        {
            PSendSysMessage(LANG_AREATRIGER_NOT_HAS_TARGET, id);
            SetSentErrorMessage(true);
            return false;
        }

        mapid = at->target_mapId;
        x = at->target_X;
        y = at->target_Y;
        z = at->target_Z;
        return true;
    }
    }

    // unknown type?
    return false;
}

std::string ChatHandler::ExtractPlayerNameFromLink(char** text)
{
    // |color|Hplayer:name|h[name]|h|r
    char* name_str = ExtractKeyFromLink(text, "Hplayer");
    if (!name_str)
        return "";

    std::string name = name_str;
    if (!normalizePlayerName(name))
        return "";

    return name;
}

/**
 * Function extract at least one from request player data (pointer/guid/name)
 *from args name/shift-link or selected player if no args
 *
 * @param args        variable pointer to non parsed args string, updated at
 *function call to new position (with skipped white spaces)
 *
 * @param player      optional arg   One from 3 optional args must be provided
 *at least (or more).
 * @param player_guid optional arg   For function success only one from provided
 *args need get result
 * @param player_name optional arg   But if early arg get value then all later
 *args will have its (if requested)
 *                                   if player_guid requested and not found then
 *name also will not found
 *                                   So at success can be returned 2 cases:
 *(player/guid/name) or (guid/name)
 *
 * @return           true if extraction successful
 */
bool ChatHandler::ExtractPlayerTarget(char** args, Player** player /*= NULL*/,
    ObjectGuid* player_guid /*= NULL*/, std::string* player_name /*= NULL*/)
{
    if (*args && **args)
    {
        std::string name = ExtractPlayerNameFromLink(args);
        if (name.empty())
        {
            SendSysMessage(LANG_PLAYER_NOT_FOUND);
            SetSentErrorMessage(true);
            return false;
        }

        Player* pl = sObjectMgr::Instance()->GetPlayer(name.c_str());

        // if allowed player pointer
        if (player)
            *player = pl;

        // if need guid value from DB (in name case for check player existence)
        ObjectGuid guid =
            !pl && (player_guid || player_name) ?
                sObjectMgr::Instance()->GetPlayerGuidByName(name) :
                ObjectGuid();

        // if allowed player guid (if no then only online players allowed)
        if (player_guid)
            *player_guid = pl ? pl->GetObjectGuid() : guid;

        if (player_name)
            *player_name = pl || guid ? name : "";
    }
    else
    {
        Player* pl = getSelectedPlayer();
        // if allowed player pointer
        if (player)
            *player = pl;
        // if allowed player guid (if no then only online players allowed)
        if (player_guid)
            *player_guid = pl ? pl->GetObjectGuid() : ObjectGuid();

        if (player_name)
            *player_name = pl ? pl->GetName() : "";
    }

    // some from req. data must be provided (note: name is empty if player not
    // exist)
    if ((!player || !*player) && (!player_guid || !*player_guid) &&
        (!player_name || player_name->empty()))
    {
        SendSysMessage(LANG_PLAYER_NOT_FOUND);
        SetSentErrorMessage(true);
        return false;
    }

    return true;
}

uint32 ChatHandler::ExtractAccountId(char** args,
    std::string* accountName /*= NULL*/, Player** targetIfNullArg /*= NULL*/)
{
    uint32 account_id = 0;

    ///- Get the account name from the command line
    char* account_str = ExtractLiteralArg(args);

    if (!account_str)
    {
        if (!targetIfNullArg)
            return 0;

        /// only target player different from self allowed (if
        /// targetPlayer!=NULL then not console)
        Player* targetPlayer = getSelectedPlayer();
        if (!targetPlayer)
            return 0;

        account_id = targetPlayer->GetSession()->GetAccountId();

        if (accountName)
            sAccountMgr::Instance()->GetName(account_id, *accountName);

        if (targetIfNullArg)
            *targetIfNullArg = targetPlayer;

        return account_id;
    }

    std::string account_name;

    if (ExtractUInt32(&account_str, account_id))
    {
        if (!sAccountMgr::Instance()->GetName(account_id, account_name))
        {
            PSendSysMessage(LANG_ACCOUNT_NOT_EXIST, account_str);
            SetSentErrorMessage(true);
            return 0;
        }
    }
    else
    {
        account_name = account_str;
        if (!AccountMgr::normalizeString(account_name))
        {
            PSendSysMessage(LANG_ACCOUNT_NOT_EXIST, account_name.c_str());
            SetSentErrorMessage(true);
            return 0;
        }

        account_id = sAccountMgr::Instance()->GetId(account_name);
        if (!account_id)
        {
            PSendSysMessage(LANG_ACCOUNT_NOT_EXIST, account_name.c_str());
            SetSentErrorMessage(true);
            return 0;
        }
    }

    if (accountName)
        *accountName = account_name;

    if (targetIfNullArg)
        *targetIfNullArg = nullptr;

    return account_id;
}

bool ChatHandler::ExtractCopper(
    char** args, int32& copper, bool send_usage_on_failure)
{
    if (!*args || !**args)
        return false;

    static const char* usage_string =
        "Incorrectly Formatted Gold. Formatting examples: 5g10s5c, -10g, or "
        "5s.";

    char* itr = *args;
    bool negative = false;

    // All whitespaces before us has been skipped by the previous code

    if (*itr == '-')
    {
        negative = true;
        ++itr;
    }

    inventory::copper c(0);
    int num = 0, stage = 0;
    for (; *itr && !isWhiteSpace(*itr); ++itr)
    {
        if (isdigit(*itr))
        {
            num *= 10; // Positional increment
            num += *itr - '0';

            if (num > 200000) // Maximum amount of gold, and silver and copper
                              // can only be 99 as most
            {
                if (send_usage_on_failure)
                    SendSysMessage(usage_string);
                return false;
            }
            continue;
        }

        if (*itr == 'g')
        {
            if (stage > 0)
            {
                if (send_usage_on_failure)
                    SendSysMessage(usage_string);
                return false;
            }
            stage = 1;
            c = num * 10000;
            num = 0;
        }
        else if (*itr == 's')
        {
            if (stage > 1)
            {
                if (send_usage_on_failure)
                    SendSysMessage(usage_string);
                return false;
            }
            stage = 2;
            if (num > 99)
            {
                if (send_usage_on_failure)
                    SendSysMessage(usage_string);
                return false;
            }
            c += num * 100;
            num = 0;
        }
        else if (*itr == 'c')
        {
            if (stage > 2)
            {
                if (send_usage_on_failure)
                    SendSysMessage(usage_string);
                return false;
            }
            stage = 3;
            if (num > 99)
            {
                if (send_usage_on_failure)
                    SendSysMessage(usage_string);
                return false;
            }
            c += num;
            num = 0;
        }
        else
        {
            if (send_usage_on_failure)
                SendSysMessage(usage_string);
            return false; // Unrecognized character
        }

        // Copper class takes care of overflows for us
        if (c.spill())
        {
            if (send_usage_on_failure)
                SendSysMessage(usage_string);
            return false;
        }
    }

    if (c.get() == 0)
    {
        if (send_usage_on_failure)
            SendSysMessage(usage_string);
        return false;
    }

    // Successful read, move args forward and skip following white spaces
    *args = itr;
    SkipWhiteSpaces(args);

    copper = c.get();
    if (negative)
        copper = -copper;

    return true;
}

struct RaceMaskName
{
    char const* literal;
    uint32 raceMask;
};

static RaceMaskName const raceMaskNames[] = {
    // races
    {"human", (1 << (RACE_HUMAN - 1))}, {"orc", (1 << (RACE_ORC - 1))},
    {"dwarf", (1 << (RACE_DWARF - 1))},
    {"nightelf", (1 << (RACE_NIGHTELF - 1))},
    {"undead", (1 << (RACE_UNDEAD - 1))}, {"tauren", (1 << (RACE_TAUREN - 1))},
    {"gnome", (1 << (RACE_GNOME - 1))}, {"troll", (1 << (RACE_TROLL - 1))},
    {"bloodelf", (1 << (RACE_BLOODELF - 1))},
    {"draenei", (1 << (RACE_DRAENEI - 1))},

    // masks
    {"alliance", RACEMASK_ALLIANCE}, {"horde", RACEMASK_HORDE},
    {"all", RACEMASK_ALL_PLAYABLE},

    // terminator
    {nullptr, 0}};

bool ChatHandler::ExtractRaceMask(
    char** text, uint32& raceMask, char const** maskName /*=NULL*/)
{
    if (ExtractUInt32(text, raceMask))
    {
        if (maskName)
            *maskName = "custom mask";
    }
    else
    {
        for (RaceMaskName const* itr = raceMaskNames; itr->literal; ++itr)
        {
            if (ExtractLiteralArg(text, itr->literal))
            {
                raceMask = itr->raceMask;

                if (maskName)
                    *maskName = itr->literal;
                break;
            }
        }

        if (!raceMask)
            return false;
    }

    return true;
}

std::string ChatHandler::GetNameLink(Player* chr) const
{
    return playerLink(chr->GetName());
}

bool ChatHandler::needReportToTarget(Player* chr) const
{
    Player* pl = m_session->GetPlayer();
    return pl != chr && pl->IsVisibleGloballyFor(chr);
}

LocaleConstant ChatHandler::GetSessionDbcLocale() const
{
    return m_session->GetSessionDbcLocale();
}

int ChatHandler::GetSessionDbLocaleIndex() const
{
    return m_session->GetSessionDbLocaleIndex();
}

const char* cli_cmd_handler::GetMangosString(int32 entry) const
{
    return sObjectMgr::Instance()->GetMangosStringForDBCLocale(entry);
}

uint32 cli_cmd_handler::GetAccountId() const
{
    return command_.acc_id();
}

AccountTypes cli_cmd_handler::GetAccessLevel() const
{
    return command_.access();
}

bool cli_cmd_handler::isAvailable(ChatCommand const& cmd) const
{
    // skip non-console commands in console case
    if (!cmd.AllowConsole)
        return false;

    // normal case
    return GetAccessLevel() >= (AccountTypes)cmd.SecurityLevel;
}

void cli_cmd_handler::SendSysMessage(const char* str)
{
    command_.print(str);
}

std::string cli_cmd_handler::GetNameLink() const
{
    return GetMangosString(LANG_CONSOLE_COMMAND);
}

bool cli_cmd_handler::needReportToTarget(Player* /*chr*/) const
{
    return true;
}

LocaleConstant cli_cmd_handler::GetSessionDbcLocale() const
{
    return sWorld::Instance()->GetDefaultDbcLocale();
}

int cli_cmd_handler::GetSessionDbLocaleIndex() const
{
    return sObjectMgr::Instance()->GetDBCLocaleIndex();
}

// Check/ Output if a NPC or GO (by guid) is part of a pool or game event
template <typename T>
void ChatHandler::ShowNpcOrGoSpawnInformation(uint32 guid)
{
    if (auto pool_id = sPoolMgr::Instance()->IsPartOfAPool<T>(guid))
    {
        auto top_pool_id = sPoolMgr::Instance()->IsPartOfTopPool<Pool>(pool_id);
        if (!top_pool_id || top_pool_id == pool_id)
            PSendSysMessage(LANG_NPC_GO_INFO_POOL, pool_id);
        else
            PSendSysMessage(LANG_NPC_GO_INFO_TOP_POOL, pool_id, top_pool_id);

        if (int16 event_id =
                sGameEventMgr::Instance()->GetGameEventId<Pool>(top_pool_id))
        {
            GameEventMgr::GameEventDataMap const& events =
                sGameEventMgr::Instance()->GetEventMap();
            GameEventData const& eventData = events[std::abs(event_id)];

            if (event_id > 0)
                PSendSysMessage(LANG_NPC_GO_INFO_POOL_GAME_EVENT_S, top_pool_id,
                    std::abs(event_id), eventData.description.c_str());
            else
                PSendSysMessage(LANG_NPC_GO_INFO_POOL_GAME_EVENT_D, top_pool_id,
                    std::abs(event_id), eventData.description.c_str());
        }
    }
    else if (int16 event_id =
                 sGameEventMgr::Instance()->GetGameEventId<T>(guid))
    {
        GameEventMgr::GameEventDataMap const& events =
            sGameEventMgr::Instance()->GetEventMap();
        GameEventData const& eventData = events[std::abs(event_id)];

        if (event_id > 0)
            PSendSysMessage(LANG_NPC_GO_INFO_GAME_EVENT_S, std::abs(event_id),
                eventData.description.c_str());
        else
            PSendSysMessage(LANG_NPC_GO_INFO_GAME_EVENT_D, std::abs(event_id),
                eventData.description.c_str());
    }
}

// Prepare ShortString for a NPC or GO (by guid) with pool or game event IDs
template <typename T>
std::string ChatHandler::PrepareStringNpcOrGoSpawnInformation(uint32 guid)
{
    std::string str = "";
    if (auto pool_id = sPoolMgr::Instance()->IsPartOfAPool<T>(guid))
    {
        uint16 top_pool_id = sPoolMgr::Instance()->IsPartOfTopPool<T>(guid);
        if (int16 event_id =
                sGameEventMgr::Instance()->GetGameEventId<Pool>(top_pool_id))
        {
            char buffer[100];
            const char* format =
                GetMangosString(LANG_NPC_GO_INFO_POOL_EVENT_STRING);
            sprintf(buffer, format, pool_id, event_id);
            str = buffer;
        }
        else
        {
            char buffer[100];
            const char* format = GetMangosString(LANG_NPC_GO_INFO_POOL_STRING);
            sprintf(buffer, format, pool_id);
            str = buffer;
        }
    }
    else if (int16 event_id =
                 sGameEventMgr::Instance()->GetGameEventId<T>(guid))
    {
        char buffer[100];
        const char* format = GetMangosString(LANG_NPC_GO_INFO_EVENT_STRING);
        sprintf(buffer, format, event_id);
        str = buffer;
    }

    return str;
}

void ChatHandler::LogCommand(char const* fullcmd)
{
    // chat case
    if (m_session)
    {
        Player* p = m_session->GetPlayer();
        ObjectGuid sel_guid = p->GetSelectionGuid();
        gm_logger.info(
            "Command: %s [Player: %s (Account: %u) X: %f Y: %f Z: %f Map: %u "
            "Selected: %s]",
            fullcmd, p->GetName(), GetAccountId(), p->GetX(), p->GetY(),
            p->GetZ(), p->GetMapId(), sel_guid.GetString().c_str());
    }
    else // 0 account -> console
    {
        gm_logger.info("Command: %s [Account: %u from %s]", fullcmd,
            GetAccountId(), GetAccountId() ? "RA-connection" : "Console");
    }
}

// Instantiate template for helper function
template void ChatHandler::ShowNpcOrGoSpawnInformation<Creature>(uint32 guid);
template void ChatHandler::ShowNpcOrGoSpawnInformation<GameObject>(uint32 guid);

template std::string
ChatHandler::PrepareStringNpcOrGoSpawnInformation<Creature>(uint32 guid);
template std::string
ChatHandler::PrepareStringNpcOrGoSpawnInformation<GameObject>(uint32 guid);
