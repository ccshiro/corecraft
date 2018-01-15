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

#include "AccountMgr.h"
#include "BattleGroundMgr.h"
#include "Chat.h"
#include "Common.h"
#include "CreatureEventAIMgr.h"
#include "DBCStores.h"
#include "GameObject.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "InstanceData.h"
#include "ItemEnchantmentMgr.h"
#include "Language.h"
#include "logging.h"
#include "Mail.h"
#include "MapManager.h"
#include "MapPersistentStateMgr.h"
#include "MassMailMgr.h"
#include "movement/PointMovementGenerator.h"
#include "movement/TargetedMovementGenerator.h"
#include "movement/WaypointMovementGenerator.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Opcodes.h"
#include "PathFinder.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SkillDiscovery.h"
#include "SkillExtraItems.h"
#include "SpecialVisCreature.h"
#include "Totem.h"
#include "TemporarySummon.h"
#include "SpellMgr.h"
#include "SystemConfig.h"
#include "Util.h"
#include "Weather.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "ban_wave.h"
#include "Config/Config.h"
#include "Database/DatabaseEnv.h"
#include "maps/visitors.h"
#include <chrono>

// reload commands
bool ChatHandler::HandleReloadAllCommand(char* /*args*/)
{
    HandleReloadSkillFishingBaseLevelCommand((char*)"");

    HandleReloadAllAreaCommand((char*)"");
    HandleReloadAllEventAICommand((char*)"");
    HandleReloadAllLootCommand((char*)"");
    HandleReloadAllNpcCommand((char*)"");
    HandleReloadAllQuestCommand((char*)"");
    HandleReloadAllSpellCommand((char*)"");
    HandleReloadAllItemCommand((char*)"");
    HandleReloadAllGossipsCommand((char*)"");
    HandleReloadAllLocalesCommand((char*)"");

    HandleReloadMailLevelRewardCommand((char*)"");
    HandleReloadCommandCommand((char*)"");
    HandleReloadReservedNameCommand((char*)"");
    HandleReloadMangosStringCommand((char*)"");
    HandleReloadGameTeleCommand((char*)"");
    HandleReloadBattleEventCommand((char*)"");
    return true;
}

bool ChatHandler::HandleReloadAllAreaCommand(char* /*args*/)
{
    // HandleReloadQuestAreaTriggersCommand((char*)""); -- reloaded in
    // HandleReloadAllQuestCommand
    HandleReloadAreaTriggerTeleportCommand((char*)"");
    HandleReloadAreaTriggerTavernCommand((char*)"");
    HandleReloadGameGraveyardZoneCommand((char*)"");
    return true;
}

bool ChatHandler::HandleReloadAllLootCommand(char* /*args*/)
{
    logging.info("Re-Loading Loot Tables...");
    LoadLootTables();
    SendGlobalSysMessage("DB tables `*_loot_template` reloaded.");
    return true;
}

bool ChatHandler::HandleReloadAllNpcCommand(char* args)
{
    if (*args != 'a') // will be reloaded from all_gossips
        HandleReloadNpcGossipCommand((char*)"a");
    HandleReloadNpcTrainerCommand((char*)"a");
    HandleReloadNpcVendorCommand((char*)"a");
    HandleReloadPointsOfInterestCommand((char*)"a");
    return true;
}

bool ChatHandler::HandleReloadAllQuestCommand(char* /*args*/)
{
    HandleReloadQuestAreaTriggersCommand((char*)"a");
    HandleReloadQuestTemplateCommand((char*)"a");

    logging.info("Re-Loading Quests Relations...");
    sObjectMgr::Instance()->LoadQuestRelations();
    SendGlobalSysMessage(
        "DB tables `*_questrelation` and `*_involvedrelation` reloaded.");
    return true;
}

bool ChatHandler::HandleReloadAllScriptsCommand(char* /*args*/)
{
    if (sScriptMgr::Instance()->IsScriptScheduled())
    {
        PSendSysMessage(
            "DB scripts used currently, please attempt reload later.");
        SetSentErrorMessage(true);
        return false;
    }

    logging.info("Re-Loading Scripts...");
    HandleReloadGameObjectScriptsCommand((char*)"a");
    HandleReloadGossipScriptsCommand((char*)"a");
    HandleReloadEventScriptsCommand((char*)"a");
    HandleReloadQuestEndScriptsCommand((char*)"a");
    HandleReloadQuestStartScriptsCommand((char*)"a");
    HandleReloadSpellScriptsCommand((char*)"a");
    SendGlobalSysMessage("DB tables `*_scripts` reloaded.");
    HandleReloadDbScriptStringCommand((char*)"a");
    return true;
}

bool ChatHandler::HandleReloadAllEventAICommand(char* /*args*/)
{
    HandleReloadEventAITextsCommand((char*)"a");
    HandleReloadEventAISummonsCommand((char*)"a");
    HandleReloadEventAIScriptsCommand((char*)"a");
    return true;
}

bool ChatHandler::HandleReloadAllSpellCommand(char* /*args*/)
{
    HandleReloadSkillDiscoveryTemplateCommand((char*)"a");
    HandleReloadSkillExtraItemTemplateCommand((char*)"a");
    HandleReloadSpellAffectCommand((char*)"a");
    HandleReloadSpellAreaCommand((char*)"a");
    HandleReloadSpellChainCommand((char*)"a");
    HandleReloadSpellElixirCommand((char*)"a");
    HandleReloadSpellLearnSpellCommand((char*)"a");
    HandleReloadSpellProcEventCommand((char*)"a");
    HandleReloadSpellBonusesCommand((char*)"a");
    HandleReloadSpellProcItemEnchantCommand((char*)"a");
    HandleReloadSpellScriptTargetCommand((char*)"a");
    HandleReloadSpellTargetPositionCommand((char*)"a");
    HandleReloadSpellThreatsCommand((char*)"a");
    HandleReloadSpellPetAurasCommand((char*)"a");
    return true;
}

bool ChatHandler::HandleReloadAllGossipsCommand(char* args)
{
    if (*args != 'a') // already reload from all_scripts
        HandleReloadGossipScriptsCommand((char*)"a");
    HandleReloadGossipMenuCommand((char*)"a");
    HandleReloadNpcGossipCommand((char*)"a");
    HandleReloadPointsOfInterestCommand((char*)"a");
    return true;
}

bool ChatHandler::HandleReloadAllItemCommand(char* /*args*/)
{
    HandleReloadPageTextsCommand((char*)"a");
    HandleReloadItemEnchantementsCommand((char*)"a");
    HandleReloadItemRequiredTragetCommand((char*)"a");
    return true;
}

bool ChatHandler::HandleReloadAllLocalesCommand(char* /*args*/)
{
    HandleReloadLocalesCreatureCommand((char*)"a");
    HandleReloadLocalesGameobjectCommand((char*)"a");
    HandleReloadLocalesGossipMenuOptionCommand((char*)"a");
    HandleReloadLocalesItemCommand((char*)"a");
    HandleReloadLocalesNpcTextCommand((char*)"a");
    HandleReloadLocalesPageTextCommand((char*)"a");
    HandleReloadLocalesPointsOfInterestCommand((char*)"a");
    HandleReloadLocalesQuestCommand((char*)"a");
    return true;
}

bool ChatHandler::HandleReloadConfigCommand(char* /*args*/)
{
    logging.info("Re-Loading config settings...");
    sWorld::Instance()->LoadConfigSettings(true);
    sMapMgr::Instance()->InitializeVisibilityDistanceInfo();
    SendGlobalSysMessage("World config settings reloaded.");
    return true;
}

bool ChatHandler::HandleReloadAreaTriggerTavernCommand(char* /*args*/)
{
    logging.info("Re-Loading Tavern Area Triggers...");
    sObjectMgr::Instance()->LoadTavernAreaTriggers();
    SendGlobalSysMessage("DB table `areatrigger_tavern` reloaded.");
    return true;
}

bool ChatHandler::HandleReloadAreaTriggerTeleportCommand(char* /*args*/)
{
    logging.info("Re-Loading AreaTrigger teleport definitions...");
    sObjectMgr::Instance()->LoadAreaTriggerTeleports();
    SendGlobalSysMessage("DB table `areatrigger_teleport` reloaded.");
    return true;
}

bool ChatHandler::HandleReloadCommandCommand(char* /*args*/)
{
    load_command_table = true;
    SendGlobalSysMessage(
        "DB table `command` will be reloaded at next chat command use.");
    return true;
}

bool ChatHandler::HandleReloadCreatureQuestRelationsCommand(char* /*args*/)
{
    logging.info("Loading Quests Relations... (`creature_questrelation`)");
    sObjectMgr::Instance()->LoadCreatureQuestRelations();
    SendGlobalSysMessage(
        "DB table `creature_questrelation` (creature quest givers) reloaded.");
    return true;
}

bool ChatHandler::HandleReloadCreatureQuestInvRelationsCommand(char* /*args*/)
{
    logging.info("Loading Quests Relations... (`creature_involvedrelation`)");
    sObjectMgr::Instance()->LoadCreatureInvolvedRelations();
    SendGlobalSysMessage(
        "DB table `creature_involvedrelation` (creature quest takers) "
        "reloaded.");
    return true;
}

bool ChatHandler::HandleReloadConditionsCommand(char* /*args*/)
{
    logging.info("Re-Loading `conditions`... ");
    sObjectMgr::Instance()->LoadConditions();
    SendGlobalSysMessage("DB table `conditions` reloaded.");
    return true;
}

bool ChatHandler::HandleReloadGossipMenuCommand(char* /*args*/)
{
    sObjectMgr::Instance()->LoadGossipMenus();
    SendGlobalSysMessage(
        "DB tables `gossip_menu` and `gossip_menu_option` reloaded.");
    return true;
}

bool ChatHandler::HandleReloadGossipScriptsCommand(char* args)
{
    if (sScriptMgr::Instance()->IsScriptScheduled())
    {
        SendSysMessage(
            "DB scripts used currently, please attempt reload later.");
        SetSentErrorMessage(true);
        return false;
    }

    if (*args != 'a')
        logging.info("Re-Loading Scripts from `gossip_scripts`...");

    sScriptMgr::Instance()->LoadGossipScripts();

    if (*args != 'a')
        SendGlobalSysMessage("DB table `gossip_scripts` reloaded.");

    return true;
}

bool ChatHandler::HandleReloadGOQuestRelationsCommand(char* /*args*/)
{
    logging.info("Loading Quests Relations... (`gameobject_questrelation`)");
    sObjectMgr::Instance()->LoadGameobjectQuestRelations();
    SendGlobalSysMessage(
        "DB table `gameobject_questrelation` (gameobject quest givers) "
        "reloaded.");
    return true;
}

bool ChatHandler::HandleReloadGOQuestInvRelationsCommand(char* /*args*/)
{
    logging.info("Loading Quests Relations... (`gameobject_involvedrelation`)");
    sObjectMgr::Instance()->LoadGameobjectInvolvedRelations();
    SendGlobalSysMessage(
        "DB table `gameobject_involvedrelation` (gameobject quest takers) "
        "reloaded.");
    return true;
}

bool ChatHandler::HandleReloadQuestAreaTriggersCommand(char* /*args*/)
{
    logging.info("Re-Loading Quest Area Triggers...");
    sObjectMgr::Instance()->LoadQuestAreaTriggers();
    SendGlobalSysMessage(
        "DB table `areatrigger_involvedrelation` (quest area triggers) "
        "reloaded.");
    return true;
}

bool ChatHandler::HandleReloadQuestTemplateCommand(char* /*args*/)
{
    logging.info("Re-Loading Quest Templates...");
    sObjectMgr::Instance()->LoadQuests();
    SendGlobalSysMessage(
        "DB table `quest_template` (quest definitions) reloaded.");

    /// dependent also from `gameobject` but this table not reloaded anyway
    logging.info("Re-Loading GameObjects for quests...");
    sObjectMgr::Instance()->LoadGameObjectForQuests();
    SendGlobalSysMessage("Data GameObjects for quests reloaded.");
    return true;
}

bool ChatHandler::HandleReloadLootTemplatesCreatureCommand(char* /*args*/)
{
    logging.info("Re-Loading Loot Tables... (`creature_loot_template`)");
    LoadLootTemplates_Creature();
    LootTemplates_Creature.CheckLootRefs();
    SendGlobalSysMessage("DB table `creature_loot_template` reloaded.");
    return true;
}

bool ChatHandler::HandleReloadLootTemplatesDisenchantCommand(char* /*args*/)
{
    logging.info("Re-Loading Loot Tables... (`disenchant_loot_template`)");
    LoadLootTemplates_Disenchant();
    LootTemplates_Disenchant.CheckLootRefs();
    SendGlobalSysMessage("DB table `disenchant_loot_template` reloaded.");
    return true;
}

bool ChatHandler::HandleReloadLootTemplatesFishingCommand(char* /*args*/)
{
    logging.info("Re-Loading Loot Tables... (`fishing_loot_template`)");
    LoadLootTemplates_Fishing();
    LootTemplates_Fishing.CheckLootRefs();
    SendGlobalSysMessage("DB table `fishing_loot_template` reloaded.");
    return true;
}

bool ChatHandler::HandleReloadLootTemplatesGameobjectCommand(char* /*args*/)
{
    logging.info("Re-Loading Loot Tables... (`gameobject_loot_template`)");
    LoadLootTemplates_Gameobject();
    LootTemplates_Gameobject.CheckLootRefs();
    SendGlobalSysMessage("DB table `gameobject_loot_template` reloaded.");
    return true;
}

bool ChatHandler::HandleReloadLootTemplatesItemCommand(char* /*args*/)
{
    logging.info("Re-Loading Loot Tables... (`item_loot_template`)");
    LoadLootTemplates_Item();
    LootTemplates_Item.CheckLootRefs();
    SendGlobalSysMessage("DB table `item_loot_template` reloaded.");
    return true;
}

bool ChatHandler::HandleReloadLootTemplatesPickpocketingCommand(char* /*args*/)
{
    logging.info("Re-Loading Loot Tables... (`pickpocketing_loot_template`)");
    LoadLootTemplates_Pickpocketing();
    LootTemplates_Pickpocketing.CheckLootRefs();
    SendGlobalSysMessage("DB table `pickpocketing_loot_template` reloaded.");
    return true;
}

bool ChatHandler::HandleReloadLootTemplatesProspectingCommand(char* /*args*/)
{
    logging.info("Re-Loading Loot Tables... (`prospecting_loot_template`)");
    LoadLootTemplates_Prospecting();
    LootTemplates_Prospecting.CheckLootRefs();
    SendGlobalSysMessage("DB table `prospecting_loot_template` reloaded.");
    return true;
}

bool ChatHandler::HandleReloadLootTemplatesMailCommand(char* /*args*/)
{
    logging.info("Re-Loading Loot Tables... (`mail_loot_template`)");
    LoadLootTemplates_Mail();
    LootTemplates_Mail.CheckLootRefs();
    SendGlobalSysMessage("DB table `mail_loot_template` reloaded.");
    return true;
}

bool ChatHandler::HandleReloadLootTemplatesReferenceCommand(char* /*args*/)
{
    logging.info("Re-Loading Loot Tables... (`reference_loot_template`)");
    LoadLootTemplates_Reference();
    SendGlobalSysMessage("DB table `reference_loot_template` reloaded.");
    return true;
}

bool ChatHandler::HandleReloadLootTemplatesSkinningCommand(char* /*args*/)
{
    logging.info("Re-Loading Loot Tables... (`skinning_loot_template`)");
    LoadLootTemplates_Skinning();
    LootTemplates_Skinning.CheckLootRefs();
    SendGlobalSysMessage("DB table `skinning_loot_template` reloaded.");
    return true;
}

bool ChatHandler::HandleReloadMangosStringCommand(char* /*args*/)
{
    logging.info("Re-Loading mangos_string Table!");
    sObjectMgr::Instance()->LoadMangosStrings();
    SendGlobalSysMessage("DB table `mangos_string` reloaded.");
    return true;
}

bool ChatHandler::HandleReloadNpcGossipCommand(char* /*args*/)
{
    logging.info("Re-Loading `npc_gossip` Table!");
    sObjectMgr::Instance()->LoadNpcGossips();
    SendGlobalSysMessage("DB table `npc_gossip` reloaded.");
    return true;
}

bool ChatHandler::HandleReloadNpcTextCommand(char* /*args*/)
{
    logging.info("Re-Loading `npc_text` Table!");
    sObjectMgr::Instance()->LoadGossipText();
    SendGlobalSysMessage("DB table `npc_text` reloaded.");
    return true;
}

bool ChatHandler::HandleReloadNpcTrainerCommand(char* /*args*/)
{
    logging.info("Re-Loading `npc_trainer_template` Table!");
    sObjectMgr::Instance()->LoadTrainerTemplates();
    SendGlobalSysMessage("DB table `npc_trainer_template` reloaded.");

    logging.info("Re-Loading `npc_trainer` Table!");
    sObjectMgr::Instance()->LoadTrainers();
    SendGlobalSysMessage("DB table `npc_trainer` reloaded.");
    return true;
}

bool ChatHandler::HandleReloadNpcVendorCommand(char* /*args*/)
{
    // not safe reload vendor template tables independent...
    logging.info("Re-Loading `npc_vendor_template` Table!");
    sObjectMgr::Instance()->LoadVendorTemplates();
    SendGlobalSysMessage("DB table `npc_vendor_template` reloaded.");

    logging.info("Re-Loading `npc_vendor` Table!");
    sObjectMgr::Instance()->LoadVendors();
    SendGlobalSysMessage("DB table `npc_vendor` reloaded.");
    return true;
}

bool ChatHandler::HandleReloadPointsOfInterestCommand(char* /*args*/)
{
    logging.info("Re-Loading `points_of_interest` Table!");
    sObjectMgr::Instance()->LoadPointsOfInterest();
    SendGlobalSysMessage("DB table `points_of_interest` reloaded.");
    return true;
}

bool ChatHandler::HandleReloadReservedNameCommand(char* /*args*/)
{
    logging.info("Loading ReservedNames... (`reserved_name`)");
    sObjectMgr::Instance()->LoadReservedPlayersNames();
    SendGlobalSysMessage(
        "DB table `reserved_name` (player reserved names) reloaded.");
    return true;
}

bool ChatHandler::HandleReloadReputationRewardRateCommand(char* /*args*/)
{
    logging.info("Re-Loading `reputation_reward_rate` Table!");
    sObjectMgr::Instance()->LoadReputationRewardRate();
    SendGlobalSysMessage("DB table `reputation_reward_rate` reloaded.");
    return true;
}

bool ChatHandler::HandleReloadReputationSpilloverTemplateCommand(char* /*args*/)
{
    logging.info("Re-Loading `reputation_spillover_template` Table!");
    sObjectMgr::Instance()->LoadReputationSpilloverTemplate();
    SendGlobalSysMessage("DB table `reputation_spillover_template` reloaded.");
    return true;
}

bool ChatHandler::HandleReloadSkillDiscoveryTemplateCommand(char* /*args*/)
{
    logging.info("Re-Loading Skill Discovery Table...");
    LoadSkillDiscoveryTable();
    SendGlobalSysMessage(
        "DB table `skill_discovery_template` (recipes discovered at crafting) "
        "reloaded.");
    return true;
}

bool ChatHandler::HandleReloadSkillExtraItemTemplateCommand(char* /*args*/)
{
    logging.info("Re-Loading Skill Extra Item Table...");
    LoadSkillExtraItemTable();
    SendGlobalSysMessage(
        "DB table `skill_extra_item_template` (extra item creation when "
        "crafting) reloaded.");
    return true;
}

bool ChatHandler::HandleReloadSkillFishingBaseLevelCommand(char* /*args*/)
{
    logging.info("Re-Loading Skill Fishing base level requirements...");
    sObjectMgr::Instance()->LoadFishingBaseSkillLevel();
    SendGlobalSysMessage(
        "DB table `skill_fishing_base_level` (fishing base level for "
        "zone/subzone) reloaded.");
    return true;
}

bool ChatHandler::HandleReloadSpellAffectCommand(char* /*args*/)
{
    logging.info("Re-Loading SpellAffect definitions...");
    sSpellMgr::Instance()->LoadSpellAffects();
    SendGlobalSysMessage(
        "DB table `spell_affect` (spell mods apply requirements) reloaded.");
    return true;
}

bool ChatHandler::HandleReloadSpellAreaCommand(char* /*args*/)
{
    logging.info("Re-Loading SpellArea Data...");
    sSpellMgr::Instance()->LoadSpellAreas();
    SendGlobalSysMessage(
        "DB table `spell_area` (spell dependences from area/quest/auras state) "
        "reloaded.");
    return true;
}

bool ChatHandler::HandleReloadSpellBonusesCommand(char* /*args*/)
{
    logging.info("Re-Loading Spell Bonus Data...");
    sSpellMgr::Instance()->LoadSpellBonuses();
    SendGlobalSysMessage(
        "DB table `spell_bonus_data` (spell damage/healing coefficients) "
        "reloaded.");
    return true;
}

bool ChatHandler::HandleReloadSpellChainCommand(char* /*args*/)
{
    logging.info("Re-Loading Spell Chain Data... ");
    sSpellMgr::Instance()->LoadSpellChains();
    SendGlobalSysMessage("DB table `spell_chain` (spell ranks) reloaded.");
    return true;
}

bool ChatHandler::HandleReloadSpellElixirCommand(char* /*args*/)
{
    logging.info("Re-Loading Spell Elixir types...");
    sSpellMgr::Instance()->LoadSpellElixirs();
    SendGlobalSysMessage(
        "DB table `spell_elixir` (spell elixir types) reloaded.");
    return true;
}

bool ChatHandler::HandleReloadSpellLearnSpellCommand(char* /*args*/)
{
    logging.info("Re-Loading Spell Learn Spells...");
    sSpellMgr::Instance()->LoadSpellLearnSpells();
    SendGlobalSysMessage("DB table `spell_learn_spell` reloaded.");
    return true;
}

bool ChatHandler::HandleReloadSpellProcEventCommand(char* /*args*/)
{
    logging.info("Re-Loading Spell Proc Event conditions...");
    sSpellMgr::Instance()->LoadSpellProcEvents();
    SendGlobalSysMessage(
        "DB table `spell_proc_event` (spell proc trigger requirements) "
        "reloaded.");
    return true;
}

bool ChatHandler::HandleReloadSpellProcItemEnchantCommand(char* /*args*/)
{
    logging.info("Re-Loading Spell Proc Item Enchant...");
    sSpellMgr::Instance()->LoadSpellProcItemEnchant();
    SendGlobalSysMessage(
        "DB table `spell_proc_item_enchant` (item enchantment ppm) reloaded.");
    return true;
}

bool ChatHandler::HandleReloadSpellScriptTargetCommand(char* /*args*/)
{
    logging.info("Re-Loading SpellsScriptTarget...");
    sSpellMgr::Instance()->LoadSpellScriptTarget();
    SendGlobalSysMessage(
        "DB table `spell_script_target` (spell targets selection in case "
        "specific creature/GO requirements) reloaded.");
    return true;
}

bool ChatHandler::HandleReloadSpellTargetPositionCommand(char* /*args*/)
{
    logging.info("Re-Loading spell target destination coordinates...");
    sSpellMgr::Instance()->LoadSpellTargetPositions();
    SendGlobalSysMessage(
        "DB table `spell_target_position` (destination coordinates for spell "
        "targets) reloaded.");
    return true;
}

bool ChatHandler::HandleReloadSpellThreatsCommand(char* /*args*/)
{
    logging.info("Re-Loading Aggro Spells Definitions...");
    sSpellMgr::Instance()->LoadSpellThreats();
    SendGlobalSysMessage(
        "DB table `spell_threat` (spell aggro definitions) reloaded.");
    return true;
}

bool ChatHandler::HandleReloadSpellPetAurasCommand(char* /*args*/)
{
    logging.info("Re-Loading Spell pet auras...");
    sSpellMgr::Instance()->LoadSpellPetAuras();
    SendGlobalSysMessage("DB table `spell_pet_auras` reloaded.");
    return true;
}

bool ChatHandler::HandleReloadPageTextsCommand(char* /*args*/)
{
    logging.info("Re-Loading Page Texts...");
    sObjectMgr::Instance()->LoadPageTexts();
    SendGlobalSysMessage("DB table `page_texts` reloaded.");
    return true;
}

bool ChatHandler::HandleReloadItemEnchantementsCommand(char* /*args*/)
{
    logging.info("Re-Loading Item Random Enchantments Table...");
    LoadRandomEnchantmentsTable();
    SendGlobalSysMessage("DB table `item_enchantment_template` reloaded.");
    return true;
}

bool ChatHandler::HandleReloadItemRequiredTragetCommand(char* /*args*/)
{
    logging.info("Re-Loading Item Required Targets Table...");
    sObjectMgr::Instance()->LoadItemRequiredTarget();
    SendGlobalSysMessage("DB table `item_required_target` reloaded.");
    return true;
}

bool ChatHandler::HandleReloadBattleEventCommand(char* /*args*/)
{
    logging.info("Re-Loading BattleGround Eventindexes...");
    sBattleGroundMgr::Instance()->LoadBattleEventIndexes();
    SendGlobalSysMessage(
        "DB table `gameobject_battleground` and `creature_battleground` "
        "reloaded.");
    return true;
}

bool ChatHandler::HandleReloadGameObjectScriptsCommand(char* args)
{
    if (sScriptMgr::Instance()->IsScriptScheduled())
    {
        SendSysMessage(
            "DB scripts used currently, please attempt reload later.");
        SetSentErrorMessage(true);
        return false;
    }

    if (*args != 'a')
        logging.info("Re-Loading Scripts from `gameobject_scripts`...");

    sScriptMgr::Instance()->LoadGameObjectScripts();

    if (*args != 'a')
        SendGlobalSysMessage("DB table `gameobject_scripts` reloaded.");

    return true;
}

bool ChatHandler::HandleReloadEventScriptsCommand(char* args)
{
    if (sScriptMgr::Instance()->IsScriptScheduled())
    {
        SendSysMessage(
            "DB scripts used currently, please attempt reload later.");
        SetSentErrorMessage(true);
        return false;
    }

    if (*args != 'a')
        logging.info("Re-Loading Scripts from `event_scripts`...");

    sScriptMgr::Instance()->LoadEventScripts();

    if (*args != 'a')
        SendGlobalSysMessage("DB table `event_scripts` reloaded.");

    return true;
}

bool ChatHandler::HandleReloadEventAITextsCommand(char* /*args*/)
{
    logging.info("Re-Loading Texts from `creature_ai_texts`...");
    sEventAIMgr::Instance()->LoadCreatureEventAI_Texts(true);
    SendGlobalSysMessage("DB table `creature_ai_texts` reloaded.");
    return true;
}

bool ChatHandler::HandleReloadEventAISummonsCommand(char* /*args*/)
{
    logging.info("Re-Loading Summons from `creature_ai_summons`...");
    sEventAIMgr::Instance()->LoadCreatureEventAI_Summons(true);
    SendGlobalSysMessage("DB table `creature_ai_summons` reloaded.");
    return true;
}

bool ChatHandler::HandleReloadEventAIScriptsCommand(char* /*args*/)
{
    logging.info("Re-Loading Scripts from `creature_ai_scripts`...");
    sEventAIMgr::Instance()->LoadCreatureEventAI_Scripts();
    SendGlobalSysMessage("DB table `creature_ai_scripts` reloaded.");
    return true;
}

bool ChatHandler::HandleReloadQuestEndScriptsCommand(char* args)
{
    if (sScriptMgr::Instance()->IsScriptScheduled())
    {
        SendSysMessage(
            "DB scripts used currently, please attempt reload later.");
        SetSentErrorMessage(true);
        return false;
    }

    if (*args != 'a')
        logging.info("Re-Loading Scripts from `quest_end_scripts`...");

    sScriptMgr::Instance()->LoadQuestEndScripts();

    if (*args != 'a')
        SendGlobalSysMessage("DB table `quest_end_scripts` reloaded.");

    return true;
}

bool ChatHandler::HandleReloadQuestStartScriptsCommand(char* args)
{
    if (sScriptMgr::Instance()->IsScriptScheduled())
    {
        SendSysMessage(
            "DB scripts used currently, please attempt reload later.");
        SetSentErrorMessage(true);
        return false;
    }

    if (*args != 'a')
        logging.info("Re-Loading Scripts from `quest_start_scripts`...");

    sScriptMgr::Instance()->LoadQuestStartScripts();

    if (*args != 'a')
        SendGlobalSysMessage("DB table `quest_start_scripts` reloaded.");

    return true;
}

bool ChatHandler::HandleReloadSpellScriptsCommand(char* args)
{
    if (sScriptMgr::Instance()->IsScriptScheduled())
    {
        SendSysMessage(
            "DB scripts used currently, please attempt reload later.");
        SetSentErrorMessage(true);
        return false;
    }

    if (*args != 'a')
        logging.info("Re-Loading Scripts from `spell_scripts`...");

    sScriptMgr::Instance()->LoadSpellScripts();

    if (*args != 'a')
        SendGlobalSysMessage("DB table `spell_scripts` reloaded.");

    return true;
}

bool ChatHandler::HandleReloadDbScriptStringCommand(char* /*args*/)
{
    logging.info("Re-Loading Script strings from `db_script_string`...");
    sScriptMgr::Instance()->LoadDbScriptStrings();
    SendGlobalSysMessage("DB table `db_script_string` reloaded.");
    return true;
}

bool ChatHandler::HandleReloadGameGraveyardZoneCommand(char* /*args*/)
{
    logging.info("Re-Loading Graveyard-zone links...");

    sObjectMgr::Instance()->LoadGraveyardZones();

    SendGlobalSysMessage("DB table `game_graveyard_zone` reloaded.");

    return true;
}

bool ChatHandler::HandleReloadGameTeleCommand(char* /*args*/)
{
    logging.info("Re-Loading Game Tele coordinates...");

    sObjectMgr::Instance()->LoadGameTele();

    SendGlobalSysMessage("DB table `game_tele` reloaded.");

    return true;
}

bool ChatHandler::HandleReloadLocalesCreatureCommand(char* /*args*/)
{
    logging.info("Re-Loading Locales Creature ...");
    sObjectMgr::Instance()->LoadCreatureLocales();
    SendGlobalSysMessage("DB table `locales_creature` reloaded.");
    return true;
}

bool ChatHandler::HandleReloadLocalesGameobjectCommand(char* /*args*/)
{
    logging.info("Re-Loading Locales Gameobject ... ");
    sObjectMgr::Instance()->LoadGameObjectLocales();
    SendGlobalSysMessage("DB table `locales_gameobject` reloaded.");
    return true;
}

bool ChatHandler::HandleReloadLocalesGossipMenuOptionCommand(char* /*args*/)
{
    logging.info("Re-Loading Locales Gossip Menu Option ... ");
    sObjectMgr::Instance()->LoadGossipMenuItemsLocales();
    SendGlobalSysMessage("DB table `locales_gossip_menu_option` reloaded.");
    return true;
}

bool ChatHandler::HandleReloadLocalesItemCommand(char* /*args*/)
{
    logging.info("Re-Loading Locales Item ... ");
    sObjectMgr::Instance()->LoadItemLocales();
    SendGlobalSysMessage("DB table `locales_item` reloaded.");
    return true;
}

bool ChatHandler::HandleReloadLocalesNpcTextCommand(char* /*args*/)
{
    logging.info("Re-Loading Locales NPC Text ... ");
    sObjectMgr::Instance()->LoadGossipTextLocales();
    SendGlobalSysMessage("DB table `locales_npc_text` reloaded.");
    return true;
}

bool ChatHandler::HandleReloadLocalesPageTextCommand(char* /*args*/)
{
    logging.info("Re-Loading Locales Page Text ... ");
    sObjectMgr::Instance()->LoadPageTextLocales();
    SendGlobalSysMessage("DB table `locales_page_text` reloaded.");
    return true;
}

bool ChatHandler::HandleReloadLocalesPointsOfInterestCommand(char* /*args*/)
{
    logging.info("Re-Loading Locales Points Of Interest ... ");
    sObjectMgr::Instance()->LoadPointOfInterestLocales();
    SendGlobalSysMessage("DB table `locales_points_of_interest` reloaded.");
    return true;
}

bool ChatHandler::HandleReloadLocalesQuestCommand(char* /*args*/)
{
    logging.info("Re-Loading Locales Quest ... ");
    sObjectMgr::Instance()->LoadQuestLocales();
    SendGlobalSysMessage("DB table `locales_quest` reloaded.");
    return true;
}

bool ChatHandler::HandleReloadMailLevelRewardCommand(char* /*args*/)
{
    logging.info("Re-Loading Player level dependent mail rewards...");
    sObjectMgr::Instance()->LoadMailLevelRewards();
    SendGlobalSysMessage("DB table `mail_level_reward` reloaded.");
    return true;
}

bool ChatHandler::HandleLoadScriptsCommand(char* args)
{
    if (!*args)
        return false;

    switch (sScriptMgr::Instance()->LoadScriptLibrary(args))
    {
    case SCRIPT_LOAD_OK:
        sWorld::Instance()->SendWorldText(LANG_SCRIPTS_RELOADED_ANNOUNCE);
        SendSysMessage(LANG_SCRIPTS_RELOADED_OK);
        break;
    case SCRIPT_LOAD_ERR_NOT_FOUND:
        SendSysMessage(LANG_SCRIPTS_NOT_FOUND);
        break;
    case SCRIPT_LOAD_ERR_WRONG_API:
        SendSysMessage(LANG_SCRIPTS_WRONG_API);
        break;
    case SCRIPT_LOAD_ERR_OUTDATED:
        SendSysMessage(LANG_SCRIPTS_OUTDATED);
        break;
    }

    return true;
}

bool ChatHandler::HandleAccountSetGmLevelCommand(char* args)
{
    char* accountStr = ExtractOptNotLastArg(&args);

    std::string targetAccountName;
    Player* targetPlayer = nullptr;
    uint32 targetAccountId =
        ExtractAccountId(&accountStr, &targetAccountName, &targetPlayer);
    if (!targetAccountId)
        return false;

    /// only target player different from self allowed
    if (GetAccountId() == targetAccountId)
        return false;

    int32 gm;
    if (!ExtractInt32(&args, gm))
        return false;

    if (gm < SEC_PLAYER || gm > SEC_FULL_GM)
    {
        SendSysMessage(LANG_BAD_VALUE);
        SetSentErrorMessage(true);
        return false;
    }

    /// can set security level only for target with less security and to less
    /// security that we have
    /// This will reject self apply by specify account name
    if (HasLowerSecurityAccount(nullptr, targetAccountId, true))
        return false;

    /// account can't set security to same or grater level, need more power GM
    /// or console
    AccountTypes plSecurity = GetAccessLevel();
    if (AccountTypes(gm) >= plSecurity)
    {
        SendSysMessage(LANG_YOURS_SECURITY_IS_LOW);
        SetSentErrorMessage(true);
        return false;
    }

    if (targetPlayer)
    {
        ChatHandler(targetPlayer)
            .PSendSysMessage(
                LANG_YOURS_SECURITY_CHANGED, GetNameLink().c_str(), gm);
        targetPlayer->GetSession()->SetSecurity(AccountTypes(gm));
    }

    PSendSysMessage(LANG_YOU_CHANGE_SECURITY, targetAccountName.c_str(), gm);
    LoginDatabase.PExecute("UPDATE account SET gmlevel = '%i' WHERE id = '%u'",
        gm, targetAccountId);

    return true;
}

/// Set password for account
bool ChatHandler::HandleAccountSetPasswordCommand(char* args)
{
    ///- Get the command line arguments
    std::string account_name;
    uint32 targetAccountId = ExtractAccountId(&args, &account_name);
    if (!targetAccountId)
        return false;

    // allow or quoted string with possible spaces or literal without spaces
    char* szPassword1 = ExtractQuotedOrLiteralArg(&args);
    char* szPassword2 = ExtractQuotedOrLiteralArg(&args);
    if (!szPassword1 || !szPassword2)
        return false;

    /// can set password only for target with less security
    /// This is also reject self apply in fact
    if (HasLowerSecurityAccount(nullptr, targetAccountId, true))
        return false;

    if (strcmp(szPassword1, szPassword2))
    {
        SendSysMessage(LANG_NEW_PASSWORDS_NOT_MATCH);
        SetSentErrorMessage(true);
        return false;
    }

    AccountOpResult result =
        sAccountMgr::Instance()->ChangePassword(targetAccountId, szPassword1);

    switch (result)
    {
    case AOR_OK:
        SendSysMessage(LANG_COMMAND_PASSWORD);
        break;
    case AOR_NAME_NOT_EXIST:
        PSendSysMessage(LANG_ACCOUNT_NOT_EXIST, account_name.c_str());
        SetSentErrorMessage(true);
        return false;
    case AOR_PASS_TOO_LONG:
        SendSysMessage(LANG_PASSWORD_TOO_LONG);
        SetSentErrorMessage(true);
        return false;
    default:
        SendSysMessage(LANG_COMMAND_NOTCHANGEPASSWORD);
        SetSentErrorMessage(true);
        return false;
    }

    // OK, but avoid normal report for hide passwords, but log use command for
    // anyone
    char msg[100];
    snprintf(
        msg, 100, ".account set password %s *** ***", account_name.c_str());
    LogCommand(msg);
    SetSentErrorMessage(true);
    return false;
}

bool ChatHandler::HandleMaxSkillCommand(char* /*args*/)
{
    Player* SelectedPlayer = getSelectedPlayer();
    if (!SelectedPlayer)
    {
        SendSysMessage(LANG_NO_CHAR_SELECTED);
        SetSentErrorMessage(true);
        return false;
    }

    // each skills that have max skill value dependent from level seted to
    // current level max skill value
    SelectedPlayer->UpdateSkillsToMaxSkillsForLevel();
    return true;
}

bool ChatHandler::HandleMaxProfCommand(char* /*args*/)
{
    static const int MAX_SKILL_VAL = 375;
    static const int skill_ids[] = {
        // Secondary
        129, 185, 356,
        // Primary, Gathering
        182, 186, 393,
        // Primary, Crafting
        171, 164, 333, 202, 755, 165, 197,
        // Lockpicking
        633,
    };

    Player* target = getSelectedPlayer();
    if (!target)
    {
        SendSysMessage(LANG_NO_CHAR_SELECTED);
        SetSentErrorMessage(true);
        return false;
    }

    for (auto skill_id : skill_ids)
    {
        if (target && target->HasSkill(skill_id))
            target->SetSkill(skill_id, MAX_SKILL_VAL, MAX_SKILL_VAL);
    }

    return true;
}

bool ChatHandler::HandleSetSkillCommand(char* args)
{
    Player* target = getSelectedPlayer();
    if (!target)
    {
        SendSysMessage(LANG_NO_CHAR_SELECTED);
        SetSentErrorMessage(true);
        return false;
    }

    // number or [name] Shift-click form |color|Hskill:skill_id|h[name]|h|r
    char* skill_p = ExtractKeyFromLink(&args, "Hskill");
    if (!skill_p)
        return false;

    int32 skill;
    if (!ExtractInt32(&skill_p, skill))
        return false;

    int32 level;
    if (!ExtractInt32(&args, level))
        return false;

    int32 maxskill;
    if (!ExtractOptInt32(&args, maxskill, target->GetPureMaxSkillValue(skill)))
        return false;

    if (skill <= 0)
    {
        PSendSysMessage(LANG_INVALID_SKILL_ID, skill);
        SetSentErrorMessage(true);
        return false;
    }

    SkillLineEntry const* sl = sSkillLineStore.LookupEntry(skill);
    if (!sl)
    {
        PSendSysMessage(LANG_INVALID_SKILL_ID, skill);
        SetSentErrorMessage(true);
        return false;
    }

    std::string tNameLink = GetNameLink(target);

    if (!target->GetSkillValue(skill))
    {
        PSendSysMessage(LANG_SET_SKILL_ERROR, tNameLink.c_str(), skill,
            sl->name[GetSessionDbcLocale()]);
        SetSentErrorMessage(true);
        return false;
    }

    if (level <= 0 || level > maxskill || maxskill <= 0)
        return false;

    target->SetSkill(skill, level, maxskill);
    PSendSysMessage(LANG_SET_SKILL, skill, sl->name[GetSessionDbcLocale()],
        tNameLink.c_str(), level, maxskill);

    return true;
}

bool ChatHandler::HandleUnLearnCommand(char* args)
{
    if (!*args)
        return false;

    // number or [name] Shift-click form |color|Hspell:spell_id|h[name]|h|r
    uint32 spell_id = ExtractSpellIdFromLink(&args);
    if (!spell_id)
        return false;

    bool allRanks = ExtractLiteralArg(&args, "all") != nullptr;
    if (!allRanks && *args) // can be fail also at syntax error
        return false;

    Player* target = getSelectedPlayer();
    if (!target)
    {
        SendSysMessage(LANG_NO_CHAR_SELECTED);
        SetSentErrorMessage(true);
        return false;
    }

    if (allRanks)
        spell_id = sSpellMgr::Instance()->GetFirstSpellInChain(spell_id);

    if (target->HasSpell(spell_id))
        target->removeSpell(spell_id, false, !allRanks);
    else
        SendSysMessage(LANG_FORGET_SPELL);

    return true;
}

bool ChatHandler::HandleCooldownCommand(char* args)
{
    Player* target = getSelectedPlayer();
    if (!target)
    {
        SendSysMessage(LANG_PLAYER_NOT_FOUND);
        SetSentErrorMessage(true);
        return false;
    }

    std::string tNameLink = GetNameLink(target);

    if (!*args)
    {
        target->RemoveAllSpellCooldown();
        PSendSysMessage(LANG_REMOVEALL_COOLDOWN, tNameLink.c_str());
    }
    else
    {
        // number or [name] Shift-click form |color|Hspell:spell_id|h[name]|h|r
        // or Htalent form
        uint32 spell_id = ExtractSpellIdFromLink(&args);
        if (!spell_id)
            return false;

        if (!sSpellStore.LookupEntry(spell_id))
        {
            PSendSysMessage(LANG_UNKNOWN_SPELL,
                target == m_session->GetPlayer() ? GetMangosString(LANG_YOU) :
                                                   tNameLink.c_str());
            SetSentErrorMessage(true);
            return false;
        }

        target->RemoveSpellCooldown(spell_id, true);
        PSendSysMessage(LANG_REMOVE_COOLDOWN, spell_id,
            target == m_session->GetPlayer() ? GetMangosString(LANG_YOU) :
                                               tNameLink.c_str());
    }
    return true;
}

bool ChatHandler::HandleLearnAllCommand(char* /*args*/)
{
    static const char* allSpellList[] = {"3365", "6233", "6247", "6246", "6477",
        "6478", "22810", "8386", "21651", "21652", "522", "7266", "8597",
        "2479", "22027", "6603", "5019", "133", "168", "227", "5009", "9078",
        "668", "203", "20599", "20600", "81", "20597", "20598", "20864", "1459",
        "5504", "587", "5143", "118", "5505", "597", "604", "1449", "1460",
        "2855", "1008", "475", "5506", "1463", "12824", "8437", "990", "5145",
        "8450", "1461", "759", "8494", "8455", "8438", "6127", "8416", "6129",
        "8451", "8495", "8439", "3552", "8417", "10138", "12825", "10169",
        "10156", "10144", "10191", "10201", "10211", "10053", "10173", "10139",
        "10145", "10192", "10170", "10202", "10054", "10174", "10193", "12826",
        "2136", "143", "145", "2137", "2120", "3140", "543", "2138", "2948",
        "8400", "2121", "8444", "8412", "8457", "8401", "8422", "8445", "8402",
        "8413", "8458", "8423", "8446", "10148", "10197", "10205", "10149",
        "10215", "10223", "10206", "10199", "10150", "10216", "10207", "10225",
        "10151", "116", "205", "7300", "122", "837", "10", "7301", "7322",
        "6143", "120", "865", "8406", "6141", "7302", "8461", "8407", "8492",
        "8427", "8408", "6131", "7320", "10159", "8462", "10185", "10179",
        "10160", "10180", "10219", "10186", "10177", "10230", "10181", "10161",
        "10187", "10220", "2018", "2663", "12260", "2660", "3115", "3326",
        "2665", "3116", "2738", "3293", "2661", "3319", "2662", "9983", "8880",
        "2737", "2739", "7408", "3320", "2666", "3323", "3324", "3294", "22723",
        "23219", "23220", "23221", "23228", "23338", "10788", "10790", "5611",
        "5016", "5609", "2060", "10963", "10964", "10965", "22593", "22594",
        "596", "996", "499", "768", "17002", "1448", "1082", "16979", "1079",
        "5215", "20484", "5221", "15590", "17007", "6795", "6807", "5487",
        "1446", "1066", "5421", "3139", "779", "6811", "6808", "1445", "5216",
        "1737", "5222", "5217", "1432", "6812", "9492", "5210", "3030", "1441",
        "783", "6801", "20739", "8944", "9491", "22569", "5226", "6786", "1433",
        "8973", "1828", "9495", "9006", "6794", "8993", "5203", "16914", "6784",
        "9635", "22830", "20722", "9748", "6790", "9753", "9493", "9752",
        "9831", "9825", "9822", "5204", "5401", "22831", "6793", "9845",
        "17401", "9882", "9868", "20749", "9893", "9899", "9895", "9832",
        "9902", "9909", "22832", "9828", "9851", "9883", "9869", "17406",
        "17402", "9914", "20750", "9897", "9848", "3127", "107", "204", "9116",
        "2457", "78", "18848", "331", "403", "2098", "1752", "11278", "11288",
        "11284", "6461", "2344", "2345", "6463", "2346", "2352", "775", "1434",
        "1612", "71", "2468", "2458", "2467", "7164", "7178", "7367", "7376",
        "7381", "21156", "5209", "3029", "5201", "9849", "9850", "20719",
        "22568", "22827", "22828", "22829", "6809", "8972", "9005", "9823",
        "9827", "6783", "9913", "6785", "6787", "9866", "9867", "9894", "9896",
        "6800", "8992", "9829", "9830", "780", "769", "6749", "6750", "9755",
        "9754", "9908", "20745", "20742", "20747", "20748", "9746", "9745",
        "9880", "9881", "5391", "842", "3025", "3031", "3287", "3329", "1945",
        "3559", "4933", "4934", "4935", "4936", "5142", "5390", "5392", "5404",
        "5420", "6405", "7293", "7965", "8041", "8153", "9033", "9034",
        //"9036", problems with ghost state
        "16421", "21653", "22660", "5225", "9846", "2426", "5916", "6634",
        //"6718", phasing stealth, annoying for learn all case.
        "6719", "8822", "9591", "9590", "10032", "17746", "17747", "8203",
        "11392", "12495", "16380", "23452", "4079", "4996", "4997", "4998",
        "4999", "5000", "6348", "6349", "6481", "6482", "6483", "6484", "11362",
        "11410", "11409", "12510", "12509", "12885", "13142", "21463", "23460",
        "11421", "11416", "11418", "1851", "10059", "11423", "11417", "11422",
        "11419", "11424", "11420", "27", "31", "33", "34", "35", "15125",
        "21127", "22950", "1180", "201", "12593", "12842", "16770", "6057",
        "12051", "18468", "12606", "12605", "18466", "12502", "12043", "15060",
        "12042", "12341", "12848", "12344", "12353", "18460", "11366", "12350",
        "12352", "13043", "11368", "11113", "12400", "11129", "16766", "12573",
        "15053", "12580", "12475", "12472", "12953", "12488", "11189", "12985",
        "12519", "16758", "11958", "12490", "11426", "3565", "3562", "18960",
        "3567", "3561", "3566", "3563", "1953", "2139", "12505", "13018",
        "12522", "12523", "5146", "5144", "5148", "8419", "8418", "10213",
        "10212", "10157", "12524", "13019", "12525", "13020", "12526", "13021",
        "18809", "13031", "13032", "13033", "4036", "3920", "3919", "3918",
        "7430", "3922", "3923", "7411", "7418", "7421", "13262", "7412", "7415",
        "7413", "7416", "13920", "13921", "7745", "7779", "7428", "7457",
        "7857", "7748", "7426", "13421", "7454", "13378", "7788", "14807",
        "14293", "7795", "6296", "20608", "755", "444", "427", "428", "442",
        "447", "3578", "3581", "19027", "3580", "665", "3579", "3577", "6755",
        "3576", "2575", "2577", "2578", "2579", "2580", "2656", "2657", "2576",
        "3564", "10248", "8388", "2659", "14891", "3308", "3307", "10097",
        "2658", "3569", "16153", "3304", "10098", "4037", "3929", "3931",
        "3926", "3924", "3930", "3977", "3925", "136", "228", "5487", "43",
        "202", "0"};

    int loop = 0;
    while (strcmp(allSpellList[loop], "0"))
    {
        uint32 spell = atol((char*)allSpellList[loop++]);

        if (m_session->GetPlayer()->HasSpell(spell))
            continue;

        SpellEntry const* spellInfo = sSpellStore.LookupEntry(spell);
        if (!spellInfo ||
            !SpellMgr::IsSpellValid(spellInfo, m_session->GetPlayer()))
        {
            PSendSysMessage(LANG_COMMAND_SPELL_BROKEN, spell);
            continue;
        }

        m_session->GetPlayer()->learnSpell(spell, false);
    }

    SendSysMessage(LANG_COMMAND_LEARN_MANY_SPELLS);

    return true;
}

bool ChatHandler::HandleLearnAllGMCommand(char* /*args*/)
{
    static const char* gmSpellList[] = {"24347", // Become A Fish, No Breath Bar
        "35132",                                 // Visual Boom
        "38488",                                 // Attack 4000-8000 AOE
        "38795", // Attack 2000 AOE + Slow Down 90%
        "15712", // Attack 200
        "1852",  // GM Spell Silence
        "31899", // Kill
        "31924", // Kill
        "29878", // Kill My Self
        "26644", // More Kill

        "28550", // Invisible 24
        "23452", // Invisible + Target
        "0"};

    uint16 gmSpellIter = 0;
    while (strcmp(gmSpellList[gmSpellIter], "0"))
    {
        uint32 spell = atol((char*)gmSpellList[gmSpellIter++]);

        SpellEntry const* spellInfo = sSpellStore.LookupEntry(spell);
        if (!spellInfo ||
            !SpellMgr::IsSpellValid(spellInfo, m_session->GetPlayer()))
        {
            PSendSysMessage(LANG_COMMAND_SPELL_BROKEN, spell);
            continue;
        }

        m_session->GetPlayer()->learnSpell(spell, false);
    }

    SendSysMessage(LANG_LEARNING_GM_SKILLS);
    return true;
}

bool ChatHandler::HandleLearnAllMyClassCommand(char* /*args*/)
{
    HandleLearnAllMySpellsCommand((char*)"");
    HandleLearnAllMyTalentsCommand((char*)"");
    return true;
}

bool ChatHandler::HandleLearnAllMySpellsCommand(char* /*args*/)
{
    ChrClassesEntry const* clsEntry =
        sChrClassesStore.LookupEntry(m_session->GetPlayer()->getClass());
    if (!clsEntry)
        return true;
    uint32 family = clsEntry->spellfamily;

    for (uint32 i = 0; i < sSkillLineAbilityStore.GetNumRows(); ++i)
    {
        SkillLineAbilityEntry const* entry =
            sSkillLineAbilityStore.LookupEntry(i);
        if (!entry)
            continue;

        SpellEntry const* spellInfo = sSpellStore.LookupEntry(entry->spellId);
        if (!spellInfo)
            continue;

        // skip server-side/triggered spells
        if (spellInfo->spellLevel == 0)
            continue;

        // skip wrong class/race skills
        if (!m_session->GetPlayer()->IsSpellFitByClassAndRace(spellInfo->Id))
            continue;

        // skip other spell families
        if (spellInfo->SpellFamilyName != family)
            continue;

        // skip spells with first rank learned as talent (and all talents then
        // also)
        uint32 first_rank =
            sSpellMgr::Instance()->GetFirstSpellInChain(spellInfo->Id);
        if (GetTalentSpellCost(first_rank) > 0)
            continue;

        // skip broken spells
        if (!SpellMgr::IsSpellValid(spellInfo, m_session->GetPlayer(), false))
            continue;

        m_session->GetPlayer()->learnSpell(spellInfo->Id, false);
    }

    SendSysMessage(LANG_COMMAND_LEARN_CLASS_SPELLS);
    return true;
}

bool ChatHandler::HandleLearnAllMyTalentsCommand(char* /*args*/)
{
    Player* player = m_session->GetPlayer();
    uint32 classMask = player->getClassMask();

    for (uint32 i = 0; i < sTalentStore.GetNumRows(); ++i)
    {
        TalentEntry const* talentInfo = sTalentStore.LookupEntry(i);
        if (!talentInfo)
            continue;

        TalentTabEntry const* talentTabInfo =
            sTalentTabStore.LookupEntry(talentInfo->TalentTab);
        if (!talentTabInfo)
            continue;

        if ((classMask & talentTabInfo->ClassMask) == 0)
            continue;

        // search highest talent rank
        uint32 spellid = 0;

        for (int rank = MAX_TALENT_RANK - 1; rank >= 0; --rank)
        {
            if (talentInfo->RankID[rank] != 0)
            {
                spellid = talentInfo->RankID[rank];
                break;
            }
        }

        if (!spellid) // ??? none spells in talent
            continue;

        SpellEntry const* spellInfo = sSpellStore.LookupEntry(spellid);
        if (!spellInfo ||
            !SpellMgr::IsSpellValid(spellInfo, m_session->GetPlayer(), false))
            continue;

        // learn highest rank of talent and learn all non-talent spell ranks
        // (recursive by tree)
        player->learnSpellHighRank(spellid);
    }

    SendSysMessage(LANG_COMMAND_LEARN_CLASS_TALENTS);
    return true;
}

bool ChatHandler::HandleLearnAllLangCommand(char* /*args*/)
{
    // skipping UNIVERSAL language (0)
    for (int i = 1; i < LANGUAGES_COUNT; ++i)
        m_session->GetPlayer()->learnSpell(lang_description[i].spell_id, false);

    SendSysMessage(LANG_COMMAND_LEARN_ALL_LANG);
    return true;
}

bool ChatHandler::HandleLearnAllDefaultCommand(char* args)
{
    Player* target;
    if (!ExtractPlayerTarget(&args, &target))
        return false;

    target->learnDefaultSpells();
    target->learnQuestRewardedSpells();

    PSendSysMessage(
        LANG_COMMAND_LEARN_ALL_DEFAULT_AND_QUEST, GetNameLink(target).c_str());
    return true;
}

bool ChatHandler::HandleLearnCommand(char* args)
{
    Player* targetPlayer = getSelectedPlayer();

    if (!targetPlayer)
    {
        SendSysMessage(LANG_PLAYER_NOT_FOUND);
        SetSentErrorMessage(true);
        return false;
    }

    // number or [name] Shift-click form |color|Hspell:spell_id|h[name]|h|r or
    // Htalent form
    uint32 spell = ExtractSpellIdFromLink(&args);
    if (!spell || !sSpellStore.LookupEntry(spell))
        return false;

    bool allRanks = ExtractLiteralArg(&args, "all") != nullptr;
    if (!allRanks && *args) // can be fail also at syntax error
        return false;

    SpellEntry const* spellInfo = sSpellStore.LookupEntry(spell);
    if (!spellInfo ||
        !SpellMgr::IsSpellValid(spellInfo, m_session->GetPlayer()))
    {
        PSendSysMessage(LANG_COMMAND_SPELL_BROKEN, spell);
        SetSentErrorMessage(true);
        return false;
    }

    if (!allRanks && targetPlayer->HasSpell(spell))
    {
        if (targetPlayer == m_session->GetPlayer())
            SendSysMessage(LANG_YOU_KNOWN_SPELL);
        else
            PSendSysMessage(LANG_TARGET_KNOWN_SPELL, targetPlayer->GetName());
        SetSentErrorMessage(true);
        return false;
    }

    if (allRanks)
        targetPlayer->learnSpellHighRank(spell);
    else
        targetPlayer->learnSpell(spell, false);

    return true;
}

bool ChatHandler::HandleAddItemCommand(char* args)
{
    char* cId = ExtractKeyFromLink(&args, "Hitem");
    if (!cId)
        return false;

    uint32 itemId = 0;
    if (!ExtractUInt32(&cId, itemId)) // [name] manual form
    {
        std::string itemName = cId;
        WorldDatabase.escape_string(itemName);
        std::unique_ptr<QueryResult> result(WorldDatabase.PQuery(
            "SELECT entry FROM item_template WHERE name = '%s'",
            itemName.c_str()));
        if (!result)
        {
            PSendSysMessage(LANG_COMMAND_COULDNOTFIND, cId);
            SetSentErrorMessage(true);
            return false;
        }
        itemId = result->Fetch()->GetUInt16();
    }

    int32 count;
    if (!ExtractOptInt32(&args, count, 1))
        return false;

    Player* pl = m_session->GetPlayer();
    Player* plTarget = getSelectedPlayer();
    if (!plTarget)
        plTarget = pl;

    LOG_DEBUG(logging, GetMangosString(LANG_ADDITEM), itemId, count);

    ItemPrototype const* pProto = ObjectMgr::GetItemPrototype(itemId);
    if (!pProto)
    {
        PSendSysMessage(LANG_COMMAND_ITEMIDINVALID, itemId);
        SetSentErrorMessage(true);
        return false;
    }

    // Subtract
    if (count < 0)
    {
        count = -count;
        inventory::transaction trans(false);
        trans.destroy(itemId, count);
        plTarget->storage().finalize(trans);
        PSendSysMessage(
            LANG_REMOVEITEM, itemId, count, GetNameLink(plTarget).c_str());
    }
    else if (count > 0)
    {
        inventory::transaction trans(true, inventory::transaction::send_self,
            inventory::transaction::add_vendor);
        trans.add(itemId, count);
        if (!plTarget->storage().finalize(trans))
        {
            PSendSysMessage("Unable to store item %u (failed count: %u).",
                itemId, trans.add_failures()[0]);
            plTarget->SendEquipError(
                static_cast<InventoryResult>(trans.error()), nullptr);
        }
        else
        {
            PSendSysMessage("Added item with id %u (count: %u) to Player: %s.",
                itemId, count, plTarget->GetName());
        }
    }
    return true;
}

bool ChatHandler::HandleAddItemSetCommand(char* args)
{
    uint32 itemsetId;
    if (!ExtractUint32KeyFromLink(&args, "Hitemset", itemsetId))
        return false;

    // prevent generation all items with itemset field value '0'
    if (itemsetId == 0)
    {
        PSendSysMessage(LANG_NO_ITEMS_FROM_ITEMSET_FOUND, itemsetId);
        SetSentErrorMessage(true);
        return false;
    }

    Player* pl = m_session->GetPlayer();
    Player* plTarget = getSelectedPlayer();
    if (!plTarget)
        plTarget = pl;

    LOG_DEBUG(logging, GetMangosString(LANG_ADDITEMSET), itemsetId);

    bool found = false;
    inventory::transaction trans;
    for (uint32 id = 0; id < sItemStorage.MaxEntry; id++)
    {
        ItemPrototype const* pProto =
            sItemStorage.LookupEntry<ItemPrototype>(id);
        if (!pProto)
            continue;

        if (pProto->ItemSet == itemsetId)
        {
            found = true;
            /*XXX*/
            trans.add(id, 1);
        }
    }

    if (!found)
    {
        PSendSysMessage(LANG_NO_ITEMS_FROM_ITEMSET_FOUND, itemsetId);

        SetSentErrorMessage(true);
        return false;
    }

    if (!plTarget->storage().finalize(trans))
    {
        PSendSysMessage("Unable to store itemset: %u.",
            itemsetId); // XXX: Report error type
    }

    return true;
}

bool ChatHandler::HandleListItemCommand(char* args)
{
    uint32 item_id;
    if (!ExtractUint32KeyFromLink(&args, "Hitem", item_id))
        return false;

    if (!item_id)
    {
        PSendSysMessage(LANG_COMMAND_ITEMIDINVALID, item_id);
        SetSentErrorMessage(true);
        return false;
    }

    ItemPrototype const* itemProto = ObjectMgr::GetItemPrototype(item_id);
    if (!itemProto)
    {
        PSendSysMessage(LANG_COMMAND_ITEMIDINVALID, item_id);
        SetSentErrorMessage(true);
        return false;
    }

    uint32 count;
    if (!ExtractOptUInt32(&args, count, 10))
        return false;

    QueryResult* result;

    // inventory case
    uint32 inv_count = 0;
    result = CharacterDatabase.PQuery(
        "SELECT COUNT(item_template) FROM character_inventory WHERE "
        "item_template='%u'",
        item_id);
    if (result)
    {
        inv_count = (*result)[0].GetUInt32();
        delete result;
    }

    result = CharacterDatabase.PQuery(
        //          0        1             2             3        4 5
        "SELECT ci.item, cibag.slot AS bag, ci.slot, ci.guid, "
        "characters.account,characters.name "
        "FROM character_inventory AS ci LEFT JOIN character_inventory AS cibag "
        "ON (cibag.item=ci.bag),characters "
        "WHERE ci.item_template='%u' AND ci.guid = characters.guid LIMIT %u ",
        item_id, uint32(count));

    if (result)
    {
        do
        {
            Field* fields = result->Fetch();
            uint32 item_guid = fields[0].GetUInt32();
            uint8 item_bag = fields[1].GetUInt8();
            uint8 item_index = fields[2].GetUInt8();
            uint32 owner_guid = fields[3].GetUInt32();
            uint32 owner_acc = fields[4].GetUInt32();
            std::string owner_name = fields[5].GetCppString();

            // XXX:
            inventory::slot s(inventory::personal_slot, item_bag, item_index);
            const char* item_pos = nullptr;
            if (s.main_bank() || s.extra_bank_bag())
                item_pos = "[in bank]";
            else if (s.backpack() || s.extra_bag() || s.keyring())
                item_pos = "[in inventory]";
            else if (s.equipment() || s.bagslot() || s.bank_bagslot())
                item_pos = "[equipped]";
            else
                item_pos = "";

            PSendSysMessage(LANG_ITEMLIST_SLOT, item_guid, owner_name.c_str(),
                owner_guid, owner_acc, item_pos);
        } while (result->NextRow());

        uint32 res_count = uint32(result->GetRowCount());

        delete result;

        if (count > res_count)
            count -= res_count;
        else if (count)
            count = 0;
    }

    // mail case
    uint32 mail_count = 0;
    result = CharacterDatabase.PQuery(
        "SELECT COUNT(item_template) FROM mail_items WHERE item_template='%u'",
        item_id);
    if (result)
    {
        mail_count = (*result)[0].GetUInt32();
        delete result;
    }

    if (count > 0)
    {
        result = CharacterDatabase.PQuery(
            //          0                     1            2              3
            //          4            5               6
            "SELECT mail_items.item_guid, mail.sender, mail.receiver, "
            "char_s.account, char_s.name, char_r.account, char_r.name "
            "FROM mail,mail_items,characters as char_s,characters as char_r "
            "WHERE mail_items.item_template='%u' AND char_s.guid = mail.sender "
            "AND char_r.guid = mail.receiver AND mail.id=mail_items.mail_id "
            "LIMIT %u",
            item_id, uint32(count));
    }
    else
        result = nullptr;

    if (result)
    {
        do
        {
            Field* fields = result->Fetch();
            uint32 item_guid = fields[0].GetUInt32();
            uint32 item_s = fields[1].GetUInt32();
            uint32 item_r = fields[2].GetUInt32();
            uint32 item_s_acc = fields[3].GetUInt32();
            std::string item_s_name = fields[4].GetCppString();
            uint32 item_r_acc = fields[5].GetUInt32();
            std::string item_r_name = fields[6].GetCppString();

            char const* item_pos = "[in mail]";

            PSendSysMessage(LANG_ITEMLIST_MAIL, item_guid, item_s_name.c_str(),
                item_s, item_s_acc, item_r_name.c_str(), item_r, item_r_acc,
                item_pos);
        } while (result->NextRow());

        uint32 res_count = uint32(result->GetRowCount());

        delete result;

        if (count > res_count)
            count -= res_count;
        else if (count)
            count = 0;
    }

    // auction case
    uint32 auc_count = 0;
    result = CharacterDatabase.PQuery(
        "SELECT COUNT(item_template) FROM auction WHERE item_template='%u'",
        item_id);
    if (result)
    {
        auc_count = (*result)[0].GetUInt32();
        delete result;
    }

    if (count > 0)
    {
        result = CharacterDatabase.PQuery(
            //           0                      1                       2 3
            "SELECT  auction.itemguid, auction.itemowner, characters.account, "
            "characters.name "
            "FROM auction,characters WHERE auction.item_template='%u' AND "
            "characters.guid = auction.itemowner LIMIT %u",
            item_id, uint32(count));
    }
    else
        result = nullptr;

    if (result)
    {
        do
        {
            Field* fields = result->Fetch();
            uint32 item_guid = fields[0].GetUInt32();
            uint32 owner = fields[1].GetUInt32();
            uint32 owner_acc = fields[2].GetUInt32();
            std::string owner_name = fields[3].GetCppString();

            char const* item_pos = "[in auction]";

            PSendSysMessage(LANG_ITEMLIST_AUCTION, item_guid,
                owner_name.c_str(), owner, owner_acc, item_pos);
        } while (result->NextRow());

        delete result;
    }

    // guild bank case
    uint32 guild_count = 0;
    result = CharacterDatabase.PQuery(
        "SELECT COUNT(item_entry) FROM guild_bank_item WHERE item_entry='%u'",
        item_id);
    if (result)
    {
        guild_count = (*result)[0].GetUInt32();
        delete result;
    }

    result = CharacterDatabase.PQuery(
        //      0             1           2
        "SELECT gi.item_guid, gi.guildid, guild.name "
        "FROM guild_bank_item AS gi, guild WHERE gi.item_entry='%u' AND "
        "gi.guildid = guild.guildid LIMIT %u ",
        item_id, uint32(count));

    if (result)
    {
        do
        {
            Field* fields = result->Fetch();
            uint32 item_guid = fields[0].GetUInt32();
            uint32 guild_guid = fields[1].GetUInt32();
            std::string guild_name = fields[2].GetCppString();

            char const* item_pos = "[in guild bank]";

            PSendSysMessage(LANG_ITEMLIST_GUILD, item_guid, guild_name.c_str(),
                guild_guid, item_pos);
        } while (result->NextRow());

        uint32 res_count = uint32(result->GetRowCount());

        delete result;

        if (count > res_count)
            count -= res_count;
        else if (count)
            count = 0;
    }

    if (inv_count + mail_count + auc_count + guild_count == 0)
    {
        SendSysMessage(LANG_COMMAND_NOITEMFOUND);
        SetSentErrorMessage(true);
        return false;
    }

    PSendSysMessage(LANG_COMMAND_LISTITEMMESSAGE, item_id,
        inv_count + mail_count + auc_count + guild_count, inv_count, mail_count,
        auc_count, guild_count);
    return true;
}

bool ChatHandler::HandleListObjectCommand(char* args)
{
    // number or [name] Shift-click form
    // |color|Hgameobject_entry:go_id|h[name]|h|r
    uint32 go_id;
    if (!ExtractUint32KeyFromLink(&args, "Hgameobject_entry", go_id))
        return false;

    if (!go_id)
    {
        PSendSysMessage(LANG_COMMAND_LISTOBJINVALIDID, go_id);
        SetSentErrorMessage(true);
        return false;
    }

    GameObjectInfo const* gInfo = ObjectMgr::GetGameObjectInfo(go_id);
    if (!gInfo)
    {
        PSendSysMessage(LANG_COMMAND_LISTOBJINVALIDID, go_id);
        SetSentErrorMessage(true);
        return false;
    }

    uint32 count;
    if (!ExtractOptUInt32(&args, count, 10))
        return false;

    QueryResult* result;

    uint32 obj_count = 0;
    result = WorldDatabase.PQuery(
        "SELECT COUNT(guid) FROM gameobject WHERE id='%u'", go_id);
    if (result)
    {
        obj_count = (*result)[0].GetUInt32();
        delete result;
    }

    if (m_session)
    {
        Player* pl = m_session->GetPlayer();
        result = WorldDatabase.PQuery(
            "SELECT guid, position_x, position_y, position_z, map, "
            "(POW(position_x - '%f', 2) + POW(position_y - '%f', 2) + "
            "POW(position_z - '%f', 2)) AS order_ FROM gameobject WHERE id = "
            "'%u' ORDER BY order_ ASC LIMIT %u",
            pl->GetX(), pl->GetY(), pl->GetZ(), go_id, uint32(count));
    }
    else
        result = WorldDatabase.PQuery(
            "SELECT guid, position_x, position_y, position_z, map FROM "
            "gameobject WHERE id = '%u' LIMIT %u",
            go_id, uint32(count));

    if (result)
    {
        do
        {
            Field* fields = result->Fetch();
            uint32 guid = fields[0].GetUInt32();
            float x = fields[1].GetFloat();
            float y = fields[2].GetFloat();
            float z = fields[3].GetFloat();
            int mapid = fields[4].GetUInt16();

            if (m_session)
                PSendSysMessage(LANG_GO_LIST_CHAT, guid,
                    PrepareStringNpcOrGoSpawnInformation<GameObject>(guid)
                        .c_str(),
                    guid, gInfo->name, x, y, z, mapid);
            else
                PSendSysMessage(LANG_GO_LIST_CONSOLE, guid,
                    PrepareStringNpcOrGoSpawnInformation<GameObject>(guid)
                        .c_str(),
                    gInfo->name, x, y, z, mapid);
        } while (result->NextRow());

        delete result;
    }

    PSendSysMessage(LANG_COMMAND_LISTOBJMESSAGE, go_id, obj_count);
    return true;
}

bool ChatHandler::HandleListCreatureCommand(char* args)
{
    // number or [name] Shift-click form
    // |color|Hcreature_entry:creature_id|h[name]|h|r
    uint32 cr_id;
    if (!ExtractUint32KeyFromLink(&args, "Hcreature_entry", cr_id))
        return false;

    if (!cr_id)
    {
        PSendSysMessage(LANG_COMMAND_INVALIDCREATUREID, cr_id);
        SetSentErrorMessage(true);
        return false;
    }

    CreatureInfo const* cInfo = ObjectMgr::GetCreatureTemplate(cr_id);
    if (!cInfo)
    {
        PSendSysMessage(LANG_COMMAND_INVALIDCREATUREID, cr_id);
        SetSentErrorMessage(true);
        return false;
    }

    uint32 count;
    if (!ExtractOptUInt32(&args, count, 10))
        return false;

    QueryResult* result;

    uint32 cr_count = 0;
    result = WorldDatabase.PQuery(
        "SELECT COUNT(guid) FROM creature WHERE id='%u'", cr_id);
    if (result)
    {
        cr_count = (*result)[0].GetUInt32();
        delete result;
    }

    if (m_session)
    {
        Player* pl = m_session->GetPlayer();
        result = WorldDatabase.PQuery(
            "SELECT guid, position_x, position_y, position_z, map, "
            "(POW(position_x - '%f', 2) + POW(position_y - '%f', 2) + "
            "POW(position_z - '%f', 2)) AS order_ FROM creature WHERE id = "
            "'%u' ORDER BY order_ ASC LIMIT %u",
            pl->GetX(), pl->GetY(), pl->GetZ(), cr_id, uint32(count));
    }
    else
        result = WorldDatabase.PQuery(
            "SELECT guid, position_x, position_y, position_z, map FROM "
            "creature WHERE id = '%u' LIMIT %u",
            cr_id, uint32(count));

    if (result)
    {
        do
        {
            Field* fields = result->Fetch();
            uint32 guid = fields[0].GetUInt32();
            float x = fields[1].GetFloat();
            float y = fields[2].GetFloat();
            float z = fields[3].GetFloat();
            int mapid = fields[4].GetUInt16();

            if (m_session)
                PSendSysMessage(LANG_CREATURE_LIST_CHAT, guid,
                    PrepareStringNpcOrGoSpawnInformation<Creature>(guid)
                        .c_str(),
                    guid, cInfo->Name, x, y, z, mapid);
            else
                PSendSysMessage(LANG_CREATURE_LIST_CONSOLE, guid,
                    PrepareStringNpcOrGoSpawnInformation<Creature>(guid)
                        .c_str(),
                    cInfo->Name, x, y, z, mapid);
        } while (result->NextRow());

        delete result;
    }

    PSendSysMessage(LANG_COMMAND_LISTCREATUREMESSAGE, cr_id, cr_count);
    return true;
}

void ChatHandler::ShowItemListHelper(
    uint32 itemId, int loc_idx, Player* target /*=NULL*/)
{
    ItemPrototype const* itemProto =
        sItemStorage.LookupEntry<ItemPrototype>(itemId);
    if (!itemProto)
        return;

    std::string name = itemProto->Name1;
    sObjectMgr::Instance()->GetItemLocaleStrings(
        itemProto->ItemId, loc_idx, &name);

    char const* usableStr = "";

    if (target)
    {
        if (target->can_use_item(itemProto) == EQUIP_ERR_OK)
            usableStr = GetMangosString(LANG_COMMAND_ITEM_USABLE);
    }

    if (m_session)
        PSendSysMessage(
            LANG_ITEM_LIST_CHAT, itemId, itemId, name.c_str(), usableStr);
    else
        PSendSysMessage(
            LANG_ITEM_LIST_CONSOLE, itemId, name.c_str(), usableStr);
}

bool ChatHandler::HandleLookupItemCommand(char* args)
{
    if (!*args)
        return false;

    std::string namepart = args;
    std::wstring wnamepart;

    // converting string that we try to find to lower case
    if (!Utf8toWStr(namepart, wnamepart))
        return false;

    wstrToLower(wnamepart);

    Player* pl = m_session ? m_session->GetPlayer() : nullptr;

    uint32 counter = 0;

    // Search in `item_template`
    for (uint32 id = 0; id < sItemStorage.MaxEntry; ++id)
    {
        ItemPrototype const* pProto =
            sItemStorage.LookupEntry<ItemPrototype>(id);
        if (!pProto)
            continue;

        int loc_idx = GetSessionDbLocaleIndex();

        std::string name; // "" for let later only single time check default
                          // locale name directly
        sObjectMgr::Instance()->GetItemLocaleStrings(id, loc_idx, &name);
        if ((name.empty() || !Utf8FitTo(name, wnamepart)) &&
            !Utf8FitTo(pProto->Name1, wnamepart))
            continue;

        ShowItemListHelper(id, loc_idx, pl);
        ++counter;
    }

    if (counter == 0)
        SendSysMessage(LANG_COMMAND_NOITEMFOUND);

    return true;
}

bool ChatHandler::HandleLookupItemSetCommand(char* args)
{
    if (!*args)
        return false;

    std::string namepart = args;
    std::wstring wnamepart;

    if (!Utf8toWStr(namepart, wnamepart))
        return false;

    // converting string that we try to find to lower case
    wstrToLower(wnamepart);

    uint32 counter = 0; // Counter for figure out that we found smth.

    // Search in ItemSet.dbc
    for (uint32 id = 0; id < sItemSetStore.GetNumRows(); id++)
    {
        ItemSetEntry const* set = sItemSetStore.LookupEntry(id);
        if (set)
        {
            int loc = GetSessionDbcLocale();
            std::string name = set->name[loc];
            if (name.empty())
                continue;

            if (!Utf8FitTo(name, wnamepart))
            {
                loc = 0;
                for (; loc < MAX_LOCALE; ++loc)
                {
                    if (loc == GetSessionDbcLocale())
                        continue;

                    name = set->name[loc];
                    if (name.empty())
                        continue;

                    if (Utf8FitTo(name, wnamepart))
                        break;
                }
            }

            if (loc < MAX_LOCALE)
            {
                // send item set in "id - [namedlink locale]" format
                if (m_session)
                    PSendSysMessage(LANG_ITEMSET_LIST_CHAT, id, id,
                        name.c_str(), localeNames[loc]);
                else
                    PSendSysMessage(LANG_ITEMSET_LIST_CONSOLE, id, name.c_str(),
                        localeNames[loc]);
                ++counter;
            }
        }
    }
    if (counter == 0) // if counter == 0 then we found nth
        SendSysMessage(LANG_COMMAND_NOITEMSETFOUND);
    return true;
}

bool ChatHandler::HandleLookupSkillCommand(char* args)
{
    if (!*args)
        return false;

    // can be NULL in console call
    Player* target = getSelectedPlayer();

    std::string namepart = args;
    std::wstring wnamepart;

    if (!Utf8toWStr(namepart, wnamepart))
        return false;

    // converting string that we try to find to lower case
    wstrToLower(wnamepart);

    uint32 counter = 0; // Counter for figure out that we found smth.

    // Search in SkillLine.dbc
    for (uint32 id = 0; id < sSkillLineStore.GetNumRows(); id++)
    {
        SkillLineEntry const* skillInfo = sSkillLineStore.LookupEntry(id);
        if (skillInfo)
        {
            int loc = GetSessionDbcLocale();
            std::string name = skillInfo->name[loc];
            if (name.empty())
                continue;

            if (!Utf8FitTo(name, wnamepart))
            {
                loc = 0;
                for (; loc < MAX_LOCALE; ++loc)
                {
                    if (loc == GetSessionDbcLocale())
                        continue;

                    name = skillInfo->name[loc];
                    if (name.empty())
                        continue;

                    if (Utf8FitTo(name, wnamepart))
                        break;
                }
            }

            if (loc < MAX_LOCALE)
            {
                char valStr[50] = "";
                char const* knownStr = "";
                if (target && target->HasSkill(id))
                {
                    knownStr = GetMangosString(LANG_KNOWN);
                    uint32 curValue = target->GetPureSkillValue(id);
                    uint32 maxValue = target->GetPureMaxSkillValue(id);
                    uint32 permValue = target->GetSkillPermBonusValue(id);
                    uint32 tempValue = target->GetSkillTempBonusValue(id);

                    char const* valFormat = GetMangosString(LANG_SKILL_VALUES);
                    snprintf(valStr, 50, valFormat, curValue, maxValue,
                        permValue, tempValue);
                }

                // send skill in "id - [namedlink locale]" format
                if (m_session)
                    PSendSysMessage(LANG_SKILL_LIST_CHAT, id, id, name.c_str(),
                        localeNames[loc], knownStr, valStr);
                else
                    PSendSysMessage(LANG_SKILL_LIST_CONSOLE, id, name.c_str(),
                        localeNames[loc], knownStr, valStr);

                ++counter;
            }
        }
    }
    if (counter == 0) // if counter == 0 then we found nth
        SendSysMessage(LANG_COMMAND_NOSKILLFOUND);
    return true;
}

void ChatHandler::ShowSpellListHelper(
    Player* target, SpellEntry const* spellInfo, LocaleConstant loc)
{
    uint32 id = spellInfo->Id;

    bool known = target && target->HasSpell(id);
    bool learn =
        (spellInfo->Effect[EFFECT_INDEX_0] == SPELL_EFFECT_LEARN_SPELL);

    uint32 talentCost = GetTalentSpellCost(id);

    bool talent = (talentCost > 0);
    bool passive = IsPassiveSpell(spellInfo);
    bool active = target && target->has_aura(id);

    // unit32 used to prevent interpreting uint8 as char at output
    // find rank of learned spell for learning spell, or talent rank
    uint32 rank =
        talentCost ?
            talentCost :
            sSpellMgr::Instance()->GetSpellRank(
                learn ? spellInfo->EffectTriggerSpell[EFFECT_INDEX_0] : id);

    // send spell in "id - [name, rank N] [talent] [passive] [learn] [known]"
    // format
    std::ostringstream ss;
    if (m_session)
        ss << id << " - |cffffffff|Hspell:" << id << "|h["
           << spellInfo->SpellName[loc];
    else
        ss << id << " - " << spellInfo->SpellName[loc];

    // include rank in link name
    if (rank)
        ss << GetMangosString(LANG_SPELL_RANK) << rank;

    if (m_session)
        ss << " " << localeNames[loc] << "]|h|r";
    else
        ss << " " << localeNames[loc];

    if (talent)
        ss << GetMangosString(LANG_TALENT);
    if (passive)
        ss << GetMangosString(LANG_PASSIVE);
    if (learn)
        ss << GetMangosString(LANG_LEARN);
    if (known)
        ss << GetMangosString(LANG_KNOWN);
    if (active)
        ss << GetMangosString(LANG_ACTIVE);

    SendSysMessage(ss.str().c_str());
}

bool ChatHandler::HandleLookupSpellCommand(char* args)
{
    if (!*args)
        return false;

    // can be NULL at console call
    Player* target = getSelectedPlayer();

    std::string namepart = args;
    std::wstring wnamepart;

    if (!Utf8toWStr(namepart, wnamepart))
        return false;

    // converting string that we try to find to lower case
    wstrToLower(wnamepart);

    uint32 counter = 0; // Counter for figure out that we found smth.

    // Search in Spell.dbc
    for (uint32 id = 0; id < sSpellStore.GetNumRows(); id++)
    {
        SpellEntry const* spellInfo = sSpellStore.LookupEntry(id);
        if (spellInfo)
        {
            int loc = GetSessionDbcLocale();
            std::string name = spellInfo->SpellName[loc];
            if (name.empty())
                continue;

            if (!Utf8FitTo(name, wnamepart))
            {
                loc = 0;
                for (; loc < MAX_LOCALE; ++loc)
                {
                    if (loc == GetSessionDbcLocale())
                        continue;

                    name = spellInfo->SpellName[loc];
                    if (name.empty())
                        continue;

                    if (Utf8FitTo(name, wnamepart))
                        break;
                }
            }

            if (loc < MAX_LOCALE)
            {
                ShowSpellListHelper(target, spellInfo, LocaleConstant(loc));
                ++counter;
            }
        }
    }
    if (counter == 0) // if counter == 0 then we found nth
        SendSysMessage(LANG_COMMAND_NOSPELLFOUND);
    return true;
}

void ChatHandler::ShowQuestListHelper(
    uint32 questId, int32 loc_idx, Player* target /*= NULL*/)
{
    Quest const* qinfo = sObjectMgr::Instance()->GetQuestTemplate(questId);
    if (!qinfo)
        return;

    std::string title = qinfo->GetTitle();
    sObjectMgr::Instance()->GetQuestLocaleStrings(questId, loc_idx, &title);

    char const* statusStr = "";

    if (target)
    {
        QuestStatus status = target->GetQuestStatus(qinfo->GetQuestId());

        if (status == QUEST_STATUS_COMPLETE)
        {
            if (target->GetQuestRewardStatus(qinfo->GetQuestId()))
                statusStr = GetMangosString(LANG_COMMAND_QUEST_REWARDED);
            else
                statusStr = GetMangosString(LANG_COMMAND_QUEST_COMPLETE);
        }
        else if (status == QUEST_STATUS_INCOMPLETE)
            statusStr = GetMangosString(LANG_COMMAND_QUEST_ACTIVE);
    }

    if (m_session)
        PSendSysMessage(LANG_QUEST_LIST_CHAT, qinfo->GetQuestId(),
            qinfo->GetQuestId(), qinfo->GetQuestLevel(), title.c_str(),
            statusStr);
    else
        PSendSysMessage(LANG_QUEST_LIST_CONSOLE, qinfo->GetQuestId(),
            title.c_str(), statusStr);
}

bool ChatHandler::HandleLookupQuestCommand(char* args)
{
    if (!*args)
        return false;

    // can be NULL at console call
    Player* target = getSelectedPlayer();

    std::string namepart = args;
    std::wstring wnamepart;

    // converting string that we try to find to lower case
    if (!Utf8toWStr(namepart, wnamepart))
        return false;

    wstrToLower(wnamepart);

    uint32 counter = 0;

    int loc_idx = GetSessionDbLocaleIndex();

    ObjectMgr::QuestMap const& qTemplates =
        sObjectMgr::Instance()->GetQuestTemplates();
    for (const auto& qTemplate : qTemplates)
    {
        Quest* qinfo = qTemplate.second;

        std::string title; // "" for avoid repeating check default locale
        sObjectMgr::Instance()->GetQuestLocaleStrings(
            qinfo->GetQuestId(), loc_idx, &title);

        if ((title.empty() || !Utf8FitTo(title, wnamepart)) &&
            !Utf8FitTo(qinfo->GetTitle(), wnamepart))
            continue;

        ShowQuestListHelper(qinfo->GetQuestId(), loc_idx, target);
        ++counter;
    }

    if (counter == 0)
        SendSysMessage(LANG_COMMAND_NOQUESTFOUND);

    return true;
}

bool ChatHandler::HandleLookupCreatureCommand(char* args)
{
    if (!*args)
        return false;

    std::string namepart = args;
    std::wstring wnamepart;

    // converting string that we try to find to lower case
    if (!Utf8toWStr(namepart, wnamepart))
        return false;

    wstrToLower(wnamepart);

    uint32 counter = 0;

    for (uint32 id = 0; id < sCreatureStorage.MaxEntry; ++id)
    {
        CreatureInfo const* cInfo =
            sCreatureStorage.LookupEntry<CreatureInfo>(id);
        if (!cInfo)
            continue;

        int loc_idx = GetSessionDbLocaleIndex();

        char const* name =
            ""; // "" for avoid repeating check for default locale
        sObjectMgr::Instance()->GetCreatureLocaleStrings(id, loc_idx, &name);
        if (!*name || !Utf8FitTo(name, wnamepart))
        {
            name = cInfo->Name;
            if (!Utf8FitTo(name, wnamepart))
                continue;
        }

        if (m_session)
            PSendSysMessage(LANG_CREATURE_ENTRY_LIST_CHAT, id, id, name);
        else
            PSendSysMessage(LANG_CREATURE_ENTRY_LIST_CONSOLE, id, name);

        ++counter;
    }

    if (counter == 0)
        SendSysMessage(LANG_COMMAND_NOCREATUREFOUND);

    return true;
}

bool ChatHandler::HandleLookupObjectCommand(char* args)
{
    if (!*args)
        return false;

    std::string namepart = args;
    std::wstring wnamepart;

    // converting string that we try to find to lower case
    if (!Utf8toWStr(namepart, wnamepart))
        return false;

    wstrToLower(wnamepart);

    uint32 counter = 0;

    for (uint32 id = 0; id < sGOStorage.MaxEntry; id++)
    {
        GameObjectInfo const* gInfo =
            sGOStorage.LookupEntry<GameObjectInfo>(id);
        if (!gInfo)
            continue;

        int loc_idx = GetSessionDbLocaleIndex();
        if (loc_idx >= 0)
        {
            GameObjectLocale const* gl =
                sObjectMgr::Instance()->GetGameObjectLocale(id);
            if (gl)
            {
                if ((int32)gl->Name.size() > loc_idx &&
                    !gl->Name[loc_idx].empty())
                {
                    std::string name = gl->Name[loc_idx];

                    if (Utf8FitTo(name, wnamepart))
                    {
                        if (m_session)
                            PSendSysMessage(
                                LANG_GO_ENTRY_LIST_CHAT, id, id, name.c_str());
                        else
                            PSendSysMessage(
                                LANG_GO_ENTRY_LIST_CONSOLE, id, name.c_str());
                        ++counter;
                        continue;
                    }
                }
            }
        }

        std::string name = gInfo->name;
        if (name.empty())
            continue;

        if (Utf8FitTo(name, wnamepart))
        {
            if (m_session)
                PSendSysMessage(LANG_GO_ENTRY_LIST_CHAT, id, id, name.c_str());
            else
                PSendSysMessage(LANG_GO_ENTRY_LIST_CONSOLE, id, name.c_str());
            ++counter;
        }
    }

    if (counter == 0)
        SendSysMessage(LANG_COMMAND_NOGAMEOBJECTFOUND);

    return true;
}

bool ChatHandler::HandleLookupTaxiNodeCommand(char* args)
{
    if (!*args)
        return false;

    std::string namepart = args;
    std::wstring wnamepart;

    if (!Utf8toWStr(namepart, wnamepart))
        return false;

    // converting string that we try to find to lower case
    wstrToLower(wnamepart);

    uint32 counter = 0; // Counter for figure out that we found smth.

    // Search in TaxiNodes.dbc
    for (uint32 id = 0; id < sTaxiNodesStore.GetNumRows(); id++)
    {
        TaxiNodesEntry const* nodeEntry = sTaxiNodesStore.LookupEntry(id);
        if (nodeEntry)
        {
            int loc = GetSessionDbcLocale();
            std::string name = nodeEntry->name[loc];
            if (name.empty())
                continue;

            if (!Utf8FitTo(name, wnamepart))
            {
                loc = 0;
                for (; loc < MAX_LOCALE; ++loc)
                {
                    if (loc == GetSessionDbcLocale())
                        continue;

                    name = nodeEntry->name[loc];
                    if (name.empty())
                        continue;

                    if (Utf8FitTo(name, wnamepart))
                        break;
                }
            }

            if (loc < MAX_LOCALE)
            {
                // send taxinode in "id - [name] (Map:m X:x Y:y Z:z)" format
                if (m_session)
                    PSendSysMessage(LANG_TAXINODE_ENTRY_LIST_CHAT, id, id,
                        name.c_str(), localeNames[loc], nodeEntry->map_id,
                        nodeEntry->x, nodeEntry->y, nodeEntry->z);
                else
                    PSendSysMessage(LANG_TAXINODE_ENTRY_LIST_CONSOLE, id,
                        name.c_str(), localeNames[loc], nodeEntry->map_id,
                        nodeEntry->x, nodeEntry->y, nodeEntry->z);
                ++counter;
            }
        }
    }
    if (counter == 0) // if counter == 0 then we found nth
        SendSysMessage(LANG_COMMAND_NOTAXINODEFOUND);
    return true;
}

/** \brief GM command level 3 - Create a guild.
 *
 * This command allows a GM (level 3) to create a guild.
 *
 * The "args" parameter contains the name of the guild leader
 * and then the name of the guild.
 *
 */
bool ChatHandler::HandleGuildCreateCommand(char* args)
{
    // guildmaster name optional
    char* guildMasterStr = ExtractOptNotLastArg(&args);

    Player* target;
    if (!ExtractPlayerTarget(&guildMasterStr, &target))
        return false;

    char* guildStr = ExtractQuotedArg(&args);
    if (!guildStr)
        return false;

    std::string guildname = guildStr;

    if (target->GetGuildId())
    {
        SendSysMessage(LANG_PLAYER_IN_GUILD);
        return true;
    }

    auto guild = new Guild;
    if (!guild->Create(target, guildname))
    {
        delete guild;
        SendSysMessage(LANG_GUILD_NOT_CREATED);
        SetSentErrorMessage(true);
        return false;
    }

    sGuildMgr::Instance()->AddGuild(guild);
    return true;
}

bool ChatHandler::HandleGuildInviteCommand(char* args)
{
    // player name optional
    char* nameStr = ExtractOptNotLastArg(&args);

    // if not guild name only (in "") then player name
    ObjectGuid target_guid;
    if (!ExtractPlayerTarget(&nameStr, nullptr, &target_guid))
        return false;

    char* guildStr = ExtractQuotedArg(&args);
    if (!guildStr)
        return false;

    std::string glName = guildStr;
    Guild* targetGuild = sGuildMgr::Instance()->GetGuildByName(glName);
    if (!targetGuild)
        return false;

    // player's guild membership checked in AddMember before add
    if (!targetGuild->AddMember(target_guid, targetGuild->GetLowestRank()))
        return false;

    return true;
}

bool ChatHandler::HandleGuildUninviteCommand(char* args)
{
    Player* target;
    ObjectGuid target_guid;
    if (!ExtractPlayerTarget(&args, &target, &target_guid))
        return false;

    uint32 glId =
        target ? target->GetGuildId() : Player::GetGuildIdFromDB(target_guid);
    if (!glId)
        return false;

    Guild* targetGuild = sGuildMgr::Instance()->GetGuildById(glId);
    if (!targetGuild)
        return false;

    if (targetGuild->DelMember(target_guid))
    {
        targetGuild->Disband();
        delete targetGuild;
    }

    return true;
}

bool ChatHandler::HandleGuildRankCommand(char* args)
{
    char* nameStr = ExtractOptNotLastArg(&args);

    Player* target;
    ObjectGuid target_guid;
    std::string target_name;
    if (!ExtractPlayerTarget(&nameStr, &target, &target_guid, &target_name))
        return false;

    uint32 glId =
        target ? target->GetGuildId() : Player::GetGuildIdFromDB(target_guid);
    if (!glId)
        return false;

    Guild* targetGuild = sGuildMgr::Instance()->GetGuildById(glId);
    if (!targetGuild)
        return false;

    uint32 newrank;
    if (!ExtractUInt32(&args, newrank))
        return false;

    if (newrank > targetGuild->GetLowestRank())
        return false;

    MemberSlot* slot = targetGuild->GetMemberSlot(target_guid);
    if (!slot)
        return false;

    slot->ChangeRank(newrank);
    return true;
}

bool ChatHandler::HandleGuildDeleteCommand(char* args)
{
    if (!*args)
        return false;

    char* guildStr = ExtractQuotedArg(&args);
    if (!guildStr)
        return false;

    std::string gld = guildStr;

    Guild* targetGuild = sGuildMgr::Instance()->GetGuildByName(gld);
    if (!targetGuild)
        return false;

    targetGuild->Disband();
    delete targetGuild;

    return true;
}

bool ChatHandler::HandleGetDistanceCommand(char* args)
{
    WorldObject* obj = nullptr;

    if (*args)
    {
        if (ObjectGuid guid = ExtractGuidFromLink(&args))
            obj = (WorldObject*)m_session->GetPlayer()->GetObjectByTypeMask(
                guid, TYPEMASK_CREATURE_OR_GAMEOBJECT);

        if (!obj)
        {
            SendSysMessage(LANG_PLAYER_NOT_FOUND);
            SetSentErrorMessage(true);
            return false;
        }
    }
    else
    {
        obj = getSelectedUnit();

        if (!obj)
        {
            SendSysMessage(LANG_SELECT_CHAR_OR_CREATURE);
            SetSentErrorMessage(true);
            return false;
        }
    }

    Player* player = m_session->GetPlayer();
    // Calculate point-to-point distance
    float dx, dy, dz;
    dx = player->GetX() - obj->GetX();
    dy = player->GetY() - obj->GetY();
    dz = player->GetZ() - obj->GetZ();

    PSendSysMessage(LANG_DISTANCE, player->GetDistance(obj),
        player->GetDistance2d(obj), sqrt(dx * dx + dy * dy + dz * dz));

    return true;
}

bool ChatHandler::HandleDieCommand(char* /*args*/)
{
    Unit* target = getSelectedUnit();

    if (!target || !m_session->GetPlayer()->GetSelectionGuid())
    {
        SendSysMessage(LANG_SELECT_CHAR_OR_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    if (target->GetTypeId() == TYPEID_PLAYER)
    {
        if (HasLowerSecurity((Player*)target, ObjectGuid(), false))
            return false;
    }

    if (target->isAlive())
    {
        m_session->GetPlayer()->DealDamage(target, target->GetHealth(), nullptr,
            DIRECT_DAMAGE, SPELL_SCHOOL_MASK_NORMAL, nullptr, false, false);
    }

    return true;
}

bool ChatHandler::HandleDamageCommand(char* args)
{
    if (!*args)
        return false;

    Unit* target = getSelectedUnit();

    if (!target || !m_session->GetPlayer()->GetSelectionGuid())
    {
        SendSysMessage(LANG_SELECT_CHAR_OR_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    if (!target->isAlive())
        return true;

    int32 damage_int;
    if (!ExtractInt32(&args, damage_int))
        return false;

    if (damage_int <= 0)
        return true;

    if (target->GetHealth() == 0)
        return true;

    uint32 damage = damage_int;

    if (damage >= target->GetHealth())
        damage = target->GetHealth() - 1;

    m_session->GetPlayer()->DealDamageMods(target, damage, nullptr);
    m_session->GetPlayer()->DealDamage(target, damage, nullptr, DIRECT_DAMAGE,
        SPELL_SCHOOL_MASK_NORMAL, nullptr, false, false);
    if (target != m_session->GetPlayer())
        m_session->GetPlayer()->SendAttackStateUpdate(HITINFO_NORMALSWING2,
            target, 1, SPELL_SCHOOL_MASK_NORMAL, damage, 0, 0,
            VICTIMSTATE_NORMAL, 0);
    return true;
}

bool ChatHandler::HandleModifyArenaCommand(char* args)
{
    if (!*args)
        return false;

    Player* target = getSelectedPlayer();
    if (!target)
    {
        SendSysMessage(LANG_PLAYER_NOT_FOUND);
        SetSentErrorMessage(true);
        return false;
    }

    int32 amount = (int32)atoi(args);

    target->ModifyArenaPoints(amount);

    PSendSysMessage(LANG_COMMAND_MODIFY_ARENA, GetNameLink(target).c_str(),
        target->GetArenaPoints());

    return true;
}

bool ChatHandler::HandleReviveCommand(char* args)
{
    Player* target;
    ObjectGuid target_guid;
    if (!ExtractPlayerTarget(&args, &target, &target_guid))
        return false;

    if (target)
    {
        target->ResurrectPlayer(0.5f);
        target->SpawnCorpseBones();
    }
    else
        // will resurrected at login without corpse
        sObjectAccessor::Instance()->ConvertCorpseForPlayer(target_guid);

    return true;
}

bool ChatHandler::HandleAuraCommand(char* args)
{
    Unit* target = getSelectedUnit();
    if (!target)
    {
        SendSysMessage(LANG_SELECT_CHAR_OR_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    // number or [name] Shift-click form |color|Hspell:spell_id|h[name]|h|r or
    // Htalent form
    uint32 spellID = ExtractSpellIdFromLink(&args);

    // List all active auras if no spell id was supplied
    if (spellID == 0)
    {
        target->loop_auras([this](AuraHolder* holder)
            {
                ShowSpellListHelper(m_session->GetPlayer(),
                    holder->GetSpellProto(), LOCALE_enUS);
                return true; // continue
            });
        return true;
    }

    if (target->GetTypeId() == TYPEID_UNIT &&
        static_cast<Creature*>(target)->IsInEvadeMode())
    {
        SendSysMessage("You cannot .aura a mob currently evading.");
        return true;
    }

    SpellEntry const* spellInfo = sSpellStore.LookupEntry(spellID);
    if (!spellInfo)
        return false;

    if (!IsSpellAppliesAura(spellInfo, (1 << EFFECT_INDEX_0) |
                                           (1 << EFFECT_INDEX_1) |
                                           (1 << EFFECT_INDEX_2)) &&
        !IsSpellHaveEffect(spellInfo, SPELL_EFFECT_PERSISTENT_AREA_AURA))
    {
        PSendSysMessage(LANG_SPELL_NO_HAVE_AURAS, spellID);
        SetSentErrorMessage(true);
        return false;
    }

    target->AddAuraThroughNewHolder(spellInfo->Id, m_session->GetPlayer());

    return true;
}

bool ChatHandler::HandleUnAuraCommand(char* args)
{
    Unit* target = getSelectedUnit();
    if (!target)
    {
        SendSysMessage(LANG_SELECT_CHAR_OR_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    std::string argstr = args;
    if (argstr == "all")
    {
        target->remove_auras();
        return true;
    }

    // number or [name] Shift-click form |color|Hspell:spell_id|h[name]|h|r or
    // Htalent form
    uint32 spellID = ExtractSpellIdFromLink(&args);
    if (!spellID)
        return false;

    target->remove_auras(spellID);

    return true;
}

bool ChatHandler::HandleLinkGraveCommand(char* args)
{
    uint32 g_id;
    if (!ExtractUInt32(&args, g_id))
        return false;

    char* teamStr = ExtractLiteralArg(&args);

    Team g_team;
    if (!teamStr)
        g_team = TEAM_NONE;
    else if (strncmp(teamStr, "horde", strlen(teamStr)) == 0)
        g_team = HORDE;
    else if (strncmp(teamStr, "alliance", strlen(teamStr)) == 0)
        g_team = ALLIANCE;
    else
        return false;

    WorldSafeLocsEntry const* graveyard = sWorldSafeLocsStore.LookupEntry(g_id);

    if (!graveyard)
    {
        PSendSysMessage(LANG_COMMAND_GRAVEYARDNOEXIST, g_id);
        SetSentErrorMessage(true);
        return false;
    }

    Player* player = m_session->GetPlayer();

    uint32 zoneId = player->GetZoneId();

    AreaTableEntry const* areaEntry = GetAreaEntryByAreaID(zoneId);
    if (!areaEntry || areaEntry->zone != 0)
    {
        PSendSysMessage(LANG_COMMAND_GRAVEYARDWRONGZONE, g_id, zoneId);
        SetSentErrorMessage(true);
        return false;
    }

    if (sObjectMgr::Instance()->AddGraveYardLink(g_id, zoneId, g_team))
        PSendSysMessage(LANG_COMMAND_GRAVEYARDLINKED, g_id, zoneId);
    else
        PSendSysMessage(LANG_COMMAND_GRAVEYARDALRLINKED, g_id, zoneId);

    return true;
}

bool ChatHandler::HandleNearGraveCommand(char* args)
{
    Team g_team;

    size_t argslen = strlen(args);

    if (!*args)
        g_team = TEAM_NONE;
    else if (strncmp(args, "horde", argslen) == 0)
        g_team = HORDE;
    else if (strncmp(args, "alliance", argslen) == 0)
        g_team = ALLIANCE;
    else
        return false;

    Player* player = m_session->GetPlayer();
    uint32 zone_id = player->GetZoneId();

    WorldSafeLocsEntry const* graveyard =
        sObjectMgr::Instance()->GetClosestGraveyard(player->GetX(),
            player->GetY(), player->GetZ(), player->GetMapId(), g_team);

    if (graveyard)
    {
        uint32 g_id = graveyard->ID;

        GraveYardData const* data =
            sObjectMgr::Instance()->FindGraveYardData(g_id, zone_id);
        if (!data)
        {
            PSendSysMessage(LANG_COMMAND_GRAVEYARDERROR, g_id);
            SetSentErrorMessage(true);
            return false;
        }

        g_team = data->team;

        std::string team_name = GetMangosString(LANG_COMMAND_GRAVEYARD_NOTEAM);

        if (g_team == 0)
            team_name = GetMangosString(LANG_COMMAND_GRAVEYARD_ANY);
        else if (g_team == HORDE)
            team_name = GetMangosString(LANG_COMMAND_GRAVEYARD_HORDE);
        else if (g_team == ALLIANCE)
            team_name = GetMangosString(LANG_COMMAND_GRAVEYARD_ALLIANCE);

        PSendSysMessage(
            LANG_COMMAND_GRAVEYARDNEAREST, g_id, team_name.c_str(), zone_id);
    }
    else
    {
        std::string team_name;

        if (g_team == 0)
            team_name = GetMangosString(LANG_COMMAND_GRAVEYARD_ANY);
        else if (g_team == HORDE)
            team_name = GetMangosString(LANG_COMMAND_GRAVEYARD_HORDE);
        else if (g_team == ALLIANCE)
            team_name = GetMangosString(LANG_COMMAND_GRAVEYARD_ALLIANCE);

        if (g_team == 0)
            PSendSysMessage(LANG_COMMAND_ZONENOGRAVEYARDS, zone_id);
        else
            PSendSysMessage(
                LANG_COMMAND_ZONENOGRAFACTION, zone_id, team_name.c_str());
    }

    return true;
}

//-----------------------Npc Commands-----------------------
bool ChatHandler::HandleNpcAllowMovementCommand(char* /*args*/)
{
    if (sWorld::Instance()->getAllowMovement())
    {
        sWorld::Instance()->SetAllowMovement(false);
        SendSysMessage(LANG_CREATURE_MOVE_DISABLED);
    }
    else
    {
        sWorld::Instance()->SetAllowMovement(true);
        SendSysMessage(LANG_CREATURE_MOVE_ENABLED);
    }
    return true;
}

bool ChatHandler::HandleNpcChangeEntryCommand(char* args)
{
    if (!*args)
        return false;

    uint32 newEntryNum = atoi(args);
    if (!newEntryNum)
        return false;

    Unit* unit = getSelectedUnit();
    if (!unit || unit->GetTypeId() != TYPEID_UNIT)
    {
        SendSysMessage(LANG_SELECT_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }
    Creature* creature = (Creature*)unit;
    if (creature->UpdateEntry(newEntryNum))
        SendSysMessage(LANG_DONE);
    else
        SendSysMessage(LANG_ERROR);
    return true;
}

bool ChatHandler::HandleNpcInfoCommand(char* /*args*/)
{
    Creature* target = getSelectedCreature();

    if (!target)
    {
        SendSysMessage(LANG_SELECT_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    uint32 faction = target->getFaction();
    uint32 npcflags = target->GetUInt32Value(UNIT_NPC_FLAGS);
    uint32 displayid = target->GetDisplayId();
    uint32 nativeid = target->GetNativeDisplayId();
    uint32 Entry = target->GetEntry();
    CreatureInfo const* cInfo = target->GetCreatureInfo();

    time_t curRespawnDelay =
        target->GetRespawnTimeEx() - WorldTimer::time_no_syscall();
    if (curRespawnDelay < 0)
        curRespawnDelay = 0;
    std::string curRespawnDelayStr = secsToTimeString(curRespawnDelay, true);
    std::string defRespawnDelayStr =
        secsToTimeString(target->GetRespawnDelay(), true);

    // Send information dependend on difficulty mode
    CreatureInfo const* baseInfo = ObjectMgr::GetCreatureTemplate(Entry);
    if (baseInfo->HeroicEntry == target->GetCreatureInfo()->Entry)
        PSendSysMessage(LANG_NPCINFO_CHAR_DIFFICULTY,
            target->GetGuidStr().c_str(), faction, npcflags, Entry,
            target->GetCreatureInfo()->Entry, 1, displayid, nativeid);
    else
        PSendSysMessage(LANG_NPCINFO_CHAR, target->GetGuidStr().c_str(),
            faction, npcflags, Entry, displayid, nativeid);

    std::string power_str;
    switch (target->getPowerType())
    {
    case POWER_MANA:
        power_str = "Mana";
        break;
    case POWER_RAGE:
        power_str = "Rage";
        break;
    case POWER_FOCUS:
        power_str = "Focus";
        break;
    case POWER_ENERGY:
        power_str = "Energy";
        break;
    case POWER_HAPPINESS:
        power_str = "Happiness";
        break;
    case POWER_RUNES:
        power_str = "Runes";
        break;
    case POWER_HEALTH:
        power_str = "Health";
        break;
    }
    uint32 power = target->GetPower(target->getPowerType());
    uint32 max_power = target->GetMaxPower(target->getPowerType());
    uint32 create_power = target->GetCreatePowers(target->getPowerType());

    PSendSysMessage(LANG_NPCINFO_LEVEL, target->getLevel());
    PSendSysMessage(LANG_NPCINFO_HEALTH, target->GetCreateHealth(),
        target->GetMaxHealth(), target->GetHealth());
    PSendSysMessage("Power type: %s. %u/%u (base: %u)", power_str.c_str(),
        power, max_power, create_power);
    PSendSysMessage(LANG_NPCINFO_FLAGS,
        target->GetUInt32Value(UNIT_FIELD_FLAGS),
        target->GetUInt32Value(UNIT_DYNAMIC_FLAGS), target->getFaction());
    PSendSysMessage(LANG_COMMAND_RAWPAWNTIMES, defRespawnDelayStr.c_str(),
        curRespawnDelayStr.c_str());
    PSendSysMessage(LANG_NPCINFO_LOOT, cInfo->lootid, cInfo->pickpocketLootId,
        cInfo->SkinLootId);
    PSendSysMessage(LANG_NPCINFO_DUNGEON_ID, target->GetInstanceId());
    PSendSysMessage(LANG_NPCINFO_POSITION, float(target->GetX()),
        float(target->GetY()), float(target->GetZ()));

    if ((npcflags & UNIT_NPC_FLAG_VENDOR))
    {
        SendSysMessage(LANG_NPCINFO_VENDOR);
    }
    if ((npcflags & UNIT_NPC_FLAG_TRAINER))
    {
        SendSysMessage(LANG_NPCINFO_TRAINER);
    }

    ShowNpcOrGoSpawnInformation<Creature>(target->GetGUIDLow());
    return true;
}

// add/remove leash to creature
bool ChatHandler::HandleNpcLeashCommand(char* args)
{
    float radius;

    char* arg_one = ExtractLiteralArg(&args);
    char* arg_two = ExtractLiteralArg(&args);
    bool rad_extract = ExtractFloat(&args, radius);

    if (!arg_one ||
        (strcmp(arg_one, "remove") != 0 && (!arg_two || !rad_extract)))
    {
        if (Creature* npc = getSelectedCreature())
        {
            if (auto data =
                    sObjectMgr::Instance()->GetCreatureData(npc->GetGUIDLow()))
                PSendSysMessage("Creature leash info: X:%f Y:%f Z:%f R:%f",
                    data->leash_x, data->leash_y, data->leash_z,
                    data->leash_radius);
        }
        else
            SendSysMessage(".npc leash {add/remove} {me/target} {radius}");
        return true;
    }

    if (Creature* npc = getSelectedCreature())
    {
        if (strcmp(arg_one, "remove") == 0)
        {
            WorldDatabase.PExecute(
                "UPDATE creature SET leash_x=0, leash_y=0, leash_z=0, "
                "leash_radius=0 WHERE guid=%u",
                npc->GetGUIDLow());
            SendSysMessage("Change will be active on next restart.");
            return true;
        }
        else if (strcmp(arg_one, "add") == 0)
        {
            float x, y, z;
            if (strcmp(arg_two, "me") == 0)
                m_session->GetPlayer()->GetPosition(x, y, z);
            else if (strcmp(arg_two, "target") == 0)
                npc->GetPosition(x, y, z);
            else
                return false;
            if (radius > 0)
            {
                WorldDatabase.PExecute(
                    "UPDATE creature SET leash_x=%f, leash_y=%f, leash_z=%f, "
                    "leash_radius=%f WHERE guid=%u",
                    x, y, z, radius, npc->GetGUIDLow());
                SendSysMessage("Change will be active on next restart.");
                return true;
            }
        }
    }

    return false;
}

// play npc emote
bool ChatHandler::HandleNpcPlayEmoteCommand(char* args)
{
    uint32 emote = atoi(args);

    Creature* target = getSelectedCreature();
    if (!target)
    {
        SendSysMessage(LANG_SELECT_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    target->HandleEmote(emote);

    return true;
}

// TODO: NpcCommands that needs to be fixed :

bool ChatHandler::HandleNpcAddWeaponCommand(char* /*args*/)
{
    /*if (!*args)
    return false;

    ObjectGuid guid = m_session->GetPlayer()->GetSelectionGuid();
    if (guid.IsEmpty())
    {
        SendSysMessage(LANG_NO_SELECTION);
        return true;
    }

    Creature *pCreature = ObjectAccessor::GetCreature(*m_session->GetPlayer(),
    guid);

    if(!pCreature)
    {
        SendSysMessage(LANG_SELECT_CREATURE);
        return true;
    }

    char* pSlotID = strtok((char*)args, " ");
    if (!pSlotID)
        return false;

    char* pItemID = strtok(NULL, " ");
    if (!pItemID)
        return false;

    uint32 ItemID = atoi(pItemID);
    uint32 SlotID = atoi(pSlotID);

    ItemPrototype* tmpItem = ObjectMgr::GetItemPrototype(ItemID);

    bool added = false;
    if(tmpItem)
    {
        switch(SlotID)
        {
            case 1:
                pCreature->SetUInt32Value(UNIT_VIRTUAL_ITEM_SLOT_DISPLAY,
    ItemID);
                added = true;
                break;
            case 2:
                pCreature->SetUInt32Value(UNIT_VIRTUAL_ITEM_SLOT_DISPLAY_01,
    ItemID);
                added = true;
                break;
            case 3:
                pCreature->SetUInt32Value(UNIT_VIRTUAL_ITEM_SLOT_DISPLAY_02,
    ItemID);
                added = true;
                break;
            default:
                PSendSysMessage(LANG_ITEM_SLOT_NOT_EXIST,SlotID);
                added = false;
                break;
        }

        if(added)
            PSendSysMessage(LANG_ITEM_ADDED_TO_SLOT,ItemID,tmpItem->Name1,SlotID);
    }
    else
    {
        PSendSysMessage(LANG_ITEM_NOT_FOUND,ItemID);
        return true;
    }
    */
    return true;
}
//----------------------------------------------------------

bool ChatHandler::HandleExploreCheatCommand(char* args)
{
    if (!*args)
        return false;

    int flag = atoi(args);

    Player* chr = getSelectedPlayer();
    if (chr == nullptr)
    {
        SendSysMessage(LANG_NO_CHAR_SELECTED);
        SetSentErrorMessage(true);
        return false;
    }

    if (flag != 0)
    {
        PSendSysMessage(LANG_YOU_SET_EXPLORE_ALL, GetNameLink(chr).c_str());
        if (needReportToTarget(chr))
            ChatHandler(chr).PSendSysMessage(
                LANG_YOURS_EXPLORE_SET_ALL, GetNameLink().c_str());
    }
    else
    {
        PSendSysMessage(LANG_YOU_SET_EXPLORE_NOTHING, GetNameLink(chr).c_str());
        if (needReportToTarget(chr))
            ChatHandler(chr).PSendSysMessage(
                LANG_YOURS_EXPLORE_SET_NOTHING, GetNameLink().c_str());
    }

    for (uint8 i = 0; i < PLAYER_EXPLORED_ZONES_SIZE; ++i)
    {
        if (flag != 0)
        {
            m_session->GetPlayer()->SetFlag(
                PLAYER_EXPLORED_ZONES_1 + i, 0xFFFFFFFF);
        }
        else
        {
            m_session->GetPlayer()->SetFlag(PLAYER_EXPLORED_ZONES_1 + i, 0);
        }
    }

    return true;
}

void ChatHandler::HandleCharacterLevel(
    Player* player, ObjectGuid player_guid, uint32 oldlevel, uint32 newlevel)
{
    if (player)
    {
        player->GiveLevel(newlevel);
        player->InitTalentForLevel();
        player->SetUInt32Value(PLAYER_XP, 0);

        if (needReportToTarget(player))
        {
            if (oldlevel == newlevel)
                ChatHandler(player).PSendSysMessage(
                    LANG_YOURS_LEVEL_PROGRESS_RESET, GetNameLink().c_str());
            else if (oldlevel < newlevel)
                ChatHandler(player).PSendSysMessage(
                    LANG_YOURS_LEVEL_UP, GetNameLink().c_str(), newlevel);
            else // if(oldlevel > newlevel)
                ChatHandler(player).PSendSysMessage(
                    LANG_YOURS_LEVEL_DOWN, GetNameLink().c_str(), newlevel);
        }
    }
    else
    {
        // update level and XP at level, all other will be updated at loading
        CharacterDatabase.PExecute(
            "UPDATE characters SET level = '%u', xp = 0 WHERE guid = '%u'",
            newlevel, player_guid.GetCounter());
    }
}

bool ChatHandler::HandleCharacterLevelCommand(char* args)
{
    char* nameStr = ExtractOptNotLastArg(&args);

    int32 newlevel;
    bool nolevel = false;
    // exception opt second arg: .character level $name
    if (!ExtractInt32(&args, newlevel))
    {
        if (!nameStr)
        {
            nameStr = ExtractArg(&args);
            if (!nameStr)
                return false;

            nolevel = true;
        }
        else
            return false;
    }

    Player* target;
    ObjectGuid target_guid;
    std::string target_name;
    if (!ExtractPlayerTarget(&nameStr, &target, &target_guid, &target_name))
        return false;

    int32 oldlevel =
        target ? target->getLevel() : Player::GetLevelFromDB(target_guid);
    if (nolevel)
        newlevel = oldlevel;

    if (newlevel < 1)
        return false; // invalid level

    if (newlevel > STRONG_MAX_LEVEL) // hardcoded maximum level
        newlevel = STRONG_MAX_LEVEL;

    HandleCharacterLevel(target, target_guid, oldlevel, newlevel);

    if (!m_session ||
        m_session->GetPlayer() != target) // including player==NULL
    {
        std::string nameLink = playerLink(target_name);
        PSendSysMessage(LANG_YOU_CHANGE_LVL, nameLink.c_str(), newlevel);
    }

    return true;
}

bool ChatHandler::HandleLevelUpCommand(char* args)
{
    int32 addlevel = 1;
    char* nameStr = nullptr;

    if (*args)
    {
        nameStr = ExtractOptNotLastArg(&args);

        // exception opt second arg: .levelup $name
        if (!ExtractInt32(&args, addlevel))
        {
            if (!nameStr)
                nameStr = ExtractArg(&args);
            else
                return false;
        }
    }

    Player* target;
    ObjectGuid target_guid;
    std::string target_name;
    if (!ExtractPlayerTarget(&nameStr, &target, &target_guid, &target_name))
        return false;

    int32 oldlevel =
        target ? target->getLevel() : Player::GetLevelFromDB(target_guid);
    int32 newlevel = oldlevel + addlevel;

    if (newlevel < 1)
        newlevel = 1;

    if (newlevel > STRONG_MAX_LEVEL) // hardcoded maximum level
        newlevel = STRONG_MAX_LEVEL;

    HandleCharacterLevel(target, target_guid, oldlevel, newlevel);

    if (!m_session || m_session->GetPlayer() != target) // including chr==NULL
    {
        std::string nameLink = playerLink(target_name);
        PSendSysMessage(LANG_YOU_CHANGE_LVL, nameLink.c_str(), newlevel);
    }

    return true;
}

bool ChatHandler::HandleShowAreaCommand(char* args)
{
    if (!*args)
        return false;

    Player* chr = getSelectedPlayer();
    if (chr == nullptr)
    {
        SendSysMessage(LANG_NO_CHAR_SELECTED);
        SetSentErrorMessage(true);
        return false;
    }

    int area = GetAreaFlagByAreaID(atoi(args));
    int offset = area / 32;
    uint32 val = (uint32)(1 << (area % 32));

    if (area < 0 || offset >= PLAYER_EXPLORED_ZONES_SIZE)
    {
        SendSysMessage(LANG_BAD_VALUE);
        SetSentErrorMessage(true);
        return false;
    }

    uint32 currFields = chr->GetUInt32Value(PLAYER_EXPLORED_ZONES_1 + offset);
    chr->SetUInt32Value(
        PLAYER_EXPLORED_ZONES_1 + offset, (uint32)(currFields | val));

    SendSysMessage(LANG_EXPLORE_AREA);
    return true;
}

bool ChatHandler::HandleHideAreaCommand(char* args)
{
    if (!*args)
        return false;

    Player* chr = getSelectedPlayer();
    if (chr == nullptr)
    {
        SendSysMessage(LANG_NO_CHAR_SELECTED);
        SetSentErrorMessage(true);
        return false;
    }

    int area = GetAreaFlagByAreaID(atoi(args));
    int offset = area / 32;
    uint32 val = (uint32)(1 << (area % 32));

    if (area < 0 || offset >= PLAYER_EXPLORED_ZONES_SIZE)
    {
        SendSysMessage(LANG_BAD_VALUE);
        SetSentErrorMessage(true);
        return false;
    }

    uint32 currFields = chr->GetUInt32Value(PLAYER_EXPLORED_ZONES_1 + offset);
    chr->SetUInt32Value(
        PLAYER_EXPLORED_ZONES_1 + offset, (uint32)(currFields ^ val));

    SendSysMessage(LANG_UNEXPLORE_AREA);
    return true;
}

bool ChatHandler::HandleAuctionAllianceCommand(char* /*args*/)
{
    m_session->GetPlayer()->SetAuctionAccessMode(
        m_session->GetPlayer()->GetTeam() != ALLIANCE ? -1 : 0);
    m_session->SendAuctionHello(m_session->GetPlayer());
    return true;
}

bool ChatHandler::HandleAuctionHordeCommand(char* /*args*/)
{
    m_session->GetPlayer()->SetAuctionAccessMode(
        m_session->GetPlayer()->GetTeam() != HORDE ? -1 : 0);
    m_session->SendAuctionHello(m_session->GetPlayer());
    return true;
}

bool ChatHandler::HandleAuctionGoblinCommand(char* /*args*/)
{
    m_session->GetPlayer()->SetAuctionAccessMode(1);
    m_session->SendAuctionHello(m_session->GetPlayer());
    return true;
}

bool ChatHandler::HandleAuctionCommand(char* /*args*/)
{
    m_session->GetPlayer()->SetAuctionAccessMode(0);
    m_session->SendAuctionHello(m_session->GetPlayer());

    return true;
}

bool ChatHandler::HandleAuctionItemCommand(char* args)
{
    // format: (alliance|horde|goblin) item[:count] price [buyout]
    // [short|long|verylong]
    char* typeStr = ExtractLiteralArg(&args);
    if (!typeStr)
        return false;

    uint32 houseid;
    if (strncmp(typeStr, "alliance", strlen(typeStr)) == 0)
        houseid = 1;
    else if (strncmp(typeStr, "horde", strlen(typeStr)) == 0)
        houseid = 6;
    else if (strncmp(typeStr, "goblin", strlen(typeStr)) == 0)
        houseid = 7;
    else
        return false;

    // parse item str
    char* itemStr = ExtractArg(&args);
    if (!itemStr)
        return false;

    uint32 item_id = 0;
    uint32 item_count = 1;
    if (sscanf(itemStr, "%u:%u", &item_id, &item_count) != 2)
        if (sscanf(itemStr, "%u", &item_id) != 1)
            return false;

    uint32 price;
    if (!ExtractUInt32(&args, price))
        return false;

    uint32 buyout;
    if (!ExtractOptUInt32(&args, buyout, 0))
        return false;

    uint32 etime = 4 * MIN_AUCTION_TIME;
    if (char* timeStr = ExtractLiteralArg(&args))
    {
        if (strncmp(timeStr, "short", strlen(timeStr)) == 0)
            etime = 1 * MIN_AUCTION_TIME;
        else if (strncmp(timeStr, "long", strlen(timeStr)) == 0)
            etime = 2 * MIN_AUCTION_TIME;
        else if (strncmp(timeStr, "verylong", strlen(timeStr)) == 0)
            etime = 4 * MIN_AUCTION_TIME;
        else
            return false;
    }

    AuctionHouseEntry const* auctionHouseEntry =
        sAuctionHouseStore.LookupEntry(houseid);
    AuctionHouseObject* auctionHouse =
        sAuctionMgr::Instance()->GetAuctionsMap(auctionHouseEntry);

    if (!item_id)
    {
        PSendSysMessage(LANG_COMMAND_ITEMIDINVALID, item_id);
        SetSentErrorMessage(true);
        return false;
    }

    ItemPrototype const* item_proto = ObjectMgr::GetItemPrototype(item_id);
    if (!item_proto)
    {
        PSendSysMessage(LANG_COMMAND_ITEMIDINVALID, item_id);
        SetSentErrorMessage(true);
        return false;
    }

    if (item_count < 1 ||
        (item_proto->MaxCount > 0 && item_count > uint32(item_proto->MaxCount)))
    {
        PSendSysMessage(LANG_COMMAND_INVALID_ITEM_COUNT, item_count, item_id);
        SetSentErrorMessage(true);
        return false;
    }

    do
    {
        uint32 item_stack = item_count > item_proto->GetMaxStackSize() ?
                                item_proto->GetMaxStackSize() :
                                item_count;
        item_count -= item_stack;

        Item* newItem = Item::CreateItem(item_id, item_stack);
        assert(newItem);

        auctionHouse->AddAuction(
            auctionHouseEntry, newItem, etime, price, buyout);

    } while (item_count);

    return true;
}

bool ChatHandler::HandleBankCommand(char* /*args*/)
{
    m_session->SendShowBank(m_session->GetPlayer()->GetObjectGuid());

    return true;
}

bool ChatHandler::HandleStableCommand(char* /*args*/)
{
    m_session->SendStablePet(m_session->GetPlayer()->GetObjectGuid());

    return true;
}

bool ChatHandler::HandleChangeWeatherCommand(char* args)
{
    // Weather is OFF
    if (!sWorld::Instance()->getConfig(CONFIG_BOOL_WEATHER))
    {
        SendSysMessage(LANG_WEATHER_DISABLED);
        SetSentErrorMessage(true);
        return false;
    }

    uint32 type;
    if (!ExtractUInt32(&args, type))
        return false;

    // 0 to 3, 0: fine, 1: rain, 2: snow, 3: sand
    if (type > 3)
        return false;

    float grade;
    if (!ExtractFloat(&args, grade))
        return false;

    // 0 to 1, sending -1 is instand good weather
    if (grade < 0.0f || grade > 1.0f)
        return false;

    Player* player = m_session->GetPlayer();
    uint32 zoneid = player->GetZoneId();

    Weather* wth = sWorld::Instance()->FindWeather(zoneid);

    if (!wth)
        wth = sWorld::Instance()->AddWeather(zoneid);
    if (!wth)
    {
        SendSysMessage(LANG_NO_WEATHER);
        SetSentErrorMessage(true);
        return false;
    }

    wth->SetWeather(WeatherType(type), grade);

    return true;
}

bool ChatHandler::HandleListTalentsCommand(char* /*args*/)
{
    Player* player = getSelectedPlayer();
    if (!player)
    {
        SendSysMessage(LANG_NO_CHAR_SELECTED);
        SetSentErrorMessage(true);
        return false;
    }

    SendSysMessage(LANG_LIST_TALENTS_TITLE);
    uint32 count = 0;
    uint32 cost = 0;
    PlayerSpellMap const& uSpells = player->GetSpellMap();
    for (const auto& uSpell : uSpells)
    {
        if (uSpell.second.state == PLAYERSPELL_REMOVED ||
            uSpell.second.disabled)
            continue;

        uint32 cost_itr = GetTalentSpellCost(uSpell.first);

        if (cost_itr == 0)
            continue;

        SpellEntry const* spellEntry = sSpellStore.LookupEntry(uSpell.first);
        if (!spellEntry)
            continue;

        ShowSpellListHelper(player, spellEntry, GetSessionDbcLocale());
        ++count;
        cost += cost_itr;
    }
    PSendSysMessage(LANG_LIST_TALENTS_COUNT, count, cost);

    return true;
}

bool ChatHandler::HandleResetHonorCommand(char* args)
{
    Player* target;
    if (!ExtractPlayerTarget(&args, &target))
        return false;

    target->SetHonorPoints(0);
    target->SetUInt32Value(PLAYER_FIELD_KILLS, 0);
    target->SetUInt32Value(PLAYER_FIELD_LIFETIME_HONORBALE_KILLS, 0);
    target->SetUInt32Value(PLAYER_FIELD_TODAY_CONTRIBUTION, 0);
    target->SetUInt32Value(PLAYER_FIELD_YESTERDAY_CONTRIBUTION, 0);
    return true;
}

static bool HandleResetStatsOrLevelHelper(Player* player)
{
    ChrClassesEntry const* cEntry =
        sChrClassesStore.LookupEntry(player->getClass());
    if (!cEntry)
    {
        logging.error(
            "Class %u not found in DBC (Wrong DBC files?)", player->getClass());
        return false;
    }

    uint8 powertype = cEntry->powerType;

    // reset m_form if no aura
    if (!player->HasAuraType(SPELL_AURA_MOD_SHAPESHIFT))
        player->SetShapeshiftForm(FORM_NONE);

    player->UpdateModelData();

    player->setFactionForRace(player->getRace());

    player->SetByteValue(UNIT_FIELD_BYTES_0, 3, powertype);

    // reset only if player not in some form;
    if (player->GetShapeshiftForm() == FORM_NONE)
        player->InitDisplayIds();

    player->SetByteValue(UNIT_FIELD_BYTES_2, 1,
        UNIT_BYTE2_FLAG_SANCTUARY | UNIT_BYTE2_FLAG_AURASLOT_SETUP_CHNG);

    player->SetUInt32Value(UNIT_FIELD_FLAGS, UNIT_FLAG_PVP_ATTACKABLE);

    //-1 is default value
    player->SetInt32Value(PLAYER_FIELD_WATCHED_FACTION_INDEX, -1);

    // player->SetUInt32Value(PLAYER_FIELD_BYTES, 0xEEE00000 );
    return true;
}

bool ChatHandler::HandleResetLevelCommand(char* args)
{
    Player* target;
    if (!ExtractPlayerTarget(&args, &target))
        return false;

    if (!HandleResetStatsOrLevelHelper(target))
        return false;

    // set starting level
    uint32 start_level =
        sWorld::Instance()->getConfig(CONFIG_UINT32_START_PLAYER_LEVEL);

    target->SetLevel(start_level);
    target->InitStatsForLevel(true);
    target->InitTaxiNodesForLevel();
    target->InitTalentForLevel();
    target->SetUInt32Value(PLAYER_XP, 0);

    // reset level for pet
    if (Pet* pet = target->GetPet())
        pet->SynchronizeLevelWithOwner();

    return true;
}

bool ChatHandler::HandleResetStatsCommand(char* args)
{
    Player* target;
    if (!ExtractPlayerTarget(&args, &target))
        return false;

    if (!HandleResetStatsOrLevelHelper(target))
        return false;

    target->InitStatsForLevel(true);
    target->InitTaxiNodesForLevel();
    target->InitTalentForLevel();

    return true;
}

bool ChatHandler::HandleResetSpellsCommand(char* args)
{
    Player* target;
    ObjectGuid target_guid;
    std::string target_name;
    if (!ExtractPlayerTarget(&args, &target, &target_guid, &target_name))
        return false;

    if (target)
    {
        target->resetSpells();

        ChatHandler(target).SendSysMessage(LANG_RESET_SPELLS);
        if (!m_session || m_session->GetPlayer() != target)
            PSendSysMessage(
                LANG_RESET_SPELLS_ONLINE, GetNameLink(target).c_str());
    }
    else
    {
        CharacterDatabase.PExecute(
            "UPDATE characters SET at_login = at_login | '%u' WHERE guid = "
            "'%u'",
            uint32(AT_LOGIN_RESET_SPELLS), target_guid.GetCounter());
        PSendSysMessage(LANG_RESET_SPELLS_OFFLINE, target_name.c_str());
    }

    return true;
}

bool ChatHandler::HandleResetTalentsCommand(char* args)
{
    Player* target;
    ObjectGuid target_guid;
    std::string target_name;
    if (!ExtractPlayerTarget(&args, &target, &target_guid, &target_name))
        return false;

    if (target)
    {
        target->resetTalents(true);

        ChatHandler(target).SendSysMessage(LANG_RESET_TALENTS);
        if (!m_session || m_session->GetPlayer() != target)
            PSendSysMessage(
                LANG_RESET_TALENTS_ONLINE, GetNameLink(target).c_str());
        return true;
    }
    else if (target_guid)
    {
        uint32 at_flags = AT_LOGIN_RESET_TALENTS;
        CharacterDatabase.PExecute(
            "UPDATE characters SET at_login = at_login | '%u' WHERE guid = "
            "'%u'",
            at_flags, target_guid.GetCounter());
        std::string nameLink = playerLink(target_name);
        PSendSysMessage(LANG_RESET_TALENTS_OFFLINE, nameLink.c_str());
        return true;
    }

    SendSysMessage(LANG_NO_CHAR_SELECTED);
    SetSentErrorMessage(true);
    return false;
}

bool ChatHandler::HandleResetAllCommand(char* args)
{
    if (!*args)
        return false;

    std::string casename = args;

    AtLoginFlags atLogin;

    // Command specially created as single command to prevent using short case
    // names
    if (casename == "spells")
    {
        atLogin = AT_LOGIN_RESET_SPELLS;
        sWorld::Instance()->SendWorldText(LANG_RESETALL_SPELLS);
        if (!m_session)
            SendSysMessage(LANG_RESETALL_SPELLS);
    }
    else if (casename == "talents")
    {
        atLogin = AT_LOGIN_RESET_TALENTS;
        sWorld::Instance()->SendWorldText(LANG_RESETALL_TALENTS);
        if (!m_session)
            SendSysMessage(LANG_RESETALL_TALENTS);
    }
    else
    {
        PSendSysMessage(LANG_RESETALL_UNKNOWN_CASE, args);
        SetSentErrorMessage(true);
        return false;
    }

    CharacterDatabase.PExecute(
        "UPDATE characters SET at_login = at_login | '%u' WHERE (at_login & "
        "'%u') = '0'",
        atLogin, atLogin);
    HashMapHolder<Player>::LockedContainer plist =
        sObjectAccessor::Instance()->GetPlayers();
    for (HashMapHolder<Player>::MapType::const_iterator itr =
             plist.get().begin();
         itr != plist.get().end(); ++itr)
        itr->second->SetAtLoginFlag(atLogin);

    return true;
}

bool ChatHandler::HandleServerShutDownCancelCommand(char* /*args*/)
{
    sWorld::Instance()->ShutdownCancel();
    return true;
}

bool ChatHandler::HandleServerShutDownCommand(char* args)
{
    uint32 delay;
    if (!ExtractUInt32(&args, delay))
        return false;

    uint32 exitcode;
    if (!ExtractOptUInt32(&args, exitcode, SHUTDOWN_EXIT_CODE))
        return false;

    // Exit code should be in range of 0-125, 126-255 is used
    // in many shells for their own return codes and code > 255
    // is not supported in many others
    if (exitcode > 125)
        return false;

    sWorld::Instance()->ShutdownServ(delay, 0, exitcode);
    return true;
}

bool ChatHandler::HandleServerRestartCommand(char* args)
{
    uint32 delay;
    if (!ExtractUInt32(&args, delay))
        return false;

    uint32 exitcode;
    if (!ExtractOptUInt32(&args, exitcode, RESTART_EXIT_CODE))
        return false;

    // Exit code should be in range of 0-125, 126-255 is used
    // in many shells for their own return codes and code > 255
    // is not supported in many others
    if (exitcode > 125)
        return false;

    sWorld::Instance()->ShutdownServ(delay, SHUTDOWN_MASK_RESTART, exitcode);
    return true;
}

bool ChatHandler::HandleServerIdleRestartCommand(char* args)
{
    uint32 delay;
    if (!ExtractUInt32(&args, delay))
        return false;

    uint32 exitcode;
    if (!ExtractOptUInt32(&args, exitcode, RESTART_EXIT_CODE))
        return false;

    // Exit code should be in range of 0-125, 126-255 is used
    // in many shells for their own return codes and code > 255
    // is not supported in many others
    if (exitcode > 125)
        return false;

    sWorld::Instance()->ShutdownServ(
        delay, SHUTDOWN_MASK_RESTART | SHUTDOWN_MASK_IDLE, exitcode);
    return true;
}

bool ChatHandler::HandleServerIdleShutDownCommand(char* args)
{
    uint32 delay;
    if (!ExtractUInt32(&args, delay))
        return false;

    uint32 exitcode;
    if (!ExtractOptUInt32(&args, exitcode, SHUTDOWN_EXIT_CODE))
        return false;

    // Exit code should be in range of 0-125, 126-255 is used
    // in many shells for their own return codes and code > 255
    // is not supported in many others
    if (exitcode > 125)
        return false;

    sWorld::Instance()->ShutdownServ(delay, SHUTDOWN_MASK_IDLE, exitcode);
    return true;
}

bool ChatHandler::HandleSpyCommand(char* args)
{
    auto gm = m_session->GetPlayer();
    if (!gm)
        return false;

    uint32_t tmp;
    uint8_t subgroup;
    if (ExtractUInt32(&args, tmp) && tmp <= 0xFF)
    {
        if (gm->spying_on_)
        {
            SendSysMessage("You're already spying.");
            return true;
        }
    }
    else if (!gm->spying_on_)
    {
        SendSysMessage("Missing argument.");
        SendSysMessage("Usage: .spy <subgroup>");
        return true;
    }
    subgroup = (uint8_t)tmp;

    if (gm->spying_on_)
    {
        auto group = sObjectMgr::Instance()->GetGroupById(gm->spying_on_);
        if (group)
            group->remove_spy(gm);
        else
        {
            WorldPacket data(SMSG_GROUP_LIST, 24);
            data << uint64(0) << uint64(0) << uint64(0);
            m_session->send_packet(std::move(data));
        }
        gm->spying_on_ = 0;
        SendSysMessage("Stopped spying.");
        return true;
    }

    if (gm->GetGroup())
    {
        SendSysMessage("You're in a group already.");
        return true;
    }

    Player* player = getSelectedPlayer();
    if (!player)
    {
        SendSysMessage(LANG_NO_CHAR_SELECTED);
        return true;
    }

    auto group = player->GetGroup();
    if (!group)
    {
        SendSysMessage("Selected player is not in a group.");
        return true;
    }

    gm->spying_on_ = group->GetId();
    gm->spy_subgroup_ = subgroup;
    group->add_spy(gm);
    PSendSysMessage("Started spying on %s's group.", player->GetName());

    return true;
}

bool ChatHandler::HandleQuestAddCommand(char* args)
{
    Player* player = getSelectedPlayer();
    if (!player)
    {
        SendSysMessage(LANG_NO_CHAR_SELECTED);
        SetSentErrorMessage(true);
        return false;
    }

    // .addquest #entry'
    // number or [name] Shift-click form
    // |color|Hquest:quest_id:quest_level|h[name]|h|r
    uint32 entry;
    if (!ExtractUint32KeyFromLink(&args, "Hquest", entry))
        return false;

    Quest const* pQuest = sObjectMgr::Instance()->GetQuestTemplate(entry);
    if (!pQuest)
    {
        PSendSysMessage(LANG_COMMAND_QUEST_NOTFOUND, entry);
        SetSentErrorMessage(true);
        return false;
    }

    // check item starting quest (it can work incorrectly if added without item
    // in inventory)
    for (uint32 id = 0; id < sItemStorage.MaxEntry; ++id)
    {
        ItemPrototype const* pProto =
            sItemStorage.LookupEntry<ItemPrototype>(id);
        if (!pProto)
            continue;

        if (pProto->StartQuest == entry)
        {
            PSendSysMessage(
                LANG_COMMAND_QUEST_STARTFROMITEM, entry, pProto->ItemId);
            SetSentErrorMessage(true);
            return false;
        }
    }

    // ok, normal (creature/GO starting) quest
    if (player->CanAddQuest(pQuest, true))
    {
        player->AddQuest(pQuest, nullptr);

        if (player->CanCompleteQuest(entry))
            player->CompleteQuest(entry);
    }

    return true;
}

bool ChatHandler::HandleQuestRemoveCommand(char* args)
{
    Player* player = getSelectedPlayer();
    if (!player)
    {
        SendSysMessage(LANG_NO_CHAR_SELECTED);
        SetSentErrorMessage(true);
        return false;
    }

    // .removequest #entry'
    // number or [name] Shift-click form
    // |color|Hquest:quest_id:quest_level|h[name]|h|r
    uint32 entry;
    if (!ExtractUint32KeyFromLink(&args, "Hquest", entry))
        return false;

    Quest const* pQuest = sObjectMgr::Instance()->GetQuestTemplate(entry);

    if (!pQuest)
    {
        PSendSysMessage(LANG_COMMAND_QUEST_NOTFOUND, entry);
        SetSentErrorMessage(true);
        return false;
    }

    // remove all quest entries for 'entry' from quest log
    for (uint8 slot = 0; slot < MAX_QUEST_LOG_SIZE; ++slot)
    {
        uint32 quest = player->GetQuestSlotQuestId(slot);
        if (quest == entry)
        {
            player->SetQuestSlot(slot, 0);

            // we ignore unequippable quest items in this case, its' still be
            // equipped
            player->TakeQuestSourceItem(quest, false);
        }
    }

    // set quest status to not started (will updated in DB at next save)
    player->SetQuestStatus(entry, QUEST_STATUS_NONE);

    // reset rewarded for restart repeatable quest
    player->getQuestStatusMap()[entry].m_rewarded = false;

    SendSysMessage(LANG_COMMAND_QUEST_REMOVED);
    return true;
}

bool ChatHandler::HandleQuestCompleteCommand(char* args)
{
    Player* player = getSelectedPlayer();
    if (!player)
    {
        SendSysMessage(LANG_NO_CHAR_SELECTED);
        SetSentErrorMessage(true);
        return false;
    }

    // .quest complete #entry
    // number or [name] Shift-click form
    // |color|Hquest:quest_id:quest_level|h[name]|h|r
    uint32 entry;
    if (!ExtractUint32KeyFromLink(&args, "Hquest", entry))
        return false;

    Quest const* pQuest = sObjectMgr::Instance()->GetQuestTemplate(entry);

    // If player doesn't have the quest
    if (!pQuest || player->GetQuestStatus(entry) == QUEST_STATUS_NONE)
    {
        PSendSysMessage(LANG_COMMAND_QUEST_NOTFOUND, entry);
        SetSentErrorMessage(true);
        return false;
    }

    inventory::transaction trans;
    // Add quest items for quests that require items
    for (uint8 x = 0; x < QUEST_ITEM_OBJECTIVES_COUNT; ++x)
    {
        uint32 id = pQuest->ReqItemId[x];
        uint32 count = pQuest->ReqItemCount[x];
        if (!id || !count)
            continue;

        uint32 has = player->storage().item_count(id);
        if (has >= count)
            continue;

        trans.add(id, count - has);
    }
    if (!player->storage().finalize(trans))
    {
        PSendSysMessage(
            "Unable to complete quest: %u. Could not add quest items",
            entry); // XXX: Report error
    }

    // All creature/GO slain/casted (not required, but otherwise it will display
    // "Creature slain 0/10")
    for (uint8 i = 0; i < QUEST_OBJECTIVES_COUNT; ++i)
    {
        int32 creature = pQuest->ReqCreatureOrGOId[i];
        uint32 creaturecount = pQuest->ReqCreatureOrGOCount[i];

        if (uint32 spell_id = pQuest->ReqSpell[i])
        {
            for (uint16 z = 0; z < creaturecount; ++z)
                player->CastedCreatureOrGO(creature, ObjectGuid(), spell_id);
        }
        else if (creature > 0)
        {
            if (CreatureInfo const* cInfo =
                    ObjectMgr::GetCreatureTemplate(creature))
                for (uint16 z = 0; z < creaturecount; ++z)
                    player->KilledMonster(cInfo, ObjectGuid());
        }
        else if (creature < 0)
        {
            for (uint16 z = 0; z < creaturecount; ++z)
                player->CastedCreatureOrGO(-creature, ObjectGuid(), 0);
        }
    }

    // If the quest requires reputation to complete
    if (uint32 repFaction = pQuest->GetRepObjectiveFaction())
    {
        uint32 repValue = pQuest->GetRepObjectiveValue();
        uint32 curRep = player->GetReputationMgr().GetReputation(repFaction);
        if (curRep < repValue)
            if (FactionEntry const* factionEntry =
                    sFactionStore.LookupEntry(repFaction))
                player->GetReputationMgr().SetReputation(
                    factionEntry, repValue);
    }

    // If the quest requires money, we add it to the target
    int32 ReqOrRewMoney = pQuest->GetRewOrReqMoney();
    if (ReqOrRewMoney < 0)
    {
        // XXX
        inventory::transaction trans;
        trans.add(-ReqOrRewMoney);
        player->storage().finalize(trans);
    }

    player->CompleteQuest(entry);
    return true;
}

bool ChatHandler::HandleBanAccountCommand(char* args)
{
    return HandleBanHelper(BAN_ACCOUNT, args);
}

bool ChatHandler::HandleBanCharacterCommand(char* args)
{
    return HandleBanHelper(BAN_CHARACTER, args);
}

bool ChatHandler::HandleBanIPCommand(char* args)
{
    return HandleBanHelper(BAN_IP, args);
}

bool ChatHandler::HandleBanHelper(BanMode mode, char* args)
{
    if (!*args)
        return false;

    char* cnameOrIP = ExtractArg(&args);
    if (!cnameOrIP)
        return false;

    std::string nameOrIP = cnameOrIP;

    char* duration = ExtractArg(&args); // time string
    if (!duration)
        return false;

    uint32 duration_secs = TimeStringToSecs(duration);

    char* reason = ExtractArg(&args);
    if (!reason)
        return false;

    switch (mode)
    {
    case BAN_ACCOUNT:
        if (!AccountMgr::normalizeString(nameOrIP))
        {
            PSendSysMessage(LANG_ACCOUNT_NOT_EXIST, nameOrIP.c_str());
            SetSentErrorMessage(true);
            return false;
        }
        break;
    case BAN_CHARACTER:
        if (!normalizePlayerName(nameOrIP))
        {
            SendSysMessage(LANG_PLAYER_NOT_FOUND);
            SetSentErrorMessage(true);
            return false;
        }
        break;
    case BAN_IP:
        if (!IsIPAddress(nameOrIP.c_str()))
            return false;
        break;
    }

    switch (sWorld::Instance()->BanAccount(mode, nameOrIP, duration_secs,
        reason, m_session ? m_session->GetPlayerName() : ""))
    {
    case BAN_SUCCESS:
        if (duration_secs > 0)
            PSendSysMessage(LANG_BAN_YOUBANNED, nameOrIP.c_str(),
                secsToTimeString(duration_secs, true).c_str(), reason);
        else
            PSendSysMessage(LANG_BAN_YOUPERMBANNED, nameOrIP.c_str(), reason);
        break;
    case BAN_SYNTAX_ERROR:
        return false;
    case BAN_NOTFOUND:
        switch (mode)
        {
        default:
            PSendSysMessage(LANG_BAN_NOTFOUND, "account", nameOrIP.c_str());
            break;
        case BAN_CHARACTER:
            PSendSysMessage(LANG_BAN_NOTFOUND, "character", nameOrIP.c_str());
            break;
        case BAN_IP:
            PSendSysMessage(LANG_BAN_NOTFOUND, "ip", nameOrIP.c_str());
            break;
        }
        SetSentErrorMessage(true);
        return false;
    }

    return true;
}

bool ChatHandler::HandleUnBanAccountCommand(char* args)
{
    return HandleUnBanHelper(BAN_ACCOUNT, args);
}

bool ChatHandler::HandleUnBanCharacterCommand(char* args)
{
    return HandleUnBanHelper(BAN_CHARACTER, args);
}

bool ChatHandler::HandleUnBanIPCommand(char* args)
{
    return HandleUnBanHelper(BAN_IP, args);
}

bool ChatHandler::HandleUnBanHelper(BanMode mode, char* args)
{
    if (!*args)
        return false;

    char* cnameOrIP = ExtractArg(&args);
    if (!cnameOrIP)
        return false;

    std::string nameOrIP = cnameOrIP;

    switch (mode)
    {
    case BAN_ACCOUNT:
        if (!AccountMgr::normalizeString(nameOrIP))
        {
            PSendSysMessage(LANG_ACCOUNT_NOT_EXIST, nameOrIP.c_str());
            SetSentErrorMessage(true);
            return false;
        }
        break;
    case BAN_CHARACTER:
        if (!normalizePlayerName(nameOrIP))
        {
            SendSysMessage(LANG_PLAYER_NOT_FOUND);
            SetSentErrorMessage(true);
            return false;
        }
        break;
    case BAN_IP:
        if (!IsIPAddress(nameOrIP.c_str()))
            return false;
        break;
    }

    if (sWorld::Instance()->RemoveBanAccount(mode, nameOrIP))
        PSendSysMessage(LANG_UNBAN_UNBANNED, nameOrIP.c_str());
    else
        PSendSysMessage(LANG_UNBAN_ERROR, nameOrIP.c_str());

    return true;
}

bool ChatHandler::HandleBanInfoAccountCommand(char* args)
{
    if (!*args)
        return false;

    std::string account_name;
    uint32 accountid = ExtractAccountId(&args, &account_name);
    if (!accountid)
        return false;

    return HandleBanInfoHelper(accountid, account_name.c_str());
}

bool ChatHandler::HandleBanInfoCharacterCommand(char* args)
{
    Player* target;
    ObjectGuid target_guid;
    if (!ExtractPlayerTarget(&args, &target, &target_guid))
        return false;

    uint32 accountid =
        target ? target->GetSession()->GetAccountId() :
                 sObjectMgr::Instance()->GetPlayerAccountIdByGUID(target_guid);

    std::string accountname;
    if (!sAccountMgr::Instance()->GetName(accountid, accountname))
    {
        PSendSysMessage(LANG_BANINFO_NOCHARACTER);
        return true;
    }

    return HandleBanInfoHelper(accountid, accountname.c_str());
}

bool ChatHandler::HandleBanInfoHelper(uint32 accountid, char const* accountname)
{
    std::unique_ptr<QueryResult> result(LoginDatabase.PQuery(
        "SELECT FROM_UNIXTIME(bandate), unbandate-bandate, active, "
        "unbandate,banreason,bannedby FROM account_banned WHERE id = '%u' "
        "ORDER BY bandate ASC",
        accountid));
    if (!result)
    {
        PSendSysMessage(LANG_BANINFO_NOACCOUNTBAN, accountname);
        return true;
    }

    PSendSysMessage(LANG_BANINFO_BANHISTORY, accountname);
    do
    {
        Field* fields = result->Fetch();

        time_t unbandate = time_t(fields[3].GetUInt64());
        bool active = false;
        if (fields[2].GetBool() &&
            (fields[1].GetUInt64() == (uint64)0 ||
                unbandate >= WorldTimer::time_no_syscall()))
            active = true;
        bool permanent = (fields[1].GetUInt64() == (uint64)0);
        std::string bantime = permanent ?
                                  GetMangosString(LANG_BANINFO_INFINITE) :
                                  secsToTimeString(fields[1].GetUInt64(), true);
        PSendSysMessage(LANG_BANINFO_HISTORYENTRY, fields[0].GetString(),
            bantime.c_str(), active ? GetMangosString(LANG_BANINFO_YES) :
                                      GetMangosString(LANG_BANINFO_NO),
            fields[4].GetString(), fields[5].GetString());
    } while (result->NextRow());

    return true;
}

bool ChatHandler::HandleBanInfoIPCommand(char* args)
{
    if (!*args)
        return false;

    char* cIP = ExtractQuotedOrLiteralArg(&args);
    if (!cIP)
        return false;

    if (!IsIPAddress(cIP))
        return false;

    std::string IP = cIP;

    LoginDatabase.escape_string(IP);
    std::unique_ptr<QueryResult> result(LoginDatabase.PQuery(
        "SELECT ip, FROM_UNIXTIME(bandate), FROM_UNIXTIME(unbandate), "
        "unbandate-UNIX_TIMESTAMP(), banreason,bannedby,unbandate-bandate FROM "
        "ip_banned WHERE ip = '%s'",
        IP.c_str()));
    if (!result)
    {
        PSendSysMessage(LANG_BANINFO_NOIP);
        return true;
    }

    Field* fields = result->Fetch();
    bool permanent = !fields[6].GetUInt64();
    PSendSysMessage(LANG_BANINFO_IPENTRY, fields[0].GetString(),
        fields[1].GetString(),
        permanent ? GetMangosString(LANG_BANINFO_NEVER) : fields[2].GetString(),
        permanent ? GetMangosString(LANG_BANINFO_INFINITE) :
                    secsToTimeString(fields[3].GetUInt64(), true).c_str(),
        fields[4].GetString(), fields[5].GetString());
    return true;
}

bool ChatHandler::HandleBanListCharacterCommand(char* args)
{
    LoginDatabase.Execute(
        "DELETE FROM ip_banned WHERE unbandate<=UNIX_TIMESTAMP() AND "
        "unbandate<>bandate");

    char* cFilter = ExtractLiteralArg(&args);
    if (!cFilter)
        return false;

    std::string filter = cFilter;
    LoginDatabase.escape_string(filter);
    std::unique_ptr<QueryResult> result(CharacterDatabase.PQuery(
        "SELECT account FROM characters WHERE name " _LIKE_
        " " _CONCAT3_("'%%'", "'%s'", "'%%'"),
        filter.c_str()));
    if (!result)
    {
        PSendSysMessage(LANG_BANLIST_NOCHARACTER);
        return true;
    }

    return HandleBanListHelper(result.get());
}

bool ChatHandler::HandleBanListAccountCommand(char* args)
{
    LoginDatabase.Execute(
        "DELETE FROM ip_banned WHERE unbandate<=UNIX_TIMESTAMP() AND "
        "unbandate<>bandate");

    char* cFilter = ExtractLiteralArg(&args);
    std::string filter = cFilter ? cFilter : "";
    LoginDatabase.escape_string(filter);

    std::unique_ptr<QueryResult> result;

    if (filter.empty())
    {
        result.reset(LoginDatabase.Query(
            "SELECT account.id, username FROM account, account_banned"
            " WHERE account.id = account_banned.id AND active = 1 GROUP BY "
            "account.id"));
    }
    else
    {
        result.reset(LoginDatabase.PQuery(
            "SELECT account.id, username FROM account, account_banned"
            " WHERE account.id = account_banned.id AND active = 1 AND "
            "username " _LIKE_
            " " _CONCAT3_("'%%'", "'%s'", "'%%'") " GROUP BY account.id",
            filter.c_str()));
    }

    if (!result)
    {
        PSendSysMessage(LANG_BANLIST_NOACCOUNT);
        return true;
    }

    return HandleBanListHelper(result.get());
}

bool ChatHandler::HandleBanListHelper(QueryResult* result)
{
    PSendSysMessage(LANG_BANLIST_MATCHINGACCOUNT);

    // Chat short output
    if (m_session)
    {
        do
        {
            Field* fields = result->Fetch();
            uint32 accountid = fields[0].GetUInt32();

            std::unique_ptr<QueryResult> banresult(LoginDatabase.PQuery(
                "SELECT account.username FROM account,account_banned WHERE "
                "account_banned.id='%u' AND account_banned.id=account.id",
                accountid));
            if (banresult)
            {
                Field* fields2 = banresult->Fetch();
                PSendSysMessage("%s", fields2[0].GetString());
            }
        } while (result->NextRow());
    }
    // Console wide output
    else
    {
        SendSysMessage(LANG_BANLIST_ACCOUNTS);
        SendSysMessage(
            "=================================================================="
            "=============");
        SendSysMessage(LANG_BANLIST_ACCOUNTS_HEADER);
        do
        {
            SendSysMessage(
                "--------------------------------------------------------------"
                "-----------------");
            Field* fields = result->Fetch();
            uint32 account_id = fields[0].GetUInt32();

            std::string account_name;

            // "account" case, name can be get in same query
            if (result->GetFieldCount() > 1)
                account_name = fields[1].GetCppString();
            // "character" case, name need extract from another DB
            else
                sAccountMgr::Instance()->GetName(account_id, account_name);

            // No SQL injection. id is uint32.
            std::unique_ptr<QueryResult> banInfo(LoginDatabase.PQuery(
                "SELECT bandate,unbandate,bannedby,banreason FROM "
                "account_banned WHERE id = %u ORDER BY unbandate",
                account_id));
            if (banInfo)
            {
                Field* fields2 = banInfo->Fetch();
                do
                {
                    time_t t_ban = fields2[0].GetUInt64();
                    tm* aTm_ban = localtime(&t_ban);

                    if (fields2[0].GetUInt64() == fields2[1].GetUInt64())
                    {
                        PSendSysMessage(
                            "|%-15.15s|%02d-%02d-%02d %02d:%02d|   permanent  "
                            "|%-15.15s|%-15.15s|",
                            account_name.c_str(), aTm_ban->tm_year % 100,
                            aTm_ban->tm_mon + 1, aTm_ban->tm_mday,
                            aTm_ban->tm_hour, aTm_ban->tm_min,
                            fields2[2].GetString(), fields2[3].GetString());
                    }
                    else
                    {
                        time_t t_unban = fields2[1].GetUInt64();
                        tm* aTm_unban = localtime(&t_unban);
                        PSendSysMessage(
                            "|%-15.15s|%02d-%02d-%02d %02d:%02d|%02d-%02d-%02d "
                            "%02d:%02d|%-15.15s|%-15.15s|",
                            account_name.c_str(), aTm_ban->tm_year % 100,
                            aTm_ban->tm_mon + 1, aTm_ban->tm_mday,
                            aTm_ban->tm_hour, aTm_ban->tm_min,
                            aTm_unban->tm_year % 100, aTm_unban->tm_mon + 1,
                            aTm_unban->tm_mday, aTm_unban->tm_hour,
                            aTm_unban->tm_min, fields2[2].GetString(),
                            fields2[3].GetString());
                    }
                } while (banInfo->NextRow());
            }
        } while (result->NextRow());
        SendSysMessage(
            "=================================================================="
            "=============");
    }

    return true;
}

bool ChatHandler::HandleBanListIPCommand(char* args)
{
    LoginDatabase.Execute(
        "DELETE FROM ip_banned WHERE unbandate<=UNIX_TIMESTAMP() AND "
        "unbandate<>bandate");

    char* cFilter = ExtractLiteralArg(&args);
    std::string filter = cFilter ? cFilter : "";
    LoginDatabase.escape_string(filter);

    QueryResult* result;

    if (filter.empty())
    {
        result = LoginDatabase.Query(
            "SELECT ip,bandate,unbandate,bannedby,banreason FROM ip_banned"
            " WHERE (bandate=unbandate OR unbandate>UNIX_TIMESTAMP())"
            " ORDER BY unbandate");
    }
    else
    {
        result = LoginDatabase.PQuery(
            "SELECT ip,bandate,unbandate,bannedby,banreason FROM ip_banned"
            " WHERE (bandate=unbandate OR unbandate>UNIX_TIMESTAMP()) AND "
            "ip " _LIKE_
            " " _CONCAT3_("'%%'", "'%s'", "'%%'") " ORDER BY unbandate",
            filter.c_str());
    }

    if (!result)
    {
        PSendSysMessage(LANG_BANLIST_NOIP);
        return true;
    }

    PSendSysMessage(LANG_BANLIST_MATCHINGIP);
    // Chat short output
    if (m_session)
    {
        do
        {
            Field* fields = result->Fetch();
            PSendSysMessage("%s", fields[0].GetString());
        } while (result->NextRow());
    }
    // Console wide output
    else
    {
        SendSysMessage(LANG_BANLIST_IPS);
        SendSysMessage(
            "=================================================================="
            "=============");
        SendSysMessage(LANG_BANLIST_IPS_HEADER);
        do
        {
            SendSysMessage(
                "--------------------------------------------------------------"
                "-----------------");
            Field* fields = result->Fetch();
            time_t t_ban = fields[1].GetUInt64();
            tm* aTm_ban = localtime(&t_ban);
            if (fields[1].GetUInt64() == fields[2].GetUInt64())
            {
                PSendSysMessage(
                    "|%-15.15s|%02d-%02d-%02d %02d:%02d|   permanent  "
                    "|%-15.15s|%-15.15s|",
                    fields[0].GetString(), aTm_ban->tm_year % 100,
                    aTm_ban->tm_mon + 1, aTm_ban->tm_mday, aTm_ban->tm_hour,
                    aTm_ban->tm_min, fields[3].GetString(),
                    fields[4].GetString());
            }
            else
            {
                time_t t_unban = fields[2].GetUInt64();
                tm* aTm_unban = localtime(&t_unban);
                PSendSysMessage(
                    "|%-15.15s|%02d-%02d-%02d %02d:%02d|%02d-%02d-%02d "
                    "%02d:%02d|%-15.15s|%-15.15s|",
                    fields[0].GetString(), aTm_ban->tm_year % 100,
                    aTm_ban->tm_mon + 1, aTm_ban->tm_mday, aTm_ban->tm_hour,
                    aTm_ban->tm_min, aTm_unban->tm_year % 100,
                    aTm_unban->tm_mon + 1, aTm_unban->tm_mday,
                    aTm_unban->tm_hour, aTm_unban->tm_min,
                    fields[3].GetString(), fields[4].GetString());
            }
        } while (result->NextRow());
        SendSysMessage(
            "=================================================================="
            "=============");
    }

    delete result;
    return true;
}

bool ChatHandler::HandleRespawnCommand(char* /*args*/)
{
    Player* pl = m_session->GetPlayer();

    // accept only explicitly selected target (not implicitly self targeting
    // case)
    Unit* target = getSelectedUnit();
    if (pl->GetSelectionGuid() && target)
    {
        if (target->GetTypeId() != TYPEID_UNIT)
        {
            SendSysMessage(LANG_SELECT_CREATURE);
            SetSentErrorMessage(true);
            return false;
        }

        if (target->isDead())
            ((Creature*)target)->Respawn();
        return true;
    }

    maps::visitors::simple<Creature>{}(pl,
        pl->GetMap()->GetVisibilityDistance(), [](Creature* c)
        {
            // prevent respawn creatures for not active BG event
            Map* map = c->GetMap();
            if (map->IsBattleGroundOrArena())
            {
                BattleGroundEventIdx eventId =
                    sBattleGroundMgr::Instance()->GetCreatureEventIndex(
                        c->GetGUIDLow());
                if (!((BattleGroundMap*)map)
                         ->GetBG()
                         ->IsActiveEvent(eventId.event1, eventId.event2))
                    return;
            }

            c->Respawn();
        });

    return true;
}

bool ChatHandler::HandleGMFlyCommand(char* args)
{
    bool value;
    if (!ExtractOnOff(&args, value))
    {
        SendSysMessage(LANG_USE_BOL);
        SetSentErrorMessage(true);
        return false;
    }

    Player* target = getSelectedPlayer();
    if (!target)
        target = m_session->GetPlayer();

    target->set_gm_fly_mode(value);
    PSendSysMessage(LANG_COMMAND_FLYMODE_STATUS, GetNameLink(target).c_str(),
        value ? "is now ON" : "is now OFF");
    return true;
}

bool ChatHandler::HandleMovegensCommand(char* args)
{
    Unit* unit = getSelectedUnit();
    if (!unit)
    {
        SendSysMessage("You need to select a unit first.");
        SetSentErrorMessage(true);
        return false;
    }

    PSendSysMessage("Movement priority queue for %s:", unit->GetName());

    for (auto& gen : unit->movement_gens)
    {
        auto debug = gen->debug_str();
        if (!debug.empty())
        {
            PSendSysMessage("%s (%d) -- %s",
                movement::generator_name(gen->id()), gen->priority(),
                debug.c_str());
            continue;
        }
        PSendSysMessage(
            "%s (%d)", movement::generator_name(gen->id()), gen->priority());
    }

    return true;
}

bool ChatHandler::HandleSplinemakeCommand(char* args)
{
    uint32 id;
    if (!ExtractUInt32(&args, id))
    {
        std::unique_ptr<QueryResult> res(
            WorldDatabase.PQuery("SELECT MAX(id) FROM splines"));
        SendSysMessage("Usage:");
        SendSysMessage(".splinemake <id> \"description\"");
        if (res)
            PSendSysMessage(
                "Next free spline id: %u", res->Fetch()[0].GetUInt32() + 1);
        return true;
    }

    char* description = ExtractQuotedArg(&args);
    if (!description)
    {
        SendSysMessage("Usage:");
        SendSysMessage(".splinemake <id> \"description\"");
        return true;
    }
    std::string desc(description);

    std::unique_ptr<QueryResult> res(WorldDatabase.PQuery(
        "SELECT COUNT(point) FROM splines WHERE id=%u", id));
    uint32 point = 0;
    if (res)
        point = res->Fetch()[0].GetUInt32();

    WorldDatabase.escape_string(desc);

    WorldDatabase.PExecute(
        "INSERT INTO splines (id, point, x, y, z, description) "
        "VALUES(%u, %u, %f, %f, %f, \"%s\")",
        id, point, m_session->GetPlayer()->GetX(),
        m_session->GetPlayer()->GetY(), m_session->GetPlayer()->GetZ(),
        desc.c_str());
    PSendSysMessage("Inserted point %u of spline %u", point, id);

    return true;
}

bool ChatHandler::HandleSplineplayCommand(char* args)
{
    uint32 id;
    if (!ExtractUInt32(&args, id))
    {
        SendSysMessage("Usage:");
        SendSysMessage(".splineplay <id>");
        return true;
    }

    Unit* unit = getSelectedUnit();
    if (!unit || unit->GetTypeId() != TYPEID_UNIT)
    {
        SendSysMessage("You need to select an NPC.");
        return true;
    }

    unit->movement_gens.push(new movement::SplineMovementGenerator(id, false));

    return true;
}

bool ChatHandler::HandleServerPLimitCommand(char* args)
{
    if (*args)
    {
        char* param = ExtractLiteralArg(&args);
        if (!param)
            return false;

        int l = strlen(param);

        int val;
        if (strncmp(param, "player", l) == 0)
            sWorld::Instance()->SetPlayerLimit(-SEC_PLAYER);
        else if (strncmp(param, "moderator", l) == 0)
            sWorld::Instance()->SetPlayerLimit(-SEC_TICKET_GM);
        else if (strncmp(param, "gamemaster", l) == 0)
            sWorld::Instance()->SetPlayerLimit(-SEC_POWER_GM);
        else if (strncmp(param, "administrator", l) == 0)
            sWorld::Instance()->SetPlayerLimit(-SEC_FULL_GM);
        else if (strncmp(param, "reset", l) == 0)
            sWorld::Instance()->SetPlayerLimit(
                sConfig::Instance()->GetIntDefault(
                    "PlayerLimit", DEFAULT_PLAYER_LIMIT));
        else if (ExtractInt32(&param, val))
        {
            if (val < -SEC_FULL_GM)
                val = -SEC_FULL_GM;

            sWorld::Instance()->SetPlayerLimit(val);
        }
        else
            return false;

        // kick all low security level players
        if (sWorld::Instance()->GetPlayerAmountLimit() > SEC_PLAYER)
            sWorld::Instance()->KickAllLess(
                sWorld::Instance()->GetPlayerSecurityLimit());
    }

    uint32 pLimit = sWorld::Instance()->GetPlayerAmountLimit();
    AccountTypes allowedAccountType =
        sWorld::Instance()->GetPlayerSecurityLimit();
    char const* secName = "";
    switch (allowedAccountType)
    {
    case SEC_PLAYER:
        secName = "Player";
        break;
    case SEC_TICKET_GM:
        secName = "Moderator";
        break;
    case SEC_POWER_GM:
        secName = "Gamemaster";
        break;
    case SEC_FULL_GM:
        secName = "Administrator";
        break;
    default:
        secName = "<unknown>";
        break;
    }

    PSendSysMessage(
        "Player limits: amount %u, min. security level %s.", pLimit, secName);

    return true;
}

bool ChatHandler::HandleCastCommand(char* args)
{
    if (!*args)
        return false;

    Unit* target = getSelectedUnit();

    if (!target)
    {
        SendSysMessage(LANG_SELECT_CHAR_OR_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    // number or [name] Shift-click form |color|Hspell:spell_id|h[name]|h|r or
    // Htalent form
    uint32 spell = ExtractSpellIdFromLink(&args);
    if (!spell)
        return false;

    SpellEntry const* spellInfo = sSpellStore.LookupEntry(spell);
    if (!spellInfo)
        return false;

    if (!SpellMgr::IsSpellValid(spellInfo, m_session->GetPlayer()))
    {
        PSendSysMessage(LANG_COMMAND_SPELL_BROKEN, spell);
        SetSentErrorMessage(true);
        return false;
    }

    bool triggered = ExtractLiteralArg(&args, "triggered") != nullptr;
    if (!triggered && *args) // can be fail also at syntax error
        return false;

    m_session->GetPlayer()->CastSpell(target, spell, triggered);

    return true;
}

bool ChatHandler::HandleCastBackCommand(char* args)
{
    Creature* caster = getSelectedCreature();

    if (!caster)
    {
        SendSysMessage(LANG_SELECT_CHAR_OR_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    // number or [name] Shift-click form |color|Hspell:spell_id|h[name]|h|r
    // number or [name] Shift-click form |color|Hspell:spell_id|h[name]|h|r or
    // Htalent form
    uint32 spell = ExtractSpellIdFromLink(&args);
    if (!spell || !sSpellStore.LookupEntry(spell))
        return false;

    bool triggered = ExtractLiteralArg(&args, "triggered") != nullptr;
    if (!triggered && *args) // can be fail also at syntax error
        return false;

    caster->SetFacingToObject(m_session->GetPlayer());

    caster->CastSpell(m_session->GetPlayer(), spell, triggered);

    return true;
}

bool ChatHandler::HandleCastDistCommand(char* args)
{
    if (!*args)
        return false;

    // number or [name] Shift-click form |color|Hspell:spell_id|h[name]|h|r or
    // Htalent form
    uint32 spell = ExtractSpellIdFromLink(&args);
    if (!spell)
        return false;

    SpellEntry const* spellInfo = sSpellStore.LookupEntry(spell);
    if (!spellInfo)
        return false;

    if (!SpellMgr::IsSpellValid(spellInfo, m_session->GetPlayer()))
    {
        PSendSysMessage(LANG_COMMAND_SPELL_BROKEN, spell);
        SetSentErrorMessage(true);
        return false;
    }

    float dist;
    if (!ExtractFloat(&args, dist))
        return false;

    bool triggered = ExtractLiteralArg(&args, "triggered") != nullptr;
    if (!triggered && *args) // can be fail also at syntax error
        return false;

    auto pos = m_session->GetPlayer()->GetPoint(0.0f, dist);

    m_session->GetPlayer()->CastSpell(pos.x, pos.y, pos.z, spell, triggered);
    return true;
}

bool ChatHandler::HandleCastTargetCommand(char* args)
{
    Creature* caster = getSelectedCreature();

    if (!caster)
    {
        SendSysMessage(LANG_SELECT_CHAR_OR_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    if (!caster->getVictim())
    {
        SendSysMessage(LANG_SELECTED_TARGET_NOT_HAVE_VICTIM);
        SetSentErrorMessage(true);
        return false;
    }

    // number or [name] Shift-click form |color|Hspell:spell_id|h[name]|h|r or
    // Htalent form
    uint32 spell = ExtractSpellIdFromLink(&args);
    if (!spell || !sSpellStore.LookupEntry(spell))
        return false;

    bool triggered = ExtractLiteralArg(&args, "triggered") != nullptr;
    if (!triggered && *args) // can be fail also at syntax error
        return false;

    caster->SetFacingToObject(m_session->GetPlayer());

    caster->CastSpell(caster->getVictim(), spell, triggered);

    return true;
}

bool ChatHandler::HandleComeToMeCommand(char* /*args*/)
{
    Creature* target = getSelectedCreature();

    if (!target)
    {
        SendSysMessage(LANG_SELECT_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    Player* pl = m_session->GetPlayer();
    target->movement_gens.push(new movement::PointMovementGenerator(
        0, pl->GetX(), pl->GetY(), pl->GetZ(), true, true));
    return true;
}

bool ChatHandler::HandleCastSelfCommand(char* args)
{
    if (!*args)
        return false;

    Unit* target = getSelectedUnit();

    if (!target)
    {
        SendSysMessage(LANG_SELECT_CHAR_OR_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    // number or [name] Shift-click form |color|Hspell:spell_id|h[name]|h|r or
    // Htalent form
    uint32 spell = ExtractSpellIdFromLink(&args);
    if (!spell)
        return false;

    SpellEntry const* spellInfo = sSpellStore.LookupEntry(spell);
    if (!spellInfo)
        return false;

    if (!SpellMgr::IsSpellValid(spellInfo, m_session->GetPlayer()))
    {
        PSendSysMessage(LANG_COMMAND_SPELL_BROKEN, spell);
        SetSentErrorMessage(true);
        return false;
    }

    bool triggered = ExtractLiteralArg(&args, "triggered") != nullptr;
    if (!triggered && *args) // can be fail also at syntax error
        return false;

    target->CastSpell(target, spell, triggered);

    return true;
}

bool ChatHandler::HandleInstanceListBindsCommand(char* args)
{
    Player* player = getSelectedPlayer();
    if (!player)
        player = m_session->GetPlayer();

    char* arg = ExtractLiteralArg(&args);
    if (!arg || (strcmp(arg, "player") != 0 && strcmp(arg, "group") != 0))
    {
        SendSysMessage("Append command with \"player\" or \"group\".");
        return true;
    }

    if (strcmp(arg, "player") == 0)
    {
        PSendSysMessage("Listing binds for %s:",
            player->GetObjectGuid().GetString().c_str());

        for (int i = 0; i < MAX_DIFFICULTY; ++i)
        {
            for (auto& elem : player->GetInstanceBindsMap((Difficulty)i))
            {
                auto state = elem.second.state.lock();
                if (!state)
                    continue;
                std::string TTL = secsToTimeString(
                    state->GetResetTime() - WorldTimer::time_no_syscall(),
                    true);
                PSendSysMessage("%s (%s) | mapid: %u binds: " SIZEFMTD
                                " co-owners: " SIZEFMTD " perm: %s TTL: %s.",
                    state->GetMapEntry()->name[GetSessionDbcLocale()],
                    state->GetDifficulty() == DUNGEON_DIFFICULTY_NORMAL ?
                        "normal" :
                        "heroic",
                    state->GetMapId(), state->GetBoundPlayers().size(),
                    state->GetCoOwners().size(),
                    state->CanReset() ? "no" : "yes", TTL.c_str());
            }
        }
    }
    else
    {
        Group* group = player->GetGroup();
        if (!group)
        {
            SendSysMessage("Player has no group.");
            return true;
        }

        PSendSysMessage("Listing binds for group with Id %u and Leader: %s:",
            group->GetId(), group->GetLeaderGuid().GetString().c_str());

        for (int i = 0; i < MAX_DIFFICULTY; ++i)
        {
            for (auto& elem : group->GetInstanceBindsMap((Difficulty)i))
            {
                auto state = elem.second.state.lock();
                if (!state)
                    continue;
                std::string TTL = secsToTimeString(
                    state->GetResetTime() - WorldTimer::time_no_syscall(),
                    true);
                PSendSysMessage("%s (%s) | mapid: %u binds: " SIZEFMTD
                                " co-owners: " SIZEFMTD " perm: %s TTL: %s.",
                    state->GetMapEntry()->name[GetSessionDbcLocale()],
                    (state->GetDifficulty() == DUNGEON_DIFFICULTY_NORMAL) ?
                        "normal" :
                        "heroic",
                    state->GetMapId(), state->GetBoundPlayers().size(),
                    state->GetCoOwners().size(),
                    state->CanReset() ? "no" : "yes", TTL.c_str());
            }
        }
    }

    return true;
}

bool ChatHandler::HandleInstanceUnbindCommand(char* args)
{
    if (!*args)
        return false;

    Player* player = getSelectedPlayer();
    if (!player)
        player = m_session->GetPlayer();
    uint32 counter = 0;
    uint32 mapid = 0;
    bool got_map = false;

    if (strncmp(args, "all", strlen(args)) != 0)
    {
        if (!isNumeric(args[0]))
            return false;

        got_map = true;
        mapid = atoi(args);
    }

    std::vector<std::pair<uint32, Difficulty>>
        unbinds; // Unbinds cannot be processed while looping
    for (int i = 0; i < MAX_DIFFICULTY; ++i)
    {
        auto& binds = player->GetInstanceBindsMap(Difficulty(i));
        for (auto& bind : binds)
        {
            if (got_map && mapid != bind.first)
                continue;

            if (bind.first != player->GetMapId())
            {
                auto save = bind.second.state.lock();
                std::string timeleft = secsToTimeString(
                    save->GetResetTime() - WorldTimer::time_no_syscall(), true);

                if (const MapEntry* entry = sMapStore.LookupEntry(bind.first))
                {
                    PSendSysMessage(
                        "unbinding map: %d (%s) inst: %d perm: %s diff: %s "
                        "canReset: %s TTL: %s",
                        bind.first, entry->name[GetSessionDbcLocale()],
                        save->GetInstanceId(), bind.second.perm ? "yes" : "no",
                        save->GetDifficulty() == DUNGEON_DIFFICULTY_NORMAL ?
                            "normal" :
                            "heroic",
                        save->CanReset() ? "yes" : "no", timeleft.c_str());
                }
                else
                    PSendSysMessage(
                        "bound for a nonexistent map %u", bind.first);

                // Reset dungeon map if empty
                {
                    auto state = bind.second.state.lock();
                    if (state)
                    {
                        if (Map* map = sMapMgr::Instance()->FindMap(
                                state->GetMapId(), state->GetInstanceId()))
                            if (map->IsDungeon() && !map->HavePlayers())
                                ((DungeonMap*)map)->Reset(INSTANCE_RESET_ALL);
                    }
                }

                unbinds.push_back(std::make_pair(bind.first, Difficulty(i)));
                ++counter;
            }
        }
    }
    PSendSysMessage("instances unbound: %d", counter);

    for (auto p : unbinds)
        player->UnbindFromInstance(p.first, p.second);

    return true;
}

bool ChatHandler::HandleInstanceResetLimitCommand(char*)
{
    Player* player = getSelectedPlayer();
    if (!player)
        player = m_session->GetPlayer();

    player->ClearDungeonLimit();
    PSendSysMessage("Cleared dungeons per hour limit for %s.",
        player->GetObjectGuid().GetString().c_str());

    return true;
}

bool ChatHandler::HandleInstanceStatsCommand(char* /*args*/)
{
    PSendSysMessage(
        "instances loaded: %d", sMapMgr::Instance()->GetNumInstances());
    PSendSysMessage("players in instances: %d",
        sMapMgr::Instance()->GetNumPlayersInInstances());

    uint32 numSaves, numBoundPlayers, numBoundGroups;
    sMapPersistentStateMgr::Instance()->GetStatistics(
        numSaves, numBoundPlayers, numBoundGroups);
    PSendSysMessage("instance saves: %d", numSaves);
    PSendSysMessage("players bound: %d", numBoundPlayers);
    PSendSysMessage("groups bound: %d", numBoundGroups);
    return true;
}

bool ChatHandler::HandleInstanceSaveDataCommand(char* /*args*/)
{
    Player* pl = m_session->GetPlayer();

    Map* map = pl->GetMap();

    InstanceData* iData = map->GetInstanceData();
    if (!iData)
    {
        PSendSysMessage("Map has no instance data.");
        SetSentErrorMessage(true);
        return false;
    }

    iData->SaveToDB();
    return true;
}

/// Display the list of GMs
bool ChatHandler::HandleGMListFullCommand(char* /*args*/)
{
    ///- Get the accounts with GM Level >0
    QueryResult* result = LoginDatabase.Query(
        "SELECT username,gmlevel FROM account WHERE gmlevel > 0");
    if (result)
    {
        SendSysMessage(LANG_GMLIST);
        SendSysMessage("========================");
        SendSysMessage(LANG_GMLIST_HEADER);
        SendSysMessage("========================");

        ///- Circle through them. Display username and GM level
        do
        {
            Field* fields = result->Fetch();
            PSendSysMessage(
                "|%15s|%6s|", fields[0].GetString(), fields[1].GetString());
        } while (result->NextRow());

        PSendSysMessage("========================");
        delete result;
    }
    else
        PSendSysMessage(LANG_GMLIST_EMPTY);
    return true;
}

/// Define the 'Message of the day' for the realm
bool ChatHandler::HandleServerSetMotdCommand(char* args)
{
    sWorld::Instance()->SetMotd(args);
    PSendSysMessage(LANG_MOTD_NEW, args);
    return true;
}

bool ChatHandler::ShowPlayerListHelper(
    QueryResult* result, uint32* limit, bool title, bool error)
{
    if (!result)
    {
        if (error)
        {
            PSendSysMessage(LANG_NO_PLAYERS_FOUND);
            SetSentErrorMessage(true);
        }
        return false;
    }

    if (!m_session && title)
    {
        SendSysMessage(LANG_CHARACTERS_LIST_BAR);
        SendSysMessage(LANG_CHARACTERS_LIST_HEADER);
        SendSysMessage(LANG_CHARACTERS_LIST_BAR);
    }

    if (result)
    {
        ///- Circle through them. Display username and GM level
        do
        {
            // check limit
            if (limit)
            {
                if (*limit == 0)
                    break;
                --*limit;
            }

            Field* fields = result->Fetch();
            uint32 guid = fields[0].GetUInt32();
            std::string name = fields[1].GetCppString();
            uint8 race = fields[2].GetUInt8();
            uint8 class_ = fields[3].GetUInt8();
            uint32 level = fields[4].GetUInt32();

            ChrRacesEntry const* raceEntry = sChrRacesStore.LookupEntry(race);
            ChrClassesEntry const* classEntry =
                sChrClassesStore.LookupEntry(class_);

            char const* race_name =
                raceEntry ? raceEntry->name[GetSessionDbcLocale()] : "<?>";
            char const* class_name =
                classEntry ? classEntry->name[GetSessionDbcLocale()] : "<?>";

            if (!m_session)
                PSendSysMessage(LANG_CHARACTERS_LIST_LINE_CONSOLE, guid,
                    name.c_str(), race_name, class_name, level);
            else
                PSendSysMessage(LANG_CHARACTERS_LIST_LINE_CHAT, guid,
                    name.c_str(), name.c_str(), race_name, class_name, level);

        } while (result->NextRow());
    }

    if (!m_session)
        SendSysMessage(LANG_CHARACTERS_LIST_BAR);

    return true;
}

/// Output list of character for account
bool ChatHandler::HandleAccountCharactersCommand(char* args)
{
    ///- Get the command line arguments
    std::string account_name;
    Player* target = nullptr; // only for triggering use targeted player account
    uint32 account_id = ExtractAccountId(&args, &account_name, &target);
    if (!account_id)
        return false;

    ///- Get the characters for account id
    std::unique_ptr<QueryResult> result(CharacterDatabase.PQuery(
        "SELECT guid, name, race, class, level FROM characters WHERE account = "
        "%u",
        account_id));

    return ShowPlayerListHelper(result.get());
}

/// Set/Unset the expansion level for an account
bool ChatHandler::HandleAccountSetAddonCommand(char* args)
{
    ///- Get the command line arguments
    char* accountStr = ExtractOptNotLastArg(&args);

    std::string account_name;
    uint32 account_id = ExtractAccountId(&accountStr, &account_name);
    if (!account_id)
        return false;

    // Let set addon state only for lesser (strong) security level
    // or to self account
    if (GetAccountId() && GetAccountId() != account_id &&
        HasLowerSecurityAccount(nullptr, account_id, true))
        return false;

    uint32 lev;
    if (!ExtractUInt32(&args, lev))
        return false;

    // No SQL injection
    LoginDatabase.PExecute(
        "UPDATE account SET expansion = '%u' WHERE id = '%u'", lev, account_id);
    PSendSysMessage(
        LANG_ACCOUNT_SETADDON, account_name.c_str(), account_id, lev);
    return true;
}

bool ChatHandler::HandleSendMailHelper(MailDraft& draft, char* args)
{
    // format: "subject text" "mail text"
    char* msgSubject = ExtractQuotedArg(&args);
    if (!msgSubject)
        return false;

    char* msgText = ExtractQuotedArg(&args);
    if (!msgText)
        return false;

    // msgSubject, msgText isn't NUL after prev. check
    draft.SetSubjectAndBody(msgSubject, msgText);

    return true;
}

bool ChatHandler::HandleSendMassMailCommand(char* args)
{
    // format: raceMask "subject text" "mail text"
    uint32 raceMask = 0;
    char const* name = nullptr;

    if (!ExtractRaceMask(&args, raceMask, &name))
        return false;

    // need dynamic object because it trasfered to mass mailer
    auto draft = new MailDraft;

    // fill mail
    if (!HandleSendMailHelper(*draft, args))
    {
        delete draft;
        return false;
    }

    // from console show nonexistent sender
    MailSender sender(MAIL_NORMAL,
        m_session ? m_session->GetPlayer()->GetObjectGuid().GetCounter() : 0,
        MAIL_STATIONERY_GM);

    sMassMailMgr::Instance()->AddMassMailTask(draft, sender, raceMask);

    PSendSysMessage(LANG_MAIL_SENT, name);
    return true;
}

bool ChatHandler::HandleSendItemsHelper(MailDraft& draft, char* args)
{
    // format: "subject text" "mail text" item1[:count1] item2[:count2] ...
    // item12[:count12]
    char* msgSubject = ExtractQuotedArg(&args);
    if (!msgSubject)
        return false;

    char* msgText = ExtractQuotedArg(&args);
    if (!msgText)
        return false;

    // extract items
    typedef std::pair<uint32, uint32> ItemPair;
    typedef std::list<ItemPair> ItemPairs;
    ItemPairs items;

    // get from tail next item str
    while (char* itemStr = ExtractArg(&args))
    {
        // parse item str
        uint32 item_id = 0;
        uint32 item_count = 1;
        if (sscanf(itemStr, "%u:%u", &item_id, &item_count) != 2)
            if (sscanf(itemStr, "%u", &item_id) != 1)
                return false;

        if (!item_id)
        {
            PSendSysMessage(LANG_COMMAND_ITEMIDINVALID, item_id);
            SetSentErrorMessage(true);
            return false;
        }

        ItemPrototype const* item_proto = ObjectMgr::GetItemPrototype(item_id);
        if (!item_proto)
        {
            PSendSysMessage(LANG_COMMAND_ITEMIDINVALID, item_id);
            SetSentErrorMessage(true);
            return false;
        }

        if (item_count < 1 || (item_proto->MaxCount > 0 &&
                                  item_count > uint32(item_proto->MaxCount)))
        {
            PSendSysMessage(
                LANG_COMMAND_INVALID_ITEM_COUNT, item_count, item_id);
            SetSentErrorMessage(true);
            return false;
        }

        while (item_count > item_proto->GetMaxStackSize())
        {
            items.push_back(ItemPair(item_id, item_proto->GetMaxStackSize()));
            item_count -= item_proto->GetMaxStackSize();
        }

        items.push_back(ItemPair(item_id, item_count));

        if (items.size() > MAX_MAIL_ITEMS)
        {
            PSendSysMessage(LANG_COMMAND_MAIL_ITEMS_LIMIT, MAX_MAIL_ITEMS);
            SetSentErrorMessage(true);
            return false;
        }
    }

    // fill mail
    draft.SetSubjectAndBody(msgSubject, msgText);

    for (ItemPairs::const_iterator itr = items.begin(); itr != items.end();
         ++itr)
    {
        if (Item* item = Item::CreateItem(itr->first, itr->second,
                m_session ? m_session->GetPlayer() : nullptr))
        {
            /*XXX:*/
            // save for prevent lost at next mail load, if send fail then item
            // will deleted
            // potential unrussian version: there is something that can reload
            // all mails from the db
            //                              which would result in this item
            //                              being lost if it wasn't saved
            item->db_save();
            draft.AddItem(item);
        }
    }

    return true;
}

bool ChatHandler::HandleSendItemsCommand(char* args)
{
    // format: name "subject text" "mail text" item1[:count1] item2[:count2] ...
    // item12[:count12]
    Player* receiver;
    ObjectGuid receiver_guid;
    std::string receiver_name;
    if (!ExtractPlayerTarget(&args, &receiver, &receiver_guid, &receiver_name))
        return false;

    MailDraft draft;

    // fill mail
    if (!HandleSendItemsHelper(draft, args))
        return false;

    // from console show nonexistent sender
    MailSender sender(MAIL_NORMAL,
        m_session ? m_session->GetPlayer()->GetObjectGuid().GetCounter() : 0,
        MAIL_STATIONERY_GM);

    draft.SendMailTo(MailReceiver(receiver, receiver_guid), sender);

    std::string nameLink = playerLink(receiver_name);
    PSendSysMessage(LANG_MAIL_SENT, nameLink.c_str());
    return true;
}

bool ChatHandler::HandleSendMassItemsCommand(char* args)
{
    // format: racemask "subject text" "mail text" item1[:count1] item2[:count2]
    // ... item12[:count12]

    uint32 raceMask = 0;
    char const* name = nullptr;

    if (!ExtractRaceMask(&args, raceMask, &name))
        return false;

    // need dynamic object because it trasfered to mass mailer
    auto draft = new MailDraft;

    // fill mail
    if (!HandleSendItemsHelper(*draft, args))
    {
        delete draft;
        return false;
    }

    // from console show nonexistent sender
    MailSender sender(MAIL_NORMAL,
        m_session ? m_session->GetPlayer()->GetObjectGuid().GetCounter() : 0,
        MAIL_STATIONERY_GM);

    sMassMailMgr::Instance()->AddMassMailTask(draft, sender, raceMask);

    PSendSysMessage(LANG_MAIL_SENT, name);
    return true;
}

bool ChatHandler::HandleSendMoneyHelper(MailDraft& draft, char* args)
{
    /// format: "subject text" "mail text" money

    char* msgSubject = ExtractQuotedArg(&args);
    if (!msgSubject)
        return false;

    char* msgText = ExtractQuotedArg(&args);
    if (!msgText)
        return false;

    uint32 money;
    if (!ExtractUInt32(&args, money))
        return false;

    if (money <= 0)
        return false;

    // msgSubject, msgText isn't NUL after prev. check
    draft.SetSubjectAndBody(msgSubject, msgText).SetMoney(money);

    return true;
}

bool ChatHandler::HandleSendMoneyCommand(char* args)
{
    /// format: name "subject text" "mail text" money

    Player* receiver;
    ObjectGuid receiver_guid;
    std::string receiver_name;
    if (!ExtractPlayerTarget(&args, &receiver, &receiver_guid, &receiver_name))
        return false;

    MailDraft draft;

    // fill mail
    if (!HandleSendMoneyHelper(draft, args))
        return false;

    // from console show nonexistent sender
    MailSender sender(MAIL_NORMAL,
        m_session ? m_session->GetPlayer()->GetObjectGuid().GetCounter() : 0,
        MAIL_STATIONERY_GM);

    draft.SendMailTo(MailReceiver(receiver, receiver_guid), sender);

    std::string nameLink = playerLink(receiver_name);
    PSendSysMessage(LANG_MAIL_SENT, nameLink.c_str());
    return true;
}

bool ChatHandler::HandleSendMassMoneyCommand(char* args)
{
    /// format: raceMask "subject text" "mail text" money

    uint32 raceMask = 0;
    char const* name = nullptr;

    if (!ExtractRaceMask(&args, raceMask, &name))
        return false;

    // need dynamic object because it trasfered to mass mailer
    auto draft = new MailDraft;

    // fill mail
    if (!HandleSendMoneyHelper(*draft, args))
    {
        delete draft;
        return false;
    }

    // from console show nonexistent sender
    MailSender sender(MAIL_NORMAL,
        m_session ? m_session->GetPlayer()->GetObjectGuid().GetCounter() : 0,
        MAIL_STATIONERY_GM);

    sMassMailMgr::Instance()->AddMassMailTask(draft, sender, raceMask);

    PSendSysMessage(LANG_MAIL_SENT, name);
    return true;
}

/// Send a message to a player in game
bool ChatHandler::HandleSendMessageCommand(char* args)
{
    ///- Find the player
    Player* rPlayer;
    if (!ExtractPlayerTarget(&args, &rPlayer))
        return false;

    ///- message
    if (!*args)
        return false;

    ///- Check that he is not logging out.
    if (rPlayer->GetSession()->isLogingOut())
    {
        SendSysMessage(LANG_PLAYER_NOT_FOUND);
        SetSentErrorMessage(true);
        return false;
    }

    ///- Send the message
    // Use SendAreaTriggerMessage for fastest delivery.
    rPlayer->GetSession()->SendAreaTriggerMessage("%s", args);
    rPlayer->GetSession()->SendAreaTriggerMessage(
        "|cffff0000[Message from administrator]:|r");

    // Confirmation message
    std::string nameLink = GetNameLink(rPlayer);
    PSendSysMessage(LANG_SENDMESSAGE, nameLink.c_str(), args);
    return true;
}

bool ChatHandler::HandleFlushArenaPointsCommand(char* /*args*/)
{
    sBattleGroundMgr::Instance()->DistributeArenaPoints();
    return true;
}

bool ChatHandler::HandleModifyGenderCommand(char* args)
{
    if (!*args)
        return false;

    Player* player = getSelectedPlayer();

    if (!player)
    {
        PSendSysMessage(LANG_PLAYER_NOT_FOUND);
        SetSentErrorMessage(true);
        return false;
    }

    PlayerInfo const* info = sObjectMgr::Instance()->GetPlayerInfo(
        player->getRace(), player->getClass());
    if (!info)
        return false;

    char* gender_str = args;
    int gender_len = strlen(gender_str);

    Gender gender;

    if (!strncmp(gender_str, "male", gender_len)) // MALE
    {
        if (player->getGender() == GENDER_MALE)
            return true;

        gender = GENDER_MALE;
    }
    else if (!strncmp(gender_str, "female", gender_len)) // FEMALE
    {
        if (player->getGender() == GENDER_FEMALE)
            return true;

        gender = GENDER_FEMALE;
    }
    else
    {
        SendSysMessage(LANG_MUST_MALE_OR_FEMALE);
        SetSentErrorMessage(true);
        return false;
    }

    // Set gender
    player->SetByteValue(UNIT_FIELD_BYTES_0, 2, gender);
    player->SetUInt16Value(
        PLAYER_BYTES_3, 0, uint16(gender) | (player->GetDrunkValue() & 0xFFFE));

    // Change display ID
    player->InitDisplayIds();

    char const* gender_full = gender ? "female" : "male";

    PSendSysMessage(LANG_YOU_CHANGE_GENDER, player->GetName(), gender_full);

    if (needReportToTarget(player))
        ChatHandler(player).PSendSysMessage(
            LANG_YOUR_GENDER_CHANGED, gender_full, GetNameLink().c_str());

    return true;
}

bool ChatHandler::HandleMmap(char* args)
{
    bool on;
    if (ExtractOnOff(&args, on))
    {
        if (on)
        {
            sWorld::Instance()->setConfig(CONFIG_BOOL_MMAP_ENABLED, true);
            SendSysMessage(
                "WORLD: mmaps are now ENABLED. Individual map settings are "
                "still in effect.");
        }
        else
        {
            sWorld::Instance()->setConfig(CONFIG_BOOL_MMAP_ENABLED, false);
            SendSysMessage("WORLD: mmaps are now DISABLED.");
        }
        return true;
    }

    on = sWorld::Instance()->getConfig(CONFIG_BOOL_MMAP_ENABLED);
    PSendSysMessage("mmaps are %sabled.", on ? "en" : "dis");

    return true;
}

bool ChatHandler::HandleMmapTestArea(char* args)
{
    if (m_session->GetPlayer()->GetTransport() != nullptr)
    {
        SendSysMessage("Cannot test area on transport.");
        return true;
    }

    float radius = 40.0f;
    ExtractFloat(&args, radius);

    auto units = maps::visitors::yield_set<Creature>{}(m_session->GetPlayer(),
        radius, [](auto&& elem)
        {
            return elem->isAlive();
        });

    if (!units.empty())
    {
        PSendSysMessage("Found " SIZEFMTD " Creatures.", units.size());

        uint32 paths = 0;
        auto t1 = std::chrono::high_resolution_clock::now();

        float gx, gy, gz;
        m_session->GetPlayer()->GetPosition(gx, gy, gz);
        for (auto creature : units)
        {
            PathFinder path(creature);
            path.calculate(gx, gy, gz);
            ++paths;
        }

        auto t2 = std::chrono::high_resolution_clock::now();

        PSendSysMessage("Generated %u paths in %u ms (%u microseconds)", paths,
            uint32(std::chrono::duration_cast<std::chrono::milliseconds>(
                       t2 - t1).count()),
            uint32(std::chrono::duration_cast<std::chrono::microseconds>(
                       t2 - t1).count()));
    }
    else
    {
        PSendSysMessage("No creatures in %f yard range.", radius);
    }

    return true;
}

bool ChatHandler::HandleSmartAIAddWp(char* args)
{
    Player* p = m_session->GetPlayer();
    if (!p)
        return false;

    uint32 pathId;
    if (!ExtractUInt32(&args, pathId))
        return false;
    const char* comment = ExtractQuotedArg(&args);
    if (!comment)
        return false;

    std::string escapedComment(comment);
    WorldDatabase.escape_string(escapedComment);

    uint32 pointId = 0;
    std::unique_ptr<QueryResult> result(WorldDatabase.PQuery(
        "SELECT `pointid` FROM `waypoints` WHERE `entry`=%u ORDER BY `pointid` "
        "DESC LIMIT 1",
        pathId));
    if (result)
        pointId = result->Fetch()->GetUInt32() + 1;

    WorldDatabase.PExecute(
        "INSERT INTO `waypoints` (`entry`, `pointid`, `position_x`, "
        "`position_y`, `position_z`, `point_comment`) VALUES (%u, %u, %f, %f, "
        "%f, '%s')",
        pathId, pointId, p->GetX(), p->GetY(), p->GetZ(),
        escapedComment.c_str());

    PSendSysMessage("Point %u of path %u inserted.", pointId, pathId);

    return true;
}

bool ChatHandler::HandleSmartAIAddGwp(char* args)
{
    Player* p = m_session->GetPlayer();
    if (!p)
        return false;

    uint32 pathId;
    if (!ExtractUInt32(&args, pathId))
        return false;
    const char* comment = ExtractQuotedArg(&args);
    if (!comment)
        return false;
    uint32 delay = 0, run = false, mmap = false;
    ExtractOptUInt32(&args, delay, 0);
    ExtractOptUInt32(&args, run, 0);
    ExtractOptUInt32(&args, mmap, 0);

    std::string escapedComment(comment);
    WorldDatabase.escape_string(escapedComment);

    uint32 pointId = 0;
    std::unique_ptr<QueryResult> result(WorldDatabase.PQuery(
        "SELECT `point` FROM `waypoints_group` WHERE `id`=%u ORDER BY "
        "`point` DESC LIMIT 1",
        pathId));
    if (result)
        pointId = result->Fetch()->GetUInt32() + 1;

    WorldDatabase.PExecute(
        "INSERT INTO `waypoints_group` (`id`, `point`, `x`, `y`, `z`, `o`, "
        "`delay`, `run`, `mmap`, `comment`) VALUES (%u, %u, %f, %f, %f, %f, "
        "%u, %u, %u, '%s')",
        pathId, pointId, p->GetX(), p->GetY(), p->GetZ(), p->GetO(), delay,
        run ? 1 : 0, mmap ? 1 : 0, escapedComment.c_str());

    PSendSysMessage("Group point %u of path %u inserted.", pointId, pathId);

    return true;
}

bool ChatHandler::HandleBanWaveAddCommand(char* args)
{
    const char* cstr_name = ExtractQuotedOrLiteralArg(&args);
    if (!cstr_name)
    {
        SendSysMessage("Error: name missing.");
        return true;
    }

    std::string name = sBanWave::Instance()->fix_name(cstr_name);

    const char* reason = ExtractQuotedOrLiteralArg(&args);
    if (!reason)
    {
        SendSysMessage("Error: missing reason.");
        return true;
    }

    CharacterDatabase.escape_string(name);
    std::unique_ptr<QueryResult> result(CharacterDatabase.PQuery(
        "SELECT guid FROM characters WHERE name=\"%s\"", name.c_str()));
    if (!result)
    {
        PSendSysMessage("Error: no player with name %s found.", name.c_str());
        return true;
    }

    sBanWave::Instance()->add_ban(
        name, reason, m_session->GetPlayer()->GetName());
    PSendSysMessage("Added %s to ban wave.", name.c_str());

    return true;
}

bool ChatHandler::HandleBanWavePrintCommand(char* /*args*/)
{
    std::string s = sBanWave::Instance()->print();
    PSendSysMessage("%s", s.c_str());
    return true;
}

bool ChatHandler::HandleBanWaveRemoveCommand(char* args)
{
    const char* cstr_name = ExtractQuotedOrLiteralArg(&args);
    if (!cstr_name)
    {
        SendSysMessage("Error: name missing.");
        return true;
    }
    std::string name = sBanWave::Instance()->fix_name(cstr_name);

    if (!sBanWave::Instance()->has_ban_on(name))
    {
        SendSysMessage("Player is not flagged for a ban wave.");
        return true;
    }

    sBanWave::Instance()->remove_ban(name, m_session->GetPlayer()->GetName());
    PSendSysMessage("Removed ban wave flagging of player %s.", name.c_str());
    return true;
}

bool ChatHandler::HandleBanWaveRunCommand(char* args)
{
    const char* verify = ExtractQuotedArg(&args);
    if (!verify || strcmp(verify, "force start") != 0)
    {
        SendSysMessage(
            "Only start a ban wave if you've made 100% sure you agree with "
            ".banwave print. "
            "All bans are executed and all cancelled bans removed from "
            "history. If you are sure, run the command like: "
            ".banwave run \"force start\"");
        return true;
    }

    uint32 count = sBanWave::Instance()->do_ban_wave();
    PSendSysMessage("Ran ban wave. Users banned: %u", count);
    return true;
}

bool ChatHandler::HandleBanWaveSearchCommand(char* args)
{
    const char* search = ExtractLiteralArg(&args);
    if (!search)
        return false;
    std::string s = sBanWave::Instance()->search(search);
    PSendSysMessage("%s", s.c_str());
    return true;
}

bool ChatHandler::HandleNpcAggroLinkCommand(char* args)
{
    Player* player = m_session->GetPlayer();
    Creature* creature = player->GetMap()->GetCreature(player->GetTargetGuid());

    if (!creature)
    {
        SendSysMessage("You must target a creature first!");
        return true;
    }

    uint32 entry, guid;
    if (!ExtractUInt32(&args, entry) || !ExtractUInt32(&args, guid))
        return false;

    Creature* boss =
        player->GetMap()->GetCreature(ObjectGuid(HIGHGUID_UNIT, entry, guid));
    if (!boss)
    {
        SendSysMessage(
            "No target with that entry & guid exists in your current map!");
        return true;
    }

    std::unique_ptr<QueryResult> res(WorldDatabase.PQuery(
        "SELECT * FROM npc_aggro_link WHERE boss_entry=%u AND boss_guid=%u AND "
        "defender_entry=%u AND defender_guid=%u",
        boss->GetEntry(), boss->GetGUIDLow(), creature->GetEntry(),
        creature->GetGUIDLow()));
    if (res)
    {
        SendSysMessage("That link already exists!");
        return true;
    }

    WorldDatabase.PExecute(
        "INSERT INTO npc_aggro_link (boss_entry, boss_guid, defender_entry, "
        "defender_guid) VALUES(%u, %u, %u, %u)",
        boss->GetEntry(), boss->GetGUIDLow(), creature->GetEntry(),
        creature->GetGUIDLow());
    return true;
}

bool ChatHandler::HandleDefmodeCommand(char* args)
{
    char* arg_one = ExtractLiteralArg(&args);
    if (!arg_one)
    {
        SendSysMessage(
            "Command: .demofe {on/off} {level}. Level is 70 if not specified.");
        return true;
    }

    uint32 lvl;
    ExtractOptUInt32(&args, lvl, 70);

    if (strcmp(arg_one, "on") == 0)
    {
        if (sWorld::Instance()->defmode_level() != 0)
        {
            SendSysMessage("Already turned on.");
            return true;
        }
        sWorld::Instance()->defmode_level(lvl);
        PSendSysMessage("Turned on, level limit is %u.", lvl);
        return true;
    }
    else if (strcmp(arg_one, "off") == 0)
    {
        if (sWorld::Instance()->defmode_level() == 0)
        {
            SendSysMessage("Already turned off.");
            return true;
        }
        sWorld::Instance()->defmode_level(0);
        SendSysMessage("Turned off.");
        return true;
    }

    return false;
}

bool ChatHandler::HandleTplocAddCommand(char* args)
{
    if (!*args)
        return false;

    Player* player = m_session->GetPlayer();

    std::string name = args;

    if (sObjectMgr::Instance()->GetGameTele(name, false))
    {
        SendSysMessage(LANG_COMMAND_TP_ALREADYEXIST);
        SetSentErrorMessage(true);
        return false;
    }

    GameTele tele;
    tele.position_x = player->GetX();
    tele.position_y = player->GetY();
    tele.position_z = player->GetZ();
    tele.orientation = player->GetO();
    tele.mapId = player->GetMapId();
    tele.name = name;

    if (sObjectMgr::Instance()->AddGameTele(tele))
    {
        SendSysMessage(LANG_COMMAND_TP_ADDED);
    }
    else
    {
        SendSysMessage(LANG_COMMAND_TP_ADDEDERR);
        SetSentErrorMessage(true);
        return false;
    }

    return true;
}

bool ChatHandler::HandleTplocDelCommand(char* args)
{
    if (!*args)
        return false;

    std::string name = args;

    if (!sObjectMgr::Instance()->DeleteGameTele(name))
    {
        SendSysMessage(LANG_COMMAND_TELE_NOTFOUND);
        SetSentErrorMessage(true);
        return false;
    }

    SendSysMessage(LANG_COMMAND_TP_DELETED);
    return true;
}

bool ChatHandler::HandleTempSummonCommand(char* args)
{
    auto usage = [this]()
    {
        SendSysMessage("Usage:");
        SendSysMessage(".tempsummon <npc/go> <entry> <duration in seconds>");
    };

    auto cmd = ExtractLiteralArg(&args);
    if (!cmd)
    {
        usage();
        return true;
    }

    uint32 entry, secs;
    if (!ExtractUInt32(&args, entry) || !ExtractUInt32(&args, secs))
    {
        usage();
        return true;
    }

    auto player = m_session->GetPlayer();
    if (strcmp(cmd, "npc") == 0)
    {
        player->SummonCreature(entry, player->GetX(), player->GetY(),
            player->GetZ(), player->GetO(), TEMPSUMMON_TIMED_DEATH,
            secs * 1000);
    }
    else if (strcmp(cmd, "go") == 0)
    {
        player->SummonGameObject(entry, player->GetX(), player->GetY(),
            player->GetZ(), player->GetO(), 0, 0, 0, 0, secs, true);
    }
    else
        usage();

    return true;
}

bool ChatHandler::HandleMobConvertCommand(char* args)
{
    auto str = ExtractLiteralArg(&args);
    if (!str)
        return false;

    Creature* unit = getSelectedCreature();
    if (!unit)
        return false;

    auto insert_smart_script =
        [unit, this](uint32 flags, uint32 event, uint32 eparam1, uint32 eparam2,
            uint32 eparam3, uint32 eparam4, uint32 action, uint32 action1,
            uint32 action2, uint32 target, std::string comment)
    {
        uint32 id = 0;
        std::unique_ptr<QueryResult> res(WorldDatabase.PQuery(
            "SELECT COUNT(*) FROM smart_scripts WHERE entryorguid=%u",
            unit->GetEntry()));
        if (!res)
        {
            SendSysMessage("FAILED INSERTING INTO SMART AI, HALP!");
            return;
        }
        id = res->Fetch()[0].GetUInt32();
        WorldDatabase.escape_string(comment);
        // Use query to prevent async execution
        std::unique_ptr<QueryResult>(WorldDatabase.PQuery(
            "INSERT INTO smart_scripts (entryorguid, id, event_flags, "
            "event_type, event_param1, event_param2, event_param3, "
            "event_param4, action_type, action_param1, action_param2, "
            "target_type, comment) VALUES( "
            "%u, %u, %u, %u, %u, %u, %u, %u, %u, %u, %u, %u, '%s')",
            unit->GetEntry(), id, flags, event, eparam1, eparam2, eparam3,
            eparam4, action, action1, action2, target, comment.c_str()));
    };

    auto spell_name = [](uint32 spell)
    {
        if (auto info = sSpellStore.LookupEntry(spell))
            return std::string(info->SpellName[0]);
        return std::string();
    };

    auto spell_string = [&spell_name](uint32 spell)
    {
        std::string str = spell_name(spell);
        str += " (" + std::to_string(spell) + ")";
        return str;
    };

    auto insert_text = [&insert_smart_script, unit, this](uint32 event_type,
        uint32 event_id, std::string text_type, uint32 opt = 0)
    {
        std::unique_ptr<QueryResult> eres(WorldDatabase.PQuery(
            "SELECT * FROM creature_ai_scripts WHERE id=%u", event_id));
        if (!eres)
        {
            SendSysMessage("Failed adding texts");
            return;
        }
        std::unique_ptr<QueryResult> ctres(WorldDatabase.PQuery(
            "SELECT MAX(groupid) FROM creature_text WHERE entry=%u",
            unit->GetEntry()));
        uint32 group = ctres->Fetch()[0].GetUInt32() + 1;
        uint32 id = 1;

        PSendSysMessage("* Adding %s:", text_type.c_str());
        Field* fields = eres->Fetch();
        for (int i = 0; i < 3; ++i)
        {
            uint32 type = fields[10 + i * 4].GetUInt32();
            if (type != 1)
                continue;
            for (int j = 0; j < 3; ++j)
            {
                int32 text_id = fields[11 + j + i * 4].GetInt32();
                if (text_id == 0)
                    continue;
                std::unique_ptr<QueryResult> res(WorldDatabase.PQuery(
                    "SELECT * FROM creature_ai_texts WHERE entry=%d", text_id));
                if (!res)
                {
                    PSendSysMessage(
                        "Missing creature_ai_texts entry=%d", text_id);
                    continue;
                }
                Field* fields = res->Fetch();
                auto str = fields[1].GetCppString();
                int sound = fields[10].GetInt32();
                int type = fields[11].GetInt32();
                int language = fields[12].GetInt32();
                int emote = fields[13].GetInt32();
                uint32 text_range = 0;
                switch (type)
                {
                case CHAT_TYPE_SAY:
                    type = 12;
                    break;
                case CHAT_TYPE_YELL:
                    type = 14;
                    break;
                case CHAT_TYPE_TEXT_EMOTE:
                    type = 16;
                    break;
                case CHAT_TYPE_BOSS_EMOTE:
                    type = 42;
                    break;
                case CHAT_TYPE_WHISPER:
                    type = 15;
                    break;
                case CHAT_TYPE_BOSS_WHISPER:
                    type = 41;
                    break;
                case CHAT_TYPE_ZONE_YELL:
                    type = 14;
                    text_range = 3;
                    break;
                }
                PSendSysMessage(
                    "    '%s' (group=%u, id=%u, type=%d, sound=%d, emote=%d, "
                    "range=%u, lang=%u)",
                    str.c_str(), group, id, type, sound, emote, text_range,
                    language);
                WorldDatabase.escape_string(str);
                // Use query to prevent async execution
                std::unique_ptr<QueryResult>(WorldDatabase.PQuery(
                    "INSERT INTO creature_text(entry, groupid, id, text, type, "
                    "probability, emote, sound, text_range, language) "
                    "VALUES(%u, %u, %u, '%s', %u, %u, %u, %u, %u, %u)",
                    unit->GetEntry(), group, id, str.c_str(), type, 100, emote,
                    sound, text_range, language));
                ++id;
            }
        }

        insert_smart_script(event_type == 2 ? 1 : 0, event_type, 0,
            event_type == 2 ? opt : 0, 0, 0, 1, group, 0,
            event_type == 5 ? 7 : 1, "Do " + text_type);
    };

    uint32 unit_class = 1;
    if (strcmp(str, "mage") == 0)
        unit_class = 8;
    else if (strcmp(str, "rogue") == 0)
        unit_class = 4;
    else if (strcmp(str, "warrior") == 0)
        unit_class = 1;
    else if (strcmp(str, "paladin") == 0)
        unit_class = 2;

    std::unique_ptr<QueryResult> ai_res(WorldDatabase.PQuery(
        "SELECT AIName FROM creature_template WHERE entry=%u",
        unit->GetEntry()));
    if (!ai_res || ai_res->Fetch()[0].GetCppString().compare("EventAI") != 0)
    {
        PSendSysMessage("%s is not EventAI", unit->GetName());
        return true;
    }

    if (strcmp(str, "print") == 0)
    {
        PSendSysMessage("%s has the following spells:", unit->GetName());
        std::unique_ptr<QueryResult> res(WorldDatabase.PQuery(
            "SELECT * FROM creature_ai_scripts WHERE creature_id=%u",
            unit->GetEntry()));
        if (res)
        {
            do
            {
                Field* fields = res->Fetch();
                for (int i = 0; i < 3; ++i)
                {
                    uint32 type = fields[10 + i * 4].GetUInt32();
                    int32 param1 = fields[11 + i * 4].GetInt32();
                    if (type == 11)
                        PSendSysMessage("%s", spell_string(param1).c_str());
                }
            } while (res->NextRow());
        }
        return true;
    }

    PSendSysMessage("Converted %s (%u), took following actions:",
        unit->GetName(), unit->GetEntry());
    std::unique_ptr<QueryResult> res(WorldDatabase.PQuery(
        "SELECT * FROM creature_ai_scripts WHERE creature_id=%u",
        unit->GetEntry()));
    if (!res)
    {
        WorldDatabase.PExecute(
            "UPDATE creature_template SET unit_class=%u, AIName=\"\" WHERE "
            "entry=%u",
            unit_class, unit->GetEntry());
        SendSysMessage("* No events found: removed AI name");
        return true;
    }

    PSendSysMessage("* Set class to %u", unit_class);
    WorldDatabase.PExecute(
        "UPDATE creature_template SET unit_class=%u, AIName=\"SmartAI\" WHERE "
        "entry=%u",
        unit_class, unit->GetEntry());

    do
    {
        Field* fields = res->Fetch();

        uint32 event_id = fields[0].GetUInt32();
        uint32 event = fields[2].GetUInt32();
        uint32 flags = fields[5].GetUInt32();
        uint32 event_param1 = fields[6].GetUInt32();
        uint32 event_param2 = fields[7].GetUInt32();
        uint32 event_param3 = fields[8].GetUInt32();
        uint32 event_param4 = fields[9].GetUInt32();

        switch (event)
        {
        case 2:
        {
            bool skip_text = false;
            if (flags & 1)
                break;
            for (int i = 0; i < 3; ++i)
            {
                uint32 type = fields[10 + i * 4].GetUInt32();
                int32 param1 = fields[11 + i * 4].GetInt32();
                // Flee
                if (type == 25)
                {
                    PSendSysMessage("* Added flee at %u%% hp", event_param1);
                    skip_text = true;
                    insert_smart_script(1, 2, 0, event_param1, 0, 0, 25, 1, 0,
                        1, "Flee at " + std::to_string(event_param1) + "% hp");
                }
                // Spell cast
                else if (type == 11)
                {
                    PSendSysMessage("* Cast spell %s at %u%% hp",
                        spell_string(param1).c_str(), event_param1);
                    insert_smart_script(1, 2, 0, event_param1, 0, 0, 11, param1,
                        2, 1, "Cast %s " + spell_name(param1) + " at " +
                                  std::to_string(event_param1) + "% hp");
                }
                // Text
                else if (type == 1 && !skip_text)
                {
                    insert_text(event, event_id,
                        std::to_string(event_param1) + "% hp text",
                        event_param1);
                    skip_text = true;
                }
            }
            break;
        }
        case 0:  // timer
        case 1:  // ooc timer
        case 9:  // range
        case 11: // spawned
        case 13: // target casting
        case 14: // missing hp
        {
            for (int i = 0; i < 3; ++i)
            {
                uint32 action = fields[10 + i * 4].GetUInt32();
                int32 spell = fields[11 + i * 4].GetInt32();
                int32 target = fields[12 + i * 4].GetInt32();
                int32 flags = fields[13 + i * 4].GetInt32();
                if (action != 11)
                    continue;

                uint32 type = 0;
                uint32 target_settings = 0;
                if (event == 14) // friendly missing hp
                    type = 1;
                else if (target == 0) // self
                    type = 4;
                else if (target == 4) // random
                    target_settings = 8;
                else if (target == 5) // random, not tank
                    target_settings = 32;
                else if (target == 13) // friendly
                    type = 1;

                if (flags & 32 && type == 0)
                    type = 9;

                if (auto info = sSpellStore.LookupEntry(spell))
                {
                    // AoE
                    if (IsAreaOfEffectSpell(info) && info->rangeIndex == 1 &&
                        !IsPositiveSpell(info))
                    {
                        type = 7;
                        target_settings = 0x200000;
                    }
                    // Interrupt
                    if (info->HasEffect(SPELL_EFFECT_INTERRUPT_CAST))
                        type = 5;
                    // Dispel
                    if (info->HasEffect(SPELL_EFFECT_DISPEL))
                        type = 6;
                    if (event == 1 || event == 11)
                    {
                        if (info->HasEffect(SPELL_EFFECT_SUMMON_PET) ||
                            info->HasEffect(SPELL_EFFECT_SUMMON))
                            type = 8;
                        else
                            continue;
                    }
                }

                uint32 min_cd = fields[8].GetUInt32();
                uint32 max_cd = fields[9].GetUInt32();
                if ((unit_class == 8 || unit_class == 4) && min_cd < 6000)
                {
                    min_cd = 2000;
                    max_cd = 4000;
                }
                else
                {
                    if (min_cd < 6000)
                    {
                        if (unit_class == 2)
                        {
                            min_cd = 5000;
                            max_cd = 10000;
                        }
                        else
                        {
                            min_cd = 4000;
                            max_cd = 8000;
                        }
                    }
                    else if (min_cd < 20000)
                    {
                        min_cd = 10000;
                        max_cd = 15000;
                    }
                    else
                    {
                        min_cd = 15000;
                        max_cd = 25000;
                    }
                }

                if (type == 8)
                    min_cd = max_cd = 0;

                uint32 prio = min_cd > 9000 ? 10 : 0;

                PSendSysMessage(
                    "* Added behavioral AI spell=%s (type=%u, cd: %u-%u, prio: "
                    "%u, settings: 0x%x)",
                    spell_string(spell).c_str(), type, min_cd, max_cd, prio,
                    target_settings);
                WorldDatabase.PExecute(
                    "INSERT INTO creature_ai_spells (creature_id, spell_id, "
                    "type, priority, cooldown_min, cooldown_max, "
                    "target_settings) VALUES (%u, %u, %u, %u, %u, %u, %u)",
                    unit->GetEntry(), spell, type, prio, min_cd, max_cd,
                    target_settings);
            }
            break;
        }
        case 4: // aggro
        case 5: // kill
        case 6: // death
        {
            auto e_str = event == 4 ? "aggro" : event == 5 ? "kill" : "death";
            bool skip_text = false;
            for (int i = 0; i < 3; ++i)
            {
                uint32 type = fields[10 + i * 4].GetUInt32();

                if (type == 1 && !skip_text)
                {
                    insert_text(event, event_id, e_str + std::string(" yell"));
                    skip_text = true;
                }
                else if (type == 11)
                {
                    int32 spell = fields[11 + i * 4].GetInt32();
                    int32 target = fields[12 + i * 4].GetInt32();
                    int32 flags = fields[13 + i * 4].GetInt32();
                    bool self = false;
                    if (target == 0)
                        self = true;
                    if (event == 4)
                    {
                        PSendSysMessage(
                            "* NOTE: Would've added spell cast on %s "
                            "(spell='%s', triggered=%s, "
                            "target=%s), but skipped since aggro event",
                            e_str, spell_string(spell).c_str(),
                            flags & 2 ? "yes" : "no",
                            self ? "self" : "invoker");
                        continue;
                    }
                    PSendSysMessage(
                        "* Added spell cast on %s (spell='%s', triggered=%s, "
                        "target=%s)",
                        e_str, spell_string(spell).c_str(),
                        flags & 2 ? "yes" : "no", self ? "self" : "invoker");
                    insert_smart_script(0, event == 6 ? 76 : event, 0, 0, 0, 0,
                        11, spell, flags & 0x2 ? 0x2 : 0, self ? 1 : 7,
                        "Cast " + spell_name(spell) + " on " + e_str);
                }
            }
            break;
        }
        case 12: // target health pct
        {
            for (int i = 0; i < 3; ++i)
            {
                uint32 action = fields[10 + i * 4].GetUInt32();
                int32 spell = fields[11 + i * 4].GetInt32();
                if (action != 11)
                    continue;

                PSendSysMessage(
                    "* Added execute spell at %u%% hp: %s (cd: %u-%u)",
                    event_param1, spell_string(spell).c_str(), event_param3,
                    event_param4);
                insert_smart_script(0, 12, event_param2, event_param1,
                    event_param3, event_param4, 11, spell, 0, 1,
                    "Target " + std::to_string(event_param1) + "% hp: Cast " +
                        spell_name(spell));
            }
            break;
        }
        case 27: // missing aura
        {
            for (int i = 0; i < 3; ++i)
            {
                uint32 action = fields[10 + i * 4].GetUInt32();
                int32 spell = fields[11 + i * 4].GetInt32();
                if (action != 11)
                    continue;

                PSendSysMessage("* Added buff armor spell: %s",
                    spell_string(spell).c_str());
                insert_smart_script(0, 60, 2000, 3000, 25000, 35000, 11, spell,
                    32, 1, "Update: Cast " + spell_name(spell));
            }
            break;
        }
        }
    } while (res->NextRow());

    return true;
}

bool ChatHandler::HandleArenaTeamCreateCommand(char* args)
{
    auto player = getSelectedPlayer();
    if (!player)
    {
        SendSysMessage("Error: Must target a player.");
        return true;
    }

    ArenaType size;
    auto lit = ExtractLiteralArg(&args);
    if (lit && strcmp(lit, "2v2") == 0)
        size = ARENA_TYPE_2v2;
    else if (lit && strcmp(lit, "3v3") == 0)
        size = ARENA_TYPE_3v3;
    else if (lit && strcmp(lit, "5v5") == 0)
        size = ARENA_TYPE_5v5;
    else
    {
        SendSysMessage("Usage: .arena team create <2v2, 3v3, 5v5> <\"name\">.");
        return true;
    }

    if (player->GetArenaTeamId(
            (size == ARENA_TYPE_2v2) ? 0 : (size == ARENA_TYPE_3v3) ? 1 : 2))
    {
        SendSysMessage("Error: Player already in team of that size.");
        return true;
    }

    auto name = ExtractQuotedArg(&args);
    if (!name || strlen(name) == 0 ||
        strlen(name) > 64) // TODO: Actual min, max len
    {
        SendSysMessage("Usage: .arena team create <2v2, 3v3, 5v5> <\"name\">.");
        return true;
    }

    auto at = new ArenaTeam;
    if (!at->Create(player->GetObjectGuid(), size, name))
    {
        delete at;
        PSendSysMessage("Failed at creating arena team with name '%s'", name);
        return true;
    }

    sObjectMgr::Instance()->AddArenaTeam(at);
    PSendSysMessage("Created team %s.", name);

    return true;
}

bool ChatHandler::HandleArenaTeamListCommand(char* /*args*/)
{
    auto player = getSelectedPlayer();
    if (!player)
    {
        SendSysMessage("Error: Must target a player.");
        return true;
    }

    for (uint32 arena_slot = 0; arena_slot < MAX_ARENA_SLOT; ++arena_slot)
    {
        uint32 team_id = player->GetArenaTeamId(arena_slot);
        if (!team_id)
            continue;

        if (auto at = sObjectMgr::Instance()->GetArenaTeamById(team_id))
        {
            std::string size = "2v2";
            if (at->GetType() == ARENA_TYPE_3v3)
                size = "3v3";
            else if (at->GetType() == ARENA_TYPE_5v5)
                size = "5v5";
            PSendSysMessage("%s: %s (id: %u)", size.c_str(),
                at->GetName().c_str(), at->GetId());
        }
    }

    return true;
}

bool ChatHandler::HandleArenaTeamRatingCommand(char* args)
{
    uint32 id, rating;
    if (!ExtractUInt32(&args, id))
    {
        SendSysMessage("Usage: .arena team rating <team id> <rating>.");
        return true;
    }
    if (!ExtractUInt32(&args, rating))
    {
        SendSysMessage("Usage: .arena team rating <team id> <rating>.");
        return true;
    }

    if (auto at = sObjectMgr::Instance()->GetArenaTeamById(id))
    {
        std::string size = "2v2";
        if (at->GetType() == ARENA_TYPE_3v3)
            size = "3v3";
        else if (at->GetType() == ARENA_TYPE_5v5)
            size = "5v5";
        PSendSysMessage("%s: %s (id: %u) rating changed from %u to %u.",
            size.c_str(), at->GetName().c_str(), at->GetId(), at->GetRating(),
            rating);

        at->RegisterFinishedMatch(
            at->GetId(), std::vector<ObjectGuid>(), rating, true);
        sObjectMgr::Instance()->update_arena_rankings();
        at->SaveToDB();
        at->NotifyStatsChanged();
    }

    return true;
}
